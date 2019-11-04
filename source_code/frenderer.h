#pragma once
#include "includes.h"

class frenderer : public cgb::cg_element {
private:
	fscene* mScene;

	std::vector<cgb::image_view> mOffscreenImageViews;
	cgb::ray_tracing_pipeline mPipeline;
	std::vector<std::shared_ptr<cgb::descriptor_set>> mDescriptorSet;
	std::vector<cgb::command_buffer> mCommandBuffers;
	cgb::storage_buffer mFocusHitBuffer;

public:
	frenderer(fscene* scene) : mScene(scene) {}

	void initialize();

	void render() override;


	int32_t priority() const override {
		return 2;
	}

	void set_scene(fscene* scene);

private:
	void create_descriptor_sets_for_scene();

	void record_command_buffers();
};