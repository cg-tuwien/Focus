#pragma once
#include "includes.h"
#define CHAR_PATH "assets/anothersimplechar2.dae"

class fgamecontrol : public cgb::cg_element
{

public:

	fgamecontrol() {
		level = 1;
		mScene = fscene::load_scene(flevel1logic::level_path(), CHAR_PATH);
		mLevelLogic = std::make_unique<flevel1logic>(mScene.get());
		mRenderer.set_scene(mScene.get());
		mRenderer.set_level_logic(mLevelLogic.get());
	}

	//Initializes the game
	void initialize() override
	{
		mInitTime = std::chrono::high_resolution_clock::now();

		//Create Scene, LevelLogic and Renderer.
		cgb::input().set_cursor_disabled(true);

		//execution order: focus_rt, levellogic, scene, renderer
	}

	int32_t execution_order() const override {
		return 1;
	}

	void update() override
	{
		mOldScene.reset();
		mOldLevelLogic.reset();
		static int counter = 0;
		if (++counter == 4) {
			auto current = std::chrono::high_resolution_clock::now();
			auto time_span = current - mInitTime;
			auto int_min = std::chrono::duration_cast<std::chrono::minutes>(time_span).count();
			auto int_sec = std::chrono::duration_cast<std::chrono::seconds>(time_span).count();
			auto fp_ms = std::chrono::duration<double, std::milli>(time_span).count();
			printf("Time from init to fourth frame: %d min, %lld sec %lf ms\n", int_min, int_sec - static_cast<decltype(int_sec)>(int_min) * 60, fp_ms - 1000.0 * int_sec);
		}
		//Esc -> Stop Game
		if (cgb::input().key_pressed(cgb::key_code::escape)) {
			cgb::current_composition().stop();
		}
		//Tab -> Pause game
		if (cgb::input().key_pressed(cgb::key_code::tab)) {
			bool newstate = cgb::input().is_cursor_disabled();
			mLevelLogic->set_paused(newstate);
			cgb::input().set_cursor_disabled(!newstate);
		}

		//Fade-In
		if (fadeIn >= 0) {
			mRenderer.set_fade_value(fadeIn);
			fadeIn -= cgb::time().delta_time() * 1.0;
			if (fadeIn <= 0.0f) {
				fadeIn = -1.0f;
				mRenderer.set_fade_value(0.0f);
			}
		}
		if (firstFrame) {
			fadeIn = 1.0f;
			firstFrame = false;
			mRenderer.set_fade_value(fadeIn);
		}

		//If Won -> Fade Out and Load Next Level
		if (mLevelLogic->level_status() == levelstatus::WON) {
			if (fadeOut < 0.0f) {
				fadeOut = 1.0f;
			}
			else {
				fadeOut -= cgb::time().delta_time() * 0.5;
				mRenderer.set_fade_value(1 - fadeOut);
				if (fadeOut <= 0) {
					next_level();
					fadeOut = -1.0f;
					mRenderer.set_fade_value(1.0f);
					firstFrame = true;
				}
			}
		}
		//If Lost -> Restart Level
		else if (mLevelLogic->level_status() == levelstatus::LOST) {
			mLevelLogic->reset();
		}
	}

	void finalize() override
	{
		cgb::context().logical_device().waitIdle();
	}

	fscene* get_scene() const {
		return mScene.get();
	}

	flevellogic* get_level_logic() const {
		return mLevelLogic.get();
	}

	frenderer* get_renderer() {
		return &mRenderer;
	}

private: // v== Member variables ==v
	std::chrono::high_resolution_clock::time_point mInitTime;

	frenderer mRenderer;
	std::unique_ptr<fscene> mScene;
	std::unique_ptr<flevellogic> mLevelLogic;
	int level = 0;
	//For In- and Out-Fading
	float fadeOut = -1.0f;
	float fadeIn = -1.0f;
	bool firstFrame = false;	//if this is the first frame of a new level (except l1)

	//Old scenes to be deleted after initialization of the new one
	std::unique_ptr<fscene> mOldScene;
	std::unique_ptr<flevellogic> mOldLevelLogic;

	//Switches the level to a new one. T is the flevellogic object
	template <typename T>
	void switch_level() {
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
		++level;
	}

	//Stops the current level and loads the next one, or stops the game if over
	void next_level() {
		switch (level) {
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
}; // focus_rt_app
