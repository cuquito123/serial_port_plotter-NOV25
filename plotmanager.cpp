#include "plotmanager.hpp"
#include "fpgaprotocol.hpp"
#include <QListWidget>
#include <QApplication>
#include <QInputDialog>
#include <QDebug>

PlotManager::PlotManager(QCustomPlot *plot, QListWidget *channelList, QObject *parent)
    : QObject(parent), m_plot(plot), m_channelList(channelList)
{
}

PlotManager::~PlotManager()
{
}

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

    /* Remove everything from the plot */
    m_plot->clearItems();

    /* Background for the plot area */
    m_plot->setBackground(m_guiColors[0]);

    /* Used for higher performance (see QCustomPlot real time example) */
    m_plot->setNotAntialiasedElements(QCP::aeAll);
    QFont font;
    font.setStyleStrategy(QFont::NoAntialias);
    m_plot->legend->setFont(font);

    /** See QCustomPlot examples / styled demo **/
    /* X Axis: Style */
    m_plot->xAxis->grid()->setPen(QPen(m_guiColors[2], 1, Qt::DotLine));
    m_plot->xAxis->grid()->setSubGridPen(QPen(m_guiColors[1], 1, Qt::DotLine));
    m_plot->xAxis->grid()->setSubGridVisible(true);
    m_plot->xAxis->setBasePen(QPen(m_guiColors[2]));
    m_plot->xAxis->setTickPen(QPen(m_guiColors[2]));
    m_plot->xAxis->setSubTickPen(QPen(m_guiColors[2]));
    m_plot->xAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    m_plot->xAxis->setTickLabelColor(m_guiColors[2]);
    m_plot->xAxis->setTickLabelFont(font);
    /* Range */
    m_plot->xAxis->setRange(m_dataPointNumber - m_visiblePoints, m_dataPointNumber);

    /* Y Axis */
    m_plot->yAxis->grid()->setPen(QPen(m_guiColors[2], 1, Qt::DotLine));
    m_plot->yAxis->grid()->setSubGridPen(QPen(m_guiColors[1], 1, Qt::DotLine));
    m_plot->yAxis->grid()->setSubGridVisible(true);
    m_plot->yAxis->setBasePen(QPen(m_guiColors[2]));
    m_plot->yAxis->setTickPen(QPen(m_guiColors[2]));
    m_plot->yAxis->setSubTickPen(QPen(m_guiColors[2]));
    m_plot->yAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    m_plot->yAxis->setTickLabelColor(m_guiColors[2]);
    m_plot->yAxis->setTickLabelFont(font);

    /* User interactions Drag and Zoom are allowed only on X axis, Y is fixed manually by UI control */
    m_plot->setInteraction(QCP::iRangeDrag, true);
    m_plot->setInteraction(QCP::iSelectPlottables, true);
    m_plot->setInteraction(QCP::iSelectLegend, true);
    m_plot->axisRect()->setRangeDrag(Qt::Horizontal);
    m_plot->axisRect()->setRangeZoom(Qt::Horizontal);

    /* Legend */
    QFont legendFont;
    legendFont.setPointSize(9);
    m_plot->legend->setVisible(true);
    m_plot->legend->setFont(legendFont);
    m_plot->legend->setBrush(m_guiColors[3]);
    m_plot->legend->setBorderPen(m_guiColors[2]);
    /* By default, the legend is in the inset layout of the main axis rect. So this is how we access it to change legend placement */
    m_plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop|Qt::AlignLeft);
}

void PlotManager::replot()
{
    if (!m_plot) return;
    m_plot->xAxis->setRange(m_dataPointNumber - m_visiblePoints, m_dataPointNumber);
    m_plot->replot();
}

void PlotManager::addDataPoint(double x, const QStringList &newData, FpgaProtocol *fpgaProtocol)
{
    if (!m_plot || !fpgaProtocol) return;

    QVector<int> tramaIndices = fpgaProtocol->activeTramaIndices();
    if (tramaIndices.isEmpty()) return;

    for (int grafico = 0; grafico < tramaIndices.size(); grafico++)
    {
        int tramIdx = tramaIndices[grafico];
        if (tramIdx >= newData.size()) continue;
        if (grafico >= m_plot->graphCount()) break;

        m_plot->graph(grafico)->addData(x, newData[tramIdx].toDouble());
    }

    m_dataPointNumber++;
}

void PlotManager::clearPlot()
{
    if (!m_plot || !m_channelList) return;

    m_plot->clearPlottables();
    m_channelList->clear();
    m_channels = 0;
    m_dataPointNumber = 0;
    setupPlot();
    m_plot->replot();
}

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

void PlotManager::onMouseMove(QMouseEvent *event)
{
    if (!m_plot) return;

    int xx = int(m_plot->xAxis->pixelToCoord(event->x()));
    int yy = int(m_plot->yAxis->pixelToCoord(event->y()));
    QString coordinates("X: %1 Y: %2");
    coordinates = coordinates.arg(xx).arg(yy);
    emit statusChanged(coordinates);
}

void PlotManager::onMouseWheel(QWheelEvent *event)
{
    if (!m_plot) return;

    QWheelEvent inverted_event = QWheelEvent(event->posF(), event->globalPosF(),
                                             -event->pixelDelta(), -event->angleDelta(),
                                             0, Qt::Vertical, event->buttons(), event->modifiers());
    QApplication::sendEvent(m_plot, &inverted_event);
}

void PlotManager::onChannelSelection()
{
    if (!m_plot) return;

    /* synchronize selection of graphs with selection of corresponding legend items */
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

void PlotManager::onLegendDoubleClick(QCPLegend *legend, QCPAbstractLegendItem *item, QMouseEvent *event)
{
    Q_UNUSED(legend)
    Q_UNUSED(event)

    if (!m_plot || !m_channelList) return;

    /* Only react if item was clicked (user could have clicked on border padding of legend where there is no item, then item is 0) */
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

void PlotManager::onAxesMinChanged(int arg1)
{
    if (!m_plot) return;
    m_plot->yAxis->setRangeLower(arg1);
    m_plot->replot();
}

void PlotManager::onAxesMaxChanged(int arg1)
{
    if (!m_plot) return;
    m_plot->yAxis->setRangeUpper(arg1);
    m_plot->replot();
}

void PlotManager::onYStepChanged(int arg1)
{
    if (!m_plot) return;
    m_plot->yAxis->ticker()->setTickCount(arg1);
    m_plot->replot();
}

void PlotManager::onPointsChanged(int arg1)
{
    Q_UNUSED(arg1)
    if (!m_plot) return;
    m_visiblePoints = arg1;
    m_plot->xAxis->setRange(m_dataPointNumber - m_visiblePoints, m_dataPointNumber);
    m_plot->replot();
}

void PlotManager::savePlotImage()
{
    if (!m_plot) return;
    m_plot->savePng(QString::number(m_dataPointNumber) + ".png", 1920, 1080, 2, 50);
}
