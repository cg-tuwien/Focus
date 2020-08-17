#pragma once
#include "includes.h"

/*
Renderer class. Responsible for rendering the image and everything related to that (creating descriptor sets, command buffers etc.).
Also manages the focus hit count buffer and the fade buffer.
*/
class frenderer : public gvk::invokee {
private:
	avk::queue* mQueue;
	avk::descriptor_cache mDescriptorCache;
	fscene* mScene = nullptr;
	flevellogic* mLevelLogic = nullptr;

	avk::ray_tracing_pipeline mPipeline;
	//We need each of the following several times, as we have several frames in flight
	std::vector<avk::image_view> mOffscreenImageViews;
	std::vector<avk::buffer> mFocusHitBuffers;
	std::vector<avk::buffer> mFadeBuffers;
	float fadeValue = 0.0f;

public:
	frenderer() {}
	frenderer(fscene* scene, flevellogic* levellogic) : mScene(scene), mLevelLogic(levellogic) {}

	//Initializes image views, pipeline, buffers, descriptor sets...
	void initialize();

	//Writes to FadeBuffer and reads from FocusHitBuffer and passes the value to level logic
	void update() override;

	//Starts rendering
	void render() override;

	//Sets the current fade-value
	void set_fade_value(float val) { fadeValue = val; }

	//Execution order per frame: Game Control, Level Logic, Scene, Renderer
	int32_t execution_order() const override {
		return 4;
	}

	//Sets the scene and updates descriptor sets and pipeline
	void set_scene(fscene* scene);
	//Sets the level logic
	void set_level_logic(flevellogic* levellogic) {
		mLevelLogic = levellogic;
	}

	void set_queue(avk::queue* q) {
		mQueue = q;
	}

private:
	//Is called when setting a new scene. Updates the descriptor sets and pipeline
	void create_descriptor_sets_for_scene();
};