#pragma once

#include <cstdint>

#include <QMainWindow>
#include <QTimer>

#include "obsclient.hpp"
#include "openlpclient.hpp"

class MainWindow: public QMainWindow {
	Q_OBJECT

   private:
	   OBSClient m_obsClient;
		OpenLPClient m_openlpClient;
		QTimer m_statusBarTimer;
		uint64_t m_openLPUpdates = 0;
		uint64_t m_obsUpdates = 0;

   public:
		MainWindow(QWidget *parent = nullptr);
		~MainWindow();

   private slots:
		void openLpConnectionInit();

		void openLpConnectionLost();

		void obsConnectionInit();

		void obsConnectionLost();

		void refreshStatusBar();

};
