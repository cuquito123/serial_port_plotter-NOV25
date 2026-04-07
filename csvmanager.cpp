#include "csvmanager.hpp"
#include "fpgaprotocol.hpp"
#include <QFileDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <QMessageBox>
#include <QWidget>
#include <QDebug>

CsvManager::CsvManager(FpgaProtocol *fpgaProtocol, QObject *parent)
    : QObject(parent), m_fpgaProtocol(fpgaProtocol)
{
}

CsvManager::~CsvManager()
{
    closeCsvFile();
}

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

    // Construir mapeo fijo de 8 columnas
    m_fpgaProtocol->fillCsvMapping(m_csvTramaIdx, m_csvLabels);

    // Escribir headers
    buildHeaders();

    m_csvFlushCounter = 0;
    emit statusChanged("Grabando en: " + filePath);
    return true;
}

void CsvManager::closeCsvFile()
{
    if (!m_csvFile) return;

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
    *m_csvStream << QString::number(tiempo_s, 'f', 3);

    // 8 columnas fijas — una por columna de la grilla (col1→col8)
    for (int col = 0; col < 8; col++) {
        int tramIdx = m_csvTramaIdx[col];
        if (tramIdx >= 0 && tramIdx < newData.size())
            *m_csvStream << ";" << newData[tramIdx];
        else
            *m_csvStream << ";";   // columna vacía → celda vacía
    }
    *m_csvStream << "\n";

    m_csvFlushCounter++;
    if (m_csvFlushCounter >= 100) {
        m_csvStream->flush();
        m_csvFlushCounter = 0;
    }
}

void CsvManager::buildHeaders()
{
    if (!m_csvStream) return;

    // Metadata
    *m_csvStream << "# Experimento: Serial Port Plotter v2.3.0\n";
    *m_csvStream << "# Fecha: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
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
            titulo += " - (vacía)";
        *m_csvStream << ";" << titulo;
    }
    *m_csvStream << "\n";
    m_csvStream->flush();
}
