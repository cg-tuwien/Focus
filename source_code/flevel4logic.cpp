//Author: Simon Fraiss
#include "includes.h"

flevel4logic::flevel4logic(fscene* scene) : flevellogic(scene) {
}

void flevel4logic::initialize() {
	//---SAVE INITIAL CAMERA DATA FOR LEVEL RESET---
	initialCameraPos = mScene->get_camera().translation();
	initialCameraRot = mScene->get_camera().rotation();

	//---- CREATE PHYSICS AND PLAYER OBJECTS -----
	physics = std::make_unique<fphysicscontroller>(mScene);
	player = std::make_unique<fplayercontrol>(physics.get(), mScene, false, 1.5, (PxUserControllerHitReport*)this);

	//---CREATE ACTORS FOR MODELS---
	for (int i = 1; i <= 4; ++i) {
		auto instance = mScene->get_model_by_name("Platform" + std::to_string(i));
		platformActors[i - 1] = physics->create_rigid_static_for_scaled_unit_box(instance, true);
	}

	auto finalRegionInstance = mScene->get_model_by_name("FinalRegion");
	finalRegionActor = physics->create_rigid_static_for_scaled_unit_box(finalRegionInstance, true);
	player->set_final_region(finalRegionActor);

	sphereInstance = mScene->get_model_by_name("Sphere");
	auto mirrorBorder1Instance = mScene->get_model_by_name("MirrorBorder1");
	auto mirrorPlane1Instance = mScene->get_model_by_name("MirrorPlane1");
	auto mirrorBorder2Instance = mScene->get_model_by_name("MirrorBorder2");
	auto mirrorPlane2Instance = mScene->get_model_by_name("MirrorPlane2");
	auto groundFloorInstance = mScene->get_model_by_name("GroundFloor");
	auto leavesInstance = mScene->get_model_by_name("g2");
	//activate leave shader
	leavesInstance->mLeave = true;
	//brighten up leaves a bit
	mScene->get_material_data(leavesInstance->mMaterialIndex).mDiffuseReflectivity *= 3;

	physics->create_rigid_static_for_scaled_plane(groundFloorInstance, false);

	mirrorBorder1Actor = physics->create_rigid_static_for_scaled_unit_box(mirrorBorder1Instance, true);
	mirrorBorder1OriginalTransform = mirrorBorder1Actor->getGlobalPose();
	mirrorPlane1Actor = physics->create_rigid_static_for_scaled_plane(mirrorPlane1Instance, true);
	mirrorPlane1OriginalTransform = mirrorPlane1Actor->getGlobalPose();
	player->add_mirror({ mirrorBorder1Actor, mirrorPlane1Actor });

	mirrorBorder2Actor = physics->create_rigid_static_for_scaled_unit_box(mirrorBorder2Instance, true);
	mirrorBorder2OriginalTransform = mirrorBorder2Actor->getGlobalPose();
	mirrorPlane2Actor = physics->create_rigid_static_for_scaled_plane(mirrorPlane2Instance, true);
	mirrorPlane2OriginalTransform = mirrorPlane2Actor->getGlobalPose();
	player->add_mirror({ mirrorBorder2Actor, mirrorPlane2Actor });
}

levelstatus flevel4logic::update(float deltaT, double focusHitValue)
{
	//---UPDATE SCORE AND LEVEL STATUS---
	if (score > 2.0f) {
		return levelstatus::WON;
	}
	if (std::fabs(focusHitValue) < 0.0002) {
		score = glm::max(score - 0.05 * deltaT, 0.0);
	}
	else {
		score = glm::min(score + deltaT * 0.1, 1.0);
	}

	mScene->set_background_color(float(1 - score) * glm::vec3(0.5, 0.7, 0.75) + float(score) * glm::vec3(1, 1, 0));
	if ((score >= 0.99 && player->on_final_region())) {
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

void flevel4logic::fixed_update(float stepSize)
{
	//---ANIMATE PLATTFORMS---
	accTime += stepSize;

	float x[4];
	float z[4];
	x[0] = -6 * sin(accTime * 0.5f);
	z[0] = 5 * cos(accTime * 0.5f);
	x[1] = 7 * sin(accTime * 0.4f);
	z[1] = 7 * cos(accTime * 0.4f);
	x[2] = -5 * sin(accTime * 0.6f);
	z[2] = 5 * cos(accTime * 0.6f);
	x[3] = 8 * sin(accTime * 0.7f);
	z[3] = 8 * cos(accTime * 0.7f);
	PxTransform oldpose[5];
	PxTransform newpose[5];
	for (int i = 0; i < 4; ++i) {
		oldpose[i] = platformActors[i]->getGlobalPose();
		newpose[i] = PxTransform(PxVec3(x[i], oldpose[i].p.y, z[i]), oldpose[i].q);
		platformActors[i]->setGlobalPose(newpose[i]);
	}
	//Final Region
	glm::vec3 camPos = mScene->get_camera().translation();
	oldpose[4] = finalRegionActor->getGlobalPose();
	auto frMin = finalRegionActor->getWorldBounds().minimum;
	auto frMax = finalRegionActor->getWorldBounds().maximum;
	if (camPos.x >= frMin.x && camPos.x <= frMax.x && camPos.z >= frMin.z && camPos.z <= frMax.z) {
		float newy = 0.9f;
		if (oldpose[4].p.y > 1.0f) {
			newy = glm::max(oldpose[4].p.y - stepSize * 2.0f, 0.9f);
		}
		newpose[4] = PxTransform(PxVec3(oldpose[4].p.x, newy, oldpose[4].p.z), oldpose[4].q);
		finalRegionActor->setGlobalPose(newpose[4]);
	}
	else {
		if (oldpose[4].p.y < 7.02) {
			float newy = glm::min(oldpose[4].p.y + stepSize * 2.0f, 7.029f);
			newpose[4] = PxTransform(PxVec3(oldpose[4].p.x, newy, oldpose[4].p.z), oldpose[4].q);
			finalRegionActor->setGlobalPose(newpose[4]);
		}
		else {
			newpose[4] = oldpose[4];
		}
	}

	//Mirror-Movement:
	PxTransform oldmirrorpose = mirrorPlane1Actor->getGlobalPose();
	float newy = -3.0f * cos(accTime * 0.7f) + 9.0f;
	PxTransform newmirrorpose = PxTransform(PxVec3(oldmirrorpose.p.x, newy, oldmirrorpose.p.z), oldmirrorpose.q);
	mirrorPlane1Actor->setGlobalPose(newmirrorpose);
	mirrorBorder1Actor->setGlobalPose(newmirrorpose);

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

void flevel4logic::reset()
{
	//---RESET TO INITIAL DATA---
	accTime = 0;
	mScene->get_camera().set_rotation(initialCameraRot);
	mScene->get_camera().set_translation(initialCameraPos); 
	mirrorBorder1Actor->setGlobalPose(mirrorBorder1OriginalTransform);
	mirrorPlane1Actor->setGlobalPose(mirrorPlane1OriginalTransform);
	mirrorBorder2Actor->setGlobalPose(mirrorBorder2OriginalTransform);
	mirrorPlane2Actor->setGlobalPose(mirrorPlane2OriginalTransform);
	player->update_position();
	player->reset_mirrors();
	score = 0;
}

void flevel4logic::onShapeHit(const PxControllerShapeHit& hit) {
	for (int i = 0; i < 4; ++i) {
		if (hit.actor == platformActors[i]) {
			onPlatform[i] = true;
			break;
		}
	}
}