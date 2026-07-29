# Manual de Usuario - MPCC — Multi-Photon Coincidence Counter (CIOp) v2.3.0

Este manual describe cómo usar la versión `v2.3.0` de MPCC — Multi-Photon Coincidence Counter (CIOp). A continuación se listan los cambios y comportamientos relevantes introducidos en esta versión:

- Escritura serie no bloqueante: la aplicación usa `QSerialPort::write()` para evitar esperas activas en el hilo de UI.
- Validación de datos: valores no numéricos recibidos se ignoran durante el ploteo y se registran como advertencias.
- Limpieza de recursos: se corrigió la gestión de memoria de objetos de protocolo en la ventana principal.

Los pasos operativos descritos más abajo aplican a la versión `v2.3.0`.

## 1. Inicio rápido

1. Abrí la aplicación.
2. Elegí el puerto serie en `PORT`.
3. Configurá `BAUD`, `DATA`, `PARITY` y `STOP`.
4. Presioná `Connect`.
5. Elegí la matriz de datos y la columna de gráfico que querés usar.
6. Ajustá `TiempoBox`, `TiempoNum`, `Ancho de Pulso` y los `Delay`.
7. Presioná `Enviar Datos` para aplicar la configuración.
8. Presioná `Connect` nuevamente para iniciar la adquisición.

## 2. Estructura de la interfaz

La ventana principal separa dos vistas dentro de `stackedWidget`:

- La vista de configuración, donde se seleccionan puertos, canales y parámetros.
- La vista de gráfico, donde se visualiza la señal y se controlan los canales visibles.

Los bloques principales de la interfaz son:

- `PORT CONTROLS`: selección y actualización del puerto serie.
- `PLOT CONTROLS`: controles de visualización del gráfico.
- Matriz de selección de datos: botones `A1_0` a `D8_31`.
- Selección de columnas del gráfico: botones `GRAF_1` a `GRAF_8`.
- Panel de canales: `listWidget_Channels`.
- Área de texto UART: `textEdit_UartWindow`.

## 3. Conexión al puerto serie

Para comenzar a trabajar, primero hay que abrir el puerto.


Cuando el puerto queda abierto, la aplicación cambia al estado de preparación y deshabilita los controles serie mientras la conexión está activa.
Nota técnica: la escritura hacia el dispositivo se realiza mediante llamadas a `QSerialPort::write()` sin esperas activas en el hilo de interfaz, de modo que la UI no queda bloqueada por operaciones de E/S. Los fallos en la escritura se reportan en la barra de estado y en el log de debug.

## 4. Configuración de datos y tiempos

La configuración de envío se arma desde la matriz de datos y los parámetros temporales.

Nota de validación: durante el ploteo `PlotManager` convierte cadenas a doble usando una verificación de éxito; valores no numéricos se descartan y no llegan al gráfico (se registran como advertencias en debug). Esto preserva la integridad visual ante datos malformados.

Cada cambio en estos controles marca la configuración como pendiente de aplicar. En ese caso, el botón `Enviar Datos` queda resaltado.

## 5. Aplicar la configuración

Presioná `Enviar Datos` cuando terminaste de configurar la matriz y los tiempos.

Al hacerlo, la aplicación:

- valida que el puerto esté conectado,
- verifica que haya canales activos,
- revisa que el tiempo sea válido,
- confirma que la grabación CSV esté lista si está activada,
- genera las etiquetas activas del protocolo,
- prepara la trama extendida,
- y envía la configuración al FPGA.

Después de aplicar la configuración, el sistema queda listo para ejecutar. En ese punto, el siguiente paso es presionar `Connect` para iniciar la adquisición.

## 6. Iniciar, pausar y desconectar

El flujo operativo usa tres acciones principales:

- `Connect`: inicia la adquisición cuando la configuración ya fue aplicada.
- `Pause`: detiene la adquisición sin cerrar el puerto serie.
- `Disconnect`: cierra el puerto y devuelve la aplicación al estado desconectado.

Si la sesión está pausada, `Connect` funciona como reanudación de la adquisición. Si se detecta un error de validación, la aplicación puede pasar al estado de falla y pedir desconexión para recuperar el flujo.

## 7. Visualización del gráfico

La vista de gráfico permite controlar cómo se ve la señal en pantalla.

- `AutoScale Yaxis` ajusta el eje Y al contenido visible.
- `Reset All Visible` vuelve a mostrar todos los canales ocultos.
- `Save PNG` guarda una imagen del gráfico.
- `POINTS` define cuántos puntos se muestran.
- `Y STEP` define el paso de marcas del eje Y.
- `MIN` y `MAX` ajustan los límites verticales.

También podés interactuar con el gráfico usando el mouse:

- La rueda del mouse cambia el zoom.
- El arrastre permite desplazarte cuando la adquisición está pausada.
- Al mover el cursor sobre el gráfico, se muestran coordenadas y valores en la barra de estado.

## 8. Canales visibles

La lista `listWidget_Channels` permite controlar qué canales se ven en el gráfico.

- Doble clic sobre un canal lo oculta o lo vuelve a mostrar.
- El botón `Show All Incoming Data` vuelve a habilitar la visualización de todo el flujo recibido.
- El botón `Reset All Visible` restablece la visibilidad completa.

La visibilidad de cada canal se sincroniza con el gráfico y con el panel de canales para que el estado sea consistente.

## 9. Texto UART

La ventana de texto UART muestra el contenido recibido por serial.

- `Esconder Caja de Texto` oculta o muestra el panel.
- `Show All Incoming Data` alterna entre mostrar todo el flujo recibido o solo el contenido procesado.

Esto permite usar la vista UART como apoyo de diagnóstico sin interferir con el gráfico.

## 10. Grabación en CSV

La acción `Record stream` activa o desactiva la grabación del flujo entrante en CSV.

- Cuando está activada, la aplicación abre un archivo CSV.
- Cada muestra recibida se guarda junto con el índice real del punto del gráfico.
- Si la grabación no puede abrirse, la acción se desactiva automáticamente.
- Al desconectar o cerrar la sesión, el archivo se cierra.
- Cada vez que se pausa o reanuda la adquisición con `Pausa/Reanuda`, se agrega una línea de comentario (`# Pausa en t=...` / `# Reanudado en t=...`) al CSV, para dejar constancia del hueco temporal en los datos.

Nota: el encabezado y metadatos del CSV incluyen la versión de la aplicación (`v2.3.0`). El índice usado para cada fila corresponde al contador de muestras del ploteo (`dataPointCount`), que se sincroniza con lo mostrado en pantalla.

## 11. Mensajes de estado

La barra inferior muestra el estado operativo de la aplicación.

Los estados principales son:

- `Desconectado`
- `Listo para configurar`
- `Listo para ejecutar`
- `Adquiriendo`
- `Pausado`
- `Falla`

El mensaje cambia según la etapa del flujo y ayuda a saber qué acción corresponde hacer después.

## 12. Trabajo con perfiles

La aplicación incluye soporte interno para perfiles operativos.

- `saveProfile()` guarda la configuración actual.
- `loadProfile()` recupera una configuración previa.
- `deleteProfile()` elimina un perfil guardado.
- `getProfileNames()` lista los perfiles disponibles.

Los perfiles guardan la matriz activa, el ancho de pulso, los delays y el tiempo configurado.

## 13. Recuperación ante fallas

Si la aplicación entra en estado `Falla`, la lógica interna puede registrar el evento y preparar una recuperación basada en la última conexión válida.

- Se conservan los parámetros de conexión usados anteriormente.
- Se limpia el estado local de la matriz y del gráfico.
- Se restablecen las métricas de salud.

## 14. Formato de datos esperado

La aplicación espera mensajes que comiencen con `$` y terminen con `;`.

Ejemplo:

```c
printf("$%d %d;", data1, data2);
```

Los valores pueden ser enteros o decimales, según el formato que se esté enviando.

## 15. Flujo recomendado de uso

1. Conectar el puerto.
2. Elegir la matriz de datos.
3. Ajustar tiempos y delays.
4. Presionar `Enviar Datos`.
5. Presionar `Connect` para iniciar la adquisición.
6. Usar `Pause` si necesitás frenar sin cerrar el puerto.
7. Usar `Disconnect` al terminar.

## 16. Atajos visuales útiles

- `How to use` abre la ventana de ayuda.
- `Clear` limpia los datos visibles.
- `Save PNG` exporta el gráfico.
- `Record stream` habilita la grabación de datos.

Este manual describe el flujo operativo disponible en la versión `v2.3.0` de la aplicación.

## 17. Notas de rendimiento y riesgos conocidos

- Repintado frecuente: bajo cargas altas de datos el repintado del gráfico puede afectar el rendimiento y la responsividad; para mitigar, reduzca la cantidad de puntos visibles o aumente el intervalo de repintado.
- Telemetría de paquetes inválidos: los paquetes no numéricos se ignoran y se registran para diagnóstico; actualmente no hay bloqueo preventivo automático sobre la adquisición.
- Empaquetado: verifique que el instalador incluya los recursos y dependencias (licencias) al generar instaladores en `build/installer`.