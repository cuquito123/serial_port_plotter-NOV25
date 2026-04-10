#include "serialmessageparser.hpp"
#include <cctype>

static constexpr char START_MSG = '$';
static constexpr char END_MSG = ';';

// Constructor: inicializa el parser en el estado de espera de inicio de mensaje.
SerialMessageParser::SerialMessageParser(QObject *parent)
    : QObject(parent),
      m_state(State::WaitStart)
{
}

// Agrega datos al parser y procesa byte a byte.
// Detecta mensajes delimitados por '$' al inicio y ';' al final.
void SerialMessageParser::appendData(const QByteArray &data)
{
    for (char rawByte : data) {
        switch (m_state) {
        case State::WaitStart:
            // Espera el byte de inicio, ignora todo lo demás.
            if (rawByte == START_MSG) {
                m_state = State::InMessage;
                m_buffer.clear();
            }
            break;

        case State::InMessage:
            if (rawByte == END_MSG) {
                // Mensaje completo recibido; dividir campos y emitir señal.
                QString rawMessage = m_buffer;
                QStringList messageParts = rawMessage.split(' ');
                emit messageParsed(messageParts, rawMessage);
                m_state = State::WaitStart;
            } else if (isdigit(rawByte) || isspace(rawByte) || rawByte == '-' || rawByte == '.') {
                // Agrega caracteres válidos al búfer del mensaje.
                m_buffer.append(rawByte);
            }
            break;
        }
    }
}
