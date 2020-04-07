//Author: Simon Fraiss
#include "includes.h"

fgamecontrol::fgamecontrol() {
	mLevelId = 1;
	mScene = fscene::load_scene(flevel1logic::level_path(), CHAR_PATH);
	mLevelLogic = std::make_unique<flevel1logic>(mScene.get());
	mRenderer.set_scene(mScene.get());
	mRenderer.set_level_logic(mLevelLogic.get());
}

void fgamecontrol::initialize()
{
	cgb::input().set_cursor_mode(cgb::cursor::cursor_disabled_raw_input);
}

void fgamecontrol::update()
{
	mOldScene.reset();
	mOldLevelLogic.reset();

	//Esc -> Stop Game
	if (cgb::input().key_pressed(cgb::key_code::escape)) {
		cgb::current_composition().stop();
	}
	//Tab -> Pause game
	if (cgb::input().key_pressed(cgb::key_code::tab)) {
		bool newstate = cgb::input().is_cursor_disabled();
		mLevelLogic->set_paused(newstate);
		cgb::input().set_cursor_mode(newstate ? cgb::cursor::arrow_cursor : cgb::cursor::cursor_disabled_raw_input);
	}

	//Fade-In
	if (mFadeIn >= 0) {
		mRenderer.set_fade_value(mFadeIn);
		mFadeIn -= cgb::time().delta_time() * 1.0;
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
			mFadeOut -= cgb::time().delta_time() * 0.5;
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
	cgb::context().logical_device().waitIdle();
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
	cgb::current_composition().remove_element(*mScene.get());
	cgb::current_composition().remove_element(*mLevelLogic.get());
	mScene->disable();
	mLevelLogic->disable();
	mOldScene = std::move(mScene);
	mOldLevelLogic = std::move(mLevelLogic);
	mScene = fscene::load_scene(T::level_path(), CHAR_PATH);
	mRenderer.set_scene(mScene.get());
	mLevelLogic = std::make_unique<T>(mScene.get());
	mRenderer.set_level_logic(mLevelLogic.get());
	cgb::current_composition().add_element(*mScene.get());
	cgb::current_composition().add_element(*mLevelLogic.get());
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
			cgb::current_composition().stop();
		}
	}
}
