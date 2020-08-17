//Author: Simon Fraiss
#include "includes.h"

flevel1logic::flevel1logic(fscene* scene) : flevellogic(scene) {
	
}

void flevel1logic::initialize() {
	//---SAVE INITIAL CAMERA DATA FOR LEVEL RESET---
	initialCameraPos = mScene->get_camera().translation();
	initialCameraRot = mScene->get_camera().rotation();

	//---CREATE PHYSICS AND PLAYER CONTROLLER---
	physics = std::make_unique<fphysicscontroller>(mScene);
	player = std::make_unique<fplayercontrol>(physics.get(), mScene, false, 1.5, (PxUserControllerHitReport*)this);
	
	//---CREATE ACTORS FOR MODELS---
	for (int i = 1; i <= 7; ++i) {
		auto instance = mScene->get_model_by_name("Wall" + std::to_string(i));
		physics->create_rigid_static_for_scaled_plane(instance);
	}
	for (int i = 1; i <= 10; ++i) {
		auto instance = mScene->get_model_by_name("Floor" + std::to_string(i));
		auto moving = i >= 7 && i <= 9;
		auto actor = physics->create_rigid_static_for_scaled_unit_box(instance, moving);
		if (moving) {
			movingFloors[i - 7] = actor;
		}
	}

	auto finalRegionRes = mScene->get_model_by_name("FinalFloor");
	finalRegionActor = physics->create_rigid_static_for_scaled_unit_box(finalRegionRes, true);
	player->set_final_region(finalRegionActor);

	sphereInstance = mScene->get_model_by_name("Sphere");
	auto mirrorBorderInstance = mScene->get_model_by_name("MirrorBorder");
	auto mirrorPlaneInstance = mScene->get_model_by_name("MirrorPlane");

	mirrorBorderActor = physics->create_rigid_static_for_scaled_unit_box(mirrorBorderInstance, true);
	mirrorPlaneActor = physics->create_rigid_static_for_scaled_plane(mirrorPlaneInstance, true);
	player->add_mirror({ mirrorBorderActor, mirrorPlaneActor });

	//---INITIALIZE HSV INTERPOLATOR---
	interpolator.add_sample(0, glm::vec3(47, 0, 0.3));
	interpolator.add_sample(1, glm::vec3(60, 1, 1));
}

levelstatus flevel1logic::update(float deltaT, double focusHitValue)
{
	player->update(deltaT);
	//---UPDATE SCORE AND LEVEL STATUS---
	if (score > 2.0f) {
		return levelstatus::WON;
	}
	if (std::fabs(focusHitValue) < 0.0001) {
		score = glm::max(score - 0.08 * deltaT, 0.0);
	}
	else {
		score = glm::min(score + deltaT * 0.1, 1.0);
	}

	mScene->set_background_color(interpolator.interpolate(score));
	if (score >= 0.99 && player->on_final_region()) {
		mScene->get_material_data(sphereInstance->mMaterialIndex).mDiffuseReflectivity = glm::vec4(1.0f);
		mScene->set_background_color(glm::vec4(1, 1, 0.5, 1));
		score = 100.0f;
		return levelstatus::WON;
	}

	if (gvk::input().key_released(gvk::key_code::f10)) {
		score = 100.0f;
		return levelstatus::WON;
	}

	if (player->fell_down()) {
		return levelstatus::LOST;
	}
	return levelstatus::RUNNING;
}

void flevel1logic::fixed_update(float stepSize)
{
	//---ANIMATE PLATTFORMS---
	accTime += stepSize;
	float xA = -(22 + 1.5 * cos(2 * accTime));
	float xB = -(22 - 1.5 * cos(2 * accTime));
	PxTransform oldpose[4];
	PxTransform newpose[4];
	oldpose[0] = movingFloors[0]->getGlobalPose();
	newpose[0] = PxTransform(PxVec3(xA, oldpose[0].p.y, oldpose[0].p.z), oldpose[0].q);
	movingFloors[0]->setGlobalPose(newpose[0]);
	oldpose[1] = movingFloors[1]->getGlobalPose();
	newpose[1] = PxTransform(PxVec3(xB, oldpose[1].p.y, oldpose[1].p.z), oldpose[1].q);
	movingFloors[1]->setGlobalPose(newpose[1]);
	oldpose[2] = movingFloors[2]->getGlobalPose();
	newpose[2] = PxTransform(PxVec3(xA, oldpose[2].p.y, oldpose[2].p.z), oldpose[2].q);
	movingFloors[2]->setGlobalPose(newpose[2]);
	oldpose[3] = finalRegionActor->getGlobalPose();
	newpose[3] = PxTransform(PxVec3(xB, oldpose[3].p.y, oldpose[3].p.z), oldpose[3].q);
	finalRegionActor->setGlobalPose(newpose[3]);

	//---UPDATE PHYSICS AND PLAYER---
	player->pre_px_update(stepSize);
	physics->update(stepSize);
	player->post_px_update(stepSize);

	//If the player stands on a platform, he has to be moved as well
	for (int i = 0; i < 4; ++i) {
		if (onPlatform[i]) {
			PxVec3 diffvec = newpose[i].p - oldpose[i].p;
			glm::vec3 distance = glm::make_vec3(&diffvec.x);
			player->beam_along(distance);
			onPlatform[i] = false;
		}
	}
}

void flevel1logic::reset()
{
	//---RESET TO INITIAL DATA---
	accTime = 0;
	mScene->get_camera().set_rotation(initialCameraRot);
	mScene->get_camera().set_translation(initialCameraPos);
	player->update_position();
	player->reset_mirrors();
	score = 0;
}

void flevel1logic::onShapeHit(const PxControllerShapeHit& hit)
{
	for (int i = 0; i < 3; ++i) {
		if (hit.actor == movingFloors[i]) {
			onPlatform[i] = true;
		}
	}
	if (hit.actor == finalRegionActor) {
		onPlatform[3] = true;
	}
}

