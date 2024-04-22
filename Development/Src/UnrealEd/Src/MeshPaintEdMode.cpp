/*================================================================================
	MeshPaintEdMode.cpp: Mesh paint tool
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/

#include "UnrealEd.h"
#include "MeshPaintEdMode.h"
#include "Factories.h"
#include "../../Engine/Src/ScenePrivate.h"
#include "ScopedTransaction.h"
#include "MeshPaintRendering.h"
#include "ImageUtils.h"
#include "../Inc/UnObjectTools.h"


#if _WINDOWS
#include "WinDrv.h"
#endif

#if WITH_MANAGED_CODE
#include "MeshPaintWindowShared.h"
#endif

/** Static: Global mesh paint settings */
FMeshPaintSettings FMeshPaintSettings::StaticMeshPaintSettings;


/** Batched element parameters for texture paint shaders used for paint blending and paint mask generation */
class FMeshPaintBatchedElementParameters
	: public FBatchedElementParameters
{

public:

	/** Binds vertex and pixel shaders for this element */
	virtual void BindShaders_RenderThread( const FMatrix& InTransform,
		const FLOAT InGamma )
	{
		MeshPaintRendering::SetMeshPaintShaders_RenderThread( InTransform, InGamma, ShaderParams );
	}

public:

	/** Shader parameters */
	MeshPaintRendering::FMeshPaintShaderParameters ShaderParams;
};


/** Batched element parameters for texture paint shaders used for texture dilation */
class FMeshPaintDilateBatchedElementParameters
	: public FBatchedElementParameters
{

public:

	/** Binds vertex and pixel shaders for this element */
	virtual void BindShaders_RenderThread( const FMatrix& InTransform,
		const FLOAT InGamma )
	{
		MeshPaintRendering::SetMeshPaintDilateShaders_RenderThread( InTransform, InGamma, ShaderParams );
	}

public:

	/** Shader parameters */
	MeshPaintRendering::FMeshPaintDilateShaderParameters ShaderParams;
};


/** Constructor */
FEdModeMeshPaint::FEdModeMeshPaint() 
	: FEdMode(),
	  bIsPainting( FALSE ),
	  bIsFloodFill( FALSE ),
	  bPushInstanceColorsToMesh( FALSE ),
	  PaintingStartTime( 0.0 ),
	  ModifiedStaticMeshes(),
	  TexturePaintingStaticMeshComponent( NULL ),
	  PaintingTexture2D( NULL ),
	  bDoRestoreRenTargets( FALSE ),
	  BrushRenderTargetTexture( NULL),
	  BrushMaskRenderTargetTexture( NULL ),
	  SeamMaskRenderTargetTexture( NULL )
{
	ID = EM_MeshPaint;
	Desc = TEXT( "Mesh Painting" );
}


/** Destructor */
FEdModeMeshPaint::~FEdModeMeshPaint()
{
	CopiedColorsByLOD.Empty();
}



/** FSerializableObject: Serializer */
void FEdModeMeshPaint::Serialize( FArchive &Ar )
{
	// Call parent implementation
	FEdMode::Serialize( Ar );

	Ar << ModifiedStaticMeshes;
	Ar << StaticMeshToTempkDOPMap;
	Ar << TexturePaintingStaticMeshComponent;
	Ar << PaintingTexture2D;
	Ar << PaintTargetData;
	Ar << BrushRenderTargetTexture;
	Ar << BrushMaskRenderTargetTexture;
	Ar << SeamMaskRenderTargetTexture;
}



/** FEdMode: Called when the mode is entered */
void FEdModeMeshPaint::Enter()
{
	// Call parent implementation
	FEdMode::Enter();


	{
		// Load config from file
		{
			const FString SectionName = TEXT( "MeshPaint" );

			// WindowPosition ("X,Y")
			const FString WindowPositionFieldName = TEXT( "WindowPosition" );
			FString WindowPositionString;
			if( GConfig->GetString( *SectionName,
									*WindowPositionFieldName,
									WindowPositionString,
									GEditorUserSettingsIni ) )
			{
				TArray< FString > PositionValues;
				const UBOOL bCullEmptyStrings = TRUE;
				WindowPositionString.ParseIntoArray( &PositionValues, TEXT( "," ), bCullEmptyStrings );
				if( PositionValues.Num() >= 1 )
				{
					FMeshPaintSettings::Get().WindowPositionX = appAtoi( *PositionValues( 0 ) );
				}
				if( PositionValues.Num() >= 2 )
				{
					FMeshPaintSettings::Get().WindowPositionY = appAtoi( *PositionValues( 1 ) );
				}
			}
		}

		// The user can manipulate the editor selection lock flag in paint mode so we save off the value here so it can be restored later
		bWasSelectionLockedOnStart = GEdSelectionLock;

		// Make sure texture list gets updated
		bShouldUpdateTextureList = TRUE;

	}

#if WITH_MANAGED_CODE
	// Create the mesh paint window
	MeshPaintWindow.Reset( FMeshPaintWindow::CreateMeshPaintWindow( this ) );
	check( MeshPaintWindow.IsValid() );
	// Refresh MeshPaint window on init
	MeshPaintWindow->RefreshAllProperties();
#endif



	// Change the engine to draw selected objects without a color boost, but unselected objects will
	// be darkened slightly.  This just makes it easier to paint on selected objects without the
	// highlight effect distorting the appearance.
	GEngine->SelectedMaterialColor = FLinearColor::Black;		// Additive (black = off)
//	GEngine->UnselectedMaterialColor = FLinearColor( 0.05f, 0.0f, 0.0f, 1.0f );



	// Force real-time viewports.  We'll back up the current viewport state so we can restore it when the
	// user exits this mode.
	const UBOOL bWantRealTime = TRUE;
	const UBOOL bRememberCurrentState = TRUE;
	ForceRealTimeViewports( bWantRealTime, bRememberCurrentState );

	// Set viewport show flags
	const UBOOL bAllowColorViewModes = TRUE;
	SetViewportShowFlags( bAllowColorViewModes );
}



/** FEdMode: Called when the mode is exited */
void FEdModeMeshPaint::Exit()
{
	// If the user has pending changes and the editor is not exiting, we want to do the commit for all the modified textures.
	if( GetNumberOfPendingPaintChanges() > 0 && !GIsRequestingExit )
	{
		WxChoiceDialog ConfirmationPrompt(
			LocalizeUnrealEd("MeshPaintMode_Warning_Commit"), 
			LocalizeUnrealEd("MeshPaintMode_TexturePaint_Transaction"),
			WxChoiceDialogBase::Choice( AMT_OK, LocalizeUnrealEd("Yes"), WxChoiceDialogBase::DCT_DefaultAffirmative ),
			WxChoiceDialogBase::Choice( AMT_OKCancel, LocalizeUnrealEd("No"), WxChoiceDialogBase::DCT_DefaultCancel ) );
		ConfirmationPrompt.ShowModal();

		if ( ConfirmationPrompt.GetChoice().ReturnCode == AMT_OK )
		{
			CommitAllPaintedTextures();
		}
		else
		{
			//Restore the original textures since Undo will commit changes.
			TArray< PaintTexture2DData > Array;
			PaintTargetData.GenerateValueArray(Array);
			for(int Index = 0; Index < Array.Num(); ++Index)
			{
				PaintTexture2DData* TextureData = &Array(Index);
				check(TextureData != NULL);

				TArray<BYTE> TexturePixels;
				TextureData->PaintingTexture2DDuplicate->GetUncompressedSourceArt(TexturePixels);
				TextureData->PaintingTexture2D->SetUncompressedSourceArt( TexturePixels.GetData(), TexturePixels.Num() * sizeof( BYTE ) );
				TextureData->PaintingTexture2D->SRGB = TextureData->PaintingTexture2DDuplicate->SRGB; //Abs( RenderTargetResource->GetDisplayGamma() - 1.0f ) >= KINDA_SMALL_NUMBER;
				TextureData->PaintingTexture2D->PostEditChange();
			}
			

			ClearAllTextureOverrides();
		}
	}
	else
	{
		ClearAllTextureOverrides();
	}

	// The user can manipulate the editor selection lock flag in paint mode so we make sure to restore it here
	GEdSelectionLock = bWasSelectionLockedOnStart;

	// Restore real-time viewport state if we changed it
	const UBOOL bWantRealTime = FALSE;
	const UBOOL bRememberCurrentState = FALSE;
	ForceRealTimeViewports( bWantRealTime, bRememberCurrentState );

	// Disable color view modes if we set those
	const UBOOL bAllowColorViewModes = FALSE;
	SetViewportShowFlags( bAllowColorViewModes );

	// Restore selection color
	GEngine->SelectedMaterialColor = GEditorModeTools().GetHighlightWithBrackets() ? FLinearColor::Black : GEngine->DefaultSelectedMaterialColor;
//	GEngine->UnselectedMaterialColor = FLinearColor::Black;			// Additive (black = off)

	// Remove all custom made kDOP's
	StaticMeshToTempkDOPMap.Empty();

	// Save any settings that may have changed
#if WITH_MANAGED_CODE
	// Kill the mesh paint window
	MeshPaintWindow.Reset();
	FImportColorsScreen::Shutdown();
#endif

	PaintTargetData.Empty();

	// Remove any existing texture targets
	TexturePaintTargetList.Empty();

	// Clear out cached settings map
	StaticMeshSettingsMap.Empty();

	// Call parent implementation
	FEdMode::Exit();
}



/** FEdMode: Called when the mouse is moved over the viewport */
UBOOL FEdModeMeshPaint::MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
{
	// We only care about perspective viewports
	if( ViewportClient->IsPerspective() )
	{
		// ...
	}

	return FALSE;
}



/**
 * Called when the mouse is moved while a window input capture is in effect
 *
 * @param	InViewportClient	Level editor viewport client that captured the mouse input
 * @param	InViewport			Viewport that captured the mouse input
 * @param	InMouseX			New mouse cursor X coordinate
 * @param	InMouseY			New mouse cursor Y coordinate
 *
 * @return	TRUE if input was handled
 */
UBOOL FEdModeMeshPaint::CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
{
	// We only care about perspective viewports
	if( InViewportClient->IsPerspective() && (InViewportClient->ShowFlags & SHOW_ModeWidgets) )
	{
		if( bIsPainting )
		{
			// Compute a world space ray from the screen space mouse coordinates
			FSceneViewFamilyContext ViewFamily(
				InViewportClient->Viewport, InViewportClient->GetScene(),
				InViewportClient->ShowFlags,
				GWorld->GetTimeSeconds(),
				GWorld->GetDeltaSeconds(),
				GWorld->GetRealTimeSeconds(),
				InViewportClient->IsRealtime()
				);
			FSceneView* View = InViewportClient->CalcSceneView( &ViewFamily );
			FViewportCursorLocation MouseViewportRay( View, (FEditorLevelViewportClient*)InViewport->GetClient(), InMouseX, InMouseY );

			
			// Paint!
			const UBOOL bVisualCueOnly = FALSE;
			const EMeshPaintAction::Type PaintAction = GetPaintAction(InViewport);
			// Apply stylus pressure
			const FLOAT StrengthScale = InViewport->IsPenActive() ? InViewport->GetTabletPressure() : 1.f;

			UBOOL bAnyPaintAbleActorsUnderCursor = FALSE;

			UBOOL bIsTexturePaintMode = FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture;
			if( bIsTexturePaintMode )
			{
				ENQUEUE_UNIQUE_RENDER_COMMAND(
					TexturePaintBeginSceneCommand,
				{
					RHIBeginScene();
				});
			}
		
			DoPaint( View->ViewOrigin, MouseViewportRay.GetOrigin(), MouseViewportRay.GetDirection(), NULL, PaintAction, bVisualCueOnly, StrengthScale, bAnyPaintAbleActorsUnderCursor );

			if( bIsTexturePaintMode )
			{
				ENQUEUE_UNIQUE_RENDER_COMMAND(
					TexturePaintEndSceneCommand,
				{
					RHIEndScene();
				});
			}
			return TRUE;
		}
	}

	return FALSE;
}



/** FEdMode: Called when a mouse button is pressed */
UBOOL FEdModeMeshPaint::StartTracking()
{
	return TRUE;
}



/** FEdMode: Called when the a mouse button is released */
UBOOL FEdModeMeshPaint::EndTracking()
{
	return TRUE;
}



/** FEdMode: Called when a key is pressed */
UBOOL FEdModeMeshPaint::InputKey( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, FName InKey, EInputEvent InEvent )
{
	UBOOL bHandled = FALSE;

	const UBOOL bIsLeftButtonDown = ( InKey == KEY_LeftMouseButton && InEvent != IE_Released ) || InViewport->KeyState( KEY_LeftMouseButton );
	const UBOOL bIsCtrlDown = ( ( InKey == KEY_LeftControl || InKey == KEY_RightControl ) && InEvent != IE_Released ) || InViewport->KeyState( KEY_LeftControl ) || InViewport->KeyState( KEY_RightControl );
	const UBOOL bIsShiftDown = ( ( InKey == KEY_LeftShift || InKey == KEY_RightShift ) && InEvent != IE_Released ) || InViewport->KeyState( KEY_LeftShift ) || InViewport->KeyState( KEY_RightShift );

	// Change Brush Size - We want to stay consistent with other brush utilities.  Here we model after landscape mode.
	if ((InEvent == IE_Pressed || InEvent == IE_Repeat) && (InKey == KEY_LeftBracket || InKey == KEY_RightBracket) )
	{
		FLOAT Radius = FMeshPaintSettings::Get().BrushRadius;
		FLOAT SliderMin = 0.f;
		FLOAT SliderMax = 2048.f;
		FLOAT LogPosition = Clamp(Radius / SliderMax, 0.0f, 1.0f);
		FLOAT Diff = 0.05f; 

		if (InKey == KEY_LeftBracket)
		{
			Diff = -Diff;
		}

		FLOAT NewValue = Radius*(1.f+Diff);

		if (InKey == KEY_LeftBracket)
		{
			NewValue = Min(NewValue, Radius - 1.f);
		}
		else
		{
			NewValue = Max(NewValue, Radius + 1.f);
		}

		NewValue = (INT)Clamp(NewValue, SliderMin, SliderMax);

		FMeshPaintSettings::Get().BrushRadius = NewValue;
#if WITH_MANAGED_CODE
		if( MeshPaintWindow != NULL )
		{
			MeshPaintWindow->RefreshAllProperties();
		}
#endif
		bHandled = TRUE;
	}

	if( FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture )
	{
		// Prev texture 
		if( InEvent == IE_Pressed && InKey == KEY_Comma )
		{
			if(TexturePaintTargetList.Num() > 0)
			{
				DuplicateTextureMaterialCombo();
			}
			return TRUE;

			SelectPrevTexture();
			bHandled = TRUE;
		}

		// Next texture 
		if( InEvent == IE_Pressed && InKey == KEY_Period )
		{
			SelectNextTexture();
			bHandled = TRUE;
		}

		if( bIsCtrlDown && bIsShiftDown && InEvent == IE_Pressed && InKey == KEY_T )
		{
			FindSelectedTextureInContentBrowser();
			bHandled = TRUE;
		}

		if( bIsCtrlDown && bIsShiftDown && InEvent == IE_Pressed && InKey == KEY_C )
		{
			// Only process commit requests if the user isn't painting.
			if( PaintingTexture2D == NULL )
			{
				CommitAllPaintedTextures();
			}
			bHandled = TRUE;
		}
	}

	// When painting we only care about perspective viewports where we are we are allowed to show mode widgets
	if( InViewportClient->IsPerspective() && (InViewportClient->ShowFlags & SHOW_ModeWidgets))
	{
		// Does the user want to paint right now?
		const UBOOL bUserWantsPaint = bIsLeftButtonDown && bIsCtrlDown;
		UBOOL bAnyPaintAbleActorsUnderCursor = FALSE;

		// Stop current tracking if the user released Ctrl+LMB
		if( bIsPainting && !bUserWantsPaint )
		{
			bHandled = TRUE;
			bIsPainting = FALSE;
			FinishPaintingTexture();

			// Rebuild any static meshes that we painted on last stroke
			{
				for( INT CurMeshIndex = 0; CurMeshIndex < ModifiedStaticMeshes.Num(); ++CurMeshIndex )
				{
					UStaticMesh* CurStaticMesh = ModifiedStaticMeshes( CurMeshIndex );

					// @todo MeshPaint: Do we need to do bother doing a full rebuild even with real-time turbo-rebuild?
					if( 0 )
					{
						// Rebuild the modified mesh
						CurStaticMesh->Build();
					}
				}

				ModifiedStaticMeshes.Empty();
			}	
		}
		else if( !bIsPainting && bUserWantsPaint )
		{
			// Re-initialize new tracking only if a new button was pressed, otherwise we continue the previous one.
			bIsPainting = TRUE;
			PaintingStartTime = appSeconds();
			bHandled = TRUE;


			// Go ahead and paint immediately
			{
				// Compute a world space ray from the screen space mouse coordinates
				FSceneViewFamilyContext ViewFamily(
					InViewportClient->Viewport, InViewportClient->GetScene(),
					InViewportClient->ShowFlags,
					GWorld->GetTimeSeconds(),
					GWorld->GetDeltaSeconds(),
					GWorld->GetRealTimeSeconds(),
					InViewportClient->IsRealtime()
					);
				FSceneView* View = InViewportClient->CalcSceneView( &ViewFamily );
				FViewportCursorLocation MouseViewportRay( View, (FEditorLevelViewportClient*)InViewport->GetClient(), InViewport->GetMouseX(), InViewport->GetMouseY() );

				// Paint!
				const UBOOL bVisualCueOnly = FALSE;
				const EMeshPaintAction::Type PaintAction = GetPaintAction(InViewport);
				const FLOAT StrengthScale = 1.0f;

				UBOOL bIsTexturePaintMode = FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture;
				if( bIsTexturePaintMode )
				{
					ENQUEUE_UNIQUE_RENDER_COMMAND(
						TexturePaintBeginSceneCommand,
					{
						RHIBeginScene();
					});
				}

				DoPaint( View->ViewOrigin, MouseViewportRay.GetOrigin(), MouseViewportRay.GetDirection(), NULL, PaintAction, bVisualCueOnly, StrengthScale, bAnyPaintAbleActorsUnderCursor );
				
				if( bIsTexturePaintMode )
				{
					ENQUEUE_UNIQUE_RENDER_COMMAND(
						TexturePaintEndSceneCommand,
					{
						RHIEndScene();
					});
				}
			}
		}


		// Also absorb other mouse buttons, and Ctrl/Alt/Shift events that occur while we're painting as these would cause
		// the editor viewport to start panning/dollying the camera
		{
			const UBOOL bIsOtherMouseButtonEvent = ( InKey == KEY_MiddleMouseButton || InKey == KEY_RightMouseButton );
			const UBOOL bCtrlButtonEvent = (InKey == KEY_LeftControl || InKey == KEY_RightControl);
			const UBOOL bShiftButtonEvent = (InKey == KEY_LeftShift || InKey == KEY_RightShift);
			const UBOOL bAltButtonEvent = (InKey == KEY_LeftAlt || InKey == KEY_RightAlt);
			if( bIsPainting && ( bIsOtherMouseButtonEvent || bShiftButtonEvent || bAltButtonEvent ) )
			{
				bHandled = TRUE;
			}

			if( bCtrlButtonEvent)
			{
				bHandled = FALSE;
			}
			else if( bIsCtrlDown)
			{
				//default to assuming this is a paint command
				bHandled = TRUE;

				// If no other button was pressed && if a first press and we click OFF of an actor and we will let this pass through so multi-select can attempt to handle it 
				if ((!(bShiftButtonEvent || bAltButtonEvent || bIsOtherMouseButtonEvent)) && ((InKey == KEY_LeftMouseButton) && ((InEvent == IE_Pressed) || (InEvent == IE_Released)) && (!bAnyPaintAbleActorsUnderCursor)))
				{
					bHandled = FALSE;
					bIsPainting = FALSE;
				}

				// Allow Ctrl+B to pass through so we can support the finding of a selected static mesh in the content browser.
				if ( !(bShiftButtonEvent || bAltButtonEvent || bIsOtherMouseButtonEvent) && ( (InKey == KEY_B) && (InEvent == IE_Pressed) ) )
				{
					bHandled = FALSE;
				}

				// If we are not painting, we will let the CTRL-Z and CTRL-Y key presses through to support undo/redo.
				if ( !bIsPainting && ( InKey == KEY_Z || InKey == KEY_Y ) )
				{
					bHandled = FALSE;
				}
			}
		}
	}


	return bHandled;
}




/** Mesh paint parameters */
class FMeshPaintParameters
{

public:

	EMeshPaintMode::Type PaintMode;
	EMeshPaintAction::Type PaintAction;
	FVector BrushPosition;
	FVector BrushNormal;
	FLinearColor BrushColor;
	FLOAT SquaredBrushRadius;
	FLOAT BrushRadialFalloffRange;
	FLOAT InnerBrushRadius;
	FLOAT BrushDepth;
	FLOAT BrushDepthFalloffRange;
	FLOAT InnerBrushDepth;
	FLOAT BrushStrength;
	FMatrix BrushToWorldMatrix;
	UBOOL bWriteRed;
	UBOOL bWriteGreen;
	UBOOL bWriteBlue;
	UBOOL bWriteAlpha;
	INT TotalWeightCount;
	INT PaintWeightIndex;
	INT UVChannel;

};




/** Static: Determines if a world space point is influenced by the brush and reports metrics if so */
UBOOL FEdModeMeshPaint::IsPointInfluencedByBrush( const FVector& InPosition,
												  const FMeshPaintParameters& InParams,
												  FLOAT& OutSquaredDistanceToVertex2D,
												  FLOAT& OutVertexDepthToBrush )
{
	// Project the vertex into the plane of the brush
	FVector BrushSpaceVertexPosition = InParams.BrushToWorldMatrix.InverseTransformFVectorNoScale( InPosition );
	FVector2D BrushSpaceVertexPosition2D( BrushSpaceVertexPosition.X, BrushSpaceVertexPosition.Y );

	// Is the brush close enough to the vertex to paint?
	const FLOAT SquaredDistanceToVertex2D = BrushSpaceVertexPosition2D.SizeSquared();
	if( SquaredDistanceToVertex2D <= InParams.SquaredBrushRadius )
	{
		// OK the vertex is overlapping the brush in 2D space, but is it too close or
		// two far (depth wise) to be influenced?
		const FLOAT VertexDepthToBrush = Abs( BrushSpaceVertexPosition.Z );
		if( VertexDepthToBrush <= InParams.BrushDepth )
		{
			OutSquaredDistanceToVertex2D = SquaredDistanceToVertex2D;
			OutVertexDepthToBrush = VertexDepthToBrush;
			return TRUE;
		}
	}

	return FALSE;
}



/** Paints the specified vertex!  Returns TRUE if the vertex was in range. */
UBOOL FEdModeMeshPaint::PaintVertex( const FVector& InVertexPosition,
									 const FMeshPaintParameters& InParams,
									 const UBOOL bIsPainting,
									 FColor& InOutVertexColor )
{
	FLOAT SquaredDistanceToVertex2D;
	FLOAT VertexDepthToBrush;
	if( IsPointInfluencedByBrush( InVertexPosition, InParams, SquaredDistanceToVertex2D, VertexDepthToBrush ) )
	{
		if( bIsPainting )
		{
			// Compute amount of paint to apply
			FLOAT PaintAmount = 1.0f;

			// Apply radial-based falloff
			{
				// Compute the actual distance
				FLOAT DistanceToVertex2D = 0.0f;
				if( SquaredDistanceToVertex2D > KINDA_SMALL_NUMBER )
				{
					DistanceToVertex2D = appSqrt( SquaredDistanceToVertex2D );
				}

				if( DistanceToVertex2D > InParams.InnerBrushRadius )
				{
					const FLOAT RadialBasedFalloff = ( DistanceToVertex2D - InParams.InnerBrushRadius ) / InParams.BrushRadialFalloffRange;
					PaintAmount *= 1.0f - RadialBasedFalloff;
				}
			}

			// Apply depth-based falloff
			{
				if( VertexDepthToBrush > InParams.InnerBrushDepth )
				{
					const FLOAT DepthBasedFalloff = ( VertexDepthToBrush - InParams.InnerBrushDepth ) / InParams.BrushDepthFalloffRange;
					PaintAmount *= 1.0f - DepthBasedFalloff;
					
//						debugf( TEXT( "Painted Vertex:  DepthBasedFalloff=%.2f" ), DepthBasedFalloff );
				}
			}

			PaintAmount *= InParams.BrushStrength;

			
			// Paint!

			// NOTE: We manually perform our own conversion between FColor and FLinearColor (and vice versa) here
			//	  as we want values to be linear (not gamma corrected.)  These color values are often used as scalars
			//	  to blend between textures, etc, and must be linear!

			const FLinearColor OldColor = InOutVertexColor.ReinterpretAsLinear();
			FLinearColor NewColor = OldColor;



			if( InParams.PaintMode == EMeshPaintMode::PaintColors )
			{
				// Color painting

				if( InParams.bWriteRed )
				{
					if( OldColor.R < InParams.BrushColor.R )
					{
						NewColor.R = Min( InParams.BrushColor.R, OldColor.R + PaintAmount );
					}
					else
					{
						NewColor.R = Max( InParams.BrushColor.R, OldColor.R - PaintAmount );
					}
				}

				if( InParams.bWriteGreen )
				{
					if( OldColor.G < InParams.BrushColor.G )
					{
						NewColor.G = Min( InParams.BrushColor.G, OldColor.G + PaintAmount );
					}
					else
					{
						NewColor.G = Max( InParams.BrushColor.G, OldColor.G - PaintAmount );
					}
				}

				if( InParams.bWriteBlue )
				{
					if( OldColor.B < InParams.BrushColor.B )
					{
						NewColor.B = Min( InParams.BrushColor.B, OldColor.B + PaintAmount );
					}
					else
					{
						NewColor.B = Max( InParams.BrushColor.B, OldColor.B - PaintAmount );
					}
				}

				if( InParams.bWriteAlpha )
				{
					if( OldColor.A < InParams.BrushColor.A )
					{
						NewColor.A = Min( InParams.BrushColor.A, OldColor.A + PaintAmount );
					}
					else
					{
						NewColor.A = Max( InParams.BrushColor.A, OldColor.A - PaintAmount );
					}
				}
			}
			else if( InParams.PaintMode == EMeshPaintMode::PaintWeights )
			{
				// Weight painting
				
				
				// Total number of texture blend weights we're using
				check( InParams.TotalWeightCount > 0 );
				check( InParams.TotalWeightCount <= MeshPaintDefs::MaxSupportedWeights );

				// True if we should assume the last weight index is composed of one minus the sum of all
				// of the other weights.  This effectively allows an additional weight with no extra memory
				// used, but potentially requires extra pixel shader instructions to render.
				//
				// NOTE: If you change the default here, remember to update the MeshPaintWindow UI and strings
				//
				// NOTE: Materials must be authored to match the following assumptions!
				const UBOOL bUsingOneMinusTotal =
					InParams.TotalWeightCount == 2 ||		// Two textures: Use a lerp() in pixel shader (single value)
					InParams.TotalWeightCount == 5;			// Five texture: Requires 1.0-sum( R+G+B+A ) in shader
				check( bUsingOneMinusTotal || InParams.TotalWeightCount <= MeshPaintDefs::MaxSupportedPhysicalWeights );

				// Prefer to use RG/RGB instead of AR/ARG when we're only using 2/3 physical weights
				const INT TotalPhysicalWeights = bUsingOneMinusTotal ? InParams.TotalWeightCount - 1 : InParams.TotalWeightCount;
				const UBOOL bUseColorAlpha =
					TotalPhysicalWeights != 2 &&			// Two physical weights: Use RG instead of AR
					TotalPhysicalWeights != 3;				// Three physical weights: Use RGB instead of ARG

				// Index of the blend weight that we're painting
				check( InParams.PaintWeightIndex >= 0 && InParams.PaintWeightIndex < MeshPaintDefs::MaxSupportedWeights );


				// Convert the color value to an array of weights
				FLOAT Weights[ MeshPaintDefs::MaxSupportedWeights ];
				{
					for( INT CurWeightIndex = 0; CurWeightIndex < InParams.TotalWeightCount; ++CurWeightIndex )
					{											  
						if( CurWeightIndex == TotalPhysicalWeights )
						{
							// This weight's value is one minus the sum of all previous weights
							FLOAT OtherWeightsTotal = 0.0f;
							for( INT OtherWeightIndex = 0; OtherWeightIndex < CurWeightIndex; ++OtherWeightIndex )
							{
								OtherWeightsTotal += Weights[ OtherWeightIndex ];
							}
							Weights[ CurWeightIndex ] = 1.0f - OtherWeightsTotal;
						}
						else
						{
							switch( CurWeightIndex )
							{
								case 0:
									Weights[ CurWeightIndex ] = bUseColorAlpha ? OldColor.A : OldColor.R;
									break;

								case 1:
									Weights[ CurWeightIndex ] = bUseColorAlpha ? OldColor.R : OldColor.G;
									break;

								case 2:
									Weights[ CurWeightIndex ] = bUseColorAlpha ? OldColor.G : OldColor.B;
									break;

								case 3:
									check( bUseColorAlpha );
									Weights[ CurWeightIndex ] = OldColor.B;
									break;

								default:
									appErrorf( TEXT( "Invalid weight index" ) );
									break;
							}
						}
					}
				}
				

				// Go ahead any apply paint!
				{
					Weights[ InParams.PaintWeightIndex ] += PaintAmount;
					Weights[ InParams.PaintWeightIndex ] = Clamp( Weights[ InParams.PaintWeightIndex ], 0.0f, 1.0f );
				}


				// Now renormalize all of the other weights
				{
					FLOAT OtherWeightsTotal = 0.0f;
					for( INT CurWeightIndex = 0; CurWeightIndex < InParams.TotalWeightCount; ++CurWeightIndex )
					{
						if( CurWeightIndex != InParams.PaintWeightIndex )
						{
							OtherWeightsTotal += Weights[ CurWeightIndex ];
						}
					}
					const FLOAT NormalizeTarget = 1.0f - Weights[ InParams.PaintWeightIndex ];
					for( INT CurWeightIndex = 0; CurWeightIndex < InParams.TotalWeightCount; ++CurWeightIndex )
					{
						if( CurWeightIndex != InParams.PaintWeightIndex )
						{
							if( OtherWeightsTotal == 0.0f )
							{
								Weights[ CurWeightIndex ] = NormalizeTarget / ( InParams.TotalWeightCount - 1 );
							}
							else
							{
								Weights[ CurWeightIndex ] = Weights[ CurWeightIndex ] / OtherWeightsTotal * NormalizeTarget;
							}
						}
					}
				}


				// The total of the weights should now always equal 1.0
				{
					FLOAT WeightsTotal = 0.0f;
					for( INT CurWeightIndex = 0; CurWeightIndex < InParams.TotalWeightCount; ++CurWeightIndex )
					{
						WeightsTotal += Weights[ CurWeightIndex ];
					}
					check( appIsNearlyEqual( WeightsTotal, 1.0f, 0.01f ) );
				}


				// Convert the weights back to a color value					
				{
					for( INT CurWeightIndex = 0; CurWeightIndex < InParams.TotalWeightCount; ++CurWeightIndex )
					{
						// We can skip the non-physical weights as it's already baked into the others
						if( CurWeightIndex != TotalPhysicalWeights )
						{
							switch( CurWeightIndex )
							{
								case 0:
									if( bUseColorAlpha )
									{
										NewColor.A = Weights[ CurWeightIndex ];
									}
									else
									{
										NewColor.R = Weights[ CurWeightIndex ];
									}
									break;

								case 1:
									if( bUseColorAlpha )
									{
										NewColor.R = Weights[ CurWeightIndex ];
									}
									else
									{
										NewColor.G = Weights[ CurWeightIndex ];
									}
									break;

								case 2:
									if( bUseColorAlpha )
									{
										NewColor.G = Weights[ CurWeightIndex ];
									}
									else
									{
										NewColor.B = Weights[ CurWeightIndex ];
									}
									break;

								case 3:
									NewColor.B = Weights[ CurWeightIndex ];
									break;

								default:
									appErrorf( TEXT( "Invalid weight index" ) );
									break;
							}
						}
					}
				}
				
			}




			// Save the new color
			InOutVertexColor.R = Clamp( appRound( NewColor.R * 255.0f ), 0, 255 );
			InOutVertexColor.G = Clamp( appRound( NewColor.G * 255.0f ), 0, 255 );
			InOutVertexColor.B = Clamp( appRound( NewColor.B * 255.0f ), 0, 255 );
			InOutVertexColor.A = Clamp( appRound( NewColor.A * 255.0f ), 0, 255 );


			// debugf( TEXT( "Painted Vertex:  OldColor=[%.2f,%.2f,%.2f,%.2f], NewColor=[%.2f,%.2f,%.2f,%.2f]" ), OldColor.R, OldColor.G, OldColor.B, OldColor.A, NewColor.R, NewColor.G, NewColor.B, NewColor.A );
		}

		return TRUE;
	}


	// Out of range
	return FALSE;
}

/* Builds a temporary kDOP tree for doing line checks on meshes without collision */
static void BuildTempKDOPTree( UStaticMesh* Mesh, FEdModeMeshPaint::kDOPTreeType& OutKDOPTree )
{
	// First LOD is the only lod supported for mesh paint.
	FStaticMeshRenderData& LODModel = Mesh->LODModels( 0 );
	TArray<FkDOPBuildCollisionTriangle<WORD> > kDOPBuildTriangles;

	// Get the kDOP triangles from the lod model and build the tree
	LODModel.GetKDOPTriangles( kDOPBuildTriangles );
	OutKDOPTree.Build( kDOPBuildTriangles );
}


/** Paint the mesh that impacts the specified ray */
void FEdModeMeshPaint::DoPaint( const FVector& InCameraOrigin,
							    const FVector& InRayOrigin,
							    const FVector& InRayDirection,
								FPrimitiveDrawInterface* PDI,
								const EMeshPaintAction::Type InPaintAction,
								const UBOOL bVisualCueOnly,
								const FLOAT InStrengthScale,
								OUT UBOOL& bAnyPaintAbleActorsUnderCursor)
{
	const FLOAT BrushRadius = FMeshPaintSettings::Get().BrushRadius;

	// Fire out a ray to see if there is a *selected* static mesh under the mouse cursor.
	// NOTE: We can't use a GWorld line check for this as that would ignore actors that have collision disabled
	TArray< AActor* > PaintableActors;
	FCheckResult BestTraceResult;
	{
		const FVector TraceStart( InRayOrigin );
		const FVector TraceEnd( InRayOrigin + InRayDirection * HALF_WORLD_MAX );

		// Iterate over selected actors looking for static meshes
		USelection& SelectedActors = *GEditor->GetSelectedActors();
		TArray< AActor* > ValidSelectedActors;
		for( INT CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex )
		{
			UBOOL bHasKDOPTree = TRUE;
			UBOOL bCurActorIsValid = FALSE;
			AActor* CurActor = CastChecked< AActor >( SelectedActors( CurSelectedActorIndex ) );

			// Disregard non-selected actors
			if( !CurActor->IsSelected() )
			{
				continue;
			}
			const UBOOL bOldCollideActors = CurActor->bCollideActors;
			UBOOL bOldCollideActorsComponent = FALSE;

			UStaticMesh* CurStaticMesh = NULL;
			UStaticMeshComponent* CurStaticMeshComponent = NULL;

			// Is this a static mesh actor?
			AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( CurActor );
			if( StaticMeshActor != NULL &&
				StaticMeshActor->StaticMeshComponent != NULL &&
				StaticMeshActor->StaticMeshComponent->StaticMesh != NULL )
			{
				// ignore Actors who's static meshes are hidden in the editor
				CurStaticMeshComponent = StaticMeshActor->StaticMeshComponent;
				if( CurStaticMeshComponent->HiddenEditor )
				{
					continue;
				}

				bCurActorIsValid = TRUE;

				CurStaticMesh = StaticMeshActor->StaticMeshComponent->StaticMesh;
				bOldCollideActorsComponent = StaticMeshActor->StaticMeshComponent->CollideActors;

				if( CurStaticMesh->kDOPTree.Triangles.Num() == 0 )
				{
					// This mesh does not have a valid kDOP tree so the line check will fail. 
					// We will build a temporary one so we can paint on meshes without collision
					bHasKDOPTree = FALSE;

					// See if the mesh already has a kDOP tree we previously built.  
					kDOPTreeType* Tree = StaticMeshToTempkDOPMap.Find( CurStaticMesh );
					if( !Tree )
					{
						// We need to build a new kDOP tree
						kDOPTreeType kDOPTree;
						BuildTempKDOPTree( CurStaticMesh, kDOPTree);
						// Set the mapping between the current static mesh and the new kDOP tree.  
						// This will avoid having to rebuild the tree over and over
						StaticMeshToTempkDOPMap.Set( CurStaticMesh, kDOPTree );
					}
				}

				// Line check requires that the static mesh component and its owner are both marked to collide with actors, so
				// here we temporarily force the collision enabled if it's not already. Note we do not use SetCollision because
				// there is no reason to incur the cost of component reattaches when the collision value will be restored back to
				// what it was momentarily.
				if ( !bOldCollideActors || !bOldCollideActorsComponent )
				{
					CurActor->bCollideActors = TRUE;
					CurStaticMeshComponent->CollideActors = TRUE;
				}
			}
			// If this wasn't a static mesh actor, is it a dynamic static mesh actor?
			else
			{
				ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( CurActor );
				if ( DynamicSMActor != NULL &&
					 DynamicSMActor->StaticMeshComponent != NULL &&
					 DynamicSMActor->StaticMeshComponent->StaticMesh != NULL )
				{

					// ignore Actors who's static meshes are hidden in the editor
					CurStaticMeshComponent = DynamicSMActor->StaticMeshComponent;
					if( CurStaticMeshComponent->HiddenEditor )
					{
						continue;
					}

					bCurActorIsValid = TRUE;

					CurStaticMesh = DynamicSMActor->StaticMeshComponent->StaticMesh;
					bOldCollideActorsComponent = DynamicSMActor->StaticMeshComponent->CollideActors;

					if( CurStaticMesh->kDOPTree.Triangles.Num() == 0 )
					{
						// This mesh does not have a valid kDOP tree so the line check will fail. 
						// We will build a temporary one so we can paint on meshes without collision
						bHasKDOPTree = FALSE;

						// See if the mesh already has a kDOP tree we previously built.  
						kDOPTreeType* Tree = StaticMeshToTempkDOPMap.Find( CurStaticMesh );
						if( !Tree )
						{
							// We need to build a new kDOP tree
							kDOPTreeType kDOPTree;
							BuildTempKDOPTree( CurStaticMesh, kDOPTree);
							// Set the mapping between the current static mesh and the new kDOP tree.  
							// This will avoid having to rebuild the tree over and over
							StaticMeshToTempkDOPMap.Set( CurStaticMesh, kDOPTree );
						}
					}

					// Line check requires that the static mesh component and its owner are both marked to collide with actors, so
					// here we temporarily force the collision enabled if it's not already. Note we do not use SetCollision because
					// there is no reason to incur the cost of component reattaches when the collision value will be restored back to
					// what it was momentarily.
					if ( !bOldCollideActors || !bOldCollideActorsComponent )
					{
						CurActor->bCollideActors = TRUE;
						CurStaticMeshComponent->CollideActors = TRUE;
					}
				}
			}

			if ((InPaintAction == EMeshPaintAction::Fill) && (CurStaticMesh))
			{
				PaintableActors.AddItem( CurActor );
				continue;
			}
			else if ((InPaintAction == EMeshPaintAction::PushInstanceColorsToMesh) && CurStaticMesh && (SelectedActors.Num() == 1))
			{
				PaintableActors.AddItem( CurActor );
				break;
			}

			// If the actor was a static mesh or dynamic static mesh actor, it's potentially valid for mesh painting
			if ( bCurActorIsValid )
			{
				ValidSelectedActors.AddItem( CurActor );

				// Ray trace
				FCheckResult TestTraceResult( 1.0f );
				const FVector TraceExtent( 0.0f, 0.0f, 0.0f );

				kDOPTreeType OldTree;

				if( !bHasKDOPTree )
				{
					// Temporarily replace the current static meshes kDOP tree with one we built.  This will ensure we get good results from the line check
					OldTree = CurStaticMesh->kDOPTree;
					kDOPTreeType* kDOPTree = StaticMeshToTempkDOPMap.Find( CurStaticMesh );
					if( kDOPTree )
					{
						CurStaticMesh->kDOPTree = *kDOPTree;
					}
				}

				if( !CurActor->ActorLineCheck( TestTraceResult, TraceEnd, TraceStart, TraceExtent, TRACE_ComplexCollision | TRACE_Accurate | TRACE_Visible ) )
				{
					// Find the closest impact
					if( BestTraceResult.Actor == NULL || ( TestTraceResult.Time < BestTraceResult.Time ) )
					{
						BestTraceResult = TestTraceResult;
					}
				}

				if( !bHasKDOPTree )
				{
					// Replace the kDOP tree we built with static meshes actual one.
					CurStaticMesh->kDOPTree = OldTree;
				}
				
				// Restore the old collision values for the actor and component if they were forcibly changed to support mesh painting
				if ( !bOldCollideActors || !bOldCollideActorsComponent )
				{
					CurActor->bCollideActors = bOldCollideActors;
					CurStaticMeshComponent->CollideActors = bOldCollideActorsComponent;
				}

			}
		}

		if( BestTraceResult.Actor != NULL )
		{
			// If we're using texture paint, just use the best trace result we found as we currently only
			// support painting a single mesh at a time in that mode.
			if( FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture )
			{
				PaintableActors.AddItem( BestTraceResult.Actor );
			}
			else
			{
				FBox BrushBounds = FBox::BuildAABB( BestTraceResult.Location, FVector( BrushRadius * 1.25f, BrushRadius * 1.25f, BrushRadius * 1.25f ) );

				// Vertex paint mode, so we want all valid actors overlapping the brush
				for( INT CurActorIndex = 0; CurActorIndex < ValidSelectedActors.Num(); ++CurActorIndex )
				{
					AActor* CurValidActor = ValidSelectedActors( CurActorIndex );

					const FBox ActorBounds = CurValidActor->GetComponentsBoundingBox( TRUE );
					
					if( ActorBounds.Intersect( BrushBounds ) )
					{
						// OK, this mesh potentially overlaps the brush!
						PaintableActors.AddItem( CurValidActor );
					}
				}
			}
		}
	}

	// Iterate over the selected static meshes under the cursor and paint them!
	for( INT CurActorIndex = 0; CurActorIndex < PaintableActors.Num(); ++CurActorIndex )
	{
		AStaticMeshActor* HitStaticMeshActor = Cast<AStaticMeshActor>( PaintableActors( CurActorIndex ) );
		ADynamicSMActor* HitDynamicSMActor = Cast<ADynamicSMActor> ( PaintableActors( CurActorIndex ) );
		check( HitStaticMeshActor || HitDynamicSMActor );

		UStaticMeshComponent* StaticMeshComponent = HitStaticMeshActor ? HitStaticMeshActor->StaticMeshComponent : HitDynamicSMActor->StaticMeshComponent;
		UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh;

		check( StaticMesh->LODModels.Num() > PaintingMeshLODIndex );
		FStaticMeshRenderData& LODModel = StaticMeshComponent->StaticMesh->LODModels( PaintingMeshLODIndex );
		
		// Precache mesh -> world transform
		const FMatrix ActorToWorldMatrix = StaticMeshComponent->LocalToWorld;

		// Brush properties
		const FLOAT BrushDepth = FMeshPaintSettings::Get().BrushRadius;	// NOTE: Actually half of the total depth (like a radius)
		const FLOAT BrushFalloffAmount = FMeshPaintSettings::Get().BrushFalloffAmount;
		const FLinearColor BrushColor = ((InPaintAction == EMeshPaintAction::Paint) || (InPaintAction == EMeshPaintAction::Fill))? FMeshPaintSettings::Get().PaintColor : FMeshPaintSettings::Get().EraseColor;

		// NOTE: We square the brush strength to maximize slider precision in the low range
		const FLOAT BrushStrength =
			FMeshPaintSettings::Get().BrushStrength * FMeshPaintSettings::Get().BrushStrength *
			InStrengthScale;

		// Display settings
		const FLOAT VisualBiasDistance = 0.15f;
		const FLOAT NormalLineSize( BrushRadius * 0.35f );	// Make the normal line length a function of brush size
		const FLinearColor NormalLineColor( 0.3f, 1.0f, 0.3f );
		const FLinearColor BrushCueColor = bIsPainting ? FLinearColor( 1.0f, 1.0f, 0.3f ) : FLinearColor( 0.3f, 1.0f, 0.3f );
		const FLinearColor InnerBrushCueColor = bIsPainting ? FLinearColor( 0.5f, 0.5f, 0.1f ) : FLinearColor( 0.1f, 0.5f, 0.1f );

		FVector BrushXAxis, BrushYAxis;
		BestTraceResult.Normal.FindBestAxisVectors( BrushXAxis, BrushYAxis );
		const FVector BrushVisualPosition = BestTraceResult.Location + BestTraceResult.Normal * VisualBiasDistance;

		// Compute the camera position in actor space.  We need this later to check for
		// backfacing triangles.
		const FVector ActorSpaceCameraPosition( ActorToWorldMatrix.InverseTransformFVector( InCameraOrigin ) );
		const FVector ActorSpaceBrushPosition( ActorToWorldMatrix.InverseTransformFVector( BestTraceResult.Location ) );
		
		// @todo MeshPaint: Input vector doesn't work well with non-uniform scale
		const FLOAT ActorSpaceBrushRadius = ActorToWorldMatrix.InverseTransformNormal( FVector( BrushRadius, 0.0f, 0.0f ) ).Size();
		const FLOAT ActorSpaceSquaredBrushRadius = ActorSpaceBrushRadius * ActorSpaceBrushRadius;


		if( PDI != NULL )
		{
			// Draw brush circle
			const INT NumCircleSides = 64;
			DrawCircle( PDI, BrushVisualPosition, BrushXAxis, BrushYAxis, BrushCueColor, BrushRadius, NumCircleSides, SDPG_World );

			// Also draw the inner brush radius
			const FLOAT InnerBrushRadius = BrushRadius - BrushFalloffAmount * BrushRadius;
			DrawCircle( PDI, BrushVisualPosition, BrushXAxis, BrushYAxis, InnerBrushCueColor, InnerBrushRadius, NumCircleSides, SDPG_World );

			// If we just started painting then also draw a little brush effect
			if( bIsPainting )
			{
				const FLOAT EffectDuration = 0.2f;

				const DOUBLE CurTime = appSeconds();
				const FLOAT TimeSinceStartedPainting = (FLOAT)( CurTime - PaintingStartTime );
				if( TimeSinceStartedPainting <= EffectDuration )
				{
					// Invert the effect if we're currently erasing
					FLOAT EffectAlpha = TimeSinceStartedPainting / EffectDuration;
					if( InPaintAction == EMeshPaintAction::Erase )
					{
						EffectAlpha = 1.0f - EffectAlpha;
					}

					const FLinearColor EffectColor( 0.1f + EffectAlpha * 0.4f, 0.1f + EffectAlpha * 0.4f, 0.1f + EffectAlpha * 0.4f );
					const FLOAT EffectRadius = BrushRadius * EffectAlpha * EffectAlpha;	// Squared curve here (looks more interesting)
					DrawCircle( PDI, BrushVisualPosition, BrushXAxis, BrushYAxis, EffectColor, EffectRadius, NumCircleSides, SDPG_World );
				}
			}

			// Draw trace surface normal
			const FVector NormalLineEnd( BrushVisualPosition + BestTraceResult.Normal * NormalLineSize );
			PDI->DrawLine( BrushVisualPosition, NormalLineEnd, NormalLineColor, SDPG_World );
		}



		// Mesh paint settings
		FMeshPaintParameters Params;
		{
			Params.PaintMode = FMeshPaintSettings::Get().PaintMode;
			Params.PaintAction = InPaintAction;
			Params.BrushPosition = BestTraceResult.Location;
			Params.BrushNormal = BestTraceResult.Normal;
			Params.BrushColor = BrushColor;
			Params.SquaredBrushRadius = BrushRadius * BrushRadius;
			Params.BrushRadialFalloffRange = BrushFalloffAmount * BrushRadius;
			Params.InnerBrushRadius = BrushRadius - Params.BrushRadialFalloffRange;
			Params.BrushDepth = BrushDepth;
			Params.BrushDepthFalloffRange = BrushFalloffAmount * BrushDepth;
			Params.InnerBrushDepth = BrushDepth - Params.BrushDepthFalloffRange;
			Params.BrushStrength = BrushStrength;
			Params.BrushToWorldMatrix = FMatrix( BrushXAxis, BrushYAxis, Params.BrushNormal, Params.BrushPosition );
			Params.bWriteRed = FMeshPaintSettings::Get().bWriteRed;
			Params.bWriteGreen = FMeshPaintSettings::Get().bWriteGreen;
			Params.bWriteBlue = FMeshPaintSettings::Get().bWriteBlue;
			Params.bWriteAlpha = FMeshPaintSettings::Get().bWriteAlpha;
			Params.TotalWeightCount = FMeshPaintSettings::Get().TotalWeightCount;

			// Select texture weight index based on whether or not we're painting or erasing
			{
				const INT PaintWeightIndex = 
					( InPaintAction == EMeshPaintAction::Paint ) ? FMeshPaintSettings::Get().PaintWeightIndex : FMeshPaintSettings::Get().EraseWeightIndex;

				// Clamp the weight index to fall within the total weight count
				Params.PaintWeightIndex = Clamp( PaintWeightIndex, 0, Params.TotalWeightCount - 1 );
			}

			// @todo MeshPaint: Ideally we would default to: TexturePaintingStaticMeshComponent->StaticMesh->LightMapCoordinateIndex
			//		Or we could indicate in the GUI which channel is the light map set (button to set it?)
			Params.UVChannel = FMeshPaintSettings::Get().UVChannel;
		}

		// Are we actually applying paint here?
		const UBOOL bShouldApplyPaint = (bIsPainting && !bVisualCueOnly) || 
			(InPaintAction == EMeshPaintAction::Fill) || 
			(InPaintAction == EMeshPaintAction::PushInstanceColorsToMesh);

		if( FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::VertexColors )
		{
			// Painting vertex colors
			PaintMeshVertices( StaticMeshComponent, Params, bShouldApplyPaint, LODModel, ActorSpaceCameraPosition, ActorToWorldMatrix, PDI, VisualBiasDistance);

		}
		else
		{
			// Painting textures
			PaintMeshTexture( StaticMeshComponent, Params, bShouldApplyPaint, LODModel, ActorSpaceCameraPosition, ActorToWorldMatrix, ActorSpaceSquaredBrushRadius, ActorSpaceBrushPosition );

		}
	}

	bAnyPaintAbleActorsUnderCursor = (PaintableActors.Num() > 0);
}



/** Paints mesh vertices */
void FEdModeMeshPaint::PaintMeshVertices( 
	UStaticMeshComponent* StaticMeshComponent, 
	const FMeshPaintParameters& Params, 
	const UBOOL bShouldApplyPaint, 
	FStaticMeshRenderData& LODModel, 
	const FVector& ActorSpaceCameraPosition, 
	const FMatrix& ActorToWorldMatrix, 
	FPrimitiveDrawInterface* PDI, 
	const FLOAT VisualBiasDistance)
{
	const UBOOL bOnlyFrontFacing = FMeshPaintSettings::Get().bOnlyFrontFacingTriangles;
	const UBOOL bUsingInstancedVertexColors = ( FMeshPaintSettings::Get().VertexPaintTarget == EMeshVertexPaintTarget::ComponentInstance ) && (Params.PaintAction != EMeshPaintAction::PushInstanceColorsToMesh);

	const FLOAT InfluencedVertexCuePointSize = 3.5f;

	UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh;


	// Paint the mesh
	UINT NumVerticesInfluencedByBrush = 0;
	{
		TScopedPointer< FStaticMeshComponentReattachContext > MeshComponentReattachContext;
		TScopedPointer< FComponentReattachContext > ComponentReattachContext;


		FStaticMeshComponentLODInfo* InstanceMeshLODInfo = NULL;
		if( bUsingInstancedVertexColors)
		{
			if( bShouldApplyPaint )
			{
				// We're only changing instanced vertices on this specific mesh component, so we
				// only need to detach our mesh component
				ComponentReattachContext.Reset( new FComponentReattachContext( StaticMeshComponent ) );

				// Mark the mesh component as modified
				StaticMeshComponent->Modify();

				// Ensure LODData has enough entries in it, free not required.
				StaticMeshComponent->SetLODDataCount(PaintingMeshLODIndex + 1, StaticMeshComponent->LODData.Num());

				InstanceMeshLODInfo = &StaticMeshComponent->LODData( PaintingMeshLODIndex );

				// Destroy the instance vertex  color array if it doesn't fit
				if(InstanceMeshLODInfo->OverrideVertexColors
					&& InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices() != LODModel.NumVertices)
				{
					InstanceMeshLODInfo->ReleaseOverrideVertexColorsAndBlock();
				}

				// Destroy the cached paint data if it was for a mesh of a different size.
				if ( InstanceMeshLODInfo->OverrideVertexColors &&
					 InstanceMeshLODInfo->PaintedVertices.Num() != InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices() )
				{
					InstanceMeshLODInfo->PaintedVertices.Empty();
				}

				if(InstanceMeshLODInfo->OverrideVertexColors)
				{
					InstanceMeshLODInfo->BeginReleaseOverrideVertexColors();
					FlushRenderingCommands();
				}
				else
				{
					// Setup the instance vertex color array if we don't have one yet
					InstanceMeshLODInfo->OverrideVertexColors = new FColorVertexBuffer;

					if(LODModel.ColorVertexBuffer.GetNumVertices() >= LODModel.NumVertices)
					{
						// copy mesh vertex colors to the instance ones
						InstanceMeshLODInfo->OverrideVertexColors->InitFromColorArray(&LODModel.ColorVertexBuffer.VertexColor(0), LODModel.NumVertices);
					}
					else
					{
						UBOOL bConvertSRGB = FALSE;
						FColor FillColor = Params.BrushColor.ToFColor(bConvertSRGB);
						// Original mesh didn't have any colors, so just use a default color
						InstanceMeshLODInfo->OverrideVertexColors->InitFromSingleColor(FColor(255, 255, 255), LODModel.NumVertices);
					}

					// The instance vertex color byte count has changed so tell the mesh paint window
					// to refresh it's properties
#if WITH_MANAGED_CODE
					if( MeshPaintWindow != NULL )
					{
						MeshPaintWindow->RefreshAllProperties();
					}
#endif
				}
				// See if the component has to cache its mesh vertex positions associated with override colors
				StaticMeshComponent->CachePaintedDataIfNecessary();
				StaticMeshComponent->VertexPositionVersionNumber = StaticMesh->VertexPositionVersionNumber;
			}
			else
			{
				if( StaticMeshComponent->LODData.Num() > PaintingMeshLODIndex )
				{
					InstanceMeshLODInfo = &StaticMeshComponent->LODData( PaintingMeshLODIndex );
				}
			}
		}
		else
		{
			if( bShouldApplyPaint )
			{
				// We're changing the mesh itself, so ALL static mesh components in the scene will need
				// to be detached for this (and reattached afterwards.)
				MeshComponentReattachContext.Reset( new FStaticMeshComponentReattachContext( StaticMesh ) );

				// Dirty the mesh
				StaticMesh->Modify();

				// Add to our modified list
				ModifiedStaticMeshes.AddUniqueItem( StaticMesh );

				// Release the static mesh's resources.
				StaticMesh->ReleaseResources();

				// Flush the resource release commands to the rendering thread to ensure that the build doesn't occur while a resource is still
				// allocated, and potentially accessing the UStaticMesh.
				StaticMesh->ReleaseResourcesFence.Wait();

#if WITH_MANAGED_CODE
				// The staticmesh will be modified, we refresh to make sure the save icon enables itself if needed.
				if( MeshPaintWindow != NULL )
				{
					MeshPaintWindow->RefreshAllProperties();
				}
#endif
			}
		}



		// Paint the mesh vertices
		{
			if (Params.PaintAction == EMeshPaintAction::Fill)
			{
				//flood fill
				UBOOL bConvertSRGB = FALSE;
				FColor FillColor = Params.BrushColor.ToFColor(bConvertSRGB);
				FColor NewMask = FColor(Params.bWriteRed ? 255 : 0, Params.bWriteGreen ? 255 : 0, Params.bWriteBlue ? 255 : 0, Params.bWriteAlpha ? 255 : 0);
				FColor KeepMaskColor (~NewMask.DWColor());

				FColor MaskedFillColor = FillColor;
				MaskedFillColor.R &= NewMask.R;
				MaskedFillColor.G &= NewMask.G;
				MaskedFillColor.B &= NewMask.B;
				MaskedFillColor.A &= NewMask.A;

				//make sure there is room if we're painting on the source mesh
				if( !bUsingInstancedVertexColors && LODModel.ColorVertexBuffer.GetNumVertices() == 0 )
				{
					// Mesh doesn't have a color vertex buffer yet!  We'll create one now.
					LODModel.ColorVertexBuffer.InitFromSingleColor(FColor( 255, 255, 255, 255), LODModel.NumVertices);
				}

				for (UINT ColorIndex = 0; ColorIndex < LODModel.NumVertices; ++ColorIndex)
				{
					FColor CurrentColor;
					if( bUsingInstancedVertexColors )
					{
						check(InstanceMeshLODInfo->OverrideVertexColors);
						check((UINT)ColorIndex < InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices());

						CurrentColor = InstanceMeshLODInfo->OverrideVertexColors->VertexColor( ColorIndex );
					}
					else
					{
						CurrentColor = LODModel.ColorVertexBuffer.VertexColor( ColorIndex );
					}

					CurrentColor.R &= KeepMaskColor.R;
					CurrentColor.G &= KeepMaskColor.G;
					CurrentColor.B &= KeepMaskColor.B;
					CurrentColor.A &= KeepMaskColor.A;
					CurrentColor += MaskedFillColor;

					if( bUsingInstancedVertexColors )
					{
						check( InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices() == InstanceMeshLODInfo->PaintedVertices.Num() );
						InstanceMeshLODInfo->OverrideVertexColors->VertexColor( ColorIndex ) = CurrentColor;
						InstanceMeshLODInfo->PaintedVertices( ColorIndex ).Color = CurrentColor;
					}
					else
					{
						LODModel.ColorVertexBuffer.VertexColor( ColorIndex ) = CurrentColor;
					}
				}
				GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
			}
			else if (Params.PaintAction == EMeshPaintAction::PushInstanceColorsToMesh)
			{
				InstanceMeshLODInfo = &StaticMeshComponent->LODData( PaintingMeshLODIndex );

				UINT LODColorNum = LODModel.ColorVertexBuffer.GetNumVertices();
				UINT InstanceColorNum = InstanceMeshLODInfo->OverrideVertexColors ? InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices() : 0;

				if ((InstanceColorNum > 0) && 
					((LODColorNum == InstanceColorNum) || (LODColorNum == 0)))
				{
					LODModel.ColorVertexBuffer.InitFromColorArray(&InstanceMeshLODInfo->OverrideVertexColors->VertexColor(0), InstanceColorNum);
				}

				RemoveInstanceVertexColors();

				GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
			}
			else
			{
				// @todo MeshPaint: Use a spatial database to reduce the triangle set here (kdop)


				// Make sure we're dealing with triangle lists
				const INT NumIndexBufferIndices = LODModel.IndexBuffer.Indices.Num();
				check( NumIndexBufferIndices % 3 == 0 );

				// We don't want to paint the same vertex twice and many vertices are shared between
				// triangles, so we use a set to track unique front-facing vertex indices
				static TSet< INT > FrontFacingVertexIndices;
				FrontFacingVertexIndices.Empty( NumIndexBufferIndices );


				// For each triangle in the mesh
				const INT NumTriangles = NumIndexBufferIndices / 3;
				for( INT TriIndex = 0; TriIndex < NumTriangles; ++TriIndex )
				{
					// Grab the vertex indices and points for this triangle
					INT VertexIndices[ 3 ];
					FVector TriVertices[ 3 ];
					for( INT TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum )
					{
						VertexIndices[ TriVertexNum ] = LODModel.IndexBuffer.Indices( TriIndex * 3 + TriVertexNum );
						TriVertices[ TriVertexNum ] = LODModel.PositionVertexBuffer.VertexPosition( VertexIndices[ TriVertexNum ] );
					}

					// Check to see if the triangle is front facing
					FVector TriangleNormal = ( TriVertices[ 1 ] - TriVertices[ 0 ] ^ TriVertices[ 2 ] - TriVertices[ 0 ] ).SafeNormal();
					const FLOAT SignedPlaneDist = FPointPlaneDist( ActorSpaceCameraPosition, TriVertices[ 0 ], TriangleNormal );
					if( !bOnlyFrontFacing || SignedPlaneDist < 0.0f )
					{
						FrontFacingVertexIndices.Add( VertexIndices[ 0 ] );
						FrontFacingVertexIndices.Add( VertexIndices[ 1 ] );
						FrontFacingVertexIndices.Add( VertexIndices[ 2 ] );
					}
				}


				for( TSet< INT >::TConstIterator CurIndexIt( FrontFacingVertexIndices ); CurIndexIt != NULL; ++CurIndexIt )
				{
					// Grab the mesh vertex and transform it to world space
					const INT VertexIndex = *CurIndexIt;
					const FVector& ModelSpaceVertexPosition = LODModel.PositionVertexBuffer.VertexPosition( VertexIndex );
					FVector WorldSpaceVertexPosition = ActorToWorldMatrix.TransformFVector( ModelSpaceVertexPosition );

					FColor OriginalVertexColor = FColor( 255, 255, 255 );

					// Grab vertex color (read/write)
					if( bUsingInstancedVertexColors )
					{
						if( InstanceMeshLODInfo 
						&& InstanceMeshLODInfo->OverrideVertexColors
						&& InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices() == LODModel.NumVertices )
						{
							// Actor mesh component LOD
							OriginalVertexColor = InstanceMeshLODInfo->OverrideVertexColors->VertexColor( VertexIndex );
						}
					}
					else
					{
						// Static mesh
						if( bShouldApplyPaint )
						{
							if( LODModel.ColorVertexBuffer.GetNumVertices() == 0 )
							{
								// Mesh doesn't have a color vertex buffer yet!  We'll create one now.
								LODModel.ColorVertexBuffer.InitFromSingleColor(FColor( 255, 255, 255, 255), LODModel.NumVertices);

								// @todo MeshPaint: Make sure this is the best place to do this
								BeginInitResource( &LODModel.ColorVertexBuffer );
							}
						}

						if( LODModel.ColorVertexBuffer.GetNumVertices() > 0 )
						{
							check( (INT)LODModel.ColorVertexBuffer.GetNumVertices() > VertexIndex );
							OriginalVertexColor = LODModel.ColorVertexBuffer.VertexColor( VertexIndex );
						}
					}


					// Paint the vertex!
					FColor NewVertexColor = OriginalVertexColor;
					UBOOL bVertexInRange = FALSE;
					{
						FColor PaintedVertexColor = OriginalVertexColor;
						bVertexInRange = PaintVertex( WorldSpaceVertexPosition, Params, bShouldApplyPaint, PaintedVertexColor );
						if( bShouldApplyPaint )
						{
							NewVertexColor = PaintedVertexColor;
						}
					}


					if( bVertexInRange )
					{
						++NumVerticesInfluencedByBrush;

						// Update the mesh!
						if( bShouldApplyPaint )
						{
							if( bUsingInstancedVertexColors )
							{
								check(InstanceMeshLODInfo->OverrideVertexColors);
								check((UINT)VertexIndex < InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices());
								check( InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices() == InstanceMeshLODInfo->PaintedVertices.Num() );

								InstanceMeshLODInfo->OverrideVertexColors->VertexColor( VertexIndex ) = NewVertexColor;
								InstanceMeshLODInfo->PaintedVertices( VertexIndex ).Color = NewVertexColor;
							}
							else
							{
								LODModel.ColorVertexBuffer.VertexColor( VertexIndex ) = NewVertexColor;
							}
						}


						// Draw vertex visual cue
						if( PDI != NULL )
						{
							const FLinearColor InfluencedVertexCueColor( NewVertexColor );
							const FVector VertexVisualPosition = WorldSpaceVertexPosition + Params.BrushNormal * VisualBiasDistance;
							PDI->DrawPoint( VertexVisualPosition, InfluencedVertexCueColor, InfluencedVertexCuePointSize, SDPG_World );
						}
					}
				} 
			}
		}

		if( bShouldApplyPaint )
		{
			if( bUsingInstancedVertexColors )
			{
				BeginInitResource(InstanceMeshLODInfo->OverrideVertexColors);
			}
			else
			{
				// Reinitialize the static mesh's resources.
				StaticMesh->InitResources();
			}
		}
	}


	// Were any vertices in the brush's influence?
	if( NumVerticesInfluencedByBrush > 0 )
	{
		// Also paint raw vertices
		const INT RawTriangleCount = LODModel.RawTriangles.GetElementCount();
		FStaticMeshTriangle* RawTriangleData =
			(FStaticMeshTriangle*)LODModel.RawTriangles.Lock( bShouldApplyPaint ? LOCK_READ_WRITE : LOCK_READ_ONLY );

		// @todo MeshPaint: Ideally we could reduce the triangle set here using a spatial database
		for( INT RawTriangleIndex = 0; RawTriangleIndex < RawTriangleCount; ++RawTriangleIndex )
		{
			FStaticMeshTriangle& CurRawTriangle = RawTriangleData[ RawTriangleIndex ];


			// Check to see if the triangle is front facing
			FVector TriangleNormal = ( CurRawTriangle.Vertices[ 1 ] - CurRawTriangle.Vertices[ 0 ] ^ CurRawTriangle.Vertices[ 2 ] - CurRawTriangle.Vertices[ 0 ] ).SafeNormal();
			const FLOAT SignedPlaneDist = FPointPlaneDist( ActorSpaceCameraPosition, CurRawTriangle.Vertices[ 0 ], TriangleNormal );
			if( !bOnlyFrontFacing || SignedPlaneDist < 0.0f )
			{
				// For each vertex in this triangle
				for( INT CurTriVertexIndex = 0; CurTriVertexIndex < 3; ++CurTriVertexIndex )
				{
					const FVector& ActorSpaceVertexPosition = CurRawTriangle.Vertices[ CurTriVertexIndex ];
					FVector WorldSpaceVertexPosition = ActorToWorldMatrix.TransformFVector( ActorSpaceVertexPosition );

					// Grab vertex color (read/write)
					FColor& VertexColor = CurRawTriangle.Colors[ CurTriVertexIndex ];


					// Paint the vertex!
					FColor NewVertexColor = VertexColor;
					UBOOL bVertexInRange = PaintVertex( WorldSpaceVertexPosition, Params, bShouldApplyPaint && !bUsingInstancedVertexColors, NewVertexColor );


					if( bVertexInRange )
					{
						// Should we actually update the color in the static mesh's raw vertex array?  We
						// only want to do this when configured to edit the actual static mesh (rather than
						// instanced vertex color data on the actor's component.)
						if( bShouldApplyPaint && !bUsingInstancedVertexColors )
						{
							VertexColor = NewVertexColor;
						}

					}
				}
			}
		}

		LODModel.RawTriangles.Unlock();
	}
}



/** Paints mesh texture */
void FEdModeMeshPaint::PaintMeshTexture( UStaticMeshComponent* StaticMeshComponent, const FMeshPaintParameters& Params, const UBOOL bShouldApplyPaint, FStaticMeshRenderData& LODModel, const FVector& ActorSpaceCameraPosition, const FMatrix& ActorToWorldMatrix, const FLOAT ActorSpaceSquaredBrushRadius, const FVector& ActorSpaceBrushPosition )
{
	UTexture2D* TargetTexture2D = GetSelectedTexture();
	
	// No reason to continue if we dont have a target texture;
	if( TargetTexture2D == NULL )
	{
		return;
	}

	const UBOOL bOnlyFrontFacing = FMeshPaintSettings::Get().bOnlyFrontFacingTriangles;
	if( bShouldApplyPaint )
	{
		// @todo MeshPaint: Use a spatial database to reduce the triangle set here (kdop)



		// Make sure we're dealing with triangle lists
		const UINT NumIndexBufferIndices = LODModel.IndexBuffer.Indices.Num();
		check( NumIndexBufferIndices % 3 == 0 );
		const UINT NumTriangles = NumIndexBufferIndices / 3;

		// Keep a list of front-facing triangles that are within a reasonable distance to the brush
		static TArray< INT > InfluencedTriangles;
		InfluencedTriangles.Empty( NumTriangles );

		// Use a bit of distance bias to make sure that we get all of the overlapping triangles.  We
		// definitely don't want our brush to be cut off by a hard triangle edge
		const FLOAT SquaredRadiusBias = ActorSpaceSquaredBrushRadius * 0.025f;

		INT NumElements = StaticMeshComponent->GetNumElements();
		
		PaintTexture2DData* TextureData = GetPaintTargetData( TargetTexture2D );

		// Store info that tells us if the element material uses our target texture so we don't have to do a usestexture() call for each tri
		TArray< UBOOL > ElementUsesTargetTexture;
		ElementUsesTargetTexture.AddZeroed( NumElements );
		for ( INT ElementIndex = 0; ElementIndex < NumElements; ElementIndex++ )
		{
			ElementUsesTargetTexture( ElementIndex ) = FALSE;

			// @todo MeshPaint: if LODs can use different materials/textures then this will cause us problems
			UMaterialInterface* ElementMat = StaticMeshComponent->GetElementMaterial( ElementIndex );
			if( ElementMat != NULL )
			{
				
				ElementUsesTargetTexture( ElementIndex ) |=  ElementMat->UsesTexture( TargetTexture2D );
				
				if( ElementUsesTargetTexture( ElementIndex ) == FALSE && TextureData != NULL && TextureData->PaintRenderTargetTexture != NULL)
				{
					// If we didn't get a match on our selected texture, we'll check to see if the the material uses a
					//  render target texture override that we put on during painting.
					ElementUsesTargetTexture( ElementIndex ) |=  ElementMat->UsesTexture( TextureData->PaintRenderTargetTexture );
				}
			}
		}

		// For each triangle in the mesh
		for( UINT TriIndex = 0; TriIndex < NumTriangles; ++TriIndex )
		{
			// Grab the vertex indices and points for this triangle
			FVector TriVertices[ 3 ];
			for( INT TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum )
			{
				const INT VertexIndex = LODModel.IndexBuffer.Indices( TriIndex * 3 + TriVertexNum );
				TriVertices[ TriVertexNum ] = LODModel.PositionVertexBuffer.VertexPosition( VertexIndex );
			}


			// Check to see if the triangle is front facing
			FVector TriangleNormal = ( TriVertices[ 1 ] - TriVertices[ 0 ] ^ TriVertices[ 2 ] - TriVertices[ 0 ] ).SafeNormal();
			const FLOAT SignedPlaneDist = FPointPlaneDist( ActorSpaceCameraPosition, TriVertices[ 0 ], TriangleNormal );
			if( !bOnlyFrontFacing || SignedPlaneDist < 0.0f )
			{
				// Compute closest point to the triangle in actor space
				// @todo MeshPaint: Perform AABB test first to speed things up?
				FVector ClosestPointOnTriangle = ClosestPointOnTriangleToPoint( ActorSpaceBrushPosition, TriVertices[ 0 ], TriVertices[ 1 ], TriVertices[ 2 ] );
				FVector ActorSpaceBrushToMeshVector = ActorSpaceBrushPosition - ClosestPointOnTriangle;
				const FLOAT ActorSpaceSquaredDistanceToBrush = ActorSpaceBrushToMeshVector.SizeSquared();
				if( ActorSpaceSquaredDistanceToBrush <= ( ActorSpaceSquaredBrushRadius + SquaredRadiusBias ) )
				{

					// At least one triangle vertex was influenced.
					bool bAddTri = FALSE;

					// Check to see if the sub-element that this triangle belongs to actually uses our paint target texture in its material
					for (INT ElementIndex = 0; ElementIndex < NumElements; ElementIndex++)
					{
						FStaticMeshElement& Element = LODModel.Elements( ElementIndex );
						
						
						if( ( TriIndex >= Element.FirstIndex / 3 ) && 
							( TriIndex < Element.FirstIndex / 3 + Element.NumTriangles ) )
						{
							
							// The triangle belongs to this element, now we need to check to see if the element material uses our target texture.
							if( TargetTexture2D != NULL && ElementUsesTargetTexture( ElementIndex ) == TRUE)
							{
								bAddTri = TRUE;
							}

							// Triangles can only be part of one element so we do not need to continue to other elements.
							break;
						}

					}

					if( bAddTri == TRUE )
					{
						InfluencedTriangles.AddItem( TriIndex );
					}
				}
			}
		}


		{

			if( TexturePaintingStaticMeshComponent != NULL && TexturePaintingStaticMeshComponent != StaticMeshComponent )
			{
				// Mesh has changed, so finish up with our previous texture
				FinishPaintingTexture();
				bIsPainting = FALSE;
			}

			if( TexturePaintingStaticMeshComponent == NULL )
			{
				StartPaintingTexture( StaticMeshComponent );
			}

			if( TexturePaintingStaticMeshComponent != NULL )
			{
				PaintTexture( Params, InfluencedTriangles, ActorToWorldMatrix );
			}
		}
	}
}




/** Starts painting a texture */
void FEdModeMeshPaint::StartPaintingTexture( UStaticMeshComponent* InStaticMeshComponent )
{
	check( InStaticMeshComponent != NULL );
	check( TexturePaintingStaticMeshComponent == NULL );
	check( PaintingTexture2D == NULL );
	
	UTexture2D* Texture2D = GetSelectedTexture();
	if( Texture2D == NULL )
	{
		return;
	}

	UBOOL bStartedPainting = FALSE;
	PaintTexture2DData* TextureData = GetPaintTargetData( Texture2D );

	// Check all the materials on the mesh to see if the user texture is there
	INT MaterialIndex = 0;
	UMaterialInterface* MaterialToCheck = InStaticMeshComponent->GetMaterial( MaterialIndex, PaintingMeshLODIndex );
	while( MaterialToCheck != NULL )
	{
		UBOOL bIsTextureUsed = MaterialToCheck->UsesTexture( Texture2D, FALSE );

		if( !bIsTextureUsed && TextureData != NULL && TextureData->PaintRenderTargetTexture != NULL )
		{
			bIsTextureUsed = MaterialToCheck->UsesTexture( TextureData->PaintRenderTargetTexture );
		}

		if( bIsTextureUsed == TRUE && bStartedPainting == FALSE )
		{
			UBOOL bIsSourceTextureStreamedIn = Texture2D->IsFullyStreamedIn();

			if( !bIsSourceTextureStreamedIn )
			{
				// We found that this texture is used in one of the meshes materials but not fully loaded, we will
				//   attempt to fully stream in the texture before we try to do anything with it.
				Texture2D->SetForceMipLevelsToBeResident(30.0f);
				Texture2D->WaitForStreaming();

				// We do a quick sanity check to make sure it is streamed fully streamed in now.
				bIsSourceTextureStreamedIn = Texture2D->IsFullyStreamedIn();

			}

			if( bIsSourceTextureStreamedIn )
			{
				const INT TextureWidth = Texture2D->HasSourceArt() ? Texture2D->GetOriginalSurfaceWidth() : Texture2D->GetSurfaceWidth();
				const INT TextureHeight = Texture2D->HasSourceArt() ? Texture2D->GetOriginalSurfaceHeight() : Texture2D->GetSurfaceHeight();
				
				if( TextureData == NULL )
				{
					TextureData = AddPaintTargetData( Texture2D );
				}
				check( TextureData != NULL );

				// Create our paint render target texture
				if( TextureData->PaintRenderTargetTexture == NULL ||
					TextureData->PaintRenderTargetTexture->GetSurfaceWidth() != TextureWidth ||
					TextureData->PaintRenderTargetTexture->GetSurfaceHeight() != TextureHeight )
				{
					TextureData->PaintRenderTargetTexture = NULL;
					TextureData->PaintRenderTargetTexture = CastChecked<UTextureRenderTarget2D>( UObject::StaticConstructObject( UTextureRenderTarget2D::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Transient ) );
					const UBOOL bForceLinearGamma = TRUE;
					TextureData->PaintRenderTargetTexture->bUpdateImmediate = TRUE;
					TextureData->PaintRenderTargetTexture->Init( TextureWidth, TextureHeight, PF_A16B16G16R16, bForceLinearGamma );
		
					//Duplicate the texture we are painting and store it in the transient package. This texture is a backup of the data incase we want to revert before commiting.
					TextureData->PaintingTexture2DDuplicate = (UTexture2D*)GEditor->StaticDuplicateObject(Texture2D, Texture2D, UObject::GetTransientPackage(), *FString::Printf(TEXT("%s_TEMP"), *Texture2D->GetName()));

				}
				TextureData->PaintRenderTargetTexture->AddressX = Texture2D->AddressX;
				TextureData->PaintRenderTargetTexture->AddressY = Texture2D->AddressY;

				const INT BrushTargetTextureWidth = TextureWidth;
				const INT BrushTargetTextureHeight = TextureHeight;

				// Create the rendertarget used to store our paint delta
				if( BrushRenderTargetTexture == NULL ||
					BrushRenderTargetTexture->GetSurfaceWidth() != BrushTargetTextureWidth ||
					BrushRenderTargetTexture->GetSurfaceHeight() != BrushTargetTextureHeight )
				{
					BrushRenderTargetTexture = NULL;
					BrushRenderTargetTexture = CastChecked<UTextureRenderTarget2D>( UObject::StaticConstructObject( UTextureRenderTarget2D::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Transient ) );
					const UBOOL bForceLinearGamma = TRUE;
					BrushRenderTargetTexture->bUpdateImmediate = TRUE;
					BrushRenderTargetTexture->ClearColor = FLinearColor::Black;
					BrushRenderTargetTexture->Init( BrushTargetTextureWidth, BrushTargetTextureHeight, PF_A16B16G16R16, bForceLinearGamma );	
					BrushRenderTargetTexture->AddressX = TextureData->PaintRenderTargetTexture->AddressX;
					BrushRenderTargetTexture->AddressY = TextureData->PaintRenderTargetTexture->AddressY;

				}
				
				// Create the rendertarget used to store a mask for our paint delta area 
				if( BrushMaskRenderTargetTexture == NULL ||
					BrushMaskRenderTargetTexture->GetSurfaceWidth() != BrushTargetTextureWidth ||
					BrushMaskRenderTargetTexture->GetSurfaceHeight() != BrushTargetTextureHeight )
				{
					BrushMaskRenderTargetTexture = NULL;
					BrushMaskRenderTargetTexture = CastChecked<UTextureRenderTarget2D>( UObject::StaticConstructObject( UTextureRenderTarget2D::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Transient ) );
					const UBOOL bForceLinearGamma = TRUE;
					BrushMaskRenderTargetTexture->bUpdateImmediate = TRUE;
					BrushMaskRenderTargetTexture->ClearColor = FLinearColor::Black;
					BrushMaskRenderTargetTexture->Init( BrushTargetTextureWidth, BrushTargetTextureHeight, PF_A8R8G8B8, bForceLinearGamma );	
					BrushMaskRenderTargetTexture->AddressX = TextureData->PaintRenderTargetTexture->AddressX;
					BrushMaskRenderTargetTexture->AddressY = TextureData->PaintRenderTargetTexture->AddressY;
				}				

				const UBOOL bEnableSeamPainting = FMeshPaintSettings::Get().bEnableSeamPainting;
				if( bEnableSeamPainting )
				{

					// Create the rendertarget used to store a texture seam mask
					if( SeamMaskRenderTargetTexture == NULL ||
						SeamMaskRenderTargetTexture->GetSurfaceWidth() != TextureWidth ||
						SeamMaskRenderTargetTexture->GetSurfaceHeight() != TextureHeight )
					{
						SeamMaskRenderTargetTexture = NULL;
						SeamMaskRenderTargetTexture = CastChecked<UTextureRenderTarget2D>( UObject::StaticConstructObject( UTextureRenderTarget2D::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Transient ) );
						const UBOOL bForceLinearGamma = TRUE;
						SeamMaskRenderTargetTexture->bUpdateImmediate = TRUE;
						SeamMaskRenderTargetTexture->ClearColor = FLinearColor::Black;
						SeamMaskRenderTargetTexture->Init( BrushTargetTextureWidth, BrushTargetTextureHeight, PF_A8R8G8B8, bForceLinearGamma );	
						SeamMaskRenderTargetTexture->AddressX = TextureData->PaintRenderTargetTexture->AddressX;
						SeamMaskRenderTargetTexture->AddressY = TextureData->PaintRenderTargetTexture->AddressY;

					}
					bGenerateSeamMask = TRUE;
				}
				
				bStartedPainting = TRUE;				
			}
		}

		// @todo MeshPaint: Here we override the textures on the mesh with the render target.  The problem is that other meshes in the scene that use
		//    this texture do not get the override. Do we want to extend this to all other selected meshes or maybe even to all meshes in the scene?
		if( bIsTextureUsed == TRUE && bStartedPainting == TRUE && TextureData->PaintingMaterials.ContainsItem( MaterialToCheck ) == FALSE)
		{
			TextureData->PaintingMaterials.AddUniqueItem( MaterialToCheck ); 
			MaterialToCheck->OverrideTexture( Texture2D, TextureData->PaintRenderTargetTexture );
		}

		MaterialIndex++;
		MaterialToCheck = InStaticMeshComponent->GetMaterial( MaterialIndex, PaintingMeshLODIndex );
	}

	if( bStartedPainting )
	{
		TexturePaintingStaticMeshComponent = InStaticMeshComponent;

		check( Texture2D != NULL );
		PaintingTexture2D = Texture2D;
		// OK, now we need to make sure our render target is filled in with data
		SetupInitialRenderTargetData( TextureData->PaintingTexture2D, TextureData->PaintRenderTargetTexture );
	}

}



/** Paints on a texture */
void FEdModeMeshPaint::PaintTexture( const FMeshPaintParameters& InParams,
									 const TArray< INT >& InInfluencedTriangles,
									 const FMatrix& InActorToWorldMatrix )
{
	// We bail early if there are no influenced triangles
	if( InInfluencedTriangles.Num() <= 0 )
	{
		return;
	}

	FStaticMeshRenderData& LODModel = TexturePaintingStaticMeshComponent->StaticMesh->LODModels( PaintingMeshLODIndex );
	const UINT PaintUVCoordinateIndex = InParams.UVChannel;

	// Check to see if the UV set is available on the LOD model, if not then there is no point in continuing.
	if( PaintUVCoordinateIndex >= LODModel.VertexBuffer.GetNumTexCoords() )
	{
		// @todo MeshPaint: Do we want to give the user some sort of indication that the paint failed because the UV set is not available on the object?
		return;
	}

	PaintTexture2DData* TextureData = GetPaintTargetData( PaintingTexture2D );
	check( TextureData != NULL && TextureData->PaintRenderTargetTexture != NULL );

	const UBOOL bEnableSeamPainting = FMeshPaintSettings::Get().bEnableSeamPainting;
	const FMatrix WorldToBrushMatrix = InParams.BrushToWorldMatrix.Inverse();

	static TArray< FTexturePaintTriangleInfo > TriangleInfo;
	TriangleInfo.Empty( InInfluencedTriangles.Num() );

	// Store off info about our influenced triangles.
	for( INT CurIndex = 0; CurIndex < InInfluencedTriangles.Num(); ++CurIndex )
	{
		const INT TriIndex = InInfluencedTriangles( CurIndex );
		FTexturePaintTriangleInfo CurTriangle;

		FVector2D UVMin( 99999.9f, 99999.9f );
		FVector2D UVMax( -99999.9f, -99999.9f );
			 
		// Grab the vertex indices and points for this triangle
		for( INT TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum )
		{																		 
			const INT VertexIndex = LODModel.IndexBuffer.Indices( TriIndex * 3 + TriVertexNum );
			CurTriangle.TriVertices[ TriVertexNum ] = InActorToWorldMatrix.TransformFVector( LODModel.PositionVertexBuffer.VertexPosition( VertexIndex ) );
			CurTriangle.TriUVs[ TriVertexNum ] = LODModel.VertexBuffer.GetVertexUV( VertexIndex, PaintUVCoordinateIndex );

			// Update bounds
			FLOAT U = CurTriangle.TriUVs[ TriVertexNum ].X;
			FLOAT V = CurTriangle.TriUVs[ TriVertexNum ].Y;

			if( U < UVMin.X )
			{
				UVMin.X = U;
			}
			if( U > UVMax.X )
			{
				UVMax.X = U;
			}
			if( V < UVMin.Y )
			{
				UVMin.Y = V;
			}
			if( V > UVMax.Y )
			{
				UVMax.Y = V;
			}
		}

		// If the triangle lies entirely outside of the 0.0-1.0 range, we'll transpose it back
		FVector2D UVOffset( 0.0f, 0.0f );
		if( UVMax.X > 1.0f )
		{
			UVOffset.X = -appFloor( UVMin.X );
		}
		else if( UVMin.X < 0.0f )
		{
			UVOffset.X = 1.0f + appFloor( -UVMax.X );
		}
			
		if( UVMax.Y > 1.0f )
		{
			UVOffset.Y = -appFloor( UVMin.Y );
		}
		else if( UVMin.Y < 0.0f )
		{
			UVOffset.Y = 1.0f + appFloor( -UVMax.Y );
		}


		// Note that we "wrap" the texture coordinates here to handle the case where the user
		// is painting on a tiling texture, or with the UVs out of bounds.  Ideally all of the
		// UVs would be in the 0.0 - 1.0 range but sometimes content isn't setup that way.
		// @todo MeshPaint: Handle triangles that cross the 0.0-1.0 UV boundary?
		for( INT TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum )
		{
			CurTriangle.TriUVs[ TriVertexNum ].X += UVOffset.X;
			CurTriangle.TriUVs[ TriVertexNum ].Y += UVOffset.Y;

			// @todo: Need any half-texel offset adjustments here? Some info about offsets and MSAA here: http://drilian.com/2008/11/25/understanding-half-pixel-and-half-texel-offsets/
			// @todo: MeshPaint: Screen-space texture coords: http://diaryofagraphicsprogrammer.blogspot.com/2008/09/calculating-screen-space-texture.html
			CurTriangle.TrianglePoints[ TriVertexNum ].X = CurTriangle.TriUVs[ TriVertexNum ].X * TextureData->PaintRenderTargetTexture->GetSurfaceWidth();
			CurTriangle.TrianglePoints[ TriVertexNum ].Y = CurTriangle.TriUVs[ TriVertexNum ].Y * TextureData->PaintRenderTargetTexture->GetSurfaceHeight();
		}

		TriangleInfo.AddItem(CurTriangle);

	}

	// Copy the current image to the brush rendertarget texture.
	{
		check( BrushRenderTargetTexture != NULL );
		CopyTextureToRenderTargetTexture( TextureData->PaintRenderTargetTexture, BrushRenderTargetTexture );
	}

	// Grab the actual render target resource from the textures.  Note that we're absolutely NOT ALLOWED to
	// dereference these pointers.  We're just passing them along to other functions that will use them on the render
	// thread.  The only thing we're allowed to do is check to see if they are NULL or not.
	FTextureRenderTargetResource* BrushRenderTargetResource = BrushRenderTargetTexture->GameThread_GetRenderTargetResource();
	FTextureRenderTargetResource* BrushMaskRenderTargetResource = BrushMaskRenderTargetTexture->GameThread_GetRenderTargetResource();
	check( BrushRenderTargetResource != NULL );
	check( BrushMaskRenderTargetResource != NULL );

	{
		// Create a canvas for the brush render target.
		FCanvas Canvas( BrushRenderTargetResource, NULL );

		for( INT CurIndex = 0; CurIndex < TriangleInfo.Num(); ++CurIndex )
		{

			TRefCountPtr< FMeshPaintBatchedElementParameters > MeshPaintBatchedElementParameters( new FMeshPaintBatchedElementParameters() );
			{
				MeshPaintBatchedElementParameters->ShaderParams.CloneTexture = TextureData->PaintRenderTargetTexture;
				MeshPaintBatchedElementParameters->ShaderParams.WorldToBrushMatrix = WorldToBrushMatrix;
				MeshPaintBatchedElementParameters->ShaderParams.BrushRadius = InParams.InnerBrushRadius + InParams.BrushRadialFalloffRange;
				MeshPaintBatchedElementParameters->ShaderParams.BrushRadialFalloffRange = InParams.BrushRadialFalloffRange;
				MeshPaintBatchedElementParameters->ShaderParams.BrushDepth = InParams.InnerBrushDepth + InParams.BrushDepthFalloffRange;
				MeshPaintBatchedElementParameters->ShaderParams.BrushDepthFalloffRange = InParams.BrushDepthFalloffRange;
				MeshPaintBatchedElementParameters->ShaderParams.BrushStrength = InParams.BrushStrength;
				MeshPaintBatchedElementParameters->ShaderParams.BrushColor = InParams.BrushColor;
				MeshPaintBatchedElementParameters->ShaderParams.RedChannelFlag = InParams.bWriteRed;
				MeshPaintBatchedElementParameters->ShaderParams.GreenChannelFlag = InParams.bWriteGreen;
				MeshPaintBatchedElementParameters->ShaderParams.BlueChannelFlag = InParams.bWriteBlue;
				MeshPaintBatchedElementParameters->ShaderParams.AlphaChannelFlag = InParams.bWriteAlpha;
				MeshPaintBatchedElementParameters->ShaderParams.GenerateMaskFlag = FALSE;
			}

			FTexturePaintTriangleInfo CurTriangle = TriangleInfo( CurIndex );

			DrawTriangle2DWithParameters(
				&Canvas,
				CurTriangle.TrianglePoints[ 0 ],
				CurTriangle.TriUVs[ 0 ],
				FLinearColor( CurTriangle.TriVertices[ 0 ].X, CurTriangle.TriVertices[ 0 ].Y, CurTriangle.TriVertices[ 0 ].Z ),
				CurTriangle.TrianglePoints[ 1 ],
				CurTriangle.TriUVs[ 1 ],
				FLinearColor( CurTriangle.TriVertices[ 1 ].X, CurTriangle.TriVertices[ 1 ].Y, CurTriangle.TriVertices[ 1 ].Z ),
				CurTriangle.TrianglePoints[ 2 ],
				CurTriangle.TriUVs[ 2 ],
				FLinearColor( CurTriangle.TriVertices[ 2 ].X, CurTriangle.TriVertices[ 2 ].Y, CurTriangle.TriVertices[ 2 ].Z ),
				MeshPaintBatchedElementParameters,	// Parameters
				FALSE );		// Alpha blend?


		} 

		// Tell the rendering thread to draw any remaining batched elements
		Canvas.Flush(TRUE);
		
	}


	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			UpdateMeshPaintRTCommand1,
			FTextureRenderTargetResource*, BrushRenderTargetResource, BrushRenderTargetResource,
		{
			// Copy (resolve) the rendered image from the frame buffer to its render target texture
			RHICopyToResolveTarget(
				BrushRenderTargetResource->GetRenderTargetSurface(),	// Source texture
				TRUE,													// Do we need the source image content again?
				FResolveParams() );										// Resolve parameters
		});

	}

	if( bEnableSeamPainting )
	{
		// Create a canvas for the brush mask rendertarget and clear it to black.
		FCanvas Canvas( BrushMaskRenderTargetResource, NULL );
		Clear( &Canvas, FLinearColor::Black);

		for( INT CurIndex = 0; CurIndex < InInfluencedTriangles.Num(); ++CurIndex )
		{

			TRefCountPtr< FMeshPaintBatchedElementParameters > MeshPaintMaskBatchedElementParameters( new FMeshPaintBatchedElementParameters() );
			{
				MeshPaintMaskBatchedElementParameters->ShaderParams.CloneTexture = TextureData->PaintRenderTargetTexture;
				MeshPaintMaskBatchedElementParameters->ShaderParams.WorldToBrushMatrix = WorldToBrushMatrix;
				MeshPaintMaskBatchedElementParameters->ShaderParams.BrushRadius = InParams.InnerBrushRadius + InParams.BrushRadialFalloffRange;
				MeshPaintMaskBatchedElementParameters->ShaderParams.BrushRadialFalloffRange = InParams.BrushRadialFalloffRange;
				MeshPaintMaskBatchedElementParameters->ShaderParams.BrushDepth = InParams.InnerBrushDepth + InParams.BrushDepthFalloffRange;
				MeshPaintMaskBatchedElementParameters->ShaderParams.BrushDepthFalloffRange = InParams.BrushDepthFalloffRange;
				MeshPaintMaskBatchedElementParameters->ShaderParams.BrushStrength = InParams.BrushStrength;
				MeshPaintMaskBatchedElementParameters->ShaderParams.BrushColor = InParams.BrushColor;
				MeshPaintMaskBatchedElementParameters->ShaderParams.RedChannelFlag = InParams.bWriteRed;
				MeshPaintMaskBatchedElementParameters->ShaderParams.GreenChannelFlag = InParams.bWriteGreen;
				MeshPaintMaskBatchedElementParameters->ShaderParams.BlueChannelFlag = InParams.bWriteBlue;
				MeshPaintMaskBatchedElementParameters->ShaderParams.AlphaChannelFlag = InParams.bWriteAlpha;
				MeshPaintMaskBatchedElementParameters->ShaderParams.GenerateMaskFlag = TRUE;
			}


			FTexturePaintTriangleInfo CurTriangle = TriangleInfo( CurIndex );

			DrawTriangle2DWithParameters(
				&Canvas,
				CurTriangle.TrianglePoints[ 0 ],
				CurTriangle.TriUVs[ 0 ],
				FLinearColor( CurTriangle.TriVertices[ 0 ].X, CurTriangle.TriVertices[ 0 ].Y, CurTriangle.TriVertices[ 0 ].Z ),
				CurTriangle.TrianglePoints[ 1 ],
				CurTriangle.TriUVs[ 1 ],
				FLinearColor( CurTriangle.TriVertices[ 1 ].X, CurTriangle.TriVertices[ 1 ].Y, CurTriangle.TriVertices[ 1 ].Z ),
				CurTriangle.TrianglePoints[ 2 ],
				CurTriangle.TriUVs[ 2 ],
				FLinearColor( CurTriangle.TriVertices[ 2 ].X, CurTriangle.TriVertices[ 2 ].Y, CurTriangle.TriVertices[ 2 ].Z ),
				MeshPaintMaskBatchedElementParameters,	// Parameters
				FALSE );		// Alpha blend?

		}
		Canvas.Flush(TRUE);
	}

	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			UpdateMeshPaintRTCommand2,
			FTextureRenderTargetResource*, BrushMaskRenderTargetResource, BrushMaskRenderTargetResource,
		{
			// Copy (resolve) the rendered image from the frame buffer to its render target texture
			RHICopyToResolveTarget(
				BrushMaskRenderTargetResource->GetRenderTargetSurface(),		// Source texture
				TRUE,												// Do we need the source image content again?
				FResolveParams() );									// Resolve parameters
		});

	}

	if( !bEnableSeamPainting )
	{
		// Seam painting is not enabled so we just copy our delta paint info to the paint target.
		CopyTextureToRenderTargetTexture( BrushRenderTargetTexture, TextureData->PaintRenderTargetTexture );
	}
	else
	{

		// Constants used for generating quads accross entire paint rendertarget
		const FLOAT MinU = 0.0f;
		const FLOAT MinV = 0.0f;
		const FLOAT MaxU = 1.0f;
		const FLOAT MaxV = 1.0f;
		const FLOAT MinX = 0.0f;
		const FLOAT MinY = 0.0f;
		const FLOAT MaxX = TextureData->PaintRenderTargetTexture->GetSurfaceWidth();
		const FLOAT MaxY = TextureData->PaintRenderTargetTexture->GetSurfaceHeight();

		if( bGenerateSeamMask == TRUE )
		{
			// Generate the texture seam mask.  This is a slow operation when the object has many triangles so we only do it
			//  once when painting is started.
			GenerateSeamMask(TexturePaintingStaticMeshComponent, InParams.UVChannel, SeamMaskRenderTargetTexture);
			bGenerateSeamMask = FALSE;
		}
		
		FTextureRenderTargetResource* RenderTargetResource = TextureData->PaintRenderTargetTexture->GameThread_GetRenderTargetResource();
		check( RenderTargetResource != NULL );
		// Dilate the paint stroke into the texture seams.
		{
			// Create a canvas for the render target.
			FCanvas Canvas3( RenderTargetResource, NULL );


			TRefCountPtr< FMeshPaintDilateBatchedElementParameters > MeshPaintDilateBatchedElementParameters( new FMeshPaintDilateBatchedElementParameters() );
			{
				MeshPaintDilateBatchedElementParameters->ShaderParams.Texture0 = BrushRenderTargetTexture;
				MeshPaintDilateBatchedElementParameters->ShaderParams.Texture1 = SeamMaskRenderTargetTexture;
				MeshPaintDilateBatchedElementParameters->ShaderParams.Texture2 = BrushMaskRenderTargetTexture;
				MeshPaintDilateBatchedElementParameters->ShaderParams.WidthPixelOffset = (FLOAT) (1.0f / TextureData->PaintRenderTargetTexture->GetSurfaceWidth());
				MeshPaintDilateBatchedElementParameters->ShaderParams.HeightPixelOffset = (FLOAT) (1.0f / TextureData->PaintRenderTargetTexture->GetSurfaceHeight());

			}

			// Draw a quad to copy the texture over to the render target
			{

				DrawTriangle2DWithParameters(
					&Canvas3,
					FVector2D( MinX, MinY ),
					FVector2D( MinU, MinV ),
					FLinearColor::White,
					FVector2D( MaxX, MinY ),
					FVector2D( MaxU, MinV ),
					FLinearColor::White,
					FVector2D( MaxX, MaxY ),
					FVector2D( MaxU, MaxV ),
					FLinearColor::White,
					MeshPaintDilateBatchedElementParameters,
					FALSE );		// Alpha blend?

				DrawTriangle2DWithParameters(
					&Canvas3,
					FVector2D( MaxX, MaxY ),
					FVector2D( MaxU, MaxV ),
					FLinearColor::White,
					FVector2D( MinX, MaxY ),
					FVector2D( MinU, MaxV ),
					FLinearColor::White,
					FVector2D( MinX, MinY ),
					FVector2D( MinU, MinV ),
					FLinearColor::White,
					MeshPaintDilateBatchedElementParameters,
					FALSE );		// Alpha blend?
			}

			// Tell the rendering thread to draw any remaining batched elements
			Canvas3.Flush(TRUE);

		}

		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				UpdateMeshPaintRTCommand3,
				FTextureRenderTargetResource*, RenderTargetResource, RenderTargetResource,
			{
				// Copy (resolve) the rendered image from the frame buffer to its render target texture
				RHICopyToResolveTarget(
					RenderTargetResource->GetRenderTargetSurface(),		// Source texture
					TRUE,												// Do we need the source image content again?
					FResolveParams() );									// Resolve parameters
			});

		}

	}

	FlushRenderingCommands();

	TextureData->bIsPaintingTexture2DModified = TRUE;

#if WITH_MANAGED_CODE
	if( MeshPaintWindow != NULL )
	{
		// One of the textures has been modified, we notify the UI so it can enable the commit changes button if needed
		MeshPaintWindow->RefreshAllProperties();
	}
#endif
	

}

void FEdModeMeshPaint::CommitAllPaintedTextures(UBOOL bShouldTriggerUIRefresh/*=TRUE*/)
{
	if( PaintTargetData.Num() > 0)
	{
		check( PaintingTexture2D == NULL );

		UBOOL bTriggerUIRefresh = FALSE;

		GWarn->BeginSlowTask( *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MeshPaintMode_TexturePaint_Commit" ), TEXT(" ") ) ), TRUE );

		INT CurStep = 1;
		INT TotalSteps = GetNumberOfPendingPaintChanges();
		for ( TMap< UTexture2D*, PaintTexture2DData >::TIterator It(PaintTargetData); It; ++It)
		{
			PaintTexture2DData* TextureData = &It.Value();
			
			// Commit the texture
			if( TextureData->bIsPaintingTexture2DModified == TRUE )
			{
				GWarn->StatusUpdatef( CurStep++, TotalSteps, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MeshPaintMode_TexturePaint_Commit" ), *TextureData->PaintingTexture2D->GetName() ) ) );

				const INT TexWidth = TextureData->PaintRenderTargetTexture->SizeX;
				const INT TexHeight = TextureData->PaintRenderTargetTexture->SizeY;
				TArray< FColor > TexturePixels;
				TexturePixels.Add( TexWidth * TexHeight );

				// Copy the contents of the remote texture to system memory
				// NOTE: OutRawImageData must be a preallocated buffer!

				FlushRenderingCommands();
				// NOTE: You are normally not allowed to dereference this pointer on the game thread! Normally you can only pass the pointer around and
				//  check for NULLness.  We do it in this context, however, and it is only ok because this does not happen every frame and we make sure to flush the
				//  rendering thread.
				FTextureRenderTargetResource* RenderTargetResource = TextureData->PaintRenderTargetTexture->GameThread_GetRenderTargetResource();
				check(RenderTargetResource != NULL);
				RenderTargetResource->ReadPixels( ( BYTE* )&TexturePixels( 0 ) );

				{
					TextureData->PaintingTexture2D->Modify();

					// Store source art
					TextureData->PaintingTexture2D->SetUncompressedSourceArt( TexturePixels.GetData(), TexturePixels.Num() * sizeof( FColor ) );

					// If render target gamma used was 1.0 then disable SRGB for the static texture
					// @todo MeshPaint: We are not allowed to dereference the RenderTargetResource pointer, figure out why we need this when the GetDisplayGamma() function is hard coded to return 2.2.
					TextureData->PaintingTexture2D->SRGB = TRUE;
					TextureData->PaintingTexture2D->bHasBeenPaintedInEditor = TRUE;

					// Update the texture (generate mips, compress if needed)
					TextureData->PaintingTexture2D->PostEditChange();

					TextureData->bIsPaintingTexture2DModified = FALSE;

					// Reduplicate the duplicate so that if we cancel our future changes, it will restore to how the texture looked at this point.
					TextureData->PaintingTexture2DDuplicate = (UTexture2D*)GEditor->StaticDuplicateObject(TextureData->PaintingTexture2D, TextureData->PaintingTexture2D, UObject::GetTransientPackage(), *FString::Printf(TEXT("%s_TEMP"), *TextureData->PaintingTexture2D->GetName()));

				}

				bTriggerUIRefresh = bShouldTriggerUIRefresh;
			}
		}

		ClearAllTextureOverrides();
	
	
	//	PaintTargetData.Empty();

		GWarn->EndSlowTask();

#if WITH_MANAGED_CODE
		if( MeshPaintWindow != NULL && bTriggerUIRefresh == TRUE )
		{
			// Once we update the target texture we will tell the UI to refresh the any updated properties
			MeshPaintWindow->RefreshAllProperties();
		}
#endif
	}

}
/** Used to tell the texture paint system that we will need to restore the rendertargets */
void FEdModeMeshPaint::RestoreRenderTargets()
{
	bDoRestoreRenTargets = TRUE;
}

/** Clears all texture overrides for this static mesh */
void FEdModeMeshPaint::ClearStaticMeshTextureOverrides(UStaticMeshComponent* InStaticMeshComponent)
{
	if(!InStaticMeshComponent)
	{
		return;
	}

	TArray<UMaterialInterface*> UsedMaterials;

	// Get all the used materials for this StaticMeshComponent
	InStaticMeshComponent->GetUsedMaterials( UsedMaterials );

	for( INT MatIndex = 0; MatIndex < UsedMaterials.Num(); MatIndex++)
	{
		UMaterialInterface* Material = UsedMaterials( MatIndex );

		if( Material != NULL )
		{
			TArray<UTexture*> UsedTextures;
			Material->GetUsedTextures( UsedTextures, MSQ_UNSPECIFIED, FALSE, FALSE );

			for( INT UsedIndex = 0; UsedIndex < UsedTextures.Num(); UsedIndex++ )
			{
				//Reset the texture to it's default.
				Material->OverrideTexture( UsedTextures( UsedIndex ), NULL );
			}		
		}
	}
}

/** Clears all texture overrides, removing any pending texture paint changes */
void FEdModeMeshPaint::ClearAllTextureOverrides()
{
	for ( TMap< UTexture2D*, PaintTexture2DData >::TIterator It(PaintTargetData); It; ++It)
	{
		PaintTexture2DData* TextureData = &It.Value();

		for( INT MaterialIndex = 0; MaterialIndex < TextureData->PaintingMaterials.Num(); MaterialIndex++)
		{
			UMaterialInterface* PaintingMaterialInterface = TextureData->PaintingMaterials(MaterialIndex);
			PaintingMaterialInterface->OverrideTexture( TextureData->PaintingTexture2D, NULL );
		}

		TextureData->PaintingMaterials.Empty();
	}
}

/** Sets all texture overrides available for the mesh. */
void FEdModeMeshPaint::SetAllTextureOverrides(UStaticMeshComponent* InStaticMeshComponent)
{
	if(!InStaticMeshComponent)
	{
		return;
	}

	TArray<UMaterialInterface*> UsedMaterials;

	// Get all the used materials for this StaticMeshComponent
	InStaticMeshComponent->GetUsedMaterials( UsedMaterials );

	// Add the materials this actor uses to the list we maintain for ALL the selected actors, but only if
	//  it does not appear in the list already.
	for( INT MatIndex = 0; MatIndex < UsedMaterials.Num(); MatIndex++)
	{
		UMaterialInterface* Material = UsedMaterials( MatIndex );

		if( Material != NULL )
		{
			TArray<UTexture*> UsedTextures;
			Material->GetUsedTextures( UsedTextures );

			for( INT UsedIndex = 0; UsedIndex < UsedTextures.Num(); UsedIndex++ )
			{

				PaintTexture2DData* TextureData = GetPaintTargetData( (UTexture2D*)UsedTextures( UsedIndex ) );

				if(TextureData)
				{
					Material->OverrideTexture( UsedTextures( UsedIndex ), TextureData->PaintRenderTargetTexture );
				}
			}		
		}
	}
		
}

/** Sets the override for a specific texture for any materials using it in the mesh, clears the override if it has no overrides. */
void FEdModeMeshPaint::SetSpecificTextureOverrideForMesh(UStaticMeshComponent* InStaticMeshComponent, UTexture* Texture)
{
	PaintTexture2DData* TextureData = GetPaintTargetData( (UTexture2D*)Texture );

	// Check all the materials on the mesh to see if the user texture is there
	INT MaterialIndex = 0;
	UMaterialInterface* MaterialToCheck = InStaticMeshComponent->GetMaterial( MaterialIndex, PaintingMeshLODIndex );
	while( MaterialToCheck != NULL )
	{
		UBOOL bIsTextureUsed = MaterialToCheck->UsesTexture( Texture, FALSE );

		if(bIsTextureUsed)
		{
			if(TextureData)
			{
				// If there is texture data, that means we have an override ready, so set it. 
				MaterialToCheck->OverrideTexture( Texture, TextureData->PaintRenderTargetTexture );
			}
			else
			{
				// If there is no data, then remove the override so we can at least see the texture without the changes to the other texture.
					// This is important because overrides are shared between material instances with the same parent. We want to disable a override in place,
					// making the action more comprehensive to the user.
				MaterialToCheck->OverrideTexture( Texture, NULL );
			}
		}

		++MaterialIndex;
		MaterialToCheck = InStaticMeshComponent->GetMaterial( MaterialIndex, PaintingMeshLODIndex );
	}
}

INT FEdModeMeshPaint::GetNumberOfPendingPaintChanges()
{
	INT Result = 0;
	for ( TMap< UTexture2D*, PaintTexture2DData >::TIterator It(PaintTargetData); It; ++It)
	{
		PaintTexture2DData* TextureData = &It.Value();

		// Commit the texture
		if( TextureData->bIsPaintingTexture2DModified == TRUE )
		{
			Result++;
		}
	}
	return Result;
}


/** Finishes painting a texture */
void FEdModeMeshPaint::FinishPaintingTexture( )
{
	if( TexturePaintingStaticMeshComponent != NULL )
	{
		check( PaintingTexture2D != NULL );

		UBOOL bTriggerUIRefresh =  FALSE;

		PaintTexture2DData* TextureData = GetPaintTargetData( PaintingTexture2D );
		check( TextureData );

		// Commit to the texture source art but don't do any compression, compression is saved for the CommitAllPaintedTextures function.
		if( TextureData->bIsPaintingTexture2DModified == TRUE )
		{
			const INT TexWidth = TextureData->PaintRenderTargetTexture->SizeX;
			const INT TexHeight = TextureData->PaintRenderTargetTexture->SizeY;
			TArray< BYTE > TexturePixels;

			FlushRenderingCommands();
			// NOTE: You are normally not allowed to dereference this pointer on the game thread! Normally you can only pass the pointer around and
			//  check for NULLness.  We do it in this context, however, and it is only ok because this does not happen every frame and we make sure to flush the
			//  rendering thread.
			FTextureRenderTargetResource* RenderTargetResource = TextureData->PaintRenderTargetTexture->GameThread_GetRenderTargetResource();
			check(RenderTargetResource != NULL);
			RenderTargetResource->ReadPixels( TexturePixels, FReadSurfaceDataFlags(),0,0,TexWidth,TexHeight );

			{
				FScopedTransaction Transaction( *LocalizeUnrealEd( TEXT("MeshPaintMode_TexturePaint_Transaction") ) );

				// For undo
				TextureData->PaintingTexture2D->SetFlags(RF_Transactional);
				TextureData->PaintingTexture2D->Modify();

				// Store source art
				TextureData->PaintingTexture2D->SetUncompressedSourceArt( TexturePixels.GetData(), TexturePixels.Num());

				// If render target gamma used was 1.0 then disable SRGB for the static texture
				TextureData->PaintingTexture2D->SRGB = TRUE;
				TextureData->PaintingTexture2D->bHasBeenPaintedInEditor = TRUE;
			}

			TArray<FTextureTargetListInfo>* TargetListInfo = GetTexturePaintTargetList();

			// Get the transaction size.
			INT TransactionSize = GEditor->GetLastTransactionSize();

			// Check to make sure it is not 0, we are dividing by the number.
			if(TransactionSize != 0)
			{
				// Set the UndoCount in the FTextureTargetListInfo (the data that appears in the window's drop down list).
				(*TargetListInfo)(GetCurrentTextureTargetIndex()).UndoCount = (GEditor->GetFreeTransactionBufferSpace() / GEditor->GetLastTransactionSize()) + 1; // Add one because we can go one over.
			}

			bTriggerUIRefresh = TRUE;
		}

		PaintingTexture2D = NULL;
		TexturePaintingStaticMeshComponent = NULL;

#if WITH_MANAGED_CODE
		if( MeshPaintWindow != NULL && bTriggerUIRefresh == TRUE )
		{
			// Once we update the target texture we will tell the UI to refresh the target texture list properties
			MeshPaintWindow->RefreshTextureTargetListProperties();

			// This paint operation may have dirtied the target texture so we tell the UI to refresh so the icon can enable itself
			MeshPaintWindow->RefreshAllProperties();
		}
#endif


	}
}




/** FEdMode: Called when mouse drag input it applied */
UBOOL FEdModeMeshPaint::InputDelta( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale )
{
	// We only care about perspective viewports
	if( InViewportClient->IsPerspective() )
	{
		// ...
	}

	return FALSE;
}

/** FEdMode: Called after an Undo operation */
void FEdModeMeshPaint::PostUndo()
{
	FEdMode::PostUndo();
	bDoRestoreRenTargets = TRUE;
}

/** Returns TRUE if we need to force a render/update through based fill/copy */
UBOOL FEdModeMeshPaint::IsForceRendered (void) const
{
	return (bIsFloodFill || bPushInstanceColorsToMesh);
}


/** FEdMode: Render the mesh paint tool */
void FEdModeMeshPaint::Render( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI )
{
	/** Call parent implementation */
	FEdMode::Render( View, Viewport, PDI );

	// If this viewport does not support Mode widgets we will not draw it here.
	FEditorLevelViewportClient* ViewportClient = (FEditorLevelViewportClient*)Viewport->GetClient();

	// We only care about perspective viewports
	const UBOOL bIsPerspectiveViewport = ( View->ProjectionMatrix.M[ 3 ][ 3 ] < ( 1.0f - SMALL_NUMBER ) );
	if( bIsPerspectiveViewport )
	{
		// Make sure perspective viewports are still set to real-time
		const UBOOL bWantRealTime = TRUE;
		const UBOOL bRememberCurrentState = FALSE;
		ForceRealTimeViewports( bWantRealTime, bRememberCurrentState );


		// Set viewport show flags
		const UBOOL bAllowColorViewModes = ( FMeshPaintSettings::Get().ResourceType != EMeshPaintResource::Texture );
		SetViewportShowFlags( bAllowColorViewModes );


		// Make sure the cursor is visible OR we're flood filling.  No point drawing a paint cue when there's no cursor.
		if( Viewport->IsCursorVisible() || IsForceRendered())
		{
			// Make sure the cursor isn't underneath the mesh paint window (unless we're already painting)
			if( bIsPainting || IsForceRendered()
	#if WITH_MANAGED_CODE
				|| MeshPaintWindow == NULL || !MeshPaintWindow->IsMouseOverWindow()
	#endif
				)
			{
				if( !PDI->IsHitTesting() )
				{
					// Grab the mouse cursor position
					FIntPoint MousePosition;
					Viewport->GetMousePos( MousePosition );

					// Is the mouse currently over the viewport? or flood filling
					if(IsForceRendered() || ( MousePosition.X >= 0 && MousePosition.Y >= 0 && MousePosition.X < (INT)Viewport->GetSizeX() && MousePosition.Y < (INT)Viewport->GetSizeY()) )
					{
						// Compute a world space ray from the screen space mouse coordinates
						FViewportCursorLocation MouseViewportRay( View, ViewportClient, MousePosition.X, MousePosition.Y );


						// Unless "Flow" mode is enabled, we'll only draw a visual cue while rendering and won't
						// do any actual painting.  When "Flow" is turned on we will paint here, too!
						const UBOOL bVisualCueOnly = !FMeshPaintSettings::Get().bEnableFlow;
						FLOAT StrengthScale = FMeshPaintSettings::Get().bEnableFlow ? FMeshPaintSettings::Get().FlowAmount : 1.0f;

						// Apply stylus pressure if it's active
						if( Viewport->IsPenActive() )
						{
							StrengthScale *= Viewport->GetTabletPressure();
						}

						const EMeshPaintAction::Type PaintAction = GetPaintAction(Viewport);
						UBOOL bAnyPaintAbleActorsUnderCursor = FALSE;
						DoPaint( View->ViewOrigin, MouseViewportRay.GetOrigin(), MouseViewportRay.GetDirection(), PDI, PaintAction, bVisualCueOnly, StrengthScale, bAnyPaintAbleActorsUnderCursor );
					}
				}
			}
		}
	}
}



/** FEdMode: Render HUD elements for this tool */
void FEdModeMeshPaint::DrawHUD( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas )
{
	// We only care about perspective viewports and we only show mode specific widgets if the viewport SHOW_ModeWidgets flag is set.
	if( ViewportClient->IsPerspective() && (ViewportClient->ShowFlags & SHOW_ModeWidgets) )
	{
		// Draw screen space bounding box around selected actors.  We do this because while in mesh paint mode,
		// we opt to not highlight selected actors (as that makes it more difficult to paint colors.)  Drawing
		// a box around the selected actors doesn't interfere with painting as much.
		DrawBrackets(ViewportClient, Viewport, View, Canvas);
	}
}

// @TODO MeshPaint: Cache selected static mesh components each time selection change
/** Returns valid StaticMesheComponents in the current selection */
TArray<UStaticMeshComponent*> GetValidStaticMeshComponents()
{
	TArray<UStaticMeshComponent*> SMComponents;

	// Iterate over selected actors looking for static meshes
	USelection& SelectedActors = *GEditor->GetSelectedActors();
	for( INT CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex )
	{
		AActor* CurActor = CastChecked< AActor >( SelectedActors( CurSelectedActorIndex ) );

		// Ignore actors that are hidden or not selected
		if ( CurActor->bHidden || !CurActor->IsSelected() )
		{
			continue;
		}

		// Currently we only support static mesh actors or dynamic static mesh actors
		AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( CurActor );
		ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( CurActor );

		// Grab the StaticMeshComponents from valid static mesh actors
		if( StaticMeshActor != NULL && StaticMeshActor->StaticMeshComponent != NULL )
		{
			SMComponents.AddUniqueItem(StaticMeshActor->StaticMeshComponent);
		}
		else if ( DynamicSMActor != NULL && DynamicSMActor->StaticMeshComponent != NULL )
		{
			SMComponents.AddUniqueItem(DynamicSMActor->StaticMeshComponent);
		}
		else
		{
			// An unsupported actor type is part of the user selection
			//warnf( NAME_Warning, TEXT("%s is not supported for Texture Painting"), *CurActor->GetName() );
		}
	}

	return SMComponents;
}

/** Saves out cached mesh settings for the given actor */
void FEdModeMeshPaint::SaveSettingsForActor( AActor* InActor )
{
	if( InActor != NULL )
	{
		AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( InActor );
		ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( InActor );

		UStaticMeshComponent* StaticMeshComponent = NULL;
		if( StaticMeshActor != NULL )
		{
			StaticMeshComponent = StaticMeshActor->StaticMeshComponent;
		}
		else if( DynamicSMActor != NULL )
		{
			StaticMeshComponent = DynamicSMActor->StaticMeshComponent;
		}

		SaveSettingsForStaticMeshComponent(StaticMeshComponent);
	}
}

void FEdModeMeshPaint::SaveSettingsForStaticMeshComponent( UStaticMeshComponent* InStaticMeshComponent )
{
	if( InStaticMeshComponent != NULL )
	{
		// Get the currently selected texture
		UTexture2D* SelectedTexture = GetSelectedTexture();

		// Get all the used materials for this InStaticMeshComponent
		TArray<UMaterialInterface*> UsedMaterials;
		InStaticMeshComponent->GetUsedMaterials( UsedMaterials );

		// Check this mesh's textures against the selected one before we save the settings to make sure it's a valid texture
		for( INT MatIndex = 0; MatIndex < UsedMaterials.Num(); MatIndex++)
		{
			if(UsedMaterials( MatIndex ) == NULL)
			{
				continue;
			}

			TArray<UTexture*> UsedTextures;
			UsedMaterials( MatIndex )->GetUsedTextures( UsedTextures );

			for( INT TexIndex = 0; TexIndex < UsedTextures.Num(); TexIndex++ )
			{
				UTexture2D* Texture2D = Cast<UTexture2D>( UsedTextures( TexIndex ) );
				if(Texture2D == NULL )
				{
					UTextureRenderTarget2D* TextureRenderTarget2D = Cast<UTextureRenderTarget2D>( UsedTextures( TexIndex ) );
					if( TextureRenderTarget2D )
					{
						Texture2D = GetOriginalTextureFromRenderTarget( TextureRenderTarget2D );
					}
				}

				if( SelectedTexture == Texture2D )
				{
					// Save the settings for this mesh with its valid texture
					StaticMeshSettings MeshSettings = StaticMeshSettings(SelectedTexture, FMeshPaintSettings::Get().UVChannel);
					StaticMeshSettingsMap.Set(InStaticMeshComponent, MeshSettings);
					return;
				}
			}		
		}

		// No valid Texture found, attempt to find the previous texture setting or leave it as NULL to be handled by the default texture on selection
		StaticMeshSettings* FoundMeshSettings = StaticMeshSettingsMap.Find(InStaticMeshComponent);
		UTexture2D* SavedTexture = FoundMeshSettings != NULL ? FoundMeshSettings->SelectedTexture : NULL;
		StaticMeshSettings MeshSettings = StaticMeshSettings(SavedTexture, FMeshPaintSettings::Get().UVChannel);
		StaticMeshSettingsMap.Set(InStaticMeshComponent, MeshSettings);
	}
}

void FEdModeMeshPaint::UpdateSettingsForStaticMeshComponent( UStaticMeshComponent* InStaticMeshComponent, UTexture2D* InOldTexture, UTexture2D* InNewTexture )
{
	if( InStaticMeshComponent != NULL )
	{
		// Get all the used materials for this InStaticMeshComponent
		TArray<UMaterialInterface*> UsedMaterials;
		InStaticMeshComponent->GetUsedMaterials( UsedMaterials );

		// Check this mesh's textures against the selected one before we save the settings to make sure it's a valid texture
		for( INT MatIndex = 0; MatIndex < UsedMaterials.Num(); MatIndex++)
		{
			if(UsedMaterials( MatIndex ) == NULL)
			{
				continue;
			}

			TArray<UTexture*> UsedTextures;
			UsedMaterials( MatIndex )->GetUsedTextures( UsedTextures );

			for( INT TexIndex = 0; TexIndex < UsedTextures.Num(); TexIndex++ )
			{
				UTexture2D* Texture2D = Cast<UTexture2D>( UsedTextures( TexIndex ) );
				if(Texture2D == NULL )
				{
					UTextureRenderTarget2D* TextureRenderTarget2D = Cast<UTextureRenderTarget2D>( UsedTextures( TexIndex ) );
					if( TextureRenderTarget2D )
					{
						Texture2D = GetOriginalTextureFromRenderTarget( TextureRenderTarget2D );
					}
				}

				if( InOldTexture == Texture2D )
				{
					// Save the settings for this mesh with its valid texture
					StaticMeshSettings MeshSettings = StaticMeshSettings(InNewTexture, FMeshPaintSettings::Get().UVChannel);
					StaticMeshSettingsMap.Set(InStaticMeshComponent, MeshSettings);
					return;
				}
			}		
		}
	}
}

/** FEdMode: Handling SelectActor */
UBOOL FEdModeMeshPaint::Select( AActor* InActor, UBOOL bInSelected )
{
	// When un-selecting a mesh, save it's settings based on the current properties
	if( !bInSelected && FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture )
	{
		// Get all the used materials for the actor.
		AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( InActor );
		ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( InActor );

		UStaticMeshComponent* StaticMeshComponent = NULL;
		if( StaticMeshActor != NULL )
		{
			StaticMeshComponent = StaticMeshActor->StaticMeshComponent;
		}
		else if( DynamicSMActor != NULL )
		{
			StaticMeshComponent = DynamicSMActor->StaticMeshComponent;
		}
		ClearStaticMeshTextureOverrides(StaticMeshComponent);

		SaveSettingsForActor(InActor);
	}
	else if(bInSelected && FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture)
	{	
		AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( InActor );
		ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( InActor );

		UStaticMeshComponent* StaticMeshComponent = NULL;
		if( StaticMeshActor != NULL )
		{
			StaticMeshComponent = StaticMeshActor->StaticMeshComponent;
		}
		else if( DynamicSMActor != NULL )
		{
			StaticMeshComponent = DynamicSMActor->StaticMeshComponent;
		}

		SetAllTextureOverrides(StaticMeshComponent);
	}
	return FALSE;
}

/** FEdMode: Called when the currently selected actor has changed */
void FEdModeMeshPaint::ActorSelectionChangeNotify()
{
// Any updates require a valid mesh paint window, so make sure it's valid before doing anything else
#if WITH_MANAGED_CODE
	if( MeshPaintWindow != NULL )
	{	
		if( FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture )
		{
			// Make sure we update the texture list to case for the new actor
			bShouldUpdateTextureList = TRUE;

			// Update any settings on the current selection
			StaticMeshSettings* MeshSettings = NULL;

			// For now, just grab the first mesh we find with some cached settings
			TArray<UStaticMeshComponent*> SMComponents = GetValidStaticMeshComponents();
			for( INT CurSMIndex = 0; CurSMIndex < SMComponents.Num(); ++CurSMIndex )
			{
				UStaticMeshComponent* StaticMesh = SMComponents(CurSMIndex);
				MeshSettings = StaticMeshSettingsMap.Find(StaticMesh);
				if( MeshSettings != NULL )
				{
					break;
				}
			}

			if( MeshSettings != NULL)
			{
				// Refresh properties to get latest settings based on selection
				MeshPaintWindow->RefreshAllProperties();

				// Set UVChannel to our cached setting
				FMeshPaintSettings::Get().UVChannel = MeshSettings->SelectedUVChannel;

				// Loop through our list of textures and match up from the user cache
				UBOOL bFoundSavedTexture = FALSE;
				for ( TArray<FTextureTargetListInfo>::TIterator It(TexturePaintTargetList); It; ++It)
				{
					It->bIsSelected = FALSE;
					if(It->TextureData == MeshSettings->SelectedTexture)
					{
						// Found the texture we were looking for, continue through to 'un-select' the other textures.
						It->bIsSelected = bFoundSavedTexture = TRUE;
					}
				}

				// Saved texture wasn't found, default to first selection. Don't have to 'un-select' anything since we already did so above.
				if(!bFoundSavedTexture && TexturePaintTargetList.Num() > 0)
				{
					TexturePaintTargetList(0).bIsSelected = TRUE;
				}

				// Update texture list below to reflect any selection changes
				bShouldUpdateTextureList = TRUE;
			}
			else if( SMComponents.Num() > 0 )
			{
				// Refresh properties to get latest settings based on selection
				MeshPaintWindow->RefreshAllProperties();

				// No cached settings, default UVChannel to 0 and Texture Target list to first selection
				FMeshPaintSettings::Get().UVChannel = 0;

				INT Index = 0;
				for ( TArray<FTextureTargetListInfo>::TIterator It(TexturePaintTargetList); It; ++It)
				{
					It->bIsSelected = Index == 0;
					++Index;
				}
				// Update texture list below to reflect any selection changes
				bShouldUpdateTextureList = TRUE;
			}
		}
		// Update Mesh Paint window to make sure its up to date based on any changes above or from the actor selection
		MeshPaintWindow->RefreshAllProperties();
	}
#endif
}



/** Forces real-time perspective viewports */
void FEdModeMeshPaint::ForceRealTimeViewports( const UBOOL bEnable, const UBOOL bStoreCurrentState )
{
	// Force perspective views to be real-time
	for( INT CurViewportIndex = 0; CurViewportIndex < GApp->EditorFrame->ViewportConfigData->GetViewportCount(); ++CurViewportIndex )
	{
		WxLevelViewportWindow* CurLevelViewportWindow =
			GApp->EditorFrame->ViewportConfigData->AccessViewport( CurViewportIndex ).ViewportWindow;
		if( CurLevelViewportWindow != NULL )
		{
			if( CurLevelViewportWindow->ViewportType == LVT_Perspective )
			{				
				if( bEnable )
				{
					CurLevelViewportWindow->SetRealtime( bEnable, bStoreCurrentState );
				}
				else
				{
					const UBOOL bAllowDisable = TRUE;
					CurLevelViewportWindow->RestoreRealtime( bAllowDisable );
				}

			}
		}
	}
}



/** Sets show flags for perspective viewports */
void FEdModeMeshPaint::SetViewportShowFlags( const UBOOL bAllowColorViewModes )
{
	// Force perspective views to be real-time
	for( INT CurViewportIndex = 0; CurViewportIndex < GApp->EditorFrame->ViewportConfigData->GetViewportCount(); ++CurViewportIndex )
	{
		WxLevelViewportWindow* CurLevelViewportWindow =
			GApp->EditorFrame->ViewportConfigData->AccessViewport( CurViewportIndex ).ViewportWindow;
		if( CurLevelViewportWindow != NULL )
		{
			if( CurLevelViewportWindow->ViewportType == LVT_Perspective )
			{				
				// Update viewport show flags
				{
					// show flags forced on during vertex color modes
					const EShowFlags VertexColorShowFlags = SHOW_BSPTriangles | SHOW_Materials | SHOW_PostProcess | SHOW_VertexColors;

					EMeshPaintColorViewMode::Type ColorViewMode = FMeshPaintSettings::Get().ColorViewMode;
					if( !bAllowColorViewModes )
					{
						ColorViewMode = EMeshPaintColorViewMode::Normal;
					}

					if(ColorViewMode == EMeshPaintColorViewMode::Normal)
					{
						if( ( CurLevelViewportWindow->ShowFlags & SHOW_VertexColors ) == SHOW_VertexColors )
						{
							// If we're transitioning to normal mode then restore the backup
							// Clear the flags relevant to vertex color modes
							CurLevelViewportWindow->ShowFlags &= ~VertexColorShowFlags;
							// Restore the vertex color mode flags that were set when we last entered vertex color mode
							CurLevelViewportWindow->ShowFlags |= CurLevelViewportWindow->PreviousEdModeVertColorShowFlags;
							GVertexColorViewMode = EVertexColorViewMode::Color;
						}
					}
					else
					{
						// If we're transitioning from normal mode then backup the current view mode
						if( ( CurLevelViewportWindow->ShowFlags & SHOW_VertexColors ) == 0 )
						{
							// Save the vertex color mode flags that are set
							CurLevelViewportWindow->PreviousEdModeVertColorShowFlags = CurLevelViewportWindow->ShowFlags & VertexColorShowFlags;
						}

						CurLevelViewportWindow->ShowFlags |= VertexColorShowFlags;
						
						switch( ColorViewMode )
						{
							case EMeshPaintColorViewMode::RGB:
								{
									GVertexColorViewMode = EVertexColorViewMode::Color;
								}
								break;

							case EMeshPaintColorViewMode::Alpha:
								{
									GVertexColorViewMode = EVertexColorViewMode::Alpha;
								}
								break;

							case EMeshPaintColorViewMode::Red:
								{
									GVertexColorViewMode = EVertexColorViewMode::Red;
								}
								break;

							case EMeshPaintColorViewMode::Green:
								{
									GVertexColorViewMode = EVertexColorViewMode::Green;
								}
								break;

							case EMeshPaintColorViewMode::Blue:
								{
									GVertexColorViewMode = EVertexColorViewMode::Blue;
								}
								break;
						}
					}
				}
			}
		}
	}
}



/** Makes sure that the render target is ready to paint on */
void FEdModeMeshPaint::SetupInitialRenderTargetData( UTexture2D* InTextureSource, UTextureRenderTarget2D* InRenderTarget )
{
	check( InTextureSource != NULL );
	check( InRenderTarget != NULL );

	if( InTextureSource->HasSourceArt() )
	{
		// Great, we have source data!  We'll use that as our image source.

		// Create a texture in memory from the source art
		{
			// @todo MeshPaint: This generates a lot of memory thrash -- try to cache this texture and reuse it?
			UTexture2D* TempSourceArtTexture = CreateTempUncompressedTexture( InTextureSource );
			check( TempSourceArtTexture != NULL );

			// Copy the texture to the render target using the GPU
			CopyTextureToRenderTargetTexture( TempSourceArtTexture, InRenderTarget );

			// NOTE: TempSourceArtTexture is no longer needed (will be GC'd)
		}
	}
	else
	{
		// Just copy (render) the texture in GPU memory to our render target.  Hopefully it's not
		// compressed already!
		check( InTextureSource->IsFullyStreamedIn() );
		CopyTextureToRenderTargetTexture( InTextureSource, InRenderTarget );
	}

}



/** Static: Creates a temporary texture used to transfer data to a render target in memory */
UTexture2D* FEdModeMeshPaint::CreateTempUncompressedTexture( UTexture2D* SourceTexture )
{
	check( SourceTexture->HasSourceArt() );

	// Decompress PNG image
	TArray<BYTE> RawData;
	SourceTexture->GetUncompressedSourceArt(RawData);

	// We are using the source art so grab the original width/height
	const INT Width = SourceTexture->GetOriginalSurfaceWidth();
	const INT Height = SourceTexture->GetOriginalSurfaceHeight();
	const UBOOL bUseSRGB = SourceTexture->SRGB;

	check( Width > 0 && Height > 0 && RawData.Num() > 0 );

	// Allocate the new texture
	const EObjectFlags ObjectFlags = RF_NotForClient | RF_NotForServer | RF_Transient;
	UTexture2D* NewTexture2D = CastChecked< UTexture2D >( UObject::StaticConstructObject( UTexture2D::StaticClass(), UObject::GetTransientPackage(), NAME_None, ObjectFlags ) );
	NewTexture2D->Init( Width, Height, PF_A8R8G8B8 );
	
	// Fill in the base mip for the texture we created
	BYTE* MipData = ( BYTE* )NewTexture2D->Mips( 0 ).Data.Lock( LOCK_READ_WRITE );
	for( INT y=0; y<Height; y++ )
	{
		BYTE* DestPtr = &MipData[(Height - 1 - y) * Width * sizeof(FColor)];
		const FColor* SrcPtr = &( (FColor*)( &RawData(0) ) )[ ( Height - 1 - y ) * Width ];
		for( INT x=0; x<Width; x++ )
		{
			*DestPtr++ = SrcPtr->B;
			*DestPtr++ = SrcPtr->G;
			*DestPtr++ = SrcPtr->R;
			*DestPtr++ = 0xFF;
			SrcPtr++;
		}
	}
	NewTexture2D->Mips( 0 ).Data.Unlock();

	// Set options
	NewTexture2D->SRGB = bUseSRGB;
	NewTexture2D->CompressionNone = TRUE;
	NewTexture2D->MipGenSettings = TMGS_NoMipmaps;
	NewTexture2D->CompressionSettings = TC_Default;

	// Update the remote texture data
	NewTexture2D->UpdateResource();

	return NewTexture2D;
}



/** Static: Copies a texture to a render target texture */
void FEdModeMeshPaint::CopyTextureToRenderTargetTexture( UTexture* SourceTexture, UTextureRenderTarget2D* RenderTargetTexture )
{	
	check( SourceTexture != NULL );
	check( RenderTargetTexture != NULL );

	/*
	// Grab an editor viewport to use as a frame buffer
	FViewport* Viewport = NULL;
	for( INT CurMainViewportIndex = 0; CurMainViewportIndex < GApp->EditorFrame->ViewportConfigData->GetViewportCount(); ++CurMainViewportIndex )
	{
		// Grab the first level viewport to use for rendering
		FEditorLevelViewportClient* LevelVC = GApp->EditorFrame->ViewportConfigData->AccessViewport( CurMainViewportIndex ).ViewportWindow;
		if( LevelVC != NULL )
		{
			Viewport = LevelVC->Viewport;
			break;
		}
	}
	check( Viewport != NULL );

	// Begin scene
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		BeginDrawingCommand,
		FViewport*,Viewport,Viewport,
	{
		Viewport->BeginRenderFrame();
	});*/



	// Grab the actual render target resource from the texture.  Note that we're absolutely NOT ALLOWED to
	// dereference this pointer.  We're just passing it along to other functions that will use it on the render
	// thread.  The only thing we're allowed to do is check to see if it's NULL or not.
	FTextureRenderTargetResource* RenderTargetResource = RenderTargetTexture->GameThread_GetRenderTargetResource();
	check( RenderTargetResource != NULL );


	{
		// Create a canvas for the render target and clear it to black
		FCanvas Canvas( RenderTargetResource, NULL );

		const UINT Width = RenderTargetTexture->GetSurfaceWidth();
		const UINT Height = RenderTargetTexture->GetSurfaceHeight();

		// @todo MeshPaint: Need full color/alpha writes enabled to get alpha
		// @todo MeshPaint: Texels need to line up perfectly to avoid bilinear artifacts
		// @todo MeshPaint: Potential gamma issues here
		// @todo MeshPaint: Probably using CLAMP address mode when reading from source (if texels line up, shouldn't matter though.)

		// @todo MeshPaint: Should use scratch texture built from original source art (when possible!)
		//		-> Current method will have compression artifacts!


		// Grab the texture resource.  We only support 2D textures and render target textures here.
		FTexture* TextureResource = NULL;
		UTexture2D* Texture2D = Cast<UTexture2D>( SourceTexture );
		if( Texture2D != NULL )
		{
			TextureResource = Texture2D->Resource;
		}
		else
		{
			UTextureRenderTarget2D* TextureRenderTarget2D = Cast<UTextureRenderTarget2D>( SourceTexture );
			TextureResource = TextureRenderTarget2D->GameThread_GetRenderTargetResource();
		}
		check( TextureResource != NULL );


		// Draw a quad to copy the texture over to the render target
		{		
			const FLOAT MinU = 0.0f;
			const FLOAT MinV = 0.0f;
			const FLOAT MaxU = 1.0f;
			const FLOAT MaxV = 1.0f;
			const FLOAT MinX = 0.0f;
			const FLOAT MinY = 0.0f;
			const FLOAT MaxX = Width;
			const FLOAT MaxY = Height;

			DrawTriangle2D(
				&Canvas,
				FVector2D( MinX, MinY ),
				FVector2D( MinU, MinV ),
				FVector2D( MaxX, MinY ),
				FVector2D( MaxU, MinV ),
				FVector2D( MaxX, MaxY ),
				FVector2D( MaxU, MaxV ),
				FLinearColor::White,
				TextureResource,
				FALSE );		// Alpha blend?

			DrawTriangle2D(
				&Canvas,
				FVector2D( MaxX, MaxY ),
				FVector2D( MaxU, MaxV ),
				FVector2D( MinX, MaxY ),
				FVector2D( MinU, MaxV ),
				FVector2D( MinX, MinY ),
				FVector2D( MinU, MinV ),
				FLinearColor::White,
				TextureResource,
				FALSE );		// Alpha blend?
		}

		// Tell the rendering thread to draw any remaining batched elements
		Canvas.Flush(TRUE);
	}


	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			UpdateMeshPaintRTCommand4,
			FTextureRenderTargetResource*, RenderTargetResource, RenderTargetResource,
		{
			// Copy (resolve) the rendered image from the frame buffer to its render target texture
			RHICopyToResolveTarget(
				RenderTargetResource->GetRenderTargetSurface(),		// Source texture
				TRUE,												// Do we need the source image content again?
				FResolveParams() );									// Resolve parameters
		});

	}

	  /*
	// End scene
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		EndDrawingCommand,
		FViewport*,Viewport,Viewport,
	{
		const UBOOL bShouldPresent = FALSE;
		const UBOOL bLockToVSync = FALSE;
		Viewport->EndRenderFrame( bShouldPresent, bLockToVSync );
	});
	*/
}

/** Will generate a mask texture, used for texture dilation, and store it in the passed in rendertarget */
UBOOL FEdModeMeshPaint::GenerateSeamMask(UStaticMeshComponent* StaticMeshComponent, INT UVSet, UTextureRenderTarget2D* RenderTargetTexture)
{
	check(StaticMeshComponent != NULL);
	check(StaticMeshComponent->StaticMesh != NULL);
	check(RenderTargetTexture != NULL);
	check(StaticMeshComponent->StaticMesh->LODModels(PaintingMeshLODIndex).VertexBuffer.GetNumTexCoords() > (UINT)UVSet);
		
	UBOOL RetVal = FALSE;

	FStaticMeshRenderData& LODModel = StaticMeshComponent->StaticMesh->LODModels(PaintingMeshLODIndex);

	const UINT Width = RenderTargetTexture->GetSurfaceWidth();
	const UINT Height = RenderTargetTexture->GetSurfaceHeight();

	// Grab the actual render target resource from the texture.  Note that we're absolutely NOT ALLOWED to
	// dereference this pointer.  We're just passing it along to other functions that will use it on the render
	// thread.  The only thing we're allowed to do is check to see if it's NULL or not.
	FTextureRenderTargetResource* RenderTargetResource = RenderTargetTexture->GameThread_GetRenderTargetResource();
	check( RenderTargetResource != NULL );

	INT NumElements = StaticMeshComponent->GetNumElements();
	UTexture2D* TargetTexture2D = GetSelectedTexture();
	PaintTexture2DData* TextureData = GetPaintTargetData( TargetTexture2D );

	// Store info that tells us if the element material uses our target texture so we don't have to do a usestexture() call for each tri.  We will
	// use this info to eliminate triangles that do not use our texture.
	TArray< UBOOL > ElementUsesTargetTexture;
	ElementUsesTargetTexture.AddZeroed( NumElements );
	for ( INT ElementIndex = 0; ElementIndex < NumElements; ElementIndex++ )
	{
		ElementUsesTargetTexture( ElementIndex ) = FALSE;

		UMaterialInterface* ElementMat = StaticMeshComponent->GetElementMaterial( ElementIndex );
		if( ElementMat != NULL )
		{
			ElementUsesTargetTexture( ElementIndex ) |=  ElementMat->UsesTexture( TargetTexture2D );

			if( ElementUsesTargetTexture( ElementIndex ) == FALSE && TextureData != NULL && TextureData->PaintRenderTargetTexture != NULL)
			{
				// If we didn't get a match on our selected texture, we'll check to see if the the material uses a
				//  render target texture override that we put on during painting.
				ElementUsesTargetTexture( ElementIndex ) |=  ElementMat->UsesTexture( TextureData->PaintRenderTargetTexture );
			}
		}
	}

	// Make sure we're dealing with triangle lists
	const UINT NumIndexBufferIndices = LODModel.IndexBuffer.Indices.Num();
	check( NumIndexBufferIndices % 3 == 0 );
	const UINT NumTriangles = NumIndexBufferIndices / 3;

	static TArray< INT > InfluencedTriangles;
	InfluencedTriangles.Empty( NumTriangles );

	// For each triangle in the mesh
	for( UINT TriIndex = 0; TriIndex < NumTriangles; ++TriIndex )
	{
		// At least one triangle vertex was influenced.
		bool bAddTri = FALSE;

		// Check to see if the sub-element that this triangle belongs to actually uses our paint target texture in its material
		for (INT ElementIndex = 0; ElementIndex < NumElements; ElementIndex++)
		{
			FStaticMeshElement& Element = LODModel.Elements( ElementIndex );


			if( ( TriIndex >= Element.FirstIndex / 3 ) && 
				( TriIndex < Element.FirstIndex / 3 + Element.NumTriangles ) )
			{

				// The triangle belongs to this element, now we need to check to see if the element material uses our target texture.
				if( TargetTexture2D != NULL && ElementUsesTargetTexture( ElementIndex ) == TRUE)
				{
					bAddTri = TRUE;
				}

				// Triangles can only be part of one element so we do not need to continue to other elements.
				break;
			}

		}

		if( bAddTri )
		{
			InfluencedTriangles.AddItem( TriIndex );
		}

	}

	{
		// Create a canvas for the render target and clear it to white
		FCanvas Canvas( RenderTargetResource, NULL );
		Clear(&Canvas, FLinearColor::White);

		for( INT CurIndex = 0; CurIndex < InfluencedTriangles.Num(); ++CurIndex )
		{
			const INT TriIndex = InfluencedTriangles( CurIndex );

			// Grab the vertex indices and points for this triangle
			FVector2D TriUVs[ 3 ];
			FVector2D UVMin( 99999.9f, 99999.9f );
			FVector2D UVMax( -99999.9f, -99999.9f );
			for( INT TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum )
			{																		 
				const INT VertexIndex = LODModel.IndexBuffer.Indices( TriIndex * 3 + TriVertexNum );
				TriUVs[ TriVertexNum ] = LODModel.VertexBuffer.GetVertexUV( VertexIndex, UVSet );

				// Update bounds
				FLOAT U = TriUVs[ TriVertexNum ].X;
				FLOAT V = TriUVs[ TriVertexNum ].Y;

				if( U < UVMin.X )
				{
					UVMin.X = U;
				}
				if( U > UVMax.X )
				{
					UVMax.X = U;
				}
				if( V < UVMin.Y )
				{
					UVMin.Y = V;
				}
				if( V > UVMax.Y )
				{
					UVMax.Y = V;
				}

			}

			// If the triangle lies entirely outside of the 0.0-1.0 range, we'll transpose it back
			FVector2D UVOffset( 0.0f, 0.0f );
			if( UVMax.X > 1.0f )
			{
				UVOffset.X = -appFloor( UVMin.X );
			}
			else if( UVMin.X < 0.0f )
			{
				UVOffset.X = 1.0f + appFloor( -UVMax.X );
			}

			if( UVMax.Y > 1.0f )
			{
				UVOffset.Y = -appFloor( UVMin.Y );
			}
			else if( UVMin.Y < 0.0f )
			{
				UVOffset.Y = 1.0f + appFloor( -UVMax.Y );
			}


			// Note that we "wrap" the texture coordinates here to handle the case where the user
			// is painting on a tiling texture, or with the UVs out of bounds.  Ideally all of the
			// UVs would be in the 0.0 - 1.0 range but sometimes content isn't setup that way.
			// @todo MeshPaint: Handle triangles that cross the 0.0-1.0 UV boundary?
			FVector2D TrianglePoints[ 3 ];
			for( INT TriVertexNum = 0; TriVertexNum < 3; ++TriVertexNum )
			{
				TriUVs[ TriVertexNum ].X += UVOffset.X;
				TriUVs[ TriVertexNum ].Y += UVOffset.Y;

				TrianglePoints[ TriVertexNum ].X = TriUVs[ TriVertexNum ].X * Width;
				TrianglePoints[ TriVertexNum ].Y = TriUVs[ TriVertexNum ].Y * Height;
			}


			DrawTriangle2D(
				&Canvas,
				TrianglePoints[ 0 ],
				TriUVs[ 0 ],
				TrianglePoints[ 1 ],
				TriUVs[ 1 ],
				TrianglePoints[ 2 ],
				TriUVs[ 2 ],
				FLinearColor::Black,
				RenderTargetResource,
				FALSE );		// Alpha blend?

		}

		// Tell the rendering thread to draw any remaining batched elements
		Canvas.Flush(TRUE);
	}


	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			UpdateMeshPaintRTCommand5,
			FTextureRenderTargetResource*, RenderTargetResource, RenderTargetResource,
		{
			// Copy (resolve) the rendered image from the frame buffer to its render target texture
			RHICopyToResolveTarget(
				RenderTargetResource->GetRenderTargetSurface(),		// Source texture
				TRUE,												// Do we need the source image content again?
				FResolveParams() );									// Resolve parameters
		});

	}

	return RetVal;
}

/** Helper function to get the current paint action for use in DoPaint */
EMeshPaintAction::Type FEdModeMeshPaint::GetPaintAction(FViewport* InViewport)
{
	check(InViewport);
	const UBOOL bShiftDown = InViewport->KeyState( KEY_LeftShift ) || InViewport->KeyState( KEY_RightShift );
	EMeshPaintAction::Type PaintAction;
	if (bIsFloodFill)
	{
		PaintAction = EMeshPaintAction::Fill;
		//turn off so we don't do this next frame!
		bIsFloodFill = FALSE;
	}
	else if (bPushInstanceColorsToMesh)
	{
		PaintAction = EMeshPaintAction::PushInstanceColorsToMesh;
		//turn off so we don't do this next frame!
		bPushInstanceColorsToMesh = FALSE;
	}
	else
	{
		PaintAction = bShiftDown ? EMeshPaintAction::Erase : EMeshPaintAction::Paint;
	}
	return PaintAction;

}

/** Removes vertex colors associated with the object (if it's a StaticMeshActor or DynamicSMActor) */
void FEdModeMeshPaint::RemoveInstanceVertexColors(UObject* Obj) const
{
	UStaticMeshComponent* StaticMeshComponent = NULL;
	AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( Obj );
	if( StaticMeshActor != NULL )
	{
		StaticMeshComponent = StaticMeshActor->StaticMeshComponent;
	}
	else
	{
		ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( Obj );
		if( DynamicSMActor != NULL )
		{
			StaticMeshComponent = DynamicSMActor->StaticMeshComponent;
		}
	}

	if( StaticMeshComponent != NULL )
	{
		check( StaticMeshComponent->StaticMesh->LODModels.Num() > PaintingMeshLODIndex );

		// Make sure we have component-level LOD information
		if( StaticMeshComponent->LODData.Num() > PaintingMeshLODIndex )
		{
			FStaticMeshComponentLODInfo* InstanceMeshLODInfo = &StaticMeshComponent->LODData( PaintingMeshLODIndex );

			if(InstanceMeshLODInfo->OverrideVertexColors)
			{
				// Detach all instances of this static mesh from the scene.
				FComponentReattachContext ComponentReattachContext( StaticMeshComponent );

				// @todo MeshPaint: Should make this undoable

				// Mark the mesh component as modified
				StaticMeshComponent->Modify();

				InstanceMeshLODInfo->ReleaseOverrideVertexColorsAndBlock();

				// With no colors, there's no longer a reason to store vertex color positions. Remove them and count
				// the component as up-to-date with the source mesh.
				InstanceMeshLODInfo->PaintedVertices.Empty();
				StaticMeshComponent->VertexPositionVersionNumber = StaticMeshComponent->StaticMesh->VertexPositionVersionNumber;

#if WITH_MANAGED_CODE
					// The instance vertex color byte count has changed so tell the mesh paint window
					// to refresh it's properties
					if( MeshPaintWindow != NULL )
					{
						MeshPaintWindow->RefreshAllProperties();
					}
#endif
			}
		}
	}
}

/** Removes vertex colors associated with the currently selected mesh */
void FEdModeMeshPaint::RemoveInstanceVertexColors() const
{
	USelection& SelectedActors = *GEditor->GetSelectedActors();
	for( INT CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex )
	{
		RemoveInstanceVertexColors( SelectedActors( CurSelectedActorIndex ) );
	}
}

/** Copies vertex colors associated with the currently selected mesh */
void FEdModeMeshPaint::CopyInstanceVertexColors()
{
	CopiedColorsByLOD.Empty();

	USelection& SelectedActors = *GEditor->GetSelectedActors();
	if( SelectedActors.Num() != 1 )
	{
		// warning - works only with 1 actor selected..!
	}
	else
	{
		UStaticMeshComponent* StaticMeshComponent = NULL;
		UObject *CurrentObject = SelectedActors( 0 );
		AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( CurrentObject );
		if( StaticMeshActor != NULL )
		{
			StaticMeshComponent = StaticMeshActor->StaticMeshComponent;
		}
		else
		{
			ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( CurrentObject );
			if( DynamicSMActor != NULL )
			{
				StaticMeshComponent = DynamicSMActor->StaticMeshComponent;
			}
		}

		if( StaticMeshComponent )
		{
			for( INT CurLODIndex = 0; CurLODIndex < StaticMeshComponent->StaticMesh->LODModels.Num(); ++CurLODIndex )
			{
				FPerLODVertexColorData& LodColorData = CopiedColorsByLOD(CopiedColorsByLOD.AddZeroed());

				UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh;
				FStaticMeshRenderData& LODModel = StaticMeshComponent->StaticMesh->LODModels( CurLODIndex );
				FColorVertexBuffer* ColBuffer = &LODModel.ColorVertexBuffer;
				FPositionVertexBuffer* PosBuffer = &LODModel.PositionVertexBuffer;

				// Is there an override buffer? If so, copy colors from there instead...
				if( StaticMeshComponent->LODData.Num() > CurLODIndex )
				{
					FStaticMeshComponentLODInfo& ComponentLODInfo = StaticMeshComponent->LODData( CurLODIndex );
					if( ComponentLODInfo.OverrideVertexColors )
					{
						ColBuffer = ComponentLODInfo.OverrideVertexColors;
					}
				}

				// Copy the colour buffer
				if( ColBuffer && PosBuffer )
				{
					UINT NumColVertices = ColBuffer->GetNumVertices();
					UINT NumPosVertices = PosBuffer->GetNumVertices();

					if (NumColVertices == NumPosVertices)
					{
						// valid color buffer matching the pos verts
						for( UINT VertexIndex = 0; VertexIndex < NumColVertices; VertexIndex++ )
						{
							LodColorData.ColorsByIndex.AddItem( ColBuffer->VertexColor( VertexIndex ) );
							LodColorData.ColorsByPosition.Set( PosBuffer->VertexPosition( VertexIndex ), ColBuffer->VertexColor( VertexIndex ) );
						}						
					}
					else
					{
						// mismatched or empty color buffer - just use white
						for( UINT VertexIndex = 0; VertexIndex < NumPosVertices; VertexIndex++ )
						{
							LodColorData.ColorsByIndex.AddItem( FColor(255,255,255,255) );
							LodColorData.ColorsByPosition.Set( PosBuffer->VertexPosition( VertexIndex ), FColor(255,255,255,255) );
						}
					}
				}
			}
		}
	}
}

/** Pastes vertex colors to the currently selected mesh */
void FEdModeMeshPaint::PasteInstanceVertexColors()
{
	INT NumLodsInCopyBuffer = CopiedColorsByLOD.Num();
	if (0 == NumLodsInCopyBuffer)
	{
		return;
	}

	USelection& SelectedActors = *GEditor->GetSelectedActors();
	TScopedPointer< FComponentReattachContext > ComponentReattachContext;

	for( INT iMesh = 0; iMesh < SelectedActors.Num(); iMesh++ )
	{
		UStaticMeshComponent* StaticMeshComponent = NULL;
		UObject* CurrentObject = SelectedActors( iMesh );
		AActor* CurrentActor = Cast< AActor >( CurrentObject );
		AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( CurrentObject );
		if( StaticMeshActor != NULL )
		{
			CurrentActor = StaticMeshActor;
			StaticMeshComponent = StaticMeshActor->StaticMeshComponent;
		}
		else
		{
			ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( CurrentObject );
			if( DynamicSMActor != NULL )
			{
				CurrentActor = DynamicSMActor;
				StaticMeshComponent = DynamicSMActor->StaticMeshComponent;
			}
		}

		if( StaticMeshComponent )
		{
			INT NumLods = StaticMeshComponent->StaticMesh->LODModels.Num();
			if (0 == NumLods)
			{
				continue;
			}

			ComponentReattachContext.Reset( new FComponentReattachContext( StaticMeshComponent ) );
			StaticMeshComponent->Modify();
			StaticMeshComponent->SetLODDataCount( NumLods, NumLods );
			RemoveInstanceVertexColors( CurrentActor );

			for( INT CurLODIndex = 0; CurLODIndex < NumLods; ++CurLODIndex )
			{
				FStaticMeshRenderData& LodRenderData = StaticMeshComponent->StaticMesh->LODModels(CurLODIndex);
				FStaticMeshComponentLODInfo& ComponentLodInfo = StaticMeshComponent->LODData(CurLODIndex);

				TArray< FColor > ReOrderedColors;
				TArray< FColor >* PasteFromBufferPtr = &ReOrderedColors;

				if (CurLODIndex >= NumLodsInCopyBuffer)
				{
					// no corresponding LOD in color paste buffer CopiedColorsByLOD
					// create array of all white verts
					ReOrderedColors.Add(LodRenderData.NumVertices);

					for (UINT TargetVertIdx = 0; TargetVertIdx < LodRenderData.NumVertices; TargetVertIdx++)
					{
						ReOrderedColors(TargetVertIdx) = FColor(255,255,255,255);
					}
				}
				else if (LodRenderData.NumVertices == CopiedColorsByLOD(CurLODIndex).ColorsByIndex.Num())
				{
					// verts counts match - copy from color array by index
					PasteFromBufferPtr = &(CopiedColorsByLOD(CurLODIndex).ColorsByIndex);
				}
				else
				{
					// verts counts mismatch - build translation/fixup list of colors in ReOrderedColors
					ReOrderedColors.Add(LodRenderData.NumVertices);

					// make ReOrderedColors contain one FColor for each vertex in the target mesh
					// matching the position of the target's vert to the position values in LodColorData.ColorsByPosition
					check (LodRenderData.NumVertices == LodRenderData.PositionVertexBuffer.GetNumVertices());
					for (UINT TargetVertIdx = 0; TargetVertIdx < LodRenderData.NumVertices; TargetVertIdx++)
					{
						const FColor* FoundColor =
							CopiedColorsByLOD(CurLODIndex).ColorsByPosition.Find(LodRenderData.PositionVertexBuffer.VertexPosition(TargetVertIdx));

						if (FoundColor)
						{
							// A matching color for this vertex was found
							ReOrderedColors(TargetVertIdx) = *FoundColor;
						}
						else
						{
							// A matching color for this vertex could not be found. Make this vertex white
							ReOrderedColors(TargetVertIdx) = FColor(255,255,255,255);
						}
					}
				}

				if( ComponentLodInfo.OverrideVertexColors )
				{
					ComponentLodInfo.ReleaseOverrideVertexColorsAndBlock();
				}
				if( ComponentLodInfo.OverrideVertexColors )
				{
					ComponentLodInfo.BeginReleaseOverrideVertexColors();
					FlushRenderingCommands();
				}
				else
				{
					ComponentLodInfo.OverrideVertexColors = new FColorVertexBuffer;
					ComponentLodInfo.OverrideVertexColors->InitFromColorArray( *PasteFromBufferPtr );
				}
				BeginInitResource( ComponentLodInfo.OverrideVertexColors );				
			}

			StaticMeshComponent->CachePaintedDataIfNecessary();
			StaticMeshComponent->VertexPositionVersionNumber = StaticMeshComponent->StaticMesh->VertexPositionVersionNumber;

			// The instance vertex color byte count has changed so tell the mesh paint window to refresh it's properties
#if WITH_MANAGED_CODE
			if( MeshPaintWindow != NULL )
			{
				MeshPaintWindow->RefreshAllProperties();
			}
#endif
		}
	}
}

/** Returns whether the instance vertex colors associated with the currently selected mesh need to be fixed up or not */
UBOOL FEdModeMeshPaint::RequiresInstanceVertexColorsFixup() const
{
	UBOOL bRequiresFixup = FALSE;

	// Find each static mesh component of any selected actors
	USelection& SelectedActors = *GEditor->GetSelectedActors();
	for( INT CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex )
	{
		UStaticMeshComponent* StaticMeshComponent = NULL;
		AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( SelectedActors( CurSelectedActorIndex ) );
		if( StaticMeshActor )
		{
			StaticMeshComponent = StaticMeshActor->StaticMeshComponent;
		}
		else
		{
			ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( SelectedActors( CurSelectedActorIndex ) );
			if( DynamicSMActor )
			{
				StaticMeshComponent = DynamicSMActor->StaticMeshComponent;
			}
		}

		// If a static mesh component was found and it requires fixup, exit out and indicate as such
		TArray<INT> LODsToFixup;
		if( StaticMeshComponent && StaticMeshComponent->RequiresOverrideVertexColorsFixup( LODsToFixup ) )
		{
			bRequiresFixup = TRUE;
			break;
		}
	}

	return bRequiresFixup;
}

/** Attempts to fix up the instance vertex colors associated with the currently selected mesh, if necessary */
void FEdModeMeshPaint::FixupInstanceVertexColors() const
{
	// Find each static mesh component of any selected actors
	USelection& SelectedActors = *GEditor->GetSelectedActors();
	for( INT CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex )
	{
		UStaticMeshComponent* StaticMeshComponent = NULL;
		AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( SelectedActors( CurSelectedActorIndex ) );
		if( StaticMeshActor != NULL )
		{
			StaticMeshComponent = StaticMeshActor->StaticMeshComponent;
		}
		else
		{
			ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( SelectedActors( CurSelectedActorIndex ) );
			if( DynamicSMActor != NULL )
			{
				StaticMeshComponent = DynamicSMActor->StaticMeshComponent;
			}
		}

		// If a static mesh component was found, attempt to fixup its override colors
		if( StaticMeshComponent )
		{
			StaticMeshComponent->FixupOverrideColorsIfNecessary();
		}
	}

#if WITH_MANAGED_CODE
	if( MeshPaintWindow != NULL )
	{
		MeshPaintWindow->RefreshAllProperties();
	}
#endif
}

/** Fills the vertex colors associated with the currently selected mesh*/
void FEdModeMeshPaint::FillInstanceVertexColors()
{
	//force this on for next render
	bIsFloodFill = TRUE;
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

/** Pushes instance vertex colors to the  mesh*/
void FEdModeMeshPaint::PushInstanceVertexColorsToMesh()
{
	INT NumBaseVertexColorBytes = 0;
	INT NumInstanceVertexColorBytes = 0;
	UBOOL bHasInstanceMaterialAndTexture = FALSE;

	// Check that there's actually a mesh selected and that it has instanced vertex colors before actually proceeding
	const UBOOL bMeshSelected = GetSelectedMeshInfo( NumBaseVertexColorBytes, NumInstanceVertexColorBytes, bHasInstanceMaterialAndTexture );
	if ( bMeshSelected && NumInstanceVertexColorBytes > 0 )
	{
		// Prompt the user to see if they really want to push the vert colors to the source mesh and to explain
		// the ramifications of doing so
		WxChoiceDialog ConfirmationPrompt(
			LocalizeUnrealEd("PushInstanceVertexColorsPrompt_Message"), 
			LocalizeUnrealEd("PushInstanceVertexColorsPrompt_Title"),
			WxChoiceDialogBase::Choice( ART_Yes, LocalizeUnrealEd("PushInstanceVertexColorsPrompt_Ok"), WxChoiceDialogBase::DCT_DefaultAffirmative ),
			WxChoiceDialogBase::Choice( ART_No, LocalizeUnrealEd("PushInstanceVertexColorsPrompt_Cancel"), WxChoiceDialogBase::DCT_DefaultCancel ) );
		ConfirmationPrompt.ShowModal();
		
		if ( ConfirmationPrompt.GetChoice().ReturnCode == ART_Yes )
		{
			//force this on for next render
			bPushInstanceColorsToMesh = TRUE;
			GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
		}
	}
}

/** Creates a paintable material/texture for the selected mesh */
void FEdModeMeshPaint::CreateInstanceMaterialAndTexture() const
{
	// @todo MeshPaint: NOT supported at this time.
	return;

/*
	USelection& SelectedActors = *GEditor->GetSelectedActors();
	for( INT CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex )
	{
		AActor* CurSelectedActor = CastChecked<AActor>( SelectedActors( CurSelectedActorIndex ) );
		UStaticMeshComponent* StaticMeshComponent = NULL;
		AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( CurSelectedActor );
		if( StaticMeshActor != NULL )
		{
			StaticMeshComponent = StaticMeshActor->StaticMeshComponent;
		}
		else
		{
			ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( CurSelectedActor );
			if( DynamicSMActor != NULL )
			{
				StaticMeshComponent = DynamicSMActor->StaticMeshComponent;
			}
		}
		if( StaticMeshComponent != NULL )
		{
			check( StaticMeshComponent->StaticMesh->LODModels.Num() > PaintingMeshLODIndex );
			const FStaticMeshLODInfo& LODInfo = StaticMeshComponent->StaticMesh->LODInfo( PaintingMeshLODIndex );

			// @todo MeshPaint: Make mesh element selectable?
			const INT MeshElementIndex = 0;

			UMaterialInterface* OriginalMaterial = NULL;

			// First check to see if we have an existing override material on the actor instance.
			if( StaticMeshComponent->Materials.Num() > MeshElementIndex )
			{
				if( StaticMeshComponent->Materials( MeshElementIndex ) != NULL )
				{
					OriginalMaterial = StaticMeshComponent->Materials( MeshElementIndex );
				}
			}

			if( OriginalMaterial == NULL )
			{
				// Grab the material straight from the mesh
				if( LODInfo.Elements.Num() > MeshElementIndex )
				{
					const FStaticMeshLODElement& ElementInfo = LODInfo.Elements( MeshElementIndex );
					if( ElementInfo.Material != NULL )
					{
						OriginalMaterial = ElementInfo.Material;
					}
				}
			}


			if( OriginalMaterial != NULL )
			{
				// @todo MeshPaint: Undo support

				FComponentReattachContext ComponentReattachContext( StaticMeshComponent );


				// Create the new texture
				UTexture2D* NewTexture = NULL;
				{
					// @todo MeshPaint: Make configurable
					// @todo MeshPaint: Expose compression options? (default to off?)
					// @todo MeshPaint: Expose SRGB/RGBE options?
					// @todo MeshPaint: Use bWantSourceArt=FALSE to speed this up
					const INT TexWidth = 1024;
					const INT TexHeight = 1024;
					const UBOOL bUseAlpha = TRUE;

					TArray< FColor > ImageData;
					ImageData.Add( TexWidth * TexHeight );
					const FColor WhiteColor( 255, 255, 255, 255 );
					for( INT CurTexelIndex = 0; CurTexelIndex < ImageData.Num(); ++CurTexelIndex )
					{
						ImageData( CurTexelIndex ) = WhiteColor;
					}

					FCreateTexture2DParameters Params;
					Params.bUseAlpha = bUseAlpha;

					NewTexture = FImageUtils::CreateTexture2D(
						TexWidth,
						TexHeight,
						ImageData,
						CurSelectedActor->GetOuter(),
						FString::Printf( TEXT( "%s_PaintTexture" ), *CurSelectedActor->GetName() ),
						RF_Public,
						Params );

					NewTexture->MarkPackageDirty();
				}


				// Create the new material instance
				UMaterialInstanceConstant* NewMaterialInstance = NULL;
				{
					NewMaterialInstance = CastChecked<UMaterialInstanceConstant>( UObject::StaticConstructObject(
						UMaterialInstanceConstant::StaticClass(), 
						CurSelectedActor->GetOuter(), 
						FName( *FString::Printf( TEXT( "%s_PaintMIC" ), *CurSelectedActor->GetName() ) ),
						RF_Public
						));

					// Bind texture to the material instance
					NewMaterialInstance->SetTextureParameterValue( PaintTextureName, NewTexture );

					NewMaterialInstance->SetParent( OriginalMaterial );
					NewMaterialInstance->MarkPackageDirty();
				}

				
				// Assign material to the static mesh component
				{
					StaticMeshComponent->Modify();
					while( StaticMeshComponent->Materials.Num() <= MeshElementIndex )
					{
						StaticMeshComponent->Materials.AddZeroed();
					}
					StaticMeshComponent->Materials( MeshElementIndex ) = NewMaterialInstance;
				}
			}
		}
	}
*/
}



/** Removes instance of paintable material/texture for the selected mesh */
void FEdModeMeshPaint::RemoveInstanceMaterialAndTexture() const
{
	USelection& SelectedActors = *GEditor->GetSelectedActors();
	for( INT CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex )
	{
		AActor* CurSelectedActor = CastChecked<AActor>( SelectedActors( CurSelectedActorIndex ) );
		UStaticMeshComponent* StaticMeshComponent = NULL;
		AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( CurSelectedActor );
		if( StaticMeshActor != NULL )
		{
			StaticMeshComponent = StaticMeshActor->StaticMeshComponent;
		}
		else
		{
			ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( CurSelectedActor );
			if( DynamicSMActor != NULL )
			{
				StaticMeshComponent = DynamicSMActor->StaticMeshComponent;
			}
		}

		if( StaticMeshComponent != NULL )
		{
			// ...
		}
	}
}



/** Returns information about the currently selected mesh */
UBOOL FEdModeMeshPaint::GetSelectedMeshInfo( INT& OutTotalBaseVertexColorBytes, INT& OutTotalInstanceVertexColorBytes, UBOOL& bOutHasInstanceMaterialAndTexture ) const
{
	OutTotalInstanceVertexColorBytes = 0;
	OutTotalBaseVertexColorBytes = 0;
	bOutHasInstanceMaterialAndTexture = FALSE;

	INT NumValidMeshes = 0;

	USelection& SelectedActors = *GEditor->GetSelectedActors();
	for( INT CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex )
	{
		AActor* CurSelectedActor = CastChecked<AActor>( SelectedActors( CurSelectedActorIndex ) );
		UStaticMeshComponent* StaticMeshComponent = NULL;
		AStaticMeshActor* StaticMeshActor = Cast< AStaticMeshActor >( CurSelectedActor );
		if( StaticMeshActor != NULL )
		{
			StaticMeshComponent = StaticMeshActor->StaticMeshComponent;
		}
		else
		{
			ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( CurSelectedActor );
			if( DynamicSMActor != NULL )
			{
				StaticMeshComponent = DynamicSMActor->StaticMeshComponent;
			}
		}
		if( StaticMeshComponent != NULL )
		{
			check( StaticMeshComponent->StaticMesh->LODModels.Num() > PaintingMeshLODIndex );

			// count the base mesh color data
			FStaticMeshRenderData& LODModel = StaticMeshComponent->StaticMesh->LODModels( PaintingMeshLODIndex );
			OutTotalBaseVertexColorBytes += LODModel.ColorVertexBuffer.GetNumVertices();

			// count the instance color data
			if( StaticMeshComponent->LODData.Num() > PaintingMeshLODIndex )
			{
				const FStaticMeshComponentLODInfo& InstanceMeshLODInfo = StaticMeshComponent->LODData( PaintingMeshLODIndex );
				if( InstanceMeshLODInfo.OverrideVertexColors )
				{
					OutTotalInstanceVertexColorBytes += InstanceMeshLODInfo.OverrideVertexColors->GetAllocatedSize();
				}
			}

			++NumValidMeshes;
		}
	}

	return ( NumValidMeshes > 0 );
}

/**
* PickVertexColorFromTex() - Color picker function. Retrieves pixel color from coordinates and mask
* @param NewVertexColor - returned color
* @param MipData - Highest mip-map with pixels data
* @param UV - texture coordinate to read
* @param Tex - texture info
* @param ColorMask - mask for filtering which colors to use
*/
void ImportVertexTextureHelper::PickVertexColorFromTex(FColor & NewVertexColor, BYTE* MipData, FVector2D & UV, UTexture2D* Tex, BYTE & ColorMask)
{	
	check(MipData);
	NewVertexColor = FColor(0,0,0, 0);

	if (UV.X >=0 && UV.X <1 && UV.Y >=0 && UV.Y <1)
	{
		INT x =  Tex->SizeX*UV.X;
		INT y =  Tex->SizeY*UV.Y;

		const INT idx = ((y * Tex->SizeX) + x) * 4;
		BYTE B = MipData[idx];
		BYTE G = MipData[idx+1];
		BYTE R = MipData[idx+2];
		BYTE A = MipData[idx+3];

		if (ColorMask & ChannelsMask::ERed)
			NewVertexColor.R = R;
		if (ColorMask & ChannelsMask::EGreen)
			NewVertexColor.G = G;
		if (ColorMask & ChannelsMask::EBlue)
			NewVertexColor.B = B;
		if (ColorMask & ChannelsMask::EAlpha)
			NewVertexColor.A = A ;
	}	
}
/**
* Imports Vertex Color date from texture scanning thought uv vertex coordinates for selected actors.  
* @param Path - path for loading TGA file
* @param UV - Coordinate index
* @param ImportLOD - LOD level to work with
* @param Tex - texture info
* @param ColorMask - mask for filtering which colors to use
*/
void ImportVertexTextureHelper::Apply(FString & filename, INT UVIndex, INT PaintingMeshLODIndex, BYTE ColorMask)
{
	if (filename.Len() == 0)
	{
		appMsgf( AMT_OK, TEXT("Fail: Path invalid!")  );
		return;
	}
	TArray<UObject*> Actors;
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{

		AStaticMeshActor* StaticActor = Cast<AStaticMeshActor>( *It );

		if(StaticActor)
		{
			Actors.AddUniqueItem(*It);
		}
		ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( *It );
		if(DynamicSMActor)
		{
			Actors.AddUniqueItem(*It);
		}
	}
	if (Actors.Num() <1) 
	{
		appMsgf( AMT_OK, TEXT("Fail: No valid actors selected!")  );
		return;
	}

	if (filename.Len() ==0)
	{
		appMsgf( AMT_OK, TEXT("Fail: No tga file specified !")  );
		return;
	}

	if (ColorMask == 0)
	{
		appMsgf( AMT_OK, TEXT("Fail: No Channels Mask selected!")  );
		return;
	}

	UBOOL bComponent =(FMeshPaintSettings::Get().VertexPaintTarget == EMeshVertexPaintTarget::ComponentInstance);

	const FString FullFilename = filename;
	UTexture2D* Tex = ImportObject<UTexture2D>( GEngine, NAME_None, RF_Public, *FullFilename, NULL, NULL, TEXT("NOMIPMAPS=1 NOCOMPRESSION=1") );
	// If we can't load the file from the disk, create a small empty image as a placeholder and return that instead.
	if( !Tex )
	{
		//GWarn->Logf( NAME_Warning, TEXT("Error: Importing Failed : Couldn't load '%s'"), *FullFilename );
		appMsgf( AMT_OK, TEXT("Error: Importing Failed : Couldn't load '%s'"), *FullFilename  );
		return;
	}
	if (Tex->Format != PF_A8R8G8B8)
	{
		appMsgf( AMT_OK, TEXT("Error: Tga format is not supported. Use RGBA uncompressed file!"));
		return;
	}
	BYTE* MipData = (BYTE*) Tex->Mips(0).Data.Lock(LOCK_READ_ONLY);
	if (!MipData)
	{
		appMsgf( AMT_OK, TEXT("Error: Can`t get mip level 0 rom loaded tga"));
		return;
	}
	TArray <UStaticMesh*> ModifiedStaticMeshes;

	for (INT i =0; i < Actors.Num() ; ++i)
	{
		//AStaticMeshActor* _Actor = Actors(i);

		AStaticMeshActor* StaticActor = Cast<AStaticMeshActor>( Actors(i) );
		ADynamicSMActor* DynamicSMActor = Cast< ADynamicSMActor >( Actors(i) );

		UStaticMeshComponent* StaticMeshComponent = NULL;
		UStaticMesh* StaticMesh = NULL; 
		if (StaticActor)
		{
			StaticMeshComponent = StaticActor->StaticMeshComponent;
			StaticMesh = StaticMeshComponent->StaticMesh;
		}
		else if (DynamicSMActor)
		{
			StaticMeshComponent = DynamicSMActor->StaticMeshComponent;
			StaticMesh = StaticMeshComponent->StaticMesh;
		}

		if (!StaticMeshComponent || !StaticMesh ) continue;

		if (PaintingMeshLODIndex >= StaticMesh->LODModels.Num() )
		{
			continue;
		}
		FStaticMeshRenderData& LODModel = StaticMesh->LODModels( PaintingMeshLODIndex );

		TScopedPointer< FStaticMeshComponentReattachContext > MeshComponentReattachContext;
		TScopedPointer< FComponentReattachContext > ComponentReattachContext;

		FStaticMeshComponentLODInfo* InstanceMeshLODInfo = NULL;

		if (UVIndex >= (INT)LODModel.VertexBuffer.GetNumTexCoords()) 
		{
			continue;
		}

		if (bComponent)
		{
			ComponentReattachContext.Reset( new FComponentReattachContext( StaticMeshComponent ) );
			StaticMeshComponent->Modify();

			// Ensure LODData has enough entries in it, free not required.
			StaticMeshComponent->SetLODDataCount(PaintingMeshLODIndex + 1, StaticMeshComponent->LODData.Num());

			InstanceMeshLODInfo = &StaticMeshComponent->LODData( PaintingMeshLODIndex );
			InstanceMeshLODInfo->ReleaseOverrideVertexColorsAndBlock();

			if(InstanceMeshLODInfo->OverrideVertexColors)
			{
				InstanceMeshLODInfo->BeginReleaseOverrideVertexColors();
				FlushRenderingCommands();
			}
			else
			{
				// Setup the instance vertex color array if we don't have one yet
				InstanceMeshLODInfo->OverrideVertexColors = new FColorVertexBuffer;

				if(LODModel.ColorVertexBuffer.GetNumVertices() >= LODModel.NumVertices)
				{
					// copy mesh vertex colors to the instance ones
					InstanceMeshLODInfo->OverrideVertexColors->InitFromColorArray(&LODModel.ColorVertexBuffer.VertexColor(0), LODModel.NumVertices);
				}
				else
				{
					// Original mesh didn't have any colors, so just use a default color
					InstanceMeshLODInfo->OverrideVertexColors->InitFromSingleColor(FColor( 255, 255, 255, 255 ), LODModel.NumVertices);
				}
			}
		}
		else
		{
			if (PaintingMeshLODIndex >= StaticMesh->LODInfo.Num() )
			{
				continue;
			}

			if (ModifiedStaticMeshes.FindItemIndex(StaticMesh) != INDEX_NONE)
			{
				continue;
			}
			else
			{
				ModifiedStaticMeshes.AddUniqueItem(StaticMesh);
			}
			// We're changing the mesh itself, so ALL static mesh components in the scene will need
			// to be detached for this (and reattached afterwards.)
			MeshComponentReattachContext.Reset( new FStaticMeshComponentReattachContext( StaticMesh ) );

			// Dirty the mesh
			StaticMesh->Modify();


			// Release the static mesh's resources.
			StaticMesh->ReleaseResources();

			// Flush the resource release commands to the rendering thread to ensure that the build doesn't occur while a resource is still
			// allocated, and potentially accessing the UStaticMesh.
			StaticMesh->ReleaseResourcesFence.Wait();		

			if( LODModel.ColorVertexBuffer.GetNumVertices() == 0 )
			{
				// Mesh doesn't have a color vertex buffer yet!  We'll create one now.
				LODModel.ColorVertexBuffer.InitFromSingleColor(FColor( 255, 255, 255, 255), LODModel.NumVertices);

				// @todo MeshPaint: Make sure this is the best place to do this
				BeginInitResource( &LODModel.ColorVertexBuffer );
			}

		}

		FColor NewVertexColor;
		for( UINT VertexIndex = 0 ; VertexIndex < LODModel.VertexBuffer.GetNumVertices() ; ++VertexIndex )
		{
			FVector2D UV = LODModel.VertexBuffer.GetVertexUV(VertexIndex,UVIndex) ;
			PickVertexColorFromTex(NewVertexColor, MipData, UV, Tex, ColorMask);
			if (bComponent)
			{
				InstanceMeshLODInfo->OverrideVertexColors->VertexColor( VertexIndex ) = NewVertexColor;
			}
			else
			{
				LODModel.ColorVertexBuffer.VertexColor( VertexIndex ) = NewVertexColor;
			}
		}
		if (bComponent)
		{
			BeginInitResource(InstanceMeshLODInfo->OverrideVertexColors);
		}
		else
		{
			StaticMesh->InitResources();
		}
	}
	Tex->Mips(0).Data.Unlock();
}


/** 
 * Will update the list of available texture paint targets based on selection 
 */
void FEdModeMeshPaint::UpdateTexturePaintTargetList()
{
	if( bShouldUpdateTextureList && FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture )
	{
		// We capture the user texture selection before the refresh.  If this texture appears in the
		//  list after the update we will make it the initial selection.
		UTexture2D* PreviouslySelectedTexture = GetSelectedTexture();

		TexturePaintTargetList.Empty();

		TArray<UMaterialInterface*> MaterialsInSelection;
		TArray<UTexture*> TexturesInSelection;

		// Iterate over selected static mesh components
		TArray<UStaticMeshComponent*> SMComponents = GetValidStaticMeshComponents();
		for( INT CurSMIndex = 0; CurSMIndex < SMComponents.Num(); ++CurSMIndex )
		{
			TArray<UMaterialInterface*> UsedMaterials;

			// Currently we only support static mesh actors or dynamic static mesh actors
			UStaticMeshComponent* StaticMesh = SMComponents(CurSMIndex);

			// Get all the used materials for this StaticMeshComponent
			StaticMesh->GetUsedMaterials( UsedMaterials );

			// Add the materials this actor uses to the list we maintain for ALL the selected actors, but only if
			//  it does not appear in the list already.
			for( INT UsedIndex = 0; UsedIndex < UsedMaterials.Num(); UsedIndex++)
			{
				if( UsedMaterials( UsedIndex ) != NULL )
				{
					MaterialsInSelection.AddUniqueItem( UsedMaterials( UsedIndex ) );			
				}
			}
		}

		// Find all the unique textures used in the selected actor materials
		for( INT MatIndex = 0; MatIndex < MaterialsInSelection.Num(); MatIndex++ )
		{
			UMaterialInterface* Material = MaterialsInSelection( MatIndex );

			TArray<UTexture*> UsedTextures;
			Material->GetUsedTextures( UsedTextures, MSQ_UNSPECIFIED, FALSE, FALSE );

			// Find all the textures that belong to normal parameters.
			//TArray<FName> NormalParameterNames;
			//TArray<FGuid> Guids;
			//TArray<UTexture*> NormalTexturesInSelection;
			//Material->GetMaterial()->GetAllNormalParameterNames( NormalParameterNames, Guids );

			//// Once this loop is done we should have all the textures that are pluged into normal parameters
			//for( INT NormalParamIndex = 0; NormalParamIndex < NormalParameterNames.Num(); NormalParamIndex++ )
			//{
			//	UTexture* NormalTexture = NULL;
			//	if( Material->GetTextureParameterValue( NormalParameterNames( NormalParamIndex ), NormalTexture ) && NormalTexture != NULL )
			//	{
			//		NormalTexturesInSelection.AddUniqueItem( NormalTexture );
			//	}
			//}

			for( INT UsedIndex = 0; UsedIndex < UsedTextures.Num(); UsedIndex++ )
			{
				PaintTexture2DData* TextureData = GetPaintTargetData( (UTexture2D*)UsedTextures(UsedIndex) );

				// If this texture is not in the list of Normal map textures, then we'll add it to the list of valid textures in our selection
				//if( !NormalTexturesInSelection.ContainsItem( UsedTextures( UsedIndex ) ) )
				{
					TexturesInSelection.AddUniqueItem( UsedTextures( UsedIndex ) );
				}
			}
		}		

		// Generate the list of target paint textures that will be displaying in the UI
		for( INT TexIndex = 0; TexIndex < TexturesInSelection.Num(); TexIndex++ )
		{
			UTexture2D* Texture2D = Cast<UTexture2D>( TexturesInSelection( TexIndex ) );
			
			// If this is not a UTexture2D we check to see if it is a rendertarget texture
			if( Texture2D == NULL )
			{
				UTextureRenderTarget2D* TextureRenderTarget2D = Cast<UTextureRenderTarget2D>( TexturesInSelection( TexIndex ) );
				if( TextureRenderTarget2D )
				{
					// Since this is a rendertarget, we lookup the original texture that we overrode during the paint operation
					Texture2D = GetOriginalTextureFromRenderTarget( TextureRenderTarget2D );

					// Since we looked up a texture via a rendertarget, it is possible that this texture already exists in our list.  If so 
					//  we will not add it and continue processing other elements.
					if( Texture2D != NULL && TexturesInSelection.ContainsItem( Texture2D ) )
					{
						continue;
					}
				}
			}

			if( Texture2D != NULL )
			{
				// @todo MeshPaint: We rely on filtering out normal maps by name here.  Obviously a user can name a diffuse with _N_ in the name so
				//   this is not a good option.  We attempted to find all the normal maps from the material above with GetAllNormalParameterNames(),
				//   but that always seems to return an empty list.  This needs to be revisited.
	
				// Some normalmaps in the content will fail checks we do in the if statement below.  So we also check to make sure 
				//   the name does not end with "_N", and that the following substrings do not appear in the name "_N_" "_N0".
				FString Texture2DName;
				Texture2D->GetName(Texture2DName);
				Texture2DName = Texture2DName.ToUpper();

				// Make sure the texture is not a normalmap, we don't support painting on those at the moment.
				if( Texture2D->IsNormalMap() == TRUE 
					|| Texture2D->HasAlphaChannel() == TRUE							// @todo Remove when textures with alpha channels are supported
					|| Texture2D->LODGroup == TEXTUREGROUP_WorldNormalMap
					|| Texture2D->LODGroup == TEXTUREGROUP_CharacterNormalMap
					|| Texture2D->LODGroup == TEXTUREGROUP_WeaponNormalMap
					|| Texture2D->LODGroup == TEXTUREGROUP_VehicleNormalMap
					|| Texture2D->LODGroup == TEXTUREGROUP_WorldNormalMap
					|| Texture2DName.InStr( TEXT("_N0" )) != INDEX_NONE
					|| Texture2DName.InStr( TEXT("_N_" )) != INDEX_NONE
					|| Texture2DName.InStr( TEXT("_NORMAL" )) != INDEX_NONE
					|| (Texture2DName.Right(2)).InStr( TEXT("_N" )) != INDEX_NONE )
				{
					continue;
				}
			
				// Add the texture to our list
				new(TexturePaintTargetList) FTextureTargetListInfo(Texture2D);

				// We stored off the user's selection before we began the update.  Since we cleared the list we lost
				//  that selection info. If the same texture appears in our list after update, we will select it again.
				if( PreviouslySelectedTexture != NULL && Texture2D == PreviouslySelectedTexture )
				{
					TexturePaintTargetList( TexturePaintTargetList.Num() - 1 ).bIsSelected = TRUE;
				}
			}
		}

		//We refreshed the list so we default to the first texture if nothing is currently selected.
		if(TexturePaintTargetList.Num() > 0 && GetSelectedTexture() == NULL)
		{
			TexturePaintTargetList(0).bIsSelected = TRUE;
		}

		bShouldUpdateTextureList = FALSE;
	}
}

/** Returns index of the currently selected Texture Target */
INT FEdModeMeshPaint::GetCurrentTextureTargetIndex() const
{
	INT TextureTargetIndex = 0;
	for ( TArray<FTextureTargetListInfo>::TConstIterator It(TexturePaintTargetList); It; ++It )
	{
		if( It->bIsSelected )
		{
			break;
		}
		TextureTargetIndex++;
	}
	return TextureTargetIndex;
}

/** Returns highest number of UV Sets based on current selection */
INT FEdModeMeshPaint::GetMaxNumUVSets() const
{
	INT MaxNumUVSets = 0;

	// Iterate over selected static mesh components
	TArray<UStaticMeshComponent*> SMComponents = GetValidStaticMeshComponents();
	for( INT CurSMIndex = 0; CurSMIndex < SMComponents.Num(); ++CurSMIndex )
	{
		UStaticMeshComponent* StaticMeshComponent = SMComponents( CurSMIndex );

		// Get the number of UV sets for this static mesh
		INT NumUVSets = StaticMeshComponent->StaticMesh->LODModels(PaintingMeshLODIndex).VertexBuffer.GetNumTexCoords();
		MaxNumUVSets = Max(NumUVSets, MaxNumUVSets);
	}

	return MaxNumUVSets;
}


/** Will return the list of available texture paint targets */
TArray<FTextureTargetListInfo>* FEdModeMeshPaint::GetTexturePaintTargetList()
{
	return &TexturePaintTargetList;
}

/** Will return the selected target paint texture if there is one. */
UTexture2D* FEdModeMeshPaint::GetSelectedTexture()
{
	// Loop through our list of textures and see which one the user has selected
	for( INT targetIndex = 0; targetIndex < TexturePaintTargetList.Num(); targetIndex++ )
	{
		if(TexturePaintTargetList(targetIndex).bIsSelected)
		{
			return TexturePaintTargetList(targetIndex).TextureData;
		}
	}
	return NULL;
}

/** will find the currently selected paint target texture in the content browser */
void FEdModeMeshPaint::FindSelectedTextureInContentBrowser()
{
	UTexture2D* SelectedTexture = GetSelectedTexture();
	if( NULL != SelectedTexture )
	{
		TArray<UObject*> Objects;
		Objects.AddItem( SelectedTexture );
		GApp->EditorFrame->SyncBrowserToObjects( Objects );
	}
}

/**
 * Used to change the currently selected paint target texture.
 *
 * @param	bToTheRight 	True if a shift to next texture desired, false if a shift to the previous texture is desired.
 * @param	bCycle		 	If set to False, this function will stop at the first or final element.  It will cycle to the opposite end of the list if set to TRUE.
 */
void FEdModeMeshPaint::ShiftSelectedTexture( UBOOL bToTheRight, UBOOL bCycle )
{
	if( TexturePaintTargetList.Num() <= 1 )
	{
		return;
	}

	FTextureTargetListInfo* Prev = NULL;
	FTextureTargetListInfo* Curr = NULL;
	FTextureTargetListInfo* Next = NULL;
	INT SelectedIndex = -1;

	// Loop through our list of textures and see which one the user has selected, while we are at it we keep track of the prev/next textures
	for( INT TargetIndex = 0; TargetIndex < TexturePaintTargetList.Num(); TargetIndex++ )
	{
		Curr = &TexturePaintTargetList( TargetIndex );
		if( TargetIndex < TexturePaintTargetList.Num() - 1 )
		{
			Next = &TexturePaintTargetList( TargetIndex + 1 );
		}
		else
		{
			Next = &TexturePaintTargetList( 0 );
		}

		if( TargetIndex == 0 )
		{
			Prev = &TexturePaintTargetList( TexturePaintTargetList.Num() - 1 );
		}


		if( Curr->bIsSelected )
		{
			SelectedIndex = TargetIndex;
			
			// Once we find the selected texture we bail.  At this point Next, Prev, and Curr will all be set correctly.
			break;
		}

		Prev = Curr;
	}

	// Nothing is selected so we won't be changing anything.
	if( SelectedIndex == -1 )
	{
		return;
	}

	check( Curr && Next && Prev );

	if( bToTheRight == TRUE )
	{
		// Shift to the right(Next texture)
		if( bCycle == TRUE || SelectedIndex != TexturePaintTargetList.Num() - 1  )
		{
			Curr->bIsSelected = FALSE;
			Next->bIsSelected = TRUE;
		}

	}
	else
	{
		//  Shift to the left(Prev texture)
		if( bCycle == TRUE || SelectedIndex != 0 )
		{
			Curr->bIsSelected = FALSE;
			Prev->bIsSelected = TRUE;
		}
	}

#if WITH_MANAGED_CODE
	if( MeshPaintWindow != NULL && MeshPaintWindow.IsValid() )
	{
		// Once we update the target texture we will tell the UI to refresh the any updated properties
		MeshPaintWindow->RefreshAllProperties();
	}
#endif
}

/**
 * Used to get a reference to data entry associated with the texture.  Will create a new entry if one is not found.
 *
 * @param	inTexture 		The texture we want to retrieve data for.
 * @return					Returns a reference to the paint data associated with the texture.  This reference
 *								is only valid until the next change to any key in the map.  Will return NULL if 
 *								the an entry for this texture is not found or when inTexture is NULL.
 */
FEdModeMeshPaint::PaintTexture2DData* FEdModeMeshPaint::GetPaintTargetData(  UTexture2D* inTexture )
{
	if( inTexture == NULL )
	{
		return NULL;
	}
	
	PaintTexture2DData* TextureData = PaintTargetData.Find( inTexture );
	return TextureData;
}

/**
 * Used to add an entry to to our paint target data.
 *
 * @param	inTexture 		The texture we want to create data for.
 * @return					Returns a reference to the newly created entry.  If an entry for the input texture already exists it will be returned instead.
 *								Will return NULL only when inTexture is NULL.   This reference is only valid until the next change to any key in the map.
 *								 
 */
FEdModeMeshPaint::PaintTexture2DData* FEdModeMeshPaint::AddPaintTargetData(  UTexture2D* inTexture )
{
	if( inTexture == NULL )
	{
		return NULL;
	}

	PaintTexture2DData* TextureData = GetPaintTargetData( inTexture );
	if( TextureData == NULL )
	{
		// If we didn't find data associated with this texture we create a new entry and return a reference to it.
		//   Note: This reference is only valid until the next change to any key in the map.
		TextureData = &PaintTargetData.Set( inTexture,  PaintTexture2DData( inTexture, FALSE ) );
	}
	return TextureData;
}

/**
 * Used to get the original texture that was overridden with a render target texture.
 *
 * @param	inTexture 		The render target that was used to override the original texture.
 * @return					Returns a reference to texture that was overridden with the input render target texture.  Returns NULL if we don't find anything.
 *								 
 */
UTexture2D* FEdModeMeshPaint::GetOriginalTextureFromRenderTarget( UTextureRenderTarget2D* inTexture )
{
	if( inTexture == NULL )
	{
		return NULL;
	}

	UTexture2D* Texture2D = NULL;

	// We loop through our data set and see if we can find this rendertarget.  If we can, then we add the corresponding UTexture2D to the UI list.
	for ( TMap< UTexture2D*, PaintTexture2DData >::TIterator It(PaintTargetData); It; ++It)
	{
		PaintTexture2DData* TextureData = &It.Value();

		if( TextureData->PaintRenderTargetTexture != NULL &&
			TextureData->PaintRenderTargetTexture == inTexture )
		{
			Texture2D = TextureData->PaintingTexture2D;

			// We found the the matching texture so we can stop searching
			break;
		}
	}

	return Texture2D;
}


/** FEdModeMeshPaint: Called once per frame */
void FEdModeMeshPaint::Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
{
	FEdMode::Tick(ViewportClient,DeltaTime);

#if WITH_MANAGED_CODE
	// Inform the window if it should display the warning for overflowing the Undo Buffer.
	MeshPaintWindow->TransactionBufferSizeBreech(GEditor->IsTransactionBufferBreeched()? true : false);
#endif
	//Will set the texture override up for the selected texture, important for the drop down combo-list and selecting between material instances.
	if(FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture)
	{
		TArray<UStaticMeshComponent*> SMComponents = GetValidStaticMeshComponents();

		for( INT CurSMIndex = 0; CurSMIndex < SMComponents.Num(); ++CurSMIndex )
		{
			SetSpecificTextureOverrideForMesh(SMComponents( CurSMIndex ), GetSelectedTexture());
		}
	}

	if( bDoRestoreRenTargets && FMeshPaintSettings::Get().ResourceType == EMeshPaintResource::Texture )
	{
		if( PaintingTexture2D == NULL )
		{
			for ( TMap< UTexture2D*, PaintTexture2DData >::TIterator It(PaintTargetData); It; ++It)
			{
				PaintTexture2DData* TextureData = &It.Value();
				if( TextureData->PaintRenderTargetTexture != NULL )
				{

					UBOOL bIsSourceTextureStreamedIn = TextureData->PaintingTexture2D->IsFullyStreamedIn();

					if( !bIsSourceTextureStreamedIn )
					{
						//   Make sure it is fully streamed in before we try to do anything with it.
						TextureData->PaintingTexture2D->SetForceMipLevelsToBeResident(30.0f);
						TextureData->PaintingTexture2D->WaitForStreaming();
					}

					//Use the duplicate texture here because as we modify the texture and do undo's, it will be different over the original.
					SetupInitialRenderTargetData( TextureData->PaintingTexture2D, TextureData->PaintRenderTargetTexture );

				}
			}
		}
		// We attempted a restore of the rendertargets so go ahead and clear the flag
		bDoRestoreRenTargets = FALSE;
	}
	


#if _WINDOWS
	// Process Deferred Messages
	if (GLastKeyLevelEditingViewportClient)
	{
		for (int i = 0; i < WindowMessages.Num(); ++i)
		{
			FDeferredWindowMessage& Message = WindowMessages(i);

			FWindowsViewport* Viewport = static_cast<FWindowsViewport*>(GLastKeyLevelEditingViewportClient->Viewport);
			if (Viewport)
			{
				Viewport->ViewportWndProc(Message.Message, Message.wParam, Message.lParam);
			}
		}
	}
	WindowMessages.Empty();
#endif
}

void FEdModeMeshPaint::DuplicateTextureMaterialCombo()
{
	UTexture2D* SelectedTexture = GetSelectedTexture();
	if( NULL != SelectedTexture )
	{
		UBOOL bFoundMaterialTextureCombo = FALSE;

		TArray<UStaticMeshComponent*> SMComponents = GetValidStaticMeshComponents();

		//Make sure we have items.
		if(SMComponents.Num() == 0)
		{
			return;
		}

		// Check all the materials on the mesh to see if the user texture is there
		INT MaterialIndex = 0;
		UMaterialInterface* MaterialToCheck = SMComponents(0)->GetMaterial( MaterialIndex, PaintingMeshLODIndex );
		while( MaterialToCheck != NULL )
		{
			UBOOL bIsTextureUsed = MaterialToCheck->UsesTexture( SelectedTexture, FALSE );

			if( bIsTextureUsed == TRUE)// && bStartedPainting == FALSE )
			{
				UBOOL bIsSourceTextureStreamedIn = SelectedTexture->IsFullyStreamedIn();

				if( !bIsSourceTextureStreamedIn )
				{
					// We found that this texture is used in one of the meshes materials but not fully loaded, we will
					//   attempt to fully stream in the texture before we try to do anything with it.
					SelectedTexture->SetForceMipLevelsToBeResident(30.0f);
					SelectedTexture->WaitForStreaming();

					// We do a quick sanity check to make sure it is streamed fully streamed in now.
					bIsSourceTextureStreamedIn = SelectedTexture->IsFullyStreamedIn();

				}

				if( bIsSourceTextureStreamedIn )
				{
					//We found the correct combo, break out and do the duplication.
					bFoundMaterialTextureCombo = true;
					break;
				}
			}

			++MaterialIndex;
			MaterialToCheck = SMComponents(0)->GetMaterial( MaterialIndex, PaintingMeshLODIndex );
		}

		if(bFoundMaterialTextureCombo)
		{
			UMaterial* NewMaterial = NULL;

			FString SelectedPackage;
			FString SelectedGroup;

			//Duplicate the texture.
			UTexture2D* NewTexture;
			{
				TArray< UObject* > SelectedObjects, OutputObjects;
				SelectedObjects.AddItem(SelectedTexture);
				ObjectTools::DuplicateWithRefs(SelectedObjects, NULL, &OutputObjects);

				if(OutputObjects.Num() > 0)
				{
					NewTexture = (UTexture2D*)OutputObjects(0);

					TArray<BYTE> TexturePixels;
					SelectedTexture->GetUncompressedSourceArt(TexturePixels);
					NewTexture->SetUncompressedSourceArt( TexturePixels.GetData(), TexturePixels.Num() * sizeof( BYTE ) );
					NewTexture->SRGB = SelectedTexture->SRGB;
					NewTexture->PostEditChange();
				}
				else
				{
					//The user backed out, end this quietly.
					return;
				}
			}

			// Create the new material instance
			UMaterialInstanceConstant* NewMaterialInstance = NULL;
			{
				NewMaterialInstance = CastChecked<UMaterialInstanceConstant>( UObject::StaticConstructObject(
					UMaterialInstanceConstant::StaticClass(), 
					MaterialToCheck->GetOuter(), 
					NAME_None,
					RF_Public
					));

				if(!NewMaterialInstance)
				{
					appErrorf(TEXT("Could not duplicate %s"), *MaterialToCheck->GetName());
					return;
				}

				//We want all uses of this texture to be replaced so go through the entire list.
				for(INT IndexMP(0); IndexMP < MP_MAX; ++IndexMP)
				{
					TArray<UTexture*> OutTextures;
					TArray<FName> OutTextureParamNames;
					MaterialToCheck->GetTexturesInPropertyChain((EMaterialProperty)IndexMP, OutTextures, &OutTextureParamNames, NULL);
					for(INT ValueIndex(0); ValueIndex < OutTextureParamNames.Num(); ++ValueIndex)
					{
						UTexture* OutTexture;
						if(MaterialToCheck->GetTextureParameterValue(OutTextureParamNames(ValueIndex), OutTexture) == TRUE && OutTexture == SelectedTexture)
						{
							// Bind texture to the material instance
							NewMaterialInstance->SetTextureParameterValue(OutTextureParamNames(ValueIndex), NewTexture);
						}
					}
				}
				NewMaterialInstance->SetParent( MaterialToCheck );
				NewMaterialInstance->MarkPackageDirty();

				//Make sure the static parameter values are updated, otherwise there will be some confusion with texture overrides.
				FStaticParameterSet StaticParameters;
				NewMaterialInstance->GetStaticParameterValues(&StaticParameters);
				NewMaterialInstance->SetStaticParameterValues(&StaticParameters);
				NewMaterialInstance->UpdateStaticPermutation();
			}

			// Iterate over selected static mesh components
			for( INT CurSMIndex = 0; CurSMIndex < SMComponents.Num(); ++CurSMIndex )
			{
				TArray<UMaterialInterface*> UsedMaterials;

				// Currently we only support static mesh actors or dynamic static mesh actors
				UStaticMeshComponent* StaticMesh = SMComponents(CurSMIndex);

				//Release the old overrides.
				ClearStaticMeshTextureOverrides(StaticMesh);

				// Get all the used materials for this StaticMeshComponent
				StaticMesh->GetUsedMaterials( UsedMaterials );

				// Look in the mesh's materials to see which of the selected meshes use this material. 
				// If it does, change it to the newly created instance.
				for( INT UsedIndex = 0; UsedIndex < UsedMaterials.Num(); UsedIndex++)
				{
					if( UsedMaterials( UsedIndex ) ==  MaterialToCheck)
					{
						StaticMesh->SetMaterial(UsedIndex, NewMaterialInstance);
						UpdateSettingsForStaticMeshComponent(StaticMesh, SelectedTexture, NewTexture);
					}
				}
			}
			ActorSelectionChangeNotify();
		}
	}
}

void FEdModeMeshPaint::CreateNewTexture()
{
	FString SelectedPackage = TEXT("MyPackage"), SelectedGroup;
	UClass* FactoryClass = UTexture2DFactoryNew::StaticClass();
		
	WxDlgNewGeneric Dialog;
	if( Dialog.ShowModal( SelectedPackage, SelectedGroup, FactoryClass, NULL ) == wxID_OK )
	{
		// The user hit OK, now we should try to open the corresponding object editor if possible

		if( FactoryClass == NULL )
		{
			FactoryClass = Dialog.GetFactoryClass();
		}
		check(FactoryClass);

		UFactory* FactoryCDO = FactoryClass->GetDefaultObject<UFactory>();

		// Does the created object have an editor to open after being created?
		if( FactoryCDO->bEditAfterNew )
		{

			UObject* CreatedObject = Dialog.GetCreatedObject();

			if( CreatedObject )
			{
				// An object was successfully created and the object created has an editor to open.
				// So, inform the content browser to open the editor associated to this object. 
				GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_ActivateObject, CreatedObject ) );
			}
		}
	}
}
