//Author: Simon Fraiss
#include "includes.h"

/*
Main-Function, Starting point of the application.
Initializes the application.
*/
int main() // <== Starting point ==
{
	try {
		// Create a window and open it
		auto mainWnd = gvk::context().create_window("Focus!");
		mainWnd->set_resolution({ 1920, 1080 });
		mainWnd->set_presentaton_mode(gvk::presentation_mode::mailbox);
		mainWnd->set_number_of_concurrent_frames(3u);
		mainWnd->set_number_of_presentable_images(3u);
		mainWnd->request_srgb_framebuffer(false);
		mainWnd->open();

		auto& singleQueue = gvk::context().create_queue({}, avk::queue_selection_preference::versatile_queue, mainWnd);
		mainWnd->add_queue_family_ownership(singleQueue);
		mainWnd->set_present_queue(singleQueue);

		// Create an instance of fgamecontrol, which in turn will create the other cg_elements for our composition
		auto control = fgamecontrol(&singleQueue);

		// Create a composition of game control, level logic, scene and renderer.
		// These objects have their own initialize, update, fixed_update, render and finalize functions 
		// which will be called automatically by Gears-Vk:
		gvk::start(
			gvk::application_name("Focus!"),
#if VK_HEADER_VERSION >= 162
			gvk::required_device_extensions()
			.add_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
			.add_extension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME)
			.add_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)
			.add_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)
			.add_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)
			.add_extension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME),
			[](vk::PhysicalDeviceVulkan12Features& aVulkan12Featues) {
				aVulkan12Featues.setBufferDeviceAddress(VK_TRUE);
			},
			[](vk::PhysicalDeviceRayTracingPipelineFeaturesKHR& aRayTracingFeatures) {
				aRayTracingFeatures.setRayTracingPipeline(VK_TRUE);
			},
				[](vk::PhysicalDeviceAccelerationStructureFeaturesKHR& aAccelerationStructureFeatures) {
				aAccelerationStructureFeatures.setAccelerationStructure(VK_TRUE);
			},
#else 
			gvk::required_device_extensions()
			.add_extension(VK_KHR_RAY_TRACING_EXTENSION_NAME)
			.add_extension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME)
			.add_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)
			.add_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)
			.add_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)
			.add_extension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME),
			[](vk::PhysicalDeviceVulkan12Features& aVulkan12Featues) {
				aVulkan12Featues.setBufferDeviceAddress(VK_TRUE);
			},
			[](vk::PhysicalDeviceRayTracingFeaturesKHR& aRayTracingFeatures) {
				aRayTracingFeatures.setRayTracing(VK_TRUE);
			},
#endif
			mainWnd,
			control
		);

	}
	catch (std::runtime_error & re)
	{
		LOG_ERROR_EM(re.what());
	}
}


