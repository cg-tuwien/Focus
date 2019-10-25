#pragma once
#include "includes.h"

using namespace physx;

class utility {
public:
	static glm::mat4x3 to_glm_mat4x3(PxMat44 mat);
	static glm::mat4x3 to_glm_mat4x3(PxTransform t);
};