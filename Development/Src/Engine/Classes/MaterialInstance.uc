class MaterialInstance extends Object
	abstract
	native;

struct MaterialInstancePointer
{
	var const native pointer P;
};

cpptext
{
	// GetMaterial - Get the material which this is an instance of.

	virtual class UMaterial* GetMaterial() { check(0); return 0; }

	// GetLayerMask - Returns the layers this material needs to be rendered on.

	virtual DWORD GetLayerMask() { check(0); return 0; }

	// GetMaterialInterface - Returns a pointer to the FMaterial used for rendering.

	virtual FMaterial* GetMaterialInterface(UBOOL Selected) { check(0); return NULL; }

	// GetInstanceInterface - Returns a pointer to the FMaterialInstance used for rendering.

	virtual FMaterialInstance* GetInstanceInterface() { check(0); return NULL; }

	// UObject interface.

	virtual void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, INT InPitch, INT InYaw, FChildViewport* InViewport, FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	virtual void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz ) { DrawThumbnail(InPrimType,InX,InY,0,0,InViewport,InRI,InZoom,InShowBackground,InZoomPct,InFixedSz); }
	virtual FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	virtual INT GetThumbnailLabels( TArray<FString>* InLabels );
	INT GetWidth() const;
	INT GetHeight() const;
}

native function Material GetMaterial();