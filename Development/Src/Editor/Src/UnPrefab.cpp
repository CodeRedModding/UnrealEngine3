
#include "EditorPrivate.h" 

/*-----------------------------------------------------------------------------
	UPrefab implementation.
-----------------------------------------------------------------------------*/

void UPrefab::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << FileType;
	if( Ar.IsLoading() || Ar.IsSaving() )
	{
		Ar << T3DText;
	}
}

void UPrefab::Destroy()
{
	Super::Destroy();
}

void UPrefab::PostLoad()
{
	Super::PostLoad();
};

FString UPrefab::GetDesc()
{
	return TEXT("");
}

void UPrefab::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	UTexture2D* Icon = CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.UnrealEdIcon_Prefab") ));

	InRI->DrawTile
	(
		InX,
		InY,
		InFixedSz ? InFixedSz : Icon->SizeX*InZoom,
		InFixedSz ? InFixedSz : Icon->SizeY*InZoom,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		FLinearColor::White,
		Icon
	);
}

FThumbnailDesc UPrefab::GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
{
	UTexture2D* Icon = CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.UnrealEdIcon_Prefab") ));
	FThumbnailDesc td;

	td.Width = InFixedSz ? InFixedSz : Icon->SizeX*InZoom;
	td.Height = InFixedSz ? InFixedSz : Icon->SizeX*InZoom;
	return td;
}

INT UPrefab::GetThumbnailLabels( TArray<FString>* InLabels )
{
	InLabels->Empty();

	new( *InLabels )FString( GetName() );

	return InLabels->Num();
}


IMPLEMENT_CLASS(UPrefab);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
