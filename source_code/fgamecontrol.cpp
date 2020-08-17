//Author: Simon Fraiss
#include "includes.h"

fgamecontrol::fgamecontrol(avk::queue* q) {
	mQueue = q;
	mLevelId = 1;
}

void fgamecontrol::initialize()
{
	mScene = fscene::load_scene(flevel1logic::level_path(), CHAR_PATH);
	mLevelLogic = std::make_unique<flevel1logic>(mScene.get());
	mRenderer.set_scene(mScene.get());
	mRenderer.set_level_logic(mLevelLogic.get());
	
	gvk::input().set_cursor_mode(gvk::cursor::cursor_disabled_raw_input);

	gvk::current_composition()->add_element(*get_level_logic());
	gvk::current_composition()->add_element(*get_scene());
	gvk::current_composition()->add_element(*get_renderer());
}

void fgamecontrol::update()
{
	mOldScene.reset();
	mOldLevelLogic.reset();

	//Esc -> Stop Game
	if (gvk::input().key_pressed(gvk::key_code::escape)) {
		gvk::current_composition()->stop();
	}
	//Tab -> Pause game
	if (gvk::input().key_pressed(gvk::key_code::tab)) {
		bool newstate = gvk::input().is_cursor_disabled();
		mLevelLogic->set_paused(newstate);
		gvk::input().set_cursor_mode(newstate ? gvk::cursor::arrow_cursor : gvk::cursor::cursor_disabled_raw_input);
	}

	//Fade-In
	if (mFadeIn >= 0) {
		mRenderer.set_fade_value(mFadeIn);
		mFadeIn -= gvk::time().delta_time() * 1.0;
		if (mFadeIn <= 0.0f) {
			mFadeIn = -1.0f;
			mRenderer.set_fade_value(0.0f);
		}
	}
	if (mFirstFrame) {
		mFadeIn = 1.0f;
		mFirstFrame = false;
		mRenderer.set_fade_value(mFadeIn);
	}

	//If Won -> Fade Out and Load Next Level
	if (mLevelLogic->level_status() == levelstatus::WON) {
		if (mFadeOut < 0.0f) {
			mFadeOut = 1.0f;
		}
		else {
			mFadeOut -= gvk::time().delta_time() * 0.5;
			mRenderer.set_fade_value(1 - mFadeOut);
			if (mFadeOut <= 0) {
				next_level();
				mFadeOut = -1.0f;
				mRenderer.set_fade_value(1.0f);
				mFirstFrame = true;
			}
		}
	}
	//If Lost -> Restart Level
	else if (mLevelLogic->level_status() == levelstatus::LOST) {
		mLevelLogic->reset();
	}
}

void fgamecontrol::finalize()
{
	gvk::context().device().waitIdle();
}

fscene* fgamecontrol::get_scene() {
	return mScene.get();
}

flevellogic* fgamecontrol::get_level_logic() {
	return mLevelLogic.get();
}

frenderer* fgamecontrol::get_renderer() {
	return &mRenderer;
}

//Switches the level to a new one. T is the flevellogic class
template <typename T>
void fgamecontrol::switch_level() {
	gvk::current_composition()->remove_element(*mScene.get());
	gvk::current_composition()->remove_element(*mLevelLogic.get());
	mScene->disable();
	mLevelLogic->disable();
	mOldScene = std::move(mScene);
	mOldLevelLogic = std::move(mLevelLogic);
	mScene = fscene::load_scene(T::level_path(), CHAR_PATH);
	mRenderer.set_scene(mScene.get());
	mLevelLogic = std::make_unique<T>(mScene.get());
	mRenderer.set_level_logic(mLevelLogic.get());
	gvk::current_composition()->add_element(*mScene.get());
	gvk::current_composition()->add_element(*mLevelLogic.get());
	++mLevelId;
}

//Stops the current level and loads the next one, or stops the game if over
void fgamecontrol::next_level() {
	switch (mLevelId) {
		case 1: {
			switch_level<flevel2logic>();
			break;
		}
		case 2: {
			switch_level<flevel3logic>();
			break;
		}
		case 3: {
			switch_level<flevel4logic>();
			break;
		}
		default: {
			gvk::current_composition()->stop();
		}
	}
}
