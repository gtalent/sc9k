/*
 * Copyright 2021 - 2026 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpacerItem>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "consts.hpp"
#include "settingsdialog.hpp"

enum ViewColumn {
	Name = 0,
	Slides,
	ObsSlides,
	CameraPreset,
	Count
};

SettingsDialog::SettingsDialog(QWidget *parent): QDialog(parent) {
	auto const lyt = new QVBoxLayout(this);
	auto const tabs = new QTabWidget(this);
	lyt->addWidget(tabs);
	tabs->addTab(setupViewConfig(tabs), tr("&Views"));
	tabs->addTab(setupCameraPresetConfig(tabs), tr("Ca&mera Presets"));
	tabs->addTab(setupNetworkInputs(tabs), tr("&Network"));
	lyt->addWidget(setupButtons(this));
	setFixedSize(480, 540);
}

QWidget *SettingsDialog::setupNetworkInputs(QWidget *parent) {
	auto const root = new QWidget(parent);
	auto const lyt = new QFormLayout(root);
	auto const portValidator = new QIntValidator(1, 65536, this);
	QSettings settings;
	// camera settings
	{
		auto const c = getCameraConnectionData(settings);
		m_cameraHostLe = new QLineEdit(root);
		m_cameraPortLe = new QLineEdit(root);
		m_cameraHostLe->setText(c.host);
		m_cameraPortLe->setText(QString::number(c.port));
		m_cameraPortLe->setValidator(portValidator);
		lyt->addRow(tr("Camera &Host:"), m_cameraHostLe);
		lyt->addRow(tr("Ca&mera Port:"), m_cameraPortLe);
	}
	// OpenLP settings
	{
		auto const c = getOpenLPConnectionData(settings);
		m_openLpHostLe = new QLineEdit(root);
		m_openLpPortLe = new QLineEdit(root);
		m_openLpHostLe->setText(c.host);
		m_openLpPortLe->setText(QString::number(c.port));
		m_openLpPortLe->setValidator(portValidator);
		lyt->addRow(tr("Op&enLP Host:"), m_openLpHostLe);
		lyt->addRow(tr("Open&LP Port:"), m_openLpPortLe);
	}
	// OBS settings
	{
		auto const c = getOBSConnectionData(settings);
		m_obsHostLe = new QLineEdit(root);
		m_obsPortLe = new QLineEdit(root);
		m_obsPasswordLe = new QLineEdit(root);
		m_obsPasswordLe->setEchoMode(QLineEdit::Password);
		m_obsHostLe->setText(c.host);
		m_obsPortLe->setText(QString::number(c.port));
		m_obsPasswordLe->setText(c.password);
		m_obsPortLe->setValidator(portValidator);
		lyt->addRow(tr("O&BS Host:"), m_obsHostLe);
		lyt->addRow(tr("OB&S Port:"), m_obsPortLe);
		lyt->addRow(tr("OBS &Password:"), m_obsPasswordLe);
	}
	for (auto const le : {
			m_cameraHostLe, m_cameraPortLe,
			m_openLpHostLe, m_openLpPortLe,
			m_obsHostLe, m_obsPortLe, m_obsPasswordLe}) {
		connect(le, &QLineEdit::textEdited, this, [this] { setDirty(true); });
	}
	return root;
}

QWidget *SettingsDialog::setupCameraPresetConfig(QWidget *parent) {
	auto const root = new QWidget(parent);
	auto const lyt = new QVBoxLayout(root);
	m_cameraPresets = getCameraPresets();
	auto const mkSb = [this](QWidget *parent, QFormLayout *formLyt, QString const &lbl, int min, int max) {
		auto const s = new QSpinBox(parent);
		s->setAlignment(Qt::AlignRight);
		s->setRange(min, max);
		formLyt->addRow(lbl, s);
		connect(s, &QSpinBox::valueChanged, this, [this] { setDirty(true); });
		return s;
	};
	auto constexpr mkForm = [](QWidget *parent) {
		auto const formLyt = new QFormLayout(parent);
		// size hints vary with spin box range, so stretch fields for consistent widths
		formLyt->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		return formLyt;
	};
	auto const presetNo = new QComboBox(root);
	lyt->addWidget(presetNo);
	{
		auto const presetRoot = new QWidget(root);
		auto const presetLyt = new QHBoxLayout(presetRoot);
		presetLyt->setContentsMargins(0, 0, 0, 0);
		lyt->addWidget(presetRoot);
		// image
		{
			auto const grp = new QGroupBox(tr("Image"), presetRoot);
			auto const formLyt = mkForm(grp);
			presetLyt->addWidget(grp);
			m_vidBrightness = mkSb(grp, formLyt, tr("&Brightness:"), 0, 14);
			m_vidSaturation = mkSb(grp, formLyt, tr("&Saturation:"), 0, 14);
			m_vidContrast = mkSb(grp, formLyt, tr("Con&trast:"), 0, 14);
			m_vidSharpness = mkSb(grp, formLyt, tr("Sharpn&ess:"), 0, 14);
			m_vidHue = mkSb(grp, formLyt, tr("&Hue:"), 0, 14);
		}
		// position
		{
			auto const grp = new QGroupBox(tr("Position"), presetRoot);
			auto const formLyt = mkForm(grp);
			presetLyt->addWidget(grp);
			m_usePtz = new QCheckBox(tr("&Use PTZ"), grp);
			m_usePtz->setToolTip(tr("When off, the camera's own stored preset is recalled"));
			formLyt->addRow(m_usePtz);
			connect(m_usePtz, &QCheckBox::toggled, this, [this] {
				updatePtzEnabled();
				setDirty(true);
			});
			// 5 nibble pan on some cameras, 4 nibble tilt and zoom
			m_pan = mkSb(grp, formLyt, tr("Pan:"), -0x80000, 0x7FFFF);
			m_tilt = mkSb(grp, formLyt, tr("T&ilt:"), -0x8000, 0x7FFF);
			m_zoom = mkSb(grp, formLyt, tr("&Zoom:"), 0, 0xFFFF);
			m_getPtzBtn = new QPushButton(tr("&Get Current"), grp);
			m_getPtzBtn->setToolTip(tr("Fill in the camera's current position"));
			formLyt->addRow(m_getPtzBtn);
			connect(m_getPtzBtn, &QPushButton::clicked, this, [this] {
				m_getPtzBtn->setEnabled(false);
				m_errLbl->clear();
				emit requestPtzPosition();
			});
		}
	}
	for (auto i = 0; i < MaxCameraPresets; ++i) {
		presetNo->addItem(tr("Camera Preset %1").arg(i + 1));
	}
	updateCameraPreset(0);
	// connect after populating, otherwise the initial selection would collect the empty fields
	connect(presetNo, &QComboBox::currentIndexChanged, this, &SettingsDialog::updateCameraPresetCollect);
	// transition speed, shared by all presets
	{
		auto const grp = new QGroupBox(tr("Transition Speed"), root);
		auto const formLyt = mkForm(grp);
		lyt->addWidget(grp);
		auto const speed = getPtzSpeed();
		m_panTiltSpeed = mkSb(grp, formLyt, tr("Pan/Tilt Spee&d:"), 1, 24);
		m_zoomSpeed = mkSb(grp, formLyt, tr("Zoom Speed:"), 0, 8);
		m_zoomSpeed->setSpecialValueText(tr("Camera Default"));
		QSignalBlocker const ptBlocker(m_panTiltSpeed);
		QSignalBlocker const zBlocker(m_zoomSpeed);
		m_panTiltSpeed->setValue(speed.panTilt);
		m_zoomSpeed->setValue(speed.zoom);
	}
	{
		auto const btnRoot = new QWidget(root);
		auto const btnLyt = new QHBoxLayout(btnRoot);
		lyt->addWidget(btnRoot);
		btnLyt->setAlignment(Qt::AlignRight);
		auto const previewBtn = new QPushButton(tr("&Preview"), btnRoot);
		btnLyt->addWidget(previewBtn);
		connect(previewBtn, &QPushButton::clicked, this, [this] {
			this->collectCameraPreset();
			auto const &cp = m_cameraPresets[m_vidCurrentPreset];
			emit previewPreset(m_vidCurrentPreset + 1, cp);
		});
	}
	return root;
}

void SettingsDialog::setPtzPosition(int const pan, int const tilt, int const zoom) {
	m_getPtzBtn->setEnabled(true);
	m_usePtz->setChecked(true);
	m_pan->setValue(pan);
	m_tilt->setValue(tilt);
	m_zoom->setValue(zoom);
}

void SettingsDialog::ptzPositionQueryFailed() {
	m_getPtzBtn->setEnabled(true);
	m_errLbl->setText(tr("Camera did not report its position."));
}

QWidget *SettingsDialog::setupViewConfig(QWidget *parent) {
	auto const root = new QWidget(parent);
	auto const lyt = new QVBoxLayout(root);
	auto const btnsRoot = new QWidget(root);
	m_viewTable = new QTableWidget(root);
	lyt->addWidget(btnsRoot);
	lyt->addWidget(m_viewTable);
	{ // table
		QStringList columns;
		columns.resize(ViewColumn::Count);
		columns[ViewColumn::Name] = tr("Name");
		columns[ViewColumn::Slides] = tr("Slides");
		columns[ViewColumn::ObsSlides] = tr("OBS Slides");
		columns[ViewColumn::CameraPreset] = tr("Camera Preset");
		m_viewTable->setColumnCount(static_cast<int>(columns.size()));
		m_viewTable->setHorizontalHeaderLabels(columns);
		m_viewTable->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
		m_viewTable->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
		auto const hdr = m_viewTable->horizontalHeader();
		m_viewTable->setColumnWidth(1, 70);
		m_viewTable->setColumnWidth(2, 75);
		m_viewTable->setColumnWidth(3, 70);
		hdr->setStretchLastSection(true);
	}
	{ // add/removes buttons
		auto const btnsLyt = new QHBoxLayout(btnsRoot);
		auto const addBtn = new QPushButton("A&dd", btnsRoot);
		auto const rmBtn = new QPushButton("&Remove", btnsRoot);
		addBtn->setFixedWidth(70);
		rmBtn->setFixedWidth(70);
		rmBtn->setDisabled(true);
		btnsLyt->addWidget(addBtn);
		btnsLyt->addWidget(rmBtn);
		btnsLyt->setAlignment(Qt::AlignLeft);
		connect(addBtn, &QPushButton::clicked, this, [this, addBtn] {
			auto const row = m_viewTable->rowCount();
			m_viewTable->setRowCount(row + 1);
			setupViewRow(row);
			addBtn->setEnabled(m_viewTable->rowCount() < MaxViews);
			setDirty(true);
		});
		connect(rmBtn, &QPushButton::clicked, this, [this, addBtn] {
			auto const row = m_viewTable->currentRow();
			m_viewTable->removeRow(row);
			addBtn->setEnabled(m_viewTable->rowCount() < MaxViews);
			setDirty(true);
		});
		connect(m_viewTable, &QTableWidget::currentCellChanged, rmBtn, [this, addBtn, rmBtn] (int row) {
			rmBtn->setEnabled(row > -1 && row < m_viewTable->rowCount());
			addBtn->setEnabled(m_viewTable->rowCount() < MaxViews);
		});
		auto const views = getViews();
		m_viewTable->setRowCount(static_cast<int>(views.size()));
		for (auto row = 0; auto const &view : views) {
			setupViewRow(row, view);
			++row;
		}
		// connect after populating so the initial load doesn't count as a change
		connect(m_viewTable, &QTableWidget::itemChanged, this, [this] { setDirty(true); });
	}
	return root;
}

QWidget *SettingsDialog::setupButtons(QWidget *parent) {
	auto const root = new QWidget(parent);
	auto const lyt = new QHBoxLayout(root);
	m_errLbl = new QLabel(root);
	m_okBtn = new QPushButton(tr("&OK"), root);
	m_applyBtn = new QPushButton(tr("&Apply"), root);
	auto const cancelBtn = new QPushButton(tr("&Cancel"), root);
	lyt->addWidget(m_errLbl);
	lyt->addSpacerItem(new QSpacerItem(1000, 0, QSizePolicy::Expanding, QSizePolicy::Ignored));
	lyt->addWidget(m_okBtn);
	lyt->addWidget(m_applyBtn);
	lyt->addWidget(cancelBtn);
	connect(m_okBtn, &QPushButton::clicked, this, &SettingsDialog::handleOK);
	connect(m_applyBtn, &QPushButton::clicked, this, &SettingsDialog::handleApply);
	connect(cancelBtn, &QPushButton::clicked, this, &SettingsDialog::reject);
	setDirty(false);
	return root;
}

void SettingsDialog::setDirty(bool const dirty) {
	if (m_okBtn) {
		m_okBtn->setEnabled(dirty);
	}
	if (m_applyBtn) {
		m_applyBtn->setEnabled(dirty);
	}
}

int SettingsDialog::handleApply() {
	QSettings settings;
	QVector<View> views;
	auto const viewsErr = collectViews(views);
	if (viewsErr) {
        return -1;
	}
	setViews(settings, views);
	setCameraConnectionData(settings, {
		.host = m_cameraHostLe->text(),
		.port = m_cameraPortLe->text().toUShort(),
	});
	setOpenLPConnectionData(settings, {
		.host = m_openLpHostLe->text(),
		.port = m_openLpPortLe->text().toUShort(),
	});
	setOBSConnectionData(settings, {
		.host = m_obsHostLe->text(),
		.port = m_obsPortLe->text().toUShort(),
		.password = m_obsPasswordLe->text(),
	});
	collectCameraPreset();
	setCameraPresets(settings, m_cameraPresets);
	setPtzSpeed(settings, {
		.panTilt = m_panTiltSpeed->value(),
		.zoom = m_zoomSpeed->value(),
	});
	setDirty(false);
	emit settingsApplied();
    return 0;
}

void SettingsDialog::handleOK() {
    if (handleApply() == 0) {
        accept();
    }
}

void SettingsDialog::setupViewRow(int const row, View const &view) {
	// name
	auto const nameItem = new QTableWidgetItem(view.name);
	m_viewTable->setItem(row, ViewColumn::Name, nameItem);
	// slides
	auto const slidesCb = new QCheckBox(m_viewTable);
	slidesCb->setChecked(view.slides);
	connect(slidesCb, &QCheckBox::toggled, this, [this] { setDirty(true); });
	m_viewTable->setCellWidget(row, ViewColumn::Slides, slidesCb);
	// obs slides
	auto const obsSlidesCb = new QCheckBox(m_viewTable);
	obsSlidesCb->setChecked(view.obsSlides);
	connect(obsSlidesCb, &QCheckBox::toggled, this, [this] { setDirty(true); });
	m_viewTable->setCellWidget(row, ViewColumn::ObsSlides, obsSlidesCb);
	// camera preset
	auto const presetItem = new QTableWidgetItem(QString::number(view.cameraPreset));
	m_viewTable->setItem(row, ViewColumn::CameraPreset, presetItem);
}

int SettingsDialog::collectViews(QVector<View> &views) const {
	for (auto row = 0; row < m_viewTable->rowCount(); ++row) {
		auto const viewNo = row + 1;
		bool ok = false;
		auto const name = m_viewTable->item(row, ViewColumn::Name)->text();
		if (name.trimmed() == "") {
			m_errLbl->setText(tr("View %1 has no name.").arg(viewNo));
			return 1;
		}
		auto const cameraPreset = m_viewTable->item(row, ViewColumn::CameraPreset)->text().toInt(&ok);
		if (!ok || cameraPreset < 1 || cameraPreset > MaxCameraPresets) {
			m_errLbl->setText(tr("View %1 has invalid preset (1-%2)").arg(viewNo).arg(MaxCameraPresets));
			return 2;
		}
		views.emplace_back(View{
			.name = name,
			.slides = dynamic_cast<QCheckBox*>(m_viewTable->cellWidget(row, ViewColumn::Slides))->isChecked(),
			.obsSlides = dynamic_cast<QCheckBox*>(m_viewTable->cellWidget(row, ViewColumn::ObsSlides))->isChecked(),
			.cameraPreset = cameraPreset,
		});
	}
	return 0;
}

void SettingsDialog::collectCameraPreset() {
	auto &cp = m_cameraPresets[m_vidCurrentPreset];
	auto constexpr getVal = [](int &val, QSpinBox *src) {
		if (src) {
			val = src->value();
		}
	};
	getVal(cp.brightness, m_vidBrightness);
	getVal(cp.saturation, m_vidSaturation);
	getVal(cp.contrast, m_vidContrast);
	getVal(cp.sharpness, m_vidSharpness);
	getVal(cp.hue, m_vidHue);
	if (m_usePtz) {
		cp.usePtz = m_usePtz->isChecked();
	}
	getVal(cp.pan, m_pan);
	getVal(cp.tilt, m_tilt);
	getVal(cp.zoom, m_zoom);
}

void SettingsDialog::updateCameraPreset(int const preset) {
	// update to new value
	auto constexpr setVal = [](int val, QSpinBox *dst) {
		if (dst) {
			// switching presets isn't a change to the settings
			QSignalBlocker const blocker(dst);
			dst->setValue(val);
		}
	};
	auto const &cp = m_cameraPresets[preset];
	setVal(cp.brightness, m_vidBrightness);
	setVal(cp.saturation, m_vidSaturation);
	setVal(cp.contrast, m_vidContrast);
	setVal(cp.sharpness, m_vidSharpness);
	setVal(cp.hue, m_vidHue);
	if (m_usePtz) {
		QSignalBlocker const blocker(m_usePtz);
		m_usePtz->setChecked(cp.usePtz);
	}
	setVal(cp.pan, m_pan);
	setVal(cp.tilt, m_tilt);
	setVal(cp.zoom, m_zoom);
	updatePtzEnabled();
	m_vidCurrentPreset = preset;
}

void SettingsDialog::updateCameraPresetCollect(int const preset) {
	collectCameraPreset();
	updateCameraPreset(preset);
}

void SettingsDialog::updatePtzEnabled() {
	auto const enabled = m_usePtz && m_usePtz->isChecked();
	for (auto const sb : {m_pan, m_tilt, m_zoom}) {
		if (sb) {
			sb->setEnabled(enabled);
		}
	}
}
