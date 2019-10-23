#include "includes.h"

glm::mat4x3 utility::to_glm_mat4x3(PxMat44 mat)
{
	return glm::mat4x3(mat.column0.x, mat.column0.y, mat.column0.z, mat.column1.x, mat.column1.y, mat.column1.z, mat.column2.x, mat.column2.y, mat.column2.z, mat.column3.x, mat.column3.y, mat.column3.z);
}

glm::mat4x3 utility::to_glm_mat4x3(PxTransform t)
{
	return to_glm_mat4x3(PxMat44(t));
}