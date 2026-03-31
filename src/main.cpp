#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

typedef struct struct_message {
  int humedad;
} struct_message;

struct_message datosRecibidos;



#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t *incomingData, int len) {
  int rssiReal = info->rx_ctrl->rssi; //da la potencia - RSSI es literal la funcion que apunta a eso
#else
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
                                                          //aca retrocedo 25 pos. hacia atras en la memoria
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)(incomingData - 25);
  int rssiReal = pkt->rx_ctrl.rssi;
#endif
//memcpy: Copia los bytes "crudos" que llegaron por aire a tu estructura datosRecibidos.
  memcpy(&datosRecibidos, incomingData, sizeof(datosRecibidos));
  //TERMINAL
  Serial.println("\n----------------------------");
  Serial.printf("Humedad recibida: %d%%\n", datosRecibidos.humedad);
  Serial.printf("Potencia (RSSI): %d dBm\n", rssiReal);
  Serial.println("----------------------------");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) return;
  //La linea siguiente es basicamente una interrupcion
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("Gateway S3 listo. Versión auto-detectada.");
}

void loop() {/*se activa la interrupcion, no es necesario estar buscando constantemente el paquete*/}