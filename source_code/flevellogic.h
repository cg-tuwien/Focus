#pragma once
#include "includes.h"


enum levelstatus {
	RUNNING, WON, LOST
};

/*
Animates a scene
*/
class flevellogic {
public:
	flevellogic(fscene* scene) {
		this->scene = scene;
	}

	//You should override exactly one of the update functions
	//Returns true iff the player won the level
	virtual levelstatus update(const float& deltaT, const double& focusHitCount) { return update(deltaT); };

	virtual void fixed_update(const float& stepSize) {};

	//Resets to initial state
	virtual void reset() {};

	virtual void cleanup() {};

protected:
	fscene* scene;

	virtual levelstatus update(const float& deltaT) { return RUNNING; }

	double getTimestamp() {
		return glfwGetTime();
	}
};