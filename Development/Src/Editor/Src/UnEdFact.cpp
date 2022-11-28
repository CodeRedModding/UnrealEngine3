/*=============================================================================
	UnEdFact.cpp: Editor class factories.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EditorPrivate.h"
#include "EngineSoundClasses.h"
#include "EngineParticleClasses.h"
#include "EngineAnimClasses.h"
#include "EngineSequenceClasses.h"

// Needed for DDS support.
#pragma pack(push,8)
#include "..\..\..\External\DirectX9\Include\ddraw.h"
#pragma pack(pop)

/*------------------------------------------------------------------------------
	ULevelFactoryNew implementation.
------------------------------------------------------------------------------*/

void ULevelFactoryNew::StaticConstructor()
{
	SupportedClass			= ULevel::StaticClass();
	bCreateNew				= 0;
	Description				= TEXT("Level");
	LevelTitle				= TEXT("Untitled");
	Author					= TEXT("Unknown");

	new(GetClass(),TEXT("LevelTitle"),           RF_Public)UStrProperty(CPP_PROPERTY(LevelTitle          ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Author"),               RF_Public)UStrProperty(CPP_PROPERTY(Author              ), TEXT(""), CPF_Edit );

}
ULevelFactoryNew::ULevelFactoryNew()
{
}
void ULevelFactoryNew::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar << LevelTitle << Author;

}
UObject* ULevelFactoryNew::FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, FFeedbackContext* Warn )
{
	//!!needs updating: optionally close existing windows, create NEW level, create new wlevelframe
	GEditor->Trans->Reset( TEXT("clearing map") );
	GEditor->Level = new( GEditor->Level->GetOuter(), TEXT("MyLevel") )ULevel( GEditor, 1 );
	GEditor->Level->GetLevelInfo()->Title  = LevelTitle;
	GEditor->Level->GetLevelInfo()->Author = Author;

	GEditor->RedrawLevel( GEditor->Level );
	GEditor->NoteSelectionChange( GEditor->Level );
	GCallback->Send( CALLBACK_MapChange, 1 );
	GEditor->Cleanse( 1, TEXT("starting new map") );

	return GEditor->Level;
}
IMPLEMENT_CLASS(ULevelFactoryNew);

/*------------------------------------------------------------------------------
	UClassFactoryNew implementation.
------------------------------------------------------------------------------*/

void UClassFactoryNew::StaticConstructor()
{
	SupportedClass		= UClass::StaticClass();
	bCreateNew			= 1;
	Description			= TEXT("UnrealScript Class");
	Superclass			= AActor::StaticClass();
	new(GetClass(),TEXT("Superclass"),   RF_Public)UClassProperty (CPP_PROPERTY(Superclass  ), TEXT(""), CPF_Edit, UObject::StaticClass() );

	new(GetClass()->HideCategories) FName(TEXT("Object"));

}
UClassFactoryNew::UClassFactoryNew()
{
}
void UClassFactoryNew::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar << Superclass;

}
UObject* UClassFactoryNew::FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, FFeedbackContext* Warn )
{
	UClass*	Result = new(InParent,Name,Flags|RF_Public) UClass(Superclass);
	FString	ClassScript = FString::Printf(TEXT("class %s extends %s;\n"),*Name,Superclass->GetName());
	Result->ScriptText = new(Result,TEXT("ScriptText"),RF_NotForClient|RF_NotForServer) UTextBuffer(*ClassScript);
	return Result;
}
IMPLEMENT_CLASS(UClassFactoryNew);

/*------------------------------------------------------------------------------
	UTextureCubeFactoryNew implementation.
------------------------------------------------------------------------------*/

void UTextureCubeFactoryNew::StaticConstructor()
{
	SupportedClass		= UTextureCube::StaticClass();
	bCreateNew			= 1;
	Description			= TEXT("Cubemap");

	new(GetClass()->HideCategories) FName(TEXT("Object"));

}
UObject* UTextureCubeFactoryNew::FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, FFeedbackContext* Warn )
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}
IMPLEMENT_CLASS(UTextureCubeFactoryNew);

/*------------------------------------------------------------------------------
	UMaterialInstanceConstantFactoryNew implementation.
------------------------------------------------------------------------------*/

//
//	UMaterialInstanceConstantFactoryNew::StaticConstructor
//

#include "EngineMaterialClasses.h"

void UMaterialInstanceConstantFactoryNew::StaticConstructor()
{
	SupportedClass		= UMaterialInstanceConstant::StaticClass();
	bCreateNew			= 1;
	Description			= TEXT("MaterialInstanceConstant");
	new(GetClass()->HideCategories) FName(TEXT("Object"));
}

//
//	UMaterialInstanceConstantFactoryNew::FactoryCreateNew
//

UObject* UMaterialInstanceConstantFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

IMPLEMENT_CLASS(UMaterialInstanceConstantFactoryNew);

/*------------------------------------------------------------------------------
	UMaterialFactoryNew implementation.
------------------------------------------------------------------------------*/

//
//	UMaterialFactoryNew::StaticConstructor
//

void UMaterialFactoryNew::StaticConstructor()
{
	SupportedClass		= UMaterial::StaticClass();
	bCreateNew			= 1;
	Description			= TEXT("Material");
	new(GetClass()->HideCategories) FName(TEXT("Object"));
}

//
//	UMaterialFactoryNew::FactoryCreateNew
//

UObject* UMaterialFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

IMPLEMENT_CLASS(UMaterialFactoryNew);

/*------------------------------------------------------------------------------
	UClassFactoryUC implementation.
------------------------------------------------------------------------------*/

void UClassFactoryUC::StaticConstructor()
{
	SupportedClass = UClass::StaticClass();
	new(Formats)FString(TEXT("uc;Unreal class definitions"));
	bCreateNew = 0;
	bText  = 1;

}
UClassFactoryUC::UClassFactoryUC()
{
}
UObject* UClassFactoryUC::FactoryCreateText
(
	ULevel*				InLevel,
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	const TCHAR* InBuffer=Buffer;
	FString StrLine, ClassName, BaseClassName;

	// Validate format.
	if( Class != UClass::StaticClass() )
	{
		Warn->Logf( TEXT("Can only import classes"), Type );
		return NULL;
	}
	if( appStricmp(Type,TEXT("UC"))!=0 )
	{
		Warn->Logf( TEXT("Can't import classes from files of type '%s'"), Type );
		return NULL;
	}

	// Import the script text.
	TArray<FName> DependentOn;
	FStringOutputDevice ScriptText, DefaultPropText, CppText;
	while( ParseLine(&Buffer,StrLine,1) )
	{
		const TCHAR* Str=*StrLine, *Temp;
		if( ParseCommand(&Str,TEXT("cpptext")) )
		{
			ScriptText.Logf( TEXT("// (cpptext)\r\n// (cpptext)\r\n")  );
			ParseLine(&Buffer,StrLine,1);
			ParseNext( &Str );
			Str = *StrLine;
			if( *Str!='{' )
				appErrorf( TEXT("%s: Missing '{' after cpptext."), *ClassName );

			// Get cpptext.
			while( ParseLine(&Buffer,StrLine,1) )
			{
				ScriptText.Logf( TEXT("// (cpptext)\r\n")  );
				Str = *StrLine;
				if( *Str=='}' )
					break;
				CppText.Logf( TEXT("%s\r\n"), *StrLine );
			}
		}
		else
		if( ParseCommand(&Str,TEXT("defaultproperties")) )
		{
			// Get default properties text.
			while( ParseLine(&Buffer,StrLine,1) )
			{
				Str = *StrLine;
				ParseNext( &Str );
				if( *Str=='}' )
					break;
				DefaultPropText.Logf( TEXT("%s\r\n"), *StrLine );
			}
		}
		else
		{
			// Get script text.
			ScriptText.Logf( TEXT("%s\r\n"), *StrLine );

			// Stub out the comments.
			INT Pos = StrLine.InStr(TEXT("//"));
			if( Pos>=0 )
				StrLine = StrLine.Left( Pos );
			Str=*StrLine;

			// Get class name.
			if( ClassName==TEXT("") && (Temp=appStrfind(Str, TEXT("class")))!=0 )
			{
				Temp+=6;
				ParseToken( Temp, ClassName, 0 );
			}
			if
			(	BaseClassName==TEXT("")
			&&	(Temp=appStrfind(Str, TEXT("extends")))!=0 )
			{
				Temp+=7;
				ParseToken( Temp, BaseClassName, 0 );
				while( BaseClassName.Right(1)==TEXT(";") )
					BaseClassName = BaseClassName.LeftChop( 1 );
			}

			Temp = Str;
			while ((Temp=appStrfind(Temp, TEXT("dependson(")))!=0)
			{
				Temp+=10;
				TCHAR dependency[NAME_SIZE];
				INT i = 0;
				while (*Temp != ')' && i<NAME_SIZE)
					dependency[i++] = *Temp++;
				dependency[i] = 0;
				FName dependsOnName(dependency);
				DependentOn.AddUniqueItem(dependsOnName);
			}
		}
	}

	debugf(TEXT("Class: %s extends %s"),*ClassName,*BaseClassName);

	// Handle failure.
	if( ClassName==TEXT("") || (BaseClassName==TEXT("") && ClassName!=TEXT("Object")) )
	{
		Warn->Logf ( NAME_Error, 
				TEXT("Bad class definition '%s'/'%s'/%i/%i"), *ClassName, *BaseClassName, BufferEnd-InBuffer, appStrlen(InBuffer) );
		return NULL;
	}
	else if( ClassName!=*Name )
		Warn->Logf ( NAME_Error, 
				TEXT("Script vs. class name mismatch (%s/%s)"), *Name, *ClassName ); 

	UClass* ResultClass = FindObject<UClass>( InParent, *ClassName );
	if( ResultClass && (ResultClass->GetFlags() & RF_Native) )
	{
		// Gracefully update an existing hardcoded class.
		debugf( NAME_Log, TEXT("Updated native class '%s'"), *ResultClass->GetFullName() );
		ResultClass->ClassFlags &= ~(CLASS_Parsed | CLASS_Compiled);
	}
	else
	{
		// Create new class.
		ResultClass = new( InParent, *ClassName, Flags )UClass( NULL );

		// Find or forward-declare base class.
		ResultClass->SuperField = FindObject<UClass>( InParent, *BaseClassName );
		if( !ResultClass->SuperField )
			ResultClass->SuperField = FindObject<UClass>( ANY_PACKAGE, *BaseClassName );
		if( !ResultClass->SuperField )
			ResultClass->SuperField = new( InParent, *BaseClassName )UClass( ResultClass );
		debugf( NAME_Log, TEXT("Imported: %s"), *ResultClass->GetFullName() );
	}

	// Set class info.
	ResultClass->ScriptText      = new( ResultClass, TEXT("ScriptText"),   RF_NotForClient|RF_NotForServer )UTextBuffer( *ScriptText );
	ResultClass->DefaultPropText = DefaultPropText;
	ResultClass->DependentOn	 = DependentOn;
	if( CppText.Len() )
		ResultClass->CppText     = new( ResultClass, TEXT("CppText"),      RF_NotForClient|RF_NotForServer )UTextBuffer( *CppText );

	return ResultClass;
}
IMPLEMENT_CLASS(UClassFactoryUC);

/*------------------------------------------------------------------------------
	ULevelFactory.
------------------------------------------------------------------------------*/

static void ForceValid( ULevel* Level, UStruct* Struct, BYTE* Data )
{
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Struct); It; ++It )
	{
		for( INT i=0; i<It->ArrayDim; i++ )
		{
			BYTE* Value = Data + It->Offset + i*It->ElementSize;
			if( Cast<UObjectProperty>(*It) )
			{
				UObject*& Obj = *(UObject**)Value;
				if( Cast<AActor>(Obj) )
				{
					INT j;
					for( j=0; j<Level->Actors.Num(); j++ )
						if( Level->Actors(j)==Obj )
							break;
					if( j==Level->Actors.Num() )
					{
						debugf( NAME_Log, TEXT("Usurped %s"), Obj->GetClass()->GetName() );
						Obj = NULL;
					}
				}
			}
			else if( Cast<UStructProperty>(*It) )
			{
				ForceValid( Level, ((UStructProperty*)*It)->Struct, Value );
			}
		}
	}
}

void ULevelFactory::StaticConstructor()
{
	SupportedClass = ULevel::StaticClass();
	new(Formats)FString(TEXT("t3d;Unreal Level"));
	bCreateNew = 0;
	bText = 1;

}
ULevelFactory::ULevelFactory()
{
	bEditorImport = 1;
}
UObject* ULevelFactory::FactoryCreateText
(
	ULevel*				InLevel,
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	TMap<AActor*,FString> Map;

	// Create (or replace) the level object.
	InLevel->CompactActors();
	check(InLevel->Actors.Num()>1);
	check(Cast<ALevelInfo>(InLevel->Actors(0)));
	check(Cast<ABrush>(InLevel->Actors(1)));

	// Init actors.
	for( INT i=0; i<InLevel->Actors.Num(); i++ )
	{
		if( InLevel->Actors(i) )
		{
			InLevel->Actors(i)->bTempEditor = 1;

			if( GSelectionTools.IsSelected( InLevel->Actors(i) ) )
				GEditor->SelectActor( InLevel, InLevel->Actors(i), 0, 0 );
		}
	}
	// Assumes data is being imported over top of a new, valid map.
	ParseNext( &Buffer );
	if( !GetBEGIN( &Buffer, TEXT("MAP")) )
		return InLevel;

	// Import everything.
	INT ImportedActive=appStricmp(Type,TEXT("paste"))==0;
	FString StrLine;
	while( ParseLine(&Buffer,StrLine) )
	{
		const TCHAR* Str = *StrLine;
		if( GetEND(&Str,TEXT("MAP")) )
		{
			// End of brush polys.
			break;
		}
		else if( GetBEGIN(&Str,TEXT("BRUSH")) )
		{
			Warn->StatusUpdatef( 0, 0, TEXT("Importing Brushes") );
			TCHAR BrushName[NAME_SIZE];
			if( Parse(Str,TEXT("NAME="),BrushName,NAME_SIZE) )
			{
				ABrush* Actor;
				if( !ImportedActive )
				{
					// Parse the active brush, which has already been allocated.
					Actor          = InLevel->Brush();
					ImportedActive = 1;
				}
				else
				{
					// Parse a new brush which has not yet been allocated.
					Actor             = InLevel->SpawnBrush();
					GEditor->SelectActor( InLevel, Actor, 1, 0 );
					Actor->Brush      = new( InParent, NAME_None, RF_NotForClient|RF_NotForServer )UModel( NULL );			
				}

				// Import.
				Actor->SetFlags       ( RF_NotForClient | RF_NotForServer );
				Actor->Brush->SetFlags( RF_NotForClient | RF_NotForServer );
				UModelFactory* It = new UModelFactory;
				Actor->Brush = (UModel*)It->FactoryCreateText(InLevel,UModel::StaticClass(),InParent,Actor->Brush->GetFName(),0,Actor,Type,Buffer,BufferEnd,Warn);
				check(Actor->Brush);
				Actor->BrushComponent->Brush = Actor->Brush;
				if( (Actor->PolyFlags&PF_Portal) )
					Actor->PolyFlags |= PF_Invisible;
			}
		}
		else if( GetBEGIN(&Str,TEXT("ACTOR")) )
		{
			UClass* TempClass;
			if( ParseObject<UClass>( Str, TEXT("CLASS="), TempClass, ANY_PACKAGE ) )
			{
				// Get actor name.
				FName ActorName(NAME_None);
				Parse( Str, TEXT("NAME="), ActorName );

				// Make sure this name is unique.
				AActor* Found=NULL;
				if( ActorName!=NAME_None )
					Found = FindObject<AActor>( InParent, *ActorName );
				if( Found )
					Found->Rename();

				// Import it.
				AActor* Actor = InLevel->SpawnActor( TempClass, ActorName, FVector(0,0,0), TempClass->GetDefaultActor()->Rotation, NULL, 1, 0 );
				check(Actor);

				// Get property text.
				FString PropText, StrLine;
				while
				(	GetEND( &Buffer, TEXT("ACTOR") )==0
				&&	ParseLine( &Buffer, StrLine ) )
				{
					PropText += *StrLine;
					PropText += TEXT("\r\n");
				}
				Map.Set( Actor, *PropText );

				// Handle class.
				if( Cast<ALevelInfo>(Actor) )
				{
					// Copy the one LevelInfo to position #0.
					check(InLevel->Actors.Num()>0);
					INT iActor=0; InLevel->Actors.FindItem( Actor, iActor );
					InLevel->Actors(0)       = Actor;
					InLevel->Actors(iActor)  = NULL;
					InLevel->Actors.ModifyItem(0);
					InLevel->Actors.ModifyItem(iActor);
				}
				else if( Actor->GetClass()==ABrush::StaticClass() && !ImportedActive )
				{
					// Copy the active brush to position #1.
					INT iActor=0; InLevel->Actors.FindItem( Actor, iActor );
					InLevel->Actors(1)       = Actor;
					InLevel->Actors(iActor)  = NULL;
					InLevel->Actors.ModifyItem(1);
					InLevel->Actors.ModifyItem(iActor);
					ImportedActive = 1;
				}
			}
		}
		else if( GetBEGIN(&Str,TEXT("SURFACE")) )
		{
			FString PropText, StrLine;

			UMaterialInstance* Material = NULL;
			FVector Base, TextureU, TextureV, Normal;
			DWORD PolyFlags = 0;
			INT Check = 0;

			Base = FVector(0,0,0);
			TextureU = FVector(0,0,0);
			TextureV = FVector(0,0,0);
			Normal = FVector(0,0,0);

			do
			{
				if( ParseCommand(&Buffer,TEXT("TEXTURE")) )
				{
					Buffer++;	// Move past the '=' sign

					FString	TextureName(Buffer);
					
					TextureName = TextureName.Left(TextureName.InStr(TEXT("\r")));

					Material = Cast<UMaterialInstance>(StaticLoadObject( UMaterialInstance::StaticClass(), NULL, *TextureName, NULL, LOAD_NoWarn, NULL ));

					Check++;
				}
				else if( ParseCommand(&Buffer,TEXT("BASE")) )
				{
					GetFVECTOR( Buffer, Base );
					Check++;
				}
				else if( ParseCommand(&Buffer,TEXT("TEXTUREU")) )
				{
					GetFVECTOR( Buffer, TextureU );
					Check++;
				}
				else if( ParseCommand(&Buffer,TEXT("TEXTUREV")) )
				{
					GetFVECTOR( Buffer, TextureV );
					Check++;
				}
				else if( ParseCommand(&Buffer,TEXT("NORMAL")) )
				{
					GetFVECTOR( Buffer, Normal );
					Check++;
				}
				else if( ParseCommand(&Buffer,TEXT("POLYFLAGS")) )
				{
					Parse( Buffer, TEXT("="), PolyFlags );
					Check++;
				}
			}
			while
			(	GetEND( &Buffer, TEXT("SURFACE") )==0
				&&	ParseLine( &Buffer, StrLine ) );

			if( Check == 6 )
			{
				GEditor->Trans->Begin( TEXT("paste texture to surface") );

				for( INT i = 0 ; i < InLevel->Model->Surfs.Num() ; i++ )
				{
					FBspSurf* Surf = &InLevel->Model->Surfs(i);
					FVector SurfNormal = InLevel->Model->Vectors( Surf->vNormal );
					
					InLevel->Model->ModifySurf( i, 1 );

					if( Surf->PolyFlags & PF_Selected )
					{
						FVector NewBase, NewTextureU, NewTextureV;

						NewBase = Base;
						NewTextureU = TextureU;
						NewTextureV = TextureV;

						// Need to compensate for changes in the polygon normal
						FRotator		DeltaRot	= (Normal * -1).Rotation();
						FRotationMatrix RotMatrix( DeltaRot );
						DeltaRot += SurfNormal.Rotation();

						NewBase		= RotMatrix.TransformFVector( Base );
						NewTextureU = RotMatrix.TransformNormal( TextureU );
						NewTextureV = RotMatrix.TransformNormal( TextureV );

						Surf->Material = Material;
						Surf->pBase = GEditor->bspAddPoint( InLevel->Model, &NewBase, 1 );
						Surf->vTextureU = GEditor->bspAddVector( InLevel->Model, &NewTextureU, 0 );
						Surf->vTextureV = GEditor->bspAddVector( InLevel->Model, &NewTextureV, 0 );
						Surf->PolyFlags = PolyFlags;

						Surf->PolyFlags &= ~PF_Selected;

						GEditor->polyUpdateMaster( InLevel->Model, i, 1 );
					}
				}

				GEditor->Trans->End();
			}

		}
	}

	// Import actor properties.
	// We do this after creating all actors so that actor references can be matched up.
	check(Cast<ALevelInfo>(InLevel->Actors(0)));
	for( INT i=0; i<InLevel->Actors.Num(); i++ )
	{
		AActor* Actor = InLevel->Actors(i);
		if( Actor )
		{
			if(!Actor->bTempEditor)
				GEditor->SelectActor( InLevel, Actor, !Actor->bTempEditor, 0 );

			FString*	PropText = Map.Find(Actor);
			UBOOL		ActorChanged = 0;
			if( PropText )
			{
				Actor->PreEditChange();
				ImportObjectProperties( Actor->GetClass(), (BYTE*)Actor, InLevel, **PropText, InParent, Actor, Warn, 0, Actor );
				ActorChanged = 1;
			}

			if(Actor->Level != InLevel->Actors(0))
			{
				Actor->Level  = (ALevelInfo*)InLevel->Actors(0);
				Actor->Region = FPointRegion( (ALevelInfo*)InLevel->Actors(0) );

				ForceValid( InLevel, Actor->GetClass(), (BYTE*)Actor );
				ActorChanged = 1;
			}

			if(ActorChanged)
			{
				// Notify actor its properties have changed.
				Actor->PostEditChange(NULL);
			}

			// Copy brushes' model pointers over to their BrushComponent, to keep compatibility with old T3Ds.
			if(Actor->IsA(ABrush::StaticClass()))
			{
				ABrush*	Brush = Cast<ABrush>(Actor);
				check(Brush->BrushComponent);
				Brush->BrushComponent->Brush = Brush->Brush;
			}

			// Take this opportunity to clean up any CSG_Active brushes that might be
			// in the level.  These are useless unless they're the builder brush.
			if( Actor->IsStaticBrush()
					&& ((ABrush*)Actor)->CsgOper == CSG_Active
					&& ((ABrush*)Actor) != InLevel->Brush()
					&& !Actor->IsVolumeBrush() )
				InLevel->DestroyActor( Actor );
		}
	}
	return InLevel;
}
IMPLEMENT_CLASS(ULevelFactory);

// Bitmap compression types.
enum EBitmapCompression
{
	BCBI_RGB       = 0,
	BCBI_RLE8      = 1,
	BCBI_RLE4      = 2,
	BCBI_BITFIELDS = 3,
};

// .BMP file header.
#pragma pack(push,1)
struct FBitmapFileHeader
{
	_WORD bfType;
	DWORD bfSize;
	_WORD bfReserved1;
	_WORD bfReserved2;
	DWORD bfOffBits;
	friend FArchive& operator<<( FArchive& Ar, FBitmapFileHeader& H )
	{
		Ar << H.bfType << H.bfSize << H.bfReserved1 << H.bfReserved2 << H.bfOffBits;
		return Ar;
	}
};
#pragma pack(pop)

// .BMP subheader.
#pragma pack(push,1)
struct FBitmapInfoHeader
{
	DWORD biSize;
	DWORD biWidth;
	DWORD biHeight;
	_WORD biPlanes;
	_WORD biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	DWORD biXPelsPerMeter;
	DWORD biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
	friend FArchive& operator<<( FArchive& Ar, FBitmapInfoHeader& H )
	{
		Ar << H.biSize << H.biWidth << H.biHeight;
		Ar << H.biPlanes << H.biBitCount;
		Ar << H.biCompression << H.biSizeImage;
		Ar << H.biXPelsPerMeter << H.biYPelsPerMeter;
		Ar << H.biClrUsed << H.biClrImportant;
		return Ar;
	}
};
#pragma pack(pop)

/*-----------------------------------------------------------------------------
	UPolysFactory.
-----------------------------------------------------------------------------*/

struct FASEMaterial
{
	FASEMaterial()
	{
		Width = Height = 256;
		UTiling = VTiling = 1;
		Material = NULL;
		bTwoSided = bUnlit = bAlphaTexture = bMasked = bTranslucent = 0;
	}
	FASEMaterial( TCHAR* InName, INT InWidth, INT InHeight, FLOAT InUTiling, FLOAT InVTiling, UMaterialInstance* InMaterial, UBOOL InTwoSided, UBOOL InUnlit, UBOOL InAlphaTexture, UBOOL InMasked, UBOOL InTranslucent )
	{
		appStrcpy( Name, InName );
		Width = InWidth;
		Height = InHeight;
		UTiling = InUTiling;
		VTiling = InVTiling;
		Material = InMaterial;
		bTwoSided = InTwoSided;
		bUnlit = InUnlit;
		bAlphaTexture = InAlphaTexture;
		bMasked = InMasked;
		bTranslucent = InTranslucent;
	}

	TCHAR Name[128];
	INT Width, Height;
	FLOAT UTiling, VTiling;
	UMaterialInstance* Material;
	UBOOL bTwoSided, bUnlit, bAlphaTexture, bMasked, bTranslucent;
};

struct FASEMaterialHeader
{
	FASEMaterialHeader()
	{
	}

	TArray<FASEMaterial> Materials;
};

void UPolysFactory::StaticConstructor()
{
	SupportedClass = UPolys::StaticClass();
	new(Formats)FString(TEXT("t3d;Unreal brush text"));
	bCreateNew = 0;
	bText = 1;

}
UPolysFactory::UPolysFactory()
{
}
UObject* UPolysFactory::FactoryCreateText
(
	ULevel*				InLevel,
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	// Create polys.
	UPolys* Polys = Context ? CastChecked<UPolys>(Context) : new(InParent,Name,Flags)UPolys;

	// Eat up if present.
	GetBEGIN( &Buffer, TEXT("POLYLIST") );

	// Parse all stuff.
	INT First=1, GotBase=0;
	FString StrLine, ExtraLine;
	FPoly Poly;
	while( ParseLine( &Buffer, StrLine ) )
	{
		const TCHAR* Str = *StrLine;
		if( GetEND(&Str,TEXT("POLYLIST")) )
		{
			// End of brush polys.
			break;
		}
		//
		//
		// AutoCad - DXF File
		//
		//
		else if( appStrstr(Str,TEXT("ENTITIES")) && First )
		{
			debugf(NAME_Log,TEXT("Reading Autocad DXF file"));
			INT Started=0, NumPts=0, IsFace=0;
			FVector PointPool[4096];
			FPoly NewPoly; NewPoly.Init();

			while
			(	ParseLine( &Buffer, StrLine, 1 )
			&&	ParseLine( &Buffer, ExtraLine, 1 ) )
			{
				// Handle the line.
				Str = *ExtraLine;
				INT Code = appAtoi(*StrLine);
				if( Code==0 )
				{
					// Finish up current poly.
					if( Started )
					{
						if( NewPoly.NumVertices == 0 )
						{
							// Got a vertex definition.
							NumPts++;
						}
						else if( NewPoly.NumVertices>=3 && NewPoly.NumVertices<FPoly::MAX_VERTICES )
						{
							// Got a poly definition.
							if( IsFace ) NewPoly.Reverse();
							NewPoly.Base = NewPoly.Vertex[0];
							NewPoly.Finalize(NULL,0);
							new(Polys->Element)FPoly( NewPoly );
						}
						else
						{
							// Bad.
							Warn->Logf( TEXT("DXF: Bad vertex count %i"), NewPoly.NumVertices );
						}
						
						// Prepare for next.
						NewPoly.Init();
					}
					Started=0;

					if( ParseCommand(&Str,TEXT("VERTEX")) )
					{
						// Start of new vertex.
						PointPool[NumPts] = FVector(0,0,0);
						Started = 1;
						IsFace  = 0;
					}
					else if( ParseCommand(&Str,TEXT("3DFACE")) )
					{
						// Start of 3d face definition.
						Started = 1;
						IsFace  = 1;
					}
					else if( ParseCommand(&Str,TEXT("SEQEND")) )
					{
						// End of sequence.
						NumPts=0;
					}
					else if( ParseCommand(&Str,TEXT("EOF")) )
					{
						// End of file.
						break;
					}
				}
				else if( Started )
				{
					// Replace commas with periods to handle european dxf's.
					//for( TCHAR* Stupid = appStrchr(*ExtraLine,','); Stupid; Stupid=appStrchr(Stupid,',') )
					//	*Stupid = '.';

					// Handle codes.
					if( Code>=10 && Code<=19 )
					{
						// X coordinate.
						if( IsFace && Code-10==NewPoly.NumVertices )
						{
							NewPoly.Vertex[NewPoly.NumVertices++] = FVector(0,0,0);
						}
						NewPoly.Vertex[Code-10].X = PointPool[NumPts].X = appAtof(*ExtraLine);
					}
					else if( Code>=20 && Code<=29 )
					{
						// Y coordinate.
						NewPoly.Vertex[Code-20].Y = PointPool[NumPts].Y = appAtof(*ExtraLine);
					}
					else if( Code>=30 && Code<=39 )
					{
						// Z coordinate.
						NewPoly.Vertex[Code-30].Z = PointPool[NumPts].Z = appAtof(*ExtraLine);
					}
					else if( Code>=71 && Code<=79 && (Code-71)==NewPoly.NumVertices )
					{
						INT iPoint = Abs(appAtoi(*ExtraLine));
						if( iPoint>0 && iPoint<=NumPts )
							NewPoly.Vertex[NewPoly.NumVertices++] = PointPool[iPoint-1];
						else debugf( NAME_Warning, TEXT("DXF: Invalid point index %i/%i"), iPoint, NumPts );
					}
				}
			}
		}
		//
		//
		// 3D Studio MAX - ASC File
		//
		//
		else if( appStrstr(Str,TEXT("Tri-mesh,")) && First )
		{
			debugf( NAME_Log, TEXT("Reading 3D Studio ASC file") );
			FVector PointPool[4096];

			AscReloop:
			int NumVerts = 0, TempNumPolys=0, TempVerts=0;
			while( ParseLine( &Buffer, StrLine ) )
			{
				Str = *StrLine;

				FString VertText = FString::Printf( TEXT("Vertex %i:"), NumVerts );
				FString FaceText = FString::Printf( TEXT("Face %i:"), TempNumPolys );
				if( appStrstr(Str,*VertText) )
				{
					PointPool[NumVerts].X = appAtof(appStrstr(Str,TEXT("X:"))+2);
					PointPool[NumVerts].Y = appAtof(appStrstr(Str,TEXT("Y:"))+2);
					PointPool[NumVerts].Z = appAtof(appStrstr(Str,TEXT("Z:"))+2);
					NumVerts++;
					TempVerts++;
				}
				else if( appStrstr(Str,*FaceText) )
				{
					Poly.Init();
					Poly.NumVertices=3;
					Poly.Vertex[0] = PointPool[appAtoi(appStrstr(Str,TEXT("A:"))+2)];
					Poly.Vertex[1] = PointPool[appAtoi(appStrstr(Str,TEXT("B:"))+2)];
					Poly.Vertex[2] = PointPool[appAtoi(appStrstr(Str,TEXT("C:"))+2)];
					Poly.Base = Poly.Vertex[0];
					Poly.Finalize(NULL,0);
					new(Polys->Element)FPoly(Poly);
					TempNumPolys++;
				}
				else if( appStrstr(Str,TEXT("Tri-mesh,")) )
					goto AscReloop;
			}
			debugf( NAME_Log, TEXT("Imported %i vertices, %i faces"), TempVerts, Polys->Element.Num() );
		}
		//
		//
		// 3D Studio MAX - ASE File
		//
		//
		else if( appStrstr(Str,TEXT("*3DSMAX_ASCIIEXPORT")) && First )
		{
			debugf( NAME_Log, TEXT("Reading 3D Studio ASE file") );

			TArray<FVector> Vertex;						// 1 FVector per entry
			TArray<INT> FaceIdx;						// 3 INT's for vertex indices per entry
			TArray<INT> FaceMaterialsIdx;				// 1 INT for material ID per face
			TArray<FVector> TexCoord;					// 1 FVector per entry
			TArray<INT> FaceTexCoordIdx;				// 3 INT's per entry
			TArray<FASEMaterialHeader> ASEMaterialHeaders;	// 1 per material (multiple sub-materials inside each one)
			TArray<DWORD>	SmoothingGroups;			// 1 DWORD per face.
			
			INT NumVertex = 0, NumFaces = 0, NumTVertex = 0, NumTFaces = 0, ASEMaterialRef = -1;

			UBOOL IgnoreMcdGeometry = 0;

			enum {
				GROUP_NONE			= 0,
				GROUP_MATERIAL		= 1,
				GROUP_GEOM			= 2,
			} Group;

			enum {
				SECTION_NONE		= 0,
				SECTION_MATERIAL	= 1,
				SECTION_MAP_DIFFUSE	= 2,
				SECTION_VERTS		= 3,
				SECTION_FACES		= 4,
				SECTION_TVERTS		= 5,
				SECTION_TFACES		= 6,
			} Section;

			Group = GROUP_NONE;
			Section = SECTION_NONE;
			while( ParseLine( &Buffer, StrLine ) )
			{
				Str = *StrLine;

				if( Group == GROUP_NONE )
				{
					if( StrLine.InStr(TEXT("*MATERIAL_LIST")) != -1 )
						Group = GROUP_MATERIAL;
					else if( StrLine.InStr(TEXT("*GEOMOBJECT")) != -1 )
						Group = GROUP_GEOM;
				}
				else if ( Group == GROUP_MATERIAL )
				{
					static FLOAT UTiling = 1, VTiling = 1;
					static UMaterialInstance* Material = NULL;
					static INT Height = 256, Width = 256;
					static UBOOL bTwoSided = 0, bUnlit = 0, bAlphaTexture = 0, bMasked = 0, bTranslucent = 0;

					// Determine the section and/or extract individual values
					if( StrLine == TEXT("}") )
						Group = GROUP_NONE;
					else if( StrLine.InStr(TEXT("*MATERIAL ")) != -1 )
						Section = SECTION_MATERIAL;
					else if( StrLine.InStr(TEXT("*MATERIAL_WIRE")) != -1 )
					{
						if( StrLine.InStr(TEXT("*MATERIAL_WIRESIZE")) == -1 )
							bTranslucent = 1;
					}
					else if( StrLine.InStr(TEXT("*MATERIAL_TWOSIDED")) != -1 )
					{
						bTwoSided = 1;
					}
					else if( StrLine.InStr(TEXT("*MATERIAL_SELFILLUM")) != -1 )
					{
						INT Pos = StrLine.InStr( TEXT("*") );
						FString NewStr = StrLine.Right( StrLine.Len() - Pos );
						FLOAT temp;
						appSSCANF( *NewStr, TEXT("*MATERIAL_SELFILLUM %f"), &temp );
						if( temp == 100.f || temp == 1.f )
							bUnlit = 1;
					}
					else if( StrLine.InStr(TEXT("*MATERIAL_TRANSPARENCY")) != -1 )
					{
						INT Pos = StrLine.InStr( TEXT("*") );
						FString NewStr = StrLine.Right( StrLine.Len() - Pos );
						FLOAT temp;
						appSSCANF( *NewStr, TEXT("*MATERIAL_TRANSPARENCY %f"), &temp );
						if( temp > 0.f )
							bAlphaTexture = 1;
					}
					else if( StrLine.InStr(TEXT("*MATERIAL_SHADING")) != -1 )
					{
						INT Pos = StrLine.InStr( TEXT("*") );
						FString NewStr = StrLine.Right( StrLine.Len() - Pos );
						TCHAR Buffer[20];
						appSSCANF( *NewStr, TEXT("*MATERIAL_SHADING %s"), Buffer );
						if( !appStrcmp( Buffer, TEXT("Constant") ) )
							bMasked = 1;
					}
					else if( StrLine.InStr(TEXT("*MAP_DIFFUSE")) != -1 )
					{
						Section = SECTION_MAP_DIFFUSE;
						Material = NULL;
						UTiling = VTiling = 1;
						Width = Height = 256;
					}
					else
					{
						if ( Section == SECTION_MATERIAL )
						{
							// We are entering a new material definition.  Allocate a new material header.
							new( ASEMaterialHeaders )FASEMaterialHeader();
							Section = SECTION_NONE;
						}
						else if ( Section == SECTION_MAP_DIFFUSE )
						{
							if( StrLine.InStr(TEXT("*BITMAP")) != -1 )
							{
								// Remove tabs from the front of this string.  The number of tabs differs
								// depending on how many materials are in the file.
								INT Pos = StrLine.InStr( TEXT("*") );
								FString NewStr = StrLine.Right( StrLine.Len() - Pos );

								NewStr = NewStr.Right( NewStr.Len() - NewStr.InStr(TEXT("\\"), -1 ) - 1 );	// Strip off path info
								NewStr = NewStr.Left( NewStr.Len() - 5 );									// Strip off '.bmp"' at the end

								// Find the texture
								Material = NULL;
							}
							else if( StrLine.InStr(TEXT("*UVW_U_TILING")) != -1 )
							{
								INT Pos = StrLine.InStr( TEXT("*") );
								FString NewStr = StrLine.Right( StrLine.Len() - Pos );
								appSSCANF( *NewStr, TEXT("*UVW_U_TILING %f"), &UTiling );
							}
							else if( StrLine.InStr(TEXT("*UVW_V_TILING")) != -1 )
							{
								INT Pos = StrLine.InStr( TEXT("*") );
								FString NewStr = StrLine.Right( StrLine.Len() - Pos );
								appSSCANF( *NewStr, TEXT("*UVW_V_TILING %f"), &VTiling );

								check(ASEMaterialHeaders.Num());
								new( ASEMaterialHeaders(ASEMaterialHeaders.Num()-1).Materials )FASEMaterial( (TCHAR*)*Name, Width, Height, UTiling, VTiling, Material, bTwoSided, bUnlit, bAlphaTexture, bMasked, bTranslucent );

								Section = SECTION_NONE;
								bTwoSided = bUnlit = bAlphaTexture = bMasked = bTranslucent = 0;
							}
						}
					}
				}
				else if ( Group == GROUP_GEOM )
				{
					// Determine the section and/or extract individual values
					if( StrLine == TEXT("}") )
					{
						IgnoreMcdGeometry = 0;
						Group = GROUP_NONE;
					}
					// See if this is an MCD thing
					else if( StrLine.InStr(TEXT("*NODE_NAME")) != -1 )
					{
						TCHAR NodeName[512];                     
						appSSCANF( Str, TEXT("\t*NODE_NAME \"%s\""), &NodeName );
						if( appStrstr(NodeName, TEXT("MCD")) == NodeName )
							IgnoreMcdGeometry = 1;
						else 
							IgnoreMcdGeometry = 0;
					}

					// Now do nothing if it's an MCD Geom
					if( !IgnoreMcdGeometry )
					{              
						if( StrLine.InStr(TEXT("*MESH_NUMVERTEX")) != -1 )
							appSSCANF( Str, TEXT("\t\t*MESH_NUMVERTEX %d"), &NumVertex );
						else if( StrLine.InStr(TEXT("*MESH_NUMFACES")) != -1 )
							appSSCANF( Str, TEXT("\t\t*MESH_NUMFACES %d"), &NumFaces );
						else if( StrLine.InStr(TEXT("*MESH_VERTEX_LIST")) != -1 )
							Section = SECTION_VERTS;
						else if( StrLine.InStr(TEXT("*MESH_FACE_LIST")) != -1 )
							Section = SECTION_FACES;
						else if( StrLine.InStr(TEXT("*MESH_NUMTVERTEX")) != -1 )
							appSSCANF( Str, TEXT("\t\t*MESH_NUMTVERTEX %d"), &NumTVertex );
						else if( StrLine == TEXT("\t\t*MESH_TVERTLIST {") )
							Section = SECTION_TVERTS;
						else if( StrLine.InStr(TEXT("*MESH_NUMTVFACES")) != -1 )
							appSSCANF( Str, TEXT("\t\t*MESH_NUMTVFACES %d"), &NumTFaces );
						else if( StrLine.InStr(TEXT("*MATERIAL_REF")) != -1 )
							appSSCANF( Str, TEXT("\t*MATERIAL_REF %d"), &ASEMaterialRef );
						else if( StrLine == TEXT("\t\t*MESH_TFACELIST {") )
							Section = SECTION_TFACES;
						else
						{
							// Extract data specific to sections
							if( Section == SECTION_VERTS )
							{
								if( StrLine.InStr(TEXT("\t\t}")) != -1 )
									Section = SECTION_NONE;
								else
								{
									int temp;
									FVector vtx;
									appSSCANF( Str, TEXT("\t\t\t*MESH_VERTEX    %d\t%f\t%f\t%f"),
										&temp, &vtx.X, &vtx.Y, &vtx.Z );
									new(Vertex)FVector(vtx);
								}
							}
							else if( Section == SECTION_FACES )
							{
								if( StrLine.InStr(TEXT("\t\t}")) != -1 )
									Section = SECTION_NONE;
								else
								{
									INT temp, idx1, idx2, idx3;
									appSSCANF( Str, TEXT("\t\t\t*MESH_FACE %d:    A: %d B: %d C: %d"),
										&temp, &idx1, &idx2, &idx3 );
									new(FaceIdx)INT(idx1);
									new(FaceIdx)INT(idx2);
									new(FaceIdx)INT(idx3);

									// Determine the right  part of StrLine which contains the smoothing group(s).
									FString SmoothTag(TEXT("*MESH_SMOOTHING"));
									INT SmGroupsLocation = StrLine.InStr( SmoothTag );
									if( SmGroupsLocation != -1 )
									{
										FString	SmoothingString;
										DWORD	SmoothingMask = 0;

										SmoothingString = StrLine.Right( StrLine.Len() - SmGroupsLocation - SmoothTag.Len() );

										while(SmoothingString.Len())
										{
											INT	Length = SmoothingString.InStr(TEXT(",")),
												SmoothingGroup = (Length != -1) ? appAtoi(*SmoothingString.Left(Length)) : appAtoi(*SmoothingString);

											if(SmoothingGroup <= 32)
												SmoothingMask |= (1 << (SmoothingGroup - 1));

											SmoothingString = (Length != -1) ? SmoothingString.Right(SmoothingString.Len() - Length - 1) : TEXT("");
										};

										SmoothingGroups.AddItem(SmoothingMask);
									}
									else
										SmoothingGroups.AddItem(0);

									// Sometimes "MESH_SMOOTHING" is a blank instead of a number, so we just grab the 
									// part of the string we need and parse out the material id.
									INT MaterialID;
									StrLine = StrLine.Right( StrLine.Len() - StrLine.InStr( TEXT("*MESH_MTLID"), -1 ) - 1 );
									appSSCANF( *StrLine , TEXT("MESH_MTLID %d"), &MaterialID );
									new(FaceMaterialsIdx)INT(MaterialID);
								}
							}
							else if( Section == SECTION_TVERTS )
							{
								if( StrLine.InStr(TEXT("\t\t}")) != -1 )
									Section = SECTION_NONE;
								else
								{
									int temp;
									FVector vtx;
									appSSCANF( Str, TEXT("\t\t\t*MESH_TVERT %d\t%f\t%f"),
										&temp, &vtx.X, &vtx.Y );
									vtx.Z = 0;
									new(TexCoord)FVector(vtx);
								}
							}
							else if( Section == SECTION_TFACES )
							{
								if( StrLine == TEXT("\t\t}") )
									Section = SECTION_NONE;
								else
								{
									int temp, idx1, idx2, idx3;
									appSSCANF( Str, TEXT("\t\t\t*MESH_TFACE %d\t%d\t%d\t%d"),
										&temp, &idx1, &idx2, &idx3 );
									new(FaceTexCoordIdx)INT(idx1);
									new(FaceTexCoordIdx)INT(idx2);
									new(FaceTexCoordIdx)INT(idx3);
								}
							}
						}
					}
				}
			}

			// Create the polys from the gathered info.
			if( FaceIdx.Num() != FaceTexCoordIdx.Num() )
			{
				appMsgf( 0, TEXT("ASE Importer Error : Did you forget to include geometry AND materials in the export?\n\nFaces : %d, Texture Coordinates : %d"),
					FaceIdx.Num(), FaceTexCoordIdx.Num());
				continue;
			}
			if( ASEMaterialRef == -1 )
			{
				appMsgf( 0, TEXT("ASE Importer Warning : This object has no material reference (current texture will be applied to object).") );
			}
			for( int x = 0 ; x < FaceIdx.Num() ; x += 3 )
			{
				Poly.Init();
				Poly.NumVertices = 3;
				Poly.Vertex[0] = Vertex( FaceIdx(x) );
				Poly.Vertex[1] = Vertex( FaceIdx(x+1) );
				Poly.Vertex[2] = Vertex( FaceIdx(x+2) );

				FASEMaterial ASEMaterial;
				if( ASEMaterialRef != -1 )
					if( ASEMaterialHeaders(ASEMaterialRef).Materials.Num() )
						if( ASEMaterialHeaders(ASEMaterialRef).Materials.Num() == 1 )
							ASEMaterial = ASEMaterialHeaders(ASEMaterialRef).Materials(0);
						else
						{
							// Sometimes invalid material references appear in the ASE file.  We can't do anything about
							// it, so when that happens just use the first material.
							if( FaceMaterialsIdx(x/3) >= ASEMaterialHeaders(ASEMaterialRef).Materials.Num() )
								ASEMaterial = ASEMaterialHeaders(ASEMaterialRef).Materials(0);
							else
								ASEMaterial = ASEMaterialHeaders(ASEMaterialRef).Materials( FaceMaterialsIdx(x/3) );
						}

				if( ASEMaterial.Material )
					Poly.Material = ASEMaterial.Material;

				Poly.SmoothingMask = SmoothingGroups(x / 3);

				Poly.Finalize(NULL,1);

				// The brushes come in flipped across the X axis, so adjust for that.
				FVector Flip(1,-1,1);
				Poly.Vertex[0] *= Flip;
				Poly.Vertex[1] *= Flip;
				Poly.Vertex[2] *= Flip;

				FVector	ST1 = TexCoord(FaceTexCoordIdx(x + 0)),
						ST2 = TexCoord(FaceTexCoordIdx(x + 1)),
						ST3 = TexCoord(FaceTexCoordIdx(x + 2));

				FTexCoordsToVectors(
					Poly.Vertex[0],
					FVector(ST1.X * ASEMaterial.Width * ASEMaterial.UTiling,(1.0f - ST1.Y) * ASEMaterial.Height * ASEMaterial.VTiling,ST1.Z),
					Poly.Vertex[1],
					FVector(ST2.X * ASEMaterial.Width * ASEMaterial.UTiling,(1.0f - ST2.Y) * ASEMaterial.Height * ASEMaterial.VTiling,ST2.Z),
					Poly.Vertex[2],
					FVector(ST3.X * ASEMaterial.Width * ASEMaterial.UTiling,(1.0f - ST3.Y) * ASEMaterial.Height * ASEMaterial.VTiling,ST3.Z),
					&Poly.Base,
					&Poly.TextureU,
					&Poly.TextureV
					);

				Poly.Reverse();
				Poly.CalcNormal();

				new(Polys->Element)FPoly(Poly);
			}

			debugf( NAME_Log, TEXT("Imported %i vertices, %i faces"), NumVertex, NumFaces );
		}
		//
		//
		// T3D FORMAT
		//
		//
		else if( GetBEGIN(&Str,TEXT("POLYGON")) )
		{
			// Init to defaults and get group/item and texture.
			Poly.Init();
			Parse( Str, TEXT("LINK="), Poly.iLink );
			Parse( Str, TEXT("ITEM="), Poly.ItemName );
			Parse( Str, TEXT("FLAGS="), Poly.PolyFlags );
			Poly.PolyFlags &= ~PF_NoImport;

			FString TextureName;
			Parse( Str, TEXT("TEXTURE="), TextureName );

			Poly.Material = Cast<UMaterialInstance>(StaticLoadObject( UMaterialInstance::StaticClass(), NULL, *TextureName, NULL, LOAD_NoWarn, NULL ));

			// Fix to make it work with import of T3D textured brushes  - Erik
			if( ! Poly.Material)
				Poly.Material = Cast<UMaterialInstance> ( StaticFindObject( UMaterialInstance::StaticClass(), ANY_PACKAGE, *TextureName));		
		}
		else if( ParseCommand(&Str,TEXT("PAN")) )
		{
			INT	PanU = 0,
				PanV = 0;

			Parse( Str, TEXT("U="), PanU );
			Parse( Str, TEXT("V="), PanV );

			Poly.Base += Poly.TextureU * PanU;
			Poly.Base += Poly.TextureV * PanV;
		}
		else if( ParseCommand(&Str,TEXT("ORIGIN")) )
		{
			GotBase=1;
			GetFVECTOR( Str, Poly.Base );
		}
		else if( ParseCommand(&Str,TEXT("VERTEX")) )
		{
			if( Poly.NumVertices < FPoly::MAX_VERTICES )
			{
				GetFVECTOR( Str, Poly.Vertex[Poly.NumVertices] );
				Poly.NumVertices++;
			}
		}
		else if( ParseCommand(&Str,TEXT("TEXTUREU")) )
		{
			GetFVECTOR( Str, Poly.TextureU );
		}
		else if( ParseCommand(&Str,TEXT("TEXTUREV")) )
		{
			GetFVECTOR( Str, Poly.TextureV );
		}
		else if( GetEND(&Str,TEXT("POLYGON")) )
		{
			if( !GotBase )
				Poly.Base = Poly.Vertex[0];
			if( Poly.Finalize(NULL,1)==0 )
				new(Polys->Element)FPoly(Poly);
			GotBase=0;
		}
	}

	// Success.
	return Polys;
}
IMPLEMENT_CLASS(UPolysFactory);

/*-----------------------------------------------------------------------------
	UModelFactory.
-----------------------------------------------------------------------------*/

void UModelFactory::StaticConstructor()
{
	SupportedClass = UModel::StaticClass();
	new(Formats)FString(TEXT("t3d;Unreal model text"));
	bCreateNew = 0;
	bText = 1;

}
UModelFactory::UModelFactory()
{
}
UObject* UModelFactory::FactoryCreateText
(
	ULevel*				InLevel,
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	ABrush* TempOwner = (ABrush*)Context;
	UModel* Model = new( InParent, Name, Flags )UModel( TempOwner, 1 );

	const TCHAR* StrPtr;
	FString StrLine;
	if( TempOwner )
	{
		TempOwner->InitPosRotScale();
		GSelectionTools.Select( TempOwner, 0 );
		TempOwner->bTempEditor = 0;
	}
	while( ParseLine( &Buffer, StrLine ) )
	{
		StrPtr = *StrLine;
		if( GetEND(&StrPtr,TEXT("BRUSH")) )
		{
			break;
		}
		else if( GetBEGIN (&StrPtr,TEXT("POLYLIST")) )
		{
			UPolysFactory* PolysFactory = new UPolysFactory;
			Model->Polys = (UPolys*)PolysFactory->FactoryCreateText(InLevel,UPolys::StaticClass(),InParent,NAME_None,0,NULL,Type,Buffer,BufferEnd,Warn);
			check(Model->Polys);
		}
		if( TempOwner )
		{
			if      (ParseCommand(&StrPtr,TEXT("PREPIVOT"	))) GetFVECTOR 	(StrPtr,TempOwner->PrePivot);
			else if (ParseCommand(&StrPtr,TEXT("LOCATION"	))) GetFVECTOR	(StrPtr,TempOwner->Location);
			else if (ParseCommand(&StrPtr,TEXT("ROTATION"	))) GetFROTATOR  (StrPtr,TempOwner->Rotation,1);
			if( ParseCommand(&StrPtr,TEXT("SETTINGS")) )
			{
				Parse( StrPtr, TEXT("CSG="), TempOwner->CsgOper );
				Parse( StrPtr, TEXT("POLYFLAGS="), TempOwner->PolyFlags );
			}
		}
	}
	if( GEditor )
		GEditor->bspValidateBrush( Model, 1, 0 );

	return Model;
}
IMPLEMENT_CLASS(UModelFactory);

/*-----------------------------------------------------------------------------
	USoundFactory.
-----------------------------------------------------------------------------*/

void USoundFactory::StaticConstructor()
{
	SupportedClass = USoundNodeWave::StaticClass();
	new(Formats)FString(TEXT("wav;Sound"));
	bCreateNew = 0;

}
USoundFactory::USoundFactory()
{
	bEditorImport = 1;
}
UObject* USoundFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		FileType,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	if
	(	appStricmp(FileType, TEXT("WAV"))==0
	||	appStricmp(FileType, TEXT("MP3"))==0
	||	appStricmp(FileType, TEXT("OGG"))==0
	)
	{
		// Create new sound and import raw data.
		USoundNodeWave* Sound = new(InParent,Name,Flags) USoundNodeWave;	
		Sound->FileType = FName(FileType);
		
		Sound->RawData.Empty( BufferEnd-Buffer );
		Sound->RawData.Add( BufferEnd-Buffer );
		appMemcpy( &Sound->RawData(0), Buffer, Sound->RawData.Num() );
		Sound->RawData.Detach();
		
		// Calculate duration.
		if( appStricmp(FileType, TEXT("WAV"))==0 )
		{
			FWaveModInfo WaveInfo;
			if( WaveInfo.ReadWaveInfo(Sound->RawData) )
			{
				INT DurationDiv = *WaveInfo.pChannels * *WaveInfo.pBitsPerSample * *WaveInfo.pSamplesPerSec;  
				if( DurationDiv ) 
					Sound->Duration = *WaveInfo.pWaveDataSize * 8.f / DurationDiv;
				else
					Sound->Duration = 0.f;
			}
		}
		else
			Sound->Duration = 0.f;		
		
		return Sound;
	}
	else
	{
		// Unrecognized.
		Warn->Logf( TEXT("Unrecognized sound format '%s' in %s"), FileType, *Name );
		return NULL;
	}
}
IMPLEMENT_CLASS(USoundFactory);

/*------------------------------------------------------------------------------
	USoundCueFactoryNew.
------------------------------------------------------------------------------*/

void USoundCueFactoryNew::StaticConstructor()
{
	SupportedClass		= USoundCue::StaticClass();
	bCreateNew			= 1;
	Description			= TEXT("SoundCue");
	new(GetClass()->HideCategories) FName(TEXT("Object"));
}

UObject* USoundCueFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

IMPLEMENT_CLASS(USoundCueFactoryNew);

/*------------------------------------------------------------------------------
	UParticleSystemFactoryNew.
------------------------------------------------------------------------------*/

void UParticleSystemFactoryNew::StaticConstructor()
{
	SupportedClass		= UParticleSystem::StaticClass();
	bCreateNew			= 1;
	Description			= TEXT("ParticleSystem");
	new(GetClass()->HideCategories) FName(TEXT("Object"));
}

UObject* UParticleSystemFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

IMPLEMENT_CLASS(UParticleSystemFactoryNew);

/*------------------------------------------------------------------------------
	UAnimSetFactoryNew.
------------------------------------------------------------------------------*/

void UAnimSetFactoryNew::StaticConstructor()
{
	SupportedClass		= UAnimSet::StaticClass();
	bCreateNew			= 1;
	Description			= TEXT("AnimSet");
	new(GetClass()->HideCategories) FName(TEXT("Object"));
}

UObject* UAnimSetFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

IMPLEMENT_CLASS(UAnimSetFactoryNew);

/*------------------------------------------------------------------------------
	UAnimTreeFactoryNew.
------------------------------------------------------------------------------*/

void UAnimTreeFactoryNew::StaticConstructor()
{
	SupportedClass		= UAnimTree::StaticClass();
	bCreateNew			= 1;
	Description			= TEXT("AnimTree");
	new(GetClass()->HideCategories) FName(TEXT("Object"));
}

UObject* UAnimTreeFactoryNew::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,DWORD Flags,UObject* Context,FFeedbackContext* Warn)
{
	return StaticConstructObject(Class,InParent,Name,Flags);
}

IMPLEMENT_CLASS(UAnimTreeFactoryNew);

/*-----------------------------------------------------------------------------
	UPrefabFactory.
-----------------------------------------------------------------------------*/

void UPrefabFactory::StaticConstructor()
{
	SupportedClass = UPrefab::StaticClass();
	new(Formats)FString(TEXT("t3d;Prefab"));
	bCreateNew = 0;

}
UPrefabFactory::UPrefabFactory()
{
	bEditorImport = 1;
}
UObject* UPrefabFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		FileType,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	if
	(	appStricmp(FileType, TEXT("T3D"))==0 )
	{
		// Prefab file.
		UPrefab* Prefab = new(InParent,Name,Flags)UPrefab;
		Prefab->T3DText = winAnsiToTCHAR( (ANSICHAR*)Buffer );
		return Prefab;
	}
	else
	{
		// Unrecognized.
		Warn->Logf( TEXT("Unrecognized prefab format '%s' in %s"), FileType, *Name );
		return NULL;
	}
}
IMPLEMENT_CLASS(UPrefabFactory);

/*-----------------------------------------------------------------------------
	UTextureMovieFactory.
-----------------------------------------------------------------------------*/

void UTextureMovieFactory::StaticConstructor()
{
	SupportedClass = UTextureMovie::StaticClass();
	new(Formats)FString(TEXT("ogg;Movie"));
	bCreateNew = 0;
}
UTextureMovieFactory::UTextureMovieFactory()
{
	bEditorImport = 1;
}
UObject* UTextureMovieFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	INT Length = BufferEnd - Buffer;

	UTextureMovie* Movie = NULL;

	//
	// OGG (Theora)
	//
	if( appStricmp( Type, TEXT("OGG") ) == 0 )
	{
		Movie = CastChecked<UTextureMovie>(StaticConstructObject(Class,InParent,Name,Flags));		
	
		Movie->RawData.Empty( Length );
		Movie->RawData.Add( Length );
		Movie->RawSize = Length;
		appMemcpy( Movie->RawData.GetData(), Buffer, Length );
		Movie->RawData.Detach();

		Movie->DecoderClass = UObject::StaticLoadClass( UCodecMovie::StaticClass(), NULL, TEXT("Engine.CodecMovieTheora"), NULL, LOAD_NoFail, NULL );

		Movie->PostLoad();
	}
	else
	{
		// Unknown format.
		Warn->Logf( TEXT("Bad movie format for movie import") );
		return NULL;
 	}

	// Invalidate any materials using the newly imported movie texture. (occurs if you import over an existing movie)
	Movie->PostEditChange(NULL);

	return Movie;
}
IMPLEMENT_CLASS(UTextureMovieFactory);

/*-----------------------------------------------------------------------------
	UTextureFactory.
-----------------------------------------------------------------------------*/

// .PCX file header.
#pragma pack(push,1)
class FPCXFileHeader
{
public:
	BYTE	Manufacturer;		// Always 10.
	BYTE	Version;			// PCX file version.
	BYTE	Encoding;			// 1=run-length, 0=none.
	BYTE	BitsPerPixel;		// 1,2,4, or 8.
	_WORD	XMin;				// Dimensions of the image.
	_WORD	YMin;				// Dimensions of the image.
	_WORD	XMax;				// Dimensions of the image.
	_WORD	YMax;				// Dimensions of the image.
	_WORD	XDotsPerInch;		// Horizontal printer resolution.
	_WORD	YDotsPerInch;		// Vertical printer resolution.
	BYTE	OldColorMap[48];	// Old colormap info data.
	BYTE	Reserved1;			// Must be 0.
	BYTE	NumPlanes;			// Number of color planes (1, 3, 4, etc).
	_WORD	BytesPerLine;		// Number of bytes per scanline.
	_WORD	PaletteType;		// How to interpret palette: 1=color, 2=gray.
	_WORD	HScreenSize;		// Horizontal monitor size.
	_WORD	VScreenSize;		// Vertical monitor size.
	BYTE	Reserved2[54];		// Must be 0.
	friend FArchive& operator<<( FArchive& Ar, FPCXFileHeader& H )
	{
		Ar << H.Manufacturer << H.Version << H.Encoding << H.BitsPerPixel;
		Ar << H.XMin << H.YMin << H.XMax << H.YMax << H.XDotsPerInch << H.YDotsPerInch;
		for( INT i=0; i<ARRAY_COUNT(H.OldColorMap); i++ )
			Ar << H.OldColorMap[i];
		Ar << H.Reserved1 << H.NumPlanes;
		Ar << H.BytesPerLine << H.PaletteType << H.HScreenSize << H.VScreenSize;
		for( INT i=0; i<ARRAY_COUNT(H.Reserved2); i++ )
			Ar << H.Reserved2[i];
		return Ar;
	}
};

struct FTGAFileHeader
{
	BYTE IdFieldLength;
	BYTE ColorMapType;
	BYTE ImageTypeCode;		// 2 for uncompressed RGB format
	_WORD ColorMapOrigin;
	_WORD ColorMapLength;
	BYTE ColorMapEntrySize;
	_WORD XOrigin;
	_WORD YOrigin;
	_WORD Width;
	_WORD Height;
	BYTE BitsPerPixel;
	BYTE ImageDescriptor;
	friend FArchive& operator<<( FArchive& Ar, FTGAFileHeader& H )
	{
		Ar << H.IdFieldLength << H.ColorMapType << H.ImageTypeCode;
		Ar << H.ColorMapOrigin << H.ColorMapLength << H.ColorMapEntrySize;
		Ar << H.XOrigin << H.YOrigin << H.Width << H.Height << H.BitsPerPixel;
		Ar << H.ImageDescriptor;
		return Ar;
	}
};

struct FTGAFileFooter
{
	DWORD ExtensionAreaOffset;
	DWORD DeveloperDirectoryOffset;
	BYTE Signature[16];
	BYTE TrailingPeriod;
	BYTE NullTerminator;
};

struct FDDSFileHeader
{
	DWORD				Magic;
	DDSURFACEDESC2		desc;
};
#pragma pack(pop)

void UTextureFactory::StaticConstructor()
{
	SupportedClass = UTexture2D::StaticClass();
	new(Formats)FString(TEXT("bmp;Texture"));
	new(Formats)FString(TEXT("pcx;Texture"));
	new(Formats)FString(TEXT("tga;Texture"));
	new(Formats)FString(TEXT("float;Texture"));
	bCreateNew = 0;

	// This needs to be mirrored in UnTex.h, Texture.uc and UnEdFact.cpp.
	UEnum* Enum = new(GetClass(),TEXT("TextureCompressionSettings"),RF_Public) UEnum(NULL);
	new(Enum->Names) FName(TEXT("TC_Default"));
	new(Enum->Names) FName(TEXT("TC_Normalmap"));
	new(Enum->Names) FName(TEXT("TC_Displacementmap"));
	new(Enum->Names) FName(TEXT("TC_NormalmapAlpha"));
	new(Enum->Names) FName(TEXT("TC_Grayscale"));
	new(Enum->Names) FName(TEXT("TC_HighDynamicRange"));

	new(GetClass()->HideCategories) FName(TEXT("Object"));
	new(GetClass(),TEXT("NoCompression")		,RF_Public) UBoolProperty(CPP_PROPERTY(NoCompression		),TEXT(""),0				);
	new(GetClass(),TEXT("CompressionNoAlpha")	,RF_Public)	UBoolProperty(CPP_PROPERTY(NoAlpha				),TEXT(""),CPF_Edit			);
	new(GetClass(),TEXT("Create Material?")		,RF_Public) UBoolProperty(CPP_PROPERTY(bCreateMaterial		),TEXT(""),CPF_Edit			);
	new(GetClass(),TEXT("CompressionSettings")	,RF_Public) UByteProperty(CPP_PROPERTY(CompressionSettings	),TEXT(""),CPF_Edit, Enum 	);	
}
UTextureFactory::UTextureFactory()
{
	bEditorImport = 1;
}
UObject* UTextureFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	UTexture2D* Texture = NULL;

	const FTGAFileHeader*    TGA   = (FTGAFileHeader *)Buffer;
	const FPCXFileHeader*    PCX   = (FPCXFileHeader *)Buffer;
	const FBitmapFileHeader* bmf   = (FBitmapFileHeader *)(Buffer + 0);
	const FBitmapInfoHeader* bmhdr = (FBitmapInfoHeader *)(Buffer + sizeof(FBitmapFileHeader));

	// Validate it.
	INT Length = BufferEnd - Buffer;
	
	//
	// FLOAT (raw)
	//
	if( appStricmp( Type, TEXT("FLOAT") ) == 0 )
	{
		const INT	SrcComponents	= 3;
		INT			Dimension		= appCeil( appSqrt( Length / sizeof(FLOAT) / SrcComponents ) );

		if( Dimension * Dimension * SrcComponents * sizeof(FLOAT) != Length )
		{
			Warn->Logf( TEXT("Couldn't figure out texture dimensions") );
			return NULL;
		}

		Texture = CastChecked<UTexture2D>(StaticConstructObject(Class,InParent,Name,Flags));
		Texture->Init(Dimension,Dimension,PF_A8R8G8B8);
		Texture->PostLoad();
		Texture->RGBE					= 1;
		Texture->SRGB					= 0;
		Texture->CompressionNoMipmaps	= 1;
		CompressionSettings				= TC_HighDynamicRange;

		const INT	NumTexels			= Dimension * Dimension;
		FColor*		Dst					= (FColor*) &Texture->Mips(0).Data(0);
		FVector*	Src					= (FVector*) Buffer;
	 	
		for( INT y=0; y<Dimension; y++ )
			for( INT x=0; x<Dimension; x++ )
			{
				FLinearColor SrcColor = FLinearColor( Src[(Dimension-y)*Dimension+x].X, Src[(Dimension-y)*Dimension+x].Y, Src[(Dimension-y)*Dimension+x].Z );
				Dst[y*Dimension+x] = SrcColor.ToRGBE();
			}
	}
	//
	// BMP
	//
	else if( (Length>=sizeof(FBitmapFileHeader)+sizeof(FBitmapInfoHeader)) && Buffer[0]=='B' && Buffer[1]=='M' )
	{
		if( (bmhdr->biWidth&(bmhdr->biWidth-1)) || (bmhdr->biHeight&(bmhdr->biHeight-1)) )
		{
			Warn->Logf( TEXT("Texture dimensions are not powers of two") );
			return NULL;
		}
		if( bmhdr->biCompression != BCBI_RGB )
		{
			Warn->Logf( TEXT("RLE compression of BMP images not supported") );
			return NULL;
		}
		if( bmhdr->biPlanes==1 && bmhdr->biBitCount==8 )
		{
			// Do palette.
			const BYTE* bmpal = (BYTE*)Buffer + sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader);

			// Set texture properties.			
			Texture = CastChecked<UTexture2D>(StaticConstructObject( Class, InParent, Name, Flags ) );
			Texture->Init( bmhdr->biWidth, bmhdr->biHeight, PF_A8R8G8B8 );
			Texture->PostLoad();

			TArray<FColor>	Palette;
			for( INT i=0; i<Min<INT>(256,bmhdr->biClrUsed?bmhdr->biClrUsed:126); i++ )
				Palette.AddItem(FColor( bmpal[i*4+2], bmpal[i*4+1], bmpal[i*4+0], bmpal[i*4+3] ));
			while( Palette.Num()<256 )
				Palette.AddItem(FColor(0,0,0,255));

			// Copy upside-down scanlines.
			for(UINT Y = 0;Y < bmhdr->biHeight;Y++)
			{
				for(UINT X = 0;X < bmhdr->biWidth;X++)
					((FColor*)&Texture->Mips(0).Data(0))[(Texture->SizeY - Y - 1) * Texture->SizeX + X] = Palette(*((BYTE*)Buffer + bmf->bfOffBits + Y * Align(bmhdr->biWidth,4) + X));
			}
		}
		else if( bmhdr->biPlanes==1 && bmhdr->biBitCount==24 )
		{
			// Set texture properties.
			Texture = CastChecked<UTexture2D>(StaticConstructObject( Class, InParent, Name, Flags ) );
			Texture->Init( bmhdr->biWidth, bmhdr->biHeight, PF_A8R8G8B8 );
			Texture->PostLoad();

			// Copy upside-down scanlines.
			const BYTE* Ptr = (BYTE*)Buffer + bmf->bfOffBits;
			for( INT y=0; y<(INT)bmhdr->biHeight; y++ )
			{
				BYTE* DestPtr = &Texture->Mips(0).Data((bmhdr->biHeight - 1 - y) * bmhdr->biWidth * 4);
				BYTE* SrcPtr = (BYTE*) &Ptr[y * bmhdr->biWidth * 3];
				for( INT x=0; x<(INT)bmhdr->biWidth; x++ )
				{
					*DestPtr++ = *SrcPtr++;
					*DestPtr++ = *SrcPtr++;
					*DestPtr++ = *SrcPtr++;
					*DestPtr++ = 0xFF;
				}
			}
		}
		else if( bmhdr->biPlanes==1 && bmhdr->biBitCount==16 )
		{
			Texture = CastChecked<UTexture2D>(StaticConstructObject( Class, InParent, Name, Flags ) );
			Texture->Init( bmhdr->biWidth, bmhdr->biHeight, PF_G16 );
			Texture->PostLoad();

			// Copy upside-down scanlines.
			const BYTE* Ptr = (BYTE*)Buffer + bmf->bfOffBits;
			for( INT y=0; y<(INT)bmhdr->biHeight; y++ )
			{
				_WORD* DestPtr = (_WORD*)&Texture->Mips(0).Data((bmhdr->biHeight - 1 - y) * bmhdr->biWidth * 2);
				_WORD* SrcPtr = (_WORD*) &Ptr[y * bmhdr->biWidth * 2];
				appMemcpy( DestPtr, SrcPtr, (INT)bmhdr->biWidth * 2 );
			}
		}
		else
		{
			Warn->Logf( TEXT("BMP uses an unsupported format (%i/%i)"), bmhdr->biPlanes, bmhdr->biBitCount );
			return NULL;
		}
	}
	//
	// PCX
	//
	else if( Length >= sizeof(FPCXFileHeader) && PCX->Manufacturer==10 )
	{
		INT NewU = PCX->XMax + 1 - PCX->XMin;
		INT NewV = PCX->YMax + 1 - PCX->YMin;
		if( (NewU&(NewU-1)) || (NewV&(NewV-1)) )
		{
			Warn->Logf( TEXT("Texture dimensions are not powers of two") );
			return NULL;
		}
		else if( PCX->NumPlanes==1 && PCX->BitsPerPixel==8 )
		{
			// Set texture properties.
			Texture = CastChecked<UTexture2D>(StaticConstructObject( Class, InParent, Name, Flags ) );
			Texture->Init( NewU, NewV, PF_A8R8G8B8 );
			Texture->PostLoad();

			// Import the palette.
			BYTE* PCXPalette = (BYTE *)(BufferEnd - 256 * 3);
			TArray<FColor>	Palette;
			for(UINT i=0; i<256; i++ )
				Palette.AddItem(FColor(PCXPalette[i*3+0],PCXPalette[i*3+1],PCXPalette[i*3+2],i == 0 ? 0 : 255));

			// Import it.
			FColor* DestPtr	= (FColor*)&Texture->Mips(0).Data(0);
			FColor* DestEnd	= DestPtr + NewU * NewV;
			Buffer += 128;
			while( DestPtr < DestEnd )
			{
				BYTE Color = *Buffer++;
				if( (Color & 0xc0) == 0xc0 )
				{
					UINT RunLength = Color & 0x3f;
					Color = *Buffer++;
					for(UINT Index = 0;Index < RunLength;Index++)
						*DestPtr++ = Palette(Color);
				}
				else *DestPtr++ = Palette(Color);
			}
		}
		else if( PCX->NumPlanes==3 && PCX->BitsPerPixel==8 )
		{
			// Set texture properties.
			Texture = CastChecked<UTexture2D>(StaticConstructObject( Class, InParent, Name, Flags ) );
			Texture->Init( NewU, NewV, PF_A8R8G8B8 );
			Texture->PostLoad();

			// Copy upside-down scanlines.
			Buffer += 128;
			INT CountU = Min<INT>(PCX->BytesPerLine,NewU);
			TArray<BYTE>& Dest = Texture->Mips(0).Data;
			for( INT i=0; i<NewV; i++ )
			{

				// We need to decode image one line per time building RGB image color plane by color plane.
				INT RunLength, Overflow=0;
				BYTE Color=0;
				for( INT ColorPlane=2; ColorPlane>=0; ColorPlane-- )
				{
					for( INT j=0; j<CountU; j++ )
					{
						if(!Overflow)
						{
							Color = *Buffer++;
							if((Color & 0xc0) == 0xc0)
							{
								RunLength=Min ((Color&0x3f), CountU-j);
								Overflow=(Color&0x3f)-RunLength;
								Color=*Buffer++;
							}
							else
								RunLength = 1;
						}
						else
						{
							RunLength=Min (Overflow, CountU-j);
							Overflow=Overflow-RunLength;
						}
	
						for( INT k=j; k<j+RunLength; k++ )
							Dest( (i*NewU+k)*4 + ColorPlane ) = Color;
						j+=RunLength-1;
					}
				}				
			}
		}
		else
		{
			Warn->Logf( TEXT("PCX uses an unsupported format (%i/%i)"), PCX->NumPlanes, PCX->BitsPerPixel );
			return NULL;
		}
	}
	//
	// TGA
	//
	else if( Length >= sizeof(FTGAFileHeader) && 
       ( appStricmp( Type, TEXT("TGA") ) == 0 || 
            appStricmp( Type, TEXT("tga") ) == 0 ) )
	{
		if( TGA->ImageTypeCode == 10 ) // 10 = RLE compressed 
		{
			// RLE compression: CHUNKS: 1 -byte header, high bit 0 = raw, 1 = compressed
			// bits 0-6 are a 7-bit count; count+1 = number of raw pixels following, or rle pixels to be expanded. 

			if(TGA->BitsPerPixel == 32)
			{
				Texture = CastChecked<UTexture2D>(StaticConstructObject(Class,InParent,Name,Flags));
				Texture->Init(TGA->Width,TGA->Height,PF_A8R8G8B8);
				Texture->PostLoad();

				BYTE*	IdData		= (BYTE*)TGA + sizeof(FTGAFileHeader); 
				BYTE*	ColorMap	= IdData + TGA->IdFieldLength;
				BYTE*	ImageData	= (BYTE*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);				
				DWORD*	TextureData = (DWORD*) &Texture->Mips(0).Data(0);
				DWORD	Pixel		= 0;
				INT     RLERun		= 0;
				INT     RAWRun		= 0;

				for(INT Y = TGA->Height-1; Y >=0; Y--) // Y-flipped.
				{					
					for(INT X = 0;X < TGA->Width;X++)
					{						
						if( RLERun > 0 )
						{
							RLERun--;  // reuse current Pixel data.
						}
						else if( RAWRun == 0 ) // new raw pixel or RLE-run.
						{
							BYTE RLEChunk = *(ImageData++);							
							if( RLEChunk & 0x80 )
							{
								RLERun = ( RLEChunk & 0x7F ) + 1;
								RAWRun = 1;
							}
							else
							{
								RAWRun = ( RLEChunk & 0x7F ) + 1;
							}
						}							
						// Retrieve new pixel data - raw run or single pixel for RLE stretch.
						if( RAWRun > 0 )
						{
							Pixel = *(DWORD*)ImageData; // RGBA 32-bit dword.
							ImageData += 4;
							RAWRun--;
							RLERun--;
						}
						// Store.
						*( (TextureData + Y*TGA->Width)+X ) = Pixel;
					}
				}
			}
			else if( TGA->BitsPerPixel == 24 )
			{	
				Texture = CastChecked<UTexture2D>(StaticConstructObject(Class,InParent,Name,Flags));
				Texture->Init(TGA->Width,TGA->Height,PF_A8R8G8B8);
				Texture->PostLoad();

				BYTE*	IdData = (BYTE*)TGA + sizeof(FTGAFileHeader); 
				BYTE*	ColorMap = IdData + TGA->IdFieldLength;
				BYTE*	ImageData = (BYTE*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
				DWORD*	TextureData = (DWORD*) &Texture->Mips(0).Data(0);
				BYTE    Pixel[4];
				INT     RLERun = 0;
				INT     RAWRun = 0;
				
				for(INT Y = TGA->Height-1; Y >=0; Y--) // Y-flipped.
				{					
					for(INT X = 0;X < TGA->Width;X++)
					{						
						if( RLERun > 0 )
							RLERun--;  // reuse current Pixel data.
						else if( RAWRun == 0 ) // new raw pixel or RLE-run.
						{
							BYTE RLEChunk = *(ImageData++);
							if( RLEChunk & 0x80 )
							{
								RLERun = ( RLEChunk & 0x7F ) + 1;
								RAWRun = 1;
							}
							else
							{
								RAWRun = ( RLEChunk & 0x7F ) + 1;
							}
						}							
						// Retrieve new pixel data - raw run or single pixel for RLE stretch.
						if( RAWRun > 0 )
						{
							Pixel[0] = *(ImageData++);
							Pixel[1] = *(ImageData++);
							Pixel[2] = *(ImageData++);
							Pixel[3] = 255;
							RAWRun--;
							RLERun--;
						}
						// Store.
						*( (TextureData + Y*TGA->Width)+X ) = *(DWORD*)&Pixel;
					}
				}
			}
			else if( TGA->BitsPerPixel == 16 )
			{
				Texture = CastChecked<UTexture2D>(StaticConstructObject(Class,InParent,Name,Flags));
				Texture->Init(TGA->Width,TGA->Height,PF_G16);
				Texture->PostLoad();
				
				BYTE*	IdData = (BYTE*)TGA + sizeof(FTGAFileHeader);
				BYTE*	ColorMap = IdData + TGA->IdFieldLength;				
				_WORD*	ImageData = (_WORD*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
				_WORD*	TextureData = (_WORD*) &Texture->Mips(0).Data(0);
				_WORD    Pixel = 0;
				INT     RLERun = 0;
				INT     RAWRun = 0;

				for(INT Y = TGA->Height-1; Y >=0; Y--) // Y-flipped.
				{					
					for( INT X=0;X<TGA->Width;X++ )
					{						
						if( RLERun > 0 )
							RLERun--;  // reuse current Pixel data.
						else if( RAWRun == 0 ) // new raw pixel or RLE-run.
						{
							BYTE RLEChunk =  *((BYTE*)ImageData);
							ImageData = (_WORD*)(((BYTE*)ImageData)+1);
							if( RLEChunk & 0x80 )
							{
								RLERun = ( RLEChunk & 0x7F ) + 1;
								RAWRun = 1;
							}
							else
							{
								RAWRun = ( RLEChunk & 0x7F ) + 1;
							}
						}							
						// Retrieve new pixel data - raw run or single pixel for RLE stretch.
						if( RAWRun > 0 )
						{ 
							Pixel = *(ImageData++);
							RAWRun--;
							RLERun--;
						}
						// Store.
						*( (TextureData + Y*TGA->Width)+X ) = Pixel;
					}
				}
			}
			else
			{
				Warn->Logf(TEXT("TGA uses an unsupported rle-compressed bit-depth: %u"),TGA->BitsPerPixel);
				return NULL;
			}
		}
		else if(TGA->ImageTypeCode == 2) // 2 = Uncompressed 
		{
			if(TGA->BitsPerPixel == 32)
			{
				Texture = CastChecked<UTexture2D>(StaticConstructObject(Class,InParent,Name,Flags));
				Texture->Init(TGA->Width,TGA->Height,PF_A8R8G8B8);
				Texture->PostLoad();

				BYTE*	IdData = (BYTE*)TGA + sizeof(FTGAFileHeader);
				BYTE*	ColorMap = IdData + TGA->IdFieldLength;
				DWORD*	ImageData = (DWORD*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
				DWORD*	TextureData = (DWORD*) &Texture->Mips(0).Data(0);

				for(INT Y = 0;Y < TGA->Height;Y++)
					appMemcpy(TextureData + Y * TGA->Width,ImageData + (TGA->Height - Y - 1) * TGA->Width,TGA->Width * 4);
			}
			else if(TGA->BitsPerPixel == 16)
			{
				Texture = CastChecked<UTexture2D>(StaticConstructObject(Class,InParent,Name,Flags));
				Texture->Init(TGA->Width,TGA->Height,PF_G16);
				Texture->PostLoad();

				BYTE*	IdData = (BYTE*)TGA + sizeof(FTGAFileHeader);
				BYTE*	ColorMap = IdData + TGA->IdFieldLength;
				_WORD*	ImageData = (_WORD*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
				_WORD*	TextureData = (_WORD*) &Texture->Mips(0).Data(0);

				// Monochrome 16-bit format - terrain heightmaps.
				for(INT Y = 0;Y < TGA->Height;Y++)
					appMemcpy(TextureData + Y * TGA->Width,ImageData + (TGA->Height - Y - 1) * TGA->Width,TGA->Width * 2);
			}            
			else if(TGA->BitsPerPixel == 24)
			{
				Texture = CastChecked<UTexture2D>(StaticConstructObject(Class,InParent,Name,Flags));
				Texture->Init(TGA->Width,TGA->Height,PF_A8R8G8B8);
				Texture->PostLoad();

				BYTE*	IdData = (BYTE*)TGA + sizeof(FTGAFileHeader);
				BYTE*	ColorMap = IdData + TGA->IdFieldLength;
				BYTE*	ImageData = (BYTE*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
				DWORD*	TextureData = (DWORD*) &Texture->Mips(0).Data(0);
				BYTE    Pixel[4];

				for(INT Y = 0;Y < TGA->Height;Y++)
				{
					for(INT X = 0;X < TGA->Width;X++)
					{
						Pixel[0] = *(( ImageData+( TGA->Height-Y-1 )*TGA->Width*3 )+X*3+0);
						Pixel[1] = *(( ImageData+( TGA->Height-Y-1 )*TGA->Width*3 )+X*3+1);
						Pixel[2] = *(( ImageData+( TGA->Height-Y-1 )*TGA->Width*3 )+X*3+2);
						Pixel[3] = 255;
						*((TextureData+Y*TGA->Width)+X) = *(DWORD*)&Pixel;
					}
				}
			}
			else
			{
				Warn->Logf(TEXT("TGA uses an unsupported bit-depth: %u"),TGA->BitsPerPixel);
				return NULL;
			}
		}
		else
		{
			Warn->Logf(TEXT("TGA is an unsupported type: %u"),TGA->ImageTypeCode);
			return NULL;
		}
	}
	else
	{
		// Unknown format.
		Warn->Logf( TEXT("Bad image format for texture import") );
		return NULL;
 	}

	// Propagate options.

	Texture->CompressionSettings	= CompressionSettings;
	Texture->CompressionNone		= NoCompression;
	Texture->CompressionNoAlpha		= NoAlpha;

	// Packed normal map

	if( CompressionSettings == TC_Normalmap || CompressionSettings == TC_NormalmapAlpha )
	{
		Texture->SRGB					= 0;
		Texture->UnpackMin				= FPlane(-1,-1,-1,0);
		Texture->UnpackMax				= FPlane(+1,+1,+1,1);
	}

	// Compress RGBA textures and also store source art.
	
	if( Texture->Format == PF_A8R8G8B8 )
	{
		// PNG Compress.

		FPNGHelper PNG;
		PNG.InitRaw( &Texture->Mips(0).Data(0), Texture->Mips(0).Data.Num(), Texture->SizeX, Texture->SizeY );
		TArray<BYTE> CompressedData = PNG.GetCompressedData();
		check( CompressedData.Num() );

		// Store source art.

		Texture->SourceArt.Empty( CompressedData.Num() );
		Texture->SourceArt.Add( CompressedData.Num() );
		appMemcpy( &Texture->SourceArt(0), &CompressedData(0), CompressedData.Num() );

		// Detach lazy loader.

		Texture->SourceArt.Detach();

		// Compress texture based on options.
	
		// Texture->Compress(); // PostEditChange below will automatically recompress.
	}
	else
		Texture->CompressionNone = 1;

	// Invalidate any materials using the newly imported texture. (occurs if you import over an existing texture)

	Texture->PostEditChange(NULL);

	// If we are automatically creating a material for this texture...

	if( bCreateMaterial )
	{
		// Create the material

		UMaterialFactoryNew* factory = new UMaterialFactoryNew;
		UMaterial* material = (UMaterial*)factory->FactoryCreateNew( UMaterial::StaticClass(), InParent, *FString::Printf( TEXT("%s_Mat"), *Name ), Flags, Context, Warn );

		// Create a texture reference for the texture we just imported and hook it up to the diffuse channel

		material->DiffuseColor.Expression = ConstructObject<UMaterialExpression>( UMaterialExpressionTextureSample::StaticClass(), material );
		((UMaterialExpressionTextureSample*)material->DiffuseColor.Expression)->Texture = Texture;

		TArray<FExpressionOutput>	Outputs;
		material->DiffuseColor.Expression->GetOutputs(Outputs);

		FExpressionOutput* output = &Outputs(0);
		material->DiffuseColor.Mask = output->Mask;
		material->DiffuseColor.MaskR = output->MaskR;
		material->DiffuseColor.MaskG = output->MaskG;
		material->DiffuseColor.MaskB = output->MaskB;
		material->DiffuseColor.MaskA = output->MaskA;

		// Clean up

		delete factory;
	}

	return Texture;
}
IMPLEMENT_CLASS(UTextureFactory);

/*-----------------------------------------------------------------------------
	UVolumeTextureFactory.
-----------------------------------------------------------------------------*/

void UVolumeTextureFactory::StaticConstructor()
{
	SupportedClass = UTexture3D::StaticClass();
	new(Formats)FString(TEXT("dds;Texture"));
	bCreateNew = 0;

	new(GetClass()->HideCategories) FName(TEXT("Object"));
	new(GetClass(),TEXT("CompressionNoAlpha")	,RF_Public)	UBoolProperty(CPP_PROPERTY(NoAlpha				),TEXT(""),CPF_Edit			);
}
UVolumeTextureFactory::UVolumeTextureFactory()
{
	bEditorImport = 1;
}
UObject* UVolumeTextureFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	UTexture3D*				Texture = NULL;
	UINT					Length	= BufferEnd - Buffer;
	const FDDSFileHeader*	DDS		= (FDDSFileHeader *)Buffer;

	//
	// DDS
	//
	if( Length >= sizeof(FDDSFileHeader) && DDS->Magic==0x20534444 && appStricmp( Type, TEXT("DDS") ) == 0 )
	{
		Buffer += sizeof(FDDSFileHeader);
		Length -= sizeof(FDDSFileHeader);

		UINT	Width	= DDS->desc.dwWidth,
				Height	= DDS->desc.dwHeight,
				Depth	= DDS->desc.dwDepth;

		DWORD	RequiredFlags;
		
		RequiredFlags = DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT | DDSD_DEPTH;
		if( (DDS->desc.dwFlags & RequiredFlags) != RequiredFlags )
		{
			Warn->Logf( TEXT("Only uncompressed RBGA8 volume textures can be imported.") );
			return NULL;
		}

		RequiredFlags = DDPF_ALPHAPIXELS | DDPF_RGB;
		if( (DDS->desc.ddpfPixelFormat.dwFlags & RequiredFlags) != RequiredFlags )
		{
			Warn->Logf( TEXT("Only uncompressed RBGA8 volume textures can be imported.") );
			return NULL;
		}

		if( DDS->desc.ddpfPixelFormat.dwRBitMask		!= 0x00FF0000 
		||	DDS->desc.ddpfPixelFormat.dwGBitMask		!= 0x0000FF00 
		||	DDS->desc.ddpfPixelFormat.dwBBitMask		!= 0x000000FF 
		||	DDS->desc.ddpfPixelFormat.dwRGBAlphaBitMask	!= 0xFF000000 )
		{
			Warn->Logf( TEXT("Only uncompressed RBGA8 volume textures can be imported.") );
			return NULL;
		}

		Texture = CastChecked<UTexture3D>(StaticConstructObject(Class,InParent,Name,Flags));
		Texture->Init(Width,Height,Depth,PF_A8R8G8B8);
		Texture->PostLoad();
		Texture->RGBE					= 0;
		Texture->SRGB					= 1;
		Texture->CompressionNoMipmaps	= 0;

		check( Length >= Width * Height * Depth * 4 );
		appMemcpy( &Texture->Mips(0).Data(0), Buffer, Width * Height * Depth * 4 );
	}
	else
	{
		// Unknown format.
		Warn->Logf( TEXT("Bad image format for texture import.") );
		return NULL;
 	}

	// Propagate options.
	Texture->CompressionSettings	= TC_Default;
	Texture->CompressionNone		= 0;
	Texture->CompressionNoAlpha		= NoAlpha;
	
	// PNG Compress.
	FPNGHelper PNG;
	PNG.InitRaw( &Texture->Mips(0).Data(0), Texture->Mips(0).Data.Num(), Texture->SizeX, Texture->SizeY * Texture->SizeZ );
	TArray<BYTE> CompressedData = PNG.GetCompressedData();
	check( CompressedData.Num() );

	// Store source art.
	Texture->SourceArt.Empty( CompressedData.Num() );
	Texture->SourceArt.Add( CompressedData.Num() );
	appMemcpy( &Texture->SourceArt(0), &CompressedData(0), CompressedData.Num() );

	// Detach lazy loader.
	Texture->SourceArt.Detach();

	// Invalidate any materials using the newly imported texture - occurs if you import over an existing texture. Also calls "Compress".
	Texture->PostEditChange(NULL);

	return Texture;
}
IMPLEMENT_CLASS(UVolumeTextureFactory);


/*------------------------------------------------------------------------------
	UTextureExporterPCX implementation.
------------------------------------------------------------------------------*/

void UTextureExporterPCX::StaticConstructor()
{
	SupportedClass = UTexture2D::StaticClass();
	new(FormatExtension)FString(TEXT("PCX"));
	new(FormatDescription)FString(TEXT("PCX File"));

}
UBOOL UTextureExporterPCX::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn )
{
	UTexture2D* Texture = CastChecked<UTexture2D>( Object );

	Texture->SourceArt.Load();

	if( !Texture->SourceArt.Num() )
		return 0;

	FPNGHelper PNG;
	PNG.InitCompressed( &Texture->SourceArt(0), Texture->SourceArt.Num(), Texture->SizeX, Texture->SizeY );
	TArray<BYTE> RawData = PNG.GetRawData();

	// Set all PCX file header properties.
	FPCXFileHeader PCX;
	appMemzero( &PCX, sizeof(PCX) );
	PCX.Manufacturer	= 10;
	PCX.Version			= 05;
	PCX.Encoding		= 1;
	PCX.BitsPerPixel	= 8;
	PCX.XMin			= 0;
	PCX.YMin			= 0;
	PCX.XMax			= Texture->SizeX-1;
	PCX.YMax			= Texture->SizeY-1;
	PCX.XDotsPerInch	= Texture->SizeX;
	PCX.YDotsPerInch	= Texture->SizeY;
	PCX.BytesPerLine	= Texture->SizeX;
	PCX.PaletteType		= 0;
	PCX.HScreenSize		= 0;
	PCX.VScreenSize		= 0;

	// Copy all RLE bytes.
	BYTE RleCode=0xc1;

	PCX.NumPlanes = 3;
	Ar << PCX;
	for( UINT Line=0; Line<Texture->SizeY; Line++ )
	{
		for( INT ColorPlane = 2; ColorPlane >= 0; ColorPlane-- )
		{
			BYTE* ScreenPtr = &RawData(0) + (Line * Texture->SizeX * 4) + ColorPlane;
			for( UINT Row=0; Row<Texture->SizeX; Row++ )
			{
				if( (*ScreenPtr&0xc0)==0xc0 )
					Ar << RleCode;
				Ar << *ScreenPtr;
				ScreenPtr += 4;
			}
		}
	}

	Texture->SourceArt.Unload();

	return 1;

}
IMPLEMENT_CLASS(UTextureExporterPCX);

/*------------------------------------------------------------------------------
	UTextureExporterBMP implementation.
------------------------------------------------------------------------------*/

void UTextureExporterBMP::StaticConstructor()
{
	SupportedClass = UTexture2D::StaticClass();
	new(FormatExtension)FString(TEXT("BMP"));
	new(FormatDescription)FString(TEXT("Windows Bitmap"));

}
UBOOL UTextureExporterBMP::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn )
{
	UTexture2D* Texture = CastChecked<UTexture2D>( Object );

	// Figure out format.
	EPixelFormat Format = (EPixelFormat)Texture->Format;

	FBitmapFileHeader bmf;
	FBitmapInfoHeader bmhdr;

	// File header.
	bmf.bfType      = 'B' + (256*(INT)'M');
	bmf.bfReserved1 = 0;
	bmf.bfReserved2 = 0;
	INT biSizeImage;

	Texture->SourceArt.Load();
	
	if( Format == PF_G16 )
	{
		biSizeImage		= Texture->SizeX * Texture->SizeY * 2;
		bmf.bfOffBits   = sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader);
		bmhdr.biBitCount= 16;
	}
	else if( Texture->SourceArt.Num() )
	{
		biSizeImage		= Texture->SizeX * Texture->SizeY * 3;
		bmf.bfOffBits   = sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader);
		bmhdr.biBitCount= 24;
	}
	else
		return 0;

	bmf.bfSize		= bmf.bfOffBits + biSizeImage;
	Ar << bmf;

	// Info header.
	bmhdr.biSize          = sizeof(FBitmapInfoHeader);
	bmhdr.biWidth         = Texture->SizeX;
	bmhdr.biHeight        = Texture->SizeY;
	bmhdr.biPlanes        = 1;
	bmhdr.biCompression   = BCBI_RGB;
	bmhdr.biSizeImage     = biSizeImage;
	bmhdr.biXPelsPerMeter = 0;
	bmhdr.biYPelsPerMeter = 0;
	bmhdr.biClrUsed       = 0;
	bmhdr.biClrImportant  = 0;
	Ar << bmhdr;

	if( Format == PF_G16 )
	{
		Texture->Mips(0).Data.Load();
		for( INT i=Texture->SizeY-1; i>=0; i-- )
			Ar.Serialize( &Texture->Mips(0).Data(i*Texture->SizeX*2), Texture->SizeY*2 );		
		Texture->Mips(0).Data.Unload();
	}
	else if( Texture->SourceArt.Num() )
	{		
		FPNGHelper PNG;
		PNG.InitCompressed( &Texture->SourceArt(0), Texture->SourceArt.Num(), Texture->SizeX, Texture->SizeY );
		TArray<BYTE> RawData = PNG.GetRawData();

		// Upside-down scanlines.
		for( INT i=Texture->SizeY-1; i>=0; i-- )
		{
			BYTE* ScreenPtr = &RawData(i*Texture->SizeX*4);
			for( INT j=Texture->SizeX; j>0; j-- )
			{
				Ar << *ScreenPtr++;
				Ar << *ScreenPtr++;
				Ar << *ScreenPtr++;
				*ScreenPtr++;
			}
		}
	}
	else
		return 0;

	Texture->SourceArt.Unload();

	return 1;
}
IMPLEMENT_CLASS(UTextureExporterBMP);

/*------------------------------------------------------------------------------
	UTextureExporterTGA implementation.
------------------------------------------------------------------------------*/

void UTextureExporterTGA::StaticConstructor()
{
	SupportedClass = UTexture2D::StaticClass();
	new(FormatExtension)FString(TEXT("TGA"));
	new(FormatDescription)FString(TEXT("Targa"));

}
UBOOL UTextureExporterTGA::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn )
{
	UTexture2D* Texture = CastChecked<UTexture2D>( Object );

	Texture->SourceArt.Load();

	if( !Texture->SourceArt.Num() )
		return 0;

	FPNGHelper PNG;
	PNG.InitCompressed( &Texture->SourceArt(0), Texture->SourceArt.Num(), Texture->SizeX, Texture->SizeY );
	TArray<BYTE> RawData = PNG.GetRawData();

	FTGAFileHeader TGA;
	appMemzero( &TGA, sizeof(TGA) );
	TGA.ImageTypeCode = 2;
	TGA.BitsPerPixel = 32;
	TGA.Height = Texture->SizeY;
	TGA.Width = Texture->SizeX;

	Ar.Serialize( &TGA, sizeof(TGA) );

	for( UINT Y=0;Y < Texture->SizeY;Y++ )
		Ar.Serialize( &RawData( (Texture->SizeY - Y - 1) * Texture->SizeX * 4 ), Texture->SizeX * 4 );

	FTGAFileFooter Ftr;
	appMemzero( &Ftr, sizeof(Ftr) );
	appMemcpy( Ftr.Signature, "TRUEVISION-XFILE", 16 );
	Ftr.TrailingPeriod = '.';
	Ar.Serialize( &Ftr, sizeof(Ftr) );

	Texture->SourceArt.Unload();

	return 1;
}
IMPLEMENT_CLASS(UTextureExporterTGA);
/*------------------------------------------------------------------------------
	UFontFactory.
------------------------------------------------------------------------------*/

//
//	Fast pixel-lookup.
//
static inline BYTE AT( BYTE* Screen, UINT SXL, UINT X, UINT Y )
{
	return Screen[X+Y*SXL];
}

//
// Codepage 850 -> Latin-1 mapping table:
//
BYTE FontRemap[256] = 
{
	0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,

	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
	80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
	112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,

	000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,
	032,173,184,156,207,190,124,245,034,184,166,174,170,196,169,238,
	248,241,253,252,239,230,244,250,247,251,248,175,172,171,243,168,

	183,181,182,199,142,143,146,128,212,144,210,211,222,214,215,216,
	209,165,227,224,226,229,153,158,157,235,233,234,154,237,231,225,
	133,160,131,196,132,134,145,135,138,130,136,137,141,161,140,139,
	208,164,149,162,147,228,148,246,155,151,163,150,129,236,232,152,
};

//
//	Find the border around a font glyph that starts at x,y (it's upper
//	left hand corner).  If it finds a glyph box, it returns 0 and the
//	glyph 's length (xl,yl).  Otherwise returns -1.
//
static UBOOL ScanFontBox( UTexture2D* Texture, INT X, INT Y, INT& XL, INT& YL )
{
	BYTE* Data = &Texture->Mips(0).Data(0);
	INT FontXL = Texture->SizeX;

	// Find x-length.
	INT NewXL = 1;
	while ( AT(Data,FontXL,X+NewXL,Y)==255 && AT(Data,FontXL,X+NewXL,Y+1)!=255 )
		NewXL++;

	if( AT(Data,FontXL,X+NewXL,Y)!=255 )
		return 0;

	// Find y-length.
	INT NewYL = 1;
	while( AT(Data,FontXL,X,Y+NewYL)==255 && AT(Data,FontXL,X+1,Y+NewYL)!=255 )
		NewYL++;

	if( AT(Data,FontXL,X,Y+NewYL)!=255 )
		return 0;

	XL = NewXL - 1;
	YL = NewYL - 1;

	return 1;
}

void UFontFactory::StaticConstructor()
{
	SupportedClass = UFont::StaticClass();

}
UFontFactory::UFontFactory()
{
	bEditorImport = 0;
}

#define NUM_FONT_CHARS 256

UObject* UFontFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	check(Class==UFont::StaticClass());
	UFont* Font = new( InParent, Name, Flags )UFont;
	UTexture2D* Tex = CastChecked<UTexture2D>( UTextureFactory::FactoryCreateBinary( UTexture2D::StaticClass(), Font, NAME_None, 0, Context, Type, Buffer, BufferEnd, Warn ) );
	Font->Textures.AddItem(Tex);
	if( Tex )
	{
		// Init.
		BYTE* TextureData = &Tex->Mips(0).Data(0);
		Font->Characters.AddZeroed( NUM_FONT_CHARS );

		// Scan in all fonts, starting at glyph 32.
		UINT i = 32;
		UINT Y = 0;
		do
		{
			UINT X = 0;
			while( AT(TextureData,Tex->SizeX,X,Y)!=255 && Y<Tex->SizeY )
			{
				X++;
				if( X >= Tex->SizeX )
				{
					X = 0;
					if( ++Y >= Tex->SizeY )
						break;
				}
			}

			// Scan all glyphs in this row.
			if( Y < Tex->SizeY )
			{
				INT XL=0, YL=0, MaxYL=0;
				while( i<(UINT)Font->Characters.Num() && ScanFontBox(Tex,X,Y,XL,YL) )
				{
					Font->Characters(i).StartU = X+1;
					Font->Characters(i).StartV = Y+1;
					Font->Characters(i).USize  = XL;
					Font->Characters(i).VSize  = YL;
					Font->Characters(i).TextureIndex = 0;
					X += XL + 1;
					i++;
					if( YL > MaxYL )
						MaxYL = YL;
				}
				Y += MaxYL + 1;
			}
		} while( i<(UINT)Font->Characters.Num() && Y<Tex->SizeY );

		// Cleanup font data.
		for( i=0; i<(UINT)Tex->Mips.Num(); i++ )
			for( INT j=0; j<Tex->Mips(i).Data.Num(); j++ )
				if( Tex->Mips(i).Data(j)==255 )
					Tex->Mips(i).Data(j) = 0;

		// Remap old fonts.
		TArray<FFontCharacter> Old = Font->Characters;
		for( i=0; i<(UINT)Font->Characters.Num(); i++ )
			Font->Characters(i) = Old(FontRemap[i]);
		return Font;
	}
	else return NULL;
}
IMPLEMENT_CLASS(UFontFactory);

/*------------------------------------------------------------------------------
	USequenceFactory implementation.
------------------------------------------------------------------------------*/

void USequenceFactory::StaticConstructor()
{
	SupportedClass = USequence::StaticClass();
	new(Formats)FString(TEXT("t3d;Unreal Sequence"));
	bCreateNew = 0;
	bText = 1;

}

USequenceFactory::USequenceFactory()
{
	bEditorImport = 1;
}

static FName MakeUniqueSubsequenceName( UObject* Outer, USequence* SubSeq )
{
	TCHAR NewBase[NAME_SIZE], Result[NAME_SIZE];
	TCHAR TempIntStr[NAME_SIZE];

	// Make base name sans appended numbers.
	appStrcpy( NewBase, SubSeq->GetName() );
	TCHAR* End = NewBase + appStrlen(NewBase);
	while( End>NewBase && (appIsDigit(End[-1]) || End[-1] == TEXT('_')) )
		End--;
	*End = 0;

	// Append numbers to base name.
	INT TryNum = 0;
	do
	{
		appSprintf( TempIntStr, TEXT("_%i"), TryNum++ );
		appStrncpy( Result, NewBase, NAME_SIZE-appStrlen(TempIntStr)-1 );
		appStrcat( Result, TempIntStr );
	} 
	while( FindObject<USequenceObject>( Outer, Result ) );

	return Result;
}

/**
 *	Create a USequence from text.
 *
 *	@param	InParent	Usually the parent sequence, but might be a package for example. Used as outer for created SequenceObjects.
 *	@param	Flags		Flags used when creating SequenceObjects
 *	@param	Type		If "paste", the newly created SequenceObjects are added to GSelectionTools.
 *	@param	Buffer		Text buffer with description of sequence
 *	@param	BufferEnd	End of text info buffer
 *	@param	Warn		Device for squirting warnings to
 *
 *	
 *	Note that we assume that all subobjects of a USequence are SequenceObjects, and will be explicitly added to its SequenceObjects array ( SequenceObjects(x)=... lines are ignored )
 *	This is because objects may already exist in parent sequence etc. It does mean that if we ever add non-SequenceObject subobjects to Sequence it won't work. Hmm..
 */
UObject* USequenceFactory::FactoryCreateText
(
	ULevel*				UnusedInLevel,
	UClass*				UnusedClass,
	UObject*			InParent,
	FName				UnusedName,
	DWORD				Flags,
	UObject*			UnusedContext,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	USequence* ParentSeq = Cast<USequence>(InParent);

	// We keep a mapping of new, empty sequence objects to their property text.
	// We want to create all new SequenceObjects first before importing their properties (which will create links)
	TArray<USequenceObject*>		NewSeqObjs;
	TMap<USequenceObject*,FString>	PropMap;
	TMap<USequence*,FString>		SubObjMap;

	UBOOL bIsPasting = (appStricmp(Type,TEXT("paste"))==0);

	USequence* OutSequence = NULL;

	ParseNext( &Buffer );

	FString StrLine;
	while( ParseLine(&Buffer,StrLine) )
	{
		const TCHAR* Str = *StrLine;
		if( GetBEGIN(&Str,TEXT("OBJECT")) )
		{
			UClass* SeqObjClass;
			if( ParseObject<UClass>( Str, TEXT("CLASS="), SeqObjClass, ANY_PACKAGE ) )
			{
				check( SeqObjClass->IsChildOf(USequenceObject::StaticClass()) );

				FName SeqObjName(NAME_None);
				Parse( Str, TEXT("NAME="), SeqObjName );

				// Make sure this name is unique within this sequence.
				USequenceObject* Found=NULL;
				if( SeqObjName!=NAME_None )
				{
					Found = FindObject<USequenceObject>( InParent, *SeqObjName );
				}

				// If there is already a SeqObj with this name, rename it.
				if( Found )
				{
					check(Found->GetOuter() == InParent);

					// For renaming subsequences, we want to use its current name (not the class name) as the basis for the new name as we actually display it.
					// We also want to keep the ObjName string the same as the new name.
					USequence* FoundSeq = Cast<USequence>(Found);
					if(FoundSeq)
					{
						FName NewFoundSeqName = MakeUniqueSubsequenceName(InParent, FoundSeq);
						FoundSeq->Rename(*NewFoundSeqName, InParent);
						FoundSeq->ObjName = FString(*NewFoundSeqName);
					}
					else
					{
						Found->Rename();
					}
				}

				USequenceObject* NewSeqObj = ConstructObject<USequenceObject>( SeqObjClass, InParent, SeqObjName, Flags );
				USequence* NewSeq = Cast<USequence>(NewSeqObj);

				// If our text contains multiple top-level Sequences, we will just return the last one parsed.
				if(NewSeq)
				{
					OutSequence = NewSeq;
				}

				// Get property text for the new sequence object.
				FString PropText, PropLine;
				FString SubObjText;
				INT ObjDepth = 1;
				while ( ParseLine( &Buffer, PropLine ) )
				{
					const TCHAR* PropStr = *PropLine;

					// Ignore lines that assign the objects to SequenceObjects.
					// We just add all SequenceObjects within this sequence to the SequenceObjects array. Thats because we may need to rename them etc.
					if( appStrstr(PropStr, TEXT(" SequenceObjects(")) )
						continue;

					// Track how deep we are in contained sets of sub-objects.
					UBOOL bEndLine = false;
					if( GetBEGIN(&PropStr, TEXT("OBJECT")) )
					{
						ObjDepth++;
					}
					else if( GetEND(&PropStr, TEXT("OBJECT")) )
					{
						bEndLine = true;

						// When close out our initial BEGIN OBJECT, we are done with this object.
						if(ObjDepth == 1)
						{
							break;
						}
					}

					// If this is a sequence object, we stick its sub-object properties in a seperate string.
					if(NewSeq && ObjDepth > 1)
					{
						SubObjText += *PropLine;
						SubObjText += TEXT("\r\n");
					}
					else
					{
						PropText += *PropLine;
						PropText += TEXT("\r\n");
					}

					if(bEndLine)
					{
						ObjDepth--;
					}
				}

				// Save property text and possibly sub-object text in the case of sub-sequence.
				PropMap.Set( NewSeqObj, *PropText );
				if(NewSeq)
				{
					SubObjMap.Set( NewSeq, *SubObjText );
				}

				NewSeqObjs.AddItem(NewSeqObj);

				// Add sequence object to the parent sequence
				if(ParentSeq)
				{
					ParentSeq->SequenceObjects.AddItem(NewSeqObj);
				}
			}
		}
	}

	if(bIsPasting)
	{
		GSelectionTools.SelectNone( USequenceObject::StaticClass() );
	}

	DWORD OldUglyHackFlags = GUglyHackFlags;
	GUglyHackFlags |= 0x02; // When parsing text object names, look for objects inside Parent first.

	for(INT i=0; i<NewSeqObjs.Num(); i++)
	{
		USequenceObject* NewSeqObj = NewSeqObjs(i);
		FString* PropText = PropMap.Find(NewSeqObj);
		check(PropText); // Every new object should have property text.

		// First, if this is a sub-sequence, use this factory again and feed in the sub object text to create all the sequence sub-objects.
		USequence* NewSeq = Cast<USequence>(NewSeqObj);
		if(NewSeq)
		{
			FString* SubObjText = SubObjMap.Find(NewSeq);
			check(SubObjText);

			const TCHAR* SubObjStr = **SubObjText;

			//debugf( TEXT("-----SUBOBJECT STRING------") );
			//debugf( TEXT("%s"), SubObjStr );

			USequenceFactory* TempSebSeqFactor = new USequenceFactory;
			TempSebSeqFactor->FactoryCreateText( NULL, NULL, NewSeqObj, NAME_None, Flags, NULL, TEXT(""), SubObjStr, SubObjStr+appStrlen(SubObjStr), GWarn );
			delete TempSebSeqFactor;
		}

		// Now import properties into newly created sequence object. Should create links correctly...
		const TCHAR* PropStr = **PropText;

		//debugf( TEXT("-----PROP STRING------") );
		//debugf( TEXT("%s"), PropStr );

		NewSeqObj->PreEditChange();

		UInterpData* IData = Cast<UInterpData>(NewSeqObj);
		if(IData)
		{
			ImportObjectProperties( NewSeqObj->GetClass(), (BYTE*)NewSeqObj, NULL, PropStr, NewSeqObj, NewSeqObj, Warn, 0, NewSeqObj );
		}
		else
		{
			ImportObjectProperties( NewSeqObj->GetClass(), (BYTE*)NewSeqObj, NULL, PropStr, NewSeqObj, InParent, Warn, 0, NewSeqObj );
		}

		// If this is a sequence Op, ensure that no output, var or event links point to an SequenceObject with a different Outer ie. is in a different SubSequence.
		USequenceOp* SeqOp = Cast<USequenceOp>(NewSeqObj);
		if(SeqOp)
		{
			SeqOp->CleanupConnections();
		}

		// If this is Matinee data - clear the CurveEdSetup as the references to tracks get screwed up by text export/import.
		if(IData)
		{
			IData->CurveEdSetup = NULL;
		}

		// If this is a paste, add the newly created sequence objects to the selection list.
		if(bIsPasting)
		{
			GSelectionTools.Select(NewSeqObj);
		}

		NewSeqObj->PostEditChange(NULL);
	}

	GUglyHackFlags = OldUglyHackFlags;

	return OutSequence;
}

IMPLEMENT_CLASS(USequenceFactory);


/*------------------------------------------------------------------------------
	The end.
------------------------------------------------------------------------------*/
