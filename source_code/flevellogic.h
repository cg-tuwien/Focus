//Author: Simon Fraiss
#pragma once
#include "includes.h"

enum class levelstatus {
	RUNNING, WON, LOST
};

/*
This class resembles the logic of a specific level and is responsible for animations and player controls.
*/
class flevellogic : public gvk::invokee {
public:

	//Please write a new level_path-function in your subclass!
	//Returns the path to the corresponding level file.
	static std::string level_path() {
		throw new std::runtime_error("No level path given!");
	}

	flevellogic(fscene* scene) {
		this->mScene = scene;
	}

	//--------------------------
	//---Overridden functions---
	//make sure to override initialize and finalize!
	//if you want to override update and fixed_udpate, please override the corresponding protected functions.

	void update() override { 
		if (!mLevelPaused) {
			mStatus = update(gvk::time().delta_time(), mFocusHitValue);
		}
	}

	void fixed_update() override {
		if (!mLevelPaused) {
			fixed_update(gvk::time().fixed_delta_time());
		}
	};

	//Execution order per frame: Game Control, Level Logic, Scene, Renderer
	int32_t execution_order() const override {
		return 2;
	}

	//--------------------------
	//--------------------------

	//Returns the current level status (running/won/lost)
	levelstatus level_status() {
		return mStatus;
	}

	//Resets to initial state.
	//May be overriden in subclass! Default implementation is empty.
	virtual void reset() {};

	//Pauses or unpauses the level.
	void set_paused(bool paused) {
		this->mLevelPaused = paused;
	}

	//Sets the current Focus Hit Value (sphere-visibility-score).
	//This is managed by the renderer, which fetches this value from the gpu.
	void set_focus_hit_value(double val) {
		this->mFocusHitValue = val;
	}

protected:
	fscene* mScene;

	//Overrideable update-function. Called every frame. Returns the current level status.
	//Delta Time and current Focus Hit Value are passed automatically.
	virtual levelstatus update(float deltaT, double focusHitValue) {
		return levelstatus::RUNNING;
	}

	//Overrideable fixed_update-function. Called in regular time steps. Used for physics updates.
	//Step Size is passed automatically.
	virtual void fixed_update(float stepSize) {};

private:
	levelstatus mStatus = levelstatus::RUNNING;
	bool mLevelPaused = false;
	double mFocusHitValue = 0;
};