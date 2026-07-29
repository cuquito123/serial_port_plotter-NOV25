/***************************************************************************
**  This file is part of MPCC — Multi-Photon Coincidence Counter (CIOp)   **
**                                                                        **
**                                                                        **
**  MPCC is a program for plotting integer data from serial port using    **
**  Qt and QCustomPlot                                                    **
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
#include <QElapsedTimer>
#include <QTimer>
#include <QLabel>
#include "helpwindow.hpp"
#include "qcustomplot/qcustomplot.h"
#include <QVector>

// Máquina de estados operativa de la aplicación
enum class AppState {
    Disconnected,             // Desconectado: puerto cerrado, sin comunicación
    ReadyForConfiguration,    // Listo para configurar: puerto abierto, sin adquisición
    ReadyForExecution,        // Listo para ejecutar: config enviada al FPGA, sin adquisición aún
    Acquiring,                // Adquiriendo: toma de datos activa
    Paused,                   // Pausado: comunicación detenida durante adquisición
    Fault                     // Falla: error detectado, requiere recuperación
};

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
class SerialPortManager;
class SerialMessageParser;
class FpgaProtocol;
class PlotManager;
class CsvManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_comboPort_currentIndexChanged(const QString &arg1);                           // Muestra informacion del puerto seleccionado en la barra de estado
    void portOpenedSuccess();                                                             // Maneja la apertura correcta del puerto
    void portOpenedFail();                                                                // Maneja el fallo de apertura del puerto
    void onPortClosed();                                                                  // Maneja el cierre del puerto
    void replot();                                                                        // Solicita el repintado del grafico
    void onNewDataArrived(QStringList newData);                                           // Procesa nueva data parseada desde el puerto serie
    void on_spinAxesMin_valueChanged(int arg1);                                           // Actualiza limite inferior del eje Y
    void on_spinAxesMax_valueChanged(int arg1);                                           // Actualiza limite superior del eje Y
    void writeData(const QByteArray &data);                                               // Reenvia datos a traves del gestor serie
    //void on_comboAxes_currentIndexChanged(int index);                                     // Muestra cantidad de ejes y colores en la barra de estado
    void on_spinYStep_valueChanged(int arg1);                                             // Ajusta el paso de marcas del eje Y
    void on_savePNGButton_clicked();                                                      // Guarda una imagen del grafico
    void onMouseMoveInPlot (QMouseEvent *event);                                          // Actualiza coordenadas del mouse sobre el grafico
    void on_spinPoints_valueChanged (int arg1);                                           // Cambia la cantidad de puntos visibles en el grafico

    /* Seleccion de canal desde grafico o leyenda */
    void channel_selection (void);
    void legend_double_click (QCPLegend *legend, QCPAbstractLegendItem *item, QMouseEvent *event);

    void on_actionConnect_triggered();
    void on_actionDisconnect_triggered();
    void on_actionHow_to_use_triggered();
    void on_actionPause_Plot_triggered();
    void on_actionClear_triggered();
    void on_actionRecord_stream_triggered();
    void on_actionSalir_triggered();
    void on_actionPropiedades_de_Puerto_triggered();
    void on_actionExportar_datos_triggered();
    void on_actionManage_Profiles_triggered();
    void on_actionPropiedades_de_grabacion_triggered();
    void on_actionManual_de_Usuario_triggered();
    void on_actionAcerca_de_triggered();
    void on_actionAutoScale_en_Y_triggered();
    void on_actionMostar_todos_los_datos_toggled(bool checked);

//    void on_pushButton_TextEditHide_clicked();

    void on_pushButton_ShowallData_clicked();

    void on_pushButton_AutoScale_clicked();

    void on_pushButton_ResetVisible_clicked();

    void on_listWidget_Channels_itemDoubleClicked(QListWidgetItem *item);

    void on_pushButton_clicked();

//    void on_textEdit_UartWindow_textChanged();

    void on_actionEsconder_Caja_de_Texto_toggled(bool arg1);

    void on_ir_a_grafico_clicked();

//    void on_actionProperty_toggled(bool arg1);

//    void on_actionPropiedades_de_Puerto_changed();

    void on_actionconfig_triggered();

//    void on_actionGraphic_toggled(bool arg1);

//    void on_actionconfig_triggered();


    void on_EnviarDatos_clicked();

    void on_ResetearDatos_clicked();


    // Recalcula limites validos de TiempoNum al cambiar unidad y conserva escala equivalente.
    void actualizarMaximoDeTiempo(int index);
    // Actualizan en tiempo real los 5 parametros de configuracion del protocolo.
    void on_Ancho_de_pulso_valueChanged(int arg1);
    void on_Delay_A_valueChanged(int arg1);
    void on_Delay_B_valueChanged(int arg1);
    void on_Delay_C_valueChanged(int arg1);
    void on_Delay_D_valueChanged(int arg1);
    // Actualiza el texto y color del indicador de estado.
    void cambiarEstado(QString texto, QString color);
    // Limpia el estado interno de la matriz y los controles asociados.
    void limpiarMatrizInterna();
    
    // Gestión de máquina de estados operativa
    void setAppState(AppState newState);
    AppState currentAppState() const { return m_appState; }
    QString getStateDisplayName(AppState state) const;
    void updateUIForState();
    bool canTransitionToState(AppState newState) const;
    void onReplotProfileWindow(double averageMs, double maxMs, int samples);

signals:
    void portOpenFail();                                                                  // Emitida cuando no se puede abrir el puerto
    void portOpenOK();                                                                    // Emitida cuando el puerto queda abierto
    void portClosed();                                                                    // Emitida cuando el puerto se cierra
    void newData(QStringList data);                                                       // Emitida al recibir una trama parseada

private:
    // Profile UI actions
    void on_actionSave_Profile_triggered();
    void on_actionLoad_Profile_triggered();
    Ui::MainWindow *ui;

    /* Indicador visual de estado en la barra inferior */
    QLabel *statusLabel;

    /* Paleta de lineas y colores base de UI */
    QColor line_colors[CUSTOM_LINE_COLORS];
    QColor gui_colors[GCP_CUSTOM_LINE_COLORS];

    /* Estado principal de la aplicacion */
    bool connected;                                                                       // Estado de conexion serie
    bool enviar;
    bool Preparado;
    bool triggered;
    bool toggled;
    bool plotting;                                                                        // Estado de ploteo en tiempo real
    bool arg4 = 0;
    bool arg1 = 1;
    bool arg2 = 0;
   //int dim = 32;

    int dataPointNumber;                                                                  // Contador de puntos acumulados
    /* Cantidad de canales/graficos activos */
    int channels;

    /* Formato de datos de entrada */
    int data_format;   

    /* Configuracion de visualizacion de texto UART */
    bool filterDisplayedData = true;

    /* La caja de texto UART solo debe mostrar datos luego de presionar "Enviar Datos" */
    bool m_datosEnviados = false;

    /* Estado del listado de canales */
    QStringListModel *channelListModel;
    QStringList     channelStrList;

    //-- CSV file to save data

   // QAction *guardarCSVAction;  // Declaración del botón en la barra de menú

    QTimer updateTimer;                                                                   // Temporizador de refresco del grafico
    static constexpr int kPlotUpdateIntervalMs = 20;                                      // Intervalo nominal (50 FPS)
    static constexpr int kPlotUpdateFallbackIntervalMs = 33;                              // Mitigación (30 FPS)
    int m_plotUpdateIntervalMs = kPlotUpdateIntervalMs;
    bool m_replotThrottleApplied = false;
    QTime timeOfFirstData;                                                                // Marca temporal del primer dato recibido
    // Experimento: tiempo transcurrido con pausa/reanudar
    QElapsedTimer m_experimentTimer;                                                       // Temporizador de alta resolución
    qint64 m_experimentAccumulatedMs = 0;                                                   // Milisegundos acumulados antes de la pausa
    QTimer m_experimentUpdateTimer;                                                        // Timer para actualizar la UI con tiempo transcurrido
    bool m_experimentFinished = false;                                                     // Marca fin natural por duración alcanzada
    QLabel *experimentTimeLabel = nullptr;                                                 // Etiqueta mostrada en la statusBar
    QLabel *experimentCountdownLabel = nullptr;                                            // Etiqueta para countdown restante
    void startExperimentTimer();                                                           // Inicia y reinicia el temporizador
    void pauseExperimentTimer();                                                           // Pausa (acumula tiempo)
    void resumeExperimentTimer();                                                          // Reanuda sin resetear acumulado
    void resetExperimentTimer();                                                           // Resetea todo
    void updateExperimentTimeLabel();                                                      // Actualiza etiqueta con formato hh:mm:ss
    double timeBetweenSamples;                                                            // Intervalo estimado entre muestras
    QString receivedData;                                                                 // Buffer de texto para UART
    HelpWindow *helpWindow;
    void createUI();                                                                      // Inicializa y rellena controles de UI
    void setupPlot();                                                                     // Inicializa el area de grafico
    void buildMenus();                                                                    // Reconstruye la barra de menues con la estructura actual
    bool exportPlotData(const QString &filePath) const;                                    // Exporta el contenido actual del plot a CSV
    void setIncomingDataDisplayMode(bool showAll);                                         // Sincroniza el modo de visualizacion de datos crudos
    void setRecordingControlsState(bool recording);                                        // Sincroniza el estado visual del boton y la accion de grabacion
    qint64 selectedExperimentDurationMs() const;                                           // Convierte la duracion elegida en UI a milisegundos
                                                                                          // Abre el puerto serie interno con estos parametros
    void openPort(QSerialPortInfo portInfo, int baudRate, QSerialPort::DataBits dataBits, QSerialPort::Parity parity, QSerialPort::StopBits stopBits);
    Console *m_console = nullptr;

    // Vectores de botones para columna de grafico y matriz de datos.
    QVector<QPushButton*> botonesGraf;
    QVector<QPushButton*> botonesDatos;
    // Columna actualmente seleccionada para enviar al protocolo.
    int columnaSeleccionada;

    // Funciones auxiliares de logica de UI/protocolo.
    void actualizarEstadoGraf(int indiceBotonPresionado);
    void actualizarBotonDato(int bit, QPushButton* boton);
    QString activeMatrixButtonStyle() const;
    QString inactiveMatrixButtonStyle() const;
    void limpiarPlot();
    QStringList generarLabels();

    // Ultima unidad de tiempo usada para conversion entre indices.
    int indiceUnidadAnterior;

    void initActionsConnections();
    SerialPortManager *m_serialManager = nullptr;
    SerialMessageParser *m_messageParser = nullptr;
    FpgaProtocol *m_fpgaProtocol = nullptr;
    PlotManager *m_plotManager = nullptr;
    CsvManager *m_csvManager = nullptr;

    // Tracking de cambios pendientes: config local vs config aplicada al FPGA
    bool m_hasPendingChanges = false;
    FpgaProtocol *m_fpgaProtocolApplied = nullptr; // Snapshot de config aplicada al FPGA

    // Métodos para gestión de cambios
    void markPendingChanges();
    void clearPendingChanges();
    bool hasPendingChanges() const { return m_hasPendingChanges; }
    
    // Preflight check antes de ejecutar
    struct PreflightResult {
        bool success = false;
        QString errorMessage;
    };
    PreflightResult performPreflightCheck();
    void updatePendingChangesIndicator();

    // Estado operativo de la máquina de estados
    AppState m_appState = AppState::Disconnected;

    // ─── Telemetría de Salud en Tiempo Real ───────────────────────────────────────
    struct HealthMetrics {
        quint64 validPacketCount = 0;      // Paquetes parseados correctamente
        quint64 invalidPacketCount = 0;    // Paquetes con error de formato
        quint64 lostPacketCount = 0;       // Paquetes perdidos estimados
        qint64 lastPacketTimestampMs = 0;  // Timestamp (ms) del último paquete recibido
        float frameLatencyMs = 0.0f;        // Latencia estimada de trama en ms
        qint64 firstPacketTime = 0;        // Marca temporal del primer paquete para calcular estadísticas
    };
    
    HealthMetrics m_healthMetrics;
    void updateHealthMetrics(const QStringList &);
    void resetHealthMetrics();
    QString getHealthMetricsString() const;

    // ─── Recuperación Guiada ante Fallos ───────────────────────────────────────────
    void attemptRecovery();
    bool isRecoveryPossible() const;
    
    // Almacena parámetros actuales de conexión para recuperación
    struct ConnectionParams {
        QSerialPortInfo portInfo;
        int baudRate = 115200;
        QSerialPort::DataBits dataBits = QSerialPort::Data8;
        QSerialPort::Parity parity = QSerialPort::NoParity;
        QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    };
    ConnectionParams m_lastConnectionParams;

    // ─── Trazabilidad Operativa (Event Logging) ───────────────────────────────────
    enum class EventType {
        Started,        // Adquisición iniciada
        Stopped,        // Adquisición detenida
        ConfigApplied,  // Configuración aplicada al FPGA
        Error,          // Error ocurrido
        Reset,          // Reset ejecutado
        Recovery,       // Intento de recuperación
        PortOpened,     // Puerto abierto
        PortClosed      // Puerto cerrado
    };

    struct OperativeEvent {
        qint64 timestampMs;
        EventType eventType;
        QString description;
        
        QString toString() const {
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestampMs);
            QString typeStr;
            switch (eventType) {
                case EventType::Started: typeStr = "STARTED"; break;
                case EventType::Stopped: typeStr = "STOPPED"; break;
                case EventType::ConfigApplied: typeStr = "CONFIG_APPLIED"; break;
                case EventType::Error: typeStr = "ERROR"; break;
                case EventType::Reset: typeStr = "RESET"; break;
                case EventType::Recovery: typeStr = "RECOVERY"; break;
                case EventType::PortOpened: typeStr = "PORT_OPENED"; break;
                case EventType::PortClosed: typeStr = "PORT_CLOSED"; break;
            }
            return QString("[%1] %2: %3").arg(dt.toString("HH:mm:ss.zzz"), typeStr, description);
        }
    };
    
    QList<OperativeEvent> m_eventLog;
    static constexpr int MAX_LOG_ENTRIES = 1000;
    
    void logEvent(EventType type, const QString &description);
    QString getEventLogAsString(int maxEntries = 50) const;
    void clearEventLog();
    void exportEventLog(const QString &filePath);

};

#endif                                                                                    // MAINWINDOW_HPP
