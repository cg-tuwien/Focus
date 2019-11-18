#pragma once
#include "includes.h"

class flevel1logic : public flevellogic, public PxUserControllerHitReport {
public:

	static std::string level_path() {
		return "assets/level1.dae";
	}

	flevel1logic(fscene* scene);

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
	float accTime = 0;
	glm::vec3 initialCameraPos;
	glm::quat initialCameraRot;
	std::unique_ptr<fphysicscontroller> physics;
	std::unique_ptr<fplayercontrol> player;
	fmodel* sphereInstance;
	PxRigidStatic* mirrorBorderActor;
	PxRigidStatic* mirrorPlaneActor;
	PxRigidStatic* movingFloors[3];
	PxRigidStatic* finalRegionActor;
	double score = 0;
	bool onPlatform[4] = { false, false, false, false };
};