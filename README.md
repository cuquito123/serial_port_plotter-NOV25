# Serial Port Plotter

This is a Windows application that displays real time data from serial port. The application is 32-bit and built with Qt and QCustomPlot library.

## Features

- No axes limit: An unknown/new channel data create a new graph and uses it (palette of 14 cyclic colors)
- No data point limit: All received data is kept so user can explore old data
- No baud rate limit: Tested up to 912600 bps
- Zooming and dragging using the mouse (wheel or click, restricted to X axis only)
- Moving around the plot displays the X and Y values of the graph in the status bar
- Channel's name legend (double-click to modify)
- Channel selection (click on legend's text)
- Supports positive and negative integers and floats
- Exports to PNG
- Exports to CSV
- Autoscale to visible graph

## Screenshot

![Serial Port Plotter screenshot](res/screen_0.png)

## How to use the application

Just send your integer data over the serial port. The application expects certain format so it can differentiate between values. Each message you send must start with '$' and end with ';' (semi-colon) and the integers inside need to be space-separated. The Help button displays comprehensive instructions.

Use the mouse wheel over controls to change its values and use it over plot area to zoom.

When stopped/paused, plot area can be dragged and file saving can be enabled.

To enable the file saving, click on the document button before starting the plot.

Double click on a channel in the Graph Control panel to hide/show a specific channel.

![File Save Button](res/screen_1.png)

## Serial Port Plotter NOV25

Esta versión reorganiza la ventana principal alrededor de una máquina de estados operativa, agrega validación previa antes de aplicar la configuración al FPGA y suma trazabilidad, telemetría y perfiles operativos. El flujo general pasa de una UI reactiva a un ciclo más controlado entre configuración, aplicación y adquisición.

### 1. Cambios estructurales

La arquitectura interna de MainWindow ahora incorpora estado operativo explícito, verificación previa y módulos auxiliares para seguimiento y persistencia. La lógica ya no depende solo de botones sueltos, sino de transiciones controladas entre estados.

- Se agregó el enum class AppState en [mainwindow.hpp](mainwindow.hpp) con los estados Disconnected, ReadyForConfiguration, ReadyForExecution, Acquiring, Paused y Fault.
- Se agregaron los métodos setAppState(), currentAppState(), getStateDisplayName(), updateUIForState() y canTransitionToState() para gobernar el flujo de la ventana principal.
- Se incorporó la estructura PreflightResult junto con performPreflightCheck() para validar el puerto, los canales activos, el tiempo configurado y el estado de la grabación CSV antes de enviar la configuración.
- Se sumaron los bloques HealthMetrics, ConnectionParams, EventType, OperativeEvent y OperativeProfile para telemetría, recuperación, logging y perfiles.
- El constructor de MainWindow ahora conecta SerialPortManager, SerialMessageParser, PlotManager y CsvManager en una cadena de procesamiento clara: recepción cruda, parseo, nueva data, ploteo y guardado.
- Se añadieron helpers de estado local como markPendingChanges(), clearPendingChanges(), updatePendingChangesIndicator(), limpiarMatrizInterna() y limpiarPlot().

### 2. Cambios visuales y de UI

La interfaz quedó más segmentada y explícita. La pantalla principal separa mejor la configuración del gráfico y expone controles visuales para tiempo, matriz, canales y puerto.

- Se usa un QStackedWidget llamado stackedWidget para alternar entre la vista de configuración y la vista de gráfico.
- Se reorganizaron los controles en dos bloques visibles: PlotControlsBox y Port Controls.
- Se agregaron o conservaron widgets de configuración directa como TiempoBox, TiempoNum, EnviarDatos, ResetearDatos, Ancho_de_pulso, Delay_A, Delay_B, Delay_C y Delay_D.
- Se mantiene la matriz de selección con botones GRAF_1 a GRAF_8 y A1_0 a D8_31, que ahora se colorean según su estado.
- Se incorporó listWidget_Channels como panel de canales visibles, con acciones asociadas para AutoScale, Reset Visible y Show All Incoming Data.
- actionRecord_stream sigue presente en la barra superior, y el botón EnviarDatos cambia visualmente cuando hay cambios pendientes de aplicar.

### 3. Cambios en poderes del usuario

El usuario dispone ahora de un flujo operativo más guiado. La aplicación diferencia entre configurar, aplicar, ejecutar, pausar y desconectar, y habilita cada acción según el estado real de la sesión.

- El usuario puede configurar matriz y tiempos antes de aplicar la configuración con on_EnviarDatos_clicked().
- El usuario debe pasar por Conectar para iniciar la adquisición después de haber enviado la configuración, en lugar de iniciar la ejecución directamente.
- El usuario puede pausar la adquisición con on_actionPause_Plot_triggered() sin cerrar el puerto serie.
- El usuario puede volver a conectar o reanudar desde el estado Paused, sujeto a la validación de performPreflightCheck().
- El usuario puede ocultar o mostrar el texto UART con actionEsconder_Caja_de_Texto y alternar entre la vista de configuración y la del gráfico con actionconfig u on_ir_a_grafico_clicked().
- El usuario puede controlar la visualización de canales con on_listWidget_Channels_itemDoubleClicked(), on_pushButton_ResetVisible_clicked() y on_pushButton_ShowallData_clicked().
- La grabación CSV con actionRecord_stream queda condicionada al estado de adquisición o pausa, y se detiene al desconectar o cerrar el archivo.

### 4. Cambios en el comportamiento del sistema

El comportamiento interno ahora cubre más que recepción y ploteo: también valida, registra, recupera y persiste el estado operativo. El envío al FPGA, la grabación CSV y el mapeo de gráficos quedaron más controlados.

- La recepción serial sigue el flujo SerialPortManager -> SerialMessageParser -> newData(QStringList) -> onNewDataArrived(), y el texto UART se muestra o filtra según filterDisplayedData.
- onNewDataArrived() actualiza HealthMetrics con updateHealthMetrics() antes de enviar la muestra al PlotManager.
- on_EnviarDatos_clicked() realiza un preflight, limpia el gráfico, genera etiquetas con FpgaProtocol::generateLabels(), fija el mapeo activo con PlotManager::setActiveTramaIndices() y envía buildStartCommand() más buildExtendedPacket().
- on_ResetearDatos_clicked() envía buildResetCommand() y buildResetSweepPacket(), además de limpiar la matriz local y el texto UART.
- El guardado CSV usa CsvManager::saveData() y toma el índice de punto desde el contador real del plot, de modo que la grabación acompaña la secuencia visual.
- Se agregó trazabilidad operativa con logEvent(), getEventLogAsString() y exportEventLog(), registrando eventos como PortOpened, PortClosed, Started, Stopped, ConfigApplied, Reset, Recovery y Error.
- Se incorporó persistencia de perfiles con saveProfile(), loadProfile(), deleteProfile(), getProfileNames() y getProfilesDirectory(), usando OperativeProfile::toJson() y OperativeProfile::fromJson() para serializar la configuración completa.

## Send data over the serial port

```c
/* Example: Plot two values */
printf ("$%d %d;", data1, data2);
```

Depending on how much data you want to display, you can adjust the number of data points. For example, if you send data from the serial port of the mbed every 10 ms (100 Hz) and the plotter is set to display 500 points, it will contain information for 5 seconds of data.

The software supports integer and decimal numbers ( float/double )

## Source

Source and .pro file of the Qt Project are available. A standalone .exe is included for the people who do not want to build the source. Search for it at [releases](https://github.com/CieNTi/serial_port_plotter/releases)

## Credits

- [Serial Port Plotter at mbed forums](https://developer.mbed.org/users/borislav/notebook/serial-port-plotter/) by [Borislav K](https://developer.mbed.org/users/borislav/)
- [Line Icon Set](http://www.flaticon.com/packs/line-icon-set) by [Situ Herrera](http://www.flaticon.com/authors/situ-herrera) icon pack
- [Lynny](http://www.1001freedownloads.com/free-vector/lynny-icons-full) icon pack
- [Changelog](http://keepachangelog.com/)
- Base of this software by [CieNTi](https://github.com/CieNTi)
- CSV export by [HackInventOrg](https://github.com/HackInventOrg)

## Changelog

All notable changes to this project will be documented below this line.
This project adheres to [Semantic Versioning](http://semver.org/).

## [1.3.0] - 2018-08-01

### Info

- Built with QT 5.11.1
- QT libraries updated and new plot features implemented
- Beginning of version 1.3

### Added

- COM port refresh button to update the list
- Channel visibility control added to turn off unwanted channel
- Autoscale button for Y axis will autoscale to the highest value + 10%
- Save to CSV support

### Changed

- qDarkStyle updated to 2.5.4
- qCustomplot updated 2.0.1

### Bugfix

- Axis rename dialog gets focus when popup occurs

## [1.2.2] - 2018-07-26

### Info

- Project forked from HackInvent since 1.2.1

### Added

- UART debug textBox
- Textbox control ( toggle visible and toggle data filter )

## [1.2.1] - 2017-09-24

### Fixed

- Support for float/double has been added
- Linux build fails because no `serial_port_plotter_res.o` file was found (Issue #4)

## [1.2.0] - 2016-08-28

### Added

- Negative numbers support ([cap we](https://developer.mbed.org/users/capwe/) FIX at [mbed forums](https://developer.mbed.org/comments/perm/22672/))
- Support for high baud rates (tested up to 912600 bps)

## [1.1.0] - 2016-08-28

### Added

- Original qdarkstyle resources (icons are working now)
- Manifest and all Windows related/recommended configs
- *Line Icon Set* icons in 3 colors
- *Lynny* icons in 3 colors
- Inno Setup file with auto-pack .bat file (installer tested on WinXP-32b and Win10-64b)
- Play/Pause/Stop, Clear and Help toolbar buttons

### Changed

- Resources structure
- Updated qcustomplot to v1.3.2
- Menubar is replaced by icon toolbar for usability
- [WiP] mainwindow.cpp doxygen friendly comments

### Removed

- Control over number of points
- Delete previous graph data
- *Connect* and *Start/Stop plot* buttons

## [1.0.0] - 2014-08-31

### Added

- Original [Borislav Kereziev](mailto:b.kereziev@gmail.com) work commit [source](https://developer.mbed.org/users/borislav/notebook/serial-port-plotter/)


## To-Do

- Port list refresh
- Fill baud automatically and allow custom by textbox (when COM ui)
- PNG *WITH* transparency
- Separate `receive_data` from `process_data` to allow non-throttled operations

[1.3.0]: https://github.com/Eriobis/serial_port_plotter/releases/tag/v1.3.0
[1.2.2]: https://github.com/Eriobis/serial_port_plotter/releases/tag/v1.2.2
[1.2.0]: https://github.com/CieNTi/serial_port_plotter/releases/tag/v1.2.0
[1.1.0]: https://github.com/CieNTi/serial_port_plotter/releases/tag/v1.1.0
[1.0.0]: https://github.com/CieNTi/serial_port_plotter/releases/tag/v1.0.0
