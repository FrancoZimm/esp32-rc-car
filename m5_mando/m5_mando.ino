/*
  Mando del coche con M5Stack (MQTT)
  ----------------------------------
  - Placa: M5Stack Core (Basic / Gray / Fire), con los botones A, B y C
  - Se conecta al WiFi y publica el comando elegido en un topic MQTT
  - El coche (esp32_rc_car.ino) está suscrito a ese mismo topic

  Botones:
    A / C  -> cambiar de opción (anterior / siguiente)
    B      -> enviar el comando al coche

  Librerías: M5Unified, PubSubClient

  Autor: Franco Zimmermann (@FrancoZimm)
*/

#include <M5Unified.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ---------- Configuración ----------
const char* WIFI_SSID   = "TU_WIFI";
const char* WIFI_PASS   = "TU_PASSWORD";
const char* MQTT_BROKER = "broker.hivemq.com";   // o la IP de tu Mosquitto
const int   MQTT_PORT   = 1883;
const char* MQTT_TOPIC  = "francozimm/coche/cmd"; // el mismo que en el coche

// ---------- Comandos ----------
// Mismo número que entiende el coche por MQTT
struct Comando {
  const char* codigo;
  const char* nombre;
};

const Comando COMANDOS[] = {
  {"0", "PARAR"},
  {"1", "ADELANTE"},
  {"2", "ATRAS"},
  {"3", "IZQUIERDA"},
  {"4", "DERECHA"},
  {"5", "BOCINA"},
};
const int NUM_COMANDOS = sizeof(COMANDOS) / sizeof(COMANDOS[0]);

// ---------- Estado ----------
WiFiClient wifi;
PubSubClient mqtt(wifi);

int opcion = 1;          // empieza en ADELANTE
String ultimoEnviado = "";
bool hayError = false;
unsigned long ultimoIntento = 0;

// ---------- Pantalla ----------
void dibujar() {
  auto& d = M5.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextSize(2);
  d.setCursor(10, 10);

  d.setTextColor(TFT_WHITE);
  d.println("M5 -> MQTT -> COCHE");
  d.println("A/C: cambiar opcion");
  d.println("B: ENVIAR");
  d.println();

  d.println("CMD actual:");
  d.setTextColor(TFT_CYAN);
  d.printf("%s = %s\n\n", COMANDOS[opcion].codigo, COMANDOS[opcion].nombre);

  if (hayError) {
    d.setTextColor(TFT_RED);
    d.println("ERROR: sin conexion");
  } else if (ultimoEnviado.length() > 0) {
    d.setTextColor(TFT_GREEN);
    d.println("ENVIADO: " + ultimoEnviado);
  }

  // Estado de la conexión abajo del todo
  d.setTextSize(1);
  d.setCursor(10, d.height() - 14);
  d.setTextColor(mqtt.connected() ? TFT_GREEN : TFT_ORANGE);
  d.print(mqtt.connected() ? "MQTT conectado" : "Conectando...");
}

// ---------- Conexiones ----------
void conectarWiFi() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(10, 10);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextSize(2);
  M5.Display.println("Conectando WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    M5.Display.print(".");
  }
}

// Reintenta cada 2 s sin bloquear los botones
void mantenerMQTT() {
  if (mqtt.connected()) return;
  if (millis() - ultimoIntento < 2000) return;
  ultimoIntento = millis();

  String id = "m5-mando-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  if (mqtt.connect(id.c_str())) {
    hayError = false;
  }
  dibujar();
}

void enviar() {
  if (!mqtt.connected()) {
    hayError = true;
    dibujar();
    return;
  }
  const Comando& c = COMANDOS[opcion];
  hayError = !mqtt.publish(MQTT_TOPIC, c.codigo);
  if (!hayError) {
    ultimoEnviado = String(c.codigo) + " = " + c.nombre;
  }
  dibujar();
}

// ---------- Programa ----------
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);

  conectarWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mantenerMQTT();
  dibujar();
}

void loop() {
  M5.update();
  mantenerMQTT();
  mqtt.loop();

  if (M5.BtnA.wasPressed()) {
    opcion = (opcion - 1 + NUM_COMANDOS) % NUM_COMANDOS;
    dibujar();
  }
  if (M5.BtnC.wasPressed()) {
    opcion = (opcion + 1) % NUM_COMANDOS;
    dibujar();
  }
  if (M5.BtnB.wasPressed()) {
    enviar();
  }
}
