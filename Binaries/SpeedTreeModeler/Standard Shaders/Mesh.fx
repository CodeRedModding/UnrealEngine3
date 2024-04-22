///////////////////////////////////////////////////////////////////////  
//  Mesh Shaders
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
//  Branch-specific global variables

struct SMeshPixelShaderInput
{
	float4 vPosition   			: POSITION;
	float  fDimming				: COLOR;
	float3 vNormal				: TEXCOORD0;
	float3 vBinormal			: TEXCOORD1;
	float3 vTangent				: TEXCOORD2;
	float2 vDiffuseTexCoords	: TEXCOORD3;
	float4 vLightPosCoords		: TEXCOORD6;
	float3 vWorldSpacePosition	: TEXCOORD7;
};

struct SMeshDepthPixelShaderInput
{
	float4 vPosition   			: POSITION;
	float2 vDiffuseTexCoords	: TEXCOORD3;
};


///////////////////////////////////////////////////////////////////////  
//  MeshVS
	
SMeshPixelShaderInput MeshVS(float4 vPosition			: POSITION,
						     float3 vColor				: COLOR0,
						     float4 vNormal				: NORMAL,
						     float4 vBinormal			: TEXCOORD2,
						     float4 vTangent			: TEXCOORD1,
						     float2 vTexCoords			: TEXCOORD0)
{
    SMeshPixelShaderInput sOutput;
    
	sOutput.vPosition = mul(vPosition, g_mWorldViewProj);
	float3 vWorldSpacePosition = mul(vPosition, g_mModelView).xyz;
	sOutput.vWorldSpacePosition = vWorldSpacePosition.xyz;
	sOutput.vLightPosCoords = mul(float4(vWorldSpacePosition.xyz, 1.0), g_mLightViewProj);

	sOutput.vNormal = mul(float4(vNormal.xyz, 0.0f), g_mModelView).xyz;
	sOutput.vBinormal = mul(float4(vBinormal.xyz, 0.0f), g_mModelView).xyz;
	sOutput.vTangent = mul(float4(vTangent.xyz, 0.0f), g_mModelView).xyz;
	
	sOutput.vDiffuseTexCoords = vTexCoords;
	
	sOutput.fDimming = vColor.x;

    return sOutput;
}


///////////////////////////////////////////////////////////////////////  
//  MeshPS

float4 MeshPS(SMeshPixelShaderInput sInput) : COLOR
{
	float fShadow = ShadowMapLookup(sInput.vLightPosCoords);

	float4 vBaseColor = tex2D(samDiffuseMap, sInput.vDiffuseTexCoords);
	float4 vBaseNormal = tex2D(samNormalMap, sInput.vDiffuseTexCoords);
	float4 vTexSpecularTransmission = tex2D(samSpecularTransmissionMap, sInput.vDiffuseTexCoords);
	float4 vBaseLayer = LightingWithTransmission(sInput.vNormal, sInput.vBinormal, sInput.vTangent, sInput.vWorldSpacePosition, sInput.fDimming, vBaseColor, vBaseNormal, vTexSpecularTransmission, fShadow);
	
	clip(vBaseColor.a - c_fClipValue);

	return vBaseLayer;
}

///////////////////////////////////////////////////////////////////////  
//  Techniques

technique Meshes
{
    pass P0
    {          
        VS_COMPILE_COMMAND MeshVS( );
        PS_COMPILE_COMMAND MeshPS( );
    }
}

