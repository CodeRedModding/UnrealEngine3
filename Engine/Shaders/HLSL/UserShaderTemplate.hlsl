/*
	Defined by the C++ code:
		SHADERBLENDING_SOLID
		SHADERBLENDING_MASKED
		SHADERBLENDING_TRANSLUCENT
		SHADERBLENDING_ADDITIVE

		SHADER_TWOSIDED
		SHADER_UNLIT
		SHADER_SHM
		SHADER_NONDIRECTIONALLIGHTING
*/

#define NUM_USER_TEXTURES %u
#define NUM_USER_TEXCOORDS %u
#define NUM_USER_VECTOR_INPUTS %u
#define NUM_USER_SCALAR_INPUTS %u

#if NUM_USER_VECTOR_INPUTS
float4	UserVectorInputs[NUM_USER_VECTOR_INPUTS];
#endif

#if NUM_USER_SCALAR_INPUTS
float4	UserScalarInputs[(NUM_USER_SCALAR_INPUTS + 3) / 4];
#endif

#if NUM_USER_TEXTURES
sampler UserTextures[NUM_USER_TEXTURES];
#endif

float	ObjectTime,
		SceneTime;

struct userShaderInput
{
#if NUM_USER_TEXCOORDS
	float2 UserTexCoords[NUM_USER_TEXCOORDS];
#endif
	float4	UserVertexColor;
	float3	TangentNormal,
			TangentReflectionVector,
			TangentCameraVector;
};

half3 userNormal(userShaderInput Input)
{
	return %s;
}

half3 userEmissive(userShaderInput Input)
{
	return %s;
}

half3 userDiffuseColor(userShaderInput Input)
{
	return %s;
}

half3 userSpecularColor(userShaderInput Input)
{
	return %s;
}

half userSpecularPower(userShaderInput Input)
{
	return %s;
}

half userOpacity(userShaderInput Input)
{
	return %s;
}

#if SHADERBLENDING_MASKED
float userMask(userShaderInput Input)
{
	return %s - %s;
}
#endif

float2 userDistortion(userShaderInput Input)
{
	return %s;
}

float3 userTwoSidedLightingMask(userShaderInput Input)
{
	return %s;
}

#if SHADER_SHM
float4 userSHM(userShaderInput Input)
{
	return %s;
}
#endif

float3 userPointLightTransfer(userShaderInput Input,float3 LightVector,float3 WorldLightVector)
{
	float3	TwoSidedLighting = 0;
	float3	TwoSidedLightingMask = 0;
	TwoSidedLightingMask = userTwoSidedLightingMask(Input);
	TwoSidedLighting = TwoSidedLightingMask * userDiffuseColor(Input) * radialAttenuation(WorldLightVector);

	float3	Lighting;
#if SHADER_NONDIRECTIONALLIGHTING
	Lighting = userDiffuseColor(Input) * radialAttenuation(WorldLightVector);;
#elif SHADER_SHM
	Lighting = pointLightSHM(
			userDiffuseColor(Input),
			userSpecularColor(Input),
			userSpecularPower(Input),
			userSHM(Input),
			LightVector,
			Input.TangentCameraVector,
			Input.TangentNormal,
			Input.TangentReflectionVector
			) *
		radialAttenuation(WorldLightVector);
#else
	Lighting = pointLightPhong(
		userDiffuseColor(Input),
		userSpecularColor(Input),
		userSpecularPower(Input),
		LightVector,
		Input.TangentCameraVector,
		Input.TangentNormal,
		Input.TangentReflectionVector
		) *
		radialAttenuation(WorldLightVector);
#endif

	return lerp(Lighting,TwoSidedLighting,TwoSidedLightingMask);
}

float3 userDirectionalLightTransfer(userShaderInput Input,float3 LightVector)
{
	float3	TwoSidedLighting = 0;
	float3	TwoSidedLightingMask = 0;
	TwoSidedLightingMask = userTwoSidedLightingMask(Input);
	TwoSidedLighting = TwoSidedLightingMask * userDiffuseColor(Input);

	float3	Lighting;
#if SHADER_NONDIRECTIONALLIGHTING
	Lighting = userDiffuseColor(Input);
#elif SHADER_SHM
	Lighting = pointLightSHM(
			userDiffuseColor(Input),
			userSpecularColor(Input),
			userSpecularPower(Input),
			userSHM(Input),
			LightVector,
			Input.TangentCameraVector,
			Input.TangentNormal,
			Input.TangentReflectionVector
			);
#else
	Lighting = pointLightPhong(
		userDiffuseColor(Input),
		userSpecularColor(Input),
		userSpecularPower(Input),
		LightVector,
		Input.TangentCameraVector,
		Input.TangentNormal,
		Input.TangentReflectionVector
		);
#endif

	return lerp(Lighting,TwoSidedLighting,TwoSidedLightingMask);
}

float3 userHemisphereLightTransfer(userShaderInput Input,float3 SkyVector)
{
	float3	TwoSidedLighting = 0;
	float3	TwoSidedLightingMask = 0;
	TwoSidedLightingMask = userTwoSidedLightingMask(Input);
	TwoSidedLighting = TwoSidedLightingMask * userDiffuseColor(Input);

	float3	Lighting;
#if SHADER_NONDIRECTIONALLIGHTING
	Lighting = userDiffuseColor(Input);
#elif SHADER_SHM
	Lighting = hemisphereLightSHM(userDiffuseColor(Input),userSHM(Input),SkyVector);
#else
	Lighting = hemisphereLightPhong(userDiffuseColor(Input),SkyVector,Input.TangentNormal);
#endif

	return lerp(Lighting,TwoSidedLighting,TwoSidedLightingMask);
}

#if SHADERBLENDING_TRANSLUCENT
	void userClipping(userShaderInput Input) { clip(userOpacity(Input) - 1.0 / 255.0); }
	#if OPAQUELAYER
		float4 userBlendBase(userShaderInput Input,float4 ScreenPosition,float3 Color) { userClipping(Input); return opaqueBlendBase(ScreenPosition,Color); }
		float4 userBlendAdd(userShaderInput Input,float4 ScreenPosition,float3 Color) { userClipping(Input); return opaqueBlendAdd(ScreenPosition,Color); }
	#else
		float4 userBlendBase(userShaderInput Input,float4 ScreenPosition,float3 Color) { userClipping(Input); return translucentBlendBase(ScreenPosition,Color,userDistortion(Input),userOpacity(Input)); }
		float4 userBlendAdd(userShaderInput Input,float4 ScreenPosition,float3 Color) { userClipping(Input); return translucentBlendAdd(ScreenPosition,Color,userOpacity(Input)); }
	#endif
#elif SHADERBLENDING_ADDITIVE
	void userClipping(userShaderInput Input) { clip(userOpacity(Input) - 1.0 / 255.0); }
	#if OPAQUELAYER
		float4 userBlendBase(userShaderInput Input,float4 ScreenPosition,float3 Color) { userClipping(Input); return opaqueBlendBase(ScreenPosition,Color); }
		float4 userBlendAdd(userShaderInput Input,float4 ScreenPosition,float3 Color) { userClipping(Input); return opaqueBlendAdd(ScreenPosition,Color); }
	#else
		float4 userBlendBase(userShaderInput Input,float4 ScreenPosition,float3 Color) { userClipping(Input); return additiveBlendBase(ScreenPosition,Color,userOpacity(Input)); }
		float4 userBlendAdd(userShaderInput Input,float4 ScreenPosition,float3 Color) { userClipping(Input); return additiveBlendAdd(ScreenPosition,Color,userOpacity(Input)); }
	#endif
#else
	#if SHADERBLENDING_MASKED
		void userClipping(userShaderInput Input) { clip(userMask(Input)); }
	#else
		void userClipping(userShaderInput Input) {}
	#endif
	float4 userBlendBase(userShaderInput Input,float4 ScreenPosition,float3 Color) { userClipping(Input); return opaqueBlendBase(ScreenPosition,Color); }
	float4 userBlendAdd(userShaderInput Input,float4 ScreenPosition,float3 Color) { userClipping(Input); return opaqueBlendAdd(ScreenPosition,Color); }
#endif

void calcUserVectors(in out userShaderInput UserShaderInput,float3 CameraVector)
{
	UserShaderInput.TangentCameraVector = normalize(CameraVector);
	UserShaderInput.TangentNormal = normalize(userNormal(UserShaderInput));
#if SHADER_TWOSIDED
	UserShaderInput.TangentNormal *= TwoSidedSign;
#endif
	UserShaderInput.TangentReflectionVector = -UserShaderInput.TangentCameraVector + UserShaderInput.TangentNormal * dot(UserShaderInput.TangentNormal,UserShaderInput.TangentCameraVector) * 2.0;
}