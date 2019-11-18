#pragma once
#include "includes.h"

#define MOUSE_SENTIVITY 0.001f
#define WALK_SPEED 0.075f

/*
Control for Player. Responsible for player and mirror movements
*/
class fplayercontrol : private PxUserControllerHitReport {
private:
	fscene* scene;
	fphysicscontroller* physics;
	cgb::camera* camera;
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
	fplayercontrol(fphysicscontroller* physics, fscene* scene, bool fly = false, float eyeheight = 1.0, PxUserControllerHitReport* callback = nullptr);

	void pre_px_update(float deltaT);
	void post_px_update(float deltaT);

	void look_into_direction(const glm::vec3& direction);

	void beam_along(const glm::vec3& direction);

	//updates position and direction of the character according to the camera data
	void update_position();

	void add_mirror(std::vector<PxRigidStatic*> actors, float maxDistance = 10000);
	void reset_mirrors();
	void set_final_region(PxRigidStatic* finalRegion);

	bool on_final_region();

	bool fell_down();

	void cleanup();

	void onShapeHit(const PxControllerShapeHit& hit) override;
	void onControllerHit(const PxControllersHit& hit) override;
	void onObstacleHit(const PxControllerObstacleHit& hit) override;
};