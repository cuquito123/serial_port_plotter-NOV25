#include "fpgaprotocol.hpp"

// Constructor: inicializa el estado de la matriz de teclas y la columna seleccionada.
// Se llama a resetMatrix() para poner todos los valores en cero.
FpgaProtocol::FpgaProtocol()
    : m_tecla(32),
      m_selectedColumn(0)
{
    resetMatrix();
}

// Resetea el estado de la matriz de teclas y la configuración interna.
// También reinicia la columna seleccionada a la columna 0.
void FpgaProtocol::resetMatrix()
{
    m_tecla.fill(false);
    m_selectedColumn = 0;
    m_config[0] = 0;
    m_config[1] = 0;
    m_config[2] = 0;
    m_config[3] = 0;
    m_config[4] = 0;
}

// Selecciona una columna válida entre 0 y 7.
// Si el índice está fuera de rango, no hace ningún cambio.
void FpgaProtocol::selectColumn(int columnIndex)
{
    if (columnIndex >= 0 && columnIndex < 8) {
        m_selectedColumn = columnIndex;
    }
}

// Alterna el estado de un botón en la matriz de teclas si el índice es válido.
void FpgaProtocol::toggleButton(int bit)
{
    if (bit >= 0 && bit < m_tecla.size()) {
        m_tecla.toggleBit(bit);
    }
}

// Establece explícitamente el estado de un botón en true o false.
void FpgaProtocol::setButton(int bit, bool active)
{
    if (bit >= 0 && bit < m_tecla.size()) {
        m_tecla.setBit(bit, active);
    }
}

// Devuelve el estado actual de un botón. Si el índice no es válido, retorna false.
bool FpgaProtocol::buttonState(int bit) const
{
    if (bit >= 0 && bit < m_tecla.size()) {
        return m_tecla.testBit(bit);
    }
    return false;
}

// Ajusta el ancho de pulso en la configuración interna.
void FpgaProtocol::setPulseWidth(quint8 value)
{
    m_config[0] = value;
}

// Ajusta el retardo A en la configuración interna.
void FpgaProtocol::setDelayA(quint8 value)
{
    m_config[1] = value;
}

// Ajusta el retardo B en la configuración interna.
void FpgaProtocol::setDelayB(quint8 value)
{
    m_config[2] = value;
}

// Ajusta el retardo C en la configuración interna.
void FpgaProtocol::setDelayC(quint8 value)
{
    m_config[3] = value;
}

// Ajusta el retardo D en la configuración interna.
void FpgaProtocol::setDelayD(quint8 value)
{
    m_config[4] = value;
}

// Genera etiquetas de columnas activas con los identificadores A/B/C/D.
// Omite las columnas que no tienen ningún botón activo.
QStringList FpgaProtocol::generateLabels() const
{
    QStringList labels;

    for (int col = 7; col >= 0; col--) {
        bool bA = m_tecla.testBit(col * 4 + 0);
        bool bB = m_tecla.testBit(col * 4 + 1);
        bool bC = m_tecla.testBit(col * 4 + 2);
        bool bD = m_tecla.testBit(col * 4 + 3);

        if (!bA && !bB && !bC && !bD) {
            continue;
        }

        QStringList activos;
        if (bA) activos << "A";
        if (bB) activos << "B";
        if (bC) activos << "C";
        if (bD) activos << "D";
        labels << activos.join("&");
    }

    return labels;
}

// Devuelve los índices de trama activos según las columnas que contienen al menos un botón activo.
// El índice se calcula como 7 - col para mantener el orden de tramas esperado.
QVector<int> FpgaProtocol::activeTramaIndices() const
{
    QVector<int> indices;
    for (int col = 7; col >= 0; col--) {
        bool bA = m_tecla.testBit(col * 4 + 0);
        bool bB = m_tecla.testBit(col * 4 + 1);
        bool bC = m_tecla.testBit(col * 4 + 2);
        bool bD = m_tecla.testBit(col * 4 + 3);
        if (!bA && !bB && !bC && !bD) {
            continue;
        }
        indices << (7 - col);
    }
    return indices;
}

// Llena los vectores de mapeo CSV con el índice de trama y la etiqueta por columna.
// Para columnas inactivas deja etiqueta vacía y -1 como índice.
void FpgaProtocol::fillCsvMapping(QVector<int> &csvTramaIdx, QStringList &csvLabels) const
{
    csvTramaIdx.clear();
    csvLabels.clear();
    for (int col = 0; col < 8; col++) {
        bool bA = m_tecla.testBit(col * 4 + 0);
        bool bB = m_tecla.testBit(col * 4 + 1);
        bool bC = m_tecla.testBit(col * 4 + 2);
        bool bD = m_tecla.testBit(col * 4 + 3);
        if (bA || bB || bC || bD) {
            QStringList activos;
            if (bA) activos << "A";
            if (bB) activos << "B";
            if (bC) activos << "C";
            if (bD) activos << "D";
            csvLabels << activos.join("&");
            csvTramaIdx << (7 - col);
        } else {
            csvLabels << "";
            csvTramaIdx << -1;
        }
    }
}

// Construye un paquete extendido de 47 bytes que contiene:
// - estado de los 32 botones
// - configuración de 5 bytes
// - estado y número de columna seleccionada
// - el tiempo normalizado en formato ASCII de 8 dígitos.
QByteArray FpgaProtocol::buildExtendedPacket(quint32 timeValue, int timeUnitIndex) const
{
    quint32 baseTime = convertToBase(static_cast<int>(timeValue), timeUnitIndex);
    baseTime = normalizeTimeValue(baseTime);

    QByteArray packet(47, 0);
    for (int b = 0; b < 32; ++b) {
        packet[b] = m_tecla.testBit(b) ? char(0x01) : char(0x00);
    }
    for (int i = 0; i < 5; ++i) {
        packet[32 + i] = static_cast<char>(m_config[i]);
    }
    packet[37] = static_cast<char>(buildColumnState(m_selectedColumn) + 0x30);
    packet[38] = static_cast<char>(m_selectedColumn + 0x30);

    QVector<quint8> digits = decomposeNumber(baseTime);
    for (int i = 0; i < digits.size() && (39 + i) < packet.size(); ++i) {
        packet[39 + i] = static_cast<char>(digits[i]);
    }

    return packet;
}

// Construye un paquete de reinicio de barrido (reset sweep) de 38 bytes con la configuración actual.
QByteArray FpgaProtocol::buildResetSweepPacket() const
{
    QByteArray packet(38, 0);
    for (int i = 0; i < 5; ++i) {
        packet[32 + i] = static_cast<char>(m_config[i]);
    }
    return packet;
}

// Comando de reinicio simple de un byte.
QByteArray FpgaProtocol::buildResetCommand() const
{
    return QByteArray(1, char(0x5F));
}

// Comando de inicio simple de un byte.
QByteArray FpgaProtocol::buildStartCommand() const
{
    return QByteArray(1, char(0x23));
}

// Convierte un valor de tiempo desde una unidad seleccionada a la unidad base interna.
// Las unidades pueden ser: 0=µs, 1=ms, 2=s, 3=min, 4=hs.
quint32 FpgaProtocol::convertToBase(int valor, int indiceUnidad) const
{
    quint64 calculo = 0;
    switch (indiceUnidad) {
    case 0:
        calculo = static_cast<quint64>(valor) / 100;
        break;
    case 1:
        calculo = static_cast<quint64>(valor) * 10;
        break;
    case 2:
        calculo = static_cast<quint64>(valor) * 10000;
        break;
    case 3:
        calculo = static_cast<quint64>(valor) * 600000;
        break;
    case 4:
        calculo = static_cast<quint64>(valor) * 36000000;
        break;
    default:
        calculo = 0;
        break;
    }
    return static_cast<quint32>(calculo);
}

// Convierte un valor de la unidad base interna a la unidad seleccionada.
quint32 FpgaProtocol::convertFromBase(quint32 numeroBase, int indiceUnidad) const
{
    switch (indiceUnidad) {
    case 0: return numeroBase * 100;      // µs
    case 1: return numeroBase / 10;       // ms
    case 2: return numeroBase / 10000;    // s
    case 3: return numeroBase / 600000;   // min
    case 4: return numeroBase / 36000000; // hs
    default: return 0;
    }
}

// Normaliza el valor de tiempo para que sea múltiplo de 8.
// Esto asegura compatibilidad con la trama de tiempo esperada.
quint32 FpgaProtocol::normalizeTimeValue(quint32 timeValue) const
{
    quint32 remainder = timeValue % 8;
    if (remainder != 0) {
        return timeValue - remainder;
    }
    return timeValue;
}

// Descompone un número en 8 dígitos ASCII, rellenando con ceros a la izquierda.
QVector<quint8> FpgaProtocol::decomposeNumber(quint32 numero) const
{
    QVector<quint8> digitos(8, '0');
    for (int i = 7; i >= 0; --i) {
        if (numero == 0) {
            break;
        }
        digitos[i] = static_cast<quint8>((numero % 10) + 0x30);
        numero /= 10;
    }
    return digitos;
}

// Construye el estado de una columna en un byte de 4 bits con el orden D C B A.
quint8 FpgaProtocol::buildColumnState(int columnIndex) const
{
    if (columnIndex < 0 || columnIndex > 7) {
        return 0;
    }
    bool bA = m_tecla.testBit(columnIndex * 4 + 0);
    bool bB = m_tecla.testBit(columnIndex * 4 + 1);
    bool bC = m_tecla.testBit(columnIndex * 4 + 2);
    bool bD = m_tecla.testBit(columnIndex * 4 + 3);
    return static_cast<quint8>((bD << 3) | (bC << 2) | (bB << 1) | (bA << 0));
}
