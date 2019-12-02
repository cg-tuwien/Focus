//Author: Simon Fraiss
#pragma once
#include "includes.h"

//Level Logic for Focus! Level 1
class flevel1logic : public flevellogic, public PxUserControllerHitReport {
public:

	static std::string level_path() {
		return "assets/level1g.dae";
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

	//PxUserControllerHitReport-Callbacks
	void onShapeHit(const PxControllerShapeHit& hit) override;
	void onControllerHit(const PxControllersHit& hit) override {};
	void onObstacleHit(const PxControllerObstacleHit& hit) override {};

private:
	std::unique_ptr<fphysicscontroller> physics;			//Physics controller
	std::unique_ptr<fplayercontrol> player;					//Player controller
	hsvinterpolator interpolator;							//Interpolator for background interpolation
	glm::vec3 initialCameraPos;								//Initial camera position
	glm::quat initialCameraRot;								//Initial camera rotation
	fmodel* sphereInstance;									//Focusphere model data
	PxRigidStatic* mirrorBorderActor;						//Actor for mirror border
	PxRigidStatic* mirrorPlaneActor;						//Actor for mirror plane
	PxRigidStatic* movingFloors[3];							//Actors for moving floors (except goal floor)
	PxRigidStatic* finalRegionActor;						//Actor for goal floor
	float accTime = 0;										//Accumulated time since start
	double score = 0;										//Current score
	bool onPlatform[4] = { false, false, false, false };	//Whether the player touches a platform
};