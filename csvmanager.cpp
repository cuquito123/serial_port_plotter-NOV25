#include "csvmanager.hpp"
#include "fpgaprotocol.hpp"
#include <QFileDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <QMessageBox>
#include <QWidget>
#include <QDebug>
#include <QTextCodec>

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
    // Se usa la codificación ANSI del sistema (sin BOM) en vez de UTF-8: Excel en
    // Windows, al abrir un .csv con ";" como delimitador con doble clic, suele
    // ignorar el BOM UTF-8 y reinterpretar los bytes como ANSI, mostrando
    // caracteres corruptos (mojibake) en tildes y demás símbolos no ASCII.
    // La codificación ANSI local es exactamente lo que Excel asume por defecto
    // en ese flujo, así que coincide sin ambigüedad.
    m_csvStream->setCodec(QTextCodec::codecForLocale());

    // Record start timestamp for metadata
    m_experimentStart = QDateTime::currentDateTime();

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

    // 8 columnas fijas — una por columna de la grilla (col1→col8)
    for (int col = 0; col < 8; col++) {
        int tramIdx = m_csvTramaIdx[col];
        if (tramIdx >= 0 && tramIdx < newData.size()) {
            const QString value = newData[tramIdx];
            *m_csvStream << ";" << value;
        } else {
            *m_csvStream << ";";   // columna vacía → celda vacía
        }
    }
    *m_csvStream << "\n";

    m_csvFlushCounter++;
    if (m_csvFlushCounter >= 100) {
        m_csvStream->flush();
        m_csvFlushCounter = 0;
    }
}

// Escribe un comentario marcando pausa/reanudación de la adquisición.
void CsvManager::logPauseEvent(bool paused)
{
    if (!m_csvStream) return;

    // Tiempo real transcurrido desde el inicio del experimento, no el contador
    // de muestras: éste último se congela en 0 si todavía no llegó ningún dato,
    // lo que hacía que el marcador siempre mostrara "t=0.000s".
    const double tiempo_s = m_experimentStart.isValid()
        ? m_experimentStart.msecsTo(QDateTime::currentDateTime()) / 1000.0
        : 0.0;
    *m_csvStream << "# " << (paused ? "Pausa" : "Reanudado")
                 << " en t=" << QString::number(tiempo_s, 'f', 3) << "s ("
                 << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << ")\n";
    m_csvStream->flush();
}

// Escribe metadatos de experimento y fila de titulos de columnas.
void CsvManager::buildHeaders()
{
    if (!m_csvStream) return;

    // Metadata
    *m_csvStream << "# Experimento: MPCC — Multi-Photon Coincidence Counter (CIOp) v2.3.0\n";
    *m_csvStream << "# Fecha: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
    if (m_experimentDurationMs > 0) {
        qint64 secs = m_experimentDurationMs / 1000;
        *m_csvStream << "# Duración (s): " << QString::number(secs) << "\n";
    } else {
        *m_csvStream << "# Duración (s): (sin especificar)\n";
    }
    // Sin el ";" literal: al ser el delimitador del propio CSV, un ";" dentro
    // de esta línea de comentario la partiría en dos columnas al abrirla.
    *m_csvStream << "# Separador de columnas: punto y coma\n";
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
}
