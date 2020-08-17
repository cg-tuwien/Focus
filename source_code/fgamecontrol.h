//Author: Simon Fraiss
#pragma once
#include "includes.h"
#define CHAR_PATH "assets/anothersimplechar2.dae"

/*
This class manages the game on a high level. It creates scene, level logic and renderer, and
it is responsible for changing the levels, as well as reacting to key strokes for pausing and stopping the game.
*/
class fgamecontrol : public gvk::invokee
{

public:
	//When creating the Game Control object, the initial scene and level logic of level 1 are loaded.
	fgamecontrol(avk::queue* q);

	//--------------------------
	//---Overridden functions---
	//--------------------------

	//Initializes the game
	void initialize() override;

	//Execution order per frame: Game Control, Level Logic, Scene, Renderer
	int32_t execution_order() const override {
		return 1;
	}

	//Checks for controlling key strokes (esc/tab) and level status change
	void update() override;

	//Called at the end of the game
	void finalize() override;

	//--------------------------
	//---Getter functions-------
	//--------------------------

	fscene* get_scene();

	flevellogic* get_level_logic();

	frenderer* get_renderer();

private: 
	//--------------------------
	//---Member variables-------
	//--------------------------
	avk::queue* mQueue;
	
	frenderer mRenderer;						//Renderer object (constant)
	std::unique_ptr<fscene> mScene;				//Scene object pointer (changes)
	std::unique_ptr<flevellogic> mLevelLogic;	//Level Logic object pointer (changes)

	int mLevelId = 0;							//Index of the current level
	float mFadeOut = -1.0f;						//Helper variable for fading out
	float mFadeIn = -1.0f;						//Helper variable for fading in
	bool mFirstFrame = false;					//Helepr variable, identifying whether this is the first frame of a new level (except for L1)

	std::unique_ptr<fscene> mOldScene;			//Old scene to be deleted after successful initialization of a new one
	std::unique_ptr<flevellogic> mOldLevelLogic;//Old level logic to be deleted after successful initialization of a new one


	//--------------------------
	//---Helper functions-------
	//--------------------------

	//Stops the current level and loads the next one, or stops the game if over.
	void next_level();

	//Switches the level to a new one. T is the flevellogic class.
	template <typename T> void switch_level();
}; 
