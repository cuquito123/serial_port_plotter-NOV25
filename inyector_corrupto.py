"""
inyector_corrupto.py — Simulador de trama FPGA con inyección de tramas corruptas (CIOp) v2.3.0
=====================================================================
Variante de inyector3.py pensada para probar la robustez del parser de
Serial Port Plotter frente a datos malformados.

Manda tramas válidas con el mismo formato que la FPGA:
    $D C B A col1 col2 col3 col4 0 0 0 0;

pero cada cierto intervalo reemplaza la trama por una versión corrupta,
eligiendo al azar entre varios tipos de corrupción (campos faltantes,
campos de más, texto no numérico, delimitadores rotos, basura binaria,
trama truncada, etc.).

Configuración
─────────────
Editá la sección CONFIG para ajustar:
  - Puerto y baudrate
  - Frecuencia y fase de cada canal
  - Qué combinaciones simular (col1..col4)
  - Cada cuánto se inyecta una trama corrupta y qué tipos se usan
"""

import time
import math
import random
import serial

# ══════════════════════════════════════════════════════════════════════════════
#  CONFIG — editá acá
# ══════════════════════════════════════════════════════════════════════════════

PORT  = "COM4"       # Puerto serie
BAUD  = 115200       # Baudrate

# Frecuencia de envío al Qt (tramas por segundo)
SEND_HZ = 20

# Resolución interna: cuántas muestras se calculan por cada trama enviada.
RESOLUTION = 50

# ── Canales físicos ───────────────────────────────────────────────────────────
CANALES = {
    "A": {"freq_hz": 0.5,  "duty": 0.5, "phase": 0.00},
    "B": {"freq_hz": 0.5,  "duty": 0.5, "phase": 0.50},
    "C": {"freq_hz": 1.0,  "duty": 0.5, "phase": 0.00},
    "D": {"freq_hz": 1.0,  "duty": 0.5, "phase": 0.25},
}

# ── Combinaciones (col1..col4) ────────────────────────────────────────────────
COMBINACIONES = {
    "col1": ["A", "B"],
    "col2": ["A", "B", "C"],
    "col3": ["C", "D"],
    "col4": None,
}

# ── Inyección de corrupción ───────────────────────────────────────────────────
# Cada cuántos segundos se manda una trama corrupta en lugar de una normal.
CORRUPT_INTERVAL_S = 5.0

# Tipos de corrupción a usar (se elige uno al azar cada vez que toca corromper).
# Comentá los que no quieras probar.
CORRUPT_TYPES = [
    "campos_faltantes",     # menos campos de los esperados
    "campos_de_mas",        # más campos de los esperados
    "texto_no_numerico",    # letras en vez de números
    "sin_dolar",             # falta el "$" inicial
    "sin_punto_y_coma",     # falta el ";" final
    "basura_binaria",       # bytes aleatorios no imprimibles
    "trama_truncada",       # se corta a mitad de trama, sin ";"
    "separadores_raros",    # comas/tabs en vez de espacios
    "valores_fuera_rango",  # números negativos / gigantes
    "trama_vacia",          # solo "$;" o línea en blanco
    "doble_trama_pegada",   # dos tramas concatenadas sin separación clara
]

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
    """Calcula los valores de una trama completa (igual que inyector3.py)."""
    conteos = {k: 0 for k in COMBINACIONES}
    ultimo = {}

    for s in range(RESOLUTION):
        t = t_inicio + s * dt_interno

        estado = {
            nombre: onda_cuadrada(t, cfg["freq_hz"], cfg["duty"], cfg["phase"])
            for nombre, cfg in CANALES.items()
        }
        ultimo = estado

        for col, canales in COMBINACIONES.items():
            if canales is None:
                continue
            if all(estado[c] == 1 for c in canales):
                conteos[col] += 1

    return ultimo, conteos


def trama_valida(estado, conteos):
    """Arma la trama normal: $D C B A col1 col2 col3 col4 0 0 0 0;"""
    valores = [
        estado["D"], estado["C"], estado["B"], estado["A"],
        conteos["col1"], conteos["col2"], conteos["col3"], conteos["col4"],
        0, 0, 0, 0,
    ]
    return "$" + " ".join(str(v) for v in valores) + ";"


def trama_corrupta(estado, conteos, tipo):
    """Genera una trama corrupta del tipo indicado, a partir de datos válidos."""
    valores = [
        estado["D"], estado["C"], estado["B"], estado["A"],
        conteos["col1"], conteos["col2"], conteos["col3"], conteos["col4"],
        0, 0, 0, 0,
    ]

    if tipo == "campos_faltantes":
        n = random.randint(0, len(valores) - 1)
        recortados = valores[:n]
        return "$" + " ".join(str(v) for v in recortados) + ";"

    if tipo == "campos_de_mas":
        extra = valores + [random.randint(0, 999) for _ in range(random.randint(1, 8))]
        return "$" + " ".join(str(v) for v in extra) + ";"

    if tipo == "texto_no_numerico":
        basura = list(valores)
        palabras = ["NaN", "ERROR", "xx", "####", "null", "--"]
        for _ in range(random.randint(1, 3)):
            idx = random.randint(0, len(basura) - 1)
            basura[idx] = random.choice(palabras)
        return "$" + " ".join(str(v) for v in basura) + ";"

    if tipo == "sin_dolar":
        return " ".join(str(v) for v in valores) + ";"

    if tipo == "sin_punto_y_coma":
        return "$" + " ".join(str(v) for v in valores)

    if tipo == "basura_binaria":
        n = random.randint(6, 20)
        return "".join(chr(random.randint(0, 31)) for _ in range(n))

    if tipo == "trama_truncada":
        corte = random.randint(1, len(valores) - 1)
        return "$" + " ".join(str(v) for v in valores[:corte])

    if tipo == "separadores_raros":
        sep = random.choice([",", "\t", ";", "|"])
        return "$" + sep.join(str(v) for v in valores) + ";"

    if tipo == "valores_fuera_rango":
        raros = list(valores)
        for _ in range(random.randint(1, 3)):
            idx = random.randint(0, len(raros) - 1)
            raros[idx] = random.choice([-999999, 999999999, -1])
        return "$" + " ".join(str(v) for v in raros) + ";"

    if tipo == "trama_vacia":
        return random.choice(["$;", "", "$"])

    if tipo == "doble_trama_pegada":
        t1 = "$" + " ".join(str(v) for v in valores) + ";"
        t2 = "$" + " ".join(str(v) for v in valores) + ";"
        return t1 + t2

    # fallback, no debería llegar acá
    return trama_valida(estado, conteos)


def main():
    if not CORRUPT_TYPES:
        print("CORRUPT_TYPES está vacío, no hay nada para inyectar. Agregá al menos un tipo.")
        return

    dt_envio    = 1.0 / SEND_HZ
    dt_interno  = dt_envio / RESOLUTION

    print(f"Conectando a {PORT} @ {BAUD} baud...")
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
    except serial.SerialException as e:
        print(f"Error al abrir puerto: {e}")
        return

    print(f"Enviando tramas a {SEND_HZ} Hz  (resolución interna: {RESOLUTION} muestras/trama)")
    print(f"Inyectando trama corrupta cada {CORRUPT_INTERVAL_S:.1f}s, tipos: {', '.join(CORRUPT_TYPES)}")
    print("Presioná Ctrl+C para detener.\n")

    t = 0.0
    t_ultima_corrupcion = -CORRUPT_INTERVAL_S  # fuerza corrupción temprana opcional
    try:
        while True:
            t0_real = time.perf_counter()

            estado, conteos = calcular_trama(t, dt_interno)

            if t - t_ultima_corrupcion >= CORRUPT_INTERVAL_S:
                tipo = random.choice(CORRUPT_TYPES)
                trama = trama_corrupta(estado, conteos, tipo)
                t_ultima_corrupcion = t
                etiqueta = f"[CORRUPTA:{tipo}]"
            else:
                trama = trama_valida(estado, conteos)
                etiqueta = ""

            # Los tipos "basura_binaria"/"trama_vacia" pueden no ser ASCII limpio;
            # se codifica igual, ignorando caracteres problemáticos.
            ser.write((trama + "\r\n").encode("ascii", errors="ignore"))

            print(f"t={t:.2f}s {etiqueta} {trama!r}")

            t += dt_envio

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
