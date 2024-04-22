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

struct SBranchPixelShaderInput
{
	float4 vPosition   			: POSITION;
	float  fDimming				: COLOR;
	float3 vNormal				: TEXCOORD0;
	float3 vBinormal			: TEXCOORD1;
	float3 vTangent				: TEXCOORD2;
	float2 vDiffuseTexCoords	: TEXCOORD3;
	float2 vDetailTexCoords		: TEXCOORD4;
	float3 vBlendTexCoords		: TEXCOORD5;
	float4 vLightPosCoords		: TEXCOORD6;
	float3 vWorldSpacePosition	: TEXCOORD7;
};

struct SBranchDepthPixelShaderInput
{
	float4 vPosition   			: POSITION;
	float2 vDiffuseTexCoords	: TEXCOORD3;
};


///////////////////////////////////////////////////////////////////////  
//  BranchVS
	
SBranchPixelShaderInput BranchVS(float4 vPosition			: POSITION,
						         float3 vColor				: COLOR0,
						         float4 vNormal				: NORMAL,
						         float4 vBinormal			: TEXCOORD2,
						         float4 vTangent			: TEXCOORD1,
						         float4 vTexCoords			: TEXCOORD0,
						         float2 vDetailTexCoords	: TEXCOORD3,
						         float3 vLodData			: TEXCOORD4,
						         float3 vBlendTexCoords		: TEXCOORD5,
						         float4 vWindData			: TEXCOORD6)
{
    SBranchPixelShaderInput sOutput;
    
	float4 vLod = float4(lerp(vLodData.xyz, vPosition.xyz, g_vLodTransition.x), 1.0);

	vLod = Wind(vLod, vWindData);

	sOutput.vPosition = mul(vLod, g_mWorldViewProj);
	sOutput.vWorldSpacePosition = vLod.xyz;
	sOutput.vLightPosCoords = mul(float4(vLod.xyz, 1.0), g_mLightViewProj);

	sOutput.vNormal = vNormal.xyz;
	sOutput.vBinormal = vBinormal.xyz;
	sOutput.vTangent = vTangent.xyz;
	
	sOutput.vDiffuseTexCoords = vTexCoords.xy;
	sOutput.vDetailTexCoords = vDetailTexCoords;
	sOutput.vBlendTexCoords = vBlendTexCoords;
	
	sOutput.fDimming = vColor.x;

    return sOutput;
}



///////////////////////////////////////////////////////////////////////  
//  BranchVS
	
SBranchDepthPixelShaderInput BranchDepthVS(float4 vPosition		: POSITION,
								           float4 vTexCoords	: TEXCOORD0,
								           float3 vLodData		: TEXCOORD4,
										   float4 vWindData		: TEXCOORD6)

{
    SBranchDepthPixelShaderInput sOutput;
    
	float4 vLod = float4(lerp(vLodData.xyz, vPosition.xyz, g_vLodTransition.x), 1.0);

	vLod = Wind(vLod, vWindData);

	sOutput.vPosition = mul(vLod, g_mWorldViewProj);

	sOutput.vDiffuseTexCoords = vTexCoords.xy;

    return sOutput;
}


///////////////////////////////////////////////////////////////////////  
//  BranchPS

float4 BranchPS(SBranchPixelShaderInput sInput) : COLOR
{
	float4 vBaseColor = tex2D(samDiffuseMap, sInput.vDiffuseTexCoords);
	float4 vBaseSpecular = tex2D(samSpecularTransmissionMap, sInput.vDiffuseTexCoords);

	float fShadow = ShadowMapLookup(sInput.vLightPosCoords);

	float4 vBaseColorBlend = tex2D(samDiffuseMap, sInput.vBlendTexCoords.xy);

	float fInterp = sInput.vBlendTexCoords.z;
	vBaseColor = lerp(vBaseColor, vBaseColorBlend, fInterp);

	float4 vBaseNormal = tex2D(samNormalMap, sInput.vDiffuseTexCoords);
	float4 vNormalBlend = tex2D(samNormalMap, sInput.vBlendTexCoords.xy);
	vBaseNormal = lerp(vBaseNormal, vNormalBlend, fInterp);

	float4 vBaseLayer = Lighting(sInput.vNormal, sInput.vBinormal, sInput.vTangent, sInput.vWorldSpacePosition, sInput.fDimming, vBaseColor, vBaseNormal, vBaseSpecular, 1.0, fShadow);

	float4 vDetailColor = tex2D(samDetailMap, sInput.vDetailTexCoords);
	float4 vDetailNormal = tex2D(samDetailNormalMap, sInput.vDetailTexCoords);
	float4 vDetailLayer = Lighting(sInput.vNormal, sInput.vBinormal, sInput.vTangent, sInput.vWorldSpacePosition, sInput.fDimming, vDetailColor, vDetailNormal, float4(1.0, 1.0, 1.0, 1.0), 1.0, fShadow);

	float4 vFinal = lerp(vBaseLayer, vDetailLayer, saturate(vDetailColor.a));
	vFinal.a = vBaseColor.a;

	return vFinal;
}


///////////////////////////////////////////////////////////////////////  
//  BranchDepthPS

float4 BranchDepthPS(SBranchDepthPixelShaderInput sInput) : COLOR
{
	float4 vBaseColor = tex2D(samDiffuseMap, sInput.vDiffuseTexCoords);
	clip(vBaseColor.a - c_fAlphaTestValue);
	return vBaseColor;
}


///////////////////////////////////////////////////////////////////////  
//  Techniques

technique Branches
{
    pass P0
    {          
        VS_COMPILE_COMMAND BranchVS( );
        PS_COMPILE_COMMAND BranchPS( );
    }
}


///////////////////////////////////////////////////////////////////////  
//  Techniques

technique BranchesDepth
{
    pass P0
    {          
        VS_COMPILE_COMMAND BranchDepthVS( );
        PS_COMPILE_COMMAND BranchDepthPS( );
    }
}


