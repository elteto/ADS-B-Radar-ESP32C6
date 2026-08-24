# ADS-B Radar ESP32-C6 1.47"

Radar de vuelos cercanos para la placa **Waveshare ESP32-C6-LCD-1.47**.

El proyecto consulta tráfico ADS-B por Internet, muestra los aviones cercanos en un radar gráfico y calcula una estimación de qué aeronave va a pasar más cerca de la ubicación configurada y en cuánto tiempo ocurrirá esa máxima aproximación.

La configuración de Wi-Fi, coordenadas y radio base se realiza desde un **portal cautivo**, por lo que no es necesario guardar claves ni ubicación dentro del código fuente.

## Funciones principales

- Radar gráfico en pantalla **ST7789 de 1.47"** en orientación horizontal.
- Consulta de aeronaves cercanas mediante **Airplanes.live**.
- Cálculo local de distancia, posición relativa, rumbo, altitud y velocidad.
- Cálculo de **máxima aproximación (CPA)** usando posición, `track` y velocidad actuales.
- Estimación del **tiempo hasta el punto de máxima aproximación**.
- El avión destacado en amarillo es el que proyecta pasar más cerca; si ninguno se aproxima, se muestra el más cercano actual.
- Indicador `PASA CERCA` cuando la trayectoria proyectada pasa a menos de 10 km del radar.
- Línea de trayectoria sobre el avión destacado.
- Consulta de **origen y destino** del vuelo destacado mediante **ADSBDB**.
- Hasta **3 redes Wi-Fi configurables** desde el portal cautivo.
- Latitud, longitud y radio base configurables sin recompilar.
- Configuración persistente mediante `Preferences`.
- Botón **BOOT** para búsquedas manuales por rangos.
- Mantener **BOOT durante 3 segundos al encender** para abrir el portal de configuración.
- LED RGB integrado como alerta de nuevos vuelos.
- Apagado automático del backlight cuando no hay vuelos en el radio base.
- Búsqueda inicial automática en rangos crecientes.

## Portal cautivo de configuración

En el primer arranque el ESP32 crea automáticamente el punto de acceso:

```text
ADS-B-Radar-Setup
```

La pantalla muestra también la dirección:

```text
192.168.4.1
```

Conectate a esa red desde el teléfono o una computadora. El DNS cautivo intenta abrir automáticamente la página de configuración. Si no ocurre, ingresá manualmente:

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

Al presionar **Guardar y reiniciar**, los datos se almacenan en la flash del ESP32.

### Volver a abrir el portal

Para cambiar Wi-Fi, ubicación o radio:

1. reiniciá el ESP32;
2. mantené presionado **BOOT** durante aproximadamente 3 segundos al arrancar;
3. la pantalla mostrará `MODO CONFIG`;
4. conectate a `ADS-B-Radar-Setup`.

Si ninguna de las redes guardadas está disponible, el dispositivo también entra automáticamente al portal cautivo.

## Seguridad de las credenciales

El repositorio **no contiene claves Wi-Fi reales**.

Las credenciales se guardan localmente en NVS/flash mediante `Preferences`. No es necesario escribir SSID, contraseña, latitud ni longitud dentro del `.ino`.

## Comportamiento del radar

### Radio base

El radio base se define desde el portal cautivo.

Solamente los vuelos detectados dentro de ese radio mantienen la pantalla encendida de forma permanente.

Por ejemplo, `15 NM` son aproximadamente `28 km`.

El modo normal consulta la API cada 30 segundos.

### Búsqueda inicial

Después de conectarse al Wi-Fi, el dispositivo realiza una búsqueda escalonada.

Con un radio base de `15 NM`, los niveles son:

1. 15 NM
2. 30 NM
3. 60 NM
4. 120 NM
5. 240 NM

Se detiene en el **primer rango donde encuentra al menos una aeronave**.

Si encuentra vuelos, muestra ese radar durante **60 segundos**. Después vuelve al modo normal usando exclusivamente el radio base.

Si no encuentra ningún vuelo, apaga la pantalla y continúa funcionando en segundo plano.

El radio consultado nunca supera los `250 NM`.

### Pantalla apagada

Cuando no hay aviones dentro del radio base:

- el backlight se apaga;
- el LED RGB se apaga;
- el ESP32 sigue conectado al Wi-Fi;
- continúa consultando vuelos cada 30 segundos.

En cuanto aparece una aeronave dentro del radio base, la pantalla se enciende automáticamente.

## Botón BOOT

Una vez terminado el modo inicial, si no hay vuelos dentro del radio base, **BOOT** permite explorar radios progresivamente mayores.

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

Los vuelos encontrados fuera del radio base no mantienen la pantalla encendida cuando termina el modo manual.

BOOT tiene además una segunda función:

- pulsación normal durante el uso: ampliar el rango manual;
- mantenerlo presionado unos 3 segundos al arrancar: abrir el portal cautivo.

## Alerta RGB

Cuando el radar pasa de cero vuelos a detectar al menos uno dentro del radio base:

- la pantalla se enciende;
- el LED RGB parpadea en rojo;
- la alerta dura 30 segundos.

Después el LED se apaga, pero la pantalla permanece encendida mientras haya vuelos dentro del radio base.

## Predicción de paso y máxima aproximación

Para cada aeronave se proyecta su movimiento suponiendo que mantiene el **rumbo (`track`) y la velocidad actuales**.

El ESP32 transforma la posición relativa del avión en un plano local Este/Norte y calcula el **Closest Point of Approach (CPA)**: el punto futuro en el que la distancia entre el avión y el radar sería mínima.

A partir de eso obtiene:

- distancia actual;
- distancia mínima proyectada;
- minutos hasta esa máxima aproximación;
- si el avión todavía se está acercando o ya se aleja.

El avión destacado en amarillo es el que tiene la **menor distancia mínima proyectada**, aunque en ese momento no sea el avión físicamente más cercano.

Si la mínima proyectada es de `10 km` o menos, la pantalla muestra:

```text
PASA CERCA
4m
MIN
3.2km
```

Si pasará más lejos pero todavía se aproxima, muestra:

```text
MAX APROX
8m
MIN
24.6km
```

Si el punto de máxima aproximación ya pasó, muestra `SE ALEJA`.

La predicción se limita a un horizonte de 2 horas.

### Limitación de la estimación

No es un plan de vuelo ni una predicción aeronáutica oficial. Es una extrapolación geométrica basada en el estado ADS-B actual.

Si el avión cambia de rumbo o velocidad, el cálculo cambia en la siguiente actualización. En vuelos de crucero suele ser una estimación útil; cerca de aeropuertos, durante ascensos, descensos o virajes, puede variar rápidamente.

## Información mostrada

Para el avión destacado se muestra:

- callsign;
- origen y destino;
- distancia actual;
- tiempo estimado hasta máxima aproximación;
- distancia mínima proyectada.

Cuando no existe una proyección válida se muestran altitud y velocidad como información alternativa.

Ejemplo:

```text
AZU2080
AEP>SLA

AHORA
42.1km

PASA CERCA
4m

MIN
3.2km
```

En la mitad izquierda se muestra un radar circular con:

- Norte, Sur, Este y Oeste;
- posición del observador en el centro;
- posición relativa de las aeronaves;
- orientación aproximada según `track`;
- callsigns de los primeros vuelos;
- avión proyectado a pasar más cerca resaltado en amarillo;
- pequeña línea indicando su dirección de movimiento.

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

También se utilizan librerías del core ESP32:

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

Se utiliza para intentar obtener la ruta asociada al callsign del avión destacado y mostrar aeropuerto de origen y destino.

La ruta puede no estar disponible para todos los vuelos.

## Archivo principal

```text
ADS-B-Radar-ESP32C6.ino
```

Abrilo en Arduino IDE, instalá las librerías requeridas, compilá y cargalo. La configuración se realiza después desde el portal cautivo.

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
                  +-- calcula CPA de los vuelos
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
- La calidad de los datos depende de la cobertura ADS-B disponible.
- Origen y destino pueden faltar o estar desactualizados.
- El límite de búsqueda es 250 NM.
- Las credenciales se guardan en la flash local del dispositivo, no en GitHub.
- La estimación de máxima aproximación supone rumbo y velocidad constantes.

## Estado

Proyecto en desarrollo.

Posibles mejoras futuras:

- selección de ubicación usando el navegador del teléfono;
- página de estado del radar dentro del portal;
- botón para borrar configuración;
- caché de rutas más completa;
- mostrar tipo de aeronave;
- hora de última actualización;
- intensidad de señal Wi-Fi;
- filtros por altitud o distancia;
- dibujar la trayectoria proyectada completa en el radar.
