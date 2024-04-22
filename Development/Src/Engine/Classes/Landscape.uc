/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class Landscape extends LandscapeProxy
	dependson(LightComponent)
	native(Terrain)
	hidecategories(LandscapeProxy)
	showcategories(Display, Movement, Collision, Lighting, LOD);

enum ELandscapeSetupErrors
{
	LSE_None,
	LSE_NoLandscapeInfo, // No Landscape Info available
	LSE_CollsionXY, // There was already component with same X,Y
	LSE_NoLayerInfo // No Layer Info, need to add proper layers
};

/** Layers that can be painted on the landscape */
var deprecated array<Name> LayerNames;

/** Structure storing Layer Data */
struct native LandscapeLayerInfo
{
	var() Name LayerName;
	// Used to erosion caculation?
	var() float Hardness;
	var editoronly bool bNoWeightBlend;
	var() PhysicalMaterial PhysMaterial;
	var editoronly MaterialInstanceConstant ThumbnailMIC;
	var editoronly transient bool bSelected;
	var editoronly transient int DebugColorChannel;
	var editoronly transient string LayerSourceFile;

	structcpptext
	{
		// tor
		FLandscapeLayerInfo(FName InName, FLOAT InHardness=0.5f, UBOOL InNoWeightBlend=FALSE, const TCHAR* SourceFile=NULL)
		:	LayerName(InName)
		,	Hardness(InHardness)
		,	bNoWeightBlend(InNoWeightBlend)
		,	PhysMaterial(NULL)
#if WITH_EDITORONLY_DATA
		,	ThumbnailMIC(NULL)
		,	bSelected(FALSE)
		,	DebugColorChannel(0)
		,	LayerSourceFile(E_ForceInit)
#endif // WITH_EDITORONLY_DATA
		{
#if WITH_EDITORONLY_DATA
			LayerSourceFile = SourceFile;
#endif // WITH_EDITORONLY_DATA
		}

		// for TArray::FindItemIndexByKey
		UBOOL operator==( const FLandscapeLayerInfo& Other ) const
		{
			return LayerName == Other.LayerName;
		}
	}
};

var deprecated array<LandscapeLayerInfo> LayerInfos;

cpptext
{
	// Make a key for XYtoComponentMap
	static QWORD MakeKey( INT X, INT Y ) { return ((QWORD)(*(DWORD*)(&X)) << 32) | (*(DWORD*)(&Y) & 0xffffffff); }
	static void UnpackKey( QWORD Key, INT& OutX, INT& OutY ) { *(DWORD*)(&OutX) = (Key >> 32); *(DWORD*)(&OutY) = Key&0xffffffff; }

	virtual class ALandscape* GetLandscapeActor();
	virtual void ClearComponents();

	static FName DataWeightmapName; 
#if WITH_EDITOR
	virtual void PreSave();
	virtual void UpdateComponentsInternal(UBOOL bCollisionUpdate = FALSE);

	// ALandscape interface
	UBOOL ImportFromOldTerrain(class ATerrain* OldTerrain);
	void Import(INT VertsX, INT VertsY, INT ComponentSizeQuads, INT NumSubsections, INT SubsectionSizeQuads, WORD* HeightData, const TCHAR* HeightmapFileName, TArray<FLandscapeLayerInfo> ImportLayerInfos, BYTE* AlphaDataPointers[] );
	virtual void UpdateLandscapeActor(class ALandscape* Landscape, UBOOL bSearchForActor = TRUE);

	virtual UMaterialInterface* GetLandscapeMaterial() const;
	UBOOL HasAllComponent(); // determine all component is in this actor

	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent);
	virtual void PostEditMove(UBOOL bFinished);

	// Include Components with overlapped vertices
	static void CalcComponentIndicesOverlap(const INT X1, const INT Y1, const INT X2, const INT Y2, const INT ComponentSizeQuads, 
		INT& ComponentIndexX1, INT& ComponentIndexY1, INT& ComponentIndexX2, INT& ComponentIndexY2);

	// Exclude Components with overlapped vertices
	static void CalcComponentIndices(const INT X1, const INT Y1, const INT X2, const INT Y2, const INT ComponentSizeQuads, 
		INT& ComponentIndexX1, INT& ComponentIndexY1, INT& ComponentIndexX2, INT& ComponentIndexY2);

	static void SplitHeightmap(ULandscapeComponent* Comp, UBOOL bMoveToCurrentLevel = FALSE);

	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
	virtual void CheckForErrors();

	void UpdateOldLayerInfo();
#endif
	virtual void Serialize(FArchive& Ar);
	virtual void BeginDestroy();
	virtual void PostLoad();
}

defaultproperties
{
	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.S_Terrain'
		SpriteCategoryName="Landscape"
	End Object

	DrawScale3D=(X=128.0,Y=128.0,Z=256.0)
	bEdShouldSnap=True
	bCollideActors=True
	bBlockActors=True
	bWorldGeometry=True
	bStatic=True
	bNoDelete=True
	bHidden=False
	bMovable=False
	StaticLightingResolution=1.0
	StreamingDistanceMultiplier=1.0
	bIsProxy=False
	bLockLocation=False
}
 