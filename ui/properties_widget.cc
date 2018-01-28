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

#include "properties_widget.h"

#include <iostream>

#include <QFrame>
#include <QHBoxLayout>
#include <QScrollArea>

#include <kamikaze/context.h>
#include <kamikaze/noeud.h>
#include <kamikaze/operateur.h>

#include <kangao/kangao.h>

#include "utils_ui.h"

#include "core/graphs/object_graph.h"
#include "core/object.h"
#include "core/scene.h"

#include "util/utils.h"

PropertiesWidget::PropertiesWidget(QWidget *parent)
    : WidgetBase(parent)
    , m_widget(new QWidget())
    , m_scroll(new QScrollArea())
    , m_glayout(new QGridLayout(m_widget))
{
	m_widget->setSizePolicy(m_frame->sizePolicy());

	m_scroll->setWidget(m_widget);
	m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_scroll->setWidgetResizable(true);

	/* Hide scroll area's frame. */
	m_scroll->setFrameStyle(0);

	m_main_layout->addWidget(m_scroll);
}

void PropertiesWidget::update_state(event_type event)
{
	kangao::Manipulable *manipulable = nullptr;
	bool set_context = true;
	auto scene = m_context->scene;
	const char *chemin_interface = "";

	if (scene->active_node() == nullptr) {
		return;
	}

	const auto &event_category = get_category(event);
	const auto &event_action = get_action(event);

	std::vector<std::string> warnings;

	if (event_category == event_type::object) {
		if (is_elem(event_action, event_type::added, event_type::selected)) {
			manipulable = scene->active_node();
			chemin_interface = "interface/proprietes_objet.kangao";
		}
		else if (is_elem(event_action, event_type::removed)) {
			efface_disposition();
			return;
		}
	}
	else if (event_category == (event_type::node)) {
		if (is_elem(event_action, event_type::selected, event_type::processed)) {
			auto scene_node = scene->active_node();
			auto object = static_cast<Object *>(scene_node);
			auto graph = object->graph();
			auto noeud = graph->noeud_actif();

			if (noeud == nullptr) {
				return;
			}

			auto operateur = noeud->operateur();
			manipulable = operateur;
			chemin_interface = operateur->chemin_interface();
			warnings = operateur->avertissements();

			/* Only update/evaluate the graph if the node is connected. */
			set_context = noeud->est_connecte();
		}
		else if (is_elem(event_action, event_type::removed)) {
			efface_disposition();
			return;
		}
	}
	else {
		return;
	}

	if (manipulable == nullptr) {
		return;
	}

	efface_disposition();

	/* À FAIRE : affiche avertissements */

	/* À FAIRE : set_context */
	dessine_interface(manipulable, chemin_interface);
}

void PropertiesWidget::dessine_interface(kangao::Manipulable *manipulable, const char *chemin_interface)
{
	manipulable->ajourne_proprietes();

	const auto &texte = kangao::contenu_fichier(chemin_interface);

	if (texte.empty()) {
		return;
	}

	kangao::DonneesInterface donnees;
	donnees.manipulable = manipulable;
	donnees.conteneur = this;

	auto disposition = kangao::compile_interface(donnees, texte.c_str());

	m_conteneur_disposition->setLayout(disposition);
}

void PropertiesWidget::ajourne_manipulable()
{
	std::cerr << "PropertiesWidget::ajourne_manipulable";
	m_manipulable->ajourne_proprietes();
}

void PropertiesWidget::efface_disposition()
{
	delete m_conteneur_disposition->layout();
}

void PropertiesWidget::evalObjectGraph()
{
	this->set_active();
	auto scene = m_context->scene;
	auto scene_node = scene->active_node();
	auto object = static_cast<Object *>(scene_node);
	auto graph = object->graph();
	auto noeud = graph->noeud_actif();

	signifie_sale_aval(noeud);

	scene->evalObjectDag(*m_context, scene_node);
	scene->notify_listeners(static_cast<event_type>(-1));
}

void PropertiesWidget::tagObjectUpdate()
{
	std::cerr << __func__ << "\n";
	this->set_active();
	m_context->scene->tagObjectUpdate();
}

void PropertiesWidget::updateProperties()
{
#if 0
	auto scene = m_context->scene;
	auto scene_node = scene->active_node();

	if (!scene_node) {
		return;
	}

	Persona *persona = nullptr;

	if (m_context->eval_ctx->edit_mode) {
		auto graph = static_cast<Object *>(scene_node)->graph();
		auto noeud = graph->noeud_actif();

		if (noeud == nullptr) {
			return;
		}

		persona = noeud->operateur();
	}
	else {
		persona = scene_node;
	}

	if (persona->update_properties()) {
		for (Property &prop : persona->props()) {
			m_callback.setVisible(prop.ui_name.c_str(), prop.visible);
		}
	}
#endif
}

void PropertiesWidget::drawProperties(Persona *persona, bool set_context)
{
#if 0
	persona->update_properties();

	for (Property &prop : persona->props()) {
		assert(!prop.data.empty());

		switch (prop.type) {
			case property_type::prop_bool:
				bool_param(m_callback,
				           prop.ui_name.c_str(),
						   std::experimental::any_cast<bool>(&prop.data),
						   std::experimental::any_cast<bool>(prop.data));
				break;
			case property_type::prop_float:
				float_param(m_callback,
				            prop.ui_name.c_str(),
							std::experimental::any_cast<float>(&prop.data),
				            prop.min, prop.max,
							std::experimental::any_cast<float>(prop.data));
				break;
			case property_type::prop_int:
				int_param(m_callback,
				          prop.ui_name.c_str(),
						  std::experimental::any_cast<int>(&prop.data),
				          prop.min, prop.max,
						  std::experimental::any_cast<int>(prop.data));
				break;
			case property_type::prop_enum:
				enum_param(m_callback,
				           prop.ui_name.c_str(),
						   std::experimental::any_cast<int>(&prop.data),
				           prop.enum_items,
						   std::experimental::any_cast<int>(prop.data));
				break;
			case property_type::prop_vec3:
				xyz_param(m_callback,
				          prop.ui_name.c_str(),
						  &(std::experimental::any_cast<glm::vec3>(&prop.data)->x),
				          prop.min, prop.max);
				break;
			case property_type::prop_input_file:
				input_file_param(m_callback,
				                 prop.ui_name.c_str(),
								 std::experimental::any_cast<std::string>(&prop.data));
				break;
			case property_type::prop_output_file:
				output_file_param(m_callback,
				                  prop.ui_name.c_str(),
								  std::experimental::any_cast<std::string>(&prop.data));
				break;
			case property_type::prop_string:
				string_param(m_callback,
				             prop.ui_name.c_str(),
							 std::experimental::any_cast<std::string>(&prop.data),
							 std::experimental::any_cast<std::string>(prop.data).c_str());
				break;
			case property_type::prop_list:
				list_selection_param(m_callback,
				                     prop.ui_name.c_str(),
				                     prop.enum_items,
									 std::experimental::any_cast<std::string>(&prop.data));
				break;
		}

		if (!prop.tooltip.empty()) {
			param_tooltip(m_callback, prop.tooltip.c_str());
		}

		m_callback.setVisible(prop.ui_name.c_str(), prop.visible);
	}

	if (set_context) {
		if (m_context->eval_ctx->edit_mode) {
			m_callback.setContext(this, SLOT(evalObjectGraph()));
		}
		else {
			m_callback.setContext(this, SLOT(tagObjectUpdate()));
		}
	}

	m_callback.setContext(this, SLOT(updateProperties()));
#endif
}
