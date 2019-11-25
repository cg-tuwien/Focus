//Author: Simon Fraiss
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

	//PxUserControllerHitReport-Callbacks
	void onShapeHit(const PxControllerShapeHit& hit) override;
	void onControllerHit(const PxControllersHit& hit) override {};
	void onObstacleHit(const PxControllerObstacleHit& hit) override {};

private:
	std::unique_ptr<fphysicscontroller> physics;			//Physics controller
	std::unique_ptr<fplayercontrol> player;					//Player controller
	glm::vec3 initialCameraPos;								//Initial camera position
	glm::quat initialCameraRot;								//Initial camera rotation
	fmodel* sphereInstance;									//Focusphere model data
	PxRigidStatic* mirrorBorder1Actor;						//Actor for mirror 1 border
	PxRigidStatic* mirrorPlane1Actor;						//Actor for mirror 1 plane
	PxRigidStatic* mirrorBorder2Actor;						//Actor for mirror 2 border
	PxRigidStatic* mirrorPlane2Actor;						//Actor for mirror 2 plane
	PxRigidStatic* platformActors[4];						//Actors for moving platforms
	PxRigidStatic* finalRegionActor;						//Actor for goal platform
	bool onPlatform[4] = { false, false, false, false };	//Whether the player touches a platform
	float accTime = 0;										//Accumulated time since start
	double score = 0;										//Current score
};