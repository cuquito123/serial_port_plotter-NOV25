# Informe Final de Auditoría

## Alcance
Auditoría estática y funcional del proyecto MPCC — Multi-Photon Coincidence Counter (CIOp), enfocada en seguridad operativa, estabilidad de UI, rendimiento de ploteo, dependencias/licencias y empaquetado.

## Resumen Ejecutivo
La base del proyecto es funcional y conserva una arquitectura clara de captura serial, parseo, ploteo y persistencia. Los riesgos más relevantes encontrados estaban concentrados en tres zonas: escritura serial bloqueante, conversión numérica sin validación en el ploteo y una fuga de memoria en la ventana principal. También se detectó una incoherencia de versión entre la aplicación y el instalador.

Los tres riesgos técnicos principales ya fueron corregidos en esta misma revisión. El riesgo residual más importante es el repintado frecuente del gráfico bajo carga alta y la telemetría de paquetes inválidos, que actualmente es más diagnóstica que preventiva.

## Hallazgos

### 1. Escritura serial potencialmente bloqueante
- **Severidad:** Alta
- **Evidencia:** En `serialportmanager.cpp`, `writeData()` usaba una espera activa con `waitForBytesWritten(200)` dentro de un bucle de escritura.
- **Impacto:** Riesgo de congelamiento parcial de la interfaz y degradación de responsividad durante envíos frecuentes o dispositivos lentos.
- **Recomendación:** Evitar espera activa en el hilo de UI. Usar escritura asíncrona o mover la operación a un hilo dedicado.
- **Estado:** Corregido. La escritura quedó reducida a una llamada directa a `QSerialPort::write()`.
- **Verificación:** En `serialportmanager.cpp` la función `writeData()` realiza `m_serialPort->write(data)` sin llamadas a `waitForBytesWritten()`.

### 2. Parseo numérico sin validación
- **Severidad:** Media
- **Evidencia:** En `plotmanager.cpp`, `addDataPoint()` convertía con `QString::toDouble()` sin comprobar éxito.
- **Impacto:** Datos inválidos podían entrar al gráfico de forma silenciosa y ensuciar la visualización.
- **Recomendación:** Validar conversión con bandera `ok` y descartar valores no numéricos, contabilizando errores si se requiere trazabilidad.
- **Estado:** Corregido. Ahora los valores inválidos se ignoran explícitamente.
- **Verificación:** En `plotmanager.cpp` `addDataPoint()` usa `toDouble(&ok)` y descarta valores cuando `ok` es falso.

### 3. Fuga de memoria en `MainWindow`
- **Severidad:** Media
- **Evidencia:** `m_fpgaProtocolApplied` se creaba con `new FpgaProtocol()` y no se liberaba en el destructor.
- **Impacto:** Fuga de memoria acumulativa y ownership inconsistente en objetos de vida ligada a la ventana.
- **Recomendación:** Liberar explícitamente el puntero o asignar ownership con padre Qt.
- **Estado:** Corregido. Se agregó su liberación en el destructor.
- **Verificación:** En `mainwindow.cpp` el destructor ahora contiene `delete m_fpgaProtocolApplied;`.

### 4. Incoherencia entre versión de la app e instalador
- **Severidad:** Baja
- **Evidencia:** La ventana principal mostraba `v2.3.0`, mientras `installer.iss` aún declaraba `1.2.1`.
- **Impacto:** Confusión en distribución, soporte y trazabilidad de artefactos.
- **Recomendación:** Mantener una única fuente de verdad para la versión y propagarla al binario e instalador.
- **Estado:** Corregido. El instalador quedó alineado con `2.3.0`.
- **Verificación:** En `installer.iss` la macro `MyAppVersion` está definida como `"2.3.0"`.

## Riesgos Residuales
- El repintado del gráfico sigue siendo el principal foco de rendimiento bajo alta frecuencia de datos.
- La telemetría de salud cuenta paquetes válidos, pero la invalidación de paquetes no está integrada como mecanismo preventivo fuerte.
- La distribución depende de que el empaquetado copie correctamente recursos y dependencias de licencia.

## Dependencias y Licencias
El repositorio incluye textos de licencia en `res/lgplv3.rtf` y `qcustomplot/GPL.txt`. No se detectó un bloqueo inmediato de distribución, aunque debe mantenerse la consistencia con las condiciones de Qt y QCustomPlot.

## Documentación y Empaquetado
La documentación de usuario existe y describe el flujo operativo actualizado. El script de Inno Setup sigue una estructura válida y ya quedó alineado en versión con la aplicación. Conviene revisar en una pasada posterior que el directorio de dependencias empaquetado incluya todos los artefactos requeridos en cada plataforma objetivo.

## Conclusión
El proyecto se encuentra en un estado razonablemente sólido para su línea actual de funcionamiento. Los problemas más serios identificados en esta auditoría quedaron corregidos. La prioridad siguiente no es funcionalidad nueva, sino endurecimiento de rendimiento y observabilidad: limitar el coste del repintado, formalizar mejor la telemetría de errores y, si el caudal sube, desacoplar aún más la E/S del hilo de interfaz.
