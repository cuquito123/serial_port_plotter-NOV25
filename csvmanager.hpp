#ifndef CSVMANAGER_HPP
#define CSVMANAGER_HPP

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QVector>

class FpgaProtocol;

class CsvManager : public QObject
{
    Q_OBJECT

public:
    explicit CsvManager(FpgaProtocol *fpgaProtocol, QObject *parent = nullptr);
    ~CsvManager();

    // Gestión de archivos CSV
    bool openCsvFile(QWidget *parent = nullptr);
    void closeCsvFile();
    bool isOpen() const { return m_csvFile != nullptr; }

    // Escritura de datos
    void saveData(const QStringList &newData, int dataPointNumber);

    // Validación
    bool hasValidMapping() const { return !m_csvTramaIdx.isEmpty(); }

signals:
    void statusChanged(const QString &message);
    void fileClosed();

private:
    void buildHeaders();

    FpgaProtocol *m_fpgaProtocol = nullptr;

    QFile *m_csvFile = nullptr;
    QTextStream *m_csvStream = nullptr;
    int m_csvFlushCounter = 0;

    // Mapeo fijo de 8 columnas
    QVector<int> m_csvTramaIdx;
    QStringList m_csvLabels;
};

#endif // CSVMANAGER_HPP
