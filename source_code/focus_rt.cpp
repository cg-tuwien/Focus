//Author: Simon Fraiss
#include "includes.h"

class update_counter : public cgb::cg_element
{
public:
	void initialize() override
	{
		mFixCount = 0;
		mUpdCount = 0;
		mNextStatsTime = std::floor(cgb::time().time_since_start_dp()) + 1.0;
	}

	void evaluate()
	{
		if (cgb::time().time_since_start_dp() > mNextStatsTime) {
			LOG_INFO(fmt::format("Between {} and {}, {} fixed_updates and {} updates occured.", mNextStatsTime - 1.0, mNextStatsTime, mFixCount, mUpdCount));
			mFixCount = 0;
			mUpdCount = 0;
			mNextStatsTime += 1.0;
		}
	}

	void fixed_update() override
	{
		evaluate();
		mFixCount++;
	}

	void update() override
	{
		evaluate();
		mUpdCount++;
	}

private:
	int mFixCount;
	int mUpdCount;
	double mNextStatsTime;
};

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

		auto counter = new update_counter();

		// Create a composition of game control, level logic, scene and renderer.
		// These objects have their own initialize, update, fixed_update, render and finalize functions 
		// which will be called automatically by cgbase.
		auto hello = cgb::composition<cgb::fixed_update_timer, cgb::sequential_executor>({
				&control,
				control.get_level_logic(),
				control.get_scene(),
				control.get_renderer(),
				counter
			});

		// Start the composition.
		hello.start();
	}
	catch (std::runtime_error & re)
	{
		LOG_ERROR_EM(re.what());
	}
}


