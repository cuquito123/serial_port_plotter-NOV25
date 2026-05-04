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
#include <QJsonObject>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QThread>
#include <QCoreApplication>
#include <QDateTime>

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
  m_fpgaProtocolApplied(new FpgaProtocol()),
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
  connect(m_serialManager, &SerialPortManager::writeFailed, this, [this](const QString &err){
      qDebug() << "Serial write failed:" << err;
      ui->statusBar->showMessage("Serial write failed: " + err);
  });
  connect(m_serialManager, &SerialPortManager::writeSucceeded, this, [this](qint64 bytes){
      qDebug() << "Serial write succeeded, bytes:" << bytes;
  });

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
    // Guardar parámetros para recuperación
    m_lastConnectionParams.portInfo = portInfo;
    m_lastConnectionParams.baudRate = baudRate;
    m_lastConnectionParams.dataBits = dataBits;
    m_lastConnectionParams.parity = parity;
    m_lastConnectionParams.stopBits = stopBits;
    
    logEvent(EventType::PortOpened, QString("Abriendo puerto %1 a %2 bps").arg(portInfo.portName()).arg(baudRate));
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

    logEvent(EventType::PortClosed, "Puerto serie cerrado");

    // Transición de máquina de estados
    setAppState(AppState::Disconnected);
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
    
    logEvent(EventType::PortOpened, "Conexión exitosa al puerto serie");
    resetHealthMetrics();

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

    // Transición de máquina de estados
    setAppState(AppState::ReadyForConfiguration);
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

    // Actualizar métricas de salud
    updateHealthMetrics(newData);
    
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
                {
                    // Preflight check antes de reanudar adquisición
                    PreflightResult check = performPreflightCheck();
                    if (!check.success) {
                        logEvent(EventType::Error, "Preflight check fallido al reanudar: " + check.errorMessage);
                        setAppState(AppState::Fault);
                        ui->statusBar->showMessage(check.errorMessage);
                        cambiarEstado("FALLA: " + check.errorMessage, "red");
                        return;
                    }

                    logEvent(EventType::Started, "Iniciando adquisición de datos");
                    updateTimer.start();
          plotting = true;
          ui->actionConnect->setEnabled (false);
          ui->actionPause_Plot->setEnabled (true);
          ui->statusBar->showMessage ("Plot restarted!");

          // Transición de máquina de estados
          setAppState(AppState::Acquiring);
        }
    }
  else
    {
    /* Si no esta conectado, toma parametros de UI y conecta */
    QSerialPortInfo portInfo (ui->comboPort->currentText());
    int baudRate = ui->comboBaud->currentText().toInt();
    int dataBitsIndex = ui->comboData->currentIndex();
    int parityIndex = ui->comboParity->currentIndex();
    int stopBitsIndex = ui->comboStop->currentIndex();
      QSerialPort::DataBits dataBits;
      QSerialPort::Parity parity;
      QSerialPort::StopBits stopBits;

      switch (dataBitsIndex)
        {
        case 0:
          dataBits = QSerialPort::Data8;
          break;
        default:
          dataBits = QSerialPort::Data7;
        }

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

      switch (stopBitsIndex)
        {
        case 0:
          stopBits = QSerialPort::OneStop;
          break;
        default:
          stopBits = QSerialPort::TwoStop;
        }

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
    logEvent(EventType::Stopped, "Pausando adquisición de datos");
    updateTimer.stop();
      plotting = false;
      ui->actionConnect->setEnabled (true);
      ui->actionPause_Plot->setEnabled (false);
      ui->statusBar->showMessage ("Plot paused, new data will be ignored");
      cambiarEstado("PAUSA (Experimento Interrumpido)", "orange");

      // Transición de máquina de estados
      setAppState(AppState::Paused);
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
      logEvent(EventType::Stopped, "Desconectando puerto");
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

      // Transición de máquina de estados
      setAppState(AppState::Disconnected);
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
    // Preflight check
    PreflightResult check = performPreflightCheck();
    if (!check.success) {
        logEvent(EventType::Error, "Preflight check fallido: " + check.errorMessage);
        setAppState(AppState::Fault);
        ui->statusBar->showMessage(check.errorMessage);
        cambiarEstado("FALLA: " + check.errorMessage, "red");
        return;
    }

    // Aplicar configuración al FPGA
    cambiarEstado("Aplicando y armando...", "green");
    logEvent(EventType::ConfigApplied, "Aplicando configuración al FPGA");

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
        QThread::msleep(10);
        m_serialManager->writeData(packet);
        QThread::msleep(10);
        m_serialManager->writeData(startCommand);
    }

    qDebug() << "Paquete Extendido Enviado:" << packet.toHex();

    // Snapshot: guardar configuración aplicada
    // (en futuro se puede clonar FpgaProtocol para comparativa)

    // Limpiar indicador de cambios pendientes
    clearPendingChanges();

    // Transición de máquina de estados
    cambiarEstado("Listo para ejecutar. Presioná 'Conectar'.", "blue");
    setAppState(AppState::ReadyForExecution);
}
void MainWindow::on_ResetearDatos_clicked()
{
    // Reinicia estado remoto y local: limpia interfaz y reenvia configuracion base.
    if (connected == true)
    {
        logEvent(EventType::Reset, "Ejecutando reset de FPGA");
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
    
    // Marcar cambios pendientes cuando cambia unidad de tiempo
    markPendingChanges();
}

// Propaga cambios de controles de timing hacia la configuracion del protocolo FPGA.
void MainWindow::on_Ancho_de_pulso_valueChanged(int arg1) { m_fpgaProtocol->setPulseWidth(static_cast<quint8>(arg1)); markPendingChanges(); }
void MainWindow::on_Delay_A_valueChanged(int arg1)        { m_fpgaProtocol->setDelayA(static_cast<quint8>(arg1)); markPendingChanges(); }
void MainWindow::on_Delay_B_valueChanged(int arg1)        { m_fpgaProtocol->setDelayB(static_cast<quint8>(arg1)); markPendingChanges(); }
void MainWindow::on_Delay_C_valueChanged(int arg1)        { m_fpgaProtocol->setDelayC(static_cast<quint8>(arg1)); markPendingChanges(); }
void MainWindow::on_Delay_D_valueChanged(int arg1)        { m_fpgaProtocol->setDelayD(static_cast<quint8>(arg1)); markPendingChanges(); }

void MainWindow::actualizarEstadoGraf(int indiceBotonPresionado)
{
    // Marca visualmente la columna activa y la comunica al protocolo.
    for (QPushButton* boton : botonesGraf) {
        boton->setStyleSheet("background-color: rgb(150, 50, 50);");
    }

    botonesGraf[indiceBotonPresionado]->setStyleSheet("background-color: rgb(15, 125, 15);");
    columnaSeleccionada = indiceBotonPresionado;
    m_fpgaProtocol->selectColumn(indiceBotonPresionado);
    
    // Marcar cambios pendientes
    markPendingChanges();
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
    
    // Marcar cambios pendientes
    markPendingChanges();
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

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

// ─── Máquina de Estados Operativa ────────────────────────────────────────

void MainWindow::setAppState(AppState newState)
{
    if (m_appState == newState) {
        return; // No cambiar si ya está en ese estado
    }

    // Registrar transición
    qDebug() << "Estado anterior:" << getStateDisplayName(m_appState)
             << "-> Estado nuevo:" << getStateDisplayName(newState);

    m_appState = newState;
    updateUIForState();
}

QString MainWindow::getStateDisplayName(AppState state) const
{
    switch (state) {
    case AppState::Disconnected:
        return "Desconectado";
    case AppState::ReadyForConfiguration:
        return "Listo para configurar";
    case AppState::ReadyForExecution:
        return "Listo para ejecutar";
    case AppState::Acquiring:
        return "Adquiriendo";
    case AppState::Paused:
        return "Pausado";
    case AppState::Fault:
        return "Falla";
    default:
        return "Desconocido";
    }
}

void MainWindow::updateUIForState()
{
    const bool isDisconnected = (m_appState == AppState::Disconnected);
    const bool isReadyForConfig = (m_appState == AppState::ReadyForConfiguration);
    const bool isReadyForExecution = (m_appState == AppState::ReadyForExecution);
    const bool isAcquiring = (m_appState == AppState::Acquiring);
    const bool isPaused = (m_appState == AppState::Paused);
    const bool isFault = (m_appState == AppState::Fault);

    // Habilitar/deshabilitar controles según el estado
    bool canConfigure = isReadyForConfig || isReadyForExecution || isPaused;
    bool canExecute = isReadyForExecution;
    bool canPause = isAcquiring;
    bool canResume = isPaused;

    // Controles de puerto COM
    ui->comboPort->setEnabled(isDisconnected);
    ui->comboBaud->setEnabled(isDisconnected);
    ui->comboData->setEnabled(isDisconnected);
    ui->comboParity->setEnabled(isDisconnected);
    ui->comboStop->setEnabled(isDisconnected);

    // Botones de acción principal
    ui->actionConnect->setEnabled(isDisconnected || canResume);
    ui->actionDisconnect->setEnabled(!isDisconnected && !isFault);
    ui->actionPause_Plot->setEnabled(canPause);

    // Controles de configuración
    bool enableConfig = canConfigure && !isFault;
    for (auto btn : botonesGraf) btn->setEnabled(enableConfig);
    for (auto btn : botonesDatos) btn->setEnabled(enableConfig);
    ui->TiempoNum->setEnabled(enableConfig);
    ui->TiempoBox->setEnabled(enableConfig);
    ui->Ancho_de_pulso->setEnabled(enableConfig);
    ui->Delay_A->setEnabled(enableConfig);
    ui->Delay_B->setEnabled(enableConfig);
    ui->Delay_C->setEnabled(enableConfig);
    ui->Delay_D->setEnabled(enableConfig);

    // Botones de envío/reset
    ui->EnviarDatos->setEnabled(enableConfig);
    ui->ResetearDatos->setEnabled(enableConfig && (isReadyForConfig || isPaused));

    // Grabación CSV
    ui->actionRecord_stream->setEnabled(isAcquiring || isPaused);

    // Actualizar mensaje de estado
    QString stateMsg = getStateDisplayName(m_appState);
    QString color = "black";

    switch (m_appState) {
    case AppState::Disconnected:
        stateMsg += " - Abrí puerto para comenzar";
        color = "red";
        break;
    case AppState::ReadyForConfiguration:
        stateMsg += " - Configurá matriz y parámetros";
        color = "blue";
        break;
    case AppState::ReadyForExecution:
        stateMsg += " - Presioná 'Conectar' para iniciar";
        color = "green";
        break;
    case AppState::Acquiring:
        stateMsg += " - Adquisición en progreso";
        color = "darkgreen";
        break;
    case AppState::Paused:
        stateMsg += " - Reconfigurar o reanudar";
        color = "orange";
        break;
    case AppState::Fault:
        stateMsg += " - Error detectado, desconectar para recuperar";
        color = "red";
        break;
    }

    cambiarEstado(stateMsg, color);
}

bool MainWindow::canTransitionToState(AppState newState) const
{
    // Define transiciones válidas
    switch (m_appState) {
    case AppState::Disconnected:
        return (newState == AppState::ReadyForConfiguration || newState == AppState::Fault);
    case AppState::ReadyForConfiguration:
        return (newState == AppState::ReadyForExecution || newState == AppState::Disconnected || newState == AppState::Fault);
    case AppState::ReadyForExecution:
        return (newState == AppState::Acquiring || newState == AppState::ReadyForConfiguration || newState == AppState::Disconnected || newState == AppState::Fault);
    case AppState::Acquiring:
        return (newState == AppState::Paused || newState == AppState::Disconnected || newState == AppState::Fault);
    case AppState::Paused:
        return (newState == AppState::Acquiring || newState == AppState::ReadyForConfiguration || newState == AppState::Disconnected || newState == AppState::Fault);
    case AppState::Fault:
        return (newState == AppState::Disconnected);
    default:
        return false;
    }
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

// ─── Gestión de Cambios Pendientes ─────────────────────────────────────────

void MainWindow::markPendingChanges()
{
    if (!m_hasPendingChanges) {
        m_hasPendingChanges = true;
        updatePendingChangesIndicator();
    }
}

void MainWindow::clearPendingChanges()
{
    if (m_hasPendingChanges) {
        m_hasPendingChanges = false;
        updatePendingChangesIndicator();
    }
}

void MainWindow::updatePendingChangesIndicator()
{
    // Actualizar estilo y mensaje según estado de cambios pendientes
    if (m_hasPendingChanges) {
        // Resaltar botón "Aplicar y Armar" en naranja para atraer atención
        ui->EnviarDatos->setStyleSheet("background-color: rgb(255, 140, 0); color: white; font-weight: bold;");
        ui->statusBar->showMessage("⚠ Cambios pendientes de aplicar. Presioná 'Aplicar y Armar'.");
    } else {
        // Restaurar color normal del botón
        ui->EnviarDatos->setStyleSheet("");
        ui->statusBar->showMessage("");
    }
}

MainWindow::PreflightResult MainWindow::performPreflightCheck()
{
    PreflightResult result;
    result.success = true;

    // 1. Verificar puerto abierto
    if (!connected) {
        result.errorMessage = "Puerto serie no conectado. Abrí puerto primero.";
        return result;
    }

    // 2. Verificar que haya canales activos
    QStringList labels = m_fpgaProtocol->generateLabels();
    if (labels.isEmpty()) {
        result.errorMessage = "Sin canales activos. Configurá matriz de botones.";
        return result;
    }

    // 3. Verificar tiempo válido
    int timeValue = ui->TiempoNum->value();
    if (timeValue <= 0) {
        result.errorMessage = "Valor de tiempo inválido. Asegurate de que sea > 0.";
        return result;
    }

    // 4. Verificar rango de tiempos
    int timeUnitIndex = ui->TiempoBox->currentIndex();
    quint32 baseTime = m_fpgaProtocol->convertToBase(timeValue, timeUnitIndex);
    if (baseTime < 56) {  // 5.6 ms en unidad base
        result.errorMessage = "Tiempo mínimo permitido: 5.6 ms.";
        return result;
    }
    if (baseTime > 99999999) {
        result.errorMessage = "Tiempo máximo permitido: 99,999,999 (unidad base).";
        return result;
    }

    // 5. Verificar CSV si está grabando
    if (ui->actionRecord_stream->isChecked()) {
        if (!m_csvManager || !m_csvManager->isOpen()) {
            result.errorMessage = "Grabación CSV habilitada pero archivo no está abierto.";
            return result;
        }
    }

    result.success = true;
    return result;
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

// ─── Telemetría de Salud en Tiempo Real ────────────────────────────────────────

void MainWindow::updateHealthMetrics(const QStringList &newData)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    
    if (m_healthMetrics.firstPacketTime == 0) {
        m_healthMetrics.firstPacketTime = now;
    }
    
    m_healthMetrics.lastPacketTimestampMs = now;
    m_healthMetrics.validPacketCount++;
    
    // Calcular latencia estimada en ms (diferencia entre muestras)
    static qint64 lastTime = 0;
    if (lastTime > 0) {
        m_healthMetrics.frameLatencyMs = static_cast<float>(now - lastTime);
    }
    lastTime = now;
}

void MainWindow::resetHealthMetrics()
{
    m_healthMetrics.validPacketCount = 0;
    m_healthMetrics.invalidPacketCount = 0;
    m_healthMetrics.lostPacketCount = 0;
    m_healthMetrics.lastPacketTimestampMs = 0;
    m_healthMetrics.frameLatencyMs = 0.0f;
    m_healthMetrics.firstPacketTime = 0;
}

QString MainWindow::getHealthMetricsString() const
{
    return QString(
        "Paquetes válidos: %1 | Inválidos: %2 | Latencia: %.1f ms | Última recepción: %3 ms"
    ).arg(m_healthMetrics.validPacketCount)
     .arg(m_healthMetrics.invalidPacketCount)
     .arg(m_healthMetrics.frameLatencyMs, 0, 'f', 1)
     .arg(m_healthMetrics.lastPacketTimestampMs);
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

// ─── Recuperación Guiada ante Fallos ───────────────────────────────────────────

bool MainWindow::isRecoveryPossible() const
{
    return m_appState == AppState::Fault && !m_lastConnectionParams.portInfo.portName().isEmpty();
}

void MainWindow::attemptRecovery()
{
    logEvent(EventType::Recovery, "Iniciando secuencia de recuperación");
    
    // 1. Cerrar puerto si está abierto
    if (connected && m_serialManager) {
        m_serialManager->closePort();
        QThread::msleep(100);  // Pequeña pausa para limpiar buffers
    }
    
    // 2. Resetear estado local
    resetHealthMetrics();
    limpiarMatrizInterna();
    limpiarPlot();
    
    // 3. Limpiar parser
    if (m_messageParser) {
        // Reset interno del parser si es necesario
    }
    
    // 4. Reconectar con parámetros previos
    openPort(
        m_lastConnectionParams.portInfo,
        m_lastConnectionParams.baudRate,
        m_lastConnectionParams.dataBits,
        m_lastConnectionParams.parity,
        m_lastConnectionParams.stopBits
    );
    
    logEvent(EventType::Recovery, "Secuencia de recuperación completada");
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

// ─── Trazabilidad Operativa (Event Logging) ───────────────────────────────

void MainWindow::logEvent(EventType type, const QString &description)
{
    OperativeEvent event;
    event.timestampMs = QDateTime::currentMSecsSinceEpoch();
    event.eventType = type;
    event.description = description;
    
    m_eventLog.append(event);
    
    // Limitar tamaño del log
    if (m_eventLog.size() > MAX_LOG_ENTRIES) {
        m_eventLog.removeFirst();
    }
    
    // Debug: mostrar en consola
    qDebug() << event.toString();
}

QString MainWindow::getEventLogAsString(int maxEntries) const
{
    QString result;
    int startIdx = qMax(0, m_eventLog.size() - maxEntries);
    
    for (int i = startIdx; i < m_eventLog.size(); ++i) {
        result += m_eventLog.at(i).toString() + "\n";
    }
    
    return result;
}

void MainWindow::clearEventLog()
{
    m_eventLog.clear();
}

void MainWindow::exportEventLog(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logEvent(EventType::Error, "No se pudo abrir archivo de log: " + filePath);
        return;
    }
    
    QTextStream stream(&file);
    stream << getEventLogAsString(m_eventLog.size());
    file.close();
    
    logEvent(EventType::ConfigApplied, "Log exportado a: " + filePath);
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

// ─── Perfiles Operativos (Config Profiles) ────────────────────────────────

QString MainWindow::getProfilesDirectory() const
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString profilesDir = appDir + "/profiles";
    
    QDir dir(profilesDir);
    if (!dir.exists()) {
        QDir().mkpath(profilesDir);
    }
    
    return profilesDir;
}

void MainWindow::saveProfile(const QString &profileName)
{
    if (profileName.isEmpty()) {
        logEvent(EventType::Error, "Nombre de perfil vacío");
        return;
    }
    
    OperativeProfile profile;
    profile.name = profileName;
    profile.matrixButtons = m_fpgaProtocol->getTecla();  // Requiere método en FpgaProtocol
    profile.pulseWidth = m_fpgaProtocol->getPulseWidth();
    profile.delayA = m_fpgaProtocol->getDelayA();
    profile.delayB = m_fpgaProtocol->getDelayB();
    profile.delayC = m_fpgaProtocol->getDelayC();
    profile.delayD = m_fpgaProtocol->getDelayD();
    profile.timeValue = ui->TiempoNum->value();
    profile.timeUnitIndex = ui->TiempoBox->currentIndex();
    profile.createdTimestamp = QDateTime::currentMSecsSinceEpoch();
    
    QString filePath = getProfilesDirectory() + "/" + profileName + ".json";
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logEvent(EventType::Error, "No se pudo guardar perfil: " + profileName);
        return;
    }
    
    file.write(profile.toJson().toUtf8());
    file.close();
    
    logEvent(EventType::ConfigApplied, "Perfil guardado: " + profileName);
}

void MainWindow::loadProfile(const QString &profileName)
{
    QString filePath = getProfilesDirectory() + "/" + profileName + ".json";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logEvent(EventType::Error, "No se pudo cargar perfil: " + profileName);
        return;
    }
    
    QString json = QString::fromUtf8(file.readAll());
    file.close();
    
    OperativeProfile profile = OperativeProfile::fromJson(json);
    
    // Aplicar configuración
    if (!profile.name.isEmpty()) {
        // Actualizar matriz
        for (int i = 0; i < qMin(32, profile.matrixButtons.size()); ++i) {
            if (botonesDatos.size() > i) {
                if (profile.matrixButtons.testBit(i) != (botonesDatos[i]->styleSheet().contains("15, 125, 15"))) {
                    actualizarBotonDato(i, botonesDatos[i]);
                }
            }
        }
        
        // Actualizar parámetros de timing
        ui->Ancho_de_pulso->setValue(profile.pulseWidth);
        ui->Delay_A->setValue(profile.delayA);
        ui->Delay_B->setValue(profile.delayB);
        ui->Delay_C->setValue(profile.delayC);
        ui->Delay_D->setValue(profile.delayD);
        ui->TiempoBox->setCurrentIndex(profile.timeUnitIndex);
        ui->TiempoNum->setValue(profile.timeValue);
        
        logEvent(EventType::ConfigApplied, "Perfil cargado: " + profileName);
    }
}

void MainWindow::deleteProfile(const QString &profileName)
{
    QString filePath = getProfilesDirectory() + "/" + profileName + ".json";
    if (QFile::remove(filePath)) {
        logEvent(EventType::ConfigApplied, "Perfil eliminado: " + profileName);
    } else {
        logEvent(EventType::Error, "No se pudo eliminar perfil: " + profileName);
    }
}

QStringList MainWindow::getProfileNames() const
{
    QStringList names;
    QDir dir(getProfilesDirectory());
    QStringList filters;
    filters << "*.json";
    dir.setNameFilters(filters);
    
    foreach (QString filename, dir.entryList()) {
        names << filename.left(filename.length() - 5);  // Quitar .json
    }
    
    return names;
}

// ─── Implementación de Serialización de Perfil ─────────────────────────────

QString MainWindow::OperativeProfile::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;
    obj["pulseWidth"] = (int)pulseWidth;
    obj["delayA"] = (int)delayA;
    obj["delayB"] = (int)delayB;
    obj["delayC"] = (int)delayC;
    obj["delayD"] = (int)delayD;
    obj["timeValue"] = timeValue;
    obj["timeUnitIndex"] = timeUnitIndex;
    obj["createdTimestamp"] = (qint64)createdTimestamp;
    
    // Serializar matriz de bits
    QString matrixStr;
    for (int i = 0; i < matrixButtons.size(); ++i) {
        matrixStr += matrixButtons.testBit(i) ? "1" : "0";
    }
    obj["matrixButtons"] = matrixStr;
    
    QJsonDocument doc(obj);
    return QString::fromUtf8(doc.toJson());
}

MainWindow::OperativeProfile MainWindow::OperativeProfile::fromJson(const QString &json)
{
    OperativeProfile profile;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    QJsonObject obj = doc.object();
    
    profile.name = obj["name"].toString();
    profile.pulseWidth = (quint8)obj["pulseWidth"].toInt();
    profile.delayA = (quint8)obj["delayA"].toInt();
    profile.delayB = (quint8)obj["delayB"].toInt();
    profile.delayC = (quint8)obj["delayC"].toInt();
    profile.delayD = (quint8)obj["delayD"].toInt();
    profile.timeValue = obj["timeValue"].toInt();
    profile.timeUnitIndex = obj["timeUnitIndex"].toInt();
    profile.createdTimestamp = (qint64)obj["createdTimestamp"].toInt();
    
    // Deserializar matriz de bits
    QString matrixStr = obj["matrixButtons"].toString();
    profile.matrixButtons.resize(32);
    for (int i = 0; i < qMin(32, matrixStr.size()); ++i) {
        profile.matrixButtons.setBit(i, matrixStr[i] == '1');
    }
    
    return profile;
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

