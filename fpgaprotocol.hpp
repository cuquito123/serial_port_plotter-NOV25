#ifndef FPGAPROTOCOL_HPP
#define FPGAPROTOCOL_HPP

#include <QBitArray>
#include <QByteArray>
#include <QStringList>
#include <QVector>

class FpgaProtocol
{
public:
    // Constructor: inicializa la matriz de teclas y el estado interno.
    explicit FpgaProtocol();

    // Restaura el estado por defecto de la matriz y la configuración.
    void resetMatrix();

    // Selecciona la columna activa entre 0 y 7. Ignora índices inválidos.
    void selectColumn(int columnIndex);

    // Alterna el estado del bit de tecla indicado.
    void toggleButton(int bit);

    // Fija el estado del bit de tecla en activo o inactivo.
    void setButton(int bit, bool active);

    // Devuelve el estado actual del bit de tecla; retorna false en caso de índice inválido.
    bool buttonState(int bit) const;

    // Ajusta el ancho de pulso de la configuración.
    void setPulseWidth(quint8 value);

    // Ajusta el valor del retardo A en la configuración.
    void setDelayA(quint8 value);

    // Ajusta el valor del retardo B en la configuración.
    void setDelayB(quint8 value);

    // Ajusta el valor del retardo C en la configuración.
    void setDelayC(quint8 value);

    // Ajusta el valor del retardo D en la configuración.
    void setDelayD(quint8 value);

    // Genera una lista de etiquetas para columnas activas con combinaciones A/B/C/D.
    QStringList generateLabels() const;

    // Devuelve los índices de trama activos en el orden esperado por el protocolo.
    QVector<int> activeTramaIndices() const;

    // Llena el mapeo CSV de índices de trama y etiquetas por columna.
    void fillCsvMapping(QVector<int> &csvTramaIdx, QStringList &csvLabels) const;

    // Construye el paquete extendido completo para enviar al FPGA.
    QByteArray buildExtendedPacket(quint32 timeValue, int timeUnitIndex) const;

    // Construye el paquete de reinicio de barrido con la configuración actual.
    QByteArray buildResetSweepPacket() const;

    // Construye el comando simple de reinicio.
    QByteArray buildResetCommand() const;

    // Construye el comando simple de inicio.
    QByteArray buildStartCommand() const;

    // Convierte un valor de tiempo de la unidad seleccionada a la unidad base interna.
    quint32 convertToBase(int valor, int indiceUnidad) const;

    // Convierte un valor de la unidad base interna a la unidad seleccionada.
    quint32 convertFromBase(quint32 numeroBase, int indiceUnidad) const;

    // Getters para lectura de estado (necesarios para perfiles)
    QBitArray getTecla() const { return m_tecla; }
    quint8 getPulseWidth() const { return m_config[0]; }
    quint8 getDelayA() const { return m_config[1]; }
    quint8 getDelayB() const { return m_config[2]; }
    quint8 getDelayC() const { return m_config[3]; }
    quint8 getDelayD() const { return m_config[4]; }

private:
    // Normaliza el tiempo a un múltiplo de 8.
    quint32 normalizeTimeValue(quint32 timeValue) const;

    // Descompone un número en ocho dígitos ASCII con ceros a la izquierda.
    QVector<quint8> decomposeNumber(quint32 numero) const;

    // Genera el estado de la columna seleccionada en un byte de 4 bits.
    quint8 buildColumnState(int columnIndex) const;

    QBitArray m_tecla;
    int m_selectedColumn;
    quint8 m_config[5];
};

#endif // FPGAPROTOCOL_HPP
