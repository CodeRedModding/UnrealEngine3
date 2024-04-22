/*=============================================================================
	UExporter.cpp: Exporter class implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Core includes.
#include "CorePrivate.h"

/*----------------------------------------------------------------------------
	UExporter.
----------------------------------------------------------------------------*/
FString UExporter::CurrentFilename(TEXT(""));

void UExporter::StaticConstructor()
{
	UArrayProperty* A = new(GetClass(),TEXT("FormatExtension"),RF_Public)UArrayProperty(CPP_PROPERTY(FormatExtension),TEXT(""),0);
	A->Inner = new(A,TEXT("StrProperty0"),RF_Public)UStrProperty;

	UArrayProperty* B = new(GetClass(),TEXT("FormatDescription"),RF_Public)UArrayProperty(CPP_PROPERTY(FormatDescription),TEXT(""),0);
	B->Inner = new(B,TEXT("StrProperty0"),RF_Public)UStrProperty;	

	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UExporter, SupportedClass ) );
}

UExporter::UExporter()
: FormatExtension( E_NoInit ), FormatDescription( E_NoInit), bSelectedOnly(0)
{}

void UExporter::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << SupportedClass << FormatExtension << FormatDescription;
	Ar << PreferredFormatIndex;
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
			{
				for( INT i=0; i<Default->FormatExtension.Num(); i++ )
				{
					const UBOOL bIsFileType = (appStricmp( *Default->FormatExtension(i), FileType  ) == 0);
					const UBOOL bIsWildCardType = ( appStricmp( *Default->FormatExtension(i), TEXT("*") )== 0 );
					if(	bIsFileType==TRUE || bIsWildCardType==TRUE )
					{
						Exporters.Set( Default->SupportedClass, *It );
					}
				}
			}
		}
	}

	UClass** E;
	for (UClass* TempClass = Object->GetClass(); TempClass != NULL; TempClass = TempClass->GetSuperClass())
	{
		const UBOOL bFoundExporter = ((E = Exporters.Find( TempClass )) != NULL);

		if( bFoundExporter )
		{
			return ConstructObject<UExporter>( *E );
		}
	}
		
	return NULL;
}

//
// Export an object to an archive.
//
UBOOL UExporter::ExportToArchive( UObject* Object, UExporter* InExporter, FArchive& Ar, const TCHAR* FileType, INT FileIndex )
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
		return( FALSE );
	}
	check( Object->IsA( Exporter->SupportedClass ) );
	return( Exporter->ExportBinary( Object, FileType, Ar, GWarn, FileIndex ) );
}

//
// Export an object to an output device.
//
void UExporter::ExportToOutputDevice(const FExportObjectInnerContext* Context, UObject* Object, UExporter* InExporter, FOutputDevice& Out, const TCHAR* FileType, INT Indent, DWORD PortFlags, UBOOL bInSelectedOnly)
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
	Exporter->bSelectedOnly = bInSelectedOnly;

	// this tells the lower-level code that determines whether property values are identical that
	// it should recursively compare subobjects properties as well
	if ( (PortFlags&PPF_ComponentsOnly) == 0 )
	{
		PortFlags |= PPF_DeepComparison;
	}

	if ( appStricmp(FileType, TEXT("COPY")) == 0 )
	{
		// some code which doesn't have access to the exporter's file type needs to handle copy/paste differently than exporting to file,
		// so set the export flag accordingly
		PortFlags |= PPF_Copy;
	}

	Exporter->ExportText( Context, Object, FileType, Out, GWarn, PortFlags );
	Exporter->TextIndent = SavedIndent;
}

/**
 * Export this object to a file.  Child classes do not override this, but they do provide an Export() function
 * to do the resoource-specific export work.
 * 
 * @param	Object				the object to export
 * @param	InExporter			exporter to use for exporting this object.  If NULL, attempts to create a valid exporter.
 * @param	Filename			the name of the file to export this object to
 * @param	InSelectedOnly		@todo
 * @param	NoReplaceIdentical	FALSE if we always want to overwrite any existing files, even if they're identical
 * @param	Prompt				TRUE if the user should be prompted to checkout/overwrite the output file if it already exists and is read-only
 *
 * @return	1 if the the object was successfully exported, 0 if a fatal error was encountered during export, or -1 if a non fatal error was encountered
 */
INT UExporter::ExportToFile( UObject* Object, UExporter* InExporter, const TCHAR* Filename, UBOOL InSelectedOnly, UBOOL NoReplaceIdentical, UBOOL Prompt )
{
#if !CONSOLE
	check(Object);

	CurrentFilename = Filename;

	UExporter*	Exporter	= InExporter;
	UBOOL		Result		= 0;
	FString		Extension;

	if (!Exporter)
	{
		// look for an exporter with all possible extensions, so an exporter can have something like *.xxx.yyy as an extension
		INT SearchStart = 0;
		INT DotLocation;
		while (!Exporter && (DotLocation = CurrentFilename.InStr(TEXT("."), FALSE, FALSE, SearchStart)) != INDEX_NONE)
		{
			// get everything after the current .
			Extension = CurrentFilename.Mid(DotLocation + 1);

			// try to find an exporter with it
			Exporter = FindExporter( Object, *Extension );

			// skip past the dot in case we look again
			SearchStart = DotLocation + 1;
		}
	}

	if( !Exporter )
	{
		warnf( TEXT("No %s exporter found for %s"), *Extension, *Object->GetFullName() );
		CurrentFilename = TEXT("");
		return 0;
	}

	Exporter->bSelectedOnly = InSelectedOnly;

	if( Exporter->bText )
	{
		FStringOutputDevice Buffer;
		const FExportObjectInnerContext Context;
		ExportToOutputDevice( &Context, Object, Exporter, Buffer, *Extension, 0, PPF_ExportsNotFullyQualified, InSelectedOnly );
		if ( Buffer.Len() == 0 )
		{
			Result = -1;
		}
		else
		{
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
					if( !GWarn->YesNof( LocalizeSecure(LocalizeQuery(TEXT("Overwrite"),TEXT("Core")), Filename) ) )
					{
						Result = 1;
						goto Done;
					}
				}
			}
			if( !appSaveStringToFile( Buffer, Filename ) )
			{
	#if 0
				if( GWarn->YesNof( LocalizeSecure(LocalizeQuery(TEXT("OverwriteReadOnly"),TEXT("Core")), Filename) ) )
				{
					GFileManager->Delete( Filename, 0, 1 );
					if( appSaveStringToFile( Buffer, Filename ) )
					{
						Result = 1;
						goto Done;
					}
				}
	#endif
				warnf( NAME_Error, LocalizeSecure(LocalizeError(TEXT("ExportOpen"),TEXT("Core")), *Object->GetFullName(), Filename));
				goto Done;
			}
			Result = 1;
		}
	}
	else
	{
		for( INT i = 0; i < Exporter->GetFileCount(); i++ )
		{
			FBufferArchive Buffer;
			if( ExportToArchive( Object, Exporter, Buffer, *Extension, i ) )
			{
				FString UniqueFilename = Exporter->GetUniqueFilename( Filename, i );

				if( NoReplaceIdentical )
				{
					TArray<BYTE> FileBytes;

					if(	appLoadFileToArray( FileBytes, *UniqueFilename )
					&&	FileBytes.Num() == Buffer.Num()
					&&	appMemcmp( &FileBytes( 0 ), &Buffer( 0 ), Buffer.Num() ) == 0 )
					{
						debugf( TEXT( "Not replacing %s because identical" ), *UniqueFilename );
						Result = 1;
						goto Done;
					}
					if( Prompt )
					{
						if( !GWarn->YesNof( LocalizeSecure(LocalizeQuery( TEXT( "Overwrite" ), TEXT( "Core" ) ), *UniqueFilename) ) )
						{
							Result = 1;
							goto Done;
						}
					}
				}

				if( !appSaveArrayToFile( Buffer, *UniqueFilename ) )
				{
					warnf( NAME_Error, LocalizeSecure(LocalizeError(TEXT("ExportOpen"),TEXT("Core")), *Object->GetFullName(), *UniqueFilename));
					goto Done;
				}
			}
		}
		Result = 1;
	}
Done:
	CurrentFilename = TEXT("");

	return Result;
#else
	return 0;
#endif
}

/**
 * Export the given object to a file.  Child classes do not override this, but they do provide an Export() function
 * to do the resource-specific export work.
 * 
 * @param	ExportParams		The parameters for the export.
 *
 * @return	1 if the the object was successfully exported, 0 if a fatal error was encountered during export, or -1 if a non fatal error was encountered
 */
INT UExporter::ExportToFileEx( FExportToFileParams& ExportParams )
{
#if !CONSOLE
	check(ExportParams.Object);

	CurrentFilename = ExportParams.Filename;

	UExporter*	Exporter	= ExportParams.Exporter;
	FString		Extension	= FFilename(ExportParams.Filename).GetExtension();
	UBOOL		Result		= 0;

	if (!Exporter)
	{
		// look for an exporter with all possible extensions, so an exporter can have something like *.xxx.yyy as an extension
		INT SearchStart = 0;
		INT DotLocation;
		while (!Exporter && (DotLocation = CurrentFilename.InStr(TEXT("."), FALSE, FALSE, SearchStart)) != INDEX_NONE)
		{
			// get everything after the current .
			Extension = CurrentFilename.Mid(DotLocation + 1);

			// try to find an exporter with it
			Exporter = FindExporter( ExportParams.Object, *Extension );

			// skip past the dot in case we look again
			SearchStart = DotLocation + 1;
		}
	}

	if( !Exporter )
	{
		warnf( TEXT("No %s exporter found for %s"), *Extension, *(ExportParams.Object->GetFullName()) );
		CurrentFilename = TEXT("");
		return 0;
	}

	Exporter->bSelectedOnly = ExportParams.InSelectedOnly;

	FOutputDevice* TextBuffer = NULL;
	if( Exporter->bText )
	{
		UBOOL bIsFileDevice = FALSE;
		FString TempFile = FFilename(ExportParams.Filename).GetPath();
		if (Exporter->bForceFileOperations || ExportParams.bUseFileArchive)
		{
			GFileManager->MakeDirectory(*TempFile);

			TempFile += TEXT("\\UE3ExportFile.tmp");
			TextBuffer = new FOutputDeviceFile(*TempFile);
			if (TextBuffer)
			{
				TextBuffer->SetSuppressEventTag(TRUE);
				TextBuffer->SetAutoEmitLineTerminator(FALSE);
				bIsFileDevice = TRUE;
			}
		}

		if (TextBuffer == NULL)
		{
			if (ExportParams.bUseFileArchive)
			{
				warnf(TEXT("Failed to create file output device... defaulting to string buffer"));
			}
			TextBuffer = new FStringOutputDevice();
		}
		const FExportObjectInnerContext Context(ExportParams.IgnoreObjectList);
		ExportToOutputDevice( &Context, ExportParams.Object, Exporter, *TextBuffer, *Extension, 0, PPF_ExportsNotFullyQualified, ExportParams.InSelectedOnly );
		if (bIsFileDevice)
		{
			TextBuffer->TearDown();
			GFileManager->Move(ExportParams.Filename, *TempFile, 1, 1);
		}
		else
		{
			FStringOutputDevice& StringBuffer = *((FStringOutputDevice*)TextBuffer);
			if ( StringBuffer.Len() == 0 )
			{
				Result = -1;
			}
			else
			{
				if( ExportParams.NoReplaceIdentical )
				{
					FString FileBytes;
					if
						(	appLoadFileToString(FileBytes,ExportParams.Filename)
						&&	appStrcmp(*StringBuffer,*FileBytes)==0 )
					{
						debugf( TEXT("Not replacing %s because identical"), ExportParams.Filename );
						Result = 1;
						goto Done;
					}
					if( ExportParams.Prompt )
					{
						if( !GWarn->YesNof( LocalizeSecure(LocalizeQuery(TEXT("Overwrite"),TEXT("Core")), ExportParams.Filename) ) )
						{
							Result = 1;
							goto Done;
						}
					}
				}
				if( !appSaveStringToFile( StringBuffer, ExportParams.Filename ) )
				{
#if 0
					if( GWarn->YesNof( LocalizeSecure(LocalizeQuery(TEXT("OverwriteReadOnly"),TEXT("Core")), ExportParams.Filename) ) )
					{
						GFileManager->Delete( ExportParams.Filename, 0, 1 );
						if( appSaveStringToFile( StringBuffer, ExportParams.Filename ) )
						{
							Result = 1;
							goto Done;
						}
					}
#endif
					warnf( NAME_Error, LocalizeSecure(LocalizeError(TEXT("ExportOpen"),TEXT("Core")), *(ExportParams.Object->GetFullName()), ExportParams.Filename));
					goto Done;
				}
				Result = 1;
			}
		}
	}
	else
	{
		for( INT i = 0; i < Exporter->GetFileCount(); i++ )
		{
			FBufferArchive Buffer;
			if( ExportToArchive( ExportParams.Object, Exporter, Buffer, *Extension, i ) )
			{
				FString UniqueFilename = Exporter->GetUniqueFilename( ExportParams.Filename, i );

				if( ExportParams.NoReplaceIdentical )
				{
					TArray<BYTE> FileBytes;

					if(	appLoadFileToArray( FileBytes, *UniqueFilename )
					&&	FileBytes.Num() == Buffer.Num()
					&&	appMemcmp( &FileBytes( 0 ), &Buffer( 0 ), Buffer.Num() ) == 0 )
					{
						debugf( TEXT( "Not replacing %s because identical" ), *UniqueFilename );
						Result = 1;
						goto Done;
					}
					if( ExportParams.Prompt )
					{
						if( !GWarn->YesNof( LocalizeSecure(LocalizeQuery( TEXT( "Overwrite" ), TEXT( "Core" ) ), *UniqueFilename) ) )
						{
							Result = 1;
							goto Done;
						}
					}
				}

				if ( !ExportParams.WriteEmptyFiles && !Buffer.Num() )
				{
					Result = 1;
					goto Done;
				}

				if( !appSaveArrayToFile( Buffer, *UniqueFilename ) )
				{
					warnf( NAME_Error, LocalizeSecure(LocalizeError(TEXT("ExportOpen"),TEXT("Core")), *(ExportParams.Object->GetFullName()), *UniqueFilename));
					goto Done;
				}
			}
		}
		Result = 1;
	}
Done:
	if ( TextBuffer != NULL )
	{
		delete TextBuffer;
		TextBuffer = NULL;
	}
	CurrentFilename = TEXT("");

	return Result;
#else
	return 0;
#endif
}

const UBOOL UExporter::bEnableDebugBrackets = FALSE;

/**
 * Emits the starting line for a subobject definition.
 *
 * @param	Ar					the archive to output the text to
 * @param	Obj					the object to emit the subobject block for
 * @param	bIncludeBrackets	(debugging purposes only)
 */
void UExporter::EmitBeginObject( FOutputDevice& Ar, UObject* Obj, DWORD PortFlags )
{
	check(Obj);

	// figure out how to export
	UBOOL bIsExportingForDefaultProperties = (PortFlags & PPF_ExportDefaultProperties) != 0;
	UBOOL bIsExportingDefaultObject = Obj->HasAnyFlags(RF_ClassDefaultObject) || Obj->GetArchetype()->HasAnyFlags(RF_ClassDefaultObject);

	// start outputting the string for the Begin Object line
	Ar.Logf(TEXT("%sBegin Object"), appSpc(TextIndent));

	UBOOL bNewComponent = FALSE;
	if( Obj->IsA(UComponent::StaticClass()) )
	{
		// If the archetype is a class default object, then this is a 'new' component
		bNewComponent = Obj->GetArchetype()->HasAnyFlags(RF_ClassDefaultObject);
	}

	// use Class= if we are not exporting a default property, or adding a new component
	if(!bIsExportingForDefaultProperties || bNewComponent)
	{
		Ar.Logf(TEXT(" Class=%s"), *Obj->GetClass()->GetName());
	}

	// always need a name
	Ar.Logf(TEXT(" Name=%s"), *Obj->GetName());

	// do we want the archetype string?
	if (!bIsExportingDefaultObject && !bIsExportingForDefaultProperties)
	{
		Ar.Logf(TEXT(" Archetype=%s'%s'"), *Obj->GetArchetype()->GetClass()->GetName(), *Obj->GetArchetype()->GetPathName());
	}

	// end in a return
	Ar.Logf(TEXT("\r\n"));

	if ( bEnableDebugBrackets )
	{
		Ar.Logf(TEXT("%s{%s"), appSpc(TextIndent), LINE_TERMINATOR);
	}
}

/**
 * Emits the ending line for a subobject definition.
 *
 * @param	Ar					the archive to output the text to
 * @param	bIncludeBrackets	(debugging purposes only)
 */
void UExporter::EmitEndObject( FOutputDevice& Ar )
{
	if ( bEnableDebugBrackets )
	{
		Ar.Logf(TEXT("%s}%s"), appSpc(TextIndent), LINE_TERMINATOR);
	}
	Ar.Logf( TEXT("%sEnd Object\r\n"), appSpc(TextIndent) );
}

/**
 * Creates the map from objects to their direct inners.
 */
FExportObjectInnerContext::FExportObjectInnerContext()
{
	// For each object . . .
	for ( TObjectIterator<UObject> It ; It ; ++It )
	{
		UObject* InnerObj = *It;
		UObject* OuterObj = InnerObj->GetOuter();
		if ( OuterObj )
		{
			InnerList* Inners = ObjectToInnerMap.Find( OuterObj );
			if ( Inners )
			{
				// Add object to existing inner list.
				Inners->AddItem( InnerObj );
			}
			else
			{
				// Create a new inner list for the outer object.
				InnerList& InnersForOuterObject = ObjectToInnerMap.Set( OuterObj, InnerList() );
				InnersForOuterObject.AddItem( InnerObj );
			}
		}
	}
}

/**
 * Creates the map from objects to their direct inners.
 *	@param	ObjsToIgnore	An array of objects that should NOT be put in the list
 */
FExportObjectInnerContext::FExportObjectInnerContext(TArray<UObject*>& ObjsToIgnore)
{
	// For each object . . .
	for ( TObjectIterator<UObject> It ; It ; ++It )
	{
		UObject* InnerObj = *It;
		if ( !InnerObj->IsPendingKill() )
		{
			if ( !ObjsToIgnore.ContainsItem(InnerObj) )
			{
				UObject* OuterObj = InnerObj->GetOuter();
				if ( OuterObj && !OuterObj->IsPendingKill() )
				{
					InnerList* Inners = ObjectToInnerMap.Find( OuterObj );
					if ( Inners )
					{
						// Add object to existing inner list.
						Inners->AddItem( InnerObj );
					}
					else
					{
						// Create a new inner list for the outer object.
						InnerList& InnersForOuterObject = ObjectToInnerMap.Set( OuterObj, InnerList() );
						InnersForOuterObject.AddItem( InnerObj );
					}
				}
			}
		}
	}
}

/**
 * Single entry point to export an object's subobjects, its components, and its properties
 *
 * @param Context			Context from which the set of 'inner' objects is extracted.  If NULL, an object iterator will be used.
 * @param Object			The object to export 
 * @param Ar				OutputDevice to print to
 * @param PortFlags			Flags controlling export behavior
 * @param bSkipComponents	TRUE if components should not be exported
 */
void UExporter::ExportObjectInner(const FExportObjectInnerContext* Context, UObject* Object, FOutputDevice& Ar, DWORD PortFlags, UBOOL bSkipComponents)
{
	// indent all the text in here
	TextIndent += 3;

	FExportObjectInnerContext::InnerList ObjectInners;
	if ( Context )
	{
		const FExportObjectInnerContext::InnerList* Inners = Context->ObjectToInnerMap.Find( Object );
		if ( Inners )
		{
			ObjectInners = *Inners;
		}
	}
	else
	{
		for (TObjectIterator<UObject> It; It; ++It)
		{
			if ( It->GetOuter() == Object )
			{
				ObjectInners.AddItem( *It );
			}
		}
	}

	for ( INT ObjIndex = 0 ; ObjIndex < ObjectInners.Num() ; ++ObjIndex )
	{
		// NOTE: We ignore inner objects that have been tagged for death
		UObject* Obj = ObjectInners(ObjIndex);
		if ( !Obj->IsPendingKill() && !Obj->IsA(UComponent::StaticClass()) && appStricmp(*Obj->GetClass()->GetName(), TEXT("Model")) != 0)
		{
			// export the object
			UExporter::ExportToOutputDevice( Context, Obj, NULL, Ar, (PortFlags & PPF_Copy) ? TEXT("Copy") : TEXT("T3D"), TextIndent, PortFlags );

			// don't reexport below in ExportProperties
			Obj->SetFlags(RF_TagImp);
		}
	}

	TArray<UComponent*> Components;
	if (!bSkipComponents)
	{
		// first export the components
		Object->CollectComponents(Components,TRUE);
		ExportComponentDefinitions(Context, Components, Ar, PortFlags);
	}

	// export the object's properties
	// Note: we now use the class default object as the object to diff properties against before they exported.  This is because archetype objects aren't properly exported if the object being
	// exported has the same values as its archetype. Now, values are written to the export file if they differ from class default object properties.  This ensures proper export of archetype objects 
	// and results in no difference for non archetype objects, which have always used class default object properties for comparison.
	ExportProperties( Context, Ar, Object->GetClass(), (BYTE*)Object, TextIndent, Object->GetClass(), (BYTE*)Object->GetClass()->GetDefaultObject(), Object, PortFlags );

	if (!bSkipComponents)
	{
		// Export anything extra for the components. Used for instanced foliage.
		// This is done after the actor properties so these are set when regenerating the extra data objects.
		ExportComponentExtra( Context, Components, Ar, PortFlags );
	}

	// remove indent
	TextIndent -= 3;
}

/**
 * Exports subobject definitions for all components specified.  Makes sure that components which are referenced
 * by other components in the map are exported first.
 *
 * @param	Components		the components to export.  This map is typically generated by calling CollectComponents on the object being exported
 * @param	Ar				the archive to output the subobject definitions to...usually the same archive that you're exporting the rest of the properties
 * @param	PortFlags		the flags that were passed into the call to ExportText
 */
void UExporter::ExportComponentDefinitions(const FExportObjectInnerContext* Context, const TArray<UComponent*>& Components, FOutputDevice& Ar, DWORD PortFlags)
{
	PortFlags |= PPF_ExportsNotFullyQualified;

	// keeps a map of component name to subobject definition, so that we can export any components which are referenced by other components first
	TMap<UComponent*,FStringOutputDevice>		ComponentDefinitionMap;

	// this is an ordered list of components which are referenced by other components in this actor...the order in which the components
	// appear in the array is the order in which they should be exported, in order to guarantee that components are exported prior to the
	// components which reference them...
	TArray<UComponent*> ReferencedComponents;

	for ( INT ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++ )
	{
		UComponent* Component = Components(ComponentIndex);
		FName ComponentName = Component->GetInstanceMapName();
		if ( !Component->HasAnyFlags(RF_TagImp) )
		{
			FStringOutputDevice& ComponentDefinitionAr = ComponentDefinitionMap.Set(Component, FStringOutputDevice());

			if ( Component->HasAnyFlags(RF_ClassDefaultObject) || Component->GetArchetype()->HasAllFlags(RF_ClassDefaultObject) )
			{
				ComponentDefinitionAr.Logf(TEXT("%sBegin Object Class=%s Name=%s ObjName=%s%s"), appSpc(TextIndent), *Component->GetClass()->GetName(), *ComponentName.ToString(), *Component->GetName(), LINE_TERMINATOR);
			}
			else
			{
				ComponentDefinitionAr.Logf(TEXT("%sBegin Object Class=%s Name=%s ObjName=%s Archetype=%s'%s'%s"),appSpc(TextIndent),*Component->GetClass()->GetName(), *ComponentName.ToString(), *Component->GetName(), *Component->GetArchetype()->GetClass()->GetName(), *Component->GetArchetype()->GetPathName(), LINE_TERMINATOR);
			}
			
			ExportObjectInner( Context, Component, ComponentDefinitionAr, PortFlags | PPF_ExportsNotFullyQualified, TRUE);

			ComponentDefinitionAr.Logf(TEXT("%sEnd Object%s"),appSpc(TextIndent), LINE_TERMINATOR);

			for ( INT InnerComponentIndex = 0; InnerComponentIndex < Components.Num(); InnerComponentIndex++ )
			{
				UComponent* OtherComponent = Components(InnerComponentIndex);
				if ( OtherComponent != Component && OtherComponent->HasAnyFlags(RF_TagExp) )
				{
					// this component is marked as RF_TagExp, which means that the component we just exported had a reference
					// to it....add it to our list of ReferencedComponents so that we can export those first, in order.
					// - man...this is some hackilicious stuff here....
					ReferencedComponents.AddUniqueItem(OtherComponent);

					// now clear the flag so that we know that if a component has the RF_TagExp flag, it's because the component that was just exported
					// was referencing it, and not some other component that was encountered previously...Also clear the RF_TagImp flag just in case it 
					// was set by something, since we'll be using RF_TagImp to skip over the referenced components when we export the remaining components
					OtherComponent->ClearFlags(RF_TagExp|RF_TagImp);
				}
			}
		}
	}

	// now, add the property text for the referenced components to the output archive first
	for ( INT i = 0; i < ReferencedComponents.Num(); i++ )
	{
		// export the property values for all referenced components first
		FStringOutputDevice* ComponentPropertyText = ComponentDefinitionMap.Find(ReferencedComponents(i));
		checkSlow(ComponentPropertyText);

		Ar.Log(*ComponentPropertyText);

		UComponent* ReferencedComponent = ReferencedComponents(i);
		checkSlow(ReferencedComponent);

		// mark this so that the below ExportProperties won't cause the component to be exported again (see UObject::ExportProperties())
		// and we know that this component has already been exported
		ReferencedComponent->SetFlags(RF_TagImp);
	}

	// finally, export the rest of the components
	for ( INT ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++ )
	{
		UComponent* Component = Components(ComponentIndex);
		FName ComponentName = Component->GetInstanceMapName();
		if ( !Component->HasAnyFlags(RF_TagImp) )
		{
			FStringOutputDevice* ComponentPropertyText = ComponentDefinitionMap.Find(Component);
			checkSlow(ComponentPropertyText);

			Ar.Log(*ComponentPropertyText);
			Component->SetFlags(RF_TagImp);
		}
	}
}
