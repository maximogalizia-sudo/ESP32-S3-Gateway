#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <Preferences.h>

// --- UUIDs COMPARTIDOS (deben ser IGUALES en C3 y S3) ---
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define UMBRAL_PROXIMIDAD -60

// --- ESTADO GLOBAL ---
Preferences preferences;
String macRegistrada      = "";
bool   haySensorVinculado = false;

bool   sensorEncontrado   = false;
String macDelSensorActual = "";
int    rssiActual         = 0;
BLEAdvertisedDevice* dispositivoObjetivo = nullptr;

// ─────────────────────────────────────────────
// ETAPA 1: Callback del escaneo BLE
// ─────────────────────────────────────────────
class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {

    // Filtro 1: ¿Tiene nuestro UUID?
    if (!advertisedDevice.haveServiceUUID() ||
        !advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      return;
    }

    String macEncontrada = advertisedDevice.getAddress().toString().c_str();
    int    rssi          = advertisedDevice.getRSSI();

    Serial.printf("[SCAN] UUID correcto | MAC: %s | RSSI: %d\n",
                  macEncontrada.c_str(), rssi);

    // CASO A: Sin sensor vinculado → vincular por proximidad
    if (!haySensorVinculado) {
      if (rssi >= UMBRAL_PROXIMIDAD) {
        Serial.println("[VINC] Sensor nuevo en rango. Vinculando...");

        preferences.begin("gw_conf", false);
        preferences.putString("mac_sensor", macEncontrada);
        preferences.end();

        macRegistrada      = macEncontrada;
        haySensorVinculado = true;

        Serial.printf("[VINC] MAC guardada: %s\n", macRegistrada.c_str());

        sensorEncontrado    = true;
        macDelSensorActual  = macEncontrada;
        rssiActual          = rssi;
        dispositivoObjetivo = new BLEAdvertisedDevice(advertisedDevice);
        BLEDevice::getScan()->stop();

      } else {
        Serial.printf("[VINC] Sensor muy lejos (RSSI %d < umbral %d). Ignorando.\n",
                      rssi, UMBRAL_PROXIMIDAD);
      }
      return;
    }

    // CASO B: Sensor ya vinculado → verificar MAC
    if (macEncontrada == macRegistrada) {
      Serial.printf("[SCAN] Sensor conocido | RSSI: %d\n", rssi);
      sensorEncontrado    = true;
      macDelSensorActual  = macEncontrada;
      rssiActual          = rssi;
      dispositivoObjetivo = new BLEAdvertisedDevice(advertisedDevice);
      BLEDevice::getScan()->stop();
    } else {
      Serial.printf("[SCAN] MAC desconocida (%s). Posible sensor vecino. Ignorando.\n",
                    macEncontrada.c_str());
    }
  }
};

// ─────────────────────────────────────────────
// ETAPA 2: Conectarse al C3 y leer el dato
// ─────────────────────────────────────────────
void conectarYLeerSensor(BLEAdvertisedDevice* dispositivo) {
  Serial.println("[GATT] Conectando al sensor...");

  BLEClient* pClient = BLEDevice::createClient();

  if (!pClient->connect(dispositivo)) {
    Serial.println("[GATT] Error: no se pudo conectar.");
    delete pClient;
    return;
  }
  Serial.println("[GATT] Conectado.");

  BLERemoteService* pService = pClient->getService(BLEUUID(SERVICE_UUID));
  if (pService == nullptr) {
    Serial.println("[GATT] Error: servicio no encontrado.");
    pClient->disconnect();
    delete pClient;
    return;
  }

  BLERemoteCharacteristic* pChar = pService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
  if (pChar == nullptr) {
    Serial.println("[GATT] Error: característica no encontrada.");
    pClient->disconnect();
    delete pClient;
    return;
  }

  if (pChar->canRead()) {
    std::string valor  = pChar->readValue();
    String      humedad = String(valor.c_str());

    Serial.println("╔══════════════════════════════╗");
    Serial.printf( "║  HUMEDAD RECIBIDA: %s%%\n", humedad.c_str());
    Serial.printf( "║  MAC: %s\n", macDelSensorActual.c_str());
    Serial.printf( "║  RSSI: %d dBm\n", rssiActual);
    Serial.println("╚══════════════════════════════╝");

    // Aquí podés agregar: MQTT, SD card, pantalla, etc.
  }

  pClient->disconnect();
  delete pClient;
  Serial.println("[GATT] Desconectado. Volviendo a escanear...\n");
}

// ─────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Gateway ESP32-S3 Iniciando ===");

  // Leer MAC guardada previamente en Flash
  preferences.begin("gw_conf", true);
  macRegistrada = preferences.getString("mac_sensor", "");
  preferences.end();

  if (macRegistrada != "") {
    haySensorVinculado = true;
    Serial.printf("[NVS] Sensor vinculado: %s\n", macRegistrada.c_str());
  } else {
    Serial.printf("[NVS] Sin sensor vinculado. Umbral: %d dBm\n", UMBRAL_PROXIMIDAD);
    Serial.println("[NVS] Acercá el sensor para vincularlo por primera vez.");
  }

  BLEDevice::init("Gateway_S3");
}

// ─────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────
void loop() {
  sensorEncontrado    = false;
  dispositivoObjetivo = nullptr;

  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new ScanCallbacks(), true);
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->start(5, false);
  pScan->clearResults();

  if (sensorEncontrado && dispositivoObjetivo != nullptr) {
    delay(100);
    conectarYLeerSensor(dispositivoObjetivo);
    delete dispositivoObjetivo;
    dispositivoObjetivo = nullptr;
  }

  delay(500);
}