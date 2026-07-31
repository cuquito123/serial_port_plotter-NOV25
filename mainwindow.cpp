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

#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include "serialportmanager.hpp"
#include "serialmessageparser.hpp"
#include "fpgaprotocol.hpp"
#include "plotmanager.hpp"
#include "csvmanager.hpp"
#include "profilemanager.hpp"
#include <x86intrin.h>
#include <QWidget>
#include <QLabel>
#include <QDebug>

#include "console.h"
#include "QMessageBox"

namespace {
constexpr double kInvalidPacketWarningRatio = 0.10;
constexpr double kInvalidPacketFaultRatio = 0.25;
constexpr qint64 kInvalidPacketSustainMs = 5000;

struct CommunicationDegradationState {
    qint64 warningSinceMs = 0;
    qint64 faultSinceMs = 0;
    bool warningLogged = false;
    bool faultLogged = false;
};

CommunicationDegradationState g_commDegradationState;
QLabel *g_commHealthLabel = nullptr;

bool isInvalidParsedPacket(const QStringList &data, const QString &rawMessage)
{
    if (rawMessage.trimmed().isEmpty() || data.isEmpty()) {
        return true;
    }

    for (const QString &field : data) {
        if (field.trimmed().isEmpty()) {
            return true;
        }

        bool ok = false;
        field.toDouble(&ok);
        if (!ok) {
            return true;
        }
    }

    return false;
}

void setCommunicationHealthLabel(const QString &text, const QString &styleSheet, bool visible)
{
    if (!g_commHealthLabel) {
        return;
    }

    g_commHealthLabel->setText(text);
    g_commHealthLabel->setStyleSheet(styleSheet);
    g_commHealthLabel->setVisible(visible);
}
}
#include <QJsonObject>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QUrl>
#include <QTextStream>
#include <QSignalBlocker>
#include <QThread>
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QInputDialog>

namespace {
QString formatDurationHms(qint64 totalMs)
{
    if (totalMs < 0) {
        totalMs = 0;
    }

    qint64 seconds = totalMs / 1000;
    const int hrs = int(seconds / 3600);
    const int mins = int((seconds % 3600) / 60);
    const int secs = int(seconds % 60);
    return QString("%1:%2:%3").arg(hrs, 2, 10, QChar('0')).arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
}
}

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
  m_plotManager(nullptr),
  m_fpgaProtocolApplied(new FpgaProtocol())

{
  initActionsConnections();

  connect(m_console, &Console::getData, this, &MainWindow::writeData);
  connect(m_serialManager, &SerialPortManager::rawDataReady, m_messageParser, &SerialMessageParser::appendData);
  connect(m_serialManager, &SerialPortManager::rawDataReady, this, [this](const QByteArray &raw) {
      if (m_datosEnviados && !filterDisplayedData) {
          ui->textEdit_UartWindow->append(QString::fromLatin1(raw));
      }
  });
  connect(m_messageParser, &SerialMessageParser::messageParsed, this, [this](const QStringList &data, const QString &rawMessage) {
      if (m_datosEnviados && filterDisplayedData) {
          ui->textEdit_UartWindow->append(rawMessage);
      }

      if (isInvalidParsedPacket(data, rawMessage)) {
          m_healthMetrics.invalidPacketCount++;
      }

      emit newData(data);
  });
  connect(this, SIGNAL(newData(QStringList)), this, SLOT(onNewDataArrived(QStringList)));
  connect(this, &MainWindow::newData, this, [this](const QStringList &data) {
      // Only write to CSV if recording is enabled and the app is not paused
      if (m_csvManager && ui->actionRecord_stream->isChecked() && m_appState != AppState::Paused) {
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

    {
        QSizePolicy sp = ui->textEdit_UartWindow->sizePolicy();
        sp.setRetainSizeWhenHidden(true);
        ui->textEdit_UartWindow->setSizePolicy(sp);
    }

    buildMenus();

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
            g_commHealthLabel = new QLabel(this);
            g_commHealthLabel->setMinimumWidth(260);
            g_commHealthLabel->setVisible(false);
            ui->statusBar->addPermanentWidget(g_commHealthLabel);
    // Experiment time label
    experimentTimeLabel = new QLabel(this);
    experimentTimeLabel->setText("Exp: 00:00:00");
    experimentTimeLabel->setMinimumWidth(100);
    ui->statusBar->addPermanentWidget(experimentTimeLabel);
    // Experiment countdown label
    experimentCountdownLabel = new QLabel(this);
    experimentCountdownLabel->setText("");
    experimentCountdownLabel->setMinimumWidth(140);
    ui->statusBar->addPermanentWidget(experimentCountdownLabel);
    // Configure experiment update timer
    m_experimentUpdateTimer.setParent(this);
    m_experimentUpdateTimer.setInterval(500);
    connect(&m_experimentUpdateTimer, &QTimer::timeout, this, &MainWindow::updateExperimentTimeLabel);
    // Sin esto, cambiar la duración mientras no se está adquiriendo (ej. antes de
    // arrancar, con la grabación armada) no se reflejaba en "Restante" hasta el
    // próximo evento que la tocara a mano (armar grabación, iniciar, etc.).
    connect(ui->ExperimentDurationNum, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::updateExperimentTimeLabel);
    connect(ui->ExperimentDurationUnit, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateExperimentTimeLabel);
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

      // setAppState() no hace nada si el estado nuevo es igual al actual, y
      // m_appState ya arranca en Disconnected: sin esto, los controles quedaban
      // con el "enabled" por defecto del diseñador (ej. EnviarDatos quedaba
      // clickeable sin haber conectado el puerto).
      updateUIForState();
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

// ---------------- Experiment timer implementation -----------------------

void MainWindow::startExperimentTimer()
{
    m_experimentAccumulatedMs = 0;
    m_experimentTimer.start();
    m_experimentUpdateTimer.start();
    updateExperimentTimeLabel();
}

void MainWindow::on_actionSave_Profile_triggered()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, "Guardar Perfil", "Nombre del perfil:", QLineEdit::Normal, "", &ok);
    if (ok && !name.isEmpty()) {
        const QString profileName = QFileInfo(name).completeBaseName().trimmed();
        if (profileName.isEmpty()) {
            ui->statusBar->showMessage("Nombre de perfil inválido.");
            return;
        }

        const QString path = ProfileManager::profilePath(profileName);
        if (QFileInfo::exists(path)) {
            const QMessageBox::StandardButton answer = QMessageBox::question(
                this,
                "Sobrescribir perfil",
                QString("El perfil '%1' ya existe. ¿Deseas sobrescribirlo?").arg(profileName),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );
            if (answer != QMessageBox::Yes) {
                return;
            }
        }

        ProfileManager::OperativeProfile profile;
        profile.name = profileName;
        profile.matrixButtons = m_fpgaProtocol ? m_fpgaProtocol->getTecla() : QBitArray(32);
        profile.pulseWidth = m_fpgaProtocol ? m_fpgaProtocol->getPulseWidth() : static_cast<quint8>(ui->Ancho_de_pulso->value());
        profile.delayA = m_fpgaProtocol ? m_fpgaProtocol->getDelayA() : static_cast<quint8>(ui->Delay_A->value());
        profile.delayB = m_fpgaProtocol ? m_fpgaProtocol->getDelayB() : static_cast<quint8>(ui->Delay_B->value());
        profile.delayC = m_fpgaProtocol ? m_fpgaProtocol->getDelayC() : static_cast<quint8>(ui->Delay_C->value());
        profile.delayD = m_fpgaProtocol ? m_fpgaProtocol->getDelayD() : static_cast<quint8>(ui->Delay_D->value());
        profile.timeValue = ui->TiempoNum->value();
        profile.timeUnitIndex = ui->TiempoBox->currentIndex();
        profile.createdTimestamp = QDateTime::currentMSecsSinceEpoch();

        QString errorMessage;
        if (!ProfileManager::saveProfile(profileName, profile, &errorMessage)) {
            ui->statusBar->showMessage(errorMessage);
            return;
        }
        ui->statusBar->showMessage("Perfil guardado: " + profileName);
    }
}

void MainWindow::on_actionLoad_Profile_triggered()
{
    const QString file = QFileDialog::getOpenFileName(this, "Cargar Perfil", ProfileManager::profilesDirectory(), "JSON Files (*.json);;All Files (*)");
    if (!file.isEmpty()) {
        ProfileManager::OperativeProfile profile;
        QString errorMessage;
        if (!ProfileManager::loadProfile(file, &profile, &errorMessage)) {
            ui->statusBar->showMessage(errorMessage);
            return;
        }

        ProfileManager::UiContext context;
        context.timeValueSpin = ui->TiempoNum;
        context.timeUnitCombo = ui->TiempoBox;
        context.pulseWidthSpin = ui->Ancho_de_pulso;
        context.delayASpin = ui->Delay_A;
        context.delayBSpin = ui->Delay_B;
        context.delayCSpin = ui->Delay_C;
        context.delayDSpin = ui->Delay_D;
        context.matrixButtons = botonesDatos;
        context.fpgaProtocol = m_fpgaProtocol;
        context.markPendingChanges = [this]() { markPendingChanges(); };

        ProfileManager::applyProfileToUi(profile, context);
        ui->statusBar->showMessage("Perfil cargado: " + profile.name);
    }
}

void MainWindow::on_actionManage_Profiles_triggered()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Gestionar perfiles");

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *previewLabel = new QLabel("Selecciona un perfil para ver sus detalles.", &dialog);
    previewLabel->setWordWrap(true);
    layout->addWidget(previewLabel);

    QListWidget *listWidget = new QListWidget(&dialog);
    layout->addWidget(listWidget);

    for (const QString &profileName : ProfileManager::profileNames()) {
        listWidget->addItem(profileName);
    }

    auto updatePreview = [previewLabel](const QString &profileName) {
        if (profileName.isEmpty()) {
            previewLabel->setText("Selecciona un perfil para ver sus detalles.");
            return;
        }

        ProfileManager::OperativeProfile profile;
        QString errorMessage;
        if (!ProfileManager::loadProfile(ProfileManager::profilePath(profileName), &profile, &errorMessage)) {
            previewLabel->setText(errorMessage);
            return;
        }

        int activeBits = 0;
        for (int i = 0; i < profile.matrixButtons.size(); ++i) {
            if (profile.matrixButtons.testBit(i)) {
                ++activeBits;
            }
        }

        previewLabel->setText(
            QString("Perfil: %1\nBits activos: %2\nPulso: %3\nDelay A/B/C/D: %4 / %5 / %6 / %7\nTiempo: %8 (unidad %9)\nCreado: %10")
                .arg(profile.name)
                .arg(activeBits)
                .arg(profile.pulseWidth)
                .arg(profile.delayA)
                .arg(profile.delayB)
                .arg(profile.delayC)
                .arg(profile.delayD)
                .arg(profile.timeValue)
                .arg(profile.timeUnitIndex)
                .arg(QDateTime::fromMSecsSinceEpoch(profile.createdTimestamp).toString("yyyy-MM-dd HH:mm:ss"))
        );
    };

    connect(listWidget, &QListWidget::currentTextChanged, &dialog, updatePreview);
    if (listWidget->count() > 0) {
        listWidget->setCurrentRow(0);
    }

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *loadButton = new QPushButton("Cargar", &dialog);
    QPushButton *deleteButton = new QPushButton("Eliminar", &dialog);
    QPushButton *renameButton = new QPushButton("Renombrar", &dialog);
    QPushButton *closeButton = new QPushButton("Cerrar", &dialog);
    buttonLayout->addWidget(loadButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addWidget(renameButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);

    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(loadButton, &QPushButton::clicked, &dialog, [this, listWidget, &dialog]() {
        QListWidgetItem *item = listWidget->currentItem();
        if (!item) {
            return;
        }
        ProfileManager::OperativeProfile profile;
        QString errorMessage;
        const QString filePath = ProfileManager::profilePath(item->text());
        if (!ProfileManager::loadProfile(filePath, &profile, &errorMessage)) {
            QMessageBox::warning(this, "Gestionar perfiles", errorMessage);
            return;
        }

        ProfileManager::UiContext context;
        context.timeValueSpin = ui->TiempoNum;
        context.timeUnitCombo = ui->TiempoBox;
        context.pulseWidthSpin = ui->Ancho_de_pulso;
        context.delayASpin = ui->Delay_A;
        context.delayBSpin = ui->Delay_B;
        context.delayCSpin = ui->Delay_C;
        context.delayDSpin = ui->Delay_D;
        context.matrixButtons = botonesDatos;
        context.fpgaProtocol = m_fpgaProtocol;
        context.markPendingChanges = [this]() { markPendingChanges(); };

        ProfileManager::applyProfileToUi(profile, context);
        ui->statusBar->showMessage("Perfil cargado: " + profile.name);
        dialog.accept();
    });
    connect(deleteButton, &QPushButton::clicked, &dialog, [this, listWidget]() {
        QListWidgetItem *item = listWidget->currentItem();
        if (!item) {
            return;
        }

        const QString profileName = item->text();
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            "Eliminar perfil",
            QString("¿Eliminar el perfil '%1'?").arg(profileName),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (answer != QMessageBox::Yes) {
            return;
        }

        QString errorMessage;
        if (!ProfileManager::deleteProfile(profileName, &errorMessage)) {
            QMessageBox::warning(this, "Gestionar perfiles", errorMessage);
            return;
        }

        delete listWidget->takeItem(listWidget->row(item));
        ui->statusBar->showMessage("Perfil eliminado: " + profileName);
    });

    connect(renameButton, &QPushButton::clicked, &dialog, [this, listWidget, updatePreview]() {
        QListWidgetItem *item = listWidget->currentItem();
        if (!item) {
            return;
        }

        const QString oldName = item->text();
        bool ok = false;
        const QString newName = QInputDialog::getText(
            this,
            "Renombrar perfil",
            "Nuevo nombre del perfil:",
            QLineEdit::Normal,
            oldName,
            &ok
        ).trimmed();
        if (!ok || newName.isEmpty()) {
            return;
        }

        QString errorMessage;
        if (!ProfileManager::renameProfile(oldName, newName, &errorMessage)) {
            QMessageBox::warning(this, "Gestionar perfiles", errorMessage);
            return;
        }

        item->setText(newName);
        ui->statusBar->showMessage("Perfil renombrado: " + newName);
        updatePreview(newName);
    });

    dialog.exec();
}
void MainWindow::pauseExperimentTimer()
{
    if (m_experimentTimer.isValid()) {
        m_experimentAccumulatedMs += m_experimentTimer.elapsed();
    }
    m_experimentTimer.invalidate();
    m_experimentUpdateTimer.stop();
    updateExperimentTimeLabel();
}

void MainWindow::resumeExperimentTimer()
{
    // If already running, nothing to do
    if (m_experimentTimer.isValid()) {
        return;
    }
    m_experimentTimer.start();
    m_experimentUpdateTimer.start();
    updateExperimentTimeLabel();
}

void MainWindow::resetExperimentTimer()
{
    m_experimentUpdateTimer.stop();
    m_experimentAccumulatedMs = 0;
    m_experimentTimer.invalidate();
    if (experimentTimeLabel) {
        experimentTimeLabel->setText("Exp: 00:00:00");
    }
    // Recalcula "Restante" con el estado y la duración configurada actuales:
    // sin esto, quedaba mostrando el valor de la corrida anterior (ej. "Finalizado: ...")
    // hasta el próximo tick del timer, que solo corre mientras se está adquiriendo.
    updateExperimentTimeLabel();
}

void MainWindow::updateExperimentTimeLabel()
{
    qint64 totalMs = m_experimentAccumulatedMs;
    if (m_experimentTimer.isValid()) {
        totalMs += m_experimentTimer.elapsed();
    }

    qint64 seconds = totalMs / 1000;
    int hrs = int(seconds / 3600);
    int mins = int((seconds % 3600) / 60);
    int secs = int(seconds % 60);

    QString text = QString("Exp: %1:%2:%3").arg(hrs,2,10,QChar('0')).arg(mins,2,10,QChar('0')).arg(secs,2,10,QChar('0'));
    if (experimentTimeLabel) experimentTimeLabel->setText(text);

    // Actualizar countdown de duración restante
    if (experimentCountdownLabel && ui && ui->ExperimentDurationNum && ui->ExperimentDurationUnit) {
        qint64 desiredMs = selectedExperimentDurationMs();
        if (desiredMs > 0 && (m_appState == AppState::Acquiring || (m_appState == AppState::Paused && ui->actionRecord_stream->isChecked()))) {
            const qint64 remainingMs = qMax<qint64>(0, desiredMs - totalMs);
            experimentCountdownLabel->setText(QString("Restante: %1").arg(formatDurationHms(remainingMs)));
        } else if (desiredMs > 0 && ui->actionRecord_stream->isChecked() && m_appState != AppState::Acquiring && m_appState != AppState::Paused) {
            experimentCountdownLabel->setText(QString("Restante: %1").arg(formatDurationHms(desiredMs)));
        } else if (m_appState != AppState::Acquiring && m_appState != AppState::Paused) {
            experimentCountdownLabel->setText("");
        }
    }

    // If the user configured a duration, and we've reached it, auto-pause acquisition
    if (ui && ui->ExperimentDurationNum && ui->ExperimentDurationUnit) {
        qint64 desiredMs = 0;
        int val = ui->ExperimentDurationNum->value();
        int unitIdx = ui->ExperimentDurationUnit->currentIndex();
        switch (unitIdx) {
            case 0: desiredMs = qint64(val) * 1000; break; // segundos
            case 1: desiredMs = qint64(val) * 60 * 1000; break; // minutos
            case 2: desiredMs = qint64(val) * 3600 * 1000; break; // horas
            default: desiredMs = qint64(val) * 1000; break;
        }

        if (desiredMs > 0 && totalMs >= desiredMs) {
            if (m_appState == AppState::Acquiring) {
                const QString finishedMsg = QString("Experimento finalizado: duración máxima alcanzada (%1)")
                    .arg(formatDurationHms(desiredMs));
                logEvent(EventType::Stopped, finishedMsg);
                // Pause acquisition
                updateTimer.stop();
                plotting = false;
                // El estado debe cambiar ANTES de llamar a pauseExperimentTimer():
                // esa función dispara internamente otro updateExperimentTimeLabel(),
                // y si m_appState siguiera en Acquiring, este mismo bloque se
                // volvería a ejecutar dentro de esa llamada anidada, entrando en
                // una recursión infinita (stack overflow / crash) cada vez que
                // totalMs siguiera siendo >= desiredMs.
                setAppState(AppState::Paused);
                pauseExperimentTimer();
                // Mensaje contextual en statusBar para informar finalización y CSV
                // (no usar cambiarEstado aquí; la máquina de estados decidirá el texto)
                ui->statusBar->showMessage(finishedMsg);
                // Stop recording and close CSV cleanly
                const bool csvWasOpen = (m_csvManager && m_csvManager->isOpen());
                if (csvWasOpen) {
                    m_csvManager->closeCsvFile();
                    const QSignalBlocker blockRecordAction(ui->actionRecord_stream);
                    ui->actionRecord_stream->setChecked(false);
                }
                setRecordingControlsState(false);
                experimentCountdownLabel->setText(QString("Finalizado: %1").arg(formatDurationHms(desiredMs)));
                m_experimentFinished = true;
                if (csvWasOpen) {
                    ui->statusBar->showMessage("Duración alcanzada: experimento finalizado y CSV guardado automáticamente");
                } else {
                    ui->statusBar->showMessage("Duración alcanzada: experimento finalizado");
                }
                // Transicionar estado; updateUIForState mostrará el mensaje correcto
                setAppState(AppState::Paused);
            }
        }
    }
}


/**
 * @brief Destructor.
 */

MainWindow::~MainWindow()
{
    if (m_csvManager) {
        m_csvManager->closeCsvFile();
    }
    delete m_fpgaProtocolApplied;
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
        ui->statusBar->showMessage ("No ports detected.");
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

void MainWindow::buildMenus()
{
    if (!ui || !ui->menuBar) {
        return;
    }

    ui->actionConnect->setText("Conectar");
    ui->actionConnect->setShortcut(QKeySequence(Qt::Key_MediaPlay));
    ui->actionDisconnect->setText("Desconectar");
    ui->actionDisconnect->setShortcut(QKeySequence(Qt::Key_MediaStop));
    ui->actionPause_Plot->setText("Pausa/Reanuda");
    ui->actionPause_Plot->setShortcut(QKeySequence(Qt::Key_MediaPause));
    ui->actionClear->setText("Limpiar Gráfico");
    ui->actionHow_to_use->setText("Cómo usar");
    ui->actionHow_to_use->setShortcut(QKeySequence::HelpContents);
    ui->actionRecord_stream->setText("Grabar Stream (CSV)");
    ui->actionRecord_stream->setShortcut(QKeySequence::Save);
    ui->actionEsconder_Caja_de_Texto->setChecked(true);
    ui->actionEsconder_Caja_de_Texto->setText("Esconder Caja de Texto");
    ui->actionMostar_todos_los_datos->setText("Mostrar Todos los Datos");
    ui->actionPropiedades_de_Puerto->setText("Propiedades de Puerto...");
    ui->actionconfig->setText("Panel de configuración");
    ui->actionconfig->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Tab));

    ui->toolBar->clear();
    ui->toolBar->addAction(ui->actionConnect);
    ui->toolBar->addAction(ui->actionPause_Plot);
    ui->toolBar->addAction(ui->actionDisconnect);
    ui->toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    ui->toolBar_2->setVisible(false);

    auto *actionSalir = new QAction("Salir", this);
    actionSalir->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    connect(actionSalir, &QAction::triggered, this, &MainWindow::on_actionSalir_triggered);

    auto *actionAutoScaleY = new QAction("AutoScale en Y", this);
    connect(actionAutoScaleY, &QAction::triggered, this, &MainWindow::on_actionAutoScale_en_Y_triggered);

    auto *actionExportData = new QAction("Exportar datos...", this);
    connect(actionExportData, &QAction::triggered, this, &MainWindow::on_actionExportar_datos_triggered);

    auto *actionRecordingProps = new QAction("Propiedades de grabación...", this);
    connect(actionRecordingProps, &QAction::triggered, this, &MainWindow::on_actionPropiedades_de_grabacion_triggered);

    auto *actionManual = new QAction("Manual de Usuario", this);
    connect(actionManual, &QAction::triggered, this, &MainWindow::on_actionManual_de_Usuario_triggered);

    auto *actionAbout = new QAction("Acerca de...", this);
    connect(actionAbout, &QAction::triggered, this, &MainWindow::on_actionAcerca_de_triggered);

    // No conectar ui->actionPropiedades_de_Puerto ni ui->actionMostar_todos_los_datos
    // manualmente acá: setupUi() ya los conecta automáticamente a
    // on_actionPropiedades_de_Puerto_triggered() y on_actionMostar_todos_los_datos_toggled()
    // por convención de nombres (QMetaObject::connectSlotsByName). Conectarlos de nuevo
    // duplicaba la señal: "Propiedades de Puerto..." abría el diálogo dos veces por click.

    ui->menuBar->clear();

    QMenu *menuPuertoSerial = ui->menuBar->addMenu("Puerto Serial");
    menuPuertoSerial->addAction(ui->actionPropiedades_de_Puerto);
    // Profile actions
    auto *actionSaveProfile = new QAction("Guardar Perfil...", this);
    connect(actionSaveProfile, &QAction::triggered, this, &MainWindow::on_actionSave_Profile_triggered);
    auto *actionLoadProfile = new QAction("Cargar Perfil...", this);
    connect(actionLoadProfile, &QAction::triggered, this, &MainWindow::on_actionLoad_Profile_triggered);
    auto *actionManageProfiles = new QAction("Gestionar Perfiles...", this);
    connect(actionManageProfiles, &QAction::triggered, this, &MainWindow::on_actionManage_Profiles_triggered);
    menuPuertoSerial->addSeparator();
    menuPuertoSerial->addAction(actionSaveProfile);
    menuPuertoSerial->addAction(actionLoadProfile);
    menuPuertoSerial->addAction(actionManageProfiles);
    menuPuertoSerial->addSeparator();
    menuPuertoSerial->addAction(actionSalir);

    QMenu *menuVisualizacion = ui->menuBar->addMenu("Visualización");
    menuVisualizacion->addAction(ui->actionconfig);
    menuVisualizacion->addSeparator();
    menuVisualizacion->addAction(ui->actionEsconder_Caja_de_Texto);
    menuVisualizacion->addAction(ui->actionMostar_todos_los_datos);
    menuVisualizacion->addSeparator();
    menuVisualizacion->addAction(ui->actionConnect);
    menuVisualizacion->addAction(ui->actionPause_Plot);
    menuVisualizacion->addAction(ui->actionDisconnect);
    QMenu *menuControlesGrafico = menuVisualizacion->addMenu("Controles del Gráfico");
    menuControlesGrafico->addAction(actionAutoScaleY);
    menuControlesGrafico->addAction(ui->actionClear);

    QMenu *menuGrabacion = ui->menuBar->addMenu("Grabación & Exportación");
    menuGrabacion->addAction(ui->actionRecord_stream);
    menuGrabacion->addAction(actionExportData);
    menuGrabacion->addAction(actionRecordingProps);

    QMenu *menuAyuda = ui->menuBar->addMenu("Ayuda");
    menuAyuda->addAction(ui->actionHow_to_use);
    menuAyuda->addAction(actionManual);
    menuAyuda->addAction(actionAbout);

    setIncomingDataDisplayMode(false);
    setRecordingControlsState(false);
}

bool MainWindow::exportPlotData(const QString &filePath) const
{
    if (!ui || !ui->plot) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream.setGenerateByteOrderMark(true);

    const int graphCount = ui->plot->graphCount();
    stream << "Tiempo (s)";
    for (int i = 0; i < graphCount; ++i) {
        QString graphName = ui->plot->graph(i)->name();
        if (graphName.isEmpty()) {
            graphName = QString("Serie %1").arg(i + 1);
        }
        stream << ";" << graphName;
    }
    stream << "\n";

    if (graphCount == 0) {
        return true;
    }

    QVector<QCPGraphDataContainer::const_iterator> iterators;
    QVector<QCPGraphDataContainer::const_iterator> ends;
    iterators.reserve(graphCount);
    ends.reserve(graphCount);

    for (int i = 0; i < graphCount; ++i) {
        QSharedPointer<QCPGraphDataContainer> data = ui->plot->graph(i)->data();
        iterators << data->constBegin();
        ends << data->constEnd();
    }

    while (iterators[0] != ends[0]) {
        stream << QString::number(iterators[0]->key, 'f', 6);
        for (int i = 0; i < graphCount; ++i) {
            if (iterators[i] != ends[i]) {
                stream << ";" << QString::number(iterators[i]->value, 'f', 6);
                ++iterators[i];
            } else {
                stream << ";";
            }
        }
        stream << "\n";
    }

    return true;
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
    m_datosEnviados = false;

    if (m_csvManager) {
        m_csvManager->closeCsvFile();
    }

    logEvent(EventType::PortClosed, "Puerto serie cerrado");
    setRecordingControlsState(false);

    // Transición de máquina de estados
    setAppState(AppState::Disconnected);
    // Reset experiment timer when port is closed
    resetExperimentTimer();
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
    ui->statusBar->showMessage("Puerto abierto. Presiona 'Enviar Datos' para configurar e iniciar.");
    
    logEvent(EventType::PortOpened, "Conexión exitosa al puerto serie");
    resetHealthMetrics();

    limpiarMatrizInterna();
    for (QPushButton* boton : botonesDatos) {
        boton->setStyleSheet(inactiveMatrixButtonStyle());
    }
    connected = true;
    plotting = false;  // NO iniciar plotting aqui. Esperar a EnviarDatos

    // Transicion de maquina de estados; updateUIForState() mostrará el mensaje
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
  HelpWindow *helpDialog = new HelpWindow (this);
  helpDialog->setAttribute(Qt::WA_DeleteOnClose);
  helpDialog->setWindowTitle ("Cómo usar la aplicación");
  helpDialog->show();
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void MainWindow::on_actionSalir_triggered()
{
    close();
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void MainWindow::on_actionPropiedades_de_Puerto_triggered()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Propiedades de Puerto");
    dialog.setMinimumWidth(420);

    QFormLayout *formLayout = new QFormLayout(&dialog);

    QComboBox *portCombo = new QComboBox(&dialog);
    for (const QSerialPortInfo &port : QSerialPortInfo::availablePorts()) {
        portCombo->addItem(port.portName());
    }
    portCombo->setCurrentText(ui->comboPort->currentText());

    QComboBox *baudCombo = new QComboBox(&dialog);
    for (int i = 0; i < ui->comboBaud->count(); ++i) {
        baudCombo->addItem(ui->comboBaud->itemText(i));
    }
    baudCombo->setCurrentText(ui->comboBaud->currentText());

    QComboBox *dataBitsCombo = new QComboBox(&dialog);
    for (int i = 0; i < ui->comboData->count(); ++i) {
        dataBitsCombo->addItem(ui->comboData->itemText(i));
    }
    dataBitsCombo->setCurrentIndex(ui->comboData->currentIndex());

    QComboBox *parityCombo = new QComboBox(&dialog);
    for (int i = 0; i < ui->comboParity->count(); ++i) {
        parityCombo->addItem(ui->comboParity->itemText(i));
    }
    parityCombo->setCurrentIndex(ui->comboParity->currentIndex());

    QComboBox *stopBitsCombo = new QComboBox(&dialog);
    for (int i = 0; i < ui->comboStop->count(); ++i) {
        stopBitsCombo->addItem(ui->comboStop->itemText(i));
    }
    stopBitsCombo->setCurrentIndex(ui->comboStop->currentIndex());

    formLayout->addRow("Puerto", portCombo);
    formLayout->addRow("Baudios", baudCombo);
    formLayout->addRow("Bits de datos", dataBitsCombo);
    formLayout->addRow("Paridad", parityCombo);
    formLayout->addRow("Bits de parada", stopBitsCombo);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText("Aceptar");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("Cancelar");
    formLayout->addRow(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        ui->comboPort->setCurrentText(portCombo->currentText());
        ui->comboBaud->setCurrentText(baudCombo->currentText());
        ui->comboData->setCurrentIndex(dataBitsCombo->currentIndex());
        ui->comboParity->setCurrentIndex(parityCombo->currentIndex());
        ui->comboStop->setCurrentIndex(stopBitsCombo->currentIndex());

        if (connected) {
            ui->statusBar->showMessage("Propiedades actualizadas. Se aplicarán en la próxima reconexión.");
        } else {
            ui->statusBar->showMessage("Propiedades de puerto actualizadas.");
        }
    }
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void MainWindow::on_actionExportar_datos_triggered()
{
    if (!ui->plot || ui->plot->graphCount() == 0) {
        QMessageBox::information(this, "Exportar datos", "No hay datos en el gráfico para exportar.");
        return;
    }

    const QString defaultName = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss") + "_grafico.csv";
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        "Exportar datos",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + defaultName,
        "CSV Files (*.csv);;Todos los archivos (*)"
    );

    if (filePath.isEmpty()) {
        return;
    }

    if (!exportPlotData(filePath)) {
        QMessageBox::warning(this, "Exportar datos", "No se pudo escribir el archivo de exportación.");
        return;
    }

    ui->statusBar->showMessage("Datos exportados a " + filePath);
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void MainWindow::on_actionPropiedades_de_grabacion_triggered()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Propiedades de grabación");
    dialog.setMinimumWidth(420);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *summary = new QLabel(&dialog);
    summary->setWordWrap(true);
    summary->setText(
        QString("La grabación CSV usa el mapeo activo.\n\nEstado actual: %1")
            .arg(ui->actionRecord_stream->isChecked() ? "grabación habilitada" : "grabación deshabilitada")
    );
    layout->addWidget(summary);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, &dialog);
    buttonBox->button(QDialogButtonBox::Close)->setText("Cerrar");
    QAbstractButton *openDocsButton = buttonBox->addButton("Abrir carpeta de documentos", QDialogButtonBox::ActionRole);
    layout->addWidget(buttonBox);

    connect(openDocsButton, &QAbstractButton::clicked, &dialog, [this]() {
        const QUrl docsUrl = QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        QDesktopServices::openUrl(docsUrl);
    });
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.exec();
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void MainWindow::on_actionManual_de_Usuario_triggered()
{
    const QString candidatePaths[] = {
        QDir::current().filePath("MANUAL_USUARIO.md"),
        QDir(QCoreApplication::applicationDirPath()).filePath("../MANUAL_USUARIO.md"),
        QDir(QCoreApplication::applicationDirPath()).filePath("MANUAL_USUARIO.md")
    };

    for (const QString &path : candidatePaths) {
        if (QFileInfo(path).exists()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()));
            return;
        }
    }

    QMessageBox::warning(this, "Manual de Usuario", "No se encontró MANUAL_USUARIO.md en el entorno actual.");
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void MainWindow::on_actionAcerca_de_triggered()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Acerca de MPCC — Multi-Photon Coincidence Counter (CIOp)");
    dialog.setMinimumWidth(420);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *info = new QLabel(&dialog);
    info->setWordWrap(true);
    info->setText(
        "MPCC — Multi-Photon Coincidence Counter (CIOp) v3.0.0\n\n"
        "Herramienta para visualizar y registrar datos de puerto serie.\n"
        "Distribuido bajo GPLv3."
    );
    layout->addWidget(info);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, &dialog);
    buttonBox->button(QDialogButtonBox::Close)->setText("Cerrar");
    QAbstractButton *openReadmeButton = buttonBox->addButton("Abrir README", QDialogButtonBox::ActionRole);
    layout->addWidget(buttonBox);

    connect(openReadmeButton, &QAbstractButton::clicked, &dialog, [this]() {
        const QString candidatePaths[] = {
            QDir::current().filePath("README.md"),
            QDir(QCoreApplication::applicationDirPath()).filePath("../README.md"),
            QDir(QCoreApplication::applicationDirPath()).filePath("README.md")
        };

        for (const QString &path : candidatePaths) {
            if (QFileInfo(path).exists()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()));
                return;
            }
        }

        QMessageBox::warning(this, "Acerca de", "No se encontró README.md en el entorno actual.");
    });
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.exec();
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void MainWindow::on_actionAutoScale_en_Y_triggered()
{
    on_pushButton_AutoScale_clicked();
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void MainWindow::on_actionMostar_todos_los_datos_toggled(bool checked)
{
    setIncomingDataDisplayMode(checked);
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Conecta al puerto COM.
 * Nuevo flujo: Conectar abre puerto → Enviar Datos auto-inicia adquisición
 */
void MainWindow::on_actionConnect_triggered()
{
  if (connected)
    {
            /* Si ya está conectado y pausado, usa Pausa/Reanuda para continuar */
            // Esta rama es para casos de reconexión después de pausa
            // Pero normalmente no se alcanza porque Pausa/Reanuda maneja todo
            return;
    }
  else
    {
    /* Si no está conectado, abre el puerto */
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

      logEvent(EventType::PortOpened, "Abriendo puerto");
      openPort (portInfo, baudRate, dataBits, parity, stopBits);
  }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Pausa el ploteo manteniendo abierto el puerto COM.
 */
void MainWindow::on_actionPause_Plot_triggered()
{
    // Toggle behavior: if currently acquiring, pause; if paused, resume.
    if (m_experimentFinished) {
        ui->statusBar->showMessage("El experimento ya finalizó. Reconfigurá o enviá nuevos datos para comenzar otro ciclo.");
        // No llamar a cambiarEstado() aquí: dejar que updateUIForState() maneje
        // el mensaje permanente. Solo asegurarse de que el estado esté en Paused.
        setAppState(AppState::Paused);
        return;
    }

    if (m_appState == AppState::Paused || !plotting) {
                // Resume acquisition
                logEvent(EventType::Started, "Reanudando adquisición de datos");
                if (m_csvManager) {
                    m_csvManager->logPauseEvent(false);
                }
        updateTimer.start(kPlotUpdateIntervalMs);
                plotting = true;
        ui->statusBar->showMessage("Plot reanudado. La adquisición y la grabación continúan.");
                // Dejar que setAppState/updateUIForState actualice el mensaje de estado.
                setAppState(AppState::Acquiring);
        // El cronómetro de experimento siempre debe reanudarse al volver a adquirir.
        resumeExperimentTimer();
        } else {
                // Pause acquisition
                logEvent(EventType::Stopped, "Pausando adquisición de datos");
                if (m_csvManager) {
                    m_csvManager->logPauseEvent(true);
                }
                updateTimer.stop();
                plotting = false;
        ui->statusBar->showMessage("Plot pausado. Presiona 'Pausa/Reanuda' para continuar.");
            // Dejar que updateUIForState() establezca el mensaje permanente.
            setAppState(AppState::Paused);
                // Pause experiment timer
                pauseExperimentTimer();
        }
}
/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief Inicia o detiene la grabacion del stream en CSV.
 */
void MainWindow::on_actionRecord_stream_triggered()
{
    if (!m_csvManager) {
        ui->statusBar->showMessage("Grabación no disponible: CsvManager no inicializado.");
        return;
    }

    const qint64 desiredMs = selectedExperimentDurationMs();

    if (!ui->actionRecord_stream->isChecked() && desiredMs > 0 &&
        (m_appState == AppState::Acquiring || m_appState == AppState::Paused)) {
        setRecordingControlsState(true);
        ui->statusBar->showMessage("La duración del experimento obliga a mantener la grabación activa hasta finalizar.");
        return;
    }

    if (ui->actionRecord_stream->isChecked())
    {
        if (m_appState == AppState::Disconnected || m_appState == AppState::Fault) {
            ui->actionRecord_stream->setChecked(false);
            setRecordingControlsState(false);
            ui->statusBar->showMessage("Primero enviá datos y conectá el puerto antes de grabar.");
            return;
        }

        // Compute desired duration ms from UI and set metadata on CsvManager
        m_csvManager->setExperimentDurationMs(desiredMs);
        m_csvManager->openCsvFile(this);
        if (!m_csvManager->isOpen()) {
            ui->actionRecord_stream->setChecked(false);
            setRecordingControlsState(false);
        } else {
            setRecordingControlsState(true);
            if (m_appState == AppState::Acquiring) {
                startExperimentTimer();
                ui->statusBar->showMessage("Grabación iniciada.");
                logEvent(EventType::Started, "Grabación iniciada por usuario");
            } else {
                resetExperimentTimer();
                if (desiredMs > 0) {
                    experimentCountdownLabel->setText(QString("Restante: %1").arg(formatDurationHms(desiredMs)));
                }
                ui->statusBar->showMessage("Grabación armada. Iniciará al comenzar el experimento.");
                logEvent(EventType::ConfigApplied, "Grabación armada por usuario");
            }
        }
    }
    else
    {
        m_csvManager->closeCsvFile();
        setRecordingControlsState(false);
        resetExperimentTimer();
        experimentCountdownLabel->setText("");
        ui->statusBar->showMessage("Grabación detenida.");
        logEvent(EventType::Stopped, "Grabación detenida por usuario");
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
      m_datosEnviados = false;

    setRecordingControlsState(false);

      receivedData.clear();
      ui->textEdit_UartWindow->append(receivedData);

      limpiarMatrizInterna();

    ui->statusBar->showMessage("Puerto desconectado. La secuencia quedó detenida.");
    cambiarEstado("DESCONECTADO", "red");

      // Transición de máquina de estados
      setAppState(AppState::Disconnected);
            resetExperimentTimer();
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
    setIncomingDataDisplayMode(ui->pushButton_ShowallData->isChecked());
}

void MainWindow::setIncomingDataDisplayMode(bool showAll)
{
    filterDisplayedData = !showAll;

    const QSignalBlocker blockButton(ui->pushButton_ShowallData);
    const QSignalBlocker blockAction(ui->actionMostar_todos_los_datos);

    ui->pushButton_ShowallData->setChecked(showAll);
    ui->actionMostar_todos_los_datos->setChecked(showAll);
    ui->pushButton_ShowallData->setText(showAll ? "Filter Incoming Data" : "Show All Incoming Data");
}

void MainWindow::setRecordingControlsState(bool recording)
{
    const QSignalBlocker blockAction(ui->actionRecord_stream);

    ui->actionRecord_stream->setChecked(recording);
}

qint64 MainWindow::selectedExperimentDurationMs() const
{
    if (!ui || !ui->ExperimentDurationNum || !ui->ExperimentDurationUnit) {
        return 0;
    }

    const int value = ui->ExperimentDurationNum->value();
    switch (ui->ExperimentDurationUnit->currentIndex()) {
    case 0:
        return qint64(value) * 1000;
    case 1:
        return qint64(value) * 60 * 1000;
    case 2:
        return qint64(value) * 3600 * 1000;
    default:
        return qint64(value) * 1000;
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
    // El botón ya queda deshabilitado en este caso (ver updateUIForState), pero
    // se valida también acá: la pausa es solo pausa/reanuda, no una ventana
    // para reconfigurar y reenviar a mitad de un ciclo.
    if (m_appState == AppState::Paused) {
        ui->statusBar->showMessage("Reanudá la adquisición ('Pausa/Reanuda') antes de reconfigurar y reenviar datos.");
        return;
    }

    const qint64 desiredMs = selectedExperimentDurationMs();

    m_experimentFinished = false;

    ui->textEdit_UartWindow->clear();
    m_datosEnviados = true;

    if (m_csvManager) {
        m_csvManager->setExperimentDurationMs(desiredMs);
    }

    if (desiredMs > 0) {
        if (!ui->actionRecord_stream->isChecked()) {
            setRecordingControlsState(true);
        }

        if (m_csvManager && !m_csvManager->isOpen()) {
            if (!m_csvManager->openCsvFile(this)) {
                setRecordingControlsState(false);
                ui->statusBar->showMessage("No se pudo iniciar la grabación obligatoria del experimento.");
                cambiarEstado("GRABACIÓN CANCELADA", "orange");
                setAppState(AppState::ReadyForConfiguration);
                return;
            }
        }
    }

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
    cambiarEstado("Aplicando y armando...", "yellow");
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

    // Limpiar indicador de cambios pendientes
    clearPendingChanges();

    // ====================================================================
    // AHORA: Automáticamente inicia adquisición
    // ====================================================================
    logEvent(EventType::Started, "Iniciando adquisición de datos");
    updateTimer.start(kPlotUpdateIntervalMs);
    plotting = true;
    ui->statusBar->showMessage("Adquisición iniciada. Presioná 'Pausa/Reanuda' para pausar.");

    // Transición de máquina de estados: confiar en updateUIForState para el mensaje
    setAppState(AppState::Acquiring);

    // El cronómetro corre siempre, haya o no grabación activa.
    startExperimentTimer();
}
void MainWindow::on_ResetearDatos_clicked()
{
    // El botón ya queda deshabilitado en este caso (ver updateUIForState), pero
    // se valida también acá por la misma razón que en EnviarDatos: la pausa es
    // solo pausa/reanuda, no una ventana para resetear a mitad de un ciclo.
    if (m_appState == AppState::Paused) {
        ui->statusBar->showMessage("Reanudá la adquisición ('Pausa/Reanuda') antes de resetear.");
        return;
    }

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
    // Cuando está marcado, el cuadro de texto se muestra.
    ui->textEdit_UartWindow->setVisible(arg1);
    ui->pushButton_TextEditHide->setText(arg1 ? "Hide TextBox" : "Show TextBox");
    ui->actionEsconder_Caja_de_Texto->setText(arg1 ? "Esconder Caja de Texto" : "Mostrar Caja de Texto");
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
            nuevoMaximo = 2147483647;
            break;

        default:
            nuevoMinimo = 1;
            nuevoMaximo = 2147483647;
            break;
    }

    if (nuevoValor < nuevoMinimo) {
        nuevoValor = nuevoMinimo;
    }
    if (nuevoValor > nuevoMaximo) {
        nuevoValor = nuevoMaximo;
    }

    ui->TiempoNum->blockSignals(true);
    ui->TiempoNum->setRange(nuevoMinimo, nuevoMaximo);
    ui->TiempoNum->setValue(nuevoValor);
    ui->TiempoNum->blockSignals(false);

    indiceUnidadAnterior = nuevoIndice;
    markPendingChanges();
}
void MainWindow::on_Delay_C_valueChanged(int arg1)        { m_fpgaProtocol->setDelayC(static_cast<quint8>(arg1)); markPendingChanges(); }
void MainWindow::on_Delay_D_valueChanged(int arg1)        { m_fpgaProtocol->setDelayD(static_cast<quint8>(arg1)); markPendingChanges(); }

void MainWindow::on_Ancho_de_pulso_valueChanged(int arg1) { m_fpgaProtocol->setPulseWidth(static_cast<quint8>(arg1)); markPendingChanges(); }
void MainWindow::on_Delay_A_valueChanged(int arg1)        { m_fpgaProtocol->setDelayA(static_cast<quint8>(arg1)); markPendingChanges(); }
void MainWindow::on_Delay_B_valueChanged(int arg1)        { m_fpgaProtocol->setDelayB(static_cast<quint8>(arg1)); markPendingChanges(); }

void MainWindow::actualizarEstadoGraf(int indiceBotonPresionado)
{
    // Marca visualmente la columna activa y la comunica al protocolo.
    for (QPushButton* boton : botonesGraf) {
        boton->setStyleSheet(inactiveMatrixButtonStyle());
    }

    botonesGraf[indiceBotonPresionado]->setStyleSheet(activeMatrixButtonStyle());
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
        boton->setStyleSheet(activeMatrixButtonStyle());
    } else {
        boton->setStyleSheet(inactiveMatrixButtonStyle());
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
        boton->setStyleSheet(inactiveMatrixButtonStyle());
    }

    for (QPushButton* boton : botonesGraf) {
        boton->setStyleSheet(inactiveMatrixButtonStyle());
    }

    columnaSeleccionada = 0;
}

QString MainWindow::activeMatrixButtonStyle() const
{
    return QStringLiteral(
        "QPushButton { background-color: rgb(15, 125, 15); }"
        "QPushButton:disabled { background-color: rgb(70, 90, 70); color: rgb(150, 150, 150); }"
    );
}

QString MainWindow::inactiveMatrixButtonStyle() const
{
    return QStringLiteral(
        "QPushButton { background-color: rgb(150, 50, 50); }"
        "QPushButton:disabled { background-color: rgb(90, 70, 70); color: rgb(150, 150, 150); }"
    );
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

    if (!canTransitionToState(newState)) {
        qWarning() << "Transición de estado inválida:" << getStateDisplayName(m_appState)
                   << "->" << getStateDisplayName(newState);
        return;
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
    const bool isAcquiring = (m_appState == AppState::Acquiring);
    const bool isPaused = (m_appState == AppState::Paused);
    const bool isFault = (m_appState == AppState::Fault);

    const bool isRecording = (m_csvManager && m_csvManager->isOpen());

    // Pausa/Reanuda es solo eso: pausar o reanudar la adquisición y el
    // guardado en curso. Ningún parámetro (matriz, selectores 1–8, tiempo,
    // ancho de pulso, retardos) ni Enviar Datos/Reset se puede tocar mientras
    // se está en pausa, haya o no grabación activa; primero hay que reanudar.
    const bool canConfigure = isReadyForConfig && !isFault;

    // Controles de puerto COM
    ui->comboPort->setEnabled(isDisconnected);
    ui->comboBaud->setEnabled(isDisconnected);
    ui->comboData->setEnabled(isDisconnected);
    ui->comboParity->setEnabled(isDisconnected);
    ui->comboStop->setEnabled(isDisconnected);

    // Botones de acción principal
    // Atados al estado real del puerto (connected), no al estado lógico:
    // si se entra en Fault (ej. preflight check fallido) con el puerto
    // realmente abierto, Desconectar debe seguir disponible para poder
    // recuperarse, y viceversa si el puerto nunca llegó a abrirse.
    ui->actionConnect->setEnabled(!connected);
    ui->actionDisconnect->setEnabled(connected);
    ui->actionPause_Plot->setEnabled(isAcquiring || isPaused);
    ui->savePNGButton->setEnabled(!isDisconnected);

    // Controles de configuración
    ui->GRAF_1->setEnabled(canConfigure);
    ui->GRAF_2->setEnabled(canConfigure);
    ui->GRAF_3->setEnabled(canConfigure);
    ui->GRAF_4->setEnabled(canConfigure);
    ui->GRAF_5->setEnabled(canConfigure);
    ui->GRAF_6->setEnabled(canConfigure);
    ui->GRAF_7->setEnabled(canConfigure);
    ui->GRAF_8->setEnabled(canConfigure);
    for (QPushButton *btn : botonesDatos) {
        btn->setEnabled(canConfigure);
    }
    ui->TiempoNum->setEnabled(canConfigure);
    ui->TiempoBox->setEnabled(canConfigure);
    ui->Ancho_de_pulso->setEnabled(canConfigure);
    ui->Delay_A->setEnabled(canConfigure);
    ui->Delay_B->setEnabled(canConfigure);
    ui->Delay_C->setEnabled(canConfigure);
    ui->Delay_D->setEnabled(canConfigure);
    // La duración del experimento se lee recién al presionar Enviar Datos
    // (selectedExperimentDurationMs()); si quedara editable durante la
    // adquisición o la pausa, cambiarla no tendría ningún efecto hasta el
    // próximo ciclo, dando la falsa impresión de que se aplicó al vuelo.
    ui->ExperimentDurationNum->setEnabled(canConfigure);
    ui->ExperimentDurationUnit->setEnabled(canConfigure);

    // Botones de envío/reset
    // Enviar Datos queda inhabilitado durante toda la pausa (haya o no
    // grabación activa): reenviar configuración a mitad de un ciclo pausado
    // reinicia la adquisición de forma confusa, así que el operador debe
    // reanudar primero. Misma política que Reset.
    ui->EnviarDatos->setEnabled(canConfigure);
    // Reset queda inhabilitado durante la pausa (haya o no grabación activa):
    // resetear a mitad de un ciclo pausado invalida la adquisición en curso,
    // así que el operador debe primero reanudar o reconfigurar, misma política
    // que ya se aplica a Enviar Datos y Pausa/Reanuda.
    ui->ResetearDatos->setEnabled(canConfigure);

    // Grabación CSV
    const bool canRecord = isReadyForConfig || isAcquiring || isPaused;
    ui->actionRecord_stream->setEnabled(canRecord);

    // Actualizar mensaje de estado
    QString stateMsg = getStateDisplayName(m_appState);
    QString color = "black";

    switch (m_appState) {
    case AppState::Disconnected:
        stateMsg += " - Abrí el puerto para comenzar";
        color = "red";
        break;
    case AppState::ReadyForConfiguration:
        stateMsg += " - Presioná 'Enviar Datos' para iniciar";
        color = "blue";
        break;
    case AppState::Acquiring:
        stateMsg += isRecording ? " - Adquisición y guardado en curso" : " - Adquisición en curso";
        color = "darkgreen";
        break;
    case AppState::Paused:
        // Si el experimento ya terminó, mostrar mensaje específico en lugar
        // del mensaje genérico de pausa para no confundir al operador.
        if (m_experimentFinished) {
            stateMsg = "Experimento finalizado - Reconfigurá y presioná 'Enviar Datos' para un nuevo ciclo";
            color = "blue";
        } else if (isRecording) {
            stateMsg += " - Pausa activa (grabación en pausa), presioná 'Pausa/Reanuda' para continuar";
            color = "orange";
        } else {
            stateMsg += " - Pausa activa, presioná 'Pausa/Reanuda' para continuar";
            color = "orange";
        }
        break;
    case AppState::Fault:
        stateMsg += " - Error detectado, desconectá para recuperar";
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
        return (newState == AppState::Acquiring || newState == AppState::Disconnected || newState == AppState::Fault);
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
        // Resaltar botón "Enviar Datos" en naranja para atraer atención.
        // La regla :disabled es necesaria porque un stylesheet con colores
        // explícitos pisa la paleta gris que Qt aplica automáticamente a un
        // botón deshabilitado (ej. durante la pausa): sin ella, el botón se
        // veía "activo" en naranja aunque no respondiera a los clics.
        ui->EnviarDatos->setStyleSheet(
            "QPushButton { background-color: rgb(255, 140, 0); color: white; font-weight: bold; }"
            "QPushButton:disabled { background-color: rgb(200, 200, 200); color: rgb(120, 120, 120); }");
        ui->statusBar->showMessage("⚠ Cambios pendientes de aplicar. Presioná 'Enviar Datos'.");
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
        result.success = false;
        return result;
    }

    // 2. Verificar que haya canales activos
    QStringList labels = m_fpgaProtocol->generateLabels();
    if (labels.isEmpty()) {
        result.errorMessage = "Sin canales activos. Configurá matriz de botones.";
        result.success = false;
        return result;
    }

    // 3. Verificar tiempo válido
    int timeValue = ui->TiempoNum->value();
    if (timeValue <= 0) {
        result.errorMessage = "Valor de tiempo inválido. Asegurate de que sea > 0.";
        result.success = false;
        return result;
    }

    // 4. Verificar rango de tiempos
    int timeUnitIndex = ui->TiempoBox->currentIndex();
    quint32 baseTime = m_fpgaProtocol->convertToBase(timeValue, timeUnitIndex);
    if (baseTime < 56) {  // 5.6 ms en unidad base
        result.errorMessage = "Tiempo mínimo permitido: 5.6 ms.";
        result.success = false;
        return result;
    }
    if (baseTime > 99999999) {
        result.errorMessage = "Tiempo máximo permitido: 99,999,999 (unidad base).";
        result.success = false;
        return result;
    }

    // 5. Verificar CSV si está grabando
    if (ui->actionRecord_stream->isChecked()) {
        if (!m_csvManager || !m_csvManager->isOpen()) {
            result.errorMessage = "Grabación CSV habilitada pero archivo no está abierto.";
            result.success = false;
            return result;
        }
    }

    result.success = true;
    return result;
}

/** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

// ─── Telemetría de Salud en Tiempo Real ────────────────────────────────────────

void MainWindow::updateHealthMetrics(const QStringList &)
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

    const quint64 totalPackets = m_healthMetrics.validPacketCount
                               + m_healthMetrics.invalidPacketCount
                               + m_healthMetrics.lostPacketCount;
    if (totalPackets == 0) {
        return;
    }

    const double invalidRatio = static_cast<double>(m_healthMetrics.invalidPacketCount)
                                / static_cast<double>(totalPackets);
    const bool warningActive = invalidRatio >= kInvalidPacketWarningRatio;
    const bool faultActive = invalidRatio >= kInvalidPacketFaultRatio;

    if (!warningActive) {
        g_commDegradationState = {};
        setCommunicationHealthLabel(QString(), QString(), false);
        return;
    }

    if (g_commDegradationState.warningSinceMs == 0) {
        g_commDegradationState.warningSinceMs = now;
    }

    if (faultActive && g_commDegradationState.faultSinceMs == 0) {
        g_commDegradationState.faultSinceMs = now;
    }

    const qint64 warningElapsedMs = now - g_commDegradationState.warningSinceMs;
    const qint64 faultElapsedMs = faultActive ? (now - g_commDegradationState.faultSinceMs) : 0;

    if (faultActive && faultElapsedMs >= kInvalidPacketSustainMs) {
        const QString faultMessage = QStringLiteral("Comunicación en falla: %1%% de tramas inválidas")
                                         .arg(QString::number(invalidRatio * 100.0, 'f', 1));
        setCommunicationHealthLabel(faultMessage, QStringLiteral("color: red; font-weight: bold;"), true);
        ui->statusBar->showMessage(faultMessage);

        if (!g_commDegradationState.faultLogged) {
            logEvent(EventType::Error, faultMessage);
            g_commDegradationState.faultLogged = true;
        }

        if (m_appState != AppState::Fault) {
            setAppState(AppState::Fault);
        }
        return;
    }

    if (warningElapsedMs >= kInvalidPacketSustainMs) {
        const QString warningMessage = QStringLiteral("Advertencia: %1%% de tramas inválidas")
                                           .arg(QString::number(invalidRatio * 100.0, 'f', 1));
        setCommunicationHealthLabel(warningMessage, QStringLiteral("color: darkorange; font-weight: bold;"), true);
        ui->statusBar->showMessage(warningMessage);

        if (!g_commDegradationState.warningLogged) {
            logEvent(EventType::Error, warningMessage);
            g_commDegradationState.warningLogged = true;
        }
    }
}

void MainWindow::resetHealthMetrics()
{
    m_healthMetrics.validPacketCount = 0;
    m_healthMetrics.invalidPacketCount = 0;
    m_healthMetrics.lostPacketCount = 0;
    m_healthMetrics.lastPacketTimestampMs = 0;
    m_healthMetrics.frameLatencyMs = 0.0f;
    m_healthMetrics.firstPacketTime = 0;
    g_commDegradationState = {};
    setCommunicationHealthLabel(QString(), QString(), false);
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

