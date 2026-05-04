#ifndef PROFILEMANAGER_HPP
#define PROFILEMANAGER_HPP

#include <QBitArray>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class QPushButton;
class QSpinBox;
class QComboBox;
class FpgaProtocol;

class ProfileManager
{
public:
    struct OperativeProfile {
        QString name;
        QBitArray matrixButtons;
        quint8 pulseWidth = 0;
        quint8 delayA = 0;
        quint8 delayB = 0;
        quint8 delayC = 0;
        quint8 delayD = 0;
        int timeValue = 0;
        int timeUnitIndex = 0;
        qint64 createdTimestamp = 0;

        QString toJson() const;
        static OperativeProfile fromJson(const QString &json);
    };

    static QString profilesDirectory();
    static QStringList profileNames();
    static QString profilePath(const QString &profileNameOrPath);
    static bool saveProfile(const QString &profileName, const OperativeProfile &profile, QString *errorMessage = nullptr);
    static bool loadProfile(const QString &profileNameOrPath, OperativeProfile *profile, QString *errorMessage = nullptr);
    static bool deleteProfile(const QString &profileNameOrPath, QString *errorMessage = nullptr);
    static bool renameProfile(const QString &oldProfileNameOrPath, const QString &newProfileName, QString *errorMessage = nullptr);

    struct UiContext {
        QSpinBox *timeValueSpin = nullptr;
        QComboBox *timeUnitCombo = nullptr;
        QSpinBox *pulseWidthSpin = nullptr;
        QSpinBox *delayASpin = nullptr;
        QSpinBox *delayBSpin = nullptr;
        QSpinBox *delayCSpin = nullptr;
        QSpinBox *delayDSpin = nullptr;
        QVector<QPushButton*> matrixButtons;
        FpgaProtocol *fpgaProtocol = nullptr;
        std::function<void()> markPendingChanges;
    };

    static void applyProfileToUi(const OperativeProfile &profile, const UiContext &context);

private:
    static QString resolveProfilePath(const QString &profileNameOrPath);
};

#endif // PROFILEMANAGER_HPP