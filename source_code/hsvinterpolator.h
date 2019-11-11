#pragma once
#include <glm/glm.hpp>
#include <vector>

void RGB_to_HSV(float& fR, float& fG, float fB, float& fH, float& fS, float& fV);
void HSV_to_RGB(float& fR, float& fG, float& fB, float& fH, float& fS, float& fV);

class hsvinterpolator {

	std::vector<float> ts;
	std::vector<glm::vec3> colors;

public:
	hsvinterpolator() { };

	//PLEASE INSERT IN ASCENDING ORDER
	//takes hsv colors
	void add_sample(float t, glm::vec3 hsvcolor) {
		ts.push_back(t);
		colors.push_back(hsvcolor);
	}

	//returns rgb colors
	glm::vec3 interpolate(float t) {
		if (t <= ts.at(0)) {
			glm::vec3 hsvc = colors.at(0);
			glm::vec3 rgbc;
			HSV_to_RGB(rgbc.x, rgbc.y, rgbc.z, hsvc.x, hsvc.y, hsvc.z);
			return rgbc;
		}
		float ti = 0;
		float tip1 = ts.at(0);
		for (int i = 0; i < ts.size() - 1; ++i) {
			ti = tip1;
			tip1 = ts.at(i + 1);
			if (ti < t && t <= tip1) {
				float s = (t - ti) / (tip1 - ti);
				glm::vec3 hsvc = (1 - s) * colors.at(i) + s * colors.at(i + 1);
				glm::vec3 rgbc;
				HSV_to_RGB(rgbc.x, rgbc.y, rgbc.z, hsvc.x, hsvc.y, hsvc.z);
				return rgbc;
			}
		}
		glm::vec3 hsvc = colors.at(ts.size() - 1);
		glm::vec3 rgbc;
		HSV_to_RGB(rgbc.x, rgbc.y, rgbc.z, hsvc.x, hsvc.y, hsvc.z);
		return rgbc;
	}
};