#include "plotmanager.hpp"
#include <QListWidget>
#include <QInputDialog>
#include <QDebug>

PlotManager::PlotManager(QCustomPlot *plot, QListWidget *channelList, QObject *parent)
    : QObject(parent), m_plot(plot), m_channelList(channelList)
{
}

PlotManager::~PlotManager()
{
}

// Copia las paletas entregadas por MainWindow para estilo de curvas y UI.
void PlotManager::setColors(const QColor lineColors[14], const QColor guiColors[4])
{
    for (int i = 0; i < 14; ++i) {
        m_lineColors[i] = lineColors[i];
    }
    for (int i = 0; i < 4; ++i) {
        m_guiColors[i] = guiColors[i];
    }
}

void PlotManager::setupPlot()
{
    if (!m_plot) return;

    /* Limpia elementos existentes del grafico */
    m_plot->clearItems();

    /* Fondo del area de grafico */
    m_plot->setBackground(m_guiColors[0]);

    /* Configuracion para mayor rendimiento en tiempo real */
    m_plot->setNotAntialiasedElements(QCP::aeAll);
    QFont font;
    font.setStyleStrategy(QFont::NoAntialias);
    m_plot->legend->setFont(font);

    /** Estilo basado en ejemplos de QCustomPlot **/
    /* Eje X: estilo */
    m_plot->xAxis->grid()->setPen(QPen(m_guiColors[2], 1, Qt::DotLine));
    m_plot->xAxis->grid()->setSubGridPen(QPen(m_guiColors[1], 1, Qt::DotLine));
    m_plot->xAxis->grid()->setSubGridVisible(true);
    m_plot->xAxis->setBasePen(QPen(m_guiColors[2]));
    m_plot->xAxis->setTickPen(QPen(m_guiColors[2]));
    m_plot->xAxis->setSubTickPen(QPen(m_guiColors[2]));
    m_plot->xAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    m_plot->xAxis->setTickLabelColor(m_guiColors[2]);
    m_plot->xAxis->setTickLabelFont(font);
    /* Rango visible */
    m_plot->xAxis->setRange(m_dataPointNumber - m_visiblePoints, m_dataPointNumber);

    /* Eje Y */
    m_plot->yAxis->grid()->setPen(QPen(m_guiColors[2], 1, Qt::DotLine));
    m_plot->yAxis->grid()->setSubGridPen(QPen(m_guiColors[1], 1, Qt::DotLine));
    m_plot->yAxis->grid()->setSubGridVisible(true);
    m_plot->yAxis->setBasePen(QPen(m_guiColors[2]));
    m_plot->yAxis->setTickPen(QPen(m_guiColors[2]));
    m_plot->yAxis->setSubTickPen(QPen(m_guiColors[2]));
    m_plot->yAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    m_plot->yAxis->setTickLabelColor(m_guiColors[2]);
    m_plot->yAxis->setTickLabelFont(font);

    /* Drag y zoom solo en X; Y se controla manualmente desde UI */
    m_plot->setInteraction(QCP::iRangeDrag, true);
    m_plot->setInteraction(QCP::iSelectPlottables, true);
    m_plot->setInteraction(QCP::iSelectLegend, true);
    m_plot->axisRect()->setRangeDrag(Qt::Horizontal);
    m_plot->axisRect()->setRangeZoom(Qt::Horizontal);

    /* Leyenda */
    QFont legendFont;
    legendFont.setPointSize(9);
    m_plot->legend->setVisible(true);
    m_plot->legend->setFont(legendFont);
    m_plot->legend->setBrush(m_guiColors[3]);
    m_plot->legend->setBorderPen(m_guiColors[2]);
    /* La leyenda vive en el inset del axisRect principal; se ajusta su alineacion */
    m_plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop|Qt::AlignLeft);
}

// Mantiene una ventana deslizante en X y repinta.
void PlotManager::replot()
{
    if (!m_plot) return;
    m_plot->xAxis->setRange(m_dataPointNumber - m_visiblePoints, m_dataPointNumber);
    m_plot->replot();
}

// Define el mapeo fijo trama->grafico usado durante la adquisicion activa.
void PlotManager::setActiveTramaIndices(const QVector<int> &indices)
{
    m_activeTramaIndices = indices;
}

// Agrega una muestra usando el mapeo fijo de tramas configurado al iniciar.
void PlotManager::addDataPoint(double x, const QStringList &newData)
{
    if (!m_plot) return;

    if (m_activeTramaIndices.isEmpty()) return;

    const int count = qMin(m_activeTramaIndices.size(), m_plot->graphCount());
    for (int grafico = 0; grafico < count; grafico++)
    {
        const int tramIdx = m_activeTramaIndices[grafico];
        if (tramIdx < 0 || tramIdx >= newData.size()) continue;

        bool ok = false;
        const double value = newData[tramIdx].toDouble(&ok);
        if (!ok) {
            qDebug() << "PlotManager: invalid numeric value ignored at index" << tramIdx << ":" << newData[tramIdx];
            continue;
        }

        m_plot->graph(grafico)->addData(x, value);
    }

    m_dataPointNumber++;
}

// Limpia curvas y lista de canales, y reinicia contadores de ploteo.
void PlotManager::clearPlot()
{
    if (!m_plot || !m_channelList) return;

    m_plot->clearPlottables();
    m_channelList->clear();
    m_channels = 0;
    m_dataPointNumber = 0;
    m_activeTramaIndices.clear();
    setupPlot();
    m_plot->replot();
}

// Crea un grafico por etiqueta y sincroniza color/nombre con la lista.
void PlotManager::setupGraphsFromLabels(const QStringList &labels)
{
    if (!m_plot || !m_channelList) return;

    for (int i = 0; i < labels.size(); i++) {
        m_plot->addGraph();
        m_plot->graph(i)->setPen(m_lineColors[i % 14]);
        m_plot->graph(i)->setName(labels[i]);
        if (m_plot->legend->item(i))
            m_plot->legend->item(i)->setTextColor(m_lineColors[i % 14]);
        m_channelList->addItem(labels[i]);
        if (m_channelList->item(i))
            m_channelList->item(i)->setForeground(QBrush(m_lineColors[i % 14]));
        m_channels++;
    }
    m_plot->replot();
}

// Convierte posicion de mouse a coordenadas del grafico y emite estado.
void PlotManager::onMouseMove(QMouseEvent *event)
{
    if (!m_plot) return;

    int xx = int(m_plot->xAxis->pixelToCoord(event->x()));
    int yy = int(m_plot->yAxis->pixelToCoord(event->y()));
    QString coordinates("X: %1 Y: %2");
    coordinates = coordinates.arg(xx).arg(yy);
    emit statusChanged(coordinates);
}

// Reenvia el evento de rueda con delta invertido para mantener sentido de zoom.
void PlotManager::onMouseWheel(QWheelEvent *event)
{
    if (!m_plot || !event) return;

    const int deltaY = event->angleDelta().y();
    if (deltaY == 0) {
        event->accept();
        return;
    }

    // Zoom horizontal centrado en la posicion del cursor, sin reenviar eventos.
    const double zoomBase = 0.85;
    const double zoomFactor = (deltaY > 0) ? zoomBase : (1.0 / zoomBase);
    const double centerX = m_plot->xAxis->pixelToCoord(event->pos().x());
    const QCPRange current = m_plot->xAxis->range();

    const double lower = centerX - (centerX - current.lower) * zoomFactor;
    const double upper = centerX + (current.upper - centerX) * zoomFactor;

    if (upper - lower <= 1e-9) {
        event->accept();
        return;
    }

    m_plot->xAxis->setRange(lower, upper);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
    event->accept();
}

// Sincroniza seleccion visual entre curvas y elementos de leyenda.
void PlotManager::onChannelSelection()
{
    if (!m_plot) return;

    /* Sincroniza seleccion de curvas con sus items de leyenda */
    for (int i = 0; i < m_plot->graphCount(); i++)
    {
        QCPGraph *graph = m_plot->graph(i);
        QCPPlottableLegendItem *item = m_plot->legend->itemWithPlottable(graph);
        if (item->selected())
        {
            item->setSelected(true);
        }
        else
        {
            item->setSelected(false);
        }
    }
}

// Permite renombrar un canal al hacer doble click en su item de leyenda.
void PlotManager::onLegendDoubleClick(QCPLegend *legend, QCPAbstractLegendItem *item, QMouseEvent *event)
{
    Q_UNUSED(legend)
    Q_UNUSED(event)

    if (!m_plot || !m_channelList) return;

    /* Solo actua si se hizo click sobre un item valido de leyenda */
    if (item)
    {
        QCPPlottableLegendItem *plItem = qobject_cast<QCPPlottableLegendItem*>(item);
        bool ok;
        QString newName = QInputDialog::getText(nullptr, "Change channel name", "New name:", QLineEdit::Normal, plItem->plottable()->name(), &ok, Qt::Popup);
        if (ok)
        {
            plItem->plottable()->setName(newName);
            for(int i=0; i<m_plot->graphCount(); i++)
            {
                if (m_channelList->item(i))
                    m_channelList->item(i)->setText(m_plot->graph(i)->name());
            }
            m_plot->replot();
        }
    }
}

// Ajusta limite inferior del eje Y desde la UI.
void PlotManager::onAxesMinChanged(int arg1)
{
    if (!m_plot) return;
    m_plot->yAxis->setRangeLower(arg1);
    m_plot->replot();
}

// Ajusta limite superior del eje Y desde la UI.
void PlotManager::onAxesMaxChanged(int arg1)
{
    if (!m_plot) return;
    m_plot->yAxis->setRangeUpper(arg1);
    m_plot->replot();
}

// Ajusta cantidad de divisiones principales del eje Y.
void PlotManager::onYStepChanged(int arg1)
{
    if (!m_plot) return;
    m_plot->yAxis->ticker()->setTickCount(arg1);
    m_plot->replot();
}

// Cambia cantidad de puntos visibles en la ventana temporal de X.
void PlotManager::onPointsChanged(int arg1)
{
    Q_UNUSED(arg1)
    if (!m_plot) return;
    m_visiblePoints = arg1;
    m_plot->xAxis->setRange(m_dataPointNumber - m_visiblePoints, m_dataPointNumber);
    m_plot->replot();
}

// Guarda captura PNG del plot con resolucion fija de exportacion.
void PlotManager::savePlotImage()
{
    if (!m_plot) return;
    m_plot->savePng(QString::number(m_dataPointNumber) + ".png", 1920, 1080, 2, 50);
}
