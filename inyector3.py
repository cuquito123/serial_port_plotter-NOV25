"""
inyector3.py — Simulador de trama FPGA para MPCC — Multi-Photon Coincidence Counter (CIOp) v2.3.0
=====================================================================
Emula el formato de trama que manda la FPGA:
    $D C B A col1 col2 col3 col4 0 0 0 0;

Cada valor es una tasa instantánea:
  - Canales físicos (A,B,C,D): 1 si la señal está en HIGH, 0 si está en LOW.
  - Combinaciones (col1..col4): cantidad de muestras internas en las que
    TODOS los canales de esa combinación estuvieron en HIGH simultáneamente,
    durante el último intervalo de envío.

Configuración
─────────────
Editá la sección CONFIG para ajustar:
  - Puerto y baudrate
  - Frecuencia y fase de cada canal
  - Qué combinaciones simular (col1..col4)
  - Velocidad de envío y resolución interna
"""

import time
import math
import serial

# ══════════════════════════════════════════════════════════════════════════════
#  CONFIG — editá acá
# ══════════════════════════════════════════════════════════════════════════════

PORT  = "COM4"       # Puerto serie
BAUD  = 115200       # Baudrate

# Frecuencia de envío al Qt (tramas por segundo)
SEND_HZ = 20

# Resolución interna: cuántas muestras se calculan por cada trama enviada.
# Mayor resolución = conteos de coincidencia más precisos.
# Ej: SEND_HZ=20, RESOLUTION=50 → se calculan 50 muestras internas por trama.
RESOLUTION = 50

# ── Canales físicos ───────────────────────────────────────────────────────────
# freq_hz : frecuencia de la onda cuadrada
# duty    : fracción del período en HIGH (0.0 a 1.0)
# phase   : desfase en segundos
CANALES = {
    "A": {"freq_hz": 0.5,  "duty": 0.5, "phase": 0.00},
    "B": {"freq_hz": 0.5,  "duty": 0.5, "phase": 0.50},  # desfasado 0.5s respecto a A
    "C": {"freq_hz": 1.0,  "duty": 0.5, "phase": 0.00},
    "D": {"freq_hz": 1.0,  "duty": 0.5, "phase": 0.25},  # desfasado 0.25s respecto a C
}

# ── Combinaciones (col1..col4) ────────────────────────────────────────────────
# Corresponden a las columnas 5-8 de la grilla de botones en Qt.
# trama[4] → combinación columna 5
# trama[5] → combinación columna 6
# trama[6] → combinación columna 7
# trama[7] → combinación columna 8
# Poné None si esa columna no tiene combinación (mandará 0).
COMBINACIONES = {
    "col1": ["A", "B"],        # columna 5: coincidencia A&B
    "col2": ["A", "B", "C"],   # columna 6: coincidencia A&B&C
    "col3": ["C", "D"],        # columna 7: coincidencia C&D
    "col4": None,              # columna 8: sin combinación
}

# ══════════════════════════════════════════════════════════════════════════════
#  LÓGICA — no hace falta editar abajo
# ══════════════════════════════════════════════════════════════════════════════

def onda_cuadrada(t, freq_hz, duty, phase):
    """Devuelve 1 (HIGH) o 0 (LOW) para el instante t."""
    if freq_hz == 0:
        return 0
    periodo = 1.0 / freq_hz
    t_local = math.fmod(t - phase, periodo)
    if t_local < 0:
        t_local += periodo
    return 1 if t_local < (duty * periodo) else 0


def calcular_trama(t_inicio, dt_interno):
    """
    Calcula los valores de una trama completa.
    Evalúa RESOLUTION muestras internas y devuelve:
      - valor instantáneo al final del intervalo para A,B,C,D (0 o 1)
      - conteo de coincidencias para cada combinación
    """
    conteos = {k: 0 for k in COMBINACIONES}
    ultimo = {}

    for s in range(RESOLUTION):
        t = t_inicio + s * dt_interno

        # Estado instantáneo de cada canal
        estado = {
            nombre: onda_cuadrada(t, cfg["freq_hz"], cfg["duty"], cfg["phase"])
            for nombre, cfg in CANALES.items()
        }
        ultimo = estado  # guardamos el último para el valor instantáneo

        # Evaluar cada combinación
        for col, canales in COMBINACIONES.items():
            if canales is None:
                continue
            if all(estado[c] == 1 for c in canales):
                conteos[col] += 1

    return ultimo, conteos


def main():
    dt_envio    = 1.0 / SEND_HZ
    dt_interno  = dt_envio / RESOLUTION

    print(f"Conectando a {PORT} @ {BAUD} baud...")
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
    except serial.SerialException as e:
        print(f"Error al abrir puerto: {e}")
        return

    print(f"Enviando tramas a {SEND_HZ} Hz  (resolución interna: {RESOLUTION} muestras/trama)")
    print("Presioná Ctrl+C para detener.\n")

    t = 0.0
    try:
        while True:
            t0_real = time.perf_counter()

            estado, conteos = calcular_trama(t, dt_interno)

            # Orden de la trama: D C B A col1 col2 col3 col4 0 0 0 0
            valores = [
                estado["D"],
                estado["C"],
                estado["B"],
                estado["A"],
                conteos["col1"],
                conteos["col2"],
                conteos["col3"],
                conteos["col4"],
                0, 0, 0, 0,   # los 4 canales vacíos que manda el FPGA
            ]

            trama = "$" + " ".join(str(v) for v in valores) + ";"
            ser.write((trama + "\r\n").encode("ascii"))

            print(f"t={t:.2f}s  {trama}")

            t += dt_envio

            # Mantener cadencia exacta
            elapsed = time.perf_counter() - t0_real
            sleep_t = dt_envio - elapsed
            if sleep_t > 0:
                time.sleep(sleep_t)

    except KeyboardInterrupt:
        print("\nDetenido.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
