# Serial Port Plotter

Software de instrumentación científica para el **detector de fotones en coincidencias múltiples** desarrollado en el Centro de Investigaciones Ópticas (CIOp — CONICET / CIC-PBA / UNLP).

La aplicación comanda y monitorea un detector implementado sobre una placa **FPGA DE0 Nano SoC** (Altera/Intel) programada en VHDL, que se comunica con la PC por **UART/RS232**. El dispositivo opera con 12 canales: 4 de conteo individual y el resto para coincidencias configurables de hasta 4 canales en simultáneo.

Desarrollada en **C++ con el framework Qt**, permite configurar el experimento, aplicar la configuración al hardware, visualizar los conteos en tiempo real y registrar la adquisición para su procesamiento estadístico posterior.

**Versión actual:** 2.3.0

---

## Contexto

El proyecto se enmarca en *"Diseño y desarrollo de dispositivos ópticos para comunicaciones cuánticas aeroespaciales"* (PICT-2020-SERIE A-I-GRF), orientado a distribución cuántica de claves (QKD) como carga útil del **Satélite Universitario** de la UNLP, bajo el estándar CubeSat.

El desarrollo se realizó en el marco de una **Práctica Profesional Supervisada** de la carrera de Ingeniería Industrial (Facultad de Ingeniería, UNLP), bajo la dirección del Dr. Ing. Fabián Alfredo Videla, con la co-dirección de la Dra. Lorena Rebón.

---

## Documentación

| Documento | Contenido |
|---|---|
| [`MANUAL_USUARIO.md`](MANUAL_USUARIO.md) | Manual completo para operadores del instrumento: puesta en marcha, operación y referencia |

El manual se distribuye junto al ejecutable y es accesible desde la propia aplicación en *Ayuda → Manual de Usuario*.

---

## Arquitectura

La base de código original consistía en una clase `MainWindow` monolítica que concentraba comunicación, parseo, protocolo, visualización y persistencia sin separación de responsabilidades. La reestructuración extrajo esas responsabilidades transversales a módulos independientes:

| Módulo | Responsabilidad |
|---|---|
| `SerialPortManager` | Comunicación serie: apertura, cierre, lectura asíncrona y escritura no bloqueante |
| `SerialMessageParser` | Parseo por máquina de estados del protocolo `$…;`, con validación carácter a carácter |
| `FpgaProtocol` | Construcción de paquetes, conversión y normalización de tiempos, etiquetas y mapeo de trama |
| `PlotManager` | Visualización en tiempo real con QCustomPlot |
| `CsvManager` | Exportación a CSV con metadatos, más un archivo HTML de formato paralelo |
| `ProfileManager` | Persistencia de perfiles de experimento en formato JSON |

`MainWindow` quedó como **coordinador**: instancia los módulos, los conecta mediante señales y slots, y gobierna la máquina de estados de la aplicación.

**Flujo de datos:**

```
FPGA → SerialPortManager → SerialMessageParser → MainWindow → PlotManager / CsvManager
```

### Máquina de estados

La aplicación gobierna qué controles están disponibles mediante un estado operativo explícito (`enum class AppState`):

`Disconnected` · `ReadyForConfiguration` · `ReadyForExecution` · `Acquiring` · `Paused` · `Fault`

Las transiciones se validan formalmente en `canTransitionToState()`, invocada desde `setAppState()`: cualquier cambio de estado no contemplado en el flujo operativo es rechazado y registrado.

### Robustez operativa

- **Control previo a la ejecución** (`performPreflightCheck()`): valida puerto, canales activos, valor de tiempo, rango de la ventana de integración y disponibilidad del archivo CSV antes de transmitir la configuración.
- **Telemetría de salud:** conteo de paquetes válidos, inválidos y perdidos, con umbrales de advertencia al 10 % y paso a estado `Fault` al 25 %.
- **Registro de eventos** con marca temporal y categorización por tipo, exportable.
- **Recuperación ante fallos:** almacenamiento de los parámetros de la última conexión exitosa para reconexión automática.
- **Ajuste adaptativo del gráfico:** reducción automática de 50 a 30 cuadros por segundo si el repintado consume más de la mitad del intervalo de refresco.

---

## Compilación

**Requisitos:** Qt 5.12.2 con MinGW 7.3.0 (32 bits).

> La ruta del proyecto no debe contener espacios: rompe a `mingw32-make`.

Desde la consola *Qt 5.12.2 (MinGW 7.3.0 32-bit)*, que trae el `PATH` ya configurado:

```cmd
cd /d <ruta-del-proyecto>
mkdir build-release
cd build-release
qmake ..\SerialPortPlotter.pro "CONFIG+=release"
mingw32-make -j4
```

El ejecutable resultante es `build-release\release\serial_port_plotter.exe`.

> El nombre del ejecutable (`serial_port_plotter.exe`) no coincide con el del archivo de proyecto (`SerialPortPlotter.pro`).

Para recompilar tras cambios en el código alcanza con `mingw32-make -j4`. Sólo hay que volver a ejecutar `qmake` si se modifica el `.pro` o se agregan archivos nuevos.

### Empaquetado

```cmd
mkdir C:\deploy
copy release\serial_port_plotter.exe C:\deploy
windeployqt --release C:\deploy\serial_port_plotter.exe
copy C:\Qt\Qt5.12.2\Tools\mingw730_32\bin\libgcc_s_dw2-1.dll C:\deploy
copy C:\Qt\Qt5.12.2\Tools\mingw730_32\bin\libstdc++-6.dll C:\deploy
copy C:\Qt\Qt5.12.2\Tools\mingw730_32\bin\libwinpthread-1.dll C:\deploy
copy MANUAL_USUARIO.md C:\deploy
```

Conviene verificar el paquete ejecutándolo desde una consola limpia, sin el `PATH` de Qt cargado.

El instalador de Windows se genera con Inno Setup a partir de `installer.iss`.

---

## Banco de pruebas virtual

El repositorio incluye `inyector3.py`, un simulador de tramas de FPGA escrito en Python con `pyserial`. Combinado con un par de puertos COM virtuales (com0com), permite verificar la aplicación sin depender del hardware:

```
inyector3.py → puerto virtual A ↔ puerto virtual B → serial_port_plotter.exe
```

El inyector genera tramas con el formato del protocolo a 20 Hz, modelando cada canal físico como una onda cuadrada con frecuencia, ciclo de trabajo y fase configurables. Las columnas de coincidencia cuentan las muestras internas en que todos los canales de esa combinación estuvieron simultáneamente en nivel alto, de modo que el resultado esperado es predecible analíticamente antes de correr la prueba.

**Valida:** el parseo de tramas, el mapeo de columnas, el conteo de coincidencias contra valores calculados de antemano y las métricas de salud bajo carga sostenida.

**No valida:** temporización real de la FPGA, ruido eléctrico ni comportamiento del adaptador USB-serie físico. Eso sigue requiriendo el banco con osciloscopio y generador de señales.

La configuración se ajusta editando la sección `CONFIG` del script.

---

## Formato de trama

La aplicación espera mensajes que comiencen con `$` y terminen con `;`, con los valores separados por espacios:

```c
/* Ejemplo de envío desde el dispositivo */
printf("$%d %d;", dato1, dato2);
```

Se admiten enteros y decimales, positivos y negativos. Las tramas que no cumplen el formato se descartan y se contabilizan como inválidas.

---

## Créditos

Este software deriva del proyecto **Serial Port Plotter** de código abierto. Se reconoce el trabajo de sus autores originales:

- Trabajo original: [Borislav Kereziev](https://developer.mbed.org/users/borislav/) — [Serial Port Plotter en los foros de mbed](https://developer.mbed.org/users/borislav/notebook/serial-port-plotter/)
- Base del software: [CieNTi](https://github.com/CieNTi/serial_port_plotter)
- Exportación a CSV: [HackInventOrg](https://github.com/HackInventOrg)
- Biblioteca de graficado: [QCustomPlot](https://www.qcustomplot.com/)
- Iconos: *Line Icon Set* por [Situ Herrera](http://www.flaticon.com/authors/situ-herrera) y *Lynny icon pack*

Las adaptaciones para el detector de coincidencias múltiples, la reestructuración modular y las funcionalidades de robustez operativa fueron desarrolladas en el CIOp.

---

## Licencia

Distribuido bajo la **GNU General Public License v3.0**. Ver [`LICENSE`](LICENSE) y [`GPL.txt`](GPL.txt).
