///////////////////////////////////////////////////////////////////////  
//  Leaf Mesh Shaders
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
//  Leaf-specific global variables

struct SLeafMeshPixelShaderInput
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

struct SLeafMeshDepthPixelShaderInput
{
	float4 vPosition   			: POSITION;
	float2 vDiffuseTexCoords	: TEXCOORD3;
};


///////////////////////////////////////////////////////////////////////  
//  LeafMeshVS
	
SLeafMeshPixelShaderInput LeafMeshVS(float4 vPosition		: POSITION,
							         float3 vColor			: COLOR0,
							         float4 vNormal			: NORMAL,
							         float4 vBinormal		: TEXCOORD2,
							         float4 vTangent		: TEXCOORD1,
							         float4 vTexCoords		: TEXCOORD0,
							         float3 vLodData		: TEXCOORD4,
									 float4 vWindData		: TEXCOORD6)
{
    SLeafMeshPixelShaderInput sOutput;

	float4 vLod = float4(lerp(vPosition.xyz, vLodData.xyz, 1.0 - g_vLodTransition.x), 1.0);

	vLod = LeafWind(vLod, vNormal.xyz, vTexCoords.z);
	vLod = Wind(vLod, vWindData);

	sOutput.vPosition = mul(vLod, g_mWorldViewProj);
	sOutput.vWorldSpacePosition = vLod.xyz;
	sOutput.vLightPosCoords = mul(float4(vLod.xyz, 1.0), g_mLightViewProj);

	sOutput.vNormal = vNormal.xyz;
	sOutput.vBinormal = vBinormal.xyz;
	sOutput.vTangent = -vTangent.xyz;
	
	sOutput.vDiffuseTexCoords = vTexCoords.xy;
	
	sOutput.fDimming = vColor.x;
    return sOutput;
}



///////////////////////////////////////////////////////////////////////  
//  LeafMeshDepthVS
	
SLeafMeshDepthPixelShaderInput LeafMeshDepthVS(float4 vPosition		: POSITION,
											   float4 vNormal		: NORMAL,
								               float4 vTexCoords	: TEXCOORD0,
							                   float3 vLodData		: TEXCOORD4,
											   float4 vWindData		: TEXCOORD6)
{
    SLeafMeshDepthPixelShaderInput sOutput;

	float4 vLod = float4(lerp(vPosition.xyz, vLodData.xyz, 1.0 - g_vLodTransition.x), 1.0);

	vLod = LeafWind(vLod, vNormal.xyz, vTexCoords.z);
	vLod = Wind(vLod, vWindData);

	sOutput.vPosition = mul(vLod, g_mWorldViewProj);
	
	sOutput.vDiffuseTexCoords = vTexCoords.xy;
	
    return sOutput;
}



///////////////////////////////////////////////////////////////////////  
//  LeafMeshPS

float4 LeafMeshPS(SLeafMeshPixelShaderInput sInput) : COLOR
{
	float4 vTexDiffuse = tex2D(samDiffuseMap, sInput.vDiffuseTexCoords);

	float fShadow = ShadowMapLookup(sInput.vLightPosCoords);

	float4 vTexNormal = tex2D(samNormalMap, sInput.vDiffuseTexCoords);
	float4 vTexSpecularTransmission = tex2D(samSpecularTransmissionMap, sInput.vDiffuseTexCoords);

	clip(vTexDiffuse.a - c_fClipValue);

	return LightingWithTransmission(sInput.vNormal, sInput.vBinormal, sInput.vTangent, sInput.vWorldSpacePosition, sInput.fDimming, vTexDiffuse, vTexNormal, vTexSpecularTransmission, fShadow);
}



///////////////////////////////////////////////////////////////////////  
//  LeafMeshDepthPS

float4 LeafMeshDepthPS(SLeafMeshDepthPixelShaderInput sInput) : COLOR
{
	float4 vTexDiffuse = tex2D(samDiffuseMap, sInput.vDiffuseTexCoords);
	clip(vTexDiffuse.a - c_fAlphaTestValue);
	return vTexDiffuse;
}


///////////////////////////////////////////////////////////////////////  
//  Techniques

technique LeafMeshes
{
    pass P0
    {          
        VS_COMPILE_COMMAND LeafMeshVS( );
        PS_COMPILE_COMMAND LeafMeshPS( );
    }
}

technique LeafMeshesDepth
{
    pass P0
    {          
        VS_COMPILE_COMMAND LeafMeshDepthVS( );
        PS_COMPILE_COMMAND LeafMeshDepthPS( );
    }
}


