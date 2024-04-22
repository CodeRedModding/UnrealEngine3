/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
 
class SpeedTree extends Object
	native(SpeedTree);
	
/** If TRUE, this tree was imported with the Speedtree 4 integration. */
var private{private} const bool bLegacySpeedTree;

/** Helper object to allow SpeedTree to be script exposed.										*/
var duplicatetransient native const pointer	SRH{class FSpeedTreeResourceHelper};

// Editor-accesible variables

/** The probability of a shadow ray being blocked by the leaf material. */
var(Lighting) float LeafStaticShadowOpacity;

/** Branch 1 material.																			*/
var(Material) MaterialInterface		Branch1Material;
/** Branch 2 material.																			*/
var(Material) MaterialInterface		Branch2Material;
/** Frond material.																				*/
var(Material) MaterialInterface		FrondMaterial;
/** Leaf card material.																			*/
var(Material) MaterialInterface		LeafCardMaterial;
/** Leaf mesh material.																			*/
var(Material) MaterialInterface		LeafMeshMaterial;
/** Billboard material.																			*/
var(Material) MaterialInterface		BillboardMaterial;

// Wind variables
var(Wind) float						WindStrength;
var(Wind) vector					WindDirection;

/** Unique ID for this speed tree, used for caching during distributed lighting */
var private const editoronly Guid LightingGuid;

cpptext
{
	void StaticConstructor(void);

	virtual void BeginDestroy();
	virtual UBOOL IsReadyForFinishDestroy();
	virtual void FinishDestroy();
	
	virtual void PreEditChange(UProperty* PropertyAboutToChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();

	virtual	INT	GetResourceSize(void);
	virtual	FString	GetDetailedDescription( INT InIndex );
	virtual FString	GetDesc(void);
	
	UBOOL IsInitialized() const;
	UBOOL IsLegacySpeedTree() const { return bLegacySpeedTree; }

	virtual const FGuid& GetLightingGuid() const
	{
#if WITH_EDITORONLY_DATA
		return LightingGuid;
#else
		static const FGuid NullGuid( 0, 0, 0, 0 );
		return NullGuid; 
#endif // WITH_EDITORONLY_DATA
	}

	virtual void SetLightingGuid()
	{
#if WITH_EDITORONLY_DATA
		LightingGuid = appCreateGuid();
#endif // WITH_EDITORONLY_DATA
	}
}

defaultproperties
{	
	LeafStaticShadowOpacity=0.5
	
	WindStrength=0.2
	WindDirection=(X=1,Y=0,Z=0)
}

