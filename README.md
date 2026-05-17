# Serial Port Plotter

Aplicación de Windows para visualizar datos en tiempo real desde un puerto serie. La aplicación es de 32 bits y está construida con Qt y la librería QCustomPlot.

## Características

- Sin límite de ejes: canales desconocidos/nuevos crean automáticamente un gráfico (paleta cíclica de 14 colores).
- Sin límite de puntos: todos los datos recibidos se conservan para que el usuario pueda explorar datos antiguos.
- Sin límite de baudios: probado hasta 912600 bps.
- Zoom y arrastre con el ratón (rueda o clic, limitado al eje X).
- Al mover el cursor sobre el gráfico se muestran los valores X e Y en la barra de estado.
- Leyenda con nombre de canal (doble clic para modificar).
- Selección de canal (clic sobre el texto de la leyenda).
- Soporta enteros y números decimales positivos y negativos.
- Exporta a PNG.
- Exporta a CSV.
- Autoescalado al gráfico visible.

## Captura de pantalla

![Serial Port Plotter screenshot](res/screen_0.png)

## Cómo usar la aplicación

Envía los datos (enteros o decimales) por el puerto serie usando el formato esperado para que la aplicación pueda distinguir valores. Cada mensaje debe comenzar con `$` y terminar con `;` (punto y coma), con los valores separados por espacios. El botón de `Help` muestra instrucciones detalladas.

Usá la rueda del ratón sobre controles para cambiar valores y sobre el área del gráfico para hacer zoom.

Cuando la adquisición está detenida/pausada, se puede arrastrar el área del gráfico y habilitar el guardado a fichero.

Para habilitar el guardado a fichero, presioná el botón de documento antes de iniciar el ploteo.

Hacé doble clic en un canal en el panel de control del gráfico para ocultarlo/mostrarlo.

![File Save Button](res/screen_1.png)

## Serial Port Plotter v2.3.0

Esta versión organiza la ventana principal alrededor de una máquina de estados operativa, agrega validación previa antes de aplicar la configuración al FPGA y suma trazabilidad, telemetría y perfiles operativos. El flujo general pasa de una UI puramente reactiva a un ciclo más controlado entre configuración, aplicación y adquisición.

### 1. Cambios estructurales

La arquitectura interna de MainWindow ahora incorpora estado operativo explícito, verificación previa y módulos auxiliares para seguimiento y persistencia. La lógica ya no depende solo de botones sueltos, sino de transiciones controladas entre estados.

- Se agregó el `enum class AppState` en `mainwindow.hpp` con los estados `Disconnected`, `ReadyForConfiguration`, `ReadyForExecution`, `Acquiring`, `Paused` y `Fault`.
- Se agregaron los métodos `setAppState()`, `currentAppState()`, `getStateDisplayName()`, `updateUIForState()` y `canTransitionToState()` para gobernar el flujo de la ventana principal.
- Se incorporó la estructura `PreflightResult` junto con `performPreflightCheck()` para validar el puerto, los canales activos, el tiempo configurado y el estado de la grabación CSV antes de enviar la configuración.
- Se sumaron los bloques `HealthMetrics`, `ConnectionParams`, `EventType`, `OperativeEvent` y `OperativeProfile` para telemetría, recuperación, registro y perfiles.
- El constructor de `MainWindow` ahora conecta `SerialPortManager`, `SerialMessageParser`, `PlotManager` y `CsvManager` en una cadena de procesamiento clara: recepción cruda -> parseo -> nueva data -> ploteo -> guardado.
- Se añadieron helpers de estado local como `markPendingChanges()`, `clearPendingChanges()`, `updatePendingChangesIndicator()`, `limpiarMatrizInterna()` y `limpiarPlot()`.

### 2. Cambios visuales y de interfaz

La interfaz quedó más segmentada y explícita. La pantalla principal separa mejor la configuración del gráfico y expone controles visuales para tiempo, matriz, canales y puerto.

- Se usa un `QStackedWidget` llamado `stackedWidget` para alternar entre la vista de configuración y la vista de gráfico.
- Se reorganizaron los controles en dos bloques visibles: `PlotControlsBox` y `Port Controls`.
- Se agregaron o conservaron widgets de configuración directa como `TiempoBox`, `TiempoNum`, `EnviarDatos`, `ResetearDatos`, `Ancho_de_pulso`, `Delay_A`, `Delay_B`, `Delay_C` y `Delay_D`.
- Se mantiene la matriz de selección con botones `GRAF_1` a `GRAF_8` y `A1_0` a `D8_31`, que ahora se colorean según su estado.
- Se incorporó `listWidget_Channels` como panel de canales visibles, con acciones asociadas para AutoScale, Reset Visible y Show All Incoming Data.
- `actionRecord_stream` sigue presente en la barra superior, y el botón `EnviarDatos` cambia visualmente cuando hay cambios pendientes de aplicar.

### 3. Cambios en las capacidades para el usuario

El usuario dispone ahora de un flujo operativo más guiado. La aplicación diferencia entre configurar, aplicar, ejecutar, pausar y desconectar, y habilita cada acción según el estado real de la sesión.

- El usuario puede configurar matriz y tiempos antes de aplicar la configuración con `on_EnviarDatos_clicked()`.
- El usuario debe pasar por `Connect` para iniciar la adquisición después de haber enviado la configuración, en lugar de iniciar la ejecución directamente.
- El usuario puede pausar la adquisición con `on_actionPause_Plot_triggered()` sin cerrar el puerto serie.
- El usuario puede volver a conectar o reanudar desde el estado `Paused`, sujeto a la validación de `performPreflightCheck()`.
- El usuario puede ocultar o mostrar el texto UART con `actionEsconder_Caja_de_Texto` y alternar entre la vista de configuración y la del gráfico con `actionconfig` o `on_ir_a_grafico_clicked()`.
- El usuario puede controlar la visualización de canales con `on_listWidget_Channels_itemDoubleClicked()`, `on_pushButton_ResetVisible_clicked()` y `on_pushButton_ShowallData_clicked()`.
- La grabación CSV con `actionRecord_stream` queda condicionada al estado de adquisición o pausa, y se detiene al desconectar o cerrar el archivo.

### 4. Cambios en el comportamiento del sistema

El comportamiento interno ahora cubre más que recepción y ploteo: también valida, registra, recupera y persiste el estado operativo. El envío al FPGA, la grabación CSV y el mapeo de gráficos quedaron más controlados.

- La recepción serial sigue el flujo SerialPortManager -> SerialMessageParser -> newData(QStringList) -> onNewDataArrived(), y el texto UART se muestra o filtra según filterDisplayedData.
- onNewDataArrived() actualiza HealthMetrics con updateHealthMetrics() antes de enviar la muestra al PlotManager.
- on_EnviarDatos_clicked() realiza un preflight, limpia el gráfico, genera etiquetas con FpgaProtocol::generateLabels(), fija el mapeo activo con PlotManager::setActiveTramaIndices() y envía buildStartCommand() más buildExtendedPacket().
- on_ResetearDatos_clicked() envía buildResetCommand() y buildResetSweepPacket(), además de limpiar la matriz local y el texto UART.
- El guardado CSV usa CsvManager::saveData() y toma el índice de punto desde el contador real del plot, de modo que la grabación acompaña la secuencia visual.
- Se agregó trazabilidad operativa con logEvent(), getEventLogAsString() y exportEventLog(), registrando eventos como PortOpened, PortClosed, Started, Stopped, ConfigApplied, Reset, Recovery y Error.
- Se incorporó persistencia de perfiles con saveProfile(), loadProfile(), deleteProfile(), getProfileNames() y getProfilesDirectory(), usando OperativeProfile::toJson() y OperativeProfile::fromJson() para serializar la configuración completa.

## Envío de datos por puerto serie

```c
/* Ejemplo: plotea dos valores */
printf("$%d %d;", data1, data2);
```

Dependiendo de la frecuencia de envío y de la cantidad de puntos visible, podés ajustar el número de puntos mostrados. Por ejemplo, si envías datos cada 10 ms (100 Hz) y el ploteador muestra 500 puntos, representarás ~5 segundos de datos.

El software soporta números enteros y decimales (float/double).

## Código fuente

El código fuente y el archivo `.pro` del proyecto Qt están disponibles. También hay un ejecutable independiente para quien no quiera compilar: ver [releases](https://github.com/CieNTi/serial_port_plotter/releases).

## Créditos

- [Serial Port Plotter en mbed forums](https://developer.mbed.org/users/borislav/notebook/serial-port-plotter/) por [Borislav K](https://developer.mbed.org/users/borislav/)
- Line Icon Set por [Situ Herrera](http://www.flaticon.com/authors/situ-herrera)
- Lynny icon pack
- Changelog (keepachangelog.com)
- Base del software por [CieNTi](https://github.com/CieNTi)
- Exportación CSV por [HackInventOrg](https://github.com/HackInventOrg)

## Registro de cambios (Changelog)

Los cambios relevantes del proyecto se documentan a continuación. Este proyecto sigue [Semantic Versioning](http://semver.org/).

### [1.3.0] - 2018-08-01

**Info**

- Compilado con QT 5.11.1
- Librerías QT actualizadas y nuevas funciones de ploteo

**Añadido**

- Botón para refrescar la lista de puertos COM
- Control de visibilidad de canales para ocultar canales no deseados
- Botón de AutoScale para el eje Y ajustando al valor máximo +10%
- Soporte para guardar en CSV

**Cambiado**

- `qDarkStyle` actualizado a 2.5.4
- `qCustomplot` actualizado a 2.0.1

**Corrección de bugs**

- El diálogo de renombrado de ejes ahora recibe foco correctamente al abrirse

### [1.2.2] - 2018-07-26

**Info**

- Proyecto derivado de HackInvent desde 1.2.1

**Añadido**

- Cuadro de texto UART para debug
- Control de visibilidad y filtrado del textbox

### [1.2.1] - 2017-09-24

**Corregido**

- Soporte para float/double añadido
- Corrección de fallo de compilación en Linux relacionado con `serial_port_plotter_res.o`

### [1.2.0] - 2016-08-28

**Añadido**

- Soporte para números negativos
- Soporte para tasas de baudios altas (probado hasta 912600 bps)

### [1.1.0] - 2016-08-28

**Añadido**

- Recursos `qdarkstyle` originales (iconos funcionando)
- Manifest y configuraciones recomendadas para Windows
- Iconos *Line Icon Set* en 3 colores
- Iconos *Lynny* en 3 colores
- Archivo Inno Setup con script de empaquetado automático (probado en WinXP-32b y Win10-64b)
- Botones de Play/Pause/Stop, Clear y Help en la barra de herramientas

**Cambiado**

- Estructura de recursos
- `qcustomplot` actualizado a v1.3.2
- El menú principal fue reemplazado por una barra de iconos para mejorar usabilidad

**Eliminado**

- Control sobre número de puntos
- Borrar datos de gráficos previos
- Botones separados *Connect* y *Start/Stop plot*

### [1.0.0] - 2014-08-31

**Añadido**

- Trabajo original de Borislav Kereziev


## Tareas pendientes (To-Do)

- Refrescar lista de puertos
- Autocompletar baudios y permitir personalizados por textbox (cuando UI COM)
- PNG con transparencia
- Separar `receive_data` de `process_data` para operaciones no limitadas por throttling

[1.3.0]: https://github.com/Eriobis/serial_port_plotter/releases/tag/v1.3.0
[1.2.2]: https://github.com/Eriobis/serial_port_plotter/releases/tag/v1.2.2
[1.2.0]: https://github.com/CieNTi/serial_port_plotter/releases/tag/v1.2.0
[1.1.0]: https://github.com/CieNTi/serial_port_plotter/releases/tag/v1.1.0
[1.0.0]: https://github.com/CieNTi/serial_port_plotter/releases/tag/v1.0.0
