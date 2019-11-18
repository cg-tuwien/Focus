#pragma once
#include "includes.h"

/*
Renderer class. Responsible for rendering the image and everything related to that (creating descriptor sets, command buffers etc.).
Also manages the focus hit count buffer and the fade buffer.
*/
class frenderer : public cgb::cg_element {
private:
	fscene* mScene = nullptr;
	flevellogic* mLevelLogic = nullptr;

	std::vector<cgb::image_view> mOffscreenImageViews;
	cgb::ray_tracing_pipeline mPipeline;
	std::vector<std::shared_ptr<cgb::descriptor_set>> mDescriptorSet;
	std::vector<cgb::storage_buffer> mFocusHitBuffers;
	std::vector<cgb::uniform_buffer> mFadeBuffers;
	float fadeValue = 0.0f;

public:
	frenderer() {}
	frenderer(fscene* scene, flevellogic* levellogic) : mScene(scene), mLevelLogic(levellogic) {}

	void initialize();

	void update() override;

	void render() override;

	void set_fade_value(float val) { fadeValue = val; }


	int32_t execution_order() const override {
		return 4;
	}

	void set_scene(fscene* scene);
	void set_level_logic(flevellogic* levellogic) {
		mLevelLogic = levellogic;
	}

private:
	void create_descriptor_sets_for_scene();
};