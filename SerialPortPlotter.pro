#-------------------------------------------------
#
# Project created by QtCreator 2014-12-22T14:53:33
#
#-------------------------------------------------

QT       += core gui
QT       += serialport
QT += widgets
CONFIG += c++11

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

TARGET = serial_port_plotter
TEMPLATE = app

SOURCES += main.cpp\
        console.cpp \
        mainwindow.cpp \
        qcustomplot/qcustomplot.cpp \
        helpwindow.cpp \
        serialportmanager.cpp \
        serialmessageparser.cpp \
        fpgaprotocol.cpp \
        plotmanager.cpp \
        csvmanager.cpp \
        profilemanager.cpp

HEADERS  += mainwindow.hpp \
        console.h \
        qcustomplot/qcustomplot.h \
        helpwindow.hpp \
        serialportmanager.hpp \
        serialmessageparser.hpp \
        fpgaprotocol.hpp \
        plotmanager.hpp \
        csvmanager.hpp \
        profilemanager.hpp


FORMS    += mainwindow.ui \
    helpwindow.ui

RESOURCES += \
    res/serial_port_plotter.qrc \
   ## res/qdark_stylesheet/qdarkstyle/style.qrc

# The following line compiles on Release but not on Debug, so this workaroung is used:
RC_FILE = res/serial_port_plotter.rc
# ---------------------------------------------------------------------------
# Manual de usuario: se copia junto al ejecutable en cada build, para que
# Ayuda -> Manual de Usuario lo encuentre al correr desde Qt Creator.
# ---------------------------------------------------------------------------
MANUAL_FILES = MANUAL_USUARIO.md

win32 {
    CONFIG(release, debug|release) {
        MANUAL_DEST = $$OUT_PWD/release
    } else {
        MANUAL_DEST = $$OUT_PWD/debug
    }
} else {
    MANUAL_DEST = $$OUT_PWD
}

for(manual_file, MANUAL_FILES) {
    QMAKE_POST_LINK += $$QMAKE_COPY \
        $$shell_quote($$shell_path($$PWD/$$manual_file)) \
        $$shell_quote($$shell_path($$MANUAL_DEST)) $$escape_expand(\\n\\t)
}

DISTFILES += $$MANUAL_FILES
# Call the resource compiler
#win32:mkver_rc.target = serial_port_plotter_res.o
#win32:mkver_rc.commands = windres --use-temp-file -i serial_port_plotter.rc -o serial_port_plotter_res.o --include-dir=../res -DVERSION_H_INTERN $(DEFINES)
#win32:QMAKE_EXTRA_TARGETS += mkver_rc
#win32:PRE_TARGETDEPS += serial_port_plotter_res.o
#win32:LIBS += serial_port_plotter_res.o
