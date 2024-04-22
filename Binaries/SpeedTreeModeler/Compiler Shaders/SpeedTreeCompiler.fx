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

//#define VS_COMPILE_COMMAND VertexProgram = compile arbvp1
//#define PS_COMPILE_COMMAND FragmentProgram = compile arbfp1
#define VS_COMPILE_COMMAND VertexProgram = compile glslv
#define PS_COMPILE_COMMAND FragmentProgram = compile glslf


///////////////////////////////////////////////////////////////////////  
//  Global Variables

float4x4    g_mWorldViewProj;
float4x4    g_mWorldInverseTranspose;
float4x4    g_mWorld;
float4x4	g_mWorldView;
float4		g_vCameraPos;
float3		g_vDiffuseColor;
float		g_fAmbient;			// the grey-scale material ambient value * ambient sky influence
float		g_fAmbientAlpha;	// the material ambient value alpha
float		g_fContrast;
float		g_fLightScalar;
float3		g_vTransmissionColor;
float		g_fDiffuseAlphaTest;


float2		g_vCameraAngles;
float4		g_vLodTransition;


float4		g_vDebug;


const float c_fAlphaTestValue = 0.3f;
const float c_fSkyAmbientInterpolationValue  = 0.8f;


///////////////////////////////////////////////////////////////////////  
//  Textures

texture     g_tDiffuseMap;
texture     g_tNormalMap; 
texture		g_tTransmissionMap;
texture     g_tDetailMap; 
texture     g_tSpecularMap; 


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

sampler2D samTransmissionMap : register(s2) = sampler_state
{
    Texture = <g_tTransmissionMap>;
};


///////////////////////////////////////////////////////////////////////  
//  Utility functions

struct SPixelShaderInput
{
	float4 vPosition   			: POSITION;
	float  fDimming				: COLOR0;
	float3 vNormal				: TEXCOORD0;
	float3 vBinormal			: TEXCOORD1;
	float3 vTangent				: TEXCOORD2;
	float2 vDiffuseTexCoords	: TEXCOORD3;
	float3 vWorldSpacePosition	: TEXCOORD4;
	float2 vDetailTexCoords		: TEXCOORD5;
	float3 vBlendTexCoords		: TEXCOORD6;
};


#include "Utility.fx"
#include "Branch.fx"
#include "LeafCard.fx"




///////////////////////////////////////////////////////////////////////  
///////////////////////////////////////////////////////////////////////  
///////////////////////////////////////////////////////////////////////  
//  Techniques


technique Transmissions
{
	pass P0
	{
		VS_COMPILE_COMMAND BranchVS( );
        PS_COMPILE_COMMAND TransmissionsPS( );
	}
}

///////////////////////////////////////////////////////////////////////  
// Branches

technique Branches
{
    pass P0
    {          
        VS_COMPILE_COMMAND BranchVS( );
        PS_COMPILE_COMMAND DiffusePS( );
    }
}

technique BranchNormals
{
    pass P0
    {          
        VS_COMPILE_COMMAND BranchNormalVS( );
        PS_COMPILE_COMMAND BranchNormalPS( );
    }
}



///////////////////////////////////////////////////////////////////////  
// Fronds

technique Fronds
{
    pass P0
    {          
        VS_COMPILE_COMMAND BranchVS( );
        PS_COMPILE_COMMAND DiffusePS( );
    }
}

technique FrondNormals
{
    pass P0
    {          
        VS_COMPILE_COMMAND BranchNormalVS( );
        PS_COMPILE_COMMAND AlphaMaskedNormalPS( );
    }
}


///////////////////////////////////////////////////////////////////////  
// LeafMeshes

technique LeafMeshes
{
    pass P0
    {          
        VS_COMPILE_COMMAND BranchVS( );
        PS_COMPILE_COMMAND DiffusePS( );
    }
}

technique LeafMeshNormals
{
    pass P0
    {          
        VS_COMPILE_COMMAND BranchNormalVS( );
        PS_COMPILE_COMMAND AlphaMaskedNormalPS( );
    }
}


///////////////////////////////////////////////////////////////////////  
// LeafCards

technique LeafCards
{
    pass P0
    {          
        VS_COMPILE_COMMAND LeafCardVS( );
        PS_COMPILE_COMMAND DiffusePS( );
    }
}

technique LeafCardNormals
{
    pass P0
    {     
		VS_COMPILE_COMMAND LeafCardNormalVS( );
		PS_COMPILE_COMMAND LeafCardNormalPS( );
		//PS_COMPILE_COMMAND DiffusePS( );
		//VS_COMPILE_COMMAND LeafCardNormalVS( );
    }
}

technique LeafCardTransmissions
{
	pass P0
	{
		VS_COMPILE_COMMAND LeafCardVS( );
        PS_COMPILE_COMMAND TransmissionsPS( );
	}
}

