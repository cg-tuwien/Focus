#include "includes.h"

std::unique_ptr<fscene> fscene::load_scene(const std::string& filename)
{
	std::vector<cgb::semaphore> blasWaitSemaphores;
	blasWaitSemaphores.reserve(100);

	std::unique_ptr<fscene> s = std::make_unique<fscene>();
	s->mLoadedScene = cgb::model_t::load_from_file(filename, aiProcess_Triangulate | aiProcess_CalcTangentSpace);

	s->mCamera = s->mLoadedScene->get_cameras()[0];

	auto distinctMaterials = s->mLoadedScene->distinct_material_configs();
	s->mMaterials.reserve(distinctMaterials.size());
	s->mModels.reserve(distinctMaterials.size());
	s->mTexCoordBufferViews.reserve(100);
	s->mNormalBufferViews.reserve(100);
	s->mTangentBufferViews.reserve(100);
	s->mIndexBufferViews.reserve(100);

	//Iterate over materials
	for (const auto& pair : distinctMaterials) {
		s->mMaterials.push_back(pair.first);
		auto matIndex = s->mMaterials.size() - 1;

		//Iterate over meshes per material
		for (const auto& meshindex : pair.second) {
			auto& newElement = s->mModels.emplace_back();
			newElement.mModelIndex = s->mModels.size() - 1;
			newElement.mMaterialIndex = matIndex;

			std::string name = s->mLoadedScene->name_of_mesh(meshindex);

			//Get CPU-Data
			cgb::append_indices_and_vertex_data(
				cgb::additional_index_data(newElement.mIndices, [&]() { return s->mLoadedScene->indices_for_mesh<uint32_t>(meshindex);						}),
				cgb::additional_vertex_data(newElement.mPositions, [&]() { return s->mLoadedScene->positions_for_mesh(meshindex);							}),
				cgb::additional_vertex_data(newElement.mTexCoords, [&]() { return s->mLoadedScene->texture_coordinates_for_mesh<glm::vec2>(meshindex);		}),
				cgb::additional_vertex_data(newElement.mNormals, [&]() { return s->mLoadedScene->normals_for_mesh(meshindex);								}),
				cgb::additional_vertex_data(newElement.mTangents, [&]() { return s->mLoadedScene->tangents_for_mesh(meshindex);								})
			);
			newElement.mTransformation = s->mLoadedScene->transformation_matrix_for_mesh(meshindex);

			//Create Buffers
			auto positionsBuffer = cgb::create_and_fill(
				cgb::vertex_buffer_meta::create_from_data(newElement.mPositions).describe_only_member(newElement.mPositions[0], 0, cgb::content_description::position),
				cgb::memory_usage::device,
				newElement.mPositions.data(),
				[](auto _Semaphore) {
					cgb::context().main_window()->set_extra_semaphore_dependency(std::move(_Semaphore));
				}
			);
			positionsBuffer.enable_shared_ownership();

			auto indexBuffer = cgb::create_and_fill(
				cgb::index_buffer_meta::create_from_data(newElement.mIndices),
				cgb::memory_usage::device,
				newElement.mIndices.data(),
				[](auto _Semaphore) {
					cgb::context().main_window()->set_extra_semaphore_dependency(std::move(_Semaphore));
				}
			);
			indexBuffer.enable_shared_ownership();

			auto texCoordsBuffer = cgb::create_and_fill(
				cgb::uniform_texel_buffer_meta::create_from_data(newElement.mTexCoords).describe_only_member(newElement.mTexCoords[0]),
				cgb::memory_usage::device,
				newElement.mTexCoords.data()
			);
			s->mTexCoordBufferViews.push_back(cgb::buffer_view_t::create(std::move(texCoordsBuffer)));

			auto normalsBuffer = cgb::create_and_fill(
				cgb::uniform_texel_buffer_meta::create_from_data(newElement.mNormals).describe_only_member(newElement.mNormals[0]),
				cgb::memory_usage::device,
				newElement.mNormals.data()
			);
			s->mNormalBufferViews.push_back(cgb::buffer_view_t::create(std::move(normalsBuffer)));

			auto tangentsBuffer = cgb::create_and_fill(
				cgb::uniform_texel_buffer_meta::create_from_data(newElement.mTangents).describe_only_member(newElement.mTangents[0]),
				cgb::memory_usage::device,
				newElement.mTangents.data()
			);
			s->mTangentBufferViews.push_back(cgb::buffer_view_t::create(std::move(tangentsBuffer)));

			auto indexTexelBuffer = cgb::create_and_fill(
				cgb::uniform_texel_buffer_meta::create_from_data(newElement.mIndices).set_format<glm::vec3>(),
				cgb::memory_usage::device,
				newElement.mIndices.data()
			);
			s->mIndexBufferViews.push_back(cgb::buffer_view_t::create(std::move(indexTexelBuffer)));

			auto blas = cgb::bottom_level_acceleration_structure_t::create(positionsBuffer, indexBuffer);
			blas.enable_shared_ownership();
			auto instance = cgb::geometry_instance::create(blas).set_transform(newElement.mTransformation).set_custom_index(s->mBLASs.size());
			instance.mFlags = (name == "Sphere") ? vk::GeometryInstanceFlagBitsNV::eForceNoOpaque : vk::GeometryInstanceFlagBitsNV::eForceOpaque;
			s->mGeometryInstances.push_back(instance);
			blas->build([&](cgb::semaphore _Semaphore) {
				// Store them and pass them as a dependency to the TLAS-build
				blasWaitSemaphores.push_back(std::move(_Semaphore));
			});
			s->mBLASs.push_back(std::move(blas));

			newElement.mName = s->mLoadedScene->name_of_mesh(meshindex);

			s->mModelData.emplace_back(newElement);
		}
	}

	s->mModelBuffer = cgb::create_and_fill(
		cgb::storage_buffer_meta::create_from_data(s->mModelData),
		cgb::memory_usage::device,
		s->mModelData.data()
	);

	//----CREATE GPU BUFFERS-----
	//Materials + Textures
	auto [gpuMaterials, imageSamplers] = cgb::convert_for_gpu_usage(s->mMaterials,
		[](auto _Semaphore) {
			cgb::context().main_window()->set_extra_semaphore_dependency(std::move(_Semaphore));
		});
	s->mMaterialBuffer = cgb::create_and_fill(
		cgb::storage_buffer_meta::create_from_data(gpuMaterials),
		cgb::memory_usage::host_coherent,
		gpuMaterials.data()
	);
	s->mImageSamplers = std::move(imageSamplers);

	//Lights
	std::vector<cgb::lightsource_gpu_data> lights = cgb::convert_lights_for_gpu_usage(s->mLoadedScene->get_all_lights());
	uint32_t lightCount = lights.size();
	uint32_t buffersize = sizeof(cgb::lightsource_gpu_data) * lights.size() + sizeof(uint32_t)*4;
	char* data = new char[buffersize];
	uint32_t* udata = reinterpret_cast<uint32_t*>(data);
	memcpy(udata, &lightCount, sizeof(uint32_t));
	memcpy(udata + 4, lights.data(), sizeof(cgb::lightsource_gpu_data) * lights.size());
	cgb::lightsource_gpu_data* test = reinterpret_cast<cgb::lightsource_gpu_data*>(udata + 4);
	s->mLightBuffer = cgb::create_and_fill(
		cgb::storage_buffer_meta::create_from_size(buffersize),
		cgb::memory_usage::device,
		data
	);
	delete data;

	//Background Color Buffer
	glm::vec4 initialColor = glm::vec4(0.3, 0.3, 0.3, 0);
	s->mPerlinBackgroundBuffer = cgb::create_and_fill(
		cgb::uniform_buffer_meta::create_from_data(initialColor),
		cgb::memory_usage::host_coherent,
		&initialColor
	);

	//Perlin Gradient Buffer
	uint32_t lonsegs = 100;
	uint32_t latsegs = 50;
	float* gdata = new float[lonsegs * latsegs * 2];
	srand(s->mModels.size());
	for (uint32_t i = 0; i < lonsegs; ++i) {
		for (uint32_t j = 0; j < latsegs; ++j) {
			float x = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
			float y = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
			glm::vec2 vec = glm::vec2(x, y);
			vec = glm::normalize(vec);
			gdata[latsegs * i + 2 * j + 0] = vec.x;
			gdata[latsegs * i + 2 * j + 1] = vec.y;
		}
	}
	s->mPerlinGradientBuffer = cgb::create_and_fill(
		cgb::storage_buffer_meta::create_from_size(sizeof(float) * lonsegs * latsegs * 2),
		cgb::memory_usage::host_coherent,
		gdata
	);
	delete gdata;

	//---- CREATE TLAS -----
	auto fif = cgb::context().main_window()->number_of_in_flight_frames();
	s->mTLASs.reserve(fif);
	std::vector<cgb::semaphore> waitSemaphores = std::move(blasWaitSemaphores);
	for (decltype(fif) i = 0; i < fif; ++i) {
		std::vector<cgb::semaphore> toWaitOnInNextBuild;

		// Each TLAS owns every BLAS (this will only work, if the BLASs themselves stay constant, i.e. read access
		auto tlas = cgb::top_level_acceleration_structure_t::create(s->mGeometryInstances.size());
		// Build the TLAS, ...
		tlas->build(s->mGeometryInstances, [&](cgb::semaphore _Semaphore) {
			// ... the SUBSEQUENT build must wait on THIS build, ...
			toWaitOnInNextBuild.push_back(std::move(_Semaphore));
			},
			// ... and THIS build must wait for all the previous builds, each one represented by one semaphore:
				std::move(waitSemaphores)
				);
		s->mTLASs.push_back(std::move(tlas));

		// The mTLAS[0] waits on all the BLAS builds, but subsequent TLAS builds wait on previous TLAS builds.
		// In this way, we can ensure that all the BLAS data is available for the TLAS builds [1], [2], ...
		// Alternatively, we would require the BLAS builds to create multiple semaphores, one for each frame in flight.
		waitSemaphores = std::move(toWaitOnInNextBuild);
	}
	// Set the semaphore of the last TLAS build as a render dependency for the first frame:
	// (i.e. ensure that everything has been built before starting to render)
	assert(waitSemaphores.size() == 1);
	cgb::context().main_window()->set_extra_semaphore_dependency(std::move(waitSemaphores[0]));

	return std::move(s);
}

std::optional<fmodel*> fscene::get_model_by_name(const std::string& name)
{
	for (fmodel& model : mModels) {
		if (model.mName == name) {
			return &model;
		}
	}
	return {};
}

void fscene::update()
{
	int i = 0;
	//ToDo: Update Normal Matrix and Model Flags?
	for (fmodel& model : mModels) {
		mGeometryInstances[i].set_transform(model.mTransformation);
		mGeometryInstances[i].mFlags = (model.mName == "Sphere") ? vk::GeometryInstanceFlagBitsNV::eForceNoOpaque : vk::GeometryInstanceFlagBitsNV::eForceOpaque;
		++i;
	}

	//auto inFlightIndex = cgb::context().main_window()->in_flight_index_for_frame();
	auto fif = cgb::context().main_window()->number_of_in_flight_frames();
	for (decltype(fif) i = 0; i < fif; ++i) {
		mTLASs[i]->update(mGeometryInstances, [](cgb::semaphore _Semaphore) {
			cgb::context().main_window()->set_extra_semaphore_dependency(std::move(_Semaphore));
		});
	}
}
