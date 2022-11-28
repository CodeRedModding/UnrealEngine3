/*=============================================================================
	UnTerrainEdit.cpp: Terrain editing code.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EditorPrivate.h"
#include "UnTerrain.h"

/*-----------------------------------------------------------------------------
	FModeTool_Terrain.
-----------------------------------------------------------------------------*/

FModeTool_Terrain::FModeTool_Terrain()
{
	ID = MT_None;
	bUseWidget = 0;
	Settings = new FTerrainToolSettings;

	PaintingViewportClient = NULL;
}

FModeTool_Terrain::~FModeTool_Terrain()
{
}

UBOOL FModeTool_Terrain::MouseMove(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,INT x, INT y)
{
	ViewportClient->Viewport->InvalidateDisplay();

	return 1;

}

UBOOL FModeTool_Terrain::InputDelta(FEditorLevelViewportClient* InViewportClient,FChildViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	if(PaintingViewportClient == InViewportClient)
		return 1;
	else
		return 0;
}

UBOOL FModeTool_Terrain::InputKey(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,FName Key,EInputEvent Event)
{
	if((PaintingViewportClient == NULL || PaintingViewportClient == ViewportClient) && (Key == KEY_LeftMouseButton || Key == KEY_RightMouseButton))
	{
		if(Viewport->IsCtrlDown() && Event == IE_Pressed)
		{
			FSceneView	View;
			ViewportClient->CalcSceneView(&View);

			FTerrainToolSettings*	Settings = (FTerrainToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
			FChildViewport*			Viewport = ViewportClient->Viewport;
			FVector					HitLocation;
			ATerrain*				Terrain = TerrainTrace(Viewport,&View,HitLocation);

			if(Terrain)
			{
				BeginApplyTool(Terrain,Settings->DecoLayer,Settings->LayerIndex,Terrain->WorldToLocal().TransformFVector(HitLocation));

				PaintingViewportClient = ViewportClient;
				Viewport->CaptureMouseInput(1);
			}

			return 1;
		}
		else if(PaintingViewportClient)
		{
			PaintingViewportClient = NULL;
			Viewport->CaptureMouseInput(0);
			return 1;
		}
		else
			return 0;
	}

	return 0;

}

void FModeTool_Terrain::Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
{
	FTerrainToolSettings*	Settings = (FTerrainToolSettings*)GEditorModeTools.GetCurrentMode()->GetSettings();
	FChildViewport*			Viewport = ViewportClient->Viewport;
	FVector					HitLocation;

	FSceneView	View;
	ViewportClient->CalcSceneView(&View);

	ATerrain*	Terrain = TerrainTrace(Viewport,&View,HitLocation);

	if(PaintingViewportClient == ViewportClient)
	{
		if(Terrain)
		{
			FLOAT	Direction = Viewport->KeyState(KEY_LeftMouseButton) ? 1.f : -1.f;
			FVector	LocalPosition = Terrain->WorldToLocal().TransformFVector(HitLocation); // In the terrain actor's local space.
			FLOAT	LocalStrength = Settings->Strength * DeltaTime / Terrain->DrawScale3D.Z / TERRAIN_ZSCALE,
					LocalMinRadius = Settings->RadiusMin / Terrain->DrawScale3D.X / Terrain->DrawScale,
					LocalMaxRadius = Settings->RadiusMax / Terrain->DrawScale3D.X / Terrain->DrawScale;

			INT	MinX = Clamp(appFloor(LocalPosition.X - LocalMaxRadius),0,Terrain->NumVerticesX - 1),
				MinY = Clamp(appFloor(LocalPosition.Y - LocalMaxRadius),0,Terrain->NumVerticesY - 1),
				MaxX = Clamp(appCeil(LocalPosition.X + LocalMaxRadius),0,Terrain->NumVerticesX - 1),
				MaxY = Clamp(appCeil(LocalPosition.Y + LocalMaxRadius),0,Terrain->NumVerticesY - 1);

			ApplyTool( Terrain, Settings->DecoLayer, Settings->LayerIndex, LocalPosition, LocalMinRadius, LocalMaxRadius, Direction, LocalStrength, MinX, MinY, MaxX, MaxY );
			Terrain->UpdateRenderData(MinX,MinY,MaxX,MaxY);

			// Force viewport update
			Viewport->Invalidate();
		}
	}

}

ATerrain* FModeTool_Terrain::TerrainTrace(FChildViewport* Viewport,FSceneView* View,FVector& Location)
{
	if(Viewport)
	{
		FMatrix	WorldToFrustum = View->ViewMatrix * View->ProjectionMatrix;
		FVector	W = FVector(WorldToFrustum.M[0][3],WorldToFrustum.M[1][3],WorldToFrustum.M[2][3]),
				Z = FVector(WorldToFrustum.M[0][2],WorldToFrustum.M[1][2],WorldToFrustum.M[2][2]);

		FLOAT	MinZ = 0.0f,
				MaxZ = 0.5f,
				MinW = 1.0f,
				MaxW = 1.0f;

		if((Z|W) > DELTA*DELTA)
		{
			MinW = (MinZ * WorldToFrustum.M[3][3] - WorldToFrustum.M[3][2]) / ((Z|W) - MinZ) + WorldToFrustum.M[3][3];
			MaxW = (MaxZ * WorldToFrustum.M[3][3] - WorldToFrustum.M[3][2]) / ((Z|W) - MaxZ) + WorldToFrustum.M[3][3];
		}

		// Calculate world-space location of mouse cursor.

		INT		X = Viewport->GetMouseX(),
				Y = Viewport->GetMouseY();
		FVector	Start = View->Deproject( FPlane( (X-Viewport->GetSizeX()/2.0f)/(Viewport->GetSizeX()/2.0f),(Y-Viewport->GetSizeY()/2.0f)/-(Viewport->GetSizeY()/2.0f),MinZ,MinW) ),
				Dir	= View->Deproject( FPlane( (X-Viewport->GetSizeX()/2.0f)/(Viewport->GetSizeX()/2.0f),(Y-Viewport->GetSizeY()/2.0f)/-(Viewport->GetSizeY()/2.0f),MaxZ,MaxW) ) - Start,
				End = Start + Dir.SafeNormal() * WORLD_MAX;

		// Do the trace.

		FCheckResult	Hit(1);

		if(GEditor->Level)
		{
			GEditor->Level->SingleLineCheck(Hit,NULL,End,Start,TRACE_Terrain);
			if(Cast<ATerrain>(Hit.Actor))
			{
				Location = Hit.Location;
				return Cast<ATerrain>(Hit.Actor);
			}
		}
	}

	return NULL;

}

/**
 * Determines how strongly the editing circles affect a vertex.
 *
 * @param	ToolCenter		The location being pointed at on the terrain
 * @param	Vertex			The vertex being affected
 * @param	MinRadius		The outer edge of the inner circle
 * @param	MaxRadius		The outer edge of the outer circle
 *
 * @return	A vaue between 0-1, representing how strongly the tool should affect the vertex.
 */

FLOAT FModeTool_Terrain::RadiusStrength(const FVector& ToolCenter,const FVector& Vertex,FLOAT MinRadius,FLOAT MaxRadius)
{
	FLOAT	Distance = (Vertex - ToolCenter).Size2D();

	if(Distance <= MinRadius)
		return 1.0f;
	else if(Distance < MaxRadius)
		return 1.0f - (Distance - MinRadius) / (MaxRadius - MinRadius);
	else
		return 0.0f;
}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainPaint.
-----------------------------------------------------------------------------*/

FModeTool_TerrainPaint::FModeTool_TerrainPaint()
{
	ID = MT_TerrainPaint;
}

FModeTool_TerrainPaint::~FModeTool_TerrainPaint()
{
}

void FModeTool_TerrainPaint::ApplyTool(ATerrain* Terrain,UBOOL DecoLayer,INT LayerIndex,const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY)
{
	for(INT Y = MinY;Y <= MaxY;Y++)
	{
		for(INT X = MinX;X <= MaxX;X++)
		{
			if(LayerIndex == INDEX_NONE)
			{
				FVector	Vertex = Terrain->GetLocalVertex(X,Y);
				Terrain->Height(X,Y) =
					(_WORD)Clamp<INT>(
						Terrain->Height(X,Y) + (LocalStrength*InDirection) * 10.0f * RadiusStrength(LocalPosition,Vertex,LocalMinRadius,LocalMaxRadius),
						0,
						MAXWORD
						);
			}
			else
			{
				FVector	Vertex = Terrain->GetLocalVertex(X,Y);
				BYTE&	Alpha = Terrain->Alpha(DecoLayer ? Terrain->DecoLayers(LayerIndex).AlphaMapIndex : Terrain->Layers(LayerIndex).AlphaMapIndex,X,Y);
				Alpha = (BYTE)Clamp<INT>(
							appFloor(Alpha + (LocalStrength*InDirection) * (255.0f / 100.0f) * 10.0f * RadiusStrength(LocalPosition,Vertex,LocalMinRadius,LocalMaxRadius)),
							0,
							MAXBYTE
							);
			}
		}
	}

}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainNoise.
-----------------------------------------------------------------------------*/

FModeTool_TerrainNoise::FModeTool_TerrainNoise()
{
	ID = MT_TerrainNoise;
}

FModeTool_TerrainNoise::~FModeTool_TerrainNoise()
{
}

void FModeTool_TerrainNoise::ApplyTool(ATerrain* Terrain,UBOOL DecoLayer,INT LayerIndex,const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY)
{
	for(INT Y = MinY;Y <= MaxY;Y++)
	{
		for(INT X = MinX;X <= MaxX;X++)
		{
			if(LayerIndex == INDEX_NONE)
			{
				FVector	Vertex = Terrain->GetLocalVertex(X,Y);
				Terrain->Height(X,Y) = 
					(_WORD)Clamp<INT>(
						appFloor(Terrain->Height(X,Y) + ((32.f-(appFrand()*64.f)) * LocalStrength) * RadiusStrength(LocalPosition,Vertex,LocalMinRadius,LocalMaxRadius)),
						0,
						MAXWORD
						);
			}
			else
			{
				FVector	Vertex = Terrain->GetLocalVertex(X,Y);
				BYTE&	Alpha = Terrain->Alpha(DecoLayer ? Terrain->DecoLayers(LayerIndex).AlphaMapIndex : Terrain->Layers(LayerIndex).AlphaMapIndex,X,Y);
				Alpha = (BYTE)Clamp<INT>(
							appFloor(Alpha + (LocalStrength*InDirection) * (255.0f / 100.0f) * (8.f-(appFrand()*16.f)) * RadiusStrength(LocalPosition,Vertex,LocalMinRadius,LocalMaxRadius)),
							0,
							MAXBYTE
							);
			}
		}
	}

}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainSmooth.
-----------------------------------------------------------------------------*/

FModeTool_TerrainSmooth::FModeTool_TerrainSmooth()
{
	ID = MT_TerrainSmooth;
}

FModeTool_TerrainSmooth::~FModeTool_TerrainSmooth()
{
}

void FModeTool_TerrainSmooth::ApplyTool(ATerrain* Terrain,UBOOL DecoLayer,INT LayerIndex,const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY)
{
	FLOAT	Filter[3][3] =
	{
		{ 1, 1, 1 },
		{ 1, 1, 1 },
		{ 1, 1, 1 }
	};
	FLOAT	FilterSum = 0;
	for(UINT Y = 0;Y < 3;Y++)
		for(UINT X = 0;X < 3;X++)
			FilterSum += Filter[X][Y];
	FLOAT	InvFilterSum = 1.0f / FilterSum;

	INT&	AlphaMapIndex = DecoLayer ? Terrain->DecoLayers(LayerIndex).AlphaMapIndex : Terrain->Layers(LayerIndex).AlphaMapIndex;

	for(INT Y = MinY;Y <= MaxY;Y++)
	{
		for(INT X = MinX;X <= MaxX;X++)
		{
			if(LayerIndex == INDEX_NONE)
			{
				FVector	Vertex = Terrain->GetLocalVertex(X,Y);
				FLOAT	Height = (FLOAT)Terrain->Height(X,Y),
						SmoothHeight = 0.0f;

				for(INT AdjacentY = 0;AdjacentY < 3;AdjacentY++)
					for(INT AdjacentX = 0;AdjacentX < 3;AdjacentX++)
						SmoothHeight += Terrain->Height(X - 1 + AdjacentX,Y - 1 + AdjacentY) * Filter[AdjacentX][AdjacentY];
				SmoothHeight *= InvFilterSum;

				Terrain->Height(X,Y) =
					(_WORD)Clamp<INT>(
						appFloor(Lerp(Height,SmoothHeight,Min(((LocalStrength*InDirection) / 100.0f) * RadiusStrength(LocalPosition,Vertex,LocalMinRadius,LocalMaxRadius),1.0f))),
						0,
						MAXWORD
						);
			}
			else
			{
				FVector	Vertex = Terrain->GetLocalVertex(X,Y);
				FLOAT	Alpha = (FLOAT)Terrain->Alpha(AlphaMapIndex,X,Y),
						SmoothAlpha = 0.0f;

				for(INT AdjacentY = 0;AdjacentY < 3;AdjacentY++)
					for(INT AdjacentX = 0;AdjacentX < 3;AdjacentX++)
						SmoothAlpha += Terrain->Alpha(AlphaMapIndex,X - 1 + AdjacentX,Y - 1 + AdjacentY) * Filter[AdjacentX][AdjacentY];
				SmoothAlpha *= InvFilterSum;

				Terrain->Alpha(AlphaMapIndex,X,Y) =
					(_WORD)Clamp<INT>(
						appFloor(Lerp(Alpha,SmoothAlpha,Min(((LocalStrength*InDirection) / 100.0f) * RadiusStrength(LocalPosition,Vertex,LocalMinRadius,LocalMaxRadius),1.0f))),
						0,
						MAXBYTE
						);
			}
		}
	}

}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainAverage.
-----------------------------------------------------------------------------*/

FModeTool_TerrainAverage::FModeTool_TerrainAverage()
{
	ID = MT_TerrainAverage;
}

FModeTool_TerrainAverage::~FModeTool_TerrainAverage()
{
}

void FModeTool_TerrainAverage::ApplyTool(ATerrain* Terrain,UBOOL DecoLayer,INT LayerIndex,const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY)
{
	FLOAT	Numerator = 0.0f;
	FLOAT	Denominator = 0.0f;

	for(INT Y = MinY;Y <= MaxY;Y++)
	{
		for(INT X = MinX;X <= MaxX;X++)
		{
			FLOAT	Strength = RadiusStrength(LocalPosition,Terrain->GetLocalVertex(X,Y),LocalMinRadius,LocalMaxRadius);
			if(LayerIndex == INDEX_NONE)
				Numerator += (FLOAT)Terrain->Height(X,Y) * Strength;
			else
				Numerator += (FLOAT)Terrain->Alpha(DecoLayer ? Terrain->DecoLayers(LayerIndex).AlphaMapIndex : Terrain->Layers(LayerIndex).AlphaMapIndex,X,Y) * Strength;
			Denominator += Strength;
		}
	}

	FLOAT	Average = Numerator / Denominator;

	for(INT Y = MinY;Y <= MaxY;Y++)
	{
		for(INT X = MinX;X <= MaxX;X++)
		{
			if(LayerIndex == INDEX_NONE)
			{
				FLOAT	Height = (FLOAT)Terrain->Height(X,Y);
				Terrain->Height(X,Y) =
					(_WORD)Clamp<INT>(
						appFloor(Lerp(Height,Average,Min(1.0f,(LocalStrength*InDirection) * RadiusStrength(LocalPosition,Terrain->GetLocalVertex(X,Y),LocalMinRadius,LocalMaxRadius)))),
						0,
						MAXWORD
						);
			}
			else
			{
				BYTE&	Alpha = Terrain->Alpha(DecoLayer ? Terrain->DecoLayers(LayerIndex).AlphaMapIndex : Terrain->Layers(LayerIndex).AlphaMapIndex,X,Y);
				Alpha =
					(BYTE)Clamp<INT>(
						appFloor(Lerp((FLOAT)Alpha,Average,Min(1.0f,(LocalStrength*InDirection) * RadiusStrength(LocalPosition,Terrain->GetLocalVertex(X,Y),LocalMinRadius,LocalMaxRadius)))),
						0,
						MAXBYTE
						);
			}
		}
	}

}

/*-----------------------------------------------------------------------------
	FModeTool_TerrainFlatten.
-----------------------------------------------------------------------------*/

FModeTool_TerrainFlatten::FModeTool_TerrainFlatten()
{
	ID = MT_TerrainFlatten;
}

FModeTool_TerrainFlatten::~FModeTool_TerrainFlatten()
{
}

void FModeTool_TerrainFlatten::BeginApplyTool(ATerrain* Terrain,UBOOL DecoLayer,INT LayerIndex,const FVector& LocalPosition)
{
	if(LayerIndex == INDEX_NONE)
		FlatValue = Terrain->Height(appTrunc(LocalPosition.X),appTrunc(LocalPosition.Y));
	else
		FlatValue = Terrain->Alpha(DecoLayer ? Terrain->DecoLayers(LayerIndex).AlphaMapIndex : Terrain->Layers(LayerIndex).AlphaMapIndex,appTrunc(LocalPosition.X),appTrunc(LocalPosition.Y));

}

void FModeTool_TerrainFlatten::ApplyTool(ATerrain* Terrain,UBOOL DecoLayer,INT LayerIndex,const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY)
{
	for(INT Y = MinY;Y <= MaxY;Y++)
	{
		for(INT X = MinX;X <= MaxX;X++)
		{
			if(RadiusStrength(LocalPosition,Terrain->GetLocalVertex(X,Y),LocalMinRadius,LocalMaxRadius) > 0.0f)
			{
				if(LayerIndex == INDEX_NONE)
					Terrain->Height(X,Y) =
						(_WORD)Clamp<INT>(
							appFloor(FlatValue),
							0,
							MAXWORD
							);
				else
					Terrain->Alpha(DecoLayer ? Terrain->DecoLayers(LayerIndex).AlphaMapIndex : Terrain->Layers(LayerIndex).AlphaMapIndex,X,Y) =
						(BYTE)Clamp<INT>(
							appFloor(FlatValue),
							0,
							MAXBYTE
							);
			}
		}
	}

}

/*------------------------------------------------------------------------------
	UTerrainMaterialFactoryNew implementation.
------------------------------------------------------------------------------*/

//
//	UTerrainMaterialFactoryNew::StaticConstructor
//

void UTerrainMaterialFactoryNew::StaticConstructor()
{
	SupportedClass		= UTerrainMaterial::StaticClass();
	bCreateNew			= 1;
	Description			= TEXT("TerrainMaterial");
	new(GetClass()->HideCategories) FName(TEXT("Object"));
}

//
//	UTerrainMaterialFactoryNew::FactoryCreateNew
//

UObject* UTerrainMaterialFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn)
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
	SupportedClass		= UTerrainLayerSetup::StaticClass();
	bCreateNew			= 1;
	Description			= TEXT("TerrainLayerSetup");
	new(GetClass()->HideCategories) FName(TEXT("Object"));
}

//
//	UTerrainLayerSetupFactoryNew::FactoryCreateNew
//

UObject* UTerrainLayerSetupFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

IMPLEMENT_CLASS(UTerrainLayerSetupFactoryNew);
