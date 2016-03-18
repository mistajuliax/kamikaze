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
 * The Original Code is Copyright (C) 2016 Kévin Dietrich.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#pragma once

#include "undo.h"

#include <QString>

class Object;
class Scene;

class AddObjectCmd : public Command {
	Object *m_object = nullptr;
	Scene *m_scene = nullptr;
	QString m_name = "";

	/* TODO */
	bool m_was_undone = false;

public:
	AddObjectCmd() = default;
	AddObjectCmd(const QString &name);
	AddObjectCmd(Scene *scene);
	~AddObjectCmd();

	void execute(EvaluationContext *context);
	void undo();
	void redo();
	void setUIParams(ParamCallback *cb);
	static Command *registerSelf();
};

class LoadFromFileCmd : public Command {
	Scene *m_scene;
	Object *m_object;
	QString m_filename;

	/* TODO */
	bool m_was_undone;

public:
	LoadFromFileCmd(Scene *scene, const QString &filename);
	~LoadFromFileCmd();

	void execute(EvaluationContext *context);
	void undo();
	void redo();
	void setUIParams(ParamCallback *cb) { (void)cb; }
};
