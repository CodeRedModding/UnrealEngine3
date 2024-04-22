/*=============================================================================
   DebugCameraController.cpp: Native implementation for the debug camera

   Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "GameFramework.h"
#include "GameFrameworkClasses.h"

IMPLEMENT_CLASS(ADebugCameraController);

/** The currently selected actor. */
AActor* GDebugSelectedActor = NULL;
/** The currently selected component in the actor. */
UPrimitiveComponent* GDebugSelectedComponent = NULL;
/** The lightmap used by the currently selected component, if it's a static mesh component. */
FLightMap2D* GDebugSelectedLightmap = NULL;

extern UBOOL UntrackTexture( const FString& TextureName );
extern UBOOL TrackTexture( const FString& TextureName );

/**
 * Called when an actor has been selected with the primary key (e.g. left mouse button).
 *
 * @param HitLoc	World-space position of the selection point.
 * @param HitNormal	World-space normal of the selection point.
 * @param HitInfo	Info struct for the selection point.
 */
void ADebugCameraController::PrimarySelect( FVector HitLoc, FVector HitNormal, FTraceHitInfo HitInfo )
{
	// First untrack the currently tracked lightmap.
	UTexture2D* Texture2D = GDebugSelectedLightmap ? GDebugSelectedLightmap->GetTexture(0) : NULL;
	if ( Texture2D )
	{
		UntrackTexture( Texture2D->GetName() );
	}

	GDebugSelectedActor = SelectedActor;
	GDebugSelectedComponent = SelectedComponent;
	GDebugSelectedLightmap = NULL;
	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>( GDebugSelectedComponent );
	if ( StaticMeshComponent && StaticMeshComponent->LODData.Num() > 0 )
	{
		const FStaticMeshComponentLODInfo& LODInfo = StaticMeshComponent->LODData(0);
		if ( LODInfo.LightMap )
		{
			GDebugSelectedLightmap = LODInfo.LightMap->GetLightMap2D();
			UTexture2D* Texture2D = GDebugSelectedLightmap ? GDebugSelectedLightmap->GetTexture(0) : NULL;
			if ( Texture2D )
			{
				extern UBOOL TrackTexture( const FString& TextureName );
				TrackTexture( Texture2D->GetName() );
			}
		}
	}
}

/**
 * Called when an actor has been selected with the secondary key (e.g. right mouse button).
 *
 * @param HitLoc	World-space position of the selection point.
 * @param HitNormal	World-space normal of the selection point.
 * @param HitInfo	Info struct for the selection point.
 */
void ADebugCameraController::SecondarySelect( FVector HitLoc, FVector HitNormal, FTraceHitInfo HitInfo )
{
	PrimarySelect( HitLoc, HitNormal, HitInfo );
}

/**
 * Called when the user pressed the unselect key, just before the selected actor is cleared.
 */
void ADebugCameraController::Unselect()
{
	UTexture2D* Texture2D = GDebugSelectedLightmap ? GDebugSelectedLightmap->GetTexture(0) : NULL;
	if ( Texture2D )
	{
		extern UBOOL UntrackTexture( const FString& TextureName );
		UntrackTexture( Texture2D->GetName() );
	}

	GDebugSelectedActor = NULL;
	GDebugSelectedComponent = NULL;
	GDebugSelectedLightmap = NULL;
}



/**
 * This is the same as PlayerController::ConsoleCommand(), except with some extra code to 
 * give our regular PC a crack at handling the command.
 */
FString ADebugCameraController::ConsoleCommand(const FString& Cmd,UBOOL bWriteToLog)
{
	if (Player != NULL)
	{
		UConsole* ViewportConsole = (GEngine->GameViewport != NULL) ? GEngine->GameViewport->ViewportConsole : NULL;
		FConsoleOutputDevice StrOut(ViewportConsole);
	
		const INT CmdLen = Cmd.Len();
		TCHAR* CommandBuffer = (TCHAR*)appMalloc((CmdLen+1)*sizeof(TCHAR));
		TCHAR* Line = (TCHAR*)appMalloc((CmdLen+1)*sizeof(TCHAR));

		const TCHAR* Command = CommandBuffer;
		// copy the command into a modifiable buffer
		appStrcpy(CommandBuffer, (CmdLen+1), *Cmd.Left(CmdLen)); 

		// iterate over the line, breaking up on |'s
		while (ParseLine(&Command, Line, CmdLen+1))	// The ParseLine function expects the full array size, including the NULL character.
		{
			if (Player->Exec(Line, StrOut) == FALSE)
			{
				Player->Actor = OriginalControllerRef;
				Player->Exec(Line, StrOut);
				Player->Actor = this;
			}
		}

		// Free temp arrays
		appFree(CommandBuffer);
		CommandBuffer=NULL;

		appFree(Line);
		Line=NULL;

		if (!bWriteToLog)
		{
			return *StrOut;
		}
	}

	return TEXT("");
}

/**
 * Builds a list of components that are hidden based upon gameplay
 *
 * @param ViewLocation the view point to hide/unhide from
 * @param HiddenComponents the list to add to/remove from
 */
void ADebugCameraController::UpdateHiddenComponents(const FVector& ViewLocation,TSet<UPrimitiveComponent*>& HiddenComponents)
{
	if (OriginalControllerRef != NULL)
	{
		OriginalControllerRef->UpdateHiddenComponents(ViewLocation,HiddenComponents);
	}
}

