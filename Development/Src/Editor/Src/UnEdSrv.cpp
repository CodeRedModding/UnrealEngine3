/*=============================================================================
	UnEdSrv.cpp: UEditorEngine implementation, the Unreal editing server
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EditorPrivate.h"
#include "UnPath.h"
#include "EnginePhysicsClasses.h"

extern FRebuildTools GRebuildTools;

/*-----------------------------------------------------------------------------
	UnrealEd safe command line.
-----------------------------------------------------------------------------*/

void UEditorEngine::RedrawAllViewports( UBOOL bLevelViewportsOnly )
{
	for(UINT ViewportIndex = 0;ViewportIndex < (UINT)ViewportClients.Num();ViewportIndex++)
	{
		if(!bLevelViewportsOnly || ViewportClients(ViewportIndex)->GetLevel() == Level)
			ViewportClients(ViewportIndex)->Viewport->Invalidate();
	}
}

//
// Execute a macro.
//
void UEditorEngine::ExecMacro( const TCHAR* Filename, FOutputDevice& Ar )
{
	// Create text buffer and prevent garbage collection.
	UTextBuffer* Text = ImportObject<UTextBuffer>( GEditor->Level, GetTransientPackage(), NAME_None, 0, Filename );
	if( Text )
	{
		Text->AddToRoot();
		debugf( TEXT("Execing %s"), Filename );
		TCHAR Temp[MAX_EDCMD];
		const TCHAR* Data = *Text->Text;
		while( ParseLine( &Data, Temp, ARRAY_COUNT(Temp) ) )
			Exec( Temp, Ar );
		Text->RemoveFromRoot();
		delete Text;
	}
	else Ar.Logf( NAME_ExecWarning, *LocalizeError("FileNotFound",TEXT("Editor")), Filename );

}

//
// Execute a command that is safe for rebuilds.
//
UBOOL UEditorEngine::SafeExec( const TCHAR* InStr, FOutputDevice& Ar )
{
	TCHAR TempFname[MAX_EDCMD];
	const TCHAR* Str=InStr;
	if( ParseCommand(&Str,TEXT("MACRO")) || ParseCommand(&Str,TEXT("EXEC")) )//oldver (exec)
	{
		TCHAR Filename[MAX_EDCMD];
		if( ParseToken( Str, Filename, ARRAY_COUNT(Filename), 0 ) )
			ExecMacro( Filename, Ar );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("NEW")) )
	{
		// Generalized object importing.
		DWORD   Flags         = RF_Public|RF_Standalone;
		if( ParseCommand(&Str,TEXT("STANDALONE")) )
			Flags = RF_Public|RF_Standalone;
		else if( ParseCommand(&Str,TEXT("PUBLIC")) )
			Flags = RF_Public;
		else if( ParseCommand(&Str,TEXT("PRIVATE")) )
			Flags = 0;
		FString ClassName     = ParseToken(Str,0);
		UClass* Class         = FindObject<UClass>( ANY_PACKAGE, *ClassName );
		if( !Class )
		{
			Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized or missing factor class %s"), *ClassName );
			return 1;
		}
		FString  PackageName  = ParentContext ? ParentContext->GetName() : TEXT("");
		FString	 GroupName	  = TEXT("");
		FString  FileName     = TEXT("");
		FString  ObjectName   = TEXT("");
		UClass*  ContextClass = NULL;
		UObject* Context      = NULL;
		Parse( Str, TEXT("Package="), PackageName );
		Parse( Str, TEXT("Group="), GroupName );
		Parse( Str, TEXT("File="), FileName );
		ParseObject( Str, TEXT("ContextClass="), UClass::StaticClass(), *(UObject**)&ContextClass, NULL );
		ParseObject( Str, TEXT("Context="), ContextClass, Context, NULL );
		if
		(	!Parse( Str, TEXT("Name="), ObjectName )
		&&	FileName!=TEXT("") )
		{
			// Deduce object name from filename.
			ObjectName = FileName;
			for( ; ; )
			{
				INT i=ObjectName.InStr(PATH_SEPARATOR);
				if( i==-1 )
					i=ObjectName.InStr(TEXT("/"));
				if( i==-1 )
					break;
				ObjectName = ObjectName.Mid( i+1 );
			}
			if( ObjectName.InStr(TEXT("."))>=0 )
				ObjectName = ObjectName.Left( ObjectName.InStr(TEXT(".")) );
		}
		UFactory* Factory = NULL;
		if( Class->IsChildOf(UFactory::StaticClass()) )
			Factory = ConstructObject<UFactory>( Class );
		UObject* Object = UFactory::StaticImportObject
		(
			GEditor->Level,
			Factory ? Factory->SupportedClass : Class,
			CreatePackage(NULL,*(GroupName != TEXT("") ? (PackageName+TEXT(".")+GroupName) : PackageName)),
			*ObjectName,
			Flags,
			*FileName,
			Context,
			Factory,
			Str,
			GWarn
		);
		if( !Object )
			Ar.Logf( NAME_ExecWarning, TEXT("Failed factoring: %s"), InStr );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("LOAD") ) )
	{
		// Object file loading.
		if( Parse( Str, TEXT("FILE="), TempFname, 256) )
		{
			TCHAR PackageName[256]=TEXT("");
			UObject* Pkg=NULL;
			if( Parse( Str, TEXT("Package="), PackageName, ARRAY_COUNT(PackageName) ) )
			{
				TCHAR Temp[256], *End;
				appStrcpy( Temp, PackageName );
				End = appStrchr(Temp,'.');
				if( End )
					*End++ = 0;
				Pkg = CreatePackage( NULL, PackageName );
			}
 			Pkg = LoadPackage( Pkg, TempFname, GIsUCCMake ? LOAD_FindIfFail : 0 );
			if( *PackageName )
				ResetLoaders( Pkg, 0, 1 );
			if( !ParentContext )
				RedrawLevel(Level);
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("OBJ")) )//oldver
	{
		UClass* Type;
		if( ParseCommand( &Str, TEXT("LOAD") ) )//oldver
			return SafeExec( *(US+TEXT("LOAD ")+Str), Ar );
		else if( ParseCommand(&Str,TEXT("IMPORT")) )//oldver
			if( ParseObject<UClass>( Str, TEXT("TYPE="), Type, ANY_PACKAGE ) )
				return SafeExec( *(US+TEXT("NEW STANDALONE ")+Type->GetName()+TEXT(" ")+Str), Ar );
		return 0;
	}
	else if( ParseCommand( &Str, TEXT("MESHMAP")) )
	{
		appErrorf(TEXT("Deprecated command executed: %s"),Str);
	}
	else if( ParseCommand(&Str,TEXT("ANIM")) )
	{
		appErrorf(TEXT("Deprecated command executed: %s"),Str);
	}
	else if( ParseCommand(&Str,TEXT("MESH")) )
	{
		appErrorf(TEXT("Deprecated command executed: %s"),Str);
	}
	else if( ParseCommand( &Str, TEXT("AUDIO")) )
	{
		appErrorf(TEXT("Deprecated command executed: %s"),Str);
	}
	return 0;
}

/*-----------------------------------------------------------------------------
	UnrealEd command line.
-----------------------------------------------------------------------------*/

//@hack: this needs to be cleaned up!
static UModel* GBrush = NULL;
static const TCHAR* GStream = NULL;
static TCHAR TempStr[MAX_EDCMD], TempFname[MAX_EDCMD], TempName[MAX_EDCMD], Temp[MAX_EDCMD];
static _WORD Word1, Word2, Word4;

UBOOL UEditorEngine::Exec_StaticMesh( const TCHAR* Str, FOutputDevice& Ar )
{
	if(ParseCommand(&Str,TEXT("FROM")))
	{
		if(ParseCommand(&Str,TEXT("SELECTION")))	// STATICMESH FROM SELECTION PACKAGE=<name> NAME=<name>
		{
			Trans->Begin(TEXT("STATICMESH FROM SELECTION"));
			Level->Modify();
			FinishAllSnaps(Level);

			FName PkgName = NAME_None;
			Parse( Str, TEXT("PACKAGE="), PkgName );
			FName Name = NAME_None;
			Parse( Str, TEXT("NAME="), Name );
			if( PkgName != NAME_None  )
			{
				UPackage* Pkg = CreatePackage(NULL,*PkgName);
				Pkg->bDirty = 1;

				FName GroupName = NAME_None;
				if( Parse( Str, TEXT("GROUP="), GroupName ) && GroupName!=NAME_None )
					Pkg = CreatePackage(Pkg,*GroupName);

				TArray<FStaticMeshTriangle>	Triangles;
				TArray<FStaticMeshMaterial>	Materials;

				for( TSelectedActorIterator It( GetLevel() ) ; It ; ++It )
				{
					AActor*	Actor = *It;

					//
					// Brush
					//

					if( Actor->IsBrush() )
					{
						ABrush* Brush = Cast<ABrush>(Actor);
						check(Brush);

						TArray<FStaticMeshTriangle> TempTriangles;
						GetBrushTriangles( TempTriangles, Materials, Brush, Brush->Brush );

						// Transform the static meshes triangles into worldspace.
						FMatrix LocalToWorld = Brush->LocalToWorld();

						for( INT x = 0 ; x < TempTriangles.Num() ; ++x )
						{
							FStaticMeshTriangle* Tri = &TempTriangles(x);
							for( INT y = 0 ; y < 3 ; ++y )
							{
								Tri->Vertices[y]	= Brush->LocalToWorld().TransformFVector( Tri->Vertices[y] );
								Tri->Vertices[y]	-= GetPivotLocation() - Actor->PrePivot;
							}
						}

						Triangles += TempTriangles;
					}
				}

				// Create the static mesh
				if( Triangles.Num() )
					CreateStaticMesh(Triangles,Materials,Pkg,Name);
			}

			Trans->End();
			RedrawLevel(Level);
			return 1;
		}
	}
	else if(ParseCommand(&Str,TEXT("TO")))
	{
		if(ParseCommand(&Str,TEXT("BRUSH")))
		{
			Trans->Begin(TEXT("STATICMESH TO BRUSH"));
			GBrush->Modify();

			// Find the first selected static mesh actor.

			AStaticMeshActor*	SelectedActor = NULL;

			for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
				if(Level->Actors(ActorIndex) && GSelectionTools.IsSelected( Level->Actors(ActorIndex) ) && Cast<AStaticMeshActor>(Level->Actors(ActorIndex)) != NULL)
				{
					SelectedActor = Cast<AStaticMeshActor>(Level->Actors(ActorIndex));
					break;
				}

			if(SelectedActor)
			{
				Level->Brush()->Location = SelectedActor->Location;
				SelectedActor->Location = FVector(0,0,0);

				CreateModelFromStaticMesh(Level->Brush()->Brush,SelectedActor);

				SelectedActor->Location = Level->Brush()->Location;
			}
			else
				Ar.Logf(TEXT("No suitable actors found."));

			Trans->End();
			RedrawLevel(Level);
			return 1;
		}
	}
	else if(ParseCommand(&Str,TEXT("REBUILD")))	// STATICMESH REBUILD
	{
		//
		// Forces a rebuild of the selected static mesh.
		//

		Trans->Begin(TEXT("staticmesh rebuild"));

		GWarn->BeginSlowTask(TEXT("Staticmesh rebuilding"),1 );

		UStaticMesh* sm = GSelectionTools.GetTop<UStaticMesh>();
		if( sm )
		{
			sm->Modify();
			sm->Build();
		}

		GWarn->EndSlowTask();

		Trans->End();
	}
	else if(ParseCommand(&Str,TEXT("SMOOTH")))	// STATICMESH SMOOTH
	{
		//
		// Hack to set the smoothing mask of the triangles in the selected static meshes to 1.
		//

		Trans->Begin(TEXT("staticmesh smooth"));

		GWarn->BeginSlowTask(TEXT("Smoothing static meshes"),1);

		for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
		{
			AStaticMeshActor*	Actor = Cast<AStaticMeshActor>(Level->Actors(ActorIndex));

			if(Actor && GSelectionTools.IsSelected( Actor ) && Actor->StaticMeshComponent->StaticMesh)
			{
				UStaticMesh*	StaticMesh = Actor->StaticMeshComponent->StaticMesh;

				StaticMesh->Modify();

				// Generate smooth normals.

				if(!StaticMesh->RawTriangles.Num())
					StaticMesh->RawTriangles.Load();

				for(INT i = 0;i < StaticMesh->RawTriangles.Num();i++)
					StaticMesh->RawTriangles(i).SmoothingMask = 1;

				StaticMesh->Build();

				StaticMesh->RawTriangles.Detach();
			}
		}

		GWarn->EndSlowTask();

		Trans->End();
	}
	else if(ParseCommand(&Str,TEXT("UNSMOOTH")))	// STATICMESH UNSMOOTH
	{
		//
		// Hack to set the smoothing mask of the triangles in the selected static meshes to 0.
		//

		Trans->Begin(TEXT("staticmesh unsmooth"));

		GWarn->BeginSlowTask(TEXT("Unsmoothing static meshes"),1);

		for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
		{
			AStaticMeshActor*	Actor = Cast<AStaticMeshActor>(Level->Actors(ActorIndex));

			if(Actor && GSelectionTools.IsSelected( Actor ) && Actor->StaticMeshComponent->StaticMesh)
			{
				UStaticMesh*	StaticMesh = Actor->StaticMeshComponent->StaticMesh;

				if(!StaticMesh->RawTriangles.Num())
					StaticMesh->RawTriangles.Load();

				StaticMesh->Modify();

				// Generate smooth normals.

				for(INT i = 0;i < StaticMesh->RawTriangles.Num();i++)
				{
					FStaticMeshTriangle&	Triangle1 = StaticMesh->RawTriangles(i);

					Triangle1.SmoothingMask = 0;
				}

				StaticMesh->Build();

				StaticMesh->RawTriangles.Detach();
			}
		}

		GWarn->EndSlowTask();

		Trans->End();
	}
	else if( ParseCommand(&Str,TEXT("DEFAULT")) )	// STATICMESH DEFAULT NAME=<name>
	{
		GSelectionTools.SelectNone<UStaticMesh>();
		UStaticMesh* sm;
		ParseObject<UStaticMesh>(Str,TEXT("NAME="),sm,ANY_PACKAGE);
		GSelectionTools.Select( sm );
		return 1;
	}

	// Take the currently selected static mesh, and save the builder brush as its
	// low poly collision model.
	else if( ParseCommand(&Str,TEXT("SAVEBRUSHASCOLLISION")) )
	{
		// First, find the currently selected actor with a static mesh.
		// Fail if more than one actor with staticmesh is selected.
		UStaticMesh* StaticMesh = NULL;
		FMatrix MeshToWorld;
		for( TSelectedActorIterator It( Level ) ; It ; ++It )
		{
			AActor* Actor = *It;
			UStaticMesh* FoundMesh = NULL;

			AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(Actor);
			if(SMActor)
			{
				FoundMesh = SMActor->StaticMeshComponent->StaticMesh;
			}

			if(FoundMesh)
			{
				// If we find multiple actors with static meshes, warn and do nothing.
				if(StaticMesh)
				{
					appMsgf( 0, TEXT("Please select just one Actor with a StaticMesh.") );
					return 1;
				}

				StaticMesh = FoundMesh;
				MeshToWorld = SMActor->StaticMeshComponent->LocalToWorld;
			}
		}

		// If no static-mesh-toting actor found, warn and do nothing.
		if(!StaticMesh)
		{
			appMsgf( 0, TEXT("No Actor found with a StaticMesh.") );
			return 1;
		}

		// If we already have a collision model for this staticmesh, ask if we want to replace it.
		if(StaticMesh->CollisionModel)
		{
			UBOOL doReplace = appMsgf(1, TEXT("Static Mesh already has a collision model. \nDo you want to replace it with Builder Brush?"));
			if(doReplace)
				StaticMesh->CollisionModel = NULL;
			else
				return 1;
		}

		// Now get the builder brush.
		UModel* builderModel = Level->Brush()->Brush;

		// Need the transform between builder brush space and static mesh actor space.
		FMatrix BrushL2W = Level->Brush()->LocalToWorld();
		FMatrix MeshW2L = MeshToWorld.Inverse();
		FMatrix SMToBB = BrushL2W * MeshW2L;

		// Copy the current builder brush into the static mesh.
		StaticMesh->CollisionModel = new(StaticMesh->GetOuter()) UModel(NULL,1);
		StaticMesh->CollisionModel->Polys = new(StaticMesh->GetOuter()) UPolys;
		StaticMesh->CollisionModel->Polys->Element = builderModel->Polys->Element;

		// Now transform each poly into local space for the selected static mesh.
		for(INT i=0; i<StaticMesh->CollisionModel->Polys->Element.Num(); i++)
		{
			FPoly* Poly = &StaticMesh->CollisionModel->Polys->Element(i);

			for(INT j=0; j<Poly->NumVertices; j++ )
			{
				Poly->Vertex[j]  = SMToBB.TransformFVector(Poly->Vertex[j]);
			}

			Poly->Normal = SMToBB.TransformNormal(Poly->Normal);
			Poly->Normal.Normalize(); // SmToBB might have scaling in it.
		}

		// Build bounding box.
		StaticMesh->CollisionModel->BuildBound();

		// Build BSP for the brush.
		GEditor->bspBuild(StaticMesh->CollisionModel,BSP_Good,15,70,1,0);
		GEditor->bspRefresh(StaticMesh->CollisionModel,1);
		GEditor->bspBuildBounds(StaticMesh->CollisionModel);

		// Now - use this as the Rigid Body collision for this static mesh as well.

		// If we dont already have physics props, construct them here.
		if(!StaticMesh->BodySetup)
		{
			 StaticMesh->BodySetup = ConstructObject<URB_BodySetup>(URB_BodySetup::StaticClass(), StaticMesh);
		}

		// Convert collision model into a collection of convex hulls.
		// NB: This removes any convex hulls that were already part of the collision data.
		KModelToHulls(&StaticMesh->BodySetup->AggGeom, StaticMesh->CollisionModel, FVector(0, 0, 0));

		// Finally mark the parent package as 'dirty', so user will be prompted if they want to save it etc.
		StaticMesh->MarkPackageDirty();

		Ar.Logf(TEXT("Added collision model to StaticMesh %s."), StaticMesh->GetName() );
	}

	return 0;

}

UBOOL UEditorEngine::Exec_Brush( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("APPLYTRANSFORM")) )
		return Exec( TEXT("ACTOR APPLYTRANSFORM" ) );
	else if( ParseCommand(&Str,TEXT("SET")) )
	{
		Trans->Begin( TEXT("Brush Set") );
		GBrush->Modify();
		FRotator Temp(0.0f,0.0f,0.0f);
		Constraints.Snap( NULL, Level->Brush()->Location, FVector(0.f,0.f,0.f), Temp );
		Level->Brush()->Location -= Level->Brush()->PrePivot;
		Level->Brush()->PrePivot = FVector(0.f,0.f,0.f);
		GBrush->Polys->Element.Empty();
		UPolysFactory* It = new UPolysFactory;
		It->FactoryCreateText( Level,UPolys::StaticClass(), GBrush->Polys->GetOuter(), GBrush->Polys->GetName(), 0, GBrush->Polys, TEXT("t3d"), GStream, GStream+appStrlen(GStream), GWarn );
		// Do NOT merge faces.
		bspValidateBrush( GBrush, 0, 1 );
		GBrush->BuildBound();
		Trans->End();
		NoteSelectionChange( Level );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("MORE")) )
	{
		Trans->Continue();
		GBrush->Modify();
		UPolysFactory* It = new UPolysFactory;
		It->FactoryCreateText( Level,UPolys::StaticClass(), GBrush->Polys->GetOuter(), GBrush->Polys->GetName(), 0, GBrush->Polys, TEXT("t3d"), GStream, GStream+appStrlen(GStream), GWarn );
		// Do NOT merge faces.
		bspValidateBrush( Level->Brush()->Brush, 0, 1 );
		GBrush->BuildBound();
		Trans->End();	
		RedrawLevel( Level );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("RESET")) )
	{
		Trans->Begin( TEXT("Brush Reset") );
		Level->Brush()->Modify();
		Level->Brush()->InitPosRotScale();
		Trans->End();
		RedrawLevel(Level);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SCALE")) )
	{
		Trans->Begin( TEXT("Brush Scale") );

		FVector Scale;
		GetFVECTOR( Str, Scale );
		if( !Scale.X ) Scale.X = 1;
		if( !Scale.Y ) Scale.Y = 1;
		if( !Scale.Z ) Scale.Z = 1;

		FVector InvScale( 1 / Scale.X, 1 / Scale.Y, 1 / Scale.Z );

		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			ABrush* Brush = Cast<ABrush>(Level->Actors(i));
			if( Brush && GSelectionTools.IsSelected( Brush ) && Brush->IsBrush() )
			{
				Brush->Brush->Modify();
				Brush->Brush->Polys->Element.ModifyAllItems();
				for( INT poly = 0 ; poly < Brush->Brush->Polys->Element.Num() ; poly++ )
				{
					FPoly* Poly = &(Brush->Brush->Polys->Element(poly));

					Poly->TextureU *= InvScale;
					Poly->TextureV *= InvScale;
					Poly->Base = ((Poly->Base - Brush->PrePivot) * Scale) + Brush->PrePivot;

					for( INT vtx = 0 ; vtx < Poly->NumVertices ; vtx++ )
						Poly->Vertex[vtx] = ((Poly->Vertex[vtx] - Brush->PrePivot) * Scale) + Brush->PrePivot;

					Poly->CalcNormal();
				}

				Brush->Brush->BuildBound();
			}
		}
		
		Trans->End();
		RedrawLevel(Level);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("MOVETO")) )
	{
		Trans->Begin( TEXT("Brush MoveTo") );
		Level->Brush()->Modify();
		GetFVECTOR( Str, Level->Brush()->Location );
		Trans->End();
		RedrawLevel(Level);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("MOVEREL")) )
	{
		Trans->Begin( TEXT("Brush MoveRel") );
		Level->Brush()->Modify();
		FVector TempVector( 0, 0, 0 );
		GetFVECTOR( Str, TempVector );
		Level->Brush()->Location.AddBounded( TempVector, HALF_WORLD_MAX1 );
		Trans->End();
		RedrawLevel(Level);
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("ADD")))
	{
		Trans->Begin( TEXT("Brush Add") );
		FinishAllSnaps(Level);
		INT DWord1=0;
		Parse( Str, TEXT("FLAGS="), DWord1 );
		Level->Modify();
		ABrush* NewBrush = csgAddOperation( Level->Brush(), Level, DWord1, CSG_Add );
		if( NewBrush )
			bspBrushCSG( NewBrush, Level->Model, DWord1, CSG_Add, 1 );
		Trans->End();
		Level->InvalidateModelGeometry();
		Level->UpdateComponents();
		RedrawLevel(Level);
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("ADDVOLUME"))) // BRUSH ADDVOLUME
	{
		Trans->Begin( TEXT("Brush AddVolume") );
		Level->Modify();
		FinishAllSnaps( Level );

		UClass* VolumeClass = NULL;
		ParseObject<UClass>( Str, TEXT("CLASS="), VolumeClass, ANY_PACKAGE );
		if( !VolumeClass || !VolumeClass->IsChildOf(AVolume::StaticClass()) )
			VolumeClass = AVolume::StaticClass();

		Level->Modify();
		AVolume* Actor = (AVolume*)Level->SpawnActor(VolumeClass,NAME_None,Level->Brush()->Location);
		if( Actor )
		{
			Actor->PreEditChange();

			csgCopyBrush
			(
				Actor,
				Level->Brush(),
				0,
				RF_Transactional,
				1
			);

			// Set the texture on all polys to NULL.  This stops invisible texture
			// dependencies from being formed on volumes.
			if( Actor->Brush )
				for( INT poly = 0 ; poly < Actor->Brush->Polys->Element.Num() ; ++poly )
				{
					FPoly* Poly = &(Actor->Brush->Polys->Element(poly));
					Poly->Material = NULL;
				}

			Actor->PostEditChange(NULL);
		}
		Trans->End();
		RedrawLevel(Level);
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("SUBTRACT"))) // BRUSH SUBTRACT
	{
		Trans->Begin( TEXT("Brush Subtract") );
		FinishAllSnaps(Level);
		Level->Modify();
		ABrush* NewBrush = csgAddOperation(Level->Brush(),Level,0,CSG_Subtract); // Layer
		if( NewBrush )
			bspBrushCSG( NewBrush, Level->Model, 0, CSG_Subtract, 1 );
		Trans->End();
		Level->InvalidateModelGeometry();
		Level->UpdateComponents();
		RedrawLevel(Level);
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("FROM"))) // BRUSH FROM INTERSECTION/DEINTERSECTION
	{
		if( ParseCommand(&Str,TEXT("INTERSECTION")) )
		{
			Ar.Log( TEXT("Brush from intersection") );
			Trans->Begin( TEXT("Brush From Intersection") );
			GBrush->Modify();
			FinishAllSnaps( Level );
			bspBrushCSG( Level->Brush(), Level->Model, 0, CSG_Intersect, 0 );
			Trans->End();
			Level->Brush()->ClearComponents();
			Level->Brush()->UpdateComponents();
			RedrawLevel( Level );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("DEINTERSECTION")) )
		{
			Ar.Log( TEXT("Brush from deintersection") );
			Trans->Begin( TEXT("Brush From Deintersection") );
			GBrush->Modify();
			FinishAllSnaps( Level );
			bspBrushCSG( Level->Brush(), Level->Model, 0, CSG_Deintersect, 0 );
			Trans->End();
			Level->Brush()->ClearComponents();
			Level->Brush()->UpdateComponents();
			RedrawLevel( Level );
			return 1;
		}
	}
	else if( ParseCommand (&Str,TEXT("NEW")) )
	{
		Trans->Begin( TEXT("Brush New") );
		GBrush->Modify();
		GBrush->Polys->Element.Empty();
		Trans->End();
		RedrawLevel( Level );
		return 1;
	}
	else if( ParseCommand (&Str,TEXT("LOAD")) ) // BRUSH LOAD
	{
		if( Parse( Str, TEXT("FILE="), TempFname, 256 ) )
		{
			Trans->Reset( TEXT("loading brush") );
			FVector TempVector = Level->Brush()->Location;
			LoadPackage( Level->GetOuter(), TempFname, 0 );
			Level->Brush()->Location = TempVector;
			bspValidateBrush( Level->Brush()->Brush, 0, 1 );
			Cleanse( 1, TEXT("loading brush") );
			return 1;
		}
	}
	else if( ParseCommand( &Str, TEXT("SAVE") ) )
	{
		if( Parse(Str,TEXT("FILE="),TempFname, 256) )
		{
			Ar.Logf( TEXT("Saving %s"), TempFname );
			SavePackage( Level->GetOuter(), GBrush, 0, TempFname, GWarn );
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("IMPORT")) )
	{
		if( Parse(Str,TEXT("FILE="),TempFname, 256) )
		{
			GWarn->BeginSlowTask( TEXT("Importing brush"), 1);
			Trans->Begin( TEXT("Brush Import") );
			GBrush->Polys->Modify();
			GBrush->Polys->Element.Empty();
			DWORD Flags=0;
			UBOOL Merge=0;
			ParseUBOOL( Str, TEXT("MERGE="), Merge );
			Parse( Str, TEXT("FLAGS="), Flags );
			GBrush->Linked = 0;
			ImportObject<UPolys>( GEditor->Level, GBrush->Polys->GetOuter(), GBrush->Polys->GetName(), 0, TempFname );
			if( Flags )
				for( Word2=0; Word2<TempModel->Polys->Element.Num(); Word2++ )
					GBrush->Polys->Element(Word2).PolyFlags |= Flags;
			for( INT i=0; i<GBrush->Polys->Element.Num(); i++ )
				GBrush->Polys->Element(i).iLink = i;
			if( Merge )
			{
				bspMergeCoplanars( GBrush, 0, 1 );
				bspValidateBrush( GBrush, 0, 1 );
			}
			Trans->End();
			GWarn->EndSlowTask();
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("EXPORT")))
	{
		if( Parse(Str,TEXT("FILE="),TempFname, 256) )
		{
			GWarn->BeginSlowTask( TEXT("Exporting brush"), 1);
			UExporter::ExportToFile( GBrush->Polys, NULL, TempFname, 0 );
			GWarn->EndSlowTask();
		}
		else Ar.Log(NAME_ExecWarning,TEXT("Missing filename"));
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("MERGEPOLYS")) ) // BRUSH MERGEPOLYS
	{
		// Merges the polys on all selected brushes
		GWarn->BeginSlowTask( TEXT(""), 1);
		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Merging polys on selected brushes") );
			ABrush* Actor = Cast<ABrush>(Level->Actors(i));
			if( Actor && GSelectionTools.IsSelected( Actor ) && Actor->IsBrush() )
				bspValidateBrush( Actor->Brush, 1, 1 );
		}
		RedrawLevel( Level );
		GWarn->EndSlowTask();
	}
	else if( ParseCommand(&Str,TEXT("SEPARATEPOLYS")) ) // BRUSH SEPARATEPOLYS
	{
		GWarn->BeginSlowTask( TEXT(""), 1);
		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Separating polys on selected brushes") );
			ABrush* Actor = Cast<ABrush>(Level->Actors(i));
			if( Actor && GSelectionTools.IsSelected( Actor ) && Actor->IsBrush() )
				bspUnlinkPolys( Actor->Brush );
		}
		RedrawLevel( Level );
		GWarn->EndSlowTask();
	}

	return 0;

}

UBOOL UEditorEngine::Exec_Paths( const TCHAR* Str, FOutputDevice& Ar )
{
	if (ParseCommand(&Str,TEXT("UNDEFINE")))
	{
		FPathBuilder builder;
		Trans->Reset( TEXT("Paths") );
		builder.undefinePaths( Level );
		RedrawLevel(Level);
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("DEFINE")))
	{
		FPathBuilder builder;
		Trans->Reset( TEXT("Paths") );
		builder.definePaths( Level );
		RedrawLevel(Level);
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("REVIEW")))
	{
		FPathBuilder builder;
		Trans->Reset( TEXT("Paths") );
		builder.ReviewPaths( Level );
		RedrawLevel(Level);
		return 1;
	}
	return 0;

}

UBOOL UEditorEngine::Exec_BSP( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand( &Str, TEXT("REBUILD")) ) // Bsp REBUILD [LAME/GOOD/OPTIMAL] [BALANCE=0-100] [LIGHTS] [MAPS] [REJECT]
	{
		Trans->Reset( TEXT("Rebuilding Bsp") ); // Not tracked transactionally
		Ar.Log(TEXT("Bsp Rebuild"));

		FRebuildOptions* Options = GRebuildTools.GetCurrent();

		GWarn->BeginSlowTask( TEXT("Building polygons"), 1);

		bspBuildFPolys( Level->Model, 1, 0 );

		GWarn->StatusUpdatef( 0, 0, TEXT("Merging planars") );
		bspMergeCoplanars( Level->Model, 0, 0 );

		GWarn->StatusUpdatef( 0, 0, TEXT("Partitioning") );
		bspBuild( Level->Model, Options->BspOpt, Options->Balance, Options->PortalBias, 0, 0 );

		GWarn->StatusUpdatef( 0, 0, TEXT("Building visibility zones") );
		TestVisibility( Level, Level->Model, 0, 0 );

		GWarn->StatusUpdatef( 0, 0, TEXT("Optimizing geometry") );
		bspOptGeom( Level->Model );

		// Empty EdPolys.
		Level->Model->Polys->Element.Empty();

		GWarn->EndSlowTask();

		RedrawLevel(Level);
		GCallback->Send( CALLBACK_MapChange, 0 );

		Level->InvalidateModelGeometry();

		return 1;
	}

	return 0;

}

UBOOL UEditorEngine::Exec_Light( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand( &Str, TEXT("APPLY") ) )
	{
		shadowIlluminateBsp(Level);
		RedrawLevel( Level );
		return 1;
	}

	return 0;

}

UBOOL UEditorEngine::Exec_Map( const TCHAR* Str, FOutputDevice& Ar )
{
	if (ParseCommand(&Str,TEXT("ROTGRID"))) // MAP ROTGRID [PITCH=..] [YAW=..] [ROLL=..]
	{
		FinishAllSnaps (Level);
		if( GetFROTATOR( Str, Constraints.RotGridSize, 1 ) )
			RedrawLevel(Level);
		GCallback->Send( CALLBACK_UpdateUI );
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("SELECT")))
	{
		Trans->Begin( TEXT("Select") );
		if( ParseCommand(&Str,TEXT("ADDS")) )
			mapSelectOperation( Level, CSG_Add );
		else if( ParseCommand(&Str,TEXT("SUBTRACTS")) )
			mapSelectOperation( Level, CSG_Subtract );
		else if( ParseCommand(&Str,TEXT("SEMISOLIDS")) )
			mapSelectFlags( Level, PF_Semisolid );
		else if( ParseCommand(&Str,TEXT("NONSOLIDS")) )
			mapSelectFlags( Level, PF_NotSolid );
		Trans->End ();
		RedrawLevel( Level );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("DELETE")) )
	{
		Exec( TEXT("ACTOR DELETE"), Ar );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("BRUSH")) )
	{
		if( ParseCommand (&Str,TEXT("GET")) )
		{
			Trans->Begin( TEXT("Brush Get") );
			mapBrushGet( Level );
			Trans->End();
			RedrawLevel( Level );
			return 1;
		}
		else if( ParseCommand (&Str,TEXT("PUT")) )
		{
			Trans->Begin( TEXT("Brush Put") );
			mapBrushPut( Level );
			Trans->End();
			RedrawLevel( Level );
			return 1;
		}
	}
	else if (ParseCommand(&Str,TEXT("SENDTO")))
	{
		if( ParseCommand(&Str,TEXT("FIRST")) )
		{
			Trans->Begin( TEXT("Map SendTo Front") );
			mapSendToFirst( Level );
			Trans->End();
			RedrawLevel( Level );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("LAST")) )
		{
			Trans->Begin( TEXT("Map SendTo Back") );
			mapSendToLast( Level );
			Trans->End();
			RedrawLevel( Level );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("SWAP")) )
		{
			Trans->Begin( TEXT("Map SendTo Swap") );
			mapSendToSwap( Level );
			Trans->End();
			RedrawLevel( Level );
			return 1;
		}
	}
	else if( ParseCommand(&Str,TEXT("REBUILD")) )
	{
		Trans->Reset( TEXT("rebuilding map") );
		GWarn->BeginSlowTask( TEXT("Rebuilding geometry"), 1);
		Level->GetLevelInfo()->bPathsRebuilt = 0;

		csgRebuild( Level );

		Level->InvalidateModelGeometry();

		GWarn->StatusUpdatef( 0, 0, TEXT("Cleaning up...") );

		RedrawLevel( Level );
		GCallback->Send( CALLBACK_MapChange, 1 );
		GWarn->EndSlowTask();

		return 1;
	}
	else if( ParseCommand (&Str,TEXT("NEW")) )
	{
		Trans->Reset( TEXT("clearing map") );
		ResetLoaders( Level->GetOuter(), 0, 1 );
		ClearComponents();
		Level->CleanupLevel();
        if ( ParseCommand( &Str, TEXT( "NOOUTSIDE" ) ) )
        {
		    Level = new( Level->GetOuter(), TEXT("MyLevel") )ULevel( this, 0 );
        }
        else
        {
            Level = new( Level->GetOuter(), TEXT("MyLevel") )ULevel( this, 1 );
        }
		NoteSelectionChange( Level );
		GCallback->Send( CALLBACK_MapChange, 1 );
		Cleanse( 1, TEXT("starting new map") );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("LOAD") ) )
	{
		if( Parse( Str, TEXT("FILE="), TempFname, 256 ) )
		{
			Trans->Reset( TEXT("loading map") );
			GWarn->BeginSlowTask( TEXT("Loading map"), 1);
			ResetLoaders( Level->GetOuter(), 0, 1 );
			EdClearLoadErrors();
			ClearComponents();
			Level->CleanupLevel();

			LoadPackage( Level->GetOuter(), TempFname, 0 );
			Level->Engine = this;
			//GTerrainTools.Init();
			bspValidateBrush( Level->Brush()->Brush, 0, 1 );
			GWarn->EndSlowTask();
			// Hack fix for actor location being undefined (NAN)
			for ( INT i=0; i<Level->Actors.Num(); i++ )
				if ( Level->Actors(i) && (Level->Actors(i)->Location != Level->Actors(i)->Location) )
					Level->DestroyActor(Level->Actors(i));
			GCallback->Send( CALLBACK_MapChange, 1 );
			NoteSelectionChange( Level );
			Level->SetFlags( RF_Transactional );
			Level->Model->SetFlags( RF_Transactional );
			if( Level->Model->Polys ) Level->Model->Polys->SetFlags( RF_Transactional );

			Cleanse( 0, TEXT("loading map") );

			// Look for 'orphan' actors - that is, actors which are in the Package of the level we just loaded, but not in the Actors array.
			// If we find any, set bDeleteMe to 'true', so that PendingKill will return 'true' for them
			DOUBLE StartTime = appSeconds();
			UObject* LevelPackage = Level->GetOutermost();
			for( TObjectIterator<AActor> It; It; ++It )
			{
				UObject* Obj = *It;
				AActor* Actor = CastChecked<AActor>(Obj);

				// If Actor is part of the level we are loading's package, but not in Actor list, clear it
				if( (Actor->GetOutermost() == LevelPackage) && !Level->Actors.ContainsItem(Actor) && !Actor->bDeleteMe )
				{
					debugf( TEXT("Destroying orphan Actor: %s"), Actor->GetName() );					
					Actor->bDeleteMe = true;
				}
			}
			debugf( TEXT("Finished looking for orphan Actors (%3.3lf secs)"), appSeconds() - StartTime );

			// Ensure there are no pointers to any PendingKill objects.
			for( TObjectIterator<UObject> It; It; ++It )
			{
				UObject* Obj = *It;
				Obj->GetClass()->CleanupDestroyed( (BYTE*)Obj, Obj );
			}

			// Set Transactional flag and call PostEditLoad on all Actors in level
			for( INT i=0; i<Level->Actors.Num(); i++)
			{
				AActor* Actor = Level->Actors(i);
				if(Actor)
				{
					Actor->SetFlags( RF_Transactional );
					Actor->PostEditLoad();
				}
			}

			// rebuild the list of currently selected objects
			GSelectionTools.RebuildSelectedList();

			if( GEdLoadErrors.Num() )
				GCallback->Send( CALLBACK_DisplayLoadErrors );
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
		GCallback->Send( CALLBACK_MapChange, 1 );
		return 1;
	}
	else if( ParseCommand (&Str,TEXT("SAVE")) )
	{
		if( Parse(Str,TEXT("FILE="),TempFname, 256) )
		{
			FString SpacesTest = TempFname;
			if( SpacesTest.InStr( TEXT(" ") ) != INDEX_NONE )
				if( appMsgf( 0, TEXT("The filename you specified has one or more spaces in it.  Unreal filenames cannot contain spaces.") ) )
					return 0;

			INT Autosaving = 0;  // Are we autosaving?
			Parse(Str,TEXT("AUTOSAVE="),Autosaving);

			Level->ShrinkLevel();
			Level->CleanupDestroyed( 1 );
			ALevelInfo* OldInfo = FindObject<ALevelInfo>(Level->GetOuter(),TEXT("LevelInfo0"));
			if( OldInfo && OldInfo!=Level->GetLevelInfo() )
				OldInfo->Rename();
			if( Level->GetLevelInfo()!=OldInfo )
				Level->GetLevelInfo()->Rename(TEXT("LevelInfo0"));
			ULevelSummary* Summary = Level->GetLevelInfo()->Summary = new(Level->GetOuter(),TEXT("LevelSummary"),RF_Public)ULevelSummary;
			Summary->Title					= Level->GetLevelInfo()->Title;
			Summary->Author					= Level->GetLevelInfo()->Author;

			// Reset actor creation times.
			for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
				if(Level->Actors(ActorIndex))
					Level->Actors(ActorIndex)->CreationTime = 0.0f;

			// Check for duplicate actor indices.
			INT	NumDuplicates = 0;
			for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
			{
				AActor*	Actor = Level->Actors(ActorIndex);

				if(Actor)
					for(INT OtherActorIndex = ActorIndex + 1;OtherActorIndex < Level->Actors.Num();OtherActorIndex++)
						if(Level->Actors(OtherActorIndex) == Actor)
						{
							Level->Actors(OtherActorIndex) = NULL;
							NumDuplicates++;
						}
			}
			Level->CompactActors();
			debugf(TEXT("Removed duplicate actor indices: %u duplicates found."),NumDuplicates);

			if( !Autosaving )	GWarn->BeginSlowTask( TEXT("Saving map"), 1);
			if( !SavePackage( Level->GetOuter(), Level, 0, TempFname, GWarn ) )
				appMsgf( 0, TEXT("Couldn't save package - maybe file is read-only?") );
			if( !Autosaving )	GWarn->EndSlowTask();
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("IMPORT") ) )
	{
		Word1=1;
		DoImportMap:
		if( Parse( Str, TEXT("FILE="), TempFname, 256) )
		{
			Trans->Reset( TEXT("importing map") );
			GWarn->BeginSlowTask( TEXT("Importing map"), 1);
			ClearComponents();
			if( Level )
				Level->CleanupLevel();		
			if( Word1 )
				Level = new( Level->GetOuter(), TEXT("MyLevel") )ULevel( this, 1 );
			ImportObject<ULevel>( Level, Level->GetOuter(), Level->GetFName(), RF_Transactional, TempFname );
			if( Word1 )
				SelectNone( Level, 0 );
			GWarn->EndSlowTask();
			GCallback->Send( CALLBACK_MapChange, 1 );
			NoteSelectionChange( Level );
			Cleanse( 1, TEXT("importing map") );
		}
		else Ar.Log(NAME_ExecWarning,TEXT("Missing filename"));
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("IMPORTADD") ) )
	{
		Word1=0;
		SelectNone( Level, 0 );
		goto DoImportMap;
	}
	else if (ParseCommand (&Str,TEXT("EXPORT")))
	{
		if (Parse(Str,TEXT("FILE="),TempFname, 256))
		{
			GWarn->BeginSlowTask( TEXT("Exporting map"), 1);
			UBOOL bSelectedOnly = 0;
			ParseUBOOL( Str, TEXT("SELECTEDONLY="), bSelectedOnly );
			UExporter::ExportToFile( Level, NULL, TempFname, bSelectedOnly );
			GWarn->EndSlowTask();
		}
		else Ar.Log(NAME_ExecWarning,TEXT("Missing filename"));
		return 1;
	}
	else if (ParseCommand (&Str,TEXT("SETBRUSH"))) // MAP SETBRUSH (set properties of all selected brushes)
	{
		Trans->Begin( TEXT("Set Brush Properties") );

		Word1  = 0;  // Properties mask
		INT DWord1 = 0;  // Set flags
		INT DWord2 = 0;  // Clear flags
		INT CSGOper = 0;  // CSG Operation
		INT DrawType = 0;  // Draw type

		FName GroupName=NAME_None;
		if (Parse(Str,TEXT("CSGOPER="),CSGOper))		Word1 |= MSB_CSGOper;
		if (Parse(Str,TEXT("COLOR="),Word2))			Word1 |= MSB_BrushColor;
		if (Parse(Str,TEXT("GROUP="),GroupName))		Word1 |= MSB_Group;
		if (Parse(Str,TEXT("SETFLAGS="),DWord1))		Word1 |= MSB_PolyFlags;
		if (Parse(Str,TEXT("CLEARFLAGS="),DWord2))		Word1 |= MSB_PolyFlags;

		mapSetBrush(Level,(EMapSetBrushFlags)Word1,Word2,GroupName,DWord1,DWord2,CSGOper,DrawType);

		Trans->End();
		RedrawLevel(Level);

		return 1;
	}
	else if (ParseCommand (&Str,TEXT("CHECK")))
	{
		// Checks the map for common errors
		GWarn->MapCheck_Show();
		GWarn->MapCheck_Clear();

		GWarn->BeginSlowTask( TEXT("Checking map"), 1);
		for( INT i=0; i<GEditor->Level->Actors.Num(); i++ )
		{
			GWarn->StatusUpdatef( 0, i, TEXT("Checking map") );
			AActor* pActor = GEditor->Level->Actors(i);
			if( pActor )
				pActor->CheckForErrors();
		}
		GWarn->EndSlowTask();

		return 1;
	}
	else if (ParseCommand (&Str,TEXT("SCALE")))
	{
		FLOAT Factor = 1;
		if( !Parse(Str,TEXT("FACTOR="),Factor) ) return 0;

		Trans->Begin( TEXT("Map Scaling") );

		UBOOL bAdjustLights=0;
		ParseUBOOL( Str, TEXT("ADJUSTLIGHTS="), bAdjustLights );
		UBOOL bScaleSprites=0;
		ParseUBOOL( Str, TEXT("SCALESPRITES="), bScaleSprites );
		UBOOL bScaleLocations=0;
		ParseUBOOL( Str, TEXT("SCALELOCATIONS="), bScaleLocations );
		UBOOL bScaleCollision=0;
		ParseUBOOL( Str, TEXT("SCALECOLLISION="), bScaleCollision );

		GWarn->BeginSlowTask( TEXT("Scaling"), 1);
		NoteActorMovement( Level );
		for( INT i=0; i<GEditor->Level->Actors.Num(); i++ )
		{
			GWarn->StatusUpdatef( 0, i, TEXT("Scaling") );
			AActor* Actor = GEditor->Level->Actors(i);
			if( Actor && GSelectionTools.IsSelected( Actor ) )
			{
				Actor->PreEditChange();
				Actor->Modify();
				if( Actor->IsBrush() )
				{
					ABrush* Brush = Cast<ABrush>(Actor);

					Brush->Brush->Polys->Element.ModifyAllItems();
					for( INT poly = 0 ; poly < Brush->Brush->Polys->Element.Num() ; poly++ )
					{
						FPoly* Poly = &(Brush->Brush->Polys->Element(poly));

						Poly->TextureU /= Factor;
						Poly->TextureV /= Factor;
						Poly->Base = ((Poly->Base - Brush->PrePivot) * Factor) + Brush->PrePivot;

						for( INT vtx = 0 ; vtx < Poly->NumVertices ; vtx++ )
							Poly->Vertex[vtx] = ((Poly->Vertex[vtx] - Brush->PrePivot) * Factor) + Brush->PrePivot;

						Poly->CalcNormal();
					}

					Brush->Brush->BuildBound();
				}
				else
				{
					Actor->DrawScale *= Factor;
				}

				if( bScaleLocations )
				{
					Actor->Location.X *= Factor;
					Actor->Location.Y *= Factor;
					Actor->Location.Z *= Factor;
				}
				if( bScaleCollision )
				{
					// JAG_COLRADIUS_HACK
					//Actor->CollisionHeight *= Factor;
					//Actor->CollisionRadius *= Factor;
				}

				Actor->PostEditChange(NULL);
			}
		}
		GWarn->EndSlowTask();

		Trans->End();

		return 1;
	}

	return 0;

}

UBOOL UEditorEngine::Exec_Select( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("NONE")) )
	{
		Trans->Begin( TEXT("Select None") );
		SelectNone( Level, 1 );
		Trans->End();
		RedrawLevel( Level );
		return 1;
	}

	return 0;

}

UBOOL UEditorEngine::Exec_Poly( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("SELECT")) ) // POLY SELECT [ALL/NONE/INVERSE] FROM [LEVEL/SOLID/GROUP/ITEM/ADJACENT/MATCHING]
	{
		appSprintf( TempStr, TEXT("POLY SELECT %s"), Str );
		if( ParseCommand(&Str,TEXT("NONE")) )
		{
			return Exec( TEXT("SELECT NONE") );
		}
		else if( ParseCommand(&Str,TEXT("ALL")) )
		{
			Trans->Begin( TempStr );
			SelectNone( Level, 0 );
			polySelectAll( Level->Model );
			NoteSelectionChange( Level );
			Trans->End();
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("REVERSE")) )
		{
			Trans->Begin( TempStr );
			polySelectReverse (Level->Model);
			GCallback->Send( CALLBACK_SelChange );
			Trans->End();
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("MATCHING")) )
		{
			Trans->Begin( TempStr );
			if 		(ParseCommand(&Str,TEXT("GROUPS")))		polySelectMatchingGroups(Level->Model);
			else if (ParseCommand(&Str,TEXT("ITEMS")))		polySelectMatchingItems(Level->Model);
			else if (ParseCommand(&Str,TEXT("BRUSH")))		polySelectMatchingBrush(Level->Model);
			else if (ParseCommand(&Str,TEXT("TEXTURE")))	polySelectMatchingTexture(Level->Model);
			GCallback->Send( CALLBACK_SelChange );
			Trans->End();
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("ADJACENT")) )
		{
			Trans->Begin( TempStr );
			if 	  (ParseCommand(&Str,TEXT("ALL")))			polySelectAdjacents( Level->Model );
			else if (ParseCommand(&Str,TEXT("COPLANARS")))	polySelectCoplanars( Level->Model );
			else if (ParseCommand(&Str,TEXT("WALLS")))		polySelectAdjacentWalls( Level->Model );
			else if (ParseCommand(&Str,TEXT("FLOORS")))		polySelectAdjacentFloors( Level->Model );
			else if (ParseCommand(&Str,TEXT("CEILINGS")))	polySelectAdjacentFloors( Level->Model );
			else if (ParseCommand(&Str,TEXT("SLANTS")))		polySelectAdjacentSlants( Level->Model );
			GCallback->Send( CALLBACK_SelChange );
			Trans->End();
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("MEMORY")) )
		{
			Trans->Begin( TempStr );
			if 		(ParseCommand(&Str,TEXT("SET")))		polyMemorizeSet( Level->Model );
			else if (ParseCommand(&Str,TEXT("RECALL")))		polyRememberSet( Level->Model );
			else if (ParseCommand(&Str,TEXT("UNION")))		polyUnionSet( Level->Model );
			else if (ParseCommand(&Str,TEXT("INTERSECT")))	polyIntersectSet( Level->Model );
			else if (ParseCommand(&Str,TEXT("XOR")))		polyXorSet( Level->Model );
			GCallback->Send( CALLBACK_SelChange );
			Trans->End();
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("ZONE")) )
		{
			Trans->Begin( TempStr );
			polySelectZone(Level->Model);
			GCallback->Send( CALLBACK_SelChange );
			Trans->End();
			return 1;
		}
		RedrawLevel(Level);
	}
	else if( ParseCommand(&Str,TEXT("DEFAULT")) ) // POLY DEFAULT <variable>=<value>...
	{
		//CurrentMaterial=NULL;
		//ParseObject<UMaterial>(Str,TEXT("TEXTURE="),CurrentMaterial,ANY_PACKAGE);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("EXTRUDE")) )	// POLY EXTRUDE DEPTH=<value>
	{
		Trans->Begin( TEXT("Poly Extrude") );

		INT Depth;
		Parse( Str, TEXT("DEPTH="), Depth );

		Level->Modify();

		// Get a list of all the selected polygons.
		TArray<FPoly> SelectedPolys;	// The selected polygons.
		TArray<ABrush*> ActorList;		// The actors that own the polys (in synch with SelectedPolys)

		for( INT x = 0 ; x < Level->Model->Surfs.Num() ; x++ )
		{
			FBspSurf* Surf = &(Level->Model->Surfs(x));
			check(Surf->Actor);
			if( Surf->PolyFlags & PF_Selected )
			{
				FPoly Poly;
				if( polyFindMaster( Level->Model, x, Poly ) )
				{
					new( SelectedPolys )FPoly( Poly );
					ActorList.AddItem( Surf->Actor );
				}
			}
		}

		for( INT x = 0 ; x < SelectedPolys.Num() ; x++ )
		{
			ActorList(x)->Brush->Polys->Element.ModifyAllItems();

			// Find all the polys which are linked to create this surface.
			TArray<FPoly> PolyList;
			polyGetLinkedPolys( (ABrush*)ActorList(x), &SelectedPolys(x), &PolyList );

			// Get a list of the outer edges of this surface.
			TArray<FEdge> EdgeList;
			polyGetOuterEdgeList( &PolyList, &EdgeList );

			// Create new polys from the edges of the selected surface.
			for( INT edge = 0 ; edge < EdgeList.Num() ; edge++ )
			{
				FEdge* Edge = &EdgeList(edge);

				FVector v1 = Edge->Vertex[0],
					v2 = Edge->Vertex[1];

				FPoly NewPoly;
				NewPoly.Init();
				NewPoly.NumVertices = 4;
				NewPoly.Vertex[0] = v1;
				NewPoly.Vertex[1] = v2;
				NewPoly.Vertex[2] = v2 + (SelectedPolys(x).Normal * Depth);
				NewPoly.Vertex[3] = v1 + (SelectedPolys(x).Normal * Depth);

				new(ActorList(x)->Brush->Polys->Element)FPoly( NewPoly );
			}

			// Create the cap polys.
			for( INT pl = 0 ; pl < PolyList.Num() ; pl++ )
			{
				FPoly* PolyFromList = &PolyList(pl);

				for( INT poly = 0 ; poly < ActorList(x)->Brush->Polys->Element.Num() ; poly++ )
					if( *PolyFromList == ActorList(x)->Brush->Polys->Element(poly) )
					{
						FPoly* Poly = &(ActorList(x)->Brush->Polys->Element(poly));
						for( INT vtx = 0 ; vtx < Poly->NumVertices ; vtx++ )
							Poly->Vertex[vtx] += (SelectedPolys(x).Normal * Depth);
						break;
					}
			}

			// Clean up the polys.
			for( INT poly = 0 ; poly < ActorList(x)->Brush->Polys->Element.Num() ; poly++ )
			{
				FPoly* Poly = &(ActorList(x)->Brush->Polys->Element(poly));
				Poly->iLink = poly;
				Poly->Normal = FVector(0,0,0);
				Poly->Finalize(ActorList(x),0);
				Poly->Base = Poly->Vertex[0];
			}

			ActorList(x)->Brush->BuildBound();
		}

		GCallback->Send( CALLBACK_RedrawAllViewports );
		Trans->End();
	}
	else if( ParseCommand(&Str,TEXT("BEVEL")) )	// POLY BEVEL DEPTH=<value> BEVEL=<value>
	{
		Trans->Begin( TEXT("Poly Bevel") );

		INT Depth, Bevel;
		Parse( Str, TEXT("DEPTH="), Depth );
		Parse( Str, TEXT("BEVEL="), Bevel );

		Level->Modify();

		// Get a list of all the selected polygons.
		TArray<FPoly> SelectedPolys;	// The selected polygons.
		TArray<ABrush*> ActorList;		// The actors that own the polys (in synch with SelectedPolys)

		for( INT x = 0 ; x < Level->Model->Surfs.Num() ; x++ )
		{
			FBspSurf* Surf = &(Level->Model->Surfs(x));
			check(Surf->Actor);
			if( Surf->PolyFlags & PF_Selected )
			{
				FPoly Poly;
				if( polyFindMaster( Level->Model, x, Poly ) )
				{
					new( SelectedPolys )FPoly( Poly );
					ActorList.AddItem( Surf->Actor );
				}
			}
		}

		for( INT x = 0 ; x < SelectedPolys.Num() ; x++ )
		{
			ActorList(x)->Brush->Polys->Element.ModifyAllItems();

			// Find all the polys which are linked to create this surface.
			TArray<FPoly> PolyList;
			polyGetLinkedPolys( (ABrush*)ActorList(x), &SelectedPolys(x), &PolyList );

			// Get a list of the outer edges of this surface.
			TArray<FEdge> EdgeList;
			polyGetOuterEdgeList( &PolyList, &EdgeList );

			// Figure out where the center of the poly is.
			FVector PolyCenter = FVector(0,0,0);
			for( INT edge = 0 ; edge < EdgeList.Num() ; edge++ )
				PolyCenter += EdgeList(edge).Vertex[0];
			PolyCenter /= EdgeList.Num();

			// Create new polys from the edges of the selected surface.
			for( INT edge = 0 ; edge < EdgeList.Num() ; edge++ )
			{
				FEdge* Edge = &EdgeList(edge);

				FVector v1 = Edge->Vertex[0],
					v2 = Edge->Vertex[1];

				FPoly NewPoly;
				NewPoly.Init();
				NewPoly.NumVertices = 4;
				NewPoly.Vertex[0] = v1;
				NewPoly.Vertex[1] = v2;

				FVector CenterDir = PolyCenter - v2;
				CenterDir.Normalize();
				NewPoly.Vertex[2] = v2 + (SelectedPolys(x).Normal * Depth) + (CenterDir * Bevel);

				CenterDir = PolyCenter - v1;
				CenterDir.Normalize();
				NewPoly.Vertex[3] = v1 + (SelectedPolys(x).Normal * Depth) + (CenterDir * Bevel);

				new(ActorList(x)->Brush->Polys->Element)FPoly( NewPoly );
			}

			// Create the cap polys.
			for( INT pl = 0 ; pl < PolyList.Num() ; pl++ )
			{
				FPoly* PolyFromList = &PolyList(pl);

				for( INT poly = 0 ; poly < ActorList(x)->Brush->Polys->Element.Num() ; poly++ )
					if( *PolyFromList == ActorList(x)->Brush->Polys->Element(poly) )
					{
						FPoly* Poly = &(ActorList(x)->Brush->Polys->Element(poly));
						for( INT vtx = 0 ; vtx < Poly->NumVertices ; vtx++ )
						{
							FVector CenterDir = PolyCenter - Poly->Vertex[vtx];
							CenterDir.Normalize();
							Poly->Vertex[vtx] += (CenterDir * Bevel);

							Poly->Vertex[vtx] += (SelectedPolys(x).Normal * Depth);
						}
						break;
					}
			}

			// Clean up the polys.
			for( INT poly = 0 ; poly < ActorList(x)->Brush->Polys->Element.Num() ; poly++ )
			{
				FPoly* Poly = &(ActorList(x)->Brush->Polys->Element(poly));
				Poly->iLink = poly;
				Poly->Normal = FVector(0,0,0);
				Poly->Finalize(ActorList(x),0);
				Poly->Base = Poly->Vertex[0];
			}

			ActorList(x)->Brush->BuildBound();
		}

		GCallback->Send( CALLBACK_RedrawAllViewports );
		Trans->End();
	}
	else if( ParseCommand(&Str,TEXT("SETMATERIAL")) )
	{
		Trans->Begin( TEXT("Poly SetMaterial") );
		Level->Model->ModifySelectedSurfs(1);
		for( INT Index1=0; Index1<Level->Model->Surfs.Num(); Index1++ )
		{
			if( Level->Model->Surfs(Index1).PolyFlags & PF_Selected )
			{
				Level->Model->Surfs(Index1).Material = GSelectionTools.GetTop<UMaterialInstance>();
				polyUpdateMaster( Level->Model, Index1, 0 );
			}
		}
		Trans->End();
		RedrawLevel(Level);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SET")) ) // POLY SET <variable>=<value>...
	{
		Trans->Begin( TEXT("Poly Set") );
		Level->Model->ModifySelectedSurfs( 1 );
		DWORD Ptr;
		if( !Parse(Str,TEXT("TEXTURE="),Ptr) )   Ptr = 0;
		UMaterialInstance*	Material = (UMaterialInstance*)Ptr;
		if( Material )
		{
			for( INT x = 0 ; x < Level->Model->Surfs.Num() ; ++x )
			{
				if( Level->Model->Surfs(x).PolyFlags & PF_Selected )
				{
					Level->Model->Surfs(x).Material = Material;
					polyUpdateMaster( Level->Model, x, 0 );
				}
			}
		}
		Word4  = 0;
		INT DWord1 = 0;
		INT DWord2 = 0;
		if (Parse(Str,TEXT("SETFLAGS="),DWord1))   Word4=1;
		if (Parse(Str,TEXT("CLEARFLAGS="),DWord2)) Word4=1;
		if (Word4)  polySetAndClearPolyFlags (Level->Model,DWord1,DWord2,1,1); // Update selected polys' flags
		Trans->End();
		RedrawLevel(Level);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("TEXSCALE")) ) // POLY TEXSCALE [U=..] [V=..] [UV=..] [VU=..]
	{
		Trans->Begin( TEXT("Poly Texscale") );
		Level->Model->ModifySelectedSurfs( 1 );

		Word2 = 1; // Scale absolute
		if( ParseCommand(&Str,TEXT("RELATIVE")) )
			Word2=0;

		TexScale:

		// Ensure each polygon has unique texture vector indices.

		for(INT SurfaceIndex = 0;SurfaceIndex < Level->Model->Surfs.Num();SurfaceIndex++)
		{
			FBspSurf&	Surf = Level->Model->Surfs(SurfaceIndex);

			if(Surf.PolyFlags & PF_Selected)
			{
				FVector	TextureU = Level->Model->Vectors(Surf.vTextureU),
						TextureV = Level->Model->Vectors(Surf.vTextureV);

				Surf.vTextureU = Level->Model->Vectors.AddItem(TextureU);
				Surf.vTextureV = Level->Model->Vectors.AddItem(TextureV);
			}
		}

		FLOAT UU,UV,VU,VV;
		UU=1.0; Parse (Str,TEXT("UU="),UU);
		UV=0.0; Parse (Str,TEXT("UV="),UV);
		VU=0.0; Parse (Str,TEXT("VU="),VU);
		VV=1.0; Parse (Str,TEXT("VV="),VV);

		polyTexScale( Level->Model, UU, UV, VU, VV, Word2 );

		Trans->End();
		RedrawLevel( Level );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("TEXMULT")) ) // POLY TEXMULT [U=..] [V=..]
	{
		Trans->Begin( TEXT("Poly Texmult") );
		Level->Model->ModifySelectedSurfs( 1 );
		Word2 = 0; // Scale relative;
		goto TexScale;
	}
	else if( ParseCommand(&Str,TEXT("TEXPAN")) ) // POLY TEXPAN [RESET] [U=..] [V=..]
	{
		Trans->Begin( TEXT("Poly Texpan") );
		Level->Model->ModifySelectedSurfs( 1 );

		// Ensure each polygon has a unique base point index.

		for(INT SurfaceIndex = 0;SurfaceIndex < Level->Model->Surfs.Num();SurfaceIndex++)
		{
			FBspSurf&	Surf = Level->Model->Surfs(SurfaceIndex);

			if(Surf.PolyFlags & PF_Selected)
			{
				FVector	Base = Level->Model->Points(Surf.pBase);

				Surf.pBase = Level->Model->Points.AddItem(Base);
			}
		}

		if( ParseCommand (&Str,TEXT("RESET")) )
			polyTexPan( Level->Model, 0, 0, 1 );
		INT	PanU = 0; Parse (Str,TEXT("U="),PanU);
		INT	PanV = 0; Parse (Str,TEXT("V="),PanV);
		polyTexPan( Level->Model, PanU, PanV, 0 );
		Trans->End();
		RedrawLevel( Level );
		return 1;
	}

	return 0;

}

UBOOL UEditorEngine::Exec_Transaction( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("UNDO")) )
	{
		if( Trans->Undo() )
		{
			RedrawLevel( Level );
		}
		GCallback->Send( CALLBACK_Undo );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("REDO")) )
	{
		if( Trans->Redo() )
		{
			RedrawLevel(Level);
		}
		GCallback->Send( CALLBACK_Undo );
		return 1;
	}

	return 0;

}

UBOOL UEditorEngine::Exec_Obj( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("EXPORT")) )//oldver
	{
		FName Package=NAME_None;
		UClass* Type;
		UObject* Res;
		Parse( Str, TEXT("PACKAGE="), Package );
		if
		(	ParseObject<UClass>( Str, TEXT("TYPE="), Type, ANY_PACKAGE )
		&&	Parse( Str, TEXT("FILE="), TempFname, 256 )
		&&	ParseObject( Str, TEXT("NAME="), Type, Res, ANY_PACKAGE ) )
		{
			for( FObjectIterator It; It; ++It )
				It->ClearFlags( RF_TagImp | RF_TagExp );
			UExporter* Exporter = UExporter::FindExporter( Res, appFExt(TempFname) );
			if( Exporter )
			{
				Exporter->ParseParms( Str );
				UExporter::ExportToFile( Res, Exporter, TempFname, 0 );
				delete Exporter;
			}
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing file, name, or type") );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SavePackage")) )
	{
		UPackage* Pkg;
		UBOOL bSilent;
		if
		(	Parse( Str, TEXT("FILE="), TempFname, 256 )
		&&	ParseObject<UPackage>( Str, TEXT("Package="), Pkg, NULL ) )
		{
			ParseUBOOL( Str, TEXT("SILENT="), bSilent );

			FString SpacesTest = TempFname;
			if( SpacesTest.InStr( TEXT(" ") ) != INDEX_NONE )
				if( appMsgf( 0, TEXT("The filename you specified has one or more spaces in it.  Unreal filenames cannot contain spaces.") ) )
					return 0;

			if( !bSilent )
			{
				GWarn->BeginSlowTask( TEXT("Saving package"), 1);
				GWarn->StatusUpdatef( 1, 1, TEXT("Saving package...") );
			}

			SavePackage( Pkg, NULL, RF_Standalone, TempFname, GWarn );

			Pkg->bDirty = 0;
			if( !bSilent )
				GWarn->EndSlowTask();
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("Rename")) )
	{
		UObject* Object=NULL;
		UObject* OldPackage=NULL, *OldGroup=NULL;
		FString NewName, NewGroup, NewPackage;
		ParseObject<UObject>( Str, TEXT("OLDPACKAGE="), OldPackage, NULL );
		ParseObject<UObject>( Str, TEXT("OLDGROUP="), OldGroup, OldPackage );
		Cast<UPackage>(OldPackage)->bDirty = 1;
		if( OldGroup )
			OldPackage = OldGroup;
		ParseObject<UObject>( Str, TEXT("OLDNAME="), Object, OldPackage );
		Parse( Str, TEXT("NEWPACKAGE="), NewPackage );
		UPackage* Pkg = CreatePackage(NULL,*NewPackage);
		Pkg->bDirty = 1;
		if( Parse(Str,TEXT("NEWGROUP="),NewGroup) && NewGroup!=NAME_None )
			Pkg = CreatePackage( Pkg, *NewGroup );
		Parse( Str, TEXT("NEWNAME="), NewName );
		if( Object )
		{
			Object->Rename( *NewName, Pkg );
			Object->SetFlags(RF_Public|RF_Standalone);
		}
		return 1;
	}

	return 0;

}

UBOOL UEditorEngine::Exec_Class( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("SPEW")) )
	{
		GWarn->BeginSlowTask( TEXT("Exporting scripts"), 0);

		UObject* Package = NULL;
		ParseObject( Str, TEXT("PACKAGE="), Package, ANY_PACKAGE );

		for( TObjectIterator<UClass> It; It; ++It )
		{
			if( It->ScriptText )
			{
				// Check package
				if( Package )
				{
					UObject* Outer = It->GetOuter();
					while( Outer && Outer->GetOuter() )
						Outer = Outer->GetOuter();
					if( Outer != Package )
						continue;
				}

				// Create directory.
				FString Directory		= appGameDir() + TEXT("ExportedScript\\") + It->GetOuter()->GetName() + TEXT("\\Classes\\");
				GFileManager->MakeDirectory( *Directory, 1 );

				// Save file.
				FString Filename		= Directory + It->GetName() + TEXT(".uc");
				debugf( NAME_Log, TEXT("Spewing: %s"), Filename );
				UExporter::ExportToFile( *It, NULL, *Filename, 0 );
			}
		}

		GWarn->EndSlowTask();
		return 1;
	}
	return 0;
}

UBOOL UEditorEngine::Exec_Camera( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("ALIGN") ) )
	{
		// select the named actor
		if( Parse( Str, TEXT("NAME="), TempStr, NAME_SIZE ) )
		{
			AActor* Actor = NULL;
			for( INT i=0; i<Level->Actors.Num(); i++ )
			{
				Actor = Level->Actors(i);
				if( Actor && appStricmp( Actor->GetName(), TempStr ) == 0 && Actor!=Level->GetLevelInfo() )
				{
					if (Actor->IsHiddenEd())
					{
						GSelectionTools.Select( Actor, 1 );
					}
					else
					{
						SelectActor(Level,Actor,1);
					}
					break;
				}
			}
		}

		FVector NewLocation;
		if( Parse( Str, TEXT("X="), NewLocation.X ) )
		{
			Parse( Str, TEXT("Y="), NewLocation.Y );
			Parse( Str, TEXT("Z="), NewLocation.Z );

			for( INT i = 0; i < ViewportClients.Num(); i++ )
			{
				ViewportClients(i)->ViewLocation = NewLocation;
			}
		}
		else
		{
			// find the first selected actor as the target for the viewport cameras
			AActor* Target = NULL;
			for( INT i = 0; i < Level->Actors.Num(); i++ )
			{
				if( Level->Actors(i) && GSelectionTools.IsSelected( Level->Actors(i) ) )
				{
					Target = Level->Actors(i);
					break;
				}
			}
			if( Target == NULL )
			{
				Ar.Log( TEXT("Can't find target (viewport or selected actor)") );
				return 0;
			}

			// move all viewport cameras to the target actor
			FVector camLocation = Target->Location;
			// @todo attempt to find a good viewpoint for this actor
			camLocation += Target->Rotation.Vector() * -128.f + FVector(0,0,1) * 128.f;
			// and point the camera at the actor
			FRotator camRotation = (Target->Location-camLocation).Rotation();
			// and update all the viewports
			for( INT i = 0; i < ViewportClients.Num(); i++ )
			{
				if(ViewportClients(i)->GetLevel() == Level)
				{
					if (ViewportClients(i)->IsOrtho())
					{
						ViewportClients(i)->ViewLocation = Target->Location;
					}
					else
					{
						ViewportClients(i)->ViewLocation = camLocation;
						ViewportClients(i)->ViewRotation = camRotation;
					}
				}
			}
		}
		Ar.Log( TEXT("Aligned camera on the current target.") );
		RedrawAllViewports(1);
		NoteSelectionChange( Level );
		return 1;
	}
	else
		if( ParseCommand(&Str,TEXT("SELECT") ) )
	{
		if( Parse( Str, TEXT("NAME="), TempStr,NAME_SIZE ) )
		{
			AActor* Actor = NULL;
			for( INT i=0; i<Level->Actors.Num(); i++ )
			{
				Actor = Level->Actors(i);
				if( Actor && appStrcmp( Actor->GetName(), TempStr ) == 0 )
				{
					SelectActor( Level, Actor, 1, 0 );
					break;
				}
			}
			if( Actor == NULL )
			{
				Ar.Log( TEXT("Can't find the specified name.") );
				return 0;
			}

			for( INT i = 0; i < ViewportClients.Num(); i++ )
			{
				if(ViewportClients(i)->GetLevel() == Level)
					ViewportClients(i)->ViewLocation = Actor->Location - ViewportClients(i)->ViewRotation.Vector() * 48;
			}
			Ar.Log( TEXT("Aligned camera on named object.") );
			NoteSelectionChange( Level );
			return 1;
		}
	}

	return 0;

}

UBOOL UEditorEngine::Exec_Level( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("REDRAW")) )
	{
		RedrawLevel(Level);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("LINKS")) )
	{
		Results->Text.Empty();
		INT Internal=0,External=0;
		Results->Logf( TEXT("Level links:\r\n") );
		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			if( Cast<ATeleporter>(Level->Actors(i)) )
			{
				ATeleporter& Teleporter = *(ATeleporter *)Level->Actors(i);
				Results->Logf( TEXT("   %s\r\n"), *Teleporter.URL );
				if( appStrchr(*Teleporter.URL,'//') )
					External++;
				else
					Internal++;
			}
		}
		Results->Logf( TEXT("End, %i internal link(s), %i external.\r\n"), Internal, External );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("VALIDATE")) )
	{
		// Validate the level.
		Results->Text.Empty();
		Results->Log( TEXT("Level validation:\r\n") );

		// Make sure it's not empty.
		if( Level->Model->Nodes.Num() == 0 )
		{
			Results->Log( TEXT("Error: Level is empty!\r\n") );
			return 1;
		}

		// Find playerstart.
		INT i;
		for( i=0; i<Level->Actors.Num(); i++ )
			if( Cast<APlayerStart>(Level->Actors(i)) )
				break;
		if( i == Level->Actors.Num() )
		{
			Results->Log( TEXT("Error: Missing PlayerStart actor!\r\n") );
			return 1;
		}

		// Make sure PlayerStarts are outside.
		for( i=0; i<Level->Actors.Num(); i++ )
		{
			if( Cast<APlayerStart>(Level->Actors(i)) )
			{
				FCheckResult Hit(0.0f);
				if( !Level->Model->PointCheck( Hit, NULL, Level->Actors(i)->Location, FVector(0.f,0.f,0.f) ) )
				{
					Results->Log( TEXT("Error: PlayerStart doesn't fit!\r\n") );
					return 1;
				}
			}
		}

		// Check level title.
		if( Level->GetLevelInfo()->Title==TEXT("") )
		{
			Results->Logf( TEXT("Error: Level is missing a title!") );
			return 1;
		}
		else if( Level->GetLevelInfo()->Title==TEXT("Untitled") )
		{
			Results->Logf( TEXT("Warning: Level is untitled\r\n") );
		}

		// Check actors.
		for( i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor = Level->Actors(i);
			if( Actor )
			{
				check(Actor->GetClass()!=NULL);
				check(Actor->GetStateFrame());
				check(Actor->GetStateFrame()->Object==Actor);
				check(Actor->Level!=NULL);
				check(Actor->GetLevel()!=NULL);
				check(Actor->GetLevel()==Level);
				check(Actor->GetLevel()->Actors(0)!=NULL);
				check(Actor->GetLevel()->Actors(0)==Actor->Level);
			}
		}

		// Success.
		Results->Logf( TEXT("Success: Level validation succeeded!\r\n") );
		return 1;
	}

	return 0;

}

//
// Process an incoming network message meant for the editor server
//
UBOOL UEditorEngine::Exec( const TCHAR* Stream, FOutputDevice& Ar )
{
	//debugf("GEditor Exec: %s",Stream);
	TCHAR CommandTemp[MAX_EDCMD];
	TCHAR ErrorTemp[256]=TEXT("Setup: ");
	UBOOL Processed=0;

	// Echo the command to the log window
	if( appStrlen(Stream)<200 )
	{
		appStrcat( ErrorTemp, Stream );
		debugf( NAME_Cmd, Stream );
	}

	GStream = Stream;
	GBrush = Level ? Level->Brush()->Brush : NULL;

	appStrncpy( CommandTemp, Stream, 512 );
	const TCHAR* Str = &CommandTemp[0];

	appStrncpy( ErrorTemp, Str, 79 );
	ErrorTemp[79]=0;

	if( SafeExec( Stream, Ar ) )
	{
		return 1;
	}
	//------------------------------------------------------------------------------------
	// MISC
	//
	else if( ParseCommand(&Str,TEXT("EDCALLBACK")) )
	{
		if( ParseCommand(&Str,TEXT("SURFPROPS")) )
			GCallback->Send( CALLBACK_SurfProps );
		else if( ParseCommand(&Str,TEXT("ACTORPROPS")) )
			GCallback->Send( CALLBACK_ActorProps );
	}
	else if(ParseCommand(&Str,TEXT("STATICMESH")))
	{
		if( Exec_StaticMesh( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// BRUSH
	//
	else if( ParseCommand(&Str,TEXT("BRUSH")) )
	{
		if( Exec_Brush( Str, Ar ) )
			return 1;
	}
	//----------------------------------------------------------------------------------
	// PATHS
	//
	else if( ParseCommand(&Str,TEXT("PATHS")) )
	{
		if( Exec_Paths( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// BSP
	//
	else if( ParseCommand( &Str, TEXT("BSP") ) )
	{
		if( Exec_BSP( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// LIGHT
	//
	else if( ParseCommand( &Str, TEXT("LIGHT") ) )
	{
		if( Exec_Light( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// MAP
	//
	else if (ParseCommand(&Str,TEXT("MAP")))
	{
		if( Exec_Map( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// SELECT: Rerouted to mode-specific command
	//
	else if( ParseCommand(&Str,TEXT("SELECT")) )
	{
		if( Exec_Select( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// DELETE: Rerouted to mode-specific command
	//
	else if (ParseCommand(&Str,TEXT("DELETE")))
	{
		// Give the current mode a chance to handle this command.  If it doesn't,
		// do the default processing.

		if( !GEditorModeTools.GetCurrentMode()->ExecDelete() )
		{
			return Exec( TEXT("ACTOR DELETE") );
		}

		return 1;
	}
	//------------------------------------------------------------------------------------
	// DUPLICATE: Rerouted to mode-specific command
	//
	else if (ParseCommand(&Str,TEXT("DUPLICATE")))
	{
		return Exec( TEXT("ACTOR DUPLICATE") );
	}
	//------------------------------------------------------------------------------------
	// POLY: Polygon adjustment and mapping
	//
	else if( ParseCommand(&Str,TEXT("POLY")) )
	{
		if( Exec_Poly( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// ANIM: All mesh/animation management.
	//
	else if( ParseCommand(&Str,TEXT("NEWANIM")) )
	{
		appErrorf(TEXT("Deprecated command executed: %s"),Str);
	}
	//------------------------------------------------------------------------------------
	// Transaction tracking and control
	//
	else if( ParseCommand(&Str,TEXT("TRANSACTION")) )
	{
		if( Exec_Transaction( Str, Ar ) )
		{
			NoteSelectionChange( Level );
			GCallback->Send( CALLBACK_MapChange, 1 );

			return 1;
		}
	}
	//------------------------------------------------------------------------------------
	// General objects
	//
	else if( ParseCommand(&Str,TEXT("OBJ")) )
	{
		if( Exec_Obj( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// CLASS functions
	//
	else if( ParseCommand(&Str,TEXT("CLASS")) )
	{
		if( Exec_Class( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// CAMERA: cameras
	//
	else if( ParseCommand(&Str,TEXT("CAMERA")) )
	{
		if( Exec_Camera( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// LEVEL
	//
	if( ParseCommand(&Str,TEXT("LEVEL")) )
	{
		if( Exec_Level( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// Other handlers.
	//
	else if( Level && Level->Exec(Stream,Ar) )
	{
		// The level handled it.
		Processed = 1;
	}
	else if( UEngine::Exec(Stream,Ar) )
	{
		// The engine handled it.
		Processed = 1;
	}
	else if( ParseCommand(&Str,TEXT("SELECTNAME")) )
	{
		FName FindName=NAME_None;
		Parse( Str, TEXT("NAME="), FindName );
		for( INT i=0; i<Level->Actors.Num(); i++ )
			if( Level->Actors(i) )
				SelectActor( Level, Level->Actors(i), Level->Actors(i)->GetFName()==FindName, 0 );

		Processed = 1;
	}
	else if( ParseCommand(&Str,TEXT("DUMPINT")) )
	{
		while( *Str==' ' )
			Str++;
		UObject* Pkg = LoadPackage( NULL, Str, LOAD_AllowDll );
		if( Pkg )
		{
			TCHAR Tmp[512],Loc[512];
			appStrcpy( Tmp, Str );
			if( appStrchr(Tmp,'.') )
				*appStrchr(Tmp,'.') = 0;
			appStrcat( Tmp, TEXT(".int") );
			appStrcpy( Loc, appBaseDir() );
			appStrcat( Loc, Tmp );
			for( FObjectIterator It; It; ++It )
			{
				if( It->IsIn(Pkg) )
				{
					UClass* Class = Cast<UClass>( *It );
					if( Class )
					{
						if( Class->ClassFlags&CLASS_Localized )
						{
							// Generate localizable class defaults.
							for( TFieldIterator<UProperty> ItP(Class); ItP; ++ItP )
							{
								if( ItP->PropertyFlags & CPF_Localized )
								{
									for( INT i=0; i<ItP->ArrayDim; i++ )
									{
										FString	Value;
										if( ItP->ExportText( i, Value, &Class->Defaults(0), ItP->GetOuter()!=Class ? &Class->GetSuperClass()->Defaults(0) : NULL, Class, 0 ) )
										{
											FString	Key;
											if( ItP->ArrayDim!=1 )
												Key = FString::Printf( TEXT("%s[%i]"), ItP->GetName(), i );
											else
												Key = ItP->GetName();

											// Put strings with whitespace in quotes.

											if(Value.InStr(TEXT(" ")) != INDEX_NONE || Value.InStr(TEXT("\t")) != INDEX_NONE)
												Value = FString::Printf(TEXT("\"%s\""),*Value);

											GConfig->SetString( Class->GetName(), *Key, *Value, Loc );
										}
									}
								}
							}
						}
					}
					else
					{
						if( It->GetClass()->ClassFlags&CLASS_Localized )
						{
							// Generate localizable object properties.
							for( TFieldIterator<UProperty> ItP(It->GetClass()); ItP; ++ItP )
							{
								if( ItP->PropertyFlags & CPF_Localized )
								{
									for( INT i=0; i<ItP->ArrayDim; i++ )
									{
										FString	Value;
										if( ItP->ExportText( i, Value, (BYTE*)*It, &It->GetClass()->Defaults(0), *It, 0 ) )
										{
											FString	Key;
											if( ItP->ArrayDim!=1 )
												Key = FString::Printf( TEXT("%s[%i]"), ItP->GetName(), i );
											else
												Key = ItP->GetName();

											// Put strings with whitespace in quotes.

											if(Value.InStr(TEXT(" ")) != INDEX_NONE || Value.InStr(TEXT("\t")) != INDEX_NONE)
												Value = FString::Printf(TEXT("\"%s\""),*Value);

											GConfig->SetString( Class->GetName(), *Key, *Value, Loc );
										}
									}
								}
							}
						}
					}
				}
			}
			GConfig->Flush( 0 );
			Ar.Logf( TEXT("Generated %s"), Loc );
		}
		else Ar.Logf( TEXT("LoadPackage failed") );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("JUMPTO")) )
	{
		TCHAR A[32], B[32], C[32];
		ParseToken( Str, A, ARRAY_COUNT(A), 0 );
		ParseToken( Str, B, ARRAY_COUNT(B), 0 );
		ParseToken( Str, C, ARRAY_COUNT(C), 0 );
		for( INT i=0; i<ViewportClients.Num(); i++ )
			if(ViewportClients(i)->GetLevel() == Level)
				ViewportClients(i)->ViewLocation = FVector(appAtoi(A),appAtoi(B),appAtoi(C));
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SETCULLDISTANCE")) )
	{
		// SETCULLDISTANCE CULLDISTANCE=<cull distance>
		// Sets the CullDistance of the selected actors' PrimitiveComponents.

		FLOAT CullDistance = 0.0f;
		if( Parse(Str,TEXT("CULLDISTANCE="),CullDistance) )
		{
			for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
			{
				AActor* Actor = Level->Actors(ActorIndex);
				if(Actor && GSelectionTools.IsSelected(Actor))
				{
					for(INT ComponentIndex = 0;ComponentIndex < Actor->Components.Num();ComponentIndex++)
					{
						UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Actor->Components(ComponentIndex));
						if(Primitive)
						{
							Primitive->CullDistance = CullDistance;
						}
					}
				}
			}
		}
	}
	return Processed;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
