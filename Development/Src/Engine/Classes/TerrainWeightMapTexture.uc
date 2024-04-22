/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class TerrainWeightMapTexture extends Texture2D
	native(Terrain)
	hidecategories(Object);

// Structs that are mirrored properly in C++.
struct TerrainWeightedMaterial
{
	// UObject references.
};

var const Terrain						ParentTerrain;
var private native const array<pointer>	WeightedMaterials{struct FTerrainWeightedMaterial};


cpptext
{
    // UObject interface
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();

	/** 
	 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
	 */
	virtual FString GetDesc();

	/** 
	 * Returns detailed info to populate listview columns
	 */
	virtual FString GetDetailedDescription( INT InIndex );

	void Initialize(ATerrain* InTerrain);
	void UpdateData();
}

defaultproperties
{
}
