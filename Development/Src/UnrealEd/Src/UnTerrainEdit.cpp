/*=============================================================================
	UnTerrainEdit.cpp: Terrain editing code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "Factories.h"
#include "UnTerrain.h"
#include "MouseDeltaTracker.h"

/*-----------------------------------------------------------------------------
	FModeTool_Terrain.
-----------------------------------------------------------------------------*/
TMap<ATerrain*, FModeTool_Terrain::FPartialData*> FModeTool_Terrain::PartialValueData;

FModeTool_Terrain::FModeTool_Terrain()
{
	ID = MT_None;
	bUseWidget = 0;
	bIsTransacting = FALSE;
	Settings = new FTerrainToolSettings;

	PaintingViewportClient = NULL;
}

UBOOL FModeTool_Terrain::MouseMove(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,INT x, INT y)
{
	ViewportClient->Invalidate( FALSE, FALSE );
	return TRUE;
}

/**
 * @return		TRUE if the delta was handled by this editor mode tool.
 */
UBOOL FModeTool_Terrain::InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	if (PaintingViewportClient == InViewportClient)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/**
 * @return		TRUE if the key was handled by this editor mode tool.
 */
UBOOL FModeTool_Terrain::InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event)
{
	if ((PaintingViewportClient == NULL || PaintingViewportClient == ViewportClient) && (Key == KEY_LeftMouseButton || Key == KEY_RightMouseButton))
	{
		if (IsCtrlDown(Viewport) && Event == IE_Pressed)
		{
			FSceneViewFamilyContext ViewFamily(ViewportClient->Viewport,ViewportClient->GetScene(),ViewportClient->ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds());
			FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);

			FTerrainToolSettings*	Settings = (FTerrainToolSettings*)GEditorModeTools().GetActiveMode(EM_TerrainEdit)->GetSettings();
			FViewport*				Viewport = ViewportClient->Viewport;
			FVector					HitLocation, HitNormal;;
			ATerrain*				Terrain = TerrainTrace(Viewport,View,HitLocation, HitNormal, Settings);

			if (Terrain)
			{
				GEditor->BeginTransaction(TEXT("TerrainEdit"));
				Terrain->Modify();
				bIsTransacting = TRUE;

				UBOOL bMirrorX = FALSE;
				UBOOL bMirrorY = FALSE;
				if (SupportsMirroring() == TRUE)
				{
					bMirrorX = (((INT)(Settings->MirrorSetting) & FTerrainToolSettings::TTMirror_X) != 0) ? TRUE : FALSE;
					bMirrorY = (((INT)(Settings->MirrorSetting) & FTerrainToolSettings::TTMirror_Y) != 0) ? TRUE : FALSE;
				}

				if (BeginApplyTool(Terrain,Settings->DecoLayer,
					FindMatchingTerrainLayer(Terrain, Settings),
					Settings->MaterialIndex,Terrain->WorldToLocal().TransformFVector(HitLocation), 
					HitLocation, HitNormal, bMirrorX, bMirrorY
					)
					)
				{
					PaintingViewportClient = ViewportClient;
					Terrain->MarkPackageDirty();
				}
				else
				{
					GEditor->EndTransaction();
					bIsTransacting = FALSE;
				}
			}

			return 1;
		}
		else if (PaintingViewportClient)
		{
			EndApplyTool();

			if (bIsTransacting)
			{
				GEditor->EndTransaction();
				bIsTransacting = FALSE;
			}

			INT	X	= Viewport->GetMouseX();
			INT	Y	= Viewport->GetMouseY();
			PaintingViewportClient = NULL;
			Viewport->SetMouse(X, Y);
			UBOOL bShowCursor = ViewportClient->UpdateCursorVisibility ();
			Viewport->LockMouseToWindow( !bShowCursor );
			return 1;
		}
		else
		{
			// Catch missed button releases...
			if (bIsTransacting)
			{
				GEditor->EndTransaction();
				bIsTransacting = FALSE;
			}

			return 0;
		}
	}

	return 0;

}

void FModeTool_Terrain::Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
{
	FEdModeTerrainEditing*	EdMode = (FEdModeTerrainEditing*)(GEditorModeTools().GetActiveMode(EM_TerrainEdit));
	check(EdMode);
	FTerrainToolSettings*	Settings = (FTerrainToolSettings*)(EdMode->GetSettings());
	FViewport*				Viewport = ViewportClient->Viewport;
	FVector					HitLocation;

	FSceneViewFamilyContext ViewFamily(ViewportClient->Viewport,ViewportClient->GetScene(),ViewportClient->ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds());
	FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
	if (PaintingViewportClient == ViewportClient)
	{
		for (INT i = 0; i < EdMode->ToolTerrains.Num(); i++)
		{
			ATerrain* Terrain = EdMode->ToolTerrains(i);
			if (Terrain)
			{
				FLOAT	Direction = Viewport->KeyState(KEY_LeftMouseButton) ? 1.f : -1.f;
				FVector	LocalPosition = Terrain->WorldToLocal().TransformFVector(EdMode->ToolHitLocation); // In the terrain actor's local space.
				FLOAT	LocalStrength = Settings->Strength * DeltaTime / Terrain->DrawScale3D.Z / TERRAIN_ZSCALE,
						LocalMinRadius = (FLOAT)Settings->RadiusMin / Abs<FLOAT>(Terrain->DrawScale3D.X) / Terrain->DrawScale,
						LocalMaxRadius = (FLOAT)Settings->RadiusMax / Abs<FLOAT>(Terrain->DrawScale3D.X) / Terrain->DrawScale;

				INT	MinX = Clamp(appFloor(LocalPosition.X - LocalMaxRadius),0,Terrain->NumVerticesX - 1),
					MinY = Clamp(appFloor(LocalPosition.Y - LocalMaxRadius),0,Terrain->NumVerticesY - 1),
					MaxX = Clamp(appCeil(LocalPosition.X + LocalMaxRadius),0,Terrain->NumVerticesX - 1),
					MaxY = Clamp(appCeil(LocalPosition.Y + LocalMaxRadius),0,Terrain->NumVerticesY - 1);

				// Handle 'mirrored' terrain (Transform->Mirror * Axis)
				if (MinX > MaxX)
				{
					Swap<INT>(MinX, MaxX);
				}

				if (MinY > MaxY)
				{
					Swap<INT>(MinY, MaxY);
				}

				// Do the constraining here...
				if ((EdMode->bConstrained == TRUE) && (Terrain->EditorTessellationLevel > 0))
				{
					//
					INT ConstrainedTess = Terrain->MaxTesselationLevel / Terrain->EditorTessellationLevel;
					if ((MinX % ConstrainedTess) > 0)
					{
						MinX += (Terrain->MaxTesselationLevel - (MinX % ConstrainedTess));
					}
					if ((MaxX % ConstrainedTess) > 0)
					{
						MaxX -= (MinX % ConstrainedTess);
					}
					if ((MinY % ConstrainedTess) > 0)
					{
						MinY += (Terrain->MaxTesselationLevel - (MinY % ConstrainedTess));
					}
					if ((MaxY % ConstrainedTess) > 0)
					{
						MaxY -= (MinX % ConstrainedTess);
					}
				}

				UBOOL bMirrorX = FALSE;
				UBOOL bMirrorY = FALSE;
				if (SupportsMirroring() == TRUE)
				{
					bMirrorX = (((INT)(Settings->MirrorSetting) & FTerrainToolSettings::TTMirror_X) != 0) ? TRUE : FALSE;
					bMirrorY = (((INT)(Settings->MirrorSetting) & FTerrainToolSettings::TTMirror_Y) != 0) ? TRUE : FALSE;
				}


				FIntRect MirrorValues;
				MirrorValues.Min.X = MinX;
				MirrorValues.Min.Y = MinY;
				MirrorValues.Max.X = MaxX;
				MirrorValues.Max.Y = MaxY;
				INT Temp1, Temp2;
				if (bMirrorY)
				{
					Temp1 = EdMode->GetMirroredValue_X(Terrain, MinX);
					Temp2 = EdMode->GetMirroredValue_X(Terrain, MaxX);
					MirrorValues.Min.X = Min<INT>(Temp1, Temp2);
					MirrorValues.Max.X = Max<INT>(Temp1, Temp2);
				}
				if (bMirrorX)
				{
					Temp1 = EdMode->GetMirroredValue_Y(Terrain, MinY);
					Temp2 = EdMode->GetMirroredValue_Y(Terrain, MaxY);
					MirrorValues.Min.Y = Min<INT>(Temp1, Temp2);
					MirrorValues.Max.Y = Max<INT>(Temp1, Temp2);
				}

				MirrorValues.Min.X = Clamp(MirrorValues.Min.X,0,Terrain->NumVerticesX - 1),
				MirrorValues.Min.Y = Clamp(MirrorValues.Min.Y,0,Terrain->NumVerticesY - 1),
				MirrorValues.Max.X = Clamp(MirrorValues.Max.X,0,Terrain->NumVerticesX - 1),
				MirrorValues.Max.Y = Clamp(MirrorValues.Max.Y,0,Terrain->NumVerticesY - 1);

				INT LayerIndex = FindMatchingTerrainLayer(Terrain, Settings);
				if (ApplyTool(Terrain, Settings->DecoLayer, LayerIndex, Settings->MaterialIndex, LocalPosition, 
						LocalMinRadius, LocalMaxRadius, Direction, LocalStrength, MinX, MinY, MaxX, MaxY,
						bMirrorX, bMirrorY) == TRUE)
				{
					// Only update the patch bounds when editing the heightmap
					if (LayerIndex == INDEX_NONE)
					{
						Terrain->UpdatePatchBounds(MinX, MinY, MaxX, MaxY);
						if (bMirrorY || bMirrorX)
						{
							Terrain->UpdatePatchBounds(
								MirrorValues.Min.X,MirrorValues.Min.Y,
								MirrorValues.Max.X,MirrorValues.Max.Y);
						}
					}

					// Always update the render data
					Terrain->UpdateRenderData(MinX,MinY,MaxX,MaxY);
					if (bMirrorY || bMirrorX)
					{
						Terrain->UpdateRenderData(
							MirrorValues.Min.X,MirrorValues.Min.Y,
							MirrorValues.Max.X,MirrorValues.Max.Y);
					}
				}

				// remember that we modified this terrain
				ModifiedTerrains.AddUniqueItem(Terrain);

				// Force viewport update
				Viewport->Invalidate();
			}
		}
	}
}

ATerrain* FModeTool_Terrain::TerrainTrace(FViewport* Viewport,
	const FSceneView* View,FVector& Location, FVector& Normal, 
	FTerrainToolSettings* Settings, UBOOL bGetFirstHit)
{
	if (Viewport)
	{
        // Fix from BioWare for the cursor offset problem - Nov-12-2005 - START
        // Broke the transform by the inverse viewprojection matrix into two parts: transform by inverse
        // projection followed by transform by inverse view.  This solves some numerical stability problems
        // when painting on terrain with large drawscale.

        // Calculate World-space position of the raycast towards the mouse cursor.
		FIntPoint	MousePosition;
		Viewport->GetMousePos(MousePosition);
		INT		X = MousePosition.X,
				Y = MousePosition.Y;

        // Get the eye position and direction of the mouse cursor in two stages (inverse transform projection, then inverse transform view).
        // This avoids the numerical instability that occurs when a view matrix with large translation is composed with a projection matrix
        FMatrix InverseProjection = View->ProjectionMatrix.Inverse();
        FMatrix InverseView = View->ViewMatrix.Inverse();

        // The start of the raytrace is defined to be at mousex,mousey,0 in projection space
        // The end of the raytrace is at mousex, mousey, 0.5 in projection space
        FLOAT ScreenSpaceX = (X-Viewport->GetSizeX()/2.0f)/(Viewport->GetSizeX()/2.0f);
        FLOAT ScreenSpaceY = (Y-Viewport->GetSizeY()/2.0f)/-(Viewport->GetSizeY()/2.0f);
        FVector4 RayStartProjectionSpace = FVector4(ScreenSpaceX, ScreenSpaceY,    0, 1.0f);
        FVector4 RayEndProjectionSpace   = FVector4(ScreenSpaceX, ScreenSpaceY, 0.5f, 1.0f);

        // Projection (changing the W coordinate) is not handled by the FMatrix transforms that work with vectors, so multiplications
        // by the projection matrix should use homogenous coordinates (i.e. FPlane).
        FVector4 HGRayStartViewSpace = InverseProjection.TransformFVector4(RayStartProjectionSpace);
        FVector4 HGRayEndViewSpace   = InverseProjection.TransformFVector4(RayEndProjectionSpace);
        FVector RayStartViewSpace(HGRayStartViewSpace.X, HGRayStartViewSpace.Y, HGRayStartViewSpace.Z);
        FVector   RayEndViewSpace(HGRayEndViewSpace.X,   HGRayEndViewSpace.Y,   HGRayEndViewSpace.Z);
        // divide vectors by W to undo any projection and get the 3-space coordinate 
        if (HGRayStartViewSpace.W != 0.0f)
        {
            RayStartViewSpace /= HGRayStartViewSpace.W;
        }
        if (HGRayEndViewSpace.W != 0.0f)
        {
            RayEndViewSpace /= HGRayEndViewSpace.W;
        }
        FVector RayDirViewSpace = RayEndViewSpace - RayStartViewSpace;
        RayDirViewSpace = RayDirViewSpace.SafeNormal();

        // The view transform does not have projection, so we can use the standard functions that deal with vectors and normals (normals
        // are vectors that do not use the translational part of a rotation/translation)
        FVector RayStartWorldSpace = InverseView.TransformFVector(RayStartViewSpace);
        FVector RayDirWorldSpace   = InverseView.TransformNormal(RayDirViewSpace);

        // Finally, store the results in the hitcheck inputs.  The start position is the eye, and the end position
        // is the eye plus a long distance in the direction the mouse is pointing.
        FVector Start = RayStartWorldSpace;
        FVector Dir = RayDirWorldSpace.SafeNormal();
        FVector End = RayStartWorldSpace + Dir*WORLD_MAX;
        // Fix from BioWare for the cursor offset problem - Nov-12-2005 - END

		// Do the trace.
//		if (bGetFirstHit)
//@todo.SAS. This is a temp work-around for the editor not handling hidden terrain when editing
		if (0)
		{
			FCheckResult	Hit(1);
			GWorld->SingleLineCheck(Hit,NULL,End,Start,TRACE_Terrain|TRACE_TerrainIgnoreHoles);
			ATerrain* HitTerrain = Cast<ATerrain>(Hit.Actor);
			if (HitTerrain)
			{
				if (TerrainIsValid(HitTerrain, Settings))
				{
					Normal = Hit.Normal;
					Location = Hit.Location;
					return HitTerrain;
				}
			}
		}
		else
		{
			FMemMark		Mark(GMainThreadMemStack);
			FCheckResult*	FirstHit	= NULL;
			DWORD			TraceFlags	= TRACE_Terrain|TRACE_TerrainIgnoreHoles;

			FirstHit	= GWorld->MultiLineCheck(GMainThreadMemStack, End, Start, FVector(0.f,0.f,0.f), TraceFlags, NULL);
			for (FCheckResult* Test = FirstHit; Test; Test = Test->GetNext())
			{
				if (Test->Component && Test->Component->IsA(UTerrainComponent::StaticClass()))
				{
					ATerrain* HitTerrain = Cast<ATerrain>(Test->Actor);
					if (HitTerrain)
					{
						if (TerrainIsValid(HitTerrain, Settings))
						{
							Normal		= Test->Normal;
							Location	= Test->Location;
							Mark.Pop();
							return HitTerrain;
						}
					}
				}
			}

			Mark.Pop();
		}
	}

	return NULL;

}

/**
 *	Retrieve the mirrored versions of the Min/Max<X/Y> values
 *
 *	@param	Terrain		Pointer to the terrain of interest
 *	@param	InMinX		The 'source' MinX value
 *	@param	InMinY		The 'source' MinY value
 *	@param	InMaxX		The 'source' MaxX value
 *	@param	InMaxY		The 'source' MaxY value
 *	@param	bMirrorX	Whether to mirror about the X axis
 *	@param	bMirrorY	Whether to mirror about the Y axis
 *	@param	OutMinX		The output of the mirrored MinX value
 *	@param	OutMinY		The output of the mirrored MinY value
 *	@param	OutMaxX		The output of the mirrored MaxX value
 *	@param	OutMaxY		The output of the mirrored MaxY value
 */
void FModeTool_Terrain::GetMirroredExtents(ATerrain* Terrain, INT InMinX, INT InMinY, INT InMaxX, INT InMaxY, 
	UBOOL bMirrorX, UBOOL bMirrorY, INT& OutMinX, INT& OutMinY, INT& OutMaxX, INT& OutMaxY)
{
	check(Terrain);

	FLOAT HalfX = (FLOAT)(Terrain->NumPatchesX) / 2.0f;
	FLOAT HalfY = (FLOAT)(Terrain->NumPatchesY) / 2.0f;

	FLOAT Diff;
	FLOAT Temp;

	if (bMirrorX == TRUE)
	{
		// Mirror about the X-axis
		Diff = HalfY - InMinY;
		Temp = HalfY + Diff;
		Diff = HalfY - InMaxY;
		OutMinY = HalfY + Diff;
		OutMaxY = Temp;
	}

	if (bMirrorY == TRUE)
	{
		// Mirror about the Y-axis
		Diff = HalfX - InMinX;
		Temp = HalfX + Diff;
		Diff = HalfX - InMaxX;
		OutMinX = HalfX + Diff;
		OutMaxX = Temp;
	}

	//@todo. Do we want to clamp? What about uneven terrains?
	OutMinX = Clamp(OutMinX,0,Terrain->NumVerticesX - 1);
	OutMinY = Clamp(OutMinY,0,Terrain->NumVerticesY - 1);
	OutMaxX = Clamp(OutMaxX,0,Terrain->NumVerticesX - 1);
	OutMaxY = Clamp(OutMaxY,0,Terrain->NumVerticesY - 1);
}

/**
 *	UpdateIntermediateValues
 *	Update the intermediate values of the edited quads.
 *
 *	@param	Terrain			Pointer to the terrain of interest
 *	@param	DecoLayer		Boolean indicating the layer is a deco layer
 *	@param	LayerIndex		The index of the layer being edited
 *	@param	MaterialIndex	The index of the material being edited
 *	@param	InMinX			The 'source' MinX value
 *	@param	InMinY			The 'source' MinY value
 *	@param	InMaxX			The 'source' MaxX value
 *	@param	InMaxY			The 'source' MaxY value
 *	@param	bMirrorX		Whether to mirror about the X axis
 *	@param	bMirrorY		Whether to mirror about the Y axis
 */
void FModeTool_Terrain::UpdateIntermediateValues(ATerrain* Terrain, 
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	INT InMinX, INT InMinY, INT InMaxX, INT InMaxY, UBOOL bMirrorX, UBOOL bMirrorY)
{
	FEdModeTerrainEditing* EdMode = (FEdModeTerrainEditing*)(GEditorModeTools().GetActiveMode(EM_TerrainEdit));
	check(EdMode);

	INT StepSize = 1;
	if ((EdMode->bConstrained == TRUE) && (Terrain->EditorTessellationLevel > 0))
	{
		StepSize = Terrain->MaxTesselationLevel / Terrain->EditorTessellationLevel;
	}

	INT	AlphaMapIndex = -1;
	if (LayerIndex != INDEX_NONE)
	{
		AlphaMapIndex = DecoLayer ? Terrain->DecoLayers(LayerIndex).AlphaMapIndex : Terrain->Layers(LayerIndex).AlphaMapIndex;
	}

	if (StepSize != 1)
	{
		INT MirrorX;
		INT MirrorY;
		// Walk through the edited vertices and set the values in-between
		for (INT Y = InMinY - StepSize; Y <= InMaxY + StepSize; Y += StepSize)
		{
			MirrorY = Y;
			for (INT X = InMinX - StepSize; X <= InMaxX + StepSize; X += StepSize)
			{
				MirrorX = X;
				if (bMirrorY)
				{
					MirrorX = EdMode->GetMirroredValue_X(Terrain, X);
				}
				if (bMirrorX)
				{
					MirrorY = EdMode->GetMirroredValue_Y(Terrain, Y);
				}

				if (LayerIndex == INDEX_NONE)
				{
					// Get the 4-corner heights of the 'quad'...
					INT HeightValue[4];

					HeightValue[0] = Terrain->Height(X,Y);
					HeightValue[1] = Terrain->Height(X + StepSize,Y);
					HeightValue[2] = Terrain->Height(X,Y + StepSize);
					HeightValue[3] = Terrain->Height(X + StepSize,Y + StepSize);


					FLOAT Interp;
					INT TempHeight;
					WORD NewHeight;

					// Horizontal exterior
					for (INT Sub = 1; Sub < StepSize; Sub++)
					{
						Interp = (FLOAT)(Sub % StepSize) / (FLOAT)StepSize;
						// Bottom
						TempHeight = (INT)(((FLOAT)HeightValue[0] * (1.0f - Interp)) + ((FLOAT)HeightValue[1] * Interp));
						NewHeight = (WORD)Clamp<INT>(TempHeight, 0, MAXWORD);
						Terrain->Height(X + Sub,Y) = NewHeight;
						MirrorX = X + Sub;
						if (bMirrorY)
						{
							MirrorX = EdMode->GetMirroredValue_X(Terrain, MirrorX);
						}
						MirrorY = Y;
						if (bMirrorX)
						{
							MirrorY = EdMode->GetMirroredValue_Y(Terrain, MirrorY);
						}
						if (bMirrorX || bMirrorY)
						{
							Terrain->Height(MirrorX, MirrorY) = NewHeight;
						}
						// Top
						TempHeight = (INT)(((FLOAT)HeightValue[2] * (1.0f - Interp)) + ((FLOAT)HeightValue[3] * Interp));
						NewHeight = (WORD)Clamp<INT>(TempHeight, 0, MAXWORD);
						Terrain->Height(X + Sub,Y + StepSize) = NewHeight;
						MirrorX = X + Sub;
						if (bMirrorY)
						{
							MirrorX = EdMode->GetMirroredValue_X(Terrain, MirrorX);
						}
						MirrorY = Y + StepSize;
						if (bMirrorX)
						{
							MirrorY = EdMode->GetMirroredValue_Y(Terrain, MirrorY);
						}
						if (bMirrorX || bMirrorY)
						{
							Terrain->Height(MirrorX, MirrorY) = NewHeight;
						}
					}

					// Vertical exterior
					for (INT Sub = 1; Sub < StepSize; Sub++)
					{
						Interp = (FLOAT)(Sub % StepSize) / (FLOAT)StepSize;

						// Left
						TempHeight = (INT)(((FLOAT)HeightValue[0] * (1.0f - Interp)) + ((FLOAT)HeightValue[2] * Interp));
						NewHeight = (WORD)Clamp<INT>(TempHeight, 0, MAXWORD);
						Terrain->Height(X,Y + Sub) = NewHeight;
						MirrorX = X;
						if (bMirrorY)
						{
							MirrorX = EdMode->GetMirroredValue_X(Terrain, MirrorX);
						}
						MirrorY = Y + Sub;
						if (bMirrorX)
						{
							MirrorY = EdMode->GetMirroredValue_Y(Terrain, MirrorY);
						}
						if (bMirrorX || bMirrorY)
						{
							Terrain->Height(MirrorX, MirrorY) = NewHeight;
						}
						// Right
						TempHeight = (INT)(((FLOAT)HeightValue[1] * (1.0f - Interp)) + ((FLOAT)HeightValue[3] * Interp));
						NewHeight = (WORD)Clamp<INT>(TempHeight, 0, MAXWORD);
						Terrain->Height(X + StepSize,Y + Sub) = NewHeight;
						MirrorX = X + StepSize;
						if (bMirrorY)
						{
							MirrorX = EdMode->GetMirroredValue_X(Terrain, MirrorX);
						}
						MirrorY = Y + Sub;
						if (bMirrorX)
						{
							MirrorY = EdMode->GetMirroredValue_Y(Terrain, MirrorY);
						}
						if (bMirrorX || bMirrorY)
						{
							Terrain->Height(MirrorX, MirrorY) = NewHeight;
						}
					}

					// Next, do the horizontals
					for (INT Sub2 = 1; Sub2 < StepSize; Sub2++)
					{
						HeightValue[0] = Terrain->Height(X + Sub2, Y);
						HeightValue[1] = Terrain->Height(X + Sub2, Y + StepSize);
						for (INT Sub = 1; Sub < StepSize; Sub++)
						{
							Interp = (FLOAT)(Sub % StepSize) / (FLOAT)StepSize;

							TempHeight = (INT)(((FLOAT)HeightValue[0] * (1.0f - Interp)) + ((FLOAT)HeightValue[1] * Interp));
							NewHeight = (WORD)Clamp<INT>(TempHeight, 0, MAXWORD);
							Terrain->Height(X + Sub2,Y + Sub) = NewHeight;
							MirrorX = X + Sub2;
							if (bMirrorY)
							{
								MirrorX = EdMode->GetMirroredValue_X(Terrain, MirrorX);
							}
							MirrorY = Y + Sub;
							if (bMirrorX)
							{
								MirrorY = EdMode->GetMirroredValue_Y(Terrain, MirrorY);
							}
							if (bMirrorX || bMirrorY)
							{
								Terrain->Height(MirrorX, MirrorY) = NewHeight;
							}
						}
					}
				}
				else
				{
					// Get the 4-corner alphas of the 'quad'...
					BYTE AlphaValue[4];

					AlphaValue[0] = Terrain->Alpha(AlphaMapIndex,X,Y);
					AlphaValue[1] = Terrain->Alpha(AlphaMapIndex,X + StepSize,Y);
					AlphaValue[2] = Terrain->Alpha(AlphaMapIndex,X,Y + StepSize);
					AlphaValue[3] = Terrain->Alpha(AlphaMapIndex,X + StepSize,Y + StepSize);

					FLOAT Interp;
					WORD TempAlpha;
					BYTE NewAlpha;

					// Horizontal exterior
					for (INT Sub = 1; Sub < StepSize; Sub++)
					{
						Interp = (FLOAT)(Sub % StepSize) / (FLOAT)StepSize;
						// Bottom
						TempAlpha = (WORD)(((FLOAT)AlphaValue[0] * (1.0f - Interp)) + ((FLOAT)AlphaValue[1] * Interp));
						NewAlpha = (BYTE)Clamp<WORD>(TempAlpha, 0, MAXBYTE);
						BYTE& Alpha = Terrain->Alpha(AlphaMapIndex,X+Sub,Y);
						Alpha = NewAlpha;
						MirrorX = X + Sub;
						if (bMirrorY)
						{
							MirrorX = EdMode->GetMirroredValue_X(Terrain, MirrorX);
						}
						MirrorY = Y;
						if (bMirrorX)
						{
							MirrorY = EdMode->GetMirroredValue_Y(Terrain, MirrorY);
						}
						if (bMirrorX || bMirrorY)
						{
							Alpha = Terrain->Alpha(AlphaMapIndex,MirrorX,MirrorY);
							Alpha = NewAlpha;
						}
						// Top
						TempAlpha = (WORD)(((FLOAT)AlphaValue[2] * (1.0f - Interp)) + ((FLOAT)AlphaValue[3] * Interp));
						NewAlpha = (BYTE)Clamp<WORD>(TempAlpha, 0, MAXBYTE);
						Alpha = Terrain->Alpha(AlphaMapIndex,X+Sub,Y+StepSize);
						Alpha = NewAlpha;
						MirrorX = X + Sub;
						if (bMirrorY)
						{
							MirrorX = EdMode->GetMirroredValue_X(Terrain, MirrorX);
						}
						MirrorY = Y + StepSize;
						if (bMirrorX)
						{
							MirrorY = EdMode->GetMirroredValue_Y(Terrain, MirrorY);
						}
						if (bMirrorX || bMirrorY)
						{
							Alpha = Terrain->Alpha(AlphaMapIndex,MirrorX,MirrorY);
							Alpha = NewAlpha;
						}
					}

					// Vertical exterior
					for (INT Sub = 1; Sub < StepSize; Sub++)
					{
						Interp = (FLOAT)(Sub % StepSize) / (FLOAT)StepSize;

						// Left
						TempAlpha = (WORD)(((FLOAT)AlphaValue[0] * (1.0f - Interp)) + ((FLOAT)AlphaValue[2] * Interp));
						NewAlpha = (BYTE)Clamp<WORD>(TempAlpha, 0, MAXBYTE);
						BYTE& Alpha = Terrain->Alpha(AlphaMapIndex,X,Y+Sub);
						Alpha = NewAlpha;
						MirrorX = X;
						if (bMirrorY)
						{
							MirrorX = EdMode->GetMirroredValue_X(Terrain, MirrorX);
						}
						MirrorY = Y + Sub;
						if (bMirrorX)
						{
							MirrorY = EdMode->GetMirroredValue_Y(Terrain, MirrorY);
						}
						if (bMirrorX || bMirrorY)
						{
							Alpha = Terrain->Alpha(AlphaMapIndex,MirrorX,MirrorY);
							Alpha = NewAlpha;
						}
						// Right
						TempAlpha = (WORD)(((FLOAT)AlphaValue[1] * (1.0f - Interp)) + ((FLOAT)AlphaValue[3] * Interp));
						NewAlpha = (BYTE)Clamp<WORD>(TempAlpha, 0, MAXBYTE);
						Alpha = Terrain->Alpha(AlphaMapIndex,X+StepSize,Y+Sub);
						Alpha = NewAlpha;
						MirrorX = X + StepSize;
						if (bMirrorY)
						{
							MirrorX = EdMode->GetMirroredValue_X(Terrain, MirrorX);
						}
						MirrorY = Y + Sub;
						if (bMirrorX)
						{
							MirrorY = EdMode->GetMirroredValue_Y(Terrain, MirrorY);
						}
						if (bMirrorX || bMirrorY)
						{
							Alpha = Terrain->Alpha(AlphaMapIndex,MirrorX,MirrorY);
							Alpha = NewAlpha;
						}
					}

					// Next, do the horizontals
					for (INT Sub2 = 1; Sub2 < StepSize; Sub2++)
					{
						AlphaValue[0] = Terrain->Alpha(AlphaMapIndex,X+Sub2,Y);
						AlphaValue[1] = Terrain->Alpha(AlphaMapIndex,X+Sub2,Y+StepSize);
						for (INT Sub = 1; Sub < StepSize; Sub++)
						{
							Interp = (FLOAT)(Sub % StepSize) / (FLOAT)StepSize;

							TempAlpha = (WORD)(((FLOAT)AlphaValue[0] * (1.0f - Interp)) + ((FLOAT)AlphaValue[1] * Interp));
							NewAlpha = (BYTE)Clamp<WORD>(TempAlpha, 0, MAXBYTE);
							BYTE& Alpha = Terrain->Alpha(AlphaMapIndex,X+Sub2,Y+Sub);
							Alpha = NewAlpha;
							MirrorX = X + Sub2;
							if (bMirrorY)
							{
								MirrorX = EdMode->GetMirroredValue_X(Terrain, MirrorX);
							}
							MirrorY = Y + Sub;
							if (bMirrorX)
							{
								MirrorY = EdMode->GetMirroredValue_Y(Terrain, MirrorY);
							}
							if (bMirrorX || bMirrorY)
							{
								Alpha = Terrain->Alpha(AlphaMapIndex,MirrorX,MirrorY);
								Alpha = NewAlpha;
							}
						}
					}
				}
			}
		}
	}
}

INT FModeTool_Terrain::FindMatchingTerrainLayer(ATerrain* TestTerrain, FTerrainToolSettings* Settings)
{
	if (Settings->CurrentTerrain == TestTerrain)
	{
		return Settings->LayerIndex;
	}

	if ((Settings->DecoLayer == TRUE) || (Settings->LayerIndex == INDEX_NONE))
	{
		return INDEX_NONE;
	}

	if (Settings->CurrentTerrain->Layers.Num() <= Settings->LayerIndex)
	{
		return INDEX_NONE;
	}

	UTerrainLayerSetup* Setup = Settings->CurrentTerrain->Layers(Settings->LayerIndex).Setup;
	for (INT i = 0; i < TestTerrain->Layers.Num(); i++)
	{
		if (TestTerrain->Layers(i).Setup == Setup)
		{
			return i;
		}
	}

	return INDEX_NONE;        
}

UBOOL FModeTool_Terrain::TerrainIsValid(ATerrain* TestTerrain, FTerrainToolSettings* Settings)
{
	if (TestTerrain->IsHiddenEd())
	{
		return FALSE;
	}

	// always allow matching terrain or heightmap painting
	if ((Settings->CurrentTerrain == TestTerrain) || (Settings->LayerIndex == INDEX_NONE))
	{
		return TRUE;
	}

	// allow painting layers if there is a matching LayerSetup in the other terrain
	if (FindMatchingTerrainLayer(TestTerrain, Settings) != INDEX_NONE)
	{
		return TRUE;
	}

	return FALSE;
}

UBOOL FModeTool_Terrain::BeginApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal,
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	ModifiedTerrains.Empty();
	return TRUE;
}

void FModeTool_Terrain::EndApplyTool()
{
	for (INT i = 0; i < ModifiedTerrains.Num(); i++)
	{
		ATerrain* Terrain = ModifiedTerrains(i);
		Terrain->MarkPackageDirty();
		Terrain->WeldEdgesToOtherTerrains();
	}
	ModifiedTerrains.Empty();
}

/**
 *	Clear the partial data array.
 *	Called when closing the terrain editor dialog.
 */
void FModeTool_Terrain::ClearPartialData()
{
	for (TMap<ATerrain*, FPartialData*>::TIterator It(PartialValueData); It; ++It)
	{
		FPartialData* Data = It.Value();
		if (Data)
		{
			delete Data;
		}
	}

	PartialValueData.Empty();
}

/**
 *	Add the given terrain to the partial data array.
 *
 *	@param	InTerrain		The terrain of interest
 */
void FModeTool_Terrain::AddTerrainToPartialData(ATerrain* InTerrain)
{
	INT CheckCount = (InTerrain->NumVerticesX + 1) * (InTerrain->NumVerticesY + 1);

	FPartialData** PData = PartialValueData.Find(InTerrain);
	if (PData == NULL)
	{
		// Not there, so add it
		FPartialData* NewPData = new FPartialData();
		check(NewPData);

		INT Count = 1;	// Height maps...
		Count += InTerrain->Layers.Num();
		Count += InTerrain->DecoLayers.Num();

		appMemzero(&(NewPData->EditValues), sizeof(TArray<TerrainPartialValues>));
		NewPData->EditValues.AddZeroed(Count);
		NewPData->LayerCount = InTerrain->Layers.Num();
		NewPData->DecoLayerCount = InTerrain->DecoLayers.Num();

		for (INT EntryIndex = 0; EntryIndex < Count; EntryIndex++)
		{
			TerrainPartialValues* PartialValues = &(NewPData->EditValues(EntryIndex));
			PartialValues->AddZeroed(CheckCount);
		}

		PartialValueData.Set(InTerrain, NewPData);
	}
	else
	{
		FPartialData* OldPData = *PData;

		// See if there is a new layer or deco layer
		if ((OldPData->LayerCount != InTerrain->Layers.Num()) || 
			(OldPData->DecoLayerCount != InTerrain->DecoLayers.Num()))
		{
			RemoveTerrainToPartialData(InTerrain);
			AddTerrainToPartialData(InTerrain);
			return;
		}

		// There will ALWAYS be height values
		TerrainPartialValues* HeightValues = &(OldPData->EditValues(0));
		if (HeightValues->Num() != CheckCount)
		{
			for (INT EntryIndex = 0; EntryIndex < OldPData->EditValues.Num(); EntryIndex++)
			{
				TerrainPartialValues* PartialValues = &(OldPData->EditValues(EntryIndex));
				if (PartialValues)
				{
					// Assume that PostEditChange caused this to occur...
					PartialValues->Empty();
					PartialValues->AddZeroed(CheckCount);
				}
			}
		}
	}
}

/**
 *	Remove the given terrain from the partial data array.
 *
 *	@param	InTerrain		The terrain of interest
 */
void FModeTool_Terrain::RemoveTerrainToPartialData(ATerrain* InTerrain)
{
	FPartialData** PData = PartialValueData.Find(InTerrain);
	if ((PData != NULL) && (*PData != NULL))
	{
		delete *PData;
		PartialValueData.Remove(InTerrain);
	}
}

/**
 *	Clear the partial data array to zeroes.
 *	Called when switching tools.
 */
void FModeTool_Terrain::ZeroAllPartialData()
{
	for (TMap<ATerrain*, FPartialData*>::TIterator It(PartialValueData); It; ++It)
	{
		ATerrain* Terrain = It.Key();
		FPartialData* PData = It.Value();
		if (Terrain && PData)
		{
			INT CheckCount = (Terrain->NumVerticesX + 1) * (Terrain->NumVerticesY + 1);
			for (INT EntryIndex = 0; EntryIndex < PData->EditValues.Num(); EntryIndex++)
			{
				TerrainPartialValues* PartialValues = &(PData->EditValues(EntryIndex));
				if ((PartialValues != NULL) && (PartialValues->Num() != CheckCount))
				{
					// Assume that PostEditChange caused this to occur...
					PartialValues->Empty();
					PartialValues->AddZeroed(CheckCount);
				}
			}
		}
	}
}

/**
 *	Check that the stored count of data is the same as required for the given terrain.
 *	Called when post-edit change on terrain occurs.
 *
 *	@param	InTerrain		The terrain of that had a property change
 */
void FModeTool_Terrain::CheckPartialData(ATerrain* InTerrain)
{
	for (TMap<ATerrain*, FPartialData*>::TIterator It(PartialValueData); It; ++It)
	{
		ATerrain* Terrain = It.Key();
		if (Terrain == InTerrain)
		{
			// Verify that each entry in the list has enough spots
			FPartialData* PData = It.Value();
			if (PData)
			{
				INT CheckCount = (Terrain->NumVerticesX + 1) * (Terrain->NumVerticesY + 1);
				for (INT EntryIndex = 0; EntryIndex < PData->EditValues.Num(); EntryIndex++)
				{
					TerrainPartialValues* PartialValues = &(PData->EditValues(EntryIndex));
					if ((PartialValues != NULL) && (PartialValues->Num() != CheckCount))
					{
						// Assume that PostEditChange caused this to occur...
						PartialValues->Empty();
						PartialValues->AddZeroed(CheckCount);
					}
				}
			}
		}
	}
}

/**
 *	Get the partial data value for the given terrain at the given coordinates.
 *
 *	@param	InTerrain		The terrain of interest
 *	@param	DataIndex		The index of the data required. -1 = Heightmap, otherwise AlphaMapIndex
 *	@param	X				The vertex X
 *	@param	Y				The vertex Y
 *
 *	@return FLOAT			The partial alpha value
 */
FLOAT FModeTool_Terrain::GetPartialData(ATerrain* InTerrain, INT DataIndex, INT X, INT Y)
{
	if ((InTerrain != NULL) && 
		((X >= 0) || (X <= InTerrain->NumVerticesX)) &&
		((Y >= 0) || (Y <= InTerrain->NumVerticesY))
		)
	{
		FPartialData** PData = PartialValueData.Find(InTerrain);
		if (PData != NULL)
		{
			INT Index = DataIndex + 1;
			if ((Index >= 0) && (Index < (*PData)->EditValues.Num()))
			{
				check((*PData)->EditValues(Index).Num() >= (Y * InTerrain->NumVerticesX + X));
				return (*PData)->EditValues(Index)(Y * InTerrain->NumVerticesX + X);
			}
		}
	}

	return 0.0f;
}

/**
 *	Get the alpha value for the given terrain at the given coordinates, including the partial.
 *
 *	@param	InTerrain		The terrain of interest
 *	@param	DataIndex		The index of the data required. -1 = Heightmap, otherwise AlphaMapIndex
 *	@param	X				The vertex X
 *	@param	Y				The vertex Y
 *
 *	@return FLOAT			The current + partial alpha value
 */
FLOAT FModeTool_Terrain::GetData(ATerrain* InTerrain, INT DataIndex, INT X, INT Y)
{
	FLOAT ReturnValue = 0.0f;
	
	if (InTerrain != NULL)
	{
		if (DataIndex == -1)
		{
			ReturnValue = (FLOAT)(InTerrain->Height(X, Y));
		}
		else
		{
			ReturnValue = (FLOAT)(InTerrain->Alpha(DataIndex, X, Y));
		}

		ReturnValue += GetPartialData(InTerrain, DataIndex, X, Y);
	}

	return ReturnValue;
}

/**
 *	Set the partial alpha value for the given terrain at the given coordinates to the given value.
 *
 *	@param	InTerrain		The terrain of interest
 *	@param	DataIndex		The index of the data required. -1 = Heightmap, otherwise AlphaMapIndex
 *	@param	X				The vertex X
 *	@param	Y				The vertex Y
 *	@param	NewPartial		The new value to set it to.
 */
void FModeTool_Terrain::SetPartialData(ATerrain* InTerrain, INT DataIndex, INT X, INT Y, FLOAT NewPartial)
{
	if ((InTerrain != NULL) && 
		((X >= 0) || (X <= InTerrain->NumVerticesX)) &&
		((Y >= 0) || (Y <= InTerrain->NumVerticesY))
		)
	{
		FPartialData** PData = PartialValueData.Find(InTerrain);
		if (PData != NULL)
		{
			INT Index = DataIndex + 1;
			if ((Index < 0) || (Index >= (*PData)->EditValues.Num()))
			{
				return;
			}

			INT ValueIndex = Y * InTerrain->NumVerticesX + X;
			FLOAT TempData = (*PData)->EditValues(Index)(ValueIndex);

			TempData += NewPartial;

			INT TruncValue = appTrunc(TempData);
			if (TruncValue != 0)
			{
				// There is enough movement to actually update the data value...
				if (DataIndex == -1)
				{
					INT OrigHeight = InTerrain->Height(X, Y);
					InTerrain->Height(X, Y) = (WORD)Clamp<INT>(OrigHeight + TruncValue, 0, MAXWORD);
				}
				else
				{
					INT OrigAlpha = InTerrain->Alpha(DataIndex, X, Y);
					InTerrain->Alpha(DataIndex, X, Y) = (BYTE)Clamp<INT>(OrigAlpha + TruncValue, 0, MAXBYTE);
				}
				TempData -= (FLOAT)TruncValue;
			}
			(*PData)->EditValues(Index)(ValueIndex) = TempData;
		}
	}
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainPaint.
-----------------------------------------------------------------------------*/

FModeTool_TerrainPaint::FModeTool_TerrainPaint()
{
	ID = MT_TerrainPaint;
}

UBOOL FModeTool_TerrainPaint::ApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
	FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY, 
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if (Terrain->bLocked == TRUE)
	{
		return FALSE;
	}

	INT AlphaMapIndex = INDEX_NONE;
	if (LayerIndex != INDEX_NONE)
	{
		AlphaMapIndex = DecoLayer ? Terrain->DecoLayers(LayerIndex).AlphaMapIndex : Terrain->Layers(LayerIndex).AlphaMapIndex;
	}
	else
	{
		if (Terrain->bHeightmapLocked == TRUE)
		{
			return FALSE;
		}
	}

	FEdModeTerrainEditing* EdMode = (FEdModeTerrainEditing*)(GEditorModeTools().GetActiveMode(EM_TerrainEdit));
	check(EdMode);

	INT StepSize = 1;
	if ((EdMode->bConstrained == TRUE) && (Terrain->EditorTessellationLevel > 0))
	{
		StepSize = Terrain->MaxTesselationLevel / Terrain->EditorTessellationLevel;
	}

	INT MirrorX;
	INT MirrorY;

	for (INT Y = MinY; Y <= MaxY; Y += StepSize)
	{
		MirrorY = Y;
		for (INT X = MinX; X <= MaxX; X += StepSize)
		{
			FVector	Vertex = Terrain->GetLocalVertex(X,Y);
			FLOAT RadialStrength = RadiusStrength(LocalPosition,Vertex,LocalMinRadius,LocalMaxRadius);
			FLOAT StrengthScalar = LocalStrength * 10.0f * RadialStrength;

			MirrorX = X;
			if (bMirrorY)
			{
				MirrorX = EdMode->GetMirroredValue_X(Terrain, X);
			}
			if (bMirrorX)
			{
				MirrorY = EdMode->GetMirroredValue_Y(Terrain, Y);
			}

			FLOAT NewPartial = (InDirection < 0.0f) ? -StrengthScalar : StrengthScalar;
			if (AlphaMapIndex != INDEX_NONE)
			{
				// Scale for the alpha mappings...
				NewPartial *= (255.0f / 100.0f);
			}

			FModeTool_Terrain::SetPartialData(Terrain, AlphaMapIndex, X, Y, NewPartial);
			// Mirror it
			if (bMirrorY || bMirrorX)
			{
				FModeTool_Terrain::SetPartialData(Terrain, AlphaMapIndex, MirrorX, MirrorY, NewPartial);
			}
		}
	}

	if (StepSize != 1)
	{
		UpdateIntermediateValues(Terrain, DecoLayer, LayerIndex, MaterialIndex, 
			MinX, MinY, MaxX, MaxY, bMirrorX, bMirrorY);
	}

	return TRUE;
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainNoise.
-----------------------------------------------------------------------------*/

FModeTool_TerrainNoise::FModeTool_TerrainNoise()
{
	ID = MT_TerrainNoise;
}

UBOOL FModeTool_TerrainNoise::ApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
	FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY, 
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if (Terrain->bLocked == TRUE)
	{
		return FALSE;
	}

	INT AlphaMapIndex = INDEX_NONE;
	if (LayerIndex != INDEX_NONE)
	{
		AlphaMapIndex = DecoLayer ? Terrain->DecoLayers(LayerIndex).AlphaMapIndex : Terrain->Layers(LayerIndex).AlphaMapIndex;
	}
	else
	{
		if (Terrain->bHeightmapLocked == TRUE)
		{
			return FALSE;
		}
	}

	FEdModeTerrainEditing*  EdMode = (FEdModeTerrainEditing*)GEditorModeTools().GetActiveMode(EM_TerrainEdit);
	check(EdMode);

	INT StepSize = 1;
	if ((EdMode->bConstrained == TRUE) && (Terrain->EditorTessellationLevel > 0))
	{
		StepSize = Terrain->MaxTesselationLevel / Terrain->EditorTessellationLevel;
	}

	INT MirrorX;
	INT MirrorY;
	for (INT Y = MinY; Y <= MaxY; Y += StepSize)
	{
		MirrorY = Y;
		for (INT X = MinX; X <= MaxX; X += StepSize)
		{
			MirrorX = X;
			if (bMirrorY)
			{
				MirrorX = EdMode->GetMirroredValue_X(Terrain, X);
			}
			if (bMirrorX)
			{
				MirrorY = EdMode->GetMirroredValue_Y(Terrain, Y);
			}

			FVector	Vertex = Terrain->GetLocalVertex(X,Y);
			FLOAT NewPartial = RadiusStrength(LocalPosition,Vertex,LocalMinRadius,LocalMaxRadius) * LocalStrength;
			if (AlphaMapIndex == INDEX_NONE)
			{
				NewPartial *= (32.f-(appFrand()*64.f));
			}
			else
			{
				NewPartial *= InDirection * (255.0f / 100.0f) * (8.f-(appFrand()*16.f));
			}

			FModeTool_Terrain::SetPartialData(Terrain, AlphaMapIndex, X, Y, NewPartial);
			// Mirror it
			if (bMirrorY || bMirrorX)
			{
				FModeTool_Terrain::SetPartialData(Terrain, AlphaMapIndex, MirrorX, MirrorY, NewPartial);
			}
		}
	}

	if (StepSize != 1)
	{
		UpdateIntermediateValues(Terrain, DecoLayer, LayerIndex, MaterialIndex, 
			MinX, MinY, MaxX, MaxY, bMirrorX, bMirrorY);
	}

	return TRUE;
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainSmooth.
-----------------------------------------------------------------------------*/

FModeTool_TerrainSmooth::FModeTool_TerrainSmooth()
{
	ID = MT_TerrainSmooth;
}

UBOOL FModeTool_TerrainSmooth::ApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
	FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY, 
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if (Terrain->bLocked == TRUE)
	{
		return FALSE;
	}

	INT	AlphaMapIndex = INDEX_NONE;
	if (LayerIndex != INDEX_NONE)
	{
		AlphaMapIndex = DecoLayer ? Terrain->DecoLayers(LayerIndex).AlphaMapIndex : Terrain->Layers(LayerIndex).AlphaMapIndex;
	}
	else
	{
		if (Terrain->bHeightmapLocked == TRUE)
		{
			return FALSE;
		}
	}

	FEdModeTerrainEditing*  EdMode = (FEdModeTerrainEditing*)GEditorModeTools().GetActiveMode(EM_TerrainEdit);
	check(EdMode);

	FLOAT	Filter[3][3] =
	{
		{ 1, 1, 1 },
		{ 1, 1, 1 },
		{ 1, 1, 1 }
	};
	FLOAT	FilterSum = 0;
	for (UINT Y = 0;Y < 3;Y++)
	{
		for (UINT X = 0;X < 3;X++)
		{
			FilterSum += Filter[X][Y];
		}
	}
	FLOAT	InvFilterSum = 1.0f / FilterSum;

	INT StepSize = 1;
	if ((EdMode->bConstrained == TRUE) && (Terrain->EditorTessellationLevel > 0))
	{
		StepSize = Terrain->MaxTesselationLevel / Terrain->EditorTessellationLevel;
	}

	INT MirrorX;
	INT MirrorY;
	for (INT Y = MinY; Y <= MaxY; Y += StepSize)
	{
		MirrorY = Y;
		for (INT X = MinX; X <= MaxX; X += StepSize)
		{
			MirrorX = X;
			if (bMirrorY)
			{
				MirrorX = EdMode->GetMirroredValue_X(Terrain, X);
			}
			if (bMirrorX)
			{
				MirrorY = EdMode->GetMirroredValue_Y(Terrain, Y);
			}

			FVector	Vertex = Terrain->GetLocalVertex(X,Y);
			FLOAT StrenghScalar = RadiusStrength(LocalPosition,Vertex,LocalMinRadius,LocalMaxRadius) * LocalStrength * InDirection * InvFilterSum;
			if (LayerIndex == INDEX_NONE)
			{
				FLOAT	Height = (FLOAT)Terrain->Height(X,Y) + FModeTool_Terrain::GetPartialHeight(Terrain, X, Y);
				FLOAT	SmoothHeight = 0.0f;

				for (INT AdjacentY = 0;AdjacentY < 3;AdjacentY++)
				{
					for (INT AdjacentX = 0;AdjacentX < 3;AdjacentX++)
					{
						INT VertX = X - 1 + AdjacentX;
						INT VertY = Y - 1 + AdjacentY;

						// prevent array out of bounds crash if X and Y are referencing a vertex on the edge of the map
						Terrain->ClampVertexIndex(VertX, VertY);

						SmoothHeight += (Terrain->Height(VertX, VertY) +
							 FModeTool_Terrain::GetPartialHeight(Terrain, VertX, VertY)) * 
							 Filter[AdjacentX][AdjacentY];
					}
				}
				SmoothHeight *= InvFilterSum;

				FLOAT NewPartial = Lerp(Height,SmoothHeight,Min(StrenghScalar,1.0f)) - Height;
				FModeTool_Terrain::SetPartialHeight(Terrain, X, Y, NewPartial);
				if (bMirrorX || bMirrorY)
				{
					FModeTool_Terrain::SetPartialHeight(Terrain, MirrorX, MirrorY, NewPartial);
				}
			}
			else
			{
				FLOAT	Alpha = (FLOAT)Terrain->Alpha(AlphaMapIndex,X,Y) + FModeTool_Terrain::GetPartialAlpha(Terrain, AlphaMapIndex, X, Y);
				FLOAT	SmoothAlpha = 0.0f;

				for (INT AdjacentY = 0;AdjacentY < 3;AdjacentY++)
				{
					for (INT AdjacentX = 0;AdjacentX < 3;AdjacentX++)
					{
						INT VertX = X - 1 + AdjacentX;
						INT VertY = Y - 1 + AdjacentY;

						// prevent array out of bounds crash if X and Y are referencing a vertex on the edge of the map
						Terrain->ClampVertexIndex(VertX, VertY);

						SmoothAlpha += (Terrain->Alpha(AlphaMapIndex, VertX, VertY) +
							FModeTool_Terrain::GetPartialAlpha(Terrain, AlphaMapIndex, VertX, VertY)) * 
							Filter[AdjacentX][AdjacentY];
					}
				}
				SmoothAlpha *= InvFilterSum;

				FLOAT NewPartial = Lerp(Alpha,SmoothAlpha,Min(StrenghScalar,1.0f)) - Alpha;
				FModeTool_Terrain::SetPartialAlpha(Terrain, AlphaMapIndex, X, Y, NewPartial);
				if (bMirrorX || bMirrorY)
				{
					FModeTool_Terrain::SetPartialAlpha(Terrain, AlphaMapIndex, MirrorX, MirrorY, NewPartial);
				}
			}
		}
	}

	if (StepSize != 1)
	{
		UpdateIntermediateValues(Terrain, DecoLayer, LayerIndex, MaterialIndex, 
			MinX, MinY, MaxX, MaxY, bMirrorX, bMirrorY);
	}

	return TRUE;
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainAverage.
-----------------------------------------------------------------------------*/

FModeTool_TerrainAverage::FModeTool_TerrainAverage()
{
	ID = MT_TerrainAverage;
}

UBOOL FModeTool_TerrainAverage::ApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
	FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY, 
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if (Terrain->bLocked == TRUE)
	{
		return FALSE;
	}

	INT AlphaMapIndex = -1;
	if (LayerIndex != INDEX_NONE)
	{
		AlphaMapIndex = DecoLayer ? Terrain->DecoLayers(LayerIndex).AlphaMapIndex : Terrain->Layers(LayerIndex).AlphaMapIndex;
	}
	else
	{
		if (Terrain->bHeightmapLocked == TRUE)
		{
			return FALSE;
		}
	}

	FEdModeTerrainEditing*  EdMode = (FEdModeTerrainEditing*)GEditorModeTools().GetActiveMode(EM_TerrainEdit);
	check(EdMode);

	FLOAT	Numerator = 0.0f;
	FLOAT	Denominator = 0.0f;

	INT StepSize = 1;
	if ((EdMode->bConstrained == TRUE) && (Terrain->EditorTessellationLevel > 0))
	{
		StepSize = Terrain->MaxTesselationLevel / Terrain->EditorTessellationLevel;
	}

	for (INT Y = MinY; Y <= MaxY; Y += StepSize)
	{
		for (INT X = MinX; X <= MaxX; X += StepSize)
		{
			FLOAT	Strength = RadiusStrength(LocalPosition,Terrain->GetLocalVertex(X,Y),LocalMinRadius,LocalMaxRadius);
			if (LayerIndex == INDEX_NONE)
			{
				Numerator += (FLOAT)Terrain->Height(X,Y) * Strength;
			}
			else
			{
				Numerator += (FLOAT)Terrain->Alpha(AlphaMapIndex,X,Y) * Strength;
			}
			Denominator += Strength;
		}
	}

	FLOAT	Average = Numerator / Denominator;

	INT MirrorX;
	INT MirrorY;
	for (INT Y = MinY; Y <= MaxY; Y += StepSize)
	{
		MirrorY = Y;
		for (INT X = MinX; X <= MaxX; X += StepSize)
		{
			MirrorX = X;
			if (bMirrorY)
			{
				MirrorX = EdMode->GetMirroredValue_X(Terrain, X);
			}
			if (bMirrorX)
			{
				MirrorY = EdMode->GetMirroredValue_Y(Terrain, Y);
			}

			FLOAT ScalarStrength = (LocalStrength*InDirection) * RadiusStrength(LocalPosition,Terrain->GetLocalVertex(X,Y),LocalMinRadius,LocalMaxRadius);
			if (LayerIndex == INDEX_NONE)
			{
				FLOAT Height = (FLOAT)Terrain->Height(X,Y) + FModeTool_Terrain::GetPartialHeight(Terrain, X, Y);
				FLOAT NewPartial = Lerp(Height,Average,Min(1.0f,ScalarStrength)) - Height;
				FModeTool_Terrain::SetPartialHeight(Terrain, X, Y, NewPartial);
				if (bMirrorX || bMirrorY)
				{
					FModeTool_Terrain::SetPartialHeight(Terrain, MirrorX, MirrorY, NewPartial);
				}
			}
			else
			{
				FLOAT Alpha = (FLOAT)(Terrain->Alpha(AlphaMapIndex,X,Y)) + FModeTool_Terrain::GetPartialAlpha(Terrain, AlphaMapIndex, X, Y);
				FLOAT NewPartial = Lerp(Alpha, Average, Min(1.0f, ScalarStrength)) - Alpha;
				FModeTool_Terrain::SetPartialAlpha(Terrain, AlphaMapIndex, X, Y, NewPartial);
				if (bMirrorX || bMirrorY)
				{
					FModeTool_Terrain::SetPartialAlpha(Terrain, AlphaMapIndex, MirrorX, MirrorY, NewPartial);
				}
			}
		}
	}

	if (StepSize != 1)
	{
		UpdateIntermediateValues(Terrain, DecoLayer, LayerIndex, MaterialIndex, 
			MinX, MinY, MaxX, MaxY, bMirrorX, bMirrorY);
	}

	return TRUE;
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainFlatten.
-----------------------------------------------------------------------------*/

FModeTool_TerrainFlatten::FModeTool_TerrainFlatten()
{
	ID = MT_TerrainFlatten;
}

UBOOL FModeTool_TerrainFlatten::BeginApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal,
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if (Terrain->bLocked == TRUE || Terrain->bHeightmapLocked == TRUE)
	{
		return FALSE;
	}

	// For the flatten tool, we do not want to use the exact cursor position, 
	// but rather the position of the ball and stick. (This is what the user
	// expects given the visual clues.)
	FVector LookupPosition;
	if (Terrain->GetClosestLocalSpaceVertex(LocalPosition, LookupPosition, TRUE) == FALSE)
	{
		warnf(TEXT("Failed to find valid vertex for Flatten tool!"));
		return FALSE;
	}

	if (LayerIndex == INDEX_NONE)
	{
		FlatValue = Terrain->Height(appTrunc(LookupPosition.X),appTrunc(LookupPosition.Y));
		FlatWorldPosition = WorldPosition;
		FlatWorldNormal = WorldNormal;
	}
	else
	{
		FlatValue = Terrain->Alpha(
			DecoLayer ? Terrain->DecoLayers(LayerIndex).AlphaMapIndex : Terrain->Layers(LayerIndex).AlphaMapIndex,
			appTrunc(LookupPosition.X),appTrunc(LookupPosition.Y));
		FlatWorldPosition = WorldPosition;
		FlatWorldNormal = WorldNormal;
	}

	return FModeTool_Terrain::BeginApplyTool(Terrain,
		DecoLayer,LayerIndex,MaterialIndex,
		LocalPosition,WorldPosition,WorldNormal,
		bMirrorX, bMirrorY);
}

UBOOL FModeTool_TerrainFlatten::ApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
	FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY, 
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if (Terrain->bLocked == TRUE)
	{
		return FALSE;
	}

	INT AlphaMapIndex = -1;
	if (LayerIndex != INDEX_NONE)
	{
		AlphaMapIndex = DecoLayer ? Terrain->DecoLayers(LayerIndex).AlphaMapIndex : Terrain->Layers(LayerIndex).AlphaMapIndex;
	}
	else
	{
		if (Terrain->bHeightmapLocked == TRUE)
		{
			return FALSE;
		}
	}

	FEdModeTerrainEditing*  EdMode = (FEdModeTerrainEditing*)GEditorModeTools().GetActiveMode(EM_TerrainEdit);
	check(EdMode);
	FTerrainToolSettings*	Settings = (FTerrainToolSettings*)(EdMode->GetSettings());

	INT StepSize = 1;
	if ((EdMode->bConstrained == TRUE) && (Terrain->EditorTessellationLevel > 0))
	{
		StepSize = Terrain->MaxTesselationLevel / Terrain->EditorTessellationLevel;
	}

	INT MirrorX;
	INT MirrorY;
	for (INT Y = MinY; Y <= MaxY; Y += StepSize)
	{
		MirrorY = Y;
		for (INT X = MinX; X <= MaxX; X += StepSize)
		{
			MirrorX = X;
			if (bMirrorY)
			{
				MirrorX = EdMode->GetMirroredValue_X(Terrain, X);
			}
			if (bMirrorX)
			{
				MirrorY = EdMode->GetMirroredValue_Y(Terrain, Y);
			}

			if (RadiusStrength(LocalPosition,Terrain->GetLocalVertex(X,Y),LocalMinRadius,LocalMaxRadius) > 0.0f)
			{
				if (LayerIndex == INDEX_NONE)
				{
					if (Settings->FlattenAngle)
					{
						FVector CurWorldPosition = Terrain->GetWorldVertex(X,Y);
						// P is local vertex projected down
						FVector P = FVector(CurWorldPosition.X, CurWorldPosition.Y, FlatWorldPosition.Z);
						FVector PW = (FlatWorldPosition-P);
						FVector PWn = PW.SafeNormal();
						FLOAT Alpha = appAcos(FlatWorldNormal | PWn);
						FLOAT Theta = (0.5f * PI) - Alpha;
						FVector NewWorldPosition = FVector(CurWorldPosition.X, CurWorldPosition.Y, appTan(Theta) * PW.Size() + FlatWorldPosition.Z);
						FLOAT NewHeight = (Terrain->WorldToLocal().TransformFVector(NewWorldPosition).Z / TERRAIN_ZSCALE) + 32768.0f;
						WORD ClampedHeight = (WORD)Clamp<INT>(appFloor(NewHeight), 0, MAXWORD);
						Terrain->Height(X,Y) = ClampedHeight;
						if (bMirrorX || bMirrorY)
						{
							Terrain->Height(MirrorX,MirrorY) = ClampedHeight;
						}
					}
					else
					if (Settings->UseFlattenHeight)
					{
						FVector CurWorldPosition = Terrain->GetWorldVertex(X,Y);
						FVector NewWorldPosition = FVector(CurWorldPosition.X, CurWorldPosition.Y, Settings->FlattenHeight);
						FLOAT NewHeight = (Terrain->WorldToLocal().TransformFVector(NewWorldPosition).Z / TERRAIN_ZSCALE) + 32768.0f;
						WORD ClampedHeight = (WORD)Clamp<INT>(appFloor(NewHeight), 0, MAXWORD);
						Terrain->Height(X,Y) = ClampedHeight;
						if (bMirrorX || bMirrorY)
						{
							Terrain->Height(MirrorX,MirrorY) = ClampedHeight;
						}
					}
					else
					{
						WORD ClampedHeight = (WORD)Clamp<INT>(appFloor(FlatValue), 0, MAXWORD);
						Terrain->Height(X,Y) = ClampedHeight;
						if (bMirrorX || bMirrorY)
						{
							Terrain->Height(MirrorX,MirrorY) = ClampedHeight;
						}
					}
				}
				else
				{
					BYTE Alpha = (BYTE)Clamp<INT>(appFloor(FlatValue), 0, MAXBYTE);
					Terrain->Alpha(AlphaMapIndex,X,Y) = Alpha;
					if (bMirrorX || bMirrorY)
					{
						Terrain->Alpha(AlphaMapIndex,MirrorX,MirrorY) = Alpha;
					}
				}
			}
		}
	}

	if (StepSize != 1)
	{
		UpdateIntermediateValues(Terrain, DecoLayer, LayerIndex, MaterialIndex, 
			MinX, MinY, MaxX, MaxY, bMirrorX, bMirrorY);
	}

	return TRUE;
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainVisibility.
-----------------------------------------------------------------------------*/
FModeTool_TerrainVisibility::FModeTool_TerrainVisibility()
{
	ID = MT_TerrainVisibility;
}

UBOOL FModeTool_TerrainVisibility::ApplyTool(ATerrain* Terrain, UBOOL DecoLayer, INT LayerIndex, 
	INT MaterialIndex, const FVector& LocalPosition, FLOAT LocalMinRadius, FLOAT LocalMaxRadius, 
	FLOAT InDirection, FLOAT LocalStrength, INT MinX, INT MinY, INT MaxX, INT MaxY, 
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if (Terrain->bLocked == TRUE || Terrain->bHeightmapLocked == TRUE)
	{
		return FALSE;
	}

	UBOOL bVisible = (InDirection < 0.0f);

	// Tag the 'quad' the tool is in
	INT StartX = MinX - (MinX % Terrain->MaxTesselationLevel);
	INT StartY = MinY - (MinY % Terrain->MaxTesselationLevel);
	INT EndX = MaxX - (MaxX % Terrain->MaxTesselationLevel);
	INT EndY = MaxY - (MaxY % Terrain->MaxTesselationLevel);

	if (EndX < StartX + Terrain->MaxTesselationLevel)
	{
		EndX += Terrain->MaxTesselationLevel;
	}
	if (EndY < StartY + Terrain->MaxTesselationLevel)
	{
		EndY += Terrain->MaxTesselationLevel;
	}

	if (bMirrorX || bMirrorY)
	{
		debugf(TEXT("\n"));
		debugf(TEXT("Visibility: MinX,Y   = %4d,%4d - MaxX,Y = %4d,%4d"), MinX, MinY, MaxX, MaxY);
		debugf(TEXT("            StartX,Y = %4d,%4d - EndX,Y = %4d,%4d"), StartX, StartY, EndX, EndY);
	}

	MinX = StartX;
	MinY = StartY;
	MaxX = EndX;
	MaxY = EndY;

	FEdModeTerrainEditing*  EdMode = (FEdModeTerrainEditing*)GEditorModeTools().GetActiveMode(EM_TerrainEdit);
	check(EdMode);

	INT MirrorX;
	INT MirrorY;
	for (INT Y = MinY;Y < MaxY;Y++)
	{
		MirrorY = Y;
		for (INT X = MinX;X < MaxX;X++)
		{
			MirrorX = X;
			if (bMirrorY)
			{
				MirrorX = EdMode->GetMirroredValue_X(Terrain, X, TRUE);
			}
			if (bMirrorX)
			{
				MirrorY = EdMode->GetMirroredValue_Y(Terrain, Y, TRUE);
			}

			FTerrainInfoData* InfoData = Terrain->GetInfoData(X, Y);
			if (InfoData)
			{
				InfoData->SetIsVisible(bVisible);
			}
			if (bMirrorX || bMirrorY)
			{
				InfoData = Terrain->GetInfoData(MirrorX, MirrorY);
				if (InfoData)
				{
					InfoData->SetIsVisible(bVisible);
				}
			}
		}
	}

	return TRUE;
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainTexturePan.
-----------------------------------------------------------------------------*/
FModeTool_TerrainTexturePan::FModeTool_TerrainTexturePan()
{
	ID = MT_TerrainTexturePan;
}

UBOOL FModeTool_TerrainTexturePan::BeginApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal,
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if ((Terrain->bLocked == TRUE) || (LayerIndex == INDEX_NONE))
	{
		return FALSE;
	}

	// Store off the local position
	LastPosition = LocalPosition;

	return FModeTool_Terrain::BeginApplyTool(Terrain,
		DecoLayer,LayerIndex,MaterialIndex,
		LocalPosition,WorldPosition,WorldNormal,
		bMirrorX, bMirrorY);
}

UBOOL FModeTool_TerrainTexturePan::ApplyTool(ATerrain* Terrain, UBOOL DecoLayer, INT LayerIndex,
	INT MaterialIndex, const FVector& LocalPosition, FLOAT LocalMinRadius, FLOAT LocalMaxRadius, 
	FLOAT InDirection, FLOAT LocalStrength, INT MinX, INT MinY, INT MaxX, INT MaxY, 
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if (Terrain->bLocked == TRUE)
	{
		return FALSE;
	}

	if ((LayerIndex == INDEX_NONE) || (DecoLayer == TRUE))
	{
		// Can't pan the height map or the decoration layers...
		return FALSE;
	}

	if (LayerIndex >= Terrain->Layers.Num())
	{
		// Invalid layer index.
		return FALSE;
	}

	// Determine the selected layer and/or material
	FTerrainLayer* Layer = &(Terrain->Layers(LayerIndex));
	FTerrainFilteredMaterial* Material;
	if ((Layer == NULL) || (Layer->Setup == NULL))
	{
		return FALSE;
	}

	FVector Diff = LocalPosition - LastPosition;

	if (MaterialIndex == -1)
	{
		// Pan each material in the layer
		for (INT MtlIndex = 0; MtlIndex < Layer->Setup->Materials.Num(); MtlIndex++)
		{
			Material = &(Layer->Setup->Materials(MtlIndex));
			if (Material && Material->Material)
			{
				UTerrainMaterial* TerrainMat = Material->Material;

				TerrainMat->MappingPanU += (Diff.X / 10.0f) * LocalStrength;
				TerrainMat->MappingPanV += (Diff.Y / 10.0f) * LocalStrength;
			}
		}

		//@todo. Determine a cheaper way to do this... 
		Terrain->PostEditChange();
	}
	else
	{
		if (MaterialIndex < Layer->Setup->Materials.Num())
		{
			// Pan only this material
			Material = &(Layer->Setup->Materials(MaterialIndex));
			if (Material && Material->Material)
			{
				UTerrainMaterial* TerrainMat = Material->Material;

				TerrainMat->MappingPanU += (Diff.X / 10.0f) * LocalStrength;
				TerrainMat->MappingPanV += (Diff.Y / 10.0f) * LocalStrength;

				//@todo. Determine a cheaper way to do this... 
				Terrain->PostEditChange();
			}
		}
	}

	LastPosition = LocalPosition;

	return TRUE;
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainTextureRotate.
-----------------------------------------------------------------------------*/
FModeTool_TerrainTextureRotate::FModeTool_TerrainTextureRotate()
{
	ID = MT_TerrainTextureRotate;
}

UBOOL FModeTool_TerrainTextureRotate::BeginApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal, 
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	// Store off the local position
	LastPosition = LocalPosition;

	return FModeTool_Terrain::BeginApplyTool(Terrain,
		DecoLayer,LayerIndex,MaterialIndex,
		LocalPosition,WorldPosition,WorldNormal,
		bMirrorX, bMirrorY);
}

UBOOL FModeTool_TerrainTextureRotate::ApplyTool(ATerrain* Terrain, UBOOL DecoLayer, INT LayerIndex,
	INT MaterialIndex, const FVector& LocalPosition, FLOAT LocalMinRadius, FLOAT LocalMaxRadius, 
	FLOAT InDirection, FLOAT LocalStrength, INT MinX, INT MinY, INT MaxX, INT MaxY, 
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if (Terrain->bLocked == TRUE)
	{
		return FALSE;
	}

	if ((LayerIndex == INDEX_NONE) || (DecoLayer == TRUE))
	{
		// Can't rotate the height map or the decoration layers...
		return FALSE;
	}

	if (LayerIndex >= Terrain->Layers.Num())
	{
		// Invalid layer index.
		return FALSE;
	}

	// Determine the selected layer and/or material
	FTerrainLayer* Layer = &(Terrain->Layers(LayerIndex));
	FTerrainFilteredMaterial* Material;
	if ((Layer == NULL) || (Layer->Setup == NULL))
	{
		return FALSE;
	}

	FVector Diff = LocalPosition - LastPosition;

	if (MaterialIndex == -1)
	{
		// Rotate each material in the layer
		for (INT MtlIndex = 0; MtlIndex < Layer->Setup->Materials.Num(); MtlIndex++)
		{
			Material = &(Layer->Setup->Materials(MtlIndex));
			if (Material && Material->Material)
			{
				UTerrainMaterial* TerrainMat = Material->Material;

				TerrainMat->MappingRotation += (Diff.X / 10.0f) * LocalStrength;
			}
		}
		//@todo. Determine a cheaper way to do this... 
		Terrain->PostEditChange();
	}
	else
	{
		if (MaterialIndex < Layer->Setup->Materials.Num())
		{
			// Rotate only this material
			Material = &(Layer->Setup->Materials(MaterialIndex));
			if (Material && Material->Material)
			{
				UTerrainMaterial* TerrainMat = Material->Material;

				TerrainMat->MappingRotation += (Diff.X / 10.0f) * LocalStrength;

				//@todo. Determine a cheaper way to do this... 
				Terrain->PostEditChange();
			}
		}
	}

	LastPosition = LocalPosition;

	return TRUE;
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainTextureScale.
-----------------------------------------------------------------------------*/
FModeTool_TerrainTextureScale::FModeTool_TerrainTextureScale()
{
	ID = MT_TerrainTextureScale;
}

UBOOL FModeTool_TerrainTextureScale::BeginApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal,
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	// Store off the local position
	LastPosition = LocalPosition;

	return FModeTool_Terrain::BeginApplyTool(Terrain,
		DecoLayer,LayerIndex,MaterialIndex,
		LocalPosition,WorldPosition,WorldNormal,
		bMirrorX, bMirrorY);
}

UBOOL FModeTool_TerrainTextureScale::ApplyTool(ATerrain* Terrain, UBOOL DecoLayer, INT LayerIndex,
	INT MaterialIndex, const FVector& LocalPosition, FLOAT LocalMinRadius, FLOAT LocalMaxRadius, 
	FLOAT InDirection, FLOAT LocalStrength, INT MinX, INT MinY, INT MaxX, INT MaxY, 
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if (Terrain->bLocked == TRUE)
	{
		return FALSE;
	}

	if ((LayerIndex == INDEX_NONE) || (DecoLayer == TRUE))
	{
		// Can't scale the height map or the decoration layers...
		return FALSE;
	}

	if (LayerIndex >= Terrain->Layers.Num())
	{
		// Invalid layer index.
		return FALSE;
	}

	// Determine the selected layer and/or material
	FTerrainLayer* Layer = &(Terrain->Layers(LayerIndex));
	FTerrainFilteredMaterial* Material;
	if ((Layer == NULL) || (Layer->Setup == NULL))
	{
		return FALSE;
	}

	FVector Diff = LocalPosition - LastPosition;

	if (MaterialIndex == -1)
	{
		// Scale each material in the layer
		for (INT MtlIndex = 0; MtlIndex < Layer->Setup->Materials.Num(); MtlIndex++)
		{
			Material = &(Layer->Setup->Materials(MtlIndex));
			if (Material && Material->Material)
			{
				UTerrainMaterial* TerrainMat = Material->Material;

				TerrainMat->MappingScale += (Diff.X / 10.0f) * LocalStrength;
			}
		}
		//@todo. Determine a cheaper way to do this... 
		Terrain->PostEditChange();
	}
	else
	{
		if (MaterialIndex < Layer->Setup->Materials.Num())
		{
			// Scale only this material
			Material = &(Layer->Setup->Materials(MaterialIndex));
			if (Material && Material->Material)
			{
				UTerrainMaterial* TerrainMat = Material->Material;

				TerrainMat->MappingScale += (Diff.X / 10.0f) * LocalStrength;

		//@todo. Determine a cheaper way to do this... 
				Terrain->PostEditChange();
			}
		}
	}

	LastPosition = LocalPosition;

	return TRUE;
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainSplitX.
-----------------------------------------------------------------------------*/
UBOOL FModeTool_TerrainSplitX::BeginApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal,
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if (appMsgf(AMT_YesNo, *LocalizeUnrealEd(TEXT("Prompt_SplitTerrain"))))
	{
		INT X = appRound(LocalPosition.X);
		X -= (X % Terrain->MaxTesselationLevel);
		Terrain->SplitTerrain(TRUE, X);
	}

	return FALSE;
}

UBOOL FModeTool_TerrainSplitX::RenderTerrain(ATerrain* Terrain, const FVector HitLocation, const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	INT X = appRound(Terrain->WorldToLocal().TransformFVector(HitLocation).X);
	X -= (X % Terrain->MaxTesselationLevel);
	if (X <= 0 || X >= Terrain->NumPatchesX-1)
	{
		return TRUE;
	}

	// Clamp it to the MaxTessellationLevel to prevent invalid sizes
	Terrain->SplitTerrainPreview(PDI, TRUE, X);
	return FALSE;
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainSplitY.
-----------------------------------------------------------------------------*/
UBOOL FModeTool_TerrainSplitY::BeginApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal,
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if (appMsgf(AMT_YesNo, *LocalizeUnrealEd(TEXT("Prompt_SplitTerrain"))))
	{
		INT Y = appRound(LocalPosition.Y);
		Y -= (Y % Terrain->MaxTesselationLevel);
		Terrain->SplitTerrain(FALSE, Y);
	}

	return FALSE;
}

UBOOL FModeTool_TerrainSplitY::RenderTerrain(ATerrain* Terrain, const FVector HitLocation, const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	INT Y = appRound(Terrain->WorldToLocal().TransformFVector(HitLocation).Y);
	Y -= (Y % Terrain->MaxTesselationLevel);
	if (Y <= 0 || Y >= Terrain->NumPatchesY-1)
	{
		return TRUE;
	}
	Terrain->SplitTerrainPreview(PDI, FALSE, Y);
	return FALSE;
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainMerge.
-----------------------------------------------------------------------------*/
UBOOL FModeTool_TerrainMerge::BeginApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal,
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	FEdModeTerrainEditing*  Mode = (FEdModeTerrainEditing*)GEditorModeTools().GetActiveMode(EM_TerrainEdit);
	for (INT i=0;i<Mode->ToolTerrains.Num();i++)
	{
		for (INT j=0;j<Mode->ToolTerrains.Num();j++)
		{
			if (i != j)
			{
				ATerrain* Terrain1 = Mode->ToolTerrains(i);
				ATerrain* Terrain2 = Mode->ToolTerrains(j);
				if (Terrain1 && Terrain2)
				{
					if (Terrain1->MergeTerrainPreview(NULL, Terrain2))
					{
						if (appMsgf(AMT_YesNo, *LocalizeUnrealEd(TEXT("Prompt_MergeTerrain"))))
						{
							Terrain1->MergeTerrain(Terrain2);
						}
						return FALSE;
					}
				}
			}
		}
	}


	return FALSE;
}

UBOOL FModeTool_TerrainMerge::RenderTerrain(ATerrain* Terrain, const FVector HitLocation, const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdModeTerrainEditing*  Mode = (FEdModeTerrainEditing*)GEditorModeTools().GetActiveMode(EM_TerrainEdit);
	for (INT i=0;i<Mode->ToolTerrains.Num();i++)
	{
		for (INT j=0;j<Mode->ToolTerrains.Num();j++)
		{
			if (i != j)
			{
				ATerrain* Terrain1 = Mode->ToolTerrains(i);
				ATerrain* Terrain2 = Mode->ToolTerrains(j);
				if (Terrain1 && Terrain2)
				{
					if (Terrain1 == Terrain2)
					{
						debugf(TEXT("Why is the same terrain in here 2x???"));
					}
					if (Terrain1->MergeTerrainPreview(PDI, Terrain2))
						return TRUE;
				}
			}
		}
	}

	return TRUE;
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainAddRemoveSectors.
-----------------------------------------------------------------------------*/
/**
 * For adding/removing Sectors to existing terrain
 */
UBOOL FModeTool_TerrainAddRemoveSectors::BeginApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal,
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if (Terrain->bLocked == TRUE || Terrain->bHeightmapLocked == TRUE)
	{
		return FALSE;
	}

	CurrentTerrain = Terrain;
	// Store off the local position and direction
	StartPosition = LocalPosition;
	CurrPosition = LocalPosition;

	return FModeTool_Terrain::BeginApplyTool(Terrain,
		DecoLayer,LayerIndex,MaterialIndex,
		LocalPosition,WorldPosition,WorldNormal,
		bMirrorX, bMirrorY);
}

UBOOL FModeTool_TerrainAddRemoveSectors::ApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
	FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY,
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	CurrentTerrain = Terrain;
	// Store off the local position and direction
	CurrPosition = LocalPosition;
	Direction = InDirection;

	return FALSE;
}

void FModeTool_TerrainAddRemoveSectors::EndApplyTool()
{
	if (abs(Direction) < KINDA_SMALL_NUMBER)
	{
		return;
	}
	
	FVector PosDiff = CurrPosition - StartPosition;
	PosDiff.X = (FLOAT)(appTrunc(PosDiff.X) - (appTrunc(PosDiff.X) % CurrentTerrain->MaxTesselationLevel));
	PosDiff.Y = (FLOAT)(appTrunc(PosDiff.Y) - (appTrunc(PosDiff.Y) % CurrentTerrain->MaxTesselationLevel));

	FTerrainToolSettings* TTSettings = (FTerrainToolSettings*)Settings;
	if (TTSettings->MirrorSetting == FTerrainToolSettings::TTMirror_X)
	{
		PosDiff.Y = 0.0f;
	}
	if (TTSettings->MirrorSetting == FTerrainToolSettings::TTMirror_Y)
	{
		PosDiff.X = 0.0f;
	}

	UBOOL bRemoving = Direction < 0.0f;
	UBOOL bAddBottom = PosDiff.X < 0.0f ? TRUE : FALSE;
	UBOOL bAddTop = PosDiff.X > 0.0f ? TRUE : FALSE;
	UBOOL bAddLeft = PosDiff.Y < 0.0f ? TRUE : FALSE;
	UBOOL bAddRight = PosDiff.Y > 0.0f ? TRUE : FALSE;

	INT CountX = 0;
	INT CountY = 0;
	if (bAddLeft || bAddRight)
	{
		CountY = appTrunc(PosDiff.Y) / CurrentTerrain->MaxTesselationLevel;
		if (bRemoving)
		{
			CountY *= -1;
		}
	}
	if (bAddTop || bAddBottom)
	{
		CountX = appTrunc(PosDiff.X) / CurrentTerrain->MaxTesselationLevel;
		if (bRemoving)
		{
			CountX *= -1;
		}
	}

	CurrentTerrain->AddRemoveSectors(CountX, CountY, bRemoving);

	Direction = 0.0f;
}

UBOOL FModeTool_TerrainAddRemoveSectors::RenderTerrain(ATerrain* Terrain, const FVector HitLocation, const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI )
{
	if (abs(Direction) < KINDA_SMALL_NUMBER)
	{
		return FALSE;
	}

	FVector PosDiff = CurrPosition - StartPosition;
	PosDiff.X = (FLOAT)(appTrunc(PosDiff.X) - (appTrunc(PosDiff.X) % Terrain->MaxTesselationLevel));
	PosDiff.Y = (FLOAT)(appTrunc(PosDiff.Y) - (appTrunc(PosDiff.Y) % Terrain->MaxTesselationLevel));

	FTerrainToolSettings* TTSettings = (FTerrainToolSettings*)Settings;
	if (TTSettings->MirrorSetting == FTerrainToolSettings::TTMirror_X)
	{
		PosDiff.Y = 0.0f;
	}
	if (TTSettings->MirrorSetting == FTerrainToolSettings::TTMirror_Y)
	{
		PosDiff.X = 0.0f;
	}

	UBOOL bRemoving = Direction < 0.0f;
	UBOOL bAddBottom = PosDiff.X < 0.0f ? TRUE : FALSE;
	UBOOL bAddTop	 = PosDiff.X > 0.0f ? TRUE : FALSE;
	UBOOL bAddLeft	 = PosDiff.Y < 0.0f ? TRUE : FALSE;
	UBOOL bAddRight  = PosDiff.Y > 0.0f ? TRUE : FALSE;

	FVector Left, Right, Top, Bottom;

	if ((bAddLeft && !bRemoving) || (bAddRight && bRemoving))
	{
		Left = Terrain->GetWorldVertex(0, 0);
		Right = Terrain->GetWorldVertex(Terrain->NumVerticesX - 1, 0);
	}
	else
	if ((bAddRight && !bRemoving) || (bAddLeft && bRemoving))
	{
		Left = Terrain->GetWorldVertex(0, Terrain->NumVerticesY - 1);
		Right = Terrain->GetWorldVertex(Terrain->NumVerticesX - 1, Terrain->NumVerticesY - 1);
	}

	if ((bAddTop && !bRemoving) || (bAddBottom && bRemoving))
	{
		Top = Terrain->GetWorldVertex(Terrain->NumVerticesX - 1, Terrain->NumVerticesY - 1);
		Bottom = Terrain->GetWorldVertex(Terrain->NumVerticesX - 1, 0);
	}
	else
	if ((bAddBottom && !bRemoving) || (bAddTop && bRemoving))
	{
		Top = Terrain->GetWorldVertex(0, Terrain->NumVerticesY - 1);
		Bottom = Terrain->GetWorldVertex(0, 0);
	}

	FLinearColor LineColor = bRemoving ? FLinearColor(1.0f, 0.0f, 0.0f) : FLinearColor(0.0f, 1.0f, 0.0f);
	FLOAT ScaleX = Terrain->DrawScale * Terrain->DrawScale3D.X;
	FLOAT ScaleY = Terrain->DrawScale * Terrain->DrawScale3D.Y;
	FVector AddX = FVector(appTrunc(PosDiff.X) * ScaleX, 0.0f, 0.0f);
	FVector AddY = FVector(0.0f, appTrunc(PosDiff.Y) * ScaleY, 0.0f);
	FVector StepX = FVector(Terrain->MaxTesselationLevel * ScaleX, 0.0f, 0.0f);
	if (PosDiff.X < 0.0f)
	{
		StepX.X *= -1.0f;
	}
	FVector StepY = FVector(0.0f, Terrain->MaxTesselationLevel * ScaleY, 0.0f);
	if (PosDiff.Y < 0.0f)
	{
		StepY.Y *= -1.0f;
	}

	if (bAddLeft || bAddRight)
	{
		PDI->DrawLine(Left, Left + AddY, LineColor, SDPG_Foreground);
		PDI->DrawLine(Right, Right + AddY, LineColor, SDPG_Foreground);
		for (INT Step = 0; Step <= (appTrunc(abs(PosDiff.Y)) / Terrain->MaxTesselationLevel); Step++)
		{
			PDI->DrawLine(Left + StepY * Step, Right + StepY * Step, LineColor, SDPG_Foreground);
		}
	}

	if (bAddTop || bAddBottom)
	{
		PDI->DrawLine(Top, Top + AddX, LineColor, SDPG_Foreground);
		PDI->DrawLine(Bottom, Bottom + AddX, LineColor, SDPG_Foreground);
		for (INT Step = 0; Step <= (appTrunc(abs(PosDiff.X)) / Terrain->MaxTesselationLevel); Step++)
		{
			PDI->DrawLine(Top + StepX * Step, Bottom + StepX * Step, LineColor, SDPG_Foreground);
		}
	}

	return TRUE;
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainOrientationFlip.
-----------------------------------------------------------------------------*/

UBOOL FModeTool_TerrainOrientationFlip::ApplyTool(ATerrain* Terrain, UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius, 
	FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY,
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if (Terrain->bLocked == TRUE)
	{
		return FALSE;
	}

	UBOOL bFlipped = (InDirection > 0.0f);

	// Tag the 'quad' the tool is in
	INT StartX = MinX;
	INT StartY = MinY;
	INT EndX = MaxX;
	INT EndY = MaxY;

	if (EndX < StartX)
	{
		INT Temp = EndX;
		EndX = StartX;
		StartX = Temp;
	}
	if (EndY < StartY)
	{
		INT Temp = EndY;
		EndY = StartY;
		StartY = Temp;
	}

	if (bMirrorX || bMirrorY)
	{
		debugf(TEXT("\n"));
		debugf(TEXT("EdgeFlip  : MinX,Y   = %4d,%4d - MaxX,Y = %4d,%4d"), MinX, MinY, MaxX, MaxY);
		debugf(TEXT("            StartX,Y = %4d,%4d - EndX,Y = %4d,%4d"), StartX, StartY, EndX, EndY);
	}

	MinX = StartX;
	MinY = StartY;
	MaxX = EndX;
	MaxY = EndY;

	FEdModeTerrainEditing*  EdMode = (FEdModeTerrainEditing*)GEditorModeTools().GetActiveMode(EM_TerrainEdit);
	check(EdMode);

	INT MirrorX;
	INT MirrorY;
	for (INT Y = MinY;Y < MaxY;Y++)
	{
		MirrorY = Y;
		for (INT X = MinX;X < MaxX;X++)
		{
			MirrorX = X;
			if (bMirrorY)
			{
				MirrorX = EdMode->GetMirroredValue_X(Terrain, X, TRUE);
			}
			if (bMirrorX)
			{
				MirrorY = EdMode->GetMirroredValue_Y(Terrain, Y, TRUE);
			}

			FTerrainInfoData* InfoData = Terrain->GetInfoData(X, Y);
			if (InfoData)
			{
				InfoData->SetIsOrientationFlipped(bFlipped);
			}
			if (bMirrorX || bMirrorY)
			{
				InfoData = Terrain->GetInfoData(MirrorX, MirrorY);
				if (InfoData)
				{
					InfoData->SetIsOrientationFlipped(bFlipped);
				}
			}
		}
	}

	return TRUE;
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainVertexEdit.
-----------------------------------------------------------------------------*/
/**
* For editing terrain vertices directly.
*/
// FModeTool interface.
UBOOL FModeTool_TerrainVertexEdit::MouseMove(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,INT x, INT y)
{
	return FModeTool_Terrain::MouseMove(ViewportClient, Viewport, x, y);
}

/**
 * @return		TRUE if the delta was handled by this editor mode tool.
 */
UBOOL FModeTool_TerrainVertexEdit::InputAxis(FEditorLevelViewportClient* InViewportClient,FViewport* Viewport,INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime)
{
	if (Key == KEY_MouseY)
	{
		MouseYDelta += Delta;
	}
	return FModeTool::InputAxis(InViewportClient, Viewport, ControllerId, Key, Delta, DeltaTime);
}

/**
 * @return		TRUE if the delta was handled by this editor mode tool.
 */
UBOOL FModeTool_TerrainVertexEdit::InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	return FModeTool_Terrain::InputDelta(InViewportClient, InViewport, InDrag, InRot, InScale);
}

/**
 * @return		TRUE if the key was handled by this editor mode tool.
 */
UBOOL FModeTool_TerrainVertexEdit::InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event)
{
	if (Event == IE_Pressed)
	{
		if (Key == KEY_LeftMouseButton)
		{
			bMouseLeftPressed = TRUE;
		}
		if (Key == KEY_RightMouseButton)
		{
			bMouseRightPressed = TRUE;
			if (IsCtrlDown(Viewport) && IsAltDown(Viewport) && IsShiftDown(Viewport))
			{
				// Deselect all vertices
				for (TObjectIterator<ATerrain> It; It; ++It)
				{
					ATerrain* LocalTerrain = *It;
					LocalTerrain->ClearSelectedVertexList();
				}
			}
		}
		if (IsCtrlDown(Viewport))
		{
			bCtrlIsPressed = TRUE;
		}
		if (IsAltDown(Viewport))
		{
			bAltIsPressed = TRUE;
		}
	}
	else
	if (Event == IE_Released)
	{
		if (Key == KEY_LeftMouseButton)
		{
			bMouseLeftPressed = FALSE;
		}
		if (Key == KEY_RightMouseButton)
		{
			bMouseRightPressed = FALSE;
		}
		if (IsCtrlDown(Viewport) == FALSE)
		{
			bCtrlIsPressed = FALSE;
		}
		if (IsAltDown(Viewport) == FALSE)
		{
			bAltIsPressed = FALSE;
		}
	}

	return FModeTool_Terrain::InputKey(ViewportClient, Viewport, Key, Event);
}

// FModeTool_Terrain interface.
UBOOL FModeTool_TerrainVertexEdit::BeginApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal,
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if ((bCtrlIsPressed == TRUE) && (bAltIsPressed == FALSE))
	{
		if (bMouseLeftPressed)
		{
			bSelectVertices = TRUE;
		}
		else
		if (bMouseRightPressed)
		{
			bDeselectVertices = TRUE;
		}
	}
	return FModeTool_Terrain::BeginApplyTool(Terrain, DecoLayer, LayerIndex, MaterialIndex,
		LocalPosition, WorldPosition, WorldNormal, bMirrorX, bMirrorY);
}

UBOOL FModeTool_TerrainVertexEdit::ApplyTool(ATerrain* Terrain,
	UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
	const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
	FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY,
	UBOOL bMirrorX, UBOOL bMirrorY)
{
	if (Terrain->bLocked == TRUE || Terrain->bHeightmapLocked == TRUE)
	{
		return FALSE;
	}

	if (LayerIndex != INDEX_NONE)
	{
		// We only vertex edit the height fields
		return FALSE;
	}

	if (bCtrlIsPressed == TRUE)
	{
		FEdModeTerrainEditing* EdMode = (FEdModeTerrainEditing*)(GEditorModeTools().GetActiveMode(EM_TerrainEdit));
		check(EdMode);
		FTerrainToolSettings* Settings = (FTerrainToolSettings*)(EdMode->GetSettings());
		check(Settings);

		if (bAltIsPressed == TRUE)
		{
			if (MouseYDelta != 0)
			{
				// We need to keep a list of components with selected verts that were 
				// touched to properly update them. Otherwise, only the verts in the
				// min/max area will be updated.
				TArray<UTerrainComponent*> DirtyComponents;

				FLOAT Moved = (FLOAT)(MouseYDelta) * LocalStrength * Settings->ScaleFactor;
				for (INT VertIndex = 0; VertIndex < Terrain->SelectedVertices.Num(); VertIndex++)
				{
					FSelectedTerrainVertex* SelVert = &(Terrain->SelectedVertices(VertIndex));
					FLOAT Amount = Moved * SelVert->Weight;
					FModeTool_Terrain::SetPartialHeight(Terrain, SelVert->X, SelVert->Y, Amount);
					Terrain->GetComponentsAtXY(SelVert->X, SelVert->Y, DirtyComponents);
				}

				// Update all the components
				for (INT CompIndex = 0; CompIndex < DirtyComponents.Num(); CompIndex++)
				{
					UTerrainComponent* DirtyComp = DirtyComponents(CompIndex);

					Terrain->UpdatePatchBounds(DirtyComp->SectionBaseX, DirtyComp->SectionBaseY, 
						DirtyComp->SectionBaseX + DirtyComp->TrueSectionSizeX, 
						DirtyComp->SectionBaseY + DirtyComp->TrueSectionSizeY);
					Terrain->UpdateRenderData(DirtyComp->SectionBaseX, DirtyComp->SectionBaseY, 
						DirtyComp->SectionBaseX + DirtyComp->TrueSectionSizeX, 
						DirtyComp->SectionBaseY + DirtyComp->TrueSectionSizeY);
				}
			}
		}
		else
		if ((bSelectVertices == TRUE) || (bDeselectVertices == TRUE))
		{
			INT StepSize = 1;
			if ((EdMode->bConstrained == TRUE) && (Terrain->EditorTessellationLevel > 0))
			{
				StepSize = Terrain->MaxTesselationLevel / Terrain->EditorTessellationLevel;
			}

			INT MirrorX;
			INT MirrorY;

			// Radius/Falloff of 0,0 should only select a single vertex!
			if ((LocalMaxRadius == 0.0f) && (LocalMaxRadius == 0.0f))
			{
				MinX = appRound(LocalPosition.X);
				MinY = appRound(LocalPosition.Y);
				MaxX = MinX;
				MaxY = MinY;
				StepSize = 1;
			}

			for (INT Y = MinY; Y <= MaxY; Y += StepSize)
			{
				MirrorY = Y;
				for (INT X = MinX; X <= MaxX; X += StepSize)
				{
					MirrorX = X;
					if (bMirrorY)
					{
						MirrorX = EdMode->GetMirroredValue_X(Terrain, X);
					}
					if (bMirrorX)
					{
						MirrorY = EdMode->GetMirroredValue_Y(Terrain, Y);
					}

					FVector	Vertex = Terrain->GetLocalVertex(X,Y);
					FLOAT CalculatedStrength = RadiusStrength(LocalPosition,Vertex,LocalMinRadius,LocalMaxRadius);
					if (Settings->bSoftSelectionEnabled == FALSE)
					{
						// If not soft-selecting, fully select everything in the tool circle
						CalculatedStrength = 1.0f;
					}
					FLOAT SetWeight = CalculatedStrength;
					SetWeight = Clamp<FLOAT>(SetWeight, 0.0f, 1.0f);
					if (bSelectVertices == FALSE)
					{
						SetWeight *= -1.0f;
					}

					Terrain->UpdateSelectedVertex(X, Y, SetWeight);

					// Mirror it
					if (bMirrorY || bMirrorX)
					{
						Terrain->UpdateSelectedVertex(MirrorX, MirrorY, SetWeight);
					}
				}
			}

			if (StepSize != 1)
			{
				UpdateIntermediateValues(Terrain, DecoLayer, LayerIndex, MaterialIndex, 
					MinX, MinY, MaxX, MaxY, bMirrorX, bMirrorY);
			}
		}
	}
	MouseYDelta = 0;

	return FALSE;
}

void FModeTool_TerrainVertexEdit::EndApplyTool()
{
	bSelectVertices = FALSE;
	bDeselectVertices = FALSE;
}

UBOOL FModeTool_TerrainVertexEdit::RenderTerrain(ATerrain* Terrain, const FVector HitLocation, const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI )
{
	// For each terrain opened, draw their selected vertices
	FEdModeTerrainEditing* EdMode = (FEdModeTerrainEditing*)(GEditorModeTools().GetActiveMode(EM_TerrainEdit));
	check(EdMode);

	for (TObjectIterator<ATerrain> It; It; ++It)
	{
		ATerrain* LocalTerrain = *It;
		if (LocalTerrain->SelectedVertices.Num() > 0)
		{
			for (INT Index = 0; Index < LocalTerrain->SelectedVertices.Num(); Index++)
			{
				FSelectedTerrainVertex& SelVert = LocalTerrain->SelectedVertices(Index);
				// Draw the ball and stick
				FVector Vertex = LocalTerrain->GetWorldVertex(SelVert.X, SelVert.Y);
				
				EdMode->DrawBallAndStick( View, Viewport, PDI, LocalTerrain, Vertex, SelVert.Weight );
			}
		}
	}
	return TRUE;
}

/*------------------------------------------------------------------------------
	UTerrainMaterialFactoryNew implementation.
------------------------------------------------------------------------------*/

//
//	UTerrainMaterialFactoryNew::StaticConstructor
//

void UTerrainMaterialFactoryNew::StaticConstructor()
{
	new(GetClass()->HideCategories) FName(NAME_Object);
}

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UTerrainMaterialFactoryNew::InitializeIntrinsicPropertyValues()
{
	SupportedClass		= UTerrainMaterial::StaticClass();
	bCreateNew			= TRUE;
	bEditAfterNew		= TRUE;
	Description			= SupportedClass->GetName();
}
//
//	UTerrainMaterialFactoryNew::FactoryCreateNew
//

UObject* UTerrainMaterialFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

IMPLEMENT_CLASS(UTerrainMaterialFactoryNew);

/*------------------------------------------------------------------------------
	UTerrainLayerSetupFactoryNew implementation.
------------------------------------------------------------------------------*/

//
//	UTerrainLayerSetupFactoryNew::StaticConstructor
//

void UTerrainLayerSetupFactoryNew::StaticConstructor()
{
#if !CONSOLE
	new(GetClass()->HideCategories) FName(NAME_Object);
#endif
}


/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UTerrainLayerSetupFactoryNew::InitializeIntrinsicPropertyValues()
{
	SupportedClass		= UTerrainLayerSetup::StaticClass();
	bCreateNew			= TRUE;
	bEditAfterNew		= TRUE;
	Description			= SupportedClass->GetName();
}

//
//	UTerrainLayerSetupFactoryNew::FactoryCreateNew
//

UObject* UTerrainLayerSetupFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

IMPLEMENT_CLASS(UTerrainLayerSetupFactoryNew);
