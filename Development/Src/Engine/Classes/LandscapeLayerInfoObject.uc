/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 */
class LandscapeLayerInfoObject extends Object
	native(Terrain);

var() Name LayerName;
var() PhysicalMaterial PhysMaterial;
var() float Hardness;
var editoronly bool bNoWeightBlend;

cpptext
{
	UBOOL GetSharedProperties(FLandscapeLayerInfo* Info);
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostLoad();
#endif
}

defaultproperties
{
	Hardness=0.5
	bNoWeightBlend=false
}