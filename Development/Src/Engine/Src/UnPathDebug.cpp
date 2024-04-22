/*=============================================================================
UnPathDebug.cpp: Unreal pathnode placement
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"

#if WITH_EDITOR
/** Clear all path step debug variables */
void FPathStep::Clear()
{
	PathStepCache.Empty();
	PathStepIndex = -1;
	PathStepChild =  0;
}

void FPathStep::AddStep( ANavigationPoint* Nav, APawn* P )
{
	TArray<FPathStep> StepList;
	while( Nav != NULL )
	{
		// Set initial values
		FPathStep Step;
		Step.Pawn	= P;
		Step.Origin	= Nav;
		Step.Start	= Nav->previousPath;

		ANavigationPoint* Prev = Nav->previousPath;
		while( Prev != NULL )
		{
			Step.Trail.AddItem( Prev );
			Prev = Prev->previousPath;
		}

		if( Step.Start != NULL )
		{
			// Setup path ends for each item in path list
			for( INT PathIdx = 0; PathIdx < Step.Start->PathList.Num(); PathIdx++ )
			{
				UReachSpec* Spec = Step.Start->PathList(PathIdx);
				if( Spec == NULL )
					continue;

				ANavigationPoint* E = Spec->End.Nav();
				if( E == NULL || E->previousPath != Step.Start )
					continue;

				if( E->bestPathWeight > 0 )
				{
					Step.MinWeight = ::Min( Step.MinWeight, E->bestPathWeight );
					Step.MaxWeight = ::Max( Step.MaxWeight, E->bestPathWeight );
				}				
				
				FPathEnd EndItem;
				EndItem.End			= E;
				EndItem.Weight		= E->bestPathWeight;
				EndItem.Components	= E->CostArray;
				
				Step.EndList.AddItem( EndItem );
			}
		}
		StepList.AddItem( Step );

		Nav = Nav->nextOrdered;
	}

	if( StepList.Num() )
	{
		PathStepCache.AddItem( StepList );
	}
}

void FPathStep::DrawStep( UINT StepInc, UINT ChildInc, UCanvas* Canvas )
{
	if( PathStepCache.Num() == 0 || 
		(PathStepIndex < 0 && StepInc == 0) )
		return;

	PathStepIndex = (PathStepIndex + StepInc) % PathStepCache.Num();
	if( StepInc != 0 )
	{
		PathStepChild = 0;
	}
	PathStepChild = (PathStepChild + ChildInc) % PathStepCache(PathStepIndex).Num();

	FPathStep& Step = PathStepCache(PathStepIndex)(PathStepChild);

	Step.Pawn->DrawDebugBox( Step.Origin->Location, FVector(10,10,10), 0, 255, 0 );
	for( INT Idx = 0; Idx < Step.EndList.Num(); Idx++ )
	{
		FPathEnd& End = Step.EndList(Idx);

		BYTE GVal = 255;
		BYTE RBVal = appTrunc(((FLOAT)(End.Weight - Step.MinWeight) / (FLOAT)(Step.MaxWeight - Step.MinWeight)) * 196);

		Step.Pawn->DrawDebugLine( Step.Start->Location, End.End->Location, RBVal, GVal, RBVal );

		if( Canvas != NULL && Canvas->SceneView != NULL )
		{
			const INT YL = 10;
			Canvas->SetDrawColor(255,255,255,255);
			FPlane V = Canvas->SceneView->Project(End.End->Location + (FVector(0,0,2*YL)*End.Components.Num()));
			FVector ScreenLoc(V);
			ScreenLoc.X = (Canvas->ClipX/2.f) + (ScreenLoc.X*(Canvas->ClipX/2.f));
			ScreenLoc.Y *= -1.f;
			ScreenLoc.Y = (Canvas->ClipY/2.f) + (ScreenLoc.Y*(Canvas->ClipY/2.f));

			Canvas->SetPos( ScreenLoc.X, ScreenLoc.Y );

			FString RightMost = End.End->GetName();
			INT UnderScore = RightMost.InStr(TEXT("_"), TRUE );
			if( UnderScore >= 0 )
			{
				RightMost = RightMost.Mid( UnderScore );
			}
			FString Text = FString::Printf(TEXT("%s%s = %d"), *End.End->eventGetDebugAbbrev(), *RightMost, End.Weight );
			Canvas->DrawText( Text );

			INT Cnt = 0;
			for (TArray<FDebugNavCost>::TIterator It(End.Components); It; ++It)
			{
				Canvas->SetPos( ScreenLoc.X, ScreenLoc.Y + (YL*++Cnt) );
				Text = FString::Printf(TEXT("%s = %d"), *It->Desc, It->Cost);
				Canvas->DrawText( Text );
			}
		}
	}

	ANavigationPoint* Prev = Step.Origin;
	for( INT PrevIdx = 0; PrevIdx < Step.Trail.Num(); PrevIdx++ )
	{
		Step.Pawn->DrawDebugLine( Prev->Location, Step.Trail(PrevIdx)->Location, 0, 255, 0 );
		Prev = Step.Trail(PrevIdx);
	}
}

void FPathStep::RegisterCost( ANavigationPoint* Nav, const TCHAR* Desc, INT Cost )
{
	if( Cost == 0 )
	{
		for (INT i = 0; i < Nav->CostArray.Num(); i++)
		{
			if (Nav->CostArray(i).Desc == Desc)
			{
				Nav->CostArray.Remove(i);
				break;
			}
		}
	}
	else
	{
		UBOOL bFound = FALSE;
		for (INT i = 0; i < Nav->CostArray.Num(); i++)
		{
			if (Nav->CostArray(i).Desc == Desc)
			{
				Nav->CostArray(i).Cost = Cost;
				bFound = TRUE;
				break;
			}
		}
		if (!bFound)
		{
			FDebugNavCost NewCost(EC_EventParm);
			NewCost.Desc = Desc;
			NewCost.Cost = Cost;
			Nav->CostArray.AddItem(NewCost);
		}
	}
}
#endif
