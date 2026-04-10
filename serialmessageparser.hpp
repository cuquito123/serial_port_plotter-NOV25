#ifndef SERIALMESSAGEPARSER_HPP
#define SERIALMESSAGEPARSER_HPP

#include <QObject>
#include <QStringList>

class SerialMessageParser : public QObject
{
    Q_OBJECT

public:
    // Constructor: inicializa el parser de mensajes seriales.
    explicit SerialMessageParser(QObject *parent = nullptr);

public slots:
    // Agrega datos recibidos por el puerto serie al búfer interno.
    // Cuando se encuentra un mensaje completo, emite messageParsed.
    void appendData(const QByteArray &data);

signals:
    // Señal emitida cuando se parsea un mensaje completo.
    // data: lista de campos separados extraídos del mensaje.
    // rawMessage: mensaje original sin procesamiento.
    void messageParsed(const QStringList &data, const QString &rawMessage);

private:
    enum class State {
        WaitStart,
        InMessage
    };

    // Estado actual del parser: esperando inicio o leyendo mensaje.
    State m_state;
    // Búfer de datos parciales recibidos hasta formar un mensaje completo.
    QString m_buffer;
};

#endif // SERIALMESSAGEPARSER_HPP
