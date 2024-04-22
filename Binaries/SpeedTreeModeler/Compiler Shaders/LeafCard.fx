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


float4x4    g_mLeafUnitSquare =                  // unit leaf card that's turned towards the camera and wind-rocked/rustled by the
            {                                    // vertex shader.  card is aligned on YZ plane and centered at (0.0f, 0.0f, 0.0f)
                float4(0.0f, 0.5f, 0.5f, 0.0f), 
                float4(0.0f, -0.5f, 0.5f, 0.0f), 
                float4(0.0f, -0.5f, -0.5f, 0.0f), 
                float4(0.0f, 0.5f, -0.5f, 0.0f)
            };


///////////////////////////////////////////////////////////////////////  
//  LeafCardVS
	
SPixelShaderInput LeafCardVS(float4 vPosition	: POSITION,
							 float3 vColor		: COLOR0,
							 float4 vNormal		: NORMAL,
							 float2 vTexCoords	: TEXCOORD0,
							 float4 vTangent	: TEXCOORD1,
							 float4 vBinormal	: TEXCOORD2,
							 float2 vSize		: TEXCOORD3,
							 float2 vLodData	: TEXCOORD4,
							 float2 vPivot		: TEXCOORD6)
{
    SPixelShaderInput sOutput;
    
    // access g_mLeafUnitSquare matrix with corner index and apply scales
	int nCorner = vPosition.w;
    float3 vPivotedPoint = g_mLeafUnitSquare[nCorner].xyz;

    // adjust by pivot point so rotation occurs around the correct point
    vPivotedPoint.yz -= vPivot.xy;
    
	float3 vCorner = vPivotedPoint * vSize.xxy * lerp(vLodData.x, vLodData.y, 1.0 - g_vLodTransition.x);

	// rotate the corner to face the camera
	float fAzimuth = g_vCameraAngles.x;
	float fPitch = g_vCameraAngles.y;
    float3x3 matRotation = RotationMatrix_zAxis(fAzimuth);
    matRotation = mul(matRotation, RotationMatrix_yAxis(fPitch));
	vCorner = mul(matRotation, vCorner);

	float4 vCenter = vPosition;
	vCenter.xyz += vCorner.xyz;
	vCenter.w = 1.0;

	sOutput.vPosition = mul(vCenter, g_mWorldViewProj);
	sOutput.vWorldSpacePosition = mul(vCenter, g_mWorld).xyz;

	sOutput.vNormal = mul(vNormal, g_mWorldInverseTranspose).xyz;
	sOutput.vBinormal = mul(vBinormal, g_mWorldInverseTranspose).xyz;
	sOutput.vTangent = mul(-vTangent, g_mWorldInverseTranspose).xyz;

	sOutput.vDiffuseTexCoords = vTexCoords;
	sOutput.fDimming = vColor.x;

    return sOutput;
}


///////////////////////////////////////////////////////////////////////  
//  LeafCardNormalVS
	
SPixelShaderInput LeafCardNormalVS(float4 vPosition	: POSITION,
							 float3 vColor		: COLOR0,
							 float4 vNormal		: NORMAL,
							 float2 vTexCoords	: TEXCOORD0,
							 float4 vTangent	: TEXCOORD1,
							 float4 vBinormal	: TEXCOORD2,
							 float2 vSize		: TEXCOORD3,
							 float2 vLodData	: TEXCOORD4,
							 float2 vPivot		: TEXCOORD6)
{
    SPixelShaderInput sOutput;
    
    // access g_mLeafUnitSquare matrix with corner index and apply scales
	int nCorner = vPosition.w;
    float3 vPivotedPoint = g_mLeafUnitSquare[nCorner].xyz;

    // adjust by pivot point so rotation occurs around the correct point
    vPivotedPoint.yz -= vPivot.xy;
    
	float3 vCorner = vPivotedPoint * vSize.xxy * lerp(vLodData.x, vLodData.y, 1.0 - g_vLodTransition.x);

	// rotate the corner to face the camera
	float fAzimuth = g_vCameraAngles.x;
	float fPitch = g_vCameraAngles.y;
    float3x3 matRotation = RotationMatrix_zAxis(fAzimuth);
    matRotation = mul(matRotation, RotationMatrix_yAxis(fPitch));
	vCorner = mul(matRotation, vCorner);

	float4 vCenter = vPosition;
	vCenter.xyz += vCorner.xyz;
	vCenter.w = 1.0;

	sOutput.vPosition = mul(vCenter, g_mWorldViewProj);
	sOutput.vWorldSpacePosition = mul(vNormal, g_mWorld).xyz;

	sOutput.vNormal = mul(vNormal, g_mWorldInverseTranspose).xyz;
	sOutput.vBinormal = mul(vBinormal, g_mWorldInverseTranspose).xyz;
	sOutput.vTangent = mul(-vTangent, g_mWorldInverseTranspose).xyz;

	sOutput.vDiffuseTexCoords = vTexCoords;
	sOutput.fDimming = vColor.x;

    return sOutput;
}



///////////////////////////////////////////////////////////////////////  
//  LeafCardNormalPS - normals leaf cards

float4 LeafCardNormalPS(SPixelShaderInput sInput) : COLOR
{
	float3 vReturn = normalize(sInput.vWorldSpacePosition);
	vReturn = vReturn * 0.5 + 0.5;
	
	//	alpha mask
	float4 vTexDiffuse = tex2D(samDiffuseMap, sInput.vDiffuseTexCoords);
	clip(vTexDiffuse.a - 0.3);
	float fAlpha = 0.0f;
	if (vTexDiffuse.a > 0.0f)
		fAlpha = 1.0f;

	float fAmbient = fAlpha * g_fAmbientAlpha * g_fAmbient;
	return float4(vReturn,  fAmbient / g_fLightScalar);
	
}

