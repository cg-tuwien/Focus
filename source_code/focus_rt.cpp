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

		if (mLevelLogic->level_status() == levelstatus::LOST) {
			mLevelLogic->reset();
			//mLevelLogic->update(0);	//TODO: might be necessary
		}
		else if (mLevelLogic->level_status() == levelstatus::WON) {
			switch (level) {
				case 1: {
					cgb::current_composition().remove_element(*mScene.get());
					cgb::current_composition().remove_element(*mLevelLogic.get());
					mOldScene = std::move(mScene);
					mOldLevelLogic = std::move(mLevelLogic);
					//mScene = fscene::load_scene("assets/level01c.dae", "assets/anothersimplechar2.dae");
					mScene = fscene::load_scene("assets/level02a.dae", "assets/anothersimplechar2.dae");
					mRenderer.set_scene(mScene.get());
					mLevelLogic = std::make_unique<flevel2logic>(mScene.get());
					cgb::current_composition().add_element(*mScene.get());
					cgb::current_composition().add_element(*mLevelLogic.get());
					break;
				}
				default: {
					cgb::current_composition().stop();
				}
			}
			++level;
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

	std::unique_ptr<fscene> mOldScene;
	std::unique_ptr<flevellogic> mOldLevelLogic;

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


