#include "serialmessageparser.hpp"
#include <cctype>

static constexpr char START_MSG = '$';
static constexpr char END_MSG = ';';

SerialMessageParser::SerialMessageParser(QObject *parent)
    : QObject(parent),
      m_state(State::WaitStart)
{
}

void SerialMessageParser::appendData(const QByteArray &data)
{
    for (char rawByte : data) {
        switch (m_state) {
        case State::WaitStart:
            if (rawByte == START_MSG) {
                m_state = State::InMessage;
                m_buffer.clear();
            }
            break;

        case State::InMessage:
            if (rawByte == END_MSG) {
                QString rawMessage = m_buffer;
                QStringList messageParts = rawMessage.split(' ');
                emit messageParsed(messageParts, rawMessage);
                m_state = State::WaitStart;
            } else if (isdigit(rawByte) || isspace(rawByte) || rawByte == '-' || rawByte == '.') {
                m_buffer.append(rawByte);
            }
            break;
        }
    }
}
