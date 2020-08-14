#pragma once
#include "includes.h"

/*
Represents a single model in the scene
*/
struct fmodel {
	size_t mModelIndex;					//Index of this object in the scene's model array
	std::vector<glm::vec3> mPositions;	//List of vertex positions
	std::vector<glm::vec2> mTexCoords;	//List of texture coordinates
	std::vector<glm::vec3> mNormals;	//List of normals
	std::vector<glm::vec3> mTangents;	//List of tangents
	std::vector<uint32_t> mIndices;		//List of indices
	glm::mat4 mTransformation;			//Transformation matrix
	size_t mMaterialIndex;				//Index of the object's material in the scene's material array
	uint32_t mFlags = 0;				//1 = object is focusphere, 2 = object is selected (mirror)
	std::string mName;					//name of the object
	bool mTransparent = false;			//if true, object will be set to non-opaque in the acceleration structure
	bool mLeaf = false;					//if true, the leaf-shader will be used to render this
};

/*
GPU-Representation of a model contianing only the data needed on the GPU with correct alignment
*/
struct fmodel_gpu_data {
	alignas(4) uint32_t mMaterialIndex;	//Material index
	alignas(16) glm::mat4 mNormalMatrix;//Normal transformation index
	alignas(16) uint32_t mFlags = 0;	//Flags (see above)

	//Creates the gpudata for a given fmodel
	fmodel_gpu_data(const fmodel& model) {
		mMaterialIndex = static_cast<uint32_t>(model.mMaterialIndex);
		mNormalMatrix = glm::transpose(glm::inverse(model.mTransformation));
		mFlags = model.mFlags;
	}
};

/*
Represents a whole scene with all contained objects, lights, materials etc.
Also manages the GPU-buffers relevant to the scene
*/
struct fscene : public gvk::invokee {

private:
	//CPU-Data
	gvk::model mLoadedScene;						//cgbase-object for reading in the scene data
	std::vector<gvk::material_config> mMaterials;	//List of materials (using cgbase's material representation)
	std::vector<fmodel> mModels;					//List of models
	gvk::camera mCamera;							//Camera object
	glm::vec4 mBackgroundColor;						//Current background color of the scene
	int mUpdateMaterials = 0;						//Whether the materials have to be updated (as a decrementing frame-counter)
	//Character
	gvk::model mCgbCharacter;						//Character model
	size_t mCharacterIndex;							//Index of the character model in the models-array

	//For GPU
	std::vector<fmodel_gpu_data> mModelData;				//List of model-gpu-data
	std::vector<gvk::material_gpu_data> mGpuMaterials;		//List of material-gpu-data

	//GPU-Data (Buffers and ACs)
	//Buffer Views
	std::vector<avk::buffer_view> mIndexBufferViews;		//Index buffer view for each model
	std::vector<avk::buffer_view> mTexCoordBufferViews;		//Texture coordinates buffer view for each model
	std::vector<avk::buffer_view> mNormalBufferViews;		//Normal buffer view for each model
	std::vector<avk::buffer_view> mTangentBufferViews;		//Tangent buffer view for each model
	//Various
	std::vector<avk::geometry_instance> mGeometryInstances;	//Geometry Instances for TLAS
	std::vector<avk::image_sampler> mImageSamplers;			//Textures
	//Uniform and Storage Buffers
	std::vector<avk::buffer> mMaterialBuffers;			//Material buffers, one per frame in flight
	std::vector<avk::buffer> mModelBuffers;				//Model buffers, one per frame in flight
	std::vector<avk::buffer> mPerlinBackgroundBuffers;	//Background color buffer, one per frame in flight
	avk::buffer mLightBuffer;							//Light source buffer, only one, because constant
	avk::buffer mPerlinGradientBuffer;					//Perlin gradients buffer for background, only one, because constant
	//Acceleration Structures
	std::vector<avk::bottom_level_acceleration_structure> mBLASs;	//Bottom Level Acceleration Structures (only once, constant)
	std::vector<avk::top_level_acceleration_structure> mTLASs;		//Top Level Acceleration Structures (one per frame in flight)

	//Help-function
	void create_buffers_for_model(fmodel& model);

public:

	//Loads the scene
	//filename: Path to the scene collada file
	//charachterfilename: Path to the character collada file
	/*
	Creates a fscene object for a given scene. Fills in the data and creates the buffers
	filename: Path to the scene collada file
	characterfilename: Path to the character collada file
	*/
	static std::unique_ptr<fscene> load_scene(const std::string& filename, const std::string& characterfilename);

	//----------------------
	//---Getter Functions---
	//----------------------

	const std::vector<avk::image_sampler>& get_image_samplers() const {
		return mImageSamplers;
	}

	const avk::buffer& get_model_buffer(size_t index) const {
		return mModelBuffers[index];
	}

	const avk::buffer& get_material_buffer(size_t index) const {
		return mMaterialBuffers[index];
	}

	const avk::buffer& get_light_buffer() const {
		return mLightBuffer;
	}

	const avk::buffer& get_background_buffer(size_t index) const {
		return mPerlinBackgroundBuffers[index];
	}

	const avk::buffer& get_gradient_buffer() const {
		return mPerlinGradientBuffer;
	}

	const std::vector<avk::buffer_view>& get_index_buffer_views() const {
		return mIndexBufferViews;
	}

	const std::vector<avk::buffer_view>& get_texcoord_buffer_views() const {
		return mTexCoordBufferViews;
	}

	const std::vector<avk::buffer_view>& get_normal_buffer_views() const {
		return mNormalBufferViews;
	}

	const std::vector<avk::buffer_view>& get_tangent_buffer_views() const {
		return mTangentBufferViews;
	}

	const std::vector<avk::top_level_acceleration_structure>& get_tlas() const {
		return mTLASs;
	}

	gvk::camera& get_camera() {
		return mCamera;
	}

	fmodel* get_model_by_name(const std::string& name);

	/*This function has a side effect. If this is called, it is assumed that the returned material will be changed
	and the gpu buffer will be updated in the next frames
	*/
	gvk::material_gpu_data& get_material_data(size_t materialIndex);

	//----------------------

	//Setter for background color
	void set_background_color(const glm::vec4& backgroundColor) {
		mBackgroundColor = backgroundColor;
	}

	//Convenience function
	void set_background_color(const glm::vec3& backgroundColor) {
		mBackgroundColor = glm::vec4(backgroundColor, 1.0f);
	}

	//Sets the current position of the character
	void set_character_position(const glm::vec3& position);

	//Updates all model data on the GPU, as well as the background color buffer. The material buffer is updated if needed
	void update() override;

	int32_t execution_order() const override {
		return 3;
	}
};