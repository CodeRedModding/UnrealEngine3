/*================================================================================
	LandscapeEdModeBrushes.cpp: Landscape editing mode bruses
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/

#include "UnrealEd.h"
#include "UnObjectTools.h"
#include "LandscapeEdMode.h"
#include "ScopedTransaction.h"
#include "EngineTerrainClasses.h"
#include "LandscapeEdit.h"
#include "LandscapeRender.h"

#include "LevelUtils.h"

// 
// FLandscapeBrush
//

void FLandscapeBrush::BeginStroke(FLOAT LandscapeX, FLOAT LandscapeY, class FLandscapeTool* CurrentTool)
{
	GEditor->BeginTransaction( *FString::Printf(LocalizeSecure(LocalizeUnrealEd("LandscapeMode_EditTransaction"), CurrentTool->GetIconString())) );
}

void FLandscapeBrush::EndStroke()
{
	GEditor->EndTransaction();
}

// 
// FLandscapeBrushCircle
//

class FLandscapeBrushCircle : public FLandscapeBrush
{
	TSet<ULandscapeComponent*> BrushMaterialComponents;
protected:
	FVector2D LastMousePosition;
	UMaterialInstanceConstant* BrushMaterial;
	virtual FLOAT CalculateFalloff( FLOAT Distance, FLOAT Radius, FLOAT Falloff ) = 0;
public:
	class FEdModeLandscape* EdMode;

	FLandscapeBrushCircle(class FEdModeLandscape* InEdMode)
		:	EdMode(InEdMode)
		,	LastMousePosition(0,0)
	{
		BrushMaterial = ConstructObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass());
		BrushMaterial->AddToRoot();
	}

	virtual ~FLandscapeBrushCircle()
	{
		BrushMaterial->RemoveFromRoot();
	}

	void LeaveBrush()
	{
		for( TSet<ULandscapeComponent*>::TIterator It(BrushMaterialComponents); It; ++It )
		{
			if( (*It)->EditToolRenderData != NULL )
			{
				(*It)->EditToolRenderData->Update(NULL);
			}
		}
		BrushMaterialComponents.Empty();
	}

	void BeginStroke(FLOAT LandscapeX, FLOAT LandscapeY, class FLandscapeTool* CurrentTool)
	{
		FLandscapeBrush::BeginStroke(LandscapeX,LandscapeY,CurrentTool);
		LastMousePosition = FVector2D(LandscapeX, LandscapeY);
	}

	void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
	{
		ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo;
		if( LandscapeInfo && LandscapeInfo->LandscapeProxy )
		{
			FLOAT ScaleXY = LandscapeInfo->LandscapeProxy->DrawScale3D.X * LandscapeInfo->LandscapeProxy->DrawScale;
			FLOAT Radius = (1.f - EdMode->UISettings.GetBrushFalloff()) * EdMode->UISettings.GetBrushRadius() / ScaleXY;
			FLOAT Falloff = EdMode->UISettings.GetBrushFalloff() * EdMode->UISettings.GetBrushRadius() / ScaleXY;

			// Set params for brush material.
			FVector WorldLocation = LandscapeInfo->LandscapeProxy->LocalToWorld().TransformFVector(FVector(LastMousePosition.X,LastMousePosition.Y,0));
			BrushMaterial->SetScalarParameterValue(FName(TEXT("LocalRadius")), Radius);
			BrushMaterial->SetScalarParameterValue(FName(TEXT("LocalFalloff")), Falloff);
			BrushMaterial->SetVectorParameterValue(FName(TEXT("WorldPosition")), FLinearColor(WorldLocation.X,WorldLocation.Y,WorldLocation.Z,ScaleXY));

			// Set brush material.
			INT X1 = appFloor(LastMousePosition.X - (Radius+Falloff));
			INT Y1 = appFloor(LastMousePosition.Y - (Radius+Falloff));
			INT X2 = appCeil(LastMousePosition.X + (Radius+Falloff));
			INT Y2 = appCeil(LastMousePosition.Y + (Radius+Falloff));

			TSet<ULandscapeComponent*> NewComponents;
			LandscapeInfo->GetComponentsInRegion(X1,Y1,X2,Y2,NewComponents);

			// Set brush material for components in new region
			for( TSet<ULandscapeComponent*>::TIterator It(NewComponents); It; ++It )
			{
				if( (*It)->EditToolRenderData != NULL )
				{
					(*It)->EditToolRenderData->Update(BrushMaterial);
				}
			}

			// Remove the material from any old components that are no longer in the region
			TSet<ULandscapeComponent*> RemovedComponents = BrushMaterialComponents.Difference(NewComponents);
			for ( TSet<ULandscapeComponent*>::TIterator It(RemovedComponents); It; ++It )
			{
				if( (*It)->EditToolRenderData != NULL )
				{
					(*It)->EditToolRenderData->Update(NULL);
				}
			}

			BrushMaterialComponents = NewComponents;	
		}
	}

	void MouseMove(FLOAT LandscapeX, FLOAT LandscapeY)
	{
		LastMousePosition = FVector2D(LandscapeX, LandscapeY);
	}

	UBOOL InputKey( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, FName InKey, EInputEvent InEvent )
	{
		UBOOL bUpdate = FALSE;
		return bUpdate;
	}

	FLOAT GetBrushExtent()
	{
		ALandscapeProxy* Proxy = EdMode->CurrentToolTarget.LandscapeInfo ? EdMode->CurrentToolTarget.LandscapeInfo->LandscapeProxy : NULL;
		if (Proxy)
		{
			FLOAT ScaleXY = Proxy->DrawScale3D.X * Proxy->DrawScale;
			return 2.f * EdMode->UISettings.GetBrushRadius() / ScaleXY;
		}
		return 0.f;
	}

	UBOOL ApplyBrush( TMap<QWORD, FLOAT>& OutBrush, INT& X1, INT& Y1, INT& X2, INT& Y2 )
	{
		ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo;
		ALandscapeProxy* Proxy = LandscapeInfo ? LandscapeInfo->LandscapeProxy : NULL;
		if (Proxy)
		{
			FLOAT ScaleXY = Proxy->DrawScale3D.X * Proxy->DrawScale;

			FLOAT Radius = (1.f - EdMode->UISettings.GetBrushFalloff()) * EdMode->UISettings.GetBrushRadius() / ScaleXY;
			FLOAT Falloff = EdMode->UISettings.GetBrushFalloff() * EdMode->UISettings.GetBrushRadius() / ScaleXY;

			X1 = appFloor(LastMousePosition.X - (Radius+Falloff));
			Y1 = appFloor(LastMousePosition.Y - (Radius+Falloff));
			X2 = appCeil(LastMousePosition.X + (Radius+Falloff));
			Y2 = appCeil(LastMousePosition.Y + (Radius+Falloff));

			UBOOL bHasOutput = FALSE;
			for( INT Y=Y1;Y<=Y2;Y++ )
			{
				for( INT X=X1;X<=X2;X++ )
				{
					QWORD VertexKey = ALandscape::MakeKey(X,Y);

					// Distance from mouse
					FLOAT MouseDist = appSqrt(Square(LastMousePosition.X-(FLOAT)X) + Square(LastMousePosition.Y-(FLOAT)Y));

					FLOAT PaintAmount = CalculateFalloff(MouseDist, Radius, Falloff);

					if( PaintAmount > 0.f )
					{
						if (EdMode->CurrentToolSet && EdMode->CurrentToolSet->GetTool() && EdMode->CurrentToolSet->GetTool()->GetToolType() != FLandscapeTool::TT_Mask 
							&& EdMode->UISettings.GetbUseSelectedRegion())
						{
							FLOAT MaskValue = LandscapeInfo->SelectedRegion.FindRef(VertexKey);
							if (EdMode->UISettings.GetbUseNegativeMask())
							{
								MaskValue = 1.f - MaskValue;
							}
							PaintAmount *= MaskValue;
						}
						// Set the brush value for this vertex
						OutBrush.Set(VertexKey, PaintAmount);
						bHasOutput = TRUE;
					}
				}
			}

			if (!bHasOutput) // For one quad case...
			{
				QWORD VertexKey = ALandscape::MakeKey(appFloor(LastMousePosition.X),appFloor(LastMousePosition.Y));
				OutBrush.Set(VertexKey, 1.f);
			}
			
			return (X1 <= X2 && Y1 <= Y2);
		}
		return FALSE;
	}
};

// 
// FLandscapeBrushComponent
//

class FLandscapeBrushComponent : public FLandscapeBrush
{
	TSet<ULandscapeComponent*> BrushMaterialComponents;

	const TCHAR* GetIconString() { return TEXT("Component"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Brush_Component"); };
protected:
	FVector2D LastMousePosition;
	UMaterial* BrushMaterial;
public:
	class FEdModeLandscape* EdMode;

	FLandscapeBrushComponent(class FEdModeLandscape* InEdMode)
		:	EdMode(InEdMode),
		BrushMaterial(NULL)
	{
		BrushMaterial = LoadObject<UMaterial>(NULL, TEXT("EditorLandscapeResources.SelectBrushMaterial"), NULL, LOAD_None, NULL);
		if (BrushMaterial)
		{
			BrushMaterial->AddToRoot();
		}
	}

	virtual ~FLandscapeBrushComponent()
	{
		if (BrushMaterial)
		{
			BrushMaterial->RemoveFromRoot();
		}
	}

	virtual EBrushType GetBrushType() { return BT_Component; }

	void LeaveBrush()
	{
		for( TSet<ULandscapeComponent*>::TIterator It(BrushMaterialComponents); It; ++It )
		{
			if( (*It)->EditToolRenderData != NULL )
			{
				(*It)->EditToolRenderData->Update(NULL);
			}
		}
		BrushMaterialComponents.Empty();
	}

	void BeginStroke(FLOAT LandscapeX, FLOAT LandscapeY, class FLandscapeTool* CurrentTool)
	{
		FLandscapeBrush::BeginStroke(LandscapeX,LandscapeY,CurrentTool);
		LastMousePosition = FVector2D(LandscapeX, LandscapeY);
	}

	void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
	{
		ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo;
		ALandscapeProxy* Proxy = LandscapeInfo ? LandscapeInfo->LandscapeProxy : NULL;

		if( Proxy )
		{
			INT X = appRound(LastMousePosition.X);
			INT Y = appRound(LastMousePosition.Y);

			INT ComponentIndexX = (X >= 0.f) ? X / Proxy->ComponentSizeQuads : (X+1) / Proxy->ComponentSizeQuads - 1;
			INT ComponentIndexY = (Y >= 0.f) ? Y / Proxy->ComponentSizeQuads : (Y+1) / Proxy->ComponentSizeQuads - 1;
			TSet<ULandscapeComponent*> NewComponents;
			INT BrushSize = Max(EdMode->UISettings.GetBrushComponentSize()-1, 0);
			for (int Y = -(BrushSize>>1); Y <= (BrushSize>>1) + (BrushSize%2); ++Y )
			{
				for (int X = -(BrushSize>>1); X <= (BrushSize>>1) + (BrushSize%2); ++X )
				{
					ULandscapeComponent* Component = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey((ComponentIndexX+X)*Proxy->ComponentSizeQuads,(ComponentIndexY+Y)*Proxy->ComponentSizeQuads));
					if (Component && FLevelUtils::IsLevelVisible(Component->GetLandscapeProxy()->GetLevel()) )
					{
						// For MoveToLevel
						if (EdMode->CurrentToolIndex == EdMode->MoveToLevelToolIndex )
						{
							if ( Component->GetLandscapeProxy() && Component->GetLandscapeProxy()->GetLevel() != GWorld->CurrentLevel )
							{
								NewComponents.Add(Component);
							}
						}
						else
						{
							NewComponents.Add(Component);
						}
					}
				}
			}

			// Set brush material for components in new region
			for( TSet<ULandscapeComponent*>::TIterator It(NewComponents); It; ++It )
			{
				if( (*It)->EditToolRenderData != NULL )
				{
					(*It)->EditToolRenderData->Update(BrushMaterial);
				}
			}

			// Remove the material from any old components that are no longer in the region
			TSet<ULandscapeComponent*> RemovedComponents = BrushMaterialComponents.Difference(NewComponents);
			for ( TSet<ULandscapeComponent*>::TIterator It(RemovedComponents); It; ++It )
			{
				if( (*It)->EditToolRenderData != NULL )
				{
					(*It)->EditToolRenderData->Update(NULL);
				}
			}

			BrushMaterialComponents = NewComponents;	
		}
	}

	void MouseMove(FLOAT LandscapeX, FLOAT LandscapeY)
	{
		LastMousePosition = FVector2D(LandscapeX, LandscapeY);
	}

	UBOOL InputKey( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, FName InKey, EInputEvent InEvent )
	{
		UBOOL bUpdate = FALSE;
		return bUpdate;
	}

	FLOAT GetBrushExtent()
	{
		ALandscapeProxy* Proxy = EdMode->CurrentToolTarget.LandscapeInfo ? EdMode->CurrentToolTarget.LandscapeInfo->LandscapeProxy : NULL;
		if (Proxy)
		{
			return EdMode->UISettings.GetBrushComponentSize() * (Proxy->ComponentSizeQuads+1);
		}
		return 0.f;
	}

	UBOOL ApplyBrush( TMap<QWORD, FLOAT>& OutBrush, INT& X1, INT& Y1, INT& X2, INT& Y2 )
	{
		// Selection Brush only works for 
		ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo;
		ALandscapeProxy* Proxy = LandscapeInfo ? LandscapeInfo->LandscapeProxy : NULL;
		if (Proxy)
		{
			X1= INT_MAX;
			Y1= INT_MAX;
			X2= INT_MIN;
			Y2= INT_MIN;

			// Check for add component...
			if( EdMode->LandscapeRenderAddCollision )
			{
				// Apply Brush size..
				INT X = appRound(LastMousePosition.X);
				INT Y = appRound(LastMousePosition.Y);

				INT ComponentIndexX = (X >= 0.f) ? X / Proxy->ComponentSizeQuads : (X+1) / Proxy->ComponentSizeQuads - 1;
				INT ComponentIndexY = (Y >= 0.f) ? Y / Proxy->ComponentSizeQuads : (Y+1) / Proxy->ComponentSizeQuads - 1;

				INT BrushSize = Max(EdMode->UISettings.GetBrushComponentSize()-1, 0);

				X1 = (ComponentIndexX -(BrushSize>>1)) * Proxy->ComponentSizeQuads;
				X2 = (ComponentIndexX + (BrushSize>>1) + (BrushSize%2) + 1) * Proxy->ComponentSizeQuads;
				Y1 = (ComponentIndexY -(BrushSize>>1)) * Proxy->ComponentSizeQuads;
				Y2 = (ComponentIndexY + (BrushSize>>1) + (BrushSize%2) + 1) * Proxy->ComponentSizeQuads;
			}
			else
			{
				// Get extent for all components
				for ( TSet<ULandscapeComponent*>::TIterator It(BrushMaterialComponents); It; ++It )
				{
					if( *It )
					{
						if( (*It)->SectionBaseX < X1 )
						{
							X1 = (*It)->SectionBaseX;
						}
						if( (*It)->SectionBaseY < Y1 )
						{
							Y1 = (*It)->SectionBaseY;
						}
						if( (*It)->SectionBaseX+(*It)->ComponentSizeQuads > X2 )
						{
							X2 = (*It)->SectionBaseX+(*It)->ComponentSizeQuads;
						}
						if( (*It)->SectionBaseY+(*It)->ComponentSizeQuads > Y2 )
						{
							Y2 = (*It)->SectionBaseY+(*It)->ComponentSizeQuads;
						}
					}
				}
			}

			// Should not be possible...
			//check(X1 <= X2 && Y1 <= Y2);

			for( INT Y=Y1;Y<=Y2;Y++ )
			{
				for( INT X=X1;X<=X2;X++ )
				{
					QWORD VertexKey = ALandscape::MakeKey(X,Y);

					FLOAT PaintAmount = 1.0f;
					if (EdMode->CurrentToolSet && EdMode->CurrentToolSet->GetTool() && EdMode->CurrentToolSet->GetTool()->GetToolType() != FLandscapeTool::TT_Mask 
						&& EdMode->UISettings.GetbUseSelectedRegion())
					{
						FLOAT MaskValue = LandscapeInfo->SelectedRegion.FindRef(VertexKey);
						if (EdMode->UISettings.GetbUseNegativeMask())
						{
							MaskValue = 1.f - MaskValue;
						}
						PaintAmount *= MaskValue;
					}

					// Set the brush value for this vertex
					OutBrush.Set(VertexKey, PaintAmount);
				}
			}

			return (X1 <= X2 && Y1 <= Y2);
		}
		return FALSE;
	}
};

// 
// FLandscapeBrushGizmo
//

class FLandscapeBrushGizmo : public FLandscapeBrush
{
	TSet<ULandscapeComponent*> BrushMaterialComponents;

	const TCHAR* GetIconString() { return TEXT("Gizmo"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Brush_Gizmo"); };
protected:
	FVector2D LastMousePosition;
	UMaterialInstanceConstant* BrushMaterial;
public:
	class FEdModeLandscape* EdMode;

	FLandscapeBrushGizmo(class FEdModeLandscape* InEdMode)
		:	EdMode(InEdMode),
		BrushMaterial(NULL)
	{
		BrushMaterial = ConstructObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass());
		if (BrushMaterial)
		{
			//UMaterialInstanceConstant* GizmoMaterial = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.SelectBrushMaterial_Gizmo"), NULL, LOAD_None, NULL);
			UMaterialInstanceConstant* GizmoMaterial = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.MaskBrushMaterial_Gizmo"), NULL, LOAD_None, NULL);
			BrushMaterial->SetParent(GizmoMaterial);
			BrushMaterial->AddToRoot();
		}
	}

	virtual ~FLandscapeBrushGizmo()
	{
		if (BrushMaterial)
		{
			BrushMaterial->RemoveFromRoot();
		}
	}

	virtual EBrushType GetBrushType() { return BT_Gizmo; }

	void LeaveBrush()
	{
		for( TSet<ULandscapeComponent*>::TIterator It(BrushMaterialComponents); It; ++It )
		{
			if( (*It)->EditToolRenderData != NULL )
			{
				(*It)->EditToolRenderData->Update(NULL);
			}
		}
		BrushMaterialComponents.Empty();
	}

	void BeginStroke(FLOAT LandscapeX, FLOAT LandscapeY, class FLandscapeTool* CurrentTool)
	{
		FLandscapeBrush::BeginStroke(LandscapeX,LandscapeY,CurrentTool);
		LastMousePosition = FVector2D(LandscapeX, LandscapeY);
	}

	void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
	{
		if (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo || GLandscapeEditRenderMode & ELandscapeEditRenderMode::Select)
		{
			ALandscapeGizmoActiveActor* Gizmo = EdMode->CurrentGizmoActor;
			if( Gizmo && Gizmo->TargetLandscapeInfo && (Gizmo->TargetLandscapeInfo == EdMode->CurrentToolTarget.LandscapeInfo) && Gizmo->GizmoTexture )
			{
				ULandscapeInfo* LandscapeInfo = Gizmo->TargetLandscapeInfo;
				ALandscapeProxy* Proxy = LandscapeInfo ? LandscapeInfo->LandscapeProxy : NULL;
				if (Proxy)
				{
					FVector XAxis, YAxis, Origin;
					FMatrix WToL = Proxy->WorldToLocal();
					FMatrix LToW = Proxy->LocalToWorld();
					FVector BaseLocation = WToL.TransformFVector(Gizmo->Location);
					FLOAT ScaleXY = Proxy->DrawScale * Proxy->DrawScale3D.X;
					FLOAT ScaleZ = Proxy->DrawScale * Proxy->DrawScale3D.Z;
					const FLOAT W = Gizmo->GetWidth() / (2 * ScaleXY);
					const FLOAT H = Gizmo->GetHeight() / (2 * ScaleXY);
					const FLOAT L = Gizmo->GetLength() / ScaleZ;
					FMatrix GizmoRT = FRotationTranslationMatrix(FRotator(0, Gizmo->Rotation.Yaw, 0), FVector(BaseLocation.X, BaseLocation.Y, 0)) * LToW;
					Gizmo->FrustumVerts[0] = GizmoRT.TransformFVector(FVector( - W, - H, BaseLocation.Z + L ));
					Gizmo->FrustumVerts[1] = GizmoRT.TransformFVector(FVector( + W, - H, BaseLocation.Z + L ));
					Gizmo->FrustumVerts[2] = GizmoRT.TransformFVector(FVector( + W, + H, BaseLocation.Z + L ));
					Gizmo->FrustumVerts[3] = GizmoRT.TransformFVector(FVector( - W, + H, BaseLocation.Z + L ));

					UTexture2D* DataTexture = Gizmo->GizmoTexture;
					INT MinX = MAXINT, MaxX = MININT, MinY = MAXINT, MaxY = MININT;
					FVector LocalPos[4];
					//FMatrix WorldToLocal = Proxy->LocalToWorld().Inverse();
					for (INT i = 0; i < 4; ++i)
					{
						//LocalPos[i] = WorldToLocal.TransformFVector(Gizmo->FrustumVerts[i]);
						LocalPos[i] = WToL.TransformFVector(Gizmo->FrustumVerts[i]);
						MinX = Min(MinX, (INT)LocalPos[i].X);
						MinY = Min(MinY, (INT)LocalPos[i].Y);
						MaxX = Max(MaxX, (INT)LocalPos[i].X);
						MaxY = Max(MaxY, (INT)LocalPos[i].Y);
					}

					TSet<ULandscapeComponent*> NewComponents;
					LandscapeInfo->GetComponentsInRegion(MinX, MinY, MaxX, MaxY, NewComponents);

					FLOAT SquaredScaleXY = Proxy->DrawScale3D.X * Proxy->DrawScale * Proxy->DrawScale3D.X * Proxy->DrawScale;
					FLinearColor AlphaScaleBias(
						SquaredScaleXY / (EdMode->CurrentGizmoActor->GetWidth() * DataTexture->SizeX),
						SquaredScaleXY / (EdMode->CurrentGizmoActor->GetHeight() * DataTexture->SizeY),
						Gizmo->TextureScale.X,
						Gizmo->TextureScale.Y
						);
					BrushMaterial->SetVectorParameterValue(FName(TEXT("AlphaScaleBias")), AlphaScaleBias);

					FLOAT Angle = (-EdMode->CurrentGizmoActor->Rotation.Euler().Z) * PI / 180.f;
					FLinearColor LandscapeLocation(EdMode->CurrentGizmoActor->Location.X, EdMode->CurrentGizmoActor->Location.Y, EdMode->CurrentGizmoActor->Location.Z, Angle);
					BrushMaterial->SetVectorParameterValue(FName(TEXT("LandscapeLocation")), LandscapeLocation);
					BrushMaterial->SetTextureParameterValue(FName(TEXT("AlphaTexture")), DataTexture);

					// Set brush material for components in new region
					for( TSet<ULandscapeComponent*>::TIterator It(NewComponents); It; ++It )
					{
						if( (*It)->EditToolRenderData != NULL )
						{
							(*It)->EditToolRenderData->UpdateGizmo((Gizmo->DataType != LGT_None) && (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo)? BrushMaterial : NULL);
						}
					}

					// Remove the material from any old components that are no longer in the region
					TSet<ULandscapeComponent*> RemovedComponents = BrushMaterialComponents.Difference(NewComponents);
					for ( TSet<ULandscapeComponent*>::TIterator It(RemovedComponents); It; ++It )
					{
						if( (*It)->EditToolRenderData != NULL )
						{
							(*It)->EditToolRenderData->UpdateGizmo(NULL);
						}
					}

					BrushMaterialComponents = NewComponents;	
				}
			}
		}
	}

	void MouseMove(FLOAT LandscapeX, FLOAT LandscapeY)
	{
		LastMousePosition = FVector2D(LandscapeX, LandscapeY);
	}

	UBOOL InputKey( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, FName InKey, EInputEvent InEvent )
	{
		UBOOL bUpdate = FALSE;
		return bUpdate;
	}

	FLOAT GetBrushExtent()
	{
		ALandscapeProxy* Proxy = EdMode->CurrentToolTarget.LandscapeInfo ? EdMode->CurrentToolTarget.LandscapeInfo->LandscapeProxy : NULL;
		FLOAT ScaleXY = Proxy->DrawScale3D.X * Proxy->DrawScale;

		return 2.f * EdMode->UISettings.GetBrushRadius() / ScaleXY;
	}

	UBOOL ApplyBrush( TMap<QWORD, FLOAT>& OutBrush, INT& X1, INT& Y1, INT& X2, INT& Y2 )
	{
		// Selection Brush only works for 
		ALandscapeGizmoActiveActor* Gizmo = EdMode->CurrentGizmoActor;
		ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo;
		ALandscapeProxy* Proxy = LandscapeInfo ? LandscapeInfo->LandscapeProxy : NULL;

		if (Gizmo && Proxy)
		{
			Gizmo->TargetLandscapeInfo = LandscapeInfo;
			FLOAT ScaleXY = Proxy->DrawScale3D.X * Proxy->DrawScale;
			//FLOAT Radius = EdMode->UISettings.GetBrushRadius() / ScaleXY;

			X1= INT_MAX;
			Y1= INT_MAX;
			X2= INT_MIN;
			Y2= INT_MIN;

			// Get extent for all components
			for ( TSet<ULandscapeComponent*>::TIterator It(BrushMaterialComponents); It; ++It )
			{
				if( *It )
				{
					if( (*It)->SectionBaseX < X1 )
					{
						X1 = (*It)->SectionBaseX;
					}
					if( (*It)->SectionBaseY < Y1 )
					{
						Y1 = (*It)->SectionBaseY;
					}
					if( (*It)->SectionBaseX+(*It)->ComponentSizeQuads > X2 )
					{
						X2 = (*It)->SectionBaseX+(*It)->ComponentSizeQuads;
					}
					if( (*It)->SectionBaseY+(*It)->ComponentSizeQuads > Y2 )
					{
						Y2 = (*It)->SectionBaseY+(*It)->ComponentSizeQuads;
					}
				}
			}

			// Should not be possible...
			//check(X1 <= X2 && Y1 <= Y2);

			//FMatrix LandscapeToGizmoLocal = Landscape->LocalToWorld() * Gizmo->WorldToLocal();
			const FLOAT LW = Gizmo->GetWidth() / (2 * ScaleXY);
			const FLOAT LH = Gizmo->GetHeight() / (2 * ScaleXY);

			FMatrix WToL = Proxy->WorldToLocal();
			//FMatrix LToW = Landscape->LocalToWorld();
			FVector BaseLocation = WToL.TransformFVector(Gizmo->Location);
			FMatrix LandscapeToGizmoLocal = 
				(FTranslationMatrix(FVector(- LW + 0.5, - LH + 0.5, 0)) * FRotationTranslationMatrix(FRotator(0, Gizmo->Rotation.Yaw, 0), FVector(BaseLocation.X, BaseLocation.Y, 0))).Inverse();

			FLOAT W = Gizmo->GetWidth() / ScaleXY; //Gizmo->GetWidth() / (Gizmo->DrawScale * Gizmo->DrawScale3D.X);
			FLOAT H = Gizmo->GetHeight() / ScaleXY; //Gizmo->GetHeight() / (Gizmo->DrawScale * Gizmo->DrawScale3D.Y);

			for( INT Y=Y1;Y<=Y2;Y++ )
			{
				for( INT X=X1;X<=X2;X++ )
				{
					QWORD VertexKey = ALandscape::MakeKey(X,Y);

					FVector GizmoLocal = LandscapeToGizmoLocal.TransformFVector(FVector(X,Y,0));
					if (GizmoLocal.X <= W && GizmoLocal.X >= 0 && GizmoLocal.Y <= H && GizmoLocal.Y >= 0)
					{
						FLOAT PaintAmount = 1.f;
						// Transform in 0,0 origin LW radius
						if (EdMode->UISettings.GetbSmoothGizmoBrush())
						{
							FVector TransformedLocal(Abs(GizmoLocal.X - LW), Abs(GizmoLocal.Y - LH) * (W / H), 0);
							FLOAT FalloffRadius = LW * EdMode->UISettings.GetBrushFalloff();
							FLOAT SquareRadius = LW - FalloffRadius;
							FLOAT Cos = Abs(TransformedLocal.X) / TransformedLocal.Size2D();
							FLOAT Sin = Abs(TransformedLocal.Y) / TransformedLocal.Size2D();
							FLOAT RatioX = FalloffRadius > 0.f ? 1.f - Clamp((Abs(TransformedLocal.X) - Cos*SquareRadius) / FalloffRadius, 0.f, 1.f) : 1.f;
							FLOAT RatioY = FalloffRadius > 0.f ? 1.f - Clamp((Abs(TransformedLocal.Y) - Sin*SquareRadius) / FalloffRadius, 0.f, 1.f) : 1.f;
							FLOAT Ratio = TransformedLocal.Size2D() > SquareRadius ? RatioX * RatioY : 1.f; //TransformedLocal.X / LW * TransformedLocal.Y / LW;
							PaintAmount = Ratio*Ratio*(3-2*Ratio); //Lerp(SquareFalloff, RectFalloff*RectFalloff, Ratio);
						}

						if (PaintAmount)
						{
							if (EdMode->CurrentToolSet && EdMode->CurrentToolSet->GetTool() && EdMode->CurrentToolSet->GetTool()->GetToolType() != FLandscapeTool::TT_Mask 
								&& EdMode->UISettings.GetbUseSelectedRegion())
							{
								FLOAT MaskValue = LandscapeInfo->SelectedRegion.FindRef(VertexKey);
								if (EdMode->UISettings.GetbUseNegativeMask())
								{
									MaskValue = 1.f - MaskValue;
								}
								PaintAmount *= MaskValue;
							}

							// Set the brush value for this vertex
							OutBrush.Set(VertexKey, PaintAmount);
						}
					}
				}
			}
		}
		return (X1 <= X2 && Y1 <= Y2);
	}
};

class FLandscapeBrushCircle_Linear : public FLandscapeBrushCircle
{
public:
	FLandscapeBrushCircle_Linear(class FEdModeLandscape* InEdMode)
		:	FLandscapeBrushCircle(InEdMode)
	{
		UMaterialInstanceConstant* CircleBrushMaterial_Linear = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.CircleBrushMaterial_Linear"), NULL, LOAD_None, NULL);
		BrushMaterial->SetParent(CircleBrushMaterial_Linear);
	}


	const TCHAR* GetIconString() { return TEXT("Circle_linear"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Brush_Falloff_Linear"); };

protected:
	virtual FLOAT CalculateFalloff( FLOAT Distance, FLOAT Radius, FLOAT Falloff )
	{
		return Distance < Radius ? 1.f : 
			Falloff > 0.f ? Max<FLOAT>(0.f, 1.f - (Distance - Radius) / Falloff) : 
			0.f;
	}
};

class FLandscapeBrushCircle_Smooth : public FLandscapeBrushCircle_Linear
{
public:
	FLandscapeBrushCircle_Smooth(class FEdModeLandscape* InEdMode)
		:	FLandscapeBrushCircle_Linear(InEdMode)
	{
		UMaterialInstanceConstant* CircleBrushMaterial_Smooth = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.CircleBrushMaterial_Smooth"), NULL, LOAD_None, NULL);
		BrushMaterial->SetParent(CircleBrushMaterial_Smooth);
	}

	const TCHAR* GetIconString() { return TEXT("Circle_smooth"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Brush_Falloff_Smooth"); };

protected:
	virtual FLOAT CalculateFalloff( FLOAT Distance, FLOAT Radius, FLOAT Falloff )
	{
		FLOAT y = FLandscapeBrushCircle_Linear::CalculateFalloff(Distance, Radius, Falloff);
		// Smooth-step it
		return y*y*(3-2*y);
	}
};

class FLandscapeBrushCircle_Spherical : public FLandscapeBrushCircle
{
public:
	FLandscapeBrushCircle_Spherical(class FEdModeLandscape* InEdMode)
		:	FLandscapeBrushCircle(InEdMode)
	{
		UMaterialInstanceConstant* CircleBrushMaterial_Spherical = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.CircleBrushMaterial_Spherical"), NULL, LOAD_None, NULL);
		BrushMaterial->SetParent(CircleBrushMaterial_Spherical);
	}

	const TCHAR* GetIconString() { return TEXT("Circle_spherical"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Brush_Falloff_Spherical"); };

protected:
	virtual FLOAT CalculateFalloff( FLOAT Distance, FLOAT Radius, FLOAT Falloff )
	{
		if( Distance <= Radius )
		{
			return 1.f;
		}

		if( Distance > Radius + Falloff )
		{
			return 0.f;
		}

		// Elliptical falloff
		return appSqrt( 1.f - Square((Distance - Radius) / Falloff) );
	}
};

class FLandscapeBrushCircle_Tip : public FLandscapeBrushCircle
{
public:
	FLandscapeBrushCircle_Tip(class FEdModeLandscape* InEdMode)
		:	FLandscapeBrushCircle(InEdMode)
	{
		UMaterialInstanceConstant* CircleBrushMaterial_Tip = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.CircleBrushMaterial_Tip"), NULL, LOAD_None, NULL);
		BrushMaterial->SetParent(CircleBrushMaterial_Tip);
	}

	const TCHAR* GetIconString() { return TEXT("Circle_tip"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Brush_Falloff_Tip"); };

protected:
	virtual FLOAT CalculateFalloff( FLOAT Distance, FLOAT Radius, FLOAT Falloff )
	{
		if( Distance <= Radius )
		{
			return 1.f;
		}

		if( Distance > Radius + Falloff )
		{
			return 0.f;
		}

		// inverse elliptical falloff
		return 1.f - appSqrt( 1.f - Square((Falloff + Radius - Distance) / Falloff) );
	}
};


// FLandscapeBrushAlphaBase
class FLandscapeBrushAlphaBase : public FLandscapeBrushCircle_Smooth
{
public:
	FLandscapeBrushAlphaBase(class FEdModeLandscape* InEdMode)
		:	FLandscapeBrushCircle_Smooth(InEdMode)
	{}

	FLOAT GetAlphaSample( FLOAT SampleX, FLOAT SampleY )
	{
		INT SizeX = EdMode->UISettings.GetAlphaTextureSizeX();
		INT SizeY = EdMode->UISettings.GetAlphaTextureSizeY();

		// Bilinear interpolate the values from the alpha texture
		INT SampleX0 = appFloor(SampleX);
		INT SampleX1 = (SampleX0+1) % SizeX;
		INT SampleY0 = appFloor(SampleY);
		INT SampleY1 = (SampleY0+1) % SizeY;

		const BYTE* AlphaData = EdMode->UISettings.GetAlphaTextureData();

		FLOAT Alpha00 = (FLOAT)AlphaData[ SampleX0 + SampleY0 * SizeX ] / 255.f;
		FLOAT Alpha01 = (FLOAT)AlphaData[ SampleX0 + SampleY1 * SizeX ] / 255.f;
		FLOAT Alpha10 = (FLOAT)AlphaData[ SampleX1 + SampleY0 * SizeX ] / 255.f;
		FLOAT Alpha11 = (FLOAT)AlphaData[ SampleX1 + SampleY1 * SizeX ] / 255.f;

		return Lerp(
			Lerp( Alpha00, Alpha01, appFractional(SampleX) ),
			Lerp( Alpha10, Alpha11, appFractional(SampleX) ),
			appFractional(SampleY)
			);
	}

};

//
// FLandscapeBrushAlphaPattern
//
class FLandscapeBrushAlphaPattern : public FLandscapeBrushAlphaBase
{

public:
	FLandscapeBrushAlphaPattern(class FEdModeLandscape* InEdMode)
		:	FLandscapeBrushAlphaBase(InEdMode)
	{
		UMaterialInstanceConstant* PatternBrushMaterial = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.PatternBrushMaterial_Smooth"), NULL, LOAD_None, NULL);
		BrushMaterial->SetParent(PatternBrushMaterial);
	}

	virtual EBrushType GetBrushType() { return BT_Alpha; }

	UBOOL ApplyBrush( TMap<QWORD, FLOAT>& OutBrush, INT& X1, INT& Y1, INT& X2, INT& Y2 )
	{
		ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo;
		ALandscapeProxy* Proxy = LandscapeInfo ? LandscapeInfo->LandscapeProxy : NULL;
		if (Proxy)
		{
			FLOAT ScaleXY = Proxy->DrawScale3D.X * Proxy->DrawScale;

			FLOAT Radius = (1.f - EdMode->UISettings.GetBrushFalloff()) * EdMode->UISettings.GetBrushRadius() / ScaleXY;
			FLOAT Falloff = EdMode->UISettings.GetBrushFalloff() * EdMode->UISettings.GetBrushRadius() / ScaleXY;

			INT SizeX = EdMode->UISettings.GetAlphaTextureSizeX();
			INT SizeY = EdMode->UISettings.GetAlphaTextureSizeY();

			X1 = appFloor(LastMousePosition.X - (Radius+Falloff));
			Y1 = appFloor(LastMousePosition.Y - (Radius+Falloff));
			X2 = appCeil(LastMousePosition.X + (Radius+Falloff));
			Y2 = appCeil(LastMousePosition.Y + (Radius+Falloff));

			for( INT Y=Y1;Y<=Y2;Y++ )
			{
				for( INT X=X1;X<=X2;X++ )
				{
					QWORD VertexKey = ALandscape::MakeKey(X,Y);

					// Find alphamap sample location
					FLOAT SampleX = (FLOAT)X / EdMode->UISettings.GetAlphaBrushScale() + (FLOAT)SizeX * EdMode->UISettings.GetAlphaBrushPanU();
					FLOAT SampleY = (FLOAT)Y / EdMode->UISettings.GetAlphaBrushScale() + (FLOAT)SizeY * EdMode->UISettings.GetAlphaBrushPanV();

					FLOAT Angle = PI * EdMode->UISettings.GetAlphaBrushRotation() / 180.f;

					FLOAT ModSampleX = appFmod( SampleX * appCos(Angle) - SampleY * appSin(Angle), (FLOAT)SizeX );
					FLOAT ModSampleY = appFmod( SampleY * appCos(Angle) + SampleX * appSin(Angle), (FLOAT)SizeY );

					if( ModSampleX < 0.f )
					{
						ModSampleX += (FLOAT)SizeX;
					}
					if( ModSampleY < 0.f )
					{
						ModSampleY += (FLOAT)SizeY;
					}

					// Sample the alpha texture
					FLOAT Alpha = GetAlphaSample(ModSampleX, ModSampleY);

					// Distance from mouse
					FLOAT MouseDist = appSqrt(Square(LastMousePosition.X-(FLOAT)X) + Square(LastMousePosition.Y-(FLOAT)Y));

					FLOAT PaintAmount = CalculateFalloff(MouseDist, Radius, Falloff) * Alpha;

					if( PaintAmount > 0.f )
					{
						if (EdMode->CurrentToolSet && EdMode->CurrentToolSet->GetTool() && EdMode->CurrentToolSet->GetTool()->GetToolType() != FLandscapeTool::TT_Mask 
							&& EdMode->UISettings.GetbUseSelectedRegion())
						{
							FLOAT MaskValue = LandscapeInfo->SelectedRegion.FindRef(VertexKey);
							if (EdMode->UISettings.GetbUseNegativeMask())
							{
								MaskValue = 1.f - MaskValue;
							}
							PaintAmount *= MaskValue;
						}
						// Set the brush value for this vertex
						OutBrush.Set(VertexKey, PaintAmount);
					}
				}
			}
			return (X1 <= X2 && Y1 <= Y2);
		}
		return FALSE;
	}

	void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
	{
		FLandscapeBrushCircle::Tick(ViewportClient,DeltaTime);

		ALandscapeProxy* Proxy = EdMode->CurrentToolTarget.LandscapeInfo ? EdMode->CurrentToolTarget.LandscapeInfo->LandscapeProxy : NULL;
		if( Proxy )
		{
			INT SizeX = EdMode->UISettings.GetAlphaTextureSizeX();
			INT SizeY = EdMode->UISettings.GetAlphaTextureSizeY();

			FLinearColor AlphaScaleBias(
				1.f / (EdMode->UISettings.GetAlphaBrushScale() * SizeX),
				1.f / (EdMode->UISettings.GetAlphaBrushScale() * SizeY),
				EdMode->UISettings.GetAlphaBrushPanU(),
				EdMode->UISettings.GetAlphaBrushPanV()
				);
			BrushMaterial->SetVectorParameterValue(FName(TEXT("AlphaScaleBias")), AlphaScaleBias);

			FLOAT Angle = PI * EdMode->UISettings.GetAlphaBrushRotation() / 180.f;
			FLinearColor LandscapeLocation(Proxy->Location.X,Proxy->Location.Y,Proxy->Location.Z,Angle);
			BrushMaterial->SetVectorParameterValue(FName(TEXT("LandscapeLocation")), LandscapeLocation);

			INT Channel = EdMode->UISettings.GetAlphaTextureChannel();
			FLinearColor AlphaTextureMask(Channel==0?1:0,Channel==1?1:0,Channel==2?1:0,Channel==3?1:0);
			BrushMaterial->SetVectorParameterValue(FName(TEXT("AlphaTextureMask")), AlphaTextureMask);
			BrushMaterial->SetTextureParameterValue(FName(TEXT("AlphaTexture")), EdMode->UISettings.GetAlphaTexture() );
		}
	}

	const TCHAR* GetIconString() { return TEXT("Pattern"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Brush_PatternAlpha"); };
};


//
// FLandscapeBrushAlpha
//
class FLandscapeBrushAlpha : public FLandscapeBrushAlphaBase
{
	FLOAT LastMouseAngle;
	FVector2D OldMousePosition;	// a previous mouse position, kept until we move a certain distance away, for smoothing deltas
	DOUBLE LastMouseSampleTime;
public:
	FLandscapeBrushAlpha(class FEdModeLandscape* InEdMode)
		:	FLandscapeBrushAlphaBase(InEdMode)
		,	OldMousePosition(0.f,0.f)
		,	LastMouseAngle(0.f)
		,	LastMouseSampleTime(appSeconds())
	{
		UMaterialInstanceConstant* AlphaBrushMaterial = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.AlphaBrushMaterial_Smooth"), NULL, LOAD_None, NULL);
		BrushMaterial->SetParent(AlphaBrushMaterial);
	}

	UBOOL ApplyBrush( TMap<QWORD, FLOAT>& OutBrush, INT& X1, INT& Y1, INT& X2, INT& Y2 )
	{
		ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo;
		ALandscapeProxy* Proxy = LandscapeInfo ? LandscapeInfo->LandscapeProxy : NULL;
		if (Proxy)
		{
			if( OldMousePosition.IsZero() )
			{
				X1 = appFloor(LastMousePosition.X);
				Y1 = appFloor(LastMousePosition.Y);
				X2 = appCeil(LastMousePosition.X);
				Y2 = appCeil(LastMousePosition.Y);
				OldMousePosition = LastMousePosition;
				LastMouseAngle = 0.f;
				LastMouseSampleTime = appSeconds();
			}
			else
			{
				FLOAT ScaleXY = Proxy->DrawScale3D.X * Proxy->DrawScale;
				FLOAT Radius = EdMode->UISettings.GetBrushRadius() / ScaleXY;
				INT SizeX = EdMode->UISettings.GetAlphaTextureSizeX();
				INT SizeY = EdMode->UISettings.GetAlphaTextureSizeY();
				FLOAT MaxSize = 2.f * appSqrt( Square(Radius) / 2.f );
				FLOAT AlphaBrushScale = MaxSize / (FLOAT)Max<INT>(SizeX,SizeY);

				X1 = appFloor(LastMousePosition.X - Radius);
				Y1 = appFloor(LastMousePosition.Y - Radius);
				X2 = appCeil(LastMousePosition.X + Radius);
				Y2 = appCeil(LastMousePosition.Y + Radius);

				for( INT Y=Y1;Y<=Y2;Y++ )
				{
					for( INT X=X1;X<=X2;X++ )
					{
						// Find alphamap sample location
						FLOAT ScaleSampleX = ((FLOAT)X - LastMousePosition.X) / AlphaBrushScale;
						FLOAT ScaleSampleY = ((FLOAT)Y - LastMousePosition.Y) / AlphaBrushScale;

						// Rotate around center to match angle
						FLOAT SampleX = ScaleSampleX * appCos(LastMouseAngle) - ScaleSampleY * appSin(LastMouseAngle);
						FLOAT SampleY = ScaleSampleY * appCos(LastMouseAngle) + ScaleSampleX * appSin(LastMouseAngle);

						SampleX += (FLOAT)SizeX * 0.5f;
						SampleY += (FLOAT)SizeY * 0.5f;

						if( SampleX >= 0.f && SampleX < (FLOAT)SizeX &&
							SampleY >= 0.f && SampleY < (FLOAT)SizeY )
						{
							// Sample the alpha texture
							FLOAT Alpha = GetAlphaSample(SampleX, SampleY);

							if( Alpha > 0.f )
							{
								// Set the brush value for this vertex
								QWORD VertexKey = ALandscape::MakeKey(X,Y);

								if (EdMode->CurrentToolSet && EdMode->CurrentToolSet->GetTool() && EdMode->CurrentToolSet->GetTool()->GetToolType() != FLandscapeTool::TT_Mask 
									&& EdMode->UISettings.GetbUseSelectedRegion())
								{
									FLOAT MaskValue = LandscapeInfo->SelectedRegion.FindRef(VertexKey);
									if (EdMode->UISettings.GetbUseNegativeMask())
									{
										MaskValue = 1.f - MaskValue;
									}
									Alpha *= MaskValue;
								}

								OutBrush.Set(VertexKey, Alpha);
							}
						}
					}
				}
			}
			return (X1 <= X2 && Y1 <= Y2);
		}
		return FALSE;
	}

	void MouseMove(FLOAT LandscapeX, FLOAT LandscapeY)
	{
		FLandscapeBrushAlphaBase::MouseMove(LandscapeX, LandscapeY);

		// don't do anything with the angle unless we move at least 0.1 units.
		FVector2D MouseDelta = LastMousePosition - OldMousePosition;
		if( MouseDelta.SizeSquared() >= Square(0.5f) )
		{
			DOUBLE SampleTime = appSeconds();
			FLOAT DeltaTime = (FLOAT)(SampleTime - LastMouseSampleTime);
			FVector2D MouseDirection = MouseDelta.SafeNormal();
			FLOAT MouseAngle = Lerp( LastMouseAngle, appAtan2( -MouseDirection.Y, MouseDirection.X ), Min<FLOAT>(10.f * DeltaTime, 1.f) );		// lerp over 100ms
			LastMouseAngle = MouseAngle;
			LastMouseSampleTime = SampleTime;
			OldMousePosition = LastMousePosition;
			// debugf(TEXT("(%f,%f) delta (%f,%f) angle %f"), LandscapeX, LandscapeY, MouseDirection.X, MouseDirection.Y, MouseAngle);
		}
	}

	void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
	{
		FLandscapeBrushCircle::Tick(ViewportClient,DeltaTime);

		ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo;
		ALandscapeProxy* Proxy = LandscapeInfo ? LandscapeInfo->LandscapeProxy : NULL;
		if( Proxy )
		{
			FLOAT ScaleXY = Proxy->DrawScale3D.X * Proxy->DrawScale;
			INT SizeX = EdMode->UISettings.GetAlphaTextureSizeX();
			INT SizeY = EdMode->UISettings.GetAlphaTextureSizeY();
			FLOAT Radius = EdMode->UISettings.GetBrushRadius() / ScaleXY;
			FLOAT MaxSize = 2.f * appSqrt( Square(Radius) / 2.f );
			FLOAT AlphaBrushScale = MaxSize / (FLOAT)Max<INT>(SizeX,SizeY);

			FLinearColor BrushScaleRot(
				1.f / (AlphaBrushScale * SizeX),
				1.f / (AlphaBrushScale * SizeY),
				0.f,
				LastMouseAngle
				);
			BrushMaterial->SetVectorParameterValue(FName(TEXT("BrushScaleRot")), BrushScaleRot);

			INT Channel = EdMode->UISettings.GetAlphaTextureChannel();
			FLinearColor AlphaTextureMask(Channel==0?1:0,Channel==1?1:0,Channel==2?1:0,Channel==3?1:0);
			BrushMaterial->SetVectorParameterValue(FName(TEXT("AlphaTextureMask")), AlphaTextureMask);
			BrushMaterial->SetTextureParameterValue(FName(TEXT("AlphaTexture")), EdMode->UISettings.GetAlphaTexture() );
		}
	}

	const TCHAR* GetIconString() { return TEXT("Alpha"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Brush_Alpha"); };
};


void FEdModeLandscape::InitializeBrushes()
{
	FLandscapeBrushSet* BrushSet; 
	BrushSet = new(LandscapeBrushSets) FLandscapeBrushSet(TEXT("BrushSet_Circle"), *LocalizeUnrealEd("LandscapeMode_Brush_Circle"));
	BrushSet->Brushes.AddItem(new FLandscapeBrushCircle_Smooth(this));
	BrushSet->Brushes.AddItem(new FLandscapeBrushCircle_Linear(this));
	BrushSet->Brushes.AddItem(new FLandscapeBrushCircle_Spherical(this));
	BrushSet->Brushes.AddItem(new FLandscapeBrushCircle_Tip(this));

	BrushSet = new(LandscapeBrushSets) FLandscapeBrushSet(TEXT("BrushSet_Alpha"), *LocalizeUnrealEd("LandscapeMode_Brush_Alpha"));
	BrushSet->Brushes.AddItem(new FLandscapeBrushAlphaPattern(this));
	BrushSet->Brushes.AddItem(new FLandscapeBrushAlpha(this));

	BrushSet = new(LandscapeBrushSets) FLandscapeBrushSet(TEXT("BrushSet_Component"), *LocalizeUnrealEd("LandscapeMode_Brush_Component"));
	BrushSet->Brushes.AddItem(new FLandscapeBrushComponent(this));

	BrushSet = new(LandscapeBrushSets) FLandscapeBrushSet(TEXT("BrushSet_Gizmo"), *LocalizeUnrealEd("LandscapeMode_Brush_Gizmo"));
	GizmoBrush = new FLandscapeBrushGizmo(this);
	BrushSet->Brushes.AddItem(GizmoBrush);
}