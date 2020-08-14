//Author: Simon Fraiss
#pragma once
#include "includes.h"

#define MOUSE_SENTIVITY 0.001f
#define WALK_SPEED 0.075f

/*
Control for Player. Implements the player and mirror movements.
Can be used by the level logic objects.
*/
class fplayercontrol : private PxUserControllerHitReport {
private:
	fscene* scene;
	fphysicscontroller* physics;
	gvk::camera* camera;
	PxController* cameraController;
	PxUserControllerHitReport* additionalCallback;
	std::vector<std::vector<PxRigidStatic*>> mirrorActors;
	std::vector<std::vector<PxTransform>> mirrorOriginalTransformations;
	std::vector<float> mirrorMaxDistances;
	PxRigidStatic* finalRegion = nullptr;

	double lastCursorX = NAN, lastCursorY = NAN;
	float horizontalAngle = 0;
	float verticalAngle = 0;
	float jumpystart = -2;
	float jump = -1;
	PxRigidActor* jumpbase = nullptr;
	bool fly = false;
	float eyeheight;
	int movingMirror = -1;
	bool onfinalregion = false;

	int raycastMirror();

public:
	/*
	Constructor.
	Parameters:
	physics: Physics Controller to use
	scene: Scene Object
	fly: If flymode should be activated or not (default: false)
	eyehight: How high above the ground the camera should be (default: 1.0)
	callback: Callback to call when character touches something
	*/
	fplayercontrol(fphysicscontroller* physics, fscene* scene, bool fly = false, float eyeheight = 1.0, PxUserControllerHitReport* callback = nullptr);

	//Update-Functions update the player and the mirrors
	//This method should be called before the physics-update
	void pre_px_update(float deltaT);
	//This method should be called after the physics-update
	void post_px_update(float deltaT);
	//This method should be called during update (only mouse motion)
	void update(float deltaT);

	//Makes the player look into a specific direction
	void look_into_direction(const glm::vec3& direction);

	//Moves the player along the given direction
	void beam_along(const glm::vec3& direction);

	//Updates position and direction of the character according to the camera data. Should be called when changing the camera data
	void update_position();

	//Registers a mirror to move. actors is a list of actors belonging to the mirror (e.g. border and plane)
	//maxDistance is the maximum distance from which the mirror can be moved
	void add_mirror(std::vector<PxRigidStatic*> actors, float maxDistance = 10000);
	//Resets the mirrors to their original positions
	void reset_mirrors();
	//Sets the final region
	void set_final_region(PxRigidStatic* finalRegion);
	//Returns true if the player stands on the final region
	bool on_final_region();
	//Returns true, if the players y coordinate is below 20
	bool fell_down();
	//Cleans up physx
	void cleanup();

	void onShapeHit(const PxControllerShapeHit& hit) override;
	void onControllerHit(const PxControllersHit& hit) override;
	void onObstacleHit(const PxControllerObstacleHit& hit) override;
};