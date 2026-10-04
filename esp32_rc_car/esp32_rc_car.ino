/*
  Coche RC con ESP32 + Bluetooth
  ------------------------------
  - Placa: ESP32 Dev Module
  - Dos motores DC con driver L298N (o compatible)
  - Sensor ultrasónico HC-SR04 para no chocar de frente
  - Control desde una app de MIT App Inventor o cualquier
    mando/app Bluetooth que mande los mismos caracteres

  Autor: Franco Zimmermann (@FrancoZimm)
*/

#include <BluetoothSerial.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Bluetooth no está habilitado. Usa la placa 'ESP32 Dev Module'."
#endif

// ---------- Pines ----------
// Motor izquierdo (L298N: ENA, IN1, IN2)
const int PIN_ENA = 14;
const int PIN_IN1 = 27;
const int PIN_IN2 = 26;

// Motor derecho (L298N: ENB, IN3, IN4)
const int PIN_ENB = 32;
const int PIN_IN3 = 25;
const int PIN_IN4 = 33;

// HC-SR04 (ECHO va a 5 V: usar divisor de tensión 1 kΩ / 2 kΩ)
const int PIN_TRIG = 4;
const int PIN_ECHO = 18;

// Extras
const int PIN_BUZZER = 23;   // opcional, buzzer activo
const int PIN_LED    = 2;    // LED integrado de la placa

// ---------- Ajustes ----------
const char* NOMBRE_BT        = "CocheFranco";
const int   DISTANCIA_FRENO  = 20;   // cm: por debajo no deja avanzar
const int   DISTANCIA_AVISO  = 35;   // cm: por debajo va más despacio
const int   PWM_FREQ         = 1000; // Hz
const int   PWM_BITS         = 8;    // 0..255
const int   VELOCIDAD_MIN    = 90;   // por debajo los motores no arrancan
const unsigned long INTERVALO_SENSOR = 60;  // ms entre medidas

// ---------- Estado ----------
BluetoothSerial bt;

enum Movimiento { PARADO, ADELANTE, ATRAS, IZQUIERDA, DERECHA,
                  ADELANTE_IZQ, ADELANTE_DER, ATRAS_IZQ, ATRAS_DER };

Movimiento movimiento = PARADO;
int velocidad = 200;          // 0..255, se cambia con '0'..'9' y 'q'
int distancia = 999;          // última medida en cm
unsigned long ultimaMedida = 0;
bool estabaConectado = false;
bool bocina = false;          // 'V' / 'v' desde la app

// ---------- PWM (compatible con el core 2.x y 3.x) ----------
void prepararPWM(int pin, int canal) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(pin, PWM_FREQ, PWM_BITS);
#else
  ledcSetup(canal, PWM_FREQ, PWM_BITS);
  ledcAttachPin(pin, canal);
#endif
}

void escribirPWM(int pin, int canal, int valor) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, valor);
#else
  ledcWrite(canal, valor);
#endif
}

// ---------- Motores ----------
// potencia: -255 (atrás) .. 255 (adelante)
void motor(int pinA, int pinB, int pinPWM, int canal, int potencia) {
  potencia = constrain(potencia, -255, 255);
  digitalWrite(pinA, potencia > 0);
  digitalWrite(pinB, potencia < 0);
  escribirPWM(pinPWM, canal, abs(potencia));
}

void mover(int izq, int der) {
  motor(PIN_IN1, PIN_IN2, PIN_ENA, 0, izq);
  motor(PIN_IN3, PIN_IN4, PIN_ENB, 1, der);
}

void parar() {
  mover(0, 0);
}

bool vaHaciaDelante(Movimiento m) {
  return m == ADELANTE || m == ADELANTE_IZQ || m == ADELANTE_DER;
}

// Aplica el movimiento actual teniendo en cuenta el obstáculo
void aplicarMovimiento() {
  int v = velocidad;

  if (vaHaciaDelante(movimiento)) {
    if (distancia < DISTANCIA_FRENO) {
      parar();
      return;
    }
    if (distancia < DISTANCIA_AVISO) {
      v = max(VELOCIDAD_MIN, v / 2);
    }
  }

  int giro = v / 2;  // en las diagonales una rueda va más lenta

  switch (movimiento) {
    case ADELANTE:     mover( v,     v);     break;
    case ATRAS:        mover(-v,    -v);     break;
    case IZQUIERDA:    mover(-v,     v);     break;  // gira sobre sí mismo
    case DERECHA:      mover( v,    -v);     break;
    case ADELANTE_IZQ: mover( giro,  v);     break;
    case ADELANTE_DER: mover( v,     giro);  break;
    case ATRAS_IZQ:    mover(-giro, -v);     break;
    case ATRAS_DER:    mover(-v,    -giro);  break;
    default:           parar();              break;
  }
}

// ---------- Sensor ultrasónico ----------
int medirDistancia() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  // 25 ms de espera máxima ≈ 4 m; si no hay eco, damos el camino por libre
  unsigned long duracion = pulseIn(PIN_ECHO, HIGH, 25000);
  if (duracion == 0) return 999;
  return duracion / 58;  // µs -> cm
}

// ---------- Comandos ----------
// Mismo formato que las apps típicas de "Bluetooth RC Car":
//   F B L R       adelante, atrás, izquierda, derecha
//   G I H J       adelante-izq, adelante-der, atrás-izq, atrás-der
//   S             parar
//   0..9, q       velocidad (0 = mínima, q = máxima)
//   V / v         pitar / dejar de pitar
//   D             devuelve la distancia por Bluetooth
void procesarComando(char c) {
  switch (c) {
    case 'F': movimiento = ADELANTE;     break;
    case 'B': movimiento = ATRAS;        break;
    case 'L': movimiento = IZQUIERDA;    break;
    case 'R': movimiento = DERECHA;      break;
    case 'G': movimiento = ADELANTE_IZQ; break;
    case 'I': movimiento = ADELANTE_DER; break;
    case 'H': movimiento = ATRAS_IZQ;    break;
    case 'J': movimiento = ATRAS_DER;    break;
    case 'S': movimiento = PARADO;       break;

    case 'q': velocidad = 255; break;
    case 'V': bocina = true;  break;
    case 'v': bocina = false; break;
    case 'D': bt.println(distancia);          break;

    default:
      if (c >= '0' && c <= '9') {
        velocidad = map(c - '0', 0, 9, VELOCIDAD_MIN, 255);
      }
      return;  // caracteres desconocidos (\n, \r...) se ignoran
  }
}

// ---------- Programa ----------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  prepararPWM(PIN_ENA, 0);
  prepararPWM(PIN_ENB, 1);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED, OUTPUT);

  parar();
  bt.begin(NOMBRE_BT);
  Serial.printf("Listo. Busca '%s' en el Bluetooth del movil.\n", NOMBRE_BT);
}

void loop() {
  // 1. Comandos: por Bluetooth (app / mando) o por USB (pruebas desde el PC)
  while (bt.available())     procesarComando(bt.read());
  while (Serial.available()) procesarComando(Serial.read());

  // 2. Si se pierde la conexión, el coche se para
  bool conectado = bt.hasClient();
  if (estabaConectado && !conectado) {
    movimiento = PARADO;
    bocina = false;
    Serial.println("Conexion perdida: coche parado");
  }
  estabaConectado = conectado;
  digitalWrite(PIN_LED, conectado);

  // 3. Medir distancia cada cierto tiempo
  if (millis() - ultimaMedida >= INTERVALO_SENSOR) {
    ultimaMedida = millis();
    distancia = medirDistancia();

    // Pita si hay algo muy cerca y se intenta avanzar (o si se usa la bocina)
    bool bloqueado = vaHaciaDelante(movimiento) && distancia < DISTANCIA_FRENO;
    digitalWrite(PIN_BUZZER, bocina || bloqueado);
  }

  // 4. Mover según el comando y el obstáculo
  aplicarMovimiento();
}
