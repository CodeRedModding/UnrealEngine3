/**
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *	This will hold all of our enums and types and such that we need to
 *	use in multiple files where the enum can'y be mapped to a specific file.
 */
class UnrealEdTypes extends Object
	native
	abstract
	config(UnrealEd);

// Structures for 'optional' Lightmass parameters...

/** Base LightmassParameterValue class */
struct native LightmassParameterValue
{
	/** If TRUE, override the given parameter with the given settings */
	var() bool			bOverride;
};

/** Boolean parameter value */
struct native LightmassBooleanParameterValue extends LightmassParameterValue
{
	/** The boolean value to override the parent value with */
    var() bool		ParameterValue;
};

/** Scalar parameter value */
struct native LightmassScalarParameterValue extends LightmassParameterValue
{
	/** The scalar value to override the parent value with */
	var() float		ParameterValue;
};

/** Structure for 'parameterized' Lightmass settings */
struct native LightmassParameterizedMaterialSettings
{
	/** If TRUE, forces translucency to cast static shadows as if the material were masked. */
	var()	LightmassBooleanParameterValue		CastShadowAsMasked;
	/** Scales the emissive contribution of this material to static lighting. */
	var()	LightmassScalarParameterValue		EmissiveBoost;
	/** Scales the diffuse contribution of this material to static lighting. */
	var()	LightmassScalarParameterValue		DiffuseBoost;
	/** Scales the specular contribution of this material to static lighting. */
	var		LightmassScalarParameterValue		SpecularBoost;
	/** 
	 * Scales the resolution that this material's attributes were exported at. 
	 * This is useful for increasing material resolution when details are needed.
	 */
	var()	LightmassScalarParameterValue		ExportResolutionScale;
	/** Scales the penumbra size of distance field shadows.  This is useful to get softer precomputed shadows on certain material types like foliage. */
	var()	LightmassScalarParameterValue		DistanceFieldPenumbraScale;
	
	structdefaultproperties
	{
		EmissiveBoost=(ParameterValue=1.0)
		DiffuseBoost=(ParameterValue=1.0)
		SpecularBoost=(ParameterValue=1.0)
		ExportResolutionScale=(ParameterValue=1.0)
		DistanceFieldPenumbraScale=(ParameterValue=1.0)
	}
};

// Must put this here because both MaterialEditorInstanceConstant and MaterialEditorInstanceTimeVarying need it
// Declaring it in their respective script files causes compile errors because the script would be defined twice in UnrealEdClasses.h

struct native PhysicalMaterialMaskSettings 
{
	/** A 1 bit monochrome texture that represents a mask for what physical material should be used if the collided texel is black or white. */
	var()	Texture2D	PhysMaterialMask;				
	/** The UV channel to use for the PhysMaterialMask. */
	var()	INT	PhysMaterialMaskUVChannel;
	/** The physical material to use when a black pixel in the PhysMaterialMask texture is hit. */
	var()	PhysicalMaterial BlackPhysicalMaterial;
	/** The physical material to use when a white pixel in the PhysMaterialMask texture is hit. */
	var()	PhysicalMaterial WhitePhysicalMaterial;
};
