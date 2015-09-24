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

struct VBOData {
	GLuint vao;
	GLuint vbo;
	GLuint index_vbo;
};

void gl_check_errors();

void texture_bind(const GLenum target, const GLuint texture_id, const GLint num);
void texture_unbind(const GLenum target, const GLint num);

void create_texture_1D(GLuint &texture_id, const int size, GLfloat *data);
void create_texture_3D(GLuint &texture_id, const int size[3], const int channels, GLfloat *data);

VBOData *create_vertex_buffers(GLuint attribute, const GLfloat *vertices, size_t vsize,
                           const GLuint *indices, size_t isize);

void update_vertex_buffers(VBOData *data, const GLfloat *vertices, size_t vsize,
                           const GLuint *indices, size_t isize);

void delete_vertex_buffers(VBOData *data);
