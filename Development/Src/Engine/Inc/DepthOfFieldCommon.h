/*=============================================================================
	DepthOfFieldCommon.h: Definitions for rendering Depth of Field.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DEPTHOFFIELDCOMMON_H__
#define __DEPTHOFFIELDCOMMON_H__

/** DepthOfField parameters */
struct FDepthOfFieldParams
{
	// default settings should be no Depth Of Field
	FDepthOfFieldParams() :
		FocusDistance(0),
		FocusInnerRadius(1000),
		FalloffExponent(1),
		MaxNearBlurAmount(0),
		MinBlurAmount(0),
		MaxFarBlurAmount(0)
	{
	}

	FLOAT FocusDistance;
	FLOAT FocusInnerRadius;
	FLOAT FalloffExponent;
	FLOAT MaxNearBlurAmount;
	FLOAT MinBlurAmount;
	FLOAT MaxFarBlurAmount;
};


/** Encapsulates the DOF parameters which are used by multiple shader types (used in base pass rendering so try to keep the amount of values in here minimal). */
class FDOFShaderParameters
{
public:

	/** Default constructor. */
	FDOFShaderParameters() {}

	/** Initialization constructor. */
	FDOFShaderParameters(const FShaderParameterMap& ParameterMap);

	void Bind(const FShaderParameterMap& ParameterMap);

	/** Set the dof pixel shader parameter values. */
	void SetPS(FShader* PixelShader, const FDepthOfFieldParams& DepthOfFieldParams) const;

	/** Set the dof vertex shader parameter values from SceneView */
	void SetVS(FShader* VertexShader, const FDepthOfFieldParams& DepthOfFieldParams) const;

#if WITH_D3D11_TESSELLATION
	/** Set the dof geometry shader parameter values. */
	void SetGS(FShader* GeometryShader, const FDepthOfFieldParams& DepthOfFieldParams) const;

	/** Set the dof domain shader parameter values from SceneView */
	void SetDS(FShader* DomainShader, const FDepthOfFieldParams& DepthOfFieldParams) const;
#endif

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FDOFShaderParameters& P);

private:
	FShaderParameter PackedParameters0;
	FShaderParameter PackedParameters1;

	static void ComputeShaderConstants(const FDepthOfFieldParams& Params, FVector4 Out[2]);
};

#endif