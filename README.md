# ADS-B Radar ESP32-C6 1.47"

Radar de vuelos cercanos para la placa **Waveshare ESP32-C6-LCD-1.47**.

El proyecto consulta tráfico ADS-B por Internet, muestra los aviones cercanos en un radar gráfico y calcula qué aeronave proyecta pasar más cerca de la ubicación configurada y en cuánto tiempo ocurrirá esa máxima aproximación.

La configuración de Wi-Fi, coordenadas y radio base se realiza desde un **portal cautivo**, por lo que no es necesario guardar claves ni ubicación dentro del código fuente.

## Funciones principales

- Radar gráfico en pantalla **ST7789 de 1.47"** en orientación horizontal.
- Consulta de aeronaves cercanas mediante **Airplanes.live**.
- Cálculo local de distancia, posición relativa, rumbo, altitud y velocidad.
- Cálculo de **máxima aproximación (CPA)** usando posición, `track` y velocidad actuales.
- Estimación del **tiempo hasta el punto de máxima aproximación**.
- El avión destacado inicialmente es el que proyecta pasar más cerca; si ninguno se aproxima, se selecciona el más cercano actual.
- Indicador `PASA CERCA` cuando la trayectoria proyectada pasa a menos de 10 km del radar.
- Consulta de **origen y destino** mediante **ADSBDB**.
- Panel derecho con **dos vistas que rotan automáticamente cada 10 segundos**.
- Vista clásica con `DISTANCIA`, `ALTURA` y `VELOCIDAD` claramente identificadas.
- Vista de proximidad con tiempo hasta la máxima aproximación y distancia mínima proyectada.
- Si hay varios vuelos, una pulsación de **BOOT cambia al siguiente avión**.
- Hasta **3 redes Wi-Fi configurables** desde el portal cautivo.
- Latitud, longitud y radio base configurables sin recompilar.
- Configuración persistente mediante `Preferences`.
- BOOT para búsquedas manuales por rangos cuando no hay vuelos.
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

El modo normal consulta la API cada **30 segundos**.

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

## Pantalla

La pantalla está dividida en dos sectores principales.

### Radar izquierdo

La mitad izquierda muestra:

- círculos concéntricos de distancia;
- Norte, Sur, Este y Oeste;
- posición del observador en el centro;
- posición relativa de las aeronaves;
- orientación aproximada de cada avión según su `track`;
- callsigns de los primeros vuelos;
- avión seleccionado resaltado en amarillo.

### Panel derecho

El panel derecho mantiene siempre arriba:

- callsign del avión seleccionado;
- origen y destino cuando ADSBDB dispone de la ruta.

Debajo, el contenido **alterna automáticamente cada 10 segundos** entre dos vistas.

#### Vista clásica

Muestra los datos de vuelo habituales, con una etiqueta pequeña que identifica claramente cada valor:

```text
DISTANCIA
18.4km

ALTURA
10360m

VELOCIDAD
842kmh
```

Los tres campos son:

- **DISTANCIA:** distancia actual desde el radar hasta el avión;
- **ALTURA:** altitud barométrica convertida a metros;
- **VELOCIDAD:** velocidad sobre el suelo convertida a km/h.

#### Vista de proximidad

Muestra la proyección de máxima aproximación con texto blanco y los valores principales destacados.

Si el avión proyecta pasar a menos de 10 km:

```text
PASA
CERCA

EN
4m

MIN DIST
3.2km
```

Si se aproxima pero pasará más lejos, se muestra `APROXIMACION`, el tiempo restante y la distancia mínima proyectada.

Si el punto de máxima aproximación ya quedó atrás:

```text
SE
ALEJA

DIST ACTUAL
18.4km
```

Si no hay datos suficientes para realizar la proyección se muestra `SIN PROY.`.

Cada vez que se reciben datos nuevos o se cambia manualmente de avión, el panel vuelve primero a la **vista clásica** y luego continúa alternando cada 10 segundos.

## Selección de vuelos con BOOT

El comportamiento de BOOT depende del estado del radar.

### Cuando hay más de un vuelo

Una pulsación corta de **BOOT** selecciona el siguiente avión disponible.

Por ejemplo, si hay tres vuelos:

```text
Vuelo 1 -> BOOT -> Vuelo 2 -> BOOT -> Vuelo 3 -> BOOT -> Vuelo 1
```

Al cambiar de avión:

- el nuevo avión queda resaltado en amarillo;
- el panel derecho muestra sus datos;
- se intenta obtener su origen y destino;
- la pantalla vuelve primero a la vista clásica;
- después continúa la rotación clásica/proximidad cada 10 segundos.

Esto permite recorrer manualmente todos los vuelos detectados sin esperar a que cambie automáticamente el avión destacado.

### Cuando no hay vuelos en el radio base

BOOT conserva la función de exploración manual de radios progresivamente mayores.

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

### BOOT al encender

Mantener BOOT presionado aproximadamente **3 segundos durante el arranque** abre el portal cautivo de configuración.

En resumen:

| Estado | Acción BOOT |
|---|---|
| Hay varios vuelos | cambia al siguiente avión |
| Hay un solo vuelo | mantiene el vuelo seleccionado |
| No hay vuelos | amplía progresivamente el rango de búsqueda |
| BOOT mantenido al arrancar | abre el portal cautivo |

## Alerta RGB

Cuando el radar pasa de cero vuelos a detectar al menos uno dentro del radio base:

- la pantalla se enciende;
- el LED RGB parpadea en rojo;
- la alerta dura **30 segundos**.

Después el LED se apaga, pero la pantalla permanece encendida mientras haya vuelos dentro del radio base.

## Predicción de paso y máxima aproximación

Para cada aeronave se proyecta su movimiento suponiendo que mantiene el **rumbo (`track`) y la velocidad actuales**.

El ESP32 transforma la posición relativa del avión en un plano local Este/Norte y calcula el **Closest Point of Approach (CPA)**: el punto futuro en el que la distancia entre el avión y el radar sería mínima.

A partir de eso obtiene:

- distancia actual;
- distancia mínima proyectada;
- minutos hasta esa máxima aproximación;
- si el avión todavía se está acercando o ya se aleja.

Inicialmente se destaca el avión con la **menor distancia mínima proyectada**, aunque en ese momento no sea el avión físicamente más cercano. Si no hay una proyección futura válida, se utiliza el avión actualmente más cercano.

La predicción se limita a un horizonte de **2 horas**.

### Limitación de la estimación

No es un plan de vuelo ni una predicción aeronáutica oficial. Es una extrapolación geométrica basada en el estado ADS-B actual.

Si el avión cambia de rumbo o velocidad, el cálculo cambia en la siguiente actualización. En vuelos de crucero suele ser una estimación útil; cerca de aeropuertos, durante ascensos, descensos o virajes, puede variar rápidamente.

## Origen y destino

Para el avión seleccionado se consulta ADSBDB utilizando el callsign.

Cuando la información está disponible se muestra, por ejemplo:

```text
AEP>SLA
```

Si no existe una ruta disponible, se muestra `---`.

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

Se utiliza para intentar obtener la ruta asociada al callsign del avión seleccionado y mostrar aeropuerto de origen y destino.

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
                          |      |
                          |      +-- vista clasica
                          |      |      |
                          |      |      +-- 10 s
                          |      |             |
                          |      +------> vista proximidad
                          |      |             |
                          |      |             +-- 10 s --> clasica
                          |      |
                          |      +-- BOOT --> siguiente vuelo
                          |
                          +-- no hay -----> LCD OFF
                                             |
                                             +-- BOOT --> busqueda manual
```

## Temporizadores principales

| Función | Tiempo |
|---|---:|
| Actualización de vuelos | 30 s |
| Rotación clásica / proximidad | 10 s |
| Pantalla de búsqueda manual | 15 s |
| Búsqueda inicial encontrada | 60 s |
| Parpadeo de alerta RGB | 30 s |
| BOOT para configuración al arrancar | 3 s |

## Notas

- El proyecto necesita conexión a Internet.
- La calidad de los datos depende de la cobertura ADS-B disponible.
- Origen y destino pueden faltar o estar desactualizados.
- El límite de búsqueda es 250 NM.
- Las credenciales se guardan en la flash local del dispositivo, no en GitHub.
- La estimación de máxima aproximación supone rumbo y velocidad constantes.
- Las unidades mostradas son kilómetros, metros y km/h, aunque el radio de búsqueda se configura en millas náuticas.

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
