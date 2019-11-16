#include "includes.h"

class focus_rt_app : public cgb::cg_element
{
	struct transformation_matrices {
		glm::mat4 mViewMatrix;
	};

public: // v== cgb::cg_element overrides which will be invoked by the framework ==v

	void initialize() override
	{
		mInitTime = std::chrono::high_resolution_clock::now();

		level = 1;
		mScene = fscene::load_scene("assets/level01c.dae", "assets/anothersimplechar2.dae");
		mRenderer.set_scene(mScene.get());
		mLevelLogic = std::make_unique<flevel1logic>(mScene.get());
		mRenderer.set_level_logic(mLevelLogic.get());
		
		//priorities: focus_rt, levellogic, scene, renderer
		cgb::current_composition().add_element(*mScene.get());
		cgb::current_composition().add_element(*mLevelLogic.get());
		cgb::current_composition().add_element(mRenderer);
		cgb::input().set_cursor_disabled(true);
	}

	int32_t priority() const override {
		return 5;
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

		if (cgb::input().key_pressed(cgb::key_code::escape)) {
			// Stop the current composition:
			cgb::current_composition().stop();
		}
		if (cgb::input().key_pressed(cgb::key_code::tab)) {
			bool newstate = cgb::input().is_cursor_disabled();
			mLevelLogic->set_paused(newstate);
			cgb::input().set_cursor_disabled(!newstate);
		}

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
		} else if (mLevelLogic->level_status() == levelstatus::LOST) {
			mLevelLogic->reset();
		}
	}

	void finalize() override
	{
		cgb::context().logical_device().waitIdle();
	}

private: // v== Member variables ==v
	std::chrono::high_resolution_clock::time_point mInitTime;

	frenderer mRenderer;
	std::unique_ptr<fscene> mScene;
	std::unique_ptr<flevellogic> mLevelLogic;
	int level = 0;
	float fadeOut = -1.0f;
	float fadeIn = -1.0f;
	bool firstFrame = false;	//if this is the first frame of a new level

	std::unique_ptr<fscene> mOldScene;
	std::unique_ptr<flevellogic> mOldLevelLogic;

	template <typename T>
	void switch_level(const std::string& path) {
		cgb::current_composition().remove_element(*mScene.get());
		cgb::current_composition().remove_element(*mLevelLogic.get());
		mScene->disable();
		mLevelLogic->disable();
		mOldScene = std::move(mScene);
		mOldLevelLogic = std::move(mLevelLogic);
		mScene = fscene::load_scene(path, "assets/anothersimplechar2.dae");
		mRenderer.set_scene(mScene.get());
		mLevelLogic = std::make_unique<T>(mScene.get());
		mRenderer.set_level_logic(mLevelLogic.get());
		cgb::current_composition().add_element(*mScene.get());
		cgb::current_composition().add_element(*mLevelLogic.get());
		++level;
	}

	void next_level() {
		level = 3; //ToDo: Remove This
		switch (level) {
			case 1: {
				switch_level<flevel2logic>("assets/level02a.dae");
				break;
			}
			case 2: {
				switch_level<flevel3logic>("assets/level03f.dae");
				break;
			}
			case 3: {
				switch_level<flevel4logic>("assets/level04a.dae");
				break;
			}
			default: {
				cgb::current_composition().stop();
			}
		}
	}
}; // focus_rt_app

int main() // <== Starting point ==
{
	try {
		
		// What's the name of our application
		cgb::settings::gApplicationName = "cg_base::focus_rt";
		cgb::settings::gQueueSelectionPreference = cgb::device_queue_selection_strategy::prefer_everything_on_single_queue;
		cgb::settings::gRequiredDeviceExtensions.push_back(VK_NV_RAY_TRACING_EXTENSION_NAME);
		cgb::settings::gRequiredDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

		// Create a window and open it
		auto mainWnd = cgb::context().create_window("Focus!");
		mainWnd->set_resolution({ 1600, 900 });
		mainWnd->set_presentaton_mode(cgb::presentation_mode::vsync);
		mainWnd->set_additional_back_buffer_attachments({ cgb::attachment::create_depth(cgb::image_format::default_depth_format()) });
		mainWnd->open(); 

		// Create an instance of vertex_buffers_app which, in this case,
		// contains the entire functionality of our application. 
		auto element = focus_rt_app();

		// Create a composition of:
		//  - a timer
		//  - an executor
		//  - a behavior
		// ...
		auto hello = cgb::composition<cgb::varying_update_timer, cgb::sequential_executor>({
				&element
			});

		// ... and start that composition!
		hello.start();
	}
	catch (std::runtime_error& re)
	{
		LOG_ERROR_EM(re.what());
		//throw re;
	}
}


