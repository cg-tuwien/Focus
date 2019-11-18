#include "includes.h"

void fscene::create_buffers_for_model(fmodel& newElement, std::vector<cgb::semaphore>& blasWaitSemaphores)
{

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
	mTexCoordBufferViews.push_back(cgb::buffer_view_t::create(std::move(texCoordsBuffer)));

	auto normalsBuffer = cgb::create_and_fill(
		cgb::uniform_texel_buffer_meta::create_from_data(newElement.mNormals).describe_only_member(newElement.mNormals[0]),
		cgb::memory_usage::device,
		newElement.mNormals.data()
	);
	mNormalBufferViews.push_back(cgb::buffer_view_t::create(std::move(normalsBuffer)));

	auto tangentsBuffer = cgb::create_and_fill(
		cgb::uniform_texel_buffer_meta::create_from_data(newElement.mTangents).describe_only_member(newElement.mTangents[0]),
		cgb::memory_usage::device,
		newElement.mTangents.data()
	);
	mTangentBufferViews.push_back(cgb::buffer_view_t::create(std::move(tangentsBuffer)));

	auto indexTexelBuffer = cgb::create_and_fill(
		cgb::uniform_texel_buffer_meta::create_from_data(newElement.mIndices).set_format<glm::vec3>(),
		cgb::memory_usage::device,
		newElement.mIndices.data()
	);
	mIndexBufferViews.push_back(cgb::buffer_view_t::create(std::move(indexTexelBuffer)));

	auto blas = cgb::bottom_level_acceleration_structure_t::create(positionsBuffer, indexBuffer);
	blas.enable_shared_ownership();
	auto instance = cgb::geometry_instance::create(blas).set_transform(newElement.mTransformation).set_custom_index(mBLASs.size());
	instance.mFlags = (newElement.mTransparent) ? vk::GeometryInstanceFlagBitsNV::eForceNoOpaque : vk::GeometryInstanceFlagBitsNV::eForceOpaque;
	mGeometryInstances.push_back(instance);
	blas->build([&](cgb::semaphore _Semaphore) {
		// Store them and pass them as a dependency to the TLAS-build
		blasWaitSemaphores.push_back(std::move(_Semaphore));
	});
	mBLASs.push_back(std::move(blas));

	mModelData.emplace_back(newElement);
}

cgb::material_gpu_data& fscene::get_material_data(size_t materialIndex)
{
	mUpdateMaterials = cgb::context().main_window()->number_of_in_flight_frames();
	return mGpuMaterials[materialIndex];
}

std::unique_ptr<fscene> fscene::load_scene(const std::string& filename, const std::string& characterfilename)
{
	auto fif = cgb::context().main_window()->number_of_in_flight_frames();

	std::vector<cgb::semaphore> blasWaitSemaphores;
	blasWaitSemaphores.reserve(100);

	std::unique_ptr<fscene> s = std::make_unique<fscene>();
	s->mLoadedScene = cgb::model_t::load_from_file(filename, aiProcess_Triangulate | aiProcess_CalcTangentSpace);
	s->mCgbCharacter = cgb::model_t::load_from_file(characterfilename, aiProcess_Triangulate | aiProcess_CalcTangentSpace);

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
			cgb::append_indices_and_vertex_data(
				cgb::additional_index_data(newElement.mIndices, [&]() { return s->mLoadedScene->indices_for_mesh<uint32_t>(meshindex);						}),
				cgb::additional_vertex_data(newElement.mPositions, [&]() { return s->mLoadedScene->positions_for_mesh(meshindex);							}),
				cgb::additional_vertex_data(newElement.mTexCoords, [&]() { return s->mLoadedScene->texture_coordinates_for_mesh<glm::vec2>(meshindex);		}),
				cgb::additional_vertex_data(newElement.mNormals, [&]() { return s->mLoadedScene->normals_for_mesh(meshindex);								}),
				cgb::additional_vertex_data(newElement.mTangents, [&]() { return s->mLoadedScene->tangents_for_mesh(meshindex);								})
			);
			
			s->create_buffers_for_model(newElement, blasWaitSemaphores);

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
	auto charMat = cgb::material_config();
	charMat.mDiffuseReflectivity = glm::vec4(0.5);
	s->mMaterials.push_back(charMat);
	s->create_buffers_for_model(character, blasWaitSemaphores);

	s->mModelBuffers.resize(fif);
	for (size_t i = 0; i < fif; ++i) {
		s->mModelBuffers[i] = cgb::create_and_fill(
			cgb::storage_buffer_meta::create_from_data(s->mModelData),
			cgb::memory_usage::host_coherent,
			s->mModelData.data()
		);
	}

	//----CREATE GPU BUFFERS-----
	//Materials + Textures
	cgb::settings::gLoadImagesInSrgbFormatByDefault = true;
	auto [gpuMaterials, imageSamplers] = cgb::convert_for_gpu_usage(s->mMaterials,
		[](auto _Semaphore) {
			cgb::context().main_window()->set_extra_semaphore_dependency(std::move(_Semaphore));
		});
	s->mMaterialBuffers.resize(fif);
	for (size_t i = 0; i < fif; ++i) {
		s->mMaterialBuffers[i] = cgb::create_and_fill(
			cgb::storage_buffer_meta::create_from_data(gpuMaterials),
			cgb::memory_usage::host_coherent,
			gpuMaterials.data()
		);
	}
	s->mGpuMaterials = gpuMaterials;
	s->mImageSamplers = std::move(imageSamplers);

	//Lights
	std::vector<cgb::lightsource_gpu_data> lights = cgb::convert_lights_for_gpu_usage(s->mLoadedScene->lights());
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
	s->mBackgroundColor = glm::vec4(0.3, 0.3, 0.3, 0);
	s->mPerlinBackgroundBuffers.resize(fif);
	for (size_t i = 0; i < fif; ++i) {
		s->mPerlinBackgroundBuffers[i] = cgb::create_and_fill(
			cgb::uniform_buffer_meta::create_from_data(s->mBackgroundColor),
			cgb::memory_usage::host_coherent,
			&s->mBackgroundColor
		);
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
	s->mPerlinGradientBuffer = cgb::create_and_fill(
		cgb::storage_buffer_meta::create_from_size(sizeof(float) * lonsegs * latsegs * 2),
		cgb::memory_usage::host_coherent,
		gdata
	);
	delete gdata;

	//---- CREATE TLAS -----
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
		mGeometryInstances[i].set_transform(model.mTransformation);
		if (model.mLeave) {
			model.mTransparent = true;
			mGeometryInstances[i].set_instance_offset(4);
		}
		mGeometryInstances[i].mFlags = (model.mTransparent) ? vk::GeometryInstanceFlagBitsNV::eForceNoOpaque : vk::GeometryInstanceFlagBitsNV::eForceOpaque;
		mModelData[i] = model;
		++i;
	}
	auto fidx = cgb::context().main_window()->in_flight_index_for_frame();
	cgb::fill(mModelBuffers[fidx], mModelData.data());
	if (mUpdateMaterials > 0) {
		--mUpdateMaterials;
		cgb::fill(mMaterialBuffers[fidx], mGpuMaterials.data());
	}

	cgb::fill(mPerlinBackgroundBuffers[fidx], &mBackgroundColor);

	mTLASs[fidx]->update(mGeometryInstances, [](cgb::semaphore _Semaphore) {
		cgb::context().main_window()->set_extra_semaphore_dependency(std::move(_Semaphore));
	});
}