#include "includes.h"

void fscene::create_buffers_for_model(fmodel& newElement)
{
	auto mainWindow = gvk::context().main_window();
	
	//Create Buffers
	auto positionsBuffer = gvk::context().create_buffer(
		avk::memory_usage::device, vk::BufferUsageFlagBits::eRayTracingKHR | vk::BufferUsageFlagBits::eShaderDeviceAddressKHR,
		avk::vertex_buffer_meta::create_from_data(newElement.mPositions).describe_only_member(newElement.mPositions[0], avk::content_description::position)
	);
	positionsBuffer->fill(
		newElement.mPositions.data(), 0,
		avk::sync::with_barriers(mainWindow->command_buffer_lifetime_handler())
	);
	positionsBuffer.enable_shared_ownership();

	auto indexBuffer = gvk::context().create_buffer(
		avk::memory_usage::device, vk::BufferUsageFlagBits::eRayTracingKHR | vk::BufferUsageFlagBits::eShaderDeviceAddressKHR,
		avk::index_buffer_meta::create_from_data(newElement.mIndices)
	);
	indexBuffer->fill(
		newElement.mIndices.data(), 0,
		avk::sync::with_barriers(mainWindow->command_buffer_lifetime_handler())
	);
	indexBuffer.enable_shared_ownership();
	
	auto texCoordsBuffer = gvk::context().create_buffer(
		avk::memory_usage::device, {},
		avk::uniform_texel_buffer_meta::create_from_data(newElement.mTexCoords).describe_only_member(newElement.mTexCoords[0])
	);
	texCoordsBuffer->fill(
		newElement.mTexCoords.data(), 0,
		avk::sync::with_barriers(mainWindow->command_buffer_lifetime_handler())
	);
	mTexCoordBufferViews.push_back(gvk::context().create_buffer_view(std::move(texCoordsBuffer)));

	auto normalsBuffer = gvk::context().create_buffer(
		avk::memory_usage::device, {},
		avk::uniform_texel_buffer_meta::create_from_data(newElement.mNormals).describe_only_member(newElement.mNormals[0])
	);
	normalsBuffer->fill(
		newElement.mNormals.data(), 0,
		avk::sync::with_barriers(mainWindow->command_buffer_lifetime_handler())
	);
	mNormalBufferViews.push_back(gvk::context().create_buffer_view(std::move(normalsBuffer)));

	auto tangentsBuffer = gvk::context().create_buffer(
		avk::memory_usage::device, {},
		avk::uniform_texel_buffer_meta::create_from_data(newElement.mTangents).describe_only_member(newElement.mTangents[0])
	);
	tangentsBuffer->fill(
		newElement.mTangents.data(), 0,
		avk::sync::with_barriers(mainWindow->command_buffer_lifetime_handler())
	);
	mTangentBufferViews.push_back(gvk::context().create_buffer_view(std::move(tangentsBuffer)));

	auto indexTexelBuffer = gvk::context().create_buffer(
		avk::memory_usage::device, {},
		avk::uniform_texel_buffer_meta::create_from_data(newElement.mIndices).set_format<glm::uvec3>()
	);
	indexTexelBuffer->fill(
		newElement.mIndices.data(), 0,
		avk::sync::with_barriers(mainWindow->command_buffer_lifetime_handler())
	);
	mIndexBufferViews.push_back(gvk::context().create_buffer_view(std::move(indexTexelBuffer)));

	auto blas = gvk::context().create_bottom_level_acceleration_structure({
			avk::acceleration_structure_size_requirements::from_buffers( avk::vertex_index_buffer_pair{ positionsBuffer, indexBuffer } )
		}, true);
	blas.enable_shared_ownership();
	auto instance = gvk::context().create_geometry_instance(blas)
		.set_transform_column_major(gvk::to_array(newElement.mTransformation))
		.set_custom_index(mBLASs.size());
	instance.mFlags = (newElement.mTransparent) ? vk::GeometryInstanceFlagBitsNV::eForceNoOpaque : vk::GeometryInstanceFlagBitsNV::eForceOpaque;
	mGeometryInstances.push_back(instance);
	// Build BLAS and do not sync at all at this point (passing two empty handlers as parameters):
	//   The idea of this is that multiple BLAS can be built
	//   in parallel, we only have to make sure to synchronize
	//   before we start building the TLAS.
	blas->build({ avk::vertex_index_buffer_pair{ positionsBuffer, indexBuffer } }, {}, avk::sync::wait_idle()); // Wait idle could be optimized, but lifetime of positionsBuffer and indexBuffer must be handled
	mBLASs.push_back(std::move(blas));

	mModelData.emplace_back(newElement);
}

gvk::material_gpu_data& fscene::get_material_data(size_t materialIndex)
{
	mUpdateMaterials = gvk::context().main_window()->number_of_frames_in_flight();
	return mGpuMaterials[materialIndex];
}

std::unique_ptr<fscene> fscene::load_scene(const std::string& filename, const std::string& characterfilename)
{
	auto mainWindow = gvk::context().main_window();
	auto fif = mainWindow->number_of_frames_in_flight();

	std::unique_ptr<fscene> s = std::make_unique<fscene>();
	s->mLoadedScene = gvk::model_t::load_from_file(filename, aiProcess_Triangulate | aiProcess_CalcTangentSpace);
	s->mCgbCharacter = gvk::model_t::load_from_file(characterfilename, aiProcess_Triangulate | aiProcess_CalcTangentSpace);

	auto cameras = s->mLoadedScene->cameras();
	assert(cameras.size() > 0);
	s->mCamera = cameras[0];

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

			newElement.mName = s->mLoadedScene->name_of_mesh(meshindex);
			newElement.mTransparent = (newElement.mName == "Sphere");
			newElement.mFlags = (newElement.mTransparent) ? 1 : 0;
			newElement.mTransformation = s->mLoadedScene->transformation_matrix_for_mesh(meshindex);

			//Get CPU-Data
			gvk::append_indices_and_vertex_data(
				gvk::additional_index_data(newElement.mIndices, [&]() { return s->mLoadedScene->indices_for_mesh<uint32_t>(meshindex);						}),
				gvk::additional_vertex_data(newElement.mPositions, [&]() { return s->mLoadedScene->positions_for_mesh(meshindex);							}),
				gvk::additional_vertex_data(newElement.mTexCoords, [&]() { return s->mLoadedScene->texture_coordinates_for_mesh<glm::vec2>(meshindex);		}),
				gvk::additional_vertex_data(newElement.mNormals, [&]() { return s->mLoadedScene->normals_for_mesh(meshindex);								}),
				gvk::additional_vertex_data(newElement.mTangents, [&]() { return s->mLoadedScene->tangents_for_mesh(meshindex);								})
			);
			
			s->create_buffers_for_model(newElement);

		}
	}

	//Character
	s->mCharacterIndex = s->mModelData.size();
	fmodel character;
	character.mModelIndex = s->mCharacterIndex;
	character.mPositions = s->mCgbCharacter->positions_for_mesh(0);
	character.mTexCoords = s->mCgbCharacter->texture_coordinates_for_mesh<glm::vec2>(0);
	character.mNormals = s->mCgbCharacter->normals_for_mesh(0);
	character.mTangents = s->mCgbCharacter->tangents_for_mesh(0);
	character.mIndices = s->mCgbCharacter->indices_for_mesh<uint32_t>(0);
	character.mTransformation = s->mCgbCharacter->transformation_matrix_for_mesh(0);
	character.mMaterialIndex = s->mMaterials.size();
	character.mTransparent = true;
	character.mName = "Character";
	s->mModels.push_back(character);
	//Create Character Material
	auto charMat = gvk::material_config();
	charMat.mDiffuseReflectivity = glm::vec4(0.5);
	s->mMaterials.push_back(charMat);
	s->create_buffers_for_model(character);

	s->mModelBuffers.resize(fif);
	for (size_t i = 0; i < fif; ++i) {
		s->mModelBuffers[i] = gvk::context().create_buffer(
			avk::memory_usage::host_coherent, {},
			avk::storage_buffer_meta::create_from_data(s->mModelData)
		);
		s->mModelBuffers[i]->fill(s->mModelData.data(), 0, avk::sync::not_required());
	}

	//----CREATE GPU BUFFERS-----
	//Materials + Textures
	auto [gpuMaterials, imageSamplers] = gvk::convert_for_gpu_usage(
		s->mMaterials, true,
		avk::image_usage::general_texture,
		avk::filter_mode::trilinear,
		avk::border_handling_mode::repeat,
		avk::sync::with_barriers(mainWindow->command_buffer_lifetime_handler())
	);
	s->mMaterialBuffers.resize(fif);
	for (size_t i = 0; i < fif; ++i) {
		s->mMaterialBuffers[i] = gvk::context().create_buffer(
			avk::memory_usage::host_coherent, {},
			avk::storage_buffer_meta::create_from_data(gpuMaterials)
		);
		s->mMaterialBuffers[i]->fill(gpuMaterials.data(), 0, avk::sync::not_required());
	}
	s->mGpuMaterials = gpuMaterials;
	s->mImageSamplers = std::move(imageSamplers);

	//Lights
	std::vector<gvk::lightsource> loadedLights = s->mLoadedScene->lights();
	std::vector<gvk::lightsource_gpu_data> lights;
	lights.resize(loadedLights.size());
	gvk::convert_for_gpu_usage(loadedLights, loadedLights.size(), glm::mat4{1.0f}, lights);
	
	uint32_t lightCount = lights.size();
	uint32_t buffersize = sizeof(gvk::lightsource_gpu_data) * lights.size() + sizeof(uint32_t)*4;
	char* data = new char[buffersize];
	uint32_t* udata = reinterpret_cast<uint32_t*>(data);
	memcpy(udata, &lightCount, sizeof(uint32_t));
	memcpy(udata + 4, lights.data(), sizeof(gvk::lightsource_gpu_data) * lights.size());
	gvk::lightsource_gpu_data* test = reinterpret_cast<gvk::lightsource_gpu_data*>(udata + 4);
	s->mLightBuffer = gvk::context().create_buffer(
		avk::memory_usage::device, {},
		avk::storage_buffer_meta::create_from_size(buffersize)
	);
	s->mLightBuffer->fill(data, 0, avk::sync::wait_idle(true));
	delete[] data;

	//Background Color Buffer
	s->mBackgroundColor = glm::vec4(0.3, 0.3, 0.3, 0);
	s->mPerlinBackgroundBuffers.resize(fif);
	for (size_t i = 0; i < fif; ++i) {
		s->mPerlinBackgroundBuffers[i] = gvk::context().create_buffer(
			avk::memory_usage::host_coherent, {},
			avk::uniform_buffer_meta::create_from_data(s->mBackgroundColor)
		);
		s->mPerlinBackgroundBuffers[i]->fill(&s->mBackgroundColor, 0, avk::sync::not_required());
	}

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
	s->mPerlinGradientBuffer = gvk::context().create_buffer(
		avk::memory_usage::host_coherent, {},
		avk::storage_buffer_meta::create_from_size(sizeof(float) * lonsegs * latsegs * 2)
	);
	s->mPerlinGradientBuffer->fill(gdata, 0, avk::sync::not_required());
	delete gdata;

	//---- CREATE TLAS -----
	s->mTLASs.reserve(fif);
	for (decltype(fif) i = 0; i < fif; ++i) {
		// Each TLAS owns every BLAS (this will only work, if the BLASs themselves stay constant, i.e. read access
		auto tlas = gvk::context().create_top_level_acceleration_structure(s->mGeometryInstances.size(), true);
		// Build the TLAS, ...
		tlas->build(s->mGeometryInstances, {}, avk::sync::with_barriers(
					gvk::context().main_window()->command_buffer_lifetime_handler(),
					// Sync before building the TLAS:
					[](avk::command_buffer_t& commandBuffer, avk::pipeline_stage destinationStage, std::optional<avk::read_memory_access> readAccess){
						assert(avk::pipeline_stage::acceleration_structure_build == destinationStage);
						assert(!readAccess.has_value() || avk::memory_access::acceleration_structure_read_access == readAccess.value().value());
						// Wait on all the BLAS builds that happened before (and their memory):
						commandBuffer.establish_global_memory_barrier_rw(
							avk::pipeline_stage::acceleration_structure_build, destinationStage,
							avk::memory_access::acceleration_structure_write_access, readAccess
						);
					},
					// Whatever comes after must wait for this TLAS build to finish (and its memory to be made available):
					//   However, that's exactly what the default_handler_after_operation
					//   does, so let's just use that (it is also the default value for
					//   this handler)
					avk::sync::presets::default_handler_after_operation
				)
		);
		s->mTLASs.push_back(std::move(tlas));
	}

	return std::move(s);
}

fmodel* fscene::get_model_by_name(const std::string& name)
{
	for (fmodel& model : mModels) {
		if (model.mName == name) {
			return &model;
		}
	}
	throw new std::runtime_error("Did not find model " + name);
}

void fscene::set_character_position(const glm::vec3& position)
{
	mModels[mCharacterIndex].mTransformation[3] = glm::vec4(position, 1.0f);
}

void fscene::update()
{
	int i = 0;
	for (fmodel& model : mModels) {
		mGeometryInstances[i].set_transform_column_major(gvk::to_array(model.mTransformation));
		if (model.mLeaf) {
			model.mTransparent = true;
			mGeometryInstances[i].set_instance_offset(4);
		}
		mGeometryInstances[i].mFlags = (model.mTransparent) ? vk::GeometryInstanceFlagBitsNV::eForceNoOpaque : vk::GeometryInstanceFlagBitsNV::eForceOpaque;
		mModelData[i] = model;
		++i;
	}
	auto fidx = gvk::context().main_window()->in_flight_index_for_frame();
	mModelBuffers[fidx]->fill(mModelData.data(), 0, avk::sync::not_required());
	if (mUpdateMaterials > 0) {
		--mUpdateMaterials;
		mMaterialBuffers[fidx]->fill(mGpuMaterials.data(), 0, avk::sync::not_required());
	}

	mPerlinBackgroundBuffers[fidx]->fill(&mBackgroundColor, 0, avk::sync::not_required());

	mTLASs[fidx]->update(mGeometryInstances, {}, avk::sync::with_barriers(
			gvk::context().main_window()->command_buffer_lifetime_handler(),
			{}, // Nothing to wait for
			[](avk::command_buffer_t& commandBuffer, avk::pipeline_stage srcStage, std::optional<avk::write_memory_access> srcAccess){
				// We want this update to be as efficient/as tight as possible
				commandBuffer.establish_global_memory_barrier(
					srcStage, avk::pipeline_stage::ray_tracing_shaders, // => ray tracing shaders must wait on the building of the acceleration structure
					srcAccess.value().value(), avk::memory_access::acceleration_structure_read_access // TLAS-update's memory must be made visible to ray tracing shader's caches (so they can read from)
				);
			}
		)
	);
}