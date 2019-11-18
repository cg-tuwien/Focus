#pragma once
#include "includes.h"

class flevel3logic : public flevellogic, private PxUserControllerHitReport {
public:

	static std::string level_path() {
		return "assets/level3.dae";
	}

	flevel3logic(fscene* scene);

	void initialize() override;

	levelstatus update(float deltaT, double focusHitValue) override;
	void fixed_update(float stepSize) override;

	void reset();

	void finalize() override {
		player->cleanup();
		physics->cleanup();
	}

	void onShapeHit(const PxControllerShapeHit& hit) override;
	void onControllerHit(const PxControllersHit& hit) override {};
	void onObstacleHit(const PxControllerObstacleHit& hit) override {};

private:
	hsvinterpolator interpolator;
	glm::vec3 initialCameraPos;
	glm::quat initialCameraRot;
	std::unique_ptr<fphysicscontroller> physics;
	std::unique_ptr<fplayercontrol> player;
	fmodel* sphereInstance;
	PxRigidStatic* mirrorBorderActor;
	PxRigidStatic* mirrorPlaneActor;
	PxRigidStatic* movingFloorActor;
	PxRigidStatic* movingWallActor;
	PxTransform movingWallPxOriginalTransformation;
	PxTransform mirrorPlaneOriginalTransform;
	PxTransform mirrorBorderOriginalTransform;
	double score = 0;
	bool onPlatform = false;
	bool platformMoving = false;
	float platformAccTime = 0;
};