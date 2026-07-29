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

#ifndef HELPWINDOW_HPP
#define HELPWINDOW_HPP

#include <QDialog>

namespace Ui {
    class HelpWindow;
}

class HelpWindow : public QDialog
{
    Q_OBJECT

public:
    // Constructor de la ventana modal/no modal de ayuda.
    explicit HelpWindow(QWidget *parent = 0);
    // Destructor: libera recursos de UI generados por Qt Designer.
    ~HelpWindow();

private:
    // Puntero a la interfaz generada desde helpwindow.ui.
    Ui::HelpWindow *ui;
};

#endif // HELPWINDOW_HPP
