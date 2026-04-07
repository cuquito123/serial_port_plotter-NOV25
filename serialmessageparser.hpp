#ifndef SERIALMESSAGEPARSER_HPP
#define SERIALMESSAGEPARSER_HPP

#include <QObject>
#include <QStringList>

class SerialMessageParser : public QObject
{
    Q_OBJECT

public:
    explicit SerialMessageParser(QObject *parent = nullptr);

public slots:
    void appendData(const QByteArray &data);

signals:
    void messageParsed(const QStringList &data, const QString &rawMessage);

private:
    enum class State {
        WaitStart,
        InMessage
    };

    State m_state;
    QString m_buffer;
};

#endif // SERIALMESSAGEPARSER_HPP
