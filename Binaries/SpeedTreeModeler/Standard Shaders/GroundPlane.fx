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

struct SGroundPlanePixelShaderInput
{
	float4 vPosition   			: POSITION;
	float  fDimming				: COLOR;
	float4 vLightPosCoords		: TEXCOORD0;
// 	float2 vDiffuseTexCoords	: TEXCOORD3;
};


///////////////////////////////////////////////////////////////////////  
//  BranchVS
	
SGroundPlanePixelShaderInput GroundPlaneVS(float4 vPosition		: POSITION,
										   float4 vColor		: COLOR)
										   //float2 vTexCoords	: TEXCOORD0)
{
    SGroundPlanePixelShaderInput sOutput;
    
	sOutput.vPosition = mul(vPosition, g_mWorldViewProj);

 	//sOutput.vDiffuseTexCoords = vTexCoords;

	sOutput.vLightPosCoords = mul(float4(vPosition.xyz, 1.0), g_mLightViewProj);
	sOutput.fDimming = vColor.a;

    return sOutput;
}


///////////////////////////////////////////////////////////////////////  
//  GroundPlanePS

float4 GroundPlanePS(SGroundPlanePixelShaderInput sInput) : COLOR
{
// 	float fValue = tex2D(samDepthShadow, sInput.vDiffuseTexCoords).r;
// 	return float4(fValue, fValue, fValue, 1.0f);

	const float c_fMaxAlpha = 0.25f;

	float fShadow = ShadowMapLookup(sInput.vLightPosCoords);
	fShadow = (1.0 - fShadow) * c_fMaxAlpha;
	fShadow *= sInput.fDimming;
	return float4(0.0, 0.0, 0.0, fShadow);
}


///////////////////////////////////////////////////////////////////////  
//  Techniques

technique GroundPlane
{
    pass P0
    {          
        VS_COMPILE_COMMAND GroundPlaneVS( );
        PS_COMPILE_COMMAND GroundPlanePS( );
    }
}


