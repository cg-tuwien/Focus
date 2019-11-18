#pragma once
#include "includes.h"


enum class levelstatus {
	RUNNING, WON, LOST
};

/*
Level Logic of a scene. Responsible for animations and player controls
*/
class flevellogic : public cgb::cg_element {
public:

	//Please write a new level_path-function in your subclass
	static std::string level_path() {
		throw new std::runtime_error("No level path given!");
	}

	flevellogic(fscene* scene) {
		this->scene = scene;
	}

	//make sure to override initialize and finalize!

	//Returns true iff the player won the level
	virtual void update() override { 
		if (!levelpaused) {
			status = update(cgb::time().delta_time(), focushitvalue);
		}
	}

	virtual void fixed_update() override {
		if (!levelpaused) {
			fixed_update(cgb::time().fixed_delta_time());
		}
	};

	levelstatus level_status() {
		return status;
	}

	//Resets to initial state
	virtual void reset() {};

	void set_paused(bool paused) {
		this->levelpaused = paused;
	}

	void set_focus_hit_value(double val) {
		this->focushitvalue = val;
	}

	int32_t execution_order() const override {
		return 2;
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
	double focushitvalue = 0;
};