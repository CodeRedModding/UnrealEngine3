/*=============================================================================
	UnPlayer.cpp: Unreal player implementation.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"
#include "UnStatChart.h"

IMPLEMENT_CLASS(UPlayer);
IMPLEMENT_CLASS(ULocalPlayer);

//
//	ULocalPlayer::ULocalPlayer
//

ULocalPlayer::ULocalPlayer():
	ViewMode(SVM_Lit),
	ShowFlags(SHOW_DefaultGame),
	UseAutomaticBrightness(0)
{
	ViewState = new FSceneViewState();
}

//
//	ULocalPlayer::Destroy
//

void ULocalPlayer::Destroy()
{
	Super::Destroy();

	if(Viewport)
		Viewport->ViewportClient = NULL;

	delete ViewState;
	ViewState = NULL;
}

//
//	ULocalPlayer::InputKey
//

void ULocalPlayer::InputKey(FChildViewport* Viewport,FName Key,EInputEvent Event,FLOAT AmountDepressed)
{
	for(UINT InteractionIndex = 0;InteractionIndex < (UINT)Actor->Interactions.Num();InteractionIndex++)
		if(Actor->Interactions(InteractionIndex) && Actor->Interactions(InteractionIndex)->InputKey(Key,Event,AmountDepressed))
			break;
}

//
//	ULocalPlayer::InputAxis
//

void ULocalPlayer::InputAxis(FChildViewport* Viewport,FName Key,FLOAT Delta,FLOAT DeltaTime)
{
	for(UINT InteractionIndex = 0;InteractionIndex < (UINT)Actor->Interactions.Num();InteractionIndex++)
		if(Actor->Interactions(InteractionIndex) && Actor->Interactions(InteractionIndex)->InputAxis(Key,Delta,DeltaTime))
			break;
}

//
//	ULocalPlayer::InputChar
//

void ULocalPlayer::InputChar(FChildViewport* Viewport,TCHAR Character)
{
	for(UINT InteractionIndex = 0;InteractionIndex < (UINT)Actor->Interactions.Num();InteractionIndex++)
		if(Actor->Interactions(InteractionIndex) && Actor->Interactions(InteractionIndex)->InputChar(Character))
			break;
}

/**
 * Returns the forcefeedback manager associated with the PlayerController.
 */
class UForceFeedbackManager* ULocalPlayer::GetForceFeedbackManager(void)
{
	return Actor ? Actor->ForceFeedbackManager : NULL;
}

//
//	ULocalPlayer::Draw
//

void ULocalPlayer::Draw(FChildViewport* Viewport,FRenderInterface* RI)
{
	if ( !Actor )
		return;

	FLOAT		fFOV;
	FVector		ViewLocation;
	FRotator	ViewRotation;

	fFOV = Actor->eventGetFOVAngle();
	Actor->eventGetPlayerViewPoint( ViewLocation, ViewRotation );

	ULevel*		Level = Actor->XLevel;

	FSceneView	View;
	View.ViewMatrix = FTranslationMatrix(-ViewLocation);
	View.ViewMatrix = View.ViewMatrix * FInverseRotationMatrix(ViewRotation);
	View.ViewMatrix = View.ViewMatrix * FMatrix(
		FPlane(0,	0,	1,	0),
		FPlane(1,	0,	0,	0),
		FPlane(0,	1,	0,	0),
		FPlane(0,	0,	0,	1));

	if(Actor && Actor->PlayerCamera && Actor->PlayerCamera->bConstrainAspectRatio)
	{
		View.ProjectionMatrix = FPerspectiveMatrix(
			fFOV * (FLOAT)PI / 360.0f,
			Actor->PlayerCamera->ConstrainedAspectRatio,
			1.f,
			NEAR_CLIPPING_PLANE
			);
	}
	else
	{
		View.ProjectionMatrix = FPerspectiveMatrix(
			fFOV * (FLOAT)PI / 360.0f,
			Viewport->GetSizeX(),
			Viewport->GetSizeY(),
			NEAR_CLIPPING_PLANE
			);
	}

	View.BackgroundColor = FColor(0,0,0);

	View.ViewMode				= ViewMode;
	View.ShowFlags				= ShowFlags;
	View.State					= ViewState;
	View.AutomaticBrightness	= UseAutomaticBrightness;
	View.ViewActor				= Actor->GetViewTarget();

	FSceneContext Context(&View,Level,0,0,Viewport->GetSizeX(),Viewport->GetSizeY());		

	if(Actor && Actor->PlayerCamera)
	{
		// If desired, enforce a particular aspect ratio for the render of the scene. 
		// Results in black bars at top/bottom etc.
		if(Actor->PlayerCamera->bConstrainAspectRatio)
		{
			Context.FixAspectRatio(Actor->PlayerCamera->ConstrainedAspectRatio);
		}

		// Apply screen fade effect to screen.
		if(Actor->PlayerCamera->bEnableFading)
		{
			View.OverlayColor = Actor->PlayerCamera->FadeColor;
			View.OverlayColor.A = Clamp(appFloor(Actor->PlayerCamera->FadeAmount*256),0,255);
		}
	}

	// Pass new camera on to streaming manager.
	GStreamingManager->ProcessViewer( this, &Context );

	// Update the listener.
	UAudioDevice* AudioDevice = GEngine->Client->GetAudioDevice();	
	if( AudioDevice )
	{
		FMatrix CameraToWorld		= View.ViewMatrix.Inverse();

		FVector ProjUp				= CameraToWorld.TransformNormal(FVector(0,1000,0));
		FVector ProjRight			= CameraToWorld.TransformNormal(FVector(1000,0,0));
		FVector ProjFront			= ProjRight ^ ProjUp;

		ProjUp.Z = Abs( ProjUp.Z ); // Don't allow flipping "up".

		ProjUp.Normalize();
		ProjRight.Normalize();
		ProjFront.Normalize();

		AudioDevice->SetListener( ViewLocation, ProjUp, ProjRight, ProjFront );
	}

	UCanvas*	TempCanvas = ConstructObject<UCanvas>(UCanvas::StaticClass(),UObject::GetTransientPackage(),TEXT("TempCanvas"));
	TempCanvas->Init();
	TempCanvas->RenderInterface = RI;
	TempCanvas->SizeX = Viewport->GetSizeX();
	TempCanvas->SizeY = Viewport->GetSizeY();
	TempCanvas->SceneView = &View;
	TempCanvas->Update();

	RI->Clear(FColor(0,0,0));
	Actor->eventPreRender(TempCanvas);

	RI->DrawScene( Context );

	// Remove temporary debug lines.
	Level->LineBatcher->BatchedLines.Empty();

	// Render the HUD.
	BEGINCYCLECOUNTER(GEngineStats.HudTime);
	if ( Actor->myHUD )
	{
		Actor->myHUD->Canvas = TempCanvas;
		Actor->myHUD->eventPostRender();
		Actor->myHUD->Canvas = NULL;
	}
	Actor->Console->eventPostRender(TempCanvas);
	ENDCYCLECOUNTER;

	delete TempCanvas;

	//@todo joeg: Move this stuff to a function, make safe to use on consoles by
	// respecting the various safe zones, and make it compile out.
	INT	Y = 20;

	if( EnabledStats.FindItemIndex(TEXT("FPS")) != INDEX_NONE )
	{
		DOUBLE	Average = 0.0;

		for(DWORD HistoryIndex = 0;HistoryIndex < STAT_HISTORY_SIZE;HistoryIndex++)
			Average += GEngineStats.FrameTime.History[HistoryIndex] / DOUBLE(STAT_HISTORY_SIZE);
		
		Average = 1.0f / (GSecondsPerCycle * Average);

		RI->DrawShadowedString(
			Viewport->GetSizeX() - 70,
			96,
			*FString::Printf(TEXT("%2.1f FPS"),Average),
			GEngine->SmallFont,
			Average < 30.0f ? FColor(255,0,0) : (Average < 55.0f ? FColor(255,255,0) : FColor(0,255,0))
			);

		Y += 16;
	}

	for(FStatGroup* Group = GFirstStatGroup;Group;Group = Group->NextGroup)
		Group->Enabled = 0;

	for(UINT EnabledIndex = 0;EnabledIndex < (UINT)EnabledStats.Num();EnabledIndex++)
	{
		FStatGroup* EnabledGroup = NULL;
		for(FStatGroup* Group = GFirstStatGroup;Group;Group = Group->NextGroup)
		{
			if(EnabledStats(EnabledIndex) == Group->Label)
			{
				Y += Group->Render(RI,4,Y);
				EnabledGroup = Group;
				break;
			}
		}
		if( EnabledGroup )
			EnabledGroup->Enabled = 1;
	}

	// Render the stat chart.
	if(GStatChart)
	{
		GStatChart->Render(Viewport, RI);
	}
}

//
//	ULocalPlayer::Precache
//

void ULocalPlayer::Precache(FChildViewport* Viewport,FPrecacheInterface* P)
{
	// Precache textures...
	for(TObjectIterator<UTexture> It;It;++It)
		P->CacheResource(It->GetTexture());

	// Precache sounds... @todo audio
	if( GEngine->UseSound )
	{
		for(TObjectIterator<USoundNodeWave> It;It;++It)
		{
			It->RawData.Load();
		}
	}

	// Precache resources used by components.
	for(INT ComponentIndex = 0;ComponentIndex < Actor->XLevel->ModelComponents.Num();ComponentIndex++)
		if(Actor->XLevel->ModelComponents(ComponentIndex)->Initialized)
			Actor->XLevel->ModelComponents(ComponentIndex)->Precache(P);
	for(INT ActorIndex = 0;ActorIndex < Actor->XLevel->Actors.Num();ActorIndex++)
		if(Actor->XLevel->Actors(ActorIndex))
			for(UINT ComponentIndex = 0;ComponentIndex < (UINT)Actor->XLevel->Actors(ActorIndex)->Components.Num();ComponentIndex++)
				if(Actor->XLevel->Actors(ActorIndex)->Components(ComponentIndex) && Actor->XLevel->Actors(ActorIndex)->Components(ComponentIndex)->Initialized)
					Actor->XLevel->Actors(ActorIndex)->Components(ComponentIndex)->Precache(P);

	// Precache materials.
	for(TObjectIterator<UMaterial> It;It;++It)
		P->CacheResource(It->GetMaterialInterface(0));
}

//
//	ULocalPlayer::LostFocus
//

void ULocalPlayer::LostFocus(FChildViewport* Viewport)
{
	Viewport->CaptureMouseInput(0);
}

//
//	ULocalPlayer::ReceivedFocus
//

void ULocalPlayer::ReceivedFocus(FChildViewport* Viewport)
{
	Viewport->CaptureMouseInput(1);
}

//
//	ULocalPlayer::CloseRequested
//

void ULocalPlayer::CloseRequested(FViewport* Viewport)
{
	check(Viewport == this->Viewport);

	GEngine->Client->CloseViewport(this->Viewport);
	this->Viewport = this->MasterViewport = NULL;

}

//
//	ULocalPlayer::Serialize
//

void ULocalPlayer::Serialize(const TCHAR* V,EName Event)
{
}

//
//	ULocalPlayer::Exec
//

UBOOL ULocalPlayer::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	if( ParseCommand(&Cmd,TEXT("STAT")) )
	{
		FString	Group = ParseToken(Cmd,0).Caps();

		if(!EnabledStats.RemoveItem(Group))
			new(EnabledStats) FString(Group);
	}
	else
	if( ParseCommand(&Cmd,TEXT("SHOW")) )
	{
		struct { const TCHAR* Name; EShowFlags Flag; }	Flags[] =
		{
			{ TEXT("COLLISION"),	SHOW_Collision },
			{ TEXT("CONSTRAINTS"),	SHOW_Constraints },
			{ TEXT("STATICMESHES"),	SHOW_StaticMeshes },
			{ TEXT("TERRAIN"),		SHOW_Terrain },
			{ TEXT("BSP"),			SHOW_BSP },
			{ TEXT("SKELMESHES"),	SHOW_SkeletalMeshes },
			{ TEXT("BOUNDS"),		SHOW_Bounds },
			{ TEXT("FOG"),			SHOW_Fog },
			{ TEXT("FOLIAGE"),		SHOW_Foliage },
			{ TEXT("PATHS"),		SHOW_Paths },
			{ TEXT("MESHEDGES"),	SHOW_MeshEdges },
			{ TEXT("ZONECOLORS"),	SHOW_ZoneColors },
			{ TEXT("PORTALS"),		SHOW_Portals },
			{ TEXT("HITPROXIES"),	SHOW_HitProxies },
			{ TEXT("SHADOWFRUSTUMS"), SHOW_ShadowFrustums },
		};
		for(UINT FlagIndex = 0;FlagIndex < ARRAY_COUNT(Flags);FlagIndex++)
		{
			if(ParseCommand(&Cmd,Flags[FlagIndex].Name))
			{
				ShowFlags ^= Flags[FlagIndex].Flag;
				return 1;
			}
		}
		Ar.Logf(TEXT("Unknown show flag specified."));
		return 0;
	}
	else
	if( ParseCommand(&Cmd,TEXT("VIEWMODE")) )
	{
		if( ParseCommand(&Cmd,TEXT("WIREFRAME")) )
			ViewMode = SVM_Wireframe;
		else if( ParseCommand(&Cmd,TEXT("BRUSHWIREFRAME")) )
			ViewMode = SVM_BrushWireframe;
		else if( ParseCommand(&Cmd,TEXT("UNLIT")) )
			ViewMode = SVM_Unlit;
		else if( ParseCommand(&Cmd,TEXT("LIGHTINGONLY")) )
			ViewMode = SVM_LightingOnly;
		else if( ParseCommand(&Cmd,TEXT("LIGHTCOMPLEXITY")) )
			ViewMode = SVM_LightComplexity;
		else
			ViewMode = SVM_Lit;
		return 1;
	}
	else
	if( ParseCommand(&Cmd,TEXT("SETRES")) )
	{
		INT X=appAtoi(Cmd);
		TCHAR* CmdTemp = appStrchr(Cmd,'x') ? appStrchr(Cmd,'x')+1 : appStrchr(Cmd,'X') ? appStrchr(Cmd,'X')+1 : TEXT("");
		INT Y=appAtoi(CmdTemp);
		Cmd = CmdTemp;
		UBOOL	Fullscreen = MasterViewport->IsFullscreen();
		if(appStrchr(Cmd,'w') || appStrchr(Cmd,'W'))
			Fullscreen = 0;
		else if(appStrchr(Cmd,'f') || appStrchr(Cmd,'F'))
			Fullscreen = 1;
		if( X && Y )
			MasterViewport->Resize(X,Y,Fullscreen);
		return 1;
	}
	else
	if( ParseCommand(&Cmd,TEXT("SHOT")) )
	{
		// Read the contents of the viewport into an array.

		TArray<FColor>	Bitmap;
		if(!Viewport->ReadPixels(Bitmap))
			return 1;
		check(Bitmap.Num() == Viewport->GetSizeX() * Viewport->GetSizeY());

		// Save the contents of the array to a bitmap file.

		appCreateBitmap(TEXT("Shot"),Viewport->GetSizeX(),Viewport->GetSizeY(),&Bitmap(0),GFileManager);

		return 1;
	}
	else
	if(ParseCommand(&Cmd,TEXT("TOGGLEAUTOMATICBRIGHTNESS")))
	{
		UseAutomaticBrightness = !UseAutomaticBrightness;
		return 1;
	}
	else
	if( ParseCommand(&Cmd,TEXT("EXEC")) )
	{
		TCHAR Filename[512];
		if( ParseToken( Cmd, Filename, ARRAY_COUNT(Filename), 0 ) )
			ExecMacro( Filename, Ar );
		return 1;
	}
	else
	if(Actor)
	{
		if( Actor->GetLevel()->Exec(Cmd,Ar) )
		{
			return 1;
		}

		if( Actor->Level && Actor->Level->Game && Actor->Level->Game->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
		{
			return 1;
		}

		if( Actor->myHUD && Actor->myHUD->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
		{
			return 1;
		}

		if( Actor->CheatManager && Actor->CheatManager->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
		{
			return 1;
		}

		if( Actor->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
		{
			return 1;
		}

		if( Actor->PlayerInput && Actor->PlayerInput->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
		{
			return 1;
		}

		if( Actor->Pawn )
		{
			if( Actor->Pawn->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
				return 1;
			if( Actor->Pawn->InvManager->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
				return 1;
			if( Actor->Pawn->Weapon && Actor->Pawn->Weapon->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
				return 1;
		}

		if( Actor->GetLevel()->Engine->Exec(Cmd,Ar) )
		{
			return 1;
		}
	}

	return 0;

}

void ULocalPlayer::ExecMacro( const TCHAR* Filename, FOutputDevice& Ar )
{
	// Create text buffer and prevent garbage collection.
	UTextBuffer* Text = ImportObject<UTextBuffer>( Actor->GetLevel(), GetTransientPackage(), NAME_None, 0, Filename );
	if( Text )
	{
		Text->AddToRoot();
		debugf( TEXT("Execing %s"), Filename );
		TCHAR Temp[256];
		const TCHAR* Data = *Text->Text;
		while( ParseLine( &Data, Temp, ARRAY_COUNT(Temp) ) )
			Exec( Temp, Ar );
		Text->RemoveFromRoot();
		delete Text;
	}
	else
		Ar.Logf( NAME_ExecWarning, *LocalizeError("FileNotFound",TEXT("Core")), Filename );
}
