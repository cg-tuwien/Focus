//Author: Simon Fraiss
#pragma once
#include "includes.h"

//Level Logic for Focus! Level 2
class flevel2logic : public flevellogic {
public:

	static std::string level_path() {
		return "assets/level2.dae";
	}

	flevel2logic(fscene* scene);

	void initialize() override;

	levelstatus update(float deltaT, double focusHitValue) override;
	void fixed_update(float stepSize) override;

	void reset();

	void finalize() override {
		player->cleanup();
		physics->cleanup();
	}

private:
	std::unique_ptr<fphysicscontroller> physics;		//Physics controller
	std::unique_ptr<fplayercontrol> player;				//Player controller
	hsvinterpolator interpolator;						//Interpolator for background interpolation
	glm::vec3 initialCameraPos;							//Initial camera position
	glm::quat initialCameraRot;							//Initial camera rotation
	fmodel* sphereInstance;								//Focusphere model data
	PxRigidStatic* mirrorBorderActor;					//Actor for mirror border
	PxRigidStatic* mirrorPlaneActor;					//Actor for mirror plane
	PxRigidStatic* movingWallActor;						//Actor for moving wall
	PxRigidStatic* finalFloorActor;						//Actor for last floor contianing the goal floor
	PxRigidStatic* finalRegionActor;					//Actor for goal floor
	float wallMovingRightStart = -1.0f;					//Used for animating the wall
	float wallMovingLeftStart = -1.0f;					//Used for animating the wall
	float accTime = 0;									//Accumulated time since start
	double score = 0;									//Current score
};