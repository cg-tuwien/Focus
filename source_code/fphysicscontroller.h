#pragma once
#include "includes.h"

using namespace physx;

class fphysicserrorcallback : public physx::PxErrorCallback {
public:
	virtual void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line);

};

class fphysicscontroller {
public:
	PxFoundation* mFoundation;
	PxPvdTransport* mTransport;
	PxPvd* mPvd;
	PxPhysics* mPhysics;
	PxCpuDispatcher* mDispatcher;
	PxScene* mPxScene;
	PxCooking* mCooking;
	PxControllerManager* mControllerManager;
	fphysicserrorcallback gDefaultErrorCallback;
	PxDefaultAllocator gDefaultAllocatorCallback;
	PxMaterial* mDefaultMaterial;

	fphysicscontroller(fscene* scene);

	void update(const float& stepSize);

	//PxShape* createConvexHull(const Mesh* mesh, const PxMaterial& material, const glm::mat4x3& transform, const bool& exclusive);
	PxRigidStatic* create_rigid_static_for_scaled_unit_box(fmodel* model, bool dynamic = false);
	PxRigidStatic* create_rigid_static_for_scaled_plane(fmodel* model, bool dynamic = false);

	void cleanup();

private:

	struct dynamicobject {
		PxRigidStatic* dynamicActor;
		fmodel* dynamicInstance;
		glm::mat4 scale;
	};

	fscene* scene;
	std::vector<dynamicobject> dynamicObjects;
};