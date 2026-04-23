/***************************************************************************
**  This file is part of Serial Port Plotter                              **
**                                                                        **
**                                                                        **
**  Serial Port Plotter is a program for plotting integer data from       **
**  serial port using Qt and QCustomPlot                                  **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program.  If not, see http://www.gnu.org/licenses/.   **
**                                                                        **
****************************************************************************
**           Author: Borislav                                             **
**           Contact: b.kereziev@gmail.com                                **
**           Date: 29.12.14                                               **
****************************************************************************/

#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include "serialportmanager.hpp"
#include "serialmessageparser.hpp"
#include "fpgaprotocol.hpp"
#include "plotmanager.hpp"
#include "csvmanager.hpp"
#include <x86intrin.h>
#include <QWidget>
#include <QDebug>

#include "console.h"
#include "QMessageBox"

/**
 * @brief Constructor principal de la ventana.
 * @param parent
 */
MainWindow::MainWindow (QWidget *parent) :
  QMainWindow (parent),
  ui (new Ui::MainWindow),

    line_colors{
      QColor ("#fb4934"),
      QColor ("#b8bb26"),
      QColor ("#fabd2f"),
      QColor ("#83a598"),
      QColor ("#d3869b"),
      QColor ("#8ec07c"),
      QColor ("#fe8019"),
      QColor ("#cc241d"),
      QColor ("#98971a"),
      QColor ("#d79921"),
      QColor ("#458588"),
      QColor ("#b16286"),
      QColor ("#689d6a"),
      QColor ("#d65d0e"),
   },
  gui_colors {
            QColor (48,  47,  47,  255), /**<  0: color oscuro/fondo de UI */
            QColor (80,  80,  80,  255), /**<  1: color medio/rejilla */
            QColor (170, 170, 170, 255), /**<  2: color claro/texto */
            QColor (48,  47,  47,  200)  /**<  3: color oscuro con transparencia */
    },

  connected (false),
  plotting (false),
  dataPointNumber (0),
  channels(0),

  m_console(new Console),
  m_serialManager(new SerialPortManager(this)),
  m_messageParser(new SerialMessageParser(this)),
  m_fpgaProtocol(new FpgaProtocol()),
  m_plotManager(nullptr)

{
  initActionsConnections();

  connect(m_console, &Console::getData, this, &MainWindow::writeData);
  connect(m_serialManager, &SerialPortManager::rawDataReady, m_messageParser, &SerialMessageParser::appendData);
  connect(m_serialManager, &SerialPortManager::rawDataReady, this, [this](const QByteArray &raw) {
      if (!filterDisplayedData) {
          ui->textEdit_UartWindow->append(QString::fromLatin1(raw));
      }
  });
  connect(m_messageParser, &SerialMessageParser::messageParsed, this, [this](const QStringList &data, const QString &rawMessage) {
      if (filterDisplayedData) {
          ui->textEdit_UartWindow->append(rawMessage);
      }
      emit newData(data);
  });
  connect(this, SIGNAL(newData(QStringList)), this, SLOT(onNewDataArrived(QStringList)));
  connect(this, &MainWindow::newData, this, [this](const QStringList &data) {
      if (m_csvManager && ui->actionRecord_stream->isChecked()) {
          int csvPointNumber = dataPointNumber;
          if (m_plotManager) {
              // onNewDataArrived agrega la muestra primero y luego incrementa el contador interno.
              csvPointNumber = m_plotManager->dataPointCount() - 1;
              if (csvPointNumber < 0) {
                  csvPointNumber = 0;
              }
          }
          m_csvManager->saveData(data, csvPointNumber);
      }
  });
  connect(m_serialManager, &SerialPortManager::portOpened, this, &MainWindow::portOpenedSuccess);
  connect(m_serialManager, &SerialPortManager::portOpenFailed, this, &MainWindow::portOpenedFail);
  connect(m_serialManager, &SerialPortManager::portClosed, this, &MainWindow::onPortClosed);

  ui->setupUi (this);

  m_plotManager = new PlotManager(ui->plot, ui->listWidget_Channels, this);
  m_plotManager->setColors(line_colors, gui_colors);
  connect(m_plotManager, &PlotManager::statusChanged, this, [this](const QString &msg) {
      ui->statusBar->showMessage(msg);
  });

  m_csvManager = new CsvManager(m_fpgaProtocol, this);
  connect(m_csvManager, &CsvManager::statusChanged, this, [this](const QString &msg) {
      ui->statusBar->showMessage(msg);
  });

  createUI();
  m_plotManager->setupPlot();

  connect (ui->plot, SIGNAL (mouseWheel (QWheelEvent*)), m_plotManager, SLOT (onMouseWheel (QWheelEvent*)));
  connect (ui->plot, SIGNAL (mouseMove (QMouseEvent*)), m_plotManager, SLOT (onMouseMove (QMouseEvent*)));
  connect (ui->plot, SIGNAL(selectionChangedByUser()), m_plotManager, SLOT(onChannelSelection()));
  connect (ui->plot, SIGNAL(legendDoubleClick (QCPLegend*, QCPAbstractLegendItem*, QMouseEvent*)), m_plotManager, SLOT(onLegendDoubleClick (QCPLegend*, QCPAbstractLegendItem*, QMouseEvent*)));
  connect (&updateTimer, SIGNAL (timeout()), m_plotManager, SLOT (replot()));

      statusLabel = new QLabel(this);
      statusLabel->setText("Listo");
      statusLabel->setMinimumWidth(100);
      ui->statusBar->addPermanentWidget(statusLabel);
      columnaSeleccionada = 0;

      botonesGraf << ui->GRAF_1 << ui->GRAF_2 << ui->GRAF_3 << ui->GRAF_4
                  << ui->GRAF_5 << ui->GRAF_6 << ui->GRAF_7 << ui->GRAF_8;

      for (int i = 0; i < botonesGraf.size(); ++i) {
          connect(botonesGraf[i], &QPushButton::clicked, this, [=]() {
              actualizarEstadoGraf(i);
          });
      }

      botonesDatos << ui->A1_0 << ui->B1_1 << ui->C1_2 << ui->D1_3
                   << ui->A2_4 << ui->B2_5 << ui->C2_6 << ui->D2_7
                   << ui->A3_8 << ui->B3_9 << ui->C3_10 << ui->D3_11
                   << ui->A4_12 << ui->B4_13 << ui->C4_14 << ui->D4_15
                   << ui->A5_16 << ui->B5_17 << ui->C5_18 << ui->D5_19
                   << ui->A6_20 << ui->B6_21 << ui->C6_22 << ui->D6_23
                   << ui->A7_24 << ui->B7_25 << ui->C7_26 << ui->D7_27
                   << ui->A8_28 << ui->B8_29 << ui->C8_30 << ui->D8_31;

      for (int i = 0; i < botonesDatos.size(); ++i) {
          connect(botonesDatos[i], &QPushButton::clicked, this, [=]() {
              actualizarBotonDato(i, botonesDatos[i]);
          });
      }

      actualizarEstadoGraf(columnaSeleccionada);

      connect(ui->TiempoBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
              this, &MainWindow::actualizarMaximoDeTiempo);

      indiceUnidadAnterior = ui->TiempoBox->currentIndex();
      actualizarMaximoDeTiempo(indiceUnidadAnterior);
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Destructor.
 */

MainWindow::~MainWindow()
{
    if (m_csvManager) {
        m_csvManager->closeCsvFile();
    }
    delete m_fpgaProtocol;
    delete m_console;
    delete ui;
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Crea elementos de UI y completa los controles.
 */
void MainWindow::createUI()
{
    /* Si no hay puertos disponibles, deshabilita controles y retorna */
    if (QSerialPortInfo::availablePorts().size() == 0)
      {
        enable_com_controls (false);
        ui->statusBar->showMessage ("No ports detected.");
        ui->savePNGButton->setEnabled (false);
        return;
      }

    /* Lista puertos serie disponibles y llena el combo */
    for (QSerialPortInfo port : QSerialPortInfo::availablePorts())
      {
        ui->comboPort->addItem (port.portName());
      }

    /* Llena tasas de baudios estandar */
    ui->comboBaud->addItem ("1200");
    ui->comboBaud->addItem ("2400");
    ui->comboBaud->addItem ("4800");
    ui->comboBaud->addItem ("9600");
    ui->comboBaud->addItem ("19200");
    ui->comboBaud->addItem ("38400");
    ui->comboBaud->addItem ("57600");
    ui->comboBaud->addItem ("115200");
    /* Agrega tasas no estandar */
    ui->comboBaud->addItem ("128000");
    ui->comboBaud->addItem ("153600");
    ui->comboBaud->addItem ("230400");
    ui->comboBaud->addItem ("256000");
    ui->comboBaud->addItem ("460800");
    ui->comboBaud->addItem ("921600");

    /* Selecciona 115200 por defecto */
    ui->comboBaud->setCurrentIndex (7);

    /* Llena combo de bits de datos */
    ui->comboData->addItem ("8 bits");
    ui->comboData->addItem ("7 bits");

    /* Llena combo de paridad */
    ui->comboParity->addItem ("none");
    ui->comboParity->addItem ("odd");
    ui->comboParity->addItem ("even");

    /* Llena combo de bits de parada */
    ui->comboStop->addItem ("1 bit");
    ui->comboStop->addItem ("2 bits");

    /* Inicializa la lista de canales */
    ui->listWidget_Channels->clear();
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Inicializa el area de grafico (delegado en PlotManager).
 */
void MainWindow::setupPlot()
{
    if (m_plotManager) {
        m_plotManager->setupPlot();
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Habilita o deshabilita controles de comunicacion COM.
 * @param enable true para habilitar, false para deshabilitar
 */
void MainWindow::enable_com_controls (bool enable)
{
    /* Propiedades del puerto COM */
  ui->comboBaud->setEnabled (enable);
  ui->comboData->setEnabled (enable);
  ui->comboParity->setEnabled (enable);
  ui->comboPort->setEnabled (enable);
  ui->comboStop->setEnabled (enable);

    /* Acciones de barra de herramientas */
  ui->actionConnect->setEnabled (enable);
  ui->actionPause_Plot->setEnabled (!enable);
  ui->actionDisconnect->setEnabled (!enable);
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Abre el puerto serie interno con la configuracion indicada.
 * @param portInfo
 * @param baudRate
 * @param dataBits
 * @param parity
 * @param stopBits
 */
void MainWindow::openPort (QSerialPortInfo portInfo, int baudRate, QSerialPort::DataBits dataBits, QSerialPort::Parity parity, QSerialPort::StopBits stopBits)
{
    m_serialManager->openPort(portInfo, baudRate, dataBits, parity, stopBits);
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Maneja el cierre del puerto.
 */
void MainWindow::onPortClosed()
{
    updateTimer.stop();
    connected = false;
    plotting = false;

    if (m_csvManager) {
        m_csvManager->closeCsvFile();
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Muestra informacion del puerto seleccionado al cambiar el combo.
 * @param arg1
 */
void MainWindow::on_comboPort_currentIndexChanged (const QString &arg1)
{
    QSerialPortInfo selectedPort (arg1);                                                   // Muestra info del puerto seleccionado
    ui->statusBar->showMessage (selectedPort.description());
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Maneja la apertura correcta del puerto.
 */
void MainWindow::portOpenedSuccess()
{
    if (m_plotManager) {
        m_plotManager->setupPlot();
    }
    ui->statusBar->showMessage ("Connected!");

    ui->A1_0->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->B1_1->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->C1_2->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->D1_3->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->A2_4->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->B2_5->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->C2_6->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->D2_7->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->A3_8->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->B3_9->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->C3_10->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->D3_11->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->A4_12->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->B4_13->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->C4_14->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->D4_15->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->A5_16->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->B5_17->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->C5_18->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->D5_19->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->A6_20->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->B6_21->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->C6_22->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->D6_23->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->A7_24->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->B7_25->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->C7_26->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->D7_27->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->A8_28->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->B8_29->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->C8_30->setStyleSheet("background-color: rgb(150, 50, 50);");
    ui->D8_31->setStyleSheet("background-color: rgb(150, 50, 50);");
    enable_com_controls(false);
    updateTimer.start(20);
    connected = true;
    plotting = true;
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Maneja el fallo al abrir el puerto.
 */
void MainWindow::portOpenedFail()
{
    qDebug() << "Port cannot be open signal received!";
    ui->statusBar->showMessage ("Cannot open port!");
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Repinta el grafico (delegado en PlotManager).
 */
void MainWindow::replot()
{
    if (m_plotManager) {
        m_plotManager->replot();
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Procesa nueva data del puerto serie en formato de lista de cadenas.
 * @param newData
 */
void MainWindow::onNewDataArrived(QStringList newData)
{
    if (!plotting || !m_plotManager) return;

    m_plotManager->addDataPoint(m_plotManager->dataPointCount(), newData);
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Actualiza el minimo del eje Y.
 * @param arg1
 */
void MainWindow::on_spinAxesMin_valueChanged(int arg1)
{
    if (m_plotManager) {
        m_plotManager->onAxesMinChanged(arg1);
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Actualiza el maximo del eje Y.
 * @param arg1
 */
void MainWindow::on_spinAxesMax_valueChanged(int arg1)
{
    if (m_plotManager) {
        m_plotManager->onAxesMaxChanged(arg1);
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void MainWindow::writeData(const QByteArray &data)
{
    if (m_serialManager) {
        m_serialManager->writeData(data);
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Ajusta el paso de marcas del eje Y.
 * @param arg1
 */
void MainWindow::on_spinYStep_valueChanged(int arg1)
{
    if (m_plotManager) {
        m_plotManager->onYStepChanged(arg1);
    }
    ui->spinYStep->setValue(ui->plot->yAxis->ticker()->tickCount());
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Guarda una imagen PNG del grafico.
 */
void MainWindow::on_savePNGButton_clicked()
{
    if (m_plotManager) {
        m_plotManager->savePlotImage();
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Muestra coordenadas del puntero sobre el grafico en la barra de estado.
 * @param event
 */
void MainWindow::onMouseMoveInPlot(QMouseEvent *event)
{
    if (m_plotManager) {
        m_plotManager->onMouseMove(event);
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Sincroniza seleccion entre curva y leyenda (canal).
 */
void MainWindow::channel_selection (void)
{
    if (m_plotManager) {
        m_plotManager->onChannelSelection();
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Renombra un grafico con doble click en su item de leyenda.
 * @param legend
 * @param item
 */
void MainWindow::legend_double_click(QCPLegend *legend, QCPAbstractLegendItem *item, QMouseEvent *event)
{
    if (m_plotManager) {
        m_plotManager->onLegendDoubleClick(legend, item, event);
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Ajusta cuantos puntos de datos se muestran en pantalla.
 * @param arg1
 */
void MainWindow::on_spinPoints_valueChanged (int arg1)
{
    if (m_plotManager) {
        m_plotManager->onPointsChanged(arg1);
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Abre la ventana de ayuda.
 */
void MainWindow::on_actionHow_to_use_triggered()
{
  helpWindow = new HelpWindow (this);
  helpWindow->setWindowTitle ("How to use this application");
  helpWindow->show();
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Conecta al puerto COM o reinicia el ploteo si ya estaba conectado.
 */
void MainWindow::on_actionConnect_triggered()
{
  if (connected)
    {
            /* Si ya esta conectado, reinicia si estaba pausado */
      if (!plotting)
                {                                                                                   // Reinicia ploteo
                    updateTimer.start();                                                              // Reinicia timer de actualizacion
          plotting = true;
          ui->actionConnect->setEnabled (false);
          ui->actionPause_Plot->setEnabled (true);
          ui->statusBar->showMessage ("Plot restarted!");
        }
    }
  else
    {
    /* Si no esta conectado, toma parametros de UI y conecta */
    QSerialPortInfo portInfo (ui->comboPort->currentText());                          // Objeto temporal para crear QSerialPort
    int baudRate = ui->comboBaud->currentText().toInt();                              // Baud rate seleccionado
    int dataBitsIndex = ui->comboData->currentIndex();                                // Indice de bits de datos
    int parityIndex = ui->comboParity->currentIndex();                                // Indice de paridad
    int stopBitsIndex = ui->comboStop->currentIndex();                                // Indice de bits de parada
      QSerialPort::DataBits dataBits;
      QSerialPort::Parity parity;
      QSerialPort::StopBits stopBits;

    /* Configura bits de datos segun el indice seleccionado */
      switch (dataBitsIndex)
        {
        case 0:
          dataBits = QSerialPort::Data8;
          break;
        default:
          dataBits = QSerialPort::Data7;
        }

    /* Configura paridad segun el indice seleccionado */
      switch (parityIndex)
        {
        case 0:
          parity = QSerialPort::NoParity;
          break;
        case 1:
          parity = QSerialPort::OddParity;
          break;
        default:
          parity = QSerialPort::EvenParity;
        }

    /* Configura bits de parada segun el indice seleccionado */
      switch (stopBitsIndex)
        {
        case 0:
          stopBits = QSerialPort::OneStop;
          break;
        default:
          stopBits = QSerialPort::TwoStop;
        }

    /* Abre el puerto serie con la configuracion construida */
      openPort (portInfo, baudRate, dataBits, parity, stopBits);
  }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Pausa el ploteo manteniendo abierto el puerto COM.
 */
void MainWindow::on_actionPause_Plot_triggered()
{
  if (plotting)
    {
    updateTimer.stop();                                                               // Detiene timer de actualizacion
      plotting = false;
      ui->actionConnect->setEnabled (true);
      ui->actionPause_Plot->setEnabled (false);
      ui->statusBar->showMessage ("Plot paused, new data will be ignored");
      cambiarEstado("PAUSA (Experimento Interrumpido)", "orange");
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Inicia o detiene la grabacion del stream en CSV.
 */
void MainWindow::on_actionRecord_stream_triggered()
{
    if (ui->actionRecord_stream->isChecked())
    {
        m_csvManager->openCsvFile(this);
        if (!m_csvManager->isOpen()) {
            ui->actionRecord_stream->setChecked(false);
        }
    }
    else
    {
        m_csvManager->closeCsvFile();
        ui->statusBar->showMessage("Grabación detenida.");
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Cierra el puerto COM y detiene el ploteo.
 */
void MainWindow::on_actionDisconnect_triggered()
{
  if (connected)
    {
      if (m_serialManager) {
          m_serialManager->closePort();
      }

      enviar = false;
      connected = false;
      plotting = false;

      ui->actionConnect->setEnabled(true);
      ui->actionPause_Plot->setEnabled(false);
      ui->actionDisconnect->setEnabled(false);
      ui->savePNGButton->setEnabled(false);
      enable_com_controls(true);

      receivedData.clear();
      ui->textEdit_UartWindow->append(receivedData);

      limpiarMatrizInterna();

      ui->statusBar->showMessage("Disconnected!");
      cambiarEstado("DETENIDO (Requiere Rearme)", "red");
    }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Limpia todos los canales y resetea el area de grafico.
 *
 * Esta funcion no borra definiciones persistentes de la leyenda.
 */
void MainWindow::on_actionClear_triggered()
{
    ui->plot->clearPlottables();
    ui->listWidget_Channels->clear();
    channels = 0;
    dataPointNumber = 0;
    emit setupPlot();
    ui->plot->replot();
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void MainWindow::on_pushButton_ShowallData_clicked()
{
    // Alterna entre mostrar el flujo crudo completo o solo mensajes filtrados.
    if(ui->pushButton_ShowallData->isChecked())
    {
        filterDisplayedData = false;
        ui->pushButton_ShowallData->setText("Filter Incoming Data");
    }
    else
    {
        filterDisplayedData = true;
        ui->pushButton_ShowallData->setText("Show All Incoming Data");
    }
}

void MainWindow::on_pushButton_AutoScale_clicked()
{
    // Reescala el eje Y segun los datos visibles y actualiza los spinboxes.
    ui->plot->yAxis->rescale(true);
    ui->spinAxesMax->setValue(int(ui->plot->yAxis->range().upper) + int(ui->plot->yAxis->range().upper*0.1));
    ui->spinAxesMin->setValue(int(ui->plot->yAxis->range().lower) + int(ui->plot->yAxis->range().lower*0.1));
}

void MainWindow::on_pushButton_ResetVisible_clicked()
{
    // Vuelve a mostrar todos los graficos y limpia el resaltado de la lista.
    for(int i=0; i<ui->plot->graphCount(); i++)
    {
        ui->plot->graph(i)->setVisible(true);
        ui->listWidget_Channels->item(i)->setBackground(Qt::NoBrush);
    }
}

void MainWindow::on_listWidget_Channels_itemDoubleClicked(QListWidgetItem *item)
{
    // Alterna visibilidad del canal seleccionado con doble click en la lista.
    int graphIdx = ui->listWidget_Channels->currentRow();

    if(ui->plot->graph(graphIdx)->visible())
    {
        ui->plot->graph(graphIdx)->setVisible(false);
        item->setBackgroundColor(Qt::black);
    }
    else
    {
        ui->plot->graph(graphIdx)->setVisible(true);
        item->setBackground(Qt::NoBrush);
    }
    ui->plot->replot();
}

void MainWindow::on_pushButton_clicked()
{
    // Recarga la lista de puertos disponibles en el combo de seleccion.
    ui->comboPort->clear();
    /* Lista puertos disponibles y los agrega al combo */
    for (QSerialPortInfo port : QSerialPortInfo::availablePorts())
    {
        ui->comboPort->addItem (port.portName());
    }
}

void MainWindow::initActionsConnections()
{
    // Reservado para conexiones de acciones adicionales del menu/barra.
}

void MainWindow::on_EnviarDatos_clicked()
{
    // Envia al equipo el comando de inicio y el paquete extendido de configuracion.
    if (connected == true)
    {
        cambiarEstado("Enviando (Extendido 48B)...", "green");

        limpiarPlot();
        QStringList labels = m_fpgaProtocol->generateLabels();
        QVector<int> tramaIndices = m_fpgaProtocol->activeTramaIndices();
        if (m_plotManager) {
            m_plotManager->setActiveTramaIndices(tramaIndices);
            m_plotManager->setupGraphsFromLabels(labels);
            channels = m_plotManager->channelCount();
        }

        QByteArray startCommand = m_fpgaProtocol->buildStartCommand();
        QByteArray packet = m_fpgaProtocol->buildExtendedPacket(ui->TiempoNum->value(), ui->TiempoBox->currentIndex());

        if (m_serialManager) {
            m_serialManager->writeData(startCommand);
            m_serialManager->writeData(packet);
            m_serialManager->writeData(startCommand);
        }

        qDebug() << "Paquete Extendido Enviado:" << packet.toHex();
    }
    else
    {
        emit portOpenFail();
        cambiarEstado("ERROR: Desconectado", "red");
        ui->statusBar->showMessage("MASTER: CONFIGURASTE EL PUERTO??");
    }
}
void MainWindow::on_ResetearDatos_clicked()
{
    // Reinicia estado remoto y local: limpia interfaz y reenvia configuracion base.
    if (connected == true)
    {
        cambiarEstado("RESETEANDO...", "orange");

        if (m_serialManager) {
            m_serialManager->writeData(m_fpgaProtocol->buildResetCommand());
        }

        emit portOpenOK();

        ui->textEdit_UartWindow->clear();
        ui->textEdit_UartWindow->append(receivedData);

        limpiarMatrizInterna();

        if (m_serialManager) {
            m_serialManager->writeData(m_fpgaProtocol->buildResetSweepPacket());
        }

        cambiarEstado("Esperando...", "black");
    }
    else
    {
        emit portOpenFail();
        cambiarEstado("ERROR: Desconectado", "red");
        ui->statusBar->showMessage ("MASTER: CONFIGURASTE EL PUERTO??");
    }
}

void MainWindow::on_actionEsconder_Caja_de_Texto_toggled(bool arg1)
{
    // Muestra u oculta el cuadro de texto de UART segun el estado del toggle.
    if (arg1)
    {
        ui->textEdit_UartWindow->setVisible(false);
        ui->pushButton_TextEditHide->setText("Show TextBox");
    }
     else
     {
        ui->textEdit_UartWindow->setVisible(true);
    }
}

void MainWindow::on_ir_a_grafico_clicked()
{
    // Cambia a la vista de grafico.
    ui->stackedWidget->setCurrentIndex(1);
}

void MainWindow::on_actionconfig_triggered()
{
    // Alterna entre vista de configuracion y vista de grafico.
    if (ui->stackedWidget->currentIndex() == 0)
        ui->stackedWidget->setCurrentIndex(1);
    else
        ui->stackedWidget->setCurrentIndex(0);
}

void MainWindow::actualizarMaximoDeTiempo(int nuevoIndice)
{
    // Convierte el valor actual entre unidades y ajusta limites validos del spinbox.
    int valorActual = ui->TiempoNum->value();
    quint32 numeroBase = m_fpgaProtocol->convertToBase(valorActual, indiceUnidadAnterior);
    int nuevoValor = m_fpgaProtocol->convertFromBase(numeroBase, nuevoIndice);

    int nuevoMinimo;
    int nuevoMaximo; // Usamos int porque el QSpinBox es int32

    switch (nuevoIndice) {
        case 0: // µs
            nuevoMinimo = 5600;       // Mínimo físico: 5.6 ms
            // El máximo teórico es 9,999,999,900, pero el QSpinBox llega a 2,147,483,647.
            // Usamos el límite del widget, que entra sobrado en 8 dígitos.
            nuevoMaximo = 2147483647;
            break;

        case 1: // ms
            nuevoMinimo = 6;          // 5.6 ms redondeado a 6
            // 99,999,999 / 10 = 9,999,999
            nuevoMaximo = 9999999;
            break;

        case 2: // s
            nuevoMinimo = 1;          // 1s > 5.6ms
            // 99,999,999 / 10,000 = 9,999
            nuevoMaximo = 9999;
            break;

        case 3: // min
            nuevoMinimo = 1;
            // 99,999,999 / 600,000 = 166
            nuevoMaximo = 166;
            break;

        case 4: // hs
            nuevoMinimo = 1;
            // 99,999,999 / 36,000,000 = 2.77
            nuevoMaximo = 2; // Máximo 2 horas
            break;

        default:
            nuevoMinimo = 1;
            nuevoMaximo = 99999;
    }

    ui->TiempoNum->setRange(nuevoMinimo, nuevoMaximo);
    ui->TiempoNum->setValue(nuevoValor);

    indiceUnidadAnterior = nuevoIndice;
}

// Propaga cambios de controles de timing hacia la configuracion del protocolo FPGA.
void MainWindow::on_Ancho_de_pulso_valueChanged(int arg1) { m_fpgaProtocol->setPulseWidth(static_cast<quint8>(arg1)); }
void MainWindow::on_Delay_A_valueChanged(int arg1)        { m_fpgaProtocol->setDelayA(static_cast<quint8>(arg1)); }
void MainWindow::on_Delay_B_valueChanged(int arg1)        { m_fpgaProtocol->setDelayB(static_cast<quint8>(arg1)); }
void MainWindow::on_Delay_C_valueChanged(int arg1)        { m_fpgaProtocol->setDelayC(static_cast<quint8>(arg1)); }
void MainWindow::on_Delay_D_valueChanged(int arg1)        { m_fpgaProtocol->setDelayD(static_cast<quint8>(arg1)); }

void MainWindow::actualizarEstadoGraf(int indiceBotonPresionado)
{
    // Marca visualmente la columna activa y la comunica al protocolo.
    for (QPushButton* boton : botonesGraf) {
        boton->setStyleSheet("background-color: rgb(150, 50, 50);");
    }

    botonesGraf[indiceBotonPresionado]->setStyleSheet("background-color: rgb(15, 125, 15);");
    columnaSeleccionada = indiceBotonPresionado;
    m_fpgaProtocol->selectColumn(indiceBotonPresionado);
}

void MainWindow::actualizarBotonDato(int bit, QPushButton* boton)
{
    // Alterna un bit de datos y refleja su estado con color en el boton.
    m_fpgaProtocol->toggleButton(bit);
    if (m_fpgaProtocol->buttonState(bit)) {
        boton->setStyleSheet("background-color: rgb(15, 125, 15);"); // Verde
    } else {
        boton->setStyleSheet("background-color: rgb(150, 50, 50);"); // Rojo
    }
}

void MainWindow::cambiarEstado(QString texto, QString color)
{
    // Actualiza el indicador de estado permanente en la barra de estado.
    statusLabel->setText(texto);
    statusLabel->setStyleSheet("color: " + color + "; font-weight: bold;");
}

void MainWindow::limpiarMatrizInterna()
{
    // Resetea matriz en FPGA, colores de botones y columna seleccionada local.
    if (m_fpgaProtocol) {
        m_fpgaProtocol->resetMatrix();
    }

    for (QPushButton* boton : botonesDatos) {
        boton->setStyleSheet("background-color: rgb(150, 50, 50);");
    }

    for (QPushButton* boton : botonesGraf) {
        boton->setStyleSheet("background-color: rgb(150, 50, 50);");
    }

    columnaSeleccionada = 0;
}

// ─── limpiarPlot ──────────────────────────────────────────────────────────────
// Resetea el plot completamente antes de aplicar una nueva configuración.
void MainWindow::limpiarPlot()
{
    if (m_plotManager) {
        m_plotManager->clearPlot();
        dataPointNumber = m_plotManager->dataPointCount();
        channels = m_plotManager->channelCount();
    }
}

QStringList MainWindow::generarLabels()
{
    // Expone las etiquetas activas calculadas por el protocolo FPGA.
    return m_fpgaProtocol->generateLabels();
}

