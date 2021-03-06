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

#include "utils_ui.h"

#include <QLayout>
#include <QListWidget>

void disable_list_item(QListWidget *list, int index)
{
	auto item = list->item(index);
	item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
}

void enable_list_item(QListWidget *list, int index)
{
	auto item = list->item(index);
	item->setFlags(item->flags() | Qt::ItemIsEnabled);
}

void clear_layout(QLayout *layout)
{
	QLayoutItem *item;

	while ((item = layout->takeAt(0)) != nullptr) {
		if (item->layout()) {
			clear_layout(item->layout());
			delete item->layout();
		}

		if (item->widget()) {
			delete item->widget();
		}

		delete item;
	}
}
