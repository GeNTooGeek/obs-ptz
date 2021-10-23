/* Pan Tilt Zoom settings window
 *
 * Copyright 2020 Grant Likely <grant.likely@secretlab.ca>
 *
 * SPDX-License-Identifier: GPLv2
 */
#pragma once

#include <QWidget>
#include <QString>
#include <imported/properties-view.hpp>

class Ui_PTZSettings;

class PTZSettings : public QWidget {
	Q_OBJECT

private:
	Ui_PTZSettings *ui;
	OBSData settings;
	OBSPropertiesView *propertiesView = nullptr;
	void current_device_changed();

public:
	PTZSettings();
	~PTZSettings();
	void set_selected(uint32_t device_id);

public slots:
	void on_close_clicked();

	void on_addPTZ_clicked();
	void on_removePTZ_clicked();
	void on_applyButton_clicked();
	void on_gamepadCheckBox_stateChanged(int state);

	void currentChanged(const QModelIndex &current, const QModelIndex &previous);
	obs_properties_t *getProperties(void);
	void updateProperties(OBSData old_settings, OBSData new_settings);
};

void ptz_settings_show(uint32_t device_id = 0);
extern "C" void ptz_init_settings();
