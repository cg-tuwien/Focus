#version 460
#extension GL_EXT_ray_tracing : require

rayPayloadInEXT float secondaryRayHitValue;

void main()
{
    secondaryRayHitValue = gl_HitTEXT;
}