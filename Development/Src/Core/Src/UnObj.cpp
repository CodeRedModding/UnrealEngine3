/*=============================================================================
	UnObj.cpp: Unreal object manager.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

// Object manager internal variables.
UBOOL						UObject::GObjInitialized		= 0;
UBOOL						UObject::GObjNoRegister			= 0;
INT							UObject::GObjBeginLoadCount		= 0;
INT							UObject::GObjRegisterCount		= 0;
INT							UObject::GImportCount			= 0;
UObject*					UObject::GAutoRegister			= NULL;
UPackage*					UObject::GObjTransientPkg		= NULL;
TCHAR						UObject::GObjCachedLanguage[32] = TEXT("");
TCHAR						UObject::GLanguage[64]          = TEXT("int");
UObject*					UObject::GObjHash[4096];
TArray<UObject*>			UObject::GObjLoaded;
TArray<UObject*>			UObject::GObjObjects;
TArray<INT>					UObject::GObjAvailable;
TArray<UObject*>			UObject::GObjLoaders;
TArray<UObject*>			UObject::GObjRoot;
TArray<UObject*>			UObject::GObjRegistrants;
static INT GGarbageRefCount=0;

/*-----------------------------------------------------------------------------
	UObject constructors.
-----------------------------------------------------------------------------*/

UObject::UObject()
{}
UObject::UObject( const UObject& Src )
{
	check(&Src);
	if( Src.GetClass()!=GetClass() )
		appErrorf( TEXT("Attempt to copy-construct %s from %s"), *GetFullName(), *Src.GetFullName() );
}
UObject::UObject( EInPlaceConstructor, UClass* InClass, UObject* InOuter, FName InName, DWORD InFlags )
{
	StaticAllocateObject( InClass, InOuter, InName, InFlags, NULL, GError, this );
}
UObject::UObject( ENativeConstructor, UClass* InClass, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags )
:	Index		( INDEX_NONE				)
,	HashNext	( NULL						)
,	StateFrame	( NULL						)
,	_Linker		( NULL						)
,	ObjectFlags	( InFlags | RF_Native	    )
,	Class		( InClass					)
,	_LinkerIndex( INDEX_NONE				)
,	Outer       ( NULL						)
,	Name        ( NAME_None					)
{
	// Make sure registration is allowed now.
	check(!GObjNoRegister);

	// Setup registration info, for processing now (if inited) or later (if starting up).
	check(sizeof(Outer       )>=sizeof(InPackageName));
	check(sizeof(_LinkerIndex)>=sizeof(GAutoRegister));

#if SERIAL_POINTER_INDEX
	check(sizeof(Name        )>=sizeof(INT          ));
	*(INT*)&Name                    = SerialPointerIndex((void *) InName);
#else
	check(sizeof(Name        )>=sizeof(InName       ));
	*(const TCHAR  **)&Name         = InName;
#endif

	*(const TCHAR  **)&Outer        = InPackageName;
	*(UObject      **)&_LinkerIndex = GAutoRegister;
	GAutoRegister                   = this;

	// Call native registration from terminal constructor.
	if( GetInitialized() && GetClass()==StaticClass() )
		Register();
}
UObject::UObject( EStaticConstructor, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags )
:	Index		( INDEX_NONE				)
,	HashNext	( NULL						)
,	StateFrame	( NULL						)
,	_Linker		( NULL						)
,	ObjectFlags	( InFlags | RF_Native	    )
,	_LinkerIndex( INDEX_NONE				)
,	Outer       ( NULL						)
,	Name        ( NAME_None					)
{
	// Setup registration info, for processing now (if inited) or later (if starting up).
	check(sizeof(Outer       )>=sizeof(InPackageName));
	check(sizeof(_LinkerIndex)>=sizeof(GAutoRegister));

#if SERIAL_POINTER_INDEX
	check(sizeof(Name        )>=sizeof(INT          ));
	*(INT*)&Name                    = SerialPointerIndex((void *) InName);
#else
	check(sizeof(Name        )>=sizeof(InName       ));
	*(const TCHAR  **)&Name         = InName;
#endif

	*(const TCHAR  **)&Outer        = InPackageName;

	
	// If we are not initialized yet, auto register.
	if (!GObjInitialized)
	{
		*(UObject      **)&_LinkerIndex = GAutoRegister;
		GAutoRegister                   = this;
	}
}
UObject& UObject::operator=( const UObject& Src )
{
	check(&Src);
	if( Src.GetClass()!=GetClass() )
		appErrorf( TEXT("Attempt to assign %s from %s"), *GetFullName(), *Src.GetFullName() );
	return *this;
}

/*-----------------------------------------------------------------------------
	UObject class initializer.
-----------------------------------------------------------------------------*/

void UObject::StaticConstructor()
{
}

/*-----------------------------------------------------------------------------
	UObject implementation.
-----------------------------------------------------------------------------*/

//
// Rename this object to a unique name.
//
void UObject::Rename( const TCHAR* InName, UObject* NewOuter )
{
	UObject::ResetLoaders( GetOuter(), 0, 1 );
	FName NewName = InName ? FName(InName) : MakeUniqueObjectName( NewOuter ? NewOuter : GetOuter(), GetClass() );
	UnhashObject( Outer ? Outer->GetIndex() : 0 );
	debugfSlow( TEXT("Renaming %s to %s"), *Name, *NewName );
	Name = NewName;
	if( NewOuter )
		Outer = NewOuter;
	HashObject();

	PostRename();
}

//
// Shutdown after a critical error.
//
void UObject::ShutdownAfterError()
{
}

//
// Make sure the object is valid.
//
UBOOL UObject::IsValid()
{
	if( !this )
	{
		debugf( NAME_Warning, TEXT("NULL object") );
		return 0;
	}
	else if( !GObjObjects.IsValidIndex(GetIndex()) )
	{
		debugf( NAME_Warning, TEXT("Invalid object index %i"), GetIndex() );
		debugf( NAME_Warning, TEXT("This is: %s"), *GetFullName() );
		return 0;
	}
	else if( GObjObjects(GetIndex())==NULL )
	{
		debugf( NAME_Warning, TEXT("Empty slot") );
		debugf( NAME_Warning, TEXT("This is: %s"), *GetFullName() );
		return 0;
	}
	else if( GObjObjects(GetIndex())!=this )
	{
		debugf( NAME_Warning, TEXT("Other object in slot") );
		debugf( NAME_Warning, TEXT("This is: %s"), *GetFullName() );
		debugf( NAME_Warning, TEXT("Other is: %s"), *GObjObjects(GetIndex())->GetFullName() );
		return 0;
	}
	else return 1;
}

//
// Do any object-specific cleanup required
// immediately after loading an object, and immediately
// after any undo/redo.
//
void UObject::PostLoad()
{
	// Note that it has propagated.
	SetFlags( RF_DebugPostLoad );

	// Load per-object localization
	if( ObjectFlags&RF_PerObjectLocalized)
		LoadLocalized();
}

//
// Edit change notification.
//
void UObject::PreEditChange()
{
	Modify();
}

/**
 * Called when a property on this object has been modified externally
 *
 * @param PropertyThatChanged the property that was modified
 */
void UObject::PostEditChange(UProperty* PropertyThatChanged)
{
	//@todo Assert in game on consoles
	if (GIsEditor == 0 && GIsUCC == 0)
	{
		debugf( TEXT("DO NOT CALL PostEditChange() in game as it will be removed on consoles (%s)"), GetName() );
	}
}

//
// Do any object-specific cleanup required immediately
// before an object is killed.  Child classes may override
// this if they have to do anything here.
//
void UObject::Destroy()
{
	SetFlags( RF_DebugDestroy );

	// Destroy properties.
	ExitProperties( (BYTE*)this, GetClass() );

	// Log message.
	if( GObjInitialized && !GIsCriticalError )
		debugfSlow( NAME_DevKill, TEXT("Destroying %s"), *GetFullName() );

	// Unhook from linker.
	SetLinker( NULL, INDEX_NONE );

	// Remember outer name index.
	_LinkerIndex = Outer ? Outer->GetIndex() : 0;
}

//
// Set the object's linker.
//
void UObject::SetLinker( ULinkerLoad* InLinker, INT InLinkerIndex )
{
	// Detach from existing linker.
	if( _Linker )
	{
		check(_Linker->ExportMap(_LinkerIndex)._Object!=NULL);
		check(_Linker->ExportMap(_LinkerIndex)._Object==this);
		_Linker->ExportMap(_LinkerIndex)._Object = NULL;
	}

	// Set new linker.
	_Linker      = InLinker;
	_LinkerIndex = InLinkerIndex;
}

//
// Return the object's path name.
//warning: Must be safe for NULL objects.
//warning: Must be safe for class-default meta-objects.
//
FString UObject::GetPathName( UObject* StopOuter ) const
{
	FString Result;
	if( this!=StopOuter )
	{
		if( Outer!=StopOuter )
			Result = Result + Outer->GetPathName( StopOuter ) + TEXT(".");
		Result = Result + GetName();
	}
	else
		Result = TEXT("None");

	return Result;
}

//
// Return the object's full name.
//warning: Must be safe for NULL objects.
//
FString UObject::GetFullName( ) const
{
	if( this )
		return FString(GetClass()->GetName()) + TEXT(" ") + GetPathName( NULL );
	else
		return TEXT("None");
}

//
// Destroy the object if necessary.
//
UBOOL UObject::ConditionalDestroy()
{
	if( Index!=INDEX_NONE && !(GetFlags() & RF_Destroyed) )
	{
		SetFlags( RF_Destroyed );
		ClearFlags( RF_DebugDestroy );
		Destroy();
		if( !(GetFlags()&RF_DebugDestroy) )
			appErrorf( TEXT("%s failed to route Destroy"), *GetFullName() );
		return 1;
	}
	else return 0;
}

//
// Register if needed.
//
void UObject::ConditionalRegister()
{
	if( GetIndex()==INDEX_NONE )
	{
#if 0
		// Verify this object is on the list to register.
		INT i;
		for( i=0; i<GObjRegistrants.Num(); i++ )
			if( GObjRegistrants(i)==this )
				break;
		check(i!=GObjRegistrants.Num());
#endif

		// Register it.
		Register();
	}
}

//
// PostLoad if needed.
//
void UObject::ConditionalPostLoad()
{
	if( GetFlags() & RF_NeedPostLoad )
	{
		ClearFlags( RF_NeedPostLoad | RF_DebugPostLoad );
		PostLoad();
		if( !(GetFlags() & RF_DebugPostLoad) )
 			appErrorf( TEXT("%s failed to route PostLoad"), *GetFullName() );
	}
}

//
// UObject destructor.
//warning: Called at shutdown.
//
UObject::~UObject()
{
	// If not initialized, skip out.
	if( Index!=INDEX_NONE && GObjInitialized && !GIsCriticalError )
	{
		// Validate it.
		check(IsValid());

		// Destroy the object if necessary.
		ConditionalDestroy();

		// Remove object from table.
		UnhashObject( _LinkerIndex );
		GObjObjects(Index) = NULL;
		GObjAvailable.AddItem( Index );
	}

	// Free execution stack.
	if( StateFrame )
		delete StateFrame;
}

//
// Note that the object has been modified.
//
void UObject::Modify()
{
	// Perform transaction tracking.
	if( GUndo && (GetFlags() & RF_Transactional) )
		GUndo->SaveObject( this );
}

//
// UObject serializer.
//
void UObject::Serialize( FArchive& Ar )
{
	SetFlags( RF_DebugSerialize );

	// Make sure this object's class's data is loaded.
	if( Class != UClass::StaticClass() )
		Ar.Preload( Class );

	// Special info.
	if( (!Ar.IsLoading() && !Ar.IsSaving()) )
		Ar << Name << Outer << Class;
	if( !Ar.IsLoading() && !Ar.IsSaving() )
		Ar << _Linker;

	// Execution stack.
	//!!how does the stack work in conjunction with transaction tracking?
	if( !Ar.IsTrans() )
	{
		if( GetFlags() & RF_HasStack )
		{
			if( !StateFrame )
				StateFrame = new FStateFrame( this );
			Ar << StateFrame->Node << StateFrame->StateNode;
			Ar << StateFrame->ProbeMask;
			Ar << StateFrame->LatentAction;
			if( StateFrame->Node )
			{
				Ar.Preload( StateFrame->Node );
				if( Ar.IsSaving() && StateFrame->Code )
					check(StateFrame->Code>=&StateFrame->Node->Script(0) && StateFrame->Code<&StateFrame->Node->Script(StateFrame->Node->Script.Num()));
				INT Offset = StateFrame->Code ? StateFrame->Code - &StateFrame->Node->Script(0) : INDEX_NONE;
				Ar << Offset;
				if( Offset!=INDEX_NONE )
					if( Offset<0 || Offset>=StateFrame->Node->Script.Num() )
						appErrorf( TEXT("%s: Offset mismatch: %i %i"), *GetFullName(), Offset, StateFrame->Node->Script.Num() );
				StateFrame->Code = Offset!=INDEX_NONE ? &StateFrame->Node->Script(Offset) : NULL;
			}
			else StateFrame->Code = NULL;
		}
		else if( StateFrame )
		{
			delete StateFrame;
			StateFrame = NULL;
		}
	}

	// Serialize object properties which are defined in the class.
	if( Class != UClass::StaticClass() )
	{
		if( (Ar.IsLoading() || Ar.IsSaving()) && !Ar.IsTrans() )
			GetClass()->SerializeTaggedProperties( Ar, (BYTE*)this, Class );
		else
			GetClass()->SerializeBin( Ar, (BYTE*)this, 0 );
	}

	// Memory counting.
	SIZE_T Size = GetClass()->GetPropertiesSize();
	Ar.CountBytes( Size, Size );
}

//
// Export text object properties.
//
void UObject::ExportProperties
(
	FOutputDevice&	Out,
	UClass*			ObjectClass,
	BYTE*			Object,
	INT				Indent,
	UClass*			DiffClass,
	BYTE*			Diff,
	UObject*		Parent
)
{
	const TCHAR* ThisName = TEXT("(none)");
	check(ObjectClass!=NULL);
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(ObjectClass); It; ++It )
	{
		if( It->Port() )
		{
			ThisName = It->GetName();
			for( INT j=0; j<It->ArrayDim; j++ )
			{
				if( It->IsA(UArrayProperty::StaticClass()) ) 
				{
					// Export dynamic array.
					UProperty* InnerProp = Cast<UArrayProperty>(*It)->Inner;
					UBOOL ExportObject = (It->PropertyFlags & CPF_ExportObject) && InnerProp->IsA(UObjectProperty::StaticClass());

					FArray* Arr = (FArray*)((BYTE*)Object + It->Offset + j*It->ElementSize);
					FArray*	DiffArr = NULL;
					if( DiffClass && DiffClass->IsChildOf(It.GetStruct()) )
					{
						DiffArr = (FArray*)(Diff + It->Offset + j*It->ElementSize);
					}
					for( INT k=0;k<Arr->Num();k++ )
					{
						FString	Value;
						if( InnerProp->ExportText( k, Value, (BYTE*)Arr->GetData(), DiffArr && k < DiffArr->Num() ? (BYTE*)DiffArr->GetData() : NULL, Parent, PPF_Delimited ) )
						{					
							if( ExportObject )
							{
								UObject* Obj = ((UObject**)Arr->GetData())[k];
								if( Obj && !(Obj->GetFlags() & RF_TagImp) )
								{
									// Don't export more than once.
									UExporter::ExportToOutputDevice( Obj, NULL, Out, TEXT("T3D"), Indent+1 );
									Obj->SetFlags( RF_TagImp );
								}
							}
		
							Out.Logf( TEXT("%s %s(%i)=%s\r\n"), appSpc(Indent), It->GetName(), k, *Value );
						}
					}
				}
				else
				{
					FString	Value;
					// Export single element.
					if( It->ExportText( j, Value, Object, (DiffClass && DiffClass->IsChildOf(It.GetStruct())) ? Diff : NULL, Parent, PPF_Delimited ) )
					{
						if
						(	(It->IsA(UObjectProperty::StaticClass()))
						&&	(It->PropertyFlags & CPF_ExportObject) )
						{
							UObject* Obj = *(UObject **)((BYTE*)Object + It->Offset + j*It->ElementSize);					
							if( Obj && !(Obj->GetFlags() & RF_TagImp) )
							{
								// Don't export more than once.
								UExporter::ExportToOutputDevice( Obj, NULL, Out, TEXT("T3D"), Indent+1 );
								Obj->SetFlags( RF_TagImp );
							}
						}						

						if( It->ArrayDim == 1 )
							Out.Logf( TEXT("%s %s=%s\r\n"), appSpc(Indent), It->GetName(), *Value );
						else
							Out.Logf( TEXT("%s %s(%i)=%s\r\n"), appSpc(Indent), It->GetName(), j, *Value );
					}
				}
			}
		}
	}
}

//
// Initialize script execution.
//
void UObject::InitExecution()
{
	check(GetClass()!=NULL);

	if( StateFrame )
		delete StateFrame;
	StateFrame = new FStateFrame( this );
	SetFlags( RF_HasStack );
}

//
// Command line.
//
UBOOL UObject::ScriptConsoleExec( const TCHAR* Str, FOutputDevice& Ar, UObject* Executor )
{
	// Find UnrealScript exec function.
	TCHAR MsgStr[NAME_SIZE];
	FName Message;
	UFunction* Function;
	if
	(	!ParseToken(Str,MsgStr,ARRAY_COUNT(MsgStr),1)
	||	(Message=FName(MsgStr,FNAME_Find))==NAME_None 
	||	(Function=FindFunction(Message))==NULL 
	||	!(Function->FunctionFlags & FUNC_Exec) )
		return 0;

	// Parse all function parameters.
	BYTE* Parms = (BYTE*)appAlloca(Function->ParmsSize);
	appMemzero( Parms, Function->ParmsSize );
	UBOOL Failed = 0;
	INT Count = 0;
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It,Count++ )
	{
		if( Count==0 && Executor )
		{
			UObjectProperty* Op = Cast<UObjectProperty>(*It);
			if( Op && Executor->IsA(Op->PropertyClass) )
			{
				// First parameter is implicit reference to object executing the command.
				*(UObject**)(Parms + It->Offset) = Executor;
				continue;
			}
		}
		ParseNext( &Str );
		Str = It->ImportText( Str, Parms+It->Offset, PPF_Localized, NULL );
		if( !Str )
		{
			if( !(It->PropertyFlags & CPF_OptionalParm) )
			{
				Ar.Logf( *LocalizeError(TEXT("BadProperty"),TEXT("Core")), *Message, It->GetName() );
				Failed = 1;
			}
			break;
		}
	}
	if( !Failed )
		ProcessEvent( Function, Parms );
	//!!destructframe see also UObject::ProcessEvent
	{for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
		It->DestroyValue( Parms + It->Offset );}

	// Success.
	return 1;
}

//
// Find an UnrealScript field.
//warning: Must be safe with class default metaobjects.
//
UField* UObject::FindObjectField( FName InName, UBOOL Global )
{
	// Search current state scope.
	if( StateFrame && StateFrame->StateNode && !Global )
		for( TFieldIterator<UField> It(StateFrame->StateNode); It; ++It )
			if( It->GetFName()==InName )
				return *It;

	// Search the global scope.
	for( TFieldIterator<UField> It(GetClass()); It; ++It )
		if( It->GetFName()==InName )
			return *It;

	// Not found.
	return NULL;
}
UFunction* UObject::FindFunction( FName InName, UBOOL Global )
{
	UFunction *func = NULL;
	if (StateFrame != NULL &&
		StateFrame->StateNode != NULL &&
		!Global)
	{
		// search current/parent states
		UState *searchState = StateFrame->StateNode;
		while (searchState != NULL &&
			   func == NULL)
		{
			func = searchState->FuncMap.FindRef(InName);
			searchState = searchState->GetSuperState();
		}
	}
	if (func == NULL)
	{
		// and search the global state
		UClass *searchClass = GetClass();
		while (searchClass != NULL &&
			   func == NULL)
		{
			func = searchClass->FuncMap.FindRef(InName);
			searchClass = searchClass->GetSuperClass();
		}
	}
    return func;
}
UFunction* UObject::FindFunctionChecked( FName InName, UBOOL Global )
{
    UFunction* Result = FindFunction(InName,Global);
	if( !Result )
	{
		appErrorf( TEXT("Failed to find function %s in %s"), *InName, *GetFullName() );
	}
	return Result;
}
UState* UObject::FindState( FName InName )
{
    return Cast<UState>( FindObjectField( InName, 1 ),CLASS_IsAUState );
}
IMPLEMENT_CLASS(UObject);

/*-----------------------------------------------------------------------------
	UObject configuration.
-----------------------------------------------------------------------------*/

//
// Load configuration.
//warning: Must be safe on class-default metaobjects.
//
void UObject::LoadConfig( UBOOL Propagate, UClass* Class, const TCHAR* InFilename )
{
	if( !Class )
		Class = GetClass();
	if( !(Class->ClassFlags & CLASS_Config) )
		return;

	if( Propagate && Class->GetSuperClass() )
		LoadConfig( Propagate, Class->GetSuperClass(), InFilename );
	UBOOL PerObject = ((GetClass()->ClassFlags & CLASS_PerObjectConfig) && GetIndex()!=INDEX_NONE);
	FString Filename
	=	InFilename
	?	InFilename
	:	(PerObject && Outer!=GObjTransientPkg)
	?	GetOuter()->GetName()
	:	GetClass()->GetConfigName();
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Class); It; ++It )
	{
		if( It->PropertyFlags & CPF_Config )
		{
			TCHAR TempKey[256];
			UClass*         BaseClass = (It->PropertyFlags & CPF_GlobalConfig) ? It->GetOwnerClass() : Class;
			FString			Section   = PerObject ? GetName() : BaseClass->GetPathName();
			const TCHAR*    Key       = It->ArrayDim==1 ? It->GetName() : TempKey;
			UArrayProperty* Array     = Cast<UArrayProperty>( *It );
			UMapProperty*   Map       = Cast<UMapProperty>( *It );
			if( Array )
			{
				TMultiMap<FString,FString>* Sec = GConfig->GetSectionPrivate( *Section, 0, 1, *Filename );
				if( Sec )
				{
					TArray<FString> List;
					Sec->MultiFind( FString(Key), List );
					// Only override default properties if there is something to override them with.
					if( List.Num() )
					{
						FArray* Ptr  = (FArray*)((BYTE*)this + It->Offset);
						INT     Size = Array->Inner->ElementSize;
						Array->DestroyValue( Ptr );
						Ptr->AddZeroed( Size, List.Num() );
						for( INT i=List.Num()-1,c=0; i>=0; i--,c++ )
							Array->Inner->ImportText( *List(i), (BYTE*)Ptr->GetData() + c*Size, 0, this );
					}
				}
			}
			else if( Map )
			{
				TMultiMap<FString,FString>* Sec = GConfig->GetSectionPrivate( *Section, 0, 1, *Filename );
				if( Sec )
				{
					TArray<FString> List;
					Sec->MultiFind( Key, List );
					//FArray* Ptr  = (FArray*)((BYTE*)this + It->Offset);
					//INT     Size = Array->Inner->ElementSize;
					//Map->DestroyValue( Ptr );//!!won't do dction
					//Ptr->AddZeroed( Size, List.Num() );
					//for( INT i=List.Num()-1,c=0; i>=0; i--,c++ )
					//	Array->Inner->ImportText( *List(i), (BYTE*)Ptr->GetData() + c*Size, 0 );
				}
			}
			else for( INT i=0; i<It->ArrayDim; i++ )
			{
				if( It->ArrayDim!=1 )
					appSprintf( TempKey, TEXT("%s[%i]"), It->GetName(), i );
				FString Value;
				if( GConfig->GetString( *Section, Key, Value, *Filename ) )
					It->ImportText( *Value, (BYTE*)this + It->Offset + i*It->ElementSize, 0, this );
			}
		}
	}
}

static void LoadLocalizedProp( UProperty* Prop, const TCHAR *IntName, const TCHAR *SectionName, const TCHAR *KeyPrefix, UObject* Parent, BYTE* Data );
static void LoadLocalizedStruct( UStruct* Struct, const TCHAR *IntName, const TCHAR *SectionName, const TCHAR *KeyPrefix, UObject* Parent, BYTE* Data );

static void LoadLocalizedProp( UProperty* Prop, const TCHAR *IntName, const TCHAR *SectionName, const TCHAR *KeyPrefix, UObject* Parent, BYTE* Data )
{
	UStructProperty* StructProperty = Cast<UStructProperty>( Prop );
	
	if( StructProperty )
	{
        LoadLocalizedStruct(StructProperty->Struct, IntName, SectionName, KeyPrefix, Parent, Data );
        return;
    }

    if( !(Prop->PropertyFlags & CPF_Localized) ) 
        return;

	FString LocalizedText = Localize( SectionName, KeyPrefix, IntName, NULL, 1 );

	if( **LocalizedText )
		Prop->ImportText( *LocalizedText, Data, 0, Parent );
}

static void LoadLocalizedStruct( UStruct* Struct, const TCHAR *IntName, const TCHAR *SectionName, const TCHAR *KeyPrefix, UObject* Parent, BYTE* Data )
{
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It( Struct ); It; ++It )
	{
	    UProperty* Prop = *It;

	    for( INT i = 0; i < Prop->ArrayDim; i++ )
	    {
    	    FString NewPrefix;

            if( KeyPrefix )
                NewPrefix = FString::Printf( TEXT("%s."), KeyPrefix );

	        if( Prop->ArrayDim > 1 )
                NewPrefix += FString::Printf( TEXT("%s[%d]"), Prop->GetName(), i );
	        else
                NewPrefix += Prop->GetName();

            BYTE* NewData = Data + (Prop->Offset) + (i * Prop->ElementSize );

            LoadLocalizedProp( Prop, IntName, SectionName, *NewPrefix, Parent, NewData );
	    }
	}
}

//
// Load localized text.
// Warning: Must be safe on class-default metaobjects.
//

void UObject::LoadLocalized()
{
	UClass* Class = GetClass();
	checkSlow( Class );
	
	if( !(Class->ClassFlags & CLASS_Localized) )
		return;
	if( GIsEditor )
		return;

	if( GetIndex()!=INDEX_NONE && ObjectFlags&RF_PerObjectLocalized && GetOuter()->GetOuter() )
	{
		// Localize as an instanced subobject
	    LoadLocalizedStruct( Class, GetOuter()->GetOuter()->GetName(), GetOuter()->GetName(), GetName(), this, (BYTE*)this );
	}
	else
		{
		const TCHAR* IntName     = GetIndex()==INDEX_NONE ? Class->GetOuter()->GetName() : GetOuter()->GetName();
		const TCHAR* SectionName = GetIndex()==INDEX_NONE ? Class->GetName()             : GetName();
	    LoadLocalizedStruct( Class, IntName, SectionName, NULL, this, (BYTE*)this );
	}
}

//
// Save configuration.
//warning: Must be safe on class-default metaobjects.
//!!may benefit from hierarchical propagation, deleting keys that match superclass...not sure what's best yet.
//
void UObject::SaveConfig( QWORD Flags, const TCHAR* InFilename )
{
	if( !(GetClass()->ClassFlags & CLASS_Config) )
		return;

	UBOOL PerObject = ((GetClass()->ClassFlags & CLASS_PerObjectConfig) && GetIndex()!=INDEX_NONE);
	FString Filename
	=	InFilename
	?	InFilename
	:	(PerObject && Outer!=GObjTransientPkg)
	?	GetOuter()->GetName()
	:	GetClass()->GetConfigName();
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(GetClass()); It; ++It )
	{
		if( (It->PropertyFlags & Flags)==Flags )
		{
			TCHAR TempKey[256];
			FString	Key     = It->GetName();
			FString	Section = PerObject ? GetName() : (It->PropertyFlags & CPF_GlobalConfig) ? It->GetOwnerClass()->GetPathName() : GetClass()->GetPathName();
			UArrayProperty* Array   = Cast<UArrayProperty>( *It );
			UMapProperty*   Map     = Cast<UMapProperty>( *It );
			if( Array )
			{
				TMultiMap<FString,FString>* Sec = GConfig->GetSectionPrivate( *Section, 1, 0, *Filename );
				check(Sec);
				Sec->Remove( *Key );
				FArray* Ptr  = (FArray*)((BYTE*)this + It->Offset);
				INT     Size = Array->Inner->ElementSize;
				for( INT i=0; i<Ptr->Num(); i++ )
				{
					FString	Buffer;
					BYTE*	Dest = (BYTE*)Ptr->GetData() + i*Size;
					Array->Inner->ExportTextItem( Buffer, Dest, Dest, this, 0 );
					Sec->Add( *Key, *Buffer );
				}
			}
			else if( Map )
			{
				TMultiMap<FString,FString>* Sec = GConfig->GetSectionPrivate( *Section, 1, 0, *Filename );
				check(Sec);
				Sec->Remove( *Key );
				//FArray* Ptr  = (FArray*)((BYTE*)this + It->Offset);
				//INT     Size = Array->Inner->ElementSize;
				//for( INT i=0; i<Ptr->Num(); i++ )
				//{
				//	TCHAR Buffer[1024]="";
				//	BYTE* Dest = (BYTE*)Ptr->GetData() + i*Size;
				//	Array->Inner->ExportTextItem( Buffer, Dest, Dest, this, 0 );
				//	Sec->Add( Key, Buffer );
				//}
			}
			else
			{
				for( INT Index=0; Index<It->ArrayDim; Index++ )
				{
					FString	Value;
					if( It->ArrayDim!=1 )
					{
						appSprintf( TempKey, TEXT("%s[%i]"), It->GetName(), Index );
						Key = TempKey;
					}
					It->ExportText( Index, Value, (BYTE*)this, (BYTE*)this, this, 0 );
					GConfig->SetString( *Section, *Key, *Value, *Filename );
				}
			}
		}
	}
	GConfig->Flush( 0 );
	UClass* BaseClass;
	for( BaseClass=GetClass(); BaseClass->GetSuperClass(); BaseClass=BaseClass->GetSuperClass() )
		if( !(BaseClass->GetSuperClass()->ClassFlags & CLASS_Config) )
			break;
	if( BaseClass )
		for( TObjectIterator<UClass> It; It; ++It )
			if( It->IsChildOf(BaseClass) )
				It->GetDefaultObject()->LoadConfig( 1 );//!!propagate??
}

/*-----------------------------------------------------------------------------
	Mo Functions.
-----------------------------------------------------------------------------*/

//
// Object accessor.
//
UObject* UObject::GetIndexedObject( INT Index )
{
	if( Index>=0 && Index<GObjObjects.Num() )
		return GObjObjects(Index);
	else
		return NULL;
}

//
// Find an optional object.
//
UObject* UObject::StaticFindObject( UClass* ObjectClass, UObject* InObjectPackage, const TCHAR* OrigInName, UBOOL ExactClass )
{
	// Resolve the object and package name.
	UObject* ObjectPackage = InObjectPackage!=ANY_PACKAGE ? InObjectPackage : NULL;
	FString InName = OrigInName;
	if( !ResolveName( ObjectPackage, InName, 0, 0 ) )
		return NULL;

	// Make sure it's an existing name.
	FName ObjectName(*InName,FNAME_Find);
	if( ObjectName==NAME_None )
		return NULL;

	// Find in the specified package.
	INT iHash = GetObjectHash( ObjectName, ObjectPackage ? ObjectPackage->GetIndex() : 0 );
	for( UObject* Hash=GObjHash[iHash]; Hash!=NULL; Hash=Hash->HashNext )
	{
		/*
		InName: the object name to search for. Two possibilities.
			A = No dots. ie: 'S_Actor', a texture in Engine
			B = Dots. ie: 'Package.Name' or 'Package.Group.Name', or an even longer chain of outers. The first one needs to be relative to InObjectPackage.
			I'll define InName's package to be everything before the last period, or "" otherwise.
		InObjectPackage: the package or Outer to look for the object in. Can be ANY_PACKAGE, or NULL. Three possibilities:
			A = Non-null. Search for the object relative to this package.
			B = Null. We're looking for the object, as specified exactly in InName.
			C = ANY_PACKAGE. Search anywhere for the package (resrictions on functionality, see below)
		ObjectPackage: The package we need to be searching in. NULL means we don't care what package to search in
			InName.A &&  InObjectPackage.C ==> ObjectPackage = NULL
			InName.A && !InObjectPackage.C ==> ObjectPackage = InObjectPackage
			InName.B &&  InObjectPackage.C ==> ObjectPackage = InName's package
			InName.B && !InObjectPackage.C ==> ObjectPackage = InName's package, but as a subpackage of InObjectPackage
		*/
		if
		(	(Hash->GetFName()==ObjectName)
			/*If there is no package (no InObjectPackage specified, and InName's package is "") 
			and the caller specified any_package, then accept it, regardless of its package.*/ 
		&&	(	(!ObjectPackage && InObjectPackage==ANY_PACKAGE)
			/*Or, if the object is directly within this package, (whether that package is non-NULL or not), 
			then accept it immediately.*/ 
			||	(Hash->Outer == ObjectPackage)
			/*Or finally, if the caller specified any_package, but they specified InName's package, 
			then check if the object IsIn (is recursively within) InName's package. 
			We don't check if IsIn if ObjectPackage is NULL, since that would be true for every object. 
			This check also allows you to specify Package.Name to access an object in Package.Group.Name.*/ 
			||	(InObjectPackage == ANY_PACKAGE && ObjectPackage && Hash->IsIn(ObjectPackage)) )
		&&	(ObjectClass==NULL || (ExactClass ? Hash->GetClass()==ObjectClass : Hash->IsA(ObjectClass))) )
			return Hash;
	}

	// Not found.
	return NULL;
}

//
// Find an object; can't fail.
//
UObject* UObject::StaticFindObjectChecked( UClass* ObjectClass, UObject* ObjectParent, const TCHAR* InName, UBOOL ExactClass )
{
	UObject* Result = StaticFindObject( ObjectClass, ObjectParent, InName, ExactClass );
	if( !Result )
		appErrorf( *LocalizeError(TEXT("ObjectNotFound"),TEXT("Core")), ObjectClass->GetName(), ObjectParent==ANY_PACKAGE ? TEXT("Any") : ObjectParent ? ObjectParent->GetName() : TEXT("None"), InName );
	return Result;
}

//
// Binary initialize object properties to zero or defaults.
//
void UObject::InitProperties( BYTE* Data, INT DataCount, UClass* DefaultsClass, BYTE* Defaults, INT DefaultsCount, UObject* DestObject, UObject* SuperObject )
{
	check(DataCount>=sizeof(UObject));
	INT Inited = sizeof(UObject);

	// Find class defaults if no template was specified.
	//warning: At startup, DefaultsClass->Defaults.Num() will be zero for some native classes.
	if( !Defaults && DefaultsClass && DefaultsClass->Defaults.Num() )
	{
		Defaults      = &DefaultsClass->Defaults(0);
		DefaultsCount =  DefaultsClass->Defaults.Num();
	}

	// Copy defaults.
	if( Defaults )
	{
		checkSlow(DefaultsCount>=Inited);
		checkSlow(DefaultsCount<=DataCount);
		appMemcpy( Data+Inited, Defaults+Inited, DefaultsCount-Inited );
		Inited = DefaultsCount;
	}
	
	// Zero-fill any remaining portion.
	if( Inited < DataCount )
		appMemzero( Data+Inited, DataCount-Inited );
	
	// This is a duplicate. Set all transients to their default value.	
	if( SuperObject )
	{
		checkSlow(DestObject);
		BYTE* ClassDefaults = &DefaultsClass->Defaults(0);
		for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(DestObject->GetClass()); It; ++It )
		{
			if( It->PropertyFlags & CPF_Transient )
			{
				// Bools are packed bitfields which might contain both transient and non- transient members so we can't use memcpy.
				if( Cast<UBoolProperty>(*It,CLASS_IsAUBoolProperty) )
				{			
					It->CopyCompleteValue(Data + It->Offset, ClassDefaults + It->Offset, NULL);
				}
				else
				{
					appMemcpy( Data + It->Offset, ClassDefaults + It->Offset, It->ArrayDim*It->ElementSize );
				}
			}
		}
	}

	// Construct anything required.
	if( DefaultsClass )
	{
		for( UProperty* P=DefaultsClass->ConstructorLink; P; P=P->ConstructorLinkNext )
		{
			if( P->Offset < DefaultsCount )
			{
				appMemzero( Data + P->Offset, P->GetSize() );//bad for bools, but not a real problem because they aren't constructed!!
				P->CopyCompleteValue( Data + P->Offset, Defaults + P->Offset, SuperObject ? SuperObject : DestObject );
			}
		}
	}
}

//
// Destroy properties.
//
void UObject::ExitProperties( BYTE* Data, UClass* Class )
{
	UProperty* P=NULL;
	for( P=Class->ConstructorLink; P; P=P->ConstructorLinkNext )
	{
		// Only destroy values of properties which have been loaded completely.
		// This can be encountered by loading a package in ucc make which references a class which has already been compiled and saved.
		// The class is already in memory, so when the class is loaded from the package on disk, it reuses the memory address of the existing class with the same name.
		// Before reusing the memory address, it destructs the default properties of the existing class.
		// However, at this point, the properties may have been reallocated but not deserialized, leaving them in an invalid state.
		if(!(P->GetFlags() & RF_NeedLoad))
		{
			P->DestroyValue( Data + P->Offset );
		}
		else
		{
			check(GIsUCC);
		}
	}
}

//
// Init class default object.
//
void UObject::InitClassDefaultObject( UClass* InClass, UBOOL SetOuter )
{
	// Init UObject portion.
	appMemset( this, 0, sizeof(UObject) );
	*(void**)this = *(void**)InClass;
	Class         = InClass;
	Index         = INDEX_NONE;

	// Init post-UObject portion.
	if( SetOuter )
		Outer = InClass->GetOuter();
	InitProperties( (BYTE*)this, InClass->GetPropertiesSize(), InClass->GetSuperClass(), NULL, 0, SetOuter ? this : NULL );
}

//
// Global property setting.
//
void UObject::GlobalSetProperty( const TCHAR* Value, UClass* Class, UProperty* Property, INT Offset, UBOOL Immediate )
{
	// Apply to existing objects of the class, with notification.
	if( Immediate )
	{
		for( FObjectIterator It; It; ++It )
		{
			if( It->IsA(Class) )
			{
				It->PreEditChange();
				Property->ImportText( Value, (BYTE*)*It + Offset, PPF_Localized, *It );
				It->PostEditChange(Property);
			}
		}
	}

	// Apply to defaults.
	Property->ImportText( Value, &Class->Defaults(Offset), PPF_Localized, Class->GetDefaultObject() );
	Class->GetDefaultObject()->SaveConfig();
}

/*-----------------------------------------------------------------------------
	Object registration.
-----------------------------------------------------------------------------*/

//
// Preregister an object.
//warning: Sometimes called at startup time.
//
void UObject::Register()
{
	check(GObjInitialized);

	// Get stashed registration info.
	const TCHAR* InOuter = *(const TCHAR**)&Outer;

#if SERIAL_POINTER_INDEX
	INT idx = *(INT*)&Name;;
	check(idx < GTotalSerializedPointers);
	const TCHAR* InName  = (const TCHAR*) GSerializedPointers[idx];
#else
	const TCHAR* InName  = *(const TCHAR**)&Name;
#endif

	// Set object properties.
	Outer        = CreatePackage(NULL,InOuter);
	Name         = InName;
	_LinkerIndex = INDEX_NONE;

	// Validate the object.
	if( Outer==NULL )
		appErrorf( TEXT("Autoregistered object %s is unpackaged"), *GetFullName() );
	if( GetFName()==NAME_None )
		appErrorf( TEXT("Autoregistered object %s has invalid name"), *GetFullName() );
	if( StaticFindObject( NULL, GetOuter(), GetName() ) )
		appErrorf( TEXT("Autoregistered object %s already exists"), *GetFullName() );

	// Add to the global object table.
	AddObject( INDEX_NONE );
}

//
// Handle language change.
//
void UObject::LanguageChange()
{
	LoadLocalized();
}

/*-----------------------------------------------------------------------------
	StaticInit & StaticExit.
-----------------------------------------------------------------------------*/

void SerTest( FArchive& Ar, DWORD& Value, DWORD Max )
{
	DWORD NewValue=0;
	for( DWORD Mask=1; NewValue+Mask<Max && Mask; Mask*=2 )
	{
		BYTE B = ((Value&Mask)!=0);
		Ar.SerializeBits( &B, 1 );
		if( B )
			NewValue |= Mask;
	}
	Value = NewValue;
}

//
// Init the object manager and allocate tables.
//
void UObject::StaticInit()
{
	GObjNoRegister = 1;

	// Checks.
	check(sizeof(BYTE)==1);
	check(sizeof(SBYTE)==1);
	check(sizeof(_WORD)==2);
	check(sizeof(DWORD)==4);
	check(sizeof(QWORD)==8);
	check(sizeof(ANSICHAR)==1);
	check(sizeof(UNICHAR)==2);
	check(sizeof(SWORD)==2);
	check(sizeof(INT)==4);
	check(sizeof(SQWORD)==8);
	check(sizeof(UBOOL)==4);
	check(sizeof(FLOAT)==4);
	check(sizeof(DOUBLE)==8);
	check(GEngineMinNetVersion<=GEngineVersion);

	// Init hash.
	for( INT i=0; i<ARRAY_COUNT(GObjHash); i++ )
		GObjHash[i] = NULL;

	// If statically linked, initialize registrants.
	AUTO_INITIALIZE_REGISTRANTS;
	
	// Note initialized.
	GObjInitialized = 1;

	// Add all autoregistered classes.
	ProcessRegistrants();

	// Allocate special packages.
	GObjTransientPkg = new( NULL, TEXT("Transient") )UPackage;
	GObjTransientPkg->AddToRoot();

	debugf( NAME_Init, TEXT("Object subsystem initialized") );
}

//
// Process all objects that have registered.
//
void UObject::ProcessRegistrants()
{
	GObjRegisterCount++;
	TArray<UObject*>	ObjRegistrants;
	// Make list of all objects to be registered.
	for( ; GAutoRegister; GAutoRegister=*(UObject **)&GAutoRegister->_LinkerIndex )
		ObjRegistrants.AddItem( GAutoRegister );
	for( INT i=0; i<ObjRegistrants.Num(); i++ )
	{
		ObjRegistrants(i)->ConditionalRegister();
		for( ; GAutoRegister; GAutoRegister=*(UObject **)&GAutoRegister->_LinkerIndex )
			ObjRegistrants.AddItem( GAutoRegister );
	}
	ObjRegistrants.Empty();
	check(!GAutoRegister);
	--GObjRegisterCount;
}

//
// Shut down the object manager.
//
void UObject::StaticExit()
{
	check(GObjLoaded.Num()==0);
	check(GObjRegistrants.Num()==0);
	check(!GAutoRegister);

	// Cleanup root.
	GObjTransientPkg->RemoveFromRoot();

	// Tag all objects as unreachable.
	for( FObjectIterator It; It; ++It )
		It->SetFlags( RF_Unreachable | RF_TagGarbage );

	// Tag all names as unreachable.
	for( INT i=0; i<FName::GetMaxNames(); i++ )
		if( FName::GetEntry(i) )
			FName::GetEntry(i)->Flags |= RF_Unreachable;

	// Purge all objects.
	GExitPurge = 1;
 	PurgeGarbage();
	GObjObjects.Empty();

	// Empty arrays to prevent falsely-reported memory leaks.
	GObjLoaded			.Empty();
	GObjObjects			.Empty();
	GObjAvailable		.Empty();
	GObjLoaders			.Empty();
	GObjRoot			.Empty();
	GObjRegistrants		.Empty();

	GObjInitialized = 0;
	debugf( NAME_Exit, TEXT("Object subsystem successfully closed.") );
}

/*-----------------------------------------------------------------------------
	UObject Tick.
-----------------------------------------------------------------------------*/

//
// Mark one unit of passing time. This is used to update the object
// caching status. This must be called when the object manager is
// in a clean state (outside of all code which retains pointers to
// object data that was gotten).
//
void UObject::StaticTick()
{
	check(GObjBeginLoadCount==0);

	// Check natives.
	extern int GNativeDuplicate;
	if( GNativeDuplicate )
		appErrorf( TEXT("Duplicate native registered: %i"), GNativeDuplicate );

	extern int GCastDuplicate;
	if( GCastDuplicate )
		appErrorf( TEXT("Duplicate cast registered: %i"), GCastDuplicate );
}

/*-----------------------------------------------------------------------------
   Shutdown.
-----------------------------------------------------------------------------*/

//
// Make sure this object has been shut down.
//
void UObject::ConditionalShutdownAfterError()
{
	if( !(GetFlags() & RF_ErrorShutdown) )
	{
		SetFlags( RF_ErrorShutdown );
		try
		{
			ShutdownAfterError();
		}
		catch( ... )
		{
			debugf( NAME_Exit, TEXT("Double fault in object ShutdownAfterError") );
		}
	}
}

//
// After a critical error, shutdown all objects which require
// mission-critical cleanup, such as restoring the video mode,
// releasing hardware resources.
//
void UObject::StaticShutdownAfterError()
{
	if( GObjInitialized )
	{
		static UBOOL Shutdown=0;
		if( Shutdown )
			return;
		Shutdown = 1;
		debugf( NAME_Exit, TEXT("Executing UObject::StaticShutdownAfterError") );
		try
		{
			for( INT i=0; i<GObjObjects.Num(); i++ )
				if( GObjObjects(i) )
					GObjObjects(i)->ConditionalShutdownAfterError();
		}
		catch( ... )
		{
			debugf( NAME_Exit, TEXT("Double fault in object manager ShutdownAfterError") );
		}
	}
}

//
// Bind package to DLL.
//warning: Must only find packages in the \Unreal\System directory!
//
void UObject::BindPackage( UPackage* Package )
{
	if( !Package->Bound && !Package->GetOuter())
	{
		Package->Bound = 1;
		GObjNoRegister = 0;
		GObjNoRegister = 1;
		ProcessRegistrants();
	}
}

/*-----------------------------------------------------------------------------
   Command line.
-----------------------------------------------------------------------------*/

//
// Archive for enumerating other objects referencing a given object.
//
class FArchiveShowReferences : public FArchive
{
public:
	FArchiveShowReferences( FOutputDevice& InAr, UObject* InOuter, UObject* InObj, TArray<UObject*>& InExclude )
	: DidRef( 0 ), Ar( InAr ), Parent( InOuter ), Obj( InObj ), Exclude( InExclude )
	{
		Obj->Serialize( *this );
	}
	UBOOL DidRef;
private:
	FArchive& operator<<( UObject*& Obj )
	{
		if( Obj && Obj->GetOuter()!=Parent )
		{
			INT i;
			for( i=0; i<Exclude.Num(); i++ )
				if( Exclude(i) == Obj->GetOuter() )
					break;
			if( i==Exclude.Num() )
			{
				if( !DidRef )
					Ar.Logf( TEXT("   %s references:"), *Obj->GetFullName() );
				Ar.Logf( TEXT("      %s"), *Obj->GetFullName() );
				DidRef=1;
			}
		}
		return *this;
	}
	FOutputDevice& Ar;
	UObject* Parent;
	UObject* Obj;
	TArray<UObject*>& Exclude;
};

//
// Archive for finding who references an object.
//
class FArchiveFindCulprit : public FArchive
{
public:
	FArchiveFindCulprit( UObject* InFind, UObject* Src )
	: Find(InFind), Count(0)
	{
		Src->Serialize( *this );
	}
	INT GetCount()
	{
		return Count;
	}
	FArchive& operator<<( class UObject*& Obj )
	{
		if( Obj==Find )
			Count++;
		return *this;
	}
protected:
	UObject* Find;
	INT Count;
};

//
// Archive for finding shortest path from root to a particular object.
// Depth-first search.
//
struct FTraceRouteRecord
{
	INT Depth;
	UObject* Referencer;
	FTraceRouteRecord( INT InDepth, UObject* InReferencer )
	: Depth(InDepth), Referencer(InReferencer)
	{}
};
class FArchiveTraceRoute : public FArchive
{
public:
	static TArray<UObject*> FindShortestRootPath( UObject* Obj )
	{
		TMap<UObject*,FTraceRouteRecord> Routes;
		FArchiveTraceRoute Rt( Routes );
		TArray<UObject*> Result;
		if( Routes.Find(Obj) )
		{
			Result.AddItem( Obj );
			for( ; ; )
			{
				FTraceRouteRecord* Rec = Routes.Find(Obj);
				if( Rec->Depth==0 )
					break;
				Obj = Rec->Referencer;
				Result.Insert(0);
				Result(0) = Obj;
			}
		}
		return Result;
	}
private:
	FArchiveTraceRoute( TMap<UObject*,FTraceRouteRecord>& InRoutes )
	: Routes(InRoutes), Depth(0), Prev(NULL)
	{
		{for( FObjectIterator It; It; ++It )
			It->SetFlags( RF_TagExp );}
		UObject::SerializeRootSet( *this, RF_Native, 0 );
		{for( FObjectIterator It; It; ++It )
			It->ClearFlags( RF_TagExp );}
	}
	FArchive& operator<<( class UObject*& Obj )
	{
		if( Obj )
		{
			FTraceRouteRecord* Rec = Routes.Find(Obj);
			if( !Rec || Depth<Rec->Depth )
				Routes.Set( Obj, FTraceRouteRecord(Depth,Prev) );
		}
		if( Obj && (Obj->GetFlags() & RF_TagExp) )
		{
			Obj->ClearFlags( RF_TagExp );
			UObject* SavedPrev = Prev;
			Prev = Obj;
			Depth++;
			Obj->Serialize( *this );
			Depth--;
			Prev = SavedPrev;
		}
		return *this;
	}
	TMap<UObject*,FTraceRouteRecord>& Routes;
	INT Depth;
	UObject* Prev;
};

//
// Show the inheretance graph of all loaded classes.
//
static void ShowClasses( UClass* Class, FOutputDevice& Ar, int Indent )
{
	Ar.Logf( TEXT("%s%s"), appSpc(Indent), Class->GetName() );
	for( TObjectIterator<UClass> It; It; ++It )
		if( It->GetSuperClass() == Class )
			ShowClasses( *It, Ar, Indent+2 );
}

struct FItem
{
	UClass*	Class;
	INT		Count;
	SIZE_T	Num, Max;
	FItem( UClass* InClass=NULL )
	: Class(InClass), Count(0), Num(0), Max(0)
	{}
	void Add( FArchiveCountMem& Ar )
	{Count++; Num+=Ar.GetNum(); Max+=Ar.GetMax();}
};
struct FSubItem
{
	UObject* Object;
	SIZE_T Num, Max;
	FSubItem( UObject* InObject, SIZE_T InNum, SIZE_T InMax )
	: Object( InObject ), Num( InNum ), Max( InMax )
	{}
};


IMPLEMENT_COMPARE_CONSTREF( FSubItem, UnObj, { return B.Max - A.Max; } );
IMPLEMENT_COMPARE_CONSTREF( FItem, UnObj, { return B.Max - A.Max; } );

UBOOL UObject::StaticExec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	const TCHAR *Str = Cmd;
	if( ParseCommand(&Str,TEXT("MEM")) )
	{
		GMalloc->DumpAllocs();
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("DUMPNATIVES")) )
	{
		// Linux: Defined out because of error: "taking
		// the address of a bound member function to form
		// a pointer to member function, say '&Object::execUndefined'
#if _MSC_VER
		for( INT i=0; i<EX_Max; i++ )
			if( GNatives[i] == &execUndefined )
				debugf( TEXT("Native index %i is available"), i );
#endif
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("GET")) )
	{
		// Get a class default variable.
		TCHAR ClassName[256], PropertyName[256];
		UClass* Class;
		UProperty* Property;
		if
		(	ParseToken( Str, ClassName, ARRAY_COUNT(ClassName), 1 )
		&&	(Class=FindObject<UClass>( ANY_PACKAGE, ClassName))!=NULL )
		{
			if
			(	ParseToken( Str, PropertyName, ARRAY_COUNT(PropertyName), 1 )
			&&	(Property=FindField<UProperty>( Class, PropertyName))!=NULL )
			{
				FString	Temp;
				if( Class->Defaults.Num() )
					Property->ExportText( 0, Temp, &Class->Defaults(0), &Class->Defaults(0), Class, PPF_Localized );
				Ar.Log( *Temp );
			}
			else Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized property %s"), PropertyName );
		}
		else Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized class %s"), ClassName );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SET")) )
	{
		// Set a class default variable.
		TCHAR ClassName[256], PropertyName[256];
		UClass* Class;
		UProperty* Property;
		if
		(	ParseToken( Str, ClassName, ARRAY_COUNT(ClassName), 1 )
		&&	(Class=FindObject<UClass>( ANY_PACKAGE, ClassName))!=NULL )
		{
			if
			(	ParseToken( Str, PropertyName, ARRAY_COUNT(PropertyName), 1 )
			&&	(Property=FindField<UProperty>( Class, PropertyName))!=NULL )
			{
				while( *Str==' ' )
					Str++;
				GlobalSetProperty( Str, Class, Property, Property->Offset, 1 );
			}
			else Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized property %s"), PropertyName );
		}
		else Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized class %s"), ClassName );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("OBJ")) )
	{
		if( ParseCommand(&Str,TEXT("GARBAGE")) )
		{
			// Purge unclaimed objects.
			CollectGarbage( RF_Native | (GIsEditor ? RF_Standalone : 0) );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("MARK")) )
		{
			debugf( TEXT("Marking objects") );
			for( FObjectIterator It; It; ++It )
				It->SetFlags( RF_Marked );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("MARKCHECK")) )
		{
			debugf( TEXT("Unmarked objects:") );
			for( FObjectIterator It; It; ++It )
				if( !(It->GetFlags() & RF_Marked) )
					debugf( TEXT("%s"), *It->GetFullName() );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("REFS")) )
		{
			UClass* Class;
			UObject* Object;
			if
			(	ParseObject<UClass>( Str, TEXT("CLASS="), Class, ANY_PACKAGE )
			&&	ParseObject(Str,TEXT("NAME="), Class, Object, ANY_PACKAGE ) )
			{
				Ar.Logf( TEXT("") );
				Ar.Logf( TEXT("Referencers of %s:"), *Object->GetFullName() );
				for( FObjectIterator It; It; ++It )
				{
					FArchiveFindCulprit ArFind(Object,*It);
					if( ArFind.GetCount() )
						Ar.Logf( TEXT("   %s"), *It->GetFullName() );
				}
				Ar.Logf(TEXT("") );
				Ar.Logf(TEXT("Shortest reachability from root to %s:"), *Object->GetFullName() );
				TArray<UObject*> Rt = FArchiveTraceRoute::FindShortestRootPath(Object);
				for( INT i=0; i<Rt.Num(); i++ )
					Ar.Logf(TEXT("   %s%s"), *Rt(i)->GetFullName(), i!=0 ? TEXT("") : (Rt(i)->GetFlags()&RF_Native)?TEXT(" (native)"):TEXT(" (root)") );
				if( !Rt.Num() )
					Ar.Logf(TEXT("   (Object is not currently rooted)"));
				Ar.Logf(TEXT("") );
			}
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("HASH")) )
		{
			// Hash info.
			FName::DisplayHash( Ar );
			INT ObjCount=0, HashCount=0;
			for( FObjectIterator It; It; ++It )
				ObjCount++;
			for( INT i=0; i<ARRAY_COUNT(GObjHash); i++ )
			{
				INT c=0;
				for( UObject* Hash=GObjHash[i]; Hash; Hash=Hash->HashNext )
					c++;
				if( c )
					HashCount++;
				//debugf( "%i: %i", i, c );
			}
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("CLASSES")) )
		{
			ShowClasses( StaticClass(), Ar, 0 );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("DEPENDENCIES")) )
		{
			UPackage* Pkg;
			if( ParseObject<UPackage>(Str,TEXT("PACKAGE="),Pkg,NULL) )
			{
				TArray<UObject*> Exclude;
				for( int i=0; i<16; i++ )
				{
					TCHAR Temp[32];
					appSprintf( Temp, TEXT("EXCLUDE%i="), i );
					FName F;
					if( Parse(Str,Temp,F) )
						Exclude.AddItem( CreatePackage(NULL,*F) );
				}
				Ar.Logf( TEXT("Dependencies of %s:"), *Pkg->GetPathName() );
				for( FObjectIterator It; It; ++It )
					if( It->GetOuter()==Pkg )
						FArchiveShowReferences ArShowReferences( Ar, Pkg, *It, Exclude );
			}
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("LIST")) )
		{
			Ar.Log( TEXT("Objects:") );
			Ar.Log( TEXT("") );
			UClass*   CheckType     = NULL;
			UPackage* CheckPackage  = NULL;
			UPackage* InsidePackage = NULL;
			ParseObject<UClass>  ( Str, TEXT("CLASS="  ), CheckType,     ANY_PACKAGE );
			ParseObject<UPackage>( Str, TEXT("PACKAGE="), CheckPackage,  NULL );
			ParseObject<UPackage>( Str, TEXT("INSIDE=" ), InsidePackage, NULL );
			TArray<FItem> List;
			TArray<FSubItem> Objects;
			FItem Total;
			for( FObjectIterator It; It; ++It )
			{
				if
				(	(CheckType     && It->IsA(CheckType))
				||	(CheckPackage  && It->GetOuter()==CheckPackage)
				||	(InsidePackage && It->IsIn(InsidePackage))
				||	(!CheckType && !CheckPackage && !InsidePackage) )
				{
					FArchiveCountMem Count( *It );
					INT i;
					for( i=0; i<List.Num(); i++ )
						if( List(i).Class == It->GetClass() )
							break;
					if( i==List.Num() )
						i = List.AddItem(FItem( It->GetClass() ));
					if( CheckType || CheckPackage || InsidePackage )
						new(Objects)FSubItem( *It, Count.GetNum(), Count.GetMax() );
					List(i).Add( Count );
					Total.Add( Count );
				}
			}
			if( Objects.Num() )
			{
				Sort<USE_COMPARE_CONSTREF(FSubItem,UnObj)>( &Objects(0), Objects.Num() );
				Ar.Logf( TEXT("%60s % 10s % 10s"), TEXT("Object"), TEXT("NumBytes"), TEXT("MaxBytes") );
				for( INT i=0; i<Objects.Num(); i++ )
					Ar.Logf( TEXT("%60s % 10i % 10i"), *Objects(i).Object->GetFullName(), Objects(i).Num, Objects(i).Max );
				Ar.Log( TEXT("") );
			}
			if( List.Num() )
			{
				Sort<USE_COMPARE_CONSTREF(FItem,UnObj)>( &List(0), List.Num() );
				Ar.Logf(TEXT(" %60s % 6s % 10s  % 10s "), TEXT("Class"), TEXT("Count"), TEXT("NumBytes"), TEXT("MaxBytes") );
				for( INT i=0; i<List.Num(); i++ )
					Ar.Logf(TEXT(" %60s % 6i % 10iK % 10iK"), List(i).Class->GetName(), List(i).Count, List(i).Num/1024, List(i).Max/1024 );
				Ar.Log( TEXT("") );
			}
			Ar.Logf( TEXT("%i Objects (%.3fM / %.3fM)"), Total.Count, (FLOAT)Total.Num/1024.0/1024.0, (FLOAT)Total.Max/1024.0/1024.0 );
			return 1;
		}
		/*
		else if( ParseCommand(&Str,TEXT("VFHASH")) )
		{
			Ar.Logf( TEXT("Class VfHashes:") );
			for( TObjectIterator<UState> It; It; ++It )
			{
				Ar.Logf( TEXT("%s:"), It->GetName() );
				for( INT i=0; i<UField::HASH_COUNT; i++ )
				{
					INT c=0;
					for( UField* F=It->VfHash[i]; F; F=F->HashNext )
						c++;
					Ar.Logf( TEXT("   %i: %i"), i, c );
				}
			}
			return 1;
		}
		*/
		else if( ParseCommand(&Str,TEXT("LINKERS")) )
		{
			Ar.Logf( TEXT("Linkers:") );
			for( INT i=0; i<GObjLoaders.Num(); i++ )
			{
				ULinkerLoad* Linker = CastChecked<ULinkerLoad>( GObjLoaders(i) );
				INT NameSize = 0;
				for( INT i=0; i<Linker->NameMap.Num(); i++ )
					if( Linker->NameMap(i) != NAME_None )
						NameSize += sizeof(FNameEntry) - (NAME_SIZE - appStrlen(*Linker->NameMap(i)) - 1) * sizeof(TCHAR);
				Ar.Logf
				(
					TEXT("%s (%s): Names=%i (%iK/%iK) Imports=%i (%iK) Exports=%i (%iK) Gen=%i Lazy=%i"),
					*Linker->Filename,
					*Linker->LinkerRoot->GetFullName(),
					Linker->NameMap.Num(),
					Linker->NameMap.Num() * sizeof(FName) / 1024,
					NameSize / 1024,
					Linker->ImportMap.Num(),
					Linker->ImportMap.Num() * sizeof(FObjectImport) / 1024,
					Linker->ExportMap.Num(),
					Linker->ExportMap.Num() * sizeof(FObjectExport) / 1024,
					Linker->Summary.Generations.Num(),
					Linker->LazyLoaders.Num()
				);
			}
			return 1;
		}
		else return 0;
	}
	else if( ParseCommand(&Str,TEXT("PROFILESCRIPT")) )
	{
		if( ParseCommand(&Str,TEXT("START")) )
		{
			delete GScriptCallGraph;
			GScriptCallGraph = new FScriptCallGraph( 32 * 1024 * 1024 );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("STOP")) )
		{
			if( GScriptCallGraph )
			{
				GScriptCallGraph->EmitEnd();

				// Create unique filename based on time.
				INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
				appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );
				FString	Filename = FString::Printf(TEXT("%sProfiling\\%sGame-%i.%02i.%02i-%02i.%02i.%02i.uprof"), *appGameDir(), GGameName, Year, Month, Day, Hour, Min, Sec );

				// Create the directory in case it doesn't exist yet.
				GFileManager->MakeDirectory( *(appGameDir() + TEXT("Profiling\\")) );

				// Create a file writer and serialize the script profiling data to it.
				FArchive* Archive = GFileManager->CreateFileWriter( *Filename );
				if( Archive )
				{
					GScriptCallGraph->Serialize( *Archive );
					delete Archive;
				}

				delete GScriptCallGraph;
				GScriptCallGraph = NULL;
			}
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("RESET"))  )
		{
			if( GScriptCallGraph )
				GScriptCallGraph->Reset();
			return 1;
		}
		return 0;
	}
	else
	{
		return 0; // Not executed
	}
}

/*-----------------------------------------------------------------------------
   File loading.
-----------------------------------------------------------------------------*/

//
// Safe load error-handling.
//
void UObject::SafeLoadError( UObject* Outer, DWORD LoadFlags, const TCHAR* Error, const TCHAR* Fmt, ... )
{
	// Variable arguments setup.
	TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt, Fmt );
	if( !(LoadFlags & LOAD_Quiet)  ) debugf( NAME_Warning, TEXT("%s"), TempStr );
	if(   LoadFlags & LOAD_Throw   ) appThrowf( TEXT("%s"), Error   );
	if(   LoadFlags & LOAD_NoFail  ) appErrorf( TEXT("%s"), TempStr );
	if( !(LoadFlags & LOAD_NoWarn) ) warnf( TEXT("%s"), TempStr );

	// Mark package containing unresolved references as dirty.
	UPackage* Package = Cast<UPackage>(Outer ? Outer->GetOutermost() : NULL);
	if( Package )
		Package->bDirty = 1;
}

//
// Find or create the linker for a package.
//
ULinkerLoad* UObject::GetPackageLinker
(
	UObject*		InOuter,
	const TCHAR*	InFilename,
	DWORD			LoadFlags,
	UPackageMap*	Sandbox,
	FGuid*			CompatibleGuid
)
{
	check(GObjBeginLoadCount);

	// See if there is already a linker for this package.
	ULinkerLoad* Result = NULL;
	if( InOuter )
		for( INT i=0; i<GObjLoaders.Num() && !Result; i++ )
			if( GetLoader(i)->LinkerRoot == InOuter )
				Result = GetLoader( i );

	// Try to load the linker.
	try
	{
		// See if the linker is already loaded.
		FString NewFilename;
		if( Result )
		{
			// Linker already found.
			NewFilename = TEXT("");
		}
		else
		if( !InFilename )
		{
			// Resolve filename from package name.
			if( !InOuter )
				appThrowf( *LocalizeError(TEXT("PackageResolveFailed"),TEXT("Core")) );
			if( !GPackageFileCache->FindPackageFile( InOuter->GetName(), CompatibleGuid, NewFilename ) )
			{
				// See about looking in the dll.
				if( (LoadFlags & LOAD_AllowDll) && InOuter->IsA(UPackage::StaticClass()) && ((UPackage*)InOuter)->Bound )
					return NULL;
				appThrowf( *LocalizeError(TEXT("PackageNotFound"),TEXT("Core")), InOuter->GetName() );
			}
		}
		else
		{
			// Verify that the file exists.
			if( !GPackageFileCache->FindPackageFile( InFilename, CompatibleGuid, NewFilename ) )
				appThrowf( *LocalizeError(TEXT("FileNotFound"),TEXT("Core")), InFilename );

			// Resolve package name from filename.
			TCHAR Tmp[256], *T=Tmp;
			appStrncpy( Tmp, InFilename, ARRAY_COUNT(Tmp) );
			while( 1 )
			{
				if( appStrstr(T,PATH_SEPARATOR) )
					T = appStrstr(T,PATH_SEPARATOR)+appStrlen(PATH_SEPARATOR);
				else if( appStrstr(T,TEXT("/")) )
					T = appStrstr(T,TEXT("/"))+1;
				else if( appStrstr(T,TEXT(":")) )
					T = appStrstr(T,TEXT(":"))+1;
				else
					break;
			}
			if( appStrstr(T,TEXT(".")) )
				*appStrstr(T,TEXT(".")) = 0;
			UPackage* FilenamePkg = CreatePackage( NULL, T );

			// If no package specified, use package from file.
			if( InOuter==NULL )
			{
				if( !FilenamePkg )
					appThrowf( *LocalizeError(TEXT("FilenameToPackage"),TEXT("Core")), InFilename );
				InOuter = FilenamePkg;
				for( INT i=0; i<GObjLoaders.Num() && !Result; i++ )
					if( GetLoader(i)->LinkerRoot == InOuter )
						Result = GetLoader(i);
			}
			else if( InOuter != FilenamePkg )//!!should be tested and validated in new UnrealEd
			{
				// Loading a new file into an existing package, so reset the loader.
				debugf( TEXT("New File, Existing Package (%s, %s)"), *InOuter->GetFullName(), *FilenamePkg->GetFullName() );
				ResetLoaders( InOuter, 0, 1 );
			}
		}

		// Make sure the package is accessible in the sandbox.
		if( Sandbox && !Sandbox->SupportsPackage(InOuter) )
			appThrowf( *LocalizeError(TEXT("Sandbox"),TEXT("Core")), InOuter->GetName() );

		// Create new linker.
		if( !Result )
			Result = new ULinkerLoad( InOuter, *NewFilename, LoadFlags );

		// Verify compatibility.
		if( CompatibleGuid && Result->Summary.Guid!=*CompatibleGuid )
			appThrowf( *LocalizeError(TEXT("PackageVersion"),TEXT("Core")), InOuter->GetName() );
	}
	catch( const TCHAR* Error )
	{
		// If we're in the editor (and not running from UCC) we don't want this to be a fatal error.
		if( GIsEditor && !GIsUCC )
		{
			EdLoadErrorf( FEdLoadError::TYPE_FILE, InFilename ? InFilename : InOuter ? *InOuter->GetPathName() : TEXT("NULL") );
			debugf( *LocalizeError(TEXT("FailedLoad"),TEXT("Core")), InFilename ? InFilename : InOuter ? InOuter->GetName() : TEXT("NULL"), Error);
		}
		else
		{
			SafeLoadError( InOuter, LoadFlags, Error, *LocalizeError(TEXT("FailedLoad"),TEXT("Core")), InFilename ? InFilename : InOuter ? InOuter->GetName() : TEXT("NULL"), Error );
		}
	}

	// Success.
	return Result;
}

//
// Find or optionally create a package.
//
UPackage* UObject::CreatePackage( UObject* InOuter, const TCHAR* OrigInName )
{
	FString InName = OrigInName;
	ResolveName( InOuter, InName, 1, 0 );
	UPackage* Result = FindObject<UPackage>( InOuter, *InName );
	if( !Result )
		Result = new( InOuter, *InName, RF_Public )UPackage;
	return Result;
}

//
// Resolve a package and name.
//
UBOOL UObject::ResolveName( UObject*& InPackage, FString& InName, UBOOL Create, UBOOL Throw )
{
	const TCHAR* IniFilename = NULL;
	
	// See if the name is specified in the .ini file.
	if( appStrnicmp( *InName, TEXT("engine-ini:"), appStrlen(TEXT("engine-ini:")) )==0 )
		IniFilename = GEngineIni;
	if( appStrnicmp( *InName, TEXT("game-ini:"), appStrlen(TEXT("game-ini:")) )==0 )
		IniFilename = GGameIni;
	if( appStrnicmp( *InName, TEXT("input-ini:"), appStrlen(TEXT("input-ini:")) )==0 )
		IniFilename = GInputIni;
	if( appStrnicmp( *InName, TEXT("editor-ini:"), appStrlen(TEXT("editor-ini:")) )==0 )
		IniFilename = GEditorIni;

	if( IniFilename && InName.InStr(TEXT("."))!=-1 )
	{
		// Get .ini key and section.
		FString Section = InName.Mid(1+InName.InStr(TEXT(":")));
		INT i = Section.InStr(TEXT("."), 1);
		FString Key;
		if( i != -1)
		{
			Key = Section.Mid(i+1);
			Section = Section.Left(i);
		}		

		// Look up name.
		FString Result;
		if( !GConfig->GetString( *Section, *Key, Result, IniFilename ) )
		{
			if( Throw )
				appThrowf( *LocalizeError(TEXT("ConfigNotFound"),TEXT("Core")), *InName );
			return 0;
		}
		InName = Result;
	}

	// Handle specified packages.
	INT i;
	while( (i=InName.InStr(TEXT("."))) != -1 )
	{
		FString PartialName = InName.Left(i);
		if( Create )
		{
			InPackage = CreatePackage( InPackage, *PartialName );
		}
		else
		{
			UObject* NewPackage = FindObject<UPackage>( InPackage, *PartialName );
			if( !NewPackage )
			{
				NewPackage = FindObject<UObject>( InPackage, *PartialName );
				if( !NewPackage )
					return 0;
			}
			InPackage = NewPackage;
		}
		InName = InName.Mid(i+1);
	}
	return 1;
}

//
// Load an object.
//
UObject* UObject::StaticLoadObject( UClass* ObjectClass, UObject* InOuter, const TCHAR* InName, const TCHAR* Filename, DWORD LoadFlags, UPackageMap* Sandbox )
{
	check(ObjectClass);
	check(InName);

	// Try to load.
	FString StrName = InName;
	UObject* Result=NULL;
	BeginLoad();
	try
	{
		// Create a new linker object which goes off and tries load the file.
		ULinkerLoad* Linker = NULL;
		ResolveName( InOuter, StrName, 1, 1 );
		UObject*	TopOuter = InOuter;
		while( TopOuter && TopOuter->GetOuter() )//!!can only load top-level packages from files
			TopOuter = TopOuter->GetOuter();
		if( !(LoadFlags & LOAD_DisallowFiles) )
			Linker = GetPackageLinker( TopOuter, Filename, LoadFlags | LOAD_Throw | LOAD_AllowDll, Sandbox, NULL );
		//!!this sucks because it supports wildcard sub-package matching of StrName, which requires a long search.
		//!!also because linker classes require exact match
		if( Linker )
			Result = Linker->Create( ObjectClass, *StrName, LoadFlags, 0 );
		if( !Result )
			Result = StaticFindObject( ObjectClass, InOuter, *StrName );
		if( !Result )
			appThrowf( *LocalizeError(TEXT("ObjectNotFound"),TEXT("Core")), ObjectClass->GetName(), InOuter ? *InOuter->GetPathName() : TEXT("None"), *StrName );
		EndLoad();
	}
	catch( const TCHAR* Error )
	{
		EndLoad();
		SafeLoadError( InOuter, LoadFlags, Error, *LocalizeError(TEXT("FailedLoadObject"),TEXT("Core")), ObjectClass->GetName(), InOuter ? *InOuter->GetPathName() : TEXT("None"), *StrName, Error );
	}
	return Result;
}

//
// Load a class.
//
UClass* UObject::StaticLoadClass( UClass* BaseClass, UObject* InOuter, const TCHAR* InName, const TCHAR* Filename, DWORD LoadFlags, UPackageMap* Sandbox )
{
	check(BaseClass);
	try
	{
		UClass* Class = LoadObject<UClass>( InOuter, InName, Filename, LoadFlags | LOAD_Throw, Sandbox );
		if( Class && !Class->IsChildOf(BaseClass) )
			appThrowf( *LocalizeError(TEXT("LoadClassMismatch"),TEXT("Core")), *Class->GetFullName(), *BaseClass->GetFullName() );
		return Class;
	}
	catch( const TCHAR* Error )
	{
		// Failed.
		SafeLoadError( InOuter, LoadFlags, Error, Error );
		return NULL;
	}
}

//
// Load all objects in a package.
//
UObject* UObject::LoadPackage( UObject* InOuter, const TCHAR* Filename, DWORD LoadFlags )
{
	UObject* Result;

    if( *Filename == '\0' )
        return NULL;

	// Try to load.
	BeginLoad();
	try
	{
		// Create a new linker object which goes off and tries load the file.
		ULinkerLoad* Linker = GetPackageLinker( InOuter, Filename ? Filename : InOuter->GetName(), LoadFlags | LOAD_Throw, NULL, NULL );
        if( !Linker )
		{
			EndLoad();
            return( NULL );
		}

		if( !(LoadFlags & LOAD_Verify) )
			Linker->LoadAllObjects();
		Result = Linker->LinkerRoot;
		if( GIsEditor && !GIsUCC )
			UObject::LoadEverything();
		EndLoad();
	}
	catch( const TCHAR* Error )
	{
		EndLoad();
		SafeLoadError( InOuter, LoadFlags, Error, *LocalizeError(TEXT("FailedLoadPackage"),TEXT("Core")), Error );
		Result = NULL;
	}
	return Result;
}

//
// Verify a linker.
//
void UObject::VerifyLinker( ULinkerLoad* Linker )
{
	Linker->Verify();
}

//
// Begin loading packages.
//warning: Objects may not be destroyed between BeginLoad/EndLoad calls.
//
void UObject::BeginLoad()
{
	if( ++GObjBeginLoadCount == 1 )
	{
		// Validate clean load state.
		//!!needed? check(GObjLoaded.Num()==0);
		check(!GAutoRegister);
		for( INT i=0; i<GObjLoaders.Num(); i++ )
			check(GetLoader(i)->Success);
	}
}

//
// End loading packages.
//
void UObject::EndLoad()
{
	check(GObjBeginLoadCount>0);
	if( --GObjBeginLoadCount == 0 )
	{
		try
		{
			TArray<UObject*>	ObjLoaded;

			while(GObjLoaded.Num())
			{
				appMemcpy(&ObjLoaded(ObjLoaded.Add(GObjLoaded.Num())),&GObjLoaded(0),GObjLoaded.Num() * sizeof(UObject*));
				GObjLoaded.Empty();

				// Finish loading everything.
				//warning: Array may expand during iteration.
				debugfSlow( NAME_DevLoad, TEXT("Loading objects...") );
				for( INT i=0; i<ObjLoaded.Num(); i++ )
				{
					// Preload.
					UObject* Obj = ObjLoaded(i);
					{
						if( Obj->GetFlags() & RF_NeedLoad )
						{
							check(Obj->GetLinker());
							Obj->GetLinker()->Preload( Obj );
						}
					}
				}
				if(GObjLoaded.Num())
					continue;

				// Postload objects.
				for( INT i=0; i<ObjLoaded.Num(); i++ )
				{
					UObject*	Obj = ObjLoaded(i);

					if( (Obj->GetClass()->ClassFlags&CLASS_HasComponents) )
					{
                        TMap<FName,UComponent*>	InstanceMap;

						// Find any legacy component instances which weren't saved in the export map.

						Obj->GetClass()->FixLegacyComponents((BYTE*)Obj,(BYTE*)&Obj->GetClass()->Defaults(0),Obj);
						Obj->GetClass()->FindInstancedComponents(InstanceMap,(BYTE*)Obj,Obj);

						// Restore the component pointers to the saved component instances.

						check(Obj->GetLinker());
						check(Obj->GetLinkerIndex() >= 0 && Obj->GetLinkerIndex() < Obj->GetLinker()->ExportMap.Num());

						FObjectExport&	Export = Obj->GetLinker()->ExportMap(Obj->GetLinkerIndex());

						for(TMap<FName,UComponent*>::TIterator It(Obj->GetClass()->ComponentNameToDefaultObjectMap);It;++It)
						{
							INT*	ComponentInstanceIndex = Export.ComponentMap.Find(It.Key());
							if(ComponentInstanceIndex)
							{
								UComponent* Instance = Cast<UComponent>(Obj->GetLinker()->CreateExport(*ComponentInstanceIndex));
								if(Instance)
								{
									InstanceMap.Set(It.Key(),Instance);
								}
							}
						}

						Obj->GetClass()->InstanceComponents( InstanceMap, (BYTE*)Obj, Obj );
						Obj->GetClass()->FixupComponentReferences( InstanceMap, (BYTE*)Obj, Obj );
					}

					Obj->ConditionalPostLoad();
				}
			}

			// Dissociate all linker import object references, since they 
			// may be destroyed, causing their pointers to become invalid.
			if( GImportCount )
			{
				for( INT i=0; i<GObjLoaders.Num(); i++ )
				{
					for( INT j=0; j<GetLoader(i)->ImportMap.Num(); j++ )
					{
						FObjectImport& Import = GetLoader(i)->ImportMap(j);
						if( Import.XObject && !(Import.XObject->GetFlags() & RF_Native) )
							Import.XObject = NULL;
					}
				}
			}
			GImportCount=0;
		}
		catch( const TCHAR* Error )
		{
			// Any errors here are fatal.
			GError->Logf( Error );
		}
	}
}

//
// Empty the loaders.
//
void UObject::ResetLoaders( UObject* Pkg, UBOOL DynamicOnly, UBOOL ForceLazyLoad )
{
	for( INT i=GObjLoaders.Num()-1; i>=0; i-- )
	{
		ULinkerLoad* Linker = CastChecked<ULinkerLoad>( GetLoader(i) );
		if( Pkg==NULL || Linker->LinkerRoot==Pkg )
		{
			if( DynamicOnly )
			{
				// Reset runtime-dynamic objects.
				for( INT i=0; i<Linker->ExportMap.Num(); i++ )
				{
					UObject*& Object = Linker->ExportMap(i)._Object;
					if( Object && !(Object->GetClass()->ClassFlags & CLASS_RuntimeStatic))
						Linker->DetachExport( i );
				}
			}
			else
			{
				// Fully reset the loader.
				if( ForceLazyLoad )
					Linker->DetachAllLazyLoaders( 1 );
			}
		}
		else
		{
			for(INT i = 0;i < Linker->ImportMap.Num();i++)
			{
				if(Linker->ImportMap(i).SourceLinker && Linker->ImportMap(i).SourceLinker->LinkerRoot == Pkg)
				{
					Linker->ImportMap(i).SourceLinker = NULL;
					Linker->ImportMap(i).SourceIndex = INDEX_NONE;
				}
			}
		}
	}
	for( INT i=GObjLoaders.Num()-1; i>=0; i-- )
	{
		ULinkerLoad* Linker = CastChecked<ULinkerLoad>( GetLoader(i) );
		if( Pkg==NULL || Linker->LinkerRoot==Pkg )
		{
			if( !DynamicOnly )
			{
				delete Linker;
			}
		}
	}
}

//
//	Entirely load all open packages.
//
void UObject::LoadEverything()
{
	BeginLoad();
	for( INT i=GObjLoaders.Num()-1; i>=0; i-- )
	{
		ULinkerLoad* Linker = CastChecked<ULinkerLoad>( GetLoader(i) );
		Linker->LoadAllObjects();
	}
	EndLoad();
}

/*-----------------------------------------------------------------------------
	File saving.
-----------------------------------------------------------------------------*/

//
// Archive for tagging objects and names that must be exported
// to the file.  It tags the objects passed to it, and recursively
// tags all of the objects this object references.
//
class FArchiveSaveTagExports : public FArchive
{
public:
	FArchiveSaveTagExports( UObject* InOuter )
	: Parent(InOuter)
	{
		ArIsSaving = ArIsPersistent = 1;
	}
	FArchive& operator<<( UObject*& Obj )
	{
		if( Obj != NULL &&
			Obj->IsIn(Parent) &&
			!(Obj->GetFlags() & (RF_Transient|RF_TagExp)))
		{
			// Set flags.
			Obj->SetFlags(RF_TagExp);
			if( Obj->NeedsLoadForEdit() ) Obj->SetFlags(RF_LoadForEdit);
			if( Obj->NeedsLoadForClient() ) Obj->SetFlags(RF_LoadForClient);
			if( Obj->NeedsLoadForServer() ) Obj->SetFlags(RF_LoadForServer);

			// Recurse with this object's class and package.
			UObject* Class  = Obj->GetClass();
			UObject* Parent = Obj->GetOuter();
			*this << Class << Parent;

			// Recurse with this object's children.
			Obj->Serialize( *this );

			// Recurse with this object's components.
			TMap<FName,UComponent*>	InstanceMap;
			Obj->GetClass()->FindInstancedComponents(InstanceMap,(BYTE*)Obj,Obj);

			for(TMap<FName,UComponent*>::TIterator InstanceIt(InstanceMap);InstanceIt;++InstanceIt)
			{
				UComponent*	Component = InstanceIt.Value();
				*this << *(UObject**)&Component;
				InstanceIt.Key().SetFlags(RF_TagExp | RF_LoadForEdit | RF_LoadForClient | RF_LoadForServer);
			}
		}
		return *this;
	}
	UObject* Parent;
};

//
// QSort comparators.
//
static ULinkerSave* GTempSave;
INT CDECL LinkerNameSort( const void* A, const void* B )
{
	return GTempSave->MapName((FName*)B) - GTempSave->MapName((FName*)A);
}
INT CDECL LinkerImportSort( const void* A, const void* B )
{
	return GTempSave->MapObject(((FObjectImport*)B)->XObject) - GTempSave->MapObject(((FObjectImport*)A)->XObject);
}
INT CDECL LinkerExportSort( const void* A, const void* B )
{
	return GTempSave->MapObject(((FObjectExport*)B)->_Object) - GTempSave->MapObject(((FObjectExport*)A)->_Object);
}

//
// Archive for tagging objects and names that must be listed in the
// file's imports table.
//
class FArchiveSaveTagImports : public FArchive
{
public:
	DWORD ContextFlags;
	FArchiveSaveTagImports( ULinkerSave* InLinker, DWORD InContextFlags )
	: ContextFlags( InContextFlags ), Linker( InLinker )
	{
		ArIsSaving = ArIsPersistent = 1;
	}
	FArchive& operator<<( UObject*& Obj )
	{
		if( Obj && !Obj->IsPendingKill() )
		{
			if( !(Obj->GetFlags() & RF_Transient) || (Obj->GetFlags() & RF_Public) )
			{
				Linker->ObjectIndices(Obj->GetIndex())++;
				if( !(Obj->GetFlags() & RF_TagExp ) )
				{
					Obj->SetFlags( RF_TagImp );
					if( !(Obj->GetFlags() & RF_NotForEdit  ) ) Obj->SetFlags(RF_LoadForEdit);
					if( !(Obj->GetFlags() & RF_NotForClient) ) Obj->SetFlags(RF_LoadForClient);
					if( !(Obj->GetFlags() & RF_NotForServer) ) Obj->SetFlags(RF_LoadForServer);
					UObject* Parent = Obj->GetOuter();
					if( Parent )
						*this << Parent;
				}
			}
		}
		return *this;
	}
	FArchive& operator<<( FName& Name )
	{
		Name.SetFlags( RF_TagExp | ContextFlags );
		Linker->NameIndices(Name.GetIndex())++;
		return *this;
	}
	ULinkerSave* Linker;
};

//
// Save one specific object into an Unrealfile.
//
UBOOL UObject::SavePackage( UObject* InOuter, UObject* Base, DWORD TopLevelFlags, const TCHAR* Filename, FOutputDevice* Error, ULinkerLoad* Conform )
{
	check(InOuter);
	check(Filename);
	DWORD Time=0; clock(Time);

	// Tag parent flags.
	//!!should be user-controllable in UnrealEd.
	if( Cast<UPackage>( InOuter ) )
		Cast<UPackage>( InOuter )->PackageFlags |= PKG_AllowDownload;

	// Make temp file.
	TCHAR TempFilename[256];
	appStrncpy( TempFilename, Filename, ARRAY_COUNT(TempFilename) );
	INT c = appStrlen(TempFilename);
	while( c>0 && TempFilename[c-1]!=PATH_SEPARATOR[0] && TempFilename[c-1]!='/' && TempFilename[c-1]!=':' )
		c--;
	TempFilename[c]=0;
	appStrcat( TempFilename, TEXT("Save.tmp") );

	// Init.
	GWarn->StatusUpdatef( 0, 0, *LocalizeProgress(TEXT("Saving"),TEXT("Core")), Filename );
	UBOOL Success = 0;

	// If we have a loader for the package, unload it to prevent conflicts.
	ResetLoaders( InOuter, 0, 1 );

	// Untag all objects and names.
	for( FObjectIterator It; It; ++It )
	{
		It->ClearFlags( RF_TagImp | RF_TagExp | RF_LoadForEdit | RF_LoadForClient | RF_LoadForServer );
		// if the object class has been marked as deprecated, mark this
		// object as transient so that it isn't serialized
		if (It->GetClass()->ClassFlags & CLASS_Deprecated)
		{
			It->SetFlags(RF_Transient);
		}
	}
	for( INT i=0; i<FName::GetMaxNames(); i++ )
		if( FName::GetEntry(i) )
			FName::GetEntry(i)->Flags &= ~(RF_TagImp | RF_TagExp | RF_LoadForEdit | RF_LoadForClient | RF_LoadForServer);
	
	// Export objects.	
	FArchiveSaveTagExports Ar( InOuter );
	if( Base )
	{
		Base->PreSave();
		Ar << Base;
	}
	for( FObjectIterator It; It; ++It )
	{
		if( (It->GetFlags() & TopLevelFlags) && It->IsIn(InOuter) )
		{
			UObject* Obj = *It;
			Obj->PreSave();
			Ar << Obj;
		}
	}
	ULinkerSave* Linker = NULL;
	try
	{
		// Allocate the linker.
		Linker = new ULinkerSave( InOuter, TempFilename );

		// Import objects and names.
		for( FObjectIterator It; It; ++It )
		{
			if( It->GetFlags() & RF_TagExp )
			{
				// Build list.
				FArchiveSaveTagImports Ar( Linker, It->GetFlags() & RF_LoadContextFlags );
				It->Serialize( Ar );
				UClass* Class = It->GetClass();
				Ar << Class;
				if( It->IsIn(GetTransientPackage()) )
					appErrorf( *LocalizeError(TEXT("TransientImport"),TEXT("Core")), *It->GetFullName() );
			}
		}

		// Export all relevant object, class, and package names.
		for( FObjectIterator It; It; ++It )
		{
			if( It->GetFlags() & (RF_TagExp|RF_TagImp) )
			{
				It->GetFName().SetFlags( RF_TagExp | RF_LoadForEdit | RF_LoadForClient | RF_LoadForServer );
				if( It->GetOuter() )
					It->GetOuter()->GetFName().SetFlags( RF_TagExp | RF_LoadForEdit | RF_LoadForClient | RF_LoadForServer );
				if( It->GetFlags() & RF_TagImp )
				{
					It->Class->GetFName().SetFlags( RF_TagExp | RF_LoadForEdit | RF_LoadForClient | RF_LoadForServer );
					check(It->Class->GetOuter());
					It->Class->GetOuter()->GetFName().SetFlags( RF_TagExp | RF_LoadForEdit | RF_LoadForClient | RF_LoadForServer );
					if( !(It->GetFlags() & RF_Public) )
					{
						warnf( TEXT("Referencers of %s:"), *It->GetFullName() );
						for( FObjectIterator RefIt; RefIt; ++RefIt )
						{
							FArchiveFindCulprit ArFind(*It,*RefIt);
							if( ArFind.GetCount() )
								warnf( TEXT("   %s"), *RefIt->GetFullName() );
						}
						appThrowf( *LocalizeError(TEXT("FailedSavePrivate"),TEXT("Core")), Filename, *It->GetFullName() );
					}
				}
				else debugfSlow( NAME_DevSave, TEXT("Saving %s"), *It->GetFullName() );
			}
		}

		// Write fixed-length file summary to overwrite later.
		if( Conform )
		{
			// Conform to previous generation of file.
			debugf( TEXT("Conformal save, relative to: %s, Generation %i"), *Conform->Filename, Conform->Summary.Generations.Num()+1 );
			Linker->Summary.Guid        = Conform->Summary.Guid;
			Linker->Summary.Generations = Conform->Summary.Generations;
		}
		else
		{
			// First generation file.
			Linker->Summary.Guid        = appCreateGuid();
			Linker->Summary.Generations = TArray<FGenerationInfo>();
		}
		new(Linker->Summary.Generations)FGenerationInfo( 0, 0 );
		*Linker << Linker->Summary; 

		// Build NameMap.
		Linker->Summary.NameOffset = Linker->Tell();
		for( INT i=0; i<FName::GetMaxNames(); i++ )
		{
			if( FName::GetEntry(i) )
			{
				FName Name( (EName)i );
				if( Name.GetFlags() & RF_TagExp )
					Linker->NameMap.AddItem( Name );
			}
		}

		// Sort names by usage count in order to maximize compression.
		GTempSave = Linker;
		INT FirstSort = 0;
		if( Conform )
		{
#if DO_GUARD_SLOW
			TArray<FName> Orig = Linker->NameMap;
#endif
			for( INT i=0; i<Conform->NameMap.Num(); i++ )
			{
				INT Index = Linker->NameMap.FindItemIndex(Conform->NameMap(i));
				if( Conform->NameMap(i)==NAME_None || Index==INDEX_NONE )
				{
					Linker->NameMap.Add();
					Linker->NameMap.Last() = Linker->NameMap(i);
				}
				else Exchange( Linker->NameMap(i), Linker->NameMap(Index) );
				Linker->NameMap(i) = Conform->NameMap(i);
			}
#if DO_GUARD_SLOW
			for( INT i=0; i<Conform->NameMap.Num(); i++ )
				check(Linker->NameMap(i)==Conform->NameMap(i));
			for( INT i=0; i<Orig.Num(); i++ )
				check(Linker->NameMap.FindItemIndex(Orig(i))!=INDEX_NONE);
#endif
			FirstSort = Conform->NameMap.Num();
		}
		appQsort( &Linker->NameMap(FirstSort), Linker->NameMap.Num()-FirstSort, sizeof(Linker->NameMap(0)), LinkerNameSort );
		
		// Save names.
		Linker->Summary.NameCount = Linker->NameMap.Num();
		for( INT i=0; i<Linker->NameMap.Num(); i++ )
		{
			*Linker << *FName::GetEntry( Linker->NameMap(i).GetIndex() );
			Linker->NameIndices(Linker->NameMap(i).GetIndex()) = i;
		}

		// Build ImportMap.
		for( FObjectIterator It; It; ++It )
			if( It->GetFlags() & RF_TagImp )
				new( Linker->ImportMap )FObjectImport( *It );
		Linker->Summary.ImportCount = Linker->ImportMap.Num();
		
		// Sort imports by usage count.
		GTempSave = Linker;
		appQsort( &Linker->ImportMap(0), Linker->ImportMap.Num(), sizeof(Linker->ImportMap(0)), LinkerImportSort );
		
		// Build ExportMap.
		for( FObjectIterator It; It; ++It )
			if( It->GetFlags() & RF_TagExp )
				new( Linker->ExportMap )FObjectExport( *It );
		
		// Sort exports by usage count.
		FirstSort = 0;
		if( Conform )
		{
			TArray<FObjectExport> Orig = Linker->ExportMap;
			Linker->ExportMap.Empty( Linker->ExportMap.Num() );
			TArray<BYTE> Used; Used.AddZeroed( Orig.Num() );
			TMap<FString,INT> Map;
			{for( INT i=0; i<Orig.Num(); i++ )
				Map.Set( *Orig(i)._Object->GetFullName(), i );}
			{for( INT i=0; i<Conform->ExportMap.Num(); i++ )
			{
				INT* Find = Map.Find( *Conform->GetExportFullName(i,*Linker->LinkerRoot->GetPathName()) );
				if( Find )
				{
					new(Linker->ExportMap)FObjectExport( Orig(*Find) );
					check(Linker->ExportMap.Last()._Object == Orig(*Find)._Object);
					Used( *Find ) = 1;
				}
				else new(Linker->ExportMap)FObjectExport( NULL );
			}}
			FirstSort = Conform->ExportMap.Num();
			{for( INT i=0; i<Used.Num(); i++ )
				if( !Used(i) )
					new(Linker->ExportMap)FObjectExport( Orig(i) );}
#if DO_GUARD_SLOW
			{for( INT i=0; i<Orig.Num(); i++ )
			{
				INT j = 0;
				for( j=0; j<Linker->ExportMap.Num(); j++ )
					if( Linker->ExportMap(j)._Object == Orig(i)._Object )
						break;
				check(j<Linker->ExportMap.Num());
			}}
#endif
		}
		appQsort( &Linker->ExportMap(FirstSort), Linker->ExportMap.Num()-FirstSort, sizeof(Linker->ExportMap(0)), LinkerExportSort );
		Linker->Summary.ExportCount = Linker->ExportMap.Num();
		
		// Set linker reverse mappings.
		{for( INT i=0; i<Linker->ExportMap.Num(); i++ )
			if( Linker->ExportMap(i)._Object )
				Linker->ObjectIndices(Linker->ExportMap(i)._Object->GetIndex()) = i+1;}
		{for( INT i=0; i<Linker->ImportMap.Num(); i++ )
			Linker->ObjectIndices(Linker->ImportMap(i).XObject->GetIndex()) = -i-1;}
		
		// Find components referenced by exports.
		for( INT i=0; i<Linker->ExportMap.Num(); i++ )
		{
			FObjectExport&	Export = Linker->ExportMap(i);

			if(Export._Object)
			{
				TMap<FName,UComponent*>	InstanceMap;
				Export._Object->GetClass()->FindInstancedComponents(InstanceMap,(BYTE*)Export._Object,Export._Object);

				for(TMap<FName,UComponent*>::TIterator It(InstanceMap);It;++It)
				{
					UComponent*	Component = It.Value();
					INT			ExportIndex = Linker->ObjectIndices(Component->GetIndex());
					check(Linker->ObjectIndices(Component->GetIndex()) > 0);
					Export.ComponentMap.Set(It.Key(),Linker->ObjectIndices(It.Value()->GetIndex()) - 1);
				}
			}
		}

		// Save exports.
		for( INT i=0; i<Linker->ExportMap.Num(); i++ )
		{
			FObjectExport& Export = Linker->ExportMap(i);
			if( Export._Object )
			{
				// Set class index.
				if( !Export._Object->IsA(UClass::StaticClass()) )
				{
					Export.ClassIndex = Linker->ObjectIndices(Export._Object->GetClass()->GetIndex());
					check(Export.ClassIndex!=0);
				}
				if( Export._Object->IsA(UStruct::StaticClass()) )
				{
					UStruct* Struct = (UStruct*)Export._Object;
					if( Struct->SuperField )
					{
						Export.SuperIndex = Linker->ObjectIndices(Struct->SuperField->GetIndex());
						check(Export.SuperIndex!=0);
					}
				}

				// Set package index.
				if( Export._Object->GetOuter() != InOuter )
				{
					check(Export._Object->GetOuter()->IsIn(InOuter));
					Export.PackageIndex = Linker->ObjectIndices(Export._Object->GetOuter()->GetIndex());
					check(Export.PackageIndex>0);
				}

				// Save it.
				Export.SerialOffset = Linker->Tell();
				Export._Object->Serialize( *Linker );
				Export.SerialSize = Linker->Tell() - Linker->ExportMap(i).SerialOffset;
			}
		}

		// Save the import map.
		Linker->Summary.ImportOffset = Linker->Tell();
		for( INT i=0; i<Linker->ImportMap.Num(); i++ )
		{
			FObjectImport& Import = Linker->ImportMap( i );

			// Set the package index.
			if( Import.XObject->GetOuter() )
			{
				if (Import.XObject->GetOuter()->IsIn(InOuter) )
					warnf(TEXT("Bad Object=%s"),*Import.XObject->GetFullName());

				check(!Import.XObject->GetOuter()->IsIn(InOuter));
				Import.PackageIndex = Linker->ObjectIndices(Import.XObject->GetOuter()->GetIndex());
				check(Import.PackageIndex<0);
			}

			// Save it.
			*Linker << Import;
		}

		// Save the export map.
		Linker->Summary.ExportOffset = Linker->Tell();
		for( INT i=0; i<Linker->ExportMap.Num(); i++ )
			*Linker << Linker->ExportMap( i );

		// Rewrite updated file summary.
		GWarn->StatusUpdatef( 0, 0, *LocalizeProgress(TEXT("Closing"),TEXT("Core")) );
		Linker->Summary.Generations.Last().ExportCount = Linker->Summary.ExportCount;
		Linker->Summary.Generations.Last().NameCount   = Linker->Summary.NameCount;
		Linker->Seek(0);
		*Linker << Linker->Summary;
		Success = 1;
	}
	catch( const TCHAR* Msg )
	{
		// Delete the temporary file.
		GFileManager->Delete( TempFilename );
		Error->Logf( NAME_Warning, TEXT("%s"), Msg );
	}
	if( Linker )
	{
		delete Linker;
	}
	unclock(Time);
	debugf( NAME_Log, TEXT("Save=%f"), GSecondsPerCycle*1000*Time );
	if( Success )
	{
		// Move the temporary file.
		debugf( NAME_Log, TEXT("Moving '%s' to '%s'"), TempFilename, Filename );
		if( !GFileManager->Move( Filename, TempFilename ) )
		{
			GFileManager->Delete( TempFilename );
			warnf( *LocalizeError(TEXT("SaveWarning"),TEXT("Core")), Filename );
			Success = 0;
		}

		// Update package file cache
		GPackageFileCache->CachePackage( Filename, 0, !GIsUCC );
	}
	return Success;
}

/*-----------------------------------------------------------------------------
	Misc.
-----------------------------------------------------------------------------*/

//
// Return whether statics are initialized.
//
UBOOL UObject::GetInitialized()
{
	return UObject::GObjInitialized;
}

//
// Return the static transient package.
//
UPackage* UObject::GetTransientPackage()
{
	return UObject::GObjTransientPkg;
}

//
// Return the ith loader.
//
ULinkerLoad* UObject::GetLoader( INT i )
{
	return (ULinkerLoad*)GObjLoaders(i);
}

//
// Add an object to the root array. This prevents the object and all
// its descendents from being deleted during garbage collection.
//
void UObject::AddToRoot()
{
	GObjRoot.AddItem( this );
}

//
// Remove an object from the root array.
//
void UObject::RemoveFromRoot()
{
	GObjRoot.RemoveItem( this );
}

/*-----------------------------------------------------------------------------
	Object name functions.
-----------------------------------------------------------------------------*/

//
// Create a unique name by combining a base name and an arbitrary number string.  
// The object name returned is guaranteed not to exist.
//
FName UObject::MakeUniqueObjectName( UObject* Parent, UClass* Class )
{
	check(Class);
	
	TCHAR NewBase[NAME_SIZE], Result[NAME_SIZE];
	TCHAR TempIntStr[NAME_SIZE];

	// Make base name sans appended numbers.
	appStrcpy( NewBase, Class->GetName() );
	TCHAR* End = NewBase + appStrlen(NewBase);
	while( End>NewBase && (appIsDigit(End[-1]) || End[-1] == TEXT('_')) )
		End--;
	*End = 0;

	// Append numbers to base name.
	do
	{
		appSprintf( TempIntStr, TEXT("_%i"), Class->ClassUnique++ );
		appStrncpy( Result, NewBase, NAME_SIZE-appStrlen(TempIntStr)-1 );
		appStrcat( Result, TempIntStr );
	} while( StaticFindObject( NULL, Parent, Result ) );

	return Result;
}

/*-----------------------------------------------------------------------------
	Object hashing.
-----------------------------------------------------------------------------*/

//
// Add an object to the hash table.
//
void UObject::HashObject()
{
	INT iHash       = GetObjectHash( Name, Outer ? Outer->GetIndex() : 0 );
	HashNext        = GObjHash[iHash];
	GObjHash[iHash] = this;
}

//
// Remove an object from the hash table.
//
void UObject::UnhashObject( INT OuterIndex )
{
	INT       iHash   = GetObjectHash( Name, OuterIndex );
	UObject** Hash    = &GObjHash[iHash];
	INT       Removed = 0;
	while( *Hash != NULL )
	{
		if( *Hash != this )
		{
			Hash = &(*Hash)->HashNext;
 		}
		else
		{
			*Hash = (*Hash)->HashNext;
			Removed++;
		}
	}
	check(Removed!=0);
	check(Removed==1);
}

/*-----------------------------------------------------------------------------
	Creating and allocating data for new objects.
-----------------------------------------------------------------------------*/

//
// Add an object to the table.
//
void UObject::AddObject( INT InIndex )
{
	// Find an available index.
	if( InIndex==INDEX_NONE )
	{
		if( GObjAvailable.Num() )
		{
			InIndex = GObjAvailable.Pop();
			check(GObjObjects(InIndex)==NULL);
		}
		else InIndex = GObjObjects.Add();
	}

	// Add to global table.
	GObjObjects(InIndex) = this;
	Index = InIndex;
	HashObject();
}

//
// Create a new object or replace an existing one.
// If Name is NAME_None, tries to create an object with an arbitrary unique name.
// No failure conditions.
//
UObject* UObject::StaticAllocateObject
(
	UClass*			InClass,
	UObject*		InOuter,
	FName			InName,
	DWORD			InFlags,
	UObject*		InTemplate,
	FOutputDevice*	Error,
	UObject*		Ptr,
	UObject*		SuperObject
)
{
	check(Error);
	check(!InClass || InClass->ClassWithin);
	check(!InClass || InClass->ClassConstructor);

	// Validation checks.
	if( !InClass )
	{
		Error->Logf( TEXT("Empty class for object %s"), *InName );
		return NULL;
	}
	if( InClass->GetIndex()==INDEX_NONE && GObjRegisterCount==0 )
	{
		Error->Logf( TEXT("Unregistered class for %s"), *InName );
		return NULL;
	}
	if( InClass->ClassFlags & CLASS_Abstract )
	{
		Error->Logf( *LocalizeError(TEXT("Abstract"),TEXT("Core")), *InName, InClass->GetName() );
		return NULL;
	}
	if( !InOuter && InClass!=UPackage::StaticClass() )
	{
		Error->Logf( *LocalizeError(TEXT("NotPackaged"),TEXT("Core")), InClass->GetName(), *InName );
		return NULL;
	}
	if( InOuter && !InOuter->IsA(InClass->ClassWithin) )
	{
		Error->Logf( *LocalizeError(TEXT("NotWithin"),TEXT("Core")), InClass->GetName(), *InName, InOuter->GetClass()->GetName(), InClass->ClassWithin->GetName() );
		return NULL;
	}

	// Compose name, if public and unnamed.
	//if( InName==NAME_None && (InFlags&RF_Public) )
	if( InName==NAME_None )//oldver?? what about compatibility matching of unnamed objects?
		InName = MakeUniqueObjectName( InOuter, InClass );

	// See if object already exists.
	UObject* Obj                        = StaticFindObject( InClass, InOuter, *InName );//oldver: Should use NULL instead of InClass to prevent conflicts by name rather than name-class.
	UClass*  Cls                        = Cast<UClass>( Obj );
	INT      Index                      = INDEX_NONE;
	UClass*  ClassWithin				= NULL;
	DWORD    ClassFlags                 = 0;
	void     (*ClassConstructor)(void*) = NULL;
	if( !Obj )
	{
		// Create a new object.
		Obj = Ptr ? Ptr : (UObject*)appMalloc( Align(InClass->GetPropertiesSize(),InClass->GetMinAlignment()) );
	}
	else
	{
		// Replace an existing object without affecting the original's address or index.
		check(!Ptr || Ptr==Obj);
		debugfSlow( NAME_DevReplace, TEXT("Replacing %s"), Obj->GetName() );

		// Can only replace if class is identical.
		if( Obj->GetClass() != InClass )
			appErrorf( *LocalizeError(TEXT("NoReplace"),TEXT("Core")), *Obj->GetFullName(), InClass->GetName() );

		// Remember flags, index, and native class info.
		InFlags |= (Obj->GetFlags() & RF_Keep);
		Index    = Obj->Index;
		if( Cls )
		{
			ClassWithin		 = Cls->ClassWithin;
			ClassFlags       = Cls->ClassFlags & CLASS_Abstract;
			ClassConstructor = Cls->ClassConstructor;
		}

		// Destroy the object.
		Obj->~UObject();
		check(GObjAvailable.Num() && GObjAvailable.Last()==Index);
		GObjAvailable.Pop();
	}

	// If class is transient, objects must be transient.
	if( InClass->ClassFlags & CLASS_Transient )
		InFlags |= RF_Transient;

	// Set the base properties.
	Obj->Index			 = INDEX_NONE;
	Obj->HashNext		 = NULL;
	Obj->StateFrame      = NULL;
	Obj->_Linker		 = NULL;
	Obj->_LinkerIndex	 = INDEX_NONE;
	Obj->Outer			 = InOuter;
	Obj->ObjectFlags	 = InFlags;
	Obj->Name			 = InName;
	Obj->Class			 = InClass;

	// Init the properties.
	InitProperties( (BYTE*)Obj, InClass->GetPropertiesSize(), InClass, (BYTE*)InTemplate, InClass->GetPropertiesSize(), (InFlags&RF_NeedLoad) ? NULL : Obj, SuperObject );

	// Add to global table.
	Obj->AddObject( Index );
	check(Obj->IsValid());

	// If config is per-object, load it.
	if( InClass->ClassFlags & CLASS_PerObjectConfig )
	{
		Obj->LoadConfig();
		Obj->LoadLocalized();
	}

	// Restore class information if replacing native class.
	if( Cls )
	{
		Cls->ClassWithin	   = ClassWithin;
		Cls->ClassFlags       |= ClassFlags;
		Cls->ClassConstructor  = ClassConstructor;
	}

	// Success.
	return Obj;
}

//
// Construct an object.
//
UObject* UObject::StaticConstructObject
(
	UClass*			InClass,
	UObject*		InOuter,
	FName			InName,
	DWORD			InFlags,
	UObject*		InTemplate,
	FOutputDevice*	Error,
	UObject*		SuperObject
)
{
	check(Error);

	// Allocate the object.
	UObject* Result = StaticAllocateObject( InClass, InOuter, InName, InFlags, InTemplate, Error, NULL, SuperObject );

	if( Result )
	{
		(*InClass->ClassConstructor)( Result );

		if( !(InFlags & RF_NeedLoad) && (InClass->ClassFlags&CLASS_HasComponents) )
		{
			TMap<FName,UComponent*> InstanceMap;
			InClass->InstanceComponents(InstanceMap,(BYTE*)Result,Result);
			InClass->FixupComponentReferences(InstanceMap,(BYTE*)Result,Result);
		}
	}
	return Result;
}

/*-----------------------------------------------------------------------------
   Duplicating Objects.
-----------------------------------------------------------------------------*/

//
//	FDuplicatedObjectInfo
//

struct FDuplicatedObjectInfo
{
	UObject*				DupObject;
	TMap<FName,UComponent*>	ComponentInstanceMap;
};

//
//	FDuplicateDataReader - Reads duplicated objects from a memory buffer, replacing object references to duplicated objects.
//

class FDuplicateDataReader : public FArchive
{
private:

	const TMap<UObject*,FDuplicatedObjectInfo>&	DuplicatedObjects;
	const TArray<BYTE>&							ObjectData;
	INT											Offset;

	// FArchive interface.

	virtual FArchive& operator<<(FName& N)
	{
		Serialize(&N,sizeof(FName));
		return *this;
	}

	virtual FArchive& operator<<(UObject*& Object)
	{
		UObject*	SourceObject = Object;
		Serialize(&SourceObject,sizeof(UObject*));
		const FDuplicatedObjectInfo*	ObjectInfo = DuplicatedObjects.Find(SourceObject);
		if(ObjectInfo)
			Object = ObjectInfo->DupObject;
		else
			Object = SourceObject;
		return *this;
	}

	virtual void Serialize(void* Data,INT Num)
	{
		check(Offset + Num <= ObjectData.Num());
		appMemcpy(Data,&ObjectData(Offset),Num);
		Offset += Num;
	}

	virtual void Seek(INT InPos)
	{
		Offset = InPos;
	}

	virtual INT Tell()
	{
		return Offset;
	}

public:

	// Constructor.

	FDuplicateDataReader(const TMap<UObject*,FDuplicatedObjectInfo>& InDuplicatedObjects,const TArray<BYTE>& InObjectData):
		DuplicatedObjects(InDuplicatedObjects),
		ObjectData(InObjectData),
		Offset(0)
	{
		ArIsLoading = ArIsPersistent = 1;
	}
};

//
//	FDuplicateDataWriter - Writes duplicated objects to a memory buffer, duplicating referenced inner objects and adding the duplicates to the DuplicatedObjects map.
//

class FDuplicateDataWriter : public FArchive
{
private:

	TMap<UObject*,FDuplicatedObjectInfo>&	DuplicatedObjects;
	TArray<BYTE>&							ObjectData;
	INT										Offset;
	DWORD									FlagMask;

	// FArchive interface.

	virtual FArchive& operator<<(FName& N)
	{
		Serialize(&N,sizeof(FName));
		return *this;
	}

	virtual FArchive& operator<<(UObject*& Object);

	virtual void Serialize(void* Data,INT Num)
	{
		if(Offset == ObjectData.Num())
			ObjectData.Add(Num);
		appMemcpy(&ObjectData(Offset),Data,Num);
		Offset += Num;
	}

	virtual void Seek(INT InPos)
	{
		Offset = InPos;
	}

	virtual INT Tell()
	{
		return Offset;
	}

	// AddDuplicate - Places a new duplicate in the DuplicatedObjects map as well as the UnserializedObjects list

	UObject* AddDuplicate(UObject* Object,UObject* DupObject)
	{
		FDuplicatedObjectInfo	Info;
		Info.DupObject = DupObject;

		DuplicatedObjects.Set(Object,Info);

		TMap<FName,UComponent*>	ComponentInstanceMap;
		Object->GetClass()->FindInstancedComponents(ComponentInstanceMap,(BYTE*)Object,Object);

		for(TMap<FName,UComponent*>::TIterator It(ComponentInstanceMap);It;++It)
		{
			UObject*	DuplicatedComponent = GetDuplicatedObject(It.Value());
			if(DuplicatedComponent)
				Info.ComponentInstanceMap.Set(It.Key(),CastChecked<UComponent>(DuplicatedComponent));
			else
				Info.ComponentInstanceMap.Set(It.Key(),It.Value());
		}

		DuplicatedObjects.Set(Object,Info);
		UnserializedObjects.AddItem(Object);

		return DupObject;
	}

public:

	TArray<UObject*>	UnserializedObjects;

	// GetDuplicatedObject - Returns a pointer to the duplicate of a given object, creating the duplicate object if necessary.

	UObject* GetDuplicatedObject(UObject* Object)
	{
		if(!Object)
			return NULL;

		// Check for an existing duplicate of the object.

		FDuplicatedObjectInfo*	DupObjectInfo = DuplicatedObjects.Find(Object);
		if(DupObjectInfo)
			return DupObjectInfo->DupObject;

		// Check to see if the object's outer is being duplicated.

		UObject*	DupOuter = GetDuplicatedObject(Object->GetOuter());
		if(DupOuter)
		{
			// The object's outer is being duplicated, create a duplicate of this object.

			return AddDuplicate(Object,UObject::StaticConstructObject(Object->GetClass(),DupOuter,Object->GetName(),Object->GetFlags() & FlagMask));
		}

		return NULL;
	}

	// Constructor.

	FDuplicateDataWriter(TMap<UObject*,FDuplicatedObjectInfo>& InDuplicatedObjects,TArray<BYTE>& InObjectData,UObject* SourceObject,UObject* DestObject,DWORD InFlagMask):
		DuplicatedObjects(InDuplicatedObjects),
		ObjectData(InObjectData),
		Offset(0),
		FlagMask(InFlagMask)
	{
		ArIsSaving = ArIsPersistent = 1;

		AddDuplicate(SourceObject,DestObject);
	}
};

//
//	FDuplicateDataWriter::operator<<
//

FArchive& FDuplicateDataWriter::operator<<(UObject*& Object)
{
	GetDuplicatedObject(Object);
	Serialize(&Object,sizeof(UObject*));
	return *this;
}


/**
 * Copies SourceOuter to DestOuter with a name of DestName.  If RootObject is different from SourceOuter, it is also copied.
 * Any objects referenced by SourceOuter or RootObject and contained by SourceOuter are also copied, maintaining their name
 * relative to SourceOuter.  RootObject must equal SourceOuter or be contained by SourceOuter.
 * The copied objects have only the flags in FlagMask copied from their source object.
 * Returns the copy of RootObject.
 */
UObject* UObject::StaticDuplicateObject(UObject* SourceObject,UObject* RootObject,UObject* DestOuter,const TCHAR* DestName,DWORD FlagMask)
{
	GUglyHackFlags |= 8; // Ensures that TLazyArray doesn't use lazy loading.

	UObject*								DupObject = UObject::StaticConstructObject(SourceObject->GetClass(),DestOuter,DestName,SourceObject->GetFlags() & FlagMask);
	TArray<BYTE>							ObjectData;
	TMap<UObject*,FDuplicatedObjectInfo>	DuplicatedObjects;
	FDuplicateDataWriter					Writer(DuplicatedObjects,ObjectData,SourceObject,DupObject,FlagMask);
	TArray<UObject*>						SerializedObjects;
	UObject*								DupRoot = Writer.GetDuplicatedObject(RootObject);

	while(Writer.UnserializedObjects.Num())
	{
		UObject*	Object = Writer.UnserializedObjects.Pop();
		Object->Serialize(Writer);
		SerializedObjects.AddItem(Object);
	};

	FDuplicateDataReader	Reader(DuplicatedObjects,ObjectData);

	for(INT ObjectIndex = 0;ObjectIndex < SerializedObjects.Num();ObjectIndex++)
	{
		FDuplicatedObjectInfo*	ObjectInfo = DuplicatedObjects.Find(SerializedObjects(ObjectIndex));
		check(ObjectInfo);
		ObjectInfo->DupObject->Serialize(Reader);
	}

	for(TMap<UObject*,FDuplicatedObjectInfo>::TIterator It(DuplicatedObjects);It;++It)
	{
		FDuplicatedObjectInfo&	DupObjectInfo = It.Value();
		DupObjectInfo.DupObject->GetClass()->InstanceComponents(DupObjectInfo.ComponentInstanceMap,(BYTE*)DupObjectInfo.DupObject,DupObjectInfo.DupObject);
		DupObjectInfo.DupObject->GetClass()->FixupComponentReferences(DupObjectInfo.ComponentInstanceMap,(BYTE*)DupObjectInfo.DupObject,DupObjectInfo.DupObject);
		DupObjectInfo.DupObject->PostLoad();
	}

	GUglyHackFlags &= ~8;

	return DupRoot;
}

/*-----------------------------------------------------------------------------
   Garbage collection.
-----------------------------------------------------------------------------*/

//
// Serialize the global root set to an archive.
//
void UObject::SerializeRootSet( FArchive& Ar, DWORD KeepFlags, DWORD RequiredFlags )
{
	Ar << GObjRoot;
	for( FObjectIterator It; It; ++It )
	{
		if
		(	(It->GetFlags() & KeepFlags)
		&&	(It->GetFlags()&RequiredFlags)==RequiredFlags )
		{
			UObject* Obj = *It;
			Ar << Obj;
		}
	}
}

//
// Archive for finding unused objects.
//
class FArchiveTagUsed : public FArchive
{
public:
	FArchiveTagUsed()
	: Context( NULL )
	{
		GGarbageRefCount=0;

		// Tag all objects as unreachable.
		for( FObjectIterator It; It; ++It )
			It->SetFlags( RF_Unreachable | RF_TagGarbage );

		// Tag all names as unreachable.
		for( INT i=0; i<FName::GetMaxNames(); i++ )
			if( FName::GetEntry(i) )
				FName::GetEntry(i)->Flags |= RF_Unreachable;
	}
private:
	FArchive& operator<<( UObject*& Object )
	{
		GGarbageRefCount++;

		// Object could be a misaligned pointer.
		// Copy the contents of the pointer into a temporary and work on that.
		UObject* Obj;
		appMemcpy(&Obj, &Object, sizeof(INT));
		
#if DO_GUARD_SLOW
		if( Obj )
			check(Obj->IsValid());
#endif
		if( Obj && (Obj->GetFlags() & RF_EliminateObject) )
		{
			// Dereference it.
			Obj = NULL;
		}
		else if( Obj && (Obj->GetFlags() & RF_Unreachable) )
		{
			// Only recurse the first time object is claimed.
			Obj->ClearFlags( RF_Unreachable | RF_DebugSerialize );

			// Recurse.
			if( Obj->GetFlags() & RF_TagGarbage )
			{
				// Recurse down the object graph.
				UObject* OriginalContext = Context;
				Context = Obj;
				Obj->Serialize( *this );
				if( !(Obj->GetFlags() & RF_DebugSerialize) )
					appErrorf( TEXT("%s failed to route Serialize"), *Obj->GetFullName() );
				Context = OriginalContext;
			}
			else
			{
				// For debugging.
				debugfSlow( NAME_Log, TEXT("%s is referenced by %s"), *Obj->GetFullName(), Context ? *Context->GetFullName() : NULL );
			}
		}

		// Contents of pointer might have been modified.
		// Copy the results back into the misaligned pointer.
		appMemcpy(&Object, &Obj, sizeof(INT));
		return *this;
	}
	FArchive& operator<<( FName& Name )
	{
		Name.ClearFlags( RF_Unreachable );
		return *this;
	}
	UObject* Context;
};

//
// Purge garbage.
//
void UObject::PurgeGarbage()
{
	INT CountBefore=0, CountPurged=0;
	debugf( NAME_Log, TEXT("Purging garbage") );

	GIsGarbageCollecting = 1; // Set 'I'm garbage collecting' flag - might be checked inside UObject::Destropy etc.

	// Notify script debugger to clear its stack, since all FFrames will be destroyed.
	if ( GDebugger )
		GDebugger->NotifyGC();

	// Dispatch all Destroy messages.
	for( INT i=0; i<GObjObjects.Num(); i++ )
	{
		CountBefore += (GObjObjects(i)!=NULL);
		if
		(	GObjObjects(i)
		&&	(GObjObjects(i)->GetFlags() & RF_Unreachable)
		&&  (!(GObjObjects(i)->GetFlags() & RF_Native) || GExitPurge) )
		{
			debugfSlow( NAME_DevGarbage, TEXT("Garbage collected object %i: %s"), i, *GObjObjects(i)->GetFullName() );
			GObjObjects(i)->ConditionalDestroy();
			CountPurged++;
		}
	}

	// Purge all unreachable objects.
	//warning: Can't use FObjectIterator here because classes may be destroyed before objects.
	FName DeleteName=NAME_None;
	for( INT i=0; i<GObjObjects.Num(); i++ )
	{
		if
		(	GObjObjects(i)
		&&	(GObjObjects(i)->GetFlags() & RF_Unreachable) 
		&& !(GObjObjects(i)->GetFlags() & RF_Native) )
		{
			DeleteName = GObjObjects(i)->GetFName();
			delete GObjObjects(i);
		}
	}
	GIsGarbageCollecting = 0; // Unset flag

	// Purge all unreachable names.
	for( INT i=0; i<FName::GetMaxNames(); i++ )
	{
		FNameEntry* Name = FName::GetEntry(i);
		if
		(	(Name)
		&&	(Name->Flags & RF_Unreachable)
		&& !(Name->Flags & RF_Native     ) )
		{
			debugfSlow( NAME_DevGarbage, TEXT("Garbage collected name %i: %s"), i, Name->Name );
			FName::DeleteEntry(i);
		}
	}
	debugf(TEXT("Garbage: objects: %i->%i; refs: %i"),CountBefore,CountBefore-CountPurged,GGarbageRefCount);
}

//
// Delete all unreferenced objects.
//
void UObject::CollectGarbage( DWORD KeepFlags )
{
	debugf( NAME_Log, TEXT("Collecting garbage") );

	// Tag and purge garbage.
	FArchiveTagUsed TagUsedAr;
	SerializeRootSet( TagUsedAr, KeepFlags, RF_TagGarbage );

	// Purge it.
	PurgeGarbage();
}

//
// Returns whether an object is referenced, not counting the
// one reference at Obj. No side effects.
//
UBOOL UObject::IsReferenced( UObject*& Obj, DWORD KeepFlags, UBOOL IgnoreReference )
{
	// Remember it.
	UObject* OriginalObj = Obj;
	if( IgnoreReference )
		Obj = NULL;

	// Tag all garbage.
	FArchiveTagUsed TagUsedAr;
	OriginalObj->ClearFlags( RF_TagGarbage );
	SerializeRootSet( TagUsedAr, KeepFlags, RF_TagGarbage );

	// Stick the reference back.
	Obj = OriginalObj;

	// Return whether this is tagged.
	return (Obj->GetFlags() & RF_Unreachable)==0;
}

//
// Attempt to delete an object. Only succeeds if unreferenced.
//
UBOOL UObject::AttemptDelete( UObject*& Obj, DWORD KeepFlags, UBOOL IgnoreReference )
{
	if( !(Obj->GetFlags() & RF_Native) && !IsReferenced( Obj, KeepFlags, IgnoreReference ) )
	{
		PurgeGarbage();
		return 1;
	}
	else return 0;
}

/*-----------------------------------------------------------------------------
	Importing and exporting.
-----------------------------------------------------------------------------*/

//
// Import an object from a file.
//
void UObject::ParseParms( const TCHAR* Parms )
{
	if( !Parms )
		return;
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(GetClass()); It; ++It )
	{
		if( It->GetOuter()!=UObject::StaticClass() )
		{
			FString Value;
			if( Parse(Parms,*(FString(It->GetName())+TEXT("=")),Value) )
				It->ImportText( *Value, (BYTE*)this + It->Offset, PPF_Localized, this );
		}
	}
}

/*-----------------------------------------------------------------------------
	UTextBuffer implementation.
-----------------------------------------------------------------------------*/

UTextBuffer::UTextBuffer( const TCHAR* InText )
: Text( InText )
{}
void UTextBuffer::Serialize( const TCHAR* Data, EName Event )
{
	Text += (TCHAR*)Data;
}
void UTextBuffer::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	Ar << Pos << Top << Text;
}
IMPLEMENT_CLASS(UTextBuffer);

/*-----------------------------------------------------------------------------
	UEnum implementation.
-----------------------------------------------------------------------------*/

UEnum::UEnum( UEnum* InSuperEnum )
: UField( InSuperEnum )
{}
void UEnum::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	Ar << Names;
}
IMPLEMENT_CLASS(UEnum);

/*-----------------------------------------------------------------------------
	Implementations.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(ULinker);
IMPLEMENT_CLASS(ULinkerLoad);
IMPLEMENT_CLASS(ULinkerSave);
IMPLEMENT_CLASS(USubsystem);

/*-----------------------------------------------------------------------------
	UCommandlet.
-----------------------------------------------------------------------------*/

UCommandlet::UCommandlet()
: HelpCmd(E_NoInit), HelpOneLiner(E_NoInit), HelpUsage(E_NoInit), HelpWebLink(E_NoInit)
{}
INT UCommandlet::Main( const TCHAR* Parms )
{
	return eventMain( Parms );
}
void UCommandlet::execMain( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Parms);
	P_FINISH;

	*(INT*)Result = Main( *Parms );
}
IMPLEMENT_FUNCTION( UCommandlet, INDEX_NONE, execMain );
IMPLEMENT_CLASS(UCommandlet);

/*-----------------------------------------------------------------------------
	UPackage.
-----------------------------------------------------------------------------*/

UPackage::UPackage()
{
	// Bind to a matching DLL, if any.
	BindPackage( this );
	bDirty = 0;
}
void* UPackage::GetExport( const TCHAR* ExportName, UBOOL Checked )
{
	void* Result;
	Result = FindNative( (TCHAR*) ExportName );
	if( !Result && Checked )
		debugf(NAME_Warning, *LocalizeError(TEXT("NotInDll"),TEXT("Core")), ExportName, GetName() );
	if( Result )
		debugfSlow( NAME_DevBind, TEXT("Found %s in %s%s"), ExportName, GetName(), DLLEXT );		
	return Result;
}
IMPLEMENT_CLASS(UPackage);

void UObject::MarkPackageDirty( UBOOL InDirty )
{
	UPackage*	Package = Cast<UPackage>(GetOutermost());

	if(Package)
	{
		Package->bDirty = InDirty;
	}
}

/*-----------------------------------------------------------------------------
	UTextBufferFactory.
-----------------------------------------------------------------------------*/

void UTextBufferFactory::StaticConstructor()
{
	SupportedClass = UTextBuffer::StaticClass();
	bCreateNew     = 0;
	bText          = 1;
	new(Formats)FString( TEXT("txt;Text files") );
}
UTextBufferFactory::UTextBufferFactory()
{}
UObject* UTextBufferFactory::FactoryCreateText
(
	ULevel*				InLevel,
	UClass*				Class,
	UObject*			InOuter,
	FName				InName,
	DWORD				InFlags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	// Import.
	UTextBuffer* Result = new(InOuter,InName,InFlags)UTextBuffer;
	Result->Text = Buffer;
	return Result;
}
IMPLEMENT_CLASS(UTextBufferFactory);

/*----------------------------------------------------------------------------
	ULanguage.
----------------------------------------------------------------------------*/

const TCHAR* UObject::GetLanguage()
{
	return GLanguage;
}
void UObject::SetLanguage( const TCHAR* LangExt )
{
	if( appStricmp(LangExt,GLanguage)!=0 )
	{
		appStrcpy( GLanguage, LangExt );
		appStrcpy( GNone,  *LocalizeGeneral( TEXT("None"),  TEXT("Core")) );
		appStrcpy( GTrue,  *LocalizeGeneral( TEXT("True"),  TEXT("Core")) );
		appStrcpy( GFalse, *LocalizeGeneral( TEXT("False"), TEXT("Core")) );
		appStrcpy( GYes,   *LocalizeGeneral( TEXT("Yes"),   TEXT("Core")) );
		appStrcpy( GNo,    *LocalizeGeneral( TEXT("No"),    TEXT("Core")) );
		for( FObjectIterator It; It; ++It )
			It->LanguageChange();
	}
}
IMPLEMENT_CLASS(ULanguage);

/*-----------------------------------------------------------------------------
	UComponent.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UComponent);


FArchiveReplaceObjectRef::FArchiveReplaceObjectRef(UObject* InSearchObject,TArray<UObject*> *InObjectsToFind,UObject* InReplaceObjectWith)
: SearchObject(InSearchObject)
, ObjectsToFind(InObjectsToFind)
, ReplaceObjectWith(InReplaceObjectWith)
, Count(0)
{
	if (SearchObject != NULL &&
		ObjectsToFind->Num() > 0)
	{
		// start the initial serialization
		SerializedObjects.AddItem(SearchObject);
		SearchObject->Serialize(*this);
	}
}

FArchive& FArchiveReplaceObjectRef::operator<<( class UObject*& Obj )
{
	if (Obj != NULL)
	{
		// If these match, replace the reference
		if (ObjectsToFind->ContainsItem(Obj))
		{
			Obj = ReplaceObjectWith;
			Count++;
		}
		else
		if (Obj->IsIn(SearchObject) &&
			!SerializedObjects.ContainsItem(Obj))
		{
			// otherwise recurse down into the object if it is contained within the initial search object
			SerializedObjects.AddItem(Obj);
			Obj->Serialize(*this);
		}
	}
	return *this;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

