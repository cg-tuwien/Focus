#pragma once
#include "includes.h"

class flevel1logic : public flevellogic, public PxUserControllerHitReport {
public:

	flevel1logic(fscene* scene);

	levelstatus update(const float& deltaT, const double& focusHitValue) override;
	void fixed_update(const float& stepSize) override;

	void reset();

	void cleanup() override {
		player->cleanup();
		/*mirrorBorderActor->release();
		mirrorPlaneActor->release();*/
		physics->cleanup();
	}

	void onShapeHit(const PxControllerShapeHit& hit) override;
	void onControllerHit(const PxControllersHit& hit) override {};
	void onObstacleHit(const PxControllerObstacleHit& hit) override {};

private:
	/*HsvInterpolator interpolator;*/
	float accTime = 0;
	glm::vec3 initialCameraPos;
	glm::quat initialCameraRot;
	std::unique_ptr<fphysicscontroller> physics;
	std::unique_ptr<fplayercontrol> player;
	/*ModelInstance* sphereInstance;
	PxRigidStatic* mirrorBorderActor;
	PxRigidStatic* mirrorPlaneActor;*/
	PxRigidStatic* movingFloors[3];
	PxRigidStatic* finalRegionActor;
	/*double score = 0;*/
	bool onPlatform[4] = { false, false, false, false };
};