//Author: Simon Fraiss
#pragma once
#include "includes.h"

class flevel3logic : public flevellogic, private PxUserControllerHitReport {
public:

	static std::string level_path() {
		return "assets/level3g.dae";
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

	//PxUserControllerHitReport-Callbacks
	void onShapeHit(const PxControllerShapeHit& hit) override;
	void onControllerHit(const PxControllersHit& hit) override {};
	void onObstacleHit(const PxControllerObstacleHit& hit) override {};

private:
	std::unique_ptr<fphysicscontroller> physics;		//Physics controller
	std::unique_ptr<fplayercontrol> player;				//Player controller
	hsvinterpolator interpolator;						//Interpolator for background interpolation
	glm::vec3 initialCameraPos;							//Initial camera position
	glm::quat initialCameraRot;							//Initial camera rotation
	fmodel* sphereInstance;								//Focusphere model data
	PxRigidStatic* mirrorBorderActor;					//Actor for moveable mirror border
	PxRigidStatic* mirrorPlaneActor;					//Actor for moveable mirror plane
	PxRigidStatic* movingFloorActor;					//Actor for moving goal floor
	PxRigidStatic* movingWallActor;						//Actor for moving wall
	PxTransform movingWallPxOriginalTransformation;		//Initial transformation of moving wall actor
	bool onPlatform = false;							//Whether the player touches the goal paltform
	bool platformMoving = false;						//Whether the goal platform is in motion
	float platformAccTime = 0;							//Time since start of platform movement
	double score = 0;									//Current score
};