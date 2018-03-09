/////////////////////////////////////////////////////////////////////////////
/// Name:        main.cpp
/// Purpose:     main class : shows the main numSCAL window
/// Author:      Ahmed Hamdi Boujelben <ahmed.hamdi.boujelben@gmail.com>
/// Created:     2017
/// Copyright:   (c) 2018 Ahmed Hamdi Boujelben
/// Licence:     Attribution-NonCommercial 4.0 International
/////////////////////////////////////////////////////////////////////////////

#include <QApplication>
#include "mainwindow.h"

int main(int argc , char *argv[])
{
   QApplication a(argc,argv);

   MainWindow window;
   window.show();

   return a.exec();
}
