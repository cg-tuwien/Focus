#pragma once
#include "includes.h"


enum class levelstatus {
	RUNNING, WON, LOST
};

/*
Animates a scene
*/
class flevellogic : public cgb::cg_element {
public:
	flevellogic(fscene* scene) {
		this->scene = scene;
	}

	//Returns true iff the player won the level
	virtual void update() override { 
		if (!levelpaused) {
			status = update(cgb::time().delta_time(), 0);
		}
	}

	virtual void fixed_update() override {
		if (!levelpaused) {
			fixed_update(cgb::time().fixed_delta_time());
		}
	};

	levelstatus get_level_status() {
		return status;
	}

	//Resets to initial state
	virtual void reset() {};

	virtual void cleanup() {};

	void set_paused(bool paused) {
		this->levelpaused = paused;
	}

protected:
	fscene* scene;

	double getTimestamp() {
		return glfwGetTime();
	}

	virtual levelstatus update(float deltaT, double focusHitCount) {
		return levelstatus::RUNNING;
	}

	virtual void fixed_update(float stepSize) {};

private:
	levelstatus status = levelstatus::RUNNING;
	bool levelpaused = false;
};