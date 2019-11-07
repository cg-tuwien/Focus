#include "includes.h"

void frenderer::initialize()
{
	//Create Focus Hit Buffer
	uint32_t initialfocushit = 0;
	mFocusHitBuffer = cgb::create_and_fill(
		cgb::storage_buffer_meta::create_from_size(sizeof(uint32_t)),
		cgb::memory_usage::host_coherent,
		&initialfocushit
	);

	//Create Image Views
	size_t n = cgb::context().main_window()->number_of_in_flight_frames();
	mOffscreenImageViews.reserve(n);
	for (size_t i = 0; i < n; ++i) {
		mOffscreenImageViews.emplace_back(
			cgb::image_view_t::create(
				cgb::image_t::create(
					cgb::context().main_window()->swap_chain_extent().width,
					cgb::context().main_window()->swap_chain_extent().height,
					cgb::context().main_window()->swap_chain_image_format(),
					false, 1, cgb::memory_usage::device, cgb::image_usage::versatile_image
				)
			)
		);
		cgb::transition_image_layout(
			mOffscreenImageViews.back()->get_image(),
			mOffscreenImageViews.back()->get_image().format().mFormat,
			mOffscreenImageViews.back()->get_image().target_layout());
		assert((mOffscreenImageViews.back()->config().subresourceRange.aspectMask & vk::ImageAspectFlagBits::eColor) == vk::ImageAspectFlagBits::eColor);
	}

	create_descriptor_sets_for_scene();
}

void frenderer::render() {
	auto inFlightIndex = cgb::context().main_window()->in_flight_index_for_frame();
	auto i = inFlightIndex;

	//An alternative would be to record the command buffers in advance, that would however disable the push constants, 
	//so we would need to use a uniform buffer for the camera matrix. And recording every frame shouldn't be too much anyway.

	auto cmdbfr = cgb::context().graphics_queue().pool().get_command_buffer(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
	cmdbfr.begin_recording();

	cmdbfr.set_image_barrier(
		cgb::create_image_barrier(
			mOffscreenImageViews[i]->get_image().image_handle(),
			mOffscreenImageViews[i]->get_image().format().mFormat,
			vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral)
	);

	// Bind the pipeline
	cmdbfr.handle().bindPipeline(vk::PipelineBindPoint::eRayTracingNV, mPipeline->handle());

	// Set the descriptors:
	cmdbfr.handle().bindDescriptorSets(vk::PipelineBindPoint::eRayTracingNV, mPipeline->layout_handle(), 0,
		mDescriptorSet[i]->number_of_descriptor_sets(),
		mDescriptorSet[i]->descriptor_sets_addr(),
		0, nullptr);

	// Set the push constants:
	const glm::mat4& viewMatrix = mScene->get_camera().view_matrix();
	cmdbfr.handle().pushConstants(mPipeline->layout_handle(), vk::ShaderStageFlagBits::eRaygenNV, 0, sizeof(viewMatrix), &viewMatrix);

	// TRACE. THA. RAYZ.
	cmdbfr.handle().traceRaysNV(
		mPipeline->shader_binding_table_handle(), 0,
		mPipeline->shader_binding_table_handle(), 3 * mPipeline->table_entry_size(), mPipeline->table_entry_size(),
		mPipeline->shader_binding_table_handle(), 1 * mPipeline->table_entry_size(), mPipeline->table_entry_size(),
		nullptr, 0, 0,
		cgb::context().main_window()->swap_chain_extent().width, cgb::context().main_window()->swap_chain_extent().height, 1,
		cgb::context().dynamic_dispatch());


	cmdbfr.set_image_barrier(
		cgb::create_image_barrier(
			mOffscreenImageViews[i]->get_image().image_handle(),
			mOffscreenImageViews[i]->get_image().format().mFormat,
			vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal)
	);

	cmdbfr.end_recording();

	submit_command_buffer_ownership(std::move(cmdbfr));
	present_image(mOffscreenImageViews[inFlightIndex]->get_image());

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
	size_t n = cgb::context().main_window()->number_of_in_flight_frames();
	mPipeline = cgb::ray_tracing_pipeline_for(
		cgb::define_shader_table(
			cgb::ray_generation_shader("shaders/default.rgen.spv"),
			cgb::triangles_hit_group::create_with_rahit_and_rchit("shaders/default.rahit.spv", "shaders/default.rchit.spv"),
			cgb::triangles_hit_group::create_with_rahit_and_rchit("shaders/shadowray.rahit.spv", "shaders/shadowray.rchit.spv"),
			cgb::miss_shader("shaders/default.rmiss.spv"),
			cgb::miss_shader("shaders/shadowray.rmiss.spv")
		),
		cgb::max_recursion_depth::set_to_max(),
		// Define push constants and descriptor bindings:
		cgb::push_constant_binding_data{ cgb::shader_type::ray_generation, 0, sizeof(glm::mat4) },
		cgb::binding(0, 0, mScene->get_model_buffer()),
		cgb::binding(0, 1, mScene->get_material_buffer()),
		cgb::binding(0, 2, mScene->get_light_buffer()),
		cgb::binding(0, 3, mScene->get_image_samplers()),
		cgb::binding(0, 4, mScene->get_index_buffer_views()),
		cgb::binding(0, 5, mScene->get_texcoord_buffer_views()),
		cgb::binding(0, 6, mScene->get_normal_buffer_views()),
		cgb::binding(0, 7, mScene->get_tangent_buffer_views()),
		cgb::binding(1, 0, mOffscreenImageViews[0]),	// Just take any, this is just to define the layout
		cgb::binding(2, 0, mScene->get_tlas()[0]),		// Just take any, this is just to define the layout
		cgb::binding(3, 0, mScene->get_background_buffer()),
		cgb::binding(3, 1, mScene->get_gradient_buffer()),
		cgb::binding(4, 0, mFocusHitBuffer)
	);

	mDescriptorSet.clear();
	mDescriptorSet.reserve(n);
	for (int i = 0; i < n; ++i) {
		mDescriptorSet.emplace_back(std::make_shared<cgb::descriptor_set>());
		*mDescriptorSet.back() = cgb::descriptor_set::create({
			cgb::binding(0, 0, mScene->get_model_buffer()),
			cgb::binding(0, 1, mScene->get_material_buffer()),
			cgb::binding(0, 2, mScene->get_light_buffer()),
			cgb::binding(0, 3, mScene->get_image_samplers()),
			cgb::binding(0, 4, mScene->get_index_buffer_views()),
			cgb::binding(0, 5, mScene->get_texcoord_buffer_views()),
			cgb::binding(0, 6, mScene->get_normal_buffer_views()),
			cgb::binding(0, 7, mScene->get_tangent_buffer_views()),
			cgb::binding(1, 0, mOffscreenImageViews[i]),
			cgb::binding(2, 0, mScene->get_tlas()[i]),
			cgb::binding(3, 0, mScene->get_background_buffer()),
			cgb::binding(3, 1, mScene->get_gradient_buffer()),
			cgb::binding(4, 0, mFocusHitBuffer)
			});
	}
}
