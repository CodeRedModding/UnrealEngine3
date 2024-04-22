///////////////////////////////////////////////////////////////////////  
//  Branch Shaders
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



///////////////////////////////////////////////////////////////////////  
//  BranchVS
	
SPixelShaderInput BranchVS(float4 vPosition			: POSITION,
						   float3 vColor			: COLOR0,
						   float4 vNormal			: NORMAL,
						   float2 vTexCoords		: TEXCOORD0,
						   float4 vTangent			: TEXCOORD1,
						   float4 vBinormal			: TEXCOORD2,
						   float2 vDetailTexCoords	: TEXCOORD3,
						   float3 vLodData			: TEXCOORD4,
						   float3 vBlendTexCoords	: TEXCOORD5)
{
    SPixelShaderInput sOutput;
    
    sOutput.vWorldSpacePosition = mul(vPosition, g_mWorld).xyz;
    
	float4 vLod = float4(lerp(vLodData.xyz, vPosition.xyz, g_vLodTransition.x), 1.0);
	sOutput.vNormal = mul(vNormal, g_mWorldInverseTranspose).xyz;
	sOutput.vPosition = mul(vLod, g_mWorldViewProj);
	
	sOutput.vDiffuseTexCoords = vTexCoords;
	sOutput.fDimming = vColor.x;

    return sOutput;
}

///////////////////////////////////////////////////////////////////////  
//  BranchNormalVS
	
SPixelShaderInput BranchNormalVS(float4 vPosition	: POSITION,
						   float3 vColor			: COLOR0,
						   float4 vNormal			: NORMAL,
						   float2 vTexCoords		: TEXCOORD0,
						   float4 vTangent			: TEXCOORD1,
						   float4 vBinormal			: TEXCOORD2,
						   float2 vDetailTexCoords	: TEXCOORD3,
						   float3 vLodData			: TEXCOORD4,
						   float3 vBlendTexCoords	: TEXCOORD5)
{
    SPixelShaderInput sOutput;
    
    // this is the only difference for normals
	sOutput.vWorldSpacePosition = mul(vNormal, g_mWorld).xyz;
    
	float4 vLod = float4(lerp(vLodData.xyz, vPosition.xyz, g_vLodTransition.x), 1.0);
	sOutput.vPosition = mul(vLod, g_mWorldViewProj);
	sOutput.vNormal = mul(vNormal, g_mWorldInverseTranspose).xyz;
	
	sOutput.vDiffuseTexCoords = vTexCoords;
	sOutput.fDimming = vColor.x;

    return sOutput;
}


///////////////////////////////////////////////////////////////////////  
//  DiffusePS - this is used for ALL geometry types

float4 DiffusePS(SPixelShaderInput sInput) : COLOR
{
	float4 vBaseColor = tex2D(samDiffuseMap, sInput.vDiffuseTexCoords);
	float3 vRGB = vBaseColor.rgb * g_vDiffuseColor * g_fLightScalar;

	// alpha test
	clip(vBaseColor.a - (1.0 - g_fDiffuseAlphaTest));
	//clip(vBaseColor.a - g_fDiffuseAlphaTest);
	
	return float4(vRGB * sInput.fDimming, vBaseColor.a);
}


///////////////////////////////////////////////////////////////////////  
//  BranchNormalPS - normals for branches

float4 BranchNormalPS(SPixelShaderInput sInput) : COLOR
{
	float3 vReturn = normalize(sInput.vWorldSpacePosition);
	vReturn = vReturn * 0.5 + 0.5;
	
	float fAmbient = g_fAmbientAlpha * g_fAmbient;
	float fAlpha = fAmbient / g_fLightScalar;

	return float4(vReturn, fAlpha);
}


///////////////////////////////////////////////////////////////////////  
//  TransmissionsPS - transmission values

float4 TransmissionsPS(SPixelShaderInput sInput) : COLOR
{
	float4 vBaseColor = tex2D(samTransmissionMap, sInput.vDiffuseTexCoords);
	float4 vDiffuseColor = tex2D(samDiffuseMap, sInput.vDiffuseTexCoords);
	
	clip(vDiffuseColor.a - 0.1);

	float3 vRGB = g_vTransmissionColor;
	float fAlpha = vBaseColor.a;
	
	return float4(vRGB, fAlpha);
}




///////////////////////////////////////////////////////////////////////  
//  AlphaMaskedNormalPS - normals for fronds, leaf cards, leaf meshes

float4 AlphaMaskedNormalPS(SPixelShaderInput sInput) : COLOR
{
	float3 vReturn = normalize(sInput.vWorldSpacePosition);
	vReturn = vReturn * 0.5 + 0.5;
	

	float4 vTexDiffuse = tex2D(samDiffuseMap, sInput.vDiffuseTexCoords);
	float fAlpha = 0.0f;
	if (vTexDiffuse.a > 0.02)
		fAlpha = 1.0f;
	
	float fAmbient = fAlpha * g_fAmbientAlpha * g_fAmbient;
	
	fAlpha = fAmbient / g_fLightScalar;
	clip(fAlpha - 0.01);
	
	return float4(vReturn, fAlpha);
}

