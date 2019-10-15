#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform sampler2D textures[];

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

//struct LightGpuData ;

struct ModelInstanceGpuData {
	uint mMaterialIndex;
	mat4 mNormalMat;
};

layout(set = 4, binding = 0) buffer InstanceBuffer {
	ModelInstanceGpuData instances[];
} instanceSsbo;

layout(set = 0, binding = 1) buffer Material 
{
	MaterialGpuData materials[];
} matSsbo;

layout(set = 0, binding = 2) uniform usamplerBuffer indexBuffers[];
layout(set = 0, binding = 3) uniform samplerBuffer texCoordBuffers[];
layout(set = 0, binding = 4) uniform samplerBuffer normalBuffers[];

layout(set = 0, binding = 5) uniform LightInfo {
	uint lightCount;
} lightInfo;

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

layout(set = 5, binding = 0) buffer Light {
	LightGpuData[] lights;
} lightSsbo;

layout(set = 2, binding = 0) uniform accelerationStructureNV topLevelAS;

layout(location = 0) rayPayloadInNV vec3 hitValue;
hitAttributeNV vec3 attribs;
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
	mat3 normalMat = mat3(instanceSsbo.instances[instanceIndex].mNormalMat);
	const ivec3 indices = ivec3(texelFetch(indexBuffers[instanceIndex], gl_PrimitiveID).rgb);
	const vec2 uv0 = texelFetch(texCoordBuffers[instanceIndex], indices.x).rg;
	const vec2 uv1 = texelFetch(texCoordBuffers[instanceIndex], indices.y).rg;
	const vec2 uv2 = texelFetch(texCoordBuffers[instanceIndex], indices.z).rg;
	const vec2 uv = (barycentrics.x * uv0 + barycentrics.y * uv1 + barycentrics.z * uv2);
	const vec3 normal0 = texelFetch(normalBuffers[instanceIndex], indices.x).rgb;
	const vec3 normal1 = texelFetch(normalBuffers[instanceIndex], indices.y).rgb;
	const vec3 normal2 = texelFetch(normalBuffers[instanceIndex], indices.z).rgb;
	const vec3 normal = normalize(normalMat*(barycentrics.x * normal0 + barycentrics.y * normal1 + barycentrics.z * normal2));

    //vec3 origin = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
    //vec3 direction = normalize(vec3(0.8, 1, 0.2));
    //uint rayFlags = gl_RayFlagsCullFrontFacingTrianglesNV;	//(this doesn't make sense...)
    //uint cullMask = 0xff;
    //float tmin = 0.001;
    //float tmax = 100.0;
    //traceNV(topLevelAS, rayFlags, cullMask, 1 /* sbtRecordOffset */, 0 /* sbtRecordStride */, 1 /* missIndex */, origin, tmin, direction, tmax, 2 /*payload location*/);

	vec3 position = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
	vec3 eye = normalize(gl_WorldRayOriginNV - position);

	uint materialIndex = instanceSsbo.instances[instanceIndex].mMaterialIndex;

	int texid = matSsbo.materials[materialIndex].mDiffuseTexIndex;
	vec3 dColor = matSsbo.materials[materialIndex].mDiffuseReflectivity.rgb;
	if (texid != 0) {
		dColor = dColor * texture(textures[texid], uv).rgb;
	}

	vec3 ownColor = matSsbo.materials[materialIndex].mAmbientReflectivity.rgb*dColor;
	for (uint i = 0; i < lightInfo.lightCount; ++i) {
		if (lightSsbo.lights[i].mInfo.x == 2) {
			ownColor += phongPoint(position, eye, normal, dColor, materialIndex, lightSsbo.lights[i].mPosition.xyz, lightSsbo.lights[i].mColorDiffuse.rgb, lightSsbo.lights[i].mAttenuation.xyz, true);
		} else if (lightSsbo.lights[i].mInfo.x == 1) {
			ownColor += phongDirectional(position, eye, normal, dColor, materialIndex, lightSsbo.lights[i].mDirection.xyz, lightSsbo.lights[i].mColorDiffuse.rgb, true);
		}
	}

	hitValue = ownColor;
	//hitValue = vec3(materialIndex / 10.0f);
	//hitValue = normal;
}