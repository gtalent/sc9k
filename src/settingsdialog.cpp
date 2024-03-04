/*
 * Copyright 2021 - 2023 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
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
	tabs->addTab(setupImageConfig(tabs), tr("&Image"));
	tabs->addTab(setupNetworkInputs(tabs), tr("&Network"));
	lyt->addWidget(setupButtons(this));
	setFixedSize(440, 440);
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
		m_obsHostLe->setText(c.host);
		m_obsPortLe->setText(QString::number(c.port));
		m_obsPortLe->setValidator(portValidator);
		lyt->addRow(tr("O&BS Host:"), m_obsHostLe);
		lyt->addRow(tr("OB&S Port:"), m_obsPortLe);
	}
	return root;
}

QWidget *SettingsDialog::setupImageConfig(QWidget *parent) {
	auto const root = new QWidget(parent);
	auto const lyt = new QVBoxLayout(root);
	{
		auto const formRoot = new QWidget(parent);
		auto const formLyt = new QFormLayout(formRoot);
		lyt->addWidget(formRoot);
		m_videoConfig = getVideoConfig();
		auto const mkSb = [parent, formLyt](QString const&lbl) {
			auto const s = new QSpinBox(parent);
			s->setAlignment(Qt::AlignRight);
			s->setRange(0, 14);
			formLyt->addRow(lbl, s);
			return s;
		};
		auto const presetNo = new QComboBox(parent);
		connect(presetNo, &QComboBox::currentIndexChanged, this, &SettingsDialog::updateVidConfigPreset);
		for (auto i = 0; i < MaxCameraPresets; ++i) {
			presetNo->addItem(tr("Camera Preset %1").arg(i + 1));
		}
		formLyt->addRow(presetNo);
		m_vidBrightness = mkSb(tr("&Brightness:"));
		m_vidSaturation = mkSb(tr("&Saturation:"));
		m_vidContrast = mkSb(tr("Con&trast:"));
		m_vidSharpness = mkSb(tr("Sharpn&ess:"));
		m_vidHue = mkSb(tr("&Hue:"));
		updateVidConfigPreset(0);
	}
	{
		auto const btnRoot = new QWidget(parent);
		auto const btnLyt = new QHBoxLayout(btnRoot);
		lyt->addWidget(btnRoot);
		btnLyt->setAlignment(Qt::AlignRight);
		auto const previewBtn = new QPushButton(tr("&Preview"), btnRoot);
		btnLyt->addWidget(previewBtn);
		connect(previewBtn, &QPushButton::clicked, this, [this] {
			this->collectVideoConfig();
			auto const &vc = m_videoConfig[m_vidCurrentPreset];
			emit previewPreset(m_vidCurrentPreset + 1, vc);
		});
	}
	return root;
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
		});
		connect(rmBtn, &QPushButton::clicked, this, [this, addBtn] {
			auto const row = m_viewTable->currentRow();
			m_viewTable->removeRow(row);
			addBtn->setEnabled(m_viewTable->rowCount() < MaxViews);
		});
		connect(m_viewTable, &QTableWidget::currentCellChanged, rmBtn, [this, addBtn, rmBtn] (int row) {
			rmBtn->setEnabled(row > -1 && row < m_viewTable->rowCount());
			addBtn->setEnabled(m_viewTable->rowCount() < MaxViews);
		});
		auto const views = getViews();
		m_viewTable->setRowCount(static_cast<int>(views.size()));
		for (auto row = 0; auto const&view : views) {
			setupViewRow(row, view);
			++row;
		}
	}
	return root;
}

QWidget *SettingsDialog::setupButtons(QWidget *parent) {
	auto const root = new QWidget(parent);
	auto const lyt = new QHBoxLayout(root);
	m_errLbl = new QLabel(root);
	auto const okBtn = new QPushButton(tr("&OK"), root);
	auto const applyBtn = new QPushButton(tr("&Apply"), root);
	auto const cancelBtn = new QPushButton(tr("&Cancel"), root);
	lyt->addWidget(m_errLbl);
	lyt->addSpacerItem(new QSpacerItem(1000, 0, QSizePolicy::Expanding, QSizePolicy::Ignored));
	lyt->addWidget(okBtn);
	lyt->addWidget(applyBtn);
	lyt->addWidget(cancelBtn);
	connect(okBtn, &QPushButton::clicked, this, &SettingsDialog::handleOK);
	connect(applyBtn, &QPushButton::clicked, this, &SettingsDialog::handleApply);
	connect(cancelBtn, &QPushButton::clicked, this, &SettingsDialog::reject);
	return root;
}

void SettingsDialog::handleApply() {
	QSettings settings;
	QVector<View> views;
	auto const viewsErr = collectViews(views);
	if (viewsErr) {
		return;
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
	});
	collectVideoConfig();
	setVideoConfig(settings, m_videoConfig);
}

void SettingsDialog::handleOK() {
	handleApply();
	accept();
}

void SettingsDialog::setupViewRow(int row, View const&view) {
	// name
	auto const nameItem = new QTableWidgetItem(view.name);
	m_viewTable->setItem(row, ViewColumn::Name, nameItem);
	// slides
	auto const slidesCb = new QCheckBox(m_viewTable);
	slidesCb->setChecked(view.slides);
	m_viewTable->setCellWidget(row, ViewColumn::Slides, slidesCb);
	// obs slides
	auto const obsSlidesCb = new QCheckBox(m_viewTable);
	obsSlidesCb->setChecked(view.obsSlides);
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

void SettingsDialog::collectVideoConfig() {
	auto &vc = m_videoConfig[m_vidCurrentPreset];
	auto constexpr getVal = [](int &val, QSpinBox *src) {
		if (src) {
			val = src->value();
		}
	};
	getVal(vc.brightness, m_vidBrightness);
	getVal(vc.saturation, m_vidSaturation);
	getVal(vc.contrast, m_vidContrast);
	getVal(vc.sharpness, m_vidSharpness);
	getVal(vc.hue, m_vidHue);
}

void SettingsDialog::updateVidConfigPreset(int preset) {
	// update to new value
	auto constexpr setVal = [](int val, QSpinBox *dst) {
		if (dst) {
			dst->setValue(val);
		}
	};
	auto const&vc = m_videoConfig[preset];
	setVal(vc.brightness, m_vidBrightness);
	setVal(vc.saturation, m_vidSaturation);
	setVal(vc.contrast, m_vidContrast);
	setVal(vc.sharpness, m_vidSharpness);
	setVal(vc.hue, m_vidHue);
	m_vidCurrentPreset = preset;
}

void SettingsDialog::updateVidConfigPresetCollect(int preset) {
	collectVideoConfig();
	updateVidConfigPreset(preset);
}
