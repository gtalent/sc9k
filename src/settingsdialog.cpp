/*
 * Copyright 2021 - 2023 gary@drinkingtea.net
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QCheckBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpacerItem>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "settingsdialog.hpp"

constexpr auto MaxPresets = 9;
constexpr auto MaxViews = 9;

enum ViewColumn {
	Name = 0,
	Slides,
	ObsSlides,
	CameraPreset,
	Count
};

SettingsDialog::SettingsDialog(QWidget *parent): QDialog(parent) {
	const auto lyt = new QVBoxLayout(this);
	const auto tabs = new QTabWidget(this);
	lyt->addWidget(tabs);
	tabs->addTab(setupViewConfig(tabs), tr("&Views"));
	tabs->addTab(setupNetworkInputs(tabs), tr("&Network"));
	lyt->addWidget(setupButtons(this));
	setFixedSize(440, 440);
}

QWidget *SettingsDialog::setupNetworkInputs(QWidget *parent) {
	const auto root = new QWidget(parent);
	const auto lyt = new QFormLayout(root);
	const auto portValidator = new QIntValidator(1, 65536, this);
	QSettings settings;
	// camera settings
	{
		const auto c = getCameraConnectionData(settings);
		m_cameraIpLe = new QLineEdit(root);
		m_cameraPortLe = new QLineEdit(root);
		m_cameraIpLe->setText(c.host);
		m_cameraPortLe->setText(QString::number(c.port));
		m_cameraPortLe->setValidator(portValidator);
		lyt->addRow(tr("C&amera IP Address:"), m_cameraIpLe);
		lyt->addRow(tr("Ca&mera Port:"), m_cameraPortLe);
	}
	// OpenLP settings
	{
		const auto c = getOpenLPConnectionData(settings);
		m_openLpIpLe = new QLineEdit(root);
		m_openLpPortLe = new QLineEdit(root);
		m_openLpIpLe->setText(c.host);
		m_openLpPortLe->setText(QString::number(c.port));
		m_openLpPortLe->setValidator(portValidator);
		lyt->addRow(tr("Op&enLP IP Address:"), m_openLpIpLe);
		lyt->addRow(tr("Open&LP Port:"), m_openLpPortLe);
	}
	// OBS settings
	{
		const auto c = getOBSConnectionData(settings);
		m_obsIpLe = new QLineEdit(root);
		m_obsPortLe = new QLineEdit(root);
		m_obsIpLe->setText(c.host);
		m_obsPortLe->setText(QString::number(c.port));
		m_obsPortLe->setValidator(portValidator);
		lyt->addRow(tr("O&BS IP Address:"), m_obsIpLe);
		lyt->addRow(tr("OB&S Port:"), m_obsPortLe);
	}
	return root;
}

QWidget *SettingsDialog::setupViewConfig(QWidget *parent) {
	auto const root = new QWidget(parent);
	auto const lyt = new QVBoxLayout(root);
	// table
	m_viewTable = new QTableWidget(parent);
	{
		lyt->addWidget(m_viewTable);
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
		m_viewTable->setColumnWidth(2, 70);
		m_viewTable->setColumnWidth(3, 70);
		hdr->setStretchLastSection(true);
	}
	// add/removes buttons
	{
		auto const btnsRoot = new QWidget(root);
		auto const btnsLyt = new QHBoxLayout(btnsRoot);
		auto const addBtn = new QPushButton("+", btnsRoot);
		auto const rmBtn = new QPushButton("-", btnsRoot);
		addBtn->setFixedWidth(20);
		rmBtn->setFixedWidth(20);
		rmBtn->setDisabled(true);
		lyt->addWidget(btnsRoot);
		btnsLyt->addWidget(addBtn);
		btnsLyt->addWidget(rmBtn);
		btnsLyt->addSpacerItem(new QSpacerItem(1000, 0, QSizePolicy::Expanding, QSizePolicy::Ignored));
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
		const auto views = getViews();
		m_viewTable->setRowCount(static_cast<int>(views.size()));
		for (auto row = 0; auto const&view : views) {
			setupViewRow(row, view);
			++row;
		}
	}
	return root;
}

QWidget *SettingsDialog::setupButtons(QWidget *parent) {
	const auto root = new QWidget(parent);
	const auto lyt = new QHBoxLayout(root);
	m_errLbl = new QLabel(root);
	const auto okBtn = new QPushButton(tr("&OK"), root);
	const auto cancelBtn = new QPushButton(tr("&Cancel"), root);
	lyt->addWidget(m_errLbl);
	lyt->addSpacerItem(new QSpacerItem(1000, 0, QSizePolicy::Expanding, QSizePolicy::Ignored));
	lyt->addWidget(okBtn);
	lyt->addWidget(cancelBtn);
	connect(okBtn, &QPushButton::clicked, this, &SettingsDialog::handleOK);
	connect(cancelBtn, &QPushButton::clicked, this, &SettingsDialog::reject);
	return root;
}

void SettingsDialog::handleOK() {
	QSettings settings;
	QVector<View> views;
	auto const viewsErr = collectViews(views);
	if (viewsErr) {
		return;
	}
	setViews(settings, views);
	setCameraConnectionData(settings, {
		.host = m_cameraIpLe->text(),
		.port = m_cameraPortLe->text().toUShort(),
	});
	setOpenLPConnectionData(settings, {
		.host = m_openLpIpLe->text(),
		.port = m_openLpPortLe->text().toUShort(),
	});
	setOBSConnectionData(settings, {
		.host = m_obsIpLe->text(),
		.port = m_obsPortLe->text().toUShort(),
	});
	accept();
}

void SettingsDialog::setupViewRow(int row, View const&view) {
	// name
	const auto nameItem = new QTableWidgetItem(view.name);
	m_viewTable->setItem(row, ViewColumn::Name, nameItem);
	// slides
	const auto slidesCb = new QCheckBox(m_viewTable);
	slidesCb->setChecked(view.slides);
	m_viewTable->setCellWidget(row, ViewColumn::Slides, slidesCb);
	// obs slides
	const auto obsSlidesCb = new QCheckBox(m_viewTable);
	obsSlidesCb->setChecked(view.obsSlides);
	m_viewTable->setCellWidget(row, ViewColumn::ObsSlides, obsSlidesCb);
	// camera preset
	const auto presetItem = new QTableWidgetItem(QString::number(view.cameraPreset));
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
		const auto cameraPreset = m_viewTable->item(row, ViewColumn::CameraPreset)->text().toInt(&ok);
		if (!ok || cameraPreset < 1 || cameraPreset > MaxPresets) {
			m_errLbl->setText(tr("View %1 has invalid preset (1-%2)").arg(viewNo).arg(MaxPresets));
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