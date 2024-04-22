class LandscapeMaterialInstanceConstant extends MaterialInstanceConstant
	native(Material);

var bool bIsLayerThumbnail;
var int DataWeightmapIndex;
var int DataWeightmapSize;

cpptext
{
	/**
	* Custom version of AllocateResource to minimize the shaders we need to generate 
	* @return	The allocated resource
	*/
	FMaterialResource* AllocateResource();

	static FString LandscapeVisibilitySwitchName;
}

defaultproperties
{
	bIsLayerThumbnail=False
	DataWeightmapIndex=-1
	DataWeightmapSize=0
}