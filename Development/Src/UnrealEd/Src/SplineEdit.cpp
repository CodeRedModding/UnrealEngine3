/*=============================================================================
	SplineEdit.cpp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineSplineClasses.h"
#include "EngineSoundClasses.h"
#include "ScopedTransaction.h"
#include "SplineEdit.h"

/** Util to get the forst two selected SplineActors */
static void GetFirstTwoSelectedSplineActors(ASplineActor* &SplineA, ASplineActor* &SplineB)
{
	SplineA = NULL;
	SplineB = NULL;
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		ASplineActor* Spline = Cast<ASplineActor>( *It );
		if(Spline)
		{
			if(!SplineA)
			{
				SplineA = Spline;
			}
			else if(!SplineB)
			{
				SplineB = Spline;
				break; // have our 2 splines, exit now
			}
		}
	}
}

/** Util to break a spline, given the hit proxy that was clicked on */
void SplineBreakGivenProxy(HSplineProxy* SplineProxy)
{
	USplineComponent* SplineComp = SplineProxy->SplineComp;

	// Find the SplineActor that owns this component, and the SplineActor it connects to.
	ASplineActor* SplineActor = Cast<ASplineActor>(SplineComp->GetOwner());
	ASplineActor* TargetActor = NULL;
	if(SplineActor)
	{
		TargetActor = SplineActor->FindTargetForComponent(SplineComp);
	}

	// If we have a source and target, and holding alt, break connection
	if(SplineActor && TargetActor)
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd("Break Spline") );

		SplineActor->Modify();
		SplineActor->BreakConnectionTo(TargetActor);
	}
}

/*------------------------------------------------------------------------------
FEdModeSpline
------------------------------------------------------------------------------*/

FEdModeSpline::FEdModeSpline() : FEdMode()
{
	ID = EM_Spline;
	Desc = TEXT("Spline Editing");

	ModSplineActor = NULL;
	bModArrive = FALSE;

	TangentHandleScale = 0.5f;
}

FEdModeSpline::~FEdModeSpline()
{

}

void FEdModeSpline::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);

	UBOOL bHitTesting = PDI->IsHitTesting();
	for ( FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It )
	{
		ASplineActor *Spline = Cast<ASplineActor>(*It);
		if (Spline != NULL && Spline->IsSelected())
		{
			// Handles
			FVector LeaveHandlePos = Spline->Location + (Spline->GetWorldSpaceTangent() * TangentHandleScale);
			FVector ArriveHandlePos = Spline->Location - (Spline->GetWorldSpaceTangent() * TangentHandleScale);

			// Line
			PDI->DrawLine(LeaveHandlePos, ArriveHandlePos, FColor(255,255,255), SDPG_Foreground);

			// Leave handle
			if(bHitTesting) PDI->SetHitProxy( new HSplineHandleProxy(Spline, FALSE) );
			PDI->DrawPoint(LeaveHandlePos, FColor(255,255,255), 5.f, SDPG_Foreground);
			if(bHitTesting) PDI->SetHitProxy( NULL );

			// Arrive handle		
			if(bHitTesting) PDI->SetHitProxy( new HSplineHandleProxy(Spline, TRUE) );
			PDI->DrawPoint(ArriveHandlePos, FColor(255,255,255), 5.f, SDPG_Foreground);
			if(bHitTesting) PDI->SetHitProxy( NULL );

						
			// Let the actor draw extra info when selected
			Spline->EditModeSelectedDraw(View, Viewport, PDI);
		}
	}
}



UBOOL FEdModeSpline::InputKey(FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, FName Key, EInputEvent Event)
{
	const UBOOL bAltDown = Viewport->KeyState( KEY_LeftAlt ) || Viewport->KeyState( KEY_RightAlt );

	if(Key == KEY_LeftMouseButton)
	{
		// Press mouse button
		if(Event == IE_Pressed)
		{		
			// See if we clicked on a spline handle..
			INT HitX = ViewportClient->Viewport->GetMouseX();
			INT HitY = ViewportClient->Viewport->GetMouseY();
			HHitProxy*	HitProxy = ViewportClient->Viewport->GetHitProxy(HitX, HitY);
			if(HitProxy)
			{			
				// If left clicking on a spline handle proxy
				if( HitProxy->IsA(HSplineHandleProxy::StaticGetType()) )
				{
					// save info about what we grabbed
					HSplineHandleProxy* SplineHandleProxy = (HSplineHandleProxy*)HitProxy;
					ModSplineActor = SplineHandleProxy->SplineActor;
					bModArrive = SplineHandleProxy->bArrive;
				}		
			}
		}
		// Release mouse button
		else if(Event == IE_Released)
		{
			// End any handle-tweaking
			if(ModSplineActor)
			{
				ModSplineActor->UpdateConnectedSplineComponents(TRUE);			
				ModSplineActor = NULL;
			}
	
			bModArrive = FALSE;
		}	
	}
	else if(Key == KEY_Period)
	{
		if(Event == IE_Pressed)
		{
			// Check we have two SplineActor selected 
			ASplineActor* SplineA = NULL;
			ASplineActor* SplineB = NULL;
			GetFirstTwoSelectedSplineActors(SplineA, SplineB);
			if(SplineA && SplineB)
			{
				GEditor->SplineConnect();
				ViewportClient->Invalidate();
				return TRUE;			
			}
		}
	}

	return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}

UBOOL FEdModeSpline::InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	if(ModSplineActor)
	{
		FVector InputDeltaDrag( InDrag );

		// scale to compensate for handle length
		InputDeltaDrag /= TangentHandleScale;


		// We seem to need this - not sure why...
		if ( InViewportClient->ViewportType == LVT_OrthoXY )
		{
			InputDeltaDrag.X = InDrag.Y;
			InputDeltaDrag.Y = -InDrag.X;
		}
		// Make it a bit more sensitive in perspective
		else if(InViewportClient->ViewportType == LVT_Perspective)
		{
			InputDeltaDrag *= 2.f;
		}

		//if we're using inverted panning
		if ((InViewportClient->ViewportType != LVT_Perspective) && InViewportClient->ShouldUseMoveCanvasMovement())
		{
			InputDeltaDrag = -InputDeltaDrag;
		}

		// Invert movement if grabbing arrive handle
		if(bModArrive)
		{
			InputDeltaDrag *= -1.f;
		}

		// Update thie actors tangent
		FVector LocalDeltaDrag = ModSplineActor->LocalToWorld().InverseTransformNormal(InputDeltaDrag);
		ModSplineActor->SplineActorTangent += LocalDeltaDrag;

		// And propagate to spline component
		ModSplineActor->UpdateConnectedSplineComponents(FALSE);

		return TRUE;
	}

	return FEdMode::InputDelta(InViewportClient, InViewport, InDrag, InRot, InScale);
}

/** Stop widget moving when adjusting handle */
UBOOL FEdModeSpline::AllowWidgetMove()
{
	if(ModSplineActor)
	{
		return FALSE;
	}
	else
	{
		return TRUE;	
	}
}

/** Called when actors duplicated, to link them into current spline  */
void FEdModeSpline::ActorsDuplicatedNotify(TArray<AActor*>& PreDuplicateSelection, TArray<AActor*>& PostDuplicateSelection, UBOOL bOffsetLocations)
{
	// If offsetting locations, this isn't an alt-drag - do nothing
	if(bOffsetLocations)
	{
		return;
	}

	INT NumActors = Min(PreDuplicateSelection.Num(), PostDuplicateSelection.Num());
	for(INT i=0; i<NumActors; i++)
	{
		ASplineActor* SourceSA = Cast<ASplineActor>(PreDuplicateSelection(i));
		ASplineActor* DestSA = Cast<ASplineActor>(PostDuplicateSelection(i));
		// Only allow connection between SplineActors in the same level
		if(SourceSA && DestSA && (SourceSA->GetLevel() == DestSA->GetLevel()))
		{
			// Clear all pointers from source node
			SourceSA->BreakAllConnectionsFrom();

			// Clear all connections on newly created node
			DestSA->BreakAllConnections();
			
			// Connect source point to destination			
			SourceSA->AddConnectionTo(DestSA);
			
			// Special case for loft actors - grab mesh from previous point and assign
			ASplineLoftActor* SourceSpline = Cast<ASplineLoftActor>(SourceSA);
			if(SourceSpline && (SourceSpline->LinksFrom.Num() > 0))
			{
				ASplineLoftActor* PrevSpline = Cast<ASplineLoftActor>(SourceSpline->LinksFrom(0));
				if(PrevSpline)
				{
					SourceSpline->DeformMesh = PrevSpline->DeformMesh;
				}
			}
			
		}	
	}
}

//////////////////////////////////////////////////////////////////////////
// UEditorEngine functions



/** Break all connections to all selected SplineActors */
void UEditorEngine::SplineBreakAll()
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd("SplineBreakAll") );

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		ASplineActor* Spline = Cast<ASplineActor>( *It );
		if(Spline)
		{
			Spline->BreakAllConnections(); // 
		}
	}
}

/** Create connection between first 2 selected SplineActors (or flip connection if already connected) */
void UEditorEngine::SplineConnect()
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd("SplineConnect") );

	ASplineActor* SplineA = NULL;
	ASplineActor* SplineB = NULL;
	GetFirstTwoSelectedSplineActors(SplineA, SplineB);

	if(SplineA && SplineB)
	{
		// If they are connected, flip connection
		if(SplineA->IsConnectedTo(SplineB,FALSE))
		{
			SplineA->BreakConnectionTo(SplineB);
			SplineB->AddConnectionTo(SplineA);
		}
		else if(SplineB->IsConnectedTo(SplineA,FALSE))
		{
			SplineB->BreakConnectionTo(SplineA);
			SplineA->AddConnectionTo(SplineB);
		}
		// If they are not, connect them
		else
		{
			SplineA->AddConnectionTo(SplineB);
		}	
	}
}

/** Break any connections between selected SplineActors */
void UEditorEngine::SplineBreak()
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd("SplineBreak") );

	// TODO: Handle more than 2!
	ASplineActor* SplineA = NULL;
	ASplineActor* SplineB = NULL;
	GetFirstTwoSelectedSplineActors(SplineA, SplineB);

	if(SplineA && SplineB)
	{
		// If they are connected, break
		if(SplineA->IsConnectedTo(SplineB,FALSE))
		{
			SplineA->BreakConnectionTo(SplineB);
		}
		else if(SplineB->IsConnectedTo(SplineA,FALSE))
		{
			SplineB->BreakConnectionTo(SplineA);
		}
	}
}

/** Util that reverses direction of all splines between selected SplineActors */
void UEditorEngine::SplineReverseAllDirections()
{
	const FScopedTransaction Transaction( *LocalizeUnrealEd("SplineReverseAllDirections") );


	// Struct for storing one connection
	struct FSplineConnection
	{
		ASplineActor* FromSpline;
		ASplineActor* ToSpline;
	};

	// First create array of all selected splines
	TArray<ASplineActor*> SelectedSplineActors;
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		ASplineActor* Spline = Cast<ASplineActor>( *It );
		if(Spline)
		{
			Spline->Modify();
			SelectedSplineActors.AddItem(Spline);
		}
	}
	
	// Now find all connections and save them off
	TArray<FSplineConnection> CurrentConnections;
	// For each selected spline..
	for (INT i=0; i<SelectedSplineActors.Num(); i++)
	{
		ASplineActor* ThisSpline = SelectedSplineActors(i);
		// ..for each connection
		for(INT ConnIdx=0; ConnIdx<ThisSpline->Connections.Num(); ConnIdx++)
		{
			ASplineActor* ConnSpline = ThisSpline->Connections(ConnIdx).ConnectTo;
			// If it connects to something else in the selection set, remember it.
			if(ConnSpline && SelectedSplineActors.ContainsItem(ConnSpline))
			{
				INT NewIdx = CurrentConnections.AddZeroed();
				CurrentConnections(NewIdx).FromSpline = ThisSpline;
				CurrentConnections(NewIdx).ToSpline = ConnSpline;				
			}
		}		
	}
	
	// Flip all tangent dirs
	for(INT i=0; i<SelectedSplineActors.Num(); i++)
	{
		SelectedSplineActors(i)->SplineActorTangent = -1.f * SelectedSplineActors(i)->SplineActorTangent;
	}
	
	// Then for all connections, break the current one and reverse
	for(INT i=0; i<CurrentConnections.Num(); i++)
	{
		CurrentConnections(i).FromSpline->BreakConnectionTo(CurrentConnections(i).ToSpline);
		CurrentConnections(i).ToSpline->AddConnectionTo(CurrentConnections(i).FromSpline);
	}	
}

/** Util to test a route from one selected spline node to another */
void UEditorEngine::SplineTestRoute()
{
	ASplineActor* SplineA = NULL;
	ASplineActor* SplineB = NULL;
	GetFirstTwoSelectedSplineActors(SplineA, SplineB);
	
	if(SplineA && SplineB)
	{
		TArray<ASplineActor*> Route;
		UBOOL bSuccess = SplineA->FindSplinePathTo(SplineB, Route);
		if(bSuccess)
		{
			// Worked, so draw route!
			GWorld->PersistentLineBatcher->BatchedLines.Empty();
			
			for(INT SplineIdx=1; SplineIdx<Route.Num(); SplineIdx++)
			{
				check( Route(SplineIdx-1) );
				check( Route(SplineIdx) );
				GWorld->PersistentLineBatcher->DrawLine( Route(SplineIdx)->Location, Route(SplineIdx-1)->Location, FColor(10,200,30), SDPG_World );
			}
		}
		else
		{
			appMsgf(AMT_OK, *LocalizeUnrealEd("SplineTestRouteFailed"));
		}
	}
}

/** Set tangents on the selected two points to be straight and even. */
void UEditorEngine::SplineStraightTangents()
{
	ASplineActor* SplineStart = NULL;
	ASplineActor* SplineEnd = NULL;
	GetFirstTwoSelectedSplineActors(SplineStart, SplineEnd);
	
	if(SplineStart && SplineEnd)
	{
		// If wrong way around, swap
		if(SplineEnd->IsConnectedTo(SplineStart,FALSE))
		{
			Swap(SplineStart, SplineEnd);
		}
		// If not connected at all, do nothing and bail
		else if(!SplineStart->IsConnectedTo(SplineEnd,FALSE))
		{
			appMsgf(AMT_OK, TEXT("Spline actors are not connected."));
			return;
		}
		
		const FScopedTransaction Transaction( *LocalizeUnrealEd("SplineStraightTangents") );
		
		// At this point SplineStart connects to SplineEnd
		
		// Direct vector between them becomes the tangent for each end
		FVector Delta = SplineEnd->Location - SplineStart->Location;
		
		// Transform into local space and assign to each actor
		
		SplineStart->Modify();
		SplineStart->SplineActorTangent = FRotationMatrix(SplineStart->Rotation).InverseTransformNormal( Delta );		
		SplineStart->UpdateConnectedSplineComponents(TRUE);
		
		SplineEnd->Modify();
		SplineEnd->SplineActorTangent = FRotationMatrix(SplineEnd->Rotation).InverseTransformNormal( Delta );		
		SplineEnd->UpdateConnectedSplineComponents(TRUE);
	}
}

/** Select all nodes on the same splines as selected set */
void UEditorEngine::SplineSelectAllNodes()
{
	// Array of all spline actors connected to selected set
	TArray<ASplineActor*> AllConnectedActors;
	
	// Iterate over all selected actors
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		ASplineActor* Spline = Cast<ASplineActor>( *It );
		if(Spline)
		{
			TArray<ASplineActor*> Connected;
			Spline->GetAllConnectedSplineActors(Connected);
			
			for(INT i=0; i<Connected.Num(); i++)
			{
				if(Connected(i))
				{
					AllConnectedActors.AddUniqueItem(Connected(i));				
				}
			}			
		}
	}
	
	// Now change selection to what we found
	
	SelectNone( FALSE, TRUE );

	for(INT ActorIdx=0; ActorIdx<AllConnectedActors.Num(); ActorIdx++)
	{
		SelectActor( AllConnectedActors(ActorIdx), TRUE, NULL, FALSE );
	}
	
	NoteSelectionChange();
}

/**
  HandleProxy for spline curve editing in AmbientSoundSpline actor. Used by FEdModeAmbientSoundSpline.
*/
struct HAmbientSoundSplineHandleProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HAmbientSoundSplineHandleProxy,HHitProxy);

	/** Owner of editing spline curve */
	AAmbientSoundSpline* SplineActor;
	/** Is selected control point for 'position' or for 'tangent' */
	UBOOL bPointNotTangent;
	/** Is selected tangent control point for arriving, or leaving tangent */
	UBOOL bArriveTangent;
	/** index of selected knot in spline */
	INT PointIndex;

	HAmbientSoundSplineHandleProxy(AAmbientSoundSpline* InSplineActor, INT InPointIndex, UBOOL bInPointNotTangent, UBOOL bInArriveTangent = FALSE): 
		HHitProxy(HPP_UI), 
		SplineActor(InSplineActor),
		PointIndex(InPointIndex),
		bPointNotTangent(bInPointNotTangent),
		bArriveTangent(bInArriveTangent)
	{}

	/** Show cursor as cross when over this handle */
	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Arrow;
	}

	virtual void Serialize(FArchive& Ar)
	{
		Ar << SplineActor;
		Ar << PointIndex;
		Ar << bPointNotTangent;
		Ar << bArriveTangent;
	}
};

/**
  HandleProxy for test point moving in AmbientSoundSpline actor. Used by FEdModeAmbientSoundSpline.
  Test point is used only in editor for testing find-nearest-point-on-spline algorithm.
*/
struct HAmbientSoundSplineTestPointHandleProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HAmbientSoundSplineTestPointHandleProxy,HHitProxy);

	/** Owner of editing test point */
	AAmbientSoundSpline* SplineActor;

	HAmbientSoundSplineTestPointHandleProxy(AAmbientSoundSpline* InSplineActor): 
		HHitProxy(HPP_UI), SplineActor(InSplineActor)
	{}

	/** Show cursor as cross when over this handle */
	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Arrow;
	}

	virtual void Serialize(FArchive& Ar)
	{
		Ar << SplineActor;
	}
};

struct HSplineSimpleComponentSlotPointHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HSplineSimpleComponentSlotPointHitProxy,HHitProxy);

	/** Owner of editing test point */
	AAmbientSoundSpline* SplineActor;
	INT SlotIndex;
	ESoundRangeOnSpline RangePoint;
	INT RangePointIndex;

	HSplineSimpleComponentSlotPointHitProxy(AAmbientSoundSpline* InSplineActor, INT InSlotIndex, ESoundRangeOnSpline InRangePoint, INT InRangePointIndex): 
		HHitProxy(HPP_UI), SplineActor(InSplineActor), SlotIndex(InSlotIndex), RangePoint(InRangePoint), RangePointIndex(InRangePointIndex)
	{}

	/** Show cursor as cross when over this handle */
	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Arrow;
	}

	virtual void Serialize(FArchive& Ar)
	{
		Ar << SplineActor;
		// Ar << SlotIndex;
		// Ar << RangePoint;
	}
};

void FEdModeAmbientSoundSpline::Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);

	const FColor OutsideColor(255,0,0);
	const FColor InsideColor(200,200,255);
	const FLOAT SmallSize = 5.0f;
	const FLOAT NormalSize = 7.0f;
	const FLOAT BiggerSize = 11.0f;

	UBOOL bHitTesting = PDI->IsHitTesting();
	for ( FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It )
	{
		AAmbientSoundSpline * SplineActor = Cast<AAmbientSoundSpline>(*It);
		if (NULL == SplineActor || !SplineActor->IsSelected() || NULL == SplineActor->SplineComponent)
		{
			continue;
		}
		const FInterpCurve<FVector>& CurvePoints = SplineActor->SplineComponent->SplineInfo;
		for(INT i = 0; i < CurvePoints.Points.Num(); ++i)
		{
			const FInterpCurvePoint<FVector>& CurvePoint = CurvePoints.Points(i);
			const FVector PointPos = CurvePoint.OutVal;

			if(bHitTesting) PDI->SetHitProxy( new HAmbientSoundSplineHandleProxy(SplineActor, i, TRUE ) );
			if(SplineActor == ModSplineActor && SSS_Point == State && PointIndex == i )
			{
				PDI->DrawPoint(PointPos, FColor(250,200,10), BiggerSize+3.0f, SDPG_Foreground);
			}
			else
			{
				PDI->DrawPoint(PointPos, FColor(255,255,255), BiggerSize, SDPG_Foreground);

			}
			if(bHitTesting) PDI->SetHitProxy( NULL );

			if(CIM_Linear != CurvePoint.InterpMode && CIM_CurveAuto != CurvePoint.InterpMode )
			{
				const FVector LeaveHandlePos = PointPos + SplineActor->LocalToWorld().TransformNormal(CurvePoint.LeaveTangent);
				const FVector ArriveHandlePos = PointPos - SplineActor->LocalToWorld().TransformNormal(CurvePoint.ArriveTangent);

				if(i < CurvePoints.Points.Num()-1)
				{
					PDI->DrawLine(LeaveHandlePos, PointPos, FColor(255,255,255), SDPG_Foreground);
					if(bHitTesting) PDI->SetHitProxy( new HAmbientSoundSplineHandleProxy(SplineActor, i, FALSE, FALSE ) );
					if(SplineActor == ModSplineActor && SSS_LeaveTangent == State && PointIndex == i )
					{
						PDI->DrawPoint(LeaveHandlePos, FColor(240,240,255), BiggerSize, SDPG_Foreground);
					}
					else
					{
						PDI->DrawPoint(LeaveHandlePos, FColor(255,255,255), SmallSize, SDPG_Foreground);
					}
					if(bHitTesting) PDI->SetHitProxy( NULL );
				}
				if(i > 0)
				{
					PDI->DrawLine(PointPos, ArriveHandlePos, FColor(255,255,255), SDPG_Foreground);
					if(bHitTesting) PDI->SetHitProxy( new HAmbientSoundSplineHandleProxy(SplineActor, i, FALSE, TRUE ) );
					if(SplineActor == ModSplineActor && SSS_LeaveTangent == State && PointIndex == i )
					{
						PDI->DrawPoint(ArriveHandlePos, FColor(240,240,255), BiggerSize, SDPG_Foreground);
					}
					else
					{
						PDI->DrawPoint(ArriveHandlePos, FColor(255,255,255), SmallSize, SDPG_Foreground);
					}
					if(bHitTesting) PDI->SetHitProxy( NULL );
				}
			}
		}

		USplineAudioComponent * SplineAudioComponent = Cast<USplineAudioComponent>(SplineActor->AudioComponent);
		if(NULL != SplineAudioComponent)
		{
			const TArray< FInterpCurveVector::FPointOnSpline >& Points = SplineAudioComponent->Points;
			const FLOAT RadiusSq = SplineAudioComponent->ListenerScopeRadius * SplineAudioComponent->ListenerScopeRadius;
			const FColor InScopeColor(50, 255, 50);

			USimpleSplineAudioComponent * SimpleSplineAudioComponent = Cast<USimpleSplineAudioComponent>(SplineAudioComponent);
			AAmbientSoundSimpleSpline * AmbientSoundSimpleSpline = Cast<AAmbientSoundSimpleSpline>(SplineActor);

			UMultiCueSplineAudioComponent * MultiCueSplineAudioComponent = Cast<UMultiCueSplineAudioComponent>(SplineAudioComponent);
			AAmbientSoundSplineMultiCue * AmbientSoundSplineMultiCue = Cast<AAmbientSoundSplineMultiCue>(SplineActor);

			if(NULL == SimpleSplineAudioComponent && NULL == MultiCueSplineAudioComponent)
			{
				const FVector TestResult = USplineAudioComponent::FindVirtualSpeakerPosition(Points, SplineActor->TestPoint, SplineAudioComponent->ListenerScopeRadius);
				PDI->DrawPoint(TestResult, FColor(0, 230, 255), NormalSize, SDPG_Foreground);
				for(INT i = 0; i < Points.Num() && Points.Num() > 1; ++i)
				{
					const FLinearColor CurrentColor = (SplineActor->TestPoint.DistanceSquared(Points(i).Position) > RadiusSq) ? InsideColor : InScopeColor;
					PDI->DrawPoint(Points(i).Position, CurrentColor, NormalSize, SDPG_Foreground);
				}
			}
#if WITH_EDITORONLY_DATA
			else if(NULL != MultiCueSplineAudioComponent && NULL != AmbientSoundSplineMultiCue && MultiCueSplineAudioComponent->SoundSlots.Num() > 0)
			{
				const INT SlotIndex = AmbientSoundSplineMultiCue->EditedSlot;
				if(!(SlotIndex >= 0 && SlotIndex <= MultiCueSplineAudioComponent->SoundSlots.Num() )) continue;
				const FMultiCueSplineSoundSlot& SoundSlot = MultiCueSplineAudioComponent->SoundSlots(SlotIndex);

				for(INT i = 0; i < Points.Num() && Points.Num() > 1; ++i)
				{
					const FLinearColor CurrentColor = (SplineActor->TestPoint.DistanceSquared(Points(i).Position) > RadiusSq) ? InsideColor : InScopeColor;

					if(i < SoundSlot.StartPoint)
					{
						PDI->DrawPoint(Points(i).Position, OutsideColor, NormalSize, SDPG_Foreground);
					}
					else if( i == SoundSlot.StartPoint )
					{
						if(bHitTesting) PDI->SetHitProxy( new HSplineSimpleComponentSlotPointHitProxy(SplineActor, SlotIndex, SRS_Start, i));
						PDI->DrawPoint(Points(i).Position, CurrentColor, BiggerSize, SDPG_Foreground);
						if(bHitTesting) PDI->SetHitProxy( NULL );
					}
					else if(i < SoundSlot.EndPoint || 0 > SoundSlot.EndPoint)
					{
						PDI->DrawPoint(Points(i).Position, CurrentColor, NormalSize, SDPG_Foreground);
					}
					else if( i == SoundSlot.EndPoint )
					{
						if(bHitTesting) PDI->SetHitProxy( new HSplineSimpleComponentSlotPointHitProxy(SplineActor, SlotIndex, SRS_Stop, i));
						PDI->DrawPoint(Points(i).Position, CurrentColor, BiggerSize, SDPG_Foreground);
						if(bHitTesting) PDI->SetHitProxy( NULL );
					}
					else if(i > SoundSlot.EndPoint)
					{
						PDI->DrawPoint(Points(i).Position, OutsideColor, NormalSize, SDPG_Foreground);
					}
				}

				FLOAT DummyDistance;
				INT DummyClosestPointOnSpline;
				const FVector TestResult = UMultiCueSplineAudioComponent::FindVirtualSpeakerScaledPosition(Points, SplineActor->TestPoint, SplineAudioComponent->ListenerScopeRadius, SoundSlot, DummyDistance, DummyClosestPointOnSpline );
				PDI->DrawPoint(TestResult, FColor(0, 230, 255), 9.0f, SDPG_Foreground);
			}
			else if(NULL != SimpleSplineAudioComponent && NULL != AmbientSoundSimpleSpline && SimpleSplineAudioComponent->SoundSlots.Num() > 0)
			{
				const INT SlotIndex = AmbientSoundSimpleSpline->EditedSlot;
				if(!(SlotIndex >= 0 && SlotIndex <= SimpleSplineAudioComponent->SoundSlots.Num() )) continue;
				const FSplineSoundSlot& SoundSlot = SimpleSplineAudioComponent->SoundSlots(SlotIndex);

				for(INT i = 0; i < Points.Num() && Points.Num() > 1; ++i)
				{
					const FLinearColor CurrentColor = (SplineActor->TestPoint.DistanceSquared(Points(i).Position) > RadiusSq) ? InsideColor : InScopeColor;

					if(i < SoundSlot.StartPoint)
					{
						PDI->DrawPoint(Points(i).Position, OutsideColor, NormalSize, SDPG_Foreground);
					}
					else if( i == SoundSlot.StartPoint )
					{
						if(bHitTesting) PDI->SetHitProxy( new HSplineSimpleComponentSlotPointHitProxy(SplineActor, SlotIndex, SRS_Start, i));
						PDI->DrawPoint(Points(i).Position, CurrentColor, BiggerSize, SDPG_Foreground);
						if(bHitTesting) PDI->SetHitProxy( NULL );
					}
					else if(i < SoundSlot.EndPoint || 0 > SoundSlot.EndPoint)
					{
						PDI->DrawPoint(Points(i).Position, CurrentColor, NormalSize, SDPG_Foreground);
					}
					else if( i == SoundSlot.EndPoint )
					{
						if(bHitTesting) PDI->SetHitProxy( new HSplineSimpleComponentSlotPointHitProxy(SplineActor, SlotIndex, SRS_Stop, i));
						PDI->DrawPoint(Points(i).Position, CurrentColor, BiggerSize, SDPG_Foreground);
						if(bHitTesting) PDI->SetHitProxy( NULL );
					}
					else if(i > SoundSlot.EndPoint)
					{
						PDI->DrawPoint(Points(i).Position, OutsideColor, NormalSize, SDPG_Foreground);
					}
				}

				FLOAT DummyDistance;
				INT DummyClosestPointOnSpline;
				const FVector TestResult = USimpleSplineAudioComponent::FindVirtualSpeakerScaledPosition(Points, SplineActor->TestPoint, SplineAudioComponent->ListenerScopeRadius, SoundSlot, DummyDistance, DummyClosestPointOnSpline );
				PDI->DrawPoint(TestResult, FColor(0, 230, 255), 9.0f, SDPG_Foreground);
			}
			if(bHitTesting) PDI->SetHitProxy( new HAmbientSoundSplineTestPointHandleProxy(SplineActor) );
			PDI->DrawPoint(SplineActor->TestPoint, OutsideColor, BiggerSize, SDPG_Foreground);
			if(bHitTesting) PDI->SetHitProxy( NULL );
			DrawWireSphere(PDI,SplineActor->TestPoint, OutsideColor, SplineAudioComponent->ListenerScopeRadius, 64, SDPG_World );
#endif
		}
	}
}

UBOOL FEdModeAmbientSoundSpline::InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event)
{
	const UBOOL bAltDown = Viewport->KeyState( KEY_LeftAlt ) || Viewport->KeyState( KEY_RightAlt );

	if(Key == KEY_LeftMouseButton)
	{
		// Press mouse button
		if(Event == IE_Pressed)
		{		
			// See if we clicked on a spline handle..
			INT HitX = ViewportClient->Viewport->GetMouseX();
			INT HitY = ViewportClient->Viewport->GetMouseY();
			HHitProxy*	HitProxy = ViewportClient->Viewport->GetHitProxy(HitX, HitY);
			if(HitProxy)
			{			
				// If left clicking on a spline handle proxy
				if( HitProxy->IsA(HAmbientSoundSplineHandleProxy::StaticGetType()) )
				{
					// save info about what we grabbed
					HAmbientSoundSplineHandleProxy* SplineHandleProxy = (HAmbientSoundSplineHandleProxy*)HitProxy;
					ModSplineActor = SplineHandleProxy->SplineActor;
					PointIndex = SplineHandleProxy->PointIndex;
					State = SplineHandleProxy->bPointNotTangent ? SSS_Point : (SplineHandleProxy->bArriveTangent ? SSS_ArriveTangent : SSS_LeaveTangent);
					if( SSS_Point != State && NULL != ModSplineActor &&  NULL != ModSplineActor->SplineComponent && ModSplineActor->SplineComponent->SplineInfo.Points.Num() > PointIndex)
					{
						// If tangent control point is moved - curve mode will be changed automatically
						const BYTE InterpMode = ModSplineActor->SplineComponent->SplineInfo.Points(PointIndex).InterpMode;
						if(CIM_CurveUser != InterpMode && CIM_CurveBreak != InterpMode)
						{
							ModSplineActor->SplineComponent->SplineInfo.Points(PointIndex).InterpMode = CIM_CurveUser;
							ModSplineActor->UpdateSpline();	
						}
					}
				}
				else if( HitProxy->IsA(HAmbientSoundSplineTestPointHandleProxy::StaticGetType()) )
				{
					HAmbientSoundSplineTestPointHandleProxy * SplineHandleProxy = (HAmbientSoundSplineTestPointHandleProxy*)HitProxy;
					ModSplineActor = SplineHandleProxy->SplineActor;
					State = SSS_TestPoint;
				}
				else if( HitProxy->IsA(HSplineSimpleComponentSlotPointHitProxy::StaticGetType()) )
				{
					HSplineSimpleComponentSlotPointHitProxy * RangeHandleProxy = (HSplineSimpleComponentSlotPointHitProxy*)HitProxy;
					check(NULL != RangeHandleProxy->SplineActor);
					State = SSS_RangeOnSpline;
					ModSplineActor = RangeHandleProxy->SplineActor;
					RangePointIndex = RangeHandleProxy->RangePointIndex;
					SlotIndex = RangeHandleProxy->SlotIndex;
					RangePoint = RangeHandleProxy->RangePoint;
					USimpleSplineAudioComponent * SimpleSplineAudioComponent = Cast<USimpleSplineAudioComponent>(RangeHandleProxy->SplineActor->AudioComponent);
					UMultiCueSplineAudioComponent * MultiCueSplineAudioComponent = Cast<UMultiCueSplineAudioComponent>(RangeHandleProxy->SplineActor->AudioComponent);
					if(NULL != SimpleSplineAudioComponent)
					{
					MovedPointPos = SimpleSplineAudioComponent->Points(RangePointIndex).Position;
				}
					else if(NULL != MultiCueSplineAudioComponent)
					{
						MovedPointPos = MultiCueSplineAudioComponent->Points(RangePointIndex).Position;
					}
				}
			}
		}
		// Release mouse button
		else if(Event == IE_Released)
		{
			// End any handle-tweaking
			if(ModSplineActor)
			{
				ModSplineActor->UpdateSpline();	
				ModSplineActor = NULL;
				State = SSS_None;
			}
		}
	}
	// easily add new point
	else if(Key == KEY_Period && Event == IE_Pressed)
	{
		if( NULL != ModSplineActor)
		{
			FInterpCurve<FVector>& CurvePoints = ModSplineActor->SplineComponent->SplineInfo;
			if(SSS_Point==State && CurvePoints.Points.Num() > 0 && CurvePoints.Points.Num() > PointIndex)
			{
				const FInterpCurvePoint<FVector> LastPoint = CurvePoints.Points(PointIndex);
				if(CurvePoints.Points.Num() - 1 == PointIndex)
			{
				PointIndex = CurvePoints.AddPoint(LastPoint.InVal + 1.0f, LastPoint.OutVal);
				}
				else if(0 == PointIndex)
				{
					CurvePoints.AddPointAtFront(LastPoint.OutVal, 1.0f);
				}
				else
				{
					return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
				}
				CurvePoints.Points(PointIndex).InterpMode = CIM_CurveAuto;
				ModSplineActor->UpdateSplineGeometry();
				CurvePoints.Points(PointIndex).InterpMode = LastPoint.InterpMode;
				ModSplineActor->UpdateSpline();
			}
		}
	}
	// remove point
	else if(KEY_BackSpace == Key && Event == IE_Pressed )
	{
		if(NULL != ModSplineActor && SSS_Point == State && NULL != ModSplineActor->SplineComponent)
		{
			FInterpCurve<FVector>& CurvePoints = ModSplineActor->SplineComponent->SplineInfo;
			if(CurvePoints.Points.Num() > PointIndex)
			{
				CurvePoints.Points.Remove(PointIndex);
				ModSplineActor->UpdateSpline();
			}
		}
	}

	return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}
template<class TSlot, class TSplineComponent>
void ChangeRange(TSplineComponent * SplineComponent, INT SlotIndex, const FVector& MovedPointPos, ESoundRangeOnSpline RangePoint)
{
	check(SplineComponent->Points.Num() > 3);
	INT BestIndex = 0;
	FLOAT BestDistanceSq = SplineComponent->Points(0).Position.DistanceSquared(MovedPointPos);
	for(INT i = 1; i < SplineComponent->Points.Num(); i++)
	{
		const FLOAT DistanceSq = SplineComponent->Points(i).Position.DistanceSquared(MovedPointPos);
		if(BestDistanceSq > DistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestIndex = i;
		}
	}
	TSlot& Slot = SplineComponent->SoundSlots(SlotIndex);
	if(BestIndex > SplineComponent->Points.Num()-1)
		BestIndex = SplineComponent->Points.Num()-1;

	if(RangePoint == SRS_Start)
	{
		if(Slot.EndPoint < 0 )
			Slot.StartPoint = Min(BestIndex, SplineComponent->Points.Num()-2);
		else if(Slot.EndPoint <= BestIndex )
		{
			Slot.EndPoint = BestIndex;
			Slot.StartPoint = Slot.EndPoint - 1;
		}
		else	
			Slot.StartPoint = BestIndex;
	}
	else if(RangePoint == SRS_Stop)
	{
		if(Slot.StartPoint < 0)
			Slot.EndPoint = Max(1, BestIndex);
		else if(Slot.StartPoint >= BestIndex )
		{
			Slot.StartPoint = BestIndex;
			Slot.EndPoint = Slot.StartPoint + 1;
		}
		else	
			Slot.EndPoint = BestIndex;
	}
}
UBOOL FEdModeAmbientSoundSpline::InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
{
	if( NULL != ModSplineActor && 
		NULL != ModSplineActor->SplineComponent && 
		(SSS_RangeOnSpline==State || SSS_TestPoint==State || 
			(ModSplineActor->SplineComponent->SplineInfo.Points.Num() > PointIndex && PointIndex >= 0)))
	{
		FVector InputDeltaDrag( InDrag );

		// We seem to need this - not sure why...
		if ( InViewportClient->ViewportType == LVT_OrthoXY )
		{
			InputDeltaDrag.X = InDrag.Y;
			InputDeltaDrag.Y = -InDrag.X;
		}
		//if we're using inverted panning
		if ((InViewportClient->ViewportType != LVT_Perspective) && InViewportClient->ShouldUseMoveCanvasMovement())
		{
			InputDeltaDrag = -InputDeltaDrag;
		}

		if(SSS_ArriveTangent==State)
		{
			InputDeltaDrag *= -1.f;
		}

		const FVector LocalDeltaDrag = ModSplineActor->WorldToLocal().TransformNormal(InputDeltaDrag);
		if(SSS_TestPoint==State)
		{
#if WITH_EDITORONLY_DATA
			ModSplineActor->TestPoint += LocalDeltaDrag;
#endif
		}
		else if(SSS_Point==State)
		{
			FInterpCurvePoint<FVector>& Point = ModSplineActor->SplineComponent->SplineInfo.Points(PointIndex);
			Point.OutVal += LocalDeltaDrag;
			if(0 == PointIndex)
			{
				ModSplineActor->Location += LocalDeltaDrag;
				GEditorModeTools().PivotLocation += LocalDeltaDrag;
				GEditorModeTools().SnappedLocation += LocalDeltaDrag;
				ModSplineActor->PostEditMove(TRUE);
			}
		}
		else if(SSS_ArriveTangent==State)
		{
			FInterpCurvePoint<FVector>& Point = ModSplineActor->SplineComponent->SplineInfo.Points(PointIndex);
			Point.ArriveTangent += LocalDeltaDrag;
			if(CIM_CurveUser == Point.InterpMode)
			{
				Point.LeaveTangent = Point.ArriveTangent;
			}
		}
		else if(SSS_RangeOnSpline==State)
		{
#if WITH_EDITORONLY_DATA
			MovedPointPos += LocalDeltaDrag;

			USimpleSplineAudioComponent * SimpleSplineAudioComponent = Cast<USimpleSplineAudioComponent>(ModSplineActor->AudioComponent);
			UMultiCueSplineAudioComponent * MultiCueSplineAudioComponent = Cast<UMultiCueSplineAudioComponent>(ModSplineActor->AudioComponent);
			check(NULL != SimpleSplineAudioComponent || NULL != MultiCueSplineAudioComponent);

			if(NULL != SimpleSplineAudioComponent)
			{
				ChangeRange<FSplineSoundSlot, USimpleSplineAudioComponent>(SimpleSplineAudioComponent, SlotIndex, MovedPointPos, RangePoint);
				}
			else if(NULL != MultiCueSplineAudioComponent)
				{
				ChangeRange<FMultiCueSplineSoundSlot, UMultiCueSplineAudioComponent>(MultiCueSplineAudioComponent, SlotIndex, MovedPointPos, RangePoint);
				}

			ModSplineActor->AudioComponent->PostEditChange();
			ModSplineActor->PostEditChange();
#endif		
		}
		else
		{
			FInterpCurvePoint<FVector>& Point = ModSplineActor->SplineComponent->SplineInfo.Points(PointIndex);
			Point.LeaveTangent += LocalDeltaDrag;
			if(CIM_CurveUser == Point.InterpMode)
			{
				Point.ArriveTangent = Point.LeaveTangent;
			}
		}

		ModSplineActor->UpdateSpline();
		return TRUE;
	}
		
	return FEdMode::InputDelta(InViewportClient, InViewport, InDrag, InRot, InScale);
}
UBOOL FEdModeAmbientSoundSpline::AllowWidgetMove()
{
	if(ModSplineActor)
	{
		return FALSE;
	}
	else
	{
		return TRUE;	
	}
}
