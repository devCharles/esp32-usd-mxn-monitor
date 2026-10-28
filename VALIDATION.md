# Verificación de la instalación

Fecha local: 27 de octubre de 2026. Placa conectada por `/dev/cu.usbmodem101`.

## Orientación automática y diseño adaptable v1.5

- Cuatro orientaciones, con diseño vertical de 240 × 284 y horizontal de 284 × 240. Gráfica, precio, encabezado y barra de batería usan las dimensiones de la orientación activa.
- El acelerómetro requiere una posición estable durante 700 ms, además del filtrado. Se rechazan sacudidas, diagonales y posiciones planas; al acostar el dispositivo se conserva la orientación anterior. Girar no despierta la pantalla ni consulta datos.
- Ejes calibrados en la placa con posiciones indicadas por el usuario: vertical original ≈ `(0.97, 0.20, -0.01)` g; giro horario de 90° ≈ `(0.04, -0.92, -0.17)` g. El segundo caso produce una rotación de imagen de 270°.
- Pruebas de lógica y renderizado aprobadas: estabilidad, movimiento breve, posición plana, diagonal, sacudida, desbordamiento de temporizador, mapeo de ejes y todos los píxeles en las cuatro rotaciones, incluidos límites y última franja parcial.
- Compilación correcta: 1,263,644 bytes de programa y 69,380 bytes de variables globales. Carga USB con hash verificado.
- Comprobación real después de cargar: cotización HTTPS correcta, eventos de las cuatro orientaciones (0°, 90°, 180° y 270°) registrados en `build/orientation-verification.log` y captura horizontal de la placa revisada en `build/device-landscape.png`, sin recortes ni superposición de elementos. El usuario confirmó que se ve bien y funciona correctamente al probar las cuatro posiciones y dejarlo acostado.

## Ocultar batería llena con USB v1.4

- La barra y su fondo gris se ocultan si el AXP2101 detecta USB conectado y reporta exactamente 100 %. La animación también se detiene en ese caso.
- Cambios de conexión o porcentaje provocan un redibujado completo, retirando/restaurando la barra sin residuos. Se conserva el modo DIM.
- Revisión visual de conectado/desconectado al 100 % con el renderizador compartido. Firmware compilado (1,262,216 bytes) y cargado con hash verificado.
- Comprobación real: `battery=100 charging=0 usb=1 bar=0 mode=DIM brightness=1`, consulta HTTPS exitosa y captura USB `build/device-v1.4.png` sin barra ni fondo gris.

## Barra de carga animada v1.3

- Carga activa: barra azul con destello suave de izquierda a derecha cada 2.4 segundos, limitado al ancho del porcentaje. El azul también tiene prioridad con batería baja.
- Se actualizan únicamente las tres filas de la barra a 25 fps; la animación se detiene con la pantalla apagada y no altera el brillo de DIM. El AXP2101 se consulta cada segundo para detectar inicio/fin de carga.
- Revisión visual del renderizador real a 500, 1200 y 1900 ms: desplazamiento del brillo confirmado; también se revisó carga al 15 %. Imagen de revisión: `build/charging-review.png`.
- Firmware compilado (1,262,108 bytes), cargado con hash verificado y arranque v1.3 confirmado en modo DIM, brillo 1.
- Durante la comprobación USB, el AXP2101 reportó 100 % y `charging=0`. Por ello la barra permanece verde en ese estado; la animación durante carga activa se verificó en el renderizador, no en una batería físicamente cargándose en esta prueba.

## Tercer modo de pantalla v1.2

- La doble pulsación recorre ON → AUTO → DIM → ON. DIM mantiene permanentemente el PWM mínimo no nulo (1 de 255), también durante actualizaciones.
- Se guarda el modo con una clave nueva; la configuración anterior ON/AUTO se migra sin perder la selección. Al arrancar se aplica directamente el brillo del modo guardado.
- Pruebas aprobadas: ciclo completo de tres modos, brillo mínimo con actividad reciente e inactividad prolongada, temporizadores de AUTO y lógica anterior de controles/datos.
- Compilación: 1,261,568 bytes de programa; carga USB con hash verificado. Arranque v1.2, migración de AUTO y cotización HTTPS confirmados.
- Prueba física: dobles pulsaciones registraron DIM → ON → AUTO → DIM; el usuario confirmó que el brillo mínimo es adecuado. Reinicio por USB confirmado con `mode=DIM brightness=1`. Se dejó DIM seleccionado.

## Actualización de controles v1.1

- Toque: únicamente despierta la pantalla apagada; se eliminó la llamada a actualización de datos del controlador táctil.
- PWR simple: consulta datos tras esperar la ventana de doble pulsación de 450 ms.
- PWR doble: alterna ON/AUTO sin consultar datos.
- Se mantienen temporizador, sacudida, confirmación visual y límite de 15 segundos entre consultas manuales.
- Pruebas de lógica aprobadas: pulsación simple, doble sin actualización adicional, rebotes, límite de la ventana, pulsaciones separadas, triple pulsación y desbordamiento de `millis()`.
- Firmware compilado (1,261,454 bytes) y cargado con verificación de hash. Arranque v1.1 y consulta HTTPS exitosa confirmados.
- Eventos físicos observados: `BUTTON single` seguido de `trigger=button` y cotización exitosa; `BUTTON double` seguido de `MODE always-on`, y otro doble seguido de `MODE auto`, sin consultas adicionales asociadas a los dobles.
- Despertar por toque confirmado: `DISPLAY brightness=0`, `TOUCH wake`, `DISPLAY brightness=170`, sin iniciar actualización. El usuario confirmó que todos los nuevos controles funcionan como esperaba.

## Pruebas de la primera versión

## Completado

- Pruebas de escritorio: validación de cotización, descarte de respuestas inválidas, historial con valores nulos, fechas futuras, temporizadores y detección de sacudida con rechazo de golpes aislados.
- Revisión visual del renderizador compartido: cotización, actualización en curso, error de conexión y arranque sin datos.
- Compilación ESP32-C6 con Arduino 3.3.4: 1,261,222 bytes de programa, 56,204 bytes de variables globales, 271,476 bytes disponibles para variables locales.
- Carga USB: esptool confirmó `Hash of data verified`.
- Detección real: PMU AXP2101, táctil CST816D, IMU QMI8658 y RTC PCF85063 respondieron.
- Wi-Fi conectado y consulta HTTPS validada: se recibieron cotizaciones reales de Yahoo con 99 muestras. Ejemplos observados durante la prueba: 18.6817 y 18.6780 MXN por USD.
- Batería reportada por el AXP2101: 100 %. Se observaron estados de carga activa y carga finalizada; no se trata de una estimación manual por voltaje.
- Captura por USB del renderizador ejecutándose en la placa: `build/device.png`.
- Modo AUTO: registros de brillo 22 al atenuar y 0 al apagar la retroiluminación; vuelta a ON con brillo 170.
- Reinicio físico por USB sin borrar flash: `CLOCK restored from RTC`, hora válida y nueva cotización HTTPS correcta. El modo ON persistió.
- Interacción real registrada durante la comprobación del usuario: `trigger=touch` seguido de cotización exitosa; `trigger=shake` seguido de cotización exitosa; pulsación física PWR produjo `MODE auto`. El modo final elegido durante esta prueba quedó en AUTO.

## Ajustes realizados durante la prueba

- Arduino distribuyó un ejecutable `ctags` Intel incompatible con esta Mac sin Rosetta. Se compiló el mismo release para Apple Silicon. Reproducción: `tools/native_ctags.sh`.
- NTP no respondió en el primer arranque. Se añadió inicialización con la hora de compilación, persistencia de hora en RTC y ajuste mediante la cabecera Date de HTTPS autenticado. Se mantiene la validación TLS.

## Alcance de la revisión visual

El usuario confirmó que la apariencia y los tres controles de la primera versión funcionaban correctamente. La captura USB y los registros de eventos respaldan esa comprobación. La actualización v1.1 cambia las acciones de toque y PWR como se documenta arriba.

Los registros de compilación, carga, arranque, consulta, modo AUTO y reinicio están en `build/` (excluido de Git junto con los binarios y las credenciales).
