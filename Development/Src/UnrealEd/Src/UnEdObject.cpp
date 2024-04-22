/*=============================================================================
	UnEdObject.cpp: Unreal Editor object manipulation code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "Factories.h"
#include "BSPOps.h"
#include "EngineFoliageClasses.h"

/*
Subobject Terms -
Much of the confusion in dealing with subobjects and instancing can be traced to the ambiguity of the words used to work with the various concepts.
A standardized method of referring to these terms is highly recommended - it makes the code much more consistent, and well thought-out variable names
make the concepts and especially the relationships between each of the concepts easier to grasp.  This will become even more apparent once archetypes
and prefabs are implemented.

Once we've decided on standard terms, we should try to use these words as the name for any variables which refer to the associated concept, in any
code that deals with that concept (where possible).

Here are some terms I came up with for starters.  If you're reading this, and you have a more appropriate name for one of these concepts, feel that any
of the descriptions or terms isn't clear enough, or know of a concept that isn't represented here, feel free to modify this comment and update
the appropriate code, if applicable.



Instance:
a UObject that has been instanced from a subobject template

Template (or template object):
the UObject associated with [or created by] an inline subobject definition; stored in the UClass's Defaults array (in the case of a .uc subobject).  

TemplateName:
the name of the template object

TemplateClass:
the class of the Template object

TemplateOwner:
the UObject that contains the template object;  when dealing with templates created via inline subobject 
definitions, this corresponds to the class that contains the Begin Object block for the template

SubobjectRoot:
when dealing with nested subobjects, corresponds to the top-most Outer that is not a subobject or template (generally
the same as Outer)
*/

class FDefaultPropertiesContextSupplier : public FContextSupplier
{
public:
	/** the current line number */
	INT CurrentLine;

	/** the package we're processing */
	FString PackageName;

	/** the class we're processing */
	FString ClassName;

	FString GetContext()
	{
		return FString::Printf
		(
			TEXT("%sDevelopment\\Src\\%s\\Classes\\%s.uc(%i)"),
			*appRootDir(),
			*PackageName,
			*ClassName,
			CurrentLine
		);
	}

	FDefaultPropertiesContextSupplier() {}
	FDefaultPropertiesContextSupplier( const TCHAR* Package, const TCHAR* Class, INT StartingLine )
	: PackageName(Package), ClassName(Class), CurrentLine(StartingLine)
	{
	}

};

static FDefaultPropertiesContextSupplier* ContextSupplier = NULL;

//
//	UEditorEngine::RenameObject
//
void UEditorEngine::RenameObject(UObject* Object,UObject* NewOuter,const TCHAR* NewName, ERenameFlags Flags)
{
	Object->Rename(NewName, NewOuter, Flags);
	Object->SetFlags(RF_Public | RF_Standalone);
	Object->MarkPackageDirty();
}

//
//	ImportProperties
//

struct FDefinedProperty
{
    UProperty *Property;
    INT Index;
    bool operator== ( const FDefinedProperty& Other ) const
    {
        return( (Property == Other.Property) && (Index == Other.Index) );
    }
};

static void SkipWhitespace(const TCHAR*& Str)
{
	while(*Str == ' ' || *Str == 9)
		Str++;
}

static void CheckNewName(UObject* Outer,const TCHAR* Name)
{
	UObject*	OldObject;
	if((OldObject = UObject::StaticFindObject(UObject::StaticClass(),Outer,Name)) != NULL)
	{
		if(GEditor->Bootstrapping)
			GWarn->Logf( NAME_Error, TEXT("BEGIN OBJECT: name %s redefined."),Name );
		else
			OldObject->Rename();
	}
}

static void RemoveComponentsFromClass( UObject* Owner, TArray<UComponent*> RemovedComponents )
{
	check(Owner);

	UClass* OwnerClass = Owner->GetClass();
	if ( RemovedComponents.Num() )
	{
		// find out which components are still being referenced by the object
		TArray<UComponent*> ComponentReferences;
		TArchiveObjectReferenceCollector<UComponent> InheritedCollector(&ComponentReferences, Owner->GetArchetype(), TRUE, TRUE);
		TArchiveObjectReferenceCollector<UComponent> Collector(&ComponentReferences, Owner, TRUE, TRUE);
		Owner->Serialize(InheritedCollector);
		Owner->Serialize(Collector);

		for ( INT RemovalIndex = RemovedComponents.Num() - 1; RemovalIndex >= 0; RemovalIndex-- )
		{
			UComponent* Component = RemovedComponents(RemovalIndex);
			for ( INT ReferenceIndex = 0; ReferenceIndex < ComponentReferences.Num(); ReferenceIndex++ )
			{
				// this component is still being referenced by the object, so it needs to remain
				// in the ComponentNameToDefaultObjectMap
				UComponent* ReferencedComponent = ComponentReferences(ReferenceIndex);
				if ( ReferencedComponent == Component )
				{
					RemovedComponents.Remove(RemovalIndex);
					break;
				}
			}
		}

		for ( INT RemovalIndex = 0; RemovalIndex < RemovedComponents.Num(); RemovalIndex++ )
		{
			// these components are no longer referened by the object, so they should also be 
			// removed from the class's name->component map, so that child classes don't copy
			// those re-add those components when they copy inherited components
			UComponent* Component = RemovedComponents(RemovalIndex);
			OwnerClass->ComponentNameToDefaultObjectMap.Remove(Component->GetFName());
		}
	}
}

/**
 * Create copies of any components inherited from the parent class
 * that haven't already been explicitly overridden.
 *
 * @param	Class	The class to instance the components for.
 */
static void CopyInheritedComponents( UClass* Class, FObjectInstancingGraph& InstanceGraph )
{
	TMap<UComponent*,UComponent*> ReplacementMap;

	// Create a new class/default object for each component.
	for(TMap<FName,UComponent*>::TIterator It(Class->ComponentNameToDefaultObjectMap);It;++It)
	{
		const FName& ComponentName = It.Key();
		UComponent* Component = It.Value();

		// if this component is owned by a parent class
		if( Component->GetOuter() != Class->GetDefaultObject())
		{
			CheckNewName(Class,*ComponentName.ToString());

			UClass* ComponentClass = Component->GetClass();

			// create a new copy for this class, so that each class its own unique copy of every component template
			UComponent*	NewComponent = ConstructObject<UComponent>(
				ComponentClass,
				Class->GetDefaultObject(),
				ComponentName,
				RF_Public | ((ComponentClass->ClassFlags&CLASS_Localized) ? RF_PerObjectLocalized : 0),
				Component,
				Class->GetDefaultObject(),
				&InstanceGraph
				);

			ReplacementMap.Set(Component, NewComponent);
			Class->ComponentNameToDefaultObjectMap.Set(ComponentName,NewComponent);

			// now add any components that were instanced as a result of references to other components in the class
			TArray<UComponent*> ReferencedComponents;
			InstanceGraph.RetrieveComponents(Class->GetDefaultObject(), ReferencedComponents, FALSE);

			for ( INT ComponentIndex = 0; ComponentIndex < ReferencedComponents.Num(); ComponentIndex++ )
			{
				UComponent* Component = ReferencedComponents(ComponentIndex);
				Class->ComponentNameToDefaultObjectMap.Set(Component->GetFName(), Component);
			}
		}
		else
		{
			UComponent* BaseComponent = Component->GetArchetype<UComponent>();
			if ( !BaseComponent->HasAnyFlags(RF_ClassDefaultObject) )
			{
				// if this component's archetype is a component template contained by a parent, add it to the ReplacementMap so that
				// any inherited property values that point to the parent's component will be replaced by this entry
				ReplacementMap.Set(BaseComponent,Component);
			}
		}
	}

	FArchiveReplaceObjectRef<UComponent> ReplaceComponentRefs(Class->GetDefaultObject(), ReplacementMap, FALSE, FALSE, TRUE);
}

static FString ReadStructValue( const TCHAR* pStr, const TCHAR*& SourceTextBuffer )
{
	if ( *pStr == '{' )
	{
		INT bracketCnt = 0;
		UBOOL bInString = FALSE;
		FString Result;
		FString CurrentLine;

		// increment through each character until we run out of brackets
		do
		{
			// check to see if we've reached the end of this line
			if (*pStr == '\0' || *pStr == NULL)
			{
				// parse a new line
				if (ParseLine(&SourceTextBuffer,CurrentLine,TRUE))
				{
					if ( ContextSupplier != NULL )
					{
						ContextSupplier->CurrentLine++;
					}

					pStr = *CurrentLine;
				}
				else
				{
					// no more to parse
					break;
				}
			}

			if ( *pStr != NULL && *pStr != '\0' )
			{
				TCHAR Char = *pStr;

				// if we reach a bracket, increment the count
				if (Char == '{')
				{
					bracketCnt++;
				}
					// if we reach and end bracket, decrement the count
				else if (Char == '}')
				{
					bracketCnt--;
				}
				else if (Char == '\"')
				{
					// flip the "we are in a string" switch
					bInString = !bInString;
				}
				else if ( Char == TEXT('\n') )
				{
					if ( ContextSupplier != NULL )
					{
						ContextSupplier->CurrentLine++;
					}
				}

				// add this character to our end result
				// if we're currently inside a string value, allow the space character
				if ( bInString ||
					(	Char != '\n' && Char != '\r' && !appIsWhitespace(Char)
					&&	Char != '{' && Char != '}' ))
				{
					Result += Char;
				}

				pStr++;
			}
		}
		while (bracketCnt != 0);


		return Result;
	}

	return pStr;
}

/**
 * Attempts to read an array index (xxx) sequence.  Handles const/enum replacements, etc.
 * @param	ObjectStruct	the scope of the object/struct containing the property we're currently importing
 * @param	TopOuter		Only valid when importing defaults for structs and subobjects - the class that contains the subobject/struct in question.
 *							Used for extending the search to allow specifying const's/enums from the subobject's outer class
 * @param	Str				[out] pointer to the the buffer containing the property value to import
 * @param	Warn			the output device to send errors/warnings to
 * @return	the array index for this defaultproperties line.  INDEX_NONE if this line doesn't contains an array specifier, or 0 if there was an error parsing the specifier.
 */
static const INT ReadArrayIndex(UStruct *ObjectStruct, UClass* TopOuter, const TCHAR*& Str, FFeedbackContext *Warn)
{
	const TCHAR* Start = Str;
	INT Index = INDEX_NONE;
	SkipWhitespace(Str);

	if (*Str == '(' || *Str == '[')
	{
		Str++;
		FString IndexText(TEXT(""));
		while ( *Str && *Str != ')' && *Str != ']' )
		{
			if ( *Str == TCHAR('=') )
			{
				// we've encountered an equals sign before the closing bracket
				Warn->Logf( NAME_Warning, TEXT("Missing ')' in default properties subscript: %s"), Start);
				return 0;
			}

			IndexText += *Str++;
		}

		if ( *Str++ )
		{
			if (IndexText.Len() > 0 )
			{
				if ( appIsAlpha(IndexText[0]))
				{
					FName IndexTokenName = FName(*IndexText, FNAME_Find);
					if( IndexTokenName != NAME_None )
					{
						// Search for the enum in question.
						if( GIsUCCMake )
						{
							// The make commandlet keeps track of this information via a map, so use the shortcut in this case.
							extern TMap<FName,INT> GUCCMakeEnumNameToIndexMap;
							INT* IndexPtr = GUCCMakeEnumNameToIndexMap.Find( IndexTokenName );
							if( IndexPtr )
							{
								Index = *IndexPtr;
							}
						}
						else
						{
							for (TObjectIterator<UEnum> It; It && Index == INDEX_NONE; ++It)
							{
								Index = It->FindEnumIndex(IndexTokenName);
							}
						}

						if( Index == INDEX_NONE )
						{
							// search for const ref
							UConst* Const = FindField<UConst>(ObjectStruct, *IndexText);
							if ( Const == NULL && TopOuter != NULL )
							{
								Const = FindField<UConst>(TopOuter, *IndexText);
							}

							if ( Const != NULL )
							{
								Index = appAtoi(*Const->Value);
							}
							else
							{
								Index = 0;
								Warn->Logf(NAME_Warning, TEXT("Invalid subscript in default properties: %s"), Start);
							}
						}
					}
					else
					{
						Index = 0;

						// unknown or invalid identifier specified for array subscript
						Warn->Logf(NAME_Warning, TEXT("Invalid subscript in default properties: %s"), Start);
					}
				}
				else
				{
					Index = appAtoi(*IndexText);
				}
			}
			else
			{
				Index = 0;

				// nothing was specified between the opening and closing parenthesis
				Warn->Logf(NAME_Warning, TEXT("Invalid subscript in default properties: %s"), Start);
			}
		}
		else
		{
			Index = 0;
			Warn->Logf( NAME_Warning, TEXT("Missing ')' in default properties subscript: %s"), Start );
		}
	}
	return Index;
}

/**
 * Parse and import text as property values for the object specified.  This function should never be called directly - use ImportObjectProperties instead.
 * 
 * @param	ObjectStruct				the struct for the data we're importing
 * @param	DestData					the location to import the property values to
 * @param	SourceText					pointer to a buffer containing the values that should be parsed and imported
 * @param	SubobjectRoot					when dealing with nested subobjects, corresponds to the top-most outer that
 *										is not a subobject/template
 * @param	SubobjectOuter				the outer to use for creating subobjects/components. NULL when importing structdefaultproperties
 * @param	Warn						ouptut device to use for log messages
 * @param	Depth						current nesting level
 * @param	ComponentNameToInstanceMap	@todo
 * @param	RemovedComponents			[out] array of components that were removed from the class's Components array via the Components.Remove() functionality.  This is sort of a hack, but we need to detemine if
 *										the class has any other references to a component that has been removed (cuz if so, we'll also remove that component's name from the ComponentNameToDefaultObjectMap), but we
 *										can't do that until CopyInheritedComponents has been called....
 * @param	InstanceGraph				contains the mappings of instanced objects and components to their templates
 *
 * @return	NULL if the default values couldn't be imported
 */
static const TCHAR* ImportProperties(
	BYTE*						DestData,
	const TCHAR*				SourceText,
	UStruct*					ObjectStruct,
	UObject*					SubobjectRoot,
	UObject*					SubobjectOuter,
	TMap<FName,UComponent*>&	ComponentNameToInstanceMap,
	TArray<UComponent*>*		RemovedComponents,
	FFeedbackContext*			Warn,
	INT							Depth,
	FObjectInstancingGraph&		InstanceGraph
	)
{
	check(ObjectStruct!=NULL);
	check(DestData!=NULL);

	if ( SourceText == NULL )
		return NULL;

	// Cannot create subobjects when importing struct defaults, or if SubobjectOuter (used as the Outer for any subobject declarations encountered) is NULL
	UBOOL bSubObjectsAllowed = !ObjectStruct->IsA(UScriptStruct::StaticClass()) && SubobjectOuter != NULL;

	// TRUE when DestData corresponds to a subobject in a class default object
	UBOOL bSubObject = FALSE;

	UClass* ComponentOwnerClass = NULL;

	if ( bSubObjectsAllowed )
	{
		bSubObject = SubobjectRoot != NULL && SubobjectRoot->HasAnyFlags(RF_ClassDefaultObject);
		if ( SubobjectRoot == NULL )
		{
			SubobjectRoot = SubobjectOuter;
		}

		ComponentOwnerClass = SubobjectOuter != NULL
			? SubobjectOuter->IsA(UClass::StaticClass())
				? CastChecked<UClass>(SubobjectOuter)
				: SubobjectOuter->GetClass()
			: NULL;
	}
	

	// The PortFlags to use for all ImportText calls
	DWORD PortFlags = PPF_Delimited | PPF_CheckReferences;
	if (GIsImportingT3D)
	{
		PortFlags |= PPF_AttemptNonQualifiedSearch;
	}
	if ( GIsUCCMake )
	{
		PortFlags |= PPF_RestrictImportTypes|PPF_ParsingDefaultProperties;
	}

	FString StrLine;

	// If bootstrapping, check the class we're BEGIN OBJECTing has had its properties imported.
	if( GEditor->Bootstrapping && Depth==0 )
	{
		const TCHAR* TempData = SourceText;
		while( ParseLine( &TempData, StrLine ) )
		{
			const TCHAR* Str = *StrLine;
			if( GetBEGIN(&Str,TEXT("Object")))
			{
				UClass* TemplateClass;
				if(	ParseObject<UClass>(Str,TEXT("Class="),TemplateClass,ANY_PACKAGE) )
				{
					if( (TemplateClass->ClassFlags&CLASS_NeedsDefProps) )
						return NULL;
				}
			}
		}
	}

    TArray<FDefinedProperty> DefinedProperties;

	// Parse all objects stored in the actor.
	// Build list of all text properties.
	UBOOL ImportedBrush = 0;
	while( ParseLine( &SourceText, StrLine, TRUE ) )
	{
		const TCHAR* Str = *StrLine;

		if ( ContextSupplier != NULL )
		{
			ContextSupplier->CurrentLine++;
		}
		if ( appStrlen(Str) == 0 )
			continue;
		
		if( GetBEGIN(&Str,TEXT("Brush")) && ObjectStruct->IsChildOf(ABrush::StaticClass()) )
		{
			// If SubobjectOuter is NULL, we are importing defaults for a UScriptStruct's defaultproperties block
			if ( !bSubObjectsAllowed )
			{
				Warn->Logf(NAME_Error, TEXT("BEGIN BRUSH: Subobjects are not allowed in this context"));
				return NULL;
			}

			// Parse brush on this line.
			TCHAR BrushName[NAME_SIZE];
			if( Parse( Str, TEXT("Name="), BrushName, NAME_SIZE ) )
			{
				// If a brush with this name already exists in the
				// level, rename the existing one.  This is necessary
				// because we can't rename the brush we're importing without
				// losing our ability to associate it with the actor properties
				// that reference it.
				UModel* ExistingBrush = FindObject<UModel>( SubobjectRoot, BrushName );
				if( ExistingBrush )
					ExistingBrush->Rename();

				// Create model.
				UModelFactory* ModelFactory = new UModelFactory;
				ModelFactory->FactoryCreateText( UModel::StaticClass(), SubobjectRoot, FName(BrushName, FNAME_Add, TRUE), 0, NULL, TEXT("t3d"), SourceText, SourceText+appStrlen(SourceText), Warn );
				ImportedBrush = 1;
			}
		}
		else if( GetBEGIN(&Str,TEXT("StaticMesh")))
		{
			// If SubobjectOuter is NULL, we are importing defaults for a UScriptStruct's defaultproperties block
			if ( !bSubObjectsAllowed )
			{
				Warn->Logf(NAME_Error, TEXT("BEGIN STATICMESH: Subobjects are not allowed in this context"));
				return NULL;
			}

			// Parse static mesh on this line.
			TCHAR	StaticMeshName[NAME_SIZE];
			if(Parse(Str,TEXT("Name="),StaticMeshName,NAME_SIZE))
			{
				// Rename any static meshes that have the desired name.
				UStaticMesh*	ExistingStaticMesh = FindObject<UStaticMesh>(SubobjectRoot,StaticMeshName);

				if(ExistingStaticMesh)
				{
					ExistingStaticMesh->Rename();
				}

				// Parse the static mesh.

				UStaticMeshFactory*	StaticMeshFactory = new UStaticMeshFactory;
				StaticMeshFactory->FactoryCreateText(UStaticMesh::StaticClass(),SubobjectRoot,FName(StaticMeshName, FNAME_Add, TRUE),0,NULL,TEXT("t3d"),SourceText,SourceText + appStrlen(SourceText),Warn);
			}
		}
		else if( GetBEGIN(&Str,TEXT("Foliage")) )
		{
			UStaticMesh* StaticMesh;
			FName ComponentName;
			if( SubobjectRoot && 
				ParseObject<UStaticMesh>( Str, TEXT("StaticMesh="), StaticMesh, ANY_PACKAGE ) &&
				Parse(Str, TEXT("Component="), ComponentName) )
			{
				UActorComponent* ActorComponent = Cast<UActorComponent>(ComponentNameToInstanceMap.FindRef(ComponentName));

				if( ActorComponent )
				{
					ULevel* ComponentLevel = Cast<ULevel>(SubobjectRoot->GetOuter());
					if( ComponentLevel == GWorld->CurrentLevel )
					{
						AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActor(TRUE);

						const TCHAR* StrPtr;
						FString StrLine;
						while( ParseLine( &SourceText, StrLine ) )
						{
							StrPtr = *StrLine;
							if( GetEND(&StrPtr,TEXT("Foliage")) )
							{
								break;
							}

							// Parse the instance properties
							FFoliageInstance Instance;
							FString Temp;
							if( Parse(StrPtr,TEXT("Location="), Temp, FALSE) )
							{
								GetFVECTOR(*Temp, Instance.Location);
							}
							if( Parse(StrPtr,TEXT("Rotation="), Temp, FALSE) )
							{
								GetFROTATOR(*Temp, Instance.Rotation,1);
							}
							if( Parse(StrPtr,TEXT("PreAlignRotation="), Temp, FALSE) )
							{
								GetFROTATOR(*Temp, Instance.PreAlignRotation,1);
							}
							if( Parse(StrPtr,TEXT("DrawScale3D="), Temp, FALSE) )
							{
								GetFVECTOR(*Temp, Instance.DrawScale3D);
							}
							Parse(StrPtr,TEXT("Flags="),Instance.Flags);

							Instance.Base = ActorComponent;

							// Add the instance
							FFoliageMeshInfo* MeshInfo = IFA->FoliageMeshes.Find(StaticMesh);
							if( MeshInfo == NULL ) 
							{
								MeshInfo = IFA->AddMesh(StaticMesh);
							}
							MeshInfo->AddInstance(IFA, StaticMesh, Instance);
						}
					}
				}
			}
		}
		else if( GetBEGIN(&Str,TEXT("Object")))
		{
			// If SubobjectOuter is NULL, we are importing defaults for a UScriptStruct's defaultproperties block
			if ( !bSubObjectsAllowed )
			{
				Warn->Logf(NAME_Error, TEXT("BEGIN OBJECT: Subobjects are not allowed in this context"));
				return NULL;
			}

			// Parse subobject default properties.
			// Note: default properties subobjects have compiled class as their Outer (used for localization).
			UClass*	TemplateClass = NULL;
			ParseObject<UClass>(Str,TEXT("Class="),TemplateClass,ANY_PACKAGE);

			// parse the name of the template
			FName	TemplateName = NAME_None;
			Parse(Str,TEXT("Name="),TemplateName);
			if(TemplateName == NAME_None)
			{
				Warn->Logf(NAME_Error,TEXT("BEGIN OBJECT: Must specify valid name for subobject/component: %s"), *StrLine);
				return NULL;
			}

			// points to the parent class's template subobject/component, if we are overriding a subobject/component declared in our parent class
			UObject* BaseTemplate=NULL;
			if( TemplateClass )
			{
				if ( !TemplateClass->HasAnyClassFlags(CLASS_Compiled|CLASS_Intrinsic) )
				{
					Warn->Logf( NAME_Error, TEXT("BEGIN OBJECT: Can't create subobject as Class %s hasn't been compiled yet."), *TemplateClass->GetName() );
				}

				if( TemplateClass->HasAnyClassFlags(CLASS_NeedsDefProps) )
				{
					// defer until the subobject's class has imported its defaults and initialized its CDO
					return NULL;
				}
			}
			else
			{
				// If no class was specified, we are overriding a template from a parent class; this is only allowed during script compilation
				// and only when importing a subobject for a CDO (inheritance isn't allowed in a nested subobject) so check all that stuff first
				if ( !GIsUCCMake || !SubobjectOuter->HasAnyFlags(RF_ClassDefaultObject) )
				{
					Warn->Logf(NAME_Error, TEXT("BEGIN OBJECT: Missing class in subobject/component definition: %s"), *StrLine);
					return NULL;
				}

				// next, verify that a template actually exists in the parent class
				UClass* ParentClass = ComponentOwnerClass->GetSuperClass();
				check(ParentClass);

				UObject* ParentCDO = ParentClass->GetDefaultObject();
				check(ParentCDO);

				BaseTemplate = UObject::StaticFindObjectFast(UObject::StaticClass(), ParentCDO, TemplateName);
				if ( BaseTemplate == NULL )
				{
					// wasn't found
					Warn->Logf(NAME_Error, TEXT("BEGIN OBJECT: No base template named %s found in parent class %s: %s"), *TemplateName.ToString(), *ParentClass->GetName(), *StrLine);
					return NULL;
				}

				TemplateClass = BaseTemplate->GetClass();
			}

			// @todo nested components: the last check for RF_ClassDefaultObject is stopping nested components,
			// because the outer won't be a default object

			check(TemplateClass);
			if( TemplateClass->IsChildOf(UComponent::StaticClass()) && SubobjectOuter->HasAnyFlags(RF_ClassDefaultObject))
			{
				UComponent* OverrideComponent = ComponentOwnerClass->ComponentNameToDefaultObjectMap.FindRef(TemplateName);
				if(OverrideComponent)
				{
					if ( BaseTemplate == NULL )
					{
						// BaseTemplate should only be NULL if the Begin Object line specified a class
						Warn->Logf(NAME_Error, TEXT("BEGIN OBJECT: The component name %s is already used (if you want to override the component, don't specify a class): %s"), *TemplateName.ToString(), *StrLine);
						return NULL;
					}

					// the component currently in the component template map and the base template should be the same
					checkf(OverrideComponent==BaseTemplate,TEXT("OverrideComponent: '%s'   BaseTemplate: '%s'"), *OverrideComponent->GetFullName(), *BaseTemplate->GetFullName());
				}


				UClass*	ComponentSuperClass = TemplateClass;

				// Propagate object flags to the sub object.
				const EObjectFlags NewFlags = SubobjectOuter->GetMaskedFlags( RF_PropagateToSubObjects );

				UComponent* ComponentTemplate = ConstructObject<UComponent>(
					TemplateClass,
					SubobjectOuter,
					TemplateName,
					NewFlags | ((TemplateClass->ClassFlags&CLASS_Localized) ? RF_PerObjectLocalized : 0),
					OverrideComponent,
					SubobjectOuter,
					&InstanceGraph
					);

				ComponentTemplate->TemplateName = TemplateName;

				// if we had a template, update any properties in this class to point to the new component instead of the template
				if (OverrideComponent)
				{
					// replace all properties in this subobject outer' class that point to the original subobject with the new subobject
					TMap<UComponent*, UComponent*> ReplacementMap;
					ReplacementMap.Set(OverrideComponent, ComponentTemplate);
					FArchiveReplaceObjectRef<UComponent> ReplaceAr(SubobjectOuter, ReplacementMap, FALSE, FALSE, FALSE);

					InstanceGraph.AddComponentPair(OverrideComponent, ComponentTemplate);
				}

				// import the properties for the subobject
				SourceText = ImportObjectProperties(
					(BYTE*)ComponentTemplate, 
					SourceText, 
					TemplateClass, 
					SubobjectRoot, 
					ComponentTemplate, 
					Warn, 
					Depth+1,
					ContextSupplier ? ContextSupplier->CurrentLine : 0,
					&InstanceGraph
					);

				ComponentNameToInstanceMap.Set(TemplateName,ComponentTemplate);
				if ( GIsUCCMake == TRUE )
				{
					if ( !bSubObject || SubobjectOuter == SubobjectRoot )
					{
						ComponentOwnerClass->ComponentNameToDefaultObjectMap.Set(TemplateName,ComponentTemplate);

						// now add any components that were instanced as a result of references to other components in the class
						TArray<UComponent*> ReferencedComponents;
						InstanceGraph.RetrieveComponents(SubobjectRoot, ReferencedComponents, FALSE);

						for ( INT ComponentIndex = 0; ComponentIndex < ReferencedComponents.Num(); ComponentIndex++ )
						{
							UComponent* Component = ReferencedComponents(ComponentIndex);
							ComponentNameToInstanceMap.Set(Component->GetFName(), Component);
							ComponentOwnerClass->ComponentNameToDefaultObjectMap.Set(Component->GetFName(), Component);
						}
					}
				}

				// let the UComponent hook up it's source pointers, but only for placed subobjects (if the outer is a class, we are in the .u file, no linking)
				if ( !ComponentTemplate->IsTemplate() )
				{
					ComponentTemplate->LinkToSourceDefaultObject(NULL, ComponentOwnerClass, TemplateName);
					// make sure that this points to something
					// @todo: handle this error nicely, in case there was a poorly typed .t3d file...)
					check(ComponentTemplate->ResolveSourceDefaultObject() != NULL);
				}
			}
			// handle the non-template case (subobjects and non-template components)
			else
			{
				// prevent component subobject definitions inside of other subobject definitions, as the correct instancing behavior in this situation is undefined
				if ( TemplateClass->IsChildOf(UComponent::StaticClass()) && SubobjectRoot->HasAnyFlags(RF_ClassDefaultObject) )
				{
					Warn->Logf(NAME_Error,TEXT("Nested component definitions (%s in %s) in class files are currently not supported: %s"), *TemplateName.ToString(), *SubobjectOuter->GetFullName(), *StrLine);
					return NULL;
				}

				// if the class isn't marked as compiled, only allow import if it's intrinsic (intrinsic classes will never be compiled)
				if( !TemplateClass->HasAnyClassFlags(CLASS_Compiled|CLASS_Intrinsic) )
				{
					Warn->Logf( NAME_Error, TEXT("BEGIN OBJECT: Can't create subobject as Class %s hasn't been compiled yet."), *TemplateClass->GetName() );
					return NULL;
				}

				// don't allow Actor-derived subobjects
				if ( TemplateClass->IsChildOf(AActor::StaticClass()) )
				{
					Warn->Logf(NAME_Error,TEXT("Cannot create subobjects from Actor-derived classes: %s"), *StrLine);
					return NULL;
				}

				UObject* TemplateObject;

				// for .t3d placed components that aren't in the class, and have no template, we need the object name
				FName	ObjectName = NAME_None;
				if ( !Parse(Str, TEXT("ObjName="), ObjectName) )
				{
					ObjectName = TemplateName;
				}

				// if an archetype was specified in the Begin Object block, use that as the template for the ConstructObject call.
				// @fixme subobjects: Generalize this whole system into using Archetype= for all Object importing
				FString ArchetypeName;
				UObject* Archetype = NULL;
				if (Parse(Str, TEXT("Archetype="), ArchetypeName))
				{
					// if given a name, break it up along the ' so separate the class from the name
					TArray<FString> Refs;
					ArchetypeName.ParseIntoArray(&Refs, TEXT("'"), TRUE);
					// find the class
					UClass* ArchetypeClass = (UClass*)UObject::StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *Refs(0));
					if (ArchetypeClass)
					{
						// if we had the class, find the archetype
						// @fixme ronp subobjects: this _may_ need StaticLoadObject, but there is currently a bug in StaticLoadObject that it can't take a non-package pathname properly
						Archetype = UObject::StaticFindObject(ArchetypeClass, ANY_PACKAGE, *Refs(1));
					}
				}
				else
				{
					Archetype = BaseTemplate;
				}

				UObject* ExistingObject;
				if ((ExistingObject = FindObject<UObject>(SubobjectOuter, *ObjectName.ToString())) != NULL)
				{
					// if we're overriding a subobject declared in a parent class, we should already have an object with that name that
					// was instanced when ComponentOwnerClass's CDO was initialized; if so, it's archetype should be the BaseTemplate.  If it
					// isn't, then there are two unrelated subobject definitions using the same name.
					if ( ExistingObject->GetArchetype() != BaseTemplate )
					{
						if ( GIsUCCMake )
						{
							Warn->Logf(NAME_Error, TEXT("BEGIN OBJECT: name %s redefined: %s"), *ObjectName.ToString(), *StrLine);
						}
						else
						{
							ExistingObject->Rename();
						}
					}
					else if ( BaseTemplate == NULL )
					{
						// BaseTemplate should only be NULL if the Begin Object line specified a class
						Warn->Logf(NAME_Error, TEXT("BEGIN OBJECT: A subobject named %s is already declared in a parent class.  If you intended to override that subobject, don't specify a class in the derived subobject definition: %s"), *TemplateName.ToString(), *StrLine);
						return NULL;
					}
				}

				// Propagate object flags to the sub object.
				const EObjectFlags NewFlags = SubobjectOuter->GetMaskedFlags( RF_PropagateToSubObjects );

				// disable object instancing if we're importing default properties; it will happen later in ImportDefaultProperties()
				UBOOL bWasInstancingObjects = InstanceGraph.IsObjectInstancingEnabled();
				if (PortFlags & PPF_ParsingDefaultProperties)
				{
					InstanceGraph.EnableObjectInstancing(FALSE);
				}

				// create the subobject
				TemplateObject = ConstructObject<UObject>(
					TemplateClass,
					SubobjectOuter,
					ObjectName,
					NewFlags | ((TemplateClass->ClassFlags&CLASS_Localized) ? RF_PerObjectLocalized : 0),
					Archetype,
					SubobjectRoot,
					&InstanceGraph
					);

				InstanceGraph.EnableObjectInstancing(bWasInstancingObjects);
				// replace any already-overridden non-component subobjects with their new instances in this subobject
				{
					FArchiveReplaceObjectRef<UObject> ReplaceAr(TemplateObject, *InstanceGraph.GetSourceToDestinationMap(), FALSE, FALSE, TRUE);
				}

				if ( Archetype != NULL && !Archetype->HasAnyFlags(RF_ClassDefaultObject) )
				{
					if ( TemplateObject->IsA(UComponent::StaticClass()) )
					{
						InstanceGraph.AddComponentPair(Cast<UComponent>(Archetype), Cast<UComponent>(TemplateObject));
					}

					// replace all properties in the class we're importing with to the original subobject with the new subobject
					TMap<UObject*, UObject*> ReplacementMap;
					ReplacementMap.Set(Archetype, TemplateObject);
					FArchiveReplaceObjectRef<UObject> ReplaceAr(SubobjectOuter, ReplacementMap, FALSE, FALSE, TRUE);

					InstanceGraph.AddObjectPair(TemplateObject, Archetype);
				}

				SourceText = ImportObjectProperties( 
					(BYTE*)TemplateObject,
					SourceText,
					TemplateClass,
					SubobjectRoot,
					TemplateObject,
					Warn, 
					Depth+1,
					ContextSupplier ? ContextSupplier->CurrentLine : 0,
					&InstanceGraph
					);

				UComponent* ComponentSubobject = Cast<UComponent>(TemplateObject);
				if( ComponentSubobject != NULL )
				{
					ComponentNameToInstanceMap.Set(TemplateName,ComponentSubobject);
				}
			}
		}
		else if( ParseCommand(&Str,TEXT("CustomProperties")))
		{
			check(SubobjectOuter);

			SubobjectOuter->ImportCustomProperties(Str, Warn);
		}
		else if( GetEND(&Str,TEXT("Actor")) || GetEND(&Str,TEXT("DefaultProperties")) || GetEND(&Str,TEXT("structdefaultproperties")) || (GetEND(&Str,TEXT("Object")) && Depth) )
		{
			// End of properties.
			break;
		}
		else
		{
			// Property.
			TCHAR Token[4096];

			while( *Str==' ' || *Str==9 )
			{
				Str++;
			}
			
			const TCHAR* Start=Str;
			
			while( *Str && *Str!='=' && *Str!='(' && *Str!='[' && *Str!='.' )
			{
				Str++;
			}

			if( *Str )
			{
				appStrncpy( Token, Start, Str-Start+1 );

				// strip trailing whitespace on token
				INT l = appStrlen(Token);
				while( l && (Token[l-1]==' ' || Token[l-1]==9) )
				{
					Token[l-1] = 0;
					--l;
				}

				// Parse an array operation, if present.
				enum EArrayOp
				{
					ADO_None,
					ADO_Add,
					ADO_Remove,
					ADO_RemoveIndex,
					ADO_Empty,
				};

				EArrayOp	ArrayOp = ADO_None;
				if(*Str == '.')
				{
					Str++;
					if(ParseCommand(&Str,TEXT("Empty")))
					{
						ArrayOp = ADO_Empty;
					}
					else if(ParseCommand(&Str,TEXT("Add")))
					{
						ArrayOp = ADO_Add;
					}
					else if(ParseCommand(&Str,TEXT("Remove")))
					{
						ArrayOp = ADO_Remove;
					}
					else if (ParseCommand(&Str,TEXT("RemoveIndex")))
					{
						ArrayOp = ADO_RemoveIndex;
					}
				}

				UProperty* Property = FindField<UProperty>( ObjectStruct, Token );

				// this the default parent to use
				UObject* ImportTextParent = (GEditor->Bootstrapping && SubobjectOuter != NULL) ? SubobjectOuter : (UObject*)DestData;

				if( !Property )
				{
					// Check for a delegate property
					FString DelegateName = FString::Printf(TEXT("__%s__Delegate"), Token );
					Property = FindField<UDelegateProperty>( ObjectStruct, *DelegateName );
					if( !Property )
					{
						Warn->Logf( NAME_Warning, TEXT("Unknown property in defaults: %s (looked in %s)"), *StrLine, *ObjectStruct->GetName());
						continue;
					}
				}

				// If the property is native then we usually won't import it.  However, sometimes we want
				// an editor user to be able to copy/paste native properties, in which case the properly
				// will be marked as SerializeText.
				const UBOOL bIsNativeProperty = ( Property->PropertyFlags & CPF_Native );
				const UBOOL bAllowSerializeText = ( Property->PropertyFlags & CPF_SerializeText );
				if ( !GIsUCCMake &&
					 ( ( bIsNativeProperty && !bAllowSerializeText ) ||
					   (Property->PropertyFlags&(CPF_NoImport|CPF_DuplicateTransient)) != 0 ) )
				{
					// this property shouldn't be imported, so just skip it
					continue;
				}

				UArrayProperty*	ArrayProperty = Cast<UArrayProperty>(Property);
				if ( ArrayOp != ADO_None )
				{
					if ( ArrayProperty == NULL )
					{
						Warn->Logf(NAME_ExecWarning,TEXT("Array operation performed on non-array variable: %s"), *StrLine);
						continue;
					}

					// if we're compiling a class, we want to pass in that class for the parent of the component property, even if the property is inside a nested component
					if (GEditor->Bootstrapping && ArrayProperty->Inner->HasAnyPropertyFlags(CPF_Component))
					{
						ImportTextParent = SubobjectOuter;
					}

					if (ArrayOp == ADO_Empty)
					{
						FScriptArray*	Array = (FScriptArray*)(DestData + Property->Offset);
						Array->Empty(0,ArrayProperty->Inner->ElementSize);
					}
					else if (ArrayOp == ADO_Add || ArrayOp == ADO_Remove)
					{
						FScriptArray*	Array = (FScriptArray*)(DestData + Property->Offset);

						SkipWhitespace(Str);
						if(*Str++ != '(')
						{
							Warn->Logf( NAME_ExecWarning, TEXT("Missing '(' in default properties array operation: %s"), *StrLine);
							continue;
						}
						SkipWhitespace(Str);

						FString ValueText = ReadStructValue(Str, SourceText);
						Str = *ValueText;

						if(ArrayOp == ADO_Add)
						{
							INT Size = ArrayProperty->Inner->ElementSize;
							INT	Index = Array->AddZeroed(1, Size);
							BYTE* ElementData = (BYTE*)Array->GetData() + Index * Size;

							if (ArrayProperty->Inner->IsA(UDelegateProperty::StaticClass()))
							{
								FString Temp;
								if (appStrchr(Str, '.') == NULL)
								{
									// if no class was specified, use the class currently being imported
									Temp = Depth ? *SubobjectRoot->GetName() : *ObjectStruct->GetName();
									Temp = Temp + TEXT(".") + Str;
								}
								else
								{
									Temp = Str;
								}
								FScriptDelegate* D = (FScriptDelegate*)(ElementData);
								D->Object = NULL;
								D->FunctionName = NAME_None;
								const TCHAR* Result = ArrayProperty->Inner->ImportText(*Temp, ElementData, PortFlags, ImportTextParent);
								UBOOL bFailedImport = (Result == NULL || Result == *Temp);
								if (bFailedImport)
								{
									Warn->Logf(NAME_Warning, TEXT("Delegate assignment failed: %s"), *StrLine);
								}
							}
							else
							{
								UStructProperty* StructProperty = Cast<UStructProperty>(ArrayProperty->Inner,CLASS_IsAUStructProperty);
								if( StructProperty )
								{
									// Initialize struct defaults.
									StructProperty->CopySingleValue( ElementData, StructProperty->Struct->GetDefaults(), NULL );
								}

								const TCHAR* Result = ArrayProperty->Inner->ImportText(Str,ElementData,PortFlags,ImportTextParent);
								if ( Result == NULL || Result == Str )
								{
									Warn->Logf(NAME_Warning, TEXT("Unable to parse parameter value '%s' in defaultproperties array operation: %s"), Str, *StrLine);
									Array->Remove(Index, 1, Size);
								}
							}
						}
						else if(ArrayOp == ADO_Remove)
						{
							INT Size = ArrayProperty->Inner->ElementSize;

							BYTE* Temp = new BYTE[Size];
							appMemzero(Temp, Size);

							// export the value specified to a temporary buffer
							const TCHAR* Result = ArrayProperty->Inner->ImportText(Str,Temp,PortFlags,ImportTextParent);
							if ( Result == NULL || Result == Str )
							{
								Warn->Logf(NAME_Error, TEXT("Unable to parse parameter value '%s' in defaultproperties array operation: %s"), Str, *StrLine);
							}
							else
							{
								// find the array member corresponding to this value
								UBOOL bIsComponentsArray = ArrayProperty->GetFName() == NAME_Components && ArrayProperty->GetOwnerClass()->GetFName() == NAME_Actor;
								UBOOL bFound = FALSE;
								for(UINT Index = 0;Index < (UINT)Array->Num();Index++)
								{
									BYTE* DestData = (BYTE*)Array->GetData() + Index * Size;
									if(ArrayProperty->Inner->Identical(Temp,DestData))
									{
										if ( RemovedComponents && bIsComponentsArray )
										{
											RemovedComponents->AddUniqueItem( *(UComponent**)DestData );
										}
										Array->Remove(Index--,1,ArrayProperty->Inner->ElementSize);
										bFound = TRUE;
									}
								}
								if (!bFound)
								{
									Warn->Logf(NAME_Warning, TEXT("%s.Remove(): Value not found in array"), *ArrayProperty->GetName());
								}
							}
							ArrayProperty->Inner->DestroyValue(Temp);
							delete [] Temp;
						}
					}
					else if (ArrayOp == ADO_RemoveIndex)
					{
						FScriptArray*	Array = (FScriptArray*)(DestData + Property->Offset);

						SkipWhitespace(Str);
						if(*Str++ != '(')
						{
							Warn->Logf( NAME_ExecWarning, TEXT("Missing '(' in default properties array operation:: %s"), *StrLine );
							continue;
						}
						SkipWhitespace(Str);

						FString strIdx;
						while (*Str != ')')
						{		
							strIdx += *Str;
							Str++;
						}
						INT removeIdx = appAtoi(*strIdx);

						UBOOL bIsComponentsArray = ArrayProperty->GetFName() == NAME_Components && ArrayProperty->GetOwnerClass()->GetFName() == NAME_Actor;
						if (bIsComponentsArray && RemovedComponents )
						{
							if ( removeIdx < Array->Num() )
							{
								BYTE* ElementData = (BYTE*)Array->GetData() + removeIdx * ArrayProperty->Inner->ElementSize;
								RemovedComponents->AddUniqueItem( *(UComponent**)ElementData );
							}
						}
						Array->Remove(removeIdx,1,ArrayProperty->Inner->ElementSize);
					}
				}
				else
				{
					// try to read an array index
					INT Index = ReadArrayIndex( ObjectStruct, Cast<UClass>(SubobjectRoot), Str, Warn);

					// check for out of bounds on static arrays
					if (ArrayProperty == NULL && Index >= Property->ArrayDim)
					{
						Warn->Logf( NAME_Warning, TEXT("Out of bound array default property (%i/%i): %s"), Index, Property->ArrayDim, *StrLine );
						continue;
					}

					
					// check to see if this property has already imported data
					FDefinedProperty D;
					D.Property = Property;
					D.Index = Index;
					if( DefinedProperties.FindItemIndex( D ) != INDEX_NONE )
					{
						Warn->Logf( NAME_Warning, TEXT("redundant data: %s"), *StrLine );
						continue;
					}
					DefinedProperties.AddItem( D );

					// strip whitespace before =
					SkipWhitespace(Str);
					if( *Str++!='=' )
					{
						Warn->Logf( NAME_Warning, TEXT("Missing '=' in default properties assignment: %s"), *StrLine );
						continue;
					}
					// strip whitespace after =
					SkipWhitespace(Str);

					// limited multi-line support, look for {...} sequences and condense to a single entry
					FString FullText = ReadStructValue(Str, SourceText);

					// set the pointer to our new text
					Str = *FullText;
					if( appStricmp(*Property->GetName(),TEXT("Name"))!=0 )
					{
						l = appStrlen(Str);
						while( l && (Str[l-1]==';' || Str[l-1]==' ' || Str[l-1]==9) )
						{
							*(TCHAR*)(&Str[l-1]) = 0;
							--l;
						}
						if( Property->IsA(UStrProperty::StaticClass()) && (!l || *Str != '"' || Str[l-1] != '"') )
							Warn->Logf( NAME_Warning, TEXT("Missing '\"' in string default properties: %s"), *StrLine );

						if (Index > -1 && ArrayProperty != NULL) //set single dynamic array element
						{
							FScriptArray* Array=(FScriptArray*)(DestData + Property->Offset);
							if (Index>=Array->Num())
							{
								INT NumToAdd = Index - Array->Num() + 1;
								Array->AddZeroed(NumToAdd, ArrayProperty->Inner->ElementSize);
								UStructProperty* StructProperty = Cast<UStructProperty>(ArrayProperty->Inner, CLASS_IsAUStructProperty);
								if ( StructProperty && StructProperty->Struct->GetDefaultsCount() )
								{
									// initialize struct defaults for each element we had to add to the array
									for (INT i = 1; i <= NumToAdd; i++)
									{
										StructProperty->CopySingleValue((BYTE*)Array->GetData() + ((Array->Num() - i) * ArrayProperty->Inner->ElementSize), StructProperty->Struct->GetDefaults());
									}
								}
							}

							// if we're compiling a class, we want to pass in that class for the parent of the component property, even if the property is inside a nested component
							if ( GEditor->Bootstrapping )
							{
								ImportTextParent = SubobjectOuter;
							}

							if ( !GIsUCCMake && appStrlen(Str) == 0 )
							{
								// if we're not importing default properties for classes (i.e. we're pasting something in the editor or something)
								// and there is no property value for this element, skip it, as that means that the value of this element matches
								// the intrinsic null value of the property type and we want to skip importing it
								continue;
							}
							FStringOutputDevice ImportError;
							const TCHAR* Result;
							if (ArrayProperty->Inner->IsA(UDelegateProperty::StaticClass()))
							{
								FString Temp;
								if( appStrchr(Str, '.') == NULL)
								{
									// if no class was specified, use the class currently being imported
									Temp = Depth ? *SubobjectRoot->GetName() : *ObjectStruct->GetName();
									Temp = Temp + TEXT(".") + Str;
								}
								else
								{
									Temp = Str;
								}
								FScriptDelegate* D = (FScriptDelegate*)((BYTE*)Array->GetData() + Index * ArrayProperty->Inner->ElementSize);
								D->Object = NULL;
								D->FunctionName = NAME_None;
								Result = ArrayProperty->Inner->ImportText(*Temp, (BYTE*)Array->GetData() + Index * ArrayProperty->Inner->ElementSize, PortFlags, ImportTextParent, &ImportError);
								UBOOL bFailedImport = (Result == NULL || Result == *Temp);
								if (bFailedImport)
								{
									Warn->Logf(NAME_Warning, TEXT("Delegate assignment failed: %s"), *StrLine);
								}
							}
							else
							{
								Result = ArrayProperty->Inner->ImportText( Str, (BYTE*)Array->GetData() + Index * ArrayProperty->Inner->ElementSize, PortFlags, ImportTextParent, &ImportError );
							}
							UBOOL bFailedImport = (Result == NULL || Result == Str);
							// @todo: only run this code when importing from a .uc file, not from a .t3d file
							if (bFailedImport)
							{
								if ( GIsUCCMake )
								{
									// The property failed to import the value - this means the text was't valid for this property type
									// it could be a named constant, so let's try checking for that first

									// If this is a subobject definition, search for a matching const in the containing (top-level) class first
									UConst* Const = FindField<UConst>( bSubObject ? ComponentOwnerClass : ObjectStruct, Str );
									if ( Const == NULL && bSubObject )
									{
										// if it still wasn't found, try searching the subobject's class
										Const = FindField<UConst>(ObjectStruct, Str);
									}

									if ( Const )
									{
										// found a const matching the value specified - retry to import the text using the value of the const
										Result = ArrayProperty->Inner->ImportText( *Const->Value, (BYTE*)Array->GetData() + Index * ArrayProperty->Inner->ElementSize, PortFlags, ImportTextParent, &ImportError );
										bFailedImport = (Result == NULL || Result == *Const->Value);
									}
								}

								if ( bFailedImport )
								{
									if ( ImportError.Len() )
									{
										TArray<FString> ImportErrors;
										ImportError.ParseIntoArray(&ImportErrors,LINE_TERMINATOR,TRUE);

										for ( INT ErrorIndex = 0; ErrorIndex < ImportErrors.Num(); ErrorIndex++ )
										{
											Warn->Logf(NAME_Warning,*ImportErrors(ErrorIndex));
										}
									}
									else
									{
										Warn->Logf(NAME_Warning, TEXT("Invalid property value in defaults: %s"), *StrLine);
									}
								}

								// Spit any error we had while importing property
								else if( ImportError.Len() )
								{
									TArray<FString> ImportErrors;
									ImportError.ParseIntoArray(&ImportErrors,LINE_TERMINATOR,TRUE);

									for ( INT ErrorIndex = 0; ErrorIndex < ImportErrors.Num(); ErrorIndex++ )
									{
										Warn->Logf(NAME_Warning,*ImportErrors(ErrorIndex));
									}
								}
							}
							// Spit any error we had while importing property
							else if( ImportError.Len() )
							{
								TArray<FString> ImportErrors;
								ImportError.ParseIntoArray(&ImportErrors,LINE_TERMINATOR,TRUE);

								for ( INT ErrorIndex = 0; ErrorIndex < ImportErrors.Num(); ErrorIndex++ )
								{
									Warn->Logf(NAME_Warning,*ImportErrors(ErrorIndex));
								}
							}
						}
						else if( Property->IsA(UDelegateProperty::StaticClass()) )
						{
							if (Index == -1) Index = 0;
							FString Temp;
							if( appStrchr(Str, '.')==NULL )
							{
								// if no class was specified, use the class currently being imported
								Temp = Depth ? *SubobjectRoot->GetName() : *ObjectStruct->GetName();
								Temp = Temp + TEXT(".") + Str;
							}
							else
								Temp = Str;
							FScriptDelegate* D = (FScriptDelegate*)(DestData + Property->Offset + Index*Property->ElementSize);
							D->Object = NULL;
							D->FunctionName = NAME_None;
							const TCHAR* Result = Property->ImportText( *Temp, DestData + Property->Offset + Index*Property->ElementSize, PortFlags, ImportTextParent );
							UBOOL bFailedImport = (Result == NULL || Result == *Temp);
							if ( bFailedImport )
							{
								Warn->Logf( NAME_Warning, TEXT("Delegate assignment failed: %s"), *StrLine );
							}
						}
						else
						{
							if( Index == INDEX_NONE )
							{
								Index = 0;
							}

							FStringOutputDevice ImportError;

							// if we're compiling a class, we want to pass in that class for the parent of the component property, even if the property is inside a nested component
							if ( GEditor->Bootstrapping )
							{
								ImportTextParent = SubobjectOuter;
							}

							if ( !GIsUCCMake && appStrlen(Str) == 0 )
							{
								// if we're not importing default properties for classes (i.e. we're pasting something in the editor or something)
								// and there is no property value for this element, skip it, as that means that the value of this element matches
								// the intrinsic null value of the property type and we want to skip importing it
								continue;
							}

							const TCHAR* Result = Property->ImportText(Str, DestData + Property->Offset + Index*Property->ElementSize, PortFlags, ImportTextParent, &ImportError);
							UBOOL bFailedImport = (Result == NULL || Result == Str);

							// @todo: only run this code when importing from a .uc file, not from a .t3d file
							if( bFailedImport )
							{
								// The property failed to import the value - this means the text was't valid for this property type
								// it could be a named constant, so let's try checking for that first

								if ( GIsUCCMake )
								{
									// If this is a subobject definition, search for a matching const in the containing (top-level) class first
									UConst* Const = FindField<UConst>( bSubObject ? SubobjectRoot->GetClass() : ObjectStruct, Str );
									if ( Const == NULL && bSubObject )
									{
										// if it still wasn't found, try searching the subobject's class
										Const = FindField<UConst>(ObjectStruct, Str);
									}

									if ( Const )
									{
										// found a const matching the value specified - retry to import the text using the value of the const
										Result = Property->ImportText(*Const->Value, DestData + Property->Offset + Index*Property->ElementSize, PortFlags, ImportTextParent, &ImportError);
										bFailedImport = (Result == NULL || Result == *Const->Value);
									}
								}

								if( bFailedImport )
								{
									if( ImportError.Len() )
									{
										TArray<FString> ImportErrors;
										ImportError.ParseIntoArray(&ImportErrors,LINE_TERMINATOR,TRUE);

										for ( INT ErrorIndex = 0; ErrorIndex < ImportErrors.Num(); ErrorIndex++ )
										{
											Warn->Logf(NAME_Warning,*ImportErrors(ErrorIndex));
										}
									}
									else
									{
										Warn->Logf(NAME_Warning, TEXT("Invalid property value in defaults: %s"), *StrLine);
									}
								}
								// Spit any error we had while importing property
								else if( ImportError.Len() )
								{
									TArray<FString> ImportErrors;
									ImportError.ParseIntoArray(&ImportErrors,LINE_TERMINATOR,TRUE);

									for ( INT ErrorIndex = 0; ErrorIndex < ImportErrors.Num(); ErrorIndex++ )
									{
										Warn->Logf(NAME_Warning,*ImportErrors(ErrorIndex));
									}
								}
							}
							// Spit any error we had while importing property
							else if( ImportError.Len() )
							{
								TArray<FString> ImportErrors;
								ImportError.ParseIntoArray(&ImportErrors,LINE_TERMINATOR,TRUE);

								for ( INT ErrorIndex = 0; ErrorIndex < ImportErrors.Num(); ErrorIndex++ )
								{
									Warn->Logf(NAME_Warning,*ImportErrors(ErrorIndex));
								}
							}
						}
					}
				}
			}
		}
	}

	// Prepare brush.
	if( ImportedBrush && ObjectStruct->IsChildOf(ABrush::StaticClass()) )
	{
		check(GIsEditor);
		ABrush* Actor = (ABrush*)DestData;
		check(Actor->BrushComponent);
		if( Actor->IsStatic() )
		{
			// Prepare static brush.
			Actor->SetFlags       ( RF_NotForClient | RF_NotForServer );
			Actor->Brush->SetFlags( RF_NotForClient | RF_NotForServer );
		}
		else
		{
			// Prepare moving brush.
			FBSPOps::csgPrepMovingBrush( Actor );
		}
	}

	return SourceText;
}

/**
 * Import the entire default properties block for the class specified
 * 
 * @param	Class	the class to import defaults for
 * @param	Text	buffer containing the text to be imported
 * @param	Warn	output device for log messages
 * @param	Depth	current nested subobject depth
 *
 * @return	NULL if the default values couldn't be imported
 */
const TCHAR* ImportDefaultProperties(
	UClass*				Class,
	const TCHAR*		Text,
	FFeedbackContext*	Warn,
	INT					Depth,
	INT					LineNumber
	)
{
	UBOOL	Success = 1;

	FDefaultPropertiesContextSupplier Context(*Class->GetOuter()->GetName(), *Class->GetName(), LineNumber);
	ContextSupplier = &Context;
	Warn->SetContext(ContextSupplier);

	// this tracks any components that were removed from the class via the Components.Remove() functionality
	TArray<UComponent*> RemovedComponents;

	// Parse the default properties.
	FObjectInstancingGraph InstanceGraph;
	if(Text)
	{
		if ( Class != UObject::StaticClass() )
		{
			InstanceGraph.SetDestinationRoot(Class->GetDefaultObject());
		}

		try
		{
			Text = ImportProperties(
						Class->GetDefaults(),
						Text,
						Class,
						NULL,
						Class->GetDefaultObject(),
						Class->ComponentNameToDefaultObjectMap,
						&RemovedComponents,
						Warn,
						Depth,
						InstanceGraph
						);
		}
		catch ( TCHAR* ErrorMsg )
		{
			Warn->Log( NAME_Error, ErrorMsg );
			Text = NULL;
		}
		Success = Text != NULL;
	}

	if( Success )
	{
		// if we've removed items from the Components array we'll need
		// to also remove that component from the owner class's ClassNameToDefaultObjectMap, assuming
		// that there are no other references to that UComponent.
		RemoveComponentsFromClass(Class->GetDefaultObject(), RemovedComponents);

		UClass* SuperClass = Class->GetSuperClass();
		if ( SuperClass && (SuperClass->ClassFlags&CLASS_HasComponents) != 0 && SuperClass->ComponentNameToDefaultObjectMap.Num() > 0 )
		{
			CopyInheritedComponents(Class,InstanceGraph);
		}

		// if compiling script, we'll need to duplicate any subobjects that were inherited from the parent class
		// so that all references to those subobjects are remapped to the correct version
		if ( GIsUCCMake && SuperClass != NULL )
		{
			UObject* ParentCDO = SuperClass->GetDefaultObject();
			UObject* CurrentCDO = Class->GetDefaultObject();

			// first determine whether we need to do anything by checking whether we even have any inherited subobjects
			TArray<UObject*> InheritedSubobjects;
 			FArchiveObjectReferenceCollector InheritedSubobjectDuplicator( &InheritedSubobjects, ParentCDO, TRUE, TRUE, TRUE, FALSE );
			CurrentCDO->Serialize(InheritedSubobjectDuplicator);
			if ( InheritedSubobjects.Num() > 0 )
			{
				TMap<UObject*,UObject*> SubobjectReplacementMap;
	
//  				warnf(TEXT("Found %i inherited subobjects for %s"), InheritedSubobjects.Num(), *CurrentCDO->GetName());
				for ( INT ObjIdx = 0; ObjIdx < InheritedSubobjects.Num(); ObjIdx++ )
				{
//  					warnf(TEXT("  %i) %s"), ObjIdx, *InheritedSubobjects(ObjIdx)->GetFullName());
					// skip all components.
					UComponent* Comp = Cast<UComponent>(InheritedSubobjects(ObjIdx));
					if ( Comp == NULL )
					{
						UObject* SubobjectCopy = UObject::StaticDuplicateObject(InheritedSubobjects(ObjIdx), CurrentCDO, CurrentCDO, *InheritedSubobjects(ObjIdx)->GetName(), RF_AllFlags, NULL, TRUE);
						SubobjectReplacementMap.Set(InheritedSubobjects(ObjIdx), SubobjectCopy);
					}
				}


				FArchiveReplaceObjectRef<UObject> ReplaceAr(CurrentCDO, SubobjectReplacementMap, FALSE, TRUE, TRUE);
			}
		}
	}

	ContextSupplier = NULL;
	Warn->SetContext(NULL);
	return Text;
}



/**
 * Parse and import text as property values for the object specified.
 * 
 * @param	InParams	Parameters for object import; see declaration of FImportObjectParams.
 *
 * @return	NULL if the default values couldn't be imported
 */

const TCHAR* ImportObjectProperties( FImportObjectParams& InParams )
{
	TMap<FName,UComponent*>	ComponentNameToInstanceMap;

	FDefaultPropertiesContextSupplier Supplier;
	if ( InParams.LineNumber != INDEX_NONE )
	{
		if ( InParams.SubobjectRoot == NULL )
		{
			Supplier.PackageName = InParams.ObjectStruct->GetOwnerClass()->GetOutermost()->GetName();
			Supplier.ClassName = InParams.ObjectStruct->GetOwnerClass()->GetName();
			Supplier.CurrentLine = InParams.LineNumber;

			ContextSupplier = &Supplier;
		}
		else
		{
			checkf(!GIsUCCMake || ContextSupplier, TEXT("NULL context supplier encountered while importing defaults for %s: SubobjectRoot:%s  SubobjectOuter:%s   LineNumber:%i"), *InParams.ObjectStruct->GetFullName(), *InParams.SubobjectRoot->GetFullName(), InParams.SubobjectOuter ? *InParams.SubobjectOuter->GetFullName() : TEXT(""), InParams.LineNumber);
			if ( ContextSupplier != NULL )
			{
				ContextSupplier->CurrentLine = InParams.LineNumber;
			}
		}
		InParams.Warn->SetContext(ContextSupplier);
	}

	if ( InParams.bShouldCallEditChange && InParams.SubobjectOuter != NULL && GIsUCCMake == FALSE )
	{
		InParams.SubobjectOuter->PreEditChange(NULL);
	}

	FObjectInstancingGraph* CurrentInstanceGraph = InParams.InInstanceGraph;
	if ( InParams.SubobjectRoot != NULL && InParams.SubobjectRoot != UObject::StaticClass()->GetDefaultObject() )
	{
		if ( CurrentInstanceGraph == NULL )
		{
			CurrentInstanceGraph = new FObjectInstancingGraph;
		}
		CurrentInstanceGraph->SetDestinationRoot(InParams.SubobjectRoot);
	}

 	FObjectInstancingGraph TempGraph; 
	FObjectInstancingGraph& InstanceGraph = CurrentInstanceGraph ? *CurrentInstanceGraph : TempGraph;

	// Parse the object properties.
	const TCHAR* NewSourceText =
		ImportProperties(
			InParams.DestData,
			InParams.SourceText,
			InParams.ObjectStruct,
			InParams.SubobjectRoot,
			InParams.SubobjectOuter,
			ComponentNameToInstanceMap,
			NULL,
			InParams.Warn,
			InParams.Depth,
			InstanceGraph
			);

	if ( InParams.SubobjectOuter != NULL )
	{
		check(InParams.SubobjectRoot);

		// Update the object properties to point to the newly imported component objects.
		// Templates inside classes never need to have components instanced.
 		if ( !InParams.SubobjectRoot->HasAnyFlags(RF_ClassDefaultObject) )
		{
			for ( TMap<FName,UComponent*>::TIterator It(ComponentNameToInstanceMap); It; ++It )
			{
				FName ComponentName = It.Key();
				UComponent* Component = It.Value();
				UComponent* ComponentArchetype = Cast<UComponent>(Component->GetArchetype());

				if ( InstanceGraph.IsInitialized() && !ComponentArchetype->HasAnyFlags(RF_ClassDefaultObject) )
				{
					InstanceGraph.AddComponentPair(ComponentArchetype, Component);
				}
			}

			UObject* SubobjectArchetype = InParams.SubobjectOuter->GetArchetype();
			BYTE* DefaultData = (BYTE*)SubobjectArchetype;

			InParams.ObjectStruct->InstanceComponentTemplates(InParams.DestData, DefaultData, SubobjectArchetype ? SubobjectArchetype->GetClass()->GetPropertiesSize() : NULL,
				InParams.SubobjectOuter, CurrentInstanceGraph);
		}

		if ( InParams.bShouldCallEditChange && !GIsUCCMake )
		{
			// notify the object that it has just been imported
			InParams.SubobjectOuter->PostEditImport();

			// notify the object that it has been edited
			InParams.SubobjectOuter->PostEditChange();
		}
	}

	if ( InParams.LineNumber != INDEX_NONE )
	{
		if ( ContextSupplier == &Supplier )
		{
			ContextSupplier = NULL;
			InParams.Warn->SetContext(NULL);
		}
	}

	// if we created the instance graph, delete it now
	if ( CurrentInstanceGraph != NULL && InParams.InInstanceGraph == NULL )
	{
		delete CurrentInstanceGraph;
		CurrentInstanceGraph = NULL;
	}

	return NewSourceText;
}



	
/**
 * Parse and import text as property values for the object specified.
 * 
 * @param	DestData			the location to import the property values to
 * @param	SourceText			pointer to a buffer containing the values that should be parsed and imported
 * @param	ObjectStruct		the struct for the data we're importing
 * @param	SubobjectRoot		the original object that ImportObjectProperties was called for.
 *								if SubobjectOuter is a subobject, corresponds to the first object in SubobjectOuter's Outer chain that is not a subobject itself.
 *								if SubobjectOuter is not a subobject, should normally be the same value as SubobjectOuter
 * @param	SubobjectOuter		the object corresponding to DestData; this is the object that will used as the outer when creating subobjects from definitions contained in SourceText
 * @param	Warn				ouptut device to use for log messages
 * @param	Depth				current nesting level
 * @param	LineNumber			used when importing defaults during script compilation for tracking which line we're currently for the purposes of printing compile errors
 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates; used when recursively calling ImportObjectProperties; generally
 *								not necessary to specify a value when calling this function from other code
 *
 * @return	NULL if the default values couldn't be imported
 */

const TCHAR* ImportObjectProperties(
	BYTE*				DestData,
	const TCHAR*		SourceText,
	UStruct*			ObjectStruct,
	UObject*			SubobjectRoot,
	UObject*			SubobjectOuter,
	FFeedbackContext*	Warn,
	INT					Depth,
	INT					LineNumber,
	FObjectInstancingGraph* InInstanceGraph
	)
{
	FImportObjectParams Params;
	{
		Params.DestData = DestData;
		Params.SourceText = SourceText;
		Params.ObjectStruct = ObjectStruct;
		Params.SubobjectRoot = SubobjectRoot;
		Params.SubobjectOuter = SubobjectOuter;
		Params.Warn = Warn;
		Params.Depth = Depth;
		Params.LineNumber = LineNumber;
		Params.InInstanceGraph = InInstanceGraph;

		// This implementation always calls PreEditChange/PostEditChange
		Params.bShouldCallEditChange = TRUE;
	}

	return ImportObjectProperties( Params );
}

