/*=============================================================================
	UnEdSrv.cpp: UUnrealEdEngine implementation, the Unreal editing server
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "UnrealEd.h"
#include "UnPath.h"

//@hack: this needs to be cleaned up!
static TCHAR TempStr[MAX_EDCMD], TempName[MAX_EDCMD], Temp[MAX_EDCMD];
static _WORD Word1, Word2, Word4;

UBOOL UUnrealEdEngine::Exec( const TCHAR* Stream, FOutputDevice& Ar )
{
	if( UEditorEngine::Exec( Stream, Ar ) )
		return 1;

	const TCHAR* Str = Stream;

	//----------------------------------------------------------------------------------
	// EDIT
	//
	if( ParseCommand(&Str,TEXT("EDIT")) )
	{
		if( Exec_Edit( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// ACTOR: Actor-related functions
	//
	else if (ParseCommand(&Str,TEXT("ACTOR")))
	{
		if( Exec_Actor( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// PREFAB management:
	//
	else if( ParseCommand(&Str,TEXT("Prefab")) )
	{
		if( Exec_Prefab( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// MODE management (Global EDITOR mode):
	//
	else if( ParseCommand(&Str,TEXT("MODE")) )
	{
		if( Exec_Mode( Str, Ar ) )
			return 1;
	}
	//----------------------------------------------------------------------------------
	// PIVOT
	//
	else if( ParseCommand(&Str,TEXT("PIVOT")) )
	{
		if( Exec_Pivot( Str, Ar ) )
			return 1;
	}
	else if( ParseCommand(&Str,TEXT("MAYBEAUTOSAVE")) )
	{
		// Don't auto-save during interpolation editing.
		if( AutoSave && ++AutoSaveCount>=AutosaveTimeMinutes && GEditorModeTools.GetCurrentModeID() != EM_InterpEdit )
		{
			AutoSaveIndex = (AutoSaveIndex+1)%10;
			SaveConfig();

			// Make sure the autosave directory exists before attempting to write the file.

			FString	AbsoluteAutoSaveDir = FString(appBaseDir()) * AutoSaveDir;
			GFileManager->MakeDirectory( *AbsoluteAutoSaveDir, 1 );

			// Create a final filename

			FString Filename = AbsoluteAutoSaveDir * *FString::Printf( TEXT("Auto%i.%s"), AutoSaveIndex, *GApp->MapExt );

			// Save the map

			TCHAR Cmd[MAX_EDCMD];
			appSprintf( Cmd, TEXT("MAP SAVE AUTOSAVE=1 FILE=\"%s\""), *Filename );
			debugf( NAME_Log, TEXT("Autosaving '%s'"), *Filename );
			Exec( Cmd, Ar );
			AutoSaveCount=0;
		}
		return 1;
	}

	return 0;
}

UBOOL UUnrealEdEngine::Exec_Edit( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("CUT")) )
	{
		Trans->Begin( TEXT("Cut") );
		edactCopySelected( Level );
		edactDeleteSelected( Level );
		Trans->End();
		RedrawLevel( Level );
	}
	else if( ParseCommand(&Str,TEXT("COPY")) )
	{
		edactCopySelected( Level );
	}
	else if( ParseCommand(&Str,TEXT("PASTE")) )
	{
		enum EPasteTo
		{
			PT_OriginalLocation	= 0,
			PT_Here				= 1,
			PT_WorldOrigin		= 2
		} PasteTo = PT_OriginalLocation;

		FString TransName = TEXT("Paste");
		if( Parse( Str, TEXT("TO="), TempStr, 15 ) )
		{
			if( !appStrcmp( TempStr, TEXT("HERE") ) )
			{
				PasteTo = PT_Here;
				TransName = TEXT("Paste Here");
			}
			else
				if( !appStrcmp( TempStr, TEXT("ORIGIN") ) )
				{
					PasteTo = PT_WorldOrigin;
					TransName = TEXT("Paste to World Origin");
				}
		}

		Trans->Begin( *TransName );

		GEditor->SelectNone( Level, 1, 0 );
		edactPasteSelected( Level, 0, 0 );

		if( PasteTo != PT_OriginalLocation )
		{
			// Figure out which location to center the actors around.
			FVector Origin = FVector(0,0,0);
			if( PasteTo == PT_Here )
				Origin = GEditor->ClickLocation;

			// Get a bounding box for all the selected actors locations.
			FBox bbox(0);
			for( INT i=0; i<Level->Actors.Num(); i++ )
				if( Level->Actors(i) && GSelectionTools.IsSelected( Level->Actors(i) ) )
					bbox += Level->Actors(i)->Location;

			// Compute how far the actors have to move.
			FVector Location = bbox.GetCenter(),
				Adjust = Origin - Location;

			// Move the actors
			for( INT i=0; i<Level->Actors.Num(); i++ )
				if( Level->Actors(i) && GSelectionTools.IsSelected( Level->Actors(i) ) )
					Level->Actors(i)->Location += Adjust;
		}

		Trans->End();
		RedrawLevel( Level );
	}

	return 0;

}

UBOOL UUnrealEdEngine::Exec_Pivot( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("HERE")) )
	{
		NoteActorMovement( Level );
		SetPivot( ClickLocation, 0, 0, 0 );
		FinishAllSnaps( Level );
		RedrawLevel( Level );
	}
	else if( ParseCommand(&Str,TEXT("SNAPPED")) )
	{
		NoteActorMovement( Level );
		SetPivot( ClickLocation, 1, 0, 0 );
		FinishAllSnaps( Level );
		RedrawLevel( Level );
	}

	return 0;

}


UBOOL UUnrealEdEngine::Exec_Actor( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("ADD")) )
	{
		UClass* Class;
		if( ParseObject<UClass>( Str, TEXT("CLASS="), Class, ANY_PACKAGE ) )
		{
			AActor* Default   = Class->GetDefaultActor();
			FVector Collision = Default->GetCylinderExtent();
			INT bSnap;
			Parse(Str,TEXT("SNAP="),bSnap);
			if( bSnap )		Constraints.Snap( ClickLocation, FVector(0, 0, 0) );
			FVector Location = ClickLocation + ClickPlane * (FBoxPushOut(ClickPlane,Collision) + 0.1);
			if( bSnap )		Constraints.Snap( Location, FVector(0, 0, 0) );
			AddActor( Level, Class, Location );
			RedrawLevel(Level);
			return 1;
		}
	}
	else if( ParseCommand(&Str,TEXT("MIRROR")) )
	{
		Trans->Begin( TEXT("Mirroring Actors") );

		FVector V( 1, 1, 1 );
		GetFVECTOR( Str, V );

		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor = Level->Actors(i);
			if( Actor && GSelectionTools.IsSelected( Actor ) )
			{
				FVector LocalToWorldOffset = Actor->Location - GEditorModeTools.PivotLocation + Actor->PrePivot;
			
				ABrush* Brush = Cast<ABrush>(Actor);

				if( Brush )
				{
					Brush->Brush->Modify();

					for( INT poly = 0 ; poly < Brush->Brush->Polys->Element.Num() ; poly++ )
					{
						FPoly* Poly = &(Brush->Brush->Polys->Element(poly));
						Brush->Brush->Polys->Element.ModifyAllItems();

						Poly->TextureU *= V;
						Poly->TextureV *= V;

						Poly->Base += LocalToWorldOffset;
						Poly->Base *= V;
						Poly->Base -= LocalToWorldOffset;

						for( INT vtx = 0 ; vtx < Poly->NumVertices ; vtx++ )
						{
							Poly->Vertex[vtx] += LocalToWorldOffset;
							Poly->Vertex[vtx] *= V;
							Poly->Vertex[vtx] -= LocalToWorldOffset;
						}

						Poly->Reverse();
						Poly->CalcNormal();
					}

					Brush->ClearComponents();
				}
				else
				{
					Actor->Modify();

					Actor->Location -= GEditorModeTools.PivotLocation - Actor->PrePivot;
					Actor->Location *= V;
					Actor->Location += GEditorModeTools.PivotLocation - Actor->PrePivot;

					// Adjust the rotation as best we can.
					if( V.X != 1 )	Actor->Rotation.Yaw += (32768 * V.X);
					if( V.Y != 1 )	Actor->Rotation.Yaw += (32768 * V.Y);
					if( V.Z != 1 )	Actor->Rotation.Pitch += (32768 * V.Z);
				}

				Actor->InvalidateLightingCache();
				Actor->UpdateComponents();
			}
		}

		Trans->End();
		RedrawLevel(Level);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("HIDE")) )
	{
		if( ParseCommand(&Str,TEXT("SELECTED")) ) // ACTOR HIDE SELECTED
		{
			Trans->Begin( TEXT("Hide Selected") );
			Level->Modify();
			edactHideSelected( Level );
			Trans->End();
			SelectNone( Level, 0 );
			NoteSelectionChange( Level );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("UNSELECTED")) ) // ACTOR HIDE UNSELECTEED
		{
			Trans->Begin( TEXT("Hide Unselected") );
			Level->Modify();
			edactHideUnselected( Level );
			Trans->End();
			SelectNone( Level, 0 );
			NoteSelectionChange( Level );
			return 1;
		}
	}
	else if( ParseCommand(&Str,TEXT("UNHIDE")) ) // ACTOR UNHIDE ALL
	{
		Trans->Begin( TEXT("UnHide All") );
		Level->Modify();
		edactUnHideAll( Level );
		Trans->End();
		NoteSelectionChange( Level );
		return 1;
	}
	else if( ParseCommand(&Str, TEXT("APPLYTRANSFORM")) )
	{
		Trans->Begin( TEXT("Apply brush transform") );
		Level->Modify();
		edactApplyTransform( Level );
		Trans->End();
		RedrawLevel( Level );
		return 1;
	}
	else if( ParseCommand(&Str, TEXT("REPLACE")) )
	{
		UClass* Class;
		if( ParseCommand(&Str, TEXT("BRUSH")) ) // ACTOR REPLACE BRUSH
		{
			Trans->Begin( TEXT("Replace selected brush actors") );
			Level->Modify();
			edactReplaceSelectedBrush( Level );
			Trans->End();
			NoteSelectionChange( Level );
			return 1;
		}
		else if( ParseObject<UClass>( Str, TEXT("CLASS="), Class, ANY_PACKAGE ) ) // ACTOR REPLACE CLASS=<class>
		{
			Trans->Begin( TEXT("Replace selected non-brush actors") );
			Level->Modify();
			edactReplaceSelectedWithClass( Level, Class );
			Trans->End();
			NoteSelectionChange( Level );
			return 1;
		}
	}
	else if( ParseCommand(&Str,TEXT("SELECT")) )
	{
		if( ParseCommand(&Str,TEXT("NONE")) ) // ACTOR SELECT NONE
		{
			return Exec( TEXT("SELECT NONE") );
		}
		else if( ParseCommand(&Str,TEXT("ALL")) ) // ACTOR SELECT ALL
		{
			Trans->Begin( TEXT("Select All") );
			Level->Modify();
			edactSelectAll( Level );
			Trans->End();
			NoteSelectionChange( Level );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("INSIDE") ) ) // ACTOR SELECT INSIDE
		{
			Trans->Begin( TEXT("Select Inside") );
			Level->Modify();
			edactSelectInside( Level );
			Trans->End();
			NoteSelectionChange( Level );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("INVERT") ) ) // ACTOR SELECT INVERT
		{
			Trans->Begin( TEXT("Select Invert") );
			Level->Modify();
			edactSelectInvert( Level );
			Trans->End();
			NoteSelectionChange( Level );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("OFCLASS")) ) // ACTOR SELECT OFCLASS CLASS=<class>
		{
			UClass* Class;
			if( ParseObject<UClass>(Str,TEXT("CLASS="),Class,ANY_PACKAGE) )
			{
				Trans->Begin( TEXT("Select of class") );
				Level->Modify();
				edactSelectOfClass( Level, Class );
				Trans->End();
				NoteSelectionChange( Level );
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Missing class") );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("OFSUBCLASS")) ) // ACTOR SELECT OFSUBCLASS CLASS=<class>
		{
			UClass* Class;
			if( ParseObject<UClass>(Str,TEXT("CLASS="),Class,ANY_PACKAGE) )
			{
				Trans->Begin( TEXT("Select subclass of class") );
				Level->Modify();
				edactSelectSubclassOf( Level, Class );
				Trans->End();
				NoteSelectionChange( Level );
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Missing class") );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("DELETED")) ) // ACTOR SELECT DELETED
		{
			Trans->Begin( TEXT("Select deleted") );
			Level->Modify();
			edactSelectDeleted( Level );
			Trans->End();
			NoteSelectionChange( Level );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("MATCHINGSTATICMESH")) ) // ACTOR SELECT MATCHINGSTATICMESH
		{
			Trans->Begin( TEXT("Select Matching Static Mesh") );
			Level->Modify();
			edactSelectMatchingStaticMesh( Level );
			Trans->End();
			NoteSelectionChange( Level );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("MATCHINGZONE")) ) // ACTOR SELECT MATCHINGZONE
		{
			Trans->Begin( TEXT("Select All Actors in Matching Zone") );
			Level->Modify();
			edactSelectMatchingZone( Level );
			Trans->End();
			NoteSelectionChange( Level );
			return 1;
		}
	}
	else if( ParseCommand(&Str,TEXT("DELETE")) )		// ACTOR SELECT DELETE
	{
		Trans->Begin( TEXT("Delete Actors") );
		Level->Modify();
		edactDeleteSelected( Level );
		Trans->End();
		NoteSelectionChange( Level );
		GEditor->RedrawLevel( GEditor->Level );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("UPDATE")) )		// ACTOR SELECT UPDATE
	{
		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor=Level->Actors(i);
			if( Actor && GSelectionTools.IsSelected( Actor ) )
			{
				Actor->PreEditChange();
				Actor->PostEditChange(NULL);
			}
		}
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SET")) )
	{
		Trans->Begin( TEXT("Set Actors") );

#if VIEWPORT_ACTOR_DISABLED
		UViewport* CurrentViewport = GetCurrentViewport();
		if( !CurrentViewport )
			debugf(TEXT("No current viewport."));
		else
		{
			for( INT i=0; i<Level->Actors.Num(); i++ )
			{
				AActor* Actor=Level->Actors(i);
				if( Actor && Actor->bSelected )
				{
					Actor->Modify();
					Actor->Location = CurrentViewport->Actor->Location;
					Actor->Rotation = FRotator( CurrentViewport->Actor->Rotation.Pitch, CurrentViewport->Actor->Rotation.Yaw, Actor->Rotation.Roll );
				}
			}
		}
#endif

		Trans->End();
		RedrawLevel( Level );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("RESET")) )
	{
		Trans->Begin( TEXT("Reset Actors") );
		Level->Modify();
		UBOOL Location=0;
		UBOOL Pivot=0;
		UBOOL Rotation=0;
		UBOOL Scale=0;
		if( ParseCommand(&Str,TEXT("LOCATION")) )
		{
			Location=1;
			ResetPivot();
		}
		else if( ParseCommand(&Str, TEXT("PIVOT")) )
		{
			Pivot=1;
			ResetPivot();
		}
		else if( ParseCommand(&Str,TEXT("ROTATION")) )
		{
			Rotation=1;
		}
		else if( ParseCommand(&Str,TEXT("SCALE")) )
		{
			Scale=1;
		}
		else if( ParseCommand(&Str,TEXT("ALL")) )
		{
			Location=Rotation=Scale=1;
			ResetPivot();
		}
		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor=Level->Actors(i);
			if( Actor && GSelectionTools.IsSelected( Actor ) )
			{
				Actor->PreEditChange();
				Actor->Modify();
				if( Location ) Actor->Location  = FVector(0.f,0.f,0.f);
				if( Location ) Actor->PrePivot  = FVector(0.f,0.f,0.f);
				if( Pivot && Cast<ABrush>(Actor) )
				{
					ABrush* Brush = Cast<ABrush>(Actor);
					Brush->Location -= Brush->PrePivot;
					Brush->PrePivot = FVector(0.f,0.f,0.f);
					Brush->PostEditChange(NULL);
				}
				if( Scale    ) Actor->DrawScale = 1.0f;
			}
		}
		Trans->End();
		RedrawLevel( Level );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("DUPLICATE")) )
	{
		Trans->Begin( TEXT("Duplicate Actors") );
		edactCopySelected( Level );
		GEditor->SelectNone( Level, 1, 0 );
		edactPasteSelected( Level, 1, 1 );
		Trans->End();
		RedrawLevel( Level );
		return 1;
	}
	else if( ParseCommand(&Str, TEXT("ALIGN")) )
	{
		if( ParseCommand(&Str,TEXT("SNAPTOFLOOR")) )
		{
			Trans->Begin( TEXT("Snap actors to floor") );

			UBOOL bAlign=0;
			ParseUBOOL( Str, TEXT("ALIGN="), bAlign );

			Level->Modify();
			for( INT i=0; i<Level->Actors.Num(); i++ )
			{
				AActor* Actor = Cast<AActor>(Level->Actors(i));
				if( Actor && GSelectionTools.IsSelected( Actor ) )
				{
					Actor->Modify();
					Level->ToFloor(Actor,bAlign,Actor);
					Actor->InvalidateLightingCache();
					Actor->UpdateComponents();
				}
			}

			Trans->End();
			RedrawLevel( Level );
			return 1;
		}
		else
		{
			Trans->Begin( TEXT("Align brush vertices") );
			Level->Modify();
			edactAlignVertices( Level );
			Trans->End();
			RedrawLevel( Level );
			return 1;
		}
	}
	else if( ParseCommand(&Str,TEXT("TOGGLE")) )
	{
		if( ParseCommand(&Str,TEXT("LOCKMOVEMENT")) )			// ACTOR TOGGLE LOCKMOVEMENT
		{
			for( INT i=0; i<Level->Actors.Num(); i++ )
			{
				AActor* Actor = Cast<AActor>(Level->Actors(i));
				if( Actor && GSelectionTools.IsSelected( Actor ) )
				{
					Actor->Modify();
					Actor->bLockLocation = !Actor->bLockLocation;
				}
			}
		}

		RedrawLevel( Level );
	}


	return 0;

}


UBOOL UUnrealEdEngine::Exec_Mode( const TCHAR* Str, FOutputDevice& Ar )
{
	Word1 = GEditorModeTools.GetCurrentModeID();  // To see if we should redraw
	Word2 = GEditorModeTools.GetCurrentModeID();  // Destination mode to set

	UBOOL DWord1;
	if( ParseCommand(&Str,TEXT("WIDGETMODECYCLE")) )
	{
		if( GEditorModeTools.GetShowWidget() )
		{
			INT Wk = GEditorModeTools.GetWidgetMode();
			Wk++;

			// Roll back to the start if we go past WM_Scale
			if( Wk >= WM_ScaleNonUniform )
			{
				Wk -= WM_ScaleNonUniform;
			}
			GEditorModeTools.SetWidgetMode( (EWidgetMode)Wk );
			GCallback->Send( CALLBACK_RedrawAllViewports );
		}
	}
	if( ParseUBOOL(Str,TEXT("GRID="), DWord1) )
	{
		FinishAllSnaps (Level);
		Constraints.GridEnabled = DWord1;
		Word1=MAXWORD;
		GCallback->Send( CALLBACK_UpdateUI );
	}
	if( ParseUBOOL(Str,TEXT("ROTGRID="), DWord1) )
	{
		FinishAllSnaps (Level);
		Constraints.RotGridEnabled=DWord1;
		Word1=MAXWORD;
		GCallback->Send( CALLBACK_UpdateUI );
	}
	if( ParseUBOOL(Str,TEXT("SNAPVERTEX="), DWord1) )
	{
		FinishAllSnaps (Level);
		Constraints.SnapVertices=DWord1;
		Word1=MAXWORD;
		GCallback->Send( CALLBACK_UpdateUI );
	}
	if( ParseUBOOL(Str,TEXT("ALWAYSSHOWTERRAIN="), DWord1) )
	{
		FinishAllSnaps (Level);
		AlwaysShowTerrain=DWord1;
		Word1=MAXWORD;
	}
	if( ParseUBOOL(Str,TEXT("USEACTORROTATIONGIZMO="), DWord1) )
	{
		FinishAllSnaps (Level);
		UseActorRotationGizmo=DWord1;
		Word1=MAXWORD;
	}
	if( ParseUBOOL(Str,TEXT("SHOWBRUSHMARKERPOLYS="), DWord1) )
	{
		FinishAllSnaps (Level);
		bShowBrushMarkerPolys=DWord1;
		Word1=MAXWORD;
	}
	if( Parse(Str,TEXT("SELECTIONLOCK="), DWord1) )
	{
		FinishAllSnaps (Level);
		// If -1 is passed in, treat it as a toggle.  Otherwise, use the value as a literal assignment.
		if( DWord1 == -1 )
			GEdSelectionLock=(GEdSelectionLock == 0) ? 1 : 0;
		else
			GEdSelectionLock=DWord1;
		Word1=MAXWORD;
	}
	Parse(Str,TEXT("MAPEXT="), GApp->MapExt);
	if( Parse(Str,TEXT("USESIZINGBOX="), DWord1) )
	{
		FinishAllSnaps (Level);
		// If -1 is passed in, treat it as a toggle.  Otherwise, use the value as a literal assignment.
		if( DWord1 == -1 )
			UseSizingBox=(UseSizingBox == 0) ? 1 : 0;
		else
			UseSizingBox=DWord1;
		Word1=MAXWORD;
	}
	Parse( Str, TEXT("SPEED="), MovementSpeed );
	Parse( Str, TEXT("SNAPDIST="), Constraints.SnapDistance );
	//
	// Major modes:
	//
	if 		(ParseCommand(&Str,TEXT("CAMERAMOVE")))		Word2 = EM_Default;
	else if (ParseCommand(&Str,TEXT("TERRAINEDIT")))	Word2 = EM_TerrainEdit;
	else if	(ParseCommand(&Str,TEXT("GEOMETRY"))) 		Word2 = EM_Geometry;
	else if	(ParseCommand(&Str,TEXT("TEXTURE"))) 		Word2 = EM_Texture;

	if( Word2 != Word1 )
		GCallback->Send( CALLBACK_ChangeEditorMode, Word2 );

	// Reset the roll on all viewport cameras
	for(UINT ViewportIndex = 0;ViewportIndex < (UINT)ViewportClients.Num();ViewportIndex++)
	{
		if(ViewportClients(ViewportIndex)->ViewportType == LVT_Perspective)
			ViewportClients(ViewportIndex)->ViewRotation.Roll = 0;
	}

	GCallback->Send( CALLBACK_RedrawAllViewports );
	GCallback->Send( CALLBACK_RefreshEditor, ERefreshEditor_Misc );

	return 1;

}

UBOOL UUnrealEdEngine::Exec_Prefab( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("New")) )
	{
		FName GroupName=NAME_None;
		FName PackageName;
		UClass* PrefabClass;

		DWORD T3DDataPtr;
		Parse( Str, TEXT("T3DData="), T3DDataPtr );
		TCHAR* T3DText = (TCHAR*)T3DDataPtr;

		if( Parse( Str, TEXT("NAME="), TempName, NAME_SIZE )
				&& ParseObject<UClass>( Str, TEXT("CLASS="), PrefabClass, ANY_PACKAGE )
				&& Parse( Str, TEXT("PACKAGE="), PackageName )
				&& PrefabClass->IsChildOf( UPrefab::StaticClass() )
				&& PackageName!=NAME_None )
		{
			UPackage* Pkg = CreatePackage(NULL,*PackageName);
			Pkg->bDirty = 1;
			if( Parse( Str, TEXT("GROUP="), GroupName ) && GroupName!=NAME_None )
				Pkg = CreatePackage(Pkg,*GroupName);
			if( !StaticFindObject( PrefabClass, Pkg, TempName ) )
			{
				// Create new prefab object.
				UPrefab* Result = (UPrefab*)StaticConstructObject( PrefabClass, Pkg, TempName, RF_Public|RF_Standalone );
				Result->PostLoad();

				GSelectionTools.SelectNone<UPrefab>();
				GSelectionTools.Select( Result );
				Result->T3DText = T3DText;
			}
			else
				Ar.Logf( NAME_ExecWarning, TEXT("Prefab exists") );
		}
		else
			Ar.Logf( NAME_ExecWarning, TEXT("Bad PREFAB NEW") );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("ADD")) )		// PREFAB ADD NAME=<name> [ SNAP=<0/1> ]
	{
		GWarn->BeginSlowTask( TEXT("Inserting prefab"), 1);

		const TCHAR* Ptr = *GSelectionTools.GetTop<UPrefab>()->T3DText;
		ULevelFactory* Factory;
		Factory = ConstructObject<ULevelFactory>(ULevelFactory::StaticClass());
		Factory->FactoryCreateText( Level,ULevel::StaticClass(), Level->GetOuter(), Level->GetFName(), RF_Transactional, NULL, TEXT("paste"), Ptr, Ptr+GSelectionTools.GetTop<UPrefab>()->T3DText.Len(), GWarn );
		NoteSelectionChange( Level );

		// Now that we've added the prefab into the world figure out the bounding box, and move it
		// so it's centered on the last click location.
		FBox bbox(1);
		for( INT i=0; i<Level->Actors.Num(); ++i )
			if( Level->Actors(i) && GSelectionTools.IsSelected( Level->Actors(i) ) )
				bbox += (Level->Actors(i)->Location - Level->Actors(i)->PrePivot);

		FVector NewLocation;
		FVector Location;
		if( GetFVECTOR( Str, NewLocation ) )
			Location = NewLocation + ClickPlane;
		else
			Location = ClickLocation + ClickPlane;

		FVector Diff = Location - bbox.GetCenter();
		
		for( INT i=0; i<Level->Actors.Num(); ++i )
			if( Level->Actors(i) && GSelectionTools.IsSelected( Level->Actors(i) ) )
				Level->Actors(i)->Location += Diff;

		RedrawLevel( Level );

		GWarn->EndSlowTask();

		return 1;
	}

	return 0;

}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
