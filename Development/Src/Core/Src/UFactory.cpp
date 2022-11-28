/*=============================================================================
	UFactory.cpp: Factory class implementation.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

// Core includes.
#include "CorePrivate.h"

/*----------------------------------------------------------------------------
	UFactory.
----------------------------------------------------------------------------*/

void UFactory::StaticConstructor()
{
	new(GetClass(),TEXT("Description"),RF_Public)UStrProperty(CPP_PROPERTY(Description),TEXT(""),0);
	UArrayProperty* A = new(GetClass(),TEXT("Formats"),RF_Public)UArrayProperty(CPP_PROPERTY(Formats),TEXT(""),0);
	A->Inner = new(A,TEXT("StrProperty0"),RF_Public)UStrProperty;
}
UFactory::UFactory()
: Formats( E_NoInit )
{
	bEditorImport = 0;
}
void UFactory::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	if( !Ar.IsLoading() && !Ar.IsSaving() )
		Ar << SupportedClass << ContextClass;
}


IMPLEMENT_COMPARE_POINTER( UFactory, UFactory, { return A->AutoPriority - B->AutoPriority; } )

UObject* UFactory::StaticImportObject
(
	ULevel*				InLevel,
	UClass*				Class,
	UObject*			InOuter,
	FName				Name,
	DWORD				Flags,
	const TCHAR*		Filename,
	UObject*			Context,
	UFactory*			InFactory,
	const TCHAR*		Parms,
	FFeedbackContext*	Warn
)
{
	check(Class);

	// Make list of all applicable factories.
	TArray<UFactory*> Factories;
	if( InFactory )
	{
		// Use just the specified factory.
		check(InFactory->SupportedClass->IsChildOf(Class));
		Factories.AddItem( InFactory );
	}
	else
	{
		// Try all automatic factories, sorted by priority.
		for( TObjectIterator<UClass> It; It; ++It )
		{
			if( It->IsChildOf(UFactory::StaticClass()) )
			{
				UFactory* Default = (UFactory*)It->GetDefaultObject();
				if( Class->IsChildOf(Default->SupportedClass) && Default->AutoPriority>=0 )
					Factories.AddItem( ConstructObject<UFactory>(*It) );
			}
		}
		Sort<USE_COMPARE_POINTER(UFactory,UFactory)>( &Factories(0), Factories.Num() );
	}

	// Try each factory in turn.
	for( INT i=0; i<Factories.Num(); i++ )
	{
		UFactory* Factory = Factories(i);
		UObject* Result = NULL;
		if( Factory->bCreateNew )
		{
			if( appStricmp(Filename,TEXT(""))==0 )
			{
				debugf( NAME_Log, TEXT("FactoryCreateNew: %s with %s (%i %i %s)"), Class->GetName(), Factories(i)->GetClass()->GetName(), Factory->bCreateNew, Factory->bText, Filename );
				Factory->ParseParms( Parms );
				Result = Factory->FactoryCreateNew( Class, InOuter, Name, Flags, NULL, Warn );
			}
		}
		else if( appStricmp(Filename,TEXT(""))!=0 )
		{
			if( Factory->bText )
			{
				debugf( NAME_Log, TEXT("FactoryCreateText: %s with %s (%i %i %s)"), Class->GetName(), Factories(i)->GetClass()->GetName(), Factory->bCreateNew, Factory->bText, Filename );
				FString Data;
				if( appLoadFileToString( Data, Filename ) )
				{
					const TCHAR* Ptr = *Data;
					Factory->ParseParms( Parms );
					Result = Factory->FactoryCreateText( InLevel, Class, InOuter, Name, Flags, NULL, appFExt(Filename), Ptr, Ptr+Data.Len(), Warn );
				}
			}
			else
			{
				debugf( NAME_Log, TEXT("FactoryCreateBinary: %s with %s (%i %i %s)"), Class->GetName(), Factories(i)->GetClass()->GetName(), Factory->bCreateNew, Factory->bText, Filename );
				TArray<BYTE> Data;
				if( appLoadFileToArray( Data, Filename ) )
				{
					Data.AddItem( 0 );
					const BYTE* Ptr = &Data( 0 );
					Factory->ParseParms( Parms );
					Result = Factory->FactoryCreateBinary( Class, InOuter, Name, Flags, NULL, appFExt(Filename), Ptr, Ptr+Data.Num()-1, Warn );
				}
			}
		}
		if( Result )
		{
			check(Result->IsA(Class));
			if( !InFactory )
				for( INT i=0; i<Factories.Num(); i++ )
					delete Factories(i);
			return Result;
		}
	}
	if( !InFactory )
		for( INT i=0; i<Factories.Num(); i++ )
			delete Factories(i);
	Warn->Logf( *LocalizeError(TEXT("NoFindImport"),TEXT("Core")), Filename );
	return NULL;
}
IMPLEMENT_CLASS(UFactory);

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

