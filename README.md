# ADS-B Radar ESP32-C6 1.47"

Radar de vuelos cercano para la placa **Waveshare ESP32-C6-LCD-1.47**.

El proyecto consulta datos ADS-B por Internet, muestra los vuelos cercanos en un radar gráfico y presenta información del avión más próximo directamente en la pantalla integrada.

La configuración de Wi-Fi, coordenadas y radio base se realiza desde un **portal cautivo**, por lo que no es necesario guardar claves ni ubicación dentro del código fuente.

## Funciones principales

- Radar gráfico en pantalla **ST7789 de 1.47"** en orientación horizontal.
- Consulta de aeronaves cercanas mediante **Airplanes.live**.
- Cálculo local de distancia, posición relativa, rumbo, altitud y velocidad.
- El avión más cercano se destaca en amarillo.
- Callsign junto a los vuelos más próximos.
- Consulta de **origen y destino** del vuelo más cercano mediante **ADSBDB**.
- Hasta **3 redes Wi-Fi configurables** desde el portal cautivo.
- Latitud y longitud configurables sin recompilar.
- Radio base configurable desde el navegador.
- Configuración persistente en la memoria flash del ESP32 mediante `Preferences`.
- Botón **BOOT** para búsquedas manuales por rangos.
- Mantener **BOOT durante 3 segundos al encender** para volver a abrir el portal de configuración.
- LED RGB integrado como alerta de nuevos vuelos.
- Apagado automático del backlight cuando no hay vuelos en el radio base.
- Búsqueda inicial automática en rangos crecientes.

## Portal cautivo de configuración

En el primer arranque, como todavía no hay una red Wi-Fi configurada, el ESP32 crea automáticamente el punto de acceso:

```text
ADS-B-Radar-Setup
```

La pantalla muestra el nombre de la red y la dirección:

```text
192.168.4.1
```

Conectate a `ADS-B-Radar-Setup` desde el teléfono o una computadora. El sistema utiliza DNS cautivo para intentar abrir automáticamente la página de configuración. Si el navegador no la abre solo, ingresá manualmente:

```text
http://192.168.4.1
```

Desde allí se puede configurar:

- Wi-Fi principal y contraseña;
- segunda red Wi-Fi opcional;
- tercera red Wi-Fi / hotspot opcional;
- latitud del radar;
- longitud del radar;
- radio base en millas náuticas.

Al presionar **Guardar y reiniciar**, los datos se almacenan en la memoria flash del ESP32 y el equipo se reinicia.

### Volver a abrir el portal

Para cambiar Wi-Fi, ubicación o radio sin modificar el sketch:

1. apagá o reiniciá el ESP32;
2. mantené presionado **BOOT** durante aproximadamente 3 segundos mientras arranca;
3. la pantalla mostrará `MODO CONFIG`;
4. conectate nuevamente a `ADS-B-Radar-Setup`.

Si ninguna de las redes Wi-Fi guardadas está disponible, el dispositivo también entra automáticamente al portal cautivo.

## Seguridad de las credenciales

El repositorio **no contiene claves Wi-Fi reales**.

Las credenciales introducidas en el portal se guardan localmente en la NVS/flash del ESP32 mediante `Preferences`. No es necesario escribir SSID, contraseña, latitud ni longitud dentro del `.ino`.

## Comportamiento del radar

### Radio base

El radio base se define desde el portal cautivo.

Solamente los vuelos detectados dentro de este radio mantienen la pantalla encendida de manera permanente.

Por ejemplo, un radio base de `15 NM` equivale aproximadamente a `28 km`.

El radar normal consulta la API cada 30 segundos.

### Búsqueda inicial

Después de conectarse al Wi-Fi, el dispositivo realiza automáticamente una búsqueda escalonada.

Si el radio base configurado es `15 NM`, los niveles son:

1. 15 NM
2. 30 NM
3. 60 NM
4. 120 NM
5. 240 NM

Se detiene en el **primer rango donde encuentra al menos una aeronave**.

Si encuentra vuelos, muestra ese radar durante **60 segundos**. Luego vuelve automáticamente al funcionamiento normal usando exclusivamente el radio base.

Si no encuentra ningún vuelo en los rangos disponibles, apaga la pantalla y continúa funcionando en segundo plano.

El radio consultado nunca supera los `250 NM`.

### Pantalla apagada

Cuando no hay aviones dentro del radio base:

- el backlight se apaga;
- el LED RGB se apaga;
- el ESP32 sigue conectado al Wi-Fi;
- continúa consultando vuelos cada 30 segundos.

En cuanto aparece una aeronave dentro del radio base, la pantalla vuelve a encenderse automáticamente.

## Botón BOOT

Una vez terminado el modo de búsqueda inicial, si la pantalla está apagada y no hay vuelos dentro del radio base, **BOOT** permite explorar manualmente radios progresivamente mayores.

Con radio base de 15 NM:

| Pulsación | Radio |
|---|---:|
| 1 | 15 NM |
| 2 | 30 NM |
| 3 | 60 NM |
| 4 | 120 NM |
| 5 | 240 NM |
| 6 | vuelve a 15 NM |

Cada pulsación reinicia un temporizador de **15 segundos**.

Si hay aviones en el rango seleccionado, se muestran normalmente en el radar. Los vuelos encontrados fuera del radio base **no mantienen la pantalla encendida** cuando termina el modo manual.

### BOOT al encender

BOOT tiene una segunda función:

- pulsación normal durante el uso: ampliar el rango manual;
- mantenerlo presionado unos 3 segundos al arrancar: abrir el portal cautivo de configuración.

## Alerta RGB

Cuando el radar pasa de no tener vuelos a detectar al menos uno dentro del radio base:

- la pantalla se enciende;
- el LED RGB integrado parpadea en rojo;
- la alerta dura 30 segundos.

Después el LED se apaga, pero la pantalla permanece encendida mientras continúe habiendo vuelos dentro del radio base.

## Información mostrada

Para el vuelo más cercano se muestra:

- callsign;
- origen;
- destino;
- distancia;
- altitud;
- velocidad.

Ejemplo:

```text
AZU2080
AEP>SLA

DIST
18.4km

ALT
10320m

VEL
790kmh
```

En la mitad izquierda se muestra un radar circular con:

- Norte, Sur, Este y Oeste;
- posición del observador en el centro;
- posición relativa de las aeronaves;
- orientación aproximada según `track`;
- callsigns de los primeros vuelos.

## Hardware

Proyecto desarrollado para:

- **Waveshare ESP32-C6-LCD-1.47**
- ESP32-C6
- LCD ST7789 172×320
- botón BOOT en GPIO 9
- RGB integrado en GPIO 8

### Pines utilizados

| Función | GPIO |
|---|---:|
| LCD SCK | 7 |
| LCD MOSI | 6 |
| LCD CS | 14 |
| LCD DC | 15 |
| LCD RESET | 21 |
| Backlight | 22 |
| RGB LED | 8 |
| BOOT | 9 |

## Librerías necesarias

Instalar desde Arduino IDE:

- **Arduino_GFX**
- **ArduinoJson**

Las siguientes forman parte del core ESP32 utilizado por Arduino:

- `WiFi`
- `WiFiClientSecure`
- `HTTPClient`
- `WebServer`
- `DNSServer`
- `Preferences`

## Configuración de Arduino IDE

Configuración típica:

- Board: `ESP32C6 Dev Module`
- USB CDC On Boot: `Enabled`
- Serial Monitor: `115200 baud`

## APIs utilizadas

### Airplanes.live

Obtiene aeronaves cercanas según latitud, longitud y radio:

```text
https://api.airplanes.live/v2/point/LAT/LON/RADIO
```

### ADSBDB

Se utiliza para intentar obtener la ruta asociada al callsign del avión más cercano y mostrar aeropuerto de origen y destino.

La ruta puede no estar disponible para todos los vuelos.

## Archivo principal

```text
ADS-B-Radar-ESP32C6.ino
```

Abrilo en Arduino IDE, instalá las librerías requeridas, compilá y cargalo en la placa. La configuración del dispositivo se hace después desde el portal cautivo.

## Flujo de funcionamiento

```text
ENCENDIDO
   |
   +-- BOOT mantenido 3 s? -- SI --> Portal cautivo
   |
   +-- No hay configuracion? ------> Portal cautivo
   |
   +-- Intenta Wi-Fi 1 / 2 / 3
           |
           +-- ninguna conecta ----> Portal cautivo
           |
           +-- conectado
                  |
                  +-- busqueda inicial por rangos
                  |
                  +-- modo normal en radio base
                          |
                          +-- hay vuelos --> LCD ON
                          |
                          +-- no hay -----> LCD OFF
                                             |
                                             +-- BOOT --> busqueda manual
```

## Notas

- El proyecto necesita conexión a Internet.
- La calidad de los datos depende de la cobertura ADS-B disponible en las APIs.
- Origen y destino pueden faltar o estar desactualizados en algunos vuelos.
- El límite de búsqueda utilizado es 250 NM.
- Las credenciales se guardan en la flash local del dispositivo, no en GitHub.

## Estado

Proyecto en desarrollo.

Posibles mejoras futuras:

- selección de ubicación usando el navegador del teléfono;
- página de estado del radar dentro del portal;
- botón para borrar configuración;
- alternar automáticamente entre varios vuelos;
- caché de rutas más completa;
- mostrar tipo de aeronave;
- hora de última actualización;
- intensidad de señal Wi-Fi;
- filtros por altitud o distancia.
