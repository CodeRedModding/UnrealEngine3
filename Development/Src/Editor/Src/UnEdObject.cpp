/*=============================================================================
	UnEdObject.cpp: Unreal Editor object manipulation code.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

#include "EditorPrivate.h"

//
//	UEditorEngine::RenameObject
//

void UEditorEngine::RenameObject(UObject* Object,UObject* NewOuter,const TCHAR* NewName)
{
	Object->Rename(NewName,NewOuter);
	Object->SetFlags(RF_Public | RF_Standalone);
	Object->MarkPackageDirty();
}

//
//	UEditorEngine::DeleteObject
//

void UEditorEngine::DeleteObject(UObject* Object)
{
	if(UObject::IsReferenced(Object,RF_Native | RF_Public,0))
	{
		appMsgf(0,TEXT("%s is in use"),*Object->GetFullName());
	}
	else
	{
		Object->MarkPackageDirty();
		delete Object;
	}
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

void SkipWhitespace(const TCHAR*& Str)
{
	while(*Str == ' ' || *Str == 9)
		Str++;
}

void CheckNewName(UObject* Outer,const TCHAR* Name)
{
	UObject*	OldObject;
	if((OldObject = UObject::StaticFindObject(UObject::StaticClass(),Outer,Name)) != NULL)
	{
		if(GEditor->Bootstrapping)
			warnf( NAME_Error, TEXT("BEGIN OBJECT: name %s redefined."),Name );
		else
			OldObject->Rename();
	}
}

UClass* CreateComponentClass(UClass* ComponentOwnerClass,UClass* ComponentSuperClass,const FString& ClassName)
{
	CheckNewName(ComponentOwnerClass->GetOutermost(),*ClassName);

	UClass*		ComponentClass = new(ComponentOwnerClass->GetOutermost(),*ClassName,RF_Public) UClass(ComponentSuperClass);
	FArchive	DummyAr;
	ComponentClass->Link(DummyAr,1);
	ComponentClass->ClassFlags = ComponentSuperClass->ClassFlags | CLASS_Hidden;
	ComponentClass->HideCategories = ComponentSuperClass->HideCategories;
	ComponentClass->GetDefaultObject()->InitClassDefaultObject(ComponentClass,1);
	ComponentClass->PropagateStructDefaults();
	ComponentClass->ComponentOwnerClass = ComponentOwnerClass;
	ComponentClass->ComponentClassToNameMap = ComponentSuperClass->ComponentClassToNameMap;
	ComponentClass->ComponentNameToDefaultObjectMap = ComponentSuperClass->ComponentNameToDefaultObjectMap;
	return ComponentClass;
}

/**
 * Instance the components inherited from a class's super class.
 *
 * @param	Class - The class to instance the components for.
 */

void InstanceSuperClassComponents(UClass* Class)
{
	// Create a new class/default object for each component.
	for(TMap<FName,UComponent*>::TIterator It(Class->ComponentNameToDefaultObjectMap);It;++It)
	{
		if(It.Value()->GetOuter() != Class)
		{
			CheckNewName(Class,*It.Key());

			UClass*	ComponentClass = CreateComponentClass(Class,It.Value()->GetClass(),FString::Printf(TEXT("%s_%s_Class"),Class->GetName(),*It.Key()));
			InstanceSuperClassComponents(ComponentClass);

			UComponent*	Component = ConstructObject<UComponent>(
				ComponentClass,
				Class,
				It.Key(),
				RF_Public | ((It.Value()->GetClass()->ClassFlags&CLASS_Localized) ? RF_PerObjectLocalized : 0)
				);

			Class->ComponentClassToNameMap.Set(Component->GetClass(),It.Key());
			Class->ComponentNameToDefaultObjectMap.Set(It.Key(),Component);
		}
	}

	// Update the default properties to point to the new component default objects.
	Class->FixupComponentReferences(Class->ComponentNameToDefaultObjectMap,&Class->Defaults(0),Class);
	for(TMap<FName,UComponent*>::TIterator It(Class->ComponentNameToDefaultObjectMap);It;++It)
		It.Value()->GetClass()->FixupComponentReferences(Class->ComponentNameToDefaultObjectMap,&It.Value()->GetClass()->Defaults(0),Class);
}

//
// Import text properties.
//
const TCHAR* ImportProperties(
	UStruct*					ObjectStruct,
	BYTE*						Object,
	ULevel*						Level,
	const TCHAR*				Data,
	UObject*					InParent,
	UObject*					SubObjectOuter,
	FFeedbackContext*			Warn,
	INT							Depth,
	TMap<FName,UComponent*>&	ComponentNameToInstanceMap,
	UObject*					ComponentOwner
	)
{
	check(ObjectStruct!=NULL);
	check(Object!=NULL);

	UClass*	ComponentOwnerClass = ComponentOwner->IsA(UClass::StaticClass()) ? CastChecked<UClass>(ComponentOwner) : ComponentOwner->GetClass();

	//!! Need to fix Parse(FString&...) to not limit to 4096 chars.  Temp workaround.
	INT StrLineLength = appStrlen(Data);
	TCHAR* StrLine = (TCHAR*)appAlloca( (StrLineLength + 1) * sizeof(TCHAR));

	// If bootstrapping, check the class we're BEGIN OBJECTing has had its properties imported.
	if( GEditor->Bootstrapping && Depth==0 )
	{
		const TCHAR* TempData = Data;
		while( ParseLine( &TempData, StrLine, StrLineLength ) )
		{
			const TCHAR* Str = StrLine;
			if( GetBEGIN(&Str,TEXT("Object")))
			{
				UClass* SubObjectClass;
				if(	ParseObject<UClass>(Str,TEXT("Class="),SubObjectClass,ANY_PACKAGE) )
				{
					if( (SubObjectClass->ClassFlags&CLASS_NeedsDefProps) )
						return NULL;
				}
			}
		}
	}

    TArray<FDefinedProperty> DefinedProperties;

	// Parse all objects stored in the actor.
	// Build list of all text properties.
	UBOOL ImportedBrush = 0;
	while( ParseLine( &Data, StrLine, StrLineLength ) )
	{
		const TCHAR* Str = StrLine;

		if( GetBEGIN(&Str,TEXT("Brush")) && ObjectStruct->IsChildOf(ABrush::StaticClass()) )
		{
			// Parse brush on this line.
			TCHAR BrushName[NAME_SIZE];
			if( Parse( Str, TEXT("Name="), BrushName, NAME_SIZE ) )
			{
				// If a brush with this name already exists in the
				// level, rename the existing one.  This is necessary
				// because we can't rename the brush we're importing without
				// losing our ability to associate it with the actor properties
				// that reference it.
				UModel* ExistingBrush = FindObject<UModel>( InParent, BrushName );
				if( ExistingBrush )
					ExistingBrush->Rename();

				// Create model.
				UModelFactory* ModelFactory = new UModelFactory;
				ModelFactory->FactoryCreateText( Level,UModel::StaticClass(), InParent, BrushName, 0, NULL, TEXT("t3d"), Data, Data+appStrlen(Data), GWarn );
				ImportedBrush = 1;
			}
		}
		else if( GetBEGIN(&Str,TEXT("StaticMesh")))
		{
			// Parse static mesh on this line.

			TCHAR	StaticMeshName[NAME_SIZE];

			if(Parse(Str,TEXT("Name="),StaticMeshName,NAME_SIZE))
			{
				// Rename any static meshes that have the desired name.

				UStaticMesh*	ExistingStaticMesh = FindObject<UStaticMesh>(InParent,StaticMeshName);

				if(ExistingStaticMesh)
					ExistingStaticMesh->Rename();

				// Parse the static mesh.

				UStaticMeshFactory*	StaticMeshFactory = new UStaticMeshFactory;

				StaticMeshFactory->FactoryCreateText(Level,UStaticMesh::StaticClass(),InParent,FName(StaticMeshName),0,NULL,TEXT("t3d"),Data,Data + appStrlen(Data),GWarn);

				delete StaticMeshFactory;
			}

		}
		else if( GetBEGIN(&Str,TEXT("Object")))
		{
			// Parse subobject default properties.
			// Note: default properties subobjects have compiled class as their Outer (used for localization).
			FName	SubObjectName = NAME_None;
			UClass*	SubObjectClass = NULL;

			ParseObject<UClass>(Str,TEXT("Class="),SubObjectClass,ANY_PACKAGE);
			Parse(Str,TEXT("Name="),SubObjectName);

			if( SubObjectClass && !(SubObjectClass->ClassFlags & CLASS_Compiled) )
				warnf( NAME_Error, TEXT("BEGIN OBJECT: Can't create subobject as Class %s hasn't been compiled yet."), SubObjectClass->GetName() );

			if( SubObjectClass && (SubObjectClass->ClassFlags & CLASS_NeedsDefProps) )
				return NULL;

			if( (!SubObjectClass || SubObjectClass->IsChildOf(UComponent::StaticClass())) && SubObjectOuter->IsA(UClass::StaticClass()))
			{
				if(SubObjectName == NAME_None)
					warnf(NAME_Error,TEXT("BEGIN OBJECT: Must specify valid name for component."));

				UComponent**	OverrideComponent = ComponentOwnerClass->ComponentNameToDefaultObjectMap.Find(SubObjectName);

				if(OverrideComponent)
				{
					if(SubObjectClass)
						warnf(NAME_Error,TEXT("BEGIN OBJECT: Cannot specify class when overriding a component"));
					SubObjectClass = (*OverrideComponent)->GetClass();
				}
				else if(!SubObjectClass)
				{
					warnf(NAME_Error,TEXT("BEGIN OBJECT: Class not specified for new component"));
					SubObjectClass = UComponent::StaticClass();
				}

				UClass*	ComponentSuperClass = SubObjectClass;

				FString	LegacyClassName;
				UClass*	LegacyClass = NULL;
				if(Parse(Str,TEXT("LegacyClassName="),LegacyClassName))
				{
					LegacyClass = ComponentSuperClass = CreateComponentClass(ComponentOwnerClass,ComponentSuperClass,LegacyClassName);
					Data = ImportDefaultProperties(ComponentSuperClass,Data,Warn,Depth + 1);
				}

				ComponentSuperClass = CreateComponentClass(ComponentOwnerClass,ComponentSuperClass,FString::Printf(TEXT("%s_%s_Class"),SubObjectOuter->GetName(),*SubObjectName));
				if(!LegacyClass)
					Data = ImportDefaultProperties(ComponentSuperClass,Data,Warn,Depth + 1);

				CheckNewName(SubObjectOuter,*SubObjectName);
				UObject*	SubObject = ConstructObject<UObject>(
					ComponentSuperClass,
					SubObjectOuter,
					SubObjectName,
					RF_Public | ((SubObjectClass->ClassFlags&CLASS_Localized) ? RF_PerObjectLocalized : 0)
					);

				if(SubObject->IsA(UComponent::StaticClass()))
				{
					UComponent*	Component = CastChecked<UComponent>(SubObject);

					ComponentNameToInstanceMap.Set(SubObjectName,Component);

					if(LegacyClass)
						ComponentOwnerClass->ComponentClassToNameMap.Set(LegacyClass,SubObjectName);

					ComponentOwnerClass->ComponentClassToNameMap.Set(ComponentSuperClass,SubObjectName);
					ComponentOwnerClass->ComponentNameToDefaultObjectMap.Set(SubObjectName,Component);
				}
			}
			else
			{
				if(!SubObjectClass)
				{
					warnf(NAME_Error,TEXT("BEGIN OBJECT: Class not specified for new component"));
					SubObjectClass = UComponent::StaticClass();
				}

				if( !(SubObjectClass->ClassFlags & CLASS_Compiled) )
					warnf( NAME_Error, TEXT("BEGIN OBJECT: Can't create subobject as Class %s hasn't been compiled yet."), SubObjectClass->GetName() );

				UObject* SubObject;
				if( Parse(Str,TEXT("Name="),SubObjectName) )
				{
					UObject* OldObject;
					if( (OldObject = FindObject<UObject>(SubObjectOuter,*SubObjectName)) != NULL )
					{
						if( GEditor->Bootstrapping )
							warnf( NAME_Error, TEXT("BEGIN OBJECT: name %s redefined."), SubObjectName );
						else
							OldObject->Rename();
					}
				}

				SubObject = ConstructObject<UObject>( SubObjectClass, SubObjectOuter, SubObjectName, RF_Public | ((SubObjectClass->ClassFlags&CLASS_Localized) ? RF_PerObjectLocalized : 0) );

				Data = ImportObjectProperties( SubObjectClass, (BYTE*)SubObject, Level, Data, SubObject, SubObject, Warn, Depth+1, SubObject );

				if(SubObject->IsA(UComponent::StaticClass()))
				{
					UComponent*	Component = CastChecked<UComponent>(SubObject);
					ComponentNameToInstanceMap.Set(SubObjectName,Component);
				}
			}

		}
		else if( GetEND(&Str,TEXT("Actor")) || GetEND(&Str,TEXT("DefaultProperties")) || GetEND(&Str,TEXT("StructDefaults")) || (GetEND(&Str,TEXT("Object")) && Depth) )
		{
			// End of properties.
			break;
		}
		else
		{
			// Property.
			TCHAR Token[4096];
			while( *Str==' ' || *Str==9 )
				Str++;
			const TCHAR* Start=Str;
			while( *Str && *Str!='=' && *Str!='(' && *Str!='[' && *Str!='.' )
				Str++;
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
					ADO_Empty
				};
				EArrayOp	ArrayOp = ADO_None;
				if(*Str == '.')
				{
					Str++;
					if(ParseCommand(&Str,TEXT("Empty")))
						ArrayOp = ADO_Empty;
					else if(ParseCommand(&Str,TEXT("Add")))
						ArrayOp = ADO_Add;
					else if(ParseCommand(&Str,TEXT("Remove")))
						ArrayOp = ADO_Remove;
					else if (ParseCommand(&Str,TEXT("RemoveIndex")))
					{
						ArrayOp = ADO_RemoveIndex;
					}
				}

				UProperty* Property = FindField<UProperty>( ObjectStruct, Token );
				if( !Property )
				{
					// Check for a delegate property
					FString DelegateName = FString::Printf(TEXT("__%s__Delegate"), Token );
					Property = FindField<UDelegateProperty>( ObjectStruct, *DelegateName );
					if( !Property )
					{
						Warn->Logf( NAME_ExecWarning, TEXT("%s: Unknown property in defaults: %s"), *ObjectStruct->GetPathName(), StrLine );
						continue;
					}
				}
				UArrayProperty*	ArrayProperty = Cast<UArrayProperty>(Property);

				if(ArrayOp != ADO_None && !ArrayProperty)
				{
					Warn->Logf(NAME_ExecWarning,TEXT("%s: Array operation performed on non-array variable: %s"),*ObjectStruct->GetPathName(),StrLine);
					continue;
				}

				if(ArrayOp == ADO_Empty)
				{
					FArray*	Array = (FArray*)(Object + Property->Offset);
					Array->Empty(ArrayProperty->Inner->ElementSize);
				}
				else if(ArrayOp == ADO_Add || ArrayOp == ADO_Remove)
				{
					FArray*	Array = (FArray*)(Object + Property->Offset);

					SkipWhitespace(Str);
					if(*Str++ != '(')
					{
						Warn->Logf( NAME_ExecWarning, TEXT("%s: Missing '(' in default properties array operation: %s"), *ObjectStruct->GetPathName(), StrLine );
						continue;
					}
					SkipWhitespace(Str);

					if(ArrayOp == ADO_Add)
					{
                        INT	Index = Array->AddZeroed(ArrayProperty->Inner->ElementSize,1);
						UStructProperty* StructProperty = Cast<UStructProperty>(ArrayProperty->Inner);
						if( StructProperty )
						{
							// Initialize struct defaults.
							StructProperty->CopySingleValue( (BYTE*)Array->GetData() + Index * ArrayProperty->Inner->ElementSize, &StructProperty->Struct->Defaults(0), NULL );
						}
						ArrayProperty->Inner->ImportText(Str,(BYTE*)Array->GetData() + Index * ArrayProperty->Inner->ElementSize,PPF_Delimited|PPF_CheckReferences,GEditor->Bootstrapping ? ObjectStruct : (UObject*)Object);
					}
					else if(ArrayOp == ADO_Remove)
					{
						BYTE*	Temp = new BYTE[ArrayProperty->Inner->ElementSize];
						if(ArrayProperty->Inner->ImportText(Str,Temp,PPF_Delimited|PPF_CheckReferences,GEditor->Bootstrapping ? ObjectStruct : (UObject*)Object))
						{
							for(UINT Index = 0;Index < (UINT)Array->Num();Index++)
								if(ArrayProperty->Inner->Identical(Temp,(BYTE*)Array->GetData() + Index * ArrayProperty->Inner->ElementSize))
									Array->Remove(Index--,1,ArrayProperty->Inner->ElementSize);
						}
						ArrayProperty->Inner->DestroyValue(Temp);
						delete Temp;
					}
				}
				else if (ArrayOp == ADO_RemoveIndex)
				{
					FArray*	Array = (FArray*)(Object + Property->Offset);

					SkipWhitespace(Str);
					if(*Str++ != '(')
					{
						Warn->Logf( NAME_ExecWarning, TEXT("%s: Missing '(' in default properties array operation: %s"), *ObjectStruct->GetPathName(), StrLine );
						continue;
					}
					SkipWhitespace(Str);

					FString strIdx;
					while (*Str != ')')
					{		
						strIdx += Str;
						*Str++;
					}
					INT removeIdx = appAtoi(*strIdx);
					Array->Remove(removeIdx,1,ArrayProperty->Inner->ElementSize);
				}
				else
				{
					// Parse an array index, if present.
					INT Index=-1;
					if( *Str=='(' || *Str=='[' )
					{
						Str++;
						Index = appAtoi(Str);
						while( *Str && *Str!=')' && *Str!=']' )
							Str++;
						if( !*Str++ )
						{
							Warn->Logf( NAME_ExecWarning, TEXT("%s: Missing ')' in default properties subscript: %s"), *ObjectStruct->GetPathName(), StrLine );
							continue;
						}
					}

					// strip whitespace before =
					SkipWhitespace(Str);
					if( *Str++!='=' )
					{
						Warn->Logf( NAME_ExecWarning, TEXT("%s: Missing '=' in default properties assignment: %s"), *ObjectStruct->GetPathName(), StrLine );
						continue;
					}
					// strip whitespace after =
					SkipWhitespace(Str);

					UProperty* Property = FindField<UProperty>( ObjectStruct, Token );
					if( !Property )
					{
						// Check for a delegate property
						FString DelegateName = FString::Printf(TEXT("__%s__Delegate"), Token );
						Property = FindField<UDelegateProperty>( ObjectStruct, *DelegateName );
						if( !Property )
						{
							Warn->Logf( NAME_ExecWarning, TEXT("%s: Unknown property in defaults: %s"), *ObjectStruct->GetPathName(), StrLine );
							continue;
						}
					}
					if( ( Index>=Property->ArrayDim && !Property->IsA(UArrayProperty::StaticClass()) ) )
					{
						Warn->Logf( NAME_ExecWarning, TEXT("%s: Out of bound array default property (%i/%i)"), *ObjectStruct->GetPathName(), Index, Property->ArrayDim );
						continue;
					}

					FDefinedProperty D;
					D.Property = Property;
					D.Index = Index;
					if( DefinedProperties.FindItemIndex( D ) != INDEX_NONE )
					{
						Warn->Logf( NAME_ExecWarning, TEXT("redundant data: %s"), StrLine );
						continue;
					}
					DefinedProperties.AddItem( D );

					// limited multi-line support, look for {...} sequences and condense to a single entry
					FString FullText(TEXT(""));
					if (*Str == '{')
					{
						//debugf(TEXT("Multi-line import for property %s"),Property->GetName());
						INT bracketCnt = 0;
						UBOOL bInString = 0;
						// increment through each character until we run out of brackets
						do
						{
							// check to see if we've reached the end of this line
							if (*Str == '\0' || *Str == NULL)
							{
								// parse a new line
								if (ParseLine(&Data,StrLine,StrLineLength))
								{
									Str = StrLine;
								}
								else
								{
									// no more to parse
									break;
								}
							}
							TCHAR Char = *Str;
							// if we reach a bracket, increment the count
							if (Char == '{')
							{
								bracketCnt++;
							}
							else
							// if we reach and end bracket, decrement the count
							if (Char == '}')
							{
								bracketCnt--;
							}
							else if (Char == '\"')
							{
								// flip the "we are in a string" switch
								bInString = !bInString;
							}
							// add this character to our end result
							// if we're currently inside a string value, allow the space character
							if (bInString ||
								(Char != ' ' &&
								 Char != '\t' &&
								 Char != '\n' &&
								 Char != '\r' &&
								 (bracketCnt > 1 ||
								  (Char != '{' &&
								   Char != '}'))))
							{
								//@fixme - FString::+=(TCHAR) is broken, so this is a temp workaround
								TCHAR ugh[2];
								ugh[0] = Char;
								ugh[1] = '\0';
								FullText += ugh;
							}
							Str++;
						}
						while (bracketCnt != 0);
						// set the pointer to our new text
						Str = &FullText[0];
						//debugf(TEXT("Final line: [%s] [%s]"),Str,*FullText);
					}

					if( appStricmp(Property->GetName(),TEXT("Name"))!=0 )
					{
						l = appStrlen(Str);
						while( l && (Str[l-1]==';' || Str[l-1]==' ' || Str[l-1]==9) )
						{
							*(TCHAR*)(&Str[l-1]) = 0;
							--l;
						}
						if( Property->IsA(UStrProperty::StaticClass()) && (!l || *Str != '"' || Str[l-1] != '"') )
							Warn->Logf( NAME_ExecWarning, TEXT("%s: Missing '\"' in string default properties : %s"), *ObjectStruct->GetPathName(), StrLine );

						if (Index > -1 && Property->IsA(UArrayProperty::StaticClass())) //set single dynamic array element
						{
							FArray* Array=(FArray*)(Object + Property->Offset);
							UArrayProperty* ArrayProp = (UArrayProperty*)Property;
							if (Index>=Array->Num())
							{
								Array->AddZeroed(ArrayProp->Inner->ElementSize,Index-Array->Num()+1);
								UStructProperty* StructProperty = Cast<UStructProperty>(ArrayProp->Inner);
								if( StructProperty )
								{
									// Initialize struct defaults.
									StructProperty->CopySingleValue( (BYTE*)Array->GetData() + Index * ArrayProp->Inner->ElementSize, &StructProperty->Struct->Defaults(0), NULL );
								}
							}
							ArrayProp->Inner->ImportText( Str, (BYTE*)Array->GetData() + Index * ArrayProp->Inner->ElementSize, PPF_Delimited | PPF_CheckReferences, GEditor->Bootstrapping ? ObjectStruct : (UObject*)Object );
						}
						else
						if( Property->IsA(UDelegateProperty::StaticClass()) )
						{
							if (Index == -1) Index = 0;
							FString Temp;
							if( appStrchr(Str, '.')==NULL )
							{
								Temp = Depth ? InParent->GetName() : ObjectStruct->GetName();
								Temp = Temp + TEXT(".") + Str;
							}
							else
								Temp = Str;
							FScriptDelegate* D = (FScriptDelegate*)(Object + Property->Offset + Index*Property->ElementSize);
							D->Object = NULL;
							D->FunctionName = NAME_None;
							Property->ImportText( *Temp, Object + Property->Offset + Index*Property->ElementSize, PPF_Delimited | PPF_CheckReferences, GEditor->Bootstrapping ? ObjectStruct : (UObject*)Object  );
							if( !D->Object || D->FunctionName==NAME_None )
								Warn->Logf( NAME_ExecWarning, TEXT("%s: Delegate assignment failed: %s"), *ObjectStruct->GetPathName(), StrLine );
						}
						else
						{
							if (Index == -1) Index = 0;
							Property->ImportText( Str, Object + Property->Offset + Index*Property->ElementSize, PPF_Delimited | PPF_CheckReferences, GEditor->Bootstrapping ? ObjectStruct : (UObject*)Object );
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
		ABrush* Actor = (ABrush*)Object;
		check(Actor->BrushComponent);
		if( Actor->bStatic )
		{
			// Prepare static brush.
			Actor->SetFlags       ( RF_NotForClient | RF_NotForServer );
			Actor->Brush->SetFlags( RF_NotForClient | RF_NotForServer );
		}
		else
		{
			// Prepare moving brush.
			GEditor->csgPrepMovingBrush( Actor );
		}
	}

	return Data;
}

const TCHAR* ImportDefaultProperties(
	UClass*				Class,
	const TCHAR*		Text,
	FFeedbackContext*	Warn,
	INT					Depth
	)
{
	UBOOL	Success = 1;

	// Parse the default properties.

	if(Text)
	{
		Text = ImportProperties(
					Class,
					&Class->Defaults(0),
					NULL,
					Text,
					Class->GetOuter(),
					Class,
					Warn,
					Depth,
					Class->ComponentNameToDefaultObjectMap,
					Class
					);
		Success = Text != NULL;
	}

	if(Success)
	{
		InstanceSuperClassComponents(Class);
	}

	return Text;
}

const TCHAR* ImportObjectProperties(
	UStruct*			ObjectStruct,
	BYTE*				Object,
	ULevel*				Level,
	const TCHAR*		Text,
	UObject*			InParent,
	UObject*			SubObjectOuter,
	FFeedbackContext*	Warn,
	INT					Depth,
	UObject*			ComponentOwner
	)
{
	TMap<FName,UComponent*>	ComponentNameToInstanceMap;

	// Parse the object properties.

	Text = ImportProperties(
			ObjectStruct,
			Object,
			Level,
			Text,
			InParent,
			SubObjectOuter,
			Warn,
			Depth,
			ComponentNameToInstanceMap,
			ComponentOwner
			);

	// Update the object properties to point to the newly imported component objects.

	ObjectStruct->InstanceComponents(ComponentNameToInstanceMap,Object,ComponentOwner);
	ObjectStruct->FixupComponentReferences(ComponentNameToInstanceMap,Object,ComponentOwner);

	return Text;
}