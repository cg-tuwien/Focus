#include <cg_base.hpp>

class focus_rt_app : public cgb::cg_element
{
	struct transformation_matrices {
		glm::mat4 mViewMatrix;
	};

	struct model_gpudata {
		//alignas(16) uint32_t mMeshIndex;
		alignas(4) uint32_t mMaterialIndex;
		alignas(16) glm::mat4 mNormalMatrix;
		//alignas(16) uint32_t mFlags;
	};

	struct model
	{
		std::vector<glm::vec3> mPositions;
		std::vector<glm::vec2> mTexCoords;
		std::vector<glm::vec3> mNormals;
		std::vector<glm::vec3> mTangents;
		std::vector<uint32_t> mIndices;
		model_gpudata mGpuData;
		glm::mat4 mTransformation;

		cgb::vertex_buffer mPositionsBuffer;
		cgb::uniform_texel_buffer mTexCoordsBuffer;
		cgb::uniform_texel_buffer mNormalsBuffer;
		cgb::uniform_texel_buffer mTangentsBuffer;
		cgb::index_buffer mIndexBuffer;
		cgb::uniform_texel_buffer mIndexTexelBuffer;
		cgb::bottom_level_acceleration_structure blas;
		cgb::geometry_instance instance;

		unsigned int mMaterialIndex;
	};

	std::vector<model> mModels;

public: // v== cgb::cg_element overrides which will be invoked by the framework ==v

	void initialize() override
	{
		mInitTime = std::chrono::high_resolution_clock::now();

		// Load a collada scene from file:
		auto loadedscene = cgb::model_t::load_from_file("assets/level01c.dae", aiProcess_Triangulate | aiProcess_CalcTangentSpace);
		//auto loadedscene = cgb::model_t::load_from_file("assets/lighttest.dae");
		// Get all the different materials from the whole scene:
		auto distinctMaterials = loadedscene->distinct_material_configs();
		
		mModels.reserve(distinctMaterials.size());

		// The following loop gathers all the vertex and index data PER MATERIAL and constructs the buffers and materials.
		std::vector<cgb::material_config> allMatConfigs;
		mBLASs.reserve(100);				// Due to an internal problem, all the buffers can't properly be moved right now => use `reserve` as a workaround.
		std::vector<cgb::semaphore> blasWaitSemaphores;
		blasWaitSemaphores.reserve(100);	// Due to an internal problem, all the buffers can't properly be moved right now => use `reserve` as a workaround.
		mTexCoordBufferViews.reserve(100);	// Due to an internal problem, all the buffers can't properly be moved right now => use `reserve` as a workaround.
		mNormalBufferViews.reserve(100);
		mTangentBufferViews.reserve(100);
		mIndexBufferViews.reserve(100);		// Due to an internal problem, all the buffers can't properly be moved right now => use `reserve` as a workaround.
		for (const auto& pair : distinctMaterials) {
			auto it = std::find(std::begin(allMatConfigs), std::end(allMatConfigs), pair.first);
			allMatConfigs.push_back(pair.first);
			auto matIndex = allMatConfigs.size() - 1;
			
			// The data in distinctMaterialsOrca also references all the models and submesh-indices (at pair.second) which have a specific material (pair.first) 
			for (const auto& meshindex : pair.second) {
				// However, we have to pay attention to the specific model's scene-properties,...
				// ... specifically, to its instances:
				// (Generally, we can't combine the vertex data in this case with the vertex
				//  data of other models if we have to draw multiple instances; because in 
				//  the case of multiple instances, only the to-be-instanced geometry must
				//  be accessible independently of the other geometry.
				//  Therefore, in this example, we take the approach of building separate 
				//  buffers for everything which could potentially be instanced.)

				auto& newElement = mModels.emplace_back();
				newElement.mMaterialIndex = matIndex;

				// Get a buffer containing all positions, and one containing all indices for all submeshes with this material
				//auto [positionsBuffer, indicesBuffer] = cgb::get_combined_vertex_and_index_buffers_for_selected_meshes({ cgb::make_tuple_model_and_indices(modelData.mLoadedModel, indices.mMeshIndices) });
				cgb::append_indices_and_vertex_data(
					cgb::additional_index_data(newElement.mIndices, [&]() { return loadedscene->indices_for_mesh<uint32_t>(meshindex);						}),
					cgb::additional_vertex_data(newElement.mPositions, [&]() { return loadedscene->positions_for_mesh(meshindex);							}),
					cgb::additional_vertex_data(newElement.mTexCoords, [&]() { return loadedscene->texture_coordinates_for_mesh<glm::vec2>(meshindex);		}),
					cgb::additional_vertex_data(newElement.mNormals, [&]() { return loadedscene->normals_for_mesh(meshindex);								}),
					cgb::additional_vertex_data(newElement.mTangents, [&]() { return loadedscene->tangents_for_mesh(meshindex);								})
				);

				newElement.mPositionsBuffer = cgb::create_and_fill(
					cgb::vertex_buffer_meta::create_from_data(newElement.mPositions).describe_only_member(newElement.mPositions[0], 0, cgb::content_description::position),
					cgb::memory_usage::device,
					newElement.mPositions.data(),
					[](auto _Semaphore) {
						cgb::context().main_window()->set_extra_semaphore_dependency(std::move(_Semaphore));
					}
				);
				newElement.mPositionsBuffer.enable_shared_ownership();

				newElement.mTexCoordsBuffer = cgb::create_and_fill(
					cgb::uniform_texel_buffer_meta::create_from_data(newElement.mTexCoords).describe_only_member(newElement.mTexCoords[0]),
					cgb::memory_usage::device,
					newElement.mTexCoords.data()
				);
				newElement.mTexCoordsBuffer.enable_shared_ownership();
				mTexCoordBufferViews.push_back(cgb::buffer_view_t::create(newElement.mTexCoordsBuffer));

				newElement.mNormalsBuffer = cgb::create_and_fill(
					cgb::uniform_texel_buffer_meta::create_from_data(newElement.mNormals).describe_only_member(newElement.mNormals[0]),
					cgb::memory_usage::device,
					newElement.mNormals.data()
				);
				newElement.mNormalsBuffer.enable_shared_ownership();
				mNormalBufferViews.push_back(cgb::buffer_view_t::create(newElement.mNormalsBuffer));

				newElement.mTangentsBuffer = cgb::create_and_fill(
					cgb::uniform_texel_buffer_meta::create_from_data(newElement.mTangents).describe_only_member(newElement.mTangents[0]),
					cgb::memory_usage::device,
					newElement.mTangents.data()
				);
				newElement.mTangentsBuffer.enable_shared_ownership();
				mTangentBufferViews.push_back(cgb::buffer_view_t::create(newElement.mTangentsBuffer));

				newElement.mIndexBuffer = cgb::create_and_fill(
					cgb::index_buffer_meta::create_from_data(newElement.mIndices),
					cgb::memory_usage::device,
					newElement.mIndices.data(),
					[](auto _Semaphore) {
						cgb::context().main_window()->set_extra_semaphore_dependency(std::move(_Semaphore));
					}
				);
				newElement.mIndexBuffer.enable_shared_ownership();

				newElement.mIndexTexelBuffer = cgb::create_and_fill(
					cgb::uniform_texel_buffer_meta::create_from_data(newElement.mIndices).set_format<glm::vec3>(),
					cgb::memory_usage::device,
					newElement.mIndices.data()
				);
				newElement.mIndexTexelBuffer.enable_shared_ownership();
				mIndexBufferViews.push_back(cgb::buffer_view_t::create(newElement.mIndexTexelBuffer));
				

				// Create one bottom level acceleration structure per model
				newElement.blas = cgb::bottom_level_acceleration_structure_t::create(newElement.mPositionsBuffer, newElement.mIndexBuffer);
				// Enable shared ownership because we'll have one TLAS per frame in flight, each one referencing the SAME BLASs
				// (But that also means that we may not modify the BLASs. They must stay the same, otherwise concurrent access will fail.)
				newElement.blas.enable_shared_ownership();

				newElement.mTransformation = loadedscene->transformation_matrix_for_mesh(meshindex);

				// Handle the instances. There must at least be one!
				newElement.instance = cgb::geometry_instance::create(newElement.blas).set_transform(newElement.mTransformation).set_custom_index(mBLASs.size());
				mGeometryInstances.push_back(newElement.instance);

				newElement.blas->build([&] (cgb::semaphore _Semaphore) {
					// Store them and pass them as a dependency to the TLAS-build
					blasWaitSemaphores.push_back(std::move(_Semaphore));
				});

				mBLASs.push_back(newElement.blas);

				newElement.mGpuData = { (uint32_t)newElement.mMaterialIndex, glm::transpose(glm::inverse(newElement.mTransformation)) };//{ (uint32_t)mModelData.size(), (uint32_t)newElement.mMaterialIndex, glm::mat4(1.0f), 0 };
				//newElement.mGpuData = { 2 };//{ (uint32_t)mModelData.size(), (uint32_t)newElement.mMaterialIndex, glm::mat4(1.0f), 0 };
				mModelData.push_back(newElement.mGpuData);
			}
		}

		mModelBuffer = cgb::create_and_fill(
			cgb::storage_buffer_meta::create_from_data(mModelData),
			//cgb::storage_buffer_meta::create_from_data(mModelData),
			cgb::memory_usage::device,
			mModelData.data()
		);

		// Convert the materials that were gathered above into a GPU-compatible format, and upload into a GPU storage buffer:
		auto [gpuMaterials, imageSamplers] = cgb::convert_for_gpu_usage(allMatConfigs, 
			[](auto _Semaphore) {
				cgb::context().main_window()->set_extra_semaphore_dependency(std::move(_Semaphore));
			});
		mMaterialBuffer = cgb::create_and_fill(
			cgb::storage_buffer_meta::create_from_data(gpuMaterials),
			cgb::memory_usage::host_coherent,
			gpuMaterials.data()
		);
		mImageSamplers = std::move(imageSamplers);

		//Create light buffer
		std::vector<cgb::lightsource_gpu_data> lights = cgb::convert_lights_for_gpu_usage(loadedscene->get_all_lights());
		mLightBuffer = cgb::create_and_fill(
			cgb::storage_buffer_meta::create_from_data(lights),
			cgb::memory_usage::device,
			lights.data()
		);
		uint32_t lightCount = lights.size();
		mLightInfoBuffer = cgb::create_and_fill(
			cgb::uniform_buffer_meta::create_from_data(lightCount),
			cgb::memory_usage::device,
			&lightCount
		);

		auto fif = cgb::context().main_window()->number_of_in_flight_frames();
		mTLAS.reserve(fif);
		std::vector<cgb::semaphore> waitSemaphores = std::move(blasWaitSemaphores);
		for (decltype(fif) i = 0; i < fif; ++i) {
			std::vector<cgb::semaphore> toWaitOnInNextBuild;
			
			// Each TLAS owns every BLAS (this will only work, if the BLASs themselves stay constant, i.e. read access
			auto tlas = cgb::top_level_acceleration_structure_t::create(mGeometryInstances.size());
			// Build the TLAS, ...
			tlas->build(mGeometryInstances, [&] (cgb::semaphore _Semaphore) {
					// ... the SUBSEQUENT build must wait on THIS build, ...
					toWaitOnInNextBuild.push_back(std::move(_Semaphore));
				},
				// ... and THIS build must wait for all the previous builds, each one represented by one semaphore:
				std::move(waitSemaphores)
			);
			mTLAS.push_back(std::move(tlas));

			// The mTLAS[0] waits on all the BLAS builds, but subsequent TLAS builds wait on previous TLAS builds.
			// In this way, we can ensure that all the BLAS data is available for the TLAS builds [1], [2], ...
			// Alternatively, we would require the BLAS builds to create multiple semaphores, one for each frame in flight.
			waitSemaphores = std::move(toWaitOnInNextBuild);
		}
		// Set the semaphore of the last TLAS build as a render dependency for the first frame:
		// (i.e. ensure that everything has been built before starting to render)
		assert(waitSemaphores.size() == 1);
		cgb::context().main_window()->set_extra_semaphore_dependency(std::move(waitSemaphores[0]));
		
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
			cgb::binding(0, 0, mImageSamplers),
			cgb::binding(4, 0, mModelBuffer),
			cgb::binding(0, 1, mMaterialBuffer),
			cgb::binding(0, 2, mIndexBufferViews),
			cgb::binding(0, 3, mTexCoordBufferViews),
			cgb::binding(0, 4, mNormalBufferViews),
			cgb::binding(0, 5, mTangentBufferViews),
			cgb::binding(5, 0, mLightInfoBuffer),
			cgb::binding(5, 1, mLightBuffer),
			cgb::binding(1, 0, mOffscreenImageViews[0]), // Just take any, this is just to define the layout
			cgb::binding(2, 0, mTLAS[0])				 // Just take any, this is just to define the layout
		);

		// The following is a bit ugly and needs to be abstracted sometime in the future. Sorry for that.
		// Right now it is neccessary to upload the resource descriptors to the GPU (the information about the uniform buffer, in particular).
		// This descriptor set will be used in render(). It is only created once to save memory/to make lifetime management easier.
		mDescriptorSet.reserve(cgb::context().main_window()->number_of_in_flight_frames());
		for (int i = 0; i < cgb::context().main_window()->number_of_in_flight_frames(); ++i) {
			mDescriptorSet.emplace_back(std::make_shared<cgb::descriptor_set>());
			*mDescriptorSet.back() = cgb::descriptor_set::create({ 
				cgb::binding(0, 0, mImageSamplers),
				cgb::binding(4, 0, mModelBuffer),
				cgb::binding(0, 1, mMaterialBuffer),
				cgb::binding(0, 2, mIndexBufferViews),
				cgb::binding(0, 3, mTexCoordBufferViews),
				cgb::binding(0, 4, mNormalBufferViews),
				cgb::binding(0, 5, mTangentBufferViews),
				cgb::binding(5, 0, mLightInfoBuffer),
				cgb::binding(5, 1, mLightBuffer),
				cgb::binding(1, 0, mOffscreenImageViews[i]),
				cgb::binding(2, 0, mTLAS[i])
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

	// Multiple BLAS, concurrently used by all the (three?) TLASs:
	std::vector<cgb::bottom_level_acceleration_structure> mBLASs;
	// Multiple TLAS, one for each frame in flight:
	std::vector<cgb::top_level_acceleration_structure> mTLAS;

	std::vector<cgb::image_view> mOffscreenImageViews;

	cgb::storage_buffer mMaterialBuffer;
	std::vector<cgb::image_sampler> mImageSamplers;
	std::vector<cgb::buffer_view> mIndexBufferViews;
	std::vector<cgb::buffer_view> mTexCoordBufferViews;
	std::vector<cgb::buffer_view> mNormalBufferViews;
	std::vector<cgb::buffer_view> mTangentBufferViews;
	std::vector<cgb::geometry_instance> mGeometryInstances;
	std::vector<model_gpudata> mModelData;
	cgb::storage_buffer mModelBuffer;
	cgb::uniform_buffer mLightInfoBuffer;
	cgb::storage_buffer mLightBuffer;

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


