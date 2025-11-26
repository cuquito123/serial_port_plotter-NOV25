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

#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QtSerialPort/QtSerialPort>
#include <QSerialPortInfo>
#include "helpwindow.hpp"
#include "qcustomplot/qcustomplot.h"
#include <QBitArray>
#include <QVector>

#define START_MSG       '$'
#define END_MSG         ';'

#define WAIT_START      1
#define IN_MESSAGE      2
#define UNDEFINED       3

#define CUSTOM_LINE_COLORS   14
#define GCP_CUSTOM_LINE_COLORS 4

namespace Ui {
    class MainWindow;
}

class Console;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_comboPort_currentIndexChanged(const QString &arg1);                           // Slot displays message on status bar
    void portOpenedSuccess();                                                             // Called when port opens OK
    void portOpenedFail();                                                                // Called when port fails to open
    void onPortClosed();                                                                  // Called when closing the port
    void replot();                                                                        // Slot for repainting the plot
    void onNewDataArrived(QStringList newData);                                           // Slot for new data from serial port
    void saveStream(QStringList newData);                                                 // Save the received data to the opened file
    void on_spinAxesMin_valueChanged(int arg1);                                           // Changing lower limit for the plot
    void on_spinAxesMax_valueChanged(int arg1);                                           // Changing upper limit for the plot
    void readData();                                                                      // Slot for inside serial port
    void writeData(const QByteArray &data);                                               // Slot for outside serial port
    //void on_comboAxes_currentIndexChanged(int index);                                     // Display number of axes and colors in status bar
    void on_spinYStep_valueChanged(int arg1);                                             // Spin box for changing Y axis tick step
    void on_savePNGButton_clicked();                                                      // Button for saving JPG
    void onMouseMoveInPlot (QMouseEvent *event);                                          // Displays coordinates of mouse pointer when clicked in plot in status bar
    void on_spinPoints_valueChanged (int arg1);                                           // Spin box controls how many data points are collected and displayed
    void on_mouse_wheel_in_plot (QWheelEvent *event);                                     // Makes wheel mouse works while plotting

    /* Used when a channel is selected (plot or legend) */
    void channel_selection (void);
    void legend_double_click (QCPLegend *legend, QCPAbstractLegendItem *item, QMouseEvent *event);

    void on_actionConnect_triggered();
    void on_actionDisconnect_triggered();
    void on_actionHow_to_use_triggered();
    void on_actionPause_Plot_triggered();
    void on_actionClear_triggered();
    void on_actionRecord_stream_triggered();

   // void openCsvFile();  // Función que se ejecuta al hacer clic en el botón


//    void on_pushButton_TextEditHide_clicked();

    void on_pushButton_ShowallData_clicked();

    void on_pushButton_AutoScale_clicked();

    void on_pushButton_ResetVisible_clicked();

    void on_listWidget_Channels_itemDoubleClicked(QListWidgetItem *item);

    void on_pushButton_clicked();

//    void on_textEdit_UartWindow_textChanged();

    void on_actionEsconder_Caja_de_Texto_toggled(bool arg1);

    void on_ir_a_configuracion_clicked();

    void on_ir_a_grafico_clicked();

//    void on_actionProperty_toggled(bool arg1);

//    void on_actionPropiedades_de_Puerto_changed();

    void on_actionconfig_toggled(bool arg1);

//    void on_actionGraphic_toggled(bool arg1);

//    void on_actionconfig_triggered();

    void on_A1_0_clicked();

    void on_B1_1_clicked();

    void on_C1_2_clicked();

    void on_D1_3_clicked();

    void on_A2_4_clicked();

    void on_B2_5_clicked();

    void on_C2_6_clicked();

    void on_D2_7_clicked();

    void on_A3_8_clicked();

    void on_B3_9_clicked();

    void on_C3_10_clicked();

    void on_D3_11_clicked();

    void on_A4_12_clicked();

    void on_B4_13_clicked();

    void on_C4_14_clicked();

    void on_D4_15_clicked();

    void on_A5_16_clicked();

    void on_B5_17_clicked();

    void on_C5_18_clicked();

    void on_D5_19_clicked();

    void on_A6_20_clicked();

    void on_B6_21_clicked();

    void on_C6_22_clicked();

    void on_D6_23_clicked();

    void on_A7_24_clicked();

    void on_B7_25_clicked();

    void on_C7_26_clicked();

    void on_D7_27_clicked();

    void on_A8_28_clicked();

    void on_B8_29_clicked();

    void on_C8_30_clicked();

    void on_D8_31_clicked();

    void on_EnviarDatos_clicked();

    void on_ResetearDatos_clicked();

    void on_Ancho_de_pulso_valueChanged(int arg3);

    void on_Delay_A_valueChanged(int arg3);

    void on_Delay_B_valueChanged(int arg3);

    void on_Delay_C_valueChanged(int arg3);

    void on_Delay_D_valueChanged(int arg3);

    void on_plot_customContextMenuRequested(const QPoint &pos);

    void on_textEdit_UartWindow_customContextMenuRequested(const QPoint &pos);

    void on_textEdit_UartWindow_copyAvailable(bool b);

    void on_Delay_A_textChanged(const QString &arg1);

signals:
    void portOpenFail();                                                                  // Emitted when cannot open port
    void portOpenOK();                                                                    // Emitted when port is open
    void portClosed();                                                                    // Emitted when port is closed
    void newData(QStringList data);                                                       // Emitted when new data has arrived

private:
    Ui::MainWindow *ui;

    /* Line colors */
    QColor line_colors[CUSTOM_LINE_COLORS];
    QColor gui_colors[GCP_CUSTOM_LINE_COLORS];

    /* Main info */
    bool connected;                                                                       // Status connection variable
    bool enviar;
    bool Preparado;
    bool triggered;
    bool toggled;
    bool plotting;                                                                        // Status plotting variable
    bool arg4 = 0;
    bool arg1 = 1;
    bool arg2 = 0;
   //int dim = 32;

    int dataPointNumber;                                                                  // Keep track of data points
    /* Channels of data (number of graphs) */
    int channels;

    /* Data format */
    int data_format;   

    /* Textbox Related */
    bool filterDisplayedData = true;

    /* Listview Related */
    QStringListModel *channelListModel;
    QStringList     channelStrList;

    //-- CSV file to save data

   // QAction *guardarCSVAction;  // Declaración del botón en la barra de menú

    QFile* m_csvFile = nullptr;
    void openCsvFile(void);
    void closeCsvFile(void);

    QTimer updateTimer;                                                                   // Timer used for replotting the plot
    QTime timeOfFirstData;                                                                // Record the time of the first data point
    double timeBetweenSamples;                                                            // Store time between samples
    QSerialPort *serialPort;                                                              // Serial port; runs in this thread
    QString receivedData;                                                                 // Used for reading from the port
    int STATE;                                                                            // State of recieiving message from port
    int NUMBER_OF_POINTS;                                                                 // Number of points plotted
    HelpWindow *helpWindow;
    void createUI();                                                                      // Populate the controls
    void enable_com_controls (bool enable);                                               // Enable/disable controls
    void setupPlot();                                                                     // Setup the QCustomPlot
                                                                                          // Open the inside serial port with these parameters
    void openPort(QSerialPortInfo portInfo, int baudRate, QSerialPort::DataBits dataBits, QSerialPort::Parity parity, QSerialPort::StopBits stopBits);
    Console *m_console = nullptr;

//    QByteArray arreglo_f1;
    QByteArray arreglo_1 = QByteArray(38, false);
    QByteArray arreglo_2 = QByteArray(1, false);
    QByteArray arreglo_3 = QByteArray(1, false);
    //QBitArray tecla = QBitArray(32, true);
//    QByteArray señal_envio = QByteArray(1, false);                                          // Signal de comienzo de envio de datos
    QBitArray *tecla;
     //char  *temp=nullptr;
    void initActionsConnections();
    QSerialPort *m_serial = nullptr;

};

#endif                                                                                    // MAINWINDOW_HPP
