///////////////////////////////////////////////////////////////////////  
//  SpeedTreeCAD Shaders
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) CONFIDENTIAL AND PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization, Inc. and
//  may not be copied, disclosed, or exploited except in accordance with 
//  the terms of that agreement.
//
//      Copyright (c) 2003-2008 IDV, Inc.
//      All rights reserved in all media.
//
//      IDV, Inc.
//      Web: http://www.idvinc.com

#define VS_COMPILE_COMMAND VertexProgram = compile glslv
#define PS_COMPILE_COMMAND FragmentProgram = compile glslf
//#define VS_COMPILE_COMMAND VertexProgram = compile vp30
//#define PS_COMPILE_COMMAND FragmentProgram = compile fp30


///////////////////////////////////////////////////////////////////////  
//  Global Variables

float4x4    g_mWorldViewProj;
float4		g_vCameraPos;
float3		g_vViewDir;
float4x4	g_mLightViewProj;
float4x4	g_mModelView;
float3x3	g_mInverseWorldView;	// for normal rendering

float4		g_vAmbientColor;	
float4		g_vDiffuseColor;	
float4		g_vSpecularColor;	
float4		g_vTransmissionColor;
float4		g_vMaterialExtra[2];	// diffuse light scalar, specular shininess, 1.0 - contrast, transmission view dependence
									// transmission shadow lightness, normal map scale, diffuse alpha scalar, unused
float		g_fNormalRendering;		// flag to break out for just normal rendering								

float3      g_vLightDir;			// normalized light direction (shaders assume a single light source)
float4		g_vLightAmbientColor;	// product of ambient material and light ambient
float4		g_vLightDiffuseColor;	// product of diffuse material and light diffuse
float4		g_vLightSpecularColor;	// product of specular material and light specular

float2		g_vCameraAngles;
float		g_fLeafCardLightShift;

float4		g_vSubSelectionColor;

float4		g_vLodTransition;

float4		g_vShadowData;

float4		g_vAnimation;


float3		g_vWindDir;			// xyz = dir
float4		g_vWindTimes;		// x = primary, y = secondary, z = frond, w = leaves
float4		g_vWindDistances;	// x = primary osc, y = secondary osc, z = wind height, w = height exponent
float4		g_vWindLeaves;		// x = distance, y = lighting change, z = windward scalar, w = leaves tile
float3		g_vWindFrondRipple;	// x = amount, y = u tile, z = v tile
float3		g_vWindGust;		// x = gust amount, y = primary distance, z = gust scale
float3		g_vWindGustHints;	// x = vertical offset %, wind dir adjustment, unison

const float c_fAlphaTestValue = 0.3f;
const float c_fClipValue = 0.001f;	// this value bails out of pixel shaders for shadow render (not all transparency works in frame buffer)


///////////////////////////////////////////////////////////////////////  
//  Textures

texture     g_tDiffuseMap;
texture     g_tNormalMap; 
texture     g_tDetailMap;  
texture     g_tSpecularTransmissionMap; 
texture		g_tDepthShadow;


///////////////////////////////////////////////////////////////////////  
//  Texture samplers

sampler2D samDiffuseMap : register(s0) = sampler_state
{
    Texture = <g_tDiffuseMap>;
};

sampler2D samNormalMap : register(s1) = sampler_state
{
    Texture = <g_tNormalMap>;
};

sampler2D samDetailMap : register(s2) = sampler_state
{
    Texture = <g_tDetailMap>;
};

sampler2D samDetailNormalMap : register(s3) = sampler_state
{
    Texture = <g_tDetailMap>;
};

sampler2D samSpecularTransmissionMap : register(s4) = sampler_state
{
    Texture = <g_tSpecularTransmissionMap>;
};

sampler2D samDepthShadow : register(s5) = sampler_state
{
    Texture = <g_tDepthShadow>;
};


///////////////////////////////////////////////////////////////////////  
//  Utility functions

#include "Utility.fx"


///////////////////////////////////////////////////////////////////////  
//  LightContrast

float LightContrast(float3 vNormal)
{
	const float c_fContrast = g_vMaterialExtra[0].z;
	return lerp(c_fContrast, 1.0f, abs(dot(vNormal, g_vLightDir)));
}


///////////////////////////////////////////////////////////////////////  
//  ShadowMapLookup
//
//	Returns 1.0f for pixels that are in light, 0.0f for those in shadow

float ShadowMapLookup(float4 vPosInLightSpace)
{
 	if (g_vShadowData.a == 0.0)
	{
 		return 1.0;
	}
	else
	{
		float fDistanceFromLightActivePixel = 0.5f * vPosInLightSpace.z + 0.5f;

		const float c_fOne = 1.5;
		const float c_fTwo = 3.0f;
		const float c_fZero = 0.0f;
		float2 avSamples[25] = 
		{
			float2(c_fZero,	c_fZero),
			float2(c_fOne,	c_fOne),
			float2(-c_fOne,	c_fOne),
			float2(-c_fOne,	-c_fOne),
			float2(c_fOne,	-c_fOne),	// 5 samples stops here

			float2(c_fZero,	c_fOne),
			float2(-c_fOne,	c_fZero),
			float2(c_fZero,	-c_fOne),
			float2(c_fOne,	c_fZero),	// 9 samples stops here

			float2(c_fTwo,	c_fTwo),
			float2(-c_fTwo,	c_fTwo),
			float2(-c_fTwo,	-c_fTwo),
			float2(c_fTwo,	-c_fTwo),	

			float2(c_fZero,	c_fTwo),
			float2(-c_fTwo,	c_fZero),
			float2(c_fZero,	-c_fTwo),
			float2(c_fTwo,	c_fZero),	// 17 samples stops here

			float2(c_fTwo,	c_fOne),
			float2(c_fOne,	c_fTwo),

			float2(-c_fOne,	c_fTwo),
			float2(-c_fTwo,	c_fOne),

			float2(-c_fTwo,	-c_fOne),
			float2(-c_fOne,	-c_fTwo),

			float2(c_fOne,	-c_fTwo),
			float2(c_fTwo,	-c_fOne)	// 25 stops here
		};

		float fSum = 0.0f;
		for (int i = 0; i < NUM_SHADOW_SAMPLES; ++i)
		{
			float2 vShadowMapTexCoords = 0.5f * (vPosInLightSpace.xy + (avSamples[i] * g_vShadowData.g)) / vPosInLightSpace.w + float2(0.5f, 0.5f);
			float fDistanceFromLightStoredInMap = tex2D(samDepthShadow, vShadowMapTexCoords).r;

			fSum += (fDistanceFromLightStoredInMap + g_vShadowData.x < fDistanceFromLightActivePixel) ? 0.0f : 1.0f;
		}
		return fSum / NUM_SHADOW_SAMPLES;
	}
}


///////////////////////////////////////////////////////////////////////  
//  LightingWithTransmission
//
//	Per pixel lighting

float4 LightingWithTransmission(float3	vNormal,
								float3	vBinormal,
								float3	vTangent,
								float3	vWorldSpacePosition,
								float	fDimming,
								float4	vTexDiffuse,
								float4	vTexNormal,
								float4	vTexSpecularTransmission,
								float	fShadow)
{							
	// normalize incoming vectors
	vNormal = normalize(vNormal);
	vBinormal = normalize(vBinormal);
	vTangent = normalize(vTangent);	
	
	// expand normal map & roughness adjust
	float3 vNewTexNormal = vTexNormal.xyz * 2.0 - 1.0;
	const float c_fNormalMapScale = g_vMaterialExtra[1].y;
	vNewTexNormal.z = vNewTexNormal.z * c_fNormalMapScale;
	vNewTexNormal = normalize(vNewTexNormal);

	// compute normal perturbed by the normal map
	float3 vFinalNormal = (vTangent * vNewTexNormal.x) + (vBinormal * vNewTexNormal.y) + (vNormal * vNewTexNormal.z);
	vFinalNormal = normalize(vFinalNormal);
	
	// perturbed normal rendering
	if (g_fNormalRendering)
		return float4(mul(vFinalNormal, g_mInverseWorldView) * 0.5f + 0.5f, vTexDiffuse.a * g_vMaterialExtra[1].b);

	/////////////////////////
	// ambient

		float4 vAmbient = g_vAmbientColor * g_vLightAmbientColor * vTexDiffuse;
		vAmbient = vAmbient * LightContrast(vFinalNormal);

	/////////////////////////
	// transmission

		// how much are we looking at the light?
		float fBackContribution = (dot(g_vViewDir, -g_vLightDir) + 1.0) / 2.0;
		fBackContribution = fBackContribution * fBackContribution;
		fBackContribution *= fBackContribution;
		
		// how much does the reverse normal say we should get
		float fScatterDueToNormal = (dot(g_vLightDir, -vNormal) + 1.0) / 2.0;
		
		// choose between them based on artist controlled parameter
		const float c_fViewDependency = g_vMaterialExtra[0].w;
		fBackContribution = lerp(fScatterDueToNormal, fBackContribution, c_fViewDependency);
		
		// back it off based on how much leaf is in the way
		float fReductionDueToNormal = (dot(g_vViewDir, vNormal) + 1.0) / 2.0;
		fBackContribution *= fReductionDueToNormal;
		
		// compute back color
		float3 vAdjust = g_vTransmissionColor.rgb;
 		vAdjust *= 3.0;
 		float3 vBackColor = vTexDiffuse.rgb * vAdjust;
		vBackColor = lerp(vAmbient.xyz, vBackColor, vTexSpecularTransmission.a);


	/////////////////////////
	// ambient and diffuse

	const float c_fLightScalar = g_vMaterialExtra[0].x;
	float4 vColor = lerp(vAmbient, 
						vTexDiffuse * g_vDiffuseColor * g_vLightDiffuseColor * c_fLightScalar, 
						max(dot(vFinalNormal, -g_vLightDir), 0.0) * fShadow);

	const float c_fShadowDarkness = g_vMaterialExtra[1].x;
	vColor.rgb = lerp(vColor.rgb, vBackColor, fBackContribution * (saturate(fShadow + c_fShadowDarkness)));


	/////////////////////////
	// specular

	float fShininess = g_vMaterialExtra[0].y;
	float3 vHalf = normalize(normalize(g_vCameraPos.xyz - vWorldSpacePosition) - g_vLightDir);
	vColor += fShadow * (1.0 - fBackContribution) * g_vSpecularColor * g_vLightSpecularColor * vTexSpecularTransmission * pow(max(dot(vHalf, vFinalNormal), 0.0), fShininess);
	
	// dimming
	vColor.xyz *= fDimming;

	// clamp the incoming for selection (this will have no affect if it isn't selected)
	vColor.xyz *= g_vSubSelectionColor.w;

	// add the selection color after the clamp to ensure there is always some indication of selection
	vColor.xyz += g_vSubSelectionColor.xyz;
	
	// alpha channel
	vColor.a = vTexDiffuse.a * g_vMaterialExtra[1].b;

	return vColor;
}


///////////////////////////////////////////////////////////////////////  
//  Transmission
//
//	Per pixel transmission

float4 Transmission(float4	vFrontColor,
					float4  vTexDiffuse,
					float4  vTexTransmission,
				    float3	vNormal,
					float	fShadow)
{
	// how much are we looking at the light?
	float fBackContribution = (dot(g_vViewDir, -g_vLightDir) + 1.0) / 2.0;
	fBackContribution = fBackContribution * fBackContribution;
	fBackContribution *= fBackContribution;
	
	// how much does the reverse normal say we should get
	float fScatterDueToNormal = (dot(g_vLightDir, -vNormal) + 1.0) / 2.0;
	
	// choose between them based on artist controlled parameter
	const float c_fViewDependency = g_vMaterialExtra[0].w;
	fBackContribution = lerp(fScatterDueToNormal, fBackContribution, c_fViewDependency);
	
	// back it off based on how much leaf is in the way
	float fReductionDueToNormal = (dot(g_vViewDir, vNormal) + 1.0) / 2.0;
	fBackContribution *= fReductionDueToNormal;
	
	// compute back color
	float3 vAdjust = g_vTransmissionColor.rgb;
 	vAdjust *= 3.0;
 	float4 vBackColor = float4((vTexDiffuse.rgb * vAdjust * vTexTransmission.a), vTexDiffuse.a);

	const float c_fShadowDarkness = g_vMaterialExtra[1].x;
	return lerp(vFrontColor, vBackColor, fBackContribution * (saturate(fShadow + c_fShadowDarkness)));
}


float4 Lighting(float3	vNormal,
				float3	vBinormal,
				float3	vTangent,
				float3	vWorldSpacePosition,
				float	fDimming,
				float4	vTexDiffuse,
				float4	vTexNormal,
				float4	vTexSpecularMask,
				float	fLightScalar,
				float	fShadow)
{							
	// normalize incoming vectors
	vNormal = normalize(vNormal);
	vBinormal = normalize(vBinormal);
	vTangent = normalize(vTangent);
	
	// expand normal map & roughness adjust
	float3 vNewTexNormal = vTexNormal.xyz * 2.0 - 1.0;
	const float c_fNormalMapScale = g_vMaterialExtra[1].y;
	vNewTexNormal.z = vNewTexNormal.z * c_fNormalMapScale;
	vNewTexNormal = normalize(vNewTexNormal);

	// compute normal perturbed by the normal map
	float3 vFinalNormal = (vTangent * vNewTexNormal.x) + (vBinormal * vNewTexNormal.y) + (vNormal * vNewTexNormal.z);
	vFinalNormal = normalize(vFinalNormal);
	
	// normal rendering
	if (g_fNormalRendering)
		return float4(mul(vFinalNormal, g_mInverseWorldView) * 0.5f + 0.5f, vTexDiffuse.a * g_vMaterialExtra[1].b);

	// ambient
	float4 vAmbient = g_vAmbientColor * g_vLightAmbientColor * vTexDiffuse;
	vAmbient = vAmbient * LightContrast(vFinalNormal);

	// ambient and diffuse
	const float c_fLightScalar = g_vMaterialExtra[0].x;
	float4 vColor = lerp(vAmbient, 
						vTexDiffuse * g_vDiffuseColor * g_vLightDiffuseColor * c_fLightScalar, 
						max(dot(vFinalNormal, -g_vLightDir), 0.0) * fShadow);

	// specular
	float fShininess = g_vMaterialExtra[0].y;
	float3 vHalf = normalize(normalize(g_vCameraPos.xyz - vWorldSpacePosition) - g_vLightDir);
	vColor += g_vSpecularColor * g_vLightSpecularColor * vTexSpecularMask * pow(max(dot(vHalf, vFinalNormal), 0.0), fShininess);
	
	// dimming
	vColor.xyz *= fDimming;

	// clamp the incoming for selection (this will have no affect if it isn't selected)
	vColor.xyz *= g_vSubSelectionColor.w;

	// add the selection color after the clamp to ensure there is always some indication of selection
	vColor.xyz += g_vSubSelectionColor.xyz;

	// alpha channel
	vColor.a = vTexDiffuse.a * g_vMaterialExtra[1].b;

	return vColor;
}


#include "Branch.fx"
#include "Frond.fx"
#include "LeafMesh.fx"
#include "LeafCard.fx"
#include "GroundPlane.fx"
#include "Mesh.fx"
