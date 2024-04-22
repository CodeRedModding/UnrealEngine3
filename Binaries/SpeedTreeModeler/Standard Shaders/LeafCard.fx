///////////////////////////////////////////////////////////////////////  
//  Leaf Card Shaders
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

struct SLeafCardPixelShaderInput
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

struct SLeafCardDepthPixelShaderInput
{
	float4 vPosition   			: POSITION;
	float2 vDiffuseTexCoords	: TEXCOORD3;
};


float4x4    g_mLeafUnitSquare =                  // unit leaf card that's turned towards the camera and wind-rocked/rustled by the
            {                                    // vertex shader.  Card is aligned on YZ plane and centered at (0.0f, 0.0f, 0.0f)
                float4(0.0f, 0.5f, 0.5f, 0.0f), 
                float4(0.0f, -0.5f, 0.5f, 0.0f), 
                float4(0.0f, -0.5f, -0.5f, 0.0f), 
                float4(0.0f, 0.5f, -0.5f, 0.0f)
            };


//#define HANGING_CARDS

///////////////////////////////////////////////////////////////////////  
//  LeafCardVS
	
SLeafCardPixelShaderInput LeafCardVS(float4 vPosition	: POSITION,
							         float3 vColor		: COLOR0,
							         float4 vNormal		: NORMAL,
							         float4 vTangent	: TEXCOORD1,
							         float4 vBinormal	: TEXCOORD2,
							         float2 vSize		: TEXCOORD3,
							         float2 vPivot		: TEXCOORD6,
							         float3 vTexCoords	: TEXCOORD0,
							         float2 vLodData	: TEXCOORD4,
									 float4 vWindData	: TEXCOORD7)
{
    SLeafCardPixelShaderInput sOutput;
    
    // access g_mLeafUnitSquare matrix with corner index and apply scales
	int nCorner = vPosition.w;
    float3 vPivotedPoint = g_mLeafUnitSquare[nCorner].xyz;

    // adjust by pivot point so rotation occurs around the correct point
    vPivotedPoint.yz -= vPivot.xy;
    
	//float3 vCorner = vPivotedPoint * vSize.xxy * lerp(1.0f, vLodData.x, g_vLodTransition.y) * lerp(vLodData.y, 1.0, g_vLodTransition.x);
	float3 vCorner = vPivotedPoint * vSize.xxy * lerp(vLodData.x, vLodData.y, 1.0 - g_vLodTransition.x);

	// rotate the corner to face the camera
	float fAzimuth = g_vCameraAngles.x;
	float fPitch = g_vCameraAngles.y;

#ifdef HANGING_CARDS
	float fDir = g_vAmbientColor.w * 2.0f;
	float3 vCross = normalize(cross(g_vViewDir, float3(0.0, 0.0, 1.0f)));
	float3 vLeaf = normalize(float3(vPosition.xy, 0.0));
	fDir *= dot(vLeaf, vCross);
    float3x3 matTemp = RotationMatrix_xAxis(fDir);
    float3x3 matRotation = mul(RotationMatrix_zAxis(fAzimuth), matTemp);
    matRotation = mul(matRotation, RotationMatrix_yAxis(fPitch));
#else
    float3x3 matRotation = RotationMatrix_zAxis(fAzimuth);
    matRotation = mul(matRotation, RotationMatrix_yAxis(fPitch));
#endif	

	vCorner = mul(matRotation, vCorner);

	float4 vCenter = vPosition;
	vCenter.xyz += vCorner.xyz;
	vCenter.xyz += g_vViewDir * vTangent.w;
	vCenter.w = 1.0;
	
	// move away from the light by the size of the diagonal of the card if necessary (i.e., drawing shadows)
	float fShiftDistance = sqrt(vSize.x * vSize.x + vSize.y * vSize.y) * 0.5f * g_fLeafCardLightShift;
	vCenter.xyz += g_vLightDir * fShiftDistance * 0.5f;
	
	vCenter = LeafWind(vCenter, vNormal.xyz, vTexCoords.z);
	vCenter = Wind(vCenter, vWindData);

	sOutput.vPosition = mul(vCenter, g_mWorldViewProj);
	sOutput.vWorldSpacePosition = vCenter.xyz;
	sOutput.vLightPosCoords = mul(float4(vCenter.xyz, 1.0), g_mLightViewProj);

	sOutput.vNormal = vNormal.xyz;
	sOutput.vBinormal = vBinormal.xyz;
	sOutput.vTangent = -vTangent.xyz;

	sOutput.vDiffuseTexCoords = vTexCoords;
	sOutput.fDimming = vColor.x;

    return sOutput;
}

//#define GPU_GEMS_3_STYLE

///////////////////////////////////////////////////////////////////////  
//  LeafCardDepthVS
	
SLeafCardDepthPixelShaderInput LeafCardDepthVS(float4 vPosition		: POSITION,
											   float4 vNormal		: NORMAL,
											   float4 vTangent		: TEXCOORD1,
							                   float2 vSize			: TEXCOORD3,
							                   float2 vPivot		: TEXCOORD6,
							                   float3 vTexCoords	: TEXCOORD0,
							                   float2 vLodData		: TEXCOORD4,
											   float4 vWindData		: TEXCOORD7)
{
    SLeafCardDepthPixelShaderInput sOutput;
    
    // access g_mLeafUnitSquare matrix with corner index and apply scales
	int nCorner = vPosition.w;
    float3 vPivotedPoint = g_mLeafUnitSquare[nCorner].xyz;

    // adjust by pivot point so rotation occurs around the correct point
    vPivotedPoint.yz -= vPivot.xy;
    
	//float3 vCorner = vPivotedPoint * vSize.xxy * lerp(1.0f, vLodData.x, g_vLodTransition.y) * lerp(vLodData.y, 1.0, g_vLodTransition.x);
	float3 vCorner = vPivotedPoint * vSize.xxy * lerp(vLodData.x, vLodData.y, 1.0 - g_vLodTransition.x);

	// rotate the corner to face the camera
	float fAzimuth = g_vCameraAngles.x;
	float fPitch = g_vCameraAngles.y;
    float3x3 matRotation = RotationMatrix_zAxis(fAzimuth);
    matRotation = mul(matRotation, RotationMatrix_yAxis(fPitch));
	vCorner = mul(matRotation, vCorner);

	float4 vCenter = vPosition;
	vCenter.xyz += vCorner.xyz;
	vCenter.xyz += g_vViewDir * vTangent.w;
	vCenter.w = 1.0;
	
	// move away from the light by the size of the diagonal of the card if necessary (i.e., drawing shadows)
	float fShiftDistance = sqrt(vSize.x * vSize.x + vSize.y * vSize.y) * 0.5f * g_fLeafCardLightShift;

#ifndef GPU_GEMS_3_STYLE
	vCenter.xyz += g_vLightDir * fShiftDistance * 0.5f;
#endif

	vCenter = LeafWind(vCenter, vNormal.xyz, vTexCoords.z);
	vCenter = Wind(vCenter, vWindData);

	sOutput.vPosition = mul(vCenter, g_mWorldViewProj);
	//sOutput.vPosition = vCenter;

	sOutput.vDiffuseTexCoords = vTexCoords;

    return sOutput;
}


///////////////////////////////////////////////////////////////////////  
//  LeafCardPS

float4 LeafCardPS(SLeafCardPixelShaderInput sInput) : COLOR
{
	float4 vTexDiffuse = tex2D(samDiffuseMap, sInput.vDiffuseTexCoords);
	float4 vTexNormal = tex2D(samNormalMap, sInput.vDiffuseTexCoords);
	float4 vTexSpecularTransmission = tex2D(samSpecularTransmissionMap, sInput.vDiffuseTexCoords);

#ifdef GPU_GEMS_3_STYLE
	float4 vLightPosCoords = mul(float4(sInput.vWorldSpacePosition.xyz + (g_vViewDir * ((vTexNormal.a * vTexDiffuse.a) - 0.5f) * 10.0), 1.0), g_mLightViewProj);
	float fShadow = ShadowMapLookup(vLightPosCoords);
#else
	float fShadow = ShadowMapLookup(sInput.vLightPosCoords);
#endif

	clip(vTexDiffuse.a - c_fClipValue);

	return LightingWithTransmission(sInput.vNormal, sInput.vBinormal, sInput.vTangent, sInput.vWorldSpacePosition, sInput.fDimming, vTexDiffuse, vTexNormal, vTexSpecularTransmission, fShadow);
}


///////////////////////////////////////////////////////////////////////  
//  LeafCardDepthPS

float4 LeafCardDepthPS(SLeafCardDepthPixelShaderInput sInput) : COLOR
{
	float4 vTexDiffuse = tex2D(samDiffuseMap, sInput.vDiffuseTexCoords);
	clip(vTexDiffuse.a - c_fAlphaTestValue);
	return vTexDiffuse;
}


///////////////////////////////////////////////////////////////////////  
//  Techniques

technique LeafCards
{
    pass P0
    {          
        VS_COMPILE_COMMAND LeafCardVS( );
        PS_COMPILE_COMMAND LeafCardPS( );
    }
}

technique LeafCardsDepth
{
    pass P0
    {          
        VS_COMPILE_COMMAND LeafCardDepthVS( );
        PS_COMPILE_COMMAND LeafCardDepthPS( );
    }
}


