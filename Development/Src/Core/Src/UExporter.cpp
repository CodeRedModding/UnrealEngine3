/*=============================================================================
	UExporter.cpp: Exporter class implementation.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

// Core includes.
#include "CorePrivate.h"

/*----------------------------------------------------------------------------
	UExporter.
----------------------------------------------------------------------------*/

void UExporter::StaticConstructor()
{
	UArrayProperty* A = new(GetClass(),TEXT("FormatExtension"),RF_Public)UArrayProperty(CPP_PROPERTY(FormatExtension),TEXT(""),0);
	A->Inner = new(A,TEXT("StrProperty0"),RF_Public)UStrProperty;

	UArrayProperty* B = new(GetClass(),TEXT("FormatDescription"),RF_Public)UArrayProperty(CPP_PROPERTY(FormatDescription),TEXT(""),0);
	B->Inner = new(B,TEXT("StrProperty0"),RF_Public)UStrProperty;	
}

UExporter::UExporter()
: FormatExtension( E_NoInit ), FormatDescription( E_NoInit), bSelectedOnly(0)
{}

void UExporter::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << SupportedClass << FormatExtension << FormatDescription;
}
IMPLEMENT_CLASS(UExporter);

/*----------------------------------------------------------------------------
	Object exporting.
----------------------------------------------------------------------------*/

//
// Find an exporter.
//
UExporter* UExporter::FindExporter( UObject* Object, const TCHAR* FileType )
{
	check(Object);

	TMap<UClass*,UClass*> Exporters;

	for( TObjectIterator<UClass> It; It; ++It )
	{
		if( It->IsChildOf(UExporter::StaticClass()) )
		{
			UExporter* Default = (UExporter*)It->GetDefaultObject();
			check( Default->FormatExtension.Num() == Default->FormatDescription.Num() );
			if( Default->SupportedClass && Object->IsA(Default->SupportedClass) )
				for( INT i=0; i<Default->FormatExtension.Num(); i++ )
					if
					(	appStricmp( *Default->FormatExtension(i), FileType  )==0
					||	appStricmp( *Default->FormatExtension(i), TEXT("*") )==0 )
						Exporters.Set( Default->SupportedClass, *It );
					}
				}

	UClass** E;
	for( UClass* TempClass=Object->GetClass(); TempClass; TempClass=(UClass*)TempClass->SuperField )
		if( (E = Exporters.Find( TempClass )) != NULL )
			return ConstructObject<UExporter>( *E );
		
	return NULL;
}

//
// Export an object to an archive.
//
void UExporter::ExportToArchive( UObject* Object, UExporter* InExporter, FArchive& Ar, const TCHAR* FileType )
{
	check(Object);
	UExporter* Exporter = InExporter;
	if( !Exporter )
	{
		Exporter = FindExporter( Object, FileType );
	}
	if( !Exporter )
	{
		warnf( TEXT("No %s exporter found for %s"), FileType, *Object->GetFullName() );
		return;
	}
	check(Object->IsA(Exporter->SupportedClass));
	Exporter->ExportBinary( Object, FileType, Ar, GWarn );
	if( !InExporter )
		delete Exporter;
}

//
// Export an object to an output device.
//
void UExporter::ExportToOutputDevice( UObject* Object, UExporter* InExporter, FOutputDevice& Out, const TCHAR* FileType, INT Indent )
{
	check(Object);
	UExporter* Exporter = InExporter;
	if( !Exporter )
	{
		Exporter = FindExporter( Object, FileType );
	}
	if( !Exporter )
	{
		warnf( TEXT("No %s exporter found for %s"), FileType, *Object->GetFullName() );
		return;
	}
	check(Object->IsA(Exporter->SupportedClass));
	INT SavedIndent = Exporter->TextIndent;
	Exporter->TextIndent = Indent;
	Exporter->ExportText( Object, FileType, Out, GWarn );
	Exporter->TextIndent = SavedIndent;
	if( !InExporter )
		delete Exporter;
}

//
// Export this object to a file.  Child classes do not
// override this, but they do provide an Export() function
// to do the resoource-specific export work.  Returns 1
// if success, 0 if failed.
//
UBOOL UExporter::ExportToFile( UObject* Object, UExporter* InExporter, const TCHAR* Filename, UBOOL InSelectedOnly, UBOOL NoReplaceIdentical, UBOOL Prompt )
{
	check(Object);
	UExporter* Exporter = InExporter;
	const TCHAR* FileType = appFExt(Filename);
	UBOOL Result = 0;
	if( !Exporter )
	{
		Exporter = FindExporter( Object, FileType );
	}
	if( !Exporter )
	{
		warnf( TEXT("No %s exporter found for %s"), FileType, *Object->GetFullName() );
		return 0;
	}

	Exporter->bSelectedOnly = InSelectedOnly;

	if( Exporter->bText )
	{
		FStringOutputDevice Buffer;
		ExportToOutputDevice( Object, Exporter, Buffer, FileType, 0 );
		if( NoReplaceIdentical )
		{
			FString FileBytes;
			if
			(	appLoadFileToString(FileBytes,Filename)
			&&	appStrcmp(*Buffer,*FileBytes)==0 )
			{
				debugf( TEXT("Not replacing %s because identical"), Filename );
				Result = 1;
				goto Done;
			}
			if( Prompt )
			{
				if( !GWarn->YesNof( *LocalizeQuery(TEXT("Overwrite"),TEXT("Core")), Filename ) )
				{
					Result = 1;
					goto Done;
				}
			}
		}
		if( !appSaveStringToFile( Buffer, Filename ) )
		{
#if 0
			if( GWarn->YesNof( *LocalizeQuery(TEXT("OverwriteReadOnly"),TEXT("Core")), Filename ) )
			{
				GFileManager->Delete( Filename, 0, 1 );
				if( appSaveStringToFile( Buffer, Filename ) )
				{
					Result = 1;
					goto Done;
				}
			}
#endif
			warnf( *LocalizeError(TEXT("ExportOpen"),TEXT("Core")), *Object->GetFullName(), Filename );
			goto Done;
		}
		Result = 1;
	}
	else
	{
		FBufferArchive Buffer;
		ExportToArchive( Object, Exporter, Buffer, FileType );
		if( NoReplaceIdentical )
		{
			TArray<BYTE> FileBytes;
			if
			(	appLoadFileToArray(FileBytes,Filename)
			&&	FileBytes.Num()==Buffer.Num()
			&&	appMemcmp(&FileBytes(0),&Buffer(0),Buffer.Num())==0 )
			{
				debugf( TEXT("Not replacing %s because identical"), Filename );
				Result = 1;
				goto Done;
			}
			if( Prompt )
			{
				if( !GWarn->YesNof( *LocalizeQuery(TEXT("Overwrite"),TEXT("Core")), Filename ) )
				{
					Result = 1;
					goto Done;
				}
			}
		}
		if( !appSaveArrayToFile( Buffer, Filename ) )
		{
			warnf( *LocalizeError(TEXT("ExportOpen"),TEXT("Core")), *Object->GetFullName(), Filename );
			goto Done;
		}
		Result = 1;
	}
Done:
	if( !InExporter )
		delete Exporter;
	return Result;
}


/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

