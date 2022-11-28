/*=============================================================================
	PostProcess.hlsl: Scene post processing pixel shaders.
	Copyright 2003-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

#ifdef NUM_SAMPLES
sampler	BlurTexture;
half4	SampleWeights[NUM_SAMPLES];

struct blurInterpolants
{
	float4 OffsetUVs[(NUM_SAMPLES + 1) / 2] : TEXCOORD0;
};

half4 blur(blurInterpolants Interpolants) : COLOR0
{
	int SampleIndex;
	half4 Sum = 0;
	for(SampleIndex = 0;SampleIndex < NUM_SAMPLES - 1;SampleIndex += 2)
	{
		Sum += tex2D(BlurTexture,Interpolants.OffsetUVs[SampleIndex / 2].xy) * SampleWeights[SampleIndex + 0];
		Sum += tex2D(BlurTexture,Interpolants.OffsetUVs[SampleIndex / 2].wz) * SampleWeights[SampleIndex + 1];
	}
	if(SampleIndex < NUM_SAMPLES)
		Sum += tex2D(BlurTexture,Interpolants.OffsetUVs[SampleIndex / 2].xy) * SampleWeights[SampleIndex + 0];
	return Sum;
}

float4	SampleOffsets[(NUM_SAMPLES + 1) / 2];

void blurVertexShader(float4 InPosition : POSITION,float2 UV : TEXCOORD0,out float4 OutPosition : POSITION,out blurInterpolants Interpolants)
{
	for(int OffsetIndex = 0;OffsetIndex < (NUM_SAMPLES + 1) / 2;OffsetIndex++)
	{
		Interpolants.OffsetUVs[OffsetIndex] = UV.xyyx + SampleOffsets[OffsetIndex];
	}
	OutPosition = InPosition;
}
#endif

sampler	LinearImage;
float4	LinearImageResolution;
float4	LinearImageScaleBias;
sampler	PreviousLightingTexture;
float4	ScaleFactor;

#if SUPPORTS_FP_FILTERING
half4 bilinearFilterTexture(sampler Texture,float2 UV, float2 scale, float2 bias)
{
	return tex2D(Texture, UV * scale + bias);
}
#else
float4 bilinearFilterTexture(sampler Texture,float2 UV, float2 scale, float2 bias)
{
	UV = UV*scale + bias;
	float2	IntUV = floor(UV * LinearImageResolution.xy - 0.5);
	float2	FracUV = frac(UV * LinearImageResolution.xy - 0.5);

	return lerp(
			lerp(
				tex2D(Texture,(IntUV + float2(0.5,0.5)) * LinearImageResolution.wz),
				tex2D(Texture,(IntUV + float2(1.5,0.5)) * LinearImageResolution.wz),
				FracUV.x
				),
			lerp(
				tex2D(Texture,(IntUV + float2(0.5,1.5)) * LinearImageResolution.wz),
				tex2D(Texture,(IntUV + float2(1.5,1.5)) * LinearImageResolution.wz),
				FracUV.x
				),
			FracUV.y
			);
}
#endif

half4 accumulateImage(float2 UV : TEXCOORD0) : COLOR0
{
	return tex2D(PreviousLightingTexture,UV) + bilinearFilterTexture(LinearImage,UV,LinearImageScaleBias.xy,LinearImageScaleBias.wz) * ScaleFactor;
}

half4 blendImage(float2 UV : TEXCOORD0) : COLOR0
{
	return float4(
			tex2D(PreviousLightingTexture,UV).rgb * tex2D(LinearImage,UV).a +
			tex2D(LinearImage,UV).rgb,
			tex2D(PreviousLightingTexture,UV).a
			);
}

sampler	 ExposureTexture;
float    MaxDeltaExposure;
float    ManualExposure;

float square(float X)
{
	return X * X;
}

half4 calcExposure(float2 UV : TEXCOORD0) : COLOR0
{
	half	Luminosity = dot(tex2D(LinearImage,0).rgb,float3(0.3,0.59,0.11)),
			TargetExposure = sqrt(0.25 / clamp(Luminosity,0.1,50.0)),
			LastExposure = sqrt(tex2D(ExposureTexture,0).r);

	return square(LastExposure + clamp(TargetExposure - LastExposure,-MaxDeltaExposure,MaxDeltaExposure)) * ManualExposure;
}

float4     OverlayColor;

half4 finishImage(float2 UV : TEXCOORD0) : COLOR0
{
	half4	Color = max(tex2D(LinearImage,UV),0),
			Exposure = tex2D(ExposureTexture,0).r;
#ifndef XBOX
	return pow(lerp(Color * ScaleFactor * Exposure,OverlayColor,OverlayColor.a),1.0 / 2.2);
#else
	return pow(lerp(Color,OverlayColor,OverlayColor.a), 1.0 / 2.2);
#endif
}

sampler	CopyTexture;
float4	FillValue,
		FillAlpha;

half4 copy(float2 UV : TEXCOORD0) : COLOR0
{
	return tex2D(CopyTexture,UV);
}

half4 copyFill(float2 UV : TEXCOORD0) : COLOR0
{
	return FillAlpha > 0.5 ? FillValue : tex2D(CopyTexture,UV);
}

float4 fill() : COLOR0
{
	return FillValue;
}

struct vertexShaderOutput
{
	float4	Position : POSITION;
	float3	TexCoord : TEXCOORD0;
};

vertexShaderOutput vertexShader(float4 Position : POSITION,float3 TexCoord : TEXCOORD0)
{
	vertexShaderOutput Result;
	Result.Position = Position;
	Result.TexCoord = TexCoord;
	return Result;
}