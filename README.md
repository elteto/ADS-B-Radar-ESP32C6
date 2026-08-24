# ADS-B Radar ESP32-C6 1.47"

Radar de vuelos cercano para la placa **Waveshare ESP32-C6-LCD-1.47**.

El proyecto consulta datos ADS-B por Internet, muestra los vuelos cercanos en un pequeño radar gráfico y presenta información del avión más próximo directamente en la pantalla integrada.

## Funciones principales

- Radar gráfico en pantalla **ST7789 de 1.47"** en orientación horizontal.
- Consulta de aeronaves cercanas usando **Airplanes.live**.
- Cálculo local de distancia, posición relativa, rumbo, altitud y velocidad.
- El avión más cercano se destaca en amarillo.
- Visualización de callsign junto a los vuelos más próximos.
- Consulta de **origen y destino** del vuelo más cercano mediante **ADSBDB**.
- Soporte para múltiples redes Wi-Fi.
- Reconexión automática intentando las redes configuradas.
- Uso del botón **BOOT** como control de búsqueda manual.
- Uso del LED RGB integrado como alerta.
- Apagado automático del backlight cuando no hay vuelos próximos.

## Comportamiento del radar

### Radio base

El radar tiene un radio base configurable:

```cpp
const int BASE_RADIUS_NM = 15;
```

Solamente los vuelos detectados dentro de este radio mantienen la pantalla encendida de forma permanente.

Con el valor por defecto, `15 NM` son aproximadamente `28 km`.

El ESP32 actualiza la búsqueda normal cada 30 segundos.

### Búsqueda inicial

Al arrancar, el dispositivo realiza automáticamente una búsqueda escalonada:

1. 15 NM
2. 30 NM
3. 60 NM
4. 120 NM
5. 240 NM

Se detiene en el primer rango donde encuentra aeronaves.

Si encuentra vuelos, muestra el radar durante 60 segundos y después vuelve al modo normal de 15 NM.

Si no encuentra ningún vuelo, apaga la pantalla y continúa funcionando en segundo plano.

### Pantalla apagada

Cuando no hay aviones dentro del radio base:

- el backlight se apaga;
- el LED RGB se apaga;
- el ESP32 sigue conectado al Wi-Fi;
- continúa consultando vuelos cada 30 segundos.

En cuanto aparece un avión dentro del radio base, la pantalla vuelve a encenderse automáticamente.

## Botón BOOT

Cuando la pantalla está apagada, el botón **BOOT** permite explorar manualmente radios cada vez mayores.

| Pulsación | Radio |
|---|---:|
| 1 | 15 NM |
| 2 | 30 NM |
| 3 | 60 NM |
| 4 | 120 NM |
| 5 | 240 NM |
| 6 | vuelve a 15 NM |

Cada búsqueda manual mantiene la pantalla encendida durante 15 segundos.

Si hay aviones en el rango seleccionado, se muestran normalmente en el radar. Los vuelos encontrados fuera del radio base **no mantienen la pantalla encendida** después del timeout manual.

## Alerta RGB

Cuando el radar pasa de no tener vuelos a detectar al menos uno dentro del radio base:

- la pantalla se enciende;
- el LED RGB integrado comienza a parpadear en rojo;
- la alerta dura 30 segundos.

Después, el LED se apaga pero la pantalla continúa encendida mientras haya vuelos en el radio base.

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

En la mitad izquierda se muestra un radar circular con los puntos cardinales, la posición del observador en el centro, la posición relativa de las aeronaves, la orientación aproximada de cada avión según su rumbo y los callsigns de los primeros vuelos.

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

También se utilizan librerías incluidas en el core ESP32:

- `WiFi`
- `WiFiClientSecure`
- `HTTPClient`

## Configuración de Arduino IDE

Una configuración típica es:

- Board: `ESP32C6 Dev Module`
- USB CDC On Boot: `Enabled`
- Serial Monitor: `115200 baud`

## Configuración Wi-Fi

Por seguridad, el repositorio **no contiene claves Wi-Fi reales**.

Editá esta sección del `.ino`:

```cpp
WifiNetwork wifiNetworks[] = {
  { "TU_WIFI_1", "TU_CLAVE_1" },
  { "TU_WIFI_2", "TU_CLAVE_2" },
  { "TU_HOTSPOT", "TU_CLAVE_3" }
};
```

Podés agregar o quitar redes libremente. El dispositivo intentará conectarse a cada una hasta encontrar una disponible.

## Ubicación del radar

También hay que configurar la posición desde donde se observa el tráfico:

```cpp
const double MY_LAT = -31.380000;
const double MY_LON = -57.980000;
```

Cambiá esos valores por tu latitud y longitud.

## APIs utilizadas

### Airplanes.live

Se utiliza para obtener aeronaves cercanas según latitud, longitud y radio de búsqueda.

El proyecto consulta:

```text
https://api.airplanes.live/v2/point/LAT/LON/RADIO
```

### ADSBDB

Se utiliza para intentar obtener la ruta asociada al callsign del vuelo más cercano y mostrar códigos de aeropuerto de origen y destino cuando están disponibles.

La ruta puede no existir para todos los vuelos.

## Notas

- El proyecto depende de una conexión Wi-Fi con acceso a Internet.
- Los datos mostrados dependen de la cobertura ADS-B disponible en las APIs utilizadas.
- Origen y destino pueden faltar o estar desactualizados en determinados vuelos.
- El radio máximo utilizado por el proyecto es de 240 NM.

## Archivo principal

```text
ADS-B-Radar-ESP32C6.ino
```

Abrilo en Arduino IDE, configurá Wi-Fi y ubicación, compilá y cargalo en la placa.

## Estado

Proyecto en desarrollo.

Ideas para futuras versiones:

- alternar automáticamente entre varios vuelos;
- mostrar tipo de aeronave;
- mejorar iconos y orientación de los aviones;
- caché de rutas;
- configuración desde una interfaz web;
- hora de última actualización;
- intensidad de señal Wi-Fi;
- filtros por altitud o distancia.
