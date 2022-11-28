/*=============================================================================
	UFactory.h: Factory class definition.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*----------------------------------------------------------------------------
	UFactory.
----------------------------------------------------------------------------*/

class ULevel;

//
// An object responsible for creating and importing new objects.
//
class UFactory : public UObject
{
	DECLARE_ABSTRACT_CLASS(UFactory,UObject,0,Core)

	// Per-class variables.
	UClass*         SupportedClass;
	UClass*			ContextClass;
	FString			Description;
	TArray<FString> Formats;
	BITFIELD        bCreateNew		: 1;
	BITFIELD        bEditorImport	: 1;
	BITFIELD		bText			: 1;
	INT				AutoPriority;

	UBOOL bOKToAll;		// Used by the editor during the import process

	// Constructors.
	UFactory();
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UFactory interface.
	virtual UObject* FactoryCreateText( ULevel* InLevel, UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn ) {return NULL;}
	virtual UObject* FactoryCreateBinary( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, const TCHAR* Type, const BYTE*& Buffer, const BYTE* BufferEnd, FFeedbackContext* Warn ) {return NULL;}
	virtual UObject* FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, FFeedbackContext* Warn ) {return NULL;}

	// UFactory functions.
	static UObject* StaticImportObject( ULevel* InLevel, UClass* Class, UObject* InOuter, FName Name, DWORD Flags, const TCHAR* Filename=TEXT(""), UObject* Context=NULL, UFactory* Factory=NULL, const TCHAR* Parms=NULL, FFeedbackContext* Warn=GWarn );
};

// Import an object using a UFactory.
template< class T > T* ImportObject( ULevel* InLevel, UObject* Outer, FName Name, DWORD Flags, const TCHAR* Filename=TEXT(""), UObject* Context=NULL, UFactory* Factory=NULL, const TCHAR* Parms=NULL, FFeedbackContext* Warn=GWarn )
{
	return (T*)UFactory::StaticImportObject( InLevel, T::StaticClass(), Outer, Name, Flags, Filename, Context, Factory, Parms, Warn );
}

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

