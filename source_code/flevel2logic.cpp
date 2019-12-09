//Author: Simon Fraiss
#include "includes.h"

flevel2logic::flevel2logic(fscene* scene) : flevellogic(scene) {
	
}

void flevel2logic::initialize() {
	//---SAVE INITIAL CAMERA DATA FOR LEVEL RESET---
	initialCameraPos = mScene->get_camera().translation();
	initialCameraRot = mScene->get_camera().rotation();

	//---CREATE PHYSICS AND PLAYER CONTROLLER---
	physics = std::make_unique<fphysicscontroller>(mScene);
	player = std::make_unique<fplayercontrol>(physics.get(), mScene, false, 1.5);

	//---CREATE ACTORS FOR MODELS---
	std::vector<std::string> solids = {
		"WallX1", "WallX3",
		"Floor1", "Floor2", "Floor3", "Floor4", "Floor5", "Floor6"
	};

	for (const std::string& name : solids) {
		auto instance = mScene->get_model_by_name(name);
		auto actor = physics->create_rigid_static_for_scaled_unit_box(instance);
	}

	auto floor7 = mScene->get_model_by_name("Floor7");
	finalFloorActor = physics->create_rigid_static_for_scaled_unit_box(floor7);

	auto finalRegion = mScene->get_model_by_name("FinalRegion");
	finalRegionActor = physics->create_rigid_static_for_scaled_unit_box(finalRegion);
	player->set_final_region(finalRegionActor);

	auto wall = mScene->get_model_by_name("WallX2");
	movingWallActor = physics->create_rigid_static_for_scaled_unit_box(wall, true);

	sphereInstance = mScene->get_model_by_name("Sphere");
	auto mirrorBorderInstance = mScene->get_model_by_name("MirrorBorder");
	auto mirrorPlaneInstance = mScene->get_model_by_name("MirrorPlane");

	mirrorBorderActor = physics->create_rigid_static_for_scaled_unit_box(mirrorBorderInstance, true);
	mirrorPlaneActor = physics->create_rigid_static_for_scaled_plane(mirrorPlaneInstance, true);
	player->add_mirror({ mirrorBorderActor, mirrorPlaneActor });

	//---INITIALIZE HSV INTERPOLATOR---
	interpolator.add_sample(0, glm::vec3(47, 0, 0.5));
	interpolator.add_sample(1, glm::vec3(60, 1, 1));
}

levelstatus flevel2logic::update(float deltaT, double focusHitValue)
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

	if (cgb::input().key_released(cgb::key_code::f10)) {
		score = 100.0f;
		return levelstatus::WON;
	}

	if (player->fell_down()) {
		return levelstatus::LOST;
	}
	return levelstatus::RUNNING;
}

void flevel2logic::fixed_update(float stepSize)
{
	//---ANIMATE WALL IF NECESSARY---
	accTime += stepSize;
	PxVec3 finFloorMin = finalFloorActor->getWorldBounds().minimum;
	PxVec3 finFloorMax = finalFloorActor->getWorldBounds().maximum;
	glm::vec3 camPos = mScene->get_camera().translation();
	if (camPos.x >= finFloorMin.x && camPos.z >= finFloorMin.z && camPos.x <= finFloorMax.x && camPos.z <= finFloorMax.z) {
		//move z from -36 to -28 in 2 seconds
		if (wallMovingRightStart < 0) {
			wallMovingRightStart = accTime;
		}
		float z = glm::min(-36.0f + 8.0f * ((accTime - wallMovingRightStart) / 4.0f), -28.0f);
		PxTransform oldpose = movingWallActor->getGlobalPose();
		PxTransform newpose = PxTransform(PxVec3(oldpose.p.x, oldpose.p.y, z), oldpose.q);
		movingWallActor->setGlobalPose(newpose);
	}
	else {
		if (wallMovingRightStart > 0) {
			wallMovingRightStart = -1.0f;
			wallMovingLeftStart = accTime;
		}
		if (wallMovingLeftStart > 0) {
			float z = glm::max(-28.0f - 8.0f * ((accTime - wallMovingLeftStart) / 4.0f), -36.0f);
			PxTransform oldpose = movingWallActor->getGlobalPose();
			PxTransform newpose = PxTransform(PxVec3(oldpose.p.x, oldpose.p.y, z), oldpose.q);
			movingWallActor->setGlobalPose(newpose);
			if (accTime - wallMovingLeftStart > 4.0f) {
				wallMovingLeftStart = -1.0f;
			}
		}
	}

	//---UPDATE PHYSICS AND PLAYER---
	player->pre_px_update(stepSize);
	physics->update(stepSize);
	player->post_px_update(stepSize);
}

void flevel2logic::reset()
{
	//---RESET TO INITIAL DATA---
	accTime = 0;
	mScene->get_camera().set_rotation(initialCameraRot);
	mScene->get_camera().set_translation(initialCameraPos);
	player->update_position();
	player->reset_mirrors();
	score = 0;
}

