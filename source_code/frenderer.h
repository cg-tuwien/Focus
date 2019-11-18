#pragma once
#include "includes.h"

class frenderer : public cgb::cg_element {
private:
	fscene* mScene = nullptr;
	flevellogic* mLevelLogic = nullptr;

	std::vector<cgb::image_view> mOffscreenImageViews;
	cgb::ray_tracing_pipeline mPipeline;
	std::vector<std::shared_ptr<cgb::descriptor_set>> mDescriptorSet;
	cgb::storage_buffer mFocusHitBuffer;
	float fadeValue = 0.0f;
	cgb::uniform_buffer mFadeBuffer;

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