#include <fstream>
#include <iostream>
#include <GL/glew.h>
#include <GL/freeglut.h>

#define GLM_FORCE_RADIANS

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define DWREAL_IS_DOUBLE 0

#include <openvdb/openvdb.h>
#include <openvdb/tools/GridTransformer.h>
#include <openvdb/util/PagedArray.h>

#include "GLSLShader.h"
#include "utils.h"
#include "volume.h"

const float EPSILON = 0.0001f;

void max_leaf_per_axis(int dim[3], int voxel_per_leaf, int result[3])
{
	// x axis
	result[0] = (dim[0] - (dim[0] % voxel_per_leaf)) / voxel_per_leaf;

	// y axis
	result[1] = (dim[1] - (dim[1] % voxel_per_leaf)) / voxel_per_leaf;

	// z axis // z = num_leaf_in_tree / (x * y) + 1
	auto resolution = dim[0] * dim[1] * dim[2];
	auto difference = result[0] * result[1];

	auto temp_max = resolution / difference;

	result[2] = (temp_max - (temp_max % voxel_per_leaf)) / voxel_per_leaf;
	assert(difference * result[2] > resolution / 256);
}

void texture_from_leaf(const openvdb::FloatGrid &grid)
{
	using namespace openvdb;

	typedef FloatTree::LeafNodeType LeafType;
	typedef FloatGrid::ValueType ValueType;

	const int leafDim = LeafType::DIM;
	FloatTree::LeafCIter leaf_iter = grid.tree().cbeginLeaf();

	GLint xoffset = 0, yoffset = 0, zoffset = 0;

	for (; leaf_iter; ++leaf_iter) {
		const ValueType *data = leaf_iter.getLeaf()->buffer().data();

		glTexSubImage3D(GL_TEXTURE_3D, 0,
		                xoffset, yoffset, zoffset,
		                leafDim, leafDim, leafDim,
		                GL_RED, GL_FLOAT, data);

		zoffset += leafDim;
	}
}

void convert_grid(const openvdb::FloatGrid &grid, GLfloat *data,
                  const openvdb::Coord &min, const openvdb::Coord &max, float &scale)
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
		math::Coord ijk;
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

int axis_dominant_v3_single(const glm::vec3 &vec)
{
	const float x = glm::abs(vec[0]);
	const float y = glm::abs(vec[1]);
	const float z = glm::abs(vec[2]);
	return ((x > y) ?
	       ((x > z) ? 0 : 2) :
	       ((y > z) ? 1 : 2));
}

VolumeShader::VolumeShader()
    : m_vao(0)
    , m_vbo(0)
    , m_texture_id(0)
    , m_min(glm::vec3(0.0f))
    , m_max(glm::vec3(0.0f))
    , m_size(glm::vec3(0.0f))
    , m_inv_size(glm::vec3(0.0f))
    , m_num_slices(256)
    , m_axis(-1)
    , m_scale(0.0f)
    , m_use_lut(false)
    , m_draw_bbox(false)
{}

VolumeShader::~VolumeShader()
{
	m_shader.deleteShaderProgram();
	m_bbox_shader.deleteShaderProgram();

	glDeleteVertexArrays(1, &m_vao);
	glDeleteBuffers(1, &m_vbo);
	glDeleteVertexArrays(1, &m_bbox_vao);
	glDeleteBuffers(1, &m_bbox_index_vbo);
	glDeleteBuffers(1, &m_bbox_verts_vbo);

	glDeleteTextures(1, &m_texture_id);
	glDeleteTextures(1, &m_transfer_func_id);
}

bool VolumeShader::init(const std::string &filename, std::ostream &os)
{
	if (loadVolumeFile(filename, os)) {
		loadVolumeShader();
		loadBBoxShader();
		loadTransferFunction();
		return true;
	}

	return false;
}

bool VolumeShader::loadVolumeFile(const std::string &volume_file, std::ostream &os)
{
	using namespace openvdb;
	using namespace openvdb::math;

	initialize();
	io::File file(volume_file);

	if (file.open()) {
		FloatGrid::Ptr grid;

		if (file.hasGrid(Name("Density"))) {
			grid = gridPtrCast<FloatGrid>(file.readGrid(Name("Density")));
		}
		else if (file.hasGrid(Name("density"))) {
			grid = gridPtrCast<FloatGrid>(file.readGrid(Name("density")));
		}
		else {
			os << "No density grid found in file: \'" << volume_file << "\'!\n";
			return false;
		}

		if (grid->getGridClass() == GRID_LEVEL_SET) {
			os << "Grid \'" << grid->getName() << "\'is a level set!\n";
			return false;
		}

		auto meta_map = file.getMetadata();

		file.close();

		if ((*meta_map)["creator"]) {
			auto creator = (*meta_map)["creator"]->str();

			/* If the grid comes from Blender (Z-up), rotate it so it is Y-up */
			if (creator == "Blender/OpenVDBWriter") {
				Timer("Transform Blender Grid");
				openvdb::Mat4R mat(openvdb::Mat4R::identity());
		        mat.preRotate(openvdb::math::X_AXIS, -M_PI_2);

				FloatGrid::Ptr xformed_grid = FloatGrid::create(grid->background());

				tools::GridTransformer transformer(mat);
				transformer.transformGrid<tools::BoxSampler>(*grid, *xformed_grid);

				grid = xformed_grid;
			}
		}

		CoordBBox bbox = grid->evalActiveVoxelBoundingBox();
		Coord bbox_min = bbox.min();
		Coord bbox_max = bbox.max();

		/* Get resolution */
		auto extent = bbox_max - bbox_min;
		const int X_DIM = extent[0];
		const int Y_DIM = extent[1];
		const int Z_DIM = extent[2];

		/* Compute grid size */
		Vec3f min = grid->transform().indexToWorld(bbox_min);
		Vec3f max = grid->transform().indexToWorld(bbox_max);

		for (int i(0); i < 3; ++i) {
			m_min[i] = min[i];
			m_max[i] = max[i];
		}

		m_size = (m_max - m_min);
		m_inv_size = 1.0f / m_size;

#if 0
		printf("Dimensions: %d, %d, %d\n", X_DIM, Y_DIM, Z_DIM);
		printf("Min: %f, %f, %f\n", min[0], min[1], min[2]);
		printf("Max: %f, %f, %f\n", max[0], max[1], max[2]);
#endif

		/* Copy data */
		GLfloat *data = new GLfloat[X_DIM * Y_DIM * Z_DIM];
		convert_grid(*grid, data, bbox_min, bbox_max, m_scale);

		glGenTextures(1, &m_texture_id);
		glBindTexture(GL_TEXTURE_3D, m_texture_id);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 4);

		Timer("Move data to GPU")
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, X_DIM, Y_DIM, Z_DIM, 0, GL_RED, GL_FLOAT, data);

		glGenerateMipmap(GL_TEXTURE_3D);

		delete [] data;
		return true;
	}

	os << "Unable to open file \'" << volume_file << "\'\n";

	return false;
}

void VolumeShader::loadBBoxShader()
{
	m_bbox_shader.loadFromFile(GL_VERTEX_SHADER, "shader/flat_shader.vert");
	m_bbox_shader.loadFromFile(GL_FRAGMENT_SHADER, "shader/flat_shader.frag");

	m_bbox_shader.createAndLinkProgram();

	m_bbox_shader.use();
	{
		m_bbox_shader.addAttribute("vVertex");
		m_bbox_shader.addUniform("MVP");
	}
	m_bbox_shader.unUse();

	glm::vec3 vertices[8] = {
	    glm::vec3(m_min[0], m_min[1], m_min[2]),
	    glm::vec3(m_max[0], m_min[1], m_min[2]),
	    glm::vec3(m_max[0], m_max[1], m_min[2]),
	    glm::vec3(m_min[0], m_max[1], m_min[2]),
	    glm::vec3(m_min[0], m_min[1], m_max[2]),
	    glm::vec3(m_max[0], m_min[1], m_max[2]),
	    glm::vec3(m_max[0], m_max[1], m_max[2]),
	    glm::vec3(m_min[0], m_max[1], m_max[2])
	};

	for (int i(0); i < 8; ++i) {
		vertices[i] *= m_inv_size;
	}

	const GLushort indices[24] = {
	    0, 1, 1, 2,
	    2, 3, 3, 0,
	    4, 5, 5, 6,
	    6, 7, 7, 4,
	    0, 4, 1, 5,
	    2, 6, 3, 7
	};

	glGenVertexArrays(1, &m_bbox_vao);
	glGenBuffers(1, &m_bbox_verts_vbo);
	glGenBuffers(1, &m_bbox_index_vbo);

	glBindVertexArray(m_bbox_vao);

	glBindBuffer(GL_ARRAY_BUFFER, m_bbox_verts_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &(vertices[0].x), GL_STATIC_DRAW);
	glEnableVertexAttribArray(m_bbox_shader["vVertex"]);
	glVertexAttribPointer(m_bbox_shader["vVertex"], 3, GL_FLOAT, GL_FALSE, 0, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_bbox_index_vbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices[0], GL_STATIC_DRAW);

	glBindVertexArray(0);
}

void VolumeShader::loadVolumeShader()
{
	m_shader.loadFromFile(GL_VERTEX_SHADER, "shader/texture_slicer.vert");
	m_shader.loadFromFile(GL_FRAGMENT_SHADER, "shader/texture_slicer.frag");

	m_shader.createAndLinkProgram();

	m_shader.use();
	{
		m_shader.addAttribute("vVertex");
		m_shader.addUniform("MVP");
		m_shader.addUniform("offset");
		m_shader.addUniform("volume");
		m_shader.addUniform("lut");
		m_shader.addUniform("use_lut");
		m_shader.addUniform("scale");

		glUniform1i(m_shader("volume"), 0);
		glUniform1i(m_shader("lut"), 1);

		auto min = m_min * m_inv_size;
		glUniform3fv(m_shader("offset"), 1, &min[0]);
		glUniform1f(m_shader("scale"), m_scale);
	}
	m_shader.unUse();

	/* setup the vertex array and buffer objects */
	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);

	glBindVertexArray(m_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

	/* pass the sliced volume vector to buffer output memory */
	glBufferData(GL_ARRAY_BUFFER, sizeof(m_texture_slices), nullptr, GL_DYNAMIC_DRAW);

	/* enable vertex attribute array for position */
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	glBindVertexArray(0);
}

void VolumeShader::loadTransferFunction()
{
	/* transfer function (lookup table) color values */
	const glm::vec3 jet_values[12] = {
	    glm::vec3(1.0f, 0.0f, 0.0f),
		glm::vec3(1.0f, 0.0f, 0.5f),
		glm::vec3(1.0f, 0.0f, 1.0f),

		glm::vec3(0.5f, 0.0f, 1.0f),
		glm::vec3(0.0f, 0.5f, 1.0f),
		glm::vec3(0.0f, 1.0f, 1.0f),

		glm::vec3(0.0f, 1.0f, 0.5f),
		glm::vec3(0.0f, 1.0f, 0.0f),
		glm::vec3(0.5f, 1.0f, 0.0f),

		glm::vec3(1.0f, 1.0f, 0.0f),
		glm::vec3(1.0f, 0.5f, 0.0f),
		glm::vec3(1.0f, 0.0f, 0.0f),
	};

	float data[256][3];
	int indices[12];

	for (int i = 0; i < 12; ++i) {
		indices[i] = i * 21;
	}

	/* for each adjacent pair of colors, find the difference in the RGBA values
	 * and then interpolate */
	for (int j = 0; j < 12 - 1; ++j) {
		auto color_diff = jet_values[j + 1] - jet_values[j];
		auto index = indices[j + 1] - indices[j];
		auto inc = color_diff / static_cast<float>(index);

		for (int i = indices[j] + 1; i < indices[j + 1]; ++i) {
			data[i][0] = jet_values[j].r + i * inc.r;
			data[i][1] = jet_values[j].g + i * inc.g;
			data[i][2] = jet_values[j].b + i * inc.b;
		}
	}

	glGenTextures(1, &m_transfer_func_id);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, m_transfer_func_id);

	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 256, 0, GL_RGB, GL_FLOAT, data);
}

void VolumeShader::slice(const glm::vec3 &view_dir)
{
	auto axis = axis_dominant_v3_single(view_dir);

	if (m_axis == axis) {
		return;
	}

	m_axis = axis;
	auto count = 0;
	auto depth = m_min[m_axis];
	auto slice_size = m_size[m_axis] / m_num_slices;

	/* always process slices in back to front order! */
	if (view_dir[m_axis] > 0.0f) {
		depth = m_max[m_axis];
		slice_size = -slice_size;
	}

	for (auto slice(0); slice < m_num_slices; slice++) {
		const glm::vec3 vertices[3][4] = {
		    {
		        glm::vec3(depth, m_min[1], m_min[2]),
		        glm::vec3(depth, m_max[1], m_min[2]),
		        glm::vec3(depth, m_max[1], m_max[2]),
		        glm::vec3(depth, m_min[1], m_max[2])
		    },
		    {
		        glm::vec3(m_min[0], depth, m_min[2]),
		        glm::vec3(m_min[0], depth, m_max[2]),
		        glm::vec3(m_max[0], depth, m_max[2]),
		        glm::vec3(m_max[0], depth, m_min[2])
		    },
		    {
		        glm::vec3(m_min[0], m_min[1], depth),
		        glm::vec3(m_min[0], m_max[1], depth),
		        glm::vec3(m_max[0], m_max[1], depth),
		        glm::vec3(m_max[0], m_min[1], depth)
		    }
		};

		m_texture_slices[count++] = vertices[m_axis][0] * m_inv_size;
		m_texture_slices[count++] = vertices[m_axis][1] * m_inv_size;
		m_texture_slices[count++] = vertices[m_axis][2] * m_inv_size;
		m_texture_slices[count++] = vertices[m_axis][0] * m_inv_size;
		m_texture_slices[count++] = vertices[m_axis][2] * m_inv_size;
		m_texture_slices[count++] = vertices[m_axis][3] * m_inv_size;

		depth += slice_size;
	}

	/* update buffer object */
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(m_texture_slices), &(m_texture_slices[0].x));
}

void VolumeShader::render(const glm::vec3 &dir, const glm::mat4 &MVP, const bool is_rotated)
{
	if (is_rotated) {
		slice(dir);
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindVertexArray(m_vao);

	m_shader.use();
	glUniformMatrix4fv(m_shader("MVP"), 1, GL_FALSE, glm::value_ptr(MVP));
	glUniform1i(m_shader("use_lut"), m_use_lut);
	glDrawArrays(GL_TRIANGLES, 0, MAX_SLICES * 6);
	m_shader.unUse();

	if (m_draw_bbox) {
		glBindVertexArray(m_bbox_vao);

		m_bbox_shader.use();
		glUniformMatrix4fv(m_bbox_shader("MVP"), 1, GL_FALSE, glm::value_ptr(MVP));
		glDrawElements(GL_LINES, 24, GL_UNSIGNED_SHORT, nullptr);
		m_bbox_shader.unUse();
	}

	glDisable(GL_BLEND);
}

void VolumeShader::changeNumSlicesBy(int x)
{
	m_num_slices += x;
	m_num_slices = std::min(MAX_SLICES, std::max(m_num_slices, 3));
}

void VolumeShader::toggleUseLUT()
{
	m_use_lut = ((m_use_lut) ? false : true);
}

void VolumeShader::toggleBBoxDrawing()
{
	m_draw_bbox = ((m_draw_bbox) ? false : true);
}
