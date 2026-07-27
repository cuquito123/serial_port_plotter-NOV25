# Serial Port Plotter

Software de instrumentación en C++/Qt para el comando y monitoreo del **detector de fotones en coincidencias múltiples** desarrollado en el Centro de Investigaciones Ópticas (CIOp — CONICET / CIC-PBA / UNLP).

El detector está implementado sobre una placa **FPGA DE0 Nano SoC** (Altera/Intel) programada en VHDL, que se comunica con la PC por **UART/RS232**. La aplicación configura el experimento, transmite los parámetros al FPGA, grafica los conteos en tiempo real y registra la adquisición en disco.

**Versión actual:** 2.3.0 · **Plataforma:** Windows · **Rama de trabajo:** `fix-jul`

---

## Contexto

Desarrollado en el marco del proyecto *"Diseño y desarrollo de dispositivos ópticos para comunicaciones cuánticas aeroespaciales"* (PICT-2020-SERIE A-I-GRF), orientado a distribución cuántica de claves (QKD) como carga útil del Satélite Universitario de la UNLP (estándar CubeSat).

---

## Funcionalidad

- Configuración de canales de conteo y combinaciones de coincidencia mediante una matriz de 32 posiciones (4 filas A–D × 8 columnas).
- Ajuste de ancho de pulso y retardos independientes para los cuatro canales de entrada.
- Definición de la ventana temporal de integración y de la duración total del experimento.
- Visualización en tiempo real de conteos individuales y en coincidencia (QCustomPlot).
- Grabación en CSV con metadatos de experimento, más un HTML paralelo con formato.
- Perfiles de experimento persistidos en JSON.
- Supervisión de la salud de la comunicación y control previo a la ejecución.

### Hardware comandado

| Parámetro | Valor |
|---|---|
| Canales | 12 (4 de conteo individual, el resto para coincidencias de hasta 4 canales) |
| Matriz de configuración | 32 posiciones (4 × 8) |
| Ventana de integración | 5,6 ms – 99.999.999 unidades base |
| Ancho de pulso y retardos A–D | 0 – 255, normalizados a múltiplos de 8 por el FPGA |
| Enlace | UART/RS232, 115200 8N1 (típico) |

---

## Arquitectura

El proyecto es una refactorización de una `MainWindow` monolítica heredada, reorganizada en seis módulos:

| Módulo | Responsabilidad |
|---|---|
| `SerialPortManager` | Comunicación serie: apertura, cierre, lectura asíncrona, escritura no bloqueante |
| `SerialMessageParser` | Parseo por máquina de estados del protocolo `$…;`, con validación y descarte de tramas inválidas |
| `FpgaProtocol` | Construcción de paquetes, conversión de unidades de tiempo, etiquetas y mapeo de trama |
| `PlotManager` | Visualización en tiempo real sobre QCustomPlot, con throttling adaptativo (50 → 30 fps) |
| `CsvManager` | Exportación a CSV con metadatos y generación del HTML paralelo |
| `ProfileManager` | Persistencia de perfiles de experimento en JSON |

### Máquina de estados

`MainWindow` gobierna la disponibilidad de los controles mediante un `enum class AppState` con seis estados:

```
Disconnected → ReadyForConfiguration → ReadyForExecution → Acquiring ⇄ Paused
                                                                   ↓
                                                                 Fault
```

Las transiciones se validan formalmente en `canTransitionToState()`, invocada desde `setAppState()`. El estado `Fault` se alcanza cuando el control previo detecta una condición inválida o cuando la proporción de tramas inválidas supera el 25 % sostenido; se sale de él desconectando.

### Robustez

- **Control previo (`performPreflightCheck()`):** verifica puerto conectado, al menos un canal activo, valor de tiempo mayor que cero, ventana de integración dentro de rango y archivo CSV abierto cuando la grabación está habilitada. Si falla, no se transmite la configuración.
- **Telemetría de salud:** advertencia a partir del 10 % de tramas inválidas sostenido 5 s; paso a `Fault` a partir del 25 %.
- **Throttling adaptativo:** el gráfico baja de 50 a 30 cuadros por segundo si el repintado consume más de la mitad del intervalo.

---

## Requisitos de compilación

| Componente | Versión |
|---|---|
| Qt | 5.12.2 (módulos `serialport`, `printsupport`) |
| Compilador | MinGW 7.3.0 32-bit |
| Sistema | Windows |

> La ruta de trabajo **no debe contener espacios**: `mingw32-make` falla si los hay.

## Compilación

Abrir la consola *Qt 5.12.2 (MinGW 7.3.0 32-bit)* desde el menú Inicio (trae el `PATH` ya cargado):

```cmd
cd /d C:\ruta\al\serial_port_plotter
mkdir build-release
cd build-release
qmake ..\SerialPortPlotter.pro "CONFIG+=release"
mingw32-make -j4
```

El ejecutable resultante es `build-release\release\serial_port_plotter.exe`.

> El nombre del ejecutable (`serial_port_plotter.exe`) no coincide con el del archivo de proyecto (`SerialPortPlotter.pro`).

Tras cambios de código alcanza con volver a ejecutar `mingw32-make -j4`. Solo hace falta correr `qmake` de nuevo si se modifica el `.pro` o se agregan archivos.

## Empaquetado

Carpeta portable con las dependencias de Qt y del runtime de MinGW:

```cmd
mkdir C:\deploy
copy release\serial_port_plotter.exe C:\deploy
windeployqt --release C:\deploy\serial_port_plotter.exe
copy C:\Qt\Qt5.12.2\Tools\mingw730_32\bin\libgcc_s_dw2-1.dll C:\deploy
copy C:\Qt\Qt5.12.2\Tools\mingw730_32\bin\libstdc++-6.dll C:\deploy
copy C:\Qt\Qt5.12.2\Tools\mingw730_32\bin\libwinpthread-1.dll C:\deploy
copy ..\..\MANUAL_USUARIO.md C:\deploy
```

Verificar ejecutando desde un `cmd` limpio, sin el `PATH` de Qt cargado.

`MANUAL_USUARIO.md` debe acompañar al ejecutable: el menú *Ayuda → Manual de Usuario* lo lee del disco en tiempo de ejecución y no está embebido como recurso Qt.

## Instalador

`installer.iss` (Inno Setup) genera el instalador y copia el manual y las licencias al directorio de instalación, junto al `.exe`.

---

## Protocolo y formato de datos

### Trama serie

La aplicación espera tramas que comienzan con `$`, terminan con `;` y llevan los valores separados por espacios:

```
$valor1 valor2 … ;
```

Ejemplo de emisión desde el dispositivo:

```c
printf("$%d %d;", dato1, dato2);
```

El parser valida carácter a carácter y descarta las tramas que no cumplen el formato, contabilizándolas para la telemetría de salud. Los valores no numéricos se descartan durante el graficado y se registran como advertencia.

### Salida CSV

Cada archivo lleva un encabezado con los metadatos del experimento, incluida la versión de la aplicación. La primera columna es el **tiempo en segundos**. Junto al CSV se genera automáticamente un archivo `<nombre>_formato.html` con los mismos datos formateados, para inspección visual rápida.

Cuando la duración de experimento configurada es mayor que cero, la grabación es obligatoria: la aplicación no inicia la adquisición sin un CSV abierto.

### Perfiles

Archivos JSON en la carpeta de datos de la aplicación. Cada perfil guarda la matriz de canales activos, el ancho de pulso, los cuatro retardos, el valor y la unidad de tiempo, y la fecha de creación.

API estática de `ProfileManager`: `profilesDirectory()`, `profileNames()`, `profilePath()`, `saveProfile()`, `loadProfile()`, `deleteProfile()`, `renameProfile()`, `applyProfileToUi()`. La estructura `OperativeProfile` implementa `toJson()` / `fromJson()`.

---

## Flujo de trabajo

1. Conectar la placa FPGA y verificar el puerto COM asignado.
2. *Puerto Serial → Propiedades de Puerto…* para fijar puerto, baudios, bits de datos, paridad y bits de parada.
3. *Puerto Serial → Conectar*. La aplicación pasa a **Listo para configurar** y reinicia la matriz de canales.
4. Configurar la matriz, la columna a graficar y los parámetros temporales.
5. **Enviar Datos**: ejecuta el control previo, abre el CSV si corresponde, transmite la configuración al FPGA e inicia la adquisición.
6. *Pausa/Reanuda* detiene la adquisición sin cerrar el puerto y rehabilita los controles de configuración.
7. *Desconectar* cierra el puerto, detiene el cronómetro y cierra el archivo CSV.

### Atajos

| Atajo | Acción |
|---|---|
| `F1` | Ayuda incorporada |
| `Ctrl+S` | Activar / desactivar grabación en CSV |
| `Ctrl+Tab` | Alternar con el panel de configuración alternativo |
| `Ctrl+Q` | Salir |
| Reproducir / Pausa / Detener | Conectar, Pausa/Reanuda y Desconectar (teclas multimedia) |

---

## Estructura del repositorio

```
SerialPortPlotter.pro        Archivo de proyecto qmake
main.cpp                     Punto de entrada
mainwindow.{h,cpp,ui}        Ventana principal y máquina de estados
serialportmanager.{h,cpp}    Comunicación serie
serialmessageparser.{h,cpp}  Parseo del protocolo $…;
fpgaprotocol.{h,cpp}         Construcción de paquetes y conversiones
plotmanager.{h,cpp}          Visualización en tiempo real
csvmanager.{h,cpp}           Exportación CSV + HTML
profilemanager.{h,cpp}       Perfiles JSON
qcustomplot.{h,cpp}          Biblioteca de graficado (third-party)
installer.iss                Script de Inno Setup
MANUAL_USUARIO.md            Manual de usuario distribuido con el ejecutable
tools/build_manual.py        Generación del manual en HTML
```

---

## Documentación

- **`MANUAL_USUARIO.md`** — manual de usuario completo, dirigido a operadores de laboratorio. Se distribuye junto al ejecutable y se abre desde *Ayuda → Manual de Usuario*.

El manual es la referencia funcional del instrumento; este README cubre únicamente la construcción y la estructura del código.

---

## Créditos

Desarrollado en el **Centro de Investigaciones Ópticas (CIOp)** — CONICET / CIC-PBA / UNLP.

- **Desarrollo:** Santiago Agustín Salgado — Ingeniería Industrial, Facultad de Ingeniería, UNLP.
- **Dirección:** Dr. Ing. Fabián Alfredo Videla.
- **Codirección:** Dra. Lorena Rebón.

Trabajo realizado en el marco de la Práctica Profesional Supervisada (480 h, ago 2025 – ago 2026).

El proyecto deriva de una base de código abierto preexistente e incorpora **QCustomPlot** (GPL). Las licencias correspondientes se distribuyen con el instalador.
