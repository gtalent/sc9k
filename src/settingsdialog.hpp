/*
 * Copyright 2021 - 2023 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <QDialog>

#include "settingsdata.hpp"

class SettingsDialog: public QDialog {
	Q_OBJECT
	private:
		class QLabel *m_errLbl = nullptr;
		class QLineEdit *m_cameraIpLe = nullptr;
		class QLineEdit *m_cameraPortLe = nullptr;
		class QLineEdit *m_openLpIpLe = nullptr;
		class QLineEdit *m_openLpPortLe = nullptr;
		class QLineEdit *m_obsIpLe = nullptr;
		class QLineEdit *m_obsPortLe = nullptr;
		class QTableWidget *m_viewTable = nullptr;
	public:
		explicit SettingsDialog(QWidget *parent);
	private:
		QWidget *setupNetworkInputs(QWidget *parent);
		QWidget *setupViewConfig(QWidget *parent);
		QWidget *setupButtons(QWidget *parent);
		void handleOK();
		void setupViewRow(int row, View const&view = {});
		/**
		 * Gets views from table.
		 * @return error code
		 */
		[[nodiscard("Must check error code")]]
		int collectViews(QVector<View> &views) const;
};
