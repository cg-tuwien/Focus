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

		scene = fscene::load_scene("assets/level01c.dae");

		// Create offscreen image views to ray-trace into, one for each frame in flight:
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
				mOffscreenImageViews.back()->get_image().target_layout()); // TODO: This must be abstracted!
			assert((mOffscreenImageViews.back()->config().subresourceRange.aspectMask & vk::ImageAspectFlagBits::eColor) == vk::ImageAspectFlagBits::eColor);
		}

		// Create our ray tracing pipeline with the required configuration:
		mPipeline = cgb::ray_tracing_pipeline_for(
			cgb::define_shader_table(
				cgb::ray_generation_shader("shaders/rt_09_first.rgen"),
				cgb::triangles_hit_group::create_with_rchit_only("shaders/rt_09_first.rchit"),
				cgb::triangles_hit_group::create_with_rchit_only("shaders/rt_09_secondary.rchit"),
				cgb::miss_shader("shaders/rt_09_first.rmiss.spv"),
				cgb::miss_shader("shaders/rt_09_secondary.rmiss.spv")
			),
			cgb::max_recursion_depth::set_to_max(),
			// Define push constants and descriptor bindings:
			cgb::push_constant_binding_data { cgb::shader_type::ray_generation, 0, sizeof(transformation_matrices) },
			cgb::binding(0, 0, scene->get_image_samplers()),
			cgb::binding(4, 0, scene->get_model_buffer()),
			cgb::binding(0, 1, scene->get_material_buffer()),
			cgb::binding(0, 2, scene->get_index_buffer_views()),
			cgb::binding(0, 3, scene->get_texcoord_buffer_views()),
			cgb::binding(0, 4, scene->get_normal_buffer_views()),
			cgb::binding(0, 5, scene->get_tangent_buffer_views()),
			cgb::binding(5, 0, scene->get_light_buffer()),
			cgb::binding(1, 0, mOffscreenImageViews[0]),	// Just take any, this is just to define the layout
			cgb::binding(2, 0, scene->get_tlas()[0])		// Just take any, this is just to define the layout
		);

		// The following is a bit ugly and needs to be abstracted sometime in the future. Sorry for that.
		// Right now it is neccessary to upload the resource descriptors to the GPU (the information about the uniform buffer, in particular).
		// This descriptor set will be used in render(). It is only created once to save memory/to make lifetime management easier.
		mDescriptorSet.reserve(cgb::context().main_window()->number_of_in_flight_frames());
		for (int i = 0; i < cgb::context().main_window()->number_of_in_flight_frames(); ++i) {
			mDescriptorSet.emplace_back(std::make_shared<cgb::descriptor_set>());
			*mDescriptorSet.back() = cgb::descriptor_set::create({ 
				cgb::binding(0, 0, scene->get_image_samplers()),
				cgb::binding(4, 0, scene->get_model_buffer()),
				cgb::binding(0, 1, scene->get_material_buffer()),
				cgb::binding(0, 2, scene->get_index_buffer_views()),
				cgb::binding(0, 3, scene->get_texcoord_buffer_views()),
				cgb::binding(0, 4, scene->get_normal_buffer_views()),
				cgb::binding(0, 5, scene->get_tangent_buffer_views()),
				cgb::binding(5, 0, scene->get_light_buffer()),
				cgb::binding(1, 0, mOffscreenImageViews[i]),
				cgb::binding(2, 0, scene->get_tlas()[0])
			});	
		}
		
		// Add the camera to the composition (and let it handle the updates)
		mQuakeCam.set_translation({ 0.0f, 0.0f, 0.0f });
		mQuakeCam.set_perspective_projection(glm::radians(60.0f), cgb::context().main_window()->aspect_ratio(), 0.5f, 100.0f);
		//mQuakeCam.set_orthographic_projection(-5, 5, -5, 5, 0.5, 100);
		cgb::current_composition().add_element(mQuakeCam);
	}

	void update() override
	{
		static int counter = 0;
		if (++counter == 4) {
			auto current = std::chrono::high_resolution_clock::now();
			auto time_span = current - mInitTime;
			auto int_min = std::chrono::duration_cast<std::chrono::minutes>(time_span).count();
			auto int_sec = std::chrono::duration_cast<std::chrono::seconds>(time_span).count();
			auto fp_ms = std::chrono::duration<double, std::milli>(time_span).count();
			printf("Time from init to fourth frame: %d min, %lld sec %lf ms\n", int_min, int_sec - static_cast<decltype(int_sec)>(int_min) * 60, fp_ms - 1000.0 * int_sec);
		}

		//// Arrow Keys || Page Up/Down Keys => Move the TLAS
		//static int64_t updateUntilFrame = -1;
		//if (cgb::input().key_down(cgb::key_code::left) || cgb::input().key_down(cgb::key_code::right) || cgb::input().key_down(cgb::key_code::page_down) || cgb::input().key_down(cgb::key_code::page_up) || cgb::input().key_down(cgb::key_code::up) || cgb::input().key_down(cgb::key_code::down)) {
		//	// Make sure to update all of the in-flight TLASs, otherwise we'll get some geometry jumping:
		//	updateUntilFrame = cgb::context().main_window()->current_frame() + cgb::context().main_window()->number_of_in_flight_frames() - 1;
		//}
		//if (cgb::context().main_window()->current_frame() <= updateUntilFrame)
		//{
		//	auto inFlightIndex = cgb::context().main_window()->in_flight_index_for_frame();
		//	
		//	auto x = (cgb::input().key_down(cgb::key_code::left)      ? -cgb::time().delta_time() : 0.0f)
		//			+(cgb::input().key_down(cgb::key_code::right)     ?  cgb::time().delta_time() : 0.0f);
		//	auto y = (cgb::input().key_down(cgb::key_code::page_down) ? -cgb::time().delta_time() : 0.0f)
		//			+(cgb::input().key_down(cgb::key_code::page_up)   ?  cgb::time().delta_time() : 0.0f);
		//	auto z = (cgb::input().key_down(cgb::key_code::up)        ? -cgb::time().delta_time() : 0.0f)
		//			+(cgb::input().key_down(cgb::key_code::down)      ?  cgb::time().delta_time() : 0.0f);
		//	auto speed = 10.0f;
		//	
		//	// Change the position of one of the current TLASs BLAS, and update-build the TLAS.
		//	// The changes do not affect the BLAS, only the instance-data that the TLAS stores to each one of the BLAS.
		//	//
		//	// 1. Change every other instance:
		//	bool evenOdd = true;
		//	for (auto& inst : mGeometryInstances) {
		//		evenOdd = !evenOdd;
		//		if (evenOdd) { continue;}
		//		inst.set_transform(glm::translate(inst.mTransform, glm::vec3{x, y, z} * speed));
		//		//break; // only transform the first
		//	}
		//	//
		//	// 2. Update the TLAS for the current inFlightIndex, copying the changed BLAS-data into an internal buffer:
		//	mTLAS[inFlightIndex]->update(mGeometryInstances, [](cgb::semaphore _Semaphore) {
		//		cgb::context().main_window()->set_extra_semaphore_dependency(std::move(_Semaphore));
		//	});
		//}

		////SIMPLE ANIMATION
		//float passedtime = cgb::time().time_since_start();
		//float posy = sin(passedtime);

		//for (size_t i = 0; i < mGeometryInstances.size(); ++i) {
		//	auto& inst = mGeometryInstances[i];
		//	auto model = mModels[i];
		//	inst.set_transform(glm::translate(glm::vec3(0, posy, 0)) * model.mTransformation);
		//	//ToDo: Update Normal Matrix
		//}
		//auto inFlightIndex = cgb::context().main_window()->in_flight_index_for_frame();
		//mTLAS[inFlightIndex]->update(mGeometryInstances, [](cgb::semaphore _Semaphore) {
		//	cgb::context().main_window()->set_extra_semaphore_dependency(std::move(_Semaphore));
		//});

		if (cgb::input().key_pressed(cgb::key_code::space)) {
			// Print the current camera position
			auto pos = mQuakeCam.translation();
			LOG_INFO(fmt::format("Current camera position: {}", cgb::to_string(pos)));
		}
		if (cgb::input().key_pressed(cgb::key_code::escape)) {
			// Stop the current composition:
			cgb::current_composition().stop();
		}
		if (cgb::input().key_pressed(cgb::key_code::tab)) {
			if (mQuakeCam.is_enabled()) {
				mQuakeCam.disable();
			}
			else {
				mQuakeCam.enable();
			}
		}
	}

	void render() override
	{
		auto inFlightIndex = cgb::context().main_window()->in_flight_index_for_frame();
		
		auto cmdbfr = cgb::context().graphics_queue().pool().get_command_buffer(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
		cmdbfr.begin_recording();

		cmdbfr.set_image_barrier(
			cgb::create_image_barrier(
				mOffscreenImageViews[inFlightIndex]->get_image().image_handle(),
				mOffscreenImageViews[inFlightIndex]->get_image().format().mFormat,
				vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral)
		);

		// Bind the pipeline
		cmdbfr.handle().bindPipeline(vk::PipelineBindPoint::eRayTracingNV, mPipeline->handle());

		// Set the descriptors:
		cmdbfr.handle().bindDescriptorSets(vk::PipelineBindPoint::eRayTracingNV, mPipeline->layout_handle(), 0, 
			mDescriptorSet[inFlightIndex]->number_of_descriptor_sets(),
			mDescriptorSet[inFlightIndex]->descriptor_sets_addr(), 
			0, nullptr);

		// Set the push constants:
		auto pushConstantsForThisDrawCall = transformation_matrices { 
			mQuakeCam.view_matrix()
		};
		cmdbfr.handle().pushConstants(mPipeline->layout_handle(), vk::ShaderStageFlagBits::eRaygenNV, 0, sizeof(pushConstantsForThisDrawCall), &pushConstantsForThisDrawCall);

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
				mOffscreenImageViews[inFlightIndex]->get_image().image_handle(),
				mOffscreenImageViews[inFlightIndex]->get_image().format().mFormat,
				vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal)
		);

		//cmdbfr.copy_image(mOffscreenImageViews[inFlightIndex]->get_image(), cgb::context().main_window()->swap_chain_images()[inFlightIndex]);

		cmdbfr.end_recording();
		submit_command_buffer_ownership(std::move(cmdbfr));
		present_image(mOffscreenImageViews[inFlightIndex]->get_image());
	}

	void finalize() override
	{
		cgb::context().logical_device().waitIdle();
	}

private: // v== Member variables ==v
	std::chrono::high_resolution_clock::time_point mInitTime;
	
	std::vector<cgb::image_view> mOffscreenImageViews;

	std::unique_ptr<fscene> scene;

	std::vector<std::shared_ptr<cgb::descriptor_set>> mDescriptorSet;

	cgb::ray_tracing_pipeline mPipeline;
	cgb::graphics_pipeline mGraphicsPipeline;
	cgb::quake_camera mQuakeCam;

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


