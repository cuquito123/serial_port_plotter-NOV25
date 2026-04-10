#ifndef SERIALPORTMANAGER_HPP
#define SERIALPORTMANAGER_HPP

#include <QObject>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

class SerialPortManager : public QObject
{
    Q_OBJECT

public:
    // Inicializa el gestor de puerto serie.
    explicit SerialPortManager(QObject *parent = nullptr);
    ~SerialPortManager() override;

    // Abre un puerto serie con la configuración especificada.
    bool openPort(const QSerialPortInfo &portInfo,
                  int baudRate,
                  QSerialPort::DataBits dataBits,
                  QSerialPort::Parity parity,
                  QSerialPort::StopBits stopBits);

    // Cierra el puerto serie si está abierto.
    void closePort();

    // Envía datos al puerto serie abierto.
    void writeData(const QByteArray &data);

    // Devuelve el objeto QSerialPort usado internamente.
    QSerialPort *serialPort() const;

signals:
    void portOpened();
    void portOpenFailed();
    void portClosed();
    void rawDataReady(const QByteArray &data);

private slots:
    // Slot invocado cuando hay datos disponibles para leer.
    void handleReadyRead();

    // Slot invocado cuando ocurre un error en el puerto serie.
    void handleError(QSerialPort::SerialPortError error);

private:
    QSerialPort *m_serialPort;
};

#endif // SERIALPORTMANAGER_HPP
