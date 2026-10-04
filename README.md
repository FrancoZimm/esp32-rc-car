# Coche RC con ESP32

Coche de dos ruedas motrices controlado por **Bluetooth** desde el móvil, con un sensor **HC-SR04** que frena solo cuando tiene algo delante.

Se maneja con una app hecha en **MIT App Inventor** o con cualquier mando/app Bluetooth que envíe los comandos de abajo (las apps tipo *"Bluetooth RC Car"* de Android funcionan tal cual).

---

## Cómo funciona

- El ESP32 crea un Bluetooth clásico llamado **`CocheFranco`**. El móvil se empareja y manda letras: `F` adelante, `B` atrás, `S` parar…
- Cada 60 ms mide la distancia de frente:
  - **menos de 35 cm** → si va hacia delante, baja la velocidad a la mitad
  - **menos de 20 cm** → no deja avanzar y pita; **sí** deja ir marcha atrás y girar para salir
- Si se pierde la conexión, el coche **se para**.
- También acepta los mismos comandos por USB (Monitor Serie a 115200), útil para probar sin móvil.

---

## Material

| Componente | Cantidad |
|---|---|
| ESP32 Dev Module | 1 |
| Driver de motores L298N (o similar) | 1 |
| Motores DC con reductora + ruedas | 2 |
| Rueda loca | 1 |
| Sensor ultrasónico HC-SR04 | 1 |
| Resistencias 1 kΩ y 2 kΩ (divisor para el ECHO) | 1 + 1 |
| Buzzer activo (opcional) | 1 |
| Batería 7,4 V (2× 18650) o portapilas | 1 |

---

## Conexiones

| ESP32 | Va a | Nota |
|---|---|---|
| GPIO 14 | L298N **ENA** | quitar el jumper de ENA |
| GPIO 27 | L298N **IN1** | |
| GPIO 26 | L298N **IN2** | |
| GPIO 32 | L298N **ENB** | quitar el jumper de ENB |
| GPIO 25 | L298N **IN3** | |
| GPIO 33 | L298N **IN4** | |
| GPIO 4  | HC-SR04 **TRIG** | |
| GPIO 18 | HC-SR04 **ECHO** | **a través del divisor** 1 kΩ / 2 kΩ (el ECHO da 5 V) |
| GPIO 23 | Buzzer (+) | opcional |
| GND | GND del L298N, del HC-SR04 y de la batería | **masa común** |
| VIN | 5 V del L298N | o alimentar el ESP32 por USB |

> Si un motor gira al revés, intercambia sus dos cables en el L298N (o IN1↔IN2 / IN3↔IN4 en el código).

---

## Comandos

| Carácter | Acción |
|---|---|
| `F` `B` `L` `R` | adelante, atrás, girar izquierda, girar derecha |
| `G` `I` | adelante-izquierda, adelante-derecha |
| `H` `J` | atrás-izquierda, atrás-derecha |
| `S` | parar |
| `0` … `9`, `q` | velocidad (de mínima a máxima) |
| `V` / `v` | bocina on / off |
| `D` | el coche responde con la distancia en cm |

---

## App con MIT App Inventor

1. **Diseñador**: un `ListPicker` (para elegir el coche), botones *Adelante, Atrás, Izquierda, Derecha*, un `Slider` de 0 a 9 para la velocidad y un componente `BluetoothClient`.
2. **ListPicker.BeforePicking** → `Elements` = `BluetoothClient.AddressesAndNames`.
3. **ListPicker.AfterPicking** → `BluetoothClient.Connect(ListPicker.Selection)`.
4. Para cada botón:
   - `TouchDown` → `BluetoothClient.SendText("F")` (o `B`, `L`, `R`)
   - `TouchUp` → `BluetoothClient.SendText("S")`

   Así el coche solo se mueve mientras mantienes pulsado.
5. **Slider.PositionChanged** → `SendText` con el número redondeado (0–9).

El móvil tiene que estar **emparejado** antes con `CocheFranco` desde los ajustes de Bluetooth.

---

## Subir el código

1. Arduino IDE → instalar las placas **esp32 by Espressif** (Gestor de placas).
2. Placa: **ESP32 Dev Module**.
3. Abrir `esp32_rc_car/esp32_rc_car.ino` y subir.

Funciona con el core de ESP32 2.x y 3.x. No necesita librerías externas.

Los valores que más se suelen tocar están al principio del archivo: `NOMBRE_BT`, `DISTANCIA_FRENO`, `DISTANCIA_AVISO` y `VELOCIDAD_MIN`.

---

## Próximos pasos

- Fotos y vídeo del coche montado
- Segunda versión del chasis
- Modo autónomo (que esquive obstáculos solo)

---

Hecho por **Franco Zimmermann** · [@FrancoZimm](https://github.com/FrancoZimm)
