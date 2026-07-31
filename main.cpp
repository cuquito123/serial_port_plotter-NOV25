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
#include <QApplication>

int main(int argc, char *argv[])
{
    // Inicializa la aplicacion Qt con argumentos de linea de comandos.
    QApplication a(argc, argv);

    /* Aplica la hoja de estilos global si el recurso existe */
    QFile file(":/serial_port_plotter/styles/style.qss");
    if(file.open(QIODevice::ReadOnly | QIODevice::Text))
      {
        a.setStyleSheet(file.readAll());
        file.close();
      }

    /* Crea la ventana principal y configura icono/titulo */
    MainWindow w;
    QIcon appIcon(":/serial_port_plotter/icons/serial_port_icon.icns");
    w.setWindowIcon(appIcon);
    w.setWindowTitle("MPCC — Multi-Photon Coincidence Counter (CIOp) v3.0.0");
    w.show();

    // Entra al bucle principal de eventos.
    return a.exec();
}
