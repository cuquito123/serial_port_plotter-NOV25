# Informe Final de Auditoría

## Alcance
Auditoría estática y funcional del proyecto **MPCC — Multi-Photon Coincidence Counter** (CIOp), enfocada en seguridad operativa, estabilidad de UI, rendimiento de ploteo, dependencias/licencias y empaquetado. Esta revisión actualiza la auditoría original con el trabajo incorporado entre mayo y julio de 2026: máquina de estados operativa, control previo a la ejecución, telemetría de salud de la comunicación, centralización de los controles de puerto COM y dos correcciones de UI encontradas durante la integración final.

## Resumen Ejecutivo
La base del proyecto es funcional y conserva una arquitectura clara de captura serial, parseo, ploteo y persistencia. La auditoría original detectó tres riesgos técnicos (escritura serial bloqueante, conversión numérica sin validación en el ploteo y una fuga de memoria en la ventana principal) y una incoherencia de versión entre la aplicación y el instalador; los cuatro quedaron corregidos y se verificaron nuevamente sobre el código actual.

Desde entonces, la reestructuración de `MainWindow` alrededor de un `enum class AppState` explícito, sumada al control previo (`performPreflightCheck()`) y a la telemetría de salud, redujo significativamente la superficie de estados inconsistentes de la UI. Durante la integración final se detectaron y corrigieron además dos defectos de interfaz: conexiones de señales duplicadas que abrían el diálogo de Propiedades de Puerto dos veces por click, y la acción Desconectar deshabilitada incorrectamente al entrar en estado `Fault` con el puerto realmente abierto.

El riesgo residual más importante sigue siendo el repintado del gráfico bajo carga alta; el throttling adaptativo (50 → 30 fps) lo mitiga pero no lo elimina.

## Hallazgos de la auditoría original (verificados, siguen corregidos)

### 1. Escritura serial potencialmente bloqueante
- **Severidad:** Alta
- **Evidencia:** En `serialportmanager.cpp`, `writeData()` usaba una espera activa con `waitForBytesWritten(200)` dentro de un bucle de escritura.
- **Impacto:** Riesgo de congelamiento parcial de la interfaz y degradación de responsividad durante envíos frecuentes o dispositivos lentos.
- **Estado:** Corregido. La escritura quedó reducida a una llamada directa a `QSerialPort::write()`, con el resultado propagado mediante las señales `writeSucceeded()` / `writeFailed()`.
- **Verificación:** `serialportmanager.cpp:65` — `m_serialPort->write(data)`, sin llamadas a `waitForBytesWritten()`.

### 2. Parseo numérico sin validación
- **Severidad:** Media
- **Evidencia:** En `plotmanager.cpp`, la conversión de valores usaba `QString::toDouble()` sin comprobar éxito.
- **Impacto:** Datos inválidos podían entrar al gráfico de forma silenciosa y ensuciar la visualización.
- **Estado:** Corregido. Los valores inválidos se ignoran explícitamente.
- **Verificación:** `plotmanager.cpp:132` — `toDouble(&ok)`, con descarte del valor cuando `ok` es falso.

### 3. Fuga de memoria en `MainWindow`
- **Severidad:** Media
- **Evidencia:** `m_fpgaProtocolApplied` se creaba con `new FpgaProtocol()` y no se liberaba en el destructor.
- **Impacto:** Fuga de memoria acumulativa y ownership inconsistente en objetos de vida ligada a la ventana.
- **Estado:** Corregido.
- **Verificación:** `mainwindow.cpp:685` — el destructor contiene `delete m_fpgaProtocolApplied;`.

### 4. Incoherencia entre versión de la app e instalador
- **Severidad:** Baja
- **Evidencia:** La ventana principal mostraba `v2.3.0`, mientras `installer.iss` aún declaraba una versión anterior.
- **Impacto:** Confusión en distribución, soporte y trazabilidad de artefactos.
- **Estado:** Corregido, pero volvió a desviarse: al subir la app a `v3.0.0` (`main.cpp:47`), `installer.iss` y los recursos de versión de Windows (`res/serial_port_plotter.rc`, `res/res1/serial_port_plotter.rc`) quedaron atrás en `2.3.0`. Se corrigió nuevamente en esta revisión, y de paso se quitó el publisher/URL heredado del repositorio upstream (`CieNTi`), que ya no aplica a este fork.
- **Verificación:** `installer.iss:5` — `MyAppVersion` es `"3.0.0"`; `res/serial_port_plotter.rc:3-7` y `res/res1/serial_port_plotter.rc:3-7` — `VER_FILEVERSION`/`VER_PRODUCTVERSION` en `3,0,0,0`/`"3.0.0"`, consistentes con la aplicación.

## Hallazgos incorporados en esta revisión

### 5. Conexiones duplicadas en el menú de puerto serial
- **Severidad:** Media
- **Evidencia:** En `buildMenus()`, `ui->actionPropiedades_de_Puerto` y `ui->actionMostar_todos_los_datos` se conectaban manualmente además de la conexión automática que Qt establece por convención de nombres (`QMetaObject::connectSlotsByName`, activada por `setupUi()`).
- **Impacto:** Cada click en "Propiedades de Puerto..." disparaba el slot dos veces, abriendo el diálogo duplicado.
- **Estado:** Corregido. Se eliminaron las conexiones manuales redundantes, dejando la conexión automática por convención de nombres como única vía.
- **Verificación:** `mainwindow.cpp` — `buildMenus()` ya no contiene `connect(ui->actionPropiedades_de_Puerto, ...)` ni `connect(ui->actionMostar_todos_los_datos, ...)`.

### 6. `actionDisconnect` no disponible en estado `Fault`
- **Severidad:** Media
- **Evidencia:** En `updateUIForState()`, la habilitación de `actionConnect` / `actionDisconnect` estaba atada al estado lógico (`isDisconnected`) en lugar del estado real del puerto.
- **Impacto:** Si la aplicación entraba en `Fault` (por ejemplo, control previo fallido) con el puerto físicamente abierto, no había forma de desconectar desde la UI para recuperarse.
- **Estado:** Corregido. Ambas acciones ahora se atan a la variable `connected`, que refleja el estado real del puerto, no al estado lógico de la máquina de estados.
- **Verificación:** `mainwindow.cpp` — `updateUIForState()`: `ui->actionConnect->setEnabled(!connected)`, `ui->actionDisconnect->setEnabled(connected)`.

## Riesgos Residuales
- El repintado del gráfico sigue siendo el principal foco de rendimiento bajo alta frecuencia de datos; el throttling adaptativo (50 → 30 fps en `PlotManager`) mitiga el peor caso pero no lo elimina.
- La telemetría de salud cuenta paquetes válidos e inválidos y dispara advertencia/`Fault` a partir de umbrales fijos (10 % / 25 %, `kInvalidPacketWarningRatio` / `kInvalidPacketFaultRatio` en `mainwindow.cpp`), pero sigue siendo más diagnóstica que preventiva: no hay backoff automático ni reintento de conexión.
- La distribución depende de que el empaquetado (`windeployqt` + copia manual de DLLs de MinGW) incluya correctamente todos los recursos y dependencias en cada máquina de destino.

## Dependencias y Licencias
El repositorio incluye textos de licencia en `res/lgplv3.rtf`, `GPL.txt` y `qcustomplot/GPL.txt`. No se detectó un bloqueo inmediato de distribución, aunque debe mantenerse la consistencia con las condiciones de Qt y QCustomPlot. El instalador (`installer.iss`) copia el manual y las licencias al directorio de instalación junto al ejecutable.

## Documentación y Empaquetado
`MANUAL_USUARIO.md` describe el flujo operativo actualizado (máquina de estados, control previo, perfiles) y se distribuye junto al ejecutable, accesible desde *Ayuda → Manual de Usuario*. El script de Inno Setup mantiene una estructura válida y está alineado en versión con la aplicación.

## Conclusión
El proyecto se encuentra en un estado sólido para su línea actual de funcionamiento. Los problemas identificados en la auditoría original permanecen corregidos, y los dos defectos de UI detectados durante la integración final (diálogo duplicado, Desconectar inaccesible en `Fault`) ya están resueltos. La prioridad siguiente sigue sin ser funcionalidad nueva, sino endurecimiento de rendimiento y observabilidad: acotar mejor el coste del repintado bajo carga sostenida y evaluar mecanismos de recuperación automática ante degradación de la calidad de enlace.
