/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "GameFramework.h"

IMPLEMENT_CLASS(USecondaryViewportClient);
IMPLEMENT_CLASS(UMobileSecondaryViewportClient);

extern UBOOL GTickAndRenderUI;

void USecondaryViewportClient::Draw(FViewport* Viewport,FCanvas* Canvas)
{
	// clear the screen
	ENQUEUE_UNIQUE_RENDER_COMMAND(
		SecondaryViewportClientClear,
	{
		RHIClear(TRUE, FLinearColor::Black, FALSE, 0.0f, FALSE, 0);
	});

	UCanvas* CanvasObject = InitCanvas(Viewport, Canvas);

	DrawSecondaryHUD(CanvasObject);

	// let script code render something
	eventPostRender(CanvasObject);
}

UCanvas* USecondaryViewportClient::InitCanvas(FViewport* Viewport, FCanvas* Canvas)
{
	UCanvas* CanvasObject = FindObject<UCanvas>(UObject::GetTransientPackage(),TEXT("CanvasObject"));
	if( !CanvasObject )
	{
		CanvasObject = ConstructObject<UCanvas>(UCanvas::StaticClass(),UObject::GetTransientPackage(),TEXT("CanvasObject"));
		CanvasObject->AddToRoot();
	}
	CanvasObject->Canvas = Canvas;	

	// Reset the canvas for rendering to the full viewport.
	CanvasObject->Init();
	CanvasObject->SizeX = Viewport->GetSizeX();
	CanvasObject->SizeY = Viewport->GetSizeY();
	CanvasObject->SceneView = NULL;
	CanvasObject->Update();		

	//ensure canvas has been flushed before rendering UI
	Canvas->Flush();

	return CanvasObject;
}

void USecondaryViewportClient::DrawSecondaryHUD(UCanvas* CanvasObject)
{
	//Render the UI if enabled.
	if( GTickAndRenderUI )
	{
		// render HUD
		for(INT PlayerIndex = 0;PlayerIndex < GEngine->GamePlayers.Num();PlayerIndex++)
		{
			ULocalPlayer* Player = GEngine->GamePlayers(PlayerIndex);
			if(Player->Actor)
			{
				// Render the player's HUD.
				if( Player->Actor->mySecondaryHUD )
				{
					Player->Actor->mySecondaryHUD->Canvas = CanvasObject;
					Player->Actor->mySecondaryHUD->eventPostRender();
					// A side effect of PostRender is that the playercontroller could be destroyed
					if (!Player->Actor->IsPendingKill())
					{
						Player->Actor->mySecondaryHUD->Canvas = NULL;
					}
					break;
				}
			}
		}
	}
}
