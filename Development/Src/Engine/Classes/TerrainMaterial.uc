/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class TerrainMaterial extends Object
	native(Terrain)
	hidecategories(Object);

var matrix					LocalToMapping;

enum ETerrainMappingType
{
	TMT_Auto,
	TMT_XY,
	TMT_XZ,
	TMT_YZ
};

/** Determines the mapping place to use on the terrain. */
var(Material) ETerrainMappingType	MappingType;
/** Uniform scale to apply to the mapping. */
var(Material) float					MappingScale;
/** Rotation to apply to the mapping. */
var(Material) float					MappingRotation;
/** Offset to apply to the mapping along U. */
var(Material) float					MappingPanU;
/** Offset to apply to the mapping along V. */
var(Material) float					MappingPanV;

/** The Material to apply to the terrain. */
var(Material) MaterialInterface		Material;

/** Grayscale image to move vertices of the terrain along the surface normal. */
var(Displacement) Texture2D			DisplacementMap;
/** The amount to sacle the displacement texture by. */
var(Displacement) float				DisplacementScale;

cpptext
{
	// UpdateMappingTransform

	void UpdateMappingTransform();

	// UObject interface.
	virtual void PreEditChange(UProperty* PropertyAboutToChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostLoad();
}

defaultproperties
{
	MappingScale=4.0
	DisplacementScale=0.25
}

