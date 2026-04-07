#include "serialportmanager.hpp"
#include <QDebug>

SerialPortManager::SerialPortManager(QObject *parent)
    : QObject(parent),
      m_serialPort(new QSerialPort(this))
{
    connect(m_serialPort, &QSerialPort::readyRead, this, &SerialPortManager::handleReadyRead);
    connect(m_serialPort, &QSerialPort::errorOccurred, this, &SerialPortManager::handleError);
}

SerialPortManager::~SerialPortManager() = default;

bool SerialPortManager::openPort(const QSerialPortInfo &portInfo,
                                int baudRate,
                                QSerialPort::DataBits dataBits,
                                QSerialPort::Parity parity,
                                QSerialPort::StopBits stopBits)
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }

    m_serialPort->setPort(portInfo);
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setParity(parity);
    m_serialPort->setDataBits(dataBits);
    m_serialPort->setStopBits(stopBits);

    if (m_serialPort->open(QIODevice::ReadWrite)) {
        emit portOpened();
        return true;
    }

    emit portOpenFailed();
    qDebug() << "Failed to open serial port" << portInfo.portName() << "Error:" << m_serialPort->error();
    return false;
}

void SerialPortManager::closePort()
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }
    emit portClosed();
}

void SerialPortManager::writeData(const QByteArray &data)
{
    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->write(data);
    }
}

QSerialPort *SerialPortManager::serialPort() const
{
    return m_serialPort;
}

void SerialPortManager::handleReadyRead()
{
    const QByteArray data = m_serialPort->readAll();
    if (!data.isEmpty()) {
        emit rawDataReady(data);
    }
}

void SerialPortManager::handleError(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError) {
        qDebug() << "SerialPortManager error:" << error << m_serialPort->errorString();
    }
}
