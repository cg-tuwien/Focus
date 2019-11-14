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
	mUpdateMaterials = 1;// cgb::context().main_window()->number_of_in_flight_frames();
	return mGpuMaterials[materialIndex];
}

std::unique_ptr<fscene> fscene::load_scene(const std::string& filename, const std::string& characterfilename)
{
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

	s->mModelBuffer = cgb::create_and_fill(
		cgb::storage_buffer_meta::create_from_data(s->mModelData),
		cgb::memory_usage::host_coherent,
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
	s->mPerlinBackgroundBuffer = cgb::create_and_fill(
		cgb::uniform_buffer_meta::create_from_data(s->mBackgroundColor),
		cgb::memory_usage::host_coherent,
		&s->mBackgroundColor
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
	cgb::fill(mModelBuffer, mModelData.data());
	if (mUpdateMaterials > 0) {
		--mUpdateMaterials;
		cgb::fill(mMaterialBuffer, mGpuMaterials.data());
	}

	cgb::fill(mPerlinBackgroundBuffer, &mBackgroundColor);

	auto inFlightIndex = cgb::context().main_window()->in_flight_index_for_frame();
	mTLASs[inFlightIndex]->update(mGeometryInstances, [](cgb::semaphore _Semaphore) {
		cgb::context().main_window()->set_extra_semaphore_dependency(std::move(_Semaphore));
	});
}



//std::tuple<std::vector<cgb::material_gpu_data>, std::vector<cgb::image_sampler>> fscene::convert_for_gpu_usage(std::vector<cgb::material_config> _MaterialConfigs, std::function<void(cgb::owning_resource<cgb::semaphore_t>)> _SemaphoreHandler)
//{
//	// These are the texture names loaded from file:
//	std::unordered_map<std::string, std::vector<int*>> texNamesToUsages;
//	std::set<std::string> srgbtextures;
//	// However, if some textures are missing, provide 1x1 px textures in those spots
//	std::vector<int*> whiteTexUsages;				// Provide a 1x1 px almost everywhere in those cases,
//	std::vector<int*> straightUpNormalTexUsages;	// except for normal maps, provide a normal pointing straight up there.
//
//	std::vector<cgb::material_gpu_data> gpuMaterial;
//	gpuMaterial.reserve(_MaterialConfigs.size()); // important because of the pointers
//
//	for (auto& mc : _MaterialConfigs) {
//		auto& gm = gpuMaterial.emplace_back();
//		gm.mDiffuseReflectivity = mc.mDiffuseReflectivity;
//		gm.mAmbientReflectivity = mc.mAmbientReflectivity;
//		gm.mSpecularReflectivity = mc.mSpecularReflectivity;
//		gm.mEmissiveColor = mc.mEmissiveColor;
//		gm.mTransparentColor = mc.mTransparentColor;
//		gm.mReflectiveColor = mc.mReflectiveColor;
//		gm.mAlbedo = mc.mAlbedo;
//
//		gm.mOpacity = mc.mOpacity;
//		gm.mBumpScaling = mc.mBumpScaling;
//		gm.mShininess = mc.mShininess;
//		gm.mShininessStrength = mc.mShininessStrength;
//
//		gm.mRefractionIndex = mc.mRefractionIndex;
//		gm.mReflectivity = mc.mReflectivity;
//		gm.mMetallic = mc.mMetallic;
//		gm.mSmoothness = mc.mSmoothness;
//
//		gm.mSheen = mc.mSheen;
//		gm.mThickness = mc.mThickness;
//		gm.mRoughness = mc.mRoughness;
//		gm.mAnisotropy = mc.mAnisotropy;
//
//		gm.mAnisotropyRotation = mc.mAnisotropyRotation;
//		gm.mCustomData = mc.mCustomData;
//
//		gm.mDiffuseTexIndex = -1;
//		if (mc.mDiffuseTex.empty()) {
//			whiteTexUsages.push_back(&gm.mDiffuseTexIndex);
//		}
//		else {
//			texNamesToUsages[cgb::clean_up_path(mc.mDiffuseTex)].push_back(&gm.mDiffuseTexIndex);
//			srgbtextures.insert(cgb::clean_up_path(mc.mDiffuseTex));
//		}
//
//		gm.mSpecularTexIndex = -1;
//		if (mc.mSpecularTex.empty()) {
//			whiteTexUsages.push_back(&gm.mSpecularTexIndex);
//		}
//		else {
//			texNamesToUsages[cgb::clean_up_path(mc.mSpecularTex)].push_back(&gm.mSpecularTexIndex);
//			srgbtextures.insert(cgb::clean_up_path(mc.mSpecularTex));
//		}
//
//		gm.mAmbientTexIndex = -1;
//		if (mc.mAmbientTex.empty()) {
//			whiteTexUsages.push_back(&gm.mAmbientTexIndex);
//		}
//		else {
//			texNamesToUsages[cgb::clean_up_path(mc.mAmbientTex)].push_back(&gm.mAmbientTexIndex);
//			srgbtextures.insert(cgb::clean_up_path(mc.mAmbientTex));
//		}
//
//		gm.mEmissiveTexIndex = -1;
//		if (mc.mEmissiveTex.empty()) {
//			whiteTexUsages.push_back(&gm.mEmissiveTexIndex);
//		}
//		else {
//			texNamesToUsages[cgb::clean_up_path(mc.mEmissiveTex)].push_back(&gm.mEmissiveTexIndex);
//		}
//
//		gm.mHeightTexIndex = -1;
//		if (mc.mHeightTex.empty()) {
//			whiteTexUsages.push_back(&gm.mHeightTexIndex);
//		}
//		else {
//			texNamesToUsages[cgb::clean_up_path(mc.mHeightTex)].push_back(&gm.mHeightTexIndex);
//		}
//
//		gm.mNormalsTexIndex = -1;
//		if (mc.mNormalsTex.empty()) {
//			straightUpNormalTexUsages.push_back(&gm.mNormalsTexIndex);
//		}
//		else {
//			texNamesToUsages[cgb::clean_up_path(mc.mNormalsTex)].push_back(&gm.mNormalsTexIndex);
//		}
//
//		gm.mShininessTexIndex = -1;
//		if (mc.mShininessTex.empty()) {
//			whiteTexUsages.push_back(&gm.mShininessTexIndex);
//		}
//		else {
//			texNamesToUsages[cgb::clean_up_path(mc.mShininessTex)].push_back(&gm.mShininessTexIndex);
//		}
//
//		gm.mOpacityTexIndex = -1;
//		if (mc.mOpacityTex.empty()) {
//			whiteTexUsages.push_back(&gm.mOpacityTexIndex);
//		}
//		else {
//			texNamesToUsages[cgb::clean_up_path(mc.mOpacityTex)].push_back(&gm.mOpacityTexIndex);
//		}
//
//		gm.mDisplacementTexIndex = -1;
//		if (mc.mDisplacementTex.empty()) {
//			whiteTexUsages.push_back(&gm.mDisplacementTexIndex);
//		}
//		else {
//			texNamesToUsages[cgb::clean_up_path(mc.mDisplacementTex)].push_back(&gm.mDisplacementTexIndex);
//		}
//
//		gm.mReflectionTexIndex = -1;
//		if (mc.mReflectionTex.empty()) {
//			whiteTexUsages.push_back(&gm.mReflectionTexIndex);
//		}
//		else {
//			texNamesToUsages[cgb::clean_up_path(mc.mReflectionTex)].push_back(&gm.mReflectionTexIndex);
//		}
//
//		gm.mLightmapTexIndex = -1;
//		if (mc.mLightmapTex.empty()) {
//			whiteTexUsages.push_back(&gm.mLightmapTexIndex);
//		}
//		else {
//			texNamesToUsages[cgb::clean_up_path(mc.mLightmapTex)].push_back(&gm.mLightmapTexIndex);
//		}
//
//		gm.mExtraTexIndex = -1;
//		if (mc.mExtraTex.empty()) {
//			whiteTexUsages.push_back(&gm.mExtraTexIndex);
//		}
//		else {
//			texNamesToUsages[cgb::clean_up_path(mc.mExtraTex)].push_back(&gm.mExtraTexIndex);
//		}
//
//		gm.mDiffuseTexOffsetTiling = mc.mDiffuseTexOffsetTiling;
//		gm.mSpecularTexOffsetTiling = mc.mSpecularTexOffsetTiling;
//		gm.mAmbientTexOffsetTiling = mc.mAmbientTexOffsetTiling;
//		gm.mEmissiveTexOffsetTiling = mc.mEmissiveTexOffsetTiling;
//		gm.mHeightTexOffsetTiling = mc.mHeightTexOffsetTiling;
//		gm.mNormalsTexOffsetTiling = mc.mNormalsTexOffsetTiling;
//		gm.mShininessTexOffsetTiling = mc.mShininessTexOffsetTiling;
//		gm.mOpacityTexOffsetTiling = mc.mOpacityTexOffsetTiling;
//		gm.mDisplacementTexOffsetTiling = mc.mDisplacementTexOffsetTiling;
//		gm.mReflectionTexOffsetTiling = mc.mReflectionTexOffsetTiling;
//		gm.mLightmapTexOffsetTiling = mc.mLightmapTexOffsetTiling;
//		gm.mExtraTexOffsetTiling = mc.mExtraTexOffsetTiling;
//	}
//
//	std::vector<cgb::image_sampler> imageSamplers;
//	imageSamplers.reserve(texNamesToUsages.size() + 2); // + 2 => one for the white tex, one for the normals tex
//
//	// Create the white texture and assign its index to all usages
//	if (whiteTexUsages.size() > 0) {
//		imageSamplers.push_back(
//			cgb::image_sampler_t::create(
//				cgb::image_view_t::create(create_1px_texture({ 255, 255, 255, 255 }, cgb::memory_usage::device, cgb::image_usage::read_only_sampled_image, _SemaphoreHandler)),
//				cgb::sampler_t::create(cgb::filter_mode::nearest_neighbor, cgb::border_handling_mode::repeat)
//			)
//		);
//		int index = static_cast<int>(imageSamplers.size() - 1);
//		for (auto* img : whiteTexUsages) {
//			*img = index;
//		}
//	}
//
//	// Create the normal texture, containing a normal pointing straight up, and assign to all usages
//	if (straightUpNormalTexUsages.size() > 0) {
//		imageSamplers.push_back(
//			cgb::image_sampler_t::create(
//				cgb::image_view_t::create(create_1px_texture({ 127, 127, 255, 0 }, cgb::memory_usage::device, cgb::image_usage::read_only_sampled_image, _SemaphoreHandler)),
//				cgb::sampler_t::create(cgb::filter_mode::nearest_neighbor, cgb::border_handling_mode::repeat)
//			)
//		);
//		int index = static_cast<int>(imageSamplers.size() - 1);
//		for (auto* img : straightUpNormalTexUsages) {
//			*img = index;
//		}
//	}
//
//	// Load all the images from file, and assign them to all usages
//	for (auto& pair : texNamesToUsages) {
//		assert(!pair.first.empty());
//
//		cgb::settings::gLoadImagesInSrgbFormatByDefault = srgbtextures.contains(pair.first);
//
//		imageSamplers.push_back(
//			cgb::image_sampler_t::create(
//				cgb::image_view_t::create(create_image_from_file(
//					pair.first,
//					{}, // <-- let the format be determined automatically
//					cgb::memory_usage::device, cgb::image_usage::read_only_sampled_image,
//					_SemaphoreHandler)),
//				cgb::sampler_t::create(cgb::filter_mode::bilinear, cgb::border_handling_mode::repeat)
//			)
//		);
//		int index = static_cast<int>(imageSamplers.size() - 1);
//		for (auto* img : pair.second) {
//			*img = index;
//		}
//	}
//
//	// Hand over ownership to the caller
//	return std::make_tuple(std::move(gpuMaterial), std::move(imageSamplers));
//}