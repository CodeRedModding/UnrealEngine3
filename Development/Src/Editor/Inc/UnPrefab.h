/*=============================================================================
	UnPrefab.h: Unreal prefab related classes.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

// A prefab.
//
class UPrefab : public UObject
{
	DECLARE_CLASS(UPrefab,UObject,CLASS_SafeReplace,Editor)

	// Variables.
	FName		FileType;
	FString T3DText;

	// Constructor.
	UPrefab()
	{
	}

	// UObject interface.
	void Serialize( FArchive& Ar );
	void Destroy();
	void PostLoad();

	virtual FString GetDesc();
	virtual void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	virtual FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	virtual INT GetThumbnailLabels( TArray<FString>* InLabels );
};

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
