#pragma once
#include "includes.h"

class flevel2logic : public flevellogic {
public:

	flevel2logic(fscene* scene);

	levelstatus update(float deltaT, double focusHitCount) override;
	void fixed_update(float stepSize) override;

	void reset();

	void cleanup() override {
		player->cleanup();
		mirrorBorderActor->release();
		mirrorPlaneActor->release();
		physics->cleanup();
	}

private:
	/*HsvInterpolator interpolator;*/
	float accTime = 0;
	glm::vec3 initialCameraPos;
	glm::quat initialCameraRot;
	std::unique_ptr<fphysicscontroller> physics;
	std::unique_ptr<fplayercontrol> player;
	//ModelInstance* sphereInstance;
	PxRigidStatic* mirrorBorderActor;
	PxRigidStatic* mirrorPlaneActor;
	PxRigidStatic* finalFloorActor;
	PxRigidStatic* movingWallActor;
	PxRigidStatic* finalRegionActor;
	float wallMovingRightStart = -1.0f;
	float wallMovingLeftStart = -1.0f;
	/*double score = 0;*/
};