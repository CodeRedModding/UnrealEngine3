/*=============================================================================
	MaterialInstanceTimeVarying.h: MaterialInstanceTimeVarying definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MATERIALINSTANCETIMEVARYING_H__
#define __MATERIALINSTANCETIMEVARYING_H__

#include "MaterialInstance.h"

struct FTimeVaryingDataTypeBase
{
	UBOOL bLoop;
	FLOAT CycleTime;
	UBOOL bNormalizeTime;

	FLOAT OffsetTime;
	UBOOL bOffsetFromEnd;

	FTimeVaryingDataTypeBase(): bLoop(FALSE), CycleTime(-1.0f), bNormalizeTime(FALSE), OffsetTime(0.0f), bOffsetFromEnd(FALSE)
	{
	}
};


/** Struct to manage the Render data needed for TimeVarying scalar Data **/
struct FTimeVaryingScalarDataType : public FTimeVaryingDataTypeBase
{
	FLOAT StartTime; 
	FLOAT Value;
	FInterpCurveFloat TheCurve;

	FTimeVaryingScalarDataType(): StartTime(-1.0f), Value(0.0f)
	{
		// we need to appMemzero this as we are using a TArrayNoInit in the FInterpCurveFloat so we can use copy constructor nicely
		appMemzero( &this->TheCurve.Points, sizeof(this->TheCurve.Points) );
	}
};

/** Struct to manage the Render data needed for TimeVaring vector Data **/
struct FTimeVaryingVectorDataType : public FTimeVaryingDataTypeBase
{
	FLOAT StartTime; 
	FLinearColor Value;
	FInterpCurveVector TheCurve;

	FTimeVaryingVectorDataType(): StartTime(-1.0f), Value(0.0f,0.0f,0.0f)
	{
		// we need to appMemzero this as we are using a TArrayNoInit in the FInterpCurveFloat so we can use copy constructor nicely
		appMemzero( &this->TheCurve.Points, sizeof(this->TheCurve.Points) );
	}
};

/** Struct to manage the Render data needed for TimeVaring LinearColor Data **/
struct FTimeVaryingLinearColorDataType : public FTimeVaryingDataTypeBase
{
	FLOAT StartTime; 
	FLinearColor Value;
	FInterpCurveLinearColor TheCurve;

	FTimeVaryingLinearColorDataType(): StartTime(-1.0f), Value(0.0f,0.0f,0.0f)
	{
		// we need to appMemzero this as we are using a TArrayNoInit in the FInterpCurveFloat so we can use copy constructor nicely
		appMemzero( &this->TheCurve.Points, sizeof(this->TheCurve.Points) );
	}
};

class FMaterialInstanceTimeVaryingResource : public FMaterialInstanceResource
{
public:
	
	typedef UMaterialInstanceTimeVarying InstanceType;

	friend class MITVVectorParameterMapping;
	friend class MITVScalarParameterMapping;
	friend class MITVTextureParameterMapping;
	friend class MITVFontParameterMapping;
	friend class MITVLinearColorParameterMapping;

	/** Initialization constructor. */
	FMaterialInstanceTimeVaryingResource(UMaterialInstance* InOwner,UBOOL bInSelected,UBOOL bInHovered)
	:	FMaterialInstanceResource( InOwner, bInSelected, bInHovered )
	{
	}

	// FMaterialInstance interface.
 	virtual UBOOL GetScalarValue(const FName ParameterName,FLOAT* OutValue, const FMaterialRenderContext& Context) const;
	virtual UBOOL GetVectorValue(const FName ParameterName,FLinearColor* OutValue, const FMaterialRenderContext& Context) const;
	virtual UBOOL GetLinearColorValue(const FName ParameterName,FLinearColor* OutValue, const FMaterialRenderContext& Context) const;
	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue,const FMaterialRenderContext& Context) const;

private:

	TArray<TNamedParameter<const UTexture*> > TextureParameterArray;
	TArray<TNamedParameter<FTimeVaryingScalarDataType> > ScalarOverTimeParameterArray;
	TArray<TNamedParameter<FTimeVaryingVectorDataType> > VectorOverTimeParameterArray;
	TArray<TNamedParameter<FTimeVaryingLinearColorDataType> > LinearColorOverTimeParameterArray;
};

#endif
