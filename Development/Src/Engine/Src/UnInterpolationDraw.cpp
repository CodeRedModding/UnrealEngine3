/*=============================================================================
	UnInterpolationDraw.cpp: Code for supporting interpolation of properties in-game.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "UnInterpolationHitProxy.h"

static const INT	KeyHalfTriSize = 6;
static const FColor KeyNormalColor(0,0,0);
static const FColor KeyCurveColor(100,0,0);
static const FColor KeyLinearColor(0,100,0);
static const FColor KeyConstantColor(0,0,100);
static const FColor	KeySelectedColor(255,128,0);
static const FColor KeyLabelColor(225,225,225);
static const INT	KeyVertOffset = 5;

static const FLOAT	DrawTrackTimeRes = 0.1f;
static const FLOAT	CurveHandleScale = 0.5f;

/*-----------------------------------------------------------------------------
  UInterpTrack
-----------------------------------------------------------------------------*/

void UInterpTrack::DrawTrack(FRenderInterface* RI, INT TrackIndex, INT TrackWidth, INT TrackHeight, FLOAT StartTime, FLOAT PixelsPerSec, TArray<class FInterpEdSelKey>& SelectedKeys)
{
	UBOOL bHitTesting = RI->IsHitTesting();

	INT NumKeys = GetNumKeyframes();

	UInterpGroup* Group = CastChecked<UInterpGroup>(GetOuter());

	for(INT i=0; i<NumKeys; i++)
	{
		FLOAT KeyTime = GetKeyframeTime(i);

		INT PixelPos = (KeyTime - StartTime) * PixelsPerSec;

		FIntPoint A(PixelPos - KeyHalfTriSize,	TrackHeight - KeyVertOffset);
		FIntPoint B(PixelPos + KeyHalfTriSize,	TrackHeight - KeyVertOffset);
		FIntPoint C(PixelPos,					TrackHeight - KeyVertOffset - KeyHalfTriSize);

		UBOOL bKeySelected = false;
		for(INT j=0; j<SelectedKeys.Num() && !bKeySelected; j++)
		{
			if( SelectedKeys(j).Group == Group && 
				SelectedKeys(j).TrackIndex == TrackIndex && 
				SelectedKeys(j).KeyIndex == i )
				bKeySelected = true;
		}

		FColor KeyColor = GetKeyframeColor(i);
		
		if(bHitTesting) RI->SetHitProxy( new HInterpTrackKeypointProxy(Group, TrackIndex, i) );
		if(bKeySelected)
		{
			RI->DrawTriangle2D( A+FIntPoint(-2,1), 0.f, 0.f, B+FIntPoint(2,1), 0.f, 0.f, C+FIntPoint(0,-2), 0.f, 0.f, KeySelectedColor );
		}

		RI->DrawTriangle2D( A, 0.f, 0.f, B, 0.f, 0.f, C, 0.f, 0.f, KeyColor );
		if(bHitTesting) RI->SetHitProxy( NULL );
	}
}

FColor UInterpTrack::GetKeyframeColor(INT KeyIndex)
{
	return KeyNormalColor;
}

/*-----------------------------------------------------------------------------
  UInterpTrackMove
-----------------------------------------------------------------------------*/

FColor UInterpTrackMove::GetKeyframeColor(INT KeyIndex)
{
	if( KeyIndex < 0 || KeyIndex >= PosTrack.Points.Num() )
	{
		return KeyNormalColor;
	}

	if( PosTrack.Points(KeyIndex).IsCurveKey() )
	{
		return KeyCurveColor;
	}
	else if( PosTrack.Points(KeyIndex).InterpMode == CIM_Linear )
	{
		return KeyLinearColor;
	}
	else
	{
		return KeyConstantColor;
	}
}

void UInterpTrackMove::Render3DTrack(UInterpTrackInst* TrInst, 
									 const FSceneContext& Context, 
									 FPrimitiveRenderInterface* PRI, 
									 INT TrackIndex, 
									 const FColor& TrackColor, 
									 TArray<class FInterpEdSelKey>& SelectedKeys)
{
	// Draw nothing if no points.
	if(PosTrack.Points.Num() == 0)
		return;

	UBOOL bHitTesting = PRI->IsHitTesting();

	UInterpGroup* Group = CastChecked<UInterpGroup>(GetOuter());

	FVector OldKeyPos(0);
	FLOAT OldKeyTime = 0.f;

	for(INT i=0; i<PosTrack.Points.Num(); i++)
	{
		FLOAT NewKeyTime = PosTrack.Points(i).InVal;

		FVector NewKeyPos(0);
		FRotator NewKeyRot(0,0,0);
		GetLocationAtTime(TrInst, NewKeyTime, NewKeyPos, NewKeyRot);

		// If not the first keypoint, draw a line to the last keypoint.
		if(i>0)
		{
			INT NumSteps = appCeil( (NewKeyTime - OldKeyTime)/DrawTrackTimeRes );
			FLOAT DrawSubstep = (NewKeyTime - OldKeyTime)/NumSteps;

			// Find position on first keyframe.
			FLOAT OldTime = OldKeyTime;

			FVector OldPos(0);
			FRotator OldRot(0,0,0);
			GetLocationAtTime(TrInst, OldKeyTime, OldPos, OldRot);
		
			// For constant interpolation - don't draw ticks - just draw dotted line.
			if(PosTrack.Points(i-1).InterpMode == CIM_Constant)
			{
				PRI->DrawDashedLine(OldPos, NewKeyPos, TrackColor, 20);
			}
			else
			{
				// Then draw a line for each substep.
				for(INT j=1; j<NumSteps+1; j++)
				{
					FLOAT NewTime = OldKeyTime + j*DrawSubstep;

					FVector NewPos(0);
					FRotator NewRot(0,0,0);
					GetLocationAtTime(TrInst, NewTime, NewPos, NewRot);

					PRI->DrawLine(OldPos, NewPos, TrackColor);

					// Don't draw point for last one - its the keypoint drawn above.
					if(j != NumSteps)
					{
						PRI->DrawPoint(NewPos, TrackColor, 3.f);
					}

					OldTime = NewTime;
					OldPos = NewPos;
				}
			}
		}

		OldKeyTime = NewKeyTime;
		OldKeyPos = NewKeyPos;
	}

	// Draw keypoints on top of curve
	for(INT i=0; i<PosTrack.Points.Num(); i++)
	{
		// Find if this key is one of the selected ones.
		UBOOL bKeySelected = false;
		for(INT j=0; j<SelectedKeys.Num() && !bKeySelected; j++)
		{
			if( SelectedKeys(j).Group == Group && 
				SelectedKeys(j).TrackIndex == TrackIndex && 
				SelectedKeys(j).KeyIndex == i )
				bKeySelected = true;
		}

		// Find the time, position and orientation of this Key.
		FLOAT NewKeyTime = PosTrack.Points(i).InVal;

		FVector NewKeyPos(0);
		FRotator NewKeyRot(0,0,0);
		GetLocationAtTime(TrInst, NewKeyTime, NewKeyPos, NewKeyRot);

		UInterpTrackInstMove* MoveTrackInst = CastChecked<UInterpTrackInstMove>(TrInst);
		FMatrix RefTM = GetMoveRefFrame(MoveTrackInst);

		FColor KeyColor = bKeySelected ? KeySelectedColor : TrackColor;

		if(bHitTesting) PRI->SetHitProxy( new HInterpTrackKeypointProxy(Group, TrackIndex, i) );
		PRI->DrawPoint(NewKeyPos, KeyColor, 6.f);

		// If desired, draw directional arrow at each keyframe.
		if(bShowArrowAtKeys)
		{
			FQuat NewKeyQuat = NewKeyRot.Quaternion();
			FMatrix ArrowToWorld(NewKeyPos, NewKeyQuat);
			PRI->DrawDirectionalArrow( FScaleMatrix(FVector(16.f,16.f,16.f)) * ArrowToWorld, KeyColor, 3.f );
		}
		if(bHitTesting) PRI->SetHitProxy( NULL );

		UInterpGroupInst* GrInst = CastChecked<UInterpGroupInst>( TrInst->GetOuter() );
		USeqAct_Interp* Seq = CastChecked<USeqAct_Interp>( GrInst->GetOuter() );
		UInterpGroupInst* FirstGrInst = Seq->FindFirstGroupInst(Group);

		// If a selected key, and this is the 'first' instance of this group, draw handles.
		if(bKeySelected && (GrInst == FirstGrInst))
		{
			FVector ArriveTangent = PosTrack.Points(i).ArriveTangent;
			FVector LeaveTangent = PosTrack.Points(i).LeaveTangent;

			BYTE PrevMode = (i > 0)							? GetKeyInterpMode(i-1) : 255;
			BYTE NextMode = (i < PosTrack.Points.Num()-1)	? GetKeyInterpMode(i)	: 255;

			// If not first point, and previous mode was a curve type.
			if(PrevMode == CIM_CurveAuto || PrevMode == CIM_CurveUser || PrevMode == CIM_CurveBreak)
			{
				FVector HandlePos = NewKeyPos - RefTM.TransformNormal(ArriveTangent * CurveHandleScale);
				PRI->DrawLine(NewKeyPos, HandlePos, FColor(255,255,255));

				if(bHitTesting) PRI->SetHitProxy( new HInterpTrackKeyHandleProxy(Group, TrackIndex, i, true) );
				PRI->DrawPoint(HandlePos, FColor(255,255,255), 5.f);
				if(bHitTesting) PRI->SetHitProxy( NULL );
			}

			// If next section is a curve, draw leaving handle.
			if(NextMode == CIM_CurveAuto || NextMode == CIM_CurveUser || NextMode == CIM_CurveBreak)
			{
				FVector HandlePos = NewKeyPos + RefTM.TransformNormal(LeaveTangent * CurveHandleScale);
				PRI->DrawLine(NewKeyPos, HandlePos, FColor(255,255,255));

				if(bHitTesting) PRI->SetHitProxy( new HInterpTrackKeyHandleProxy(Group, TrackIndex, i, false) );
				PRI->DrawPoint(HandlePos, FColor(255,255,255), 5.f);
				if(bHitTesting) PRI->SetHitProxy( NULL );
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackFloatBase
-----------------------------------------------------------------------------*/

FColor UInterpTrackFloatBase::GetKeyframeColor(INT KeyIndex)
{
	if( KeyIndex < 0 || KeyIndex >= FloatTrack.Points.Num() )
	{
		return KeyNormalColor;
	}

	if( FloatTrack.Points(KeyIndex).IsCurveKey() )
	{
		return KeyCurveColor;
	}
	else if( FloatTrack.Points(KeyIndex).InterpMode == CIM_Linear )
	{
		return KeyLinearColor;
	}
	else
	{
		return KeyConstantColor;
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackEvent
-----------------------------------------------------------------------------*/

void UInterpTrackEvent::DrawTrack(FRenderInterface* RI, INT TrackIndex, INT TrackWidth, INT TrackHeight, FLOAT StartTime, FLOAT PixelsPerSec, TArray<class FInterpEdSelKey>& SelectedKeys)
{
	Super::DrawTrack(RI, TrackIndex, TrackWidth, TrackHeight, StartTime, PixelsPerSec, SelectedKeys);

	for(INT i=0; i<EventTrack.Num(); i++)
	{
		FLOAT KeyTime = EventTrack(i).Time;
		INT PixelPos = (KeyTime - StartTime) * PixelsPerSec;

		INT XL, YL;
		RI->StringSize( GEngine->SmallFont, XL, YL, *(EventTrack(i).EventName) );
		RI->DrawShadowedString( PixelPos + 2, TrackHeight - YL - KeyVertOffset, *(EventTrack(i).EventName), GEngine->SmallFont, KeyLabelColor );
	}
}

/*-----------------------------------------------------------------------------
	UInterpTrackDirector
-----------------------------------------------------------------------------*/

void UInterpTrackDirector::DrawTrack(FRenderInterface* RI, INT TrackIndex, INT TrackWidth, INT TrackHeight, FLOAT StartTime, FLOAT PixelsPerSec, TArray<class FInterpEdSelKey>& SelectedKeys)
{
	UInterpGroup* Group = CastChecked<UInterpGroup>(GetOuter());
	UInterpData* Data = CastChecked<UInterpData>(Group->GetOuter());

	// Draw background coloured blocks for camera sections
	for(INT i=0; i<CutTrack.Num(); i++)
	{
		FLOAT KeyTime = CutTrack(i).Time;

		FLOAT NextKeyTime;
		if(i < CutTrack.Num()-1)
		{
			NextKeyTime = ::Min( CutTrack(i+1).Time, Data->InterpLength );
		}
		else
		{
			NextKeyTime = Data->InterpLength;
		}

		// Find the group we are cutting to.
		INT CutGroupIndex = Data->FindGroupByName( CutTrack(i).TargetCamGroup );

		// If its valid, and its not this track, draw a box over duration of shot.
		if((CutGroupIndex != INDEX_NONE) && (CutTrack(i).TargetCamGroup != Group->GroupName))
		{
			INT StartPixelPos = (KeyTime - StartTime) * PixelsPerSec;
			INT EndPixelPos = (NextKeyTime - StartTime) * PixelsPerSec;

			RI->DrawTile( StartPixelPos, KeyVertOffset, EndPixelPos - StartPixelPos, TrackHeight - 2.f*KeyVertOffset, 0.f, 0.f, 1.f, 1.f, Data->InterpGroups(CutGroupIndex)->GroupColor );
		}
	}

	// Use base-class to draw key triangles
	Super::DrawTrack(RI, TrackIndex, TrackWidth, TrackHeight, StartTime, PixelsPerSec, SelectedKeys);

	// Draw group name for each shot.
	for(INT i=0; i<CutTrack.Num(); i++)
	{
		FLOAT KeyTime = CutTrack(i).Time;
		INT PixelPos = (KeyTime - StartTime) * PixelsPerSec;

		INT XL, YL;
		RI->StringSize( GEngine->SmallFont, XL, YL, *(CutTrack(i).TargetCamGroup) );
		RI->DrawShadowedString( PixelPos + 2, TrackHeight - YL - KeyVertOffset, *(CutTrack(i).TargetCamGroup), GEngine->SmallFont, KeyLabelColor );
	}
}