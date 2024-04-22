/*=============================================================================
	MaterialInstanceConstant.h: MaterialInstanceConstant definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MATERIALINSTANCECONSTANT_H__
#define __MATERIALINSTANCECONSTANT_H__

#include "MaterialInstance.h"

class FMaterialInstanceConstantResource : public FMaterialInstanceResource
{
public:

	typedef UMaterialInstanceConstant InstanceType;

	friend class MICVectorParameterMapping;
	friend class MICScalarParameterMapping;
	friend class MICTextureParameterMapping;
	friend class MICFontParameterMapping;

	/** Initialization constructor. */
	FMaterialInstanceConstantResource(UMaterialInstance* InOwner,UBOOL bInSelected,UBOOL bInHovered)
	:	FMaterialInstanceResource( InOwner, bInSelected, bInHovered )
	{
	}

	// FMaterialRenderProxy interface.
	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const;
	virtual UBOOL GetScalarValue(const FName ParameterName,FLOAT* OutValue, const FMaterialRenderContext& Context) const;
	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const;

private:

	TArray<TNamedParameter<FLinearColor> > VectorParameterArray;
	TArray<TNamedParameter<FLOAT> > ScalarParameterArray;
	TArray<TNamedParameter<const UTexture*> > TextureParameterArray;
};

/** A mapping for UMaterialInstanceConstant's scalar parameters. */
DEFINE_MATERIALINSTANCE_PARAMETERTYPE_MAPPING(
	MICScalarParameterMapping,
	FMaterialInstanceConstantResource,
	FLOAT,
	FScalarParameterValue,
	ScalarParameterValues,
	ScalarParameterArray,
	{ Value = Parameter.ParameterValue; }
);

/** A mapping for UMaterialInstanceConstant's vector parameters. */
DEFINE_MATERIALINSTANCE_PARAMETERTYPE_MAPPING(
	MICVectorParameterMapping,
	FMaterialInstanceConstantResource,
	FLinearColor,
	FVectorParameterValue,
	VectorParameterValues,
	VectorParameterArray,
	{ Value = Parameter.ParameterValue; }
);

/** A mapping from UMaterialInstanceConstant's texture parameters. */
DEFINE_MATERIALINSTANCE_PARAMETERTYPE_MAPPING(
	MICTextureParameterMapping,
	FMaterialInstanceConstantResource,
	const UTexture*,
	FTextureParameterValue,
	TextureParameterValues,
	TextureParameterArray,
	{ Value = Parameter.ParameterValue; }
);

/** A mapping from UMaterialInstanceConstant's font parameters. */
DEFINE_MATERIALINSTANCE_PARAMETERTYPE_MAPPING(
	MICFontParameterMapping,
	FMaterialInstanceConstantResource,
	const UTexture*,
	FFontParameterValue,
	FontParameterValues,
	TextureParameterArray,
	{
		Value = NULL;
		// add font texture to the texture parameter map
		if( Parameter.FontValue && Parameter.FontValue->Textures.IsValidIndex(Parameter.FontPage) )
		{
			// get the texture for the font page
			Value = Parameter.FontValue->Textures(Parameter.FontPage);
		}
	}
);

#endif
