//! @file SubstanceAirTexture2D.uc
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.
//!
//! @brief class for textures generated from a Substance Air output

class SubstanceAirTexture2D extends Texture2D
	native(Texture)
	hidecategories(Object)
	config(Engine);

// The Output instance to get the Substance data from,
// copy so the original can be updated
var native pointer OutputCopy{struct SubstanceAir::FOutputInstance};

var native guid OutputGuid;

var native SubstanceAirGraphInstance ParentInstance;

cpptext
{
public:
	virtual void Serialize(FArchive& Ar);
	virtual void BeginDestroy();
	virtual void PostLoad();
	virtual void PostDuplicate();

	virtual UBOOL CanEditChange(const UProperty* InProperty) const;

	// UTexture interface.
	virtual FTextureResource* CreateResource();
	UBOOL HasSourceArt() const;
	FString GetDesc();
	void StripData(UE3::EPlatformType TargetPlatform, UBOOL bStripLargeEditorData);

	// Init function which can be called from outside the main thread
	void LighterInit(UINT InSizeX,UINT InSizeY,EPixelFormat InFormat);   
}
