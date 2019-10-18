#pragma once
#include "includes.h"

class frenderer {
private:
	fscene* mScene;

	std::vector<cgb::image_view> mOffscreenImageViews;
	cgb::ray_tracing_pipeline mPipeline;
	std::vector<std::shared_ptr<cgb::descriptor_set>> mDescriptorSet;

public:
	frenderer(fscene* scene) : mScene(scene) {}

	void initialize();

	void render(cgb::cg_element* element, const glm::mat4& viewMatrix);
};