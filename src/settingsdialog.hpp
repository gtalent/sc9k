/*
 * Copyright 2021 - 2026 gary@drinkingtea.net
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
		QVector<CameraPreset> m_cameraPresets = QVector<CameraPreset>(MaxCameraPresets);
		class QLabel *m_errLbl = nullptr;
		class QPushButton *m_okBtn = nullptr;
		class QPushButton *m_applyBtn = nullptr;
		class QLineEdit *m_cameraHostLe = nullptr;
		class QLineEdit *m_cameraPortLe = nullptr;
		class QLineEdit *m_openLpHostLe = nullptr;
		class QLineEdit *m_openLpPortLe = nullptr;
		class QLineEdit *m_obsHostLe = nullptr;
		class QLineEdit *m_obsPortLe = nullptr;
		class QLineEdit *m_obsPasswordLe = nullptr;
		class QSpinBox *m_vidBrightness = nullptr;
		class QSpinBox *m_vidSaturation = nullptr;
		class QSpinBox *m_vidContrast = nullptr;
		class QSpinBox *m_vidSharpness = nullptr;
		class QSpinBox *m_vidHue = nullptr;
		class QCheckBox *m_usePtz = nullptr;
		class QSpinBox *m_pan = nullptr;
		class QSpinBox *m_tilt = nullptr;
		class QSpinBox *m_zoom = nullptr;
		class QCheckBox *m_manualFocus = nullptr;
		class QSpinBox *m_focus = nullptr;
		class QPushButton *m_getPtzBtn = nullptr;
		bool m_cameraConnected = false;
		bool m_ptzQueryPending = false;
		class QSpinBox *m_panTiltSpeed = nullptr;
		class QSpinBox *m_zoomSpeed = nullptr;
		int m_vidCurrentPreset = 0;
		class QTableWidget *m_viewTable = nullptr;
	public:
		explicit SettingsDialog(QWidget *parent);

		void setCameraConnected(bool connected);

		/**
		 * @param focus -1 if unknown
		 */
		void setPtzPosition(int pan, int tilt, int zoom, int focus);

		void ptzPositionQueryFailed();
	private:
		QWidget *setupNetworkInputs(QWidget *parent);
		QWidget *setupViewConfig(QWidget *parent);
		QWidget *setupCameraPresetConfig(QWidget *parent);
		QWidget *setupButtons(QWidget *parent);
        [[nodiscard]]
        int handleApply();
		void handleOK();
		void setDirty(bool dirty);
		/**
		 * Shows the error, with the full message in a tool tip in case the label is cut off.
		 */
		void setError(QString const &msg) const;
		void setupViewRow(int row, View const &view = {});
		/**
		 * Gets views from table.
		 * @return error code
		 */
		[[nodiscard("Must check error code")]]
		int collectViews(QVector<View> &views) const;
		void collectCameraPreset();
		void updateCameraPreset(int preset);
		void updateCameraPresetCollect(int preset);
		void updatePtzEnabled();
		void updateGetPtzEnabled();
	signals:
		void previewPreset(int, CameraPreset const&);

		void requestPtzPosition();

		void settingsApplied();
};
