/*=============================================================================
	UnEdExp.cpp: Editor exporters.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EditorPrivate.h"
#include "UnTerrain.h"
#include "EngineSequenceClasses.h"

/*------------------------------------------------------------------------------
	UTextBufferExporterTXT implementation.
------------------------------------------------------------------------------*/

void UTextBufferExporterTXT::StaticConstructor()
{
	SupportedClass = UTextBuffer::StaticClass();
	bText = 1;
	new(FormatExtension)FString(TEXT("TXT"));
	new(FormatDescription)FString(TEXT("Text file"));

}
UBOOL UTextBufferExporterTXT::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	UTextBuffer* TextBuffer = CastChecked<UTextBuffer>( Object );
	FString Str( TextBuffer->Text );

	TCHAR* Start = const_cast<TCHAR*>(*Str);
	TCHAR* End   = Start + Str.Len();
	while( Start<End && (Start[0]=='\r' || Start[0]=='\n' || Start[0]==' ') )
		Start++;
	while( End>Start && (End [-1]=='\r' || End [-1]=='\n' || End [-1]==' ') )
		End--;
	*End = 0;

	Ar.Log( Start );

	return 1;
}
IMPLEMENT_CLASS(UTextBufferExporterTXT);

/*------------------------------------------------------------------------------
	USoundExporterWAV implementation.
------------------------------------------------------------------------------*/

void USoundExporterWAV::StaticConstructor()
{
	SupportedClass = USoundNodeWave::StaticClass();
	bText = 0;
	new(FormatExtension)FString(TEXT("WAV"));
	new(FormatDescription)FString(TEXT("Sound"));

}
UBOOL USoundExporterWAV::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn )
{
	USoundNodeWave* Sound = CastChecked<USoundNodeWave>( Object );
	Sound->RawData.Load();
	Ar.Serialize( &Sound->RawData(0), Sound->RawData.Num() );
	return 1;
}
IMPLEMENT_CLASS(USoundExporterWAV);

/*------------------------------------------------------------------------------
	UClassExporterUC implementation.
------------------------------------------------------------------------------*/

void UClassExporterUC::StaticConstructor()
{
	SupportedClass = UClass::StaticClass();
	bText = 1;
	new(FormatExtension)FString(TEXT("UC"));
	new(FormatDescription)FString(TEXT("UnrealScript"));

}
UBOOL UClassExporterUC::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	UClass* Class = CastChecked<UClass>( Object );

	// Export script text.
	check(Class->Defaults.Num());
	check(Class->ScriptText);
	UExporter::ExportToOutputDevice( Class->ScriptText, NULL, Ar, TEXT("txt"), TextIndent );

	// Export cpptext.
	if( Class->CppText )
	{
		Ar.Log( TEXT("\r\n\r\ncpptext\r\n{\r\n") );
		Ar.Log( *Class->CppText->Text );
		Ar.Log( TEXT("\r\n}\r\n") );
	}

	// Export default properties that differ from parent's.
	Ar.Log( TEXT("\r\n\r\ndefaultproperties\r\n{\r\n") );
	ExportProperties
	(
		Ar,
		Class,
		&Class->Defaults(0),
		TextIndent+4,
		Class->GetSuperClass(),
		Class->GetSuperClass() ? &Class->GetSuperClass()->Defaults(0) : NULL,
		Class
	);
	Ar.Log( TEXT("}\r\n") );

	return 1;
}
IMPLEMENT_CLASS(UClassExporterUC);

/*------------------------------------------------------------------------------
	UObjectExporterT3D implementation.
------------------------------------------------------------------------------*/

void UObjectExporterT3D::StaticConstructor()
{
	SupportedClass = UObject::StaticClass();
	bText = 1;
	new(FormatExtension)FString(TEXT("T3D"));
	new(FormatDescription)FString(TEXT("Unreal object text"));
}
UBOOL UObjectExporterT3D::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	Ar.Logf( TEXT("%sBegin Object Class=%s Name=%s\r\n"),appSpc(TextIndent), Object->GetClass()->GetName(), Object->GetName() );
	ExportProperties( Ar, Object->GetClass(), (BYTE*)Object, TextIndent+3,Object->GetClass(), &Object->GetClass()->Defaults(0), Object );
	Ar.Logf( TEXT("%sEnd Object\r\n"), appSpc(TextIndent) );
	return 1;
}
IMPLEMENT_CLASS(UObjectExporterT3D);

/*------------------------------------------------------------------------------
	UComponentExporterT3D implementation.
------------------------------------------------------------------------------*/

void UComponentExporterT3D::StaticConstructor()
{
	SupportedClass = UComponent::StaticClass();
	bText = 1;
	new(FormatExtension)FString(TEXT("T3D"));
	new(FormatDescription)FString(TEXT("Unreal component text"));

}
UBOOL UComponentExporterT3D::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	UClass*	OuterClass = Object->GetOuter()->GetClass();
	FName*	ComponentName = OuterClass->ComponentClassToNameMap.Find(Object->GetClass());

	Ar.Logf( TEXT("%sBegin Object Class=%s Name=%s\r\n"), appSpc(TextIndent), Object->GetClass()->GetName(), ComponentName ? **ComponentName : Object->GetName() );
	ExportProperties( Ar, Object->GetClass(), (BYTE*)Object, TextIndent+3, Object->GetClass(), &Object->GetClass()->Defaults(0), Object );
	Ar.Logf( TEXT("%sEnd Object\r\n"), appSpc(TextIndent) );

	return 1;
}
IMPLEMENT_CLASS(UComponentExporterT3D);

/*------------------------------------------------------------------------------
	UPolysExporterT3D implementation.
------------------------------------------------------------------------------*/

void UPolysExporterT3D::StaticConstructor()
{
	SupportedClass = UPolys::StaticClass();
	bText = 1;
	new(FormatExtension)FString(TEXT("T3D"));
	new(FormatDescription)FString(TEXT("Unreal poly text"));

}
UBOOL UPolysExporterT3D::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	UPolys* Polys = CastChecked<UPolys>( Object );

	Ar.Logf( TEXT("%sBegin PolyList\r\n"), appSpc(TextIndent) );
	for( INT i=0; i<Polys->Element.Num(); i++ )
	{
		FPoly* Poly = &Polys->Element(i);
		TCHAR TempStr[256];

		// Start of polygon plus group/item name if applicable.
		Ar.Logf( TEXT("%s   Begin Polygon"), appSpc(TextIndent) );
		if( Poly->ItemName != NAME_None )
			Ar.Logf( TEXT(" Item=%s"), *Poly->ItemName );
		if( Poly->Material )
			Ar.Logf( TEXT(" Texture=%s"), *Poly->Material->GetPathName() );
		if( Poly->PolyFlags != 0 )
			Ar.Logf( TEXT(" Flags=%i"), Poly->PolyFlags );
		if( Poly->iLink != INDEX_NONE )
			Ar.Logf( TEXT(" Link=%i"), Poly->iLink );
		Ar.Logf( TEXT("\r\n") );

		// All coordinates.
		Ar.Logf( TEXT("%s      Origin   %s\r\n"), appSpc(TextIndent), SetFVECTOR(TempStr,&Poly->Base) );
		Ar.Logf( TEXT("%s      Normal   %s\r\n"), appSpc(TextIndent), SetFVECTOR(TempStr,&Poly->Normal) );
		Ar.Logf( TEXT("%s      TextureU %s\r\n"), appSpc(TextIndent), SetFVECTOR(TempStr,&Poly->TextureU) );
		Ar.Logf( TEXT("%s      TextureV %s\r\n"), appSpc(TextIndent), SetFVECTOR(TempStr,&Poly->TextureV) );
		for( INT j=0; j<Poly->NumVertices; j++ )
			Ar.Logf( TEXT("%s      Vertex   %s\r\n"), appSpc(TextIndent), SetFVECTOR(TempStr,&Poly->Vertex[j]) );
		Ar.Logf( TEXT("%s   End Polygon\r\n"), appSpc(TextIndent) );
	}
	Ar.Logf( TEXT("%sEnd PolyList\r\n"), appSpc(TextIndent) );

	return 1;
}
IMPLEMENT_CLASS(UPolysExporterT3D);

/*------------------------------------------------------------------------------
	UModelExporterT3D implementation.
------------------------------------------------------------------------------*/

void UModelExporterT3D::StaticConstructor()
{
	SupportedClass = UModel::StaticClass();
	bText = 1;
	new(FormatExtension)FString(TEXT("T3D"));
	new(FormatDescription)FString(TEXT("Unreal model text"));

}
UBOOL UModelExporterT3D::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	UModel* Model = CastChecked<UModel>( Object );

	Ar.Logf( TEXT("%sBegin Brush Name=%s\r\n"), appSpc(TextIndent), Model->GetName() );
	UExporter::ExportToOutputDevice( Model->Polys, NULL, Ar, Type, TextIndent+3 );
	Ar.Logf( TEXT("%sEnd Brush\r\n"), appSpc(TextIndent) );

	return 1;
}
IMPLEMENT_CLASS(UModelExporterT3D);

/*------------------------------------------------------------------------------
	ULevelExporterT3D implementation.
------------------------------------------------------------------------------*/

void ULevelExporterT3D::StaticConstructor()
{
	SupportedClass = ULevel::StaticClass();
	bText = 1;
	new(FormatExtension)FString(TEXT("T3D"));
	new(FormatDescription)FString(TEXT("Unreal level text"));
	new(FormatExtension)FString(TEXT("COPY"));
	new(FormatDescription)FString(TEXT("Unreal level text"));

}
UBOOL ULevelExporterT3D::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	ULevel* Level = CastChecked<ULevel>( Object );

	for( FObjectIterator It; It; ++It )
		It->ClearFlags( RF_TagImp | RF_TagExp );

	Ar.Logf( TEXT("%sBegin Map\r\n"), appSpc(TextIndent) );

	UBOOL bAllActors = appStricmp(Type,TEXT("COPY"))!=0 && !bSelectedOnly;

	for( INT iActor=0; iActor<Level->Actors.Num(); iActor++ )
	{
		AActor* Actor = Level->Actors(iActor);
		if( Actor && ( bAllActors || GSelectionTools.IsSelected( Actor ) ) )
		{
			Ar.Logf( TEXT("%sBegin Actor Class=%s Name=%s\r\n"), appSpc(TextIndent), Actor->GetClass()->GetName(), Actor->GetName() );
			TextIndent += 3;

			TMap<FName,UComponent*>	ComponentMap;
			Actor->GetClass()->FindInstancedComponents(ComponentMap,(BYTE*)Actor,Actor);
			for(TMap<FName,UComponent*>::TIterator It(ComponentMap);It;++It)
			{
				Ar.Logf(TEXT("%s Begin Object Class=%s Name=%s\r\n"),appSpc(TextIndent),It.Value()->GetClass()->GetName(),*It.Key());
				ExportProperties( Ar, It.Value()->GetClass(), (BYTE*)It.Value(), TextIndent + 4, It.Value()->GetClass(), &It.Value()->GetClass()->Defaults(0), Actor );
				Ar.Logf(TEXT("%s End Object\r\n"),appSpc(TextIndent));
			}

			ExportProperties( Ar, Actor->GetClass(), (BYTE*)Actor, TextIndent, Actor->GetClass(), &Actor->GetClass()->Defaults(0), Actor );

			TextIndent -= 3;
			Ar.Logf( TEXT("%sEnd Actor\r\n"), appSpc(TextIndent) );
		}
	}

	// Export information about the first selected surface in the map.  Used for copying/pasting
	// information from poly to poly.
	Ar.Logf( TEXT("%sBegin Surface\r\n"), appSpc(TextIndent) );
	TCHAR TempStr[256];
	for( INT i=0; i<Level->Model->Surfs.Num(); i++ )
	{
		FBspSurf *Poly = &Level->Model->Surfs(i);
		if( Poly->PolyFlags&PF_Selected )
		{
			Ar.Logf( TEXT("%sTEXTURE=%s\r\n"), appSpc(TextIndent+3), *Poly->Material->GetPathName() );
			Ar.Logf( TEXT("%sBASE      %s\r\n"), appSpc(TextIndent+3), SetFVECTOR(TempStr,&(Level->Model->Points(Poly->pBase))) );
			Ar.Logf( TEXT("%sTEXTUREU  %s\r\n"), appSpc(TextIndent+3), SetFVECTOR(TempStr,&(Level->Model->Vectors(Poly->vTextureU))) );
			Ar.Logf( TEXT("%sTEXTUREV  %s\r\n"), appSpc(TextIndent+3), SetFVECTOR(TempStr,&(Level->Model->Vectors(Poly->vTextureV))) );
			Ar.Logf( TEXT("%sNORMAL    %s\r\n"), appSpc(TextIndent+3), SetFVECTOR(TempStr,&(Level->Model->Vectors(Poly->vNormal))) );
			Ar.Logf( TEXT("%sPOLYFLAGS=%d\r\n"), appSpc(TextIndent+3), Poly->PolyFlags );
			break;
		}
	}
	Ar.Logf( TEXT("%sEnd Surface\r\n"), appSpc(TextIndent) );

	Ar.Logf( TEXT("%sEnd Map\r\n"), appSpc(TextIndent) );


	return 1;
}
IMPLEMENT_CLASS(ULevelExporterT3D);

/*------------------------------------------------------------------------------
	ULevelExporterSTL implementation.
------------------------------------------------------------------------------*/

void ULevelExporterSTL::StaticConstructor()
{
	SupportedClass = ULevel::StaticClass();
	bText = 1;
	new(FormatExtension)FString(TEXT("STL"));
	new(FormatDescription)FString(TEXT("Stereolithography"));

}
UBOOL ULevelExporterSTL::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	ULevel* Level = CastChecked<ULevel>( Object );

	for( FObjectIterator It; It; ++It )
		It->ClearFlags( RF_TagImp | RF_TagExp );

	//
	// GATHER TRIANGLES
	//

	TArray<FVector> Triangles;

	// Specific actors can be exported
	for( INT iActor=0; iActor<Level->Actors.Num(); iActor++ )
	{
		ATerrain* T = Cast<ATerrain>(Level->Actors(iActor));
		if( T && ( !bSelectedOnly || GSelectionTools.IsSelected( T ) ) )
		{
			for( int y = 0 ; y < T->NumVerticesY-1 ; y++ )
			{
				for( int x = 0 ; x < T->NumVerticesX-1 ; x++ )
				{
					FVector P1	= T->GetWorldVertex(x,y);
					FVector P2	= T->GetWorldVertex(x,y+1);
					FVector P3	= T->GetWorldVertex(x+1,y+1);
					FVector P4	= T->GetWorldVertex(x+1,y);

					Triangles.AddItem( P4 );
					Triangles.AddItem( P1 );
					Triangles.AddItem( P2 );

					Triangles.AddItem( P3 );
					Triangles.AddItem( P2 );
					Triangles.AddItem( P4 );
				}
			}
		}

		AStaticMeshActor* Actor = Cast<AStaticMeshActor>(Level->Actors(iActor));
		if( Actor && ( !bSelectedOnly || GSelectionTools.IsSelected( Actor ) ) && Actor->StaticMeshComponent->StaticMesh )
		{
			if(!Actor->StaticMeshComponent->StaticMesh->RawTriangles.Num())
				Actor->StaticMeshComponent->StaticMesh->RawTriangles.Load();

			for( INT tri = 0 ; tri < Actor->StaticMeshComponent->StaticMesh->RawTriangles.Num() ; tri++ )
			{
				FStaticMeshTriangle* smt = &Actor->StaticMeshComponent->StaticMesh->RawTriangles(tri);

				for( INT v = 2 ; v > -1 ; v-- )
				{
					FVector vtx = Actor->LocalToWorld().TransformFVector( smt->Vertices[v] );
					Triangles.AddItem( vtx );
				}
			}
		}
	}

	// Selected BSP surfaces
	for( INT i=0;i<Level->Model->Nodes.Num();i++ )
	{
		FBspNode* Node = &Level->Model->Nodes(i);
		if( !bSelectedOnly || Level->Model->Surfs(Node->iSurf).PolyFlags&PF_Selected )
		{
			if( Node->NumVertices > 2 )
			{
				FVector vtx1(Level->Model->Points(Level->Model->Verts(Node->iVertPool+0).pVertex)),
					vtx2(Level->Model->Points(Level->Model->Verts(Node->iVertPool+1).pVertex)),
					vtx3;

				for( INT v = 2 ; v < Node->NumVertices ; v++ )
				{
					vtx3 = Level->Model->Points(Level->Model->Verts(Node->iVertPool+v).pVertex);

					Triangles.AddItem( vtx1 );
					Triangles.AddItem( vtx2 );
					Triangles.AddItem( vtx3 );

					vtx2 = vtx3;
				}
			}
		}
	}

	//
	// WRITE THE FILE
	//

	Ar.Logf( TEXT("%ssolid LevelBSP\r\n"), appSpc(TextIndent) );

	for( INT tri = 0 ; tri < Triangles.Num() ; tri += 3 )
	{
		FVector vtx[3];
		vtx[0] = Triangles(tri) * FVector(1,-1,1);
		vtx[1] = Triangles(tri+1) * FVector(1,-1,1);
		vtx[2] = Triangles(tri+2) * FVector(1,-1,1);

		FPlane Normal( vtx[0], vtx[1], vtx[2] );

		Ar.Logf( TEXT("%sfacet normal %1.6f %1.6f %1.6f\r\n"), appSpc(TextIndent+2), Normal.X, Normal.Y, Normal.Z );
		Ar.Logf( TEXT("%souter loop\r\n"), appSpc(TextIndent+4) );

		Ar.Logf( TEXT("%svertex %1.6f %1.6f %1.6f\r\n"), appSpc(TextIndent+6), vtx[0].X, vtx[0].Y, vtx[0].Z );
		Ar.Logf( TEXT("%svertex %1.6f %1.6f %1.6f\r\n"), appSpc(TextIndent+6), vtx[1].X, vtx[1].Y, vtx[1].Z );
		Ar.Logf( TEXT("%svertex %1.6f %1.6f %1.6f\r\n"), appSpc(TextIndent+6), vtx[2].X, vtx[2].Y, vtx[2].Z );

		Ar.Logf( TEXT("%sendloop\r\n"), appSpc(TextIndent+4) );
		Ar.Logf( TEXT("%sendfacet\r\n"), appSpc(TextIndent+2) );
	}

	Ar.Logf( TEXT("%sendsolid LevelBSP\r\n"), appSpc(TextIndent) );

	Triangles.Empty();

	return 1;
}
IMPLEMENT_CLASS(ULevelExporterSTL);

/*------------------------------------------------------------------------------
	UPrefabExporterT3D implementation.
------------------------------------------------------------------------------*/

void UPrefabExporterT3D::StaticConstructor()
{
	SupportedClass = UPrefab::StaticClass();
	bText = 1;
	new(FormatExtension)FString(TEXT("T3D"));
	new(FormatDescription)FString(TEXT("Unreal prefab text"));

}
UBOOL UPrefabExporterT3D::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	UPrefab* Prefab = CastChecked<UPrefab>( Object );

	Ar.Logf( TEXT("%s"), *Prefab->T3DText );

	return 1;
}
IMPLEMENT_CLASS(UPrefabExporterT3D);

/*------------------------------------------------------------------------------
	ULevelExporterOBJ implementation.
------------------------------------------------------------------------------*/

void ULevelExporterOBJ::StaticConstructor()
{
	SupportedClass = ULevel::StaticClass();
	bText = 1;
	new(FormatExtension)FString(TEXT("OBJ"));
	new(FormatDescription)FString(TEXT("Object File"));
}

static void ExportPolys( UPolys* Polys, INT &PolyNum, INT TotalPolys, FOutputDevice& Ar, FFeedbackContext* Warn )
{
    UMaterialInstance *DefaultMaterial = GEngine->DefaultMaterial;

    UMaterialInstance *CurrentMaterial;

    INT i;

    CurrentMaterial = DefaultMaterial;

	for (i = 0; i < Polys->Element.Num(); i++)
	{
        Warn->StatusUpdatef( PolyNum++, TotalPolys, TEXT("Exporting Level To OBJ") );

		FPoly *Poly = &Polys->Element(i);

        int j;

        if (
            (!Poly->Material && (CurrentMaterial != DefaultMaterial)) ||
            (Poly->Material && (Poly->Material != CurrentMaterial))
           )
        {
            FString Material;

            CurrentMaterial = Poly->Material;

            if( CurrentMaterial )
        	    Material = FString::Printf (TEXT("usemtl %s"), CurrentMaterial->GetName());
            else
        	    Material = FString::Printf (TEXT("usemtl DefaultMaterial"));

            Ar.Logf (TEXT ("%s\n"), *Material );
        }

		for (j = 0; j < Poly->NumVertices; j++)
        {
            // Transform to Lightwave's coordinate system
			Ar.Logf (TEXT("v %f %f %f\n"), Poly->Vertex[j].X, Poly->Vertex[j].Z, Poly->Vertex[j].Y);
        }

		FVector	TextureBase = Poly->Base;

        FVector	TextureX, TextureY;

		TextureX = Poly->TextureU / (FLOAT)CurrentMaterial->GetWidth();
		TextureY = Poly->TextureV / (FLOAT)CurrentMaterial->GetHeight();

		for (j = 0; j < Poly->NumVertices; j++)
        {
            // Invert the y-coordinate (Lightwave has their bitmaps upside-down from us).
    		Ar.Logf (TEXT("vt %f %f\n"),
			    (Poly->Vertex[j] - TextureBase) | TextureX, -((Poly->Vertex[j] - TextureBase) | TextureY));
        }

		Ar.Logf (TEXT("f "));

        // Reverse the winding order so Lightwave generates proper normals:
		for (j = Poly->NumVertices - 1; j >= 0; j--)
			Ar.Logf (TEXT("%i/%i "), (j - Poly->NumVertices), (j - Poly->NumVertices));

		Ar.Logf (TEXT("\n"));
	}
}


UBOOL ULevelExporterOBJ::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	ULevel* Level = CastChecked<ULevel> (Object);

	GEditor->bspBuildFPolys( Level->Model, 0, 0 );
	UPolys* Polys = Level->Model->Polys;

    UMaterialInstance *DefaultMaterial = GEngine->DefaultMaterial;

    UMaterialInstance *CurrentMaterial;

    INT i, j, TotalPolys;
    INT PolyNum;
    INT iActor;

    // Calculate the total number of polygons to export:

    PolyNum = 0;
    TotalPolys = Polys->Element.Num();

	for( iActor=0; iActor<Level->Actors.Num(); iActor++ )
	{
		AActor* Actor = Level->Actors(iActor);

        if( !Actor )
            continue;
        
		AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>( Actor );
        if( !StaticMeshActor || !StaticMeshActor->StaticMeshComponent || !StaticMeshActor->StaticMeshComponent->StaticMesh )
            continue;
         
        UStaticMesh* StaticMesh = StaticMeshActor->StaticMeshComponent->StaticMesh;

        TotalPolys += StaticMesh->RawTriangles.Num();
    }

    // Export the BSP

	Ar.Logf (TEXT("# OBJ File Generated by UnrealEd\n"));

	Ar.Logf (TEXT("o MyLevel\n"));
    Ar.Logf (TEXT("g BSP\n") );

    ExportPolys( Polys, PolyNum, TotalPolys, Ar, Warn );

    // Export the static meshes

    CurrentMaterial = DefaultMaterial;

	for( iActor=0; iActor<Level->Actors.Num(); iActor++ )
	{
		AActor* Actor = Level->Actors(iActor);

        if( !Actor )
            continue;
        
		AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>( Actor );
        if( !StaticMeshActor || !StaticMeshActor->StaticMeshComponent || !StaticMeshActor->StaticMeshComponent->StaticMesh )
            continue;

        FMatrix LocalToWorld = Actor->LocalToWorld();
         
    	Ar.Logf (TEXT("g %s\n"), Actor->GetName() );

        StaticMeshActor->StaticMeshComponent->StaticMesh->RawTriangles.Load();

	    for (i = 0; i < StaticMeshActor->StaticMeshComponent->StaticMesh->RawTriangles.Num(); i++)
	    {
            Warn->StatusUpdatef( PolyNum++, TotalPolys, TEXT("Exporting Level To OBJ") );

            const FStaticMeshTriangle &Triangle = StaticMeshActor->StaticMeshComponent->StaticMesh->RawTriangles(i);

            if (
                (!Triangle.LegacyMaterial && (CurrentMaterial != DefaultMaterial)) ||
                (Triangle.LegacyMaterial && (Triangle.LegacyMaterial != CurrentMaterial))
               )
            {
                FString Material;

                CurrentMaterial = StaticMeshActor->StaticMeshComponent->GetMaterial(Triangle.MaterialIndex); // sjs

                if( CurrentMaterial )
        	        Material = FString::Printf (TEXT("usemtl %s"), CurrentMaterial->GetName());
                else
        	        Material = FString::Printf (TEXT("usemtl DefaultMaterial"));

                Ar.Logf (TEXT ("%s\n"), *Material);
            }

		    for( j = 0; j < ARRAY_COUNT(Triangle.Vertices); j++ )
            {
                FVector V = Triangle.Vertices[j];

                V = LocalToWorld.TransformFVector( V );

                // Transform to Lightwave's coordinate system
			    Ar.Logf( TEXT("v %f %f %f\n"), V.X, V.Z, V.Y );
            }

		    for( j = 0; j < ARRAY_COUNT(Triangle.Vertices); j++ )
            {
                // Invert the y-coordinate (Lightwave has their bitmaps upside-down from us).
    		    Ar.Logf( TEXT("vt %f %f\n"), Triangle.UVs[j][0].X, -Triangle.UVs[j][0].Y );
            }

		    Ar.Logf (TEXT("f "));

		    for( j = 0; j < ARRAY_COUNT(Triangle.Vertices); j++ )
			    Ar.Logf (TEXT("%i/%i "), (j - ARRAY_COUNT(Triangle.Vertices)), (j - ARRAY_COUNT(Triangle.Vertices)));

		    Ar.Logf (TEXT("\n"));
	    }
	}

	Level->Model->Polys->Element.Empty();

	Ar.Logf (TEXT("# dElaernU yb detareneG eliF JBO\n"));
	return 1;
}

IMPLEMENT_CLASS(ULevelExporterOBJ);

/*------------------------------------------------------------------------------
	UPolysExporterOBJ implementation.
------------------------------------------------------------------------------*/

void UPolysExporterOBJ::StaticConstructor()
{
	SupportedClass = UPolys::StaticClass();
	bText = 1;
	new(FormatExtension)FString(TEXT("OBJ"));
	new(FormatDescription)FString(TEXT("Object File"));
}

UBOOL UPolysExporterOBJ::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
    UPolys* Polys = CastChecked<UPolys> (Object);

    INT PolyNum = 0;
    INT TotalPolys = Polys->Element.Num();

	Ar.Logf (TEXT("# OBJ File Generated by UnrealEd\n"));

    ExportPolys( Polys, PolyNum, TotalPolys, Ar, Warn );

	Ar.Logf (TEXT("# dElaernU yb detareneG eliF JBO\n"));

	return 1;
}

IMPLEMENT_CLASS(UPolysExporterOBJ);

/*------------------------------------------------------------------------------
	USequenceExporterT3D implementation.
------------------------------------------------------------------------------*/

void USequenceExporterT3D::StaticConstructor()
{
	SupportedClass = USequence::StaticClass();
	bText = 1;
	new(FormatExtension)FString(TEXT("T3D"));
	new(FormatDescription)FString(TEXT("Unreal sequence text"));
	new(FormatExtension)FString(TEXT("COPY"));
	new(FormatDescription)FString(TEXT("Unreal sequence text"));
}

UBOOL USequenceExporterT3D::ExportText(UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn)
{
	USequence* Sequence = CastChecked<USequence>( Object );

	// Iterate over all objects making sure they import/export flags are unset. 
	// These are used for ensuring we export each object only once etc.
	for( FObjectIterator It; It; ++It )
	{
		It->ClearFlags( RF_TagImp | RF_TagExp );
	}

	UBOOL bAsSingle = (appStricmp(Type,TEXT("T3D"))==0);

	DWORD OldUglyHackFlags = GUglyHackFlags;
	GUglyHackFlags |= 0x04; // Always export name relative to parent.

	// If exporting everything - just pass in the sequence.
	if(bAsSingle)
	{
		Ar.Logf( TEXT("%sBegin Object Class=%s Name=%s\r\n"), appSpc(TextIndent), Sequence->GetClass()->GetName(), Sequence->GetName() );
		TextIndent += 3;

		ExportProperties( Ar, Sequence->GetClass(), (BYTE*)Sequence, TextIndent, Sequence->GetClass(), &Sequence->GetClass()->Defaults(0), Sequence );

		TextIndent -= 3;
		Ar.Logf( TEXT("%sEnd Object\r\n"), appSpc(TextIndent) );
	}
	// If we want only a selection, iterate over to find the SequenceObjects we want.
	else
	{
		for(INT i=0; i<Sequence->SequenceObjects.Num(); i++)
		{
			USequenceObject* SeqObj = Sequence->SequenceObjects(i);
			if( SeqObj && GSelectionTools.IsSelected(SeqObj) )
			{
				Ar.Logf( TEXT("%sBegin Object Class=%s Name=%s\r\n"), appSpc(TextIndent), SeqObj->GetClass()->GetName(), SeqObj->GetName() );
				TextIndent += 3;

				ExportProperties( Ar, SeqObj->GetClass(), (BYTE*)SeqObj, TextIndent, SeqObj->GetClass(), &SeqObj->GetClass()->Defaults(0), SeqObj );

				TextIndent -= 3;
				Ar.Logf( TEXT("%sEnd Object\r\n"), appSpc(TextIndent) );
			}
		}
	}

	GUglyHackFlags = OldUglyHackFlags;

	return true;
}

IMPLEMENT_CLASS(USequenceExporterT3D);

/*------------------------------------------------------------------------------
	The end.
------------------------------------------------------------------------------*/
