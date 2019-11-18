#pragma once
#include "includes.h"

struct fmodel {
	size_t mModelIndex;
	std::vector<glm::vec3> mPositions;
	std::vector<glm::vec2> mTexCoords;
	std::vector<glm::vec3> mNormals;
	std::vector<glm::vec3> mTangents;
	std::vector<uint32_t> mIndices;
	glm::mat4 mTransformation;
	size_t mMaterialIndex;
	uint32_t mFlags = 0; //1 = goal, 2 = selected-mirror
	std::string mName;
	bool mTransparent = false;
	bool mLeave = false;
};

struct fmodel_gpudata {
	alignas(4) uint32_t mMaterialIndex;
	alignas(16) glm::mat4 mNormalMatrix;
	alignas(16) uint32_t mFlags = 0;

	fmodel_gpudata(const fmodel& model) {
		mMaterialIndex = static_cast<uint32_t>(model.mMaterialIndex);
		mNormalMatrix = glm::transpose(glm::inverse(model.mTransformation));
		mFlags = model.mFlags;
	}
};

struct fscene : public cgb::cg_element {

private:
	//CPU-Data
	cgb::model mLoadedScene;
	std::vector<cgb::material_config> mMaterials;
	std::vector<fmodel> mModels;
	cgb::camera mCamera;
	glm::vec4 mBackgroundColor;
	//Character
	cgb::model mCgbCharacter;
	size_t mCharacterIndex;
	int mUpdateMaterials = 0;

	//For GPU
	std::vector<fmodel_gpudata> mModelData;
	std::vector<cgb::material_gpu_data> mGpuMaterials;

	//GPU-Data (Buffers and ACs)
	//Vertex Data Buffers
	std::vector<cgb::buffer_view> mIndexBufferViews;
	std::vector<cgb::buffer_view> mTexCoordBufferViews;
	std::vector<cgb::buffer_view> mNormalBufferViews;
	std::vector<cgb::buffer_view> mTangentBufferViews;
	//Geometry Instances for TLAS
	std::vector<cgb::geometry_instance> mGeometryInstances;
	//Textures
	std::vector<cgb::image_sampler> mImageSamplers;
	//Uniform and Storage Buffers
	std::vector<cgb::storage_buffer> mMaterialBuffers;
	std::vector<cgb::storage_buffer> mModelBuffers;
	cgb::storage_buffer mLightBuffer;							//constant
	std::vector<cgb::uniform_buffer> mPerlinBackgroundBuffers;
	cgb::storage_buffer mPerlinGradientBuffer;					//constant
	//Acceleration Structures
	std::vector<cgb::bottom_level_acceleration_structure> mBLASs;
	std::vector<cgb::top_level_acceleration_structure> mTLASs;

	void create_buffers_for_model(fmodel& model, std::vector<cgb::semaphore>& blasWaitSemaphores);

	std::tuple<std::vector<cgb::material_gpu_data>, std::vector<cgb::image_sampler>> convert_for_gpu_usage(std::vector<cgb::material_config> _MaterialConfigs, std::function<void(cgb::owning_resource<cgb::semaphore_t>)> _SemaphoreHandler);

public:

	static std::unique_ptr<fscene> load_scene(const std::string& filename, const std::string& characterfilename);

	~fscene() {
		LOG_DEBUG("fscene destroyed");
	}

	const std::vector<cgb::image_sampler>& get_image_samplers() const {
		return mImageSamplers;
	}

	const cgb::storage_buffer& get_model_buffer(size_t index) const {
		return mModelBuffers[index];
	}

	const cgb::storage_buffer& get_material_buffer(size_t index) const {
		return mMaterialBuffers[index];
	}

	const cgb::storage_buffer& get_light_buffer() const {
		return mLightBuffer;
	}

	const cgb::uniform_buffer& get_background_buffer(size_t index) const {
		return mPerlinBackgroundBuffers[index];
	}

	const cgb::storage_buffer& get_gradient_buffer() const {
		return mPerlinGradientBuffer;
	}

	const std::vector<cgb::buffer_view>& get_index_buffer_views() const {
		return mIndexBufferViews;
	}

	const std::vector<cgb::buffer_view>& get_texcoord_buffer_views() const {
		return mTexCoordBufferViews;
	}

	const std::vector<cgb::buffer_view>& get_normal_buffer_views() const {
		return mNormalBufferViews;
	}

	const std::vector<cgb::buffer_view>& get_tangent_buffer_views() const {
		return mTangentBufferViews;
	}

	const std::vector<cgb::top_level_acceleration_structure>& get_tlas() const {
		return mTLASs;
	}

	cgb::camera& get_camera() {
		return mCamera;
	}

	void set_background_color(const glm::vec4& backgroundColor) {
		mBackgroundColor = backgroundColor;
	}

	void set_background_color(const glm::vec3& backgroundColor) {
		mBackgroundColor = glm::vec4(backgroundColor, 1.0f);
	}

	fmodel* get_model_by_name(const std::string& name);

	void set_character_position(const glm::vec3& position);

	void update() override;

	int32_t execution_order() const override {
		return 3;
	}

	cgb::material_gpu_data& get_material_data(size_t materialIndex);
};