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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2015 Kévin Dietrich.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#pragma once

#include <ego/texture.h>

#include "volumebase.h"

class Volume : public VolumeBase {
	ego::Texture3D::Ptr m_volume_texture;
	ego::Texture1D::Ptr m_transfer_texture;

	int m_num_slices;

	int m_axis;
	float m_value_scale; // scale of the values contained in the grid (1 / (max - min))
	bool m_use_lut;
	char m_num_textures;

	void loadTransferFunction();
	void loadVolumeShader();

public:
	Volume();
	explicit Volume(openvdb::GridBase::Ptr grid);
	~Volume() = default;

	void setGrid(openvdb::GridBase::Ptr grid);

	void slice(const glm::vec3 &view_dir);
	void render(ViewerContext *context, const bool for_outline) override;
	void setCustomUIParams(ParamCallback *cb) override;

	void numSlices(int x);
	void useLUT(bool b);

	int type() const { return VOLUME; }

	static void registerSelf(ObjectFactory *factory);
};