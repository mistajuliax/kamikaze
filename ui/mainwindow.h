/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2015 Kévin Dietrich.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#pragma once

#include <QMainWindow>

#include "objects/undo.h"

namespace Ui {
class MainWindow;
}

class Command;
class CommandManager;
class ObjectFactory;
class QComboBox;
class QListWidget;
class QTimer;
class Scene;

class MainWindow : public QMainWindow {
	Q_OBJECT

	Ui::MainWindow *ui;
	Scene *m_scene;
	QTimer *m_timer;
	CommandManager *m_command_manager;
	CommandFactory *m_command_factory;
	bool m_timer_has_started;
	QComboBox *m_scene_mode_box;
	QListWidget *m_scene_mode_list;

	ObjectFactory *m_object_factory;

	/* TODO: de-duplicate this from undo.h */
	typedef Command *(*command_factory_func)(void);

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();

	void openFile(const QString &filename) const;

protected:
	bool eventFilter(QObject *obj, QEvent *e);

private:
	void registerCommandType(const char *name, CommandFactory::factory_func func);
	void registerObjectType();
	void generateObjectMenu();

private Q_SLOTS:
	void openFile();
	void updateObjectTab() const;
	void startAnimation();
	void updateFrame() const;
	void setStartFrame(int value) const;
	void setEndFrame(int value) const;
	void setSceneMode(int idx) const;
	void goToStartFrame() const;
	void goToEndFrame() const;
	void undo() const;
	void redo() const;
	void handleCommand();
	void handleObjectCommand();
};
