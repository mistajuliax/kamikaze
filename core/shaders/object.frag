#version 330 core

layout(location = 0) out vec4 fragment_color;

smooth in vec3 nor;
smooth in vec3 col;

uniform bool for_outline;

void main()
{
	if (for_outline) {
		fragment_color = vec4(0.8, 0.4, 0.2, 1.0);
		return;
	}

	vec4 color = vec4(col, 1.0);
	vec3 normalized_normal = normalize(nor);
	float w = 0.5 * (1.0 + dot(normalized_normal, vec3(0.0, 1.0, 0.0)));
	vec4 diffuseColor = w * color + (1.0 - w) * (color * 0.3);
	fragment_color = diffuseColor;
}
