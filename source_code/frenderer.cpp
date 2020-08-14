#include "includes.h"

void frenderer::initialize()
{
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
	const auto frmt = gvk::image_format::from_window_color_buffer(cgb::context().main_window());
	cgb::invoke_for_all_in_flight_frames(cgb::context().main_window(), [&](auto inFlightIndex){
		mOffscreenImageViews.emplace_back(
			cgb::image_view_t::create(
				cgb::image_t::create(wdth, hght, frmt, false, 1, cgb::memory_usage::device, cgb::image_usage::versatile_image)
			)
		);
		mOffscreenImageViews.back()->get_image().transition_to_layout({}, cgb::sync::with_barriers_on_current_frame());
		assert((mOffscreenImageViews.back()->config().subresourceRange.aspectMask & vk::ImageAspectFlagBits::eColor) == vk::ImageAspectFlagBits::eColor);
	});

	create_descriptor_sets_for_scene();
}

void frenderer::update()
{
	auto index = cgb::context().main_window()->in_flight_index_for_frame();

	cgb::fill(mFadeBuffers[index], &fadeValue);

	auto focushitcount = cgb::read<uint32_t>(mFocusHitBuffers[index], cgb::sync::not_required());
	mLevelLogic->set_focus_hit_value(double(focushitcount) / double(cgb::context().main_window()->swap_chain_extent().width * cgb::context().main_window()->swap_chain_extent().height));
	focushitcount = 0;
	cgb::fill(mFocusHitBuffers[index], &focushitcount);
}

void frenderer::render()
{
	auto mainWnd = cgb::context().main_window();
	auto inFlightIndex = mainWnd->in_flight_index_for_frame();

	//An alternative would be to record the command buffers in advance, that would however disable the push constants, 
	//so we would need to use a uniform buffer for the camera matrix. And recording every frame shouldn't be too much anyway.

	auto cmdbfr = cgb::context().graphics_queue().create_single_use_command_buffer();
	cmdbfr->begin_recording();
	cmdbfr->bind_pipeline(mPipeline);
	cmdbfr->bind_descriptors(mPipeline->layout(), {
		cgb::binding(0, 0, mScene->get_model_buffer(inFlightIndex)),
		cgb::binding(0, 1, mScene->get_material_buffer(inFlightIndex)),
		cgb::binding(0, 2, mScene->get_light_buffer()),
		cgb::binding(0, 3, mScene->get_image_samplers()),
		cgb::binding(6, 0, mScene->get_index_buffer_views()),
		cgb::binding(0, 5, mScene->get_texcoord_buffer_views()),
		cgb::binding(0, 6, mScene->get_normal_buffer_views()),
		cgb::binding(0, 7, mScene->get_tangent_buffer_views()),
		cgb::binding(1, 0, mOffscreenImageViews[inFlightIndex]),
		cgb::binding(2, 0, mScene->get_tlas()[inFlightIndex]),
		cgb::binding(3, 0, mScene->get_background_buffer(inFlightIndex)),
		cgb::binding(3, 1, mScene->get_gradient_buffer()),
		cgb::binding(4, 0, mFocusHitBuffers[inFlightIndex]),
		cgb::binding(5, 0, mFadeBuffers[inFlightIndex])
	});

	// Set the push constants:
	const glm::mat4& viewMatrix = mScene->get_camera().view_matrix();
	cmdbfr->handle().pushConstants(mPipeline->layout_handle(), vk::ShaderStageFlagBits::eRaygenNV, 0, sizeof(viewMatrix), &viewMatrix);

	// TRACE. THA. RAYZ.
	cmdbfr->handle().traceRaysNV(
		mPipeline->shader_binding_table_handle(), 0,
		mPipeline->shader_binding_table_handle(), 3 * mPipeline->table_entry_size(), mPipeline->table_entry_size(),
		mPipeline->shader_binding_table_handle(), 1 * mPipeline->table_entry_size(), mPipeline->table_entry_size(),
		nullptr, 0, 0,
		mainWnd->swap_chain_extent().width, mainWnd->swap_chain_extent().height, 1,
		cgb::context().dynamic_dispatch());


	cmdbfr->end_recording();
	submit_command_buffer_ownership(std::move(cmdbfr));
	submit_command_buffer_ownership(mainWnd->copy_to_swapchain_image(mOffscreenImageViews[inFlightIndex]->get_image(), {}, cgb::window::wait_for_previous_commands_directly_into_present).value());
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
	mPipeline = cgb::ray_tracing_pipeline_for(
		cgb::define_shader_table(
			cgb::ray_generation_shader("shaders/default.rgen.spv"),
			cgb::triangles_hit_group::create_with_rahit_and_rchit("shaders/default.rahit.spv", "shaders/default.rchit.spv"),
			cgb::triangles_hit_group::create_with_rahit_and_rchit("shaders/shadowray.rahit.spv", "shaders/shadowray.rchit.spv"),
			cgb::miss_shader("shaders/default.rmiss.spv"),
			cgb::miss_shader("shaders/shadowray.rmiss.spv"),
			cgb::triangles_hit_group::create_with_rahit_and_rchit("shaders/leaves.rahit.spv", "shaders/leaves.rchit.spv"),
			cgb::triangles_hit_group::create_with_rahit_and_rchit("shaders/leaves.rahit.spv", "shaders/shadowray.rchit.spv")
		),
		cgb::max_recursion_depth::set_to_max(),
		// Define push constants and descriptor bindings:
		cgb::push_constant_binding_data{ cgb::shader_type::ray_generation, 0, sizeof(glm::mat4) },
		cgb::binding(0, 0, mScene->get_model_buffer(0)),		// Just take any, this is just to define the layout
		cgb::binding(0, 1, mScene->get_material_buffer(0)),		// Just take any, this is just to define the layout
		cgb::binding(0, 2, mScene->get_light_buffer()),
		cgb::binding(0, 3, mScene->get_image_samplers()),
		cgb::binding(6, 0, mScene->get_index_buffer_views()),
		cgb::binding(0, 5, mScene->get_texcoord_buffer_views()),
		cgb::binding(0, 6, mScene->get_normal_buffer_views()),
		cgb::binding(0, 7, mScene->get_tangent_buffer_views()),
		cgb::binding(1, 0, mOffscreenImageViews[0]),			// Just take any, this is just to define the layout
		cgb::binding(2, 0, mScene->get_tlas()[0]),				// Just take any, this is just to define the layout
		cgb::binding(3, 0, mScene->get_background_buffer(0)),	// Just take any, this is just to define the layout
		cgb::binding(3, 1, mScene->get_gradient_buffer()),
		cgb::binding(4, 0, mFocusHitBuffers[0]),				// Just take any, this is just to define the layout
		cgb::binding(5, 0, mFadeBuffers[0])						// Just take any, this is just to define the layout
	);
}
