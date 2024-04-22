///////////////////////////////////////////////////////////////////////  
//  Frond Shaders
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

///////////////////////////////////////////////////////////////////////  
//  Frond-specific global variables

struct SFrondPixelShaderInput
{
	float4 vPosition   			: POSITION;
	float  fDimming				: COLOR;
	float3 vNormal				: TEXCOORD0;
	float3 vBinormal			: TEXCOORD1;
	float3 vTangent				: TEXCOORD2;
	float2 vDiffuseTexCoords	: TEXCOORD3;
	float4 vLightPosCoords		: TEXCOORD4;
	float3 vWorldSpacePosition	: TEXCOORD5;
};

struct SFrondDepthPixelShaderInput
{
	float4 vPosition   			: POSITION;
	float2 vDiffuseTexCoords	: TEXCOORD3;
};

const float c_fRippleSpeed = 3.0f;

#define RIPPLE

float3 ComputeRippleOffset(float3 vLodData, float4 vPosition, float2 vRipple, float4 vNormal)
{
	const float fTime = g_vWindTimes.z;

	float3 vOffset = vNormal.xyz * (sin(vRipple[0] + fTime) * cos(vRipple[0] + fTime + vLodData.z));
	vOffset = vOffset * 0.5f * vRipple[1] * g_vWindFrondRipple.x;

	return vOffset;
}

float4 RippleFrond(float3 vLodData, float4 vPosition, float2 vRipple, float4 vNormal)
{
	float3 vOffset = ComputeRippleOffset(vLodData, vPosition, vRipple, vNormal);

	float4 vLod = float4(lerp(vLodData.xyz, vPosition.xyz, g_vLodTransition.x), 1.0);
	vLod.xyz += vOffset;

	return vLod;
}


float4 RippleFrondAndLighting(float3 vLodData, float4 vPosition, float2 vRipple, inout float4 vNormal, inout float4 vBinormal, inout float4 vTangent)
{
	float3 vOffset = ComputeRippleOffset(vLodData, vPosition, vRipple, vNormal);

	float4 vLod = float4(lerp(vLodData.xyz, vPosition.xyz, g_vLodTransition.x), 1.0);
	vLod.xyz += vOffset;

	vTangent.xyz = normalize(vTangent.xyz + vOffset);
	float3 vNewNormal = normalize(cross(vTangent.xyz, vBinormal.xyz));
	if (dot(vNewNormal, vNormal.xyz) < 0.0)
		vNewNormal = -vNewNormal;
	vNormal.xyz = vNewNormal;

	return vLod;
}

///////////////////////////////////////////////////////////////////////  
//  FrondVS
	
SFrondPixelShaderInput FrondVS(float4 vPosition			: POSITION,
						       float3 vColor			: COLOR0,
						       float4 vNormal			: NORMAL,
						       float4 vBinormal			: TEXCOORD2,
						       float4 vTangent			: TEXCOORD1,
						       float4 vTexCoords		: TEXCOORD0,
						       float3 vLodData			: TEXCOORD4,
						       float4 vWindData			: TEXCOORD6)
{
    SFrondPixelShaderInput sOutput;
    
#ifdef RIPPLE
	float4 vLod = RippleFrondAndLighting(vLodData, vPosition, vTexCoords.zw, vNormal, vBinormal, vTangent);
#else
	float4 vLod = float4(lerp(vLodData.xyz, vPosition.xyz, g_vLodTransition.x), 1.0);
#endif

	vLod = Wind(vLod, vWindData);

	sOutput.vPosition = mul(vLod, g_mWorldViewProj);
	sOutput.vWorldSpacePosition = vLod.xyz;
	sOutput.vLightPosCoords = mul(float4(vLod.xyz, 1.0), g_mLightViewProj);

// #ifdef RIPPLE
// 	float3 vNewTangent = normalize(vTangent.xyz + vOffset);
// 	sOutput.vTangent = vNewTangent;
// 	float3 vNewNormal = normalize(cross(vNewTangent, vBinormal.xyz));
// 	if (dot(vNewNormal, vNormal.xyz) < 0.0)
// 		vNewNormal = -vNewNormal;
// 	sOutput.vNormal = vNewNormal;
// 	sOutput.vBinormal = vBinormal.xyz;
// #else
	sOutput.vNormal = vNormal.xyz;
	sOutput.vBinormal = vBinormal.xyz;
	sOutput.vTangent = vTangent.xyz;
// #endif
	sOutput.vDiffuseTexCoords = vTexCoords.xy;
	
	sOutput.fDimming = vColor.x;

    return sOutput;
}


///////////////////////////////////////////////////////////////////////  
//  FrondDepthVS
	
SFrondDepthPixelShaderInput FrondDepthVS(float4 vPosition	: POSITION,
										 float4 vNormal		: NORMAL,
							             float4 vTexCoords	: TEXCOORD0,
							             float3 vLodData	: TEXCOORD4,
									     float4 vWindData	: TEXCOORD6)
{
    SFrondDepthPixelShaderInput sOutput;

#ifdef RIPPLE
	float4 vLod = RippleFrond(vLodData, vPosition, vTexCoords.zw, vNormal);
#else
	float4 vLod = float4(lerp(vLodData.xyz, vPosition.xyz, g_vLodTransition.x), 1.0);
#endif
	
	vLod = Wind(vLod, vWindData);

	sOutput.vPosition = mul(vLod, g_mWorldViewProj);

	sOutput.vDiffuseTexCoords = vTexCoords.xy;

    return sOutput;
}


///////////////////////////////////////////////////////////////////////  
//  FrondPS

float4 FrondPS(SFrondPixelShaderInput sInput) : COLOR
{
	float4 vTexDiffuse = tex2D(samDiffuseMap, sInput.vDiffuseTexCoords);

	float fShadow = ShadowMapLookup(sInput.vLightPosCoords);

	float4 vTexNormal = tex2D(samNormalMap, sInput.vDiffuseTexCoords);
	float4 vTexSpecularTransmission = tex2D(samSpecularTransmissionMap, sInput.vDiffuseTexCoords);

	clip(vTexDiffuse.a - c_fClipValue);

	return LightingWithTransmission(sInput.vNormal, sInput.vBinormal, sInput.vTangent, sInput.vWorldSpacePosition, sInput.fDimming, vTexDiffuse, vTexNormal, vTexSpecularTransmission, fShadow);
}


///////////////////////////////////////////////////////////////////////  
//  FrondDepthPS

float4 FrondDepthPS(SFrondDepthPixelShaderInput sInput) : COLOR
{
	float4 vTexDiffuse = tex2D(samDiffuseMap, sInput.vDiffuseTexCoords);
	clip(vTexDiffuse.a - c_fAlphaTestValue);
	return vTexDiffuse;
}

///////////////////////////////////////////////////////////////////////  
//  Techniques

technique Fronds
{
    pass P0
    {          
        VS_COMPILE_COMMAND FrondVS( );
        PS_COMPILE_COMMAND FrondPS( );
    }
}

technique FrondsDepth
{
    pass P0
    {          
        VS_COMPILE_COMMAND FrondDepthVS( );
        PS_COMPILE_COMMAND FrondDepthPS( );
    }
}
