//Author: Simon Fraiss
#include "includes.h"

/*
Main-Function, Starting point of the application.
Initializes the application.
*/
int main() // <== Starting point ==
{
	try {

		// What's the name of our application
		cgb::settings::gApplicationName = "cg_base::focus_rt";
		cgb::settings::gQueueSelectionPreference = cgb::device_queue_selection_strategy::prefer_everything_on_single_queue;
		cgb::settings::gRequiredDeviceExtensions.push_back(VK_NV_RAY_TRACING_EXTENSION_NAME);
		cgb::settings::gRequiredDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		cgb::settings::gLoadImagesInSrgbFormatByDefault = true;

		// Create a window and open it
		auto mainWnd = cgb::context().create_window("Focus!");
		mainWnd->set_resolution({ 1600, 900 });
		mainWnd->set_presentaton_mode(cgb::presentation_mode::triple_buffering);
		mainWnd->set_additional_back_buffer_attachments({ cgb::attachment::create_depth(cgb::image_format::default_depth_format()) });
		mainWnd->open();

		// Create an instance of fgamecontrol, which in turn will create the other cg_elements for our composition
		auto control = fgamecontrol();

		// Create a composition of game control, level logic, scene and renderer.
		// These objects have their own initialize, update, fixed_update, render and finalize functions 
		// which will be called automatically by cgbase.
		auto hello = cgb::composition<cgb::fixed_update_timer, cgb::sequential_executor>({
				&control,
				control.get_level_logic(),
				control.get_scene(),
				control.get_renderer()
			});

		// Start the composition.
		hello.start();
	}
	catch (std::runtime_error & re)
	{
		LOG_ERROR_EM(re.what());
	}
}


