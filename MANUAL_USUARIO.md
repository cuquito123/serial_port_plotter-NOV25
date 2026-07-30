# Manual de Usuario — MPCC — Multi-Photon Coincidence Counter (CIOp) v3.0.0

Software de instrumentación para el detector de fotones en coincidencias múltiples basado en FPGA.
Centro de Investigaciones Ópticas (CIOp) — CONICET · CIC-PBA · UNLP.

> Este archivo es la versión de consulta rápida que acompaña al ejecutable y se abre desde
> **Ayuda → Manual de Usuario**. La versión completa, con figuras, es el documento
> *Manual de Usuario — MPCC — Multi-Photon Coincidence Counter (CIOp) v3.0.0* (formato Word).

---

## 1. Qué es esta aplicación

MPCC comanda y monitorea un detector de fotones en coincidencias múltiples
implementado sobre una placa FPGA DE0 Nano SoC (Altera/Intel) programada en VHDL, que se
comunica con la PC por puerto serie mediante protocolo UART/RS232.

El dispositivo opera con 12 canales: cuatro para el conteo individual de pulsos de entrada y
el resto para el conteo de coincidencias, de hasta cuatro canales en simultáneo.

Desde la aplicación se puede:

- Configurar qué canales se habilitan y entre cuáles se determinan coincidencias.
- Ajustar el ancho de pulso y los retardos independientes de los canales A, B, C y D.
- Definir la ventana de integración y la duración total del experimento.
- Visualizar en tiempo real los conteos individuales y en coincidencia.
- Registrar la adquisición en CSV para su procesamiento posterior.
- Guardar y recuperar configuraciones completas mediante perfiles.

---

## 2. Anatomía de la pantalla

| Zona | Contenido |
|---|---|
| Barra de menús | Puerto Serial · Visualización · Grabación & Exportación · Ayuda |
| Barra de herramientas | Conectar, Pausa/Reanuda y Desconectar, en modo solo iconos |
| Área de gráfico | Curvas en tiempo real, leyenda de canales y botón lateral `AutoScale Y` |
| Panel de configuración | Tiempo, duración, ventana de texto UART, `Enviar Datos`, `Reset`, ancho de pulso, retardos A–D, matriz de canales y selectores de columna |
| Barra de estado | Mensaje de estado, coordenadas del cursor, tiempo transcurrido y restante |

La acción **Visualización → Panel de configuración** (`Ctrl+Tab`) alterna hacia una segunda
vista con controles complementarios: selectores de puerto (con botón `Refresh` para releer
los puertos disponibles), controles finos del gráfico (cantidad de puntos, paso y límites del
eje vertical), `Save PNG` (disponible con el puerto abierto), `Reset All Visible` y la lista
de canales visibles. Incluye además dos botones rotulados en inglés — `Show All Incoming
Data` y `Hide TextBox` — que duplican las acciones del menú Visualización, y el botón
`Ir a gráfico` para volver a la vista principal. Para el flujo habitual esta segunda vista
no es necesaria.

---

## 3. Conexión al puerto serie

### 3.1. Configurar el puerto

**Puerto Serial → Propiedades de Puerto…**

| Parámetro | Valor habitual |
|---|---|
| Puerto | COM4 |
| Baudios | 115200 |
| Bits de datos | 8 bits |
| Paridad | none |
| Bits de parada | 1 bit |

Si la aplicación ya estaba conectada, los cambios se aplican en la próxima reconexión.

### 3.2. Abrir el puerto

Mediante **Puerto Serial → Conectar**, el primer botón de la barra de herramientas.

Al conectar, la matriz de canales y la selección de columna se reinician por completo. La
configuración del experimento se realiza siempre después de abrir el puerto; antes de
conectar, esos controles permanecen deshabilitados.

---

## 4. Configuración del experimento

### 4.1. Matriz de canales

Cuatro filas (A, B, C, D) por ocho columnas: 32 posiciones, cada una asociada a un bit de
configuración que se transmite al FPGA.

| Color | Significado |
|---|---|
| Verde | Canal habilitado |
| Rojo | Canal deshabilitado |
| Gris | Canal sin configurar, equivalente a canal deshabilitado |
| Atenuado | El control no está disponible en el estado operativo actual |

### 4.2. Selectores de columna

La fila de botones numerados del 1 al 8, situada sobre la matriz, es un parámetro de testing
para la FPGA: el botón presionado se transmite como parte de la trama de configuración, pero
no afecta a los resultados del experimento ni interfiere en la representación gráfica o el
guardado de datos. Su disponibilidad sigue exactamente la de la matriz de canales (4.1):
deshabilitados sin puerto conectado, durante la adquisición y durante toda la pausa, haya o
no grabación activa.

### 4.3. Parámetros temporales

| Control | Función |
|---|---|
| Unidad y valor de tiempo | Ventana de integración. Al cambiar la unidad, el valor se convierte automáticamente. El valor transmitido se ajusta hacia abajo al múltiplo de 8 de la unidad base que requiere el hardware ( 1 unidad base = 100 uS) |
| Ancho de Pulso | Ancho de los pulsos, en unidades del hardware. Rango: 0 a 255 |
| Delay: Channel A–D | Retardo independiente por canal de entrada, en unidades del hardware. Rango: 0 a 255 |
| Duración (exp) | Duración total del experimento; si es mayor que cero, exige grabación en CSV |

El hardware admite ventanas de integración entre 5,6 ms y 99.999.999 unidades base. El valor de 5.6 mS surge de valores experimentales.

Igual que la matriz de canales, estos controles quedan deshabilitados sin puerto conectado,
durante la adquisición y durante toda la pausa: `Pausa/Reanuda` solo pausa o reanuda la
adquisición y el guardado en curso, no habilita reconfigurar nada.

### 4.4. Botón Reset

El botón `Reset` reinicia el barrido en el FPGA y limpia la matriz y la configuración local.
Queda inhabilitado durante toda la pausa, haya o no grabación activa, igual que `Enviar
Datos`: resetear a mitad de un ciclo pausado invalidaría la adquisición en curso, así que
primero hay que reanudar. Después de usarlo hay que reconfigurar y presionar `Enviar Datos`
para iniciar un nuevo ciclo.

---

## 5. Aplicar la configuración

Todo cambio pendiente se señala de dos formas: el botón `Enviar Datos` se resalta en naranja
y la barra de estado avisa que hay cambios sin aplicar. `Enviar Datos` solo está disponible en
*Listo para configurar*; durante toda la pausa permanece gris e inutilizable —incluso si hay
cambios pendientes, el resaltado naranja se suprime— hasta reanudar la adquisición.

Al presionar `Enviar Datos`, la aplicación:

1. Abre el archivo CSV si se definió una duración de experimento.
2. Ejecuta el control previo.
3. Limpia el gráfico y genera las etiquetas de los canales activos.
4. Arma la trama extendida y la transmite al FPGA.
5. Pasa a *Adquiriendo* e inicia el cronómetro.

**Control previo.** Verifica que el puerto esté conectado, que haya al menos un canal activo,
que el tiempo sea mayor que cero, que la ventana esté dentro del rango admitido y que el CSV
esté abierto si la grabación está habilitada. Si alguna condición falla, la aplicación pasa a
*Falla*, informa el motivo y no transmite nada. Para recuperarse: desconectar, corregir y
repetir.

---

## 6. Adquisición

| Acción | Efecto | Atajo |
|---|---|---|
| Conectar | Abre el puerto y habilita la configuración | Reproducir |
| Pausa/Reanuda | Detiene o retoma la adquisición sin cerrar el puerto | Pausa |
| Desconectar | Cierra el puerto, detiene el cronómetro y cierra el CSV | Detener |

`Pausa/Reanuda` es exclusivamente eso: pausa o retoma la adquisición y el guardado en curso,
sin cerrar el puerto. No habilita ninguna otra acción. Durante toda la pausa —haya o no
grabación activa— quedan bloqueados por igual la matriz de canales, los selectores 1–8, el
tiempo de integración, el ancho de pulso, los retardos A–D, `Enviar Datos` y `Reset` (ver 7):
para tocar cualquiera de ellos primero hay que reanudar. El cronómetro se detiene al pausar y
retoma la cuenta al reanudar, con o sin grabación activa.

Si se configuró una duración, al alcanzarla la aplicación finaliza el experimento
automáticamente, cierra el CSV y lo informa. Para iniciar otro ciclo hay que reconfigurar y
presionar `Enviar Datos`; `Pausa/Reanuda` no reanuda un experimento finalizado.

---

## 7. Estados de la aplicación

| Estado | Cuándo ocurre |
|---|---|
| Desconectado | Al iniciar y después de cerrar el puerto |
| Listo para configurar | Inmediatamente después de abrir el puerto |
| Listo para ejecutar | Instante entre transmitir la configuración e iniciar la adquisición (transitorio) |
| Adquiriendo | Mientras se reciben datos |
| Pausado | Al presionar Pausa, o al finalizar el experimento por duración |
| Falla | Control previo fallido, o degradación severa de la comunicación |

### Controles disponibles en cada estado

| Control | Desconect. | Listo p/ config. | Adquiriendo | Pausado |
|---|:--:|:--:|:--:|:--:|
| Selectores de puerto | Sí | No | No | No |
| Conectar | Sí | No | No | No |
| Desconectar | No | Sí | Sí | Sí |
| Pausa/Reanuda | No | No | Sí | Sí |
| Matriz de canales, selectores 1–8 | No | Sí | No | No |
| Tiempo, ancho de pulso, retardos A–D | No | Sí | No | No |
| Enviar Datos | No | Sí | No | No |
| Reset | No | Sí | No | No |
| Grabar Stream (CSV) | No | No | Sí | Sí |

Los controles deshabilitados se muestran atenuados, de modo que su aspecto siempre coincide
con su disponibilidad real. En *Pausado* queda bloqueado todo el panel de configuración —
matriz de canales, selectores 1–8, tiempo de integración, ancho de pulso, retardos A–D,
*Enviar Datos* y *Reset*—, sin importar si hay grabación en curso: `Pausa/Reanuda` pausa y
reanuda únicamente la adquisición y el guardado, no habilita reconfigurar nada. Para volver a
tocar cualquiera de esos controles hay que reanudar primero (o, tras un fin natural del
experimento por duración, reconfigurar desde cero con `Enviar Datos`).

---

## 8. Visualización

| Control | Ubicación | Función |
|---|---|---|
| `AutoScale Y` | Botón lateral del gráfico | Ajusta el eje vertical al contenido visible |
| AutoScale en Y | Visualización → Controles del Gráfico | Misma función, desde el menú |
| Limpiar Gráfico | Visualización → Controles del Gráfico | Borra las curvas |
| Rueda del mouse | Sobre el gráfico | Zoom horizontal (eje de tiempo) |
| Arrastre | Sobre el gráfico | Desplazamiento horizontal; el eje vertical se ajusta con `AutoScale Y` o desde el panel de configuración |
| Movimiento del cursor | Sobre el gráfico | Muestra coordenadas en la barra de estado |

En el panel de configuración (`Ctrl+Tab`), un doble clic sobre un canal de la lista lo
oculta o lo vuelve a mostrar en el gráfico; `Reset All Visible` restablece todos.

**Ventana de texto UART.** Muestra el flujo recibido por el puerto serie como apoyo de
diagnóstico. Se controla con *Mostrar Caja de Texto* y *Mostrar Todos los Datos*, en el menú
Visualización.

---

## 9. Grabación y exportación

- **Grabar Stream (CSV)** (`Ctrl+S`): activa o desactiva el registro del flujo entrante. Con
  una duración configurada, la grabación se habilita automáticamente y es obligatoria.
- El encabezado del CSV incluye los metadatos del experimento y la versión de la aplicación.
  Cada muestra se registra con su marca de tiempo en segundos, derivada del ritmo de ploteo
  (20 muestras por segundo).
- Cada vez que se pausa o reanuda la adquisición con *Pausa/Reanuda*, se agrega una línea de
  comentario (`# Pausa en t=...` / `# Reanudado en t=...`) al CSV, para dejar constancia del
  hueco temporal en los datos.
- Pausar la adquisición bloquea siempre todo el panel de configuración —matriz de canales,
  selectores 1–8, tiempo de integración, ancho de pulso, retardos, *Enviar Datos* y
  *Reset*—, haya o no grabación activa: `Pausa/Reanuda` solo pausa y reanuda la adquisición y
  el guardado, nunca habilita reconfigurar. Para tocar cualquiera de esos controles hay que
  reanudar primero.
- Si la grabación no puede abrirse, la acción se desactiva automáticamente. Al desconectar o
  cerrar la sesión, el archivo se cierra.
- **Propiedades de grabación…**: informa el estado actual y ofrece acceso a la carpeta de
  documentos.
- **Exportar datos…**: vuelca a un archivo el contenido actual del gráfico.

---

## 10. Perfiles de experimento

Un perfil guarda la matriz de canales, el ancho de pulso, los cuatro retardos, el valor y la
unidad de tiempo, y la fecha de creación. Se almacenan como archivos JSON en la carpeta de
datos de la aplicación.

- **Guardar Perfil…**: pide un nombre y confirma antes de sobrescribir.
- **Cargar Perfil…**: aplica un perfil a la interfaz y lo marca como pendiente de aplicar, de
  modo que hay que presionar `Enviar Datos` para transmitirlo al FPGA.
- **Gestionar Perfiles…**: lista todos los perfiles con vista previa, y permite Cargar,
  Eliminar, Renombrar y Cerrar.

---

## 11. Menús y atajos

### Puerto Serial

| Ítem | Atajo |
|---|---|
| Conectar | Reproducir |
| Desconectar | Detener |
| Propiedades de Puerto… | — |
| Guardar Perfil… / Cargar Perfil… / Gestionar Perfiles… | — |
| Salir | `Ctrl+Q` |

### Visualización

| Ítem | Atajo |
|---|---|
| Mostrar Caja de Texto | — |
| Mostrar Todos los Datos | — |
| Panel de configuración | `Ctrl+Tab` |
| Pausa/Reanuda | Pausa |
| Controles del Gráfico → AutoScale en Y / Limpiar Gráfico | — |

### Grabación & Exportación

| Ítem | Atajo |
|---|---|
| Grabar Stream (CSV) | `Ctrl+S` |
| Exportar datos… | — |
| Propiedades de grabación… | — |

### Ayuda

| Ítem | Atajo |
|---|---|
| Cómo usar | `F1` |
| Manual de Usuario | — |
| Acerca de… | — |

---

## 12. Formato de datos esperado

Tramas que comienzan con `$` y terminan con `;`, con los valores separados por espacios:

```c
printf("$%d %d;", dato1, dato2);
```

Los valores pueden ser enteros o decimales. Los caracteres no válidos dentro de una trama se
eliminan en silencio, y las tramas con campos vacíos o no numéricos se contabilizan como
inválidas en la telemetría de salud; los campos numéricos que sobreviven pueden igualmente
llegar al gráfico y al CSV, por lo que ante advertencias de tramas inválidas conviene revisar
los datos registrados.

---

## 13. Barra de estado

| Indicador | Contenido |
|---|---|
| Coordenadas | Posición X e Y del cursor sobre el gráfico |
| Mensaje de estado | Estado operativo y acción sugerida, con color según la situación |
| `Exp:` | Tiempo transcurrido desde el inicio del experimento |
| `Restante:` | Tiempo que falta para completar la duración configurada |
| Salud de comunicación | Aparece solo ante degradación: naranja por encima del 10 % de tramas inválidas sostenido, rojo por encima del 25 % sostenido, caso en el que la aplicación pasa a *Falla* |

---

## 14. Resolución de problemas

| Síntoma | Causa probable | Solución |
|---|---|---|
| El panel de configuración (matriz, selectores 1–8, tiempo, duración del experimento, ancho de pulso, retardos) se ve atenuado y no responde | Solo es editable en *Listo para configurar*: se bloquea si el puerto no fue abierto, si hay una adquisición en curso, si está en pausa (siempre, haya o no grabación activa) o si la aplicación está en *Falla* | Conectar el puerto; presionar `Pausa/Reanuda` para reanudar; o desconectar y corregir si está en Falla |
| `Enviar Datos` (o `Reset`) se ve gris aunque haya cambios pendientes | Solo están habilitados en *Listo para configurar*. Fuera de ese estado —desconectado, adquiriendo, pausado o en falla— quedan grises y el resaltado naranja de cambios pendientes se suprime; esto puede pasar incluso sin haber pausado nunca, por ejemplo al cargar un perfil (*Cargar Perfil…*) antes de conectar el puerto | Conectar el puerto y/o presionar `Pausa/Reanuda` para volver a *Listo para configurar* |
| Advertencia naranja o roja de tramas inválidas | Comunicación degradada por cableado, ruido o parámetros de puerto | Revisar la conexión y los parámetros. Si llegó a rojo, desconectar, corregir y reconectar |
| La aplicación pasó a Falla al presionar `Enviar Datos` | El control previo detectó una condición inválida | Leer el motivo en la barra de estado, desconectar, corregir y repetir |
| No aparece el puerto de la placa | Placa desconectada o driver USB-serie ausente | Verificar la conexión física y el puerto COM asignado |
| No se puede iniciar con duración configurada | La grabación es obligatoria y el archivo no pudo abrirse | Verificar permisos de escritura en la carpeta de documentos |
| La configuración no llega al FPGA | Hay cambios pendientes | Presionar `Enviar Datos`; mientras esté naranja hay cambios sin transmitir |
| `Pausa/Reanuda` no reanuda | El experimento finalizó por duración alcanzada | Reconfigurar y presionar `Enviar Datos` para un nuevo ciclo |
