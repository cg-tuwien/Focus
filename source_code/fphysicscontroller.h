//Author: Simon Fraiss
#pragma once
#include "includes.h"

using namespace physx;

class fphysicserrorcallback : public physx::PxErrorCallback {
public:
	virtual void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line);

};

/*
Management of PhysX-classes. Has functions for creation of rigidbodies.
Can be used by level logic classes to use physics.
*/
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

	//Initializes PhysX
	fphysicscontroller(fscene* scene);

	//Should be called every fixed time step. Simulates physics and applies to dynamic rigid bodies
	void update(const float& stepSize);

	//Creates an actor for a box with corner vertices +-1/+-1/+-1, and a linear transformation
	//set dynamic to true if changes of the transform of the actor should be applied to the model
	PxRigidStatic* create_rigid_static_for_scaled_unit_box(fmodel* model, bool dynamic = false);

	//Creates an actor for a plane with corner vertices +-1/+-1/0, and a linear transformation
	//set dynamic to true if changes of the transform of the actor should be applied to the model
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