#include "csvmanager.hpp"
#include "fpgaprotocol.hpp"
#include <QFileDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <QMessageBox>
#include <QWidget>
#include <QDebug>
#include <QFileInfo>

namespace {
QString emptyColumnLabel()
{
    return QStringLiteral("(vac") + QChar(0x00ED) + QStringLiteral("a)");
}
}

CsvManager::CsvManager(FpgaProtocol *fpgaProtocol, QObject *parent)
    : QObject(parent), m_fpgaProtocol(fpgaProtocol)
{
}

CsvManager::~CsvManager()
{
    closeCsvFile();
}

// Abre un CSV nuevo, valida estado del protocolo y escribe cabeceras.
bool CsvManager::openCsvFile(QWidget *parent)
{
    if (!m_fpgaProtocol) {
        emit statusChanged("Error: FpgaProtocol no disponible");
        return false;
    }

    if (m_fpgaProtocol->activeTramaIndices().isEmpty()) {
        QMessageBox::warning(parent, "CSV", "Presioná 'Enviar Datos' antes de iniciar la grabación.");
        emit statusChanged("Error: Sin datos para grabar");
        return false;
    }

    QString defaultName = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss") + "_experimento.csv";
    QString filePath = QFileDialog::getSaveFileName(
        parent,
        "Guardar experimento",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + defaultName,
        "CSV Files (*.csv);;Todos los archivos (*)"
    );

    if (filePath.isEmpty()) {
        emit statusChanged("Grabación cancelada");
        return false;
    }

    m_csvFile = new QFile(filePath);
    if (!m_csvFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        delete m_csvFile;
        m_csvFile = nullptr;
        emit statusChanged("Error: no se pudo crear el archivo CSV");
        return false;
    }

    m_csvStream = new QTextStream(m_csvFile);
    m_csvStream->setCodec("UTF-8");
    m_csvStream->setGenerateByteOrderMark(true);

    // Record start timestamp for metadata
    m_experimentStart = QDateTime::currentDateTime();

    // Salida visual adicional: HTML con formato y colores.
    const QFileInfo csvInfo(filePath);
    const QString htmlPath = csvInfo.path() + "/" + csvInfo.completeBaseName() + "_formato.html";
    m_htmlFile = new QFile(htmlPath);
    if (m_htmlFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_htmlStream = new QTextStream(m_htmlFile);
        m_htmlStream->setCodec("UTF-8");
    } else {
        delete m_htmlFile;
        m_htmlFile = nullptr;
        m_htmlStream = nullptr;
    }

    // Construir mapeo fijo de 8 columnas
    m_fpgaProtocol->fillCsvMapping(m_csvTramaIdx, m_csvLabels);

    // Escribir headers
    buildHeaders();

    m_csvFlushCounter = 0;
    emit statusChanged("Grabando en: " + filePath);
    return true;
}

// Cierra y libera recursos del CSV en forma segura.
void CsvManager::closeCsvFile()
{
    if (!m_csvFile) return;

    // Write footer metadata with end time and actual duration
    if (m_csvStream) {
        QDateTime endTime = QDateTime::currentDateTime();
        qint64 actualSecs = m_experimentStart.isValid() ? m_experimentStart.secsTo(endTime) : 0;
        *m_csvStream << "#\n";
        *m_csvStream << "# End Time: " << endTime.toString("yyyy-MM-dd HH:mm:ss") << "\n";
        *m_csvStream << "# Actual Duration (s): " << QString::number(actualSecs) << "\n";
        m_csvStream->flush();
    }

    if (m_csvStream) {
        m_csvStream->flush();
        delete m_csvStream;
        m_csvStream = nullptr;
    }

    if (m_htmlStream) {
        *m_htmlStream << "  </tbody>\n"
                        "</table>\n"
                        "</body>\n"
                        "</html>\n";
        m_htmlStream->flush();
        delete m_htmlStream;
        m_htmlStream = nullptr;
    }

    if (m_htmlFile) {
        m_htmlFile->close();
        delete m_htmlFile;
        m_htmlFile = nullptr;
    }

    m_csvFile->close();
    delete m_csvFile;
    m_csvFile = nullptr;
    m_csvFlushCounter = 0;

    emit fileClosed();
}

// Escribe una fila de datos con tiempo y 8 columnas fijas segun mapeo activo.
void CsvManager::saveData(const QStringList &newData, int dataPointNumber)
{
    if (!m_csvFile || !m_csvStream) {
        return;
    }

    if (m_csvTramaIdx.isEmpty()) {
        return;
    }

    // Timestamp en segundos desde el inicio del plot (basado en 20 Hz)
    double tiempo_s = dataPointNumber / 20.0;
    const QString tiempoTxt = QString::number(tiempo_s, 'f', 3);
    *m_csvStream << tiempoTxt;

    if (m_htmlStream) {
        *m_htmlStream << "    <tr>\n";
        *m_htmlStream << "      <td class=\"tiempo\">" << tiempoTxt.toHtmlEscaped() << "</td>\n";
    }

    // 8 columnas fijas — una por columna de la grilla (col1→col8)
    for (int col = 0; col < 8; col++) {
        int tramIdx = m_csvTramaIdx[col];
        if (tramIdx >= 0 && tramIdx < newData.size()) {
            const QString value = newData[tramIdx];
            *m_csvStream << ";" << value;
            if (m_htmlStream)
                *m_htmlStream << "      <td>" << value.toHtmlEscaped() << "</td>\n";
        } else {
            *m_csvStream << ";";   // columna vacía → celda vacía
            if (m_htmlStream)
                *m_htmlStream << "      <td class=\"empty\">" << emptyColumnLabel().toHtmlEscaped() << "</td>\n";
        }
    }
    *m_csvStream << "\n";

    if (m_htmlStream) {
        *m_htmlStream << "    </tr>\n";
    }

    m_csvFlushCounter++;
    if (m_csvFlushCounter >= 100) {
        m_csvStream->flush();
        m_csvFlushCounter = 0;
    }
}

// Escribe metadatos de experimento y fila de titulos de columnas.
void CsvManager::buildHeaders()
{
    if (!m_csvStream) return;

    // Metadata
    *m_csvStream << "# Experimento: Serial Port Plotter v2.3.0\n";
    *m_csvStream << "# Fecha: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
    if (m_experimentDurationMs > 0) {
        qint64 secs = m_experimentDurationMs / 1000;
        *m_csvStream << "# Duración (s): " << QString::number(secs) << "\n";
    } else {
        *m_csvStream << "# Duración (s): (sin especificar)\n";
    }
    *m_csvStream << "# Separador: punto y coma (;)\n";
    *m_csvStream << "#\n";

    // Fila de títulos de columna
    // Formato: Tiempo (s) ; Col 1 - D ; Col 2 - (vacía) ; ...
    *m_csvStream << "Tiempo (s)";
    for (int col = 0; col < 8; col++) {
        QString titulo = QString("Col %1").arg(col + 1);
        if (!m_csvLabels[col].isEmpty())
            titulo += " - " + m_csvLabels[col];
        else
            titulo += " - " + emptyColumnLabel();
        *m_csvStream << ";" << titulo;
    }
    *m_csvStream << "\n";
    m_csvStream->flush();

    if (m_htmlStream) {
        buildHtmlHeaders();
        // Inject start time into HTML meta area if available
        if (m_experimentStart.isValid()) {
            *m_htmlStream << "  <p class=\"meta\">Start Time: " << m_experimentStart.toString("yyyy-MM-dd HH:mm:ss").toHtmlEscaped() << "</p>\n";
        }
    }
}

void CsvManager::buildHtmlHeaders()
{
    if (!m_htmlStream) return;

    *m_htmlStream
        << "<!doctype html>\n"
           "<html lang=\"es\">\n"
           "<head>\n"
           "  <meta charset=\"utf-8\">\n"
           "  <title>Experimento - Serial Port Plotter</title>\n"
           "  <style>\n"
           "    :root { color-scheme: light; }\n"
           "    body { margin: 24px; background: #f6f8fc; color: #1f2937; font-family: Segoe UI, Arial, sans-serif; }\n"
           "    h1 { margin: 0 0 8px; font-size: 22px; }\n"
           "    .meta { margin: 0 0 16px; color: #4b5563; }\n"
           "    table { border-collapse: collapse; width: 100%; background: #ffffff; border: 1px solid #d6dde8; }\n"
           "    thead th { background: linear-gradient(90deg, #0b4f8a, #0e7490); color: #ffffff; font-weight: 600; }\n"
           "    th, td { border: 1px solid #d6dde8; padding: 6px 10px; text-align: center; }\n"
           "    tbody tr:nth-child(odd) { background: #f8fbff; }\n"
           "    tbody tr:hover { background: #eaf4ff; }\n"
           "    td.tiempo { font-weight: 600; color: #0f3d66; }\n"
           "    td.empty { color: #94a3b8; font-style: italic; }\n"
           "  </style>\n"
           "</head>\n"
           "<body>\n"
           "  <h1>Experimento: Serial Port Plotter v2.3.0</h1>\n"
           "  <p class=\"meta\">Fecha: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toHtmlEscaped() << "</p>\n"
           "<table>\n"
           "  <thead>\n"
           "    <tr>\n"
           "      <th>Tiempo (s)</th>\n";

    for (int col = 0; col < 8; col++) {
        QString titulo = QString("Col %1").arg(col + 1);
        if (!m_csvLabels[col].isEmpty())
            titulo += " - " + m_csvLabels[col];
        else
            titulo += " - " + emptyColumnLabel();
        *m_htmlStream << "      <th>" << titulo.toHtmlEscaped() << "</th>\n";
    }

    *m_htmlStream
        << "    </tr>\n"
           "  </thead>\n"
           "  <tbody>\n";
}
