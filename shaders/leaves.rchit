#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

//Similar to closest.rchit, just optimized for leaves, e.g. no texture lookup necessary anymore and no normal mapping / reflection

struct RayTracingHit {
	vec4 color;
	vec4 transparentColor[2];
	float transparentDist[4];	//0 = goal, 1 = character
	uvec4 various;		//x = goal, y = recursions, z = renderCharacter
};

struct MaterialGpuData
{
	vec4 mDiffuseReflectivity;
	vec4 mAmbientReflectivity;
	vec4 mSpecularReflectivity;
	vec4 mEmissiveColor;
	vec4 mTransparentColor;
	vec4 mReflectiveColor;
	vec4 mAlbedo;

	float mOpacity;
	float mBumpScaling;
	float mShininess;
	float mShininessStrength;
	
	float mRefractionIndex;
	float mReflectivity;
	float mMetallic;
	float mSmoothness;
	
	float mSheen;
	float mThickness;
	float mRoughness;
	float mAnisotropy;
	
	vec4 mAnisotropyRotation;
	vec4 mCustomData;
	
	int mDiffuseTexIndex;
	int mSpecularTexIndex;
	int mAmbientTexIndex;
	int mEmissiveTexIndex;
	int mHeightTexIndex;
	int mNormalsTexIndex;
	int mShininessTexIndex;
	int mOpacityTexIndex;
	int mDisplacementTexIndex;
	int mReflectionTexIndex;
	int mLightmapTexIndex;
	int mExtraTexIndex;
	
	vec4 mDiffuseTexOffsetTiling;
	vec4 mSpecularTexOffsetTiling;
	vec4 mAmbientTexOffsetTiling;
	vec4 mEmissiveTexOffsetTiling;
	vec4 mHeightTexOffsetTiling;
	vec4 mNormalsTexOffsetTiling;
	vec4 mShininessTexOffsetTiling;
	vec4 mOpacityTexOffsetTiling;
	vec4 mDisplacementTexOffsetTiling;
	vec4 mReflectionTexOffsetTiling;
	vec4 mLightmapTexOffsetTiling;
	vec4 mExtraTexOffsetTiling;
};

struct ModelInstanceGpuData {
	uint mMaterialIndex;
	mat4 mNormalMat;
	uint mFlags;
};

struct LightGpuData {
	vec4 mColorAmbient;
	vec4 mColorDiffuse;
	vec4 mColorSpecular;
	vec4 mDirection;
	vec4 mPosition;
	vec4 mAngles;
	vec4 mAttenuation;
	ivec4 mInfo;
};

layout(set = 0, binding = 0) buffer InstanceBuffer {
	ModelInstanceGpuData instances[];
} instanceSsbo;
layout(set = 0, binding = 1) buffer Material 
{
	MaterialGpuData materials[];
} matSsbo;
layout(set = 0, binding = 2, std430) buffer Light {
	uvec4 lightCount;
	LightGpuData[] lights;
} lightSsbo;
layout(set = 0, binding = 3) uniform sampler2D textures[];
layout(set = 6, binding = 0) uniform usamplerBuffer indexBuffers[];
layout(set = 0, binding = 6) uniform samplerBuffer normalBuffers[];

layout(set = 2, binding = 0) uniform accelerationStructureNV topLevelAS;

layout(set = 3, binding = 0) uniform Background {
	vec4 color;
} background;

layout(location = 0) rayPayloadInNV RayTracingHit hitValue;
hitAttributeNV vec3 attribs;
layout(location = 1) rayPayloadNV RayTracingHit reflectionHit;
layout(location = 2) rayPayloadNV float shadowHit;

vec3 phongDirectional(vec3 iPosition, vec3 iEye, vec3 iNormal, vec3 iColor, uint iMatIndex, vec3 lDirection, vec3 lIntensity, bool lCheckShadow) {
	vec3 l = normalize(-lDirection);

	float shade = 1.0f;
	if (lCheckShadow) {
		float tmax = 1000.0;
		traceNV(topLevelAS, gl_RayFlagsCullBackFacingTrianglesNV , 0xff, 1 /*sbtOffset*/, 0, 1 /*missIdx*/, iPosition, 0.001, l, tmax, 2);
		shade = (shadowHit < tmax) ? 0.25 : 1.0;
	}

	float nl = max(dot (iNormal, l), 0);
	vec3 diffuse = iColor * lIntensity * nl;
	float renderspecular = float(shade > 0.99) * float(nl > 0);
	vec3 refl = reflect(-l, iNormal);
	float shininess = matSsbo.materials[iMatIndex].mShininess;
	float nr = max(pow(dot(iEye, refl), shininess),0);
	vec3 specular = matSsbo.materials[iMatIndex].mSpecularReflectivity.xyz * lIntensity * nr * renderspecular;
	return shade*(diffuse + specular);
}

vec3 phongPoint(vec3 iPosition, vec3 iEye, vec3 iNormal, vec3 iColor, uint iMatIndex, vec3 lPosition, vec3 lIntensity, vec3 lAttenuation, bool lCheckShadow) {
	vec3 l = lPosition - iPosition;
	float dist = length(l);
	l = normalize(l);

	float shade = 1.0f;
	if (lCheckShadow) {
		float tmax = dist;
		traceNV(topLevelAS, gl_RayFlagsCullBackFacingTrianglesNV , 0xff, 1 /*sbtOffset*/, 0, 1 /*missIdx*/, iPosition, 0.001, l, tmax, 2);
		shade = (shadowHit < tmax) ? 0.25 : 1.0;
	}

	float att = lAttenuation.x + dist*lAttenuation.y + pow(dist,2)*lAttenuation.z;
	vec3 intensity = lIntensity/att;
	float nl = max(dot (iNormal, l), 0);
	vec3 diffuse = iColor * intensity * nl;
	float renderspecular = float(shade > 0.99) * float(nl > 0);
	vec3 refl = reflect(-l, iNormal);
	float shininess = matSsbo.materials[iMatIndex].mShininess;
	float nr = max(pow(dot(iEye, refl), shininess), 0);
	vec3 specular = renderspecular * matSsbo.materials[iMatIndex].mSpecularReflectivity.rgb * intensity * nr;
	return shade*(diffuse + specular);
}

void main()
{
    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
	const int instanceIndex = nonuniformEXT(gl_InstanceCustomIndexNV);
	uint materialIndex = instanceSsbo.instances[instanceIndex].mMaterialIndex;
	mat3 normalMat = mat3(instanceSsbo.instances[instanceIndex].mNormalMat);
	const ivec3 indices = ivec3(texelFetch(indexBuffers[instanceIndex], gl_PrimitiveID).rgb);
	const vec3 normal0 = texelFetch(normalBuffers[instanceIndex], indices.x).rgb;
	const vec3 normal1 = texelFetch(normalBuffers[instanceIndex], indices.y).rgb;
	const vec3 normal2 = texelFetch(normalBuffers[instanceIndex], indices.z).rgb;
	const vec3 normal = normalize(normalMat*(barycentrics.x * normal0 + barycentrics.y * normal1 + barycentrics.z * normal2));

	vec3 position = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	vec3 eye = normalize(gl_WorldRayOriginNV - position);

	vec3 dColor = matSsbo.materials[materialIndex].mDiffuseReflectivity.rgb * hitValue.color.rgb;
	vec3 ownColor = (0.9*matSsbo.materials[materialIndex].mAmbientReflectivity.rgb + 0.1*background.color.rgb)*dColor;

	for (uint i = 0; i < lightSsbo.lightCount.x; ++i) {
		if (lightSsbo.lights[i].mInfo.x == 2) {
			ownColor += phongPoint(position, eye, normal, dColor, materialIndex, lightSsbo.lights[i].mPosition.xyz, lightSsbo.lights[i].mColorDiffuse.rgb, lightSsbo.lights[i].mAttenuation.xyz, true);
		} else if (lightSsbo.lights[i].mInfo.x == 1) {
			ownColor += phongDirectional(position, eye, normal, dColor, materialIndex, lightSsbo.lights[i].mDirection.xyz, lightSsbo.lights[i].mColorDiffuse.rgb, true);
		}
	}

	hitValue.various.x = uint(hitValue.transparentDist[1] < gl_HitTNV);
	hitValue.color.rgb = ownColor.rgb;
	hitValue.color.rgb += uint(gl_HitTNV > hitValue.transparentDist[0])*hitValue.transparentColor[0].rgb;
	hitValue.color.rgb += uint(gl_HitTNV > hitValue.transparentDist[1])*hitValue.transparentColor[1].rgb;

}