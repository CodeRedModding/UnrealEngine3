/*=============================================================================
	Common.hlsl: Common shader code.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

/*
	Defined by the C++ code:
		OPAQUELAYER			- Set when a translucent material is being rendered opaquely
		USE_FP_BLENDING
*/

float square(float A)
{
	return A * A;
}

float4 expandRGBE( float4 RGBE )
{
	return float4( ldexp( RGBE.xyz, 255.0 * RGBE.w - 128.0 ), 1.0 );
}

float4 expandCompressedRGBE( float4 RGBE )
{
	return float4( ldexp( RGBE.xyz, 255.0 / 16.0 * RGBE.w - 8.0 ), 1.0 );
}

float4 gammaCorrect( float4 Color )
{
	return float4( pow( Color.xyz, 2.2 ), Color.w );
}

#define PI 3.141592653
#if 1
half3 pointLightPhong(half3 DiffuseColor,half3 SpecularColor,half SpecularPower, half3 L, float3 E, half3 N, float3 R)
{
	half3	DiffuseLighting = max(dot(N,L),0),
			SpecularLighting = pow(max(dot(R,L),0.0001),SpecularPower);

	return DiffuseColor * DiffuseLighting + SpecularLighting * SpecularColor;
}
#else // Ashikhmin-Shirley BRDF
float3 pointLightPhong(float3 DiffuseColor,float3 SpecularColor,float SpecularPower,float3 L,float3 E,float3 N,float3 R)
{
	float3	H = normalize(E+L);
	
	float2	Sp = float2(SpecularPower,SpecularPower);

	float3	Rs = 0.7,//SpecularColor,
			Rd = 0.2;//DiffuseColor;

	float3	P5 = float3(1,1,0) - pow(1.0 - float3(dot(N,L),dot(N,E),dot(H,E) * 2.0) / 2.0,5.0);

	float3	FresnelTerm = Rs + (1.0 - Rs) * P5.z;

	float3	H2 = H*H;

	float3	DiffuseLighting = Rd * (1.0 - Rs) * P5.x * P5.y,
			SpecularLighting = sqrt((Sp.x + 1.0) * (Sp.y + 1.0)) * (23.0/8.0) / 28.0 * (pow(dot(N,H),dot(Sp,H2.xy) / (1.0 - H2.z)) * FresnelTerm / (dot(H,E) * max(dot(N,E),dot(N,L))));

	return dot(L,N) < 0 ? 0 : (max(DiffuseLighting,0) + max(SpecularLighting,0)) * 28.0 / (23.0 * PI);
}
#endif

half3 hemisphereLightPhong(half3 DiffuseColor, half3 S, half3 N)
{
	return DiffuseColor * square(0.5 + 0.5 * dot(N,S));
}

sampler	SHBasisTexture;

float3 pointLightSHM(float3 DiffuseColor,float3 SpecularColor,float SpecularPower,float4 SH,float3 L,float3 E,float3 N,float3 R)
{
	float	DiffuseLighting = max(dot(SH,texCUBE(SHBasisTexture,L) * 2 -1),0),
			SpecularLighting = pow(max(dot(R,L),0),SpecularPower);

	return DiffuseColor * DiffuseLighting + SpecularColor * SpecularLighting;
}

sampler	SHSkyBasisTexture;

float3 hemisphereLightSHM(float3 DiffuseColor,float4 SH,float3 S)
{
	return DiffuseColor * max(dot(SH,texCUBE(SHSkyBasisTexture,S) * 2 -1),0);
}

sampler	PreviousLightingTexture;
float4	ScreenPositionScaleBias;

half3 previousLighting(float4 ScreenPosition)
{
	return tex2D(PreviousLightingTexture,ScreenPosition.xy / ScreenPosition.w * ScreenPositionScaleBias.xy + ScreenPositionScaleBias.wz).rgb;
}

float radialAttenuation(float3 WorldLightVector)
{
	return square(max(1.0f - dot(WorldLightVector,WorldLightVector),0));
}

sampler	LightTexture;

half3 lightFunction(float4 ScreenPosition)
{
	return tex2D(LightTexture,ScreenPosition.xy / ScreenPosition.w * ScreenPositionScaleBias.xy + ScreenPositionScaleBias.wz);
}

half previousDepth(float4 ScreenPosition)
{
	return tex2D(PreviousLightingTexture,ScreenPosition.xy / ScreenPosition.w * ScreenPositionScaleBias.xy + ScreenPositionScaleBias.wz).a;
}

float4 additiveBlendBase(float4 ScreenPosition,float3 Color,float Opacity)
{
	return float4(previousLighting(ScreenPosition) + Color * Opacity,ScreenPosition.w);
}
float4 additiveBlendAdd(float4 ScreenPosition,float3 Color,float Opacity)
{
	return float4(previousLighting(ScreenPosition) + Color * Opacity,ScreenPosition.w);
}

float4 translucentBlendBase(float4 ScreenPosition,float3 Color,float2 Distortion,float Opacity)
{
	return float4(
		lerp(previousLighting(ScreenPosition + float4(Distortion * float2(1,-1),0,0)),Color,Opacity),
		ScreenPosition.w
		);
}

float4 translucentBlendAdd(float4 ScreenPosition,float3 Color,float Opacity)
{
	return float4(
			previousLighting(ScreenPosition) + Color * Opacity,
			ScreenPosition.w
			);
}

float4 opaqueBlendBase(float4 ScreenPosition,float3 Color)
{
	return float4(Color,ScreenPosition.w);
}

#if USE_FP_BLENDING
	float4 opaqueBlendAdd(float4 ScreenPosition,float3 Color)
	{
		return float4(Color,0);
	}
#else
	float4 opaqueBlendAdd(float4 ScreenPosition,float3 Color)
	{
		return float4(previousLighting(ScreenPosition) + Color,ScreenPosition.w);
	}
#endif

#define FLOAT16_EPSILON	4.8875809e-4f

float getDepthCompareEpsilon(float Depth)
{
	float	E;
	frexp(Depth,E);
	return ldexp(4.0 * FLOAT16_EPSILON,E);
}

float      TwoSidedSign;	// +1 if rendering the front side of a face, -1 if rendering the back side.
