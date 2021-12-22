#include "includes.h"

void frenderer::initialize()
{
	// Create a descriptor cache that helps us to conveniently create descriptor sets:
	mDescriptorCache = gvk::context().create_descriptor_cache();
	
	//Create Focus Hit Buffer
	uint32_t initialfocushit = 0;
	size_t n = gvk::context().main_window()->number_of_frames_in_flight();
	mFocusHitBuffers.resize(n);
	mFadeBuffers.resize(n);
	for (int i = 0; i < n; ++i) {
		mFocusHitBuffers[i] = gvk::context().create_buffer(
			avk::memory_usage::host_coherent, {},
			avk::storage_buffer_meta::create_from_size(sizeof(uint32_t))
		);
		mFocusHitBuffers[i]->fill(&initialfocushit, 0, avk::sync::not_required());

		mFadeBuffers[i] = gvk::context().create_buffer(
			avk::memory_usage::host_coherent, {},
			avk::uniform_buffer_meta::create_from_size(sizeof(float))
		);
		mFadeBuffers[i]->fill(&fadeValue, 0, avk::sync::not_required());
	}

	// Create offscreen image views to ray-trace into, one for each frame in flight:
	mOffscreenImageViews.reserve(n);
	const auto wdth = gvk::context().main_window()->resolution().x;
	const auto hght = gvk::context().main_window()->resolution().y;
	const auto frmt = gvk::format_from_window_color_buffer(gvk::context().main_window());
	for (size_t i = 0; i < n; ++i) {
		mOffscreenImageViews.emplace_back(
			gvk::context().create_image_view(
				gvk::context().create_image(wdth, hght, frmt, 1, avk::memory_usage::device, avk::image_usage::general_storage_image)
			)
		);
		mOffscreenImageViews.back()->get_image().transition_to_layout({}, avk::sync::with_barriers(gvk::context().main_window()->command_buffer_lifetime_handler()));
		assert((mOffscreenImageViews.back()->create_info().subresourceRange.aspectMask & vk::ImageAspectFlagBits::eColor) == vk::ImageAspectFlagBits::eColor);
	}

	create_descriptor_sets_for_scene();
}

void frenderer::update()
{
	auto index = gvk::context().main_window()->in_flight_index_for_frame();

	mFadeBuffers[index]->fill(&fadeValue, 0, avk::sync::not_required());

	auto focushitcount = mFocusHitBuffers[index]->read<uint32_t>(0, avk::sync::not_required());
	mLevelLogic->set_focus_hit_value(double(focushitcount) / double(gvk::context().main_window()->swap_chain_extent().width * gvk::context().main_window()->swap_chain_extent().height));
	focushitcount = 0;

	mFocusHitBuffers[index]->fill(&focushitcount, 0, avk::sync::not_required());
}

void frenderer::render()
{
	auto mainWnd = gvk::context().main_window();
	auto inFlightIndex = mainWnd->in_flight_index_for_frame();

	//An alternative would be to record the command buffers in advance, that would however disable the push constants, 
	//so we would need to use a uniform buffer for the camera matrix. And recording every frame shouldn't be too much anyway.

	auto& commandPool = gvk::context().get_command_pool_for_single_use_command_buffers(*mQueue);
	auto cmdbfr = commandPool->alloc_command_buffer(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	
	cmdbfr->begin_recording();
	cmdbfr->bind_pipeline(avk::const_referenced(mPipeline));
	cmdbfr->bind_descriptors(mPipeline->layout(), mDescriptorCache.get_or_create_descriptor_sets({
		avk::descriptor_binding(0, 0, mScene->get_model_buffer(inFlightIndex)),
		avk::descriptor_binding(0, 1, mScene->get_material_buffer(inFlightIndex)),
		avk::descriptor_binding(0, 2, mScene->get_light_buffer()),
		avk::descriptor_binding(0, 3, mScene->get_image_samplers()),
		avk::descriptor_binding(6, 0, mScene->get_index_buffer_views()),
		avk::descriptor_binding(0, 5, mScene->get_texcoord_buffer_views()),
		avk::descriptor_binding(0, 6, mScene->get_normal_buffer_views()),
		avk::descriptor_binding(0, 7, mScene->get_tangent_buffer_views()),
		avk::descriptor_binding(1, 0, mOffscreenImageViews[inFlightIndex]->as_storage_image()),
		avk::descriptor_binding(2, 0, mScene->get_tlas()[inFlightIndex]),
		avk::descriptor_binding(3, 0, mScene->get_background_buffer(inFlightIndex)),
		avk::descriptor_binding(3, 1, mScene->get_gradient_buffer()),
		avk::descriptor_binding(4, 0, mFocusHitBuffers[inFlightIndex]),
		avk::descriptor_binding(5, 0, mFadeBuffers[inFlightIndex])
	}));

	// Set the push constants:
	auto cameraTransform = mScene->get_camera().global_transformation_matrix();
	cmdbfr->handle().pushConstants(mPipeline->layout_handle(), vk::ShaderStageFlagBits::eRaygenNV, 0, sizeof(cameraTransform), &cameraTransform);

	//mPipeline->print_shader_binding_table_groups();
	
	// TRACE. THA. RAYZ.
	cmdbfr->trace_rays(
		gvk::for_each_pixel(mainWnd),
		mPipeline->shader_binding_table(),
		avk::using_raygen_group_at_index(0),
		avk::using_miss_group_at_index(0),
		avk::using_hit_group_at_index(0)
	);

	// Sync ray tracing with transfer:
	cmdbfr->establish_global_memory_barrier(
		avk::pipeline_stage::ray_tracing_shaders,                       avk::pipeline_stage::transfer,
		avk::memory_access::shader_buffers_and_images_write_access,     avk::memory_access::transfer_read_access
	);

	mainWnd->current_backbuffer()->image_view_at(0)->get_image().set_target_layout(vk::ImageLayout::ePresentSrcKHR);
	avk::copy_image_to_another(mOffscreenImageViews[inFlightIndex]->get_image(), mainWnd->current_backbuffer()->image_view_at(0)->get_image(), avk::sync::with_barriers_into_existing_command_buffer(*cmdbfr, {}, {}));
	
	// Make sure to properly sync with ImGui manager which comes afterwards (it uses a graphics pipeline):
	cmdbfr->establish_global_memory_barrier(
		avk::pipeline_stage::transfer,                                  avk::pipeline_stage::color_attachment_output,
		avk::memory_access::transfer_write_access,                      avk::memory_access::color_attachment_write_access
	);
	
	cmdbfr->end_recording();

	// The swap chain provides us with an "image available semaphore" for the current frame.
	// Only after the swapchain image has become available, we may start rendering into it.
	auto imageAvailableSemaphore = mainWnd->consume_current_image_available_semaphore();
	
	// Submit the draw call and take care of the command buffer's lifetime:
	mQueue->submit(cmdbfr, imageAvailableSemaphore);
	mainWnd->handle_lifetime(std::move(cmdbfr));
}

void frenderer::set_scene(fscene* scene)
{
	mScene = scene;
	if (mOffscreenImageViews.size() > 0) {
		//only if already initalized
		create_descriptor_sets_for_scene();
	}
}

void frenderer::create_descriptor_sets_for_scene()
{
	mPipeline = gvk::context().create_ray_tracing_pipeline_for(
		avk::define_shader_table(
			avk::ray_generation_shader("shaders/default.rgen.spv"),
			avk::triangles_hit_group::create_with_rahit_and_rchit("shaders/default.rahit.spv", "shaders/default.rchit.spv"),
			avk::triangles_hit_group::create_with_rahit_and_rchit("shaders/shadowray.rahit.spv", "shaders/shadowray.rchit.spv"),
			avk::miss_shader("shaders/default.rmiss.spv"),
			avk::miss_shader("shaders/shadowray.rmiss.spv"),
			avk::triangles_hit_group::create_with_rahit_and_rchit("shaders/leaves.rahit.spv", "shaders/leaves.rchit.spv"),
			avk::triangles_hit_group::create_with_rahit_and_rchit("shaders/leaves.rahit.spv", "shaders/shadowray.rchit.spv")
		),
		gvk::context().get_max_ray_tracing_recursion_depth(),
		// Define push constants and descriptor bindings:
		avk::push_constant_binding_data{ avk::shader_type::ray_generation, 0, sizeof(glm::mat4) },
		avk::descriptor_binding(0, 0, mScene->get_model_buffer(0)),		// Just take any, this is just to define the layout
		avk::descriptor_binding(0, 1, mScene->get_material_buffer(0)),		// Just take any, this is just to define the layout
		avk::descriptor_binding(0, 2, mScene->get_light_buffer()),
		avk::descriptor_binding(0, 3, mScene->get_image_samplers()),
		avk::descriptor_binding(6, 0, mScene->get_index_buffer_views()),
		avk::descriptor_binding(0, 5, mScene->get_texcoord_buffer_views()),
		avk::descriptor_binding(0, 6, mScene->get_normal_buffer_views()),
		avk::descriptor_binding(0, 7, mScene->get_tangent_buffer_views()),
		avk::descriptor_binding(1, 0, mOffscreenImageViews[0]->as_storage_image()),			// Just take any, this is just to define the layout
		avk::descriptor_binding(2, 0, mScene->get_tlas()[0]),				// Just take any, this is just to define the layout
		avk::descriptor_binding(3, 0, mScene->get_background_buffer(0)),	// Just take any, this is just to define the layout
		avk::descriptor_binding(3, 1, mScene->get_gradient_buffer()),
		avk::descriptor_binding(4, 0, mFocusHitBuffers[0]),				// Just take any, this is just to define the layout
		avk::descriptor_binding(5, 0, mFadeBuffers[0])						// Just take any, this is just to define the layout
	);
}
