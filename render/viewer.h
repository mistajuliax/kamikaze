#pragma once

const int WIDTH = 1280;
const int HEIGHT = 960;

class Camera;
class Grid;
class Volume;

class Viewer {
	int m_mouse_state;
	glm::vec4 m_bg;

	Camera *m_camera;
	Grid *m_grid;
	Volume *m_volume;

public:
	Viewer();
	~Viewer();

	void init();
	void resize(int w, int h);
	void mouseDownEvent(int button, int s, int x, int y);
	void mouseMoveEvent(int x, int y);
	void keyboardEvent(unsigned char key, int x, int y);
	void render();
	void setViewDir();

	void setVolume(Volume *volume);
};
