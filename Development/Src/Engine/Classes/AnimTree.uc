class AnimTree extends AnimNodeBlendBase
	native(Anim)
	hidecategories(Object);

cpptext
{
	// UObject interface
	void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	INT GetThumbnailLabels( TArray<FString>* InLabels );
}

defaultproperties
{
	Children(0)=(Name="Child",Weight=1.0)
	bFixNumChildren=true
}