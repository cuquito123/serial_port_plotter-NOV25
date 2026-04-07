#ifndef FPGAPROTOCOL_HPP
#define FPGAPROTOCOL_HPP

#include <QBitArray>
#include <QByteArray>
#include <QStringList>
#include <QVector>

class FpgaProtocol
{
public:
    explicit FpgaProtocol();

    void resetMatrix();
    void selectColumn(int columnIndex);
    void toggleButton(int bit);
    void setButton(int bit, bool active);
    bool buttonState(int bit) const;

    void setPulseWidth(quint8 value);
    void setDelayA(quint8 value);
    void setDelayB(quint8 value);
    void setDelayC(quint8 value);
    void setDelayD(quint8 value);

    QStringList generateLabels() const;
    QVector<int> activeTramaIndices() const;
    void fillCsvMapping(QVector<int> &csvTramaIdx, QStringList &csvLabels) const;

    QByteArray buildExtendedPacket(quint32 timeValue, int timeUnitIndex) const;
    QByteArray buildResetSweepPacket() const;
    QByteArray buildResetCommand() const;
    QByteArray buildStartCommand() const;

    quint32 convertToBase(int valor, int indiceUnidad) const;
    quint32 convertFromBase(quint32 numeroBase, int indiceUnidad) const;

private:
    quint32 normalizeTimeValue(quint32 timeValue) const;
    QVector<quint8> decomposeNumber(quint32 numero) const;
    quint8 buildColumnState(int columnIndex) const;

    QBitArray m_tecla;
    int m_selectedColumn;
    quint8 m_config[5];
};

#endif // FPGAPROTOCOL_HPP
