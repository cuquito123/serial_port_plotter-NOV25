#include "profilemanager.hpp"

#include "fpgaprotocol.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>

QString ProfileManager::OperativeProfile::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;

    QJsonArray matrix;
    for (int i = 0; i < matrixButtons.size(); ++i) {
        matrix.append(matrixButtons.testBit(i));
    }
    obj["matrix"] = matrix;
    obj["pulseWidth"] = pulseWidth;
    obj["delayA"] = delayA;
    obj["delayB"] = delayB;
    obj["delayC"] = delayC;
    obj["delayD"] = delayD;
    obj["timeValue"] = timeValue;
    obj["timeUnitIndex"] = timeUnitIndex;
    obj["createdTimestamp"] = QString::number(createdTimestamp);

    QJsonDocument doc(obj);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

ProfileManager::OperativeProfile ProfileManager::OperativeProfile::fromJson(const QString &json)
{
    OperativeProfile profile;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) {
        return profile;
    }

    const QJsonObject obj = doc.object();
    profile.name = obj.value("name").toString();

    const QJsonArray matrix = obj.value("matrix").toArray();
    profile.matrixButtons = QBitArray(matrix.size());
    for (int i = 0; i < matrix.size(); ++i) {
        profile.matrixButtons.setBit(i, matrix.at(i).toBool());
    }

    profile.pulseWidth = static_cast<quint8>(obj.value("pulseWidth").toInt());
    profile.delayA = static_cast<quint8>(obj.value("delayA").toInt());
    profile.delayB = static_cast<quint8>(obj.value("delayB").toInt());
    profile.delayC = static_cast<quint8>(obj.value("delayC").toInt());
    profile.delayD = static_cast<quint8>(obj.value("delayD").toInt());
    profile.timeValue = obj.value("timeValue").toInt();
    profile.timeUnitIndex = obj.value("timeUnitIndex").toInt();
    profile.createdTimestamp = obj.value("createdTimestamp").toString().toLongLong();
    return profile;
}

QString ProfileManager::profilesDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + "/.serial_port_plotter";
    }

    const QString dirPath = base + "/profiles";
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dirPath;
}

QStringList ProfileManager::profileNames()
{
    QDir dir(profilesDirectory());
    const QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);
    QStringList names;
    for (const QString &fileName : files) {
        names << fileName.left(fileName.size() - 5);
    }
    return names;
}

QString ProfileManager::resolveProfilePath(const QString &profileNameOrPath)
{
    QString path = profileNameOrPath;
    if (!path.contains('/') && !path.contains('\\')) {
        path = profilesDirectory() + "/" + path;
        if (!path.endsWith(".json")) {
            path += ".json";
        }
    }
    return path;
}

QString ProfileManager::profilePath(const QString &profileNameOrPath)
{
    return resolveProfilePath(profileNameOrPath);
}

bool ProfileManager::saveProfile(const QString &profileName, const OperativeProfile &profile, QString *errorMessage)
{
    const QString path = resolveProfilePath(profileName);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo abrir el archivo para guardar el perfil.");
        }
        return false;
    }

    file.write(profile.toJson().toUtf8());
    file.close();
    return true;
}

bool ProfileManager::loadProfile(const QString &profileNameOrPath, OperativeProfile *profile, QString *errorMessage)
{
    if (!profile) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Perfil de salida inválido.");
        }
        return false;
    }

    const QString path = resolveProfilePath(profileNameOrPath);
    QFile file(path);
    if (!file.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se encontró el perfil.");
        }
        return false;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo abrir el perfil.");
        }
        return false;
    }

    *profile = OperativeProfile::fromJson(QString::fromUtf8(file.readAll()));
    return true;
}

bool ProfileManager::deleteProfile(const QString &profileNameOrPath, QString *errorMessage)
{
    const QString path = resolveProfilePath(profileNameOrPath);
    if (!QFile::remove(path)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo eliminar el perfil.");
        }
        return false;
    }
    return true;
}

bool ProfileManager::renameProfile(const QString &oldProfileNameOrPath, const QString &newProfileName, QString *errorMessage)
{
    const QString oldPath = resolveProfilePath(oldProfileNameOrPath);
    const QString newPath = resolveProfilePath(newProfileName);

    if (oldPath == newPath) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("El nuevo nombre es igual al actual.");
        }
        return false;
    }

    if (QFile::exists(newPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Ya existe un perfil con ese nombre.");
        }
        return false;
    }

    if (!QFile::rename(oldPath, newPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo renombrar el perfil.");
        }
        return false;
    }

    return true;
}

void ProfileManager::applyProfileToUi(const OperativeProfile &profile, const UiContext &context)
{
    if (context.timeValueSpin) {
        context.timeValueSpin->setValue(profile.timeValue);
    }
    if (context.timeUnitCombo) {
        context.timeUnitCombo->setCurrentIndex(profile.timeUnitIndex);
    }
    if (context.pulseWidthSpin) {
        context.pulseWidthSpin->setValue(profile.pulseWidth);
    }
    if (context.delayASpin) {
        context.delayASpin->setValue(profile.delayA);
    }
    if (context.delayBSpin) {
        context.delayBSpin->setValue(profile.delayB);
    }
    if (context.delayCSpin) {
        context.delayCSpin->setValue(profile.delayC);
    }
    if (context.delayDSpin) {
        context.delayDSpin->setValue(profile.delayD);
    }

    for (int i = 0; i < context.matrixButtons.size() && i < profile.matrixButtons.size(); ++i) {
        const bool on = profile.matrixButtons.testBit(i);
        if (context.fpgaProtocol) {
            context.fpgaProtocol->setButton(i, on);
        }
        if (context.matrixButtons[i]) {
            context.matrixButtons[i]->setStyleSheet(on
                                                    ? "background-color: rgb(15, 125, 15);"
                                                    : "background-color: rgb(150, 50, 50);");
        }
    }

    if (context.markPendingChanges) {
        context.markPendingChanges();
    }
}