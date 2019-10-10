#version 460
#extension GL_NV_ray_tracing : require

rayPayloadInNV float secondaryRayHitValue;

void main()
{
    secondaryRayHitValue = gl_RayTmaxNV;
}