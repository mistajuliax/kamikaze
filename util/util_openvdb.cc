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

#define DWREAL_IS_DOUBLE 0
#include <openvdb/openvdb.h>
#include <openvdb/tools/GridTransformer.h>
#include <openvdb/util/PagedArray.h>

#include "util_openvdb.h"
#include "utils.h"

using openvdb::math::Coord;

void convert_grid(const openvdb::FloatGrid &grid, float *data, const Coord &min, const Coord &max, float &scale)
{
	Timer(__func__);

	using namespace openvdb;

	FloatGrid::ConstAccessor main_acc = grid.getAccessor();
	auto extent = max - min;
	auto slabsize = extent[0] * extent[1];
	util::PagedArray<float> min_array, max_array;

	tbb::parallel_for(tbb::blocked_range<int>(min[2], max[2]),
	        [&](const tbb::blocked_range<int> &r)
	{
		FloatGrid::ConstAccessor acc(main_acc);
		Coord ijk;
		int &x = ijk[0], &y = ijk[1], &z = ijk[2];
		z = r.begin();

		auto min_value = std::numeric_limits<float>::max();
		auto max_value = std::numeric_limits<float>::min();

		/* Subtract min z coord so that 'index' always start at zero or above. */
		auto index = (z - min[2]) * slabsize;

		for (auto e = r.end(); z < e; ++z) {
			for (y = min[1]; y < max[1]; ++y) {
				for (x = min[0]; x < max[0]; ++x, ++index) {
					auto value = acc.getValue(ijk);

					if (value < min_value) {
						min_value = value;
					}
					else if (value > max_value) {
						max_value = value;
					}

					data[index] = value;
				}
			}
		}

		min_array.push_back(min_value);
		max_array.push_back(max_value);
	});

	auto min_value = std::min_element(min_array.begin(), min_array.end());
	auto max_value = std::max_element(max_array.begin(), max_array.end());
	scale = 1.0f / (*max_value - *min_value);
}

openvdb::FloatGrid::Ptr transform_grid(const openvdb::FloatGrid &grid,
                                       const openvdb::Vec3s &rot,
                                       const openvdb::Vec3s &scale,
                                       const openvdb::Vec3s &translate,
                                       const openvdb::Vec3s &pivot)
{
	/* make sure the new grid has the same transform and metadatas as the old. */
	openvdb::FloatGrid::Ptr xformed = grid.copy(openvdb::CopyPolicy::CP_NEW);

	openvdb::Mat4R mat(openvdb::Mat4R::identity());
	mat.preTranslate(pivot);
	mat.preRotate(openvdb::math::X_AXIS, rot[0]);
	mat.preRotate(openvdb::math::Y_AXIS, rot[1]);
	mat.preRotate(openvdb::math::Z_AXIS, rot[2]);
	mat.preScale(scale);
	mat.preTranslate(-pivot);
	mat.preTranslate(translate);

	openvdb::tools::GridTransformer transformer(mat);
	transformer.transformGrid<openvdb::tools::PointSampler>(grid, *xformed);
	openvdb::tools::prune(xformed->tree());

	return xformed;
}