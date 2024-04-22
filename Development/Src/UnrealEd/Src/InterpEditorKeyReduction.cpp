/*=============================================================================
	InterpEditorTools.cpp: Interpolation editing support tools
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
	Written by Feeling Software inc.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "InterpEditor.h"
#include "DlgInterpEditorKeyReduction.h"
#include "ScopedTransaction.h"

void WxInterpEd::ReduceKeysForTrack( UInterpTrack* Track, UInterpTrackInst* TrackInst, FLOAT IntervalStart, FLOAT IntervalEnd, FLOAT Tolerance )
{
	Track->ReduceKeys(IntervalStart, IntervalEnd, Tolerance);
}

void WxInterpEd::ReduceKeys()
{
	// Set-up based on the "AddKey" function.
	// This set-up gives us access to the essential undo/redo functionality.
	ClearKeySelection();

	if( !HasATrackSelected() )
	{
		appMsgf(AMT_OK,*LocalizeUnrealEd("NoActiveTrack"));
	}
	else
	{
		for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
		{
			UInterpTrack* Track = *TrackIt;
			UInterpGroup* Group = TrackIt.GetGroup();
			UInterpGroupInst* GrInst = Interp->FindFirstGroupInst( Group );
			check(GrInst);

			UInterpTrackInst* TrackInst = NULL;
			UInterpTrack* Parent = Cast<UInterpTrack>( Track->GetOuter() );
			
			if( Parent )
			{
				INT Index = Group->InterpTracks.FindItemIndex( Parent );
				check(Index != INDEX_NONE);
				TrackInst = GrInst->TrackInst( Index );
			}
			else
			{
				TrackInst = GrInst->TrackInst( TrackIt.GetTrackIndex() );
			}
			check(TrackInst);

			// Request the key reduction parameters from the user.
			FLOAT IntervalStart, IntervalEnd;
			Track->GetTimeRange(IntervalStart, IntervalEnd);
			WxInterpEditorKeyReduction ParameterDialog(this, IntervalStart, IntervalEnd);
			LRESULT Result = ParameterDialog.ShowModal();
			if (Result == FALSE) return; // User cancelled..
			if (!ParameterDialog.FullInterval)
			{
				IntervalStart = ParameterDialog.IntervalStart;
				IntervalEnd = ParameterDialog.IntervalEnd;
			}

			// Allows for undo capabilities.
			InterpEdTrans->BeginSpecial( *LocalizeUnrealEd("ReduceKeys") );
			Track->Modify();
			Opt->Modify();

			ReduceKeysForTrack( Track, TrackInst, IntervalStart, IntervalEnd, ParameterDialog.Tolerance );
			
			// Update to current time, in case new key affects state of scene.
			RefreshInterpPosition();

			// Dirty the track window viewports
			InvalidateTrackWindowViewports();

			InterpEdTrans->EndSpecial();
		}
	}
}

