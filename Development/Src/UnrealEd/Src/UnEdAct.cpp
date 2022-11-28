/*=============================================================================
	UnEdAct.cpp: Unreal editor actor-related functions
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "UnrealEd.h"
#include "EngineSequenceClasses.h"

#pragma DISABLE_OPTIMIZATION /* Not performance-critical */

INT RecomputePoly( ABrush* InOwner, FPoly* Poly )
{
	// force recalculation of normal, and texture U and V coordinates in FPoly::Finalize()
	Poly->Normal = FVector(0,0,0);

	// catch normalization exceptions to warn about non-planar polys
	try
	{
		return Poly->Finalize( InOwner, 0 );
	}
	catch(...)
	{
		debugf( TEXT("WARNING: FPoly::Finalize() failed (You broke the poly!)") );
	}

	return 0;
}

/*-----------------------------------------------------------------------------
   Actor adding/deleting functions.
-----------------------------------------------------------------------------*/

//
// Copy selected actors to the clipboard.
//
void UUnrealEdEngine::edactCopySelected( ULevel* Level )
{
	// Export the actors.
	FStringOutputDevice Ar;
	UExporter::ExportToOutputDevice( Level, NULL, Ar, TEXT("copy"), 0 );
	appClipboardCopy( *Ar );

}

/**
 * Paste selected actors from the clipboard.
 *
 * @param	InLevel			The level to work within
 * @param	InDuplicate		Is this a duplicate operation (as opposed to a real paste)?
 * @param	InUseOffset		Should the actor locations be offset after they are created?
 */

void UUnrealEdEngine::edactPasteSelected( ULevel* InLevel, UBOOL InDuplicate, UBOOL InUseOffset )
{
	FVector SavePivot = GEditorModeTools.PivotLocation;

	// Save visible groups

	TArray<FString> VisibleGroupArray;
	InLevel->GetLevelInfo()->VisibleGroups.ParseIntoArray( TEXT(","), &VisibleGroupArray );

	// Get pasted text.
	FString PasteString = appClipboardPaste();
	const TCHAR* Paste = *PasteString;

	// Import the actors.
	ULevelFactory* Factory = new ULevelFactory;
	Factory->FactoryCreateText( InLevel, ULevel::StaticClass(), InLevel->GetOuter(), InLevel->GetFName(), RF_Transactional, NULL, TEXT("paste"), Paste, Paste+appStrlen(Paste), GWarn );
	delete Factory;

	for( INT i=0; i<InLevel->Actors.Num(); i++ )
	{
		AActor* Actor = InLevel->Actors(i);

		if( Actor && GSelectionTools.IsSelected( Actor ) )
		{
			INT Offset = Max<INT>( Constraints.GetGridSize(), 32 );
			if( !InUseOffset )
			{
				Offset = 0;
			}

			// Offset the actor

			if( InDuplicate )
			{
				Actor->Location += FVector(Offset,Offset,0);
			}
			else
			{
				Actor->Location += FVector(Offset,Offset,Offset);
			}

			// Add any groups this actor belongs to into the visible array

			TArray<FString> WkArray;
			FString Groups = *(Actor->Group);
			Groups.ParseIntoArray( TEXT(","), &WkArray );

			for( INT g = 0 ; g < WkArray.Num() ; ++g )
			{
				FString GroupName = WkArray(g);
				VisibleGroupArray.AddUniqueItem( GroupName );
			}

			//

			InLevel->Actors(i)->UpdateComponents();
		}
	}

	// Create the new list of visible groups and set it

	FString NewVisibleGroups;

	for( INT x = 0 ; x < VisibleGroupArray.Num() ; ++x )
	{
		if( NewVisibleGroups.Len() )
		{
			NewVisibleGroups += TEXT(",");
		}
		NewVisibleGroups += VisibleGroupArray(x);
	}

	InLevel->GetLevelInfo()->VisibleGroups = NewVisibleGroups;
	GCallback->Send( CALLBACK_RefreshEditor, ERefreshEditor_GroupBrowser );

	// If we're only duplicating, just redraw the viewports.  If it's a real paste, we need
	// to update the entire editor.

	if( InDuplicate )
	{
		RedrawLevel( InLevel );
	}
	else
	{
		GCallback->Send( CALLBACK_MapChange, 1 );
	}

	// Update the pivot location

	if( InUseOffset )
	{
		AActor* actor = GSelectionTools.GetTop<AActor>();
		if( actor )
		{
			GEditorModeTools.PivotLocation = GEditorModeTools.SnappedLocation = actor->Location;
		}
	}
	else
	{
		GEditorModeTools.PivotLocation = GEditorModeTools.SnappedLocation = SavePivot;
	}

	NoteSelectionChange( InLevel );

}

//
// Delete all selected actors.
//
void UUnrealEdEngine::edactDeleteSelected( ULevel* Level )
{
	DOUBLE StartSeconds = appSeconds();
	INT DeleteCount = 0;

	// First, see if 
	if(Level->GameSequence)
	{
		UBOOL bKismetActorSelected = false;
		for(INT i=0; i < Level->Actors.Num() && !bKismetActorSelected; i++)
		{
			AActor* Actor = Level->Actors(i);
			if( Actor && 
				GSelectionTools.IsSelected(Actor) &&
				Level->GameSequence->ReferencesObject(Actor) )
			{
				bKismetActorSelected = true;
			}

		}

		if(bKismetActorSelected)
		{
			UBOOL bDoDelete = appMsgf( 1, TEXT("Some selected Actors are referenced by a Kismet sequence.\nAre you sure you want to continue?") );
			if(!bDoDelete)
			{
				return;
			}
		}
	}

	for(INT i=0; i < Level->Actors.Num(); i++)
	{
		AActor* Actor = Level->Actors(i);
		if
			(	(Actor)
			&&	(GSelectionTools.IsSelected( Actor ) )
			&&	(Level->Actors.Num()<1 || Actor!=Level->Actors(0))
			&&	(Level->Actors.Num()<2 || Actor!=Level->Actors(1))
			&&  (Actor->GetFlags() & RF_Transactional) )
		{

			if ( (Actor->bPathColliding && Actor->bBlockActors) || Actor->IsA(ANavigationPoint::StaticClass()) )
			{
				Level->GetLevelInfo()->bPathsRebuilt = 0;
			}

			Level->DestroyActor(Actor, false, true);

			DeleteCount++;
		}
	}

	// Remove all references to destroyed actors once at the end, instead of once for each Actor destroyed..
	for( TObjectIterator<UObject> It; It; ++It )
	{
		UObject* Obj = *It;
		Obj->GetClass()->CleanupDestroyed( (BYTE*)Obj, Obj );
	}

	NoteSelectionChange( Level );

	debugf( TEXT("Deleted %d Actors (%3.3f secs)"), DeleteCount, appSeconds() - StartSeconds );
}

//
// Replace all selected brushes with the default brush
//
void UUnrealEdEngine::edactReplaceSelectedBrush( ULevel* Level )
{
	// Untag all actors.
	for( int i=0; i<Level->Actors.Num(); i++ )
		if( Level->Actors(i) )
			Level->Actors(i)->bTempEditor = 0;

	// Replace all selected brushes
	ABrush* DefaultBrush = Level->Brush();
	for( int i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if
		(	Actor
		&&	GSelectionTools.IsSelected( Actor )
		&&  !Actor->bTempEditor
		&&  Actor->IsBrush()
		&&  Actor!=DefaultBrush
		&&  (Actor->GetFlags() & RF_Transactional) )
		{
			ABrush* Brush = (ABrush*)Actor;
			ABrush* NewBrush = csgAddOperation( DefaultBrush, Level, Brush->PolyFlags, (ECsgOper)Brush->CsgOper );
			if( NewBrush )
			{
				NewBrush->Modify();
				NewBrush->Group = Brush->Group;
				NewBrush->CopyPosRotScaleFrom( Brush );
				NewBrush->PostEditMove();
				NewBrush->bTempEditor = 1;
				SelectActor( Level, NewBrush, 1, 0 );
				Level->EditorDestroyActor( Actor );
			}
		}
	}
	NoteSelectionChange( Level );
}

static void CopyActorProperties( AActor* Dest, const AActor *Src )
{
	// Events
	Dest->Tag	= Src->Tag;

	// Object
	Dest->Group	= Src->Group;
}

//
// Replace all selected non-brush actors with the specified class
//
void UUnrealEdEngine::edactReplaceSelectedWithClass( ULevel* Level,UClass* Class )
{
	// Untag all actors.
	for( int i=0; i<Level->Actors.Num(); i++ )
		if( Level->Actors(i) )
			Level->Actors(i)->bTempEditor = 0;

	// Replace all selected brushes
	for( int i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if
		(	Actor
		&&	GSelectionTools.IsSelected( Actor )
		&&  !Actor->bTempEditor
		&&  !Actor->IsBrush()
		&&  (Actor->GetFlags() & RF_Transactional) )
		{
			AActor* NewActor = Level->SpawnActor
			(
				Class,
				NAME_None,
				Actor->Location,
				Actor->Rotation,
				NULL,
				1
			);
			if( NewActor )
			{
				NewActor->Modify();
				CopyActorProperties( NewActor, Actor );
				NewActor->bTempEditor = 1;
				SelectActor( Level, NewActor, 1, 0 );
				Level->EditorDestroyActor( Actor );
			}
		}
	}
	NoteSelectionChange( Level );
}

void UUnrealEdEngine::edactReplaceClassWithClass( ULevel* Level,UClass* Class,UClass* WithClass )
{
	// Untag all actors.
	for( int i=0; i<Level->Actors.Num(); i++ )
		if( Level->Actors(i) )
			Level->Actors(i)->bTempEditor = 0;

	// Replace all matching actors
	for( int i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if
		(	Actor
		&&	Actor->IsA( Class )
		&&  !Actor->bTempEditor
		&&  (Actor->GetFlags() & RF_Transactional) )
		{
			AActor* NewActor = Level->SpawnActor
			(
				WithClass,
				NAME_None,
				Actor->Location,
				Actor->Rotation,
				NULL,
				1
			);
			if( NewActor )
			{
				NewActor->Modify();
				NewActor->bTempEditor = 1;
				SelectActor( Level, NewActor, 1, 0 );
				CopyActorProperties( NewActor, Actor );
				Level->EditorDestroyActor( Actor );
			}
		}
	}
	NoteSelectionChange( Level );
}

/*-----------------------------------------------------------------------------
   Actor hiding functions.
-----------------------------------------------------------------------------*/

//
// Hide selected actors (set their bHiddenEd=true)
//
void UUnrealEdEngine::edactHideSelected( ULevel* Level )
{
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && Actor!=Level->Brush() && GSelectionTools.IsSelected( Actor ) )
		{
			Actor->Modify();
			Actor->bHiddenEd = 1;
			Actor->ClearFlags( RF_EdSelected );
		}
	}
	NoteSelectionChange( Level );
}

//
// Hide unselected actors (set their bHiddenEd=true)
//
void UUnrealEdEngine::edactHideUnselected( ULevel* Level )
{
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && Actor!=Level->Brush() && !GSelectionTools.IsSelected( Actor ) )
		{
			Actor->Modify();
			Actor->bHiddenEd = 1;
			Actor->ClearFlags( RF_EdSelected );
		}
	}
	NoteSelectionChange( Level );
}

//
// UnHide selected actors (set their bHiddenEd=true)
//
void UUnrealEdEngine::edactUnHideAll( ULevel* Level )
{
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if
		(	Actor
		&&	Actor!=Level->Brush()
		&&	Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
		{
			Actor->Modify();
			Actor->bHiddenEd = 0;
		}
	}
	NoteSelectionChange( Level );
}

/*-----------------------------------------------------------------------------
   Actor selection functions.
-----------------------------------------------------------------------------*/

//
// Select all actors except hidden actors.
//
void UUnrealEdEngine::edactSelectAll( ULevel* Level )
{
	// Add all selected actors' group name to the GroupArray
	TArray<FName> GroupArray;
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && !Actor->IsHiddenEd() )
		{
			if( GSelectionTools.IsSelected( Actor ) && Actor->Group!=NAME_None )
			{
				GroupArray.AddUniqueItem( Actor->Group );
			}
		}
	}

	// if the default brush is the only brush selected, select objects inside the default brush
	if( GroupArray.Num() == 0 && GSelectionTools.IsSelected( Level->Brush() ) ) 
	{
		edactSelectInside( Level );
		return;
	} 
	// if GroupArray is empty, select all unselected actors (v156 default "Select All" behavior)
	else if( GroupArray.Num() == 0 ) 
	{
		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor = Level->Actors(i);
			if( Actor && !GSelectionTools.IsSelected( Actor ) && !Actor->IsHiddenEd() )
			{
				SelectActor( Level, Actor, 1, 0 );
			}
		}

	} 
	// otherwise, select all actors that match one of the groups,
	else 
	{
		// use appStrfind() to allow selection based on hierarchically organized group names
		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor = Level->Actors(i);
			if( Actor && !GSelectionTools.IsSelected( Actor ) && !Actor->IsHiddenEd() )
			{
				for( INT j=0; j<GroupArray.Num(); j++ ) {
					if( appStrfind( *Actor->Group, *GroupArray(j) ) != NULL ) {
						SelectActor( Level, Actor, 1, 0 );
						break;
					}
				}
			}
		}
	}

	NoteSelectionChange( Level );
}

//
// Select all actors inside the volume of the Default Brush
//
void UUnrealEdEngine::edactSelectInside( ULevel* Level )
{
	// Untag all actors.
	for( INT i=0; i<Level->Actors.Num(); i++ )
		if( Level->Actors(i) )
			Level->Actors(i)->bTempEditor = 0;

	// tag all candidate actors
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && Actor!=Level->Brush() && !Actor->IsHiddenEd() )
		{
			Actor->bTempEditor = 1;
		}
	}

	// deselect all actors that are outside the default brush
	UModel* DefaultBrush = Level->Brush()->Brush;
	FMatrix	DefaultBrushToWorld = Level->Brush()->LocalToWorld();
	for( INT i=0; i<DefaultBrush->Polys->Element.Num(); i++ )
	{
		// get the plane for each polygon in the default brush
		FPoly* Poly = &DefaultBrush->Polys->Element( i );
		FPlane Plane( DefaultBrushToWorld.TransformFVector(Poly->Base), DefaultBrushToWorld.TransformNormal(Poly->Normal).SafeNormal() );
		for( INT j=0; j<Level->Actors.Num(); j++ )
		{
			// deselect all actors that are in front of the plane (outside the brush)
			AActor* Actor = Level->Actors(j);
			if( Actor && Actor->bTempEditor ) {
				// treat non-brush actors as point objects
				if( !Cast<ABrush>(Actor) ) {
					FLOAT Dist = Plane.PlaneDot( Actor->Location );
					if( Dist >= 0.0 ) {
						// actor is in front of the plane (outside the default brush)
						Actor->bTempEditor = 0;
					}

				} else {
					// if the brush data is corrupt, abort this actor -- see mpoesch email to Tim sent 9/8/98
					if( Cast<ABrush>(Actor)->Brush == 0 )
						continue;
					// examine all the points
					UPolys* Polys = Cast<ABrush>(Actor)->Brush->Polys;
					for( INT k=0; k<Polys->Element.Num(); k++ )
					{
						FMatrix BrushToWorld = Actor->LocalToWorld();
						for( INT m=0; m<Polys->Element(k).NumVertices; m++ )
						{
							FLOAT Dist = Plane.PlaneDot( BrushToWorld.TransformFVector(Polys->Element(k).Vertex[m]) );
							if( Dist >= 0.0 )
							{
								// actor is in front of the plane (outside the default brush)
								Actor->bTempEditor = 0;
							}
						}
					}
				}
			}
		}
	}

	// update the selection state with the result from above
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && GSelectionTools.IsSelected( Actor ) != Actor->bTempEditor )
			SelectActor( Level, Actor, Actor->bTempEditor, 0 );
	}
	NoteSelectionChange( Level );
}

//
// Invert the selection of all actors
//
void UUnrealEdEngine::edactSelectInvert( ULevel* Level )
{
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && Actor!=Level->Brush() && !Actor->IsHiddenEd() )
			SelectActor( Level, Actor, !GSelectionTools.IsSelected( Actor ), 0 );
	}
	NoteSelectionChange( Level );
}

//
// Select all actors in a particular class.
//
void UUnrealEdEngine::edactSelectOfClass( ULevel* Level, UClass* Class )
{
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && Actor->GetClass()==Class && !GSelectionTools.IsSelected( Actor ) && !Actor->IsHiddenEd() )
			SelectActor( Level, Actor, 1, 0 );
	}
	NoteSelectionChange( Level );
}

//
// Select all actors in a particular class and its subclasses.
//
void UUnrealEdEngine::edactSelectSubclassOf( ULevel* Level, UClass* Class )
{
	FName ClassName = Class ? Class->GetFName() : NAME_None;
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && !GSelectionTools.IsSelected( Actor ) && !Actor->IsHiddenEd() )
		{
			for( UClass *TempClass=Actor->GetClass(); TempClass; TempClass=TempClass->GetSuperClass() )
			{
				if( TempClass->GetFName() == ClassName )
				{
					SelectActor( Level, Actor, 1, 0 );
					break;
				}
			}
		}
	}
	NoteSelectionChange( Level );
}

//
// Select all actors in a level that are marked for deletion.
//
void UUnrealEdEngine::edactSelectDeleted( ULevel* Level )
{
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && !GSelectionTools.IsSelected( Actor ) && !Actor->IsHiddenEd() )
			if( Actor->bDeleteMe )
				SelectActor( Level, Actor, 1, 0 );
	}
	NoteSelectionChange( Level );
}

//
// Select all actors that have the same static mesh assigned to them as the selected ones.
//
void UUnrealEdEngine::edactSelectMatchingStaticMesh( ULevel* Level )
{
	TArray<UStaticMesh*> StaticMeshes;

	// Get a list of the static meshes in the selected actors.
	for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
	{
		AStaticMeshActor* Actor = Cast<AStaticMeshActor>(Level->Actors(i));
		if( Actor && GSelectionTools.IsSelected( Actor ) )
		{
			StaticMeshes.AddUniqueItem( Actor->StaticMeshComponent->StaticMesh );
		}
	}

	// Loop through all actors in the level, selecting the ones that have
	// one of the static meshes in the list.
	for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
	{
		AStaticMeshActor* Actor = Cast<AStaticMeshActor>(Level->Actors(i));
		if( Actor && !Actor->IsHiddenEd() && StaticMeshes.FindItemIndex( Actor->StaticMeshComponent->StaticMesh ) != INDEX_NONE )
		{
			SelectActor( Level, Actor, 1, 0 );
		}
	}

	NoteSelectionChange( Level );
}

//
// Select all actors that are in the same zone as the selected actors.
//
void UUnrealEdEngine::edactSelectMatchingZone( ULevel* Level )
{
	TArray<INT> Zones;

	// Figure out which zones are represented by the current selections.
	for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && GSelectionTools.IsSelected( Actor ) )
			Zones.AddUniqueItem( Actor->Region.ZoneNumber );
	}

	GWarn->BeginSlowTask( TEXT("Selecting ..."), 1 );

	// Loop through all actors in the level, selecting the ones that have
	// a matching zone.
	for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
	{
		AActor* Actor = Level->Actors(i);
		GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Selecting Actors") );
		if( Actor && !Actor->IsBrush() && Zones.FindItemIndex( Actor->Region.ZoneNumber ) != INDEX_NONE )
			SelectActor( Level, Actor, 1, 0 );
	}

	// Loop through all the BSP nodes, checking their zones.  If a match is found, select the
	// brush actor that belongs to that surface.
	for( INT i = 0 ; i < Level->Model->Nodes.Num() ; ++i )
	{
		FBspNode* Node = &(Level->Model->Nodes(i));

		GWarn->StatusUpdatef( i, Level->Model->Nodes.Num(), TEXT("Selecting Brushes") );

		if( Zones.FindItemIndex( Node->iZone[1] ) != INDEX_NONE )
		{
			FBspSurf* Surf = &(Level->Model->Surfs(Node->iSurf));
			SelectActor( Level, Surf->Actor, 1, 0 );
		}
	}

	GWarn->EndSlowTask();

	NoteSelectionChange( Level );
}

//
// Recompute and adjust all vertices (based on the current transformations),
// then reset the transformations
//
void UUnrealEdEngine::edactApplyTransform( ULevel* Level )
{
	// apply transformations to all selected brushes
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && GSelectionTools.IsSelected( Actor ) && Actor->IsBrush() )
			edactApplyTransformToBrush( (ABrush*)Actor );
	}

}

void UUnrealEdEngine::edactApplyTransformToBrush( ABrush* InBrush )
{
	InBrush->PreEditChange();
	InBrush->Modify();

	// recompute new locations for all vertices based on the current transformations
	UPolys* Polys = InBrush->Brush->Polys;
	Polys->Element.ModifyAllItems();
	for( INT j=0; j<Polys->Element.Num(); j++ )
	{
		Polys->Element(j).Transform( FVector(0,0,0), FVector(0,0,0) );

		// the following function is a bit of a hack.  But, for some reason,
		// the normal/textureU/V recomputation in FPoly::Transform() isn't working.  LEGEND
		RecomputePoly( InBrush, &Polys->Element(j) );
	}

	InBrush->Brush->BuildBound();

	InBrush->PostEditChange(NULL);

}

//
// Align all vertices with the current grid
//
void UUnrealEdEngine::edactAlignVertices( ULevel* Level )
{
	// apply transformations to all selected brushes
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && GSelectionTools.IsSelected( Actor ) && Actor->IsBrush() )
		{
			Actor->PreEditChange();
			Actor->Modify();

			// snap each vertex in the brush to an integer grid
			UPolys* Polys = ((ABrush*)Actor)->Brush->Polys;
			Polys->Element.ModifyAllItems();
			for( INT j=0; j<Polys->Element.Num(); j++ )
			{
				FPoly* Poly = &Polys->Element(j);
				for( INT k=0; k<Poly->NumVertices; k++ )
				{
					// snap each vertex to the nearest grid
					Poly->Vertex[k].X = appRound( ( Poly->Vertex[k].X + Actor->Location.X )  / Constraints.GetGridSize() ) * Constraints.GetGridSize() - Actor->Location.X;
					Poly->Vertex[k].Y = appRound( ( Poly->Vertex[k].Y + Actor->Location.Y )  / Constraints.GetGridSize() ) * Constraints.GetGridSize() - Actor->Location.Y;
					Poly->Vertex[k].Z = appRound( ( Poly->Vertex[k].Z + Actor->Location.Z )  / Constraints.GetGridSize() ) * Constraints.GetGridSize() - Actor->Location.Z;
				}

				// If the snapping resulted in an off plane polygon, triangulate it to compensate.

				if( !Poly->IsCoplanar() )
				{
					Poly->Triangulate( (ABrush*)Actor );
				}

				if( RecomputePoly( (ABrush*)Actor, &Polys->Element(j) ) == -2 )
				{
					j = -1;
				}
			}

			((ABrush*)Actor)->Brush->BuildBound();

			Actor->PostEditChange(NULL);
		}
	}

}

/*-----------------------------------------------------------------------------
   The End.
-----------------------------------------------------------------------------*/
