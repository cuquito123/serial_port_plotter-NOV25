#ifndef SERIALPORTMANAGER_HPP
#define SERIALPORTMANAGER_HPP

#include <QObject>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

class SerialPortManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialPortManager(QObject *parent = nullptr);
    ~SerialPortManager() override;

    bool openPort(const QSerialPortInfo &portInfo,
                  int baudRate,
                  QSerialPort::DataBits dataBits,
                  QSerialPort::Parity parity,
                  QSerialPort::StopBits stopBits);
    void closePort();
    void writeData(const QByteArray &data);
    QSerialPort *serialPort() const;

signals:
    void portOpened();
    void portOpenFailed();
    void portClosed();
    void rawDataReady(const QByteArray &data);

private slots:
    void handleReadyRead();
    void handleError(QSerialPort::SerialPortError error);

private:
    QSerialPort *m_serialPort;
};

#endif // SERIALPORTMANAGER_HPP
