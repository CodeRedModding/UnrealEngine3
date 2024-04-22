/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class LandscapeInfo extends Object
	native(Terrain);

/** Landscape datas */
var const Guid LandscapeGuid;

var const native map{FName, struct FLandscapeLayerStruct*} LayerInfoMap;

var const native pointer DataInterface{struct FLandscapeDataInterface};

/** Map of the SectionBaseX/Y component offets (in heightmap space) to the component. Valid in editor only. */
var const native map{QWORD,class ULandscapeComponent*} XYtoComponentMap;

/** Map of the SectionBaseX/Y component offets (in heightmap space) to the collison components. Valid in editor only. */
var const native map{QWORD,class ULandscapeHeightfieldCollisionComponent*} XYtoCollisionComponentMap;

var const LandscapeProxy LandscapeProxy;

/** Structure storing Collision for LandscapeComponent Add */
struct native LandscapeAddCollision
{
	var editoronly vector Corners[4];
	structcpptext
	{
		FLandscapeAddCollision()
		{
#if WITH_EDITORONLY_DATA
			Corners[0] = Corners[1] = Corners[2] = Corners[3] = FVector(0.f, 0.f, 0.f);
#endif // WITH_EDITORONLY_DATA
		}
	}
};

/** Map of the SectionBaseX/Y component offets to the newly added collison components. Only available near valid LandscapeComponents. Valid in editor only. */
var const native map{QWORD,struct FLandscapeAddCollision} XYtoAddCollisionMap;

var const private native transient Set_Mirror Proxies{TSet<class ALandscapeProxy*>};

var const private native Set_Mirror SelectedComponents{TSet<class ULandscapeComponent*>};
var const private native Set_Mirror SelectedCollisionComponents{TSet<class ULandscapeHeightfieldCollisionComponent*>};
var const private native Set_Mirror SelectedRegionComponents{TSet<class ULandscapeComponent*>};

var const private native map{QWORD,FLOAT} SelectedRegion;

var editoronly string HeightmapFilePath;

var editoronly transient bool bIsValid;

cpptext
{
	void GetSharedProperties(ALandscapeProxy* Landscape);
#if WITH_EDITOR
	virtual void PreSave();
	virtual void PostEditUndo();
	
	struct FLandscapeDataInterface* GetDataInterface();

	void GetComponentsInRegion(INT X1, INT Y1, INT X2, INT Y2, TSet<ULandscapeComponent*>& OutComponents);
	UBOOL GetLandscapeExtent(INT& MinX, INT& MinY, INT& MaxX, INT& MaxY);
	void Export(TArray<FName>& Layernames, TArray<FString>& Filenames);
	UBOOL ReimportHeightmap(INT DataSize, const WORD* Data);
	UBOOL ReimportLayermap(FName LayerName, TArray<BYTE>& Data);
	ALandscape* ChangeComponentSetting(INT VertsX, INT VertsY, INT InNumSubsections, INT InSubsectionSizeQuads);

	UBOOL GetSelectedExtent(INT& MinX, INT& MinY, INT& MaxX, INT& MaxY);
	FVector GetLandscapeCenterPos(FLOAT& LengthZ, INT MinX = MAXINT, INT MinY = MAXINT, INT MaxX = MININT, INT MaxY = MININT);
	UBOOL IsValidPosition(INT X, INT Y);
	void DeleteLayer(FName LayerName);

	void UpdateDebugColorMaterial();

	// Used by all selection tool...
	void UpdateSelectedComponents(TSet<class ULandscapeComponent*>& NewComponents, UBOOL bIsComponentwise = TRUE);
	// Sort selected components based on location
	void SortSelectedComponents();
	void ClearSelectedRegion(UBOOL bIsComponentwise = TRUE);

	// Update Collision object for add LandscapeComponent tool
	void UpdateAllAddCollisions();
	void UpdateAddCollision(QWORD LandscapeKey, UBOOL bForceUpdate = FALSE);

	// Update LayerInfoMap
	UBOOL UpdateLayerInfoMap(ALandscapeProxy* Proxy = NULL, UBOOL bInvalidate = FALSE);

	void UpdateLODBias(FLOAT Threshold);

	void CheckValidate();
#endif

	// UObject interface
	virtual void Serialize(FArchive& Ar);
	virtual void BeginDestroy();
	virtual void PostLoad();
}

defaultproperties
{
	bIsValid = FALSE
}
 