#include "includes.h"

fphysicscontroller::fphysicscontroller(fscene* scene) {
	this->scene = scene;

	mFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocatorCallback, gDefaultErrorCallback);
	if (!mFoundation) {
		throw std::runtime_error("PxCreateFoundation failed!");
	}

	bool recordMemoryAllocations = true;

#if _DEBUG
	mPvd = PxCreatePvd(*mFoundation);
	mTransport = PxDefaultPvdSocketTransportCreate("localhost", 5425, 10);
	mPvd->connect(*mTransport, PxPvdInstrumentationFlag::eALL);
#endif

	mPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *mFoundation, PxTolerancesScale(), recordMemoryAllocations, mPvd);
	if (!mPhysics) {
		throw std::runtime_error("PxCreatePhysics failed!");
	}

	mCooking = PxCreateCooking(PX_PHYSICS_VERSION, *mFoundation, PxCookingParams(PxTolerancesScale()));
	if (!mCooking) {
		throw std::runtime_error("PxCreateCooking failed!");
	}
	mDispatcher = PxDefaultCpuDispatcherCreate(2);
	PxSceneDesc sceneDesc = PxSceneDesc(PxTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;
	sceneDesc.cpuDispatcher = mDispatcher;
	mPxScene = mPhysics->createScene(sceneDesc);
	mPxScene->setVisualizationParameter(PxVisualizationParameter::eSCALE, 1.0f);
	mPxScene->setVisualizationParameter(PxVisualizationParameter::eACTOR_AXES, 2.0f);

	mControllerManager = PxCreateControllerManager(*mPxScene);

	mDefaultMaterial = mPhysics->createMaterial(1, 1, 0.3);
}

void fphysicscontroller::update(const float& stepSize) {
	mPxScene->simulate(stepSize);
	mPxScene->fetchResults(true);

	double updateTime = glfwGetTime();

	//----- APPLY TO DYNAMIC RIGID BODIES -----
	for (dynamicobject obj : dynamicObjects) {
		PxTransform t = obj.dynamicActor->getGlobalPose();
		glm::mat4x3 transform = utility::to_glm_mat4x3(t);
		obj.dynamicInstance->mTransformation = transform * obj.scale;
	}
}

PxRigidStatic* fphysicscontroller::create_rigid_static_for_scaled_unit_box(fmodel* instance, bool dynamic)
{
	glm::mat4x3 transform = instance->mTransformation;
	glm::vec3 p0 = transform * glm::vec4(0, 0, 0, 1);
	glm::vec3 p1 = transform * glm::vec4(1, 0, 0, 1);
	glm::vec3 p2 = transform * glm::vec4(0, 1, 0, 1);
	glm::vec3 p3 = transform * glm::vec4(0, 0, 1, 1);
	float xscale = glm::distance(p0, p1);
	float yscale = glm::distance(p0, p2);
	float zscale = glm::distance(p0, p3);
	glm::mat4 invScale = glm::mat4(1 / xscale, 0, 0, 0, 0, 1 / yscale, 0, 0, 0, 0, 1 / zscale, 0, 0, 0, 0, 1);
	glm::mat4 newTransf = glm::mat4(transform) * invScale;
	float transvals[16] = { newTransf[0][0], newTransf[0][1], newTransf[0][2], newTransf[0][3], newTransf[1][0], newTransf[1][1], newTransf[1][2], newTransf[1][3],
		newTransf[2][0], newTransf[2][1], newTransf[2][2], newTransf[2][3], newTransf[3][0], newTransf[3][1], newTransf[3][2], newTransf[3][3] };
	PxMat44 pxMat = PxMat44(transvals);
	PxTransform pxTransform = PxTransform(pxMat);
	PxRigidStatic* actor = mPhysics->createRigidStatic(pxTransform);
	actor->userData = instance;
	PxShape* shape = mPhysics->createShape(PxBoxGeometry(xscale, yscale, zscale), *mDefaultMaterial, true);
	actor->attachShape(*shape);
	shape->release();
	mPxScene->addActor(*actor);
	if (dynamic) {
		dynamicObjects.push_back({ actor, instance, glm::scale(glm::mat4(1.0f), glm::vec3(xscale, yscale, zscale)) });
	}
	return actor;
}

PxRigidStatic* fphysicscontroller::create_rigid_static_for_scaled_plane(fmodel* instance, bool dynamic)
{
	glm::mat4x3 transform = instance->mTransformation;
	glm::vec3 p0 = transform * glm::vec4(0, 0, 0, 1);
	glm::vec3 p1 = transform * glm::vec4(1, 0, 0, 1);
	glm::vec3 p2 = transform * glm::vec4(0, 1, 0, 1);
	glm::vec3 p3 = transform * glm::vec4(0, 0, 1, 1);
	float xscale = glm::distance(p0, p1);
	float yscale = glm::distance(p0, p2);
	float zscale = glm::distance(p0, p3);
	float thickness = 0.1f;
	glm::mat4 invScale = glm::mat4(1 / xscale, 0, 0, 0, 0, 1 / yscale, 0, 0, 0, 0, 1 / zscale, 0, 0, 0, 0, 1);
	glm::mat4 newTransf = glm::mat4(transform) * invScale;
	float transvals[16] = { newTransf[0][0], newTransf[0][1], newTransf[0][2], newTransf[0][3], newTransf[1][0], newTransf[1][1], newTransf[1][2], newTransf[1][3],
		newTransf[2][0], newTransf[2][1], newTransf[2][2], newTransf[2][3], newTransf[3][0], newTransf[3][1], newTransf[3][2], newTransf[3][3] };
	PxMat44 pxMat = PxMat44(transvals);
	PxTransform pxTransform = PxTransform(pxMat);
	PxRigidStatic* actor = mPhysics->createRigidStatic(pxTransform);
	actor->userData = instance;
	PxShape* shape = mPhysics->createShape(PxBoxGeometry(xscale, yscale, thickness), *mDefaultMaterial, true);
	actor->attachShape(*shape);
	shape->release();
	mPxScene->addActor(*actor);
	if (dynamic) {
		dynamicObjects.push_back({ actor, instance, glm::scale(glm::mat4(1.0f), glm::vec3(xscale, yscale, zscale)) });
	}
	return actor;
}

void fphysicscontroller::cleanup() {
	mDefaultMaterial->release();
	mControllerManager->release();
	mPxScene->release();
	delete mDispatcher;
	mCooking->release();
	mPhysics->release();
	if (mPvd != nullptr) {
		mPvd->release();
		mTransport->release();
	}
	mFoundation->release();
}

void fphysicserrorcallback::reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line) {
	std::stringstream ss;
	ss << "PhysX Error (" << code << "): \"" << message << "\" in file \"" << file << "\"::" << line << std::endl;
	LOG_ERROR(ss.str());
}
