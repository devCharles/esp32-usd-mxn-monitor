# Monitor USD/MXN

Aplicación para la **Waveshare ESP32-C6-Touch-LCD-1.83**, pantalla ST7789 de 240 × 284, táctil CST816D, IMU QMI8658 y gestor de energía AXP2101.

## Uso

- El precio grande indica cuántos MXN vale **1 USD**.
- La gráfica muestra las últimas cinco **sesiones de mercado**, con muestras horarias. Omite los fines de semana sin negociación. El cambio en pesos y porcentaje se calcula contra la primera muestra de la gráfica; la línea punteada marca ese inicio.
- La consulta es automática cada cinco minutos. Una pulsación breve de **PWR** o una sacudida fuerte solicita una actualización y despierta la pantalla. Se muestran `Actualizando...`, `Actualizado` o `Sin cambios` como confirmación. Hay un intervalo mínimo de 15 segundos entre consultas para evitar ráfagas accidentales.
- Tocar la pantalla **solo la despierta cuando está apagada**. No consulta datos ni cambia el modo; si ya está encendida o atenuada, no hace nada.
- La pantalla se adapta automáticamente a las cuatro orientaciones: vertical (240 × 284), horizontal (284 × 240) y sus posiciones invertidas. Conserva el precio grande y adapta la anchura/altura de la gráfica. El sensor exige una posición estable durante unos 700 ms; sobre una mesa plana, en diagonal o durante una sacudida conserva la orientación anterior. Girar no consulta datos ni despierta por sí solo la pantalla apagada.
- Una **doble pulsación rápida de PWR** (hasta 450 ms entre eventos) recorre `ON → AUTO → DIM → ON`. `ON` permanece encendida al brillo normal; `AUTO` atenúa a los 30 segundos y apaga a los 60 segundos sin interacción; `DIM` permanece encendida al brillo mínimo (PWM 1 de 255). En DIM, las consultas y pulsaciones simples mantienen ese brillo mínimo. La pulsación simple espera esa ventana para distinguirla de una doble; la doble no actualiza datos. Se recuerda el modo tras reiniciar, incluso DIM. El apagado de pantalla no es suspensión profunda: el dispositivo sigue consultando y detectando toque/movimiento.
- La barra superior mide el porcentaje que reporta el AXP2101. Mientras hay carga activa y la barra está visible, es azul con un brillo que recorre de izquierda a derecha la parte llena cada 2.4 segundos, también con batería baja. Cuando está visible sin carga activa, es verde (o roja al 20 % o menos). El estado se comprueba cada segundo. Estar conectado por USB con la batería llena no implica carga activa. Un patrón gris indica que no hay una lectura válida; no se inventa un porcentaje.
- Si está conectado por USB y marca **100 %**, la barra se oculta completamente, incluido el fondo gris. Vuelve a aparecer al desconectarlo o bajar del 100 %. La conexión USB se lee por separado del estado de carga, para reconocer también la batería llena que ya no está cargándose.
- El texto junto a `5 DIAS` es la fecha/hora de la cotización, en horario de Ciudad de México. `Yahoo / HH:MM` indica la última consulta exitosa. Consultar no implica que el proveedor publique una nueva cotización en ese momento.
- Si falla la conexión, se conserva la última información y se muestra el error en ámbar. Se reintenta después de un minuto; si Yahoo devuelve HTTP 429, después de cinco minutos. Se recupera la última cotización guardada al reiniciar.

## Datos y batería

Se usa el endpoint público de gráficos de Yahoo Finance para `MXN=X` con `range=5d&interval=60m`. No necesita cuenta ni clave, pero no es una API con garantía de continuidad: puede cambiar o limitar solicitudes. La cotización de mercado puede diferir de Google (que identifica a Morningstar como proveedor de divisas). No se usa una tasa bancaria de compra/venta ni se reemplaza silenciosamente por una tasa diaria.

HTTPS valida la cadena del servidor mediante DigiCert Global Root G2. El reloj PCF85063 conserva la hora entre arranques; se inicializa con la hora de la computadora al compilar, y se ajusta con la cabecera `Date` de la respuesta HTTPS autenticada. También se solicita NTP en segundo plano. Esto permite funcionar en redes que bloquean NTP sin desactivar la validación TLS. Si se pierde por completo la alimentación del RTC y el firmware lleva mucho tiempo sin actualizarse, puede ser necesario recompilar/cargar para restablecer la hora. Se limita el tamaño de respuesta y se validan moneda, símbolo, valores y fechas antes de actualizar la pantalla.

Waveshare muestra en el contenido del paquete una batería **602525, 3.7 V, 400 mAh**. El porcentaje se lee con `getBatteryPercent()` del AXP2101 y puede requerir un ciclo de carga/descarga para calibrarse. La aplicación conserva la configuración existente de voltajes y corriente de carga, sin reprogramarla a partir de una capacidad supuesta.

Fuentes:

- [Hardware Waveshare](https://docs.waveshare.com/ESP32-C6-Touch-LCD-1.83)
- [Ejemplos oficiales](https://github.com/waveshareteam/ESP32-C6-Touch-LCD-1.83)
- [Paquete y batería](https://www.waveshare.com/esp32-c6-touch-lcd-1.83.htm), [etiqueta de batería](https://www.waveshare.com/img/devkit/general/battery-3.7V-400mAh.jpg)
- [USD/MXN en Yahoo](https://finance.yahoo.com/quote/MXN%3DX/)
- [Proveedor de datos de Google](https://www.google.com/googlefinance/disclaimer/)

## Compilar y cargar

Las herramientas y bibliotecas se guardan dentro de `.arduino/`, sin modificar la instalación global.

Versiones fijadas: ESP32 Arduino 3.3.4, GFX Library for Arduino 1.6.3, SensorLib 0.3.3, XPowersLib 0.3.0, ArduinoJson 7.4.2. Tipografía Inter, licencia OFL en `assets/fonts/OFL.txt`.

```sh
arduino-cli --config-file arduino-cli.yaml core update-index
arduino-cli --config-file arduino-cli.yaml core install esp32:esp32@3.3.4
arduino-cli --config-file arduino-cli.yaml lib install 'GFX Library for Arduino@1.6.3' 'SensorLib@0.3.3' 'XPowersLib@0.3.0' 'ArduinoJson@7.4.2'
```

En una copia nueva del repositorio, copiar `wifi.example.json` a `wifi.local.json` y completar `ssid` y `password`. Se requiere Wi-Fi de **2.4 GHz**. El archivo local, el encabezado generado `secrets.h` y los binarios quedan excluidos de Git. El firmware contiene las credenciales, por lo que tampoco deben compartirse sus binarios.

```sh
sh tools/build.sh
arduino-cli --config-file arduino-cli.yaml board list
sh tools/upload.sh /dev/cu.usbmodem101
```

El puerto puede cambiar al reconectar. `upload.sh` vuelve a compilar y solo carga si la compilación tuvo éxito. El archivo `arduino-cli.yaml` usa rutas absolutas de este proyecto; ajustar si se mueve a otra carpeta.

En esta Mac Apple Silicon fue necesario compilar el auxiliar `ctags` de Arduino de forma nativa: el distribuido por Arduino era Intel y no había Rosetta. Si se reinstalan las herramientas y aparece `bad CPU type in executable`, ejecutar `sh tools/native_ctags.sh`. El script usa la misma versión oficial 5.8-arduino11, verifica su SHA-256 y adapta un identificador antiguo que colisiona con el SDK actual de macOS.

## Verificación y diagnóstico

El renderizador de `Widget.h` es compartido por firmware y vista previa de escritorio. Renderiza en franjas de 20 filas (hasta 11.36 KB cada una), con otra franja para rotación por software. El controlador permanece en su modo nativo para evitar desplazamientos de imagen por la RAM sobrante del ST7789. La red corre en una tarea independiente; solo la tarea principal accede a I2C y a la pantalla.

```sh
c++ -std=c++17 -IUsdMxnMonitor -I.arduino/user/libraries/ArduinoJson/src tests/test_monitor.cpp -o build/test_monitor
build/test_monitor /ruta/a/respuesta-yahoo.json
c++ -std=c++17 -IUsdMxnMonitor -I.arduino/user/libraries/ArduinoJson/src tools/preview.cpp -o build/preview
build/preview /ruta/a/respuesta-yahoo.json build/preview.ppm
```

Las pruebas cubren respuestas incompletas, moneda equivocada, cotización nula, fechas futuras, historial no ordenado, temporizadores, controles y rechazo de golpes aislados. `tests/test_rotation.cpp` verifica la estabilidad de orientación, el rechazo de movimiento/posiciones ambiguas y la colocación de todos los píxeles en las cuatro rotaciones. Ejecutar todo con `sh tests/run.sh`. La vista previa permite `busy`, `offline`, `empty`, `charging`, `plugged-full` o `unplugged-full`; añadir `--landscape` para la disposición horizontal.

Por USB a 115200 baudios: `d` imprime diagnóstico sin credenciales, `a` muestra aceleración y orientación, `r` solicita actualizar, `m` alterna el modo de pantalla y `s` entrega una captura RGB565 de la interfaz con sus dimensiones actuales. `tools/serial.py --capture` guarda también un JSON con anchura y altura. Las acciones físicas de toque, sacudida, PWR y los giros se registran en serial para verificarlas en el equipo real.
