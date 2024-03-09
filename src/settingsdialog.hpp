/*
 * Copyright 2021 - 2024 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <QDialog>

#include "consts.hpp"
#include "settingsdata.hpp"

class SettingsDialog: public QDialog {
	Q_OBJECT
	private:
		QVector<VideoConfig> m_videoConfig = QVector<VideoConfig>(MaxCameraPresets);
		class QLabel *m_errLbl = nullptr;
		class QLineEdit *m_cameraHostLe = nullptr;
		class QLineEdit *m_cameraPortLe = nullptr;
		class QLineEdit *m_openLpHostLe = nullptr;
		class QLineEdit *m_openLpPortLe = nullptr;
		class QLineEdit *m_obsHostLe = nullptr;
		class QLineEdit *m_obsPortLe = nullptr;
		class QSpinBox *m_vidBrightness = nullptr;
		class QSpinBox *m_vidSaturation = nullptr;
		class QSpinBox *m_vidContrast = nullptr;
		class QSpinBox *m_vidSharpness = nullptr;
		class QSpinBox *m_vidHue = nullptr;
		int m_vidCurrentPreset = 0;
		class QTableWidget *m_viewTable = nullptr;
	public:
		explicit SettingsDialog(QWidget *parent);
	private:
		QWidget *setupNetworkInputs(QWidget *parent);
		QWidget *setupViewConfig(QWidget *parent);
		QWidget *setupImageConfig(QWidget *parent);
		QWidget *setupButtons(QWidget *parent);
		void handleApply();
		void handleOK();
		void setupViewRow(int row, View const&view = {});
		/**
		 * Gets views from table.
		 * @return error code
		 */
		[[nodiscard("Must check error code")]]
		int collectViews(QVector<View> &views) const;
		void collectVideoConfig();
		void updateVidConfigPreset(int preset);
		void updateVidConfigPresetCollect(int preset);
	signals:
		void previewPreset(int, VideoConfig const&);
};
