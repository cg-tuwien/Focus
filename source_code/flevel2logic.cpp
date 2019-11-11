#include "includes.h"
#include "flevel2logic.h"

flevel2logic::flevel2logic(fscene* scene) : flevellogic(scene) {
	
}

void flevel2logic::initialize() {
	initialCameraPos = scene->get_camera().translation();
	initialCameraRot = scene->get_camera().rotation();
	physics = std::make_unique<fphysicscontroller>(scene);
	player = std::make_unique<fplayercontrol>(physics.get(), scene, false, 1.5);

	/*scene->materials[0].diffuseColor *= 2;
	scene->materials[2].ambientColor = glm::vec3(0.4f);
	scene->materials[3].ambientColor = glm::vec3(0.4f);*/

	std::vector<std::string> solids = {
		//"WallX1", "WallX3",
		"Cube.029", "Cube.032",
		//"Floor1", "Floor2", "Floor3", "Floor4", "Floor5", "Floor6"
		"Cube.001", "Cube.000", "Cube.003", "Cube.005", "Cube.006", "Cube.011"
	};

	for (const std::string& name : solids) {
		auto searchres = scene->get_model_by_name(name);
		if (!searchres.has_value()) throw new std::runtime_error("Did not found " + name);
		fmodel* instance = searchres.value();
		auto actor = physics->create_rigid_static_for_scaled_unit_box(instance);
	}

	auto floor7 = scene->get_model_by_name("Cube.012");
	if (!floor7.has_value()) throw new std::runtime_error("Did not found Cube.012(Floor7)");
	finalFloorActor = physics->create_rigid_static_for_scaled_unit_box(floor7.value());

	auto finalRegion = scene->get_model_by_name("Cube.015");
	if (!finalRegion.has_value()) throw new std::runtime_error("Did not found Cube.015(FinalRegion)");
	finalRegionActor = physics->create_rigid_static_for_scaled_unit_box(finalRegion.value());
	player->set_final_region(finalRegionActor);

	auto wall = scene->get_model_by_name("Cube.031");
	if (!wall.has_value()) throw new std::runtime_error("Did not found Cube.031(WallX2)");
	movingWallActor = physics->create_rigid_static_for_scaled_unit_box(wall.value(), true);

	auto sphereQuery = scene->get_model_by_name("Sphere");
	if (!sphereQuery.has_value()) throw new std::runtime_error("Did not find Sphere!");
	sphereInstance = sphereQuery.value();
	auto mirrorBorderInstance = scene->get_model_by_name("Cube");//MirrorBorder1
	if (!mirrorBorderInstance.has_value()) throw new std::runtime_error("Did not find MirrorBorder1(Cube)");
	auto mirrorPlaneInstance = scene->get_model_by_name("Plane");//MirrorPlane1
	if (!mirrorPlaneInstance.has_value()) throw new std::runtime_error("Did not find MirrorPlane1(Plane)");

	mirrorBorderActor = physics->create_rigid_static_for_scaled_unit_box(mirrorBorderInstance.value(), true);
	mirrorPlaneActor = physics->create_rigid_static_for_scaled_plane(mirrorPlaneInstance.value(), true);
	player->add_mirror({ mirrorBorderActor, mirrorPlaneActor });

	interpolator.add_sample(0, glm::vec3(47, 0, 0.3));
	interpolator.add_sample(1, glm::vec3(60, 1, 1));
}

levelstatus flevel2logic::update(float deltaT, double focusHitValue)
{
	if (score > 2.0f) {
		return levelstatus::WON;
	}
	if (std::fabs(focusHitValue) < 0.0001) {
		score = glm::max(score - 0.08 * deltaT, 0.0);
	}
	else {
		score = glm::min(score + deltaT * 0.1, 1.0);
	}

	scene->set_background_color(interpolator.interpolate(score));
	if (score >= 0.99 && player->on_final_region()) {
		scene->get_material_data(sphereInstance->mMaterialIndex).mDiffuseReflectivity = glm::vec4(1.0f);
		scene->set_background_color(glm::vec4(1, 1, 0.5, 1));
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
	accTime += stepSize;
	PxVec3 finFloorMin = finalFloorActor->getWorldBounds().minimum;
	PxVec3 finFloorMax = finalFloorActor->getWorldBounds().maximum;
	glm::vec3 camPos = scene->get_camera().translation();
	if (camPos.x >= finFloorMin.x && camPos.z >= finFloorMin.z && camPos.x <= finFloorMax.x && camPos.z <= finFloorMax.z) {
		//move z from -36 to -28 in 2 seconds
		if (wallMovingRightStart < 0) {
			wallMovingRightStart = accTime;
		}
		float z = glm::min(-36.0f + 8.0f * ((accTime - wallMovingRightStart) / 2.0f), -28.0f);
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
			float z = glm::max(-28.0f - 8.0f * ((accTime - wallMovingLeftStart) / 2.0f), -36.0f);
			PxTransform oldpose = movingWallActor->getGlobalPose();
			PxTransform newpose = PxTransform(PxVec3(oldpose.p.x, oldpose.p.y, z), oldpose.q);
			movingWallActor->setGlobalPose(newpose);
			if (accTime - wallMovingLeftStart > 2.5f) {
				wallMovingLeftStart = -1.0f;
			}
		}
	}

	player->pre_px_update(stepSize);
	physics->update(stepSize);
	player->post_px_update(stepSize);
}

void flevel2logic::reset()
{
	accTime = 0;
	scene->get_camera().set_rotation(initialCameraRot);
	scene->get_camera().set_translation(initialCameraPos);
	player->update_position();
	player->reset_mirrors();
	score = 0;
}

