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
};

struct fmodel_gpudata {
	alignas(4) uint32_t mMaterialIndex;
	alignas(16) glm::mat4 mNormalMatrix;
	//alignas(16) uint32_t mFlags;

	fmodel_gpudata(const fmodel& model) {
		mMaterialIndex = static_cast<uint32_t>(model.mMaterialIndex);
		mNormalMatrix = glm::transpose(glm::inverse(model.mTransformation));
	}
};

struct fscene {

private:
	//CPU-Data
	cgb::model mLoadedScene;
	std::vector<cgb::material_config> mMaterials;
	std::vector<fmodel> mModels;

	//For GPU
	std::vector<fmodel_gpudata> mModelData;

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
	cgb::storage_buffer mMaterialBuffer;
	cgb::storage_buffer mModelBuffer;
	cgb::storage_buffer mLightBuffer;
	cgb::uniform_buffer mPerlinBackgroundBuffer;
	cgb::storage_buffer mPerlinGradientBuffer;
	//Acceleration Structures
	std::vector<cgb::bottom_level_acceleration_structure> mBLASs;
	std::vector<cgb::top_level_acceleration_structure> mTLASs;

public:

	static std::unique_ptr<fscene> load_scene(const std::string& filename);

	const std::vector<cgb::image_sampler>& get_image_samplers() const {
		return mImageSamplers;
	}

	const cgb::storage_buffer& get_model_buffer() const {
		return mModelBuffer;
	}

	const cgb::storage_buffer& get_material_buffer() const {
		return mMaterialBuffer;
	}

	const cgb::storage_buffer& get_light_buffer() const {
		return mLightBuffer;
	}

	const cgb::uniform_buffer& get_background_buffer() const {
		return mPerlinBackgroundBuffer;
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
};