/*=============================================================================
	UnEdCam.cpp: Unreal editor camera movement/selection functions
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "UnrealEd.h"
#include <math.h>            //for  fabs()

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

// Used for actor rotation gizmo.
INT GARGAxis = -1;
UBOOL GbARG = 0;

// Click flags.
enum EViewportClick
{
	CF_MOVE_ACTOR	= 1,	// Set if the actors have been moved since first click
	CF_MOVE_TEXTURE = 2,	// Set if textures have been adjusted since first click
	CF_MOVE_ALL     = (CF_MOVE_ACTOR | CF_MOVE_TEXTURE),
};

// Global variables.
ULevel* GPrefabLevel = NULL;		// A temporary level we assign to the prefab viewport, where we hold the prefabs actors for viewing.

/*-----------------------------------------------------------------------------
   Change transacting.
-----------------------------------------------------------------------------*/

//
// If this is the first time called since first click, note all selected actors.
//
void UUnrealEdEngine::NoteActorMovement( ULevel* Level )
{
	if( !GUndo && !(GUnrealEd->ClickFlags & CF_MOVE_ACTOR) )
	{
		GUnrealEd->ClickFlags |= CF_MOVE_ACTOR;
		GUnrealEd->Trans->Begin( TEXT("Actor movement") );
		GEditorModeTools.Snapping=0;
		INT i;
		for( i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor = Level->Actors(i);
			if( Actor && GSelectionTools.IsSelected( Actor ) )
				break;
		}
		if( i==Level->Actors.Num() )
			SelectActor( Level, Level->Brush() );
		for( i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor = Level->Actors(i);
			if( Actor && GSelectionTools.IsSelected( Actor ) && Actor->bEdShouldSnap )
				GEditorModeTools.Snapping = 1;
		}
		for( i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor = Cast<AActor>(Level->Actors(i));
			if( Actor && GSelectionTools.IsSelected( Actor ) )
			{
				Actor->Modify();
				if(Cast<ABrush>(Actor) && Cast<ABrush>(Actor)->Brush)
					Cast<ABrush>(Actor)->Brush->Polys->Element.ModifyAllItems();
				Actor->bEdSnap |= GEditorModeTools.Snapping;
			}
		}
		GUnrealEd->Trans->End();
	}

}

//
// Finish snapping all brushes in a level.
//
void UUnrealEdEngine::FinishAllSnaps( ULevel* Level )
{
	ClickFlags &= ~CF_MOVE_ACTOR;

	for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
	{
		AActor*	Actor = Level->Actors(ActorIndex);

		if(Actor && GSelectionTools.IsSelected( Actor ) )
			Actor->PostEditMove();
	}

}

//
// Get the editor's pivot location
//
FVector UUnrealEdEngine::GetPivotLocation()
{
	return GEditorModeTools.PivotLocation;
}

//
// Set the editor's pivot location.
//
void UUnrealEdEngine::SetPivot( FVector NewPivot, UBOOL SnapPivotToGrid, UBOOL DoMoveActors, UBOOL bIgnoreAxis )
{
	if( !bIgnoreAxis )
	{
		// Don't stomp on orthonormal axis.
		if( NewPivot.X==0 ) NewPivot.X=GEditorModeTools.PivotLocation.X;
		if( NewPivot.Y==0 ) NewPivot.Y=GEditorModeTools.PivotLocation.Y;
		if( NewPivot.Z==0 ) NewPivot.Z=GEditorModeTools.PivotLocation.Z;
	}

	// Set the pivot.
	GEditorModeTools.PivotLocation   = NewPivot;
	GEditorModeTools.GridBase        = FVector(0,0,0);
	GEditorModeTools.SnappedLocation = GEditorModeTools.PivotLocation;

	if( SnapPivotToGrid )
	{
		GEditorModeTools.GridBase = GEditorModeTools.PivotLocation - GEditorModeTools.SnappedLocation;
		GEditorModeTools.SnappedLocation = GEditorModeTools.PivotLocation;
		Constraints.Snap( Level, GEditorModeTools.SnappedLocation, GEditorModeTools.GridBase, FRotator(0,0,0) );
		GEditorModeTools.PivotLocation = GEditorModeTools.SnappedLocation;
	}
	else
	{
		GEditorModeTools.PivotLocation = GEditorModeTools.SnappedLocation;
	}

	// Check all actors.
	INT Count=0, SnapCount=0;
	AActor* SingleActor=NULL;
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		if( Level->Actors(i) && GSelectionTools.IsSelected( Level->Actors(i) ) )
		{
			Count++;
			SnapCount += Level->Actors(i)->bEdShouldSnap;
			SingleActor = Level->Actors(i);
		}
	}

	// Apply to actors.
	if( Count==1 )
	{
		if( SingleActor->IsA( ABrush::StaticClass() ) )
		{
			SingleActor->PreEditChange();
			SingleActor->Modify();
			SingleActor->SetPrePivot( SingleActor->PrePivot + (GEditorModeTools.SnappedLocation - SingleActor->Location) );

			FCheckResult Hit;
			Level->MoveActor(SingleActor, GEditorModeTools.SnappedLocation - SingleActor->Location, SingleActor->Rotation, Hit);

			SingleActor->PostEditChange(NULL);
		}
	}

	// Update showing.
	GEditorModeTools.PivotShown = SnapCount>0 || Count>1;
}

//
// Reset the editor's pivot location.
//
void UUnrealEdEngine::ResetPivot()
{
	GEditorModeTools.PivotShown = 0;
	GEditorModeTools.Snapping   = 0;
}

/*-----------------------------------------------------------------------------
	Selection.
-----------------------------------------------------------------------------*/

//
// Selection change.
//
void UUnrealEdEngine::NoteSelectionChange( ULevel* Level )
{
	// Notify the editor.
	GCallback->Send( CALLBACK_SelChange );

	// Pick a new common pivot, or not.
	INT Count=0;
	AActor* SingleActor=NULL;
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		if( Level->Actors(i) && GSelectionTools.IsSelected( Level->Actors(i) ) )
		{
			SingleActor=Level->Actors(i);
			Count++;
		}
	}
	if( Count==0 ) ResetPivot();
	else if( Count==1 ) SetPivot( SingleActor->Location, 0, 0, 1 );

#if VIEWPORT_ACTOR_DISABLED
	// Update any viewport locking
	for( INT i = 0 ; i < Client->Viewports.Num() ; i++ )
		Client->Viewports(i)->LockOnActor( SingleActor );
#endif

	GEditorModeTools.GetCurrentMode()->ActorSelectionChangeNotify();

	UpdatePropertyWindows();

	RedrawLevel( Level );

}

// Selects/Deselects an actor.
void UUnrealEdEngine::SelectActor( ULevel* Level, AActor* Actor, UBOOL InSelected, UBOOL bNotify )
{
	// If selections are globally locked, leave.

	if( GEdSelectionLock )
	{
		return;
	}

	// If the actor is NULL or hidden, leave.

	if( !Actor || Actor->IsHiddenEd() )
	{
		// Only truly abort this if we are selecting.  You can deselect hidden actors without a problem.

		if( InSelected )
		{
			return;
		}
	}

	// If the current mode won't allow selections of this type of actor, leave.

	if( !GEditorModeTools.GetCurrentMode()->CanSelect( Actor ) )
	{
		return;
	}

	// Select the actor and update its internals

	Actor->Modify();
	GSelectionTools.Select( Actor, InSelected );
	Actor->UpdateComponents();

	if( bNotify )
	{
		NoteSelectionChange( Level );
	}
}

// Selects/Deselects a BSP surface.
void UUnrealEdEngine::SelectBSPSurf( ULevel* Level, INT iSurf, UBOOL InSelected, UBOOL bNotify )
{
	if( GEdSelectionLock )
		return;

	FBspSurf& Surf = Level->Model->Surfs(iSurf);

	Level->Model->ModifySurf( iSurf, 0 );
	if( InSelected )
		Surf.PolyFlags |= PF_Selected;
	else
		Surf.PolyFlags &= ~PF_Selected;

	if( bNotify )
		NoteSelectionChange( Level );

}

//
// Select none.
//
// If "BSPSurfs" is 0, then BSP surfaces will be left selected.
//
void UUnrealEdEngine::SelectNone( ULevel *Level, UBOOL Notify, UBOOL BSPSurfs )
{
	if( GEdSelectionLock )
		return;

	if( !Level )
		return;

	// Unselect all actors.
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && GSelectionTools.IsSelected( Actor ) )
			SelectActor( Level, Actor, 0, 0 );
	}

	if( BSPSurfs )
	{
		// Unselect all surfaces.
		for( INT i=0; i<Level->Model->Surfs.Num(); i++ )
		{
			FBspSurf& Surf = Level->Model->Surfs(i);
			if( Surf.PolyFlags & PF_Selected )
			{
				Level->Model->ModifySurf( i, 0 );
				Surf.PolyFlags &= ~PF_Selected;
			}
		}
	}

	if( Notify )
		NoteSelectionChange( Level );
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
