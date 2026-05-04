#include "serialportmanager.hpp"
#include <QDebug>

// Constructor: crea el objeto QSerialPort y conecta las señales de lectura y error.
SerialPortManager::SerialPortManager(QObject *parent)
    : QObject(parent),
      m_serialPort(new QSerialPort(this))
{
    connect(m_serialPort, &QSerialPort::readyRead, this, &SerialPortManager::handleReadyRead);
    connect(m_serialPort, &QSerialPort::errorOccurred, this, &SerialPortManager::handleError);
}

SerialPortManager::~SerialPortManager() = default;

// Abre el puerto serie con los parámetros dados.
// Si ya estaba abierto, lo cierra primero para reconfigurarlo.
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

// Cierra el puerto serie si está abierto y emite portClosed.
void SerialPortManager::closePort()
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }
    emit portClosed();
}

// Envía un bloque de datos al puerto serie abierto.
void SerialPortManager::writeData(const QByteArray &data)
{
    if (!m_serialPort) {
        emit writeFailed(QStringLiteral("Serial port not initialized"));
        return;
    }

    if (!m_serialPort->isOpen()) {
        emit writeFailed(QStringLiteral("Serial port not open"));
        return;
    }

    qint64 totalWritten = 0;
    const qint64 dataSize = data.size();
    while (totalWritten < dataSize) {
        qint64 written = m_serialPort->write(data.constData() + totalWritten, dataSize - totalWritten);
        if (written == -1) {
            emit writeFailed(m_serialPort->errorString());
            return;
        }

        // Esperar que los bytes se transmitan (timeout corto)
        if (!m_serialPort->waitForBytesWritten(200)) {
            emit writeFailed(QStringLiteral("Timeout waiting for bytes written: %1").arg(m_serialPort->errorString()));
            return;
        }

        totalWritten += written;
    }

    emit writeSucceeded(totalWritten);
}

// Devuelve el puntero al QSerialPort interno.
QSerialPort *SerialPortManager::serialPort() const
{
    return m_serialPort;
}

// Lee todos los bytes disponibles y emite rawDataReady si hay datos.
void SerialPortManager::handleReadyRead()
{
    const QByteArray data = m_serialPort->readAll();
    if (!data.isEmpty()) {
        emit rawDataReady(data);
    }
}

// Maneja errores de puerto serie registrando el mensaje de error.
void SerialPortManager::handleError(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError) {
        qDebug() << "SerialPortManager error:" << error << m_serialPort->errorString();
    }
}
