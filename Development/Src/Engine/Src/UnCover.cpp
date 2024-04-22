/*=============================================================================
	UnCover.cpp:

  CoverLink and subclass functions

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineAIClasses.h"
#include "UnPath.h"

IMPLEMENT_CLASS(ACoverLink);
IMPLEMENT_CLASS(ACoverGroup);

#define WANTS_COVER_DEBUG_DRAWING 0

#if WANTS_COVER_DEBUG_DRAWING
	#define ConditionalDrawDebugLine(A,B,C,D,E,F) DrawDebugLine(A,B,C,D,E,F)
#else
	#define ConditionalDrawDebugLine(A,B,C,D,E,F)
#endif

FString FCoverInfo::ToString() const
{
	return FString::Printf(TEXT("%s/%d"), *Link->GetName(), SlotIdx);
}

/**
* Creates a new slot at the specified location/rotation and either inserts it at the specified index or
* appends it to the end.  Also rebuilds the slot information if not currently in game.
*/
INT ACoverLink::AddCoverSlot(FVector SlotLocation, FRotator SlotRotation, INT SlotIdx, UBOOL bForceSlotUpdate, AScout* Scout)
{
	// create a new slot based on default
	FCoverSlot NewSlot = GetArchetype<ACoverLink>()->Slots(0);
	NewSlot.LocationOffset = FRotationMatrix(Rotation).InverseTransformFVectorNoScale(SlotLocation - Location);
	NewSlot.RotationOffset = SlotRotation - Rotation;
	if (SlotIdx == -1)
	{
		SlotIdx = Slots.AddItem(NewSlot);
	}
	else
	{
		SlotIdx = Slots.InsertItem(NewSlot,SlotIdx);
	}
	if (!GIsGame)
	{
		// update the slot info
		AutoAdjustSlot(SlotIdx,FALSE);
		AutoAdjustSlot(SlotIdx,TRUE);
		BuildSlotInfo(SlotIdx,FALSE,Scout);
	}
	else
		if (bForceSlotUpdate)
		{
			BuildSlotInfo(SlotIdx,FALSE,Scout);
		}
		return SlotIdx;
}

/**
* Adds the given slot to this link's list.  Returns the index.
*/
INT ACoverLink::AddCoverSlot(FVector& SlotLocation, FRotator& SlotRotation, FCoverSlot Slot, INT SlotIdx)
{
	//debugf(TEXT("** adding slot **"));
	// adjust the location/rotation of the slot for it's new owner
	Slot.LocationOffset = FRotationMatrix(Rotation).InverseTransformFVectorNoScale(SlotLocation - Location);
	Slot.RotationOffset = SlotRotation - Rotation;
	if (SlotIdx == -1)
	{
		SlotIdx = Slots.AddItem(Slot);
	}
	else
	{
		SlotIdx = Slots.InsertItem(Slot,SlotIdx);
	}
	return SlotIdx;
}

UBOOL ACoverLink::FindCoverEdges(const FVector& StartLoc, FVector AxisX, FVector AxisY, FVector AxisZ)
{
	FLOAT StandingOffset = Cast<ACoverLink>(ACoverLink::StaticClass()->GetDefaultObject())->MidHeight;
	Slots.Empty();
	FCheckResult Hit;
	UBOOL bFoundEdge = FALSE;
	FVector Start = StartLoc;
	FVector LastHitNormal = AxisX * -1.f;
	INT IterationCount = 0;
	while (!bFoundEdge && IterationCount++ < 150)
	{
		// remember the initial start for this iteration in case we find an edge
		FVector InitialStart = Start;
		// trace to the left, first looking for a wall
		FVector End = Start + (AxisY * AlignDist * -2.f);
		ConditionalDrawDebugLine(Start,End,0,128,0,1);
		if (GWorld->SingleLineCheck(Hit,this,End,Start,TRACE_World,FVector(1.f)))
		{
			// hit nothing, so trace down to make sure we have a valid base
			Start = End;
			End = Start + FVector(0,0,AlignDist * -4.f);
			ConditionalDrawDebugLine(Start,End,0,128,0,1);
			if (GWorld->SingleLineCheck(Hit,this,End,Start,TRACE_World,FVector(1.f)))
			{
				// hit nothing, found a floor gap so trace back to get the correct edge
				debugf(TEXT("-+ found floor gap"));
				bFoundEdge = TRUE;
				Start = End;
				End = Start + AxisY * AlignDist * 4.f;
				ConditionalDrawDebugLine(Start,End,0,128,0,1);
				FVector SlotLocation = InitialStart;
				if (!GWorld->SingleLineCheck(Hit,this,End,Start,TRACE_World,FVector(1.f)))
				{
					// found the edge, set the slot location here
					SlotLocation += -AxisY * ((AlignDist * 2.f) - (Hit.Location - Start).Size() - AlignDist);
				}
				AddCoverSlot(SlotLocation,AxisX.Rotation());
			}
			else
			{
				// trace forward to look for a gap
				End = Start + AxisX * AlignDist * 2.f;
				ConditionalDrawDebugLine(Start,End,0,128,0,1);
				if (GWorld->SingleLineCheck(Hit,this,End,Start,TRACE_World,FVector(1.f)))
				{
					debugf(TEXT("-+ found gap"));
					// hit nothing, found gap
					bFoundEdge = TRUE;
					// trace back to find the edge
					Start = Start + AxisX * AlignDist * 1.5f;
					End = Start + AxisY * AlignDist * 4.f;
					FVector SlotLocation = InitialStart;
					ConditionalDrawDebugLine(Start,End,0,128,0,1);
					if (!GWorld->SingleLineCheck(Hit,this,End,Start,TRACE_World,FVector(1.f)))
					{
						// found the edge, set the slot location here
						SlotLocation += -AxisY * ((AlignDist * 2.f) - (Hit.Location - Start).Size() - AlignDist);
					}
					AddCoverSlot(SlotLocation,(LastHitNormal * -1).Rotation());
				}
				else
				{
					// hit something, adjust the axis and continue
					debugf(TEXT("-+ hit wall, walking"));
					ConditionalDrawDebugLine(Hit.Location,Hit.Location + (Hit.Normal * 256.f),255,0,0,1);
					FRotationMatrix((Hit.Normal * -1).Rotation()).GetAxes(AxisX,AxisY,AxisZ);
					LastHitNormal = Hit.Normal;
					Start = Hit.Location + AxisX * -AlignDist;
				}
			}
		}
		else
		{
			debugf(TEXT("-+ hit adjacent wall"));
			// hit a wall, adjust the axes and keep going
			FRotationMatrix((Hit.Normal * -1).Rotation()).GetAxes(AxisX,AxisY,AxisZ);
			Start = Hit.Location + AxisX * -AlignDist;
		}
	}
	// if we failed to find an edge then abort now
	if (!bFoundEdge)
	{
		return FALSE;
	}
	debugf(TEXT("----- found left edge, working back -----"));
	// if we found a left edge then start filling in to the right edge, looking for transitions along the way
	bFoundEdge = FALSE;
	IterationCount = 0;
	INT LastSlotIdx = 0;
	FRotationMatrix(GetSlotRotation(LastSlotIdx)).GetAxes(AxisX,AxisY,AxisZ);
	Start = GetSlotLocation(LastSlotIdx);
	FVector LastPotentialSlotLocation = Start;
	FVector End;
	FLOAT LastSlotDist = 0.f;
	while (!bFoundEdge && IterationCount++ < 150)
	{
		FCoverSlot &LastSlot = Slots(LastSlotIdx);
		FVector InitialStart = Start;
		// then walk along until we find a high gap
		End = Start + (AxisY * AlignDist * 2.f);
		ConditionalDrawDebugLine(Start,End,0,128,128,1);
		if (GWorld->SingleLineCheck(Hit,this,End,Start,TRACE_World,FVector(1.f)))
		{
			debugf(TEXT("> side clear"));
			FLOAT SideDist = (End - Start).Size();
			// trace fwd
			Start = End;
			End = Start + (AxisX * AlignDist * 3.f);
			ConditionalDrawDebugLine(Start,End,0,128,128,1);
			if (!GWorld->SingleLineCheck(Hit,this,End,Start,TRACE_World,FVector(1.f)))
			{
				debugf(TEXT(">- hit fwd wall"));
				FRotator HitRotation = (Hit.Normal * -1).Rotation();
				// hit something, check from standing height
				Start.Z += StandingOffset;
				End.Z += StandingOffset;
				ConditionalDrawDebugLine(Start,End,0,128,128,1);
				if (GWorld->SingleLineCheck(Hit,this,End,Start,TRACE_World,FVector(1.f)))
				{
					debugf(TEXT(">-- fwd tall clear"));
					if (LastSlot.CoverType == CT_Standing)
					{
						// didn't hit, trace back to find the gap
						Start = Start + (AxisX * AlignDist * 1.5f);
						End = Start + (AxisY * AlignDist * -2.f);
						ConditionalDrawDebugLine(Start,End,128,128,64,1);
						if (!GWorld->SingleLineCheck(Hit,this,End,Start,TRACE_World,FVector(1.f)))
						{
							debugf(TEXT(">--+ traced back to find edge (standing->mid)"));
							// hit the wall, lay down two slots
							FVector Mid = InitialStart + (AxisY * (SideDist - (Hit.Location - Start).Size()));
							AddCoverSlot(Mid - (AxisY * AlignDist),HitRotation);
							LastSlotIdx = AddCoverSlot(Mid + (AxisY * AlignDist),HitRotation);
							FRotationMatrix(GetSlotRotation(LastSlotIdx)).GetAxes(AxisX,AxisY,AxisZ);
							// setup the new location/rotation for the next placement check
							Start = GetSlotLocation(LastSlotIdx);
							FRotationMatrix(GetSlotRotation(LastSlotIdx)).GetAxes(AxisX,AxisY,AxisZ);
							LastSlotDist = 0.f;
						}
						else
						{
							debugf(TEXT(">--+ FAILED traced back to find edge"));
						}
					}
					else
					{
						// didn't hit something, still mid level cover
						Start = InitialStart + (AxisY * AlignDist * 2.f);
						FRotationMatrix(HitRotation).GetAxes(AxisX,AxisY,AxisZ);
						debugf(TEXT(">--+ retaining mid level"));

						LastSlotDist += AlignDist * 2.f;
						if( LastSlotDist >= AutoCoverSlotInterval )
						{
							debugf(TEXT(">--+ adding intermediate mid-level slot"));
							LastSlotIdx = AddCoverSlot(Start,HitRotation);
							LastSlotDist = 0.f;
						}	
					}
				}
				else
				{
					if (LastSlot.CoverType == CT_Standing)
					{
						debugf(TEXT(">-- fwd tall hit, retaining standing"));
						// hit something, keep on trucking
						Start = InitialStart + (AxisY * AlignDist * 2.f); 
						FRotationMatrix((Hit.Normal * -1).Rotation()).GetAxes(AxisX,AxisY,AxisZ);

						LastSlotDist += AlignDist * 2.f;
						if( LastSlotDist >= AutoCoverSlotInterval )
						{
							debugf(TEXT(">--+ adding intermediate standing-level slot"));
							LastSlotIdx = AddCoverSlot(Start,HitRotation);
							LastSlotDist = 0.f;
						}	
					}
					else
					{
						// trace back to find the edge and lay down the transition slots
						Start = InitialStart + (AxisX * AlignDist * 1.5f) + FVector(0,0,StandingOffset);
						End = Start + (AxisY * AlignDist * 2.f);
						ConditionalDrawDebugLine(Start,End,128,128,64,1);
						if (!GWorld->SingleLineCheck(Hit,this,End,Start,TRACE_World,FVector(1.f)))
						{
							debugf(TEXT(">--+ traced back to find edge (mid->standing)"));
							// hit the wall, place transition slots
							//FVector Mid = InitialStart + (AxisY * (SideDist - (Hit.Location - Start).Size() - AlignDist));
							FVector Mid = InitialStart + (AxisY * (Hit.Location - Start).Size());
							AddCoverSlot(Mid - (AxisY * AlignDist),HitRotation);
							LastSlotIdx = AddCoverSlot(Mid + (AxisY * AlignDist),HitRotation);
							FRotationMatrix(GetSlotRotation(LastSlotIdx)).GetAxes(AxisX,AxisY,AxisZ);
							// setup the new location/rotation for the next placement check
							Start = GetSlotLocation(LastSlotIdx);
							FRotationMatrix(GetSlotRotation(LastSlotIdx)).GetAxes(AxisX,AxisY,AxisZ);
							LastSlotDist = 0.f;
						}
					}
				}
			}
			else
			{
				debugf(TEXT(">- didn't hit fwd, tracing for edge"));
				// didn't hit something, trace back to find the edge
				bFoundEdge = TRUE;
				Start = Start + (AxisX * AlignDist * 1.5f);
				End = Start + (AxisY * AlignDist * -2.f);
				if (!GWorld->SingleLineCheck(Hit,this,End,Start,TRACE_World,FVector(1.f)))
				{
					debugf(TEXT(">-- found edge"));
					// hit the edge, place slot
					FVector NewSlotLocation = InitialStart + (AxisY * (AlignDist * 2.f - (Hit.Location - Start).Size() - AlignDist));
					// make sure there was enough distance to make it worth placing an extra slot
					if ((NewSlotLocation - GetSlotLocation(LastSlotIdx)).Size() > AlignDist)
					{
						// add the new cover slot
						LastSlotIdx = AddCoverSlot(NewSlotLocation,(Hit.Normal * -1).Rotation());
						LastSlotDist = 0.f;
					}
					else
					{
						debugf(TEXT(">--- too close to original slot, skipping"));
						// save this position in case we need the information to auto-center the previous slot
						LastPotentialSlotLocation = NewSlotLocation;
					}
				}
				else
				{
					debugf(TEXT(">-- missed edge, placing back at start position"));
					LastSlotIdx = AddCoverSlot(InitialStart,AxisX.Rotation());
					LastSlotDist = 0.f;
				}
			}
		}
		else
		{
			debugf(TEXT("> hit side wall"));
			// hit a side wall, throw down both slots and adjust
			FVector NewSlotLocation = Hit.Location + (Hit.Normal * AlignDist);
			//@note - the following assumes that AxisX is semi-perpindicular to Hit.Normal, and for better or worse it seems to work well enough
			// add first slot farther up facing original direction
			AddCoverSlot(NewSlotLocation + (Hit.Normal * 16.f),AxisX.Rotation());
			// add second facing new direction
			LastSlotIdx = AddCoverSlot(NewSlotLocation + (AxisX * -16.f),(Hit.Normal * -1).Rotation());
			// setup the new location/rotation for the next placement check
			Start = GetSlotLocation(LastSlotIdx);
			FRotationMatrix(GetSlotRotation(LastSlotIdx)).GetAxes(AxisX,AxisY,AxisZ);
			LastSlotDist = 0.f;
		}
	}
	if (Slots.Num() == 1 && bFoundEdge)
	{
		debugf(TEXT("> correcting single slot position"));
		FVector OldSlotLocation = GetSlotLocation(0);
		Slots(0).LocationOffset = FRotationMatrix(Rotation).InverseTransformFVectorNoScale((OldSlotLocation + (((OldSlotLocation - LastPotentialSlotLocation).Size() * 0.5f) * (LastPotentialSlotLocation - OldSlotLocation).SafeNormal())) - Location);
	}

	FPathBuilder::DestroyScout();
	return TRUE;
}

void ACoverLink::EditorAutoSetup(FVector Direction, FVector *HitLoc, FVector *HitNorm)
{
	FVector SetupLoc(0), SetupNorm(0);
	UBOOL bTrySetup = FALSE;
	// if loc/norm were passed in
	if (HitLoc != NULL && HitNorm != NULL)
	{
		// then just use those values
		SetupLoc = *HitLoc;
		SetupNorm = *HitNorm;
		bTrySetup = TRUE;
	}
	else
	{
		// search for the surface that was clicked on
		FCheckResult Hit;
		if (!GWorld->SingleLineCheck(Hit,this,Location + Direction * 256.f,Location,TRACE_World,FVector(1.f)))
		{
			SetupLoc = Hit.Location;
			SetupNorm = Hit.Normal;
			bTrySetup = TRUE;
		}
	}
	// if we have valid loc/norm
	if (bTrySetup)
	{
		// check to see if the surface is valid
		if (Abs(SetupNorm | FVector(0,0,1)) > 0.3f)
		{
			warnf(TEXT("Invalid surface normal"));
			GWorld->DestroyActor(this);
			return;
		}

		// attempt to move the coverlink into position
		SetRotation((SetupNorm * -1).Rotation());
		SetLocation(SetupLoc + SetupNorm * 128.f);
		FindBase();

		// get base rotation axes
		FRotationMatrix RotMatrix(Rotation);
		FVector AxisX, AxisY, AxisZ;
		RotMatrix.GetAxes(AxisX,AxisY,AxisZ);

		// and attempt to setup
		if (!FindCoverEdges(Location + AxisX * 96.f + AxisZ * 16.f,AxisX,AxisY,AxisZ))
		{
			warnf(TEXT("Failed to place any slots"));
			GWorld->DestroyActor(this);
			return;
		}

		ForceUpdateComponents(FALSE,FALSE);
		debugf(TEXT("all finished"));
		//GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
	}
	else
	{
		// kick up a warning letting the designer know what happened
		warnf(TEXT("Failed to find valid surface"));
		GWorld->DestroyActor(this);
	}
}

UBOOL ACoverLink::LinkCoverSlotToNavigationMesh(INT SlotIdx, UNavigationMeshBase* Mesh)
{
	FNavMeshPolyBase* Poly = NULL;
	if(Mesh != NULL)
	{
		Poly = Mesh->GetPolyFromPoint(GetSlotLocation(SlotIdx)-FVector(0,0,30),AScout::GetGameSpecificDefaultScoutObject()->WalkableFloorZ);
	}
	else
	{
		APylon* Py = NULL;
		UNavigationHandle::GetPylonAndPolyFromPos(GetSlotLocation(SlotIdx)-FVector(0,0,30),AScout::GetGameSpecificDefaultScoutObject()->WalkableFloorZ,Py,Poly);
	}

	if( Poly == NULL )
	{
		// if we found no poly for this cover slot, move slot back a bit along cover normal away from the wall
		const FLOAT NudgeDist = 15.f;
		const FVector NewSlotLoc = GetSlotLocation(SlotIdx)-GetSlotRotation(SlotIdx).Vector() * NudgeDist;

		if(Mesh != NULL)
		{
			Poly = Mesh->GetPolyFromPoint(NewSlotLoc,AScout::GetGameSpecificDefaultScoutObject()->WalkableFloorZ);
		}
		else
		{
			APylon* Py = NULL;
			UNavigationHandle::GetPylonAndPolyFromPos(NewSlotLoc,AScout::GetGameSpecificDefaultScoutObject()->WalkableFloorZ,Py,Poly);
		}

		if( Poly != NULL )
		{
			// it worked! so change location
			Slots(SlotIdx).LocationOffset = FRotationMatrix(Rotation).InverseTransformFVectorNoScale(NewSlotLoc-Location);
		}
		else
		{
			return FALSE;
		}
	}

	FCoverReference	CovRef;
	CovRef.Guid			= FGuid(EC_EventParm);
	CovRef.Actor		= this;
	CovRef.SlotIdx		= SlotIdx;
	Poly->AddCoverReference(CovRef);
	return TRUE;
}


/**
*	Gives a valid FCoverSlot* from a FCoverInfo ref
*/
FCoverSlot* ACoverLink::CoverInfoToSlotPtr( FCoverInfo& Info )
{
	FCoverSlot* Result = NULL;
	if( Info.Link &&
		Info.SlotIdx >= 0 && 
		Info.SlotIdx <  Info.Link->Slots.Num() )
	{
		Result = &Info.Link->Slots(Info.SlotIdx);
	}
	return Result;
}

FCoverSlot* ACoverLink::CoverRefToSlotPtr( FCoverReference& InRef )
{
	FCoverSlot* Result = NULL;
	ACoverLink* Link   = Cast<ACoverLink>(InRef.Nav());
	if( Link &&
		InRef.SlotIdx >= 0 &&
		InRef.SlotIdx <  Link->Slots.Num() )
	{
		Result = &Link->Slots(InRef.SlotIdx);
	}
	return Result;
}

#if WITH_EDITOR
INT ACoverLink::AddMyMarker(AActor *S)
{
	return 0;
}
#endif

void ACoverLink::GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel)
{
	Super::GetActorReferences(ActorRefs,bIsRemovingLevel);
	for (INT SlotIdx = 0; SlotIdx < Slots.Num(); SlotIdx++)
	{
		FCoverSlot &Slot = Slots(SlotIdx);
		for (INT Idx = 0; Idx < Slot.SlipRefs.Num(); Idx++)
		{
			FActorReference &ActorRef = Slot.SlipRefs(Idx).Poly.OwningPylon;
			if ((bIsRemovingLevel && ActorRef.Actor != NULL) ||
				(!bIsRemovingLevel && ActorRef.Actor == NULL))
			{
				ActorRefs.AddItem(&ActorRef);
			}
		}
	}
}

#if WITH_EDITOR
void ACoverLink::CheckForErrors()
{
	Super::CheckForErrors();

	for( INT SlotIdx = 0; SlotIdx < Slots.Num(); SlotIdx++ )
	{
		FCoverSlot* Slot = &Slots(SlotIdx);
		if( Slot->CoverType == CT_None )
		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_SlotNoCoverType" ), *GetName(), SlotIdx ) ), TEXT( "SlotNoCoverType" ) );
		}
		if( bAutoAdjust && Slot->bFailedToFindSurface )
		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_SlotFailedAlignToSurface" ), *GetName(), SlotIdx ) ), TEXT( "SlotFailedAlignToSurface" ) );
		}
	}
}
#endif

UBOOL ACoverLink::FindSlots(FVector CheckLocation, FLOAT MaxDistance, INT& LeftSlotIdx, INT& RightSlotIdx)
{
	LeftSlotIdx  = -1;
	RightSlotIdx = -1;
	// run through each slot
	FRotationMatrix RotMatrix(Rotation);
	for (INT SlotIdx = 0; SlotIdx < Slots.Num() - 1; SlotIdx++)
	{
		FVector LeftSlotLocation = Location + RotMatrix.TransformFVector(Slots(SlotIdx).LocationOffset);
		FVector RightSlotLocation = Location + RotMatrix.TransformFVector(Slots(SlotIdx+1).LocationOffset);
		// adjust for edge slots
		if (SlotIdx == 0)
		{
			LeftSlotLocation += (LeftSlotLocation - RightSlotLocation).SafeNormal() * AlignDist;
		}
		if (SlotIdx == Slots.Num() - 2)
		{
			RightSlotLocation += (RightSlotLocation - LeftSlotLocation).SafeNormal() * AlignDist;
		}

		// Get the axis between the two slots
		FVector Axis = (RightSlotLocation-LeftSlotLocation).SafeNormal();

		// Only allow in, if half way to an enabled slot
		FLOAT Pct = Clamp<FLOAT>(((CheckLocation - LeftSlotLocation) | Axis) / (RightSlotLocation - LeftSlotLocation).Size(), 0.f, 1.f);
		if( (Pct < 0.5f && Slots(SlotIdx).bEnabled)		||
			(Pct >= 0.5f && Slots(SlotIdx+1).bEnabled)	)
		{
			// check to see if we are between the two locations
			if( ((LeftSlotLocation  - CheckLocation).SafeNormal() | -Axis) > 0.f &&
				((RightSlotLocation - CheckLocation).SafeNormal() |  Axis) > 0.f )
			{
				// find the distance from the link plane
				FVector ProjectedLocation = LeftSlotLocation + (Axis * ((CheckLocation-LeftSlotLocation) | Axis));
				FLOAT	Dist = (ProjectedLocation - CheckLocation).Size();
				if( Dist <= MaxDistance )
				{
					// mark the slots and break out
					LeftSlotIdx		= SlotIdx;
					RightSlotIdx	= SlotIdx + 1;
					break;
				}
			}
		}
	}
	return (LeftSlotIdx != -1 && RightSlotIdx != -1);
}


/**
* Returns the height for the specified slot.
*/
FLOAT ACoverLink::GetSlotHeight(INT SlotIdx)
{
	if( SlotIdx >= 0 && SlotIdx < Slots.Num() )
	{
		switch (Slots(SlotIdx).CoverType)
		{
		case CT_MidLevel:
			return MidHeight;
		case CT_Standing:
		default:
			return StandHeight;
		}
	}
	return 0.f;
}

FVector ACoverLink::GetSlotLocation(INT SlotIdx, UBOOL bForceUseOffset)
{
	if (SlotIdx >= 0 && SlotIdx < Slots.Num())
	{
		return Location + FRotationMatrix(Rotation).TransformFVector(Slots(SlotIdx).LocationOffset);
	}
	return Location;
}

FRotator ACoverLink::GetSlotRotation(INT SlotIdx, UBOOL bForceUseOffset)
{
	if (SlotIdx >= 0 && SlotIdx < Slots.Num())
	{
		return FRotator(Rotation.Quaternion() * Slots(SlotIdx).RotationOffset.Quaternion());
	}
	return Rotation;
}

FVector ACoverLink::GetSlotViewPoint( INT SlotIdx, BYTE Type, BYTE Action )
{
	if( SlotIdx >= 0 && SlotIdx < Slots.Num() )
	{
		FVector Offset;
		FVector ViewPt = GetSlotLocation(SlotIdx);

		if( Type == CT_None )
		{
			Type = Slots(SlotIdx).CoverType;
		}

		switch( Type )
		{
		case CT_Standing:
			Offset	  = StandingLeanOffset;
			break;
		case CT_MidLevel:
		default:
			Offset	  = CrouchLeanOffset;
			break;
		}

		if( Action != CA_Default )
		{
			FVector X, Y, Z;
			FRotationMatrix(GetSlotRotation(SlotIdx)).GetAxes(X,Y,Z);

			if( Action == CA_LeanLeft	|| 
				Action == CA_BlindLeft	|| 
				Action == CA_PeekLeft	)
			{
				ViewPt += (Offset.X *  X + 
					Offset.Y * -Y + 
					Offset.Z *  Z);
			}
			else
				if( Action == CA_LeanRight	|| 
					Action == CA_BlindRight || 
					Action == CA_PeekRight	)
				{
					ViewPt += (Offset.X *  X + 
						Offset.Y *  Y + 
						Offset.Z *  Z);
				}
				else if( Type == CT_MidLevel )
				{
					if( Action == CA_PopUp		|| 
						Action == CA_BlindUp	|| 
						Action == CA_PeekUp		)
					{
						ViewPt += (PopupOffset.X *  X + 
							PopupOffset.Y *  Y + 
							PopupOffset.Z *  Z);
					}
				}
		}
		else
		{
			ViewPt.Z += Offset.Z;
		}

		return ViewPt;
	}
	return Location;
}

BYTE ACoverLink::GetLocationDescription(INT SlotIdx)
{
	if( SlotIdx >= 0 && SlotIdx < Slots.Num() )
	{
		BYTE Desc = Slots(SlotIdx).LocationDescription;
		if (Desc == CoverDesc_None)
		{
			Desc = LocationDescription;
		}

		return Desc;
	}

	return CoverDesc_None;
}

UBOOL ACoverLink::CanFireLinkHit( const FVector &ViewPt, const FVector &TargetLoc, UBOOL bDebugLines)
{
	//debugf(TEXT("- can fire at %s (%d)?"),*Link->GetName(),SlotIdx);
	FCheckResult Hit(1.f);
	FVector TraceExtent( 0.f );

	GWorld->SingleLineCheck( Hit, this, TargetLoc, ViewPt, TRACE_World|TRACE_StopAtAnyHit|TRACE_ComplexCollision, TraceExtent );
	if( Hit.Actor == NULL )
	{
		return 1;
	}

	//debug
	if( bDebugLines ) { DrawDebugLine( ViewPt, TargetLoc, 0, 0, 255, 1 ); }

	// no luck
	return 0;
}

UBOOL ACoverLink::GetFireLinkTargetCoverInfo( INT SlotIdx, INT FireLinkIdx, FCoverInfo &out_Info, BYTE ArrayID )
{
	FCoverSlot& Slot	 = Slots(SlotIdx);
	FFireLink&  FireLink = Slot.GetFireLinkRef( FireLinkIdx, ArrayID );
	return GetCachedCoverInfo( FireLink.GetCoverRefIdx(), out_Info );
}

UBOOL ACoverLink::GetCachedCoverInfo( INT RefIdx, FCoverInfo& out_Info )
{
	ULevel* Level = GetLevel();
	if( Level->CoverIndexPairs.IsValidIndex(RefIdx) )
	{
		FCoverIndexPair& CovPair = Level->CoverIndexPairs(RefIdx);
		if( Level->CoverLinkRefs.IsValidIndex(CovPair.ActorRefItem) )
		{
			out_Info.Link	 = Level->CoverLinkRefs(CovPair.ActorRefItem);
			out_Info.SlotIdx = CovPair.SlotIdx;
			return (out_Info.Link != NULL);
		}
	}
	return FALSE;
}

INT ACoverLink::FindCoverReference( ACoverLink* TestLink, INT TestSlotIdx, UBOOL bAddIfNotFound )
{
	check(TestLink!=NULL);
	check(TestSlotIdx >= 0 && TestSlotIdx < 256);

	ULevel* Level = GetLevel();

	// Search for existing reference in the list
	for( INT ChkIdx = 0; ChkIdx < Level->CoverIndexPairs.Num(); ChkIdx++ )
	{
		FCoverIndexPair& Pair = Level->CoverIndexPairs(ChkIdx);
		if( Pair.IsEqual( Level, TestLink, TestSlotIdx ) ) 
		{
			// Return cache index if found a match
			return ChkIdx;
		}
	}

	INT ActorRefItem = -1;
	for( INT ChkIdx = 0; ChkIdx < Level->CoverLinkRefs.Num(); ChkIdx++ )
	{
		const AActor* ChkLink = Level->CoverLinkRefs(ChkIdx);
		// If match found in cover link refs
		if( ChkLink == TestLink )
		{
			ActorRefItem = ChkIdx;
			break;
		}
	}

	if( ActorRefItem < 0 )
	{
		// If no match found, add a new entry
		ActorRefItem = Level->CoverLinkRefs.AddItem( TestLink );
	}

	// Add a new pair
	FCoverIndexPair Pair;
	Pair.ActorRefItem = ActorRefItem;
	Pair.SlotIdx = TestSlotIdx;
	INT ResultIdx = Level->CoverIndexPairs.AddItem( Pair );

	// Return cache index
	return ResultIdx;
}

UBOOL FCoverIndexPair::IsEqual( class ULevel* Level, ACoverLink* TestLink, INT TestSlotIdx )
{
	return (TestLink == Level->CoverLinkRefs(ActorRefItem) && TestSlotIdx == SlotIdx);
}

void ULevel::ClearCrossLevelCoverReferences( ULevel* LevelBeingRemoved )
{
	for( INT ChkIdx = 0; ChkIdx < CoverLinkRefs.Num(); ChkIdx++ )
	{
		ACoverLink* RefLink = CoverLinkRefs(ChkIdx);
		if( RefLink != NULL && ( (LevelBeingRemoved == NULL && !RefLink->IsInLevel( this )) || RefLink->IsInLevel(LevelBeingRemoved)) )
		{
			FGuidPair GuidPair;
			GuidPair.Guid  = *RefLink->GetGuid();
			GuidPair.RefId = ChkIdx;

			CrossLevelCoverGuidRefs.AddItem( GuidPair );

			// Clear actual pointer across level
			CoverLinkRefs(ChkIdx) = NULL;
		}
	}
}

void ULevel::FixupCrossLevelCoverReferences( UBOOL bRemovingLevel, TMap<FGuid, AActor*>* GuidHash, ULevel* LevelBeingAddedOrRemoved )
{
	if( bRemovingLevel )
	{
		ClearCrossLevelCoverReferences(LevelBeingAddedOrRemoved);
	}
	else
	{
		for( INT GuidIdx = CrossLevelCoverGuidRefs.Num() - 1; GuidIdx >= 0; GuidIdx-- )
		{
			FGuidPair& GuidPair = CrossLevelCoverGuidRefs(GuidIdx);

			AActor** FoundActor = GuidHash->Find(GuidPair.Guid);
			if( FoundActor != NULL )
			{
				CoverLinkRefs(GuidPair.RefId) = Cast<ACoverLink>(*FoundActor);
				CrossLevelCoverGuidRefs.RemoveSwap( GuidIdx );
			}
		}
	}
}

void ULevel::PurgeCrossLevelCoverArrays()
{
	CrossLevelCoverGuidRefs.Empty();
	CoverLinkRefs.Empty();
	CoverIndexPairs.Empty();
}

BYTE ACoverLink::PackFireLinkInteractionInfo( BYTE SrcType, BYTE SrcAction, BYTE DestType, BYTE DestAction )
{
	// verify inputs
	check(SrcType==CT_MidLevel||SrcType==CT_Standing);
	check(DestType==CT_MidLevel||DestType==CT_Standing);
	check(SrcAction==CA_LeanLeft||SrcAction==CA_LeanRight||SrcAction==CA_PopUp);
	check(DestAction==CA_LeanLeft||DestAction==CA_LeanRight||DestAction==CA_PopUp||DestAction==CA_Default);

	BYTE Result = 0;
	FireLinkInteraction_PackSrcType( SrcType, Result );
	FireLinkInteraction_PackSrcAction( SrcAction, Result );
	FireLinkInteraction_PackDestType( DestType, Result );
	FireLinkInteraction_PackDestAction( DestAction, Result );
	
	return Result;
}

void ACoverLink::UnPackFireLinkInteractionInfo( const BYTE PackedByte, BYTE& SrcType, BYTE& SrcAction, BYTE& DestType, BYTE& DestAction )
{
	SrcType		= FireLinkInteraction_UnpackSrcType( PackedByte );
	SrcAction	= FireLinkInteraction_UnpackSrcAction( PackedByte );
	DestType	= FireLinkInteraction_UnpackDestType( PackedByte );
	DestAction	= FireLinkInteraction_UnpackDestAction( PackedByte );
}

/**
************>>> ACoverLink::SortSlots() helper functions follow
*/
/** statics for sort comparator **/
#define SORT_DEBUG 0 && !FINAL_RELEASE && !PS3

static INT ClockWiseAngleDist(INT AYaw, INT BYaw)
{
	if(AYaw < 0)
	{
		AYaw = 65535 + AYaw;
	}
	if(BYaw < 0)
	{
		BYaw = 65535 + BYaw;
	}
	INT Delta = BYaw - AYaw;
	if(Delta < 0)
	{
		Delta += 65535;
		return Delta;
	}

	return Delta;
}
FLOAT GTraceDistance = 128.0f;
FLOAT GGapIncrement = 10.0f;
FLOAT SlotToSlotTraceWidth = 10.0f;
#define GAP_INCREMENT GGapIncrement
#define TRACE_DISTANCE GTraceDistance
#define GAP_THRESHOLD 150.f
#define SORT_TRACE_FLAGS TRACE_World|TRACE_StopAtAnyHit
#define SORT_TRACE_EXTENT FVector(1.f)
UBOOL HasGapBetween(ACoverLink* CoverLink, INT SlotA, INT SlotB)
{
	FVector SlotALoc = CoverLink->GetSlotLocation(SlotA);
	FVector SlotBLoc = CoverLink->GetSlotLocation(SlotB);

	FCheckResult Hit(1.f);
	// if there is something inbetween us and the slot, forget it
	if(!GWorld->SingleLineCheck(Hit,NULL,SlotALoc,SlotBLoc,SORT_TRACE_FLAGS,FVector(SlotToSlotTraceWidth)))
	{
#if SORT_DEBUG
		CoverLink->DrawDebugLine(SlotALoc,SlotBLoc,52,255,0,TRUE);
#endif
		return TRUE;
	}

	FVector TraceDir = ((CoverLink->GetSlotRotation(SlotA).Vector() + CoverLink->GetSlotRotation(SlotB).Vector())/2.f).SafeNormal();

	FVector TestDir = (SlotBLoc - SlotALoc).SafeNormal();

	FVector TestPos = SlotALoc;
	UBOOL LastTestWasGap = FALSE;
	FLOAT GapDist = 0.f;
	while((TestPos - SlotBLoc | TestDir) < 0.f)
	{
		//DEBUG
#if SORT_DEBUG
		CoverLink->DrawDebugLine(TestPos,TestPos+(TraceDir*TRACE_DISTANCE),255,255,255,TRUE);
#endif

		if(GWorld->SingleLineCheck(Hit,NULL,TestPos+(TraceDir*TRACE_DISTANCE),TestPos,SORT_TRACE_FLAGS,SORT_TRACE_EXTENT) && GWorld->SingleLineCheck(Hit,NULL,TestPos+(TraceDir*TRACE_DISTANCE),TestPos,SORT_TRACE_FLAGS))
		{
#if SORT_DEBUG
			CoverLink->DrawDebugLine(TestPos,TestPos+(TraceDir*TRACE_DISTANCE),200,200,200,TRUE);
#endif

			// if we pass through increment the gap size counter
			if(LastTestWasGap == TRUE)
			{
				GapDist += GAP_INCREMENT;
			}
			else
			{
				LastTestWasGap = TRUE;
			}

			if(GapDist >= GAP_THRESHOLD)
			{
#if SORT_DEBUG
				CoverLink->DrawDebugLine(TestPos,TestPos+(TraceDir*TRACE_DISTANCE),255,0,0,TRUE);
				CoverLink->DrawDebugLine(SlotALoc,SlotBLoc,255,128,0,TRUE);
#endif

				return TRUE;
			}			
		}
		else
		{
			LastTestWasGap = FALSE;
			GapDist = 0.f;
		}

		TestPos += TestDir * GAP_INCREMENT;
	}

	return FALSE;
}

#define RIGHT_DIR 1
#define LEFT_DIR -1
INT GetRatingFromAToB(ACoverLink* Link, INT SlotIdxA, INT SlotIdxB, INT Dir, INT YawConversionFactor=50)
{
	FRotationMatrix SlotRot = Link->GetSlotRotation(SlotIdxA);
	FVector ASlotLoc = Link->GetSlotLocation(SlotIdxA);

	INT AYaw = SlotRot.Rotator().Yaw;
	FVector AToB = Link->GetSlotLocation(SlotIdxB) - ASlotLoc;

	INT BYaw = AToB.Rotation().Yaw;
	INT YawDiff = Abs<INT>(ClockWiseAngleDist(AYaw,BYaw));

	if(Dir == LEFT_DIR && YawDiff > 0)
	{
		YawDiff = 65535 - YawDiff;
	}

	INT Dist = YawDiff / YawConversionFactor;

	// add in actual distance
	Dist += appTrunc(AToB.Size());

	return Dist;
}

INT FindBestMatchForSlot(ACoverLink* Link, INT SlotIdx, INT Dir, TDoubleLinkedList<INT>& ResortedList, UBOOL bCheckGap, INT YawConversionFactor=50)
{
	INT BestIdx = -1;
	FLOAT BestDist = BIG_NUMBER;
	for(INT Idx=0; Idx < Link->Slots.Num(); Idx++)
	{
		if(Idx == SlotIdx)
		{
			continue;
		}

		INT ADist = GetRatingFromAToB(Link,SlotIdx,Idx,Dir,YawConversionFactor);

		// if this is better, and valid (not already added)
		if(ADist < BestDist && ResortedList.FindNode(Idx) == NULL && (!bCheckGap || !HasGapBetween(Link,SlotIdx,Idx)))
		{
			BestDist = ADist;
			BestIdx = Idx;
		}
	}

	return BestIdx;
}

UBOOL LinkToBestCandidate(ACoverLink* Link, TDoubleLinkedList<INT>::TIterator& Itt, TDoubleLinkedList<INT>& ResortedList,INT Direction, UBOOL bCheckGap=TRUE)
{
	INT MatchIdx = FindBestMatchForSlot(Link,*Itt,Direction,ResortedList,bCheckGap);		
	if(MatchIdx != -1 && ResortedList.FindNode(MatchIdx)==NULL)
	{				
		if(Direction == LEFT_DIR)
		{
			ResortedList.InsertNode(MatchIdx,Itt.GetNode());
		}
		else if(Itt.GetNode() != ResortedList.GetTail())
		{
			ResortedList.InsertNode(MatchIdx,Itt.GetNode()->GetNextNode());
		}
		else
		{
			ResortedList.AddTail(MatchIdx);
		}

		return TRUE; 
	}


	return FALSE;
}

void InsertAtBestPoint(ACoverLink* Link, INT IdxToInsert, TDoubleLinkedList<INT>& ResortedList, INT Direction)
{
	INT BestRating = 65535;
	UBOOL bLeft = FALSE;
	TDoubleLinkedList<INT>::TIterator BestRated(NULL);
	for(TDoubleLinkedList<INT>::TIterator Itt(ResortedList.GetHead());Itt;++Itt)
	{
		INT CurRating = GetRatingFromAToB(Link,IdxToInsert,*Itt,RIGHT_DIR);

		if(CurRating < BestRating)
		{
			bLeft = FALSE;
			BestRating = CurRating;
			BestRated = Itt;
		}

	}

	for(TDoubleLinkedList<INT>::TIterator Itt(ResortedList.GetTail());Itt;--Itt)
	{
		INT CurRating = GetRatingFromAToB(Link,IdxToInsert,*Itt,LEFT_DIR);

		if(CurRating < BestRating)
		{
			bLeft = TRUE;
			BestRating = CurRating;
			BestRated = Itt;
		}
	}


	if(BestRated)
	{
		if(bLeft)
		{
			if(BestRated.GetNode()->GetNextNode() == NULL)
			{
				ResortedList.AddTail(IdxToInsert);
			}
			else
			{
				ResortedList.InsertNode(IdxToInsert,BestRated.GetNode()->GetNextNode());
			}
		}
		else
		{
			ResortedList.InsertNode(IdxToInsert,BestRated.GetNode());
		}


	}
}

/** ******>>> End SortSlots Helper functions			**/
void ACoverLink::SortSlots(FCoverSlot** LastSelectedSlot)
{
	if( bAutoSort && !bCircular && Slots.Num() > 0)
	{

#if SORT_DEBUG
		FlushPersistentDebugLines();
#endif

		//** Find the node with the worst rating to its neighbor to the left, chances are this is the leftmost node
		TDoubleLinkedList<INT> ResortedList;		
		INT BestDistSq = -1;
		INT LeftMostIdx = 0; // init to 0 in case no leftmost is found
		for(INT SlotIdx=0;SlotIdx<Slots.Num();SlotIdx++)
		{
			INT LeftOfMeIdx = FindBestMatchForSlot(this,SlotIdx,LEFT_DIR,ResortedList,TRUE,40);
#if SORT_DEBUG && 0
			FVector SlotLoc = GetSlotLocation(SlotIdx) + FVector(0.f,0.f,10.f);
			if(LeftOfMeIdx < 0)
			{
				DrawDebugLine(SlotLoc,SlotLoc + FVector(0.f,0.f,50.f),255,0,0,TRUE);
			}
			else
			{
				FVector LeftSlotLoc = GetSlotLocation(LeftOfMeIdx) + FVector(0.f,0.f,10.f);;
				DrawDebugLine(SlotLoc,SlotLoc + ((LeftSlotLoc-SlotLoc)*0.9f),255,255,0,TRUE);
			}			
#endif
			if(LeftOfMeIdx != -1)
			{
				INT CurDistSq = GetRatingFromAToB(this,SlotIdx,LeftOfMeIdx,LEFT_DIR,40);
				if( CurDistSq > BestDistSq)
				{
					LeftMostIdx = SlotIdx;
					BestDistSq = CurDistSq;
				}
			}
		}

#if SORT_DEBUG
		DrawDebugCoordinateSystem(GetSlotLocation(LeftMostIdx),GetSlotRotation(LeftMostIdx),50.f,TRUE);
#endif

		// add the leftmost at the head of the list
		ResortedList.AddHead(LeftMostIdx);

		// add in nodes to the right
		for(TDoubleLinkedList<INT>::TIterator Itt(ResortedList.GetHead());Itt;++Itt)
		{
			LinkToBestCandidate(this,Itt,ResortedList,RIGHT_DIR);
		}

		// if we're at the end, and we don't have everything sorted out yet start over from the front and add in nodes to the left
		if(ResortedList.Num() < Slots.Num())
		{
			for(TDoubleLinkedList<INT>::TIterator Itt(ResortedList.GetHead());Itt;--Itt)
			{
				LinkToBestCandidate(this,Itt,ResortedList,LEFT_DIR);
			}
		}	


		// if there are orphans, go back and place them in the list at the best point, and skip gap checks
		if(ResortedList.Num() < Slots.Num())
		{
			// add in nodes that didn't get sorted so we don't drop slots
			for(INT Idx=0;Idx<Slots.Num();Idx++)
			{
				if(ResortedList.FindNode(Idx) == NULL)
				{
#if SORT_DEBUG
					DrawDebugLine(GetSlotLocation(Idx),GetSlotLocation(Idx)+FVector(0.f,0.f,100.f),255,0,0,TRUE);
#endif
					//ResortedList.AddTail(Idx);
					InsertAtBestPoint(this,Idx,ResortedList,RIGHT_DIR);
				}
			}
		}

		if(ResortedList.Num() < Slots.Num())
		{
			debugf(TEXT("Could not fully sort slots for %s!"),*GetName());
		}

		INT IdxCounter = 0;
		TArray<FCoverSlot> Temp;

		UBOOL bFoundLast = FALSE;
		for(TDoubleLinkedList<INT>::TIterator Itt(ResortedList.GetHead());Itt;++Itt)
		{
			FCoverSlot* slot = &Slots(*Itt);
			INT SlotIdx = Temp.AddItem(*slot);

			if(!bFoundLast &&
				LastSelectedSlot &&
				*LastSelectedSlot == slot)
			{
				*LastSelectedSlot = &Slots(SlotIdx);
				bFoundLast = TRUE;
			}
			SlotIdx++;			
		}

		Slots = Temp;
	}
}

#define DESIREDMINFIRELINKANGLE  0.65
#define MINFIRELINKANGLE 0.45

static void GetOverlapSlotTestSpots( ACoverLink* Link, INT SlotIdx, FLOAT Radius, TArray<FVector>& out_List )
{
	const FVector SlotLocation = Link->GetSlotLocation( SlotIdx );
	out_List.AddItem( SlotLocation );

	const FVector YAxis = FRotationMatrix(Link->GetSlotRotation(SlotIdx)).GetAxis( 1 );
	FCoverSlot& Slot = Link->Slots(SlotIdx);
	if( Slot.bLeanRight )
	{
		out_List.AddItem( SlotLocation + (YAxis * Radius * 2.f) );
	}
	if( Slot.bLeanLeft )
	{
		out_List.AddItem( SlotLocation - (YAxis * Radius * 2.f) );
	}
}

static void FindOverlapSlots( AScout* Scout, ACoverLink* SrcLink, INT SrcSlotIdx )
{
	if( !Scout || !SrcLink )
	{
		return;
	}

	FCoverSlot& SrcSlot = SrcLink->Slots(SrcSlotIdx);
	FVector HumanSize = Scout->GetSize(FName(TEXT("Human"),FNAME_Find));
	FLOAT	ChkRadiusSum = HumanSize.X + HumanSize.X;

	TArray<FVector> CheckLocList;
	GetOverlapSlotTestSpots( SrcLink, SrcSlotIdx, HumanSize.X, CheckLocList );

	for( INT ChkIdx = 0; ChkIdx < CheckLocList.Num(); ChkIdx++ )
	{
		FVector SrcSlotLocation = CheckLocList(ChkIdx);

		FLOAT SrcTopZ = SrcSlotLocation.Z + SrcLink->GetSlotHeight(SrcSlotIdx) * 0.5f;
		FLOAT SrcBotZ = SrcTopZ - SrcLink->GetSlotHeight(SrcSlotIdx);
		for( ANavigationPoint *Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint )
		{
			ACoverLink* Link = Cast<ACoverLink>(Nav);
			if( Link )
			{
				for( INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++ )
				{
					// Skip overlaps with myself
					if( Link == SrcLink && SlotIdx == SrcSlotIdx )
					{
						continue;
					}
					// Don't allow cross level overlaps, rare and not worth the memory overhead of using ActorReference with a guid
					if( Link->GetOutermost() != SrcLink->GetOutermost() )
					{
						continue;
					}

					TArray<FVector> TestLocList;
					GetOverlapSlotTestSpots( Link, SlotIdx, HumanSize.X, TestLocList );
					for( INT TestIdx = 0; TestIdx < TestLocList.Num(); TestIdx++ )
					{
						FVector SlotLocation = TestLocList(TestIdx);
						FLOAT TopZ = SlotLocation.Z + Link->GetSlotHeight(SlotIdx) * 0.5f;
						FLOAT BotZ = TopZ - Link->GetSlotHeight(SlotIdx);

						// Check lateral overlap
						FLOAT DistSq2D = (SlotLocation-SrcSlotLocation).SizeSquared2D();
						if( DistSq2D < ChkRadiusSum*ChkRadiusSum )
						{
							// Check vertical overlap
							if( (TopZ <= SrcTopZ && BotZ >= SrcBotZ) ||
								(TopZ >= SrcTopZ && BotZ <= SrcBotZ) ||
								(TopZ <= SrcTopZ && TopZ >= SrcBotZ) ||
								(BotZ <= SrcTopZ && BotZ >= SrcBotZ) )
							{
								INT Idx = SrcSlot.OverlapClaimsList.AddZeroed();
								FCoverInfo& Info = SrcSlot.OverlapClaimsList(Idx);
								Info.Link =  Link;
								Info.SlotIdx	=  SlotIdx;
							}
						}
					}
				}
			}
		}
	}
}

static UBOOL CanPopUp(ACoverLink *Link, INT SlotIdx)
{
	FCoverSlot &Slot = Link->Slots(SlotIdx);

	// early out if LD has disabled popup for this slot
	if( !Slot.bAllowPopup )
	{
		return FALSE;
	}

	FCheckResult CheckResult;
	if( Slot.CoverType == CT_MidLevel )
	{
		// trace up to see if our body fits
		FVector CheckLocation = Link->GetSlotLocation( SlotIdx );
		FVector ViewPt = Link->GetSlotViewPoint( SlotIdx, Slot.CoverType, CA_PopUp );
		UBOOL bBodyFits = GWorld->SingleLineCheck( CheckResult, 
			Link,
			ViewPt,
			CheckLocation,
			TRACE_World,
			FVector(1.f));

		// trace forward to see if we would be looking at a wall
		FVector Forward = Link->GetSlotRotation(SlotIdx).Vector(); 		

		UBOOL bForwardClear = GWorld->SingleLineCheck( CheckResult, 
			Link,
			ViewPt + (Forward * 48.0f),
			ViewPt,
			TRACE_AllBlocking | TRACE_ComplexCollision );

		return (bBodyFits && bForwardClear);
	}

	return FALSE;
}

void ACoverLink::BuildSlotInfo(INT SlotIdx, UBOOL bSeedPylon, AScout* Scout)
{
	UBOOL bCreatedScout = FALSE;
	if( Scout == NULL )
	{
		bCreatedScout=TRUE;
		Scout = FPathBuilder::GetScout();
	}

	BuildSlotInfoInternal(Scout, SlotIdx, bSeedPylon);

	// if in game be sure to delete the scout
	if (bCreatedScout)
	{
		Scout = NULL;
		FPathBuilder::DestroyScout();
	}
}

void ACoverLink::BuildSlotInfoInternal(AScout* Scout, INT SlotIdx, UBOOL bSeedPylon)
{
	check(SlotIdx >= 0 && SlotIdx < Slots.Num());
	FCoverSlot &Slot = Slots(SlotIdx);



	// Pop up
	Slot.bCanPopUp = Slot.bForceCanPopUp || CanPopUp(this, SlotIdx);

	// Mantle over mid or std cover.
	Slot.bCanMantle = Slot.bCanPopUp && Slot.bAllowMantle;

	// Mantle up mid or std cover
	Slot.bCanClimbUp = Slot.bCanPopUp && Slot.bAllowClimbUp;

	// Set swat turn flag if can lean left/right
	Slot.bCanSwatTurn_Left	= !bCircular && Slot.bAllowSwatTurn && Slot.bLeanLeft  && (SlotIdx==0);
	Slot.bCanSwatTurn_Right = !bCircular && Slot.bAllowSwatTurn && Slot.bLeanRight && (SlotIdx==Slots.Num()-1);


	FVector	 SlotLocation = GetSlotLocation( SlotIdx );
	FRotator SlotRotation = GetSlotRotation( SlotIdx );
	FRotationMatrix R(SlotRotation);


	TArray<AActor*> ActorsCollisionChangedFor;

	// if we're not pathbuilding, we need to flip stuff near this coverslot into pathbuilding collision manually
#if WITH_EDITOR
	if ( GIsEditor && !GIsGame )
	{
		FMemMark Mark( GMainThreadMemStack );
		FCheckResult* ActorResult =
			GWorld->Hash->ActorRadiusCheck(
			GMainThreadMemStack,	// Memory stack
			SlotLocation,				// Location to test
			2.0f*Scout->MaxMantleLateralDist,					// Radius
			TRACE_AllBlocking );			// Flags

		for( FCheckResult* HitResult = ActorResult ; HitResult ; HitResult = HitResult->GetNext() )
		{
			AActor* CurActor = HitResult->Actor;
			if( CurActor != NULL )
			{
				if ( !FPathBuilder::IsBuildingPaths() )
				{
					if( !CurActor->bPathTemp && !CurActor->NeedsCollisionOverrideDuringCoverBuild() )
					{
						CurActor->SetCollisionForPathBuilding(TRUE);
						ActorsCollisionChangedFor.AddItem(CurActor);
					}
				}
				else if( CurActor->NeedsCollisionOverrideDuringCoverBuild() && CurActor->bPathTemp )
				{
					CurActor->SetCollisionForPathBuilding(FALSE);
					ActorsCollisionChangedFor.AddItem(CurActor);
				}
			}
		}

		Mark.Pop();
	}
#endif

	// only need to check this in the editor
	if (!GIsGame)
	{
		//// BEGIN OVERLAP SLOTS ////
		Slot.OverlapClaimsList.Empty();
		FindOverlapSlots( Scout, this, SlotIdx );
		//// END OVERLAP SLOTS ////
	}

	//// BEGIN MANTLE ////
	if( Slot.bCanClimbUp )
	{
		Slot.bCanClimbUp = Scout->CanDoMove( TEXT("MANTLEUP"), this, SlotIdx, bSeedPylon );
		if( Slot.bCanClimbUp )
		{
			Slot.bCanMantle = FALSE;
		}
	}
	if( Slot.bCanMantle )
	{
		Slot.bCanMantle = Scout->CanDoMove( TEXT("MANTLE"), this, SlotIdx, bSeedPylon );
	}
	//// END MANTLE ////

	//// BEGIN COVERSLIP ////
	// Set cover slip flag if standing cover and can lean left or right
	Slot.bCanCoverSlip_Left	 = Slot.bAllowCoverSlip &&
		Slot.bLeanLeft &&
		IsLeftEdgeSlot(SlotIdx,TRUE) &&
		(Slot.CoverType == CT_Standing || (Slot.CoverType == CT_MidLevel));

	Slot.bCanCoverSlip_Right = Slot.bAllowCoverSlip &&
		Slot.bLeanRight &&
		IsRightEdgeSlot(SlotIdx,TRUE) &&
		(Slot.CoverType == CT_Standing || (Slot.CoverType == CT_MidLevel));

	// Empty list of targets
	Slot.SlipRefs.Empty();
	// Verify cover slip left
	if( Slot.bCanCoverSlip_Left )
	{
		Slot.bCanCoverSlip_Left = Slot.bForceCanCoverSlip_Left || Scout->CanDoMove( TEXT("COVERSLIPLEFT"), this, SlotIdx, bSeedPylon );
	}
	// Verify cover slip right
	if( Slot.bCanCoverSlip_Right )
	{
		Slot.bCanCoverSlip_Right = Slot.bForceCanCoverSlip_Right || Scout->CanDoMove( TEXT("COVERSLIPRIGHT"), this, SlotIdx, bSeedPylon );
	}
	//// END COVERSLIP ////

	//// BEGIN SWAT TURN ////
	// Empty list of targets
	Slot.SetLeftTurnTargetCoverRefIdx( MAXWORD );
	Slot.SetRightTurnTargetCoverRefIdx( MAXWORD );
	
	// Verify swat turn left
	if( Slot.bCanSwatTurn_Left )
	{
		Slot.bCanSwatTurn_Left = Scout->CanDoMove( TEXT("SWATTURNLEFT"), this, SlotIdx, bSeedPylon );
	}
	// Verify swat turn right
	if( Slot.bCanSwatTurn_Right )
	{
		Slot.bCanSwatTurn_Right = Scout->CanDoMove( TEXT("SWATTURNRIGHT"), this, SlotIdx, bSeedPylon );
	}
	//// END SWAT TURN ////

#if WITH_EDITOR
	if ( GIsEditor && !GIsGame )
	{
		for(INT ActorIdx=0;ActorIdx<ActorsCollisionChangedFor.Num();++ActorIdx)
		{
			if( ActorsCollisionChangedFor(ActorIdx)->NeedsCollisionOverrideDuringCoverBuild() )
			{
				ActorsCollisionChangedFor(ActorIdx)->SetCollisionForPathBuilding(TRUE);
			}
			else
			{
				ActorsCollisionChangedFor(ActorIdx)->SetCollisionForPathBuilding(FALSE);
			}
		}
	}
#endif
}


void ACoverLink::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	if( appStricmp( *PropertyThatChanged->GetName(), TEXT("ForceCoverType") ) == 0 )
	{
		for( INT SlotIdx = 0; SlotIdx < Slots.Num(); SlotIdx++ )
		{
			AutoAdjustSlot( SlotIdx, TRUE );
		}
	}

	if( appStricmp( *PropertyThatChanged->GetName(), TEXT("bBlocked") )		 == 0 || 
		appStricmp( *PropertyThatChanged->GetName(), TEXT("CollisionType") ) == 0 )
	{
		GWorld->GetWorldInfo()->bPathsRebuilt = FALSE;
		bPathsChanged = TRUE;
	}
}

#if WITH_EDITOR
/** Properly handles the mirroring of cover slots associated with this link */
void ACoverLink::EditorApplyMirror(const FVector& MirrorScale, const FVector& PivotLocation)
{
	for( INT SlotIdx = 0; SlotIdx < Slots.Num(); ++SlotIdx )
	{
		FVector SlotPosition;
		FRotator SlotRotation;

		SlotPosition = GetSlotLocation(SlotIdx);
		SlotRotation = GetSlotRotation(SlotIdx);

		FRotationMatrix TempRot( SlotRotation );
		FVector New0( TempRot.GetAxis(0) * MirrorScale );
		FVector New1( TempRot.GetAxis(1) * MirrorScale );
		FVector New2( TempRot.GetAxis(2) * MirrorScale );
		FMatrix NewRot( New0, New1, New2, FVector(0,0,0) );

		SlotRotation = NewRot.Rotator();

		// Get the slot's offset from the pivot
		SlotPosition -= PivotLocation - PrePivot;
		SlotPosition *= MirrorScale;
		SlotPosition += PivotLocation - PrePivot;

		// Use offset to temporarily store world position and rotation
		Slots(SlotIdx).LocationOffset = SlotPosition;
		Slots(SlotIdx).RotationOffset = SlotRotation;
	}

	Super::EditorApplyMirror(MirrorScale, PivotLocation);

	// Setup slot offsets again
	for( INT SlotIdx = 0; SlotIdx < Slots.Num(); ++SlotIdx )
	{
		Slots(SlotIdx).LocationOffset = FRotationMatrix(Rotation).InverseTransformFVectorNoScale(Slots(SlotIdx).LocationOffset - Location);
		Slots(SlotIdx).RotationOffset = Slots(SlotIdx).RotationOffset - Rotation;
	}
}
#endif

/**
* Attempts to orient the specified slot to the nearest wall, and determine the height of the slot
* based upon the facing wall.
*/
UBOOL ACoverLink::AutoAdjustSlot(INT SlotIdx, UBOOL bOnlyCheckLeans)
{
	if( SlotIdx < 0 || SlotIdx >= Slots.Num() )
	{
		return FALSE;
	}



	UBOOL bResult = FALSE;
	FCoverSlot &Slot = Slots(SlotIdx);
	FVector SlotLocation = GetSlotLocation(SlotIdx);
	BYTE OldCoverType = Slot.CoverType;
	FCheckResult CheckResult;
	FRotationMatrix RotMatrix(Rotation);
	FVector CylExtent = GetCylinderExtent();

	TArray<AActor*> ActorsCollisionChangedFor;
	// if we're not pathbuilding, we need to flip stuff near this coverslot into pathbuilding collision manually
#if WITH_EDITOR
	if ( !FPathBuilder::IsBuildingPaths() && GIsEditor && !GIsGame )
	{
		FMemMark Mark( GMainThreadMemStack );
		FCheckResult* ActorResult =
			GWorld->Hash->ActorRadiusCheck(
			GMainThreadMemStack,	// Memory stack
			SlotLocation,				// Location to test
			CylExtent.X*2.0f,					// Radius
			TRACE_AllBlocking );			// Flags

		for( FCheckResult* HitResult = ActorResult ; HitResult ; HitResult = HitResult->GetNext() )
		{
			AActor* CurActor = HitResult->Actor;
			if( CurActor != NULL && !CurActor->bPathTemp )
			{
				CurActor->SetCollisionForPathBuilding(TRUE);
				ActorsCollisionChangedFor.AddItem(CurActor);
			}
		}

		Mark.Pop();
	}
#endif

	// determine what the height of this node is
	// by checking the normal height of the node
	if( !GIsGame || !bOnlyCheckLeans )
	{
		// first move this slot down to ground level
		// @laurent - disabled for second pass, because for height, we need to take into account width of cylinder (for slopes). But that is unsafe for first pass.
		// So we let positioning being entirely done during first pass.
		if( !Slot.bForceNoGroundAdjust && !bOnlyCheckLeans )
		{
			if( !GWorld->SingleLineCheck(CheckResult, this, SlotLocation-FVector(0.f,0.f,4.f*AlignDist), SlotLocation, TRACE_World, FVector(1.f,1.f,1.f)))
			{
				//DrawDebugCoordinateSystem(CheckResult.Location,GetSlotRotation(SlotIdx),50.f,TRUE);
				Slot.LocationOffset = RotMatrix.InverseTransformFVectorNoScale(CheckResult.Location + FVector(0.f,0.f,CylExtent.Z) - Location);
				SlotLocation = GetSlotLocation(SlotIdx);
			}
		}

		if( Slot.ForceCoverType == CT_None )
		{
			// Check for mid to standing
			FVector CheckDir = (Rotation + Slot.RotationOffset).Vector();
			FVector CheckLoc = SlotLocation;
			CheckLoc.Z += -CylExtent.Z + MidHeight;
			FLOAT CheckDist = StandHeight - MidHeight;
			UBOOL bHit = TRUE;
			while (CheckDist > 0 &&
				bHit)
			{
				bHit = !GWorld->SingleLineCheck(CheckResult,
					this,
					CheckLoc + CheckDir * (AlignDist*4.f),
					CheckLoc,
					TRACE_AllBlocking | TRACE_ComplexCollision);
				CheckDist -= 16.f;
				CheckLoc.Z += 16.f;
			}
			// if we found a gap, assume mid level cover
			if (!bHit)
			{
				// if this is in game
				if (GIsGame)
				{
					// check for the cover being removed entirely
					if ( GWorld->SingleLineCheck(CheckResult,
						this,
						SlotLocation + CheckDir * (AlignDist*4.f),
						SlotLocation,
						TRACE_World,
						FVector(1.f)))
					{
						// no cover in front of this slot, so disable
						Slot.bEnabled = FALSE;
					}
				}
				Slot.CoverType = CT_MidLevel;
			}
			else
			{
				// otherwise it's full standing cover
				//debugf(TEXT("link %s slot %d hit %s for standing"),*GetName(),SlotIdx,*CheckResult.Actor->GetName());
				Slot.CoverType = CT_Standing;
			}
		}
		else
		{
			Slot.CoverType = Slot.ForceCoverType;
		}

		// if we changed the cover type indicate that in the return value
		bResult = (OldCoverType != Slot.CoverType);
	}

	if( bAutoAdjust )
	{
		if( !bOnlyCheckLeans )
		{
			// orient to the nearest feature by tracing
			// in various directions
			{
				FRotator CheckRotation = Rotation + Slot.RotationOffset;
				FCheckResult BestCheckResult;
				FLOAT Angle = 0.f;
				FVector CheckLocation = GetSlotLocation(SlotIdx);
				CheckLocation.Z -= CylExtent.Z * 0.5f;
				FLOAT CheckDist = AlignDist * 4.f;
				const INT AngleCheckCount = 128;
				Slot.bFailedToFindSurface = FALSE;
				//FlushPersistentDebugLines();
				for (INT Idx = 0; Idx < AngleCheckCount; Idx++)
				{
					Angle += 65536.f/AngleCheckCount * Idx;
					CheckRotation.Yaw += appTrunc(Angle);
					FVector EndLocation = CheckLocation + (CheckRotation.Vector() * CheckDist);
					if (!GWorld->SingleLineCheck(CheckResult,
						this,
						EndLocation,
						CheckLocation,
						TRACE_World,
						FVector(1.f)))
					{
						ConditionalDrawDebugLine(CheckLocation,CheckResult.Location,0,255,0, TRUE);
						ConditionalDrawDebugLine(CheckResult.Location,CheckResult.Location + CheckResult.Normal * 8.f,0,255,0, TRUE);
						FLOAT Rating = 1.f - ((CheckResult.Location - SlotLocation).Size2D()/CheckDist);
						// scale by the current rotation to allow ld's a bit of control
						Rating += -0.5f * (CheckResult.Normal | (GetSlotRotation(SlotIdx).Vector()));
						// favor blocking volume hits more than regular geometery
						if (CheckResult.Actor != NULL &&
							CheckResult.Actor->IsA(ABlockingVolume::StaticClass()))
						{
							Rating *= 1.25f;
						}
						// compare against our best Check, if not set or
						// this Check resulted in a closer hit
						if (Rating > 0.f &&
							(BestCheckResult.Actor == NULL ||
							BestCheckResult.Time < Rating))
						{
							BestCheckResult = CheckResult;
							BestCheckResult.Time = Rating;
						}
					}
				}
				if (BestCheckResult.Actor != NULL)
				{
					// set the rotation based on the hit normal
					FRotator NewRotation = Rotation + Slot.RotationOffset;
					NewRotation.Yaw = (BestCheckResult.Normal * -1).Rotation().Yaw;
					FVector X, Y, Z;
					// attempt to do 2 parallel traces along the new rotation to generate an average surface normal
					FRotationMatrix(NewRotation).GetAxes(X,Y,Z);
					FCheckResult CheckResultA, CheckResultB;
					if (!GWorld->SingleLineCheck(CheckResultA,
						this,
						SlotLocation + (X * (AlignDist*4.f) + Y * 31.f),
						SlotLocation + Y * 31.f,
						TRACE_World,
						FVector(1.f)) &&
						!GWorld->SingleLineCheck(CheckResultB,
						this,
						SlotLocation + (X * (AlignDist*4.f) - Y * 31.f),
						SlotLocation - Y * 31.f,
						TRACE_World,
						FVector(1.f)))
					{
						ConditionalDrawDebugLine(SlotLocation + (X * (AlignDist*4.f) - Y * 31.f),SlotLocation - Y * 31.f,0,0,255,1);
						ConditionalDrawDebugLine(CheckResultA.Location,CheckResultA.Location + CheckResultA.Normal * 256.f,0,128,128,1);
						ConditionalDrawDebugLine(SlotLocation + (X * (AlignDist*4.f) + Y * 31.f),SlotLocation + Y * 31.f,0,0,255,1);
						ConditionalDrawDebugLine(CheckResultB.Location,CheckResultB.Location + CheckResultB.Normal * 256.f,0,128,128,1);
						//NewRotation.Yaw = (((CheckResultA.Normal + CheckResultB.Normal)/2.f) * -1).Rotation().Yaw;
						NewRotation.Yaw = ((CheckResultA.Normal + CheckResultB.Normal) * -1).Rotation().Yaw;
					}

					Slot.RotationOffset = NewRotation - Rotation;
					FVector NewDirection = NewRotation.Vector();

					// Use a line trace to get as close to the wall as we can get					
					if( !GWorld->SingleLineCheck(CheckResult,
						this,
						SlotLocation + (NewDirection * (AlignDist*4.f)),
						SlotLocation,
						TRACE_World,
						FVector(1.f)))
					{
						Slot.LocationOffset = RotMatrix.InverseTransformFVectorNoScale((CheckResult.Location + NewDirection * -AlignDist) - Location);
						SlotLocation = GetSlotLocation(SlotIdx);
					}

					// Place a fake scout at the location and move the scout out until it fits
					FVector ScoutExtent(AlignDist, AlignDist, CylinderComponent->CollisionHeight/2.f);
					FVector ScoutCheckLocation = SlotLocation + FVector(0,0,8);
					INT AdjustCounter = 16;
					UBOOL bFits = FALSE;
					while( !bFits && AdjustCounter-- > 0 )
					{
						FVector OriginalScoutCheckLocation = ScoutCheckLocation;

						//DrawDebugBox(ScoutCheckLocation, ScoutExtent, 0, 255, 0, TRUE);

						// Try to fit scout in check spot
						bFits = GWorld->FindSpot(ScoutExtent, ScoutCheckLocation, FALSE);

						//DrawDebugBox(ScoutCheckLocation, ScoutExtent, 255, 0, 0, TRUE);

						// If couldn't fit
						if( !bFits )
						{
							// Move check spot away from the wall
							ScoutCheckLocation = OriginalScoutCheckLocation - NewDirection * 4.f;
						}
					}

					// Try to trace back to original location.
					// The reason for this is when the slot rotation is not axis aligned, FindSpot() will push it away, 
					// but not away along cover normal, but along world X or Y. So that effectively makes slots slide along cover.
					// So we push the original location away from the wall from the same distance as the newly found location, and we try to move the slot back there by tracing.
					if( !GWorld->SingleLineCheck(CheckResult,
						this,
						ScoutCheckLocation + (NewDirection * (AlignDist*4.f)),
						ScoutCheckLocation,
						TRACE_World,
						FVector(1.f)) )
					{
						FLOAT DistanceToCoverWall = (ScoutCheckLocation - CheckResult.Location).Size();
						FVector AdjustedSourceLocation = SlotLocation + NewDirection * (AlignDist - DistanceToCoverWall);


						if( !GWorld->SingleLineCheck(CheckResult,
							this,
							AdjustedSourceLocation,
							ScoutCheckLocation,
							TRACE_World,
							FVector(1.f)) )
						{
							ScoutCheckLocation = CheckResult.Location; // MT->WTF?
						}
						else
						{
							ScoutCheckLocation = AdjustedSourceLocation;
						}
					}

					// Trace Down to Place Scout on the floor, taking into account its width.
					// Positions slot correctly against cover when on slopes. If we use just a single trace, then slot will sink in floor and be too low or high against the wall.
					// Downside is that collision cylinder is AABB, so it pushes slot a bit further from cover if not axis aligned.
					if( !GWorld->SingleLineCheck(CheckResult, this, ScoutCheckLocation-FVector(0.f,0.f,4.f*AlignDist), ScoutCheckLocation, TRACE_World, FVector(AlignDist, AlignDist, 1.f)) )
					{
						// Catch cases where we couldn't fit the Scout
						if( !bFits || ScoutCheckLocation.Z == CheckResult.Location.Z )
						{
							debugfSuppressed(NAME_DevPath,TEXT("FAILED TO PLACE SCOUT! AdjustCounter:%d"), AdjustCounter);

							// Resort to using a single line trace to put the slot on the ground.
							if( !GWorld->SingleLineCheck(CheckResult, this, ScoutCheckLocation-FVector(0.f,0.f,4.f*AlignDist), ScoutCheckLocation, TRACE_World, FVector(1.f)) )
							{
								ScoutCheckLocation.Z = CheckResult.Location.Z + CylinderComponent->CollisionHeight;
							}
						}
						else
						{
							ScoutCheckLocation.Z = CheckResult.Location.Z + CylinderComponent->CollisionHeight;
						}
					}

					Slot.LocationOffset = RotMatrix.InverseTransformFVectorNoScale(ScoutCheckLocation - Location);
					SlotLocation = GetSlotLocation(SlotIdx);
				}
				else
				{
					Slot.bFailedToFindSurface = TRUE;	
				}
				// limit the pitch/roll
				Slot.RotationOffset.Pitch = 0;
				Slot.RotationOffset.Roll = 0;
			}
		}

		// if dealing with circular cover
		if (bCircular &&
			Slots.Num() >= 2)
		{
			// calculate origin/radius
			FVector A = GetSlotLocation(0), B = GetSlotLocation(1);
			CircularOrigin = (A + B)/2.f;
			CircularRadius = (A - B).Size()/2.f;
			// force rotation to the origin
			Slot.RotationOffset = (CircularOrigin - GetSlotLocation(SlotIdx)).Rotation() - Rotation;
			// and enable leans for both directions
			Slot.bLeanLeft  = TRUE;
			Slot.bLeanRight = TRUE;
		}
		else
		{
			// update the lean left/right flags
			if (LeanTraceDist <= 0.f)
			{
				LeanTraceDist = 64.f;
			}

			const FLOAT LeanCheckDist = LeanTraceDist + AlignDist;

			Slot.bLeanLeft  = FALSE;
			Slot.bLeanRight = FALSE;
			UBOOL bHit;

			// Get cover axes
			FVector X, Y, Z;
			FRotationMatrix(GetSlotRotation(SlotIdx)).GetAxes( X, Y, Z );

			// Get start location for slot traces
			FVector CheckLocation = GetSlotLocation(SlotIdx);
			CheckLocation.Z += GetSlotHeight(SlotIdx) * 0.375f;

			if (IsLeftEdgeSlot(SlotIdx,FALSE))
			{
				// verify that there is no wall to the left
				FVector ViewPt = GetSlotViewPoint( SlotIdx, CT_None, CA_LeanLeft );
				bHit = !GWorld->SingleLineCheck(CheckResult,
					this,
					ViewPt,
					CheckLocation,
					TRACE_World,
					FVector(1.f));
				if (!bHit)
				{
					// verify that we are able to fire forward from the lean location
					bHit = !GWorld->SingleLineCheck( CheckResult,
						this,
						ViewPt + X * LeanCheckDist,
						ViewPt,
						TRACE_AllBlocking | TRACE_ComplexCollision );
					if( !bHit )
					{
						Slot.bLeanLeft = TRUE;
					}
				}
			}
			if (IsRightEdgeSlot(SlotIdx,FALSE))
			{
				FVector ViewPt = GetSlotViewPoint( SlotIdx, CT_None, CA_LeanRight );
				bHit = !GWorld->SingleLineCheck(CheckResult,
					this,
					ViewPt,
					CheckLocation,
					TRACE_World,
					FVector(1.f));
				if (!bHit)
				{
					// verify that we are able to fire forward from the lean location
					bHit = !GWorld->SingleLineCheck( CheckResult,
						this,
						ViewPt + X * LeanCheckDist,
						ViewPt,
						TRACE_AllBlocking | TRACE_ComplexCollision);
					if( !bHit )
					{
						Slot.bLeanRight = TRUE;
					}
				}
			}
		}

		// if in game, and the slot is enabled
		if (GIsGame && Slot.bEnabled)
		{
			// figure out the slip/mantle/swat stuff as well
			BuildSlotInfo(SlotIdx);
		}
	}

	if (GIsGame)
	{
		FPathBuilder::DestroyScout();
	}


#if WITH_EDITOR
	if ( GIsEditor && !GIsGame )
	{
		for(INT ActorIdx=0;ActorIdx<ActorsCollisionChangedFor.Num();++ActorIdx)
		{
			ActorsCollisionChangedFor(ActorIdx)->SetCollisionForPathBuilding(FALSE);
		}
	}
#endif
	return bResult;
}

UBOOL ACoverLink::IsEnabled()
{
	if( bDisabled )
	{
		return FALSE;
	}

	for( INT Idx = 0; Idx < Slots.Num(); Idx++ )
	{
		if( Slots(Idx).bEnabled )
		{
			return TRUE;
		}
	}

	return FALSE;
}

UBOOL ACoverLink::GetSwatTurnTarget( INT SlotIdx, INT Direction, FCoverInfo& out_Info )
{
	FCoverSlot& Slot = Slots(SlotIdx);
	if( Direction == 0 )
	{
		return GetCachedCoverInfo( Slot.GetLeftTurnTargetCoverRefIdx(), out_Info );
	}
	else
	{
		return GetCachedCoverInfo( Slot.GetRightTurnTargetCoverRefIdx(), out_Info );
	}
}

UBOOL ACoverLink::IsEdgeSlot(INT SlotIdx, UBOOL bIgnoreLeans)
{
	// if not circular cover, and
	// if start of list, or left slot is disabled, or end of list, or right slot is disabled
	return (!bLooped && !bCircular && (IsLeftEdgeSlot(SlotIdx,bIgnoreLeans) || IsRightEdgeSlot(SlotIdx,bIgnoreLeans)));
}

UBOOL ACoverLink::IsLeftEdgeSlot(INT SlotIdx, UBOOL bIgnoreLeans)
{
	// if not circular, and
	// is start of list or the left slot is disabled
	return (!bLooped && !bCircular && 
		SlotIdx < Slots.Num() && 
		(SlotIdx <= 0 || !Slots(SlotIdx-1).bEnabled || (!bIgnoreLeans && Slots(SlotIdx-1).CoverType > Slots(SlotIdx).CoverType)));
}

UBOOL ACoverLink::IsRightEdgeSlot(INT SlotIdx, UBOOL bIgnoreLeans)
{
	// if not circular, and
	// is end of list, or right slot is disabled
	return (!bLooped && !bCircular && 
		(SlotIdx == Slots.Num()-1 || SlotIdx >= Slots.Num() || !Slots(SlotIdx+1).bEnabled || (!bIgnoreLeans && Slots(SlotIdx+1).CoverType > Slots(SlotIdx).CoverType)));
}

INT ACoverLink::GetSlotIdxToLeft( INT SlotIdx, INT Cnt )
{
	INT NextSlotIdx = SlotIdx - Cnt;
	if( bLooped )
	{
		while( NextSlotIdx < 0 )
		{
			NextSlotIdx += Slots.Num();
		}		
	}
	return (NextSlotIdx >= 0 && NextSlotIdx < Slots.Num()) ? NextSlotIdx : -1;
}

INT ACoverLink::GetSlotIdxToRight( INT SlotIdx, INT Cnt )
{
	INT NextSlotIdx = SlotIdx + Cnt;
	if( bLooped )
	{
		while( NextSlotIdx >= Slots.Num() )
		{
			NextSlotIdx -= Slots.Num();
		}		
	}
	return (NextSlotIdx >= 0 && NextSlotIdx < Slots.Num()) ? NextSlotIdx : -1;
}

void ACoverLink::ClearExposedFireLinks()
{
	// clear out exposed fire links
	for( INT SlotIdx = 0; SlotIdx < Slots.Num(); SlotIdx++ )
	{
		FCoverSlot &Slot = Slots(SlotIdx);	
		Slot.ExposedCoverPackedProperties.Empty();
	}
}

void ACoverLink::BuildFireLinks( AScout* Scout )
{
	// clear dynamic link info
	DynamicLinkInfos.Reset();
	// For every slot
	for( INT SlotIdx = 0; SlotIdx < Slots.Num(); SlotIdx++ )
	{
		FCoverSlot &Slot = Slots(SlotIdx);		

		// Clear previous links
		Slot.FireLinks.Empty();
		Slot.Actions.Empty();

		// If we can't perform any attacks from this slot
		FFireLinkInfo Info( this, SlotIdx );
		if( Info.Actions.Num() == 0 )
		{
			// Skip it!
			continue;
		}

		// For every other slot 
		for( ANavigationPoint *Nav = GWorld->GetFirstNavigationPoint(); Nav != NULL; Nav = Nav->nextNavigationPoint )
		{
			ACoverLink* TestLink = Cast<ACoverLink>(Nav);
			if( TestLink == NULL )
				continue;

			for( INT TestSlotIdx = 0; TestSlotIdx< TestLink->Slots.Num(); TestSlotIdx++ )
			{
				if( TestLink == this && TestSlotIdx == SlotIdx )
				{
					continue;
				}

				GetFireActions( Info, TestLink, TestSlotIdx );
			}			
		}
	}
}

UBOOL ACoverLink::GetFireActions( FFireLinkInfo& SrcInfo, ACoverLink* TestLink, INT TestSlotIdx, UBOOL bFill/*=TRUE*/ )
{
	if( TestLink == NULL || TestSlotIdx < 0 || TestSlotIdx >= TestLink->Slots.Num() )
	{
		return FALSE;
	}

	//debug
	UBOOL bDebugLines = FALSE;
/*	if( SrcInfo.Slot->bSelected ) 
	{
		FlushPersistentDebugLines();
		bDebugLines = TRUE; 
	} 
*/

	// Setup info for test slot
	FFireLinkInfo DestInfo( TestLink, TestSlotIdx );
	DestInfo.Actions.AddItem( CA_Default ); // Add firing at enemy w/o a cover action

	// If test slot is too far away
	FLOAT SrcToDestDist = (DestInfo.SlotLocation-SrcInfo.SlotLocation).Size();
	if( SrcToDestDist > MaxFireLinkDist )
	{
		// Can't fire at it
		return FALSE;
	}


	// if the destination slot does not have any actions, skip it! people are a lot less likely to be in these slots, and 
	// we will fall back on non-cover action determination in this case for failsafe
	TArray<BYTE> ValidInteractionList;

	FFireLinkInfo DestActions( TestLink, TestSlotIdx );
	if (DestActions.Actions.Num() > 0)
	{

		for( INT SrcTypeIdx = 0; SrcTypeIdx < SrcInfo.Types.Num(); SrcTypeIdx++ )
		{
			BYTE SrcType = SrcInfo.Types(SrcTypeIdx);
			for( INT SrcActionIdx = 0; SrcActionIdx < SrcInfo.Actions.Num(); SrcActionIdx++ )
			{
				BYTE SrcAction = SrcInfo.Actions(SrcActionIdx);
				SrcInfo.Slot->Actions.AddUniqueItem( SrcAction );

				for( INT DestTypeIdx = 0; DestTypeIdx < DestInfo.Types.Num(); DestTypeIdx++ )
				{
					BYTE DestType = DestInfo.Types(DestTypeIdx);
					for( INT DestActionIdx = 0; DestActionIdx < DestInfo.Actions.Num(); DestActionIdx++ )
					{
						BYTE DestAction = DestInfo.Actions(DestActionIdx);

						FVector SrcViewPt  = SrcInfo.Link->GetSlotViewPoint(  SrcInfo.SlotIdx,  SrcType, SrcAction );
						FVector DestViewPt = DestInfo.Link->GetSlotViewPoint( DestInfo.SlotIdx, DestType, DestAction );
						if( CanFireLinkHit( SrcViewPt, DestViewPt, bDebugLines ) )
						{
							ValidInteractionList.AddItem( PackFireLinkInteractionInfo( SrcType, SrcAction, DestType, DestAction ) );
						}					
					}
				}
			}
		}

		if( ValidInteractionList.Num() == 0 )
		{
			return FALSE;
		}
	}



	UBOOL bResult = FALSE;

	FVector SrcToDest		= DestInfo.SlotLocation - SrcInfo.SlotLocation;
	FVector SrcToDestDir	= SrcToDest.SafeNormal();
	FLOAT	FireAngleDot	= SrcInfo.SlotRotation.Vector() | SrcToDestDir;
	FLOAT	FireAngleDist	= SrcInfo.SlotRotation.Vector() | SrcToDest;

	if( FireAngleDot  >= MINFIRELINKANGLE && 
		FireAngleDist >= 128.f )
	{
		bResult = TRUE;

		if( bFill )
		{
			INT LinkIdx	  = SrcInfo.Slot->FireLinks.AddZeroed();
			FFireLink& FL = SrcInfo.Slot->FireLinks(LinkIdx);

			// Set the cross level flag and find/add reference in cache
			FL.SetCoverRefIdx( FindCoverReference( TestLink, TestSlotIdx ) );
			
			FL.UpdateDynamicLinkInfoFor(this, TestLink, TestSlotIdx, SrcInfo.SlotLocation);
			FL.SetFallbackLink( (SrcInfo.SlotRotation.Vector() | SrcToDestDir) < DESIREDMINFIRELINKANGLE );
			for( INT ValidIdx = 0; ValidIdx < ValidInteractionList.Num(); ValidIdx++ )
			{
				FL.Interactions.AddItem( ValidInteractionList(ValidIdx) );
			}
			if( SrcInfo.out_FireLinkIdx != NULL )
			{
				*SrcInfo.out_FireLinkIdx = LinkIdx;
			}			

			// tell the node we can fire on, that it's exposed to us
			if( TestLink != NULL)
			{
				FCoverSlot& Slot = TestLink->Slots(TestSlotIdx);					
				FLOAT ExposedScale = 0.f;

				if( GetExposedInfo( SrcInfo.Link, SrcInfo.SlotIdx, TestLink, TestSlotIdx, ExposedScale ) && ExposedScale > 0.f )
				{
					INT ExpIdx = Slot.ExposedCoverPackedProperties.AddZeroed();
					Slot.SetExposedCoverRefIdx( ExpIdx, TestLink->FindCoverReference( SrcInfo.Link, SrcInfo.SlotIdx ) );
					Slot.SetExposedScale( ExpIdx, (BYTE)(ExposedScale/1.0f * 255) );
				}
			}
		}
	}

	return bResult;
}

/**
  *  Updated DynamicLinkInfos array if source or destination is dynamic
  */
void FFireLink::UpdateDynamicLinkInfoFor(ACoverLink *MyLink, ACoverLink* TestLink, INT InSlotIdx, const FVector&  LastSrcLocation)
{
	if ( MyLink->bDynamicCover || TestLink->bDynamicCover )
	{
		UBOOL bHaveValidIndex = TRUE;

		// if don't already have a DynamicLinkInfos entry, create one
		if ( !IsDynamicIndexInited() )
		{
			const INT NewIndex = MyLink->DynamicLinkInfos.Num();
			if ( NewIndex < 65535 )
			{
				SetDynamicIndexInited( TRUE );
				SetDynamicLinkInfoIndex( NewIndex );
				MyLink->DynamicLinkInfos.AddZeroed();
			}
			else
			{
				bHaveValidIndex = FALSE;
				warnf(TEXT("%s RAN OUT OF DynamicLinkInfo space!!!"), *MyLink->GetName());
			}
		}
		if ( bHaveValidIndex )
		{
			const INT TotalIndex = GetDynamicLinkInfoIndex();
			MyLink->DynamicLinkInfos(TotalIndex).LastTargetLocation = TestLink->GetSlotLocation(InSlotIdx);
			MyLink->DynamicLinkInfos(TotalIndex).LastSrcLocation = LastSrcLocation;
		}
	}
}

FVector FFireLink::GetLastTargetLocation(ACoverLink *MyLink)
{
	if ( !IsDynamicIndexInited() )
	{
		return FVector(0.f);
	}
	const INT TotalIndex = GetDynamicLinkInfoIndex();
	if ( !MyLink || (MyLink->DynamicLinkInfos.Num() <= TotalIndex) )
	{
		return FVector(0.f);
	}
	return MyLink->DynamicLinkInfos(TotalIndex).LastTargetLocation;
}

FVector FFireLink::GetLastSrcLocation(ACoverLink *MyLink)
{
	if ( !IsDynamicIndexInited() )
	{
		return FVector(0.f);
	}
	const INT TotalIndex = GetDynamicLinkInfoIndex();
	if ( !MyLink || (MyLink->DynamicLinkInfos.Num() <= TotalIndex) )
	{
		return FVector(0.f);
	}
	return MyLink->DynamicLinkInfos(TotalIndex).LastSrcLocation;
}

void ACoverLink::BuildOtherLinks( AScout* Scout )
{
}

UBOOL ACoverLink::GetExposedInfo( ACoverLink* SrcLink, INT SrcSlotIdx, ACoverLink* DestLink, INT DestSlotIdx, FLOAT& out_ExposedScale )
{
	if( (SrcLink == NULL || SrcSlotIdx < 0 || SrcSlotIdx >= SrcLink->Slots.Num()) || 
		(DestLink == NULL || DestSlotIdx < 0 || DestSlotIdx >= DestLink->Slots.Num()) )
	{
		return FALSE;
	}

	// Check to see if are exposed by check cover
	FVector VectToChk = (SrcLink->GetSlotLocation(SrcSlotIdx) - DestLink->GetSlotLocation(DestSlotIdx) );
	FLOAT	DistSq = VectToChk.SizeSquared();
	VectToChk.Normalize();

	if( DistSq > MaxFireLinkDist * MaxFireLinkDist )
	{
		return FALSE;
	}

	FRotationMatrix RotMatrix(DestLink->GetSlotRotation(DestSlotIdx));
	FVector X, Y, Z;
	RotMatrix.GetAxes( X, Y, Z );

	// Determine the angle we want to use
	// If slot is too far to the side of an edge slot, increase the valid exposure angle
	FLOAT		TestDot	  = UCONST_COVERLINK_ExposureDot;
	FLOAT		YDot = (Y | VectToChk);
	FCoverSlot& DestSlot  = DestLink->Slots(DestSlotIdx);
	if( (DestSlot.bLeanLeft  && YDot < -UCONST_COVERLINK_EdgeCheckDot) ||
		(DestSlot.bLeanRight && YDot >  UCONST_COVERLINK_EdgeCheckDot) )
	{
		TestDot = UCONST_COVERLINK_EdgeExposureDot;
	}

	FLOAT XDot = (X | VectToChk);
	if( XDot <= TestDot )
	{
		// If threat is still in front of the slot
		if( XDot > 0.f )
		{
			// Scale exposure danger
			out_ExposedScale = 1.f - (XDot / TestDot);
		}
		else
		{
			// Otherwise, threat has a great flank = max exposure
			out_ExposedScale = 1.f;
		}

		// If threat is farther than half the max firelink distance
		FLOAT Dist	  = (DestLink->GetSlotLocation(DestSlotIdx) - SrcLink->GetSlotLocation(SrcSlotIdx)).Size();
		FLOAT HalfMax = MaxFireLinkDist/2.f;
		if( Dist > HalfMax )
		{
			// Scale exposure down by distance
			out_ExposedScale *= 1.f - ((Dist - HalfMax) / HalfMax);
		}

		return TRUE;
	}

	return FALSE;
}


UBOOL ACoverLink::IsFireLinkValid( INT SlotIdx, INT FireLinkIdx, BYTE ArrayID ) 
{
	FCoverInfo DestInfo;
	if( !GetFireLinkTargetCoverInfo( SlotIdx, FireLinkIdx, DestInfo, ArrayID ) )
	{
		return FALSE;
	}
	FCoverSlot& Slot	 = Slots(SlotIdx);
	FFireLink&  FireLink = Slot.GetFireLinkRef( FireLinkIdx, ArrayID );

	UBOOL bTargetDynamic = DestInfo.Link->bDynamicCover;
	// If both src link and target link are static cover - link is always valid
	if( !bDynamicCover && !bTargetDynamic )
	{
		return TRUE;
	}

	const FLOAT Thresh = InvalidateDistance * InvalidateDistance;

	// If target is dynamic cover
	if( bTargetDynamic )
	{
		// Get distance from last valid target slot location
		FLOAT DistSq = (FireLink.GetLastTargetLocation(this) - DestInfo.Link->GetSlotLocation(DestInfo.SlotIdx)).SizeSquared();
		// Invalid if target slot is outside acceptable range
		if( DistSq > Thresh )
		{
			return FALSE;
		}
	}

	// If source is dynamic cover
	if( bDynamicCover )
	{
		// Get distance from  last valid src slot location
		FLOAT DistSq = (FireLink.GetLastSrcLocation(this) - GetSlotLocation(SlotIdx)).SizeSquared();
		// Invalid if src slot is outside acceptable range
		if( DistSq > Thresh )
		{
			return FALSE;
		}
	}

	// Everything is close enough to remain valid
	return TRUE;
}


/**
* Searches for a fire link to the specified cover/slot and returns the cover actions.
*/
UBOOL ACoverLink::GetFireLinkTo( INT SlotIdx, const FCoverInfo& ChkCover, BYTE ChkAction, BYTE ChkType, INT& out_FireLinkIdx, TArray<INT>& out_Items )
{
	UBOOL bFound  = FALSE;
	UBOOL bResult = FALSE;
	FCoverSlot& Slot = Slots(SlotIdx);
	FVector SrcSlotLocation = GetSlotLocation(SlotIdx);

	// If slot has rejected links
	if( Slot.RejectedFireLinks.Num() )
	{
		// Fail if already rejected
		for( INT Idx = 0; Idx < Slot.RejectedFireLinks.Num(); Idx++ )
		{
			FCoverInfo DestInfo;
			if( !GetFireLinkTargetCoverInfo( SlotIdx, Idx, DestInfo, FLI_RejectedFireLink ) )
			{
				continue;
			}

			if( DestInfo.Link == ChkCover.Link &&
				DestInfo.SlotIdx == ChkCover.SlotIdx )
			{
				if( IsFireLinkValid( SlotIdx, Idx, FLI_RejectedFireLink ) )
				{
					// Early exit
					return FALSE;
				}
				else
				{
#if !FINAL_RELEASE && !PS3
					//debug
					if( bDebug )
					{
						debugf(TEXT("%s ACoverLink_Dynamic::GetFireLinkTo Invalidate FireLinks"), *GetName() );
					}
#endif

					Slot.FireLinks.Empty();
					Slot.RejectedFireLinks.Empty();
				}
			}
		}
	}

	for( INT FireLinkIdx = 0; FireLinkIdx < Slot.FireLinks.Num(); FireLinkIdx++ )
	{
		FFireLink* FireLink = &Slot.FireLinks(FireLinkIdx);
		
		FCoverInfo DestInfo;
		if( !GetFireLinkTargetCoverInfo( SlotIdx, FireLinkIdx, DestInfo ) )
		{
			continue;
		}

		// If the fire link matches the link, slot, and type
		if( DestInfo.Link == ChkCover.Link &&
			DestInfo.SlotIdx == ChkCover.SlotIdx )
		{
			bFound = TRUE;
			UBOOL bValidLink = IsFireLinkValid( SlotIdx, FireLinkIdx );

			// If fire link info is valid
			// (Always valid for stationary targets... checks validity of dynamic targets)
			if( bValidLink )
			{
				out_FireLinkIdx = FireLinkIdx;
				for( INT ItemIdx = 0; ItemIdx < FireLink->Interactions.Num(); ItemIdx++ )
				{
					BYTE Interaction = FireLink->Interactions(ItemIdx);
					if( (ChkAction == CA_Default || FireLinkInteraction_UnpackDestAction(Interaction) == ChkAction) &&
						(ChkType   == CT_None	 || FireLinkInteraction_UnpackDestType(Interaction)   == ChkType)	)
					{
						out_Items.AddItem(ItemIdx);
					}
				}
				// Success if actions exist
				bResult = (out_Items.Num() > 0);
			}
			// Otherwise, if not valid
			else
			{
				// Update LastLinkLocation
				FireLink->UpdateDynamicLinkInfoFor(this, ChkCover.Link, ChkCover.SlotIdx, SrcSlotLocation);

				// Remove all actions from the fire link
				FireLink->Interactions.Empty();

				// Try to find a link to that slot
				FFireLinkInfo Info( this, SlotIdx );
				if( GetFireActions( Info, ChkCover.Link, ChkCover.SlotIdx ) )
				{
					// Remove the old fire link
					Slot.FireLinks.Remove( FireLinkIdx--, 1 );

					// Get created fire link
					FFireLink* NewLink = &Slot.FireLinks(Slot.FireLinks.Num()-1);

					out_FireLinkIdx = Slot.FireLinks.Num() - 1;
					for( INT ItemIdx = 0; ItemIdx < NewLink->Interactions.Num(); ItemIdx++ )
					{
						BYTE Interaction = NewLink->Interactions(ItemIdx);
						if( (ChkAction == CA_Default || FireLinkInteraction_UnpackDestAction(Interaction) == ChkAction) &&
							(ChkType   == CT_None	 || FireLinkInteraction_UnpackDestType(Interaction)   == ChkType)	)
						{
							out_Items.AddItem(ItemIdx);
						}
					}					
				}

				// Success if new actions found
				bResult = (out_Items.Num()>0);
			}

			break;
		}
	}

	// If a link was not found and 
	// this link is stationary while the target is dynamic
	if( !bFound )
	{
		FFireLinkInfo Info( this, SlotIdx );

		if( !bDynamicCover &&
			ChkCover.Link->bDynamicCover )
		{
			// Handles the case where a dynamic link could potentially move into view of a stationary link

			// Try to find a link to the slot
			bResult = GetFireActions( Info, ChkCover.Link, ChkCover.SlotIdx );

			// If found a link
			if( bResult )
			{
				FFireLink* NewLink = &Slot.FireLinks(Slot.FireLinks.Num()-1);

				out_FireLinkIdx = Slot.FireLinks.Num() - 1;
				for( INT ItemIdx = 0; ItemIdx < NewLink->Interactions.Num(); ItemIdx++ )
				{
					BYTE Interaction = NewLink->Interactions(ItemIdx);
					if( (ChkAction == CA_Default || FireLinkInteraction_UnpackDestAction(Interaction) == ChkAction) &&
						(ChkType   == CT_None	 || FireLinkInteraction_UnpackDestType(Interaction)   == ChkType)	)
					{
						out_Items.AddItem(ItemIdx);
					}
				}	
				// Success if new actions found
				bResult = (out_Items.Num()>0);
			}
			// If no link found
			else
			{
				// Add fire link w/ no actions
				INT LinkIdx = Slot.FireLinks.AddZeroed();
				FFireLink& FL = Slot.FireLinks(LinkIdx);

				// Set the cross level flag and find/add reference in cache
				FL.SetCoverRefIdx( FindCoverReference( ChkCover.Link, ChkCover.SlotIdx ) );
				
				FL.UpdateDynamicLinkInfoFor(this, ChkCover.Link, ChkCover.SlotIdx, SrcSlotLocation);
			}
		}
		else
			// Otherwise, if this is dynamic cover
			// Try to reacquire the target link (it's NOT yet in rejected list or we'd never get here)
			if( bDynamicCover )
			{
				// Try to find a link to that slot
				bResult = GetFireActions( Info, ChkCover.Link, ChkCover.SlotIdx );
				// If succeeded
				if( bResult )
				{
					FFireLink* NewLink = &Slot.FireLinks(Slot.FireLinks.Num()-1);
					out_FireLinkIdx = Slot.FireLinks.Num() - 1;
					for( INT ItemIdx = 0; ItemIdx < NewLink->Interactions.Num(); ItemIdx++ )
					{
						BYTE Interaction = NewLink->Interactions(ItemIdx);
						if( (ChkAction == CA_Default || FireLinkInteraction_UnpackDestAction(Interaction) == ChkAction) &&
							(ChkType   == CT_None	 || FireLinkInteraction_UnpackDestType(Interaction)   == ChkType)	)
						{
							out_Items.AddItem(ItemIdx);
						}
					}	
					// Success if new actions found
					bResult = (out_Items.Num()>0);
				}
				// Otherwise, if failed
				else
				{
					// Add to the rejected list
					INT Idx = Slot.RejectedFireLinks.AddZeroed();
					FFireLink& Reject = Slot.RejectedFireLinks(Idx);

					// Set the cross level flag and find/add reference in cache
					Reject.SetCoverRefIdx( FindCoverReference( ChkCover.Link, ChkCover.SlotIdx ) );
					
					Reject.UpdateDynamicLinkInfoFor(this, ChkCover.Link, ChkCover.SlotIdx, SrcSlotLocation);
				}
			}
	}

	return bResult;
}

void ACoverLink::execGetFireLinkTo(FFrame &Stack,RESULT_DECL)
{
	P_GET_INT(SlotIdx);
	P_GET_STRUCT(struct FCoverInfo,ChkCover);
	P_GET_BYTE(ChkAction);
	P_GET_BYTE(ChkType);
	P_GET_INT_REF(out_FireLinkIdx);
	P_GET_TARRAY_REF(INT,out_Items);
	P_FINISH;
	*(UBOOL*)Result = GetFireLinkTo( SlotIdx, ChkCover, ChkAction, ChkType, out_FireLinkIdx, out_Items );
}

/**
* Searches for a valid fire link to the specified cover/slot.
*/
UBOOL ACoverLink::HasFireLinkTo( INT SlotIdx, const FCoverInfo &ChkCover, UBOOL bAllowFallbackLinks )
{
	FCoverSlot &Slot = Slots(SlotIdx);
	for (INT FireLinkIdx = 0; FireLinkIdx < Slot.FireLinks.Num(); FireLinkIdx++)
	{
		FFireLink &FireLink = Slot.FireLinks(FireLinkIdx);

		FCoverInfo DestInfo;
		if( GetFireLinkTargetCoverInfo( SlotIdx, FireLinkIdx, DestInfo ) )
		{
			if( DestInfo.Link == ChkCover.Link && 
				DestInfo.SlotIdx == ChkCover.SlotIdx &&
				(bAllowFallbackLinks || !FireLink.IsFallbackLink()) )
			{
				return (FireLink.Interactions.Num() > 0);
			}
		}
	}

	return FALSE;
}

void ACoverLink::execHasFireLinkTo(FFrame &Stack,RESULT_DECL)
{
	P_GET_INT(SlotIdx);
	P_GET_STRUCT(FCoverInfo,ChkCover);
	P_GET_UBOOL_OPTX(bAllowFallbackLinks,FALSE);
	P_FINISH;
	*(UBOOL*)Result = HasFireLinkTo( SlotIdx, ChkCover, bAllowFallbackLinks );
}

UBOOL ACoverLink::IsExposedTo( INT SlotIdx, FCoverInfo ChkCover, FLOAT& out_ExposedScale )
{
	FCoverSlot& Slot = Slots(SlotIdx);
	for( INT Idx = 0; Idx < Slot.ExposedCoverPackedProperties.Num(); Idx++ )
	{
		FCoverInfo DestInfo;
		if( !GetCachedCoverInfo( Slot.GetExposedCoverRefIdx(Idx), DestInfo ) )
		{
			continue;
		}

		if( DestInfo.Link	 == ChkCover.Link && 
			DestInfo.SlotIdx == ChkCover.SlotIdx )
		{
			out_ExposedScale *= (FLOAT(Slot.GetExposedScale(Idx)) / 255.0);
			return TRUE;
		}
	}

	return FALSE;
}

void ACoverLink::GetSlotActions(INT SlotIdx, TArray<BYTE> &Actions)
{
	if (SlotIdx >= 0 && SlotIdx < Slots.Num())
	{
		FCoverSlot &Slot = Slots(SlotIdx);
		if (Slot.bLeanRight)
		{
			Actions.AddItem(CA_PeekRight);
		}
		if (Slot.bLeanLeft)
		{
			Actions.AddItem(CA_PeekLeft);
		}
		if (Slot.CoverType == CT_MidLevel && Slot.bAllowPopup)
		{
			Actions.AddItem(CA_PeekUp);
		}
	}
}

static INT OverlapCount = 0;
struct FOverlapCounter
{
	FOverlapCounter()  { OverlapCount++; }
	~FOverlapCounter() { OverlapCount--; }
};

UBOOL ACoverLink::IsOverlapSlotClaimed( APawn *ChkClaim, INT SlotIdx, UBOOL bSkipTeamCheck )
{
	FOverlapCounter OverlapCounter;

	FCoverSlot& Slot = Slots(SlotIdx);
	for( INT Idx = 0; Idx < Slot.OverlapClaimsList.Num(); Idx++ )
	{
		FCoverInfo& OverInfo = Slot.OverlapClaimsList(Idx);
		if(  OverInfo.Link != NULL && 
			!OverInfo.Link->IsValidClaim( ChkClaim, OverInfo.SlotIdx, bSkipTeamCheck, TRUE ) )
		{
			return TRUE;
		}			
	}

	return FALSE;
}

UBOOL ACoverLink::IsValidClaimBetween( APawn* ChkClaim, INT StartSlotIdx, INT EndSlotIdx, UBOOL bSkipTeamCheck, UBOOL bSkipOverlapCheck )
{
	const INT Dir = (StartSlotIdx < EndSlotIdx) ? 1 : -1;

	INT SlotIdx = StartSlotIdx;
	do
	{
		if( !IsValidClaim( ChkClaim, SlotIdx, bSkipTeamCheck, bSkipOverlapCheck ) )
		{
			return FALSE;
		}
		SlotIdx += Dir;
	} while( SlotIdx != EndSlotIdx );

	return TRUE;
}

UBOOL ACoverLink::IsValidClaim( APawn *ChkClaim, INT SlotIdx, UBOOL bSkipTeamCheck, UBOOL bSkipOverlapCheck )
{
	// early out if it's an invalid slot or the slot/link is disabled
	if( !IsEnabled() || ChkClaim == NULL || SlotIdx < 0 || SlotIdx >= Slots.Num() || !Slots(SlotIdx).bEnabled )
	{
		// If inside overlap check - don't invalidate claim when overlapping a disabled slot
		return (OverlapCount > 0);
	}

	// If the slot is already held by the controller or it is empty (and we accept empty slots) - valid claim
	APawn* SlotOwner = Slots(SlotIdx).SlotOwner;
	UBOOL bSlotOwnerVacantOrLessImportant = ( SlotOwner == ChkClaim || SlotOwner == NULL || SlotOwner->bDeleteMe ||
						((SlotOwner->PlayerReplicationInfo == NULL || SlotOwner->PlayerReplicationInfo->bBot) && ChkClaim->PlayerReplicationInfo != NULL && !ChkClaim->PlayerReplicationInfo->bBot) );
	
	UBOOL bHumanControlled = ChkClaim->IsHumanControlled();

	if( ! bHumanControlled  && (bPlayerOnly || Slots(SlotIdx).bPlayerOnly) )
	{
		return FALSE;
	}

	UBOOL bResult = bSlotOwnerVacantOrLessImportant && 
		(bHumanControlled || !bBlocked) && 
		(GWorld->GetTimeSeconds() >= Slots(SlotIdx).SlotValidAfterTime);

	// If we have a valid claim so far and the controller has a pawn
	if( bResult && ChkClaim != NULL )
	{
		// If we need to make sure ALL cover slots are valid by team
		if( !bSkipTeamCheck )
		{
			// Go through the claims list
			for( INT Idx = 0; Idx < Claims.Num() && bResult; Idx++ )
			{
				APawn* C = Claims(Idx);
				if( C == NULL)
				{
					Claims.Remove(Idx--,1);
				}
				else
					// And make sure all the other claims are on the same team
					if( C != NULL && !C->bDeleteMe &&
						(ChkClaim->PlayerReplicationInfo != NULL && C->PlayerReplicationInfo != NULL && C->PlayerReplicationInfo->Team != ChkClaim->PlayerReplicationInfo->Team))
					{
						// If not on same team - invalid claim
						bResult = FALSE;
						break;
					}
			}
		}
	}

	if( bResult && !bSkipOverlapCheck )
	{
		if( IsOverlapSlotClaimed( ChkClaim, SlotIdx, bSkipTeamCheck ) )
		{
			bResult = FALSE;
		}
	}

	return bResult;
}

#if WITH_EDITOR
void ACoverGroup::CheckForErrors()
{
	Super::CheckForErrors();
	TArray<INT> ContainedNetworkIDs;
	for (INT Idx = 0; Idx < CoverLinkRefs.Num(); Idx++)
	{
		ACoverLink *Link = Cast<ACoverLink>(~CoverLinkRefs(Idx));
		if (Link != NULL)
		{
			ContainedNetworkIDs.AddUniqueItem(Link->NetworkID);
		}
	}
	if (ContainedNetworkIDs.Num() > 1)
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_ReferencesLinksInDifferentNavNetwork" ), *GetName() ) ), TEXT( "ReferencesLinksInDifferentNavNetwork" ) );
	}
}
#endif

void ACoverGroup::EnableGroup()
{
	for( INT Idx = 0; Idx < CoverLinkRefs.Num(); Idx++ )
	{
		ACoverLink* Link = Cast<ACoverLink>(CoverLinkRefs(Idx).Nav());
		if( Link )
		{
			Link->eventSetDisabled(FALSE);
		}
		else
		{
			CoverLinkRefs.Remove( Idx--, 1 );
		}
	}
}

void ACoverGroup::DisableGroup()
{
	for( INT Idx = 0; Idx < CoverLinkRefs.Num(); Idx++ )
	{
		ACoverLink* Link = Cast<ACoverLink>(CoverLinkRefs(Idx).Nav());
		if( Link )
		{
			Link->eventSetDisabled(TRUE);
		}
		else
		{
			CoverLinkRefs.Remove( Idx--, 1 );
		}
	}
}

void ACoverGroup::ToggleGroup()
{
	for( INT Idx = 0; Idx < CoverLinkRefs.Num(); Idx++ )
	{
		ACoverLink* Link = Cast<ACoverLink>(CoverLinkRefs(Idx).Nav());
		if( Link )
		{
			Link->eventSetDisabled(!Link->bDisabled);
		}
		else
		{
			CoverLinkRefs.Remove( Idx--, 1 );
		}
	}
}

void ACoverGroup::PostLoad()
{
	Super::PostLoad();
}

void ACoverGroup::GetActorReferences(TArray<FActorReference*> &ActorRefs, UBOOL bIsRemovingLevel)
{
	Super::GetActorReferences(ActorRefs,bIsRemovingLevel);
	for (INT Idx = 0; Idx < CoverLinkRefs.Num(); Idx++)
	{
		FActorReference &ActorRef = CoverLinkRefs(Idx);
		if (ActorRef.Guid.IsValid())
		{
			if ((bIsRemovingLevel && ActorRef.Actor != NULL) ||
				(!bIsRemovingLevel && ActorRef.Actor == NULL))
			{
				ActorRefs.AddItem(&ActorRef);
			}
		}
	}
}

void ACoverGroup::AutoFillGroup( ECoverGroupFillAction CGFA, TArray<ACoverLink*>& Links )
{
	// If overwriting or clearing or filling by cylinder
	if( CGFA == CGFA_Overwrite || 
		CGFA == CGFA_Clear || 
		(CGFA != CGFA_Remove && CGFA != CGFA_Add) )
	{
		// Empty list
		CoverLinkRefs.Empty();
	}

	// If overwriting or adding selected items
	if( CGFA == CGFA_Overwrite || CGFA == CGFA_Add )
	{
		for( INT Idx = 0; Idx < Links.Num(); Idx++ )
		{
			// Add to the list
			CoverLinkRefs.AddUniqueItem( FActorReference(Links(Idx),*Links(Idx)->GetGuid()) );
		}
	}
	else
		// Otherwise, if removing selected items
		if( CGFA == CGFA_Remove )
		{
			// Go through each cover link
			for( INT Idx = 0; Idx < Links.Num(); Idx++ )
			{
				// Remove from the list
				for (INT RefIdx = 0; RefIdx < CoverLinkRefs.Num(); RefIdx++)
				{
					if (CoverLinkRefs(RefIdx).Actor ==  Links(Idx) ||
						CoverLinkRefs(RefIdx).Guid  == *Links(Idx)->GetGuid())
					{
						CoverLinkRefs.Remove(RefIdx--,1);
						break;
					}
				}
			}
		}
		else 
			// Otherwise, fill by cylinder
			if( CGFA == CGFA_Cylinder )
			{
				FLOAT RadiusSq = AutoSelectRadius * AutoSelectRadius;
				for( FActorIterator It; It; ++It )
				{
					ANavigationPoint *Nav = Cast<ANavigationPoint>(*It);
					if ( !Nav )
					{
						continue;
					}

					ACoverLink* Link = Cast<ACoverLink>(Nav);
					if( !Link )
					{
						continue;
					}

					FVector LinkToGroup = Link->Location - Location;
					// If link is outside the vertical range of cylinder
					if( AutoSelectHeight > 0 )
					{
						if( Link->Location.Z > Location.Z ||
							LinkToGroup.Z < -AutoSelectHeight )
						{
							continue;
						}
					}
					else
					{
						if( Link->Location.Z < Location.Z ||
							LinkToGroup.Z > -AutoSelectHeight )
						{
							continue;
						}
					}

					// If link is outside lateral range of cylinder
					FLOAT DistSq2D = LinkToGroup.SizeSquared2D();
					if( DistSq2D > RadiusSq )
					{
						continue;
					}

					// Link is within cylinder
					// Add to list of links
					CoverLinkRefs.AddUniqueItem( FActorReference(Link,*Link->GetGuid()) );
				}
			}

			ForceUpdateComponents(FALSE,FALSE);
}

/**
* @return	TRUE if the elements of the specified vector have the same value, within the specified tolerance.
*/
static inline UBOOL AllComponentsEqual(const FVector& Vec, FLOAT Tolerance=KINDA_SMALL_NUMBER)
{
	return Abs( Vec.X - Vec.Y ) < Tolerance && Abs( Vec.X - Vec.Z ) < Tolerance && Abs( Vec.Y - Vec.Z ) < Tolerance;
}

#if WITH_EDITOR
void ACoverGroup::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	const FVector ModifiedScale = DeltaScale * 500.0f;

	if ( bCtrlDown )
	{
		// CTRL+Scaling modifies AutoSelectHeight.  This is for convenience, so that height
		// can be changed without having to use the non-uniform scaling widget (which is
		// inaccessable with spacebar widget cycling).
		AutoSelectHeight += ModifiedScale.X;
		AutoSelectHeight = Max( 0.0f, AutoSelectHeight );
	}
	else
	{
		AutoSelectRadius += ModifiedScale.X;
		AutoSelectRadius = Max( 0.0f, AutoSelectRadius );

		// If non-uniformly scaling, Z scale affects height and Y can affect radius too.
		if ( !AllComponentsEqual(ModifiedScale) )
		{
			AutoSelectHeight += -ModifiedScale.Z;
			AutoSelectHeight = Max( 0.0f, AutoSelectHeight );

			AutoSelectRadius += ModifiedScale.Y;
			AutoSelectRadius = Max( 0.0f, AutoSelectRadius );
		}
	}
}
#endif
