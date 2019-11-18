#pragma once
#include "includes.h"

class flevel4logic : public flevellogic, private PxUserControllerHitReport {
public:

	static std::string level_path() {
		return "assets/level4.dae";
	}

	flevel4logic(fscene* scene);

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
	PxRigidStatic* mirrorBorder1Actor;
	PxTransform mirrorBorder1OriginalTransform;
	PxRigidStatic* mirrorPlane1Actor;
	PxTransform mirrorPlane1OriginalTransform;
	PxRigidStatic* mirrorBorder2Actor;
	PxTransform mirrorBorder2OriginalTransform;
	PxRigidStatic* mirrorPlane2Actor;
	PxTransform mirrorPlane2OriginalTransform;
	PxRigidStatic* platformActors[4];
	PxRigidStatic* finalRegionActor;
	bool onPlatform[4] = { false, false, false, false };
	double score = 0;
	float accTime = 0;
};