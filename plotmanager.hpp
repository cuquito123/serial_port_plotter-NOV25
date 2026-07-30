#ifndef PLOTMANAGER_HPP
#define PLOTMANAGER_HPP

#include <QObject>
#include <QColor>
#include <QStringList>
#include <QVector>
#include <QMouseEvent>
#include <QWheelEvent>
#include "qcustomplot/qcustomplot.h"

class FpgaProtocol;

class PlotManager : public QObject
{
    Q_OBJECT

public:
    // Constructor: recibe el plot y la lista de canales que administra.
    explicit PlotManager(QCustomPlot *plot, QListWidget *channelList, QObject *parent = nullptr);
    ~PlotManager();

    // Inicialización y configuración
    void setupPlot();
    void setColors(const QColor lineColors[14], const QColor guiColors[4]);

    // Gestión de datos
    void addDataPoint(double x, const QStringList &newData);
    void clearPlot();
    void setupGraphsFromLabels(const QStringList &labels);
    void setActiveTramaIndices(const QVector<int> &indices);

    // Acceso a propiedades
    // Cantidad total de puntos incorporados al grafico.
    int dataPointCount() const { return m_dataPointNumber; }
    // Numero de canales (graficos) actualmente configurados.
    int channelCount() const { return m_channels; }
    // Ajustes manuales del estado interno (uso de sincronizacion externa).
    void setDataPointCount(int count) { m_dataPointNumber = count; }
    void setChannelCount(int count) { m_channels = count; }

public slots:
    // Reploteo
    void replot();

    // Interacciones de ratón
    void onMouseMove(QMouseEvent *event);
    void onMouseWheel(QWheelEvent *event);

    // Selección de canales
    void onChannelSelection();
    void onLegendDoubleClick(QCPLegend *legend, QCPAbstractLegendItem *item, QMouseEvent *event);

    // Configuración de ejes
    void onAxesMinChanged(int arg1);
    void onAxesMaxChanged(int arg1);
    void onYStepChanged(int arg1);
    void onPointsChanged(int arg1);

    // Guardado de imágenes
    void savePlotImage();

signals:
    // Notifica mensajes de estado para mostrar en la barra principal.
    void statusChanged(const QString &message);

private:
    // Widgets controlados por este gestor (no son propietarios de memoria).
    QCustomPlot *m_plot = nullptr;
    QListWidget *m_channelList = nullptr;

    // Paletas de color para lineas y estilo general.
    QColor m_lineColors[14];
    QColor m_guiColors[4];

    // Estado de ploteo.
    int m_dataPointNumber = 0;
    int m_channels = 0;
    int m_visiblePoints = 100;

    // Mapeo fijo de indice de trama por grafico, tomado al iniciar adquisicion.
    QVector<int> m_activeTramaIndices;
};

#endif // PLOTMANAGER_HPP
