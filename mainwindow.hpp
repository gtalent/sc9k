#pragma once

#include <QMainWindow>

#include "obsclient.hpp"
#include "openlpclient.hpp"

class MainWindow: public QMainWindow {
	Q_OBJECT

   private:
	   OBSClient m_obsClient;
		OpenLPClient m_openlpClient;

   public:
		MainWindow(QWidget *parent = nullptr);
		~MainWindow();

   private slots:

};
