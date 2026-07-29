#ifndef CSVMANAGER_HPP
#define CSVMANAGER_HPP

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QVector>
#include <QDateTime>

class FpgaProtocol;

class CsvManager : public QObject
{
    Q_OBJECT

public:
    // Constructor: recibe el protocolo para resolver mapeo de tramas a columnas.
    explicit CsvManager(FpgaProtocol *fpgaProtocol, QObject *parent = nullptr);
    ~CsvManager();

    // Gestión de archivos CSV
    bool openCsvFile(QWidget *parent = nullptr);
    void closeCsvFile();
    // Indica si hay archivo de salida abierto y listo para escribir.
    bool isOpen() const { return m_csvFile != nullptr; }

    // Escritura de datos
    void saveData(const QStringList &newData, int dataPointNumber);
    // Registra un evento de pausa/reanudación como línea de comentario en el CSV.
    void logPauseEvent(bool paused);

    // Validación
    // Verifica que exista un mapeo de columnas/tramas listo para grabar.
    bool hasValidMapping() const { return !m_csvTramaIdx.isEmpty(); }

signals:
    // Mensajes de estado para reflejar en UI.
    void statusChanged(const QString &message);
    // Notifica que el archivo CSV fue cerrado.
    void fileClosed();

private:
    // Construye metadatos y cabecera de columnas del CSV.
    void buildHeaders();

    // Dependencia para traducir configuracion activa a mapeo de CSV.
    FpgaProtocol *m_fpgaProtocol = nullptr;

    // Recursos de archivo de salida.
    QFile *m_csvFile = nullptr;
    QTextStream *m_csvStream = nullptr;
    int m_csvFlushCounter = 0;

    // Mapeo fijo de 8 columnas
    QVector<int> m_csvTramaIdx;
    QStringList m_csvLabels;
    // Metadata de experimento (duración en ms). 0 = none
    qint64 m_experimentDurationMs = 0;
    // Timestamp de inicio del experimento (para header/footer)
    QDateTime m_experimentStart;

public:
    // Establece la duración del experimento (ms) para escribir en el header
    void setExperimentDurationMs(qint64 durationMs) { m_experimentDurationMs = durationMs; }
};

#endif // CSVMANAGER_HPP
