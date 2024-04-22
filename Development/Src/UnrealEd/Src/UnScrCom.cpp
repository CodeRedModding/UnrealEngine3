/*=============================================================================
	UnScrCom.cpp: UnrealScript compiler.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnScrPrecom.h"
#include "UnScrCom.h"
#include "OpCode.h"
#include "UnNativeClassExporter.h"
#include "UnEdTran.h"

/*-----------------------------------------------------------------------------
	Constants & declarations.
-----------------------------------------------------------------------------*/

enum {MAX_ARRAY_SIZE=2048};
static UBOOL GCheckNatives=1;

/**
 * The starting class flags (i.e. the class flags that were set before the
 * CLASS_RecompilerClear mask was applied) for the class currently being compiled
 */
static DWORD PreviousClassFlags = CLASS_None;

/**
 * The mapping of an EPropertyName to it's corresponding FName. This allows
 * property FNames to have any value.
 * Make sure you add your property type here when adding new property types!
 */
static EName PropertyTypeToNameMap[CPT_MAX] = 
{
	NAME_None,
	NAME_ByteProperty,
	NAME_IntProperty,
	NAME_BoolProperty,
	NAME_FloatProperty,
	NAME_ObjectProperty,
	NAME_NameProperty,
	NAME_DelegateProperty,
	NAME_InterfaceProperty,
	NAME_ArrayProperty,
	NAME_StructProperty,
	NAME_VectorProperty,
	NAME_RotatorProperty,
	NAME_StrProperty,
	NAME_MapProperty,
	// add new property types here
};

static inline FName GetPropertyName(EPropertyType Type)
{
	return PropertyTypeToNameMap[Type];
}

/*-----------------------------------------------------------------------------
	Utility functions.
-----------------------------------------------------------------------------*/

//
// Get conversion token between two types.
// Converting a type to itself has no conversion function.
// EX_Max indicates that a conversion isn't possible.
// Conversions to type CPT_String must not be automatic.
//
DWORD GetConversion( const FPropertyBase& Dest, const FPropertyBase& Src )
{
#define AUTOCONVERT 0x100 /* Compiler performs the conversion automatically */
#define TRUNCATE    0x200 /* Conversion requires truncation */
#define CONVERT_MASK ~(AUTOCONVERT | TRUNCATE)
#define AC  AUTOCONVERT
#define TAC TRUNCATE|AUTOCONVERT
static DWORD GConversions[CPT_MAX][CPT_MAX] =
{
			/*   None      Byte                Int                 Bool              Float                Object             Name             Delegate                  Interface        Range            Struct           Vector               Rotator              String              Map              */
			/*   --------  ------------------  ------------------  ----------------  -------------------  -----------------  ---------------  -------------------       ---------------  ---------------  ---------------- -------------------  -------------------- -----------------   ---------------- */
/* None     */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,                  CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_Max,            CST_Max,          },
/* Byte     */ { CST_Max,  CST_Max,            CST_IntToByte|TAC,  CST_BoolToByte,	 CST_FloatToByte|TAC, CST_Max,           CST_Max,         CST_Max,                  CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_StringToByte,   CST_Max,          },
/* Int      */ { CST_Max,  CST_ByteToInt|AC,   CST_Max,            CST_BoolToInt,    CST_FloatToInt|TAC,  CST_Max,           CST_Max,         CST_Max,                  CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_StringToInt,    CST_Max,          },
/* Bool     */ { CST_Max,  CST_ByteToBool,     CST_IntToBool,      CST_Max,          CST_FloatToBool,     CST_ObjectToBool,  CST_NameToBool,  CST_Max,      CST_InterfaceToBool,         CST_Max,         CST_Max,         CST_VectorToBool,    CST_RotatorToBool,   CST_StringToBool,   CST_Max,          },
/* Float    */ { CST_Max,  CST_ByteToFloat|AC, CST_IntToFloat|AC,  CST_BoolToFloat,  CST_Max,             CST_Max,           CST_Max,         CST_Max,	                CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_StringToFloat,  CST_Max,          },
/* Object   */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max, CST_InterfaceToObject|AC,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_Max,            CST_Max,          },
/* Name     */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,                  CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_StringToName,   CST_Max,          },
/* Delegate */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,                  CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,	         CST_Max,            CST_Max,          },
/* Interface*/ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,       AC|CST_ObjectToInterface,CST_Max,         CST_Max,                  CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_Max,            CST_Max,          },
/* Range    */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,                  CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_Max,            CST_Max,          },
/* Struct   */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,                  CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_Max,            CST_Max,          },
/* Vector   */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,			        CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_RotatorToVector, CST_StringToVector, CST_Max,          },
/* Rotator  */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,			        CST_Max,         CST_Max,         CST_Max,         CST_VectorToRotator, CST_Max,             CST_StringToRotator,CST_Max,          },
/* String   */ { CST_Max,  CST_ByteToString,   CST_IntToString,    CST_BoolToString, CST_FloatToString, CST_ObjectToString, CST_NameToString, CST_DelegateToString,CST_InterfaceToString,CST_Max,         CST_Max,         CST_VectorToString,  CST_RotatorToString, CST_Max,            CST_Max,          },
/* Map      */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,                  CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_Max,            CST_Max,          },
};
#undef AC
#undef TAC
	INT DestType = Dest.IsVector() ? CPT_Vector : Dest.IsRotator() ? CPT_Rotation : Dest.Type;
	INT SrcType  = Src .IsVector() ? CPT_Vector : Src .IsRotator() ? CPT_Rotation : Src.Type;
	return GConversions[DestType][SrcType];
}

#define DEFAULT_OUTERCONTEXT_SIZE	(1 + sizeof(ScriptPointerType))

#define ENABLE_NONFATAL_COMPILER_ERRORS 0

VARARG_BODY( void, FScriptCompiler::ScriptErrorf, const TCHAR*, VARARG_EXTRA(EScriptCompilerErrorLevel ErrorLevel) )
{
	INT		BufferSize	= 1024;
	TCHAR*	Buffer		= NULL;
	INT		Result		= -1;

	while(Result == -1)
	{
		Buffer = (TCHAR*) appRealloc( Buffer, BufferSize * sizeof(TCHAR) );
		GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );
		BufferSize *= 2;
	};
	Buffer[Result] = 0;

	UBOOL bThrow=TRUE;
	switch ( ErrorLevel )
	{
	// an error we're not sure how to handle yet
	case SCEL_Unknown:
		break;

	// something that isn't a problem but our rules don't allow
	case SCEL_Restricted:
		bThrow = FALSE;
		break;

	// an error encountered while parsing a declaration - skip the statement
	//@todo - need to double check where we use this to verify that we can safely and correctly skip the current declaration (might need to be handled by the calling code)
	case SCEL_Parse:
// 		SkipStatements(0, Buffer);
// 		bThrow = FALSE;
		break;

	// the number of characters parsed was more than some hard-coded limit
	case SCEL_Limit:
		bThrow = FALSE;
		break;

	// an identifier or constant wasn't formatted correctly but can still be used
	case SCEL_Formatting:
		bThrow = FALSE;
		break;

	// parsing/compiling an expression; skip the remainder of the expression or possibly pop the nest
	//@todo - need to double check where we use this to verify that we can safely and correctly skip the current expression (might need to be handled by the calling code)
	case SCEL_Expression:
// 		SkipStatements(0, Buffer);
		bThrow = FALSE;
		break;

	// an error was encountered which makes it impossible to finish compiling the current nest level
	case SCEL_NestLevel:
		break;

	// an error was encountered which makes it impossible to finish compiling the current class - go to the next one
	case SCEL_Class:
		break;

	// script compiler can no longer continue compilation...
	case SCEL_Fatal:
		break;
	}

#if ENABLE_NONFATAL_COMPILER_ERRORS
	if ( bThrow )
#else
	if ( TRUE )
#endif
	{
		FString BufferString = /*GetErrorLevelText(ErrorLevel) + TEXT(": ") + */Buffer;
		appFree(Buffer);
		Buffer = NULL;

		appThrowf(TEXT("%s"), *BufferString);
	}
	else
	{
		Warn->Log(NAME_Error, Buffer);
	}
	appFree( Buffer );
}

VARARG_BODY( void, FScriptCompiler::ScriptWarnf, const TCHAR*, VARARG_EXTRA(EScriptCompilerWarningLevel WarningLevel) )
{
	INT		BufferSize	= 1024;
	TCHAR*	Buffer		= NULL;
	INT		Result		= -1;

	while(Result == -1)
	{
		Buffer = (TCHAR*) appRealloc( Buffer, BufferSize * sizeof(TCHAR) );
		GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );
		BufferSize *= 2;
	};
	Buffer[Result] = 0;

	if ( WarningLevel >= RequiredWarningLevel )
	{
		Warn->Log(NAME_Warning, Buffer);
	}
	appFree( Buffer );
}

UClass* FindClass( const TCHAR* ClassName, UObject* ClassPackage=ANY_PACKAGE )
{
	UClass* Result = NULL;

	if ( ClassName != NULL )
	{
		Result = FindObject<UClass>(ClassPackage, ClassName);
		if ( Result == NULL )
		{
			UObjectRedirector* RenamedClassRedirector = FindObject<UObjectRedirector>(ClassPackage, ClassName);
			if ( RenamedClassRedirector != NULL )
			{
				Result = CastChecked<UClass>(RenamedClassRedirector->DestinationObject);
			}
		}
	}

	return Result;
}

/**
 * Copies the properties from this token into another.
 *
 * @param	Other	the token to copy this token's properties to.
 */
void FToken::Clone( const FToken& Other )
{
	// none of FPropertyBase's members require special handling
	(FPropertyBase&)*this	= (FPropertyBase&)Other;

	TokenType				= Other.TokenType;
	TokenName				= Other.TokenName;
	StartPos				= Other.StartPos;
	StartLine				= Other.StartLine;
	TokenProperty			= Other.TokenProperty;
	if ( Other.TokenFunction )
	{
		*TokenFunction			= *Other.TokenFunction;
	}
	else
	{
		TokenFunction = NULL;
	}

	appStrncpy(Identifier, Other.Identifier, NAME_SIZE);
	appMemcpy(String, Other.String, sizeof(String));
}

/**
 * Static helper function to determine whether the class hierarchy rooted at Suspect is
 * dependent on the hierarchy rooted at Source.
 *
 * @param	Suspect		Root of hierarchy for suspect class
 * @param	Source		Root of hierarchy for source class
 * @param	AllClasses	Array of parsed classes
 * @return	TRUE if the hierarchy rooted at Suspect is dependent on the one rooted at Source, FALSE otherwise
 */
UBOOL FScriptCompiler::IsDependentOn( UClass* Suspect, UClass* Source, const FClassTree& AllClasses )
{
	check( Source );
	check( Suspect );

    // A class is not dependent on itself.
    if( Suspect == Source )
    {
        return FALSE;
    }

    // Children are all implicitly dependent on their parent, that is, children require their parent
	// to be compiled first therefore if the source is a parent of the suspect, the suspect is
	// dependent on the source.
    if( Suspect->IsChildOf( Source ) )
    {
        return TRUE;
    }

    // Now consider all dependents of the suspect. If any of them are dependent on the source, the
	// suspect is too.
    for( INT NameIndex=0; NameIndex<Suspect->DependentOn.Num(); NameIndex++ )
	{
		FName	DependencyName	= Suspect->DependentOn(NameIndex);
		UClass*	Dependency		= FindClass(*DependencyName.ToString());/*FindObject<UClass>( ANY_PACKAGE, *DependencyName.ToString() );*/

		if( !Dependency )
		{
			// Error.
			continue;
		}

		// the parser disallows declaring the parent class as a dependency, so the only way this could occur is
		// if the parent for a native class has been changed (which causes the new parent to be inserted as a dependency),
		// if this is the case, skip it or we'll go into a loop
		if ( Suspect->GetSuperClass() == Dependency )
		{
			continue;
		}

		if( Dependency == Source || IsDependentOn( Dependency, Source, AllClasses ) )
		{
			return TRUE;
		}
	}

	// Consider all children of the suspect if any of them are dependent on the source, the suspect is
	// too because it itself is dependent on its children.

	if(!Suspect->HasAnyClassFlags(CLASS_Interface))
	{
		const FClassTree* SuspectLeaf = AllClasses.FindNode(Suspect);
		if ( SuspectLeaf == NULL )
		{
			ScriptErrorf(SCEL_Parse, TEXT("Unparsed class '%s' found while validating DependsOn entries for '%s'"), *Suspect->GetName(), *Source->GetName());
			return FALSE;
		}
	}

// 	TArray<const FClassTree*> SuspectChildren;
// 	SuspectLeaf->GetChildClasses(SuspectChildren);
// 
// 	for( INT ClassIndex=0; ClassIndex<SuspectChildren.Num(); ClassIndex++ )
// 	{
// 		const FClassTree* ChildLeaf = SuspectChildren(ClassIndex);
// 		UClass* Class = ChildLeaf->GetClass();
// 		if( IsDependentOn( Class, Source, AllClasses ) )
// 		{
// 			return TRUE;
// 		}
// 	}

    return FALSE;
}

FFunctionData* FClassMetaData::FindFunctionData( UFunction* Function )
{
	FFunctionData* Result = NULL;
	
	TScopedPointer<FFunctionData>* FuncData = FunctionData.Find(Function);
	if ( FuncData != NULL )
	{
		Result = FuncData->GetOwnedPointer();
	}

	if ( Result == NULL )
	{
		UClass* OwnerClass = Function->GetOwnerClass();
		FClassMetaData* OwnerClassData = GScriptHelper->FindClassData(OwnerClass);
		if ( OwnerClassData && OwnerClassData != this )
		{
			Result = OwnerClassData->FindFunctionData(Function);
		}
	}

	return Result;
}

/**
 * Finds the metadata for the struct specified
 * 
 * @param	Struct	the struct to search for
 *
 * @return	pointer to the metadata for the struct specified, or NULL
 *			if the struct doesn't exist in the list (for example, if it
 *			is declared in a package that is already compiled and has had its
 *			source stripped)
 */
FStructData* FClassMetaData::FindStructData( UScriptStruct* Struct )
{
	FStructData* Result = NULL;
	
	TScopedPointer<FStructData>* pStructData = StructData.Find(Struct);
	if ( pStructData != NULL )
	{
		Result = pStructData->GetOwnedPointer();
	}

	if ( Result == NULL )
	{
		UClass* OwnerClass = Struct->GetOwnerClass();
		FClassMetaData* OwnerClassData = GScriptHelper->FindClassData(OwnerClass);
		if ( OwnerClassData && OwnerClassData != this )
		{
			Result = OwnerClassData->FindStructData(Struct);
		}
	}

	return Result;
}

/**
 * Finds the metadata for the property specified
 *
 * @param	Prop	the property to search for
 *
 * @return	pointer to the metadata for the property specified, or NULL
 *			if the property doesn't exist in the list (for example, if it
 *			is declared in a package that is already compiled and has had its
 *			source stripped)
 */
FTokenData* FClassMetaData::FindTokenData( UProperty* Prop )
{
	check(Prop);

	FTokenData* Result = NULL;
	UObject* Outer = Prop->GetOuter();
	UClass* OuterClass = Cast<UClass>(Outer);
	if ( OuterClass != NULL )
	{
		Result = GlobalPropertyData.Find(Prop);
		if ( Result == NULL && OuterClass->GetSuperClass() != OuterClass )
		{
			OuterClass = OuterClass->GetSuperClass();
		}
	}
	else
	{
		UFunction* OuterFunction = Cast<UFunction>(Outer);
		if ( OuterFunction != NULL )
		{
			// function parameter, return, or local property
			TScopedPointer<FFunctionData>* pFuncData = FunctionData.Find(OuterFunction);
			if ( pFuncData != NULL )
			{
				FFunctionData* FuncData = pFuncData->GetOwnedPointer();
				check(FuncData);

				FPropertyData& FunctionParameters = FuncData->GetParameterData();
				Result = FunctionParameters.Find(Prop);
				if ( Result == NULL )
				{
					FPropertyData& LocalParameters = FuncData->GetLocalData();
					Result = LocalParameters.Find(Prop);
					if (Result == NULL)
					{
						Result = FuncData->GetReturnTokenData();
					}
				}
			}
			else
			{
				OuterClass = OuterFunction->GetOwnerClass();
			}
		}
		else
		{
			// struct property
			UScriptStruct* OuterStruct = Cast<UScriptStruct>(Outer);
			if (OuterStruct != NULL)
			{
				TScopedPointer<FStructData>* pStructInfo = StructData.Find(OuterStruct);
				if ( pStructInfo != NULL )
				{
					FStructData* StructInfo = pStructInfo->GetOwnedPointer();
					check(StructInfo);

					FPropertyData& StructProperties = StructInfo->GetStructPropertyData();
					Result = StructProperties.Find(Prop);
				}
				else
				{
					OuterClass = OuterStruct->GetOwnerClass();
				}
			}
			else
			{
				UState* OuterState = Cast<UState>(Outer);
				check(OuterState != NULL);

				TScopedPointer<FStateData>* pStateData = StateData.Find(OuterState);
				if (pStateData != NULL)
				{
					FStateData* StateDatum = pStateData->GetOwnedPointer();
					check(StateDatum);

					FPropertyData& LocalProperties = StateDatum->GetLocalData();
					Result = LocalProperties.Find(Prop);
				}
				else
				{
					OuterClass = OuterState->GetOwnerClass();
				}
 			}
		}
	}

	if ( Result == NULL && OuterClass != NULL )
	{
		FClassMetaData* SuperClassData = GScriptHelper->FindClassData(OuterClass);
		if ( SuperClassData && SuperClassData != this )
		{
			Result = SuperClassData->FindTokenData(Prop);
		}
	}
	return Result;
}

void FClassMetaData::Dump( INT Indent )
{
	debugf(TEXT("Globals:"));
	GlobalPropertyData.Dump(Indent+4);

	debugf(TEXT("Structs:"));
	for ( TMap<UScriptStruct*, TScopedPointer<FStructData> >::TIterator It(StructData); It; ++It )
	{
		UScriptStruct* Struct = It.Key();

		TScopedPointer<FStructData>& PointerVal = It.Value();
		FStructData* Data = PointerVal.GetOwnedPointer();

		debugf(TEXT("%s%s"), appSpc(Indent), *Struct->GetName());
		Data->Dump(Indent+4);
	}

	debugf(TEXT("Functions:"));
	for ( TMap<UFunction*, TScopedPointer<FFunctionData> >::TIterator It(FunctionData); It; ++It )
	{
		UFunction* Function = It.Key();

		TScopedPointer<FFunctionData>& PointerVal = It.Value();
		FFunctionData* Data = PointerVal.GetOwnedPointer();

		debugf(TEXT("%s%s"), appSpc(Indent), *Function->GetName());
		Data->Dump(Indent+4);
	}

	debugf(TEXT("States:"));
	for ( TMap<UState*, TScopedPointer<FStateData> >::TIterator It(StateData); It; ++It )
	{
		UState* State = It.Key();

		TScopedPointer<FStateData>& PointerVal = It.Value();
		FStateData* Data = PointerVal.GetOwnedPointer();

		debugf(TEXT("%s%s"), appSpc(Indent), *State->GetName());
		Data->Dump(Indent+4);
	}
}

/*-----------------------------------------------------------------------------
	FScriptWriter.
-----------------------------------------------------------------------------*/

void FScriptWriter::Serialize( void* V, INT Length )
{
	INT iStart = Compiler.TopNode->Script.Add( Length );
	appMemcpy( &Compiler.TopNode->Script(iStart), V, Length );
}

/*-----------------------------------------------------------------------------
	FContextSupplier.
-----------------------------------------------------------------------------*/

FString FScriptCompiler::GetContext()
{
	if (InDefaultPropContext)
	{
		return FString::Printf
		(
			TEXT("%sDevelopment\\Src\\%s\\Classes\\%s.uc(1) : defaultproperties"),
			*appRootDir(),
			*Class->GetOuter()->GetName(),
			*Class->GetName()
		);
	}

	return FString::Printf
	(
		TEXT("%sDevelopment\\Src\\%s\\Classes\\%s.uc(%i)"),
		*appRootDir(),
		*Class->GetOuter()->GetName(),
		*Class->GetName(),
		InputLine
	);
}

/*-----------------------------------------------------------------------------
	FNestInfo.
-----------------------------------------------------------------------------*/

/**
 * Adjusts the code location for any pending fixup requests that fall within the code region
 * specified when compiled code is moved via MoveCompiledCode().
 * 
 * @param	Location		the location [into the top node's Script array] of the
 *							bytecode that is being moved
 * @param	Count			size of the bytecode block being moved
 * @param	Displacement	how much the bytecode is being moved
 */
void FNestInfo::UpdatePlaceholderLocations( INT Location, INT Count, INT Displacement )
{
	for( FJumpTargetPlaceholder* Fixup = JumpPlaceholderList; Fixup != NULL; Fixup = Fixup->Next )
	{
		if( Fixup->Type == FIXUP_Label )
		{
			// Fixup a local label.
			FLabelRecord* LabelRecord;
			for( LabelRecord = LabelList; LabelRecord; LabelRecord=LabelRecord->Next )
			{
				if( LabelRecord->Name == Fixup->Name )
				{
					if ( Location < LabelRecord->iCode && Location + Count >= LabelRecord->iCode )
					{
						LabelRecord->iCode += Displacement;
					}
					break;
				}
			}
		}
		else
		{
			// Fixup a code structure address.

			// if the placeholder location falls within the region we're moving, we need to update the Fixup to reflect the
			// new location of the placeholder
			if ( Location < Fixup->iCode && Location + Count >= Fixup->iCode )
			{
				Fixup->iCode += Displacement;
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	Single-character processing.
-----------------------------------------------------------------------------*/

//
// Get a single character from the input stream and return it, or 0=end.
//
TCHAR FScriptCompiler::GetChar( UBOOL Literal )
{
	INT CommentCount=0;

	PrevPos  = InputPos;
	PrevLine = InputLine;

	Loop:
	const TCHAR c = Input[InputPos++];
	if ( CommentCount > 0 )
	{
		// Record the character as a comment.
		PrevComment += c;
	}
	if( c == TEXT('\n') )
	{
		InputLine++;
	}
	else if( !Literal )
	{
		const TCHAR NextChar=PeekChar();
		if ( c==TEXT('/') && NextChar==TEXT('*') )
		{
			if ( CommentCount == 0 )
			{
				ClearComment();
				// Record the slash and star.
				PrevComment += c;
				PrevComment += NextChar;
			}
			CommentCount++;
			InputPos++;
			goto Loop;
		}
		else if( c==TEXT('*') && NextChar==TEXT('/') )
		{
			if( --CommentCount < 0 )
			{
				ClearComment();
				ScriptErrorf(SCEL_Class,  TEXT("Unexpected '*/' outside of comment") );
			}
			// Star already recorded; record the slash.
			PrevComment += Input[InputPos];

			InputPos++;
			goto Loop;
		}
	}
	if( CommentCount > 0 )
	{
		if( c==0 )
		{
			ClearComment();
			ScriptErrorf(SCEL_Class,  TEXT("End of script encountered inside comment") );
		}
		goto Loop;
	}
	return c;
}

//
// Unget the previous character retrieved with GetChar().
//
void FScriptCompiler::UngetChar()
{
	InputPos  = PrevPos;
	InputLine = PrevLine;
}

//
// Look at a single character from the input stream and return it, or 0=end.
// Has no effect on the input stream.
//
TCHAR FScriptCompiler::PeekChar()
{
	return InputPos<InputLen ? Input[InputPos] : 0;
}

//
// Skip past all spaces and tabs in the input stream.
//
TCHAR FScriptCompiler::GetLeadingChar()
{
	// Skip blanks.
	TCHAR c;
	Skip1:
	do
	{
		c=GetChar();
	} while( c==TEXT(' ') || c==TEXT('\t') || c==TEXT('\r') || c==TEXT('\n') );

	if( c==TEXT('/') && PeekChar()==TEXT('/')  )
	{
		// Record the first slash.  The first iteration of the loop will get the second slash.
		PrevComment += c;
		do
		{
			c=GetChar(TRUE);
			if ( c==0 )
			{
				return c;
			}
			PrevComment += c;
		} while( c!=TEXT('\r') && c!=TEXT('\n') );
		goto Skip1;
	}
	return c;
}

//
// Return 1 if input as a valid end-of-line character, or 0 if not.
// EOL characters are: Comment, CR, linefeed, 0 (end-of-file mark)
//
UBOOL FScriptCompiler::IsEOL( TCHAR c )
{
	return c==TEXT('\n') || c==TEXT('\r') || c==0;
}

/*-----------------------------------------------------------------------------
	Code emitting.
-----------------------------------------------------------------------------*/

void FScriptCompiler::EmitDebugInfo( BYTE OpCode )
{
	if( SupressDebugInfo > 0 )
	{
		SupressDebugInfo--;
	//	warnf(TEXT("Supressed %s"), AdditionalInfo);
		return;
	}
	if( bEmitDebugInfo )
	{
		int GVERSION = 100;
		Writer << EX_DebugInfo;
		Writer << GVERSION;
		Writer << InputLine;
		Writer << InputPos;
		Writer << OpCode;
	}
}

UProperty* FScriptCompiler::GetUObjectOuterProperty() const
{
	static UProperty* OuterProp=NULL;

	// Initialize OuterProp to UProperty Engine.Object.Outer (only needs to be done once since it never needs to change).
	if( OuterProp == NULL )
	{
		for( TFieldIterator<UProperty> It(UObject::StaticClass()); It; ++It )
		{
			if( It->GetFName() == NAME_Outer )
			{
				OuterProp = *It;
				break;
			}
		}
	}

	check(OuterProp);
	return OuterProp;
}


/**
 * Emit bytecodes corresponding to an Outer context reference.
 * 
 * @return	the index into the current node's bytecode (Script array) for
 *			the context skip-over placeholder value (once the size of the
 *			field being referenced through the context expression is known,
 *			the placeholder at this index should be replaced with that size)
 */
INT FScriptCompiler::EmitOuterContext( FScriptLocation& StartOfExpression, UBOOL bHasExistingContext )
{
	UProperty* OuterProp = GetUObjectOuterProperty();

	FScriptLocation CurrentLocation;
	Writer << EX_Context;
	MoveCompiledCode(StartOfExpression, CurrentLocation);

	BYTE NullPropertyType=CPT_None;
	if ( !bHasExistingContext )
	{
		// if do not have an existing context, we need an expression to switch contexts
		// to the Outer obj first
		Writer << EX_InstanceVariable;
		Writer << OuterProp;
	}

	const INT FixupPosition = TopNode->Script.Num();

	// 1 byte for the EX_InstanceVariable bytecode, plus sizeof(ScriptPointerType) bytes for the variable 
	CodeSkipSizeType wSize = DEFAULT_OUTERCONTEXT_SIZE;
	Writer << wSize;

	// used to determine the amount of memory that must be zeroed if context is null
	Writer << OuterProp << NullPropertyType;

	// when we already have a context, the next expression in the chain needs to be one that switches
	// context to the Outer object
	if ( bHasExistingContext )
	{
		Writer << EX_InstanceVariable;
		Writer << OuterProp;
	}

	return FixupPosition;
}

//
// Emit a constant expression.
//
void FScriptCompiler::EmitConstant( FToken& Token )
{
	check(Token.TokenType==TOKEN_Const);

	switch( Token.Type )
	{
		case CPT_Int:
		{
			if( Token.Int == 0 )
			{
				Writer << EX_IntZero;
			}
			else if( Token.Int == 1 )
			{
				Writer << EX_IntOne;
			}
			else if( Token.Int>=0 && Token.Int<=255 )
			{
				BYTE B = Token.Int;
				Writer << EX_IntConstByte;
				Writer << B;
			}
			else
			{
				Writer << EX_IntConst;
				Writer << Token.Int;
			}
			break;
		}
		case CPT_Byte:
		{
			Writer << EX_ByteConst;
			Writer << Token.Byte;
			break;
		}
		case CPT_Bool:
		{
			if( Token.Bool ) Writer << EX_True;
			else Writer << EX_False;
			break;
		}
		case CPT_Float:
		{
			Writer << EX_FloatConst;
			Writer << Token.Float;
			break;
		}
		case CPT_String:
		{
			if( appIsPureAnsi(Token.String) )
			{
				Writer << EX_StringConst;
				BYTE OutCh;
				for( TCHAR* Ch=Token.String; *Ch; Ch++ )
				{
					OutCh = ToAnsi(*Ch);
					Writer << OutCh;
				}
				OutCh = 0;
				Writer << OutCh;
			}
			else
			{
				Writer << EX_UnicodeStringConst;
				WORD OutCh;
				for( TCHAR* Ch=Token.String; *Ch; Ch++ )
				{
					OutCh = ToUnicode(*Ch);
					Writer << OutCh;
				}
				OutCh = 0;
				Writer << OutCh;
			}
			break;
		}
		case CPT_ObjectReference:
		{
			if( Token.Object==NULL )
			{
				Writer << EX_NoObject;
			}
			else
			{
				if( Token.PropertyClass->IsChildOf( AActor::StaticClass() ) )
				{
					ScriptErrorf(SCEL_Restricted,  TEXT("Illegal actor constant") );
				}
				Writer << EX_ObjectConst;
				Writer << Token.Object;
			}
			break;
		}
		case CPT_Delegate:
		{
			Writer << EX_EmptyDelegate;
			break;
		}
		case CPT_Name:
		{
			FName N;
			Token.GetConstName(N);
			Writer << EX_NameConst;
			Writer << N;
			break;
		}
		case CPT_Struct:
		{
			if( Token.IsVector() )
			{
				FVector V;
				Token.GetConstVector(V);
				Writer << EX_VectorConst;
				Writer << V;
			}
			else if( Token.IsRotator() )
			{
				FRotator R;
				Token.GetConstRotation(R);
				Writer << EX_RotationConst;
				Writer << R;
			}
			else
			{
				//caveat: Constant structs aren't supported.
				ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Not yet implemented") );
			}
			break;
		}
		default:
		{
			ScriptErrorf(SCEL_Fatal,  TEXT("Internal EmitConstant token type error %i"), (BYTE)Token.Type );
		}
	}
}


extern TMap<class UFunction*,INT> FuncEmitCountMap;

/**
 * Emit the byte code corresponding to a function call
 *
 * @param	Node			the function we're calling
 * @param	ForceFinal		whether we need to bind to the original declaration of the function
 * @param	Global			whether to bind to the global version of the function
 * @param	bSuper			whether to bind to a parent version of this function
 * @param	DelegateProp	whether the function is a delegate?
 */
void FScriptCompiler::EmitStackNodeLinkFunction( UFunction* Node, UBOOL ForceFinal, UBOOL Global, UProperty *DelegateProp )
{
	UBOOL IsFinal = (Node->FunctionFlags & FUNC_Final) || ForceFinal;

	INT *EmitCnt = FuncEmitCountMap.Find(Node);
	if (EmitCnt == NULL)
	{
		FuncEmitCountMap.Set(Node,1);
	}
	else
	{
		*EmitCnt = *EmitCnt + 1;
	}

	// Emit it according to function type.
	if( (Node->FunctionFlags & FUNC_Delegate) != 0 )
	{
		// Delegate function

		// if a NULL property was passed in, assume that this is the default delegate property
		if (DelegateProp == NULL)
		{
			DelegateProp = CastChecked<UProperty>(FindField( CastChecked<UStruct>(Node->GetOuter()), *FString::Printf(TEXT("__%s__Delegate"), *Node->GetName()),TRUE,UProperty::StaticClass()));
		}

		Writer << EX_DelegateFunction;
		BYTE bLocalProp = DelegateProp->GetOuter()->IsA(UFunction::StaticClass());

		// LocalProperties is NULL if the function doesn't declare any locals (meaning this delegate property is a function parameter)
		if ( bLocalProp && LocalProperties != NULL )
		{
			FLocalProperty *CurLocal = LocalProperties->GetLocalProperty(DelegateProp);
			if (CurLocal != NULL)
			{
				CurLocal->bValueReferenced = 1;
			}
		}
		Writer << bLocalProp;
		Writer << DelegateProp;
		FName N(Node->GetFName());
		Writer << N;
	}
	else if( IsFinal && Node->iNative && Node->iNative<256 )
	{
		// One-byte call native final call.
		check( Node->iNative >= EX_FirstNative );
		BYTE B = Node->iNative;
		Writer << B;
	}
	else if( IsFinal && Node->iNative )
	{
		// Two-byte native final call.
		BYTE B = EX_ExtendedNative + (Node->iNative/256);
		check( B < EX_FirstNative );
		BYTE C = (Node->iNative) % 256;
		Writer << B;
		Writer << C;
	}
	else if( IsFinal )
	{
		// Prebound, non-overridable function.
		Writer << EX_FinalFunction;
		Writer << Node;
	}
	else if( Global )
	{
		// Non-state function.
		Writer << EX_GlobalFunction;
		FName N(Node->GetFName());
		Writer << N;
	}
	else
	{
		// Virtual function.
		Writer << EX_VirtualFunction;
		FName N(Node->GetFName());
		Writer << N;
	}
}


/**
 * Remove all placeholders of the specified type from the specified nest's JumpPlaceholderList linked list.
 *
 * @param	Nest	the nest to remove the placeholders from
 * @param	Type	the placeholder type to remove
 * 
 * @return	the number of placeholders that were removed
 */
INT FScriptCompiler::RemoveJumpAddressPlaceholders( FNestInfo* Nest, EFixupType Type )
{
	check(Nest);

	INT Result = 0;

	// first, set the head of the node's list to the first placeholder that isn't a placeholder for the type being removed
	while ( Nest->JumpPlaceholderList != NULL && Nest->JumpPlaceholderList->Type == Type )
	{
		Nest->JumpPlaceholderList = Nest->JumpPlaceholderList->Next;
		Result++;
	}

	// next, remove all intermediate placeholders of this type....
	// at this point, Nest->JumpPlaceholderList is guaranteed to either be NULL or not a placeholder of this type
	for ( FJumpTargetPlaceholder* CurrentNode = Nest->JumpPlaceholderList; CurrentNode && CurrentNode->Next; CurrentNode = CurrentNode->Next )
	{
		FJumpTargetPlaceholder* NextNode = CurrentNode->Next;
		if ( NextNode->Type == Type )
		{
			CurrentNode->Next = NextNode->Next;
			Result++;
		}
	}

	return Result;
}

/**
 * Emit a placeholder for the target of a code jump.
 *
 * @param	Nest		nest currently being compiled
 * @param	Type		type of jump
 * @param	Name		if type is FIXUP_Label, the name of the label
 * @param	Location	the location in the bytecode to emit the placeholder value.  If no value
 *						is specified, the placeholder is emitted at the top of the current node's
 *						script.
 * @param	Value		the placeholder value to emit
 */
void FScriptCompiler::EmitPlaceholderForJumpAddress( FNestInfo* Nest, EFixupType Type, FName Name, INT Location, CodeSkipSizeType Value )
{
	if ( Location == INDEX_NONE )
	{
		Location = TopNode->Script.Num();
	}
	else
	{
		check(TopNode->Script.IsValidIndex(Location));
	}

	// Add current code address to the nest level's fixup list.
	Nest->JumpPlaceholderList = new(GMainThreadMemStack) FJumpTargetPlaceholder( Type, Location, Name, Nest->JumpPlaceholderList );

	// Emit a dummy code offset as a placeholder.
	Writer << Value;
}

/**
 * Emit a code offset which should be chained to later.
 */
void FScriptCompiler::EmitPlaceholderForChainAddress( FNestInfo* Nest )
{
	// Note chain address in nest info.
	Nest->ChainJumpPlaceholderLocation = TopNode->Script.Num();

	// Emit a dummy code offset as a placeholder.
	CodeSkipSizeType Temp=0;
	Writer << Temp;

}

/**
 * Update and reset the nest info's chain address.
 */
void FScriptCompiler::EmitChainAddressValue( FNestInfo* Nest )
{
	// If there's a chain address, plug in the current script offset.
	if( Nest->ChainJumpPlaceholderLocation != INDEX_NONE )
	{
		check((INT)Nest->ChainJumpPlaceholderLocation+(INT)sizeof(CodeSkipSizeType)<=TopNode->Script.Num());
		*(CodeSkipSizeType*)&TopNode->Script( Nest->ChainJumpPlaceholderLocation ) = TopNode->Script.Num();
		Nest->ChainJumpPlaceholderLocation = INDEX_NONE;
	}
}

//( TArray<BYTE>& NodeScript, INT SkipoverSizePos, UField** Field )
void FScriptCompiler::EmitFieldPlaceholderFixup( TArray<BYTE>& NodeScript, INT FixupLocation, UField** Field )
{
	check(NodeScript.IsValidIndex(FixupLocation));

	const INT FieldPos = FixupLocation + sizeof(CodeSkipSizeType);
	check(NodeScript.IsValidIndex(FieldPos));
	check(NodeScript.IsValidIndex(FieldPos + sizeof(ScriptPointerType) - 1));

	ScriptPointerType ScriptProperty = appPointerToSPtr(*Field);
	appMemcpy( &NodeScript(FieldPos), &ScriptProperty, sizeof(ScriptPointerType) );
}

void FScriptCompiler::EmitProperty( FToken& ExpressionToken, const TCHAR* Tag )
{
	BYTE NullPropertyType = CPT_None;
	UProperty* SkipoverProperty = NULL;
	if ( ExpressionToken.TokenFunction != NULL )
	{
		check(ExpressionToken.TokenProperty == NULL);

		// the expression corresponds to a function which returns a value
		// get the token corresponding to the function's return value property
		const FToken& ReturnValueToken = ExpressionToken.TokenFunction->GetReturnData();
		SkipoverProperty = ReturnValueToken.TokenProperty;
	}
	else
	{
		// expression corresponds to a UProperty or dynamic array operation
		SkipoverProperty = ExpressionToken.TokenProperty;

		if ( ExpressionToken.TokenProperty == NULL && ExpressionToken.Type != CPT_None )
		{
			if ( ExpressionToken.TokenType == TOKEN_None )
			{
				// dynamic array Add, Length, etc. (INT)
				// dynamic cast to another type i.e. float(SomeVar), AnEnum(SomeValue), etc.
				NullPropertyType = ExpressionToken.Type;
			}
		}
	}

	Writer << SkipoverProperty;
	Writer << NullPropertyType;
}

//
// Emit an assignment.
//
void FScriptCompiler::EmitLet( const FPropertyBase& Type, const TCHAR *Tag )
{
	// Validate the required type.
	if( Type.PropertyFlags & CPF_Const )
	{
		ScriptErrorf(SCEL_Restricted,  TEXT("Can't assign Const variables") );
	}
	if( Type.ArrayDim > 1 )
	{
		ScriptErrorf(SCEL_Restricted,  TEXT("Can only assign individual elements, not arrays") );
	}

	EmitDebugInfo(DI_Let);

	// ArrayDim of 0 indicates assignment of an entire dynamic array 
	if( Type.Type == CPT_Bool && Type.ArrayDim != 0 )
	{
		Writer << EX_LetBool;
	}
	else if( Type.Type == CPT_Delegate && Type.ArrayDim != 0 )
	{
		Writer << EX_LetDelegate;
	}
	else
	{
		Writer << EX_Let;
	}
}

/**
 * Emits bytecodes for performing a type conversion.
 *
 * @param	ConversionType		the type of conversion to perform
 * @param	InsertionPoint		the location to insert the conversion bytecodes.
 * @param	DestinationToken	the token containing the data for the target of the conversion
 */
void FScriptCompiler::EmitConversion( ECastToken ConversionType, FScriptLocation& InsertionPoint, const FPropertyBase& DestinationToken )
{
	FScriptLocation CurrentScriptLocation;

	if ( (ConversionType&CONVERT_MASK) == CST_ObjectToInterface )
	{
		check(DestinationToken.PropertyClass);

		Writer << EX_InterfaceCast;
		Writer << (UObject*&)DestinationToken.PropertyClass;
	}
	else
	{
		Writer << EX_PrimitiveCast;
		Writer << (ECastToken)(ConversionType & CONVERT_MASK);
	}

	MoveCompiledCode(InsertionPoint,CurrentScriptLocation);
}

/*-----------------------------------------------------------------------------
	Signatures.
-----------------------------------------------------------------------------*/

static const TCHAR* CppTags[96] =
{TEXT("Spc")		,TEXT("Not")			,TEXT("DoubleQuote")	,TEXT("Pound")
,TEXT("Concat")		,TEXT("Percent")		,TEXT("And")			,TEXT("SingleQuote")
,TEXT("OpenParen")	,TEXT("CloseParen")		,TEXT("Multiply")		,TEXT("Add")
,TEXT("Comma")		,TEXT("Subtract")		,TEXT("Dot")			,TEXT("Divide")
,TEXT("0")			,TEXT("1")				,TEXT("2")				,TEXT("3")
,TEXT("4")			,TEXT("5")				,TEXT("6")				,TEXT("7")
,TEXT("8")			,TEXT("9")				,TEXT("Colon")			,TEXT("Semicolon")
,TEXT("Less")		,TEXT("Equal")			,TEXT("Greater")		,TEXT("Question")
,TEXT("At")			,TEXT("A")				,TEXT("B")				,TEXT("C")
,TEXT("D")			,TEXT("E")				,TEXT("F")				,TEXT("G")
,TEXT("H")			,TEXT("I")				,TEXT("J")				,TEXT("K")
,TEXT("L")			,TEXT("M")				,TEXT("N")				,TEXT("O")
,TEXT("P")			,TEXT("Q")				,TEXT("R")				,TEXT("S")
,TEXT("T")			,TEXT("U")				,TEXT("V")				,TEXT("W")
,TEXT("X")			,TEXT("Y")				,TEXT("Z")				,TEXT("OpenBracket")
,TEXT("Backslash")	,TEXT("CloseBracket")	,TEXT("Xor")			,TEXT("_")
,TEXT("Not")		,TEXT("a")				,TEXT("b")				,TEXT("c")
,TEXT("d")			,TEXT("e")				,TEXT("f")				,TEXT("g")
,TEXT("h")			,TEXT("i")				,TEXT("j")				,TEXT("k")
,TEXT("l")			,TEXT("m")				,TEXT("n")				,TEXT("o")
,TEXT("p")			,TEXT("q")				,TEXT("r")				,TEXT("s")
,TEXT("t")			,TEXT("u")				,TEXT("v")				,TEXT("w")
,TEXT("x")			,TEXT("y")				,TEXT("z")				,TEXT("OpenBrace")
,TEXT("Or")			,TEXT("CloseBrace")		,TEXT("Complement")		,TEXT("Or") };

/*-----------------------------------------------------------------------------
	Tokenizing.
-----------------------------------------------------------------------------*/

/**
 * Gets the next token from the input stream, advancing the variables which keep track of the current input position and line.
 *
 * @param	Token			receives the value of the parsed text; if Token is pre-initialized, special logic is performed
 *							to attempt to evaluated Token in the context of that type.  Useful for distinguishing between ambigous symbols
 *							like enum tags.
 * @param	Hint			optional tag for providing additional information about the type of token that is expected.
 * @param	NoConsts		specify TRUE to indicate that tokens representing literal const values are not allowed.
 *
 * @return	TRUE if a token was successfully processed, FALSE otherwise.
 */
UBOOL FScriptCompiler::GetToken( FToken& Token, const FPropertyBase* Hint/*=NULL*/, UBOOL NoConsts/*=FALSE*/ )
{
	Token.TokenName	= NAME_None;
	TCHAR c = GetLeadingChar();
	TCHAR p = PeekChar();
	if( c == 0 )
	{
		UngetChar();
		return 0;
	}
	Token.StartPos		= PrevPos;
	Token.StartLine		= PrevLine;
	if( (c>='A' && c<='Z') || (c>='a' && c<='z') || (c=='_') )
	{
		// Alphanumeric token.
		INT Length=0;
		do
		{
			Token.Identifier[Length++] = c;
			if( Length >= NAME_SIZE )
			{
				ScriptErrorf(SCEL_Limit,  TEXT("Identifer length exceeds maximum of %i"), (INT)NAME_SIZE);
				Length = ((INT)NAME_SIZE) - 1;
				break;
			}
			c = GetChar();
		} while( ((c>='A')&&(c<='Z')) || ((c>='a')&&(c<='z')) || ((c>='0')&&(c<='9')) || (c=='_') );
		UngetChar();
		Token.Identifier[Length]=0;

		// Assume this is an identifier unless we find otherwise.
		Token.TokenType = TOKEN_Identifier;

		// Lookup the token's global name.
		Token.TokenName = FName( Token.Identifier, FNAME_Find, TRUE );

		// If const values are allowed, determine whether the identifier represents a constant
		if ( !NoConsts )
		{
			// See if the identifier is part of a vector, rotation or other struct constant.
			if( Token.TokenName==NAME_Vect && MatchSymbol(TEXT("(")) )
			{
				// This is a vector constant.
				FVector V = FVector( 0.f, 0.f, 0.f );
				if(!GetConstFloat(V.X))
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing X component of vector" ));
					///return FALSE;
				}
				if( !MatchSymbol(TEXT(",")) )
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing ',' in vector" ));
					///return FALSE;
				}
				if(!GetConstFloat(V.Y))
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing Y component of vector" ));
					///return FALSE;
				}
				if(!MatchSymbol(TEXT(",")))
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing ',' in vector" ));
					///return FALSE;
				}
				if(!GetConstFloat(V.Z))
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing Z component of vector"  ));
					///return FALSE;
				}
				if(!MatchSymbol(TEXT(")")))
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing ')' in vector" ));
					///return FALSE;
				}

				Token.SetConstVector(V);
				return TRUE;
			}
			else if( Token.TokenName==NAME_Rot && MatchSymbol(TEXT("(")) )
			{
				// This is a rotation constant.
				FRotator R;
				if(!GetConstInt(R.Pitch))   
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing Pitch component of rotation" ));
					///return FALSE;
				}
				if(!MatchSymbol(TEXT(",")))
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing ',' in rotation" ));
					///return FALSE;
				}
				if(!GetConstInt(R.Yaw))
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing Yaw component of rotation" ));
					///return FALSE;
				}
				if(!MatchSymbol(TEXT(",")))
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing ',' in rotation" ));
					///return FALSE;
				}
				if(!GetConstInt(R.Roll))
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing Roll component of rotation" ));
					///return FALSE;
				}
				if(!MatchSymbol(TEXT(")")))
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing ')' in rotation" ));
					///return FALSE;
				}

				Token.SetConstRotation(R);
				return TRUE;
			}

			// boolean true/false
			else if( Token.TokenName==NAME_TRUE )
			{
				Token.SetConstBool(1);
				return TRUE;
			}
			else if( Token.TokenName==NAME_FALSE )
			{
				Token.SetConstBool(0);
				return TRUE;
			}

			// special ArrayCount token
			else if( Token.TokenName==NAME_ArrayCount )
			{
				FToken TypeToken;
				RequireSizeOfParm( TypeToken, TEXT("'ArrayCount'") );
				if( TypeToken.ArrayDim<=1 )
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("ArrayCount argument is not an array") );
					///return FALSE;
				}
				Token.SetConstInt( TypeToken.ArrayDim );
				return TRUE;
			}

			else if (Token.TokenName == NAME_NameOf)
			{
				FToken TypeToken;
				RequireSizeOfParm(TypeToken, TEXT("'NameOf'"));
				if (TypeToken.TokenType != TOKEN_Identifier && (TypeToken.TokenType != TOKEN_Const || (TypeToken.TokenProperty == NULL && TypeToken.TokenFunction == NULL)))
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("NameOf argument is not an identifier"));
					///return FALSE;
				}
				Token.SetConstName(TypeToken.TokenName);
				return TRUE;
			}

			else if( Token.Matches(TEXT("None")) )
			{
				if ( Hint != NULL && Hint->Type == CPT_Delegate )
				{
					Token.SetConstDelegate();
				}
				else
				{
					Token.SetConstObject( NULL );
				}
				return TRUE;
			}

			// resolve enum variables, which we can only evaluate with knowledge about the specified type.
			if( Token.TokenName != NAME_None && Hint && Hint->Type == CPT_Byte && Hint->Enum )
			{
				// Find index into the enumeration.
				const INT EnumIndex = Hint->Enum->FindEnumIndex(Token.TokenName);
				if( EnumIndex != INDEX_NONE )
				{
					Token.SetConstByte(Hint->Enum, EnumIndex);
					return TRUE;
				}
			}	
			
			// Try to find a const Enum reference
			if( Token.TokenName != NAME_None && Hint && (Hint->Type == CPT_Int || Hint->Type == CPT_Byte) )
			{
				check(GIsUCCMake);
				extern TMap<FName,INT> GUCCMakeEnumNameToIndexMap;
				INT* IndexPtr = GUCCMakeEnumNameToIndexMap.Find( Token.TokenName );
				if( IndexPtr && *IndexPtr != INDEX_NONE )
				{
					// Found an enum tag!
					Token.SetConstInt(*IndexPtr);
					return TRUE;
				}
			}

			// See if this is a general object constant.
			if( PeekSymbol(TEXT("'")) )
			{
				UClass* Type = FindClass(Token.Identifier);//FindObject<UClass>( ANY_PACKAGE, Token.Identifier );
				if
					(	Type
					&&	!Type->IsChildOf( AActor::StaticClass() )
					&&	MatchSymbol(TEXT("'")) )
				{
					// This is an object constant.
					FString Str			= TEXT("");
					// GetToken doesn't handle '-' as an alphanumeric character so we need to manually fudge to allow fully qualified names to contain '-'.
					UBOOL	TokenIsDash = false;
					CheckInScope( Type );

					INT StartingInputLine = InputLine;
					while (!MatchSymbol(TEXT("'")))
					{
						c = GetChar(FALSE);
						if (IsEOL(c) || InputLine != StartingInputLine)
						{
							ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Unterminated constant %s name"), *Type->GetName());
							return FALSE;
						}
						Str += c;
					}

					// validate the specified name
					if (Str.Len() == 0)
					{
						ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing %s name"), *Type->GetName());
					}
					else
					{
						for (INT i = 0; i < ARRAY_COUNT(INVALID_OBJECTNAME_CHARACTERS) - 1; i++)
						{
							if (INVALID_OBJECTNAME_CHARACTERS[i] != TEXT('.'))
							{
								TCHAR TestChar[2] = {INVALID_OBJECTNAME_CHARACTERS[i],0};
								if (Str.InStr(TestChar) != INDEX_NONE)
								{
									ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Illegal object name in constant %s"), *Type->GetName());
								}
							}
						}
					}

					// Find object.
					UObject* Ob = UObject::StaticFindObject( Type, ANY_PACKAGE, *Str );
					if( Ob == NULL )
					{
						if ( Type == UClass::StaticClass() )
						{
							Ob = FindClass(*Str);
						}

						// attempt to load the object
						if ( Ob == NULL )
						{
							FScopedRedirectorCatcher Catcher(*Str);
							Ob = UObject::StaticLoadObject( Type, NULL, *Str, NULL, LOAD_Quiet | LOAD_NoVerify | LOAD_NoWarn | LOAD_FindIfFail, NULL );

							// if the object was a redirector, spit out a compiler error
							if (Catcher.WasRedirectorFollowed())
							{
								ScriptErrorf(SCEL_Restricted, TEXT("Object reference '%s' pointed to a Redirector. Change text to:\n   '%s'."), *Str, *Ob->GetPathName());
							}
						}
					}

					if( Ob == NULL )
					{
						ScriptWarnf(SCWL_Level4, TEXT("Unresolved reference to %s '%s'"), *Type->GetName(), *Str );
					}

					CheckInScope( Ob );

					// Got a constant object.
					Token.SetConstObject( Ob );

					// Don't use the class of the resolved object to determine the type for this token - always use the object type that was specified in
					// the literal object reference.....otherwise we can end up in a situation where script that compiled fine suddenly fails to compile if
					// the object referenced is deleted or is no longer a type that matches the expected type (in cases where we're passing a literal object
					// reference to a script function using the wrong type in the object expression
					// [e.g. Texture'Package.SomeTexture2D' when the function takes a Texture2D param])
					Token.PropertyClass = Type;
					return TRUE;
				}
			}
		}
		return TRUE;
	}

	// if const values are allowed, determine whether the non-identifier token represents a const
	else if ( !NoConsts && ((c>='0' && c<='9') || ((c=='+' || c=='-') && (p>='0' && p<='9'))) )
	{
		// Integer or floating point constant.
		int  IsFloat = 0;
		int  Length  = 0;
		int  IsHex   = 0;
		do
		{
			if( c==TEXT('.') )
			{
				IsFloat = 1;
			}
			if( c==TEXT('X') || c == TEXT('x') )
			{
				IsHex   = 1;
			}

			Token.Identifier[Length++] = c;
			if( Length >= NAME_SIZE )
			{
				ScriptErrorf(SCEL_Limit,  TEXT("Number length exceeds maximum of %i "), (INT)NAME_SIZE );
				Length = ((INT)NAME_SIZE) - 1;
				break;
			}
			c = appToUpper(GetChar());
		} while ((c >= TEXT('0') && c <= TEXT('9')) || (!IsFloat && c == TEXT('.')) || (!IsHex && c == TEXT('X')) || (IsHex && c >= TEXT('A') && c <= TEXT('F')));

		Token.Identifier[Length]=0;
		if (!IsFloat || c != 'F')
		{
			UngetChar();
		}

		if( IsFloat )
		{
			Token.SetConstFloat( appAtof(Token.Identifier) );
		}
		else if( IsHex )
		{
			TCHAR* End = Token.Identifier + appStrlen(Token.Identifier);
			if ( Hint != NULL && Hint->Type == CPT_Byte )
			{
				Token.SetConstByte( Hint->Enum, static_cast<BYTE>(appStrtoi(Token.Identifier, &End, 0) & 0xFF) );
			}
			else
			{
				Token.SetConstInt( appStrtoi(Token.Identifier,&End,0) );
			}
		}
		else if ( Hint != NULL && Hint->Type == CPT_Byte )
		{
			Token.SetConstByte( Hint->Enum, static_cast<BYTE>(appAtoi(Token.Identifier) & 0xFF) );
		}
		else
		{
			Token.SetConstInt( appAtoi(Token.Identifier) );
		}
		return TRUE;
	}
	else if( !NoConsts && c=='\'' )
	{
		// Name constant.
		int Length=0;
		c = GetChar();
		while( (c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || (c=='_') || (c=='-') || (c==' ') ) //@FIXME: space in names should be illegal!
		{
			Token.Identifier[Length++] = c;
			if( Length >= NAME_SIZE )
			{
				ScriptErrorf(SCEL_Limit,  TEXT("Name length exceeds maximum of %i"), (INT)NAME_SIZE );
				// trick the error a few lines down
				c = TEXT('\'');
				Length = ((INT)NAME_SIZE) - 1;
				break;
			}
			c = GetChar();
		}
		if( c != '\'' )
		{
			UngetChar();
			ScriptErrorf(SCEL_Formatting,  TEXT("Illegal character in name") );
		}
		Token.Identifier[Length]=0;

		// Make constant name.
		Token.SetConstName( FName(Token.Identifier) );
		return TRUE;
	}
	else if( c=='"' )
	{
		// String constant.
		TCHAR Temp[MAX_STRING_CONST_SIZE];
		INT Length=0;
		c = GetChar(1);
		while( (c!='"') && !IsEOL(c) )
		{
			if( c=='\\' )
			{
				c = GetChar(1);
				if( IsEOL(c) )
				{
					break;
				}
				else if(c == 'n')
				{
					// Newline escape sequence.
					c = '\n';
				}
			}
			Temp[Length++] = c;
			if( Length >= MAX_STRING_CONST_SIZE )
			{
				ScriptErrorf(SCEL_Limit,  TEXT("String constant exceeds maximum of %i characters"), (INT)MAX_STRING_CONST_SIZE );
				c = TEXT('\"');
				Length = ((INT)MAX_STRING_CONST_SIZE) - 1;
				break;
			}
			c = GetChar(1);
		}
		Temp[Length]=0;

		if( c != '"' )
		{
			ScriptErrorf(SCEL_Formatting, TEXT("Unterminated string constant: %s"), Temp);
			UngetChar();
		}

		Token.SetConstString(Temp);
		return TRUE;
	}
	else
	{
		// Symbol.
		INT Length=0;
		Token.Identifier[Length++] = c;

		// Handle special 2-character symbols.
		#define PAIR(cc,dd) ((c==cc)&&(d==dd)) /* Comparison macro for convenience */
		TCHAR d = GetChar();
		if
		(	PAIR('<','<')
		||	PAIR('>','>')
		||	PAIR('!','=')
		||	PAIR('<','=')
		||	PAIR('>','=')
		||	PAIR('+','+')
		||	PAIR('-','-')
		||	PAIR('+','=')
		||	PAIR('-','=')
		||	PAIR('*','=')
		||	PAIR('/','=')
		||	PAIR('&','&')
		||	PAIR('|','|')
		||	PAIR('^','^')
		||	PAIR('=','=')
		||	PAIR('*','*')
		||	PAIR('~','=')
		||	PAIR('@','=')
		||	PAIR('$','=')
		)
		{
			Token.Identifier[Length++] = d;
			if( c=='>' && d=='>' )
			{
				if( GetChar()=='>' )
					Token.Identifier[Length++] = '>';
				else
					UngetChar();
			}
		}
		else UngetChar();
		#undef PAIR

		Token.Identifier[Length] = 0;
		Token.TokenType = TOKEN_Symbol;

		// Lookup the token's global name.
		Token.TokenName = FName( Token.Identifier, FNAME_Find, TRUE );

		return TRUE;
	}
}

/**
 * Put all text from the current position up to either EOL or the StopToken
 * into Token.  Advances the compiler's current position.
 *
 * @param	Token	[out] will contain the text that was parsed
 * @param	StopChar	stop processing when this character is reached
 *
 * @return	the number of character parsed
 */
UBOOL FScriptCompiler::GetRawToken( FToken& Token, TCHAR StopChar /* = TCHAR('\n') */ )
{
	// Get token after whitespace.
	TCHAR Temp[MAX_STRING_CONST_SIZE];
	INT Length=0;
	TCHAR c = GetLeadingChar();
	while( !IsEOL(c) && c != StopChar )
	{
		if( (c=='/' && PeekChar()=='/') || (c=='/' && PeekChar()=='*') )
		{
			break;
		}
		Temp[Length++] = c;
		if( Length >= MAX_STRING_CONST_SIZE )
		{
			ScriptErrorf(SCEL_Limit,  TEXT("Identifier exceeds maximum of %i characters"), (INT)MAX_STRING_CONST_SIZE );
			c = GetChar(TRUE);
			Length = ((INT)MAX_STRING_CONST_SIZE) - 1;
			break;
		}
		c = GetChar(TRUE);
	}
	UngetChar();

	// Get rid of trailing whitespace.
	while( Length>0 && (Temp[Length-1]==' ' || Temp[Length-1]==9 ) )
	{
		Length--;
	}
	Temp[Length]=0;

	Token.SetConstString(Temp);

	return Length>0;
}

//
// Get an identifier token, return 1 if gotten, 0 if not.
//
UBOOL FScriptCompiler::GetIdentifier( FToken& Token, INT NoConsts )
{
	if( !GetToken( Token, NULL, NoConsts ) )
	{
		return FALSE;
	}

	if( Token.TokenType == TOKEN_Identifier )
	{
		return TRUE;
	}

	UngetToken(Token);
	return FALSE;
}

//
// Get a symbol token, return 1 if gotten, 0 if not.
//
UBOOL FScriptCompiler::GetSymbol( FToken& Token )
{
	if( !GetToken(Token) )
	{
		return 0;
	}

	if( Token.TokenType == TOKEN_Symbol )
	{
		return 1;
	}

	UngetToken(Token);
	return 0;
}

//
// Get an integer constant, return 1 if gotten, 0 if not.
//
UBOOL FScriptCompiler::GetConstInt( INT& Result, const TCHAR* Tag, UStruct* Scope )
{
	FToken Token;
	if( GetToken(Token) )
	{
		if( Token.GetConstInt(Result) )
		{
			return TRUE;
		}
		else if( Scope != 0 )
		{
			INT OuterContextCount;
			UField* Field=FindField( Scope, Token.Identifier, TRUE, UConst::StaticClass(), NULL, &OuterContextCount );
			if( Field != NULL )
			{
				UConst* Const = CastChecked<UConst>( Field );
				FToken ConstToken;
				FScriptLocation Retry;

				// Kludge: use GetToken to parse the const's value
				Input = *Const->Value;
				InputPos = 0;
				const INT SavedInputLen = InputLen;
				InputLen = Const->Value.Len();
				if( !GetToken( ConstToken ) || ConstToken.TokenType!=TOKEN_Const )
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Error in constant") );
				}
				ReturnToLocation(Retry,0,1);
				InputLen = SavedInputLen;

				if( ConstToken.GetConstInt(Result) )
				{
					return TRUE;
				}
				else
				{
					UngetToken(Token);
				}
			}
			else
			{
				UngetToken(Token);
			}
		}
		else
			UngetToken(Token);
	}

	if( Tag )
	{
		ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("%s: Missing constant integer"), Tag );
	}
	return FALSE;

}

//
// Get a real number, return 1 if gotten, 0 if not.
//
UBOOL FScriptCompiler::GetConstFloat( FLOAT& Result, const TCHAR* Tag )
{
	FToken Token;
	if( GetToken(Token) )
	{
		if( Token.GetConstFloat(Result) )
			return 1;
		else
			UngetToken(Token);
	}

	if( Tag )
	{
		ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("%s: Missing constant float"), Tag);
	}

	return 0;
}

//
// Get a specific identifier and return 1 if gotten, 0 if not.
// This is used primarily for checking for required symbols during compilation.
//
UBOOL FScriptCompiler::MatchIdentifier( FName Match )
{
	FToken Token;

	if( !GetToken(Token) )
		return 0;

	if( (Token.TokenType==TOKEN_Identifier) && Token.TokenName==Match )
		return 1;

	UngetToken(Token);
	return 0;
}

//
// Get a specific symbol and return 1 if gotten, 0 if not.
//
UBOOL	FScriptCompiler::MatchSymbol( const TCHAR* Match )
{
	FToken Token;

	if( !GetToken(Token,NULL,1) )
		return 0;

	if( Token.TokenType==TOKEN_Symbol && !appStricmp(Token.Identifier,Match) )
		return 1;

	UngetToken(Token);
	return 0;
}

//
// Peek ahead and see if a symbol follows in the stream.
//
UBOOL FScriptCompiler::PeekSymbol( const TCHAR* Match )
{
	FToken Token;
	if( !GetToken(Token,NULL,1) )
	{
		return 0;
	}
	UngetToken(Token);

	return Token.TokenType==TOKEN_Symbol && appStricmp(Token.Identifier,Match)==0;
}

//
// Peek ahead and see if an identifier follows in the stream.
//
UBOOL FScriptCompiler::PeekIdentifier( FName Match )
{
	FToken Token;
	if( !GetToken(Token,NULL,1) )
	{
		return 0;
	}
	UngetToken(Token);

	return Token.TokenType==TOKEN_Identifier && Token.TokenName==Match;
}

//
// Unget the most recently gotten token.
//
void FScriptCompiler::UngetToken( FToken& Token )
{
	InputPos	= Token.StartPos;
	InputLine   = Token.StartLine;
}

//
// Require a symbol.
//
void FScriptCompiler::RequireSymbol( const TCHAR* Match, const TCHAR* Tag )
{
	if( !MatchSymbol(Match) )
	{
		ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing '%s' in %s"), Match, Tag );
	}
}

//
// Require an identifier.
//
void FScriptCompiler::RequireIdentifier( FName Match, const TCHAR* Tag )
{
	if( !MatchIdentifier(Match) )
	{
		ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing '%s' in %s"), *Match.ToString(), Tag );
	}
}

//
// Require a SizeOf-style parenthesis-enclosed type.
//
void FScriptCompiler::RequireSizeOfParm( FToken& TypeToken, const TCHAR* Tag )
{
	// Setup a retry point.
	FScriptLocation Retry;

	// Get leading paren.
	RequireSymbol( TEXT("("), Tag );

	// Get an expression.
	if( !CompileExpr( FPropertyBase(CPT_None,CPRT_SimpleReference), Tag, &TypeToken ) )
	{
		ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Bad or missing expression in %s"), Tag );
	}

	// Get trailing paren.
	RequireSymbol( TEXT(")"), Tag );

	// Return binary code pointer (not script text) to where it was.
	ReturnToLocation( Retry, 1, 0 );
}

//
// Get a qualified class.
//
UClass* FScriptCompiler::GetQualifiedClass( const TCHAR* Thing )
{
	UClass* Result = NULL;
	TCHAR ClassName[256]=TEXT("");

	while( 1 )
	{
		FToken Token;
		if( !GetIdentifier(Token) )
		{
			break;
		}
		appStrncat( ClassName, Token.Identifier, ARRAY_COUNT(ClassName) );
		if( !MatchSymbol(TEXT(".")) )
		{
			break;
		}
		appStrncat( ClassName, TEXT("."), ARRAY_COUNT(ClassName) );
	}

	if( ClassName[0] )
	{
		Result = FindClass(ClassName);
		if( !Result )
		{
			ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Class '%s' not found"), ClassName );
		}
	}
	else if( Thing )
	{
		ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("%s: Missing class name"), Thing );
	}
	return Result;
}

/*-----------------------------------------------------------------------------
	Fields.
-----------------------------------------------------------------------------*/

/**
 * Find a field in the specified context.  Starts with the specified scope, then iterates
 * through the Outer chain (including ClassWithin-type outers) until the field is found.
 * 
 * @param	InScope				scope to start searching for the field in 
 * @param	InIdentifier		name of the field we're searching for
 * @param	bIncludeParents		whether to allow searching in the scope of a parent struct
 * @param	FieldClass			class of the field to search for.  used to e.g. search for functions only
 * @param	Thing				hint text that will be used in the error message if an error is encountered
 * @param	OuterContextCount	[out] if non-NULL, set to the number of 'Outer' contexts which must be emitted if the field is found in an outer class (ClassWithin),
 *								which will require calling EmitOuterContext() before emitting the property reference bytecodes
 *
 * @return	a pointer to a UField with a name matching InIdentifier, or NULL if it wasn't found
 */
UField* FScriptCompiler::FindField
(
	UStruct*		Scope,
	const TCHAR*	InIdentifier,
	UBOOL			bIncludeParents,
	UClass*			FieldClass,
	const TCHAR*	Thing,
	INT*			OuterContextCount
)
{
	check(InIdentifier);
	FName InName( InIdentifier, FNAME_Find, TRUE );
	if( InName!=NAME_None )
	{
		if ( OuterContextCount != NULL )
		{
			// ensure that we're starting our count at 0
			*OuterContextCount = 0;
		}

		UBOOL bSearchingOuterContext = FALSE;
		do
		{
			UStruct* OriginalScope = Scope;
			for( Scope; Scope; Scope=Cast<UStruct>( Scope->GetOuter()) )
			{
				for( TFieldIterator<UField> It(Scope); It; ++It )
				{
					if ( bIncludeParents || It.GetStruct() == OriginalScope )
					{
						if( It->GetFName()==InName )
						{
							if( !It->IsA(FieldClass) )
							{
								if( Thing && !bSearchingOuterContext )
								{
									ScriptErrorf(SCEL_NestLevel,  TEXT("%s: expecting %s, got %s"), Thing, *FieldClass->GetName(), *It->GetClass()->GetName() );
								}
								return NULL;
							}
							return *It;
						}
					}
				}
			}

			if ( OuterContextCount != NULL )
			{
				// if the field wasn't found and this is a script class declared 'within'
				// another class, search for the field in the scope of the ClassWithin class
				check(OriginalScope);
				UClass* OuterClass = OriginalScope->GetOwnerClass()->ClassWithin;
				if( OuterClass && OuterClass!=UObject::StaticClass() )
				{
					(*OuterContextCount)++;
					bSearchingOuterContext = TRUE;
					Scope = OuterClass;
				}
			}
		}
		while( Scope != NULL );
	}

	return NULL;
}

// Check if a field obscures a field in an outer scope.
void FScriptCompiler::CheckObscures( UStruct* Scope, FToken& Token )
{
	if ( bReparsingClass )
		return;

	UStruct* BaseScope = Scope->GetInheritanceSuper();
	while( BaseScope )
	{
		INT OuterContextCount = 0;
		UField* Existing = FindField( BaseScope, Token.Identifier, FALSE, UField::StaticClass(), NULL, &OuterContextCount );
		if( Existing )
		{
			ScriptWarnf(SCWL_Level4, TEXT("'%s' obscures '%s' defined in %s class '%s'."), *Token.TokenName.ToString(), *Token.TokenName.ToString(), OuterContextCount ? TEXT("outer") : TEXT("base"), *Existing->GetOuter()->GetName());
		}
		BaseScope = BaseScope->GetInheritanceSuper();
	}
}


/**
 * @return	TRUE if Scope has UProperty objects in its list of fields
 */
UBOOL FScriptCompiler::HasMemberProperties( const UStruct* Scope )
{
	// it's safe to pass a NULL Scope to TFieldIterator, but this function shouldn't be called with a NULL Scope
	checkSlow(Scope);
	TFieldIterator<UProperty> It(Scope,FALSE);
	return It ? TRUE : FALSE;
}

/**
 * Get the parent struct specified.
 *
 * @param	CurrentScope	scope to start in
 * @param	SearchName		parent scope to search for
 *
 * @return	a pointer to the parent struct with the specified name, or NULL if the parent couldn't be found
 */
UStruct* FScriptCompiler::GetSuperScope( UStruct* CurrentScope, const FName& SearchName )
{
	UStruct* SuperScope = CurrentScope;
	while (SuperScope && !SuperScope->GetInheritanceSuper())
	{
		SuperScope = CastChecked<UStruct>(SuperScope->GetOuter());
	}
	if (SuperScope != NULL)
	{
		// iterate up the inheritance chain looking for one that has the desired name
		do
		{
			UStruct* NextScope = SuperScope->GetInheritanceSuper();
			if (NextScope)
			{
				SuperScope = NextScope;
			}
			// if we reached the top and the scope is not a class, try the current class heirarchy next
			else if (SuperScope->GetClass() != UClass::StaticClass())
			{
				SuperScope = Class;
			}
			else
			{
				// otherwise we've failed
				SuperScope = NULL;
			}
		} while (SuperScope != NULL && SuperScope->GetFName() != SearchName);
	}

	return SuperScope;
}

/*-----------------------------------------------------------------------------
	Variables.
-----------------------------------------------------------------------------*/

//
// Compile an enumeration definition.
//
UEnum* FScriptCompiler::CompileEnum( UStruct* Scope, UBOOL& NeedSemicolon )
{
	check(Scope);

	// Make sure enums can be declared here.
	if( TopNest->NestType != NEST_Class && TopNest->NestType != NEST_Interface )
	{
		ScriptErrorf(SCEL_Unknown,  TEXT("Enums can only be declared at class scope") );
	}

	FScriptLocation DeclarationPosition;

	// Get enumeration name.
	FToken EnumToken;
	if( !GetIdentifier(EnumToken) )
	{
		ScriptErrorf(SCEL_Unknown/*SCEL_Parse*/,  TEXT("Missing enumeration name") );
	}

	// Verify that the enumeration definition is unique within this scope.
	UField* Existing = FindField( Scope, EnumToken.Identifier );
	if( Existing && Existing->GetOuter()==Scope && !bReparsingClass )
	{
		ScriptErrorf(SCEL_Unknown/*SCEL_Parse*/,  TEXT("enum: '%s' already defined here"), *EnumToken.TokenName.ToString() );
	}

	CheckObscures( Scope, EnumToken );
	if (!ParsePropertyMetaData(EnumToken, EnumToken.Identifier))
	{
		ReturnToLocation(DeclarationPosition, FALSE, TRUE);
		ScriptErrorf(SCEL_Unknown/*SCEL_Parse*/, TEXT("Metadata was not formatted properly")); 
	}

	// Get opening brace.
	RequireSymbol( TEXT("{"), TEXT("'Enum'") );

	if ( bReparsingClass && Existing != NULL )
	{
		SkipStatements(1, TEXT("enum"));
		return (UEnum*)Existing;
	}

	// Create enum definition.
	UEnum* Enum = new(Scope, EnumToken.Identifier, RF_Public) UEnum();
	Enum->Next = Scope->Children;
	Scope->Children = Enum;

	// if metadata was specified for this enum, add it to the package now.
	if ( EnumToken.MetaData.Num() > 0 )
	{
		UMetaData* PackageMetaData = Enum->GetOutermost()->GetMetaData();
		check(PackageMetaData);

		PackageMetaData->SetObjectValues( Enum, EnumToken.MetaData );
	}

	// Parse all enums tags.
	FToken TagToken;

	TArray<FScriptLocation> EnumTagLocations;
	TArray<FName> EnumNames;

	TMap<FName,FString> EnumValueMetaData;
	while( GetIdentifier(TagToken) )
	{
		FScriptLocation* ValueDeclarationPos = new(EnumTagLocations) FScriptLocation();

		INT iFound;
		FName NewTag(TagToken.Identifier, FNAME_Add, TRUE);
		if( EnumNames.FindItem(NewTag,iFound) )
		{
			ScriptErrorf(SCEL_Unknown/*SCEL_Parse*/,  TEXT("Duplicate enumeration tag %s"), TagToken.Identifier );
		}
		else
		{
			if ( EnumNames.Num() < 256 )
			{
				EnumNames.AddItem( NewTag );
			}
			else
			{
				ScriptErrorf(SCEL_Limit,  TEXT("Exceeded maximum of 255 enumerators") );
				break;
			}
		}

		// check for metadata on this enum value
		if (!ParsePropertyMetaData(TagToken, TagToken.Identifier))
		{
			ReturnToLocation(*ValueDeclarationPos, FALSE, TRUE);
			ScriptErrorf(SCEL_Unknown/*SCEL_Parse*/, TEXT("Metadata was not formatted properly"));
		}
		else if ( TagToken.MetaData.Num() > 0 )
		{
			// special case for enum value metadata - we need to prepend the key name with the enum value name
			const FString TokenString = TagToken.Identifier;
			for ( TMap<FName,FString>::TIterator MetaDataIt(TagToken.MetaData); MetaDataIt; ++MetaDataIt )
			{
				FString KeyString = TokenString + TEXT(".") + MetaDataIt.Key().ToString();
				EnumValueMetaData.Set(FName(*KeyString), MetaDataIt.Value());
			}

			// now clear the metadata because we're going to reuse this token for parsing the next enum value
			TagToken.MetaData.Empty();
		}

		if( !MatchSymbol(TEXT(",")) )
		{
			break;
		}
	}

	if ( EnumValueMetaData.Num() > 0 )
	{
		// process metadata for individual enum values
		UMetaData* PackageMetaData = Enum->GetOutermost()->GetMetaData();
		checkSlow(PackageMetaData);

		PackageMetaData->SetObjectValues( Enum, EnumValueMetaData );
	}


	if( !EnumNames.Num() )
	{
		ScriptErrorf(SCEL_Unknown/*SCEL_Parse*/,  TEXT("Enumeration must contain at least one enumerator") );
	}

	// Trailing brace.
	RequireSymbol( TEXT("}"), TEXT("'Enum'") );

	// Register the list of enum names.
	if ( !Enum->SetEnums( EnumNames ) )
	{
		const FName MaxEnumItem = *(Enum->GenerateEnumPrefix() + TEXT("_MAX"));
		const INT MaxEnumItemIndex = Enum->FindEnumIndex(MaxEnumItem);
		ReturnToLocation(EnumTagLocations(MaxEnumItemIndex), FALSE, TRUE);
		ScriptErrorf(SCEL_Restricted, TEXT("Illegal enumeration tag specified.  Conflicts with auto-generated tag '%s'"), *MaxEnumItem.ToString());
		///return NULL;
	}

	return Enum;
}

/**
 * @param		Input		An input string, expected to be a script comment.
 * @return					The input string, reformatted in such a way as to be appropriate for use as a tooltip.
 */
static FString FormatCommmentForToolTip(const FString& Input)
{
	// Return an empty string if there are no alpha-numeric characters or a Unicode characters above 0xFF
	// (which would be the case for pure CJK comments) in the input string.
	UBOOL bFoundAlphaNumericChar = FALSE;
	for ( INT i = 0 ; i < Input.Len() ; ++i )
	{
		if ( appIsAlnum(Input[i]) || Input[i] > 0xFF )
		{
			bFoundAlphaNumericChar = TRUE;
			break;
		}
	}
	if ( !bFoundAlphaNumericChar )
	{
		return FString( TEXT("") );
	}

	// Check for known commenting styles.
	FString Result( Input );
	const UBOOL bJavaDocStyle = Input.InStr(TEXT("/**")) != -1;
	const UBOOL bCStyle = Input.InStr(TEXT("/*")) != -1;
	const UBOOL bCPPStyle = Input.StartsWith(TEXT("//"));

	if ( bJavaDocStyle || bCStyle)
	{
		// Remove beginning and end markers.
		Result = Result.Replace( TEXT("/**"), TEXT("") );
		Result = Result.Replace( TEXT("/*"), TEXT("") );
		Result = Result.Replace( TEXT("*/"), TEXT("") );
	}

	if ( bJavaDocStyle )
	{
		// Remove stars from left edge.
		Result = Result.Replace( TEXT("* "), TEXT("") );
		Result = Result.Replace( TEXT("*"), TEXT("") );
	}

	if ( bCPPStyle )
	{
		// Remove c++-style comment markers.  Also handle javadoc-style comments by replacing
		// all triple slashes with double-slashes
		Result = Result.Replace(TEXT("///"), TEXT("//")).Replace( TEXT("//"), TEXT("") );

		// Parser strips cpptext and replaces it with "// (cpptext)" -- prevent
		// this from being treated as a comment on variables declared below the
		// cpptext section
		Result = Result.Replace( TEXT("(cpptext)"), TEXT("") );
	}

	// Get rid of carriage return or tab characters, which mess up wxWidgets.
	Result = Result.Replace( TEXT( "\r" ), TEXT( "" ) );
	//wx widgets has a hard coded tab size of 8
	const INT SpacesPerTab = 8;
	Result = Result.ConvertTabsToSpaces (SpacesPerTab);

	// get rid of any leading line-breaks and spaces
	INT Pos;
	for ( Pos=0; Pos < Result.Len(); Pos++ )
	{
		if ( Result[Pos] != TEXT('\n') && Result[Pos] != TEXT(' ') )
		{
			break;
		}
	}
	if ( Pos > 0 )
	{
		Result = Result.Mid(Pos);
	}

	// get rid of any trailing line-breaks and spaces
	for ( Pos=Result.Len() - 1; Pos >= 0; Pos-- )
	{
		if ( Result[Pos] != TEXT('\n') && Result[Pos] != TEXT(' ') )
		{
			break;
		}
	}
	if ( Pos < Result.Len() - 1 )
	{
		Result = Result.Left(Pos+1);
	}

	// Done.
	return Result;
}

/**
 * Consumes the rest of the line  If it's an end-of-line comment, and the specified property is editable,
 * the comment will be used as tooltip metadata for the poperties.
 *
 * @param		OriginalProperty		Used to determine whether or not the property is ediatble.
 * @param		NewProperties			The newly created properties that may receive the tooltip metadat.
 * @param		Input					Script compiler input.
 * @param		InputPos				Script compiler read head.
 * @param		InputLine				Script compiler input line.
 */
/**
 * Reads a comment block from the input stream and if found, applies the comment as the tooltip for any of the
 * specified properties which do not already have a tooltip.
 *
 * @param	VarProperty			the token corresponding to the variable declaration
 * @param	Properties			the list of properties which were declared by this variable declaration
 */
void FScriptCompiler::ConvertEOLCommentToTooltip( FPropertyBase& VarProperty, const TArray<UProperty*>& Properties )
{
	UBOOL bContainsEOLComment = FALSE;

	// our current input position
	const TCHAR* CurrentInputPosition = &Input[InputPos];

	// we only care about comments that start before the end of the line, so find the position of the EOL character
	const TCHAR* EOLPos = appStrchr( CurrentInputPosition, TEXT('\n') );
	if ( EOLPos == NULL )
	{
		// otherwise, find the position of the end of the buffer
		EOLPos = appStrchr(CurrentInputPosition, TEXT('\0'));
	}

	if ( EOLPos != NULL )
	{
		const TCHAR* SingleLineCommentPos = appStrstr(CurrentInputPosition, TEXT("//"));
		const TCHAR* MultiLineCommentPos = appStrstr(CurrentInputPosition, TEXT("/*"));

		// check to see if the comment begins before the end of the line
		bContainsEOLComment =	(SingleLineCommentPos != NULL && SingleLineCommentPos < EOLPos)
							||	(MultiLineCommentPos != NULL && MultiLineCommentPos < EOLPos);
	}

	if ( bContainsEOLComment )
	{
		// clear the current value for comment
		ClearComment();

		// GetChar() will put all of the comment characters into PrevComment for us
		const TCHAR NextValidChar = GetLeadingChar();

		// if bContainsEOLComment was TRUE, we should have gotten at least one character for our tooltip
		check(PrevComment.Len()>0);

		// GetChar() keeps going until the next valid character, which we don't care about....so we back up
		// the input position by one character
		InputPos--;

		// we only need tooltips for properties that will be visible in the editor, and we only use an end-of-line
		// comment as a tooltip if the property doesn't already have a tooltip associated with it.
		const FName ToolTipName( TEXT("ToolTip") );
		if ( !bReparsingClass && (VarProperty.PropertyFlags&CPF_Edit) != 0 && !VarProperty.MetaData.HasKey(ToolTipName) )
		{
			// Format the comment for use as a tooltip and copy it
			// to the property's tooltip metadata.
			FString EOLComment = FormatCommmentForToolTip( PrevComment );

			// If there's anything left after the formatting . . .
			if ( EOLComment.Len() > 0 )
			{
				// . . . use the comment as tooltip metadata.
				VarProperty.MetaData.Set( ToolTipName, *EOLComment );

				// Propagate to UProperties.
				// @todo: Comments of the following form will favour the prefix comment to the EOL comment:
				// @todo: // Prefix Comment
				// @todo: var() int blah;   // EOL Comment
				for ( INT PropIndex = 0 ; PropIndex < Properties.Num() ; ++PropIndex )
				{
					UProperty* Prop = Properties(PropIndex);
					UMetaData* MetaData = Prop->GetOutermost()->GetMetaData();
					MetaData->SetObjectValues(Prop, VarProperty.MetaData);
				}
			}
		}
	}
}

/**
 * Compile a struct definition.
 */
UScriptStruct* FScriptCompiler::CompileStruct( UStruct* Scope, UBOOL& NeedSemicolon )
{
	check(Scope);

	// Make sure structs can be declared here.
	if( TopNest->NestType != NEST_Class && TopNest->NestType != NEST_Interface )
	{
		ScriptErrorf(SCEL_Unknown/*SCEL_Parse*/,  TEXT("Structs can only be declared in class or struct scope") );
	}

	// Get struct name.
	FToken StructToken;

	FScriptLocation StructDeclaration;

	// Get optional export-text info
	if ( ParsePropertyExportText(StructToken, TEXT("struct")) && !bReparsingClass )
	{
		// if the next symbol is a semi-colon, this means that there were no other words between 
		// the struct keyword and the body of the struct, so ParsePropertyExportText thought that
		// the struct body was the export text - all unrealscript structs must have names, however, so
		// we know we're missing at least that.  If the scripter forgot to include the name
		// AND the closing semi-colon, they'll receive a nesting-level related compiler error,
		// since the rest of the class will be parsed as the struct body
		if ( PeekSymbol(TEXT(";")) )
		{
			// go back to the line where the struct keyword was located
			ReturnToLocation(StructDeclaration, FALSE, TRUE);
			ScriptErrorf(SCEL_Unknown/*SCEL_Parse*/,  TEXT("Missing struct name") );
		}

		if ( !Class->HasAnyClassFlags(CLASS_Native) )
		{
			ReturnToLocation(StructDeclaration, FALSE, TRUE);
			ScriptErrorf(SCEL_Restricted, TEXT("Export text should only be used in native classes"));
		}
	}

	UBOOL IsNative = false, IsExport = false, IsTransient = false;
	DWORD StructFlags = 0;
	for(;;)
	{
		if( !GetIdentifier(StructToken) )
		{
			ScriptErrorf(SCEL_Unknown/*SCEL_Parse*/,  TEXT("Missing struct name") );
		}

	    if( StructToken.TokenName == NAME_Native )
		{
		    StructFlags |= STRUCT_Native;
		}
	    else if( StructToken.TokenName == NAME_Export )
		{
			StructFlags |= STRUCT_Export;
		}
		else if ( StructToken.TokenName == NAME_Transient )
		{
			StructFlags |= STRUCT_Transient;
		}
		else if ( StructToken.TokenName == NAME_Atomic )
		{
			StructFlags |= STRUCT_Atomic;
		}
		else if ( StructToken.TokenName == NAME_Immutable )
		{
			StructFlags |= STRUCT_Immutable | STRUCT_Atomic;
		}
		else if ( StructToken.TokenName == NAME_ImmutableWhenCooked )
		{
			StructFlags |= STRUCT_ImmutableWhenCooked | STRUCT_AtomicWhenCooked;
		}
		else if ( StructToken.TokenName == NAME_StrictConfig )
		{
			StructFlags |= STRUCT_StrictConfig;
		}
	    else
		{
	        break;
		}
	}

	// Verify uniqueness.
	UField* Existing = FindField( Scope, StructToken.Identifier );
	if (Existing != NULL && Existing->GetOuter() == Scope && !bReparsingClass)
	{
		ScriptErrorf(SCEL_Unknown,  TEXT("struct: '%s' already defined here"), *StructToken.TokenName.ToString());
	}
	else if (FindObject<UClass>(ANY_PACKAGE, StructToken.Identifier) != NULL)
	{
		ScriptErrorf(SCEL_Unknown,  TEXT("struct: '%s' conflicts with class name"), *StructToken.TokenName.ToString());
	}
	else
	{
		CheckObscures(Scope, StructToken);
	}

	// Get optional superstruct.
	UScriptStruct* BaseStruct = NULL;
	if( MatchIdentifier(NAME_Extends) )
	{
		FToken ParentScope, ParentName;
		if( GetIdentifier( ParentScope ) )
		{
			if( !MatchSymbol(TEXT(".")) )
			{
				UField* Field = FindField( Scope, ParentScope.Identifier, TRUE, UScriptStruct::StaticClass(), TEXT("'extends'") );
				if( Field == NULL || !Field->IsA( UScriptStruct::StaticClass() ) )
				{
					ScriptErrorf(SCEL_Parse,  TEXT("'struct': Can't find parent struct '%s'"), ParentScope.Identifier );
				}
				BaseStruct = (UScriptStruct*)Field;
			}
			else
			{
				if( GetIdentifier( ParentName ) )
				{
					UClass* TempClass = FindClass(ParentScope.Identifier);//FindObject<UClass>( ANY_PACKAGE, ParentScope.Identifier );
					if( !TempClass )
					{
						ScriptErrorf(SCEL_Fatal,  TEXT("'struct': Can't find parent struct class '%s'"), ParentScope.Identifier );
					}

					UField* Field = FindField( TempClass, ParentName.Identifier, TRUE, UScriptStruct::StaticClass(), TEXT("'extends'") );
					if( Field == NULL || !Field->IsA( UScriptStruct::StaticClass() ) )
					{
						ScriptErrorf(SCEL_Unknown,  TEXT("'struct': Can't find parent struct class '%s.%s'"), ParentScope.Identifier, ParentName.Identifier );
						Field = NULL;
					}
					BaseStruct = (UScriptStruct*)Field;
				}
				else
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Formatting*/,  TEXT("'struct': Missing parent struct type after '%s.'"), ParentScope.Identifier );
				}
			}
		}
		else
		{
			ScriptErrorf(SCEL_Unknown/*SCEL_Formatting*/,  TEXT("'struct': Missing parent struct after 'Extends'") );
		}
	}

	// if we have a base struct, propagate inherited struct flags now
	if ( BaseStruct != NULL )
	{
		StructFlags |= (BaseStruct->StructFlags&STRUCT_Inherit);
	}

	UScriptStruct* Struct = NULL;
	if ( bReparsingClass )
	{
		Struct = (UScriptStruct*)Existing;

		// StructFlags aren't serialized, so we need to reset them for structs which are already compiled
		check(Struct);
		Struct->StructFlags |= StructFlags;
	}
	else
	{
		// Create.
		Struct = new( Scope, StructToken.Identifier, RF_Public )UScriptStruct(BaseStruct);
		Struct->Next = Scope->Children;
		Scope->Children = Struct;
		Struct->StructFlags |= StructFlags;
	}

	// Get opening brace.
	RequireSymbol( TEXT("{"), TEXT("'struct'") );

	StructToken.Struct = Struct;

	// add this struct to the compiler's persistent tracking system
	ClassData->AddStruct(StructToken);

	// Parse all struct variables.
	INT NumElements=0;
	FToken Token;
	do
	{
		GetToken( Token );
		if( Token.Matches(NAME_Struct) )
		{
			UBOOL InnerNeedSemiColon = true;
			CompileStruct( Struct, InnerNeedSemiColon );
			if ( InnerNeedSemiColon )
			{
				RequireSymbol( TEXT(";"), TEXT("'struct'") );
			}
		}
		else if ( Token.Matches(NAME_Const) )
		{
			UBOOL InnerNeedSemiColon = true;
			CompileConst( Struct, InnerNeedSemiColon );
			if ( InnerNeedSemiColon )
			{
				RequireSymbol( TEXT(";"), TEXT("'const'") );
			}
		}
		else if ( Token.Matches( NAME_Var ) )
		{
			// Get editability.
			FName EdCategory = NAME_None;
			QWORD EdFlags = 0;
			if( MatchSymbol(TEXT("(")) )
			{
				EdFlags |= CPF_Edit;

				FString CategoryPath;
				do
				{
					FToken CategoryToken;
					if( GetIdentifier( CategoryToken, 1 ) )
					{
						if( CategoryPath.Len() > 0 )
						{
							// Character used to deliminate sub-categories in category path names
							// NOTE: Must match FPropertyNodeConstants::CategoryDelimiterChar
							const TCHAR CategoryDelimiterChar( ',' );
							CategoryPath.AppendChar( CategoryDelimiterChar );
						}
						CategoryPath += CategoryToken.Identifier;
					}
					else
					{
						EdCategory = FName( StructToken.Identifier, FNAME_Find, TRUE );
						break;
					}
				}
				while( MatchSymbol(TEXT(",")) );
				RequireSymbol( TEXT(")"), TEXT("Editable 'struct' member variable") );

				if( EdCategory == NAME_None )
				{
					EdCategory = FName( *CategoryPath );
				}
			}

			// Get variable type.
			EObjectFlags ObjectFlags=0;
			FPropertyBase OriginalProperty(CPT_None);
			GetVarType( Struct, OriginalProperty, ObjectFlags, CPF_ParmFlags, TEXT("'struct' member variable") );
			OriginalProperty.PropertyFlags |= EdFlags;

			// Validate.
			if( OriginalProperty.PropertyFlags & CPF_ParmFlags )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Illegal type modifiers in variable") );
			}

			// Process all variables of this type.
			TArray<UProperty*> NewProperties;
			do
			{
				FPropertyBase	Property = OriginalProperty;
				UProperty*		NewProperty = GetVarNameAndDim( Struct, Property, ObjectFlags, FALSE, FALSE, NULL, TEXT("Variable declaration"), EdCategory, FALSE );
				NewProperties.AddItem( NewProperty );
				// we'll need any metadata tags we parsed later on when we call ConvertEOLCommentToTooltip() so the tags aren't clobbered
				OriginalProperty.MetaData = Property.MetaData;

				if(NewProperty->PropertyFlags & CPF_Component)
				{
					Struct->StructFlags |= STRUCT_HasComponents;
				}

				// if the struct is marked transient, mark all properties in the struct as CPF_AlwaysInit
				if ( (Struct->StructFlags&STRUCT_Transient) != 0 )
				{
					NewProperty->PropertyFlags|=CPF_AlwaysInit;
				}
			} while( MatchSymbol(TEXT(",")) );

			// Expect a semicolon.
			RequireSymbol( TEXT(";"), TEXT("'struct'") );
			NumElements++;

			// Process any remaining input on this line, potentially treating it as tooltip metadata.
			ConvertEOLCommentToTooltip( OriginalProperty, NewProperties );
		}
		else if ( Token.Matches( TEXT("structdefaultproperties") ) )
		{
			if ( !bReparsingClass )
			{
				// Get default properties text.
				FString				StrLine;
				const TCHAR*		Str;
				const TCHAR*		Buffer = &Input[Token.StartPos];
				FStringOutputDevice DefaultStructPropText;

				INT CurrentLine = InputLine;
				while( ParseLine(&Buffer,StrLine,1) )
				{
					if ( StrLine.InStr(TEXT("{")) != INDEX_NONE )
					{
						DefaultStructPropText.Logf(TEXT("linenumber=%i\r\n"), CurrentLine);
						break;
					}
				}
				while( ParseLine(&Buffer,StrLine,1) )
				{
					Str = *StrLine;
					ParseNext( &Str );
					if( *Str=='}' )
						break;
					DefaultStructPropText.Logf( TEXT("%s\r\n"), *StrLine );
				}
				if (!DefaultStructPropText.IsEmpty())
				{
					FCommentStrippingFilter Filter(GetContext());
					Filter.Process(*DefaultStructPropText, *DefaultStructPropText + DefaultStructPropText.Len(), Struct->DefaultStructPropText);
				}
				else
				{
					Struct->DefaultStructPropText = DefaultStructPropText;
				}
			}

			// skip past the structdefault block
			RequireSymbol(TEXT("{"), TEXT("structdefaultproperties"));

			TCHAR ch = PeekChar();
			INT BraceCount=1;

			while ( ch != 0 && BraceCount > 0 )
			{
				// peek at the next one so we know when to break out of the look
				ch = GetChar(TRUE);

				if ( ch == TEXT('{') )
				{
					BraceCount++;
				}
				else if ( ch == TEXT('}') )
				{
					BraceCount--;
				}
			}
		}
		else if ( Token.Matches( TEXT("structcpptext") ) )
		{
			if ( !Class->HasAnyClassFlags(CLASS_Native) )
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Cannot use cpptext blocks in non-native classes"));
			}

			RequireSymbol( TEXT("{"), TEXT("'structcpptext'") );
			FString CppText;
			TCHAR ch;
			INT count = 1;
			ch = PeekChar();
			while ( ch != 0 && count > 0 )
			{
				CppText += GetChar(1);

				ch = PeekChar();
				if ( ch == TEXT('{') )
				{
					count++;
				}
				else if ( ch == TEXT('}') )
				{
					count--;
				}
			}

			RequireSymbol( TEXT("}"), TEXT("'structcpptext'") );
			if (CppText.Len() && !bReparsingClass)
			{
				if (Struct->CppText != NULL)
				{
					ScriptErrorf(SCEL_Restricted, TEXT("Multiple structcpptext definitions for '%s'"), *Struct->GetName());
				}
				else
				{
					Struct->CppText = new(Struct, TEXT("CppText"), RF_NotForClient | RF_NotForServer) UTextBuffer(*CppText);
				}
			}
		}
		else if ( !Token.Matches( TEXT("}") ) )
		{
			ScriptErrorf(SCEL_Unknown/*SCEL_NestLevel*/,  TEXT("'struct': Unexpected '%s'"), Token.Identifier );
			///break;
		}
	} while( !Token.Matches( TEXT("}") ) );

	if ( !bReparsingClass )
	{
		FArchive DummyAr;
		Struct->Link( DummyAr, 1 );

		// check for min alignment issues
		if ( Struct->GetPropertiesSize() < Struct->GetMinAlignment() && (Struct->StructFlags&STRUCT_Native) != 0)
		{
			ScriptWarnf(SCWL_Level4, TEXT("%s must contain at least %d bytes (currently only contains %d)"), *Struct->GetName(), Struct->GetMinAlignment(), Struct->GetPropertiesSize());
		}
	}
	return Struct;
}

//
// Compile a constant definition.
//
void FScriptCompiler::CompileConst( UStruct* Scope, UBOOL& NeedSemicolon )
{
	check(Scope);

	// Get variable name.
	FToken ConstToken;
	if( !GetIdentifier(ConstToken) )
	{
		ScriptErrorf(SCEL_Unknown/*SCEL_Parse*/,  TEXT("Missing constant name") );
	}

	// Verify uniqueness.
	FName ConstName = FName( ConstToken.Identifier );
	UField* Existing = FindField( Scope, ConstToken.Identifier );
	if( Existing && Existing->GetOuter()==Scope )
	{
		if ( bReparsingClass )
		{
			SkipStatements(0, TEXT("const"));
			NeedSemicolon = false;
			return;
		}

		ScriptErrorf(SCEL_Unknown,  TEXT("const: '%s' already defined"), ConstToken.Identifier );
	}

	CheckObscures( Scope, ConstToken );

	// Get equals and value.
	RequireSymbol( TEXT("="), TEXT("'const'") );
	const TCHAR* Start = Input+InputPos;
	FToken ValueToken;
	if( !GetToken(ValueToken) )
	{
		ScriptErrorf(SCEL_Unknown/*SCEL_Parse*/,  TEXT("const %s: Missing value"), *ConstName.ToString() );
	}
	// Format constant.
	TCHAR Value[MAX_SPRINTF]=TEXT("");
	if( ValueToken.TokenType != TOKEN_Const )
	{
		if (ValueToken.Matches(TEXT("sizeof")))
		{
			FToken ClassToken;
			UClass* Class;

			RequireSymbol( TEXT("("), TEXT("'sizeof'") );
			if( !GetIdentifier(ClassToken) )
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing class name") );
			}
			Class = FindClass(ClassToken.Identifier);//FindObject<UClass>( ANY_PACKAGE, ClassToken.Identifier );
			if( !Class )
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Specified class name not found '%s'"), ClassToken.Identifier );
			}
			RequireSymbol( TEXT(")"), TEXT("'sizeof'") );

			// Format constant
			appSprintf( Value, TEXT("%d"), Class->GetPropertiesSize() );
			ScriptWarnf(SCWL_Level1, TEXT("Assigning %s to %s"), Value, ConstToken.Identifier);
		}
		else
		{
			ScriptErrorf(SCEL_Parse,  TEXT("const %s: Value is not constant"), *ConstName.ToString() );
		}
	}
	else
	{
		// Format constant.
		// skip any initial whitespace
		while (*Start == ' ' || *Start == 9)
		{
			Start++;
		}
		appStrncpy( Value, Start, Min(1024,(INT)( Input+InputPos-Start+1 ) ) );
	}

	// Create constant.
	UConst* NewConst = new(Scope, ConstName, RF_Public) UConst(Value);
	NewConst->Next = Scope->Children;
	Scope->Children = NewConst;
}

/*-----------------------------------------------------------------------------
	Retry management.
-----------------------------------------------------------------------------*/
FScriptCompiler* FScriptLocation::Compiler = NULL;

FScriptLocation::FScriptLocation()
{
	if ( Compiler != NULL )
		Compiler->InitScriptLocation(*this);
}

/**
 * Remember the current compilation points, both in the source being
 * compiled and the object code being emitted.  Required because
 * UnrealScript grammar isn't quite LALR-1.
 *
 * @param	Retry	[out] filled in with current compiler position information
 */
void FScriptCompiler::InitScriptLocation( FScriptLocation& Retry )
{
	Retry.Input     = Input;
	Retry.InputPos	= InputPos;
	Retry.InputLine	= InputLine;
	Retry.CodeTop	= TopNode->Script.Num();
}

/**
 * Return to a previously-saved retry point.
 *
 * @param	Retry	the point to return to
 * @param	Binary	whether to modify the compiled bytecode
 * @param	Text	whether to modify the compiler's current location in the text
 */
void FScriptCompiler::ReturnToLocation( const FScriptLocation& Retry, UBOOL Binary, UBOOL Text )
{
	if( Text )
	{
		Input     = Retry.Input;
		InputPos  = Retry.InputPos;
		InputLine = Retry.InputLine;
	}
	if( Binary )
	{
		check(Retry.CodeTop <= TopNode->Script.Num());
		TopNode->Script.Remove( Retry.CodeTop, TopNode->Script.Num() - Retry.CodeTop );
		check(TopNode->Script.Num()==Retry.CodeTop);
	}
}

/**
 * Remove the remainder of the bytecode starting at location Source, and insert it into location Dest
 *
 * @param	Dest	the script location to insert the bytecode
 * @param	Source	the position in the node's Script array to remove from
 */
void FScriptCompiler::MoveCompiledCode( FScriptLocation& Dest, FScriptLocation& Source )
{
	if ( Dest.CodeTop == Source.CodeTop )
	{
		return;
	}

	FMemMark Mark(GMainThreadMemStack);
	INT SourceSize = TopNode->Script.Num() - Source.CodeTop;
	INT DestSize  = Source.CodeTop   - Dest.CodeTop;

	BYTE* Temp = new(GMainThreadMemStack,SourceSize)BYTE;

	// update any pending fixup requests with the new location of the code
	TopNest->UpdatePlaceholderLocations(Dest.CodeTop, DestSize, SourceSize);

	// update struct modification byte locations
	for (TList<INT>* StructByteFixup = TopNest->StructModificationByteList; StructByteFixup != NULL; StructByteFixup = StructByteFixup->Next)
	{
		if (Dest.CodeTop < StructByteFixup->Element && Dest.CodeTop + DestSize >= StructByteFixup->Element)
		{
			StructByteFixup->Element += SourceSize;
		}
		else if (Source.CodeTop < StructByteFixup->Element)
		{
			StructByteFixup->Element -= DestSize;
		}
	}

	// copy the source code into a temporary place
	appMemcpy ( Temp, &TopNode->Script(Source.CodeTop), SourceSize);

	// move the code currently at destination to the point where the source code will end
	appMemmove( &TopNode->Script(Dest.CodeTop + SourceSize), &TopNode->Script(Dest.CodeTop), DestSize );

	// copy the source code into the destination location
	appMemcpy ( &TopNode->Script(Dest.CodeTop), Temp, SourceSize);

	Mark.Pop();
}

/*-----------------------------------------------------------------------------
	Functions.
-----------------------------------------------------------------------------*/

/**
 * Try to compile a complete function call or field expression with a field name matching Token.
 * Handles the error condition where the function was called but the specified parameters
 * didn't match, or there was an error in a parameter expression.
 * <p>
 * The function to call must be accessible within the current scope.
 * <p>
 * This also handles unary operators identically to functions, but not binary operators.
 *
 * @param	Scope			current node
 * @param	RequiredType	if this expression must be a particular type (e.g. compling the right-hand of an assignment)
 *							this will contain information about the type to match
 * @param	Token			@todo
 * @param	ResultType		[out] filled in with info about the compiled expression
 * @param	IsSelf			@todo
 * @param	IsConcrete		@todo
 *
 * @return	1 if a function call was successfully parsed, or 0 if no matching expression was found.
 */
UBOOL FScriptCompiler::CompileFieldExpr
(
	UStruct*			Scope,
	FPropertyBase&		RequiredType,
	FToken				Token,
	FToken&				ResultType,
	UBOOL				IsSelf,
	UBOOL				IsConcrete,
	FScriptLocation*	StartOfExpression
)
{
	FScriptLocation StartOfFieldExpression;
	UBOOL    ForceFinal = 0;
	UBOOL    Global     = 0;
	UBOOL    Super      = 0;
	UBOOL    Default    = 0;
	UBOOL    Static     = 0;
	UClass*  FieldClass = NULL;
	UBOOL InSingularFunction = Cast<UFunction>(Scope) && Cast<UFunction>(Scope)->HasAnyFunctionFlags(FUNC_Singular);

	// Handle specifiers.
	if( Token.Matches(NAME_Default) )
	{
		// Default value of variable.
		Default    = 1;
		FieldClass = UProperty::StaticClass();
		RequireSymbol( TEXT("."), TEXT("'default'") );
		GetToken(Token);
	}
	else if( Token.Matches(NAME_Static) )
	{
		// Static field.
		Static     = 1;
		FieldClass = UFunction::StaticClass();
		RequireSymbol( TEXT("."), TEXT("'static'") );
		GetToken(Token);
	}
	else if ( Token.Matches(NAME_Const) )
	{
		// const field
		FieldClass = UConst::StaticClass();
		RequireSymbol( TEXT("."), TEXT("'const'") );
		// read the named const token
		GetToken(Token);
	}
	else if( Token.Matches(NAME_Global) )
	{
		// Call the highest global, final (non-state) version of a function.
		if( !IsSelf )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Can only use 'global' with self") );
		}
		if( !IsConcrete )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Can only use 'global' with concrete objects") );
		}
		Scope      = Scope->GetOwnerClass();
		FieldClass = UFunction::StaticClass();
		Global     = 1;
		IsSelf     = 0;
		RequireSymbol( TEXT("."), TEXT("'global'") );
		GetToken(Token);
	}
	else if( Token.Matches(NAME_Super) )
	{
		if ( !PeekSymbol(TEXT("(")) )
		{
			// Call the superclass version of the function.
			if( !IsSelf )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Can only use 'super' with self") );
			}

			// find the containing state/class for this function
			while( Cast<UFunction>(Scope, CLASS_IsAUFunction) != NULL )
			{
				Scope = CastChecked<UStruct>( Scope->GetOuter() );
			}
			if( !Scope )
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Can't use 'super': no superclass") );
			}
			// and get the parent of the state/class
			if (Scope->GetSuperStruct() == NULL)
			{
				// use the state outer
				Scope = CastChecked<UStruct>(Scope->GetOuter());
			}
			else
			{
				// use the state/class parent
				Scope = Scope->GetSuperStruct();
			}

			FieldClass = UFunction::StaticClass();
			ForceFinal = 1;
			IsSelf     = 0;
			Super      = 1;
			RequireSymbol( TEXT("."), TEXT("'super'") );
			GetToken(Token);
		}
		else
		{
			// Call the final version of a function residing at or below a certain class.
			if( !IsSelf )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Can only use 'super(name)' with self") );
			}
			RequireSymbol( TEXT("("), TEXT("'super'") );
			// get the desired super class/state name
			FToken SuperToken;
			if (!GetIdentifier(SuperToken))
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Missing class or state name"));
			}
			// find the current state/class

			UStruct* SuperScope = GetSuperScope(Scope, SuperToken.TokenName);
			if (SuperScope)
			{
				// success
				Scope = SuperScope;
			}
			else
			{
				// failed to find a valid super
				// check if a struct with the specified name exists to print a more specific error message
				if (FindObject<UStruct>(ANY_PACKAGE, SuperToken.Identifier))
				{
					ScriptErrorf(SCEL_Restricted, TEXT("'Super(name)': '%s' does not extend '%s'"), *Scope->GetOuter()->GetName(), SuperToken.Identifier);
				}
				else
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Bad class or state name '%s'"), SuperToken.Identifier);
				}
			}
			RequireSymbol( TEXT(")"), TEXT("'super(name)'") );
			RequireSymbol( TEXT("."), TEXT("'super(name)'") );
			FieldClass = UFunction::StaticClass();
			ForceFinal = 1;
			IsSelf     = 0;
			Super      = 1;
			GetToken(Token);
		}
	}

	INT OuterContextCount = 0;

	// Handle field type.
	UField* Field = FindField(Scope, Token.Identifier, TRUE, FieldClass, NULL, IsSelf ? &OuterContextCount : NULL);
	if (Field == NULL && FindField(Scope, Token.Identifier, TRUE, FieldClass, NULL, &OuterContextCount))
	{
		ScriptErrorf(SCEL_Restricted, TEXT("Accessing a member of %s's within class through a context expression requires explicit 'Outer'"), *Scope->GetName());
	}
	UFunction* FieldFunction = Cast<UFunction>(Field);

	if (InSingularFunction && (IsSelf || Super)
	&&	FieldFunction != NULL && FieldFunction->HasAnyFunctionFlags(FUNC_Singular))
	{
		// due to the way 'singular' uses an object flag to prevent recursion, calling a singular function within a singular function on the same object
		// will always cause the second one to be ignored
		ScriptErrorf(SCEL_Restricted, TEXT("Calling a singular function within a singular function on the same object will always fail"));
	}

	UDelegateProperty *delegateProp = NULL;
	if (Field && Cast<UDelegateProperty>(Field) && PeekSymbol(TEXT("(")))
	{
		// delegate call through a normal delegate property
		// save this delegate property for later reference
		delegateProp = ((UDelegateProperty*)Field);
		// and replace the field with the actual function for compilation down below
		Field = FieldFunction = delegateProp->Function;
	}

	// Convert a Delegate function to its corresponding UDelegateProperty if this isn't a function call
	// unless we're compiling the right side of an assignment
	// (which allows us to assign delegate function bodies to other delegates or delegate properties - useful since delegate properties can't invoke the default implementation when set to None)
	if ( FieldFunction != NULL && FieldFunction->HasAnyFunctionFlags(FUNC_Delegate)
	&&	(RequiredType.Type != CPT_Delegate || RequiredType.ReferenceType != CPRT_AssignmentReference) && !PeekSymbol(TEXT("(")) )
	{
		Field = FindField( CastChecked<UStruct>(Field->GetOuter()), *FString::Printf(TEXT("__%s__Delegate"), *Field->GetName()), TRUE, UField::StaticClass(), NULL );
		FieldFunction = NULL;
	}


	// if IsSelf = TRUE, we are compiling the first token of a field expression (i.e. from SomeObj.SomeOtherObj.SomeField, Field currently refers to SomeObj)
	INT SkipoverSizePos=0;
	if ( IsSelf )
	{
		// if the property is actually located in an Outer class, emit the context bytecode
		while ( OuterContextCount > 0 )
		{
			// If we don't have a context, 
			SkipoverSizePos = EmitOuterContext(*StartOfExpression, FALSE);
			OuterContextCount--;
		}
	}

	if( Field && Field->GetClass()==UEnum::StaticClass() && MatchSymbol(TEXT(".")) )
	{
		// Enum constant.
		UEnum* Enum = (UEnum*)Field;
		INT EnumIndex = INDEX_NONE;
		if( !GetToken(Token) )
		{
			ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing enum tag after '%s'"), *Enum->GetName() );
			///return TRUE;
		}
		else if( Token.TokenName==NAME_EnumCount )
		{
			// subtract 1 because the last entry in the enum's Names array
			// is the _MAX entry
			EnumIndex = Enum->NumEnums() - 1;
		}
		else
		{
			EnumIndex = Enum->FindEnumIndex( Token.TokenName );
			if( Token.TokenName == NAME_None || EnumIndex == INDEX_NONE )
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing enum tag after '%s'"), *Enum->GetName() );
				///return TRUE;
			}
		}
		ResultType.SetConstByte( Enum, EnumIndex );
		ResultType.PropertyFlags &= ~CPF_OutParm;
		EmitConstant( ResultType );

		if ( SkipoverSizePos > 0 )
		{
			EmitFieldPlaceholderFixup(TopNode->Script, SkipoverSizePos, &Field);
		}

		return 1;
	}
	else if (Field && Cast<UProperty>(Field, CLASS_IsAUProperty))
	{
		// Check validity.
		UProperty* Property = (UProperty*)Field;
		UClass* OwnerClass = Property->GetOwnerClass();

		if ( SkipoverSizePos > 0 )
		{
			EmitFieldPlaceholderFixup(TopNode->Script, SkipoverSizePos, &Field);
			SkipoverSizePos = 0;
		}

		const UBOOL bIsMemberProperty = Property->GetOuter()->GetClass() == UClass::StaticClass();
		const UBOOL bIsLocalProperty = !bIsMemberProperty && Property->GetOuter()->GetClass() == UFunction::StaticClass();
		const UBOOL bIsStateProperty = !bIsMemberProperty && Property->GetOuter()->GetClass() == UState::StaticClass();

		// if the property is private, check that this is the owner class of the property
		if( !Property->HasAnyFlags(RF_Public) && OwnerClass!=Class )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Can't access private variable '%s' in '%s'"), *Property->GetName(), *OwnerClass->GetName() );
		}

		// cannot access properties in native-only class
		if( Property->GetOwnerClass() && Property->GetOwnerClass()->HasAnyClassFlags(CLASS_NativeOnly) )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Can't access variable '%s' in native-only class '%s'"), *Property->GetName(), *OwnerClass->GetName() );
		}

		// if the property is protected, only outside access allowed is in child classes or within classes
		if ( Property->HasAnyFlags(RF_Protected) )
		{
			UClass* OuterClass;
			for ( OuterClass = Class; OuterClass && OuterClass != UObject::StaticClass(); OuterClass = OuterClass->ClassWithin )
			{
				if ( OuterClass->IsChildOf(OwnerClass) )
				{
					break;
				}
			}

			if ( OuterClass == NULL || OuterClass == UObject::StaticClass() )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Can't access protected variable '%s' in '%s'"), *Property->GetName(), *OwnerClass->GetName() );
			}
		}

		// only allowed to access default values of properties that are class members (i.e. not struct or local properties) 
		if( Default && !bIsMemberProperty )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("You can't access the default value of struct and local variables") );
		}

		// if we're in a static function and this isn't a local property, only allowed to access default value
		if( !IsConcrete && !Default && !bIsLocalProperty )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("You can only access default values of variables here") );
		}
 
		// if the property is deprecated, issue a warning
		if( (Property->PropertyFlags & CPF_Deprecated) )
		{
			ScriptWarnf(SCWL_Level4, TEXT("Reference to deprecated property '%s'"), *Property->GetName());
		}

		if( bIsMemberProperty && OwnerClass->HasAnyClassFlags(CLASS_Interface) )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Cannot access interface variables") );
		}

		// Process the variable we found.
		if ( Cast<UBoolProperty>(Property) )
		{
			Writer << EX_BoolVariable;
		}
		EExprToken ExprToken;
		if (Default)
		{
			ExprToken = EX_DefaultVariable;
		}
		else if (bIsLocalProperty)
		{
			ExprToken = (Property->PropertyFlags & CPF_OutParm) ? EX_LocalOutVariable : EX_LocalVariable;
		}
		else if (bIsStateProperty)
		{
			ExprToken = EX_StateVariable;
		}
		else
		{
			ExprToken = EX_InstanceVariable;
		}
		Writer << ExprToken;
		Writer << Property;

		// Return the type.
		ResultType = FToken( Property );
		ResultType.PropertyFlags |= CPF_OutParm;

		if ( Property->GetOuter() == UObject::StaticClass() )
		{
			// If this property is the 'UObject::Class' property, replace the metaclass field in the
			// token with the appropriate class
			if( Property->GetFName()==NAME_Class )
			{
				ResultType.MetaClass = Scope->GetOwnerClass();
			}

			// If this property is the 'UObject::Outer' property, replace the PropertyClass field
			// in the token with the appropriate outer
			else if( Property->GetFName() == NAME_Outer )
			{
				ResultType.PropertyClass = Scope->GetOwnerClass()->ClassWithin;
			}
		}

		// if this field is a local property, mark it as referenced
		if ( Scope->GetClass() == UFunction::StaticClass() && bIsLocalProperty )
		{
			if ( LocalProperties )
			{
				FLocalProperty* CurLocal = LocalProperties->GetLocalProperty(Property);
				if ( CurLocal )
				{
					CurLocal->bReferenced = true;

					// if we're passing this field as the value for an out parameter, consider it assigned a value.
					UBOOL bOutParam = (RequiredType.PropertyFlags & CPF_OutParm) != 0;

					// determine whether we're assigning a value to this field.
					if ( RequiredType.ReferenceType == CPRT_AssignValue || RequiredType.ReferenceType == CPRT_DualReference || bOutParam )
					{
						if ( ResultType.IsObject()				// if the local property we're accessing is an object variable
							&& PeekSymbol(TEXT("."))			// and we're actually accessing an internal member of the object
							&& !ResultType.IsDynamicArray() )	// and we aren't actually calling a function on a dynamic array of objects (e.g. .Remove, .Length, etc.)
						{
							// this is actually a reference to the internal member of a local object variable
							RequiredType.ReferenceType = CPRT_AssignmentReference;
						}
						else
						{
							CurLocal->ValueAssigned(InputLine);
							if ( bOutParam )
							{
								CurLocal->ValueReferenced(InputLine);
							}
						}
					}

					if ( RequiredType.ReferenceType == CPRT_AssignmentReference || RequiredType.ReferenceType == CPRT_DualReference )
					{
						// if we haven't already assigned a value to this local field, consider it uninitialized,
						if ( !CurLocal->bValueAssigned
							// if we're actually making a function call on a dynamic array, don't flag this as uninitialized
							//@todo this should really be done in CompileExpr
						&& !(ResultType.IsDynamicArray() && PeekSymbol(TEXT("."))) )
						{
							// special-case hack for handling post-operators.  Since post-operators cause a value
							// to be assigned, but are processed after the token, we need to switch the reference type
							// here, in case this is the only place where the value of the field is referenced.
							//@todo find a better way to check for this
							if ( PeekSymbol(TEXT("++")) || PeekSymbol(TEXT("--")) )
							{
								// we'll also "cheat" a little and allow types which have post-operators (bytes and ints)
								// to be accessed by the post-operator without being initialized (i.e. the first time the
								// value of i is accessed is in the expression i++;), since it would be EXTREMELY unlikely
								// that this is a bug, since this particular way of accessing an uninitialized field is very common.
								CurLocal->ValueAssigned(InputLine);
							}
							else
							{
                                CurLocal->bUninitializedValue = true;
							}
						}

						CurLocal->ValueReferenced(InputLine);
					}
					else if ( RequiredType.ReferenceType == CPRT_SimpleReference )
					{
						// special-case hack for handling post-operators.  Since post-operators cause a value
						// to be assigned, but are processed after the token, we need to switch the reference type
						// here, in case this is the only place where the value of the field is referenced.
						//@todo find a better way to check for this
						if ( PeekSymbol(TEXT("++")) || PeekSymbol(TEXT("--")) )
						{
							// we'll also "cheat" a little and allow types which have post-operators (bytes and ints)
							// to be accessed by the post-operator without being initialized (i.e. the first time the
							// value of i is accessed is in the expression i++;), since it would be EXTREMELY unlikely
							// that this is a bug, since this particular way of accessing an uninitialized field is very common.
							CurLocal->ValueAssigned(InputLine);
						}

						CurLocal->ValueReferenced(InputLine);
					}
				}
			}
		}


		return 1;
	}
	else if ( RequiredType.Type == CPT_Delegate )
	{
		if ( FieldFunction != NULL )
		{
			//!!DELEGATES - check if the function is callable from here (e.g. protected)

			// RequiredType.Function == NULL indicates that either any function is valid, or that the function definition will be validated by the caller
			// used for some special expressions where the required function prototype depends on the context (e.g. dynamic array Sort)
			if (RequiredType.Function != NULL)
			{
				// check return type and params
				if
				(	RequiredType.Function->NumParms!=FieldFunction->NumParms
				||	(!RequiredType.Function->GetReturnProperty())!=(!FieldFunction->GetReturnProperty()) )
				{
					ScriptErrorf(SCEL_Restricted,  TEXT("'%s' mismatches delegate '%s'"), *FieldFunction->FriendlyName.ToString(), *RequiredType.Function->FriendlyName.ToString() );
				}

				// Check all individual parameters.
				INT Count=0;
				for( TFieldIterator<UProperty> DestinationParameter(RequiredType.Function), SourceParameter(FieldFunction); 
					Count < FieldFunction->NumParms;
					++DestinationParameter,++SourceParameter,++Count )
				{
					if( !FPropertyBase(*DestinationParameter).MatchesType(FPropertyBase(*SourceParameter), 1) )
					{
						ScriptErrorf(SCEL_Restricted,  TEXT("'%s' mismatches delegate '%s'"), *FieldFunction->FriendlyName.ToString(), *RequiredType.Function->FriendlyName.ToString() );
						break;
					}
				}
			}

			INT *EmitCnt = FuncEmitCountMap.Find(FieldFunction);
			if (EmitCnt == NULL)
			{
				FuncEmitCountMap.Set(FieldFunction,1);
			}
			else
			{
				*EmitCnt = *EmitCnt + 1;
			}

			Writer << EX_DelegateProperty;
			Writer << FieldFunction->FriendlyName;

			// if we're assigning a delegate to a delegate, write the source delegate's property as well, so that
			// we properly handle both the case of the delegate property having a value (copy the property value) and the delegate
			// being set to None (assign this delegate's default body to the destination delegate)
			UDelegateProperty* DelegateProp = NULL;
			if ( FieldFunction->HasAnyFunctionFlags(FUNC_Delegate) )
			{
				// we search in outer context, but we don't need to remember the offset, since the delegate property [which
				// was created by the script compiler] must be in the same class as the function
				DelegateProp = Cast<UDelegateProperty>(FindField(CastChecked<UStruct>(Field->GetOuter()), *FString::Printf(TEXT("__%s__Delegate"), *Field->GetName()), TRUE, UField::StaticClass(), NULL));
			}
			Writer << DelegateProp;

			if( SkipoverSizePos )
			{
				//@todo ronp - should we be emitting DelegateProp here?  perhaps we should emit the function instead?
				EmitFieldPlaceholderFixup(TopNode->Script, SkipoverSizePos, (UField**)&DelegateProp);
				SkipoverSizePos = 0;
			}

			ResultType = FToken(FPropertyBase(CPT_Delegate));
			ResultType.Function = FieldFunction;
			return 1;
		}
		else
		{
			if ( Field == NULL )
			{
				if ( (RequiredType.PropertyFlags&CPF_OptionalParm) == 0 )
				{
					ScriptErrorf(SCEL_Unknown,  TEXT("Delegate assignment failed (%s): Invalid or unknown function '%s'"), *RequiredType.Function->FriendlyName.ToString(), Token.Identifier );
				}
				
				return 0;
			}
			else
			{
				ScriptErrorf(SCEL_Unknown,  TEXT("Delegate assignment failed (%s): '%s' is not a function"), *RequiredType.Function->FriendlyName.ToString(), *Field->GetFullName() );
			}
			return 1;
		}
	}
	else if( FieldFunction != NULL )
	{
		if (Super && (FieldFunction->FunctionFlags & FUNC_Final))
		{
			ScriptErrorf(SCEL_Unknown, TEXT("Cannot call 'Super' of final function '%s'"), Token.Identifier);
		}
		// Function.
		if ( MatchSymbol(TEXT("(")) )
		{
			GotAffector = 1;
			// Verify that the function is callable here.
			// NOTE[aleiby]: Obviously, a better way would be to have an IsInScope( Scope, FunctionName ) helper function, or an AttemptFunctionCall( Scope, FunctionName ) that automatically spits out the appropriate error messages.
			if( FieldFunction->HasAnyFunctionFlags(FUNC_Private) && FieldFunction->GetOwnerClass()!=Class )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Can't access private function '%s' in '%s'"), *FieldFunction->GetName(), *FieldFunction->GetOwnerClass()->GetName() );
			}
			if(	FieldFunction->HasAnyFunctionFlags(FUNC_Protected) )
			{
				//Warn->Logf( TEXT("%s calling %s::%s in scope %s"), *Class->GetName(), *Function->GetOwnerClass()->GetName(), *Function->GetName(), *Scope->GetName() );
				if( !Class->IsChildOf( FieldFunction->GetOwnerClass() ) )													// if we are not a subclass of the function's class.
				{
					if( FieldFunction->GetOwnerClass()->IsChildOf( Class ) )												// if we are a superclass of the function's class.
					{
						INT OuterContextCnt = FALSE;
						
						// Fix ARL: Use correct scope to account for functions in states.
						UFunction* ScopedFunction = Cast<UFunction>(FindField(Class, *FieldFunction->GetName(), TRUE, UFunction::StaticClass(), TEXT("'function'"), &OuterContextCnt));	
						if (ScopedFunction == NULL)																			// if the function is not in the superclass's scope.
						{
							ScriptErrorf(SCEL_Restricted,  TEXT("Can't access protected function '%s' in '%s' (not in scope)"), *FieldFunction->GetName(), *FieldFunction->GetOwnerClass()->GetName());
						}
						else if( (ScopedFunction->FunctionFlags & FUNC_Private) && ScopedFunction->GetOwnerClass()!=Class )	// if a function with the same name is in scope but is private.
						{
							ScriptErrorf(SCEL_Restricted,  TEXT("Can't access private function '%s' in '%s' (%s::%s is not in scope)"),
								*ScopedFunction->GetName(), *ScopedFunction->GetOwnerClass()->GetName(), *FieldFunction->GetOwnerClass()->GetName(), *FieldFunction->GetName() );
						}
					}
					else
					{
						UClass* OwnerClass = FieldFunction->GetOwnerClass();
						UClass* OuterClass;
						for (OuterClass = Class; OuterClass != NULL && OuterClass != UObject::StaticClass(); OuterClass = OuterClass->ClassWithin)
						{
							if (OuterClass->IsChildOf(OwnerClass))
							{
								break;
							}
						}

						if (OuterClass == NULL || OuterClass == UObject::StaticClass())
						{
							ScriptErrorf(SCEL_Restricted,  TEXT("Can't access protected function '%s' in '%s'"), *FieldFunction->GetName(), *FieldFunction->GetOwnerClass()->GetName() );
						}
					}
				}
			}


			if( FieldFunction->HasAnyFunctionFlags(FUNC_Latent) )
			{
				CheckAllow( *FieldFunction->GetName(), ALLOW_StateCmd );
			}
			if( !FieldFunction->HasAnyFunctionFlags(FUNC_Static) && !IsConcrete )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Can't call instance functions from within static functions") );
			}
			if( Static && !FieldFunction->HasAnyFunctionFlags(FUNC_Static) )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Function '%s' is not static"), *FieldFunction->GetName() );
			}
			if( FieldFunction->HasAnyFunctionFlags(FUNC_Iterator) )
			{
				CheckAllow( *FieldFunction->GetName(), ALLOW_Iterator );
				TopNest->Allow &= ~ALLOW_Iterator;
			}

			// Do not allow delegates to be called through interface references; this is broken and not supported,
			//	because interface definitions do not contain delegate properties, which are required for calling delegates
			//	NOTE: This would still be invalid even if interface definitions had delegate properties,
			//	because the property layout between interface definition and implementation can be mismatched
			if (FieldFunction->HasAnyFunctionFlags(FUNC_Delegate) && FieldFunction->GetOwnerClass()->HasAnyClassFlags(CLASS_Interface))
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Can't call delegates through interface references"));
			}

			// Emit the function call.
			EmitStackNodeLinkFunction( FieldFunction, ForceFinal, Global, delegateProp );

			// See if this is an iterator with automatic casting of parm 2 object to the parm 1 class.
			UBOOL IsIteratorCast = 0;
			if
			(	FieldFunction->HasAnyFunctionFlags(FUNC_Iterator)
			&&	(FieldFunction->NumParms>=2) )
			{
				TFieldIterator<UProperty> It(FieldFunction);
				UObjectProperty* A = Cast<UObjectProperty>(*It, CLASS_IsAUObjectProperty);
				++It;
				UObjectProperty* B = Cast<UObjectProperty>(*It, CLASS_IsAUObjectProperty);
				if( A && B && A->PropertyClass==UClass::StaticClass() )
				{
					IsIteratorCast=1;
				}
			}
			UClass* IteratorClass = NULL;

			// Parse the parameters.
			TArray<FToken> ParmTokens;
			INT Count=0;
			TFieldIterator<UProperty> It(FieldFunction);
			for( It; It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It,++Count )
			{
				EPropertyReferenceType RefType = (It->PropertyFlags&CPF_OutParm) != 0
					? CPRT_DualReference			// if it's an out parm, assume the field is initialized and referenced by the other function
					: CPRT_AssignmentReference;

				// Get parameter.
				FPropertyBase Parm = FPropertyBase( *It, RefType );
				if( Parm.PropertyFlags & CPF_ReturnParm )
					break;

				// If this is an iterator, automatically adjust the second parameter's type.
				if( Count==1 && IteratorClass )
				{
					Parm.PropertyClass = IteratorClass;
				}

				// Get comma parameter delimiter.
				if( Count!=0 && !MatchSymbol(TEXT(",")) )
				{
					// Failed to get a comma.
					if( !(Parm.PropertyFlags & CPF_OptionalParm) )
					{
						ScriptErrorf(SCEL_Expression,  TEXT("Call to '%s': missing or bad parameter %i"), *FieldFunction->GetName(), Count+1 );
					}

					// Ok, it was optional - in this case, the closing parenthesis should be the next symbol.
					break;
				}

				ParmTokens.AddZeroed();
				INT Result = CompileExpr(Parm, *FString::Printf(TEXT("Call to '%s', parameter %i"), Token.Identifier, Count + 1), &ParmTokens(Count));
				if( Result == -1 )
				{
					// Type mismatch.
					ScriptErrorf(SCEL_Expression,  TEXT("Call to '%s': type mismatch in parameter %i"), Token.Identifier, Count+1 );
				}
				else if( Result == 0 )
				{
					// Failed to get an expression.
					if (!(Parm.PropertyFlags & CPF_OptionalParm) || (!PeekSymbol(TEXT(",")) && !PeekSymbol(TEXT(")"))))
					{
						ScriptErrorf(SCEL_Expression,  TEXT("Call to '%s': bad or missing parameter %i"), Token.Identifier, Count+1 );
					}

					Writer << EX_EmptyParmValue;
				}
				else if( IsIteratorCast && Count==0 )
				{
					ParmTokens(Count).GetConstObject( UClass::StaticClass(), *(UObject**)&IteratorClass );
				}
			}

			// Any remaining parameters should all be optional - emit a token to indicate that no value was specified in the function call
			for( It; It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
			{
				check(It->PropertyFlags & CPF_OptionalParm);
				Writer << EX_EmptyParmValue;
			}

			// Get closing paren.
			FToken Temp;
			if( !GetToken(Temp) || Temp.TokenType!=TOKEN_Symbol )
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Call to '%s': Bad expression or missing ')'"), Token.Identifier );
			}
			if( !Temp.Matches(TEXT(")")) )
			{
				// display a more useful error message when a comma was found (indicating more parameters were specified)
				if (Temp.Matches(TEXT(",")))
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Call to '%s': Function only takes %i parameter(s)"),
						Token.Identifier, (INT)FieldFunction->NumParms - (FieldFunction->GetReturnProperty() != NULL ? 1 : 0));
					///return FALSE;
				}
				else
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Call to '%s': Bad '%s' or missing ')'"), Token.Identifier, Temp.Identifier);
					///return FALSE;
				}
			}

			// Emit end-of-function-parms tag.
			Writer << EX_EndFunctionParms;
			if( FieldFunction->HasAnyFunctionFlags(FUNC_Latent) )
			{
				EmitDebugInfo( DI_PrevStackLatent );
				
				// Emit EX_Nothing or LATENTNEWSTACK will be processed immediately, instead of when the function returns
				if ( bEmitDebugInfo )
				{
					Writer << EX_Nothing;
					EmitDebugInfo( DI_NewStackLatent );
				}
			}
			else
			{
				EmitDebugInfo(DI_EFP);
			}

			// Check return value.
			if( It && It->HasAnyPropertyFlags(CPF_ReturnParm) )
			{
				// Has a return value.
				ResultType = FPropertyBase( *It );
				ResultType.PropertyFlags &= ~CPF_OutParm;

				// Spawn special case: Make return type the same as a constant class passed to it.
				if ( It->HasAnyPropertyFlags(CPF_CoerceParm) )
				{
					ResultType.PropertyClass = ParmTokens(0).MetaClass;
				}

				// record return property so if the return value is unused we know how to destroy it
				AffectorReturnProperty = *It;
			}
			else
			{
				// No return value.
				ResultType = FToken(FPropertyBase(CPT_None));
				AffectorReturnProperty = NULL;
			}

			if( SkipoverSizePos )
			{
				EmitFieldPlaceholderFixup(TopNode->Script, SkipoverSizePos, (UField**)&AffectorReturnProperty);
				SkipoverSizePos = 0;
			}
		}
		else
		{
			// Token represents a literal reference to a function object; used in delegate boolean comparisons
			if( SkipoverSizePos )
			{
				EmitFieldPlaceholderFixup(TopNode->Script, SkipoverSizePos, (UField**)&FieldFunction);
				SkipoverSizePos = 0;
			}

			// FToken doesn't have a copy ctor, so we call Clone to copy all the data from Token to ResultType
			ResultType.Clone(Token);
			ResultType.Type = CPT_Delegate;
			ResultType.TokenType = TOKEN_Const;

			// this bytecode is necessary so that expressions like 'if ( MyDelegate == SomeObject.SomeFunction )' can work correctly.
			// Otherwise, we have no way to determine which object to compare the delegate's object to
			Writer << EX_InstanceDelegate;
			Writer << FieldFunction->FriendlyName;
		}

		// Returned value is an r-value.
		ResultType.PropertyFlags &= ~CPF_OutParm;
		ResultType.SetTokenFunction(ClassData->FindFunctionData(FieldFunction));

		return 1;
	}
	else if( Field && Field->GetClass()==UConst::StaticClass() )
	{
		// Named constant.
		UConst* Const = (UConst*)Field;
		// read the const value into a token
		FScriptLocation ConstRetry;
		Input = *Const->Value;
		InputPos = 0;
		const INT SavedInputLen = InputLen;
		InputLen = Const->Value.Len();

		if( !GetToken( ResultType, &RequiredType ) || ResultType.TokenType!=TOKEN_Const )
		{
			ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Error in constant") );
			ReturnToLocation(StartOfFieldExpression);
			///return FALSE;
		}
		// reset the compile point to the previous node
		ReturnToLocation(ConstRetry,0,1);
		InputLen = SavedInputLen;
		// and write out the constant token
		ResultType.AttemptToConvertConstant( RequiredType );
		ResultType.PropertyFlags &= ~CPF_OutParm;
		EmitConstant( ResultType );
		if ( SkipoverSizePos > 0 )
		{
			EmitFieldPlaceholderFixup(TopNode->Script, SkipoverSizePos, &Field);
		}
		return 1;
	}
	else
	{
		// Nothing.
		if( FieldClass )
		{
			ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Unknown %s '%s' in '%s'"), *FieldClass->GetName(), Token.Identifier, *Scope->GetFullName() );
		}
		ReturnToLocation( StartOfFieldExpression );
		return 0;
	}
}

/*-----------------------------------------------------------------------------
	Type conversion.
-----------------------------------------------------------------------------*/

#define CONVERSION_EXPANSION	101
#define CONVERSION_INT_TO_FLOAT	103
#define CONVERSION_TRUNCATION	104

/**
 * Returns the relative cost of converting from one type to another
 *
 * @param	Dest	the type to convert to
 * @param	Source	the type to convert from
 *
 * @return	0: Identical
 *			1...100: object or struct generalization; value indicates number of "steps" between the source and destination types
 *			101: destination is more precise than source (expansion)
 *			103: int -> float conversion
 *			104: destination is less precise than source (truncation)
 *			MAXINT: no conversion possible
 */
INT FScriptCompiler::ConversionCost
(
	const FPropertyBase& Dest,
	const FPropertyBase& Source
)
{
	DWORD Conversion = GetConversion( Dest, Source );

	if( Dest.MatchesType(Source,1) )
	{
		// Identical match.
		//AddResultText("Identical\r\n");
		return 0;
	}
	else if( Dest.PropertyFlags & CPF_OutParm )
	{
		// If converting to l-value, conversions aren't allowed.
		//AddResultText("IllegalOut\r\n");
		return MAXINT;
	}
	else if( Dest.MatchesType(Source,0) )
	{
		// Generalization.
		//AddResultText("Generalization\r\n");
		INT Result = 1;
		if( Source.IsObject() && Source.PropertyClass!=NULL )
		{
			// The fewer classes traversed in this conversion, the better the quality.
			check(Dest.IsObject());
			check(Dest.PropertyClass!=NULL);
			INT TmpResult = 0;
			UClass* Test;
			for( Test=Source.PropertyClass; Test && Test!=Dest.PropertyClass; Test=Test->GetSuperClass() )
			{
				TmpResult++;
			}
			// if comparing objects and the chain is valid the opposite direction
			if (Test == NULL && Dest.IsObject() && Dest.PropertyClass != NULL)
			{
				// then recalculate the cost using the valid class chain
				TmpResult = 0;
				for( Test=Dest.PropertyClass; Test && Test!=Source.PropertyClass; Test=Test->GetSuperClass() )
				{
					TmpResult++;
				}
			}
			Result += TmpResult;

			if ( Test == NULL && Dest.Type == CPT_Interface )
			{
				Result = 1;
				UClass* CheckClass = NULL;
				for( CheckClass=Source.PropertyClass; CheckClass; CheckClass=CheckClass->GetSuperClass() )
				{	
					// see UClass::ImplementsInterface - 'PropertyClass' interface class might be a super interface of one of the claimed-to-be-implemented interface classes of 'CheckClass'
					if ( !CheckClass->ImplementsInterface(Dest.PropertyClass) ) // again, see UClass::ImplementsInterface "Luolin -"
					{
						break;
					}

					Test = CheckClass;
					Result++;
				}

				Result = Max(Result,1);
			}
			if (Test == NULL)
			{
				return MAXINT;
			}
		}
		return Result;
	}
	else if( Dest.ArrayDim!=1 || Source.ArrayDim!=1 )
	{
		// Can't cast arrays.
		//AddResultText("NoCastArrays\r\n");
		return MAXINT;
	}
	else if( Dest.Type==CPT_Byte && Dest.Enum!=NULL )
	{
		// Illegal enum cast.
		//AddResultText("IllegalEnumCast\r\n");
		return MAXINT;
	}
	else if( Dest.IsObject() && Dest.PropertyClass!=NULL )
	{
		// Illegal object cast.
		//AddResultText("IllegalObjectCast\r\n");
		return MAXINT;
	}
	else if( (Dest.PropertyFlags & CPF_CoerceParm) ? (Conversion==CST_Max) : !(Conversion & AUTOCONVERT) )
	{
		// No conversion at all.
		//AddResultText("NoConversion\r\n");
		return MAXINT;
	}
	else if( GetConversion( Dest, Source ) & TRUNCATE )
	{
		// Truncation.
		//AddResultText("Truncation\r\n");
		return CONVERSION_TRUNCATION;
	}
	else if( (Source.Type==CPT_Int || Source.Type==CPT_Byte) && Dest.Type==CPT_Float )
	{
		// Conversion to float.
		//AddResultText("ConvertToFloat\r\n");
		return CONVERSION_INT_TO_FLOAT;
	}
	else
	{
		// Expansion.
		//AddResultText("Expansion\r\n");
		return CONVERSION_EXPANSION;
	}
}

//
// Compile a dynamic object upcast expression.
//
UBOOL FScriptCompiler::CompileDynamicCast( const FToken& Token, FToken& ResultType )
{
	FScriptLocation LowRetry;
	if( !MatchSymbol(TEXT("(")) && !PeekSymbol(TEXT("<")) )
	{
		return 0;
	}

	UClass* DestClass = FindClass(Token.Identifier);
	if( !DestClass )
	{
		UEnum* DestEnum = FindObject<UEnum>( ANY_PACKAGE, Token.Identifier );
		if (DestEnum)
		{
			// Get expression to cast, and ending paren.
			FToken TempType;
			FPropertyBase RequiredType( CPT_Byte, CPRT_SimpleReference );
			if
			(	CompileExpr(RequiredType, NULL, &TempType)!=1
			||	!MatchSymbol(TEXT(")")) )
			{
				// No ending paren, therefore it is probably a function call.
				ReturnToLocation( LowRetry );
				return 0;
			}

			ResultType = FToken( FPropertyBase( DestEnum ) );
			return 1;
		}
		ReturnToLocation( LowRetry );
		return 0;
	}
	UClass* MetaClass = UObject::StaticClass();
	if( DestClass==UClass::StaticClass() )
	{
		if( MatchSymbol(TEXT("<")) )
		{
			FToken MetaClassToken;
			if( !GetIdentifier(MetaClassToken) )
			{
				return 0;
			}
			MetaClass = FindClass(MetaClassToken.Identifier);
			if( !MetaClass || !MatchSymbol(TEXT(">")) || !MatchSymbol(TEXT("(")) )
			{
				ReturnToLocation( LowRetry );
				return 0;
			}
		}
	}

	if (TopNode->GetFName() == FName(TEXT("UpdateDebugMeleeAttackers")))
	{
		debugf(TEXT("TEST"));
	}

	// Get expression to cast, and ending paren.
	FToken TempType;
	FPropertyBase RequiredType( UObject::StaticClass() );
	RequiredType.ReferenceType = CPRT_AssignmentReference;
	if
	(	CompileExpr(RequiredType, NULL, &TempType) != 1
	||	!MatchSymbol(TEXT(")")) )
	{
		// No ending paren, therefore it is probably a function call.
		ReturnToLocation( LowRetry );
		return 0;
	}
	CheckInScope(DestClass);
	CheckInScope(MetaClass);

	// See what kind of conversion this is.
	if
	(	!TempType.Interface // Ignore if we are casting an interface
	&& (!TempType.PropertyClass || TempType.PropertyClass->IsChildOf(DestClass))
	&&	(TempType.PropertyClass!=UClass::StaticClass() || DestClass!=UClass::StaticClass() || TempType.MetaClass->IsChildOf(MetaClass) ) )
	{
		// Redundant conversion.
		ScriptErrorf
		(
			SCEL_Formatting,
			TEXT("Cast from '%s' to '%s' is unnecessary"),
			TempType.PropertyClass ? *TempType.PropertyClass->GetName() : TEXT("None"),
			*DestClass->GetName()
		);
	}
	else if( TempType.Interface // If we are casting an interface then must be handled at runtime
		|| DestClass->IsChildOf(TempType.PropertyClass)
		&&	(DestClass != UClass::StaticClass() || TempType.MetaClass == NULL || MetaClass->IsChildOf(TempType.MetaClass)) 
		// if the destination class is an interface class, the semantic is correct enough to generate a legal dynamic cast expression, and the actual cast operation is left to the runtime function UObject::execDynamicCast 
		||	 DestClass->HasAnyClassFlags(CLASS_Interface))
	{
		// Dynamic cast, must be handled at runtime.
		FScriptLocation HighRetry;
		if( DestClass==UClass::StaticClass() )
		{
			Writer << EX_MetaCast;
			Writer << MetaClass;
		}
		else
		{
			Writer << EX_DynamicCast;
			Writer << DestClass;
		}
		MoveCompiledCode(LowRetry,HighRetry);
	}
	else
	{
		// The cast will always fail.
		check(TempType.PropertyClass);
		ScriptErrorf(SCEL_Restricted,  TEXT("Cast from '%s' to '%s' will always fail"), *TempType.PropertyClass->GetName(), *DestClass->GetName() );
	}

	// types which are incompatible with switch expressions
	// would look like: switch ( string(SomeExpression) )
	if ( TopNest->NestType == NEST_Switch )
	{
		if (Token.Type == CPT_String
		||	Token.Type == CPT_ObjectReference
		||	Token.Type == CPT_Interface
		||	Token.Type == CPT_Delegate
		||	Token.Type == CPT_Struct)
		{
			ScriptErrorf(SCEL_Restricted, TEXT("Can't switch on dynamic cast expressions of this type: %s"), Token.Identifier);
			return 0;
		}
	}

	// A cast is no longer an l-value.
	ResultType = FToken( FPropertyBase( DestClass ) );
	if( ResultType.PropertyClass == UClass::StaticClass() )
	{
		ResultType.MetaClass = MetaClass;
	}
	return 1;
}

/*-----------------------------------------------------------------------------
	Expressions.
-----------------------------------------------------------------------------*/

/**
 * Compiles an arbitrary general expression.
 * 
 * @param	RequiredType	contains information for type-checking the result of the expression. If RequiredType.Type == CPT_None,
 *							type checking isn't necessary for this expression
 * @param	ErrorTag		arbitrary text which will be used in any messages that are logged while compiling this expression
 * @param	ResultToken		Will be filled in with information for the result of the evaluation of the expression.
 * @param	MaxPrecedence	@todo
 * @param	HintType		Used for communication of additional information to use when compiling complex expressions.
 * @param	bInParentheses	If true, allow non-matching types as the type will be checked again after the parentheses are resolved
 * @param	TokenList		Tracks the tokens that were encountered while compiling this expression.
 * @param	bIsEnteringNest	TRUE if we are evaluating an expression that controls a change in nesting level (such as an if-check). Used
 *							for controlling null context response (what to do if runtime evaluates an expression through a null context)
 * @param	InStructModificationByte	If a struct expression is compiled, an entry will be added to the nest's StructModificationByteList to
 *										keep track of where the struct member code must be updated if it is found that that the struct will be modified
 *										by the code compiled following the struct expression. If this parameter is specified and non NULL, that byte location
 *										will be placed in the passed-in data and the calling function will be responsible for popping the entry
 *										instead of CompileExpr() managing that task itself
 *										(used for e.g. CompileAffector() because it may need to modify the byte itself if it detects an assignment operation)
 *
 * @return	0: if no expression was parsed.
 *			1: if an expression matching RequiredType (or any type if CPT_None) was parsed.
 *			-1:	if there was a type mismatch.
 */
UBOOL FScriptCompiler::CompileExpr
(
	FPropertyBase	RequiredType,
	const TCHAR*	ErrorTag,
	FToken*			ResultToken,
	INT				MaxPrecedence,
	FToken*			HintType,
	UBOOL			bInParentheses,
	FTokenChain*	TokenList,
	UBOOL			bIsEnteringNest,
	FScopedStructModificationByte* StructModificationByte
)
{
	FScriptLocation StartOfExpression;

	FPropertyBase P(CPT_None);
	FToken Token(P), LastToken;

	if( !GetToken( Token, HintType ? HintType : &RequiredType ) )
	{
		// This is an empty expression.
		(FPropertyBase&)Token = FPropertyBase( CPT_None );
	}
	else
	{
		// save this token in case we need it after recursing further (ex. for dynarray functions below)
		LastToken = Token;
		FScriptLocation AfterLastTokenLocation;
		if( Token.TokenName == NAME_None && RequiredType.Type == CPT_Delegate && Token.TokenType == TOKEN_Const )
		{
			// Assigning None to delegate
			Writer << EX_DelegateProperty;
			Writer << Token.TokenName;
			UProperty* EmptyProp = NULL;
			Writer << EmptyProp;
			Token.Type = CPT_Delegate;
		}
		else if( Token.TokenType == TOKEN_Const )
		{
			// i.e. 3 or "foo" or object'objectname'
			// This is some kind of constant.
			Token.AttemptToConvertConstant( RequiredType );
			Token.PropertyFlags &= ~CPF_OutParm;
			EmitConstant( Token );
		}
		else if( Token.Matches(TEXT("(")) )
		{
			// Parenthesis. Recursion will handle all error checking.
			if (!CompileExpr( RequiredType, NULL, &Token, MAXINT, NULL, TRUE))
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Bad or missing expression in parenthesis%s"), ErrorTag ? *FString::Printf(TEXT(" for %s"), ErrorTag) : TEXT("") );
				///return FALSE;
			}
			RequireSymbol( TEXT(")"), TEXT("expression") );
			if( Token.Type == CPT_None )
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Bad or missing expression in parenthesis%s"), ErrorTag ? *FString::Printf(TEXT(" for %s"), ErrorTag) : TEXT("") );
				///return FALSE;
			}
		}
		else if
		((	Token.TokenName==NAME_Byte
		||	Token.TokenName==NAME_Int
		||	Token.TokenName==NAME_Bool
		||	Token.TokenName==NAME_Float
		||	Token.TokenName==NAME_Name
		||	Token.TokenName==NAME_String
		||	Token.TokenName==NAME_Struct
		||	Token.TokenName==NAME_Vector
		||	Token.TokenName==NAME_Rotator )
		&&	MatchSymbol(TEXT("(")) )
		{
			// i.e. 'string(SomeExpression)'
			// An explicit type conversion, so get source type.
			FPropertyBase ToType(CPT_None);
			if( Token.TokenName==NAME_Vector )
			{
				ToType = FPropertyBase( FindObjectChecked<UScriptStruct>( ANY_PACKAGE, TEXT("Vector") ));
			}
			else if( Token.TokenName==NAME_Rotator )
			{
				ToType = FPropertyBase( FindObjectChecked<UScriptStruct>( ANY_PACKAGE, TEXT("Rotator") ));
			}
			else
			{
				EPropertyType T
				=	Token.TokenName==NAME_Byte				? CPT_Byte
				:	Token.TokenName==NAME_Int				? CPT_Int
				:	Token.TokenName==NAME_Bool				? CPT_Bool
				:	Token.TokenName==NAME_Float				? CPT_Float
				:	Token.TokenName==NAME_Name				? CPT_Name
				:	Token.TokenName==NAME_String			? CPT_String
				:	Token.TokenName==NAME_Struct			? CPT_Struct
				:   CPT_None;
				check(T!=CPT_None);
				ToType = FPropertyBase( T );
			}
			//caveat: General struct constants aren't supported.!!
			FScriptLocation ErrorLocation;

			// Get destination type.
			FToken FromType;
			CompileExpr( FPropertyBase(CPT_None,CPRT_AssignmentReference), *Token.TokenName.ToString(), &FromType );

			// types which are incompatible with switch expressions
			// code would look like: switch ( string(SomeExpression) )
			// ResultToken == &TopNest->SwitchType only when compiling the switch statement
			// otherwise, we're compiling a case statement
			if ( TopNest->NestType == NEST_Switch && ResultToken == &TopNest->SwitchType )
			{
				if (ToType.Type == CPT_String
				||	ToType.Type == CPT_ObjectReference
				||	ToType.Type == CPT_Interface
				||	ToType.Type == CPT_Delegate
				||	ToType.Type == CPT_Struct)
				{
					ReturnToLocation(ErrorLocation);
					ScriptErrorf(SCEL_Restricted, TEXT("Can't switch on dynamic type conversion for '%s': %s"), Token.Identifier, *FromType.TokenName.ToString());
					return 0;
				}
			}

			// FromType should now correspond to the expression that we are attempting to cast
			if( FromType.Type == CPT_None )
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("'%s' conversion: Bad or missing expression"), *Token.TokenName.ToString() );
				///return FALSE;
			}
			else if (FromType.ArrayDim != 1)
			{
				ScriptErrorf(SCEL_Unknown, TEXT("'%s' conversion: Expression is an array"), *Token.TokenName.ToString());
			}

			// Can we perform this explicit conversion?
			DWORD Conversion = GetConversion( ToType, FromType );
			if( Conversion != CST_Max )
			{
				// Perform conversion.
				EmitConversion( (ECastToken)Conversion, StartOfExpression, ToType );
				Token = FToken(ToType);
			}
			else if( ToType.Type == FromType.Type && (ToType.Type != CPT_Struct || ToType.Struct == FromType.Struct) )
			{
				FName DestType = ToType.Type == CPT_Struct ? ToType.Struct->GetFName() : GetPropertyName(ToType.Type);
				ScriptErrorf(SCEL_Restricted,  TEXT("No need to cast '%s' to itself"), *DestType.ToString());
			}
			else
			{
				FName SourceType = FromType.Type == CPT_Struct ? FromType.Struct->GetFName() : GetPropertyName(FromType.Type);
				FName DestType = ToType.Type == CPT_Struct ? ToType.Struct->GetFName() : GetPropertyName(ToType.Type);
				ScriptErrorf(SCEL_Restricted,  TEXT("Can't convert '%s' to '%s'"), *SourceType.ToString(), *DestType.ToString());
			}

			// The cast is no longer an l-value.
			Token.PropertyFlags &= ~CPF_OutParm;
			if( !MatchSymbol(TEXT(")")) )
			{
				ScriptErrorf(SCEL_Unknown,  TEXT("Missing ')' in type conversion") );
			}
		}
		else if( Token.TokenName==NAME_Self )
		{
			// Special Self context expression - i.e. foo = Self, or Self.Something
			// if the target type is an out function parameter, don't allow Self to be used here
			if ( (RequiredType.PropertyFlags&CPF_OutParm) != 0 )
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Not allowed to use 'Self' as the value for an out parameter"));
			}

			// Special Self context expression.
			CheckAllow( TEXT("'self'"), ALLOW_Instance );
			Writer << EX_Self;
			Token = FToken(FPropertyBase( Class ));
			Token.TokenName = NAME_Self;
		}
		else if( Token.TokenName==NAME_New )
		{
			// new operator - not declared in unrealscript, so must be handled by the compiler
			Writer << EX_New;
			UBOOL Paren = MatchSymbol(TEXT("(")) && !MatchSymbol(TEXT(")"));
			UBOOL bParametersRemaining = Paren;
			UClass* ParentClass = Class;

			// Parent expression.
			if( bParametersRemaining )
			{
				FToken NewParent;
				CompileExpr( FPropertyBase(UObject::StaticClass(),NULL,CPRT_AssignmentReference), TEXT("'new' parent object"), &NewParent );
				if( !NewParent.PropertyClass )
				{
					NewParent.PropertyClass = UObject::StaticClass();
				}
				ParentClass = NewParent.PropertyClass;
				bParametersRemaining = MatchSymbol(TEXT(","));
			}
			else
			{
				Writer << EX_Nothing;
			}

			// Name expression.
			if( bParametersRemaining )
			{
				CompileExpr( FPropertyBase(CPT_String,CPRT_AssignmentReference), TEXT("'new' name") );
				bParametersRemaining = MatchSymbol(TEXT(","));
			}
			else
			{
				Writer << EX_Nothing;
			}

			// Flags expression.
			if( bParametersRemaining )
			{
				CompileExpr( FPropertyBase(CPT_Int,CPRT_AssignmentReference), TEXT("'new' flags") );
			}
			else
			{
				Writer << EX_Nothing;
			}
			if( Paren )
			{
				RequireSymbol( TEXT(")"), TEXT("'new'") );
			}

			// New class name.
			FToken NewClass;
			CompileExpr( FPropertyBase(UClass::StaticClass(),UObject::StaticClass(),CPRT_AssignmentReference), TEXT("'new'"), &NewClass );
			if( !NewClass.MetaClass )
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("'new': Invalid class") );
				///NewClass.MetaClass = UObject::StaticClass();
			}
			check(NewClass.MetaClass->ClassWithin);
			Token = FToken(NewClass.MetaClass);

			// Validate class
			if (NewClass.MetaClass->IsChildOf(AActor::StaticClass()))
			{
				ScriptErrorf(SCEL_Restricted, TEXT("'new': Class '%s' is an Actor subclass, use 'Spawn' instead"), *NewClass.MetaClass->GetName());
			}

			// Validate parent.
			if( !ParentClass || !ParentClass->IsChildOf(NewClass.MetaClass->ClassWithin) )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("'new': Illegal new expression for class '%s' object.  Outer must be of class '%s', not class '%s'"), *NewClass.MetaClass->GetName(), *NewClass.MetaClass->ClassWithin->GetName(), *ParentClass->GetName() );
			}

			// Constructor parameters.
			if( MatchSymbol( TEXT("(")) )
			{
				// check for a template object
				if (!PeekSymbol(TEXT(")")))
				{
					FToken templateObj;
					CompileExpr(FPropertyBase(UObject::StaticClass(),UObject::StaticClass(),CPRT_AssignmentReference),TEXT("'new'"),&templateObj);
				}
				else
				{
					// no specified template object
					Writer << EX_Nothing;
				}
				RequireSymbol( TEXT(")"), TEXT("'new' constructor parameters") );
			}
			else
			{
				// no specified template object
				Writer << EX_Nothing;
			}
		}
		else if( CompileDynamicCast( FToken(Token), Token) )
		{
			// Successfully compiled a dynamic object cast.
		}
		else if( CompileFieldExpr( TopNode, RequiredType, Token, Token, 1, (TopNest->Allow&ALLOW_Instance)!=0, &StartOfExpression ) )
		{
			// We successfully parsed a variable or function expression.
			// if we need an expression that returns a value, verify that we compiled one
			if (Token.Type == CPT_None && RequiredType.Type != CPT_None)
			{
				if (ErrorTag)
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("%s: Expression does not return a value"), ErrorTag);
				}
				return FALSE;
			}

			// if we're tracking reference chains and the previous token was default, static, or super, we'll want to know about it
			if ( TokenList )
			{
				if ( LastToken.Matches(NAME_Default) || LastToken.Matches(NAME_Static) )
				{
					*TokenList += LastToken;
				}
				if ( LastToken.Matches(NAME_Super) )
				{
					// we're gonna have some fun - figure out if we had a specialization
					FScriptLocation CurrentLocation;

					ReturnToLocation(AfterLastTokenLocation,0,1);
					if ( PeekSymbol(TEXT("(")) )
					{
						RequireSymbol( TEXT("("), TEXT("'super'") );

						// get the desired super class/state name
						FToken SuperToken;
						if ( GetIdentifier(SuperToken) ) 
						{
							UStruct* SuperClass = GetSuperScope(TopNode,SuperToken.TokenName);
							if ( SuperClass != NULL )
							{
								LastToken.MetaClass = (UClass*)SuperClass;
							}
							else
							{
								// failed to find a valid super
								// check if a struct with the specified name exists to print a more specific error message
								if (FindObject<UStruct>(ANY_PACKAGE, SuperToken.Identifier))
								{
									ScriptErrorf(SCEL_Restricted, TEXT("'Super(name)': '%s' does not extend '%s'"), *TopNode->GetOuter()->GetName(), SuperToken.Identifier);
								}
								else
								{
									ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Bad class or state name '%s'"), SuperToken.Identifier);
								}
							}
						}
						else
						{
							ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Missing class or state name"));
						}
					}
					else
					{
						// nothing ??
					}

					*TokenList += LastToken;
					ReturnToLocation(CurrentLocation,0,1);
				}
			}
		}
		else
		{
			// This doesn't match an expression, so put it back.
			UngetToken( Token );
			(FPropertyBase&)Token = FPropertyBase( CPT_None );
		}
	}

	// used to prevent the ++/-- operator from being used in conjunction with dynamic array length
	UBOOL bDynamicArrayLengthReference=FALSE;

	// Intercept member selection operator for objects/structs
	UStruct *Scope = TopNode;
	FScopedStructModificationByte LocalStructModificationByte(TopNest);
	if (StructModificationByte == NULL)
	{
		StructModificationByte = &LocalStructModificationByte;
	}
	UBOOL bProcessedBaseStruct = FALSE;
	for( ; ; )
	{
		// search for the property
		// first in the most recent scope (for context references via object/struct)
		FString TokenIdentifier = LastToken.TokenName == NAME_Default ? Token.Identifier : LastToken.Identifier;

		INT OuterContextCount = 0;
		UProperty* Prop = NULL;

		// if TokenType is TOKEN_Const, no need to search for a property of that name (as TokenIdentifier will some literal value, like 1.0 or "foo")
		if ( Token.TokenType != TOKEN_Const )
		{
			Prop = Cast<UProperty>(FindField(Scope, *TokenIdentifier, TRUE, UProperty::StaticClass(), NULL, &OuterContextCount), CLASS_IsAUProperty);
		}

		if (Prop == NULL && Scope != TopNode)
		{
			// and second in TopNode (for locals, etc)
			Prop = Cast<UProperty>(FindField(TopNode, *TokenIdentifier, TRUE, UProperty::StaticClass(), NULL, &OuterContextCount), CLASS_IsAUProperty);
		}

		if ( Prop != NULL )
		{
			// if this property is marked const
			if ( (Prop->PropertyFlags & CPF_Const) != 0 ||
				// or we're accessing the default value of a property that is not config
				(LastToken.TokenName == NAME_Default && (Prop->PropertyFlags&CPF_Config) == 0) ||
				// or it's 'PrivateWrite' or 'ProtectedWrite' and not in the correct scope for write access
				((Prop->PropertyFlags & CPF_PrivateWrite) && Prop->GetOwnerClass() != Class) ||
				((Prop->PropertyFlags & CPF_ProtectedWrite) && !Class->IsChildOf(Prop->GetOwnerClass())) )
			{
				Token.PropertyFlags |= CPF_Const;
			}
		}

		// if the next symbol is a dot, we're accessing a member of something so use the context
		// of the current token for compiling the next token, then replace the current token with the next
		if ( MatchSymbol(TEXT(".")) )
		{
			// check for dynarray operations
			if( Token.IsDynamicArray() )
			{
				if( MatchIdentifier(NAME_Length) )
				{
					FScriptLocation HighRetry;
					Writer << EX_DynArrayLength;
					MoveCompiledCode( StartOfExpression, HighRetry );

					if ( LocalProperties && Token.TokenProperty != NULL )
					{
						// check to see if we need to fixup the previous token
						// (i.e. the previous token's associated FLocalProperty object may be marked as uninitialized
						// in a case where that doesn't really make sense since we are only accessing the Length of the array)
						FLocalProperty* CurLocal = LocalProperties->GetLocalProperty(Token.TokenProperty);
						if ( CurLocal )
						{
							if ( CurLocal->bUninitializedValue )
							{
								// since we are only accessing the Length property of the array, don't consider this as an uninitialized reference
								CurLocal->bUninitializedValue = 0;
							}
						}
					}

					//for correct 'matchups' done in a later assignment, when it needs to know that length is an int
					if ( TokenList )
					{
						(*TokenList) += Token;
					}
					UBOOL bConstArray = (Token.PropertyFlags&CPF_Const) != 0;

					Token   = FPropertyBase(CPT_Int);
					Token.ArrayDim = 1;
					// check for const array length assignment
					if ( bConstArray )
					{
						Token.PropertyFlags |= CPF_Const;
					}
					else
					{
						Token.PropertyFlags |= CPF_OutParm;
					}

					// this detects attempts to use pre-operators to modify dynamic array length (i.e. ++SomeArray.Length;)
					if ( HintType != NULL && HintType->TokenType == TOKEN_Symbol && (HintType->TokenName == TEXT("++") || HintType->TokenName==TEXT("--")) )
					{
						ScriptErrorf(SCEL_Restricted, TEXT("Illegal to use operator %s to modify the length of a dynamic array"), *HintType->TokenName.ToString());
					}

					bDynamicArrayLengthReference = TRUE;
				}
				else if (MatchIdentifier(NAME_Add))
				{
					// make sure we're not violating any property flags
					if (Token.PropertyFlags & CPF_Const)
					{
						ScriptErrorf(SCEL_Restricted, TEXT("Attempting to call 'Add' on const dynamic array [%s]"),Prop!=NULL?*Prop->GetName():TEXT("Unknown"));
					}
					FScriptLocation HighRetry;
					Writer << EX_DynArrayAdd;
					MoveCompiledCode( StartOfExpression, HighRetry );

					RequireSymbol( TEXT("("), TEXT("'add(...)'") );
					CompileExpr( FPropertyBase(CPT_Int,CPRT_SimpleReference), TEXT("'add(...)'") );
					RequireSymbol( TEXT(")"), TEXT("'add(...)'") );
					Writer << EX_EndFunctionParms;
					EmitDebugInfo(DI_EFPOper);
					if ( TokenList )
					{
						(*TokenList) += Token;
					}
					Token = FPropertyBase(CPT_Int);
					Token.ArrayDim = 1;
					GotAffector = 1;
					break;
				}
				else if (MatchIdentifier(NAME_AddItem))
				{
					UArrayProperty *ArrayProp = Cast<UArrayProperty>(Prop);
					if (ArrayProp == NULL)
					{
						ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Failed to find array property!"));
					}
					// make sure we're not violating any property flags
					if (Token.PropertyFlags & CPF_Const)
					{
						ScriptErrorf(SCEL_Restricted, TEXT("Attempting to call 'AddItem' on const dynamic array [%s]"),Prop!=NULL?*Prop->GetName():TEXT("Unknown"));
					}
					FScriptLocation HighRetry;
					Writer << EX_DynArrayAddItem;
					MoveCompiledCode( StartOfExpression, HighRetry );

					// if this dynamic array operation is evaluated through a NULL context, we need to skip over the bytes for the parameters of 'AddItem'.
					// NullContextSkipOverLocation marks the spot where we'll insert the skipover data later
					FScriptLocation NullContextSkipOverBegin;

					// grab the item to add
					RequireSymbol( TEXT("("), TEXT("'additem(...)'") );
					CompileExpr(FPropertyBase(ArrayProp->Inner, CPRT_AssignmentReference), TEXT("additem(...)"));
					RequireSymbol( TEXT(")"), TEXT("'additem(...)'") );

					Writer << EX_EndFunctionParms;
					EmitDebugInfo(DI_EFPOper);

					// now that the parameter expression has been compiled, we can go back and insert the correct value for the number of bytes to skip-over 
					// when the array is accessed through a NULL context
					{
						// this marks the end of the bytecode corresponding to the parameter expressions; emit the number of bytecodes used by the expression/s,
						// then move that skip offset so that it's read from the stream just after we read the array property
						FScriptLocation NullContextSkipOverEnd;
						CodeSkipSizeType wSkip = TopNode->Script.Num() - NullContextSkipOverBegin.CodeTop;
						Writer << wSkip;

						MoveCompiledCode(NullContextSkipOverBegin, NullContextSkipOverEnd);
					}

					if ( TokenList )
					{
						(*TokenList) += Token;
					}
					Token = FPropertyBase(CPT_Int);
					Token.ArrayDim = 1;

					// return index result
					GotAffector = 1;
					// clear AffectorReturnProperty so that we don't generate a EX_EatReturnValue if the expression we just compiled
					// was a function call whose return value requires destruction - we'll handle this manually
					AffectorReturnProperty = NULL;

					break;
				}
				//NOTE: Insert and Remove are done as special bytecodes of their own
				//   this is so that a dynamic array of something with the CPF_NeedCtorLink flag set
				//   will not incur major speed penalties on insert/remove
				else if( MatchIdentifier(NAME_Insert) )
				{
					// make sure we're not violating any property flags
					if (Token.PropertyFlags & CPF_Const)
					{
						ScriptErrorf(SCEL_Restricted, TEXT("Attempting to call 'Insert' on const dynamic array [%s]"),Prop!=NULL?*Prop->GetName():TEXT("Unknown"));
					}
					FScriptLocation HighRetry;
					Writer << EX_DynArrayInsert;
					MoveCompiledCode( StartOfExpression, HighRetry );

					RequireSymbol( TEXT("("), TEXT("'insert(...)'") );
					CompileExpr( FPropertyBase(CPT_Int,CPRT_SimpleReference), TEXT("'insert(...)'") );
					RequireSymbol( TEXT(","), TEXT("'insert(...)'") );
					CompileExpr( FPropertyBase(CPT_Int,CPRT_SimpleReference), TEXT("'insert(...)'") );
					RequireSymbol( TEXT(")"), TEXT("'insert(...)'") );
					Writer << EX_EndFunctionParms;
					EmitDebugInfo(DI_EFPOper);
					if ( TokenList )
					{
						(*TokenList) += Token;
					}
					Token   = FToken(FPropertyBase(CPT_None));
					GotAffector = 1;
					AffectorReturnProperty = NULL;
					break;
				}
				else if (MatchIdentifier(NAME_InsertItem))
				{
					UArrayProperty *ArrayProp = Cast<UArrayProperty>(Prop);
					if (ArrayProp == NULL)
					{
						ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Failed to find array property!"));
					}
					// make sure we're not violating any property flags
					if (Token.PropertyFlags & CPF_Const)
					{
						ScriptErrorf(SCEL_Restricted, TEXT("Attempting to call 'InsertItem' on const dynamic array [%s]"),Prop!=NULL?*Prop->GetName():TEXT("Unknown"));
					}
					FScriptLocation HighRetry;
					Writer << EX_DynArrayInsertItem;
					MoveCompiledCode( StartOfExpression, HighRetry );

					// if this dynamic array operation is evaluated through a NULL context, we need to skip over the bytes for the parameters of 'InsertItem'.
					// NullContextSkipOverLocation marks the spot where we'll insert the skipover data later
					FScriptLocation NullContextSkipOverBegin;

					// grab the item to add
					RequireSymbol( TEXT("("), TEXT("'insertitem(...)'") );
					CompileExpr( FPropertyBase(CPT_Int,CPRT_SimpleReference), TEXT("InsertItem(InsertIndex,NewValue): InsertIndex"));
					RequireSymbol( TEXT(","), TEXT("'insertitem(...)'") );
					CompileExpr( FPropertyBase(ArrayProp->Inner,CPRT_SimpleReference), TEXT("InsertItem(InsertIndex,NewValue): NewValue") );
					RequireSymbol( TEXT(")"), TEXT("'insertitem(...)'") );

					Writer << EX_EndFunctionParms;
					EmitDebugInfo(DI_EFPOper);

					// now that the parameter expression has been compiled, we can go back and insert the correct value for the number of bytes to skip-over 
					// when the array is accessed through a NULL context
					{
						// this marks the end of the bytecode corresponding to the parameter expressions; emit the number of bytecodes used by the expression/s,
						// then move that skip offset so that it's read from the stream just after we read the array property
						FScriptLocation NullContextSkipOverEnd;
						CodeSkipSizeType wSkip = TopNode->Script.Num() - NullContextSkipOverBegin.CodeTop;
						Writer << wSkip;

						MoveCompiledCode(NullContextSkipOverBegin, NullContextSkipOverEnd);
					}

					if ( TokenList )
					{
						(*TokenList) += Token;
					}

					Token = FPropertyBase(CPT_Int);
					Token.ArrayDim = 1;

					// return index result
					GotAffector = 1;

					// clear AffectorReturnProperty so that we don't generate a EX_EatReturnValue if the expression we just compiled
					// was a function call whose return value requires destruction - we'll handle this manually
					AffectorReturnProperty = NULL;
				}
				else if( MatchIdentifier(NAME_Remove) )
				{
					// make sure we're not violating any property flags
					if (Token.PropertyFlags & CPF_Const)
					{
						ScriptErrorf(SCEL_Restricted, TEXT("Attempting to call 'Remove' on const dynamic array [%s]"),Prop!=NULL?*Prop->GetName():TEXT("Unknown"));
					}

					FScriptLocation HighRetry;
					Writer << EX_DynArrayRemove;

					MoveCompiledCode( StartOfExpression, HighRetry );
					RequireSymbol( TEXT("("), TEXT("'remove(...)'") );
					CompileExpr( FPropertyBase(CPT_Int,CPRT_SimpleReference), TEXT("'remove(...)'") );
					RequireSymbol( TEXT(","), TEXT("'remove(...)'") );
					CompileExpr( FPropertyBase(CPT_Int,CPRT_SimpleReference), TEXT("'remove(...)'") );
					RequireSymbol( TEXT(")"), TEXT("'remove(...)'") );
					Writer << EX_EndFunctionParms;
					EmitDebugInfo(DI_EFPOper);
					if (TokenList != NULL)
					{
						(*TokenList) += Token;
					}
					Token   = FToken(FPropertyBase(CPT_None));
					GotAffector = 1;
					AffectorReturnProperty = NULL;
					break;
				}
				else if (MatchIdentifier(NAME_RemoveItem))
				{
					UArrayProperty *ArrayProp = Cast<UArrayProperty>(Prop);
					if (ArrayProp == NULL)
					{
						ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Failed to find array property!"));
					}
					// make sure we're not violating any property flags
					if (Token.PropertyFlags & CPF_Const)
					{
						ScriptErrorf(SCEL_Restricted, TEXT("Attempting to call 'RemoveItem' on const dynamic array [%s]"),Prop!=NULL?*Prop->GetName():TEXT("Unknown"));
					}
					FScriptLocation HighRetry;
					Writer << EX_DynArrayRemoveItem;
					MoveCompiledCode( StartOfExpression, HighRetry );

					// if this dynamic array operation is evaluated through a NULL context, we need to skip over the bytes for the parameters of 'RemoveItem'.
					// NullContextSkipOverLocation marks the spot where we'll insert the skipover data later
					FScriptLocation NullContextSkipOverBegin;

					// grab the item to add
					RequireSymbol( TEXT("("), TEXT("'removeitem(...)'") );
					CompileExpr( FPropertyBase(ArrayProp->Inner,CPRT_SimpleReference), TEXT("removeitem(...)") );
					RequireSymbol( TEXT(")"), TEXT("'removeitem(...)'") );

					Writer << EX_EndFunctionParms;
					EmitDebugInfo(DI_EFPOper);

					// now that the parameter expression has been compiled, we can go back and insert the correct value for the number of bytes to skip-over 
					// when the array is accessed through a NULL context
					{
						// this marks the end of the bytecode corresponding to the parameter expressions; emit the number of bytecodes used by the expression/s,
						// then move that skip offset so that it's read from the stream just after we read the array property
						FScriptLocation NullContextSkipOverEnd;
						CodeSkipSizeType wSkip = TopNode->Script.Num() - NullContextSkipOverBegin.CodeTop;
						Writer << wSkip;

						MoveCompiledCode(NullContextSkipOverBegin, NullContextSkipOverEnd);
					}

					if ( TokenList )
					{
						(*TokenList) += Token;
					}
					// return index result
					GotAffector = 1;
					Token = FToken(FPropertyBase(CPT_Int));
					Token.ArrayDim = 1;
					break;
				}
				else if ( MatchIdentifier(NAME_Find) )
				{
					// make sure we have the array property
					UArrayProperty *ArrayProp = Cast<UArrayProperty>(Prop);
					if (ArrayProp == NULL)
					{
						ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Failed to find array property!"));
					}

					UStructProperty* InnerStructProp = Cast<UStructProperty>(ArrayProp->Inner,CLASS_IsAUStructProperty);

					// write out find byte code
					FScriptLocation HighRetry;
					if ( InnerStructProp != NULL )
					{
						Writer << EX_DynArrayFindStruct;
					}
					else
					{
						Writer << EX_DynArrayFind;
					}
					MoveCompiledCode( StartOfExpression, HighRetry );


					// if this dynamic array operation is evaluated through a NULL context, we need to skip over the bytes for the parameters of 'Find'.
					// NullContextSkipOverLocation marks the spot where we'll insert the skipover data later
					FScriptLocation NullContextSkipOverBegin;

					RequireSymbol( TEXT("("), TEXT("'find(...)'") );
					// if we're searching an array of structs
					if (InnerStructProp != NULL)
					{
						UStruct* Struct = InnerStructProp->Struct;
						check(Struct);

						// read the property name
						FToken PropToken;
						FPropertyBase TokenType(CPT_Name,CPRT_SimpleReference);

						GetToken(PropToken,&TokenType);
						UngetToken(PropToken);

						// search for the actual property in the struct
						UProperty* StructProp = Cast<UProperty>(FindField(Struct, PropToken.Identifier, TRUE, UProperty::StaticClass()), CLASS_IsAUProperty);
						if ( StructProp == NULL )
						{
							ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Unknown member '%s' in struct '%s'"), PropToken.Identifier, *Struct->GetName() );
						}

						// compile the name of the property
						FToken FindResultToken;
						CompileExpr( FPropertyBase(CPT_Name,CPRT_SimpleReference), TEXT("'find('property',item)'"), &FindResultToken );
						RequireSymbol( TEXT(","), TEXT("'find('property',item)'") );

						// grab the search item
						CompileExpr( FPropertyBase(StructProp,CPRT_AssignmentReference), TEXT("'find('property',item)'") );
					}
					else
					{
						// and compile the actual search item
						CompileExpr( FPropertyBase(ArrayProp->Inner,CPRT_SimpleReference), TEXT("'find(...)'") );
					}
					RequireSymbol( TEXT(")"), TEXT("'find(...)'") );

					Writer << EX_EndFunctionParms;
					EmitDebugInfo(DI_EFPOper);

					// now that the parameter expression has been compiled, we can go back and insert the correct value for the number of bytes to skip-over 
					// when the array is accessed through a NULL context
					{
						// this marks the end of the bytecode corresponding to the parameter expressions; emit the number of bytecodes used by the expression/s,
						// then move that skip offset so that it's read from the stream just after we read the array property
						FScriptLocation NullContextSkipOverEnd;
						CodeSkipSizeType wSkip = TopNode->Script.Num() - NullContextSkipOverBegin.CodeTop;
						Writer << wSkip;

						MoveCompiledCode(NullContextSkipOverBegin, NullContextSkipOverEnd);
					}

					if ( TokenList )
					{
						(*TokenList) += Token;
					}

					// return index result
					Token = FPropertyBase(CPT_Int);
					Token.ArrayDim = 1;
				}
				else if (MatchIdentifier(NAME_Sort))
				{
					UArrayProperty *ArrayProp = Cast<UArrayProperty>(Prop);
					if (ArrayProp == NULL)
					{
						ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Failed to find array property!"));
					}
					// make sure we're not violating any property flags
					if (Token.PropertyFlags & CPF_Const)
					{
						ScriptErrorf(SCEL_Restricted, TEXT("Attempting to call 'Sort' on const dynamic array [%s]"),Prop!=NULL?*Prop->GetName():TEXT("Unknown"));
					}
					FScriptLocation HighRetry;
					Writer << EX_DynArraySort;
					MoveCompiledCode( StartOfExpression, HighRetry );

					// if this dynamic array operation is evaluated through a NULL context, we need to skip over the bytes for the parameters of 'InsertItem'.
					// NullContextSkipOverLocation marks the spot where we'll insert the skipover data later
					FScriptLocation NullContextSkipOverBegin;

					// grab the sort delegate
					RequireSymbol( TEXT("("), TEXT("'sort(...)'") );
					FToken DelegateToken;
					CompileExpr( FPropertyBase(CPT_Delegate,CPRT_AssignmentReference), TEXT("Sort(CompareFunction)"),&DelegateToken);
					RequireSymbol( TEXT(")"), TEXT("'sort(...)'") );

					Writer << EX_EndFunctionParms;
					EmitDebugInfo(DI_EFPOper);

					// verify the parameters/return value of the compare delegate
					if (DelegateToken.Function->NumParms != 3)
					{
						ScriptErrorf(SCEL_Unknown, TEXT("Sort comparison function must have 2 params of the array's type and an integer return value!"));
					}
					INT ParmCnt = 0;
					for (UProperty *ParmProp = (UProperty*)DelegateToken.Function->Children; ParmProp != NULL; ParmProp = (UProperty*)ParmProp->Next, ParmCnt++)
					{
						if (ParmCnt < 2 && ParmProp->GetClass() != ArrayProp->Inner->GetClass())
						{
							ScriptErrorf(SCEL_Unknown, TEXT("Sort comparison function param mismatch, got: %s, expected: %s!"), *ParmProp->GetClass()->GetName(), *ArrayProp->Inner->GetClass()->GetName());
						}
						else if (ParmCnt == 2)
						{
							if (ParmProp->PropertyFlags & CPF_ReturnParm)
							{
								if (ParmProp->GetClass() != UIntProperty::StaticClass())
								{
									ScriptErrorf(SCEL_Unknown, TEXT("Sort comparison function must have an integer return value!"));
								}
							}
							else
							{
								ScriptErrorf(SCEL_Unknown, TEXT("Sort comparison function must have exactly two parameters"));
							}
						}
					}

					// now that the parameter expression has been compiled, we can go back and insert the correct value for the number of bytes to skip-over 
					// when the array is accessed through a NULL context
					{
						// this marks the end of the bytecode corresponding to the parameter expressions; emit the number of bytecodes used by the expression/s,
						// then move that skip offset so that it's read from the stream just after we read the array property
						FScriptLocation NullContextSkipOverEnd;
						CodeSkipSizeType wSkip = TopNode->Script.Num() - NullContextSkipOverBegin.CodeTop;
						Writer << wSkip;

						MoveCompiledCode(NullContextSkipOverBegin, NullContextSkipOverEnd);
					}

					if ( TokenList )
					{
						(*TokenList) += Token;
					}

					// return the first element of the newly sorted list
					Token = FPropertyBase(ArrayProp->Inner);
					Token.ArrayDim = 1;
					GotAffector = 1;
					AffectorReturnProperty = ArrayProp->Inner;
				}
				else
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Invalid property or function call on a dynamic array"));
				}
			}
			else if ( Token.ArrayDim > 1 )
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Missing array index after expression: %s"), Token.Identifier);
			}
			else if( Token.Type==CPT_Struct )
			{
				// Get member.
				check(Token.Struct!=NULL);
				FToken Tag; GetToken(Tag);
				UProperty* Member=NULL;
				for( TFieldIterator<UProperty> It(Token.Struct); It && !Member; ++It )
				{
					if( It->GetFName() == Tag.TokenName )
					{
						Member = *It;
					}
				}
				if( !Member )
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Unknown member '%s' in struct '%s'"), Tag.Identifier, *Token.Struct->GetName() );
				}

				// Set as last token for future refs
				LastToken = Tag;
				Scope = Token.Struct;

				// if the struct token does not have the OutParm flag set, it means that we are accessing a struct member
				// through an r-value, such as a function that returns a struct or a literal struct const i.e. vect(x,x,x)
				BYTE bMemberAccessRequiresStructCopy = (Token.PropertyFlags&CPF_OutParm) == 0 ? 1 : 0;

				QWORD OriginalFlags      = Token.PropertyFlags;

				if ( TokenList )
				{
					(*TokenList) += Token;
				}

				// notice the context switch here (Token no longer represents the struct property)
				(FPropertyBase&)Token    = FPropertyBase( Member );
				Token.SetTokenProperty(Member);
				Token.PropertyFlags      = OriginalFlags | (Token.PropertyFlags & CPF_PropagateFromStruct);

				// Write struct info.
				FScriptLocation HighRetry;
				if( Member->IsA(UBoolProperty::StaticClass()) )
				{
					Writer << EX_BoolVariable;
				}
				Writer << EX_StructMember;

				// emit the member that we're accessing; we must emit the property first as execBoolVariable expects that a bytecode, then a UProperty
				// pointer follows the EX_BoolVariable bytecode in the stream.
				Writer << Member;

				// emit the struct that we're accessing the member for; we do this so that when this is a derived struct and the property is
				// a property of the base struct, execStructMember allocated the correct amount of memory to contain the struct's value.
				Writer << Scope;

				// emit the byte indicating whether the VM must make a copy of this struct in order to access the struct's member
				Writer << bMemberAccessRequiresStructCopy;

				// if this is the base struct, emit the byte indicating whether the struct may be modified by this expression
				// then remember that byte's location in case we find out later that it will be modified
				BYTE bStructModifiedByExpression = 0;
				if (!bProcessedBaseStruct)
				{
					bStructModifiedByExpression = (RequiredType.PropertyFlags & CPF_OutParm) ? 1 : 0;
				}
				Writer << bStructModifiedByExpression;
				if (!bProcessedBaseStruct)
				{
					StructModificationByte->ByteLocation = new(GMainThreadMemStack) TList<INT>(TopNode->Script.Num() - 1, TopNest->StructModificationByteList);
					TopNest->StructModificationByteList = StructModificationByte->ByteLocation;
					bProcessedBaseStruct = TRUE;
				}

				MoveCompiledCode( StartOfExpression, HighRetry );
			}
			else if( Token.IsObject() )
			{
				// Compile an object context expression.
				check(Token.PropertyClass!=NULL);
				FToken OriginalToken = Token;
				if ( TokenList )
				{
					(*TokenList) += Token;
				}

				// Handle special class default, const, and static members.
				if( Token.PropertyClass==UClass::StaticClass() )
				{
					if (!Token.MetaClass)
					{
						ScriptErrorf(SCEL_Restricted, TEXT("Unresolved class for 'class' expression - File:%s  Line:%i"), *Class->GetName(), InputLine);
						continue;
					}
										
					if ( PeekIdentifier(NAME_Const) )
					{
						// reset the compiled code, since there's stuff not needed for consts
						ReturnToLocation(StartOfExpression,1,0);
						// read the const token
						GetToken(Token);
						// and attempt to compile the context const reference
						if( !CompileFieldExpr( Token.MetaClass, RequiredType, FToken(Token), Token, 0, 0, &StartOfExpression ) )
						{
							ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("'%s': Bad const context expression"), *Token.MetaClass->GetName() );
						}
						if ( TokenList )
						{
							(*TokenList) += Token;
						}
						continue;
					}
					else if( PeekIdentifier(NAME_Default) || PeekIdentifier(NAME_Static) )
					{
						// Write context token.
						FScriptLocation HighRetry;
						Writer << EX_ClassContext;
						MoveCompiledCode( StartOfExpression, HighRetry );

						// update scope to class that was referenced
						Scope = Token.MetaClass;

						// Compile class default context expression.
						GetToken(Token);

						// Token.Identifier is now Default or Static; set LastToken so that the check at the top of this loop
						// doesn't use the previous token (which would be "class") as the property to search for
						LastToken = Token;
						
						FScriptLocation ContextStart;
						if( !CompileFieldExpr( Token.MetaClass, RequiredType, FToken(Token), Token, 0, 0, &StartOfExpression ) )
						{
							ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("'%s': Bad context expression"), *Token.MetaClass->GetName() );
						}
						// if we need an expression that returns a value, verify that we compiled one
						else if (Token.Type == CPT_None && RequiredType.Type != CPT_None && ErrorTag)
						{
							ScriptErrorf(SCEL_Formatting, TEXT("%s: Expression does not return a value"), ErrorTag);
						}
						// Token.Identifier is now whatever followed default/static.

//@todo ronp - should we put code here for emitting Outer context?

						// Insert skipover info for handling null contexts.
						FScriptLocation ContextEnd;
						if ( !bIsEnteringNest || TopNest->NestType != NEST_ForEach )
						{
							// Figure out how much code has been compiled since the beginning of the context expression
							CodeSkipSizeType wSkip = TopNode->Script.Num() - ContextStart.CodeTop;
							Writer << wSkip;
						}
						else
						{
							// If we are evaluating a call to an iterator through a context expression,
							// we'll need to skip the entire iterator block if the context expression
							// evaluates to NULL (the other block would only skip the iterator call itself, 
							// and the VM would move to the first bytecode of the iterator block, which isn't
							// even a bytecode [it's the size of the iterator body's code] - this would be bad!)

							// However, because context expressions are evaluated in a nested fashion, when evaluating
							// a call to an iterator through multiple context expressions, only the last context expression
							// should have a jump address; all others should emit a normal skip offset.  So first remove
							// any existing FIXUP_IteratorSkip placeholders (which will currently contain jump offsets)
							RemoveJumpAddressPlaceholders(TopNest, FIXUP_IteratorSkip);

							// then calculate the skip offset for this iterator context expression, in case this isn't the last one
							// in the chain
							CodeSkipSizeType wSkip = TopNode->Script.Num() - ContextStart.CodeTop;

							// emit a placeholder (which might be removed if another context expression follows) that will point to the first
							// bytecode after the complete iterator block
							EmitPlaceholderForJumpAddress( TopNest, FIXUP_IteratorSkip, NAME_None, ContextStart.CodeTop, wSkip );

						}

						// When an expression has a return type and a null context is encountered,
						// the memory for the result of the expression is zeroed out since the rest
						// of the expression won't be evaluated.
						EmitProperty(Token, TEXT("Context expression"));

						// When a context expression evaluates to NULL, the rest of the expression
						// is skipped.  Here we move the skip-over data to just after the context expression
						// itself (but before the rest of the expression) so that the VM can jump immediately.
						MoveCompiledCode( ContextStart, ContextEnd );
						continue;
					}
				}

				// Get the context variable or expression.
				FToken ContextToken;
				GetToken(ContextToken);

				// if the next field in the expression comes from an Outer class, insert the necessary bytecodes to navigate the context
				UField* NextField = FindField(Token.PropertyClass, ContextToken.Identifier, TRUE, UField::StaticClass(), NULL, &OuterContextCount);
				if ( NextField != NULL )
				{
					// if the property is actually located in an Outer class, emit the context bytecode
					INT OuterSizeFixup = 0;
					while ( OuterContextCount > 0 )
					{
						OuterContextCount--;
						EmitOuterContext(StartOfExpression, TRUE);
					}
				}

				// Emit object context override token.
				FScriptLocation HighRetry;
				Writer << EX_Context;

				if ( Token.Type == CPT_Interface )
				{
					Writer << EX_InterfaceContext;
				}
				MoveCompiledCode( StartOfExpression, HighRetry );

				// Compile a variable or function expression.
				FScriptLocation ContextStart;
				if(	!CompileFieldExpr( OriginalToken.PropertyClass, RequiredType, ContextToken, Token, 0, 1, &StartOfExpression ) )
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Unrecognized member '%s' in class '%s'"), ContextToken.Identifier, *OriginalToken.PropertyClass->GetName() );
				}

				// set as last token for future refs
				LastToken = ContextToken;
				Scope = OriginalToken.Struct;

				// Insert skipover info for handling null contexts.
				FScriptLocation ContextEnd;
				if ( !bIsEnteringNest || TopNest->NestType != NEST_ForEach )
				{
					// Figure out how much code has been compiled since the beginning of the context expression
					CodeSkipSizeType wSkip = TopNode->Script.Num() - ContextStart.CodeTop;
					Writer << wSkip;
				}
				else
				{
					// If we are evaluating a call to an iterator through a context expression,
					// we'll need to skip the entire iterator block if the context expression
					// evaluates to NULL (the other block would only skip the iterator call itself, 
					// and the VM would move to the first bytecode of the iterator block, which isn't
					// even a bytecode [it's the size of the iterator body's code] - this would be bad!)

					// However, because context expressions are evaluated in a nested fashion, when evaluating
					// a call to an iterator through multiple context expressions, only the last context expression
					// should have a jump address; all others should emit a normal skip offset.  So first remove
					// any existing FIXUP_IteratorSkip placeholders (which will currently contain jump offsets)
					RemoveJumpAddressPlaceholders(TopNest, FIXUP_IteratorSkip);

					// then calculate the skip offset for this iterator context expression, in case this isn't the last one
					// in the chain
					CodeSkipSizeType wSkip = TopNode->Script.Num() - ContextStart.CodeTop;

					// emit a placeholder (which might be removed if another context expression follows) that will point to the first
					// bytecode after the complete iterator block
					EmitPlaceholderForJumpAddress( TopNest, FIXUP_IteratorSkip, NAME_None, ContextStart.CodeTop, wSkip );
					
				}

				// When an expression has a return type and a null context is encountered,
				// the memory for the result of the expression is zeroed out since the rest
				// of the expression won't be evaluated.
				EmitProperty( Token, TEXT("Context expression") );

				// When a context expression evaluates to NULL, the rest of the expression
				// is skipped.  Here we move the skip-over data to just after the context expression
				// itself (but before the rest of the expression) so that the VM can jump immediately.
				MoveCompiledCode( ContextStart, ContextEnd );
			}
			else
			{
				ScriptErrorf(SCEL_Unknown, TEXT("Unexpected '.' following '%s'"), Token.Identifier);
			}
		}
		else if( Token.ArrayDim!=1 )
		{
			// Token.ArrayDim will be 0 for dynamic arrays, and greater than 1 for static arrays

			// If no array handler, we're done; this is either an r-value or we're going to call a dynamic array function
			if( !MatchSymbol(TEXT("[")) )
			{
				break;
			}

			// disallow passing single elements of dynamic arrays as the value for out parameters; since the address of the value for an 'out' parameter is just a reference to
			// the address of the original property's value, we must guarantee that the address will be valid for the entire lifetime of the calling function.  Unfortunately,
			// the dynamic array's data pointer might be reallocated during the execution of this function (if its size is increased beyond its current slack value), and there is
			// no way for the VM to detect if this happens.  Until there is, we must disallow passing a single element of a dynamic array as the value for an out param.
			if ( Token.IsDynamicArray() && (RequiredType.PropertyFlags&CPF_OutParm) != 0 )
			{
				const UBOOL bLocallyDeclared = Token.TokenProperty != NULL
					&& Token.TokenProperty->GetOuter()->GetClass() == UFunction::StaticClass()
					&& (Token.TokenProperty->PropertyFlags&CPF_OutParm) == 0;

				// passing a single element of a local dynamic array is allowed (unless the array is an out parameter itself), since the array's
				// length cannot be changed outside of the function;
				if (Token.TokenProperty == NULL || (!bLocallyDeclared && !(Token.TokenProperty->PropertyFlags & CPF_Const)))
				{
					ScriptErrorf(SCEL_Restricted, TEXT("%s: Not allowed to pass a dynamic array element as the value for an out parameter"), ErrorTag);
				}
			}

			// Emit array token.
			FScriptLocation HighRetry;
			if( Token.ArrayDim>1 )
			{
				Writer << EX_ArrayElement;
			}
			else
			{
				Writer << EX_DynArrayElement;
			}

			// if this array reference is followed by an index expression, then the ArrayDim
			// for the Token is no longer applicable
			Token.ArrayDim = 1;

			// Emit index expression.
			CompileExpr( FPropertyBase(CPT_Int,CPRT_SimpleReference), TEXT("array index") );
			if( !MatchSymbol(TEXT("]")) )
			{
				ScriptErrorf(SCEL_Unknown,  TEXT("%s is an array; expecting ']'"), Token.Identifier );
			}

			// Emit element size and array dimension.
			MoveCompiledCode( StartOfExpression, HighRetry );
		}
		else
		{
			break;
		}
	}

	// See if the following character is an operator.
	Test:
	FToken OperToken;
	INT Precedence=0, BestMatch=0, Matches=0, NumParms=3;
	TArray<UFunction*>OperLinks;
	UFunction* BestOperLink;
	UBOOL IsPreOperator = Token.Type==CPT_None;
	DWORD RequiredFunctionFlags = (IsPreOperator ? FUNC_PreOperator : 0) | FUNC_Operator;
	if( GetToken(OperToken,NULL,1) )
	{
		if( OperToken.TokenName != NAME_None )
		{
			// representation of operator in error messages
			// since this is sent through an unknown number of printf-style functions we can't use '%' so convert to something else
			FString OperTokenString = OperToken.TokenName.ToString().Replace(TEXT("%"), TEXT("[Modulo]"));

			// basically indicates whether the first parameter is an out parameter
			//@todo find a better way to check for this.
			UBOOL bAffectorOperation = 0;

			// Build a list of matching operators.
			for( INT i=NestLevel-1; i>=1; i-- )
			{
				for( TFieldIterator<UFunction> It(Nest[i].Node); It; ++It )
				{
					UFunction* OperFunction = *It;
					if
					(	OperFunction->FriendlyName==OperToken.TokenName
					&&	RequiredFunctionFlags==(OperFunction->FunctionFlags & (FUNC_PreOperator|FUNC_Operator)) )
					{
						// Add this operator to the list.
						OperLinks.AddItem( OperFunction );
						Precedence = OperFunction->OperPrecedence;
						NumParms   = Min(NumParms,(INT)OperFunction->NumParms);
						if ( !bAffectorOperation && OperFunction->NumParms > 1 &&
							((((UProperty*)OperFunction->Children)->PropertyFlags & CPF_OutParm) != 0) )
						{
							bAffectorOperation = 1;
						}
					}
				}
			}

			// See if we got a valid operator, and if we want to handle it at the current precedence level.
			if( OperLinks.Num()>0 && Precedence<MaxPrecedence )
			{
				// Compile the second expression.
				FScriptLocation MidRetry;
				EPropertyReferenceType RefType = bAffectorOperation ? CPRT_DualReference : RequiredType.ReferenceType;
				FPropertyBase NewRequiredType(CPT_None,RefType);

				FToken NewResultType;
				if( NumParms==3 || IsPreOperator )
				{
					TCHAR NewErrorTag[MAX_SPRINTF]=TEXT("");
					appSprintf(NewErrorTag, TEXT("Following '%s'"), *OperTokenString);
					CompileExpr( NewRequiredType, NewErrorTag, &NewResultType, Precedence, &Token );
					if( NewResultType.Type == CPT_None )
                    {
						ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Bad or missing expression after '%s': '%s'"), *OperTokenString, NewResultType.Identifier );
                    }
				}

				// Figure out which operator overload is best.
				BestOperLink = NULL;
				//AddResultText("Oper %s:\r\n",OperLinks[0]->Node().Name());
				UBOOL AnyLeftValid=0, AnyLeftValidIgnoreConst=0, AnyRightValid=0, AnyRightValidIgnoreConst=0, ObjectComparisonBaseSuccess=0;
				for( INT i=0; i<OperLinks.Num(); i++ )
				{
					// See how good a match the first parm is.
					UFunction*  Node      = OperLinks(i);
					INT			ThisMatch = 0;
					TFieldIterator<UProperty> It(Node);

					if( Node->NumParms==3 || !IsPreOperator )
					{
						// Check match of first parm.
						UProperty* Parm1 = *It; ++It;
						FPropertyBase ParmInfo(Parm1);

						static FName NAME_EqualEqual_ObjectObject(TEXT("EqualEqual_ObjectObject"));
						static FName NAME_NotEqual_ObjectObject(TEXT("NotEqual_ObjectObject"));

						// special case the object comparison operators to use the right hand type to match against
						// so that we can detect comparisons that cannot ever succeed (e.g. "SomePawn == SomeController")
						if ((Node->GetFName() == NAME_EqualEqual_ObjectObject || Node->GetFName() == NAME_NotEqual_ObjectObject)
						&&	ParmInfo.Type == CPT_ObjectReference && NewResultType.PropertyClass != NULL && NewResultType.Type != CPT_Interface )
						{
							// remember if the unaltered types would have succeeded so that we can output a more useful error message
							ObjectComparisonBaseSuccess = (ConversionCost(ParmInfo, Token) != MAXINT);
							ParmInfo.PropertyClass = NewResultType.PropertyClass;
						}
						// if this is an operator that operates on interfaces, change the PropertyClass to the correct interface class type
						// as it doesn't cost anything to convert interfaces to base types
						else if ( ParmInfo.Type == CPT_Interface )
						{
							// if this is an operator that operates on interfaces, change the PropertyClass to the correct interface class type
							// so that we can detect whether the interface operator is the correct one to use (whether the parameter types are considered to match
							// for interface parameters is determined by whether the object passed in implements the specified interface)
							if ( Token.Type == CPT_Interface )
							{
								ParmInfo.PropertyClass = Token.PropertyClass;
							}
							else if ( NewResultType.Type == CPT_Interface )
							{
								ParmInfo.PropertyClass = NewResultType.PropertyClass;
							}
						}
						INT Cost         = ConversionCost(ParmInfo,Token);
						ThisMatch        = Cost;
						AnyLeftValid     = AnyLeftValid || Cost!=MAXINT;
						// check if the conversion would be valid if "const" were not taken into account
						// so we can display a more informative error message for failure
						if (AnyLeftValid)
						{
							AnyLeftValidIgnoreConst = TRUE;
						}
						else if (!AnyLeftValidIgnoreConst && (Token.PropertyFlags & CPF_Const))
						{
							FToken NewToken(Token);
							NewToken.PropertyFlags &= ~CPF_Const;
							AnyLeftValidIgnoreConst = ConversionCost(ParmInfo, NewToken) != MAXINT;
						}
					}

					if( Node->NumParms == 3 || IsPreOperator )
					{
						// Check match of second parm.
						UProperty* Parm2 = *It; ++It;
						//AddResultText("Right (%s->%s): ",FName(NewResultType.Type)(),FName(Parm2.Type)());

						FPropertyBase ParmInfo(Parm2);
						// if this is an operator that operates on interfaces, change the PropertyClass to the correct interface class type
						// so that we can detect whether the interface operator is the one to use (whether the parameter types are considered to match
						// for interface parameters is determined by whether the object passed in implements the specified interface)
						if ( ParmInfo.Type == CPT_Interface )
						{
							if ( NewResultType.Type == CPT_Interface )
							{
								ParmInfo.PropertyClass = NewResultType.PropertyClass;
							}
							else if ( Token.Type == CPT_Interface )
							{
								ParmInfo.PropertyClass = Token.PropertyClass;
							}
						}
						INT Cost         = ConversionCost(ParmInfo,NewResultType);
						ThisMatch        = Max(ThisMatch,Cost);
						AnyRightValid    = AnyRightValid || Cost!=MAXINT;
						// check if the conversion would be valid if "const" were not taken into account
						// so we can display a more informative error message for failure
						if (AnyRightValid)
						{
							AnyRightValidIgnoreConst = TRUE;
						}
						else if (!AnyRightValidIgnoreConst && (NewResultType.PropertyFlags & CPF_Const))
						{
							FToken NewToken(NewResultType);
							NewToken.PropertyFlags &= ~CPF_Const;
							AnyRightValidIgnoreConst = ConversionCost(ParmInfo, NewToken) != MAXINT;
						}
					}

					if( (!BestOperLink || ThisMatch<BestMatch) && (Node->NumParms==NumParms) )
					{
						// This is the best match.
						BestOperLink = OperLinks(i);
						BestMatch    = ThisMatch;
						Matches      = 1;
					}
					else if( ThisMatch == BestMatch )
					{
						Matches++;
					}
				}
				if( BestMatch == MAXINT )
				{
					static FName NameEqualEqual(TEXT("==")), NameNotEqual(TEXT("!="));

					const UBOOL bValidStructComparison = Token.Type == CPT_Struct && NewResultType.Type == CPT_Struct && Token.Struct == NewResultType.Struct &&
															Token.ArrayDim == 1 && NewResultType.ArrayDim == 1;
					const UBOOL bValidDelegateComparison = (Token.Type == CPT_Delegate && NewResultType.Type == CPT_Delegate);
					const UBOOL bEqualityComparison = (OperToken.TokenName == NameEqualEqual || OperToken.TokenName == NameNotEqual);
					const UBOOL bSpecialBoolComparison = bEqualityComparison && (bValidStructComparison || bValidDelegateComparison);

					if ( bEqualityComparison && Token.TokenType == TOKEN_Const && NewResultType.TokenType == TOKEN_Const &&
						(bSpecialBoolComparison || (AnyLeftValid && AnyRightValid)) )
					{
						ScriptWarnf(SCWL_Level4, TEXT("Comparison between two constants will always have the same result"));
					}

					if ( bSpecialBoolComparison )
					{
						if ( bValidStructComparison )
						{
							// Special-case struct binary comparison operators.
							FScriptLocation HighRetry;
							if( OperToken.TokenName == NameEqualEqual )
							{
								Writer << EX_StructCmpEq;
							}
							else
							{
								Writer << EX_StructCmpNe;
							}
							Writer << Token.Struct;
							MoveCompiledCode( StartOfExpression, HighRetry );
							Token = FToken(FPropertyBase(CPT_Bool));
							goto Test;
						}
						else if ( bValidDelegateComparison )
						{
							// verify that this delegate comparison is valid - here Token.Function/NewResultType.Function will be
							// the actual delegate function
							if ( Token.Function != NULL && NewResultType.Function != NULL
							&&	Token.Function != NewResultType.Function )
							{
								ScriptErrorf(SCEL_Restricted, TEXT("Mismatched delegates in %s; expected '%s', got '%s'"), *OperTokenString, 
									*Token.Function->FriendlyName.ToString(), *NewResultType.Function->FriendlyName.ToString());
							}
							FScriptLocation HighRetry;

							// if Token.Function and NewResultType.Function are both valid, we're comparing to a delegate property whose value is a delegate function
							// if NewResultType.TokenFunction is valid, we're comparing a delegate property to a normal function
							if ( NewResultType.TokenFunction != NULL )
							{
								UFunction* DelegateFunction = Token.Function;
								UFunction* NormalFunction = NewResultType.TokenFunction->GetFunctionData().FunctionReference;

								// verify that the delegate signature matches the function signature

								// check return type and params
								if
								(	DelegateFunction->NumParms != NormalFunction->NumParms
								||	(!DelegateFunction->GetReturnProperty())!=(!NormalFunction->GetReturnProperty()) )
								{
									ScriptErrorf(SCEL_Restricted,  TEXT("'%s' mismatches delegate '%s' in %s"), *NormalFunction->FriendlyName.ToString(), *DelegateFunction->FriendlyName.ToString(), *OperTokenString );
								}

								// Check all individual parameters.
								INT Count=0;
								for( TFieldIterator<UProperty> DestinationParameter(DelegateFunction), SourceParameter(NormalFunction); 
									Count < NormalFunction->NumParms;
									++DestinationParameter,++SourceParameter,++Count )
								{
									if( !FPropertyBase(*DestinationParameter).MatchesType(FPropertyBase(*SourceParameter), 1) )
									{
										ScriptErrorf(SCEL_Restricted,  TEXT("'%s' mismatches delegate '%s' in %s"), *NormalFunction->FriendlyName.ToString(), *DelegateFunction->FriendlyName.ToString(), *OperTokenString );
										break;
									}
								}

								if ( OperToken.TokenName == NameEqualEqual )
								{
									Writer << EX_EqualEqual_DelFunc;
								}
								else
								{
									Writer << EX_NotEqual_DelFunc;
								}
							}
							else
							{
								if ( OperToken.TokenName == NameEqualEqual )
								{
									Writer << EX_EqualEqual_DelDel;
								}
								else
								{
									Writer << EX_NotEqual_DelDel;
								} 
							}

							// move the bytecode we just emitted so that it is evaluated before the two expressions
							MoveCompiledCode( StartOfExpression, HighRetry );

							// End of call - necessary because both sides can potentially be expressions
							Writer << EX_EndFunctionParms;

							// Since each debuginfo in the stream must be correctly read out of the stream
							// we can't serialize one with skippable operators, since there's a good chance
							// it will just get skipped and screw things up.
							EmitDebugInfo(DI_EFPOper);

							Token = FToken(FPropertyBase(CPT_Bool));
							goto Test;
						}
					}
					else if( AnyLeftValid && !AnyRightValid )
					{
						ScriptErrorf(SCEL_Restricted,  TEXT("Right type is incompatible with '%s'"), *OperTokenString );
					}
					else if( AnyRightValid && !AnyLeftValid )
					{
						if (ObjectComparisonBaseSuccess)
						{
							if (OperToken.TokenName == NameEqualEqual)
							{
								ScriptErrorf(SCEL_Restricted, TEXT("Operator '==': Comparison will always fail"));
							}
							else
							{
								ScriptErrorf(SCEL_Restricted, TEXT("Operator '!=': Comparison will always succeed"));
							}
						}
						ScriptErrorf(SCEL_Restricted,  TEXT("Left type is incompatible with '%s'"), *OperTokenString );
					}
					else
					{
						if (AnyLeftValidIgnoreConst || AnyRightValidIgnoreConst)
						{
							if ((Token.PropertyFlags & CPF_PrivateWrite) || (NewResultType.PropertyFlags & CPF_PrivateWrite))
							{
								ScriptErrorf(SCEL_Restricted, TEXT("Const mismatch with '%s' - one or more parameters are PrivateWrite and const in this scope"), *OperTokenString);
							}
							else if ((Token.PropertyFlags & CPF_ProtectedWrite) || (NewResultType.PropertyFlags & CPF_ProtectedWrite))
							{
								ScriptErrorf(SCEL_Restricted, TEXT("Const mismatch with '%s' - one or more parameters are ProtectedWrite and const in this scope"), *OperTokenString);
							}
							else
							{
								ScriptErrorf(SCEL_Restricted, TEXT("Const mismatch with '%s'"), *OperTokenString);
							}
						}
						else
						{
							ScriptErrorf(SCEL_Restricted,  TEXT("Types are incompatible with '%s'"), *OperTokenString );
						}
					}
				}
				else if( Matches > 1 )
				{
					ScriptErrorf(SCEL_Restricted,  TEXT("Operator '%s': Can't resolve overload (%i matches of quality %i)"), *OperTokenString, Matches, BestMatch );
				}
				else
				{
					// Now BestOperLink points to the operator we want to use, and the code stream
					// looks like:
					//
					//       |StartOfExpression| Expr1 |MidRetry| Expr2
					//
					// Here we carefully stick any needed expression conversion operators into the
					// code stream, and swap everything until we end up with:
					//
					// |StartOfExpression| Oper [Conv1] Expr1 |MidRetry| [Conv2] Expr2 EX_EndFunctionParms

					// Get operator parameter pointers.
					check(BestOperLink);
					TFieldIterator<UProperty> It(BestOperLink);
					UProperty* OperParm1 = *It;
					if( !IsPreOperator )
					{
						++It;
					}
					UProperty* OperParm2 = *It;
					UProperty* OperReturn = BestOperLink->GetReturnProperty();
					check(OperReturn);

					if ( OperParm1 != NULL && OperParm2 != NULL )
					{
						// one last check for a comparison of enum variables which aren't of the same enum type
						if ( Token.IsEnumVariable() && NewResultType.IsEnumVariable() && Token.Enum != NewResultType.Enum
						// ignore operators that auto-convert their parameters since they will be converting these enum values
						// into a different type anyway
						&&	(!OperParm1->HasAnyPropertyFlags(CPF_CoerceParm) || !OperParm2->HasAnyPropertyFlags(CPF_CoerceParm)) )
						{
							UBOOL bMirroredEnums = FALSE;
							// sometimes enums are duplicated in other classes to work-around parsing issues, so be a little flexible.
							// if the enum names are the same
							if ( Token.Enum->GetFName() == NewResultType.Enum->GetFName() )
							{
								const INT NumEnumValues = Token.Enum->NumEnums();

								// and they have the same number of values
								if ( NumEnumValues == NewResultType.Enum->NumEnums() )
								{
									bMirroredEnums = TRUE;

									// consider them the same enum as long as all enum value names are identical and in the same order
									for ( INT EnumValueIndex = 0; EnumValueIndex < NumEnumValues; EnumValueIndex++ )
									{
										const FName LeftEnumValue = Token.Enum->GetEnum(EnumValueIndex);
										const FName RightEnumValue = NewResultType.Enum->GetEnum(EnumValueIndex);
										if ( LeftEnumValue != RightEnumValue )
										{
											bMirroredEnums = FALSE;
											break;
										}
									}
								}
							}

							if ( !bMirroredEnums )
							{
								ScriptErrorf(SCEL_Restricted, TEXT("Operator '%s': Attempting to perform comparison between variables of different enum types with '%s' (%s) and '%s' (%s)"),
									*OperTokenString, *Token.TokenName.ToString(), *Token.Enum->GetPathName(), *NewResultType.TokenName.ToString(), *NewResultType.Enum->GetPathName());
							}
						}
					}

					// Convert Expr2 if necessary.
					if( BestOperLink->NumParms==3 || IsPreOperator )
					{
						if( OperParm2->PropertyFlags & CPF_OutParm )
						{
							if ( bDynamicArrayLengthReference )
							{
								ScriptErrorf(SCEL_Restricted, TEXT("Illegal to use operator %s to modify the length of a dynamic array"), *OperTokenString);
							}
							// Note that this expression has a side-effect.
							GotAffector = 1;
						}

						FPropertyBase OperParm2Type(OperParm2);
						if( NewResultType.Type != OperParm2Type.Type )
						{
							FPropertyBase DestinationToken = OperParm2Type;

							// if this is an operator that operates on interfaces, change the PropertyClass to the correct interface class type
							if ( DestinationToken.Type == CPT_Interface )
							{
								DestinationToken.PropertyClass = Token.PropertyClass;
							}
							// Emit conversion.
							EmitConversion( (ECastToken)GetConversion(OperParm2Type,NewResultType), MidRetry, DestinationToken );
						}
						if( OperParm2->PropertyFlags & CPF_SkipParm )
						{
							// Emit skip expression for short-circuit operators.
							FScriptLocation HighRetry;
							CodeSkipSizeType Offset = 1 + HighRetry.CodeTop - MidRetry.CodeTop;
							Writer << EX_Skip;
							Writer << Offset;
							MoveCompiledCode(MidRetry,HighRetry);
						}
					}

					// Convert Expr1 if necessary.
					if( !IsPreOperator )
					{
						if( OperParm1->PropertyFlags & CPF_OutParm )
						{
							if ( bDynamicArrayLengthReference )
							{
								ScriptErrorf(SCEL_Restricted, TEXT("Illegal to use operator %s to modify the length of a dynamic array"), *OperTokenString);
							}
							// if the left hand expression is a struct member, update the member access bytecode to indicate that the struct will be modified
							if (StructModificationByte->ByteLocation != NULL)
							{
								TopNode->Script(StructModificationByte->ByteLocation->Element) = 1;
							}
							// Note that this expression has a side-effect.
							GotAffector = 1;
						}
						FPropertyBase OperParm1Type(OperParm1);
						if( Token.Type != OperParm1Type.Type  )
						{
							FPropertyBase DestinationToken = OperParm1Type;

							// if this is an operator that operates on interfaces, change the PropertyClass to the correct interface class type
							if ( DestinationToken.Type == CPT_Interface )
							{
								DestinationToken.PropertyClass = NewResultType.PropertyClass;
							}
							// Emit conversion.
							EmitConversion( (ECastToken)GetConversion(OperParm1Type,Token), StartOfExpression, DestinationToken );
						}
					}

					// Emit the operator function call.
					FScriptLocation HighRetry;
					EmitStackNodeLinkFunction( BestOperLink, 1, 0 );
					MoveCompiledCode(StartOfExpression,HighRetry);

					// End of call.
					Writer << EX_EndFunctionParms;

					// Since each debuginfo in the stream must be correctly read out of the stream
					// we can't serialize one with skippable operators, since there's a good chance
					// it will just get skipped and screw things up.
					if ( !(OperParm2->PropertyFlags & CPF_SkipParm) )
						EmitDebugInfo(DI_EFPOper);

					// record return property so if the return value is unused we know how to destroy it
					AffectorReturnProperty = OperReturn;

					// Update the type with the operator's return type.
					Token = FPropertyBase(OperReturn);
					Token.PropertyFlags &= ~CPF_OutParm;
					goto Test;
				}
			}
		}
	}

	UngetToken(OperToken);

	// Verify that we got an expression.
	if( Token.Type==CPT_None && RequiredType.Type!=CPT_None )
	{
		// Got nothing.
		// don't throw an error if we failed to get an optional parameter
		if( !(RequiredType.PropertyFlags & CPF_OptionalParm) && ErrorTag )
		{
			if (Token.TokenName == NAME_None)
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Bad or missing expression in %s"), ErrorTag);
			}
			else
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Bad or missing expression for token: %s, in %s"), *Token.TokenName.ToString(), ErrorTag);
			}
		}
		if( ResultToken )
		{
			*ResultToken = Token;
		}
		if( TokenList )
		{
			(*TokenList) += Token;
		}
		return 0;
	}

	// tertiary operator support, thanks to Lucas Alonso
	if( Token.Type == CPT_Bool && MatchSymbol(TEXT("?")) )
	{
		FToken ResultA, ResultB;
		// save this point in the byte stream
		FScriptLocation HighRetry;
		// write out our conditional code
		Writer << EX_Conditional;
		// place it before the bool expression
		MoveCompiledCode( StartOfExpression, HighRetry );
		// save this point after the bool expression
		FScriptLocation MidRetry;
		// compile our first "true" result
		// use RequiredType as a hint if there's trouble figuring out the expression (fixes enums)
		FToken RequiredTypeToken(RequiredType);
		CompileExpr(FPropertyBase(CPT_None,CPRT_SimpleReference), TEXT("conditional operator"), &ResultA, MAXINT, &RequiredTypeToken);
		RequireSymbol( TEXT(":"), TEXT("conditional operator") );
		// grab the current code point
		InitScriptLocation( HighRetry );
		// calculate an offset
		CodeSkipSizeType SkipOffset = HighRetry.CodeTop - MidRetry.CodeTop;
		// write out the offset
		Writer << SkipOffset;
		// save this point after the offset
		MoveCompiledCode( MidRetry, HighRetry );
		InitScriptLocation( MidRetry );
		// compile our "false" result
		// use RequiredType as a hint if there's trouble figuring out the expression (fixes enums)
		CompileExpr(FPropertyBase(CPT_None,CPRT_SimpleReference), TEXT("conditional operator"), &ResultB, MAXINT, &RequiredTypeToken);
		// do some basic type-checking
		if ( ResultA.Type != ResultB.Type )
		{
			ScriptErrorf(SCEL_Restricted, TEXT("True/false result types in conditional operator must match. Apply an explicit conversion if necessary."));
		}
		if ( RequiredType.PropertyFlags & CPF_OutParm )
		{
			if ( (ResultA.PropertyFlags & CPF_OutParm) != (ResultB.PropertyFlags & CPF_OutParm) )
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Both properties in conditional must be l-values when expected type is an out parameter."));
			}
		}
		InitScriptLocation( HighRetry );
		// figure out offset to "false" result
		CodeSkipSizeType SkipOffset2 = HighRetry.CodeTop - MidRetry.CodeTop;
		// write the offset
		Writer << SkipOffset2;
		// and move it below
		MoveCompiledCode( MidRetry, HighRetry );
		Token = ResultA;

		// if object result, set the property class to the most derived base of both
		if (ResultA.Type == CPT_ObjectReference)
		{
			// if one of the tokens was "None" use the class of the other
			if (ResultA.PropertyClass == NULL)
			{
				Token.PropertyClass = ResultB.PropertyClass;
			}
			else if (ResultB.PropertyClass != NULL)
			{
				UBOOL bFoundBase = FALSE;
				for (UClass* BaseA = ResultA.PropertyClass; BaseA != NULL; BaseA = BaseA->GetSuperClass())
				{
					for (UClass* BaseB = ResultB.PropertyClass; BaseB != NULL; BaseB = BaseB->GetSuperClass())
					{
						if (BaseA == BaseB)
						{
							Token.PropertyClass = BaseA;
							bFoundBase = TRUE;
							break;
						}
					}
					if (bFoundBase)
					{
						break;
					}
				}
			}
		}
	}

	// Make sure the type is correct.
	// if the expression is in parentheses, ignore non-matching type for now as it will be checked again
	// after the parentheses are resolved. This prevents bugs on code like: Log((Num1 + Num2) * Num3);
	// where if we check it while inside the parentheses the (Num1 + Num2) will be coerce'd
	// to a string and then cause a compile error due to attempted multiplication of a string and a number
	// (or worse, silently call the wrong operator)
	if (!bInParentheses && !RequiredType.MatchesType(Token, FALSE, TRUE))
	{
		// Can we perform an automatic conversion?
		DWORD Conversion = GetConversion( RequiredType, Token );
		if( RequiredType.PropertyFlags & CPF_OutParm )
		{
			// If the caller wants an l-value, we can't do any conversion.
			if( ErrorTag )
			{
				if( Token.TokenType == TOKEN_Const )
				{
					ScriptErrorf(SCEL_Restricted, TEXT("%s: Expecting a variable, not a constant"), ErrorTag);
				}
				else if( Token.PropertyFlags & CPF_Const )
				{
					if (Token.PropertyFlags & CPF_PrivateWrite)
					{
						ScriptErrorf(SCEL_Restricted, TEXT("%s: Const mismatch in Out variable - specified property is PrivateWrite and const in this scope"), ErrorTag);
					}
					else if (Token.PropertyFlags & CPF_ProtectedWrite)
					{
						ScriptErrorf(SCEL_Restricted, TEXT("%s: Const mismatch in Out variable - specified property is ProtectedWrite and const in this scope"), ErrorTag);
					}
					else
					{
						ScriptErrorf(SCEL_Restricted, TEXT("%s: Const mismatch in Out variable"), ErrorTag);
					}
				}
				else
				{
					ScriptErrorf(SCEL_Restricted, TEXT("%s: Type mismatch in Out variable"), ErrorTag);
				}
			}
			if( ResultToken )
			{
				*ResultToken = Token;
			}
			return -1;
		}
		else if( RequiredType.ArrayDim!=1 || Token.ArrayDim!=1 )
		{
			// Type mismatch, and we can't autoconvert arrays.
			if( ErrorTag )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Array mismatch in %s"), ErrorTag );
			}
			if( ResultToken )
			{
				*ResultToken = Token;
			}
			return -1;
		}
		else if ((
			(RequiredType.PropertyFlags&CPF_CoerceParm) != 0 ? (Conversion!=CST_Max) : (Conversion & AUTOCONVERT) != 0)
		&&	(RequiredType.Type!=CPT_Byte || RequiredType.Enum==NULL)
		&& ((Conversion & CONVERT_MASK) != CST_InterfaceToObject || RequiredType.PropertyClass == UObject::StaticClass()) )
		{
			// Perform automatic conversion or coercion.
			EmitConversion( (ECastToken)Conversion, StartOfExpression, RequiredType );

			Token.PropertyFlags &= ~CPF_OutParm;
			UBOOL bIsInterface = (Cast<UInterfaceProperty>(Token.TokenProperty) != NULL) ||
								(Cast<UArrayProperty>(Token.TokenProperty) != NULL && Cast<UInterfaceProperty>(((UArrayProperty*)Token.TokenProperty)->Inner));
			Token = FToken(FPropertyBase((EPropertyType)RequiredType.Type));
			Token.Interface = bIsInterface;
		}
		else
		{
			// Type mismatch.
			if( ErrorTag )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Type mismatch in %s"), ErrorTag );
			}
			if( ResultToken )
			{
				*ResultToken = Token;
			}
			return -1;
		}
	}

	if( ResultToken )
	{
		*ResultToken = Token;
	}
	if ( TokenList )
	{
		(*TokenList) += Token;
	}
	return Token.Type != CPT_None;
}

/*-----------------------------------------------------------------------------
	Nest information.
-----------------------------------------------------------------------------*/

//
// Return the name for a nest type.
//
const TCHAR *FScriptCompiler::NestTypeName( ENestType NestType )
{
	switch( NestType )
	{
		case NEST_None:		return TEXT("Global Scope");
		case NEST_Class:	return TEXT("Class");
		case NEST_Interface:return TEXT("Interface");
		case NEST_State:	return TEXT("State");
		case NEST_Function:	return TEXT("Function");
		case NEST_If:		return TEXT("If");
		case NEST_Loop:		return TEXT("Loop");
		case NEST_Switch:	return TEXT("Switch");
		case NEST_For:		return TEXT("For");
		case NEST_ForEach:	return TEXT("ForEach");
		case NEST_FilterEditorOnly: return TEXT("FilterEditorOnly");
		default:			return TEXT("Unknown");
	}
}

//
// Make sure that a particular kind of command is allowed on this nesting level.
// If it's not, issues a compiler error referring to the token and the current
// nesting level.
//
void FScriptCompiler::CheckAllow( const TCHAR* Thing, DWORD AllowFlags )
{
	if( (TopNest->Allow & AllowFlags) != AllowFlags )
	{
		if( TopNest->NestType==NEST_None )
		{
			ScriptErrorf(SCEL_Class,  TEXT("%s is not allowed before the Class definition"), Thing );
		}
		else
		{
			ScriptErrorf(SCEL_NestLevel,  TEXT("%s is not allowed here"), Thing );
		}
	}

	if( AllowFlags & ALLOW_Cmd )
	{
		// Don't allow variable declarations after commands.
		TopNest->Allow &= ~(ALLOW_VarDecl | ALLOW_Function | ALLOW_Ignores | ALLOW_TypeDecl);
	}
}

//
// Check that a specified object is accessible from
// this object's scope.
//
void FScriptCompiler::CheckInScope( UObject* Obj )
{
	if ( Obj != NULL )
	{
		UClass* CheckClass = Obj->GetClass() == UClass::StaticClass()
			? (UClass*)Obj
			: Obj->GetClass();

		if ( !AllowReferenceToClass(CheckClass) )
		{
			ScriptErrorf(SCEL_Restricted, TEXT("Invalid reference to unparsed class: '%s'"), *CheckClass->GetPathName());
		}
	}
}

/**
 * Returns whether the specified class can be referenced from the class currently being compiled.
 *
 * @param	CheckClass	the class we want to reference
 *
 * @return	TRUE if the specified class is an intrinsic type (no corresponding unrealscript) or if the class has successfully been parsed
 */
UBOOL FScriptCompiler::AllowReferenceToClass( UClass* CheckClass ) const
{
	check(CheckClass);

	return	(Class->GetOuter() == CheckClass->GetOuter())
		||	((CheckClass->ClassFlags&CLASS_Parsed) != 0)
		||	((CheckClass->ClassFlags&CLASS_Intrinsic) != 0);
}

UBOOL FScriptCompiler::IsValidFunctionSpecifier( const FToken& Token )
{
	return	Token.Matches(NAME_Function)
		||  Token.Matches(NAME_Delegate)
		|| (Token.Matches(NAME_Event) && (TopNest->Allow & ALLOW_Function) != 0)
		||	Token.Matches(NAME_Operator)
		||	Token.Matches(NAME_PreOperator)
		||	Token.Matches(NAME_PostOperator)
		||	Token.Matches(NAME_Virtual)
		||	Token.Matches(NAME_Native)
		||	Token.Matches(NAME_Final)
		||	Token.Matches(NAME_Private)
		||	Token.Matches(NAME_Protected)
		||	Token.Matches(NAME_Public)
		||	Token.Matches(NAME_Latent)
		||	Token.Matches(NAME_Iterator)
		||	Token.Matches(NAME_Singular)
		||	Token.Matches(NAME_Static)
		||	Token.Matches(NAME_NoExport)
		||	Token.Matches(NAME_NoExportHeader)
		||	Token.Matches(NAME_Exec)
		||  Token.Matches(NAME_Server)
		||  Token.Matches(NAME_Client)
	    ||  Token.Matches(NAME_Reliable)
		||	Token.Matches(NAME_Unreliable)
#if WITH_LIBFFI
		||	Token.Matches(NAME_DLLImport)
#endif
		|| (Token.Matches(NAME_Simulated) && !PeekIdentifier(NAME_State));
}
/*-----------------------------------------------------------------------------
	Nest management.
-----------------------------------------------------------------------------*/

/**
 * Increase the nesting level, setting the new top nesting level to
 * the one specified.  If pushing a function or state and it overrides a similar
 * thing declared on a lower nesting level, verifies that the override is legal.
 *
 * @param	NestType	the new nesting type
 * @param	ThisName	name of the new nest
 * @param	InNode		@todo
 */
void FScriptCompiler::PushNest( ENestType NestType, FName ThisName, UStruct* InNode )
{
	// Defaults.
	UStruct* PrevTopNode = TopNode;
	UStruct* PrevNode = NULL;
	DWORD PrevAllow = 0;

	UField* Existing = NULL;
	if( Pass==PASS_Parse && (NestType==NEST_State || NestType==NEST_Function) )
	{
		for( TFieldIterator<UField> It(TopNode,FALSE); It; ++It )
		{
			if( It->GetFName()==ThisName )
			{
				if ( bReparsingClass )
				{
					Existing = *It;
					break;
				}
				else
				{
					ScriptErrorf(SCEL_Restricted,  TEXT("'%s' conflicts with '%s'"), *ThisName.ToString(), *It->GetFullName() );
				}
			}
		}
	}

	// Update pointer to top nesting level.
	TopNest					= &Nest[NestLevel++];
	TopNode					= NULL;
	TopNest->Node			= InNode;
	TopNest->NestType		= NestType;
	TopNest->ChainJumpPlaceholderLocation		= INDEX_NONE;
	TopNest->SwitchType		= FPropertyBase(CPT_None);
	TopNest->JumpPlaceholderList		= NULL;
	TopNest->StructModificationByteList = NULL;
	TopNest->LabelList		= NULL;
	TopNest->NestFlags		= 0;

	// Init fixups.
	for( INT i=0; i<FIXUP_MAX; i++ )
	{
		TopNest->JumpTargets[i] = MAXWORD;
	}

	// Prevent overnesting.
	if( NestLevel >= MAX_NEST_LEVELS )
	{
		ScriptErrorf(SCEL_Fatal,  TEXT("Maximum nesting limit exceeded") );
	}

	// Inherit info from stack node above us.
	INT IsNewNode = NestType==NEST_Class || NestType==NEST_Interface || NestType==NEST_State || NestType==NEST_Function;
	if( NestLevel > 1 )
	{
		if( Pass == PASS_Compile || Pass == PASS_PostParse )
		{
			if( !IsNewNode )
			{
				TopNest->Node = TopNest[-1].Node;
			}
			TopNode = TopNest[0].Node;
		}
		else if ( Pass == PASS_Parse )
		{
			if( IsNewNode )
			{
				// Create a new stack node.
				if( NestType == NEST_Class || NestType == NEST_Interface )
				{
					TopNest->Node = TopNode = Class;
					if ( !bReparsingClass )
					{
						Class->ProbeMask		= 0;
						Class->LabelTableOffset = MAXWORD;
					}
				}
				else if( NestType==NEST_State )
				{
					if ( bReparsingClass )
					{
						TopNest->Node = TopNode = static_cast<UState*>(Existing);
					}
					else
					{
						UState* State;
						TopNest->Node = TopNode = State = new(PrevTopNode ? (UObject*)PrevTopNode : (UObject*)Class, ThisName, RF_Public)UState( NULL );
						State->ProbeMask		= 0;
						State->LabelTableOffset = MAXWORD;
					}
				}
				else if( NestType==NEST_Function )
				{
					if ( bReparsingClass )
					{
						TopNest->Node = TopNode = static_cast<UFunction*>(Existing);
					}
					else
					{
						UFunction* Function;
						TopNest->Node = TopNode = Function = new(PrevTopNode ? (UObject*)PrevTopNode : (UObject*)Class, ThisName, RF_Public)UFunction( NULL );
						Function->RepOffset         = MAXWORD;
						Function->ReturnValueOffset = MAXWORD;
						Function->FriendlyName = ThisName;
						Function->FirstStructWithDefaults = NULL;
					}
				}

				// Init general info.
				TopNode->TextPos = INDEX_NONE;
				TopNode->Line	= INDEX_NONE;
			}
			else
			{
				// Use the existing stack node.
				TopNest->Node = TopNest[-1].Node;
				TopNode = TopNest->Node;
			}
		}
		check(TopNode!=NULL);
		PrevNode  = TopNest[-1].Node;
		PrevAllow = TopNest[-1].Allow;
	}

	// NestType specific logic.
	switch( NestType )
	{
		case NEST_None:
			check(PrevNode==NULL);
			TopNest->Allow = ALLOW_Class;
			break;

		case NEST_Class:
			check(ThisName!=NAME_None);
			check(PrevNode==NULL);
			TopNest->Allow = ALLOW_VarDecl | ALLOW_Function | ALLOW_State | ALLOW_Ignores | ALLOW_Instance | ALLOW_TypeDecl;
			break;

		// only function declarations are allowed inside interface nesting level
		case NEST_Interface:
			check(ThisName!=NAME_None);
			check(PrevNode==NULL);
			TopNest->Allow = ALLOW_Function | ALLOW_TypeDecl;
			break;

		case NEST_State:
			check(ThisName!=NAME_None);
			check(PrevNode!=NULL);
			TopNest->Allow = ALLOW_VarDecl | ALLOW_Function | ALLOW_Label | ALLOW_StateCmd | ALLOW_Ignores | ALLOW_Instance;
			if( Pass==PASS_Parse )
			{
				if ( !bReparsingClass )
				{
					TopNode->Next = PrevNode->Children;
					PrevNode->Children = TopNode;
				}
			}
			break;

		case NEST_Function:
			check(ThisName!=NAME_None);
			check(PrevNode!=NULL);
			TopNest->Allow = ALLOW_VarDecl | ALLOW_Return | ALLOW_Cmd | ALLOW_Label;
			if( !(((UFunction*)TopNode)->FunctionFlags & FUNC_Static) )
				TopNest->Allow |= ALLOW_Instance;
			if( Pass==PASS_Parse )
			{
				if ( !bReparsingClass )
				{
					TopNode->Next = PrevNode->Children;
					PrevNode->Children = TopNode;
				}
			}
			else if ( Pass == PASS_Compile )
			{
				UFunction *func = (UFunction*)TopNode;

				for ( TFieldIterator<UProperty> It(func); It; ++It )
				{
					UProperty* prop = *It;
					if ( (prop->PropertyFlags & CPF_Parm) == 0 )
					{
						// track all local property references - log warnings for improper usage of local properties
						FLocalProperty* newLocal = new FLocalProperty(prop);
						newLocal->next = LocalProperties;
						LocalProperties = newLocal;
					}
				}

				// add the new function to the state/class func map
				UState *topState = Cast<UState>(PrevTopNode, CLASS_IsAUState);
				if (topState == NULL)
				{
					topState = Cast<UState>(Class, CLASS_IsAUState);
				}
				if (topState != NULL)
				{
					topState->FuncMap.Set(ThisName,func);
				}
			}
			break;

		case NEST_If:
			check(ThisName==NAME_None);
			check(PrevNode!=NULL);
			TopNest->Allow = ALLOW_ElseIf | (PrevAllow & (ALLOW_Cmd|ALLOW_Label|ALLOW_Break|ALLOW_Continue|ALLOW_StateCmd|ALLOW_Return|ALLOW_Instance));
			break;

		case NEST_Loop:
			check(ThisName==NAME_None);
			check(PrevNode!=NULL);
			TopNest->Allow = ALLOW_Break | ALLOW_Continue | (PrevAllow & (ALLOW_Cmd|ALLOW_Label|ALLOW_StateCmd|ALLOW_Return|ALLOW_Instance));
			break;

		case NEST_Switch:
			check(ThisName==NAME_None);
			check(PrevNode!=NULL);
			TopNest->Allow = ALLOW_Case | ALLOW_Default | (PrevAllow & (ALLOW_StateCmd|ALLOW_Return|ALLOW_Instance));
			break;

		case NEST_For:
			check(ThisName==NAME_None);
			check(PrevNode!=NULL);
			TopNest->Allow = ALLOW_Break | ALLOW_Continue | (PrevAllow & (ALLOW_Cmd|ALLOW_Label|ALLOW_StateCmd|ALLOW_Return|ALLOW_Instance));
			break;

		case NEST_ForEach:
			check(ThisName==NAME_None);
			check(PrevNode!=NULL);
			TopNest->Allow = ALLOW_Iterator | ALLOW_Break | ALLOW_Continue | (PrevAllow & (ALLOW_Cmd|ALLOW_Label|ALLOW_Return|ALLOW_Instance));
			break;

		case NEST_FilterEditorOnly:
			check(ThisName==NAME_None);
			check(PrevNode!=NULL);
			TopNest->Allow = (PrevAllow & (ALLOW_Cmd|ALLOW_Label|ALLOW_Break|ALLOW_Continue|ALLOW_StateCmd|ALLOW_Return|ALLOW_Instance));
			break;

		default:
			ScriptErrorf(SCEL_Fatal,  TEXT("Internal error in PushNest, type %i"), (BYTE)NestType );
			break;
	}
}

/**
 * Decrease the nesting level and handle any errors that result.
 *
 * @param	NestType	nesting type of the current node
 * @param	Descr		text to use in error message if any errors are encountered
 */
void FScriptCompiler::PopNest( ENestType NestType, const TCHAR* Descr )
{
	if( TopNode && TopNode->Script.Num() > 65534 )
	{
		ScriptErrorf(SCEL_Fatal,  TEXT("Code space for %s overflowed by %i bytes"), *TopNode->GetName(), TopNode->Script.Num() - 65534 );
	}

	// Validate the nesting state.
	if( NestLevel <= 0 )
	{
		ScriptErrorf(SCEL_Unknown,  TEXT("Unexpected '%s' at global scope"), Descr, NestTypeName(NestType) );
	}
	else if( NestType==NEST_None )
	{
		NestType = TopNest->NestType;
	}
	else if( TopNest->NestType!=NestType )
	{
		ScriptErrorf(SCEL_Unknown,  TEXT("Unexpected end of %s in '%s' block"), Descr, NestTypeName(TopNest->NestType) );
	}

	// Pass-specific handling.
	if( Pass == PASS_Parse )
	{
		// Remember code position.
		if( NestType==NEST_State || NestType==NEST_Function )
		{
			TopNode->TextPos    = InputPos;
			TopNode->Line       = InputLine;

			if ( NestType == NEST_Function && !bReparsingClass && ClassData->ContainsDelegates() )
			{
				UFunction* TopFunction = CastChecked<UFunction>(TopNode);

				// now validate all delegate variables declared in the class
				TMap<FName, UFunction*> DelegateCache;
				FixupDelegateProperties(TopFunction, TopFunction->GetOwnerClass(), DelegateCache);
			}
		}

		// Todo - One drawback to doing this here is we don't get correct line numbers
		// for conflicting implementations.
		else if( NestType==NEST_Class )
		{
			UClass* TopClass = CastChecked<UClass>(TopNode);

			// first, fixup all delegate properties with the delegate types they're supposed to be bound to
			if ( !bReparsingClass && ClassData->ContainsDelegates() )
			{
				// now validate all delegate variables declared in the class
				TMap<FName,UFunction*> DelegateCache;
				FixupDelegateProperties(TopClass, TopClass, DelegateCache);
			}

			if ( !bReparsingClass )
			{
				// Iterate over all the interfaces we claim to implement
				for ( TArray<FImplementedInterface>::TIterator It(TopClass->Interfaces); It; ++It )
				{
					// And their super-classes
					for( UClass* Interface = It->Class; Interface; Interface = Interface->GetSuperClass() )
					{
						// If this interface is not a common ancestor, we must implement all functions it declares
						if ( !TopNode->IsChildOf( Interface ) )
						{
							// So iterate over all functions this interface declares
							for( TFieldIterator<UFunction> InterfaceFunction(Interface,FALSE); InterfaceFunction; ++InterfaceFunction )
							{
								UBOOL Implemented = FALSE;

								// And try to find one that matches
								for( TFieldIterator<UFunction> ClassFunction(TopNode); ClassFunction; ++ClassFunction )
								{
									if ( ClassFunction->GetFName() == InterfaceFunction->GetFName() )
									{
										if ( (InterfaceFunction->FunctionFlags&FUNC_Native) != 0 &&
											(ClassFunction->FunctionFlags&FUNC_Native) == 0 )
										{
											ScriptErrorf(SCEL_Restricted, TEXT("Implementation of function '%s' must be native to match declaration in interface '%s'"), *ClassFunction->GetName(), *Interface->GetName());
										}

										if ( (InterfaceFunction->FunctionFlags&FUNC_Event) != 0 &&
											(ClassFunction->FunctionFlags&FUNC_Event) == 0 )
										{
											ScriptErrorf(SCEL_Restricted, TEXT("Implementation of function '%s' must be declared as 'event' to match declaration in interface '%s'"), *ClassFunction->GetName(), *Interface->GetName());
										}

										if ( (InterfaceFunction->FunctionFlags&FUNC_Delegate) != 0 &&
											(ClassFunction->FunctionFlags&FUNC_Delegate) == 0 )
										{
											ScriptErrorf(SCEL_Restricted, TEXT("Implementation of function '%s' must be declared as 'delegate' to match declaration in interface '%s'"), *ClassFunction->GetName(), *Interface->GetName());
										}
										// Making sure all the parameters match up correctly
										Implemented = TRUE;

										if ( ClassFunction->NumParms != InterfaceFunction->NumParms )
										{
											ScriptErrorf(SCEL_Restricted,  TEXT("Implementation of function '%s' conflicts with interface '%s' - different number of parameters (%i/%i)"),
												*InterfaceFunction->GetName(), *Interface->GetName(), ClassFunction->NumParms, InterfaceFunction->NumParms );
										}

										INT Count = 0;
										for( TFieldIterator<UProperty> It1(*InterfaceFunction),It2(*ClassFunction); Count<ClassFunction->NumParms; ++It1,++It2,Count++ )
										{
											if( !FPropertyBase(*It1).MatchesType(FPropertyBase(*It2), 1) )
											{
												if( It1->PropertyFlags & CPF_ReturnParm )
												{
													ScriptErrorf(SCEL_Restricted,  TEXT("Implementation of function '%s' conflicts only by return type with interface '%s'"), *InterfaceFunction->GetName(), *Interface->GetName() );
												}
												else
												{
													ScriptErrorf(SCEL_Restricted,  TEXT("Implementation of function '%s' conflicts with interface '%s' - parameter %i '%s'"),
														*InterfaceFunction->GetName(), *Interface->GetName(), Count, *It1->GetName() );
												}

												break;
											}
										}
									}
								}

								if ( !Implemented )
								{
									ScriptErrorf(SCEL_Restricted,  TEXT("Missing required implementation of function '%s' from interface '%s'"), *InterfaceFunction->GetName(), *Interface->GetName() );
								}
							}
						}
					}
				}
			}
		}
		else if ( NestType == NEST_Interface )
		{
			if ( !bReparsingClass && ClassData->ContainsDelegates() )
			{
				TMap<FName,UFunction*> DelegateCache;
				FixupDelegateProperties(TopNode, ExactCast<UClass>(TopNode), DelegateCache);
			}
		}
		else
		{
			ScriptErrorf(SCEL_Class, TEXT("Bad first pass NestType %i"), (BYTE)NestType );
		}

		if ( !bReparsingClass )
		{
			FArchive DummyAr;
			TopNode->Link( DummyAr, 1 );
		}
	}
	else if ( Pass == PASS_Compile )
	{
		// If ending a state, process labels.
		if( NestType==NEST_State )
		{
			// Emit stop command.

			EmitDebugInfo(DI_PrevStack);
			Writer << EX_Stop;

			// Write all labels to code.
			if( TopNest->LabelList )
			{
				// Make sure the label table entries are aligned.
				while( (TopNode->Script.Num() & 3) != 3 )
					Writer << EX_Nothing;
				Writer << EX_LabelTable;

				// Remember code offset.
				UState* State = CastChecked<UState>( TopNode );
				State->LabelTableOffset = TopNode->Script.Num();

				// Write out all label entries.
				for( FLabelRecord* LabelRecord = TopNest->LabelList; LabelRecord; LabelRecord=LabelRecord->Next )
					Writer << *LabelRecord;

				// Write out empty record.
				FLabelEntry Entry(NAME_None,MAXWORD);
				Writer << Entry;
			}
		}
		else if( NestType==NEST_Function )
		{
			// Emit return with no value.
			UFunction* TopFunction = CastChecked<UFunction>( TopNode );
			if( !(TopFunction->FunctionFlags & FUNC_Native) )
			{
				//!! Debugger
				EmitDebugInfo(DI_ReturnNothing);
				Writer << EX_Return;
				UProperty* ReturnProp = TopFunction->GetReturnProperty();
				if (ReturnProp != NULL)
				{
					// write a failsafe opcode that makes sure the function returns zero if control reaches the end
					Writer << EX_ReturnNothing;
					Writer << ReturnProp;
				}
				else
				{
					Writer << EX_Nothing;
				}
				EmitDebugInfo(DI_PrevStack);

 				if( LocalProperties )
				{
					// Check all local properties for unreferenced or obsolete properties
					for( TFieldIterator<UProperty> It(TopNode); It; ++It )
					{
						UProperty *property = *It;
						if ( (property->PropertyFlags & CPF_Parm) == 0 )
						{
							FLocalProperty* CurLocal = LocalProperties->GetLocalProperty(property);
							INT RealInputLine = InputLine;

							if ( !CurLocal->bReferenced )
							{
								InputLine = LocalPropertyLineNumbers.FindRef(property);
								ScriptWarnf(SCWL_Level4, TEXT("'%s' : unreferenced local variable"), *CurLocal->property->GetName ());
							}
							else
							{
								if ( CurLocal->bUninitializedValue )
								{
									InputLine = CurLocal->ReferencedLine;
									ScriptWarnf(SCWL_Level4, TEXT("'%s' : local variable used before assigned a value"), *property->GetName());
								}
								else if ( !CurLocal->bValueAssigned )
								{
									InputLine = CurLocal->ReferencedLine;
									ScriptWarnf(SCWL_Level4, TEXT("'%s' : local variable never assigned a value"), *property->GetName());
								}

								if ( !CurLocal->bValueReferenced )
								{
									InputLine = CurLocal->AssignedLine;
									ScriptWarnf(SCWL_Level4, TEXT("'%s' : unused local variable"), *property->GetName());
								}
							}

							InputLine = RealInputLine;
						}

						// remove this local property from the line number map - it's no longer needed
						LocalPropertyLineNumbers.Remove(property);
					}

					// clear out the LocalProperties list
					while ( LocalProperties != NULL )
					{
						FLocalProperty* curLocal = LocalProperties;
						LocalProperties = curLocal->next;

						delete curLocal;
					}
				}

				if ((TopFunction->FunctionFlags & FUNC_Defined)
				 && (TopFunction->GetReturnProperty())
				 && (!(TopNest->NestFlags & NESTF_ReturnValueFound)))
				{
					ScriptWarnf(SCWL_Level4, TEXT("%s: Missing return value"), *TopFunction->GetName());
				}

				FFunctionData* FunctionMetaData = ClassData->FindFunctionData(TopFunction);
				if ( FunctionMetaData != NULL )
				{
					FunctionMetaData->ClearLocalPropertyData();
				}
			}
		}
		else if( NestType==NEST_Switch )
		{
			if( TopNest->Allow & ALLOW_Case )
			{
				// No default was specified, so emit end-of-case marker.
				EmitChainAddressValue(TopNest);
				Writer << EX_Case;
				CodeSkipSizeType W=MAXWORD; Writer << W;
			}

			// Here's the big end.
			TopNest->SetJumpTargetValue(FIXUP_SwitchEnd,TopNode->Script.Num());
		}
		else if( NestType==NEST_If )
		{
			if( MatchIdentifier(NAME_Else) )
			{
				// Send current code to the end of the if block.
				Writer << EX_Jump;
				EmitPlaceholderForJumpAddress( TopNest, FIXUP_IfEnd, NAME_None );

				// Update previous If's next-address.
				EmitChainAddressValue( TopNest );
				if( MatchIdentifier(NAME_If) )
				{
					// ElseIf.
					CheckAllow( TEXT("'Else If'"), ALLOW_ElseIf );

					// Jump to next evaluator if expression is false.
					EmitDebugInfo(DI_SimpleIf);
					Writer << EX_JumpIfNot;
					EmitPlaceholderForChainAddress( TopNest );

					// Compile boolean expr.
					RequireSymbol( TEXT("("), TEXT("'Else If'") );
					CompileExpr( FPropertyBase(CPT_Bool,CPRT_SimpleReference), TEXT("'Else If'") );
					RequireSymbol( TEXT(")"), TEXT("'Else If'") );

					// Handle statements.
					if( !MatchSymbol(TEXT("{")) )
					{
						CompileStatement();
						PopNest( NEST_If, TEXT("'ElseIf'") );
					}
				}
				else
				{
					// Else.
					CheckAllow( TEXT("'Else'"), ALLOW_ElseIf );

					// Prevent further ElseIfs.
					TopNest->Allow &= ~(ALLOW_ElseIf);

					// Handle statements.
					if( !MatchSymbol(TEXT("{")) )
					{
						CompileStatement();
						PopNest( NEST_If, TEXT("'Else'") );
					}
				}
				return;
			}
			else
			{
				// Update last link, if any:
				EmitChainAddressValue( TopNest );

				// Here's the big end.
				TopNest->SetJumpTargetValue( FIXUP_IfEnd, TopNode->Script.Num() );
			}
		}
		else if( NestType==NEST_For )
		{
			// Compile the incrementor expression here.
			TopNest->SetJumpTargetValue(FIXUP_ForInc,TopNode->Script.Num());
			FScriptLocation Retry;

			ReturnToLocation(TopNest->ForRetry,0,1);
				EmitDebugInfo(DI_ForInc);
				CompileAffector();
			ReturnToLocation(Retry,0,1);

			// Jump back to start of loop.
			Writer << EX_Jump;
			EmitPlaceholderForJumpAddress(TopNest,FIXUP_ForStart,NAME_None);

			// Here's the end of the loop.
			TopNest->SetJumpTargetValue(FIXUP_ForEnd,TopNode->Script.Num());
		}
		else if( NestType==NEST_ForEach )
		{
			// Perform next iteration.
			Writer << EX_IteratorNext;

			// Here's the end of the loop - set the values for the jump target placeholders for normal loop exits
			TopNest->SetJumpTargetValue( FIXUP_IteratorEnd, TopNode->Script.Num() );

			// this tells iterators to return
			Writer << EX_IteratorPop;

			// if we access None evaluating the iterator expression, the iterator code is never executed, so we need to skip everything, including the pop bytecode
			TopNest->SetJumpTargetValue( FIXUP_IteratorSkip, TopNode->Script.Num() );
		}
		else if( NestType==NEST_Loop )
		{
			TopNest->SetJumpTargetValue( FIXUP_LoopPostCond, TopNode->Script.Num() );
			// closing a do/until loop
			if ( MatchIdentifier(NAME_Until) )
			{
				// Jump back to start of loop.
				Writer << EX_JumpIfNot;
				EmitPlaceholderForJumpAddress( TopNest, FIXUP_LoopStart, NAME_None );

				// Compile boolean expression.
				RequireSymbol( TEXT("("), TEXT("'Until'") );
				CompileExpr( FPropertyBase(CPT_Bool,CPRT_AssignmentReference), TEXT("'Until'") );

				FScriptLocation EndLocation;
				RequireSymbol( TEXT(")"), TEXT("'Until'") );

				if( !MatchSymbol(TEXT(";")) )
				{
					// we want the error to display the correct line, so don't use RequireSymbol
					ReturnToLocation(EndLocation, FALSE, TRUE);
					ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/,  TEXT("Missing '%s' in %s"), TEXT(";"), TEXT("'Until'") );
				}

				// Here's the end of the loop.
				TopNest->SetJumpTargetValue( FIXUP_LoopEnd, TopNode->Script.Num() );
			}
			else if ( (TopNest->Allow&ALLOW_InWhile) == 0 )
			{
				if ( PeekIdentifier(NAME_While) )
				{
					// Not allowed here.
					ScriptErrorf(SCEL_Formatting,  TEXT("The loop syntax is do...until, not do...while") );
				}
				else
				{
					ScriptErrorf(SCEL_Formatting, TEXT("Do loops must end with 'until(condition)' statement"));
				}

				goto EmitLoopJump;
			}
			else
			{
EmitLoopJump:
				// Jump back to start of loop.
				Writer << EX_Jump;
				EmitPlaceholderForJumpAddress( TopNest, FIXUP_LoopStart, NAME_None );

				// Here's the end of the loop.
				TopNest->SetJumpTargetValue( FIXUP_LoopEnd, TopNode->Script.Num() );
			}
		}
		else if( NestType==NEST_FilterEditorOnly )
		{
			// Here's the big end.
			TopNest->SetJumpTargetValue( FIXUP_FilterEditorOnly, TopNode->Script.Num() );
		}

		// Perform all code fixups.
		ReplacePlaceholderValues(TopNest);
	}

	// Make sure there's no dangling chain.
	check(TopNest->ChainJumpPlaceholderLocation==INDEX_NONE);

	// Pop the nesting level.
	NestType = TopNest->NestType;
	NestLevel--;
	TopNest--;
	TopNode	= TopNest->Node;

	// Update allow-flags.
	if( NestType==NEST_Function )
	{
		// Don't allow variable/struct declarations after functions.
		TopNest->Allow &= ~(ALLOW_VarDecl | ALLOW_TypeDecl);
	}
	else if( NestType == NEST_State )
	{
		// Don't allow variable/struct declarations after states.
		TopNest->Allow &= ~(ALLOW_VarDecl | ALLOW_TypeDecl);
	}
}

//
// Find the highest-up nest info of a certain type.
// Used (for example) for associating Break statements with their Loops.
//
INT FScriptCompiler::FindNest( ENestType NestType )
{
	for( int i=NestLevel-1; i>0; i-- )
	{
		if( Nest[i].NestType == NestType )
		{
			return i;
		}
	}
	return -1;
}

/**
 * Replace all skip/jump address placeholders in the specified nest with the correct values.
 * 
 * @param	Nest	the nest that contains the placeholders that are ready to be fixed up
 */
void FScriptCompiler::ReplacePlaceholderValues( FNestInfo* Nest )
{
	// Iterate through the nest's placeholder list
	for( FJumpTargetPlaceholder* Fixup = Nest->JumpPlaceholderList; Fixup!=NULL; Fixup=Fixup->Next )
	{
		// special behavior for fixing up labels
		if( Fixup->Type == FIXUP_Label )
		{
			// Fixup a local label.
			FLabelRecord* LabelRecord;
			for( LabelRecord = Nest->LabelList; LabelRecord; LabelRecord=LabelRecord->Next )
			{
				if( LabelRecord->Name == Fixup->Name )
				{
					*(CodeSkipSizeType*)&TopNode->Script(Fixup->iCode) = LabelRecord->iCode;
					break;
				}
			}
			if( LabelRecord == NULL )
			{
				ScriptErrorf(SCEL_Unknown,  TEXT("Label '%s' not found in this block of code"), *Fixup->Name.ToString() );
			}
		}
		else
		{
			// Fixup a code structure address.
			if( Nest->JumpTargets[Fixup->Type] == MAXWORD )
			{
				ScriptErrorf(SCEL_Fatal,  TEXT("Internal fixup error %i"), (BYTE)Fixup->Type );
			}

			CodeSkipSizeType Target;

			// iteratorskip placeholder values are absolute indexes into the Script array; therefore, the skip-over value is the difference between
			// the placeholder location and the target address
			if ( Fixup->Type == FIXUP_IteratorSkip )
			{
				// we need to subtract 4 bytes from the difference between the location of the placeholder and the jump target location, as the code pointer will
				// be incremented by the skip-over value AFTER we've read the skip-over (CodeSkipSizeType), return value size (ScriptPointerType), and NULL property type (BYTE) from the bytestream
				Target = Nest->JumpTargets[Fixup->Type] - (Fixup->iCode + sizeof(CodeSkipSizeType) + sizeof(ScriptPointerType) + sizeof(BYTE));
			}
			else
			{
				Target = Nest->JumpTargets[Fixup->Type];
			}

			*(CodeSkipSizeType*)&TopNode->Script(Fixup->iCode) = Target;
		}
	}
}

ENestType FScriptCompiler::GetNestType( const UStruct* InNode ) const
{
	UClass* NodeClass = InNode->GetClass();
	if ( NodeClass == UClass::StaticClass() )
	{
		// determine if the Class is an interface or a class
		if (NodeClass->HasAnyClassFlags(CLASS_Interface))
		{
			return NEST_Interface;
		}
		else
		{
			return NEST_Class;
		}
	}

	if ( NodeClass == UState::StaticClass() )
	{
		return NEST_State;
	}

	if ( NodeClass == UFunction::StaticClass() )
	{
		return NEST_Function;
	}

	return NEST_None;
}

/**
 * Binds all delegate properties declared in ValidationScope the delegate functions specified in the variable declaration, verifying that the function is a valid delegate
 * within the current scope.  This must be done once the entire class has been parsed because instance delegate properties must be declared before the delegate declaration itself.
 *
 * @todo: this function will no longer be required once the post-parse fixup phase is added (TTPRO #13256)
 *
 * @param	ValidationScope		the scope to validate delegate properties for
 * @param	OwnerClass			the class currently being compiled.
 * @param	DelegateCache		cached map of delegates that have already been found; used for faster lookup.
 */
void FScriptCompiler::FixupDelegateProperties( UStruct* ValidationScope, UClass* OwnerClass, TMap<FName, UFunction*>& DelegateCache )
{
	check(ValidationScope);
	check(OwnerClass);

	for ( UField* Field = ValidationScope->Children; Field; Field = Field->Next )
	{
		UProperty* Property = Cast<UProperty>(Field,CLASS_IsAUProperty);
		if ( Property != NULL )
		{
			UDelegateProperty* DelegateProperty = Cast<UDelegateProperty>(Property);
			if ( DelegateProperty == NULL )
			{
				// if this is an array property, see if the array's type is a delegate
				UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
				if ( ArrayProp != NULL )
				{
					DelegateProperty = Cast<UDelegateProperty>(ArrayProp->Inner);
				}
			}
			if ( DelegateProperty != NULL )
			{
				const UBOOL bDelegateVariable = (DelegateProperty->Function == NULL);
				if ( bDelegateVariable )
				{
					// this UDelegateProperty corresponds to an actual delegate variable (i.e. delegate<SomeDelegate> Foo); we need to lookup the token data for
					// this property and verify that the delegate property's "type" is an actual delegate function
					FTokenData* DelegatePropertyToken = ClassData->FindTokenData(Property);
					check(DelegatePropertyToken);

					// attempt to find the delegate function in the map of functions we've already found
					UFunction* SourceDelegateFunction = DelegateCache.FindRef(DelegatePropertyToken->Token.DelegateName);
					if ( SourceDelegateFunction == NULL )
					{
						FString NameOfDelegateFunction = DelegatePropertyToken->Token.DelegateName.ToString();
						if ( NameOfDelegateFunction.InStr(TEXT(".")) == INDEX_NONE )
						{
							// an unqualified delegate function name - search for a delegate function by this name within the current scope
							SourceDelegateFunction = Cast<UFunction>(FindField(OwnerClass, *NameOfDelegateFunction, TRUE, UFunction::StaticClass(), NULL, NULL), CLASS_IsAUFunction);
							if ( SourceDelegateFunction == NULL )
							{
								// convert this into a fully qualified path name for the error message.
								NameOfDelegateFunction = OwnerClass->GetName() + TEXT(".") + NameOfDelegateFunction;
							}
							else
							{
								// convert this into a fully qualified path name for the error message.
								NameOfDelegateFunction = SourceDelegateFunction->GetOwnerClass()->GetName() + TEXT(".") + NameOfDelegateFunction;
							}
						}
						else
						{
							FString ClassName, DelegateName;
							NameOfDelegateFunction.Split(TEXT("."), &ClassName, &DelegateName);

							// verify that we got a valid string for the class name
							if ( ClassName.Len() == 0 )
							{
								InputLine = DelegatePropertyToken->Token.StartLine;
								InputPos = DelegatePropertyToken->Token.StartPos;
								ScriptErrorf(SCEL_Unknown, TEXT("Invalid scope specified in delegate property function reference: '%s'"), *NameOfDelegateFunction);
							}

							// verify that we got a valid string for the name of the function
							if ( DelegateName.Len() == 0 )
							{
								InputLine = DelegatePropertyToken->Token.StartLine;
								InputPos = DelegatePropertyToken->Token.StartPos;
								ScriptErrorf(SCEL_Unknown, TEXT("Invalid delegate name specified in delegate property function reference '%s'"), *NameOfDelegateFunction);
							}

							// bail if the specified class cannot be found
							UClass* DelegateOwnerClass = FindClass(*ClassName);//FindObject<UClass>(ANY_PACKAGE, *ClassName);
							if ( DelegateOwnerClass == NULL )
							{
								InputLine = DelegatePropertyToken->Token.StartLine;
								InputPos = DelegatePropertyToken->Token.StartPos;
								ScriptErrorf(SCEL_Unknown,  TEXT("Class '%s' not found."), *ClassName );
							}
							
							// make sure that the class that contains the delegate can be referenced here
							CheckInScope(DelegateOwnerClass);
							SourceDelegateFunction = Cast<UFunction>(FindField(DelegateOwnerClass, *DelegateName, FALSE, UFunction::StaticClass(), NULL, NULL), CLASS_IsAUFunction);
						}

						if ( SourceDelegateFunction == NULL )
						{
							InputLine = DelegatePropertyToken->Token.StartLine;
							InputPos = DelegatePropertyToken->Token.StartPos;
							ScriptErrorf(SCEL_Unknown, TEXT("Failed to find delegate function '%s'"), *NameOfDelegateFunction);
						}
						else if ( (SourceDelegateFunction->FunctionFlags&FUNC_Delegate) == 0 )
						{
							InputLine = DelegatePropertyToken->Token.StartLine;
							InputPos = DelegatePropertyToken->Token.StartPos;
							ScriptErrorf(SCEL_Unknown, TEXT("Only delegate functions can be used as the type for a delegate property; '%s' is not a delegate."), *NameOfDelegateFunction);
						}
					}

					// successfully found the delegate function that this delegate property corresponds to

					// save this into the delegate cache for faster lookup later
					DelegateCache.Set(DelegatePropertyToken->Token.DelegateName, SourceDelegateFunction);

					// bind it to the delegate property
					DelegateProperty->Function = DelegateProperty->SourceDelegate = DelegatePropertyToken->Token.Function = SourceDelegateFunction;
				}
			}
		}
		else
		{
			// if this is a state, function, or script struct, it might have its own delegate properties which need to be validated
			UStruct* InternalStruct = Cast<UStruct>(Field);
			if ( InternalStruct != NULL )
			{
				FixupDelegateProperties(InternalStruct, OwnerClass, DelegateCache);
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	Compiler directives.
-----------------------------------------------------------------------------*/

//
// Process a compiler directive.
//
void FScriptCompiler::CompileDirective()
{
	FToken Directive;

	if( !GetIdentifier(Directive) )
	{
		ScriptErrorf(SCEL_Fatal,  TEXT("Missing compiler directive after '#'") );
	}
	else if( Directive.Matches(TEXT("Error")) )
	{
		ScriptErrorf(SCEL_Fatal,  TEXT("#Error directive encountered") );
	}
	else if( Directive.Matches(TEXT("linenumber")) )
	{
		FToken Number;
		if (!GetToken(Number) || (Number.TokenType != TOKEN_Const) || (Number.Type != CPT_Int))
		{
			ScriptErrorf(SCEL_Fatal, TEXT("Missing line number in line number directive"));
		}

		INT newInputLine;
		if ( Number.GetConstInt(newInputLine) )
		{
			InputLine = newInputLine;
		}
	}
	else
	{
		ScriptErrorf(SCEL_Fatal,  TEXT("Unrecognized compiler directive %s"), Directive.Identifier );
	}

	// Skip to end of line.
	TCHAR c;
	while( !IsEOL( c=GetChar() ) );
	if( c==0 )
	{
		UngetChar();
	}
}

/*-----------------------------------------------------------------------------
	Variable declaration parser.
-----------------------------------------------------------------------------*/

/**
 * Parses a variable or return value declaration and determines the variable type and property flags.
 *
 * @param		Scope				struct to create the property in
 * @param		VarProperty			will be filled in with type and property flag data for the property declaration that was parsed
 * @param		ObjectFlags			will contain the object flags that should be assigned to all UProperties which are part of this declaration (will not be determined
 *									until GetVarNameAndDim is called for this declaration).
 * @param		Disallow			contains a mask of variable modifiers that are disallowed in this context
 * @param		Thing				used for compiler errors to provide more information about the type of parsing that was occurring
 * @param		OuterPropertyType	only specified when compiling the inner properties for arrays or maps.  corresponds to the FToken for the outer property declaration.
 *
 * @return		
 */
UBOOL FScriptCompiler::GetVarType
(
	UStruct*		Scope,
	FPropertyBase&	VarProperty,
	EObjectFlags&	ObjectFlags,
	QWORD			Disallow,
	const TCHAR*	Thing,
	FToken*			OuterPropertyType/*=NULL*/
)
{
	check(Scope);
	FPropertyBase TypeDefType(CPT_None);
	UClass* TempClass;
	UBOOL IsVariable = FALSE;

	// Get flags.
	ObjectFlags = RF_Public;
	QWORD Flags=0;
	DWORD ExportFlags = PROPEXPORT_Public;
	for( ; ; )
	{
		FToken Specifier;
		GetToken(Specifier);
		if( Specifier.Matches(NAME_Const) )
		{
			Flags      |= CPF_Const;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_Config) )
		{
			Flags      |= CPF_Config;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_GlobalConfig) )
		{
			Flags      |= CPF_GlobalConfig | CPF_Config;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_Localized) )
		{
			Flags      |= CPF_Localized | CPF_Const;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_Private) )
		{
			ParsePropertyExportText(ExportFlags, TEXT("private"));

			if (ObjectFlags & RF_Protected)
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Property cannot be marked both 'private' and 'protected'"));
			}
			ObjectFlags &= ~RF_Public;
			ObjectFlags &= ~RF_Protected;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_Protected) )
		{
			ParsePropertyExportText(ExportFlags, TEXT("protected"));

			if (!(ObjectFlags & RF_Public))
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Property cannot be marked both 'private' and 'protected'"));
			}
			ObjectFlags |= RF_Public;
			ObjectFlags |= RF_Protected;
			IsVariable  = TRUE;
		}
		else if (Specifier.Matches(NAME_PrivateWrite))
		{
			ParsePropertyExportText(ExportFlags, TEXT("privatewrite"));

			if (!(ObjectFlags & RF_Public))
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Property cannot be marked both 'private' and 'privatewrite'"));
			}
			else if (ObjectFlags & RF_Protected)
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Property cannot be marked both 'protected' and 'privatewrite'"));
			}
			else if (Flags & CPF_ProtectedWrite)
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Property cannot be marked both 'privatewrite' and 'protectedwrite'"));
			}
			Flags |= CPF_PrivateWrite;
			IsVariable = TRUE;
		}
		else if (Specifier.Matches(NAME_ProtectedWrite))
		{
			ParsePropertyExportText(ExportFlags, TEXT("protectedwrite"));

			if (ObjectFlags & RF_Protected)
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Property cannot be marked both 'protected' and 'protectedwrite'"));
			}
			else if (Flags & CPF_PrivateWrite)
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Property cannot be marked both 'privatewrite' and 'protectedwrite'"));
			}
			Flags |= CPF_ProtectedWrite;
			IsVariable = TRUE;
		}
		else if( Specifier.Matches(NAME_Public) )
		{
			ParsePropertyExportText(ExportFlags, TEXT("public"));

			ObjectFlags |= RF_Public;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_EditConst) )
		{
			Flags      |= CPF_EditConst;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_EditHide) )
		{
			Flags      |= CPF_EditHide;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_EditTextBox) )
		{
			Flags      |= CPF_EditTextBox;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_Input) )
		{
			Flags      |= CPF_Input;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_Transient) )
		{
			Flags      |= CPF_Transient;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_Native) )
		{
			Flags      |= CPF_Native;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_NoExport) )
		{
			Flags      |= CPF_NoExport;
			IsVariable  = TRUE;
		}
		else if ( Specifier.Matches(NAME_DuplicateTransient) )
		{
			Flags      |= CPF_DuplicateTransient;
			IsVariable	= TRUE;
		}
		else if ( Specifier.Matches(NAME_NoImport) )
		{
			Flags      |= CPF_NoImport;
			IsVariable	= TRUE;
		}
		else if( Specifier.Matches(NAME_Out) )
		{
			Flags      |= CPF_OutParm;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_Export) )
		{
			Flags      |= CPF_ExportObject;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_EditInline) )
		{
			Flags      |= CPF_EditInline;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_EditInlineUse) )
		{
			Flags      |= CPF_EditInline | CPF_EditInlineUse;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_NoClear) )
		{
			Flags      |= CPF_NoClear;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_EditFixedSize) )
		{
			Flags      |= CPF_EditFixedSize;
			IsVariable  = TRUE;
		}
		else if (Specifier.Matches(NAME_RepNotify) )
		{
			Flags	   |= CPF_RepNotify;
			IsVariable = TRUE;
		}
		else if (Specifier.Matches(NAME_RepRetry))
		{
			Flags |= CPF_RepRetry;
			IsVariable = TRUE;
		}
		else if (Specifier.Matches(NAME_Interp))
		{
			Flags |= CPF_Edit;
			Flags |= CPF_Interp;
			IsVariable = TRUE;
		}
		else if( Specifier.Matches(NAME_NonTransactional) )
		{
			Flags |= CPF_NonTransactional;
			IsVariable = TRUE;
		}
		else if( Specifier.Matches(NAME_Deprecated) )
		{
			Flags      |= CPF_Deprecated;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_Skip) )
		{
			Flags     |= CPF_SkipParm;
			IsVariable = TRUE;
		}
		else if( Specifier.Matches(NAME_Coerce) )
		{
			Flags     |= CPF_CoerceParm;
			IsVariable = TRUE;
		}
		else if( Specifier.Matches(NAME_Optional) )
		{
			Flags     |= CPF_OptionalParm;
			IsVariable = TRUE;
		}
		else if ( Specifier.Matches(NAME_Init) )
		{
			Flags |= CPF_AlwaysInit;
			IsVariable = TRUE;
		}
		else if ( Specifier.Matches(NAME_Instanced) )
		{
			Flags |= CPF_EditInline | CPF_ExportObject;
			IsVariable = TRUE;
		}
		else if ( Specifier.Matches(NAME_DataBinding) )
		{
			Flags |= CPF_DataBinding;
			IsVariable = TRUE;
		}
		else if( Specifier.Matches(NAME_EditorOnly) )
		{
			Flags      |= CPF_EditorOnly;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_NotForConsole) )
		{
			Flags      |= CPF_NotForConsole;
			IsVariable  = TRUE;
		}
		else if ( Specifier.Matches(NAME_Archetype) )
		{
			Flags      |= CPF_ArchetypeProperty;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_SerializeText) )
		{
			Flags      |= CPF_SerializeText;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_CrossLevel) )
		{
			ScriptErrorf(SCEL_Unknown, TEXT("CrossLevel properties must be either 'crosslevelpassive' or 'crosslevelactive'"));
		}
		else if( Specifier.Matches(NAME_CrossLevelActive) )
		{
			Flags      |= CPF_CrossLevelActive;
			IsVariable  = TRUE;
		}
		else if( Specifier.Matches(NAME_CrossLevelPassive) )
		{
			Flags      |= CPF_CrossLevelPassive;
			IsVariable  = TRUE;
		}
		else
		{
			UngetToken(Specifier);
			break;
		}
	}

	// Get variable type.
	FToken VarType;
	UField* Field;
	if( !GetIdentifier(VarType,1) )
	{
		if( !Thing )
		{
			return 0;
		}
		ScriptErrorf(SCEL_NestLevel,  TEXT("%s: Missing variable type"), Thing );
	}
	else if( VarType.Matches(NAME_Enum) )
	{
		// Store off any leading comment.  This is necessary because we don't want comments
		// that exist in the enum block to be assigned as tooltip metadata to the variable.
		const FString StoredComment( PrevComment );
		const UBOOL bStoredPrevCommentFormatted = bPrevCommentFormatted;
		ClearComment();

		// Compile an Enum definition and variable declaration here.
		UBOOL NeedsSemicolon = TRUE;
		VarProperty = FPropertyBase( CompileEnum(Scope, NeedsSemicolon) );

		// Restore the leading comment.
		PrevComment = StoredComment;
		bPrevCommentFormatted = bStoredPrevCommentFormatted;
	}
	else if( VarType.Matches(NAME_Struct) )
	{
		// Store off any leading comment.  This is necessary because we don't want comments
		// that exist in the enum block to be assigned as tooltip metadata to the variable.
		const FString StoredComment( PrevComment );
		const UBOOL bStoredPrevCommentFormatted = bPrevCommentFormatted;
		ClearComment();

		// Compile a Struct definition and variable declaration here.
		UBOOL NeedsSemicolon = TRUE;
		VarProperty = FPropertyBase( CompileStruct(Scope, NeedsSemicolon) );

		// Restore the leading comment.
		PrevComment = StoredComment;
		bPrevCommentFormatted = bStoredPrevCommentFormatted;
	}
	else if( VarType.Matches(NAME_Byte) )
	{
		// Intrinsic Byte type.
		VarProperty = FPropertyBase(CPT_Byte);
	}
	else if( VarType.Matches(NAME_Int) )
	{
		// Intrinsic Int type.
		VarProperty = FPropertyBase(CPT_Int);
	}
	else if( VarType.Matches(NAME_Bool) )
	{
		// Intrinsic Bool type.
		VarProperty = FPropertyBase(CPT_Bool);
	}
	else if( VarType.Matches(NAME_Float) )
	{
		// Intrinsic Real type
		VarProperty = FPropertyBase(CPT_Float);
	}
	else if( VarType.Matches(NAME_Name) )
	{
		// Intrinsic Name type.
		VarProperty = FPropertyBase(CPT_Name);
	}
	else if( VarType.Matches(NAME_Array) )
	{
		RequireSymbol( TEXT("<"), TEXT("'array'") );

		// GetVarType() clears the property flags of the array var, so use dummy 
		// flags when getting the inner property
		EObjectFlags InnerFlags;
		QWORD OriginalVarTypeFlags = VarType.PropertyFlags;
		VarType.PropertyFlags |= Flags;

		GetVarType( Scope, VarProperty, InnerFlags, Disallow, TEXT("'array'"), &VarType );
		if( VarProperty.ArrayDim==0 )
		{
			ScriptErrorf(SCEL_NestLevel,  TEXT("Arrays within arrays not supported.") );
		}
		else if ( VarProperty.ArrayDim < 0 )
		{
			ScriptErrorf(SCEL_NestLevel, TEXT("Maps within arrays not supported."));
		}
		VarType.PropertyFlags = OriginalVarTypeFlags;
		VarProperty.ArrayDim = 0;
		RequireSymbol( TEXT(">"), TEXT("'array'") );
	}
	else if( VarType.Matches(NAME_Map) )
	{
		if ( (Flags&CPF_Native) == 0 && !Class->HasAnyClassFlags(CLASS_Transient) )
		{
			ScriptErrorf(SCEL_Restricted, TEXT("You must mark this property 'native'.  Non-native maps are not currently supported!"));
			Flags |= CPF_Native;
		}

		QWORD OriginalVarTypeFlags = VarType.PropertyFlags;
		VarType.PropertyFlags |= Flags;

#if 0  // this code requires that each type in the map declaration is a valid type
		RequireSymbol( TEXT("<"), TEXT("'map'") );

		EObjectFlags InnerFlags = RF_Public;
		GetVarType( Scope, VarProperty, InnerFlags, Disallow, TEXT("'map'"), &VarType );

		RequireSymbol( TEXT(","), TEXT("'map'") );

		InnerFlags = RF_Public;
		VarType.PropertyFlags = OriginalVarTypeFlags | Flags;
		GetVarType( Scope, VarProperty, InnerFlags, Disallow, TEXT("'map'"), &VarType );

		RequireSymbol( TEXT(">"), TEXT("'map'") );
#else
		VarProperty = FPropertyBase(CPT_Map);
		FScriptLocation DeclarationPosition;
		if ( !ParsePropertyExportText(VarProperty, TEXT("map")) && !bReparsingClass )
		{
			ScriptErrorf(SCEL_Unknown/*SCEL_Formatting*/, TEXT("'map': No types specified"));
		}
		else if ( !Class->HasAnyClassFlags(CLASS_Native) && !bReparsingClass )
		{
			///FScriptLocation ContinuePosition;
			ReturnToLocation(DeclarationPosition, FALSE, TRUE);
			ScriptErrorf(SCEL_Unknown/*SCEL_Formatting*/, TEXT("Export text should only be used in native classes"));

			// continue on...
			///ReturnToLocation(ContinuePosition, FALSE, TRUE);
		}
#endif
		VarType.PropertyFlags = OriginalVarTypeFlags;
		VarProperty.ArrayDim = -1;
	}
	else if( VarType.Matches(NAME_String) )
	{
		if( MatchSymbol(TEXT("[")) )
		{
			INT StringSize=0;
			if( !GetConstInt(StringSize) )
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Formatting*/,  TEXT("%s: Missing string size"),Thing?Thing:TEXT("Declaration") );
			}
			if( !MatchSymbol(TEXT("]")) )
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Formatting*/,  TEXT("%s: Missing ']'"), Thing ? Thing : TEXT("Declaration") );
			}
			ScriptWarnf(SCWL_Level4, TEXT("String sizes are now obsolete; all strings are dynamically sized") );
		}
		VarProperty = FPropertyBase(CPT_String);
	}
	else if ( VarType.Matches(NAME_Delegate) )
	{
		RequireSymbol(TEXT("<"),TEXT("'delegate'"));

		// this will contain the name of the delegate function specified between the brackets; handles fully qualified path names
		FString NameOfDelegateFunction;
		do
		{
			FToken NameToken;
			if( !GetIdentifier(NameToken,1) )
			{
				ScriptErrorf(SCEL_Unknown, TEXT("%s: Failed to read delegate name"),Thing!=NULL?Thing:TEXT("Declaration"));
			}

			if ( NameOfDelegateFunction.Len() > 0 )
			{
				NameOfDelegateFunction += TEXT(".");
			}

			NameOfDelegateFunction += NameToken.Identifier;
		}
		while( MatchSymbol(TEXT(".")) );

		RequireSymbol(TEXT(">"),TEXT("'delegate'"));
		VarProperty = FPropertyBase(CPT_Delegate);
		VarProperty.DelegateName = *NameOfDelegateFunction;
	}
	else if ( (TempClass = FindClass(VarType.Identifier))/*FindObject<UClass>(ANY_PACKAGE, VarType.Identifier))*/ != NULL )
	{
		// An object reference.
		CheckInScope( TempClass );

		VarProperty = FPropertyBase( TempClass );
		if( TempClass==UClass::StaticClass() )
		{
			if( MatchSymbol(TEXT("<")) )
			{
				FToken Limitor;
				if( !GetIdentifier(Limitor) )
				{
					ScriptErrorf(SCEL_Unknown,  TEXT("'class': Missing class limitor") );
				}
				VarProperty.MetaClass = FindClass(Limitor.Identifier);//FindObject<UClass>( ANY_PACKAGE, Limitor.Identifier );
				if( !VarProperty.MetaClass )
				{
					ScriptErrorf(SCEL_Unknown,  TEXT("'class': Limitor '%s' is not a class name"), Limitor.Identifier );
				}
				RequireSymbol( TEXT(">"), TEXT("'class limitor'") );
			}
			else
			{
				VarProperty.MetaClass = UObject::StaticClass();
			}
		}

		// allow enums/structs from classes outside the current hierarchy
		if( MatchSymbol(TEXT(".")) )
		{
			FToken ElemIdent;
			if( !GetIdentifier(ElemIdent) )
			{
				ScriptErrorf(SCEL_Unknown,  TEXT("'%s': Missing class member type after '.'"), VarType.Identifier );
			}
			if( (Field=FindField( TempClass, ElemIdent.Identifier ))!=NULL )
			{
				if( Field->GetClass()==UEnum::StaticClass() )
				{
					VarProperty = FPropertyBase( CastChecked<UEnum>(Field) );
				}
				else if( Field->GetClass()==UScriptStruct::StaticClass() )
				{
					UScriptStruct*	Struct = CastChecked<UScriptStruct>(Field);
					VarProperty = FPropertyBase( Struct );
					if((Struct->StructFlags & STRUCT_HasComponents) && !(Disallow & CPF_Component))
					{
						VarProperty.PropertyFlags |= CPF_Component;
					}
				}
				else
				{
					ScriptErrorf(SCEL_Unknown,  TEXT("Unrecognized type '%s' within '%s'"),
						ElemIdent.Identifier, VarType.Identifier );
				}
			}
			else
			{
				ScriptErrorf(SCEL_Unknown,  TEXT("Unrecognized type '%s' within '%s'"),
						ElemIdent.Identifier, VarType.Identifier );
			}
		}
		if( TempClass->IsChildOf(UComponent::StaticClass()) )
		{
			Flags |= ((CPF_Component|CPF_ExportObject) & (~Disallow));
		}
		else if( Flags & CPF_Component )
		{
			ScriptErrorf(SCEL_Unknown,  TEXT("Component modifier not allowed for non-UComponent object references") );
		}
		else if ( TempClass->HasAnyClassFlags(CLASS_Interface|CLASS_Native) )
		{
			if ( Class->HasAnyClassFlags(CLASS_Native) )
			{
			}
		}
	}
	else if( (Field = FindObject<UEnum>( ANY_PACKAGE, VarType.Identifier ))!=NULL )
	{
		// In-scope enumeration or struct.
		VarProperty = FPropertyBase((UEnum*)Field);
	}
    else if( (Field = FindObject<UScriptStruct>( ANY_PACKAGE, VarType.Identifier ))!=NULL )
	{
		UScriptStruct*	Struct = (UScriptStruct*)Field;
		VarProperty = FPropertyBase( Struct );
		if((Struct->StructFlags & STRUCT_HasComponents) && !(Disallow & CPF_Component))
		{
			VarProperty.PropertyFlags |= CPF_Component;
		}
	}
	else if( !Thing )
	{
		// Not recognized.
		UngetToken( VarType );
		return 0;
	}
	else
	{
		ScriptErrorf(SCEL_Unknown,  TEXT("Unrecognized type '%s'"), VarType.Identifier );
	}

	if ( !bReparsingClass )
	{
		VarProperty.PropertyExportFlags = ExportFlags;

		// Set FPropertyBase info.
		VarProperty.PropertyFlags |= Flags;

		// Perform some more specific validation on the property flags
		if ( VarProperty.IsObject() && (VarProperty.PropertyFlags&CPF_Config) != 0 )
		{
			ScriptErrorf(SCEL_Restricted, TEXT("Not allowed to use 'config' with object variables"));
		}

		if ( (VarProperty.PropertyFlags&CPF_Native) != 0 && !Class->HasAnyClassFlags(CLASS_Native) )
		{
			ScriptErrorf(SCEL_Restricted, TEXT("Not allowed to declare variables 'native' in non-native classes"));
		}

		if ((VarProperty.PropertyFlags & CPF_RepRetry) && VarProperty.Type != CPT_Struct)
		{
			ScriptErrorf(SCEL_Restricted, TEXT("'RepRetry' is only allowed on struct properties"));
		}

		// check to see if we have a UClassProperty using a deprecated class
		if ( VarProperty.MetaClass != NULL && VarProperty.MetaClass->HasAnyClassFlags(CLASS_Deprecated) && !(VarProperty.PropertyFlags & CPF_Deprecated) &&
			(OuterPropertyType == NULL || !(OuterPropertyType->PropertyFlags & CPF_Deprecated)) )
		{
			ScriptWarnf(SCWL_Level4, TEXT("Property is using a deprecated class: %s.  Property should be marked deprecated as well."), *VarProperty.MetaClass->GetPathName());
		}

		// check to see if we have a UObjectProperty using a deprecated class.
		// PropertyClass is part of a union, so only check PropertyClass if this token represents an object property
		if ( VarProperty.Type == CPT_ObjectReference && VarProperty.PropertyClass != NULL
		&&	VarProperty.PropertyClass->HasAnyClassFlags(CLASS_Deprecated)	// and the object class being used has been deprecated
		&& (VarProperty.PropertyFlags&CPF_Deprecated) == 0					// and this property isn't marked deprecated as well
		&& (OuterPropertyType == NULL || !(OuterPropertyType->PropertyFlags & CPF_Deprecated)) ) // and this property isn't in an array that was marked deprecated either
		{
			ScriptWarnf(SCWL_Level4, TEXT("Property is using a deprecated class: %s.  Property should be marked deprecated as well."), *VarProperty.PropertyClass->GetPathName());
		}

		if (Cast<UClass>(Scope) == NULL)
		{
			/*
			if (!(ObjectFlags & RF_Public))
			{
				ScriptErrorf(SCEL_Restricted, TEXT("'Private' is only allowed on class member variables"));
			}
			else */if (ObjectFlags & RF_Protected)
			{
				ScriptErrorf(SCEL_Restricted, TEXT("'Protected' is only allowed on class member variables"));
			}
			else if (VarProperty.PropertyFlags & CPF_PrivateWrite)
			{
				ScriptErrorf(SCEL_Restricted, TEXT("'PrivateWrite' is only allowed on class member variables"));
			}
			else if (VarProperty.PropertyFlags & CPF_ProtectedWrite)
			{
				ScriptErrorf(SCEL_Restricted, TEXT("'ProtectedWrite' is only allowed on class member variables"));
			}
			else if (VarProperty.PropertyFlags & CPF_DuplicateTransient)
			{
				ScriptErrorf(SCEL_Restricted, TEXT("'DuplicateTransient' is only allowed on class member variables"));
			}
		}

		if( ( VarProperty.PropertyFlags & CPF_SerializeText ) == CPF_SerializeText &&
			( VarProperty.PropertyFlags & CPF_Native ) != CPF_Native )
		{
			ScriptWarnf( SCWL_Level4, TEXT( "Variable is declared with 'SerializeText' but this keyword only has meaning for Native variables." ) );
		}
	}

	// Make sure the overrides are allowed here.
	if( VarProperty.PropertyFlags & Disallow )
	{
		ScriptErrorf(SCEL_Restricted,  TEXT("Specified type modifiers not allowed here") );
	}

	return 1;
}

/**
 * Groups properties together by finding the appropriate spot to append them.
 *
 * @param	Scope			Scope this property is declared in
 * @param	Previous		Current insertion point
 * @param	PropertyFlags	Property flags of property to group
 * @param	ClassName		Class name of the property to group (e.g. UByteProperty::StaticClass()->GetFName())
 *
 * @return returns the new insertion point
 */
static UProperty* GroupProperty( UStruct* Scope, UProperty* Previous, QWORD PropertyFlags, FName ClassName )
{
	UBOOL bMovable = TRUE;

	// Can't move properties in a class that is defined as noexport. Additionally we only group properties
	// in UClasses as UFunction properties can't be grouped and "structs" are usually mirrored manually so
	// we rather not group there either.
	UClass* Class = Cast<UClass>(Scope);
	if( Class == NULL || Class->HasAnyClassFlags(CLASS_NoExport) )
	{
		bMovable = FALSE;
	}

	// Can't safely move variables that are marked as noexport.
	if( (PropertyFlags&CPF_NoExport) != 0 )
	{
		bMovable = FALSE;
	}

	// Iterate over all properties and find the proper insertion point.
	if( bMovable )
	{
		for ( TFieldIterator<UProperty> It(Scope,FALSE); It; ++It )
		{
			UProperty* Property = *It;

			// Make sure we don't insert anything between noexport variables.
			if( Property->HasAnyPropertyFlags(CPF_NoExport) && Property->GetName().Left(8) != TEXT("Vftable_") )
			{
				break;
			}

			// Make sure the type and transient behaviour are identical.
			UBOOL	SamePropertyType	= Property->GetClass()->GetFName() == ClassName;
			UBOOL	SameGroup			= TRUE; // really no reason for this now... (Property->PropertyFlags & CPF_Transient) == (PropertyFlags & CPF_Transient);

			if( SamePropertyType && SameGroup )
			{
				Previous = Property;
			}
		}
	}

	return Previous;
}

/**
 * If the property has already been seen during compilation, then return add. If not,
 * then return replace so that INI files don't mess with header exporting
 *
 * @param PropertyName the string token for the property
 *
 * @return FNAME_Replace or FNAME_Add
 */
EFindName FScriptCompiler::GetFindFlagForPropertyName(const TCHAR* PropertyName)
{
	FString PropertyStr(PropertyName);
	FString UpperPropertyStr = PropertyStr.ToUpper();
	// See if it's in the list already
	if (PreviousNames.Find(UpperPropertyStr))
	{
		return FNAME_Add;
	}
	// Add it to the list for future look ups
	PreviousNames.Set(UpperPropertyStr,1);
	// Check for a mismatch between the INI file and the config property name
	FName CurrentText(PropertyName,FNAME_Find);
	if (CurrentText != NAME_None &&
		appStrcmp(PropertyName,*CurrentText.ToString()) != 0)
	{
		ScriptWarnf(SCWL_Level4,
			TEXT("INI file contains an incorrect case for (%s) should be (%s)"),
			*CurrentText.ToString(),
			PropertyName);
	}
	return FNAME_Replace;
}

/**
 * Parses a variable name declaration and creates a new UProperty object
 *
 * @param	Scope			struct to create the property in
 * @param	VarProperty		type and propertyflag info for the new property
 * @param	ObjectFlags		flags to pass on to the new property
 * @param	NoArrays		TRUE if static arrays are disallowed
 * @param	IsFunction		TRUE if the property is a function parameter or return value
 * @param	HardcodedName	name to assign to the new UProperty
 * @param	HintText		text to use in error message if error is encountered
 * @param	Category		editor category to place this property in
 * @param	Skip			TRUE if we're not supposed to actually create the UProperty object
 *
 * @return	a pointer to the new UProperty
 */
UProperty* FScriptCompiler::GetVarNameAndDim
(
	UStruct*		Scope,
	FPropertyBase&	VarProperty,
	EObjectFlags	ObjectFlags,
	UBOOL			NoArrays,
	UBOOL			IsFunction,
	const TCHAR*	HardcodedName,
	const TCHAR*	HintText,
	FName			Category,
	UBOOL			Skip
)
{
	check(Scope);

	UFunction* FunctionScope = Cast<UFunction>(Scope);

	// Get variable name.
	FToken VarToken = VarProperty;
	if( HardcodedName )
	{
		// Hard-coded variable name, such as with return value.
		VarToken.TokenType = TOKEN_Identifier;
		appStrcpy( VarToken.Identifier, HardcodedName );
	}
	else if( !GetIdentifier(VarToken) )
	{
		ScriptErrorf(SCEL_Unknown,  TEXT("Missing variable name") );
	}

	UField* Existing = NULL;

	// Make sure it doesn't conflict.
	if( !Skip )
	{
		INT OuterContextCount = 0;
		Existing = FindField(Scope, VarToken.Identifier, TRUE, UField::StaticClass(), NULL, &OuterContextCount);
		if( Existing && Existing->GetOuter()==Scope )
		{
			if ( !bReparsingClass )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("%s: '%s' already defined"), HintText, VarToken.Identifier );
			}
		}
		else
		// don't allow it to obscure class properties either
		if ( Existing
			// declaring class member of function local/parm
			&&	(Cast<UFunction>(Scope) != NULL || Cast<UClass>(Scope) != NULL || Cast<UState>(Scope) != NULL)
			// and the existing field isn't a function
			&&	Cast<UFunction>(Existing) == NULL
			// and the existing field is a class member (don't care about locals in other functions)
			&&	(Cast<UClass>(Existing->GetOuter()) != NULL || Cast<UState>(Existing->GetOuter()) != NULL) )
		{
			if (Existing->IsA(UScriptStruct::StaticClass()))
			{
				ScriptWarnf(SCWL_Level4, TEXT("%s: '%s' conflicts with struct defined in %s'%s'"), HintText, VarToken.Identifier, (OuterContextCount > 0) ? TEXT("'within' class") : TEXT(""), *Existing->GetOuter()->GetName());
			}
			else
			{
				// if this is a property and one of them is deprecated, ignore it since it will be removed soon
				UProperty* ExistingProp = Cast<UProperty>(Existing);
				if ( ExistingProp == NULL
				|| (!ExistingProp->HasAnyPropertyFlags(CPF_Deprecated) && (VarProperty.PropertyFlags&CPF_Deprecated) == 0) )
				{
					ScriptWarnf(SCWL_Level4, TEXT("%s: '%s' conflicts with previously defined field in %s'%s'"), HintText, VarToken.Identifier, (OuterContextCount > 0) ? TEXT("'within' class") : TEXT(""), *Existing->GetOuter()->GetName() );
				}
			}
		}
	}

	// Get optional dimension immediately after name.
	if( MatchSymbol(TEXT("[")) )
	{
		if( NoArrays )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Arrays aren't allowed in this context") );
		}
		if (VarProperty.ArrayDim == 0)
		{
			ScriptErrorf(SCEL_Restricted, TEXT("Static arrays of dynamic arrays are not allowed"));
		}

		if( VarProperty.Type == CPT_Bool )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Bool arrays are not allowed") );
		}

		// Default to dynamic array!!
		if( !PeekSymbol(TEXT("]")) )
		{
			if( !GetConstInt(VarProperty.ArrayDim, 0, Scope) )
			{
				FToken my_token;
				if ( GetIdentifier(my_token) )
				{
					INT ArrayIndex = INDEX_NONE;

					// this might be a const
					FName ConstName = FName( my_token.Identifier );

					INT OuterContextCount = 0;
					UField* Existing = FindField(Scope, my_token.Identifier, TRUE, UField::StaticClass(), NULL, &OuterContextCount);

					UConst* Const = Cast<UConst>(Existing);
					if( Const != NULL )
					{
						VarProperty.ArrayDim = ArrayIndex = appAtoi( *(Const->Value) );
					}
					else
					{
						UEnum* Enum = Cast<UEnum>(Existing);

						if( Enum == NULL )
						{
							// If the enum wasn't declared in this scope, then try to find it anywhere we can
							Enum = FindObject<UEnum>( ANY_PACKAGE, my_token.Identifier );
						}

						if ( Enum != NULL )
						{
							if ( MatchSymbol(TEXT(".")) )
							{
								if( !GetToken(my_token) )
								{
									ScriptErrorf(SCEL_Unknown,  TEXT("Missing enum tag after '%s'"), *Enum->GetName() );
								}
								else if( my_token.TokenName==NAME_EnumCount )
								{
									// subtract 1 because the last entry in the enum's Names array
									// is the _MAX entry
									ArrayIndex = Enum->NumEnums() - 1;
								}
								else
								{
									ArrayIndex = Enum->FindEnumIndex( my_token.TokenName );
									if ( my_token.TokenName == NAME_None || ArrayIndex == INDEX_NONE )
									{
										ScriptErrorf(SCEL_Unknown,  TEXT("Missing enum tag after '%s'"), *Enum->GetName() );
									}
								}

								VarProperty.ArrayDim = ArrayIndex;
							}
							else
							{
								// declared a static array using just an enum name - this indicates
								// that the static array should always contain the number of items in the enum
								// subtract 1 because the last entry in the enum's Names array
								// is the _MAX entry
								VarProperty.ArrayDim = ArrayIndex = Enum->NumEnums() - 1;
							}

							// set the ArraySizeEnum if applicable
							VarProperty.ArrayIndexEnum = Enum;
						}
					}

					if ( ArrayIndex == INDEX_NONE )
					{
						ScriptErrorf(SCEL_Unknown/*SCEL_Formatting*/,  TEXT("%s %s: Bad or missing array size"), HintText, VarToken.Identifier );
						///VarProperty.ArrayDim = 1;
					}
				}
				else
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Formatting*/,  TEXT("%s %s: Bad or missing array size"), HintText, VarToken.Identifier );
					///VarProperty.ArrayDim = 1;
				}
			}

			if( VarProperty.ArrayDim<=1 || VarProperty.ArrayDim>MAX_ARRAY_SIZE )
			{
				ScriptErrorf(SCEL_Limit,  TEXT("%s %s: Illegal array size %i"), HintText, VarToken.Identifier, VarProperty.ArrayDim );
			}
		}

		if( !MatchSymbol(TEXT("]")) )
		{
			ScriptErrorf(SCEL_Formatting,  TEXT("%s %s: Missing ']'"), HintText, VarToken.Identifier );
		}
	}
	else if( MatchSymbol(TEXT("(")) )
	{
		ScriptErrorf(SCEL_Unknown,  TEXT("Use [] for arrays, not ()") );
	}
	
	const UBOOL bIsFunctionParm = IsFunction && !NoArrays;
	const UBOOL bIsFunctionLocal = !bIsFunctionParm && Scope->GetClass() == UFunction::StaticClass();
	const UBOOL bIsOperatorParm = bIsFunctionParm && Skip;
	const UBOOL bIsStateLocal = !bIsFunctionParm && Scope->GetClass() == UState::StaticClass();

// 	if ( !Skip && !NoArrays && !IsFunction && Scope->GetClass() != UFunction::StaticClass() )
	if (!bIsOperatorParm && !bIsFunctionLocal && !bIsStateLocal)
	{
		// get optional export-text for this property
		FScriptLocation DeclarationPosition;
		if ( ParsePropertyExportText(VarToken, VarToken.Identifier) && !bReparsingClass )
		{
			///FScriptLocation ContinueLocation;
			if ( !bIsFunctionParm && !Class->HasAnyClassFlags(CLASS_Native) )
			{
				ReturnToLocation(DeclarationPosition, FALSE, TRUE);
				ScriptErrorf(SCEL_Restricted, TEXT("Export-text may only be used in native classes"));
				///ReturnToLocation(ContinueLocation, FALSE, TRUE);
			}

			// ok, all we're checking here is:
			// if this property is not marked native and the class is not a transient class, throw an error....unless
			if ((VarToken.PropertyFlags&CPF_Native)==0 && !Class->HasAnyClassFlags(CLASS_Transient)
			// the property is a function parameter for a native function, delegate, or event.  In the case of an event, the event must have
			// been originally declared in a native class or it means nothing
			&&	(!bIsFunctionParm ||((FunctionScope->FunctionFlags&(FUNC_Native|FUNC_Delegate|FUNC_Event)) == 0)
			||	((FunctionScope->FunctionFlags&(FUNC_Native|FUNC_Delegate|FUNC_Event)) == FUNC_Event && !FunctionScope->GetOwnerClass()->HasAnyClassFlags(CLASS_Native))))
			{
				ReturnToLocation(DeclarationPosition, FALSE, TRUE);
				if ( bIsFunctionParm )
				{
					ScriptErrorf(SCEL_Restricted, TEXT("Only native functions, delegates, and events may specify export-text."));
				}
				else
				{
					ScriptErrorf(SCEL_Restricted, TEXT("You must mark this property 'native' in order to specify export-text")); 
				}
				///ReturnToLocation(ContinueLocation, FALSE, TRUE);
			}
			
			if ( VarToken.Type != CPT_Struct )
			{
				ReturnToLocation(DeclarationPosition, FALSE, TRUE);
				ScriptErrorf(SCEL_Restricted, TEXT("Currently only struct properties support export-text on a per-property basis"));
				///ReturnToLocation(ContinueLocation, FALSE, TRUE);
			}
		}

		// if the parsing fails, go back to where we started so the line number in the error message matches up
		if ( !bIsFunctionParm )
		{
			if ( !ParsePropertyMetaData(VarToken, VarToken.Identifier))
			{
				ReturnToLocation(DeclarationPosition, FALSE, TRUE);
				ScriptErrorf(SCEL_Unknown, TEXT("Metadata was not formatted properly")); 
			}

			// If the property is editable, let's consider setting the previous
			// comment as the property's tooltip metadata.
			if ( VarToken.PropertyFlags & CPF_Edit )
			{
				// Format the previous comment if it hasn't been already.
				if ( !bPrevCommentFormatted )
				{
					PrevComment = FormatCommmentForToolTip( PrevComment );
					bPrevCommentFormatted = TRUE;
				}

				// If there's a comment . . .
				if ( PrevComment.Len() > 0 )
				{
					// . . . and no tooltip was specified in the metadata . . .
					const FName ToolTipName( TEXT("ToolTip") );
					if ( !VarToken.MetaData.Find(ToolTipName) )
					{
						// . . . use the comment as tooltip metadata.
						VarToken.MetaData.Set( ToolTipName, *PrevComment );
					}
				}
			}
		}
	}

	if (!Skip && bIsStateLocal)
	{
		UState* StateScope = Cast<UState>(Scope);
		check(Scope);
		StateScope->StateFlags |= STATE_HasLocals;
	}

	//only give indices to member variables
	if (Scope->GetClass() != UFunction::StaticClass() && Scope->GetClass() != UState::StaticClass())
	{
		//Get the depth of this class so we can keep derived class variables higher than base class variables.
		INT Depth = 0;
		UClass* SuperClass = Class;
		while (SuperClass)
		{
			SuperClass = SuperClass->GetSuperClass();
			Depth++;
		}
		//limit to depth of compiling (assuming no more than 255 levels of derivation
		check(IsWithin<DWORD>(Depth, 0, MAXBYTE));
		//Inverting the depth so the derived class has lower offsets than the base class
		DWORD InverseClassDepth = MAXBYTE - Depth;
		//Moving the class offset to the high word of a dword (as it is more significant)
		DWORD ClassOffset = InverseClassDepth << 16;

		//UObject will have the biggest offset, and each derived class will have a lower starting variable offset (0x00ff0000, 0x00fe0000, etc)
		//High Word = Class Offset
		//Low Word = Variable Offset
		DWORD VariableIndex = ClassOffset + OriginalVariableIndex;
		//increment to the next variable for this class
		++OriginalVariableIndex;

		const FName OrderIndexName(TEXT("OrderIndex") );
		FString OrderString = FString::Printf(TEXT("%d"), VariableIndex);
		VarToken.MetaData.Set(OrderIndexName, *OrderString);
	}

	// If this is the first time seeing the property name, then flag it for replace instead of add
	const EFindName FindFlag = VarProperty.PropertyFlags & CPF_Config ? GetFindFlagForPropertyName(VarToken.Identifier) : FNAME_Add;
	// create the FName for the property, splitting (ie Unnamed_3 -> Unnamed,3)
	FName PropertyName(VarToken.Identifier, FindFlag, TRUE);

	// Add property.
	UProperty* NewProperty=NULL;
	if ( bReparsingClass )
	{
		NewProperty = (UProperty*)Existing;
		VarProperty.ArrayDim = Max(1, VarProperty.ArrayDim);
	}
	else if( !Skip )
	{
		UProperty* Prev=NULL, *Array=NULL;
		UMapProperty *MapProp = NULL;
		UObject* NewScope=Scope;
	    for( TFieldIterator<UProperty> It(Scope); It && It.GetStruct()==Scope; ++It )
		{
			Prev = *It;
		}
		if( VarProperty.ArrayDim==0 )
		{
			Array = new(Scope,PropertyName,ObjectFlags)UArrayProperty;
			NewScope = Array;
			VarProperty.ArrayDim = 1;
			ObjectFlags = RF_Public;
		}

		if ( VarProperty.Type == CPT_Map )
		{
			NewProperty = MapProp = new(Scope, PropertyName, ObjectFlags) UMapProperty;
			VarProperty.ArrayDim = 1;
			MapProp->Key = NULL;
			MapProp->Value = NULL;
		}
		else if( VarProperty.Type==CPT_Byte )
		{
			if ( Class->HasAnyClassFlags(CLASS_Native) && !HasMemberProperties(Scope) )
			{
				UStruct* SuperScope = Scope->GetSuperStruct();
				while ( SuperScope != NULL && !HasMemberProperties(SuperScope) )
				{
					// if the parent class didn't have any fields, skip to the next parent class
					SuperScope = SuperScope->GetSuperStruct();
				}
				if ( SuperScope != NULL )
				{
					UProperty* LastPropertyInParent=NULL;
					
					for( TFieldIterator<UProperty> It(SuperScope,FALSE); It; ++It )
					{
						LastPropertyInParent = *It;
					}

					if ( LastPropertyInParent != NULL && LastPropertyInParent->IsA(UByteProperty::StaticClass()) )
					{
						ScriptWarnf(SCWL_Level4,TEXT("It is not recommended for the first property of a %s to be a byte if the last property in the super %s %s is a byte; different compilers may align these properties differently."), *Scope->GetClass()->GetName(), *Scope->GetClass()->GetName(), *SuperScope->GetName()); 
					}
				}
			}

			if ( Array == NULL )
			{
				// don't group dynamic byte arrays together with bytes
				Prev = GroupProperty( Scope, Prev, VarProperty.PropertyFlags, UByteProperty::StaticClass()->GetFName() );
			}

			NewProperty = new(NewScope,PropertyName,ObjectFlags)UByteProperty;
			((UByteProperty*)NewProperty)->Enum = VarProperty.Enum;
		}
		else if( VarProperty.Type==CPT_Int )
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UIntProperty;
		}
		else if( VarProperty.Type==CPT_Bool )
		{
			if ( Array == NULL )
			{
				// don't group dynamic bool arrays together with bools
				Prev = GroupProperty( Scope, Prev, VarProperty.PropertyFlags, UBoolProperty::StaticClass()->GetFName() );
			}

			NewProperty = new(NewScope,PropertyName,ObjectFlags)UBoolProperty;
		}
		else if( VarProperty.Type==CPT_Float )
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UFloatProperty;
		}
		else if( VarProperty.Type == CPT_ObjectReference )
		{
			check(VarProperty.PropertyClass);
			if( VarProperty.PropertyClass==UClass::StaticClass() )
			{
				NewProperty = new(NewScope,PropertyName,ObjectFlags)UClassProperty;
				((UClassProperty*)NewProperty)->MetaClass = VarProperty.MetaClass;
			}
			else if( VarProperty.PropertyClass->IsChildOf(UComponent::StaticClass()) )
			{
				NewProperty = new(NewScope,PropertyName,ObjectFlags)UComponentProperty;
				VarProperty.PropertyFlags |= CPF_EditInline;
			}
			else
			{
				NewProperty = new(NewScope,PropertyName,ObjectFlags)UObjectProperty;
			}

			((UObjectProperty*)NewProperty)->PropertyClass = VarProperty.PropertyClass;
		}
		else if ( VarProperty.Type == CPT_Interface )
		{
			check(VarProperty.PropertyClass);
			check(VarProperty.PropertyClass->HasAnyClassFlags(CLASS_Interface));

			UInterfaceProperty* InterfaceProperty = new(NewScope,PropertyName,ObjectFlags) UInterfaceProperty;
			InterfaceProperty->InterfaceClass = VarProperty.PropertyClass;

			NewProperty = InterfaceProperty;
		}
		else if( VarProperty.Type==CPT_Name )
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UNameProperty;
		}
		else if( VarProperty.Type==CPT_String )
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UStrProperty;
		}
		else if( VarProperty.Type==CPT_Struct )
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UStructProperty;
			((UStructProperty*)NewProperty)->Struct = VarProperty.Struct;
		}
		else if ( VarProperty.Type==CPT_Delegate )
		{
			NewProperty = new(NewScope,PropertyName,ObjectFlags)UDelegateProperty;
		}
		else
		{
			ScriptErrorf(SCEL_Unknown, TEXT("Unknown property type %i"), (BYTE)VarProperty.Type );
		}

		if( Array )
		{
			check(NewProperty);
			CastChecked<UArrayProperty>(Array)->Inner = NewProperty;

			// Copy some of the property flags to the inner property.
			NewProperty->PropertyFlags |= (VarProperty.PropertyFlags&CPF_PropagateToArrayInner);
			NewProperty = Array;
		}
		NewProperty->ArrayDim      = VarProperty.ArrayDim;
		NewProperty->PropertyFlags = VarProperty.PropertyFlags;
		NewProperty->Category      = Category;
		NewProperty->ArraySizeEnum = VarProperty.ArrayIndexEnum;
		if( Prev )
		{
			NewProperty->Next = Prev->Next;
			Prev->Next = NewProperty;
		}
		else
		{
			NewProperty->Next = Scope->Children;
			Scope->Children = NewProperty;
		}
	}

	if ( !Skip )
	{
		VarToken.TokenProperty = NewProperty;
		ClassData->AddProperty(VarToken);

		// if we had any metadata, add it to the class
		ClassData->AddMetaData(VarToken);

		// copy any MetaData we've parsed into the output variable so that EOL comments don't clobber any tooltips we parsed.
		VarProperty.MetaData = VarToken.MetaData;
		
		
		// @todo: also put metadata into struct props!
	}

	return NewProperty;
}

//
// Try to compile an affector expression or assignment.
//
void FScriptCompiler::CompileAffector( EPropertyReferenceType LeftRefType /* = CPRT_AssignValue */, EPropertyReferenceType RightRefType /* = CPRT_AssignmentReference  */ )
{
	// Try to compile an affector expression or assignment.
	FScriptLocation LowRetry;
	GotAffector = 0;
	AffectorReturnProperty = NULL;

	// Try to compile an expression here.
	FPropertyBase RequiredType(CPT_None,LeftRefType);
	FToken ResultType;
	FTokenChain TokenChain;
	FScopedStructModificationByte StructModificationByte(TopNest);
	if (CompileExpr(RequiredType, NULL, &ResultType, MAXINT, NULL, FALSE, &TokenChain, FALSE, &StructModificationByte) < 0)
	{
		FToken Token;
		GetToken(Token);
		ScriptErrorf(SCEL_Unknown,  TEXT("'%s': Bad command or expression"), Token.Identifier );
	}

	// Did we get a function call or a varible assignment?
	if( MatchSymbol(TEXT("=")) )
	{
		// Variable assignment.
		if( !(ResultType.PropertyFlags & CPF_OutParm) )
		{
			if (ResultType.PropertyFlags & CPF_Const)
			{
				ScriptErrorf(SCEL_Unknown,  TEXT("'=': Left value is const") );
			}
			else
			{
				ScriptErrorf(SCEL_Unknown,  TEXT("'=': Left value is not a variable") );
			}
		}

		// if the left hand expression is a struct member, update the member access bytecode to indicate that the struct will be modified
		if (StructModificationByte.ByteLocation != NULL)
		{
			TopNode->Script(StructModificationByte.ByteLocation->Element) = 1;
		}

		if ( (ResultType.PropertyFlags&CPF_Const) != 0
		&&	(ResultType.TokenProperty != NULL && (ResultType.TokenProperty->PropertyFlags&CPF_Const) == 0) )
		{
			// if the token's PropertyFlags has CPF_Const set but the property isn't const, it indicates that the script compiler
			// determined that assigning a value to this expression was illegal for some reason

			// first check for property flags that conditionally cause the property to be const
			if (ResultType.TokenProperty->PropertyFlags & CPF_ProtectedWrite)
			{
				ScriptErrorf(SCEL_Restricted, TEXT("%s: ProtectedWrite property is const here"), ResultType.Identifier);
			}
			else if (ResultType.TokenProperty->PropertyFlags & CPF_PrivateWrite)
			{
				ScriptErrorf(SCEL_Restricted, TEXT("%s: PrivateWrite property is const here"), ResultType.Identifier);
			}
			else
			{
				// check the token chain to try to identify the exact error
				for (INT i = TokenChain.Num() - 2; i >= 0; i--) // -2 as -1 should be the same as ResultType
				{
					if (TokenChain(i).TokenName == NAME_Default)
					{
						// attempt to illegally modify a default value
						ScriptErrorf(SCEL_Restricted, TEXT("'%s': Can't change default values of non-config properties"), ResultType.Identifier);
					}
				}
				
				// generic error message
				ScriptErrorf(SCEL_Restricted,  TEXT("'=': Left value is const") );
			}
		}

		// Compile right value.
		RequiredType = ResultType;
		RequiredType.PropertyFlags &= ~CPF_OutParm;
		RequiredType.ReferenceType = RightRefType;
		CompileExpr( RequiredType, TEXT("'='") );

		// Emit the let.
		FScriptLocation HighRetry;
		EmitLet( ResultType, TEXT("'='") );

		// move the EX_Let bytecode so that it is evaluated before both expressions
		MoveCompiledCode(LowRetry,HighRetry);
	}
	else if( GotAffector )
	{
		// Function call or operators with outparms.

		// if the affector returns something, that value will be unused
		// so if it requires destruction or is larger than the buffer that is allocated by the interpreter, it must be explicitly destroyed
		if (AffectorReturnProperty && ((AffectorReturnProperty->PropertyFlags & CPF_NeedCtorLink) || AffectorReturnProperty->GetSize() > MAX_SIMPLE_RETURN_VALUE_SIZE_IN_DWORDS*sizeof(DWORD)))
		{
			FScriptLocation HighRetry;
			Writer << EX_EatReturnValue;
			Writer << AffectorReturnProperty;
			// swap the code so that EX_EatReturnValue comes before
			// the expression returning the value we need to destroy
			MoveCompiledCode(LowRetry, HighRetry);
		}
	}
	else if( ResultType.Type != CPT_None )
	{
		// Whatever expression we parsed had no effect.
		FToken Token;
		GetToken(Token);
		ScriptErrorf(SCEL_Unknown,  TEXT("'%s': Expression has no effect"), Token.Identifier );
	}
	else
	{
		// Didn't get anything, so throw an error at the appropriate place.
		FToken Token;
		GetToken(Token);
		ScriptErrorf(SCEL_Unknown,  TEXT("'%s': Bad command or expression"), Token.Identifier );
	}
}

/*-----------------------------------------------------------------------------
	Statement compiler.
-----------------------------------------------------------------------------*/

//
// Compile a declaration in Token. Returns 1 if compiled, 0 if not.
//
UBOOL FScriptCompiler::CompileDeclaration( FToken& Token, UBOOL& NeedSemicolon )
{
	if( Token.Matches(NAME_Class) && (TopNest->Allow & ALLOW_Class) )
	{
		CompileClassDeclaration(Token, NeedSemicolon);
	}
	// compile Java or C# style interface declaration
	else if( Token.Matches(NAME_Interface) && (TopNest->Allow & ALLOW_Class) )
	{
		CompileInterfaceDeclaration(Token, NeedSemicolon);
	}
	else if ( IsValidFunctionSpecifier(Token) && !PeekSymbol(TEXT("=")) && !PeekSymbol(TEXT(".")) )
	{
		CompileFunctionDeclaration(Token,NeedSemicolon);
	}
	else if( Token.Matches(NAME_Const) )
	{
		CompileConst( Class, NeedSemicolon );
	}
	else if( Token.Matches(NAME_Var) || Token.Matches(NAME_Local) )
	{
		CompileVariableDeclaration(Token,NeedSemicolon);
	}
	else if( Token.Matches(NAME_Enum) )
	{
		// Enumeration definition.
		CheckAllow( TEXT("'Enum'"), ALLOW_TypeDecl );

		// Compile enumeration.
		CompileEnum( Class, NeedSemicolon );
	}
	else if( Token.Matches(NAME_Struct) )
	{
		// Struct definition.
		CheckAllow( TEXT("'struct'"), ALLOW_TypeDecl );

		// Compile struct.
		CompileStruct( Class, NeedSemicolon );
	}
	else if
	(	Token.Matches(NAME_State)
	||	Token.Matches(NAME_Auto)
	||	Token.Matches(NAME_Simulated) )
	{
		CompileStateDeclaration(Token,NeedSemicolon);
	}
	else if( Token.Matches(NAME_Ignores) )
	{
		CompileIgnoreDeclaration(Token,NeedSemicolon);
	}
	else if( Token.Matches(NAME_Replication) )
	{
		// Network replication definition.
		if( TopNest->NestType != NEST_Class )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("'Replication' is not allowed here") );
		}
		RequireSymbol( TEXT("{"), TEXT("'Replication'") );
		TopNode->TextPos = InputPos;
		TopNode->Line    = InputLine;
		SkipStatements( 1, TEXT("'Replication'") );

		NeedSemicolon=0;
	}
	else if( Token.Matches(TEXT("#")) )
	{
		// Compiler directive.
		CompileDirective();
		NeedSemicolon=0;
	}
	else if ( Token.Matches(TEXT(";")) )
	{
		// extra semi-colon
		NeedSemicolon = 0;
		return 1;
	}
	else
	{
		// Not a declaration.
		return 0;
	}
	return 1;
}

/**
 * Compiles a class declaration.
 *
 * @param	Token			[out] contains information about the new class's name
 * @param	NeedSemicolon	[out] whether we still need to parse the semicolon
 */
void FScriptCompiler::CompileClassDeclaration( FToken& Token, UBOOL& NeedSemicolon )
{
	if ( bReparsingClass )
	{
		SkipStatements(0, TEXT("class declaration"));
		NeedSemicolon = 0;

		// Push the class nesting.
		PushNest( NEST_Class, Class->GetFName(), NULL );

		// bail
		return;
	}

	// Start of a class block.
	CheckAllow( TEXT("'class'"), ALLOW_Class );

	// Class name.
	if( !GetToken(Token) )
	{
		ScriptErrorf(SCEL_Class,  TEXT("Missing class name") );
	}

	if( !Token.Matches(*Class->GetName()) )
	{
		ScriptErrorf(SCEL_Class,  TEXT("Class name '%s' doesn't match name of source file '%s'"), Token.Identifier, *Class->GetName() );
	}

	// Get parent class.
	if( MatchIdentifier(NAME_Extends) )
	{
		// Set the base class.
		UClass* TempClass = GetQualifiedClass( TEXT("'extends'") );
		// a class cannot 'extends' an interface, use 'implements'
		if( TempClass->ClassFlags & CLASS_Interface )
		{
			ScriptErrorf(SCEL_Class,  TEXT("Class '%s' cannot extend interface '%s', use 'implements'"), *Class->GetName(), *TempClass->GetName() );
		}

#if WITH_LIBFFI
		// A class cannot extend a DLLBind class.
		if( TempClass->DLLBindName != NAME_None )
		{
			ScriptErrorf(SCEL_Class,  TEXT("Class '%s' cannot extend '%s' as it is DLLBind(%s)'"), *Class->GetName(), *TempClass->GetName(), *TempClass->DLLBindName.ToString() );
		}
#endif

		UClass* SuperClass = Class->GetSuperClass();
		if( SuperClass == NULL )
		{
			Class->SuperStruct = TempClass;
		}
		else if( SuperClass != TempClass )
		{
			if ( !Class->ChangeParentClass(TempClass) )
			{
				ScriptErrorf(SCEL_Class,  TEXT("%s's superclass must be %s, not %s"), *Class->GetPathName(), *SuperClass->GetPathName(), *TempClass->GetPathName() );
			}
		}

		// if the parent is misaligned, then so are we
		Class->SetFlags( Class->SuperStruct->GetFlags() & RF_MisalignedObject );
		Class->ClassCastFlags |= Class->GetSuperClass()->ClassCastFlags;
	}
	else if( Class->GetSuperClass() )
	{
		ScriptErrorf(SCEL_Class,  TEXT("class: missing 'Extends %s'"), *Class->GetSuperClass()->GetName() );
	}

	// Get outer class.
	if( MatchIdentifier(NAME_Within) )
	{
		// Set the outer class.
		UClass* TempClass = GetQualifiedClass( TEXT("'within'") );
		if (TempClass->IsChildOf(UInterface::StaticClass()))
		{
			ScriptErrorf(SCEL_Restricted, TEXT("Classes cannot be 'within' interfaces"));
		}
		else if (Class->ClassWithin == NULL || Class->ClassWithin==UObject::StaticClass() || TempClass->IsChildOf(Class->ClassWithin))
		{
			Class->ClassWithin = TempClass;
		}
		else if (Class->ClassWithin != TempClass)
		{
			ScriptErrorf(SCEL_Restricted, TEXT("%s must be within %s, not %s"), *Class->GetPathName(), *Class->ClassWithin->GetPathName(), *TempClass->GetPathName());
		}
	}
	else
	{
		// classes always have a ClassWithin
		Class->ClassWithin = Class->GetSuperClass()
			? Class->GetSuperClass()->ClassWithin
			: UObject::StaticClass();
	}

	UClass* ExpectedWithin = Class->GetSuperClass()
		? Class->GetSuperClass()->ClassWithin
		: UObject::StaticClass();

	if( !Class->ClassWithin->IsChildOf(ExpectedWithin) )
	{
		ScriptErrorf(SCEL_Restricted,  TEXT("Parent class declared within '%s'.  Cannot override within class with '%s' since it isn't a child"), *ExpectedWithin->GetName(), *Class->ClassWithin->GetName() );
		///Class->ClassWithin = ExpectedWithin;
	}

	// Keep track of whether "config(ini)" was used.
	UBOOL DeclaresConfigFile = FALSE;


	// Class attributes.
	FClassMetaData* ClassData = GScriptHelper->FindClassData(Class);
	check(ClassData);

	for( ; ; )
	{
		GetToken(Token);
		if( Token.Matches(NAME_Native) || Token.Matches(NAME_NativeOnly) )
		{
			// Note that this class has C++ code dependencies.
			if( Class->GetSuperClass() && !Class->GetSuperClass()->HasAnyClassFlags(CLASS_Native) )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Native classes cannot expand non-native classes") );
			}

			Class->SetFlags( RF_Native );
			Class->ClassFlags |= CLASS_Native;

			if(Token.Matches(NAME_NativeOnly))
			{
				Class->ClassFlags |= CLASS_NativeOnly;
			}

			// Parse an optional class header filename.
			if( MatchSymbol(TEXT("(")) )
			{
				FToken Token;
				if( GetIdentifier(Token, 0) )
				{
					if ( appStricmp(Token.Identifier,TEXT("inherit")) == 0 )
					{
						UClass* SuperClass = Class->GetSuperClass();
						if ( SuperClass == NULL )
						{
							ScriptErrorf(SCEL_Restricted, TEXT("Cannot inherit native header filename: %s has no super class"), *Class->GetName());
						}
						else
						{
							Class->ClassHeaderFilename = SuperClass->ClassHeaderFilename;
						}
					}
					else
					{	
						Class->ClassHeaderFilename = Token.Identifier;
					}
				}
				else
				{
					ScriptErrorf(SCEL_Formatting,  TEXT("native: Missing native header filename") );
				}

				RequireSymbol(TEXT(")"), TEXT("native") );
			}
		}
		else if( Token.Matches(NAME_Inherits) )
		{
			RequireSymbol(TEXT("("), TEXT("inherits"));

			FToken Token;
			while ( TRUE )
			{
				if ( !GetIdentifier(Token, 0) )
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Formatting*/,  TEXT("inherits: Missing C++ base class name") );
				}
				else
				{
					ClassData->AddInheritanceParent(Token.Identifier);
				}

				// see if multiple inheritance parents have been specified
				if ( PeekSymbol(TEXT(",")) )
				{
					GetSymbol(Token);
				}
				else
				{
					break;
				}
			}
			RequireSymbol(TEXT(")"), TEXT("inherits"));
		}
		else if( Token.Matches(NAME_NoExport) )
		{
			// Don't export to C++ header.
			Class->ClassFlags |= CLASS_NoExport;
		}
		else if( Token.Matches(NAME_EditInlineNew) )
		{
			// don't allow actor classes to be declared editinlinenew
			if ( Class->IsChildOf(AActor::StaticClass()) )
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Invalid class attribute: Creating actor instances via the property window is not allowed"));
			}

			// Class can be constructed from the New button in editinline
			Class->ClassFlags |= CLASS_EditInlineNew;
		}
		else if( Token.Matches(NAME_NotEditInlineNew) )
		{
			// Class cannot be constructed from the New button in editinline
			Class->ClassFlags &= ~CLASS_EditInlineNew;
		}
		else if( Token.Matches(NAME_Placeable) )
		{
			// Allow the class to be placed in the editor.
			Class->ClassFlags |= CLASS_Placeable;
		}
		else if( Token.Matches(NAME_NotPlaceable) )
		{
			// Don't allow the class to be placed in the editor.
			if ( Class->ClassFlags & CLASS_Placeable )
			{
				Class->ClassFlags &= ~CLASS_Placeable;
			}
		}
		else if( Token.Matches(NAME_HideDropDown) )
		{
			// Prevents class from appearing in class comboboxes in the property window
			Class->ClassFlags |= CLASS_HideDropDown;
		}
		else if( Token.Matches(NAME_NativeReplication) )
		{
			// Replication is native.
			Class->ClassFlags |= CLASS_NativeReplication;
		}
		else if( Token.Matches(NAME_DependsOn) )
		{
			RequireSymbol(TEXT("("), TEXT("dependsOn") );
			FToken Token;
			while ( 1 )
			{
				if( !GetIdentifier(Token, 0) )
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Formatting*/,  TEXT("dependsOn: Missing dependent name") );
				}

				if ( PeekSymbol(TEXT(",")) )
				{
					GetSymbol(Token);
				}
				else
				{
					break;
				}
			}

			RequireSymbol(TEXT(")"), TEXT("dependsOn") );
		}
		else if( Token.Matches(NAME_PerObjectConfig) )
		{
			// Don't export to C++ header.
			Class->ClassFlags |= CLASS_PerObjectConfig;
		}
		else if( Token.Matches(NAME_PerObjectLocalized) )
		{
			// export localization properties per object
			Class->ClassFlags |= CLASS_PerObjectLocalized;
		}
		else if( Token.Matches(NAME_Abstract) )
		{
			// Hide all editable properties.
			Class->ClassFlags |= CLASS_Abstract;
		}
		else if ( Token.Matches(NAME_Deprecated) )
		{
			Class->ClassFlags |= CLASS_Deprecated;

			// Don't allow the class to be placed in the editor.
			if ( Class->ClassFlags & CLASS_Placeable )
			{
				Class->ClassFlags &= ~CLASS_Placeable;
			}
		}
		else if( Token.Matches(NAME_Transient) )
		{
			// Transient class.
			Class->ClassFlags |= CLASS_Transient;
		}
		else if ( Token.Matches(NAME_NonTransient) )
		{
			// this child of a transient class is not transient - remove the transient flag
			Class->ClassFlags &= ~CLASS_Transient;
		}
		else if( Token.Matches(NAME_Config) )
		{
			// Class containing config properties - parse the name of the config file to use
			if ( MatchSymbol(TEXT("(")) )
			{
				FToken ConfigNameToken;
				if( !GetIdentifier(ConfigNameToken, 0) )
				{
					ScriptErrorf(SCEL_Formatting,  TEXT("config: Missing name of configuration file.") );
				}
				else
				{
					// if the user specified "inherit", we're just going to use the parent class's config filename
					// this is not actually necessary but it can be useful for explicitly communicating config-ness
					if ( appStricmp(ConfigNameToken.Identifier, TEXT("inherit")) == 0 )
					{
						UClass* SuperClass = Class->GetSuperClass();
						if ( SuperClass == NULL )
						{
							ScriptErrorf(SCEL_Fatal, TEXT("Cannot inherit config filename: %s has no super class"), *Class->GetName());
						}
						else if ( SuperClass->ClassConfigName == NAME_None )
						{
							ScriptErrorf(SCEL_Parse, TEXT("Cannot inherit config filename: parent class %s is not marked config."), *SuperClass->GetPathName());
						}
					}
					else
					{
						// otherwise, set the config name to the parsed identifier
						Class->ClassConfigName = ConfigNameToken.Identifier;
					}
				}
				RequireSymbol(TEXT(")"), TEXT("config") );
				DeclaresConfigFile = TRUE;
			}
			else
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("config: Missing configuration name") );
			}
		}
		else if( Token.Matches(NAME_SafeReplace) )
		{
			ScriptWarnf(SCWL_Level4, TEXT("SafeReplace: keyword is deprecated"));
		}
		else if( Token.Matches(NAME_ShowCategories) )
		{
			RequireSymbol( TEXT("("), TEXT("'ShowCategories'") );
			do
			{
				FToken Category;
				if( !GetIdentifier( Category, 1 ) )
				{
					ScriptErrorf(SCEL_Formatting,  TEXT("ShowCategories: Expected category name") );
				}
				else
				{
					Class->HideCategories.RemoveItem( FName( Category.Identifier ) );
				}
			}
			while( MatchSymbol(TEXT(",")) );
			RequireSymbol( TEXT(")"), TEXT("'ShowCategories'") );
		}
		else if( Token.Matches(NAME_ForceScriptOrder) )
		{
			FToken ForceScriptOrderToken(CPT_Bool);

			RequireSymbol( TEXT("("), TEXT("'ForceScriptOrder'") );
			if( !GetIdentifier( ForceScriptOrderToken, 1 ) )
			{
				ScriptErrorf(SCEL_Formatting,  TEXT("ForceScriptOrder: Expected boolean value") );
			}
			else
			{
				Class->bForceScriptOrder = ForceScriptOrderToken.Matches(NAME_TRUE);
			}
			RequireSymbol( TEXT(")"), TEXT("'ForceScriptOrder'") );
		}
		else if( Token.Matches(NAME_HideCategories) )
		{
			RequireSymbol( TEXT("("), TEXT("'HideCategories'") );
			do
			{
				FToken Category;
				if( !GetIdentifier( Category, 1 ) )
				{
					ScriptErrorf(SCEL_Formatting,  TEXT("HideCategories: Expected category name") );
				}
				else
				{
					Class->HideCategories.AddItem( FName( Category.Identifier ) );
				}
			}
			while( MatchSymbol(TEXT(",")) );
			RequireSymbol( TEXT(")"), TEXT("'HideCategories'") );
		}
		else if( Token.Matches(NAME_ClassGroup) )
		{
			RequireSymbol(TEXT("("), TEXT("'ClassGroup'") );
			do 
			{
				FToken GroupToken;
				if( !GetIdentifier( GroupToken, 1) )
				{
					ScriptErrorf(SCEL_Formatting,  TEXT("ClassGroup: Expected group name") );
				}
				else
				{
					Class->ClassGroupNames.AddItem( FName( GroupToken.Identifier ) );
				}
			}
			while( MatchSymbol(TEXT(",")) );

			RequireSymbol(TEXT(")"), TEXT("'ClassGroup'") );
		}
		else if( Token.Matches(NAME_AutoExpandCategories) )
		{
			RequireSymbol( TEXT("("), TEXT("'AutoExpandCategories'") );
			do
			{
				FToken Category;
				if( !GetIdentifier( Category, 1 ) )
				{
					ScriptErrorf(SCEL_Formatting,  TEXT("AutoExpandCategories: Expected category name") );
				}
				else
				{
					const FName CategoryName = Category.Identifier;
					Class->AutoCollapseCategories.RemoveItem(CategoryName);
					Class->AutoExpandCategories.AddUniqueItem(CategoryName);
				}
			}
			while( MatchSymbol(TEXT(",")) );
			RequireSymbol( TEXT(")"), TEXT("'AutoExpandCategories'") );
		}
		else if ( Token.Matches(NAME_AutoCollapseCategories) )
		{
			RequireSymbol(TEXT("("), TEXT("'AutoCollapseCategories'"));
			do
			{
				FToken Category;
				if( GetIdentifier( Category, TRUE ) )
				{
					const FName CategoryName = Category.Identifier;
					Class->AutoExpandCategories.RemoveItem(CategoryName);
					Class->AutoCollapseCategories.AddUniqueItem(CategoryName);
				}
				else
				{
					ScriptErrorf(SCEL_Formatting,  TEXT("AutoCollapseCategories: Expected category name") );
				}
			}
			while( MatchSymbol(TEXT(",")) );
			RequireSymbol(TEXT(")"), TEXT("'AutoCollapseCategories'"));
		}
		else if( Token.Matches(NAME_DontAutoCollapseCategories) )
		{
			RequireSymbol( TEXT("("), TEXT("'DontAutoCollapseCategories'") );
			do
			{
				FToken Category;
				if( !GetIdentifier( Category, TRUE ) )
				{
					ScriptErrorf(SCEL_Formatting,  TEXT("DontAutoCollapseCategories: Expected category name") );
				}
				else
				{
					const FName CategoryName = Category.Identifier;
					Class->AutoCollapseCategories.RemoveItem(CategoryName);
				}
			}
			while( MatchSymbol(TEXT(",")) );
			RequireSymbol( TEXT(")"), TEXT("'DontAutoCollapseCategories'") );
		}
		else if( Token.Matches(NAME_DontSortCategories) )
		{
			RequireSymbol( TEXT("("), TEXT("'DontSortCategories'") );
			do
			{
				FToken Category;
				if( !GetIdentifier( Category, TRUE ) )
				{
					ScriptErrorf(SCEL_Formatting,  TEXT("DontSortCategories: Expected category name") );
				}
				else
				{
					const FName CategoryName = Category.Identifier;
					Class->DontSortCategories.AddUniqueItem(CategoryName);
				}
			}
			while( MatchSymbol(TEXT(",")) );
			RequireSymbol( TEXT(")"), TEXT("'DontSortCategories'") );
		}
		else if( Token.Matches(NAME_CollapseCategories) )
		{
			// Class' properties should not be shown categorized in the editor.
			Class->ClassFlags |= CLASS_CollapseCategories;
		}
		else if( Token.Matches(NAME_DontCollapseCategories) )
		{
			// Class' properties should be shown categorized in the editor.
			Class->ClassFlags &= ~CLASS_CollapseCategories;
		}
		else if( Token.Matches(NAME_Implements) )
		{
			RequireSymbol( TEXT("("), TEXT("'Implements'") );
			do
			{
				UClass* Interface = GetQualifiedClass( TEXT("'Implements'") );
				if ( Interface == NULL )
				{
					ScriptErrorf(SCEL_Class, TEXT("Implements: Interface class not found"));
				}
				else if ( !Interface->HasAnyClassFlags(CLASS_Interface) )
				{
					ScriptErrorf(SCEL_Class,  TEXT("Implements: Class %s is not an interface"), *Interface->GetName() );
				}
				for (UClass* TestClass = Class; TestClass != NULL; TestClass = TestClass->GetSuperClass())
				{
					for (INT i = 0; i < TestClass->Interfaces.Num(); i++)
					{
						if (TestClass->Interfaces(i).Class == Interface)
						{
							ScriptErrorf(SCEL_Class, TEXT("Implements: Interface '%s' is already implemented by '%s'"), *Interface->GetName(), *TestClass->GetName());
							break;
						}
					}
				}

				//propogate the inheritable ClassFlags
				Class->ClassFlags |= (Interface->ClassFlags) & CLASS_ScriptInherit;

				new (Class->Interfaces) FImplementedInterface(Interface, NULL);
				if ( Interface->HasAnyClassFlags(CLASS_Native) )
				{
					ClassData->AddInheritanceParent(Interface);
				}
			}
			while( MatchSymbol(TEXT(",")) );
			RequireSymbol( TEXT(")"), TEXT("'Implements'") );
		}
		else if ( Token.Matches(TEXT("ClassRedirect")) )
		{
			RequireSymbol(TEXT("("), TEXT("ClassRedirect") );
			FToken Token;
			while ( TRUE )
			{
				if( !GetIdentifier(Token, 0) )
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Formatting*/,  TEXT("ClassRedirect: Missing previous class name") );
				}

				if ( PeekSymbol(TEXT(",")) )
				{
					GetSymbol(Token);
				}
				else
				{
					break;
				}
			}

			RequireSymbol(TEXT(")"), TEXT("ClassRedirect") );
		}
#if WITH_LIBFFI
		else if ( Token.Matches(TEXT("DLLBind")) )
		{
			RequireSymbol(TEXT("("), TEXT("DLLBind") );
			FToken Token;
			if( !GetIdentifier(Token, 0) || FName(Token.Identifier) == NAME_None )
			{
				ScriptErrorf(SCEL_Unknown,  TEXT("DLLBind: Missing DLL name") );
			}
			Class->DLLBindName = Token.Identifier;

			RequireSymbol(TEXT(")"), TEXT("DLLBind") );
		}
#endif
		else
		{
			UngetToken(Token);
			break;
		}
	}
	// Validate.
	if( (Class->ClassFlags&CLASS_NoExport) )
	{
		if ( !Class->HasAnyClassFlags(CLASS_Native) )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("'noexport': Only valid for native classes") );
		}

		// if the class's class flags didn't contain CLASS_NoExport before it was parsed, it means either:
		// a) the DECLARE_CLASS macro for this native class doesn't contain the CLASS_NoExport flag (this is an error)
		// b) this is a new native class, which isn't yet hooked up to static registration (this is OK)
		if ( (PreviousClassFlags & CLASS_NoExport) == 0 &&
			(PreviousClassFlags&CLASS_Native) != 0 )	// a new native class (one that hasn't been compiled into C++ yet) won't have this set
		{
			ScriptWarnf(SCWL_Level4, TEXT("'noexport': Must include CLASS_NoExport in native class declaration"));
		}
	}

	// if this is not a native class, make sure it doesn't use any keywords that only apply to native classes
	if ( !Class->HasAnyClassFlags(CLASS_Native) )
	{
		if ( Class->Interfaces.Num() > 0 )
		{
			// non-native classes aren't allowed to implement any native interfaces
			for (TArray<FImplementedInterface>::TIterator It(Class->Interfaces); It; ++It)
			{
				if (It->Class->HasAnyClassFlags(CLASS_Native))
				{
					ScriptErrorf(SCEL_Restricted, TEXT("Non-native classes not allowed to implement native interfaces"));
				}
			}
		}

		if ( ClassData->GetInheritanceParents().Num() > 0 )
		{
			ScriptErrorf(SCEL_Restricted, TEXT("Inherits: only valid for native classes"));
		}
	}
	else
	{
#if WITH_LIBFFI
		// native classes cannot be DLLBind
		if( Class->DLLBindName != NAME_None )
		{
			ScriptErrorf(SCEL_Restricted, TEXT("DLLBind classes are not allowed to be native"));
		}
#endif
	}

	if ( !Class->HasAnyClassFlags(CLASS_Abstract) && (PreviousClassFlags&CLASS_Abstract) != 0 )
	{
		if ( Class->HasAnyClassFlags(CLASS_NoExport) )
		{
			ScriptErrorf(SCEL_Formatting, TEXT("'abstract': NoExport class missing abstract keyword from unrealscript class declaration (must change C++ version first)"));
			Class->ClassFlags |= CLASS_Abstract;
		}
		else if ( Class->HasAnyFlags(RF_Native) )
		{
			ScriptWarnf(SCWL_Level4, TEXT("'abstract': missing abstract keyword from unrealscript class declaration - class will no longer be exported as abstract"));
		}
	}

	// Invalidate config name if not specifically declared.
	if( !DeclaresConfigFile )
	{
		Class->ClassConfigName = NAME_None;
	}

	if ( Class->CppText != NULL )
	{
		if ( !Class->HasAnyClassFlags(CLASS_Native) )
		{
			ScriptErrorf(SCEL_Restricted, TEXT("Cannot use cpptext blocks in non-native classes"));
		}
		else if ( Class->HasAnyClassFlags(CLASS_NoExport) )
		{
			ScriptWarnf(SCWL_Level4, TEXT("'noexport': cpptext block used in noexport class"));
		}
	}

	// Get semicolon.
	RequireSymbol( TEXT(";"), TEXT("'Class'") );
	NeedSemicolon=0;

	// Init variables.
	Class->Script.Empty();
	Class->Children			= NULL;
	Class->Next				= NULL;
	Class->ProbeMask        = 0;
	Class->StateFlags       = 0;
	Class->LabelTableOffset = 0;
	Class->NetFields.Empty();

	// Make visible outside the package.
	Class->ClearFlags( RF_Transient );
	check(Class->HasAnyFlags(RF_Public));
	check(Class->HasAnyFlags(RF_Standalone));

	// Copy properties from parent class.
	if( Class->GetSuperClass() )
	{
		Class->SetPropertiesSize( Class->GetSuperClass()->GetPropertiesSize() );
	}

	// Push the class nesting.
	PushNest( NEST_Class, Class->GetFName(), NULL );

	// auto-create properties for all of the VFTables needed for the multiple inheritances
	// get the inheritance parents
	const TArray<FMultipleInheritanceBaseClass>& InheritanceParents = ClassData->GetInheritanceParents();

	// if we have any multiple inheritance parents
	if (InheritanceParents.Num())
	{
		// find the struct called "pointer" since we are going to make a pointer variable
		static UScriptStruct* PointerStruct = FindObject<UScriptStruct>(UObject::StaticClass(), TEXT("Pointer"));
		check(PointerStruct);

		// generate some helper classes for the struct
		FPropertyBase PointerProp(PointerStruct);
		FToken PropToken = PointerProp;
		// the property would be declared like this:
		// var private native const noexport pointer VfTable;
		// so make the corresponding flags:
		EObjectFlags ObjectFlags = 0;
		QWORD PropFlags = (CPF_Native | CPF_Const | CPF_NoExport);

		// for all base class types, make a VfTable property
		for (INT ParentIndex = InheritanceParents.Num() - 1; ParentIndex >= 0; ParentIndex--)
		{
			const FMultipleInheritanceBaseClass& InheritanceParent = InheritanceParents(ParentIndex);

			// make a unique name for this property
			FString PropName = FString::Printf(TEXT("VfTable_%s"), *InheritanceParent.ClassName);

			// create a new property, this is some of the behavior from GetVarNameAndDim, without 
			// the text parsing that it does
			UStructProperty* NewProperty = new(Class, *PropName, ObjectFlags)UStructProperty;
			// set property properties
			NewProperty->Struct = PointerStruct;
			NewProperty->PropertyFlags = PropFlags;
			NewProperty->ArrayDim      = 1;
			NewProperty->Category      = NAME_None;
			NewProperty->Next = Class->Children;
			Class->Children = NewProperty;

			// add this property to the metadata
			PropToken.TokenProperty = NewProperty;
			ClassData->AddProperty(PropToken);

			// if this base class corresponds to an interface class, assign the vtable UProperty in the class's Interfaces map now...
			if ( InheritanceParent.InterfaceClass != NULL )
			{
				UBOOL bFound = FALSE;
				for (INT i = 0; i < Class->Interfaces.Num(); i++)
				{
					if (Class->Interfaces(i).Class == InheritanceParent.InterfaceClass)
					{
						Class->Interfaces(i).PointerProperty = NewProperty;
						bFound = TRUE;
						break;
					}
				}
				if (!bFound)
				{
					new(Class->Interfaces) FImplementedInterface(InheritanceParent.InterfaceClass, NewProperty);
				}
			}
		}
	}
}


/**
 *  compiles Java or C# style interface declaration
 */
void FScriptCompiler::CompileInterfaceDeclaration( FToken& Token, UBOOL& NeedSemicolon )
{
	if( bReparsingClass == TRUE )
	{
		SkipStatements(0, TEXT("interface declaration"));
		NeedSemicolon = 0;

		// Push the interface nesting.
		PushNest( NEST_Interface, Class->GetFName(), NULL );

		// bail
		return;
	}

	// Start of an interface block. Since Interfaces and Classes are always at the same nesting level,
	// whereever a class declaration is allowed, an interface declaration is also allowed.
	CheckAllow( TEXT("'interface'"), ALLOW_Class );

	// Interface name.
	if( !GetToken(Token) )
    {
		ScriptErrorf(SCEL_Class,  TEXT("Missing interface name") );
	}

	if( !Token.Matches(*Class->GetName()) )
	{
		ScriptErrorf(SCEL_Class,  TEXT("Interface name '%s' doesn't match name of source file '%s'"), Token.Identifier, *Class->GetName() );
	}

	// Get super interface
	if( MatchIdentifier(NAME_Extends) )
	{
		// verify if our super class is an interface class
		// the super class should have been marked as CLASS_Interface at the importing stage, if it were an interface
		UClass* TempClass = GetQualifiedClass( TEXT("'extends'") );
		if( !(TempClass->ClassFlags & CLASS_Interface) )
		{
			ScriptErrorf(SCEL_Class,  TEXT("Interface class '%s' cannot inherit from non-interface class '%s'"), Token.Identifier, *TempClass->GetName() );
		}

		UClass* SuperClass = Class->GetSuperClass();
		if( SuperClass == NULL )
		{
			Class->SuperStruct = TempClass;
		}
		else if (SuperClass != TempClass)
		{
			ScriptErrorf(SCEL_Class,  TEXT("%s's superclass must be %s, not %s"), *Class->GetPathName(), *SuperClass->GetPathName(), *TempClass->GetPathName() );
		}
	}

	// Set the appropriate interface class flags
	Class->ClassFlags |= CLASS_Interface | CLASS_Abstract;
	if (Class->SuperStruct != NULL)
	{
		Class->ClassCastFlags |= Class->GetSuperClass()->ClassCastFlags;
	}

	// Interface attributes
	for (;;) 
	{
		GetToken(Token);
		if( Token.Matches(NAME_Native) || Token.Matches(NAME_NativeOnly) )
		{
			UClass* SuperClass = Class->GetSuperClass();
			if ( SuperClass != UInterface::StaticClass() )
			{
				// Native interfaces cannot inherit from non-native interfaces
				if ( SuperClass && !SuperClass->HasAnyClassFlags(CLASS_Native) )
				{
					ScriptErrorf(SCEL_Restricted,  TEXT("Native interface '%s' cannot extend non-native interface '%s'"), *Class->GetName(), *Class->GetSuperClass()->GetName() );
				}
			}

			Class->SetFlags(RF_Native);
			Class->ClassFlags |= CLASS_Native;

			// If this class is 'native only' properties should not be accessed from script
			if( Token.Matches(NAME_NativeOnly) )
			{
				Class->ClassFlags |= CLASS_NativeOnly;
			}

			// Parse an optional class header filename.
			if( MatchSymbol(TEXT("(")) )
			{
				FToken HeaderToken;
				if( !GetIdentifier(HeaderToken, 0) )
				{
					ScriptErrorf(SCEL_Formatting,  TEXT("native: Missing native header filename") );
				}
				else
				{
					if ( appStricmp(HeaderToken.Identifier,TEXT("inherit")) == 0 )
					{
						UClass* SuperClass = Class->GetSuperClass();
						if ( SuperClass == NULL )
						{
							ScriptErrorf(SCEL_Fatal, TEXT("Cannot inherit native header filename: %s has no super class"), *Class->GetName());
						}
						else
						{
							if ( SuperClass->GetOutermost() != Class->GetOutermost() )
							{
								// this means that the parent class is located in another package, so we won't be able to export this class to the same classes file
								//hmm should this be an error?
							}
							Class->ClassHeaderFilename = SuperClass->ClassHeaderFilename;
						}
					}
					else
					{	
						Class->ClassHeaderFilename = HeaderToken.Identifier;
					}
				}
				RequireSymbol(TEXT(")"), TEXT("native") );
			}
		}
		else if( Token.Matches(NAME_DependsOn) )
		{
			RequireSymbol(TEXT("("), TEXT("dependsOn") );
			FToken ClassToken;
			while ( 1 )
			{
				if( !GetIdentifier(ClassToken, 0) )
				{
					ScriptErrorf(SCEL_Unknown/*SCEL_Formatting*/,  TEXT("dependsOn: Missing dependent name") );
				}

				if ( PeekSymbol(TEXT(",")) )
				{
					GetSymbol(ClassToken);
				}
				else
				{
					break;
				}
			}

			RequireSymbol(TEXT(")"), TEXT("dependsOn") );
		}
		else if( Token.Matches(NAME_EditInlineNew) )
		{
			// Class can be constructed from the New button in editinline
			Class->ClassFlags |= CLASS_EditInlineNew;
		}
		else
		{
			UngetToken(Token);
			break;
		}
	}

	// Get semicolon.
	RequireSymbol( TEXT(";"), TEXT("'interface'") );
	NeedSemicolon = 0;

	// Init variables.
	Class->Script.Empty();
	Class->Children			= NULL;
	Class->Next				= NULL;
	Class->ProbeMask        = 0;
	Class->StateFlags       = 0;
	Class->LabelTableOffset = 0;
	Class->NetFields.Empty();

	// Make visible outside the package.
	Class->ClearFlags( RF_Transient );
	check(Class->HasAnyFlags(RF_Public));
	check(Class->HasAnyFlags(RF_Standalone));

	// Push the interface class nesting.
	// we need a more specific set of allow flags for NEST_Interface, only function declaration is allowed, no other stuff are allowed
	PushNest( NEST_Interface, Class->GetFName(), NULL );
}


/**
 * Parses and compiles a function declaration
 *
 * @param	Token			[out] contains information about the new function
 * @param	NeedSemicolon	[out] whether a semicolon is required - TRUE if the function is only declared
 */
void FScriptCompiler::CompileFunctionDeclaration( FToken& Token, UBOOL& NeedSemiColon )
{
	// Function or operator.
	const TCHAR* NestName = NULL;
	FScriptLocation FuncNameRetry;
	FFuncInfo FuncInfo;
	FuncInfo.FunctionFlags = FUNC_Public;
	UStruct* Scope = TopNode;
	EObjectFlags ReturnVariableObjectFlags = RF_Public;
	QWORD DelegatePropertyFlags=0;


	// Process all specifiers.
	UBOOL bSpecifiedUnreliable = FALSE, bSpecifiedFunctionType=FALSE;
	for( ;; )
	{
		InitScriptLocation(FuncNameRetry);
		if( Token.Matches(NAME_Function) )
		{
			// Get function name.
			CheckAllow( TEXT("'Function'"),ALLOW_Function);
			NestName = TEXT("function");

			if ( bSpecifiedFunctionType )
			{
				const FString FunctionType = (FuncInfo.FunctionFlags&FUNC_Delegate) != 0
					? TEXT("Delegate")
					: (FuncInfo.FunctionFlags&FUNC_Event) != 0
						? TEXT("Event")
						: TEXT("Function");

				ScriptErrorf(SCEL_Restricted, TEXT("Function type already declared as '%s'"), *FunctionType);
			}

			bSpecifiedFunctionType = TRUE;
			FuncInfo.FunctionFlags &= ~FUNC_Delegate;
		}
		else if( Token.Matches(NAME_Delegate) )
		{
			CheckAllow( TEXT("'Function'"),ALLOW_Function);
			NestName = TEXT("delegate");
			if ( bSpecifiedFunctionType )
			{
				const FString FunctionType = (FuncInfo.FunctionFlags&FUNC_Delegate) != 0
					? TEXT("Delegate")
					: (FuncInfo.FunctionFlags&FUNC_Event) != 0
						? TEXT("Event")
						: TEXT("Function");

				ScriptErrorf(SCEL_Restricted, TEXT("Function type already declared as '%s'"), *FunctionType);
			}

			bSpecifiedFunctionType = TRUE;
			FuncInfo.FunctionFlags |= FUNC_Delegate;
		}
		else if( Token.Matches(NAME_Event) )
		{
			CheckAllow( TEXT("'Function'"), ALLOW_Function );
			NestName = TEXT("event");
			if ( bSpecifiedFunctionType )
			{
				const FString FunctionType = (FuncInfo.FunctionFlags&FUNC_Delegate) != 0
					? TEXT("Delegate")
					: (FuncInfo.FunctionFlags&FUNC_Event) != 0
						? TEXT("Event")
						: TEXT("Function");

				ScriptErrorf(SCEL_Restricted, TEXT("Function type already declared as '%s'"), *FunctionType);
			}

			bSpecifiedFunctionType = TRUE;
			FuncInfo.FunctionFlags |= FUNC_Event;
		}
		else if( Token.Matches(NAME_Operator) )
		{
			// Get operator name or symbol.
			CheckAllow( TEXT("'Operator'"), ALLOW_Function );
			NestName = TEXT("operator");
			FuncInfo.FunctionFlags |= FUNC_Operator;
			FuncInfo.ExpectParms = 3;

			if( !MatchSymbol(TEXT("(")) )
			{
				ScriptErrorf(SCEL_Unknown,  TEXT("Missing '(' and precedence after 'Operator'") );
			}
			else if( !GetConstInt(FuncInfo.Precedence) )
			{
				ScriptErrorf(SCEL_Unknown,  TEXT("Missing precedence value") );
			}
			else if( FuncInfo.Precedence<0 || FuncInfo.Precedence>255 )
			{
				ScriptErrorf(SCEL_Unknown,  TEXT("Bad precedence value") );
			}
			else if( !MatchSymbol(TEXT(")")) )
			{
				ScriptErrorf(SCEL_Unknown,  TEXT("Missing ')' after operator precedence") );
			}
		}
		else if( Token.Matches(NAME_PreOperator) )
		{
			// Get operator name or symbol.
			CheckAllow( TEXT("'PreOperator'"), ALLOW_Function );
			NestName = TEXT("preoperator");
			FuncInfo.ExpectParms = 2;
			FuncInfo.FunctionFlags |= FUNC_Operator | FUNC_PreOperator;
		}
		else if( Token.Matches(NAME_PostOperator) )
		{
			// Get operator name or symbol.
			CheckAllow( TEXT("'PostOperator'"), ALLOW_Function );
			NestName = TEXT("postoperator");
			FuncInfo.FunctionFlags |= FUNC_Operator;
			FuncInfo.ExpectParms = 2;
		}
		else if( Token.Matches(NAME_Native) )
		{
			if ( !Class->HasAnyClassFlags(CLASS_Native) )
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Only functions of native classes can be declared native")); 
			}
			// Get internal id.
			FuncInfo.FunctionFlags |= FUNC_Native;
			if( MatchSymbol(TEXT("(")) )
			{
				if( !GetConstInt(FuncInfo.iNative) )
				{
					ScriptErrorf(SCEL_Formatting,  TEXT("Missing native id") );
				}
				
				if( !MatchSymbol(TEXT(")")) )
				{
					ScriptErrorf(SCEL_Formatting,  TEXT("Missing ')' after internal id") );
				}
			}
		}
		else if( Token.Matches(NAME_Static) )
		{
			FuncInfo.FunctionFlags |= FUNC_Static;
			if( TopNode->GetClass()==UState::StaticClass() )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Static functions cannot exist in a state") );
			}
		}
		else if( Token.Matches(NAME_Simulated) )
		{
			FuncInfo.FunctionFlags |= FUNC_Simulated;
		}
		else if( Token.Matches(NAME_Iterator) )
		{
			FuncInfo.FunctionFlags |= FUNC_Iterator;
		}
		else if( Token.Matches(NAME_Singular) )
		{
			FuncInfo.FunctionFlags |= FUNC_Singular;
		}
		else if( Token.Matches(NAME_Latent) )
		{
			FuncInfo.FunctionFlags |= FUNC_Latent;
		}
		else if( Token.Matches(NAME_Exec) )
		{
			FuncInfo.FunctionFlags |= FUNC_Exec;
			if( FuncInfo.FunctionFlags & FUNC_Net )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Exec functions cannot be replicated!") );
			}
		}
		else if( Token.Matches(NAME_Final) )
		{
			// This is a final (prebinding, non-overridable) function or operator.
			FuncInfo.FunctionFlags |= FUNC_Final;
			FuncInfo.FunctionExportFlags |= FUNCEXPORT_Final;
			if (Class->HasAnyClassFlags(CLASS_Interface))
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Interface functions cannot be declared 'final'"));
			}
		}
		else if ( Token.Matches(NAME_Server) )
		{
			FuncInfo.FunctionFlags |= FUNC_Net;
			FuncInfo.FunctionFlags |= FUNC_NetServer;
			if( FuncInfo.FunctionFlags & FUNC_Exec )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Exec functions cannot be replicated!") );
			}
		}
		else if ( Token.Matches(NAME_Client) )
		{
			FuncInfo.FunctionFlags |= FUNC_Net;
			FuncInfo.FunctionFlags |= FUNC_NetClient;
			// also implicitly mark client function as simulated
			FuncInfo.FunctionFlags |= FUNC_Simulated;
		}
		else if ( Token.Matches(NAME_Reliable) )
		{
			FuncInfo.FunctionFlags |= FUNC_NetReliable;
		}
		else if (Token.Matches(NAME_Unreliable))
		{
			bSpecifiedUnreliable = TRUE;
		}
		else if( Token.Matches(NAME_Private) )
		{
			// if we've already encountered the 'delegate' keyword, these keywords apply to the auto-generated delegate property, not to the delegate function itself.
			if ( (FuncInfo.FunctionFlags&FUNC_Delegate) == 0 )
			{
				FuncInfo.FunctionFlags &= ~FUNC_Public;
				FuncInfo.FunctionFlags |= FUNC_Private|FUNC_Final;
			}
			else
			{
				ReturnVariableObjectFlags &= ~(RF_Public|RF_Protected);
			}
		}
		else if( Token.Matches(NAME_Protected) )
		{
			// if we've already encountered the 'delegate' keyword, these keywords apply to the auto-generated delegate property, not to the delegate function itself.
			if ( (FuncInfo.FunctionFlags&FUNC_Delegate) == 0 )
			{
				FuncInfo.FunctionFlags &= ~FUNC_Public;
				FuncInfo.FunctionFlags |= FUNC_Protected;
			}
			else
			{
				ReturnVariableObjectFlags |= (RF_Public|RF_Protected);
			}
		}
		else if( Token.Matches(NAME_Public) )
		{
			if ( (FuncInfo.FunctionFlags&FUNC_Delegate) == 0 )
			{
				FuncInfo.FunctionFlags |= FUNC_Public;
			}
			else
			{
				ReturnVariableObjectFlags |= RF_Public;
			}
		}
		else if ( Token.Matches(NAME_NoExportHeader))
		{
			FuncInfo.FunctionExportFlags |= FUNCEXPORT_NoExportHeader;
		}
		else if ( Token.Matches(NAME_NoExport) )
		{
			FuncInfo.FunctionExportFlags |= FUNCEXPORT_NoExport;
		}
		else if ( Token.Matches(NAME_Virtual) )
		{
			FuncInfo.FunctionExportFlags |= FUNCEXPORT_Virtual;
		}
		else if ( Token.Matches(NAME_Transient) )
		{
			DelegatePropertyFlags |= CPF_Transient;
		}
#if WITH_LIBFFI
		else if ( Token.Matches(NAME_DLLImport) )
		{
			if ( Class->DLLBindName == NAME_None )
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Only DLLBind classes can declare DLLImport functions")); 
			}
			FuncInfo.FunctionFlags |= FUNC_DLLImport;
		}
#endif
		else
		{
			break;
		}
		GetToken(Token);
	}
	UngetToken(Token);

	// Make sure we got a function.
	if( !NestName )
	{
		ScriptErrorf(SCEL_Unknown,  TEXT("Missing 'function'") );
	}

	if ( Class->HasAnyClassFlags(CLASS_NativeOnly) && !(FuncInfo.FunctionFlags & FUNC_Native) )
	{
		ScriptErrorf(SCEL_Restricted, TEXT("Classes marked as 'nativeonly' cannot contain script functions."));
	}

	if ( (FuncInfo.FunctionFlags&FUNC_Delegate) == 0 )
	{
		if ( (DelegatePropertyFlags&CPF_Transient) != 0 )
		{
			ScriptErrorf(SCEL_Restricted, TEXT("'Transient' can only be used in delegate function declarations: %s"), NestName);
		}

		// make sure to clear any access specifiers - this means they were intended for the function itself
		ReturnVariableObjectFlags = RF_Public;
	}

	// Warn if native doesn't actually exist.
	if( FuncInfo.iNative!=0 )
	{
#if CHECK_NATIVE_MATCH
		if( FuncInfo.iNative<EX_FirstNative || FuncInfo.iNative>EX_Max || GNatives[FuncInfo.iNative]==&UObject::execUndefined )
		{
			ScriptErrorf(SCEL_Fatal,  TEXT("Bad native function id %i\r\n"),FuncInfo.iNative);
		}
#endif

		if ( !bReparsingClass )
		{
			for ( TObjectIterator<UFunction> It; It; ++It )
			{
				UFunction* Func = *It;
				if ( Func->iNative == FuncInfo.iNative )
				{
					ScriptErrorf(SCEL_Restricted, TEXT("Native function id '%i' specified for function '%s' is already assigned to '%s'"), FuncInfo.iNative, NestName, *Func->GetPathName());
				}
			}
		}
	}

	// Get return type.
	FScriptLocation Start;
	EObjectFlags ReturnValueFlags=0;
	FPropertyBase ReturnType( CPT_None );
	FToken TestToken;
	UBOOL HasReturnValue = 0;
	if( GetIdentifier(TestToken,1) )
	{
		if( !PeekSymbol(TEXT("(")) )
		{
			ReturnToLocation( Start );

			UBOOL bAutoCastReturn = PeekIdentifier(NAME_Coerce);
			HasReturnValue = GetVarType( TopNode, ReturnType, ReturnValueFlags, ~CPF_CoerceParm, NULL );
			if ( bAutoCastReturn && !bReparsingClass )
			{
				if ( (FuncInfo.FunctionFlags&FUNC_Native) == 0 )
				{
					ScriptErrorf(SCEL_Restricted, TEXT("Only native functions can auto-cast their return types"));
				}
				if ( HasReturnValue && ((ReturnType.PropertyFlags&CPF_CoerceParm) != 0) )
				{
					if ( ReturnType.Type != CPT_ObjectReference || ReturnType.MetaClass != NULL )
					{
						ScriptErrorf(SCEL_Restricted, TEXT("The return value for auto-cast functions must be an object type"));
					}
				}
				else
				{
					ScriptErrorf(SCEL_Restricted, TEXT("Auto-cast functions must return a object type"));
				}
			}
		}
		else 
		{
			ReturnToLocation( Start );
		}
	}

	// Get function or operator name.
	if( !GetIdentifier(FuncInfo.Function) && (!(FuncInfo.FunctionFlags&FUNC_Operator) || !GetSymbol(FuncInfo.Function)) )
	{
		ScriptErrorf(SCEL_Unknown,  TEXT("Missing %s name"), NestName );
	}

	if( !MatchSymbol(TEXT("(")) )
	{
		ScriptErrorf(SCEL_Unknown,  TEXT("Bad %s definition"), NestName );
	}

	// Validate flag combinations.
	if( FuncInfo.FunctionFlags & FUNC_Native )
	{
		if( FuncInfo.iNative && !(FuncInfo.FunctionFlags & FUNC_Final) )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Numbered native functions must be final") );
		}
	}
	else
	{
		if( FuncInfo.FunctionFlags & FUNC_Latent )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Only native functions may use 'Latent'") );
		}
		if( FuncInfo.FunctionFlags & FUNC_Iterator )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Only native functions may use 'Iterator'") );
		}
		if ( (FuncInfo.FunctionExportFlags&FUNCEXPORT_NoExport) != 0 )
		{
			ScriptErrorf(SCEL_Restricted, TEXT("Only native functions may use 'NoExport'"));
		}

		if ( (FuncInfo.FunctionExportFlags&FUNCEXPORT_Virtual) != 0 )
		{
			ScriptErrorf(SCEL_Restricted, TEXT("Only native functions may use 'Virtual'"));
		}
	}

#if WITH_LIBFFI
	if( FuncInfo.FunctionFlags & FUNC_DLLImport )
	{
		if (!(FuncInfo.FunctionFlags & FUNC_Final))
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("DLLImport functions must be final") );
		}
		if (FuncInfo.FunctionFlags & FUNC_Exec)
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("DLLImport functions cannot be declared exec") );
		}
		if (FuncInfo.FunctionFlags & FUNC_Static)
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("DLLImport functions cannot be declared static") );
		}
	}
#endif

	if (FuncInfo.FunctionFlags & FUNC_Net)
	{
#if WITH_LIBFFI
		if (FuncInfo.FunctionFlags & FUNC_DLLImport)
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("DLLImport functions can't be replicated") );
		}
		else
#endif
		if ( FuncInfo.FunctionFlags & FUNC_Static )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Static functions can't be replicated") );
		}
		else if (!(FuncInfo.FunctionFlags & FUNC_NetReliable) && !bSpecifiedUnreliable)
		{
			ScriptErrorf(SCEL_Restricted, TEXT("Replicated function: 'reliable' or 'unreliable' is required"));
		}
		else if ((FuncInfo.FunctionFlags & FUNC_NetReliable) && bSpecifiedUnreliable)
		{
			ScriptErrorf(SCEL_Restricted, TEXT("'reliable' and 'unreliable' are mutually exclusive"));
		}
	}
	else if (FuncInfo.FunctionFlags & FUNC_NetReliable)
	{
		ScriptErrorf(SCEL_Restricted, TEXT("'reliable' specified without 'client' or 'server'"));
	}
	else if (bSpecifiedUnreliable)
	{
		ScriptErrorf(SCEL_Restricted, TEXT("'unreliable' specified without 'client' or 'server'"));
	}

	// If operator, figure out the function signature.
	TCHAR Signature[NAME_SIZE]=TEXT("");
	if( FuncInfo.FunctionFlags & FUNC_Operator )
	{
		// Name.
		const TCHAR* In = FuncInfo.Function.Identifier;
		while( *In )
		{
			appStrncat( Signature, CppTags[*In++-32], NAME_SIZE );
		}

		// Parameter signature.
		FScriptLocation Retry;
		if( !MatchSymbol(TEXT(")")) )
		{
			// Parameter types.
			appStrncat( Signature, TEXT("_"), NAME_SIZE );
			if( FuncInfo.FunctionFlags & FUNC_PreOperator )
			{
				appStrncat( Signature, TEXT("Pre"), NAME_SIZE );
			}
			do
			{
				// Get parameter type.
				FPropertyBase Property(CPT_None);
				EObjectFlags ObjectFlags;
				//@note: we need to explicitly specify CPF_Const|CPF_AlwaysInit instead of adding it to CPF_ParmFlags because CPF_ParmFlags is treated as exclusive;
				// i.e., flags that are in CPF_ParmFlags are not allowed in other variable types and vice versa
				GetVarType( TopNode, Property, ObjectFlags, ~(CPF_ParmFlags|CPF_Const|CPF_AlwaysInit), TEXT("Function parameter") );
				GetVarNameAndDim( TopNode, Property, ObjectFlags, FALSE, TRUE, NULL, TEXT("Function parameter"), NAME_None, TRUE );

				// Add to signature.
				if
					(	Property.IsObject()
					||	Property.Type==CPT_Struct )
				{
					appStrncat( Signature, *Property.PropertyClass->GetName(), NAME_SIZE );
				}
				else
				{
					TCHAR Temp[NAME_SIZE];
					appStrcpy( Temp, *GetPropertyName(Property.Type).ToString() );
					if( appStrstr( Temp, TEXT("Property") ) )
					{
						*appStrstr( Temp, TEXT("Property") ) = 0;
					}
					appStrncat( Signature, Temp, NAME_SIZE );
				}
			} while( MatchSymbol(TEXT(",")) );
			RequireSymbol( TEXT(")"), TEXT("parameter list") );
		}
		ReturnToLocation( Retry, 1, 1 );
	}
	else
	{
		appStrcpy( Signature, FuncInfo.Function.Identifier );
	}

	// Allocate local property frame, push nesting level and verify
	// uniqueness at this scope level.
	FName SignatureName(Signature, FNAME_Add, TRUE);
	PushNest( NEST_Function, SignatureName, NULL );
	UFunction* TopFunction = ((UFunction*)TopNode);
	if ( !bReparsingClass )
	{
		TopFunction->FunctionFlags  |= FuncInfo.FunctionFlags;
		TopFunction->OperPrecedence  = FuncInfo.Precedence;
		TopFunction->iNative         = FuncInfo.iNative;
		TopFunction->FriendlyName    = FName( FuncInfo.Function.Identifier, FNAME_Add, TRUE );
	}
	FuncInfo.FunctionReference   = TopFunction;
	ClassData->AddFunction(FuncInfo);

	// Get parameter list.
	if( !MatchSymbol(TEXT(")")) )
	{
		UBOOL bEncounteredOptionalParam=0;
		do
		{
			// Get parameter type.
			FPropertyBase Property(CPT_None);
			EObjectFlags ObjectFlags;
			//@note: we need to explicitly specify CPF_Const|CPF_AlwaysInit instead of adding it to CPF_ParmFlags because CPF_ParmFlags is treated as exclusive;
			// i.e., flags that are in CPF_ParmFlags are not allowed in other variable types and vice versa
			GetVarType( TopNode, Property, ObjectFlags, ~(CPF_ParmFlags|CPF_Const|CPF_AlwaysInit), TEXT("Function parameter") );
			Property.PropertyFlags |= CPF_Parm;
			UProperty* Prop = GetVarNameAndDim( TopNode, Property, ObjectFlags, FALSE, TRUE, NULL, TEXT("Function parameter"), NAME_None, FALSE );
			if ( !bReparsingClass )
			{
				TopFunction->NumParms++;

				// Check parameters.

				// if this is the first parameter, and the function has a coerced return value
				if ( TopFunction->NumParms == 1 && HasReturnValue && (ReturnType.PropertyFlags&CPF_CoerceParm) != 0 )
				{
					// make sure that the first parameter is a class that can be meta casted to the return type
					if ( Property.Type != CPT_ObjectReference || Property.PropertyClass != UClass::StaticClass() )
					{
						ScriptErrorf(SCEL_Restricted, TEXT("The first parameter of an auto-cast function must be of type 'class'"));
					}
					else if ( Property.MetaClass != ReturnType.PropertyClass )
					{
						ScriptErrorf(SCEL_Restricted, TEXT("The first parameter of an auto-cast function must match the class of the return value"));
					}
				}
				if( (FuncInfo.FunctionFlags & FUNC_Operator) && (Property.PropertyFlags & ~(CPF_ParmFlags|CPF_Const|CPF_AlwaysInit)) )
				{
					ScriptErrorf(SCEL_Restricted,  TEXT("Operator parameters may not have modifiers") );
				}
				else if( Property.Type==CPT_Bool && (Property.PropertyFlags & CPF_OutParm) )
				{
					ScriptErrorf(SCEL_Restricted,  TEXT("Booleans may not be out parameters") );
				}
				else if
					(	(Property.PropertyFlags & CPF_SkipParm)
					&&	(!(TopFunction->FunctionFlags&FUNC_Native) || !(TopFunction->FunctionFlags&FUNC_Operator) || TopFunction->NumParms!=2) )
				{
					ScriptErrorf(SCEL_Restricted,  TEXT("Only parameter 2 of native operators may be 'Skip'") );
				}
				else if ((TopFunction->FunctionFlags & FUNC_Net) && (Property.PropertyFlags & CPF_OutParm))
				{
					ScriptErrorf(SCEL_Restricted, TEXT("Replicated functions cannot contain out parameters"));
				}
				else if ((TopFunction->FunctionFlags & FUNC_Net) && (Prop->GetClass()->ClassCastFlags & CASTCLASS_UDelegateProperty) != 0)
				{
					ScriptErrorf(SCEL_Restricted, TEXT("Replicated functions cannot contain delegate parameters (this would be insecure)"));
				}

#if WITH_LIBFFI
				if( TopFunction->FunctionFlags & FUNC_DLLImport )
				{
					if( (Prop->GetClass()->ClassCastFlags & (CASTCLASS_UStrProperty|CASTCLASS_UIntProperty|CASTCLASS_UFloatProperty|CASTCLASS_UStructProperty|CASTCLASS_UByteProperty))==0 )
					{
						ScriptErrorf(SCEL_Restricted,  TEXT("Unsupported type for parameter %s to DLLImport function %s"), *Prop->GetName(), *TopFunction->GetName() );
					}
					if( Prop->ArrayDim > 1 && (Prop->GetClass()->ClassCastFlags & (CASTCLASS_UIntProperty|CASTCLASS_UFloatProperty|CASTCLASS_UByteProperty))==0 )
					{
						ScriptErrorf(SCEL_Restricted,  TEXT("Arrays are not supported for parameter %s to DLLImport function %s"), *Prop->GetName(), *TopFunction->GetName() );
					}
				}
#endif
				// if this parameter is an out parm, mark function as containing out parms
				if ((Property.PropertyFlags & CPF_OutParm) != 0)
				{
					TopFunction->FunctionFlags |= FUNC_HasOutParms;
				}
			}

			// Default value.
			if( MatchSymbol( TEXT("=") ) )
			{
				if ( Prop->HasAnyPropertyFlags(CPF_OutParm) )
				{
					ScriptErrorf(SCEL_Restricted, TEXT("%s: Not allowed to specify a default value for optional out parms (%s)"), *TopFunction->GetName(), *Prop->GetName());
				}
#if WITH_LIBFFI
				else
				if( TopFunction->FunctionFlags & FUNC_DLLImport )
				{
					ScriptErrorf(SCEL_Restricted,  TEXT("%s: Not allowed to specify a default value for DLLImport function parm (%s)"), *TopFunction->GetName(), *Prop->GetName());
				}
#endif
				Prop->PropertyFlags |= CPF_OptionalParm;
				FTokenData* TokenData = ClassData->FindTokenData(Prop);
				if ( TokenData )
				{
					FDefaultParameterValue* Value = TokenData->SetDefaultValue();
					Value->InputLine = InputLine;
					Value->InputPos = InputPos;
				}

				// now skip past the default value for now - the actual default value for this optional parm will be compiled into the function's bytecode
				// in CompileSecondPass
				FToken SkipToken;
				INT ParenthesisNestCount=0;
				while ( GetToken(SkipToken) )
				{
					if ( ParenthesisNestCount == 0
						&& (SkipToken.Matches(TEXT(")")) || SkipToken.Matches(TEXT(","))) )
					{
						// went too far
						UngetToken(SkipToken);
						break;
					}

					if ( SkipToken.Matches(TEXT("(")) )
					{
						ParenthesisNestCount++;
					}
					else if ( SkipToken.Matches(TEXT(")")) )
					{
						ParenthesisNestCount--;
					}
				}
			}

			// Handle optionality.
			if( Prop->HasAnyPropertyFlags(CPF_OptionalParm) )
			{
				bEncounteredOptionalParam = TRUE;

				// if the function contains optional parameters, mark the function with a special flag as an optimization for ProcessEvent
				TopFunction->FunctionFlags |= FUNC_HasOptionalParms;
			}
			else if( bEncounteredOptionalParam )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("After an optional parameters, all other parameters must be optional") );
			}
		} while( MatchSymbol(TEXT(",")) );
		RequireSymbol( TEXT(")"), TEXT("parameter list") );
	}
	else if ( HasReturnValue && (ReturnType.PropertyFlags&CPF_CoerceParm) != 0 )
	{
		ScriptErrorf(SCEL_Restricted, TEXT("Auto-cast functions must specify a class for the first parameter"));
	}

	// Get return type, if any.
	if( HasReturnValue )
	{
		ReturnType.PropertyFlags |= CPF_Parm | CPF_OutParm | CPF_ReturnParm;
		UProperty* ReturnProp = GetVarNameAndDim( TopNode, ReturnType, ReturnValueFlags, TRUE, TRUE, TEXT("ReturnValue"), TEXT("Function return type"), NAME_None, FALSE );
		if ( !bReparsingClass )
		{
			TopFunction->NumParms++;
		}

#if WITH_LIBFFI
		if( TopFunction->FunctionFlags & FUNC_DLLImport )
		{
			if( (ReturnProp->GetClass()->ClassCastFlags & CASTCLASS_UIntProperty|CASTCLASS_UFloatProperty|CASTCLASS_UByteProperty|CASTCLASS_UBoolProperty|CASTCLASS_UStrProperty|CASTCLASS_UStructProperty)==0 )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Invalid return type for DLLImport function %s"), *TopFunction->GetName() );
			}
		}
#endif
	}

	// For operators, verify that: the operator is either binary or unary, there is
	// a return value, and all parameters have the same type as the return value.
	if( FuncInfo.FunctionFlags & FUNC_Operator )
	{
		INT n = TopFunction->NumParms;
		if( n != FuncInfo.ExpectParms )
		{
			ScriptErrorf(SCEL_Unknown,  TEXT("%s must have %i parameters"), NestName, FuncInfo.ExpectParms-1 );
		}

		if( !TopFunction->GetReturnProperty() )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Operator must have a return value") );
		}

		if( !(FuncInfo.FunctionFlags & FUNC_Final) )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Operators must be declared as 'Final'") );
		}
	}

	// Make new UDelegateProperty for delegate
	// NOTE: Don't do this for interfaces, as interfaces do not have properties
	if( (TopFunction->FunctionFlags&FUNC_Delegate) != 0 && !bReparsingClass && !Class->HasAnyClassFlags(CLASS_Interface) )
	{
		UProperty* Prev=NULL;
		for( TFieldIterator<UProperty> It(Scope); It && It.GetStruct()==Scope; ++It )
		{
			Prev = *It;
		}

		FName MangledName = FName(*FString::Printf(TEXT("__%s__Delegate"),*TopFunction->FriendlyName.ToString()), FNAME_Add, TRUE);
		UDelegateProperty* NewProperty = new(Scope, MangledName, 0) UDelegateProperty;

		// if the delegate was declared transient, protected, or private, set the corresponding flag on the autogenerated delegate property
		NewProperty->PropertyFlags |= DelegatePropertyFlags;
		ReturnVariableObjectFlags |= (ReturnValueFlags&CPF_CoerceParm);
		NewProperty->SetFlags(ReturnVariableObjectFlags);

		NewProperty->Function =  TopFunction;
		if( Prev )
		{
			NewProperty->Next = Prev->Next;
			Prev->Next = NewProperty;
		}
		else
		{
			NewProperty->Next = Scope->Children;
			Scope->Children = NewProperty;
		}
	}

	// determine whether this function should be exported as 'const'
	if ( MatchIdentifier(NAME_Const) )
	{
		if( (FuncInfo.FunctionFlags & FUNC_Native) == 0 )
		{
			ScriptErrorf(SCEL_Restricted, TEXT("'const' may only be used for native functions"));
		}

		FFunctionData* FuncData = ClassData->FindFunctionData(TopFunction);
		if ( FuncData != NULL )
		{
			FuncData->SetFunctionExportFlag(FUNCEXPORT_Const);
		}
	}

	// Detect whether the function is being defined or declared.
	if( PeekSymbol(TEXT(";")) )
	{
		// Function is just being declared, not defined.
		check( (TopFunction->FunctionFlags & FUNC_Defined)==0 );
	}
	else
	{
#if WITH_LIBFFI
		if( FuncInfo.FunctionFlags & FUNC_DLLImport )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("DLLImport functions must not declare a function body") );
		}
#endif
		// Require bracket.
		RequireSymbol(TEXT("{"), NestName);

		// if the next token is the closing bracket, this is an empty function, so don't mark it as defined
		if (MatchSymbol(TEXT("}")))
		{
			check((TopFunction->FunctionFlags & FUNC_Defined) == 0);
		}
		else
		{
			// Function is being defined.
			TopFunction->FunctionFlags |= FUNC_Defined;
			if (TopFunction->FunctionFlags & FUNC_Native)
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Native functions may only be declared, not defined") );
			}
		}

		NeedSemiColon=0;
	}

	if ( !bReparsingClass )
	{
		// Verify parameter list and return type compatibility within the
		// function, if any, that it overrides.
		for( INT i=NestLevel-2; i>=1; i-- )
		{
			for( TFieldIterator<UFunction> Function(Nest[i].Node); Function; ++Function )
			{
				// Allow private functions to be redefined.
				if
					(	Function->GetFName()==TopNode->GetFName()
					&&	*Function!=TopNode
					&&	(Function->FunctionFlags & FUNC_Private) )
				{
					TopNode->SuperStruct = NULL;
					goto Found;
				}

				// If the other function's name matches this one's, process it.
				if
					(	Function->GetFName()==TopNode->GetFName()
					&&	*Function!=TopNode
					&&	((Function->FunctionFlags ^ TopFunction->FunctionFlags) & (FUNC_Operator | FUNC_PreOperator))==0 )
				{
					// Check precedence.
					if( Function->OperPrecedence!=TopFunction->OperPrecedence && Function->NumParms==TopFunction->NumParms )
					{
						ReturnToLocation(FuncNameRetry);
						ScriptErrorf(SCEL_Restricted,  TEXT("Overloaded operator differs in precedence") );
					}

					// see if they both either have a return value or don't
					if ((TopFunction->GetReturnProperty() != NULL) != (Function->GetReturnProperty() != NULL))
					{
						ReturnToLocation(FuncNameRetry);
						ScriptErrorf(SCEL_Restricted,  TEXT("Redefinition of '%s %s' differs from original: return value mismatch"), NestName, FuncInfo.Function.Identifier );
					}
					// See if all parameters match.
					else if (TopFunction->NumParms!=Function->NumParms)
					{
						ReturnToLocation(FuncNameRetry);
						ScriptErrorf(SCEL_Restricted,  TEXT("Redefinition of '%s %s' differs from original; different number of parameters"), NestName, FuncInfo.Function.Identifier );
					}

					// Check all individual parameters.
					INT Count=0;
					for( TFieldIterator<UProperty> CurrentFuncParam(TopFunction),SuperFuncParam(*Function); Count<Function->NumParms; ++CurrentFuncParam,++SuperFuncParam,++Count )
					{
						if( !FPropertyBase(*CurrentFuncParam).MatchesType(FPropertyBase(*SuperFuncParam), 1) )
						{
							if( CurrentFuncParam->PropertyFlags & CPF_ReturnParm )
							{
								ReturnToLocation(FuncNameRetry);
								ScriptErrorf(SCEL_Restricted,  TEXT("Redefinition of %s %s differs only by return type"), NestName, FuncInfo.Function.Identifier );
							}
							else if( !(FuncInfo.FunctionFlags & FUNC_Operator) )
							{
								ReturnToLocation(FuncNameRetry);
								ScriptErrorf(SCEL_Restricted,  TEXT("Redefinition of '%s %s' differs from original"), NestName, FuncInfo.Function.Identifier );
							}
							break;
						}
						else if ( CurrentFuncParam->HasAnyPropertyFlags(CPF_OutParm) != SuperFuncParam->HasAnyPropertyFlags(CPF_OutParm) )
						{
							ReturnToLocation(FuncNameRetry);
							ScriptErrorf(SCEL_Restricted, TEXT("Redefinition of '%s %s' differs from original - 'out' mismatch on parameter %i"), NestName, FuncInfo.Function.Identifier, Count + 1);
						}
						else if (CurrentFuncParam->HasAnyPropertyFlags(CPF_OptionalParm) != SuperFuncParam->HasAnyPropertyFlags(CPF_OptionalParm))
						{
							ReturnToLocation(FuncNameRetry);
							ScriptErrorf(SCEL_Restricted, TEXT("Redefinition of '%s %s' differs from original - 'optional' mismatch on parameter %i"), NestName, FuncInfo.Function.Identifier, Count + 1);
						}
					}
					if( Count<TopFunction->NumParms )
					{
						continue;
					}

#if 0
					// if super version is event, overridden version must be defined as event (check before inheriting FUNC_Event)
					if ( (Function->FunctionFlags & FUNC_Event) && !(FuncInfo.FunctionFlags & FUNC_Event) )
					{
						ScriptWarnf(SCWL_Level4,TEXT("Superclass version is defined as an event so '%s' should be!"), FuncInfo.Function.Identifier);
					}
#endif
					// Function flags to copy from parent.
					FuncInfo.FunctionFlags |= (Function->FunctionFlags & FUNC_FuncInherit);

					// Make sure the replication conditions aren't being redefined
					if ((FuncInfo.FunctionFlags & FUNC_NetFuncFlags) != (Function->FunctionFlags & FUNC_NetFuncFlags))
					{
						ScriptErrorf(SCEL_Restricted, TEXT("Redefinition of replication conditions for function '%s'"), FuncInfo.Function.Identifier);
					}
					FuncInfo.FunctionFlags |= (Function->FunctionFlags & FUNC_NetFuncFlags);

					// Are we overriding a function?
					if( TopNode==Function->GetOuter() )
					{
						// Duplicate.
						ReturnToLocation( FuncNameRetry );
						ScriptErrorf(SCEL_Unknown/*SCEL_NestLevel*/,  TEXT("Duplicate function '%s'"), *Function->GetName() );
					}
					else
					{
						// Overriding an existing function.
						if( Function->FunctionFlags & FUNC_Final )
						{
							ReturnToLocation(FuncNameRetry);
							ScriptErrorf(SCEL_Restricted,  TEXT("%s: Can't override a 'final' function"), *Function->GetName() );
						}
					}

					// Balk if required specifiers differ.
					if ((Function->FunctionFlags & FUNC_FuncOverrideMatch) != (FuncInfo.FunctionFlags & FUNC_FuncOverrideMatch))
					{
						ScriptErrorf(SCEL_Restricted, TEXT("Function '%s' specifiers differ from original"), *Function->GetName());
					}

					// if super version is simulated, overridden version must be simulated
					if ( (Function->FunctionFlags & FUNC_Simulated) && !(FuncInfo.FunctionFlags & FUNC_Simulated) )
					{
						ScriptWarnf(SCWL_Level4,TEXT("Superclass version is simulated so '%s' should be!"), FuncInfo.Function.Identifier);
					}

					// Here we have found the original.
					TopNode->SuperStruct = *Function;
					goto Found;
				}
			}
		}
	Found:

		// Bind the function.
		TopFunction->Bind();
	}

	// if this function is an RPC in state scope, verify that it is an override
	// this is required because the networking code only checks the class for RPCs when initializing network data, not any states within it
	if ((TopFunction->FunctionFlags & FUNC_Net) && TopFunction->GetSuperFunction() == NULL && Cast<UClass>(TopFunction->GetOuter()) == NULL)
	{
		ScriptErrorf(SCEL_Restricted, TEXT("Function '%s': Base implementation of RPCs cannot be in a state. Add a stub outside state scope."), *TopFunction->GetName());
	}

	// If declaring a function, end the nesting.
	if( !(TopFunction->FunctionFlags & FUNC_Defined) )
	{
		PopNest( NEST_Function, NestName );
	}
}


/**
 * Parses and compiles a state declaration
 *
 * @param	Token			[out] contains information about the new state
 * @param	NeedSemicolon	[out] whether a semicolon is still required
 */
void FScriptCompiler::CompileStateDeclaration( FToken& Token, UBOOL& NeedSemiColon )
{
	// State block.
	check(TopNode!=NULL);
	CheckAllow( TEXT("'State'"), ALLOW_State );
	DWORD StateFlags=0, GotState=0;
	FStateInfo StateInfo;

	// Process all specifiers.
	for( ;; )
	{
		if( Token.Matches(NAME_State) )
		{
			GotState=1;
			if( MatchSymbol(TEXT("(")) )
			{
				RequireSymbol( TEXT(")"), TEXT("'State'") );
				StateFlags |= STATE_Editable;
			}
		}
		else if( Token.Matches(NAME_Simulated) )
		{
			StateFlags |= STATE_Simulated;
		}
		else if( Token.Matches(NAME_Auto) )
		{
			StateFlags |= STATE_Auto;
		}
		else
		{
			UngetToken(Token);
			break;
		}
		GetToken(Token);
	}
	if( !GotState )
	{
		ScriptErrorf(SCEL_Unknown/*SCEL_Parse/NestLevel?*/,  TEXT("Missing 'State'") );
	}

	// Get name and default parent state.
	if (!GetIdentifier(StateInfo.StateToken))
	{
		ScriptErrorf(SCEL_Unknown/*SCEL_Parse/NestLevel?*/,  TEXT("Missing state name") );
	}
	UState* ParentState = Cast<UState>(FindField(TopNode, StateInfo.StateToken.Identifier, TRUE, UState::StaticClass(), TEXT("'state'")), CLASS_IsAUState);
	if (ParentState != NULL && ParentState->GetOwnerClass() == Class)
	{
		if (!bReparsingClass)
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Duplicate state '%s'"), StateInfo.StateToken.Identifier);
		}
	}

	// Check for 'extends' keyword.
	if (MatchIdentifier(NAME_Extends))
	{
		FToken ParentToken;
		if (ParentState != NULL && !bReparsingClass)
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("'Extends' not allowed here: state '%s' overrides version in parent class"), StateInfo.StateToken.Identifier);
		}
		if (!GetIdentifier(ParentToken))
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Missing parent state name") );
		}
		ParentState = Cast<UState>(FindField(TopNode, ParentToken.Identifier, TRUE, UState::StaticClass(), TEXT("'state'")), CLASS_IsAUState);
		if (ParentState == NULL)
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("'extends': Parent state '%s' not found"), ParentToken.Identifier );
		}
	}

	// Begin the state block.
	FName StateName(StateInfo.StateToken.Identifier, FNAME_Add, TRUE);
	PushNest(NEST_State, StateName, NULL);
	UState* State = CastChecked<UState>(TopNode);
	if (!bReparsingClass)
	{
		State->StateFlags |= StateFlags;
		State->SuperStruct = ParentState;
	}
	StateInfo.StateReference = State;
	ClassData->AddState(StateInfo);
	RequireSymbol(TEXT("{"), TEXT("'State'"));
	NeedSemiColon = 0;
	SupressDebugInfo = -1;
}

/**
 * Parse and compiles an ignore block declaration
 *
 * @param	Token			[out] contains information about the ignore block
 * @param	NeedSemicolon	[out] whether a semicolon is still required
 */
void FScriptCompiler::CompileIgnoreDeclaration( FToken& Token, UBOOL& NeedSemiColon )
{
	if ( bReparsingClass )
	{
		SkipStatements(0, TEXT("ignores"));
		NeedSemiColon = 0;
		return;
	}

	// Functions to ignore in this state.
	TArray<FName> IgnoredNames;
	CheckAllow( TEXT("'Ignores'"), ALLOW_Ignores );
	for( ; ; )
	{
		FToken IgnoreFunction;
		if( !GetToken(IgnoreFunction) )
		{
			ScriptErrorf(SCEL_Fatal,  TEXT("'Ignores': Missing probe function name") );
		}
		if (IgnoredNames.ContainsItem(IgnoreFunction.TokenName))
		{
			ScriptErrorf(SCEL_Restricted, TEXT("'Ignores': Function '%s' specified multiple times"), IgnoreFunction.Identifier);
		}
		else
		{
			IgnoredNames.AddItem(IgnoreFunction.TokenName);
			UBOOL bFoundIgnoredFunc = FALSE;
			for (INT i = NestLevel - 2; i >= 1 && !bFoundIgnoredFunc; i--)
			{
				for (TFieldIterator<UFunction> Function(Nest[i].Node); Function != NULL && !bFoundIgnoredFunc; ++Function)
				{
					if (Function->GetFName() == IgnoreFunction.TokenName)
					{
						// Verify that function is ignoreable.
						if (Function->FunctionFlags & FUNC_Final)
						{
							ScriptErrorf(SCEL_Restricted,  TEXT("'%s': Cannot ignore final functions"), *Function->GetName() );
						}

						// Insert empty function definition to intercept the call.
						PushNest( NEST_Function, Function->GetFName(), NULL );
						UFunction* TopFunction = ((UFunction*) TopNode );
						TopFunction->FunctionFlags    |= (Function->FunctionFlags & FUNC_FuncOverrideMatch);
						TopFunction->NumParms          = Function->NumParms;
						TopFunction->SuperStruct        = *Function;

						FFunctionData* FunctionData = ClassData->FindFunctionData(*Function);
						if (FunctionData != NULL)
						{
							FFuncInfo FuncInfo = FunctionData->GetFunctionData();
							FuncInfo.FunctionReference = TopFunction;
							ClassData->AddFunction(FuncInfo);
						}

						// Copy parameters.
						UField** PrevLink = &TopFunction->Children;
						check(*PrevLink==NULL);
						for( TFieldIterator<UProperty> It(*Function); It && (It->PropertyFlags & CPF_Parm); ++It )
						{
							UProperty* NewProperty=NULL;
							if( It->IsA(UByteProperty::StaticClass()) )
							{
								NewProperty = new(TopFunction,It->GetFName(),RF_Public)UByteProperty;
								((UByteProperty*)NewProperty)->Enum = ((UByteProperty*)*It)->Enum;
							}
							else if( It->IsA(UIntProperty::StaticClass()) )
							{
								NewProperty = new(TopFunction,It->GetFName(),RF_Public)UIntProperty;
							}
							else if( It->IsA(UBoolProperty::StaticClass()) )
							{
								NewProperty = new(TopFunction,It->GetFName(),RF_Public)UBoolProperty;
							}
							else if( It->IsA(UFloatProperty::StaticClass()) )
							{
								NewProperty = new(TopFunction,It->GetFName(),RF_Public)UFloatProperty;
							}
							else if( It->IsA(UClassProperty::StaticClass()) )
							{
								NewProperty = new(TopFunction,It->GetFName(),RF_Public)UClassProperty;
								((UObjectProperty*)NewProperty)->PropertyClass = ((UObjectProperty*)*It)->PropertyClass;
								((UClassProperty*)NewProperty)->MetaClass = ((UClassProperty*)*It)->MetaClass;
							}
							else if( Cast<UObjectProperty>(*It, CLASS_IsAUObjectProperty) )
							{
								NewProperty = new(TopFunction,It->GetFName(),RF_Public)UObjectProperty;
								((UObjectProperty*)NewProperty)->PropertyClass = ((UObjectProperty*)*It)->PropertyClass;
							}
							else if ( Cast<UNameProperty>(*It) )
							{
								NewProperty = new(TopFunction,It->GetFName(),RF_Public) UNameProperty;
							}
							else if( It->IsA(UStrProperty::StaticClass()) )
							{
								NewProperty = new(TopFunction,It->GetFName(),RF_Public)UStrProperty;
							}
							else if( Cast<UStructProperty>(*It, CLASS_IsAUStructProperty) )
							{
								NewProperty = new(TopFunction,It->GetFName(),RF_Public)UStructProperty;
								((UStructProperty*)NewProperty)->Struct = ((UStructProperty*)*It)->Struct;
							}
							else if (It->IsA(UInterfaceProperty::StaticClass()))
							{
								NewProperty = new(TopFunction,It->GetFName(),RF_Public) UInterfaceProperty;
								((UInterfaceProperty*)NewProperty)->InterfaceClass = ((UInterfaceProperty*)*It)->InterfaceClass;
							}
							else if ( It->IsA(UMapProperty::StaticClass()) )
							{
								ScriptErrorf(SCEL_Restricted, TEXT("Using maps as function parameters is currently unsupported!"));
							}
							else
							{
								ScriptErrorf(SCEL_Unknown/*SCEL_Parse/Expression?*/,  TEXT("Unknown property type %s"), *It->GetClass()->GetName() );
							}
							NewProperty->ArrayDim = It->ArrayDim;
							NewProperty->PropertyFlags = It->PropertyFlags;
							if ( NewProperty->HasAnyPropertyFlags(CPF_OptionalParm) )
							{
								TopFunction->FunctionFlags |= FUNC_HasOptionalParms;
							}
							*PrevLink              = NewProperty;
							PrevLink = &(*PrevLink)->Next;
						}

						// Finish up.
						PopNest( NEST_Function, TEXT("Ignores") );
						bFoundIgnoredFunc = TRUE;
					}
				}
			}
			if (!bFoundIgnoredFunc)
			{
				ScriptErrorf(SCEL_Parse,  TEXT("'Ignores': '%s' is not a function"), IgnoreFunction.Identifier);
			}
		}

		// More?
		if( !MatchSymbol(TEXT(",")) )
		{
			break;
		}
	}
}

/**
 * Parses optional alternate text specified for various variable modifiers that will modify the way
 * the variable is exported to the header file.
 *
 * @param	ExportFlags			will be set with EPropertyHeaderExportFlags that are parsed
 * @param	ParseErrorHint		the text to use in parsing error messages
 *
 * @return	TRUE if additional export text was specified
 */
UBOOL FScriptCompiler::ParsePropertyExportText( DWORD& ExportFlags, const TCHAR* ParseErrorHint )
{
	UBOOL bResult = FALSE;

	FScriptLocation DeclarationPosition;
	if ( MatchSymbol(TEXT("{")) )
	{
		FToken ModifierHeaderText;

		// grab everything from here up to the closing brace
		if ( !GetRawToken(ModifierHeaderText, TCHAR('}')) )
		{
			ReturnToLocation(DeclarationPosition, FALSE, TRUE);
			ScriptErrorf(SCEL_Unknown, TEXT("No header text specified for modifier '%s'"), ParseErrorHint);
		}

		if ( appStricmp(ModifierHeaderText.String,TEXT("private")) == 0 )
		{
			ExportFlags |= PROPEXPORT_Private;
			ExportFlags &= ~(PROPEXPORT_Public|PROPEXPORT_Protected);
		}
		else if ( appStricmp(ModifierHeaderText.String,TEXT("protected")) == 0 )
		{
			ExportFlags |= PROPEXPORT_Protected;
			ExportFlags &= ~(PROPEXPORT_Public|PROPEXPORT_Private);
		}
		else if ( appStricmp(ModifierHeaderText.String,TEXT("public")) == 0 )
		{
			ExportFlags |= PROPEXPORT_Public;
			ExportFlags &= ~(PROPEXPORT_Private|PROPEXPORT_Protected);
		}
		else
		{
			ReturnToLocation(DeclarationPosition, FALSE, TRUE);
			ScriptErrorf(SCEL_Unknown/*SCEL_Parse*/, TEXT("Invalid header text specified for modifier '%s'"), ParseErrorHint);
		}

		RequireSymbol( TEXT("}"), *FString::Printf(TEXT("' %s modifier'"), ParseErrorHint) );
		bResult = TRUE;
	}

	return bResult;
}

/**
 * Parses optional alternate text to be used for exporting a variable declaration.
 *
 * @param	VarProperty			the token corresponding to the variable declaration
 * @param	PropertyTypeName	the type of property being parsed (used for logging)
 *
 * @return	TRUE if additional export text was specified
 */
UBOOL FScriptCompiler::ParsePropertyExportText( FPropertyBase& VarProperty , const TCHAR* PropertyTypeName )
{
	UBOOL bResult = FALSE;
	if ( MatchSymbol(TEXT("{")) )
	{
		FToken PropertyType;

		// grab everything from here up to the closing brace
		if ( !GetRawToken(PropertyType, TCHAR('}')) )
		{
			ScriptErrorf(SCEL_Unknown, TEXT("'%s': No %s type specified"), PropertyTypeName, PropertyTypeName);
		}

		VarProperty.ExportInfo = PropertyType.String;
		RequireSymbol( TEXT("}"), *FString::Printf(TEXT("' %s type'"), PropertyTypeName) );
		bResult = TRUE;
	}

	return bResult;
}

/** Clears out the stored comment. */
void FScriptCompiler::ClearComment()
{
	// Can't call Reset as FString uses protected inheritance
	PrevComment.Empty( PrevComment.Len() );
	bPrevCommentFormatted = FALSE;
}


/**
 * Ensures at script compile time that the metadata formatting is correct
 * @param	InKey			the metadata key being added
 * @param	InValue			the value string that will be associated with the InKey
 */
void FScriptCompiler::ValidateMetaDataFormat (const FString& InKey, const FString& InValue)
{
	if ((InKey == TEXT("UIMin")) || (InKey == TEXT("UIMax")) || (InKey == TEXT("ClampMin")) || (InKey == TEXT("ClampMax")))
	{
		if (!InValue.IsNumeric())
		{
			ScriptErrorf(SCEL_Unknown,  TEXT("Metadata value for '%s' is non-numeric : %s"), *InKey, *InValue);
		}
	}
}

/**
 * Parses optional metadata text.
 *
 * @param	VarProperty			the token corresponding to the variable declaration
 * @param	PropertyTypeName	the type of property being parsed (used for logging)
 *
 * @return	TRUE if metadata was specified
 */
UBOOL FScriptCompiler::ParsePropertyMetaData(FPropertyBase& VarProperty, const TCHAR* PropertyTypeName)
{
	UBOOL bResult = TRUE;
	if ( MatchSymbol(TEXT("<")) )
	{
		FToken PropertyMetaData;

		if (!GetRawToken(PropertyMetaData, TCHAR('>')))
		{
			ScriptErrorf(SCEL_Unknown, TEXT("'%s': No metadata specified"), PropertyTypeName);
		}

		RequireSymbol( TEXT(">"), *FString::Printf(TEXT("' %s metadata'"), PropertyTypeName) );

		// parse apart the string
		TArray<FString> Pairs;
		// break apart on | to get to the key/value pairs
		FString(PropertyMetaData.String).ParseIntoArray(&Pairs, TEXT("|"), TRUE);

		// go over all pairs
		for (INT PairIndex = 0; PairIndex < Pairs.Num(); PairIndex++)
		{
			// break the pair into a key and a value
			FString Token = Pairs(PairIndex);
			FString Key = Token;
			// by default, not value, just a key (allowed)
			FString Value;

			// look for a value after an =
			INT Equals = Token.InStr(TEXT("="));
			// if we have an =, break up the string
			if (Equals != -1)
			{
				Key = Token.Left(Equals);
				Value = Token.Right((Token.Len() - Equals) - 1);
			}

			// make sure the key is valid
			if (Key.Len() == 0)
			{
				return FALSE;
			}
			
			// trim extra white space and quotes
			Key = Key.Trim().TrimTrailing();
			Value = Value.Trim().TrimTrailing();
			Value = Value.TrimQuotes();

			ValidateMetaDataFormat(Key, Value);

			// finally we have enough to put it into our metadat
			VarProperty.MetaData.Set(FName(*Key), *Value);
		}

		bResult = TRUE;
	}

	return bResult;
}

/**
 * Extract only valid unrealscript from the input stream
 *
 * @param	StartPos	position into the Input array to start
 * @param	StartLine	line number to start on
 * @param	Count		how far to go before stopping
 *
 * @return	compact unrealscript text stripped of all extra stuff (whitespace, comments, new lines, etc.)
 */
FString FScriptCompiler::StripExpressionText( INT StartPos, INT StartLine, INT Count )
{
	FString Result;

	INT RestoreInputPos = InputPos;
	INT RestoreInputLine = InputLine;

	InputPos = StartPos;
	InputLine = StartLine;

	for ( TCHAR c = GetLeadingChar(); InputPos <= StartPos + Count; c = GetLeadingChar() )
	{
		Result += c;
	}

	InputPos = RestoreInputPos;
	InputLine = RestoreInputLine;

	return Result;
}

/**
 * Compiles the default value for an optional function parameter.
 *
 * @param	Prop			the function parameter to compile
 * @param	RequiredType	info used in typechecking
 */
void FScriptCompiler::CompileParameterValue( UProperty* Prop, FPropertyBase& RequiredType )
{
	FScriptLocation ParameterValueStart;

	FTokenData* Token = ClassData->FindTokenData(Prop);
	check(Token);

	FDefaultParameterValue* Value = Token->DefaultValue;
	check(Value);

	// since this expression will be evaluated in the context of the object that owns this function
	// (rather than the context of the caller, which is where we'll be in the VM at this point), the
	// UDebugger needs to know about the context switch
	EmitDebugInfo(DI_NewStack);

	// if we are here, then a default value was specified for this parameter in the function declaration; therefore, the parameter expression is no longer
	// optional, or stuff like MyOptionalParmValue=SomeUnknownIdentifier would compile fine
	QWORD OptionalFlag = (RequiredType.PropertyFlags&CPF_OptionalParm);
	RequiredType.PropertyFlags &= ~CPF_OptionalParm;

	// count this reference as an assignment - primarily used by the delegate handling to decide whether to convert between delegate function and property
	RequiredType.ReferenceType = CPRT_AssignmentReference;

	// compile the default parameter value into the FTokenData
	CompileExpr( RequiredType, TEXT("'='"), NULL, MAXINT, &Token->Token, FALSE, &Value->ParsedExpression );

	// restore the CPF_OptionalParm, if it was present
	RequiredType.PropertyFlags |= OptionalFlag;

	EmitDebugInfo(DI_PrevStack);

	Writer << EX_EndParmValue;

	INT Count = InputPos - ParameterValueStart.InputPos;
	Token->DefaultValue->RawExpression = StripExpressionText(ParameterValueStart.InputPos, ParameterValueStart.InputLine, Count);
}

/**
 * Parses and compiles a global or local property declaration.
 *
 * @param	Token			[out] contains information about the new property
 * @param	NeedSemicolon	[out] whether we still need to parse the semicolon
 */
void FScriptCompiler::CompileVariableDeclaration( FToken& Token, UBOOL& NeedSemiColon )
{
	// Variable definition.
	QWORD Disallow;
	if( Token.Matches(NAME_Var) )
	{
		// Declaring per-object variables.
		CheckAllow( TEXT("'Var'"), ALLOW_VarDecl );
		if( TopNest->NestType != NEST_Class )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Instance variables are only allowed at class scope (use 'local'?)") );
		}
		Disallow = CPF_ParmFlags;
	}
	else
	{
		// Declaring local variables.
		CheckAllow( TEXT("'Local'"), ALLOW_VarDecl );
		if( TopNest->NestType == NEST_Class )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Local variables are only allowed in functions") );
		}
		Disallow	= ~0;
	}

	// Get category, if any.
	FName EdCategory = NAME_None;
	QWORD EdFlags    = 0;
	if( MatchSymbol(TEXT("(")) )
	{
		// Get optional property editing category.
		EdFlags |= CPF_Edit;

		FString CategoryPath;
		do
		{
			FToken CategoryToken;
			if( GetIdentifier( CategoryToken, 1 ) )
			{
				if( CategoryPath.Len() > 0 )
				{
					// Character used to deliminate sub-categories in category path names
					// NOTE: Must match FPropertyNodeConstants::CategoryDelimiterChar
					const TCHAR CategoryDelimiterChar( ',' );
					CategoryPath.AppendChar( CategoryDelimiterChar );
				}
				CategoryPath += CategoryToken.Identifier;
			}
			else
			{
				EdCategory = Class->GetFName();
				break;
			}
		}
		while( MatchSymbol(TEXT(",")) );
		RequireSymbol( TEXT(")"), TEXT("Missing ')' after editable category") );

		if( EdCategory == NAME_None )
		{
			EdCategory = FName( *CategoryPath );
		}
	}

	// Compile the variable type.
	FPropertyBase OriginalProperty(CPT_None);
	EObjectFlags ObjectFlags=0;
	GetVarType( TopNode, OriginalProperty, ObjectFlags, Disallow, TEXT("Variable declaration") );
	OriginalProperty.PropertyFlags |= EdFlags;

	// If editable but no category was specified, the category name is our class name.
	if( (OriginalProperty.PropertyFlags & CPF_Edit) && (EdCategory==NAME_None) )
	{
		EdCategory = Class->GetFName();
	}

	// Validate combinations.
	if( (OriginalProperty.PropertyFlags & (CPF_Transient|CPF_Native)) && TopNest->NestType!=NEST_Class )
	{
		ScriptErrorf(SCEL_Restricted,  TEXT("Static and local variables may not be transient or native") );
	}
	if( OriginalProperty.PropertyFlags & CPF_ParmFlags )
	{
		ScriptErrorf(SCEL_Restricted,  TEXT("Illegal type modifiers in variable") );
	}

	// Process all variables of this type.
	TArray<UProperty*> NewProperties;
	do
	{
		FPropertyBase Property = OriginalProperty;
		UProperty* newProperty = GetVarNameAndDim( TopNode, Property, ObjectFlags, FALSE, FALSE, NULL, TEXT("Variable declaration"), EdCategory, FALSE );
		NewProperties.AddItem( newProperty );
		// we'll need any metadata tags we parsed later on when we call ConvertEOLCommentToTooltip() so the tags aren't clobbered
		OriginalProperty.MetaData = Property.MetaData;

		// Keep track of which line number this local property is declared on.  The declaration
		// line number will be referenced in a log warning if this local property is never referenced.
		if ( TopNest->NestType == NEST_Function )
		{
			LocalPropertyLineNumbers.Set(newProperty, InputLine);
		}

	} while( MatchSymbol(TEXT(",")) );

	RequireSymbol( TEXT(";"), TEXT("'var'") );
	NeedSemiColon = 0;

	// Process any remaining input on this line, potentially treating it as tooltip metadata.
	ConvertEOLCommentToTooltip( OriginalProperty, NewProperties );
}

//
// Compile a command in Token. Handles any errors that may occur.
//
void FScriptCompiler::CompileCommand( FToken& Token, UBOOL& NeedSemicolon )
{
	check(Pass==PASS_Compile);
	if( Token.Matches(NAME_Switch) )
	{
		// Switch.
		CheckAllow( TEXT("'Switch'"), ALLOW_Cmd );
		PushNest( NEST_Switch, TEXT(""), NULL );

		// Compile the select-expression.
		EmitDebugInfo(DI_Switch);
		Writer << EX_Switch;
		FScriptLocation StartOfExpression;

		RequireSymbol(TEXT("("), TEXT("'switch expression'"));
		CompileExpr( FPropertyBase(CPT_None,CPRT_SimpleReference), TEXT("'Switch'"), &TopNest->SwitchType, MAXINT, NULL, FALSE, NULL, TRUE );
		if( TopNest->SwitchType.ArrayDim != 1 )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Can't switch on arrays") );
		}
		RequireSymbol(TEXT(")"), TEXT("'switch expression'"));
		FScriptLocation NextExpression;

		EmitProperty( TopNest->SwitchType, TEXT("'Switch'") );
		MoveCompiledCode(StartOfExpression, NextExpression);
		TopNest->SwitchType.PropertyFlags &= ~(CPF_OutParm);

		// Get bracket.
		RequireSymbol( TEXT("{"), TEXT("'Switch'") );
		NeedSemicolon=0;

	}
	else if( Token.Matches(NAME_Case) )
	{
		CheckAllow( TEXT("'Case'"), ALLOW_Case );

		// Update previous Case's chain address.
		EmitChainAddressValue(TopNest);

		// Insert this case statement and prepare to chain it to the next one.
		Writer << EX_Case;
		EmitPlaceholderForChainAddress(TopNest);
		CompileExpr( TopNest->SwitchType, TEXT("'Case'"), NULL, MAXINT, NULL, FALSE, NULL, TRUE );
		RequireSymbol( TEXT(":"), TEXT("'Case'") );
		NeedSemicolon=0;

		TopNest->Allow |= ALLOW_Cmd | ALLOW_Label | ALLOW_Break;
	}
	else if( Token.Matches(NAME_Default) && (TopNest->Allow & ALLOW_Case) && !PeekSymbol(TEXT(".")) )
	{
		// Default case.
		CheckAllow( TEXT("'Default'"), ALLOW_Case );

		// Update previous Case's chain address.
		EmitChainAddressValue(TopNest);

		// Emit end-of-case marker.
		Writer << EX_Case;
		CodeSkipSizeType W=MAXWORD; Writer << W;
		RequireSymbol( TEXT(":"), TEXT("'Default'") );
		NeedSemicolon=0;

		// Don't allow additional Cases after Default.
		TopNest->Allow &= ~ALLOW_Case;
		TopNest->Allow |=  ALLOW_Cmd | ALLOW_Label | ALLOW_Break;
	}
	else if( Token.Matches(NAME_Return) )
	{
		// Only valid from within a function or operator.
		CheckAllow( TEXT("'Return'"), ALLOW_Return );
		INT i;
		for( i=NestLevel-1; i>0; i-- )
		{
			if( Nest[i].NestType==NEST_Function )
			{
				break;
			}
			else if( Nest[i].NestType==NEST_ForEach )
			{
				Writer << EX_IteratorPop;
			}
		}
		if( i <= 0 )
		{
			ScriptErrorf(SCEL_Fatal,  TEXT("Internal consistency error on 'Return'") );
		}
		UFunction* Function = CastChecked<UFunction>(Nest[i].Node);
		UProperty* Return = Function->GetReturnProperty();
		if ( Return )
		{
			EmitDebugInfo(DI_Return);
		}
		else
		{
			EmitDebugInfo(DI_ReturnNothing);
		}

		Writer << EX_Return;
		if( Return )
		{
			// Return expression.

			//@todo reference type should ideally be AssignmentReference, but that causes
			// false positives when the function requires a return type that doesn't have
			// a null constant value (like array or struct), since an uninitialized local
			// property of that type is used as the return value, which should be considered
			// valid usage.  Unfortunate - better would be to create null constant values for
			// each type.
//			FPropertyBase Property(Return,CPRT_AssignmentReference);

			FPropertyBase Property(Return,CPRT_SimpleReference);
			Property.PropertyFlags &= ~CPF_OutParm;
			CompileExpr( Property, TEXT("'Return'") );
			//if (TopNest->NestType == NEST_Function) // very very strict mode
				Nest[i].NestFlags |= NESTF_ReturnValueFound;
		}
		else
		{
			// No return value.
			Writer << EX_Nothing;
		}
		EmitDebugInfo(DI_PrevStack);
	}
	else if( Token.Matches(NAME_If) )
	{
		// If.
		CheckAllow( TEXT("'If'"), ALLOW_Cmd );
		PushNest( NEST_If, TEXT(""), NULL );

		// Jump to next evaluator if expression is false.
		EmitDebugInfo(DI_SimpleIf);
		Writer << EX_JumpIfNot;
		EmitPlaceholderForChainAddress(TopNest);

		// Compile boolean expr.
		RequireSymbol( TEXT("("), TEXT("'If'") );
		CompileExpr( FPropertyBase(CPT_Bool,CPRT_SimpleReference), TEXT("'If'"), NULL, MAXINT, NULL, FALSE, NULL, TRUE );
		RequireSymbol( TEXT(")"), TEXT("'If'") );

		// Handle statements.
		NeedSemicolon = 0;
		if( !MatchSymbol( TEXT("{") ) )
		{
			CompileStatements();
			PopNest( NEST_If, TEXT("'If'") );
		}
	}
	else if( Token.Matches(NAME_While) )
	{
		CheckAllow( TEXT("'While'"), ALLOW_Cmd );
		PushNest( NEST_Loop, TEXT(""), NULL );
		TopNest->Allow |= ALLOW_InWhile;

		// Here is the start of the loop.
		TopNest->SetJumpTargetValue(FIXUP_LoopStart,TopNode->Script.Num());

		// Evaluate expr and jump to end of loop if false.
		EmitDebugInfo(DI_While);
		Writer << EX_JumpIfNot;
		EmitPlaceholderForJumpAddress(TopNest,FIXUP_LoopEnd,NAME_None);

		// Compile boolean expr.
		RequireSymbol( TEXT("("), TEXT("'While'") );
		CompileExpr( FPropertyBase(CPT_Bool, CPRT_SimpleReference), TEXT("'While'"), NULL, MAXINT, NULL, FALSE, NULL, TRUE );
		RequireSymbol( TEXT(")"), TEXT("'While'") );

		// Handle statements.
		NeedSemicolon=0;
		if( !MatchSymbol(TEXT("{")) )
		{
			CompileStatements();
			PopNest( NEST_Loop, TEXT("'While'") );
		}

	}
	else if(Token.Matches(NAME_Do))
	{
		CheckAllow( TEXT("'Do'"), ALLOW_Cmd );
		PushNest( NEST_Loop, TEXT(""), NULL );

		TopNest->SetJumpTargetValue(FIXUP_LoopStart,TopNode->Script.Num());

		// Handle statements.
		NeedSemicolon=0;
		if( !MatchSymbol(TEXT("{")) )
		{
			CompileStatements();
			PopNest( NEST_Loop, TEXT("'Do'") );
		}

	}
	else if( Token.Matches(NAME_Break) )
	{
		CheckAllow( TEXT("'Break'"), ALLOW_Break );

		// Find the nearest For or Loop.
		INT iNest = FindNest(NEST_Loop);
		iNest     = Max(iNest,FindNest(NEST_For    ));
		iNest     = Max(iNest,FindNest(NEST_ForEach));
		iNest     = Max(iNest,FindNest(NEST_Switch ));
		check(iNest>0);

		switch ( Nest[iNest].NestType )
		{
		case NEST_Switch:
			EmitDebugInfo(DI_BreakSwitch);
			break;

		case NEST_ForEach:
			EmitDebugInfo(DI_BreakForEach);
			break;

		case NEST_For:
			EmitDebugInfo(DI_BreakFor);
			break;

		case NEST_Loop:
			EmitDebugInfo(DI_BreakLoop);
			break;
		}


		// Jump to the loop's end.
		Writer << EX_Jump;
		switch ( Nest[iNest].NestType )
		{
		case NEST_Switch:
			EmitPlaceholderForJumpAddress(&Nest[iNest],FIXUP_SwitchEnd,NAME_None);
			break;

		case NEST_ForEach:
			EmitPlaceholderForJumpAddress(&Nest[iNest],FIXUP_IteratorEnd,NAME_None);
			break;

		case NEST_For:
			EmitPlaceholderForJumpAddress(&Nest[iNest],FIXUP_ForEnd,NAME_None);
			break;

		case NEST_Loop:
			EmitPlaceholderForJumpAddress(&Nest[iNest],FIXUP_LoopEnd,NAME_None);
			break;

		default:
			EmitPlaceholderForJumpAddress(TopNest,FIXUP_SwitchEnd,NAME_None);
			break;
		}
	}
	else if( Token.Matches(NAME_Continue) )
	{
		CheckAllow( TEXT("'Continue'"), ALLOW_Continue );

		// Find the nearest For or Loop.
		INT iNest = FindNest(NEST_Loop);
		iNest     = Max(iNest,FindNest(NEST_For    ));
		iNest     = Max(iNest,FindNest(NEST_ForEach));
		check(iNest>0);

		// Jump to the loop's start.
		switch ( Nest[iNest].NestType )
		{
		case NEST_ForEach:
			EmitDebugInfo(DI_ContinueForeach);

			Writer << EX_IteratorNext;
			Writer << EX_Jump;
			EmitPlaceholderForJumpAddress( &Nest[iNest], FIXUP_IteratorEnd, NAME_None );
			break;

		case NEST_For:
			EmitDebugInfo(DI_ContinueFor);

			Writer << EX_Jump;
			EmitPlaceholderForJumpAddress( &Nest[iNest], FIXUP_ForInc, NAME_None );
			break;

		case NEST_Loop:
			EmitDebugInfo(DI_ContinueLoop);

			Writer << EX_Jump;
			EmitPlaceholderForJumpAddress( &Nest[iNest], FIXUP_LoopPostCond, NAME_None );
			break;
		}
	}
	else if(Token.Matches(NAME_For))
	{
		CheckAllow( TEXT("'For'"), ALLOW_Cmd );
		PushNest( NEST_For, TEXT(""), NULL );

		// Compile for parms.
		RequireSymbol( TEXT("("), TEXT("'For'") );
			EmitDebugInfo(DI_ForInit);
			// Suppress the EX_Let's debug info.
			SupressDebugInfo++;
			CompileAffector();
		RequireSymbol( TEXT(";"), TEXT("'For'") );
			TopNest->SetJumpTargetValue(FIXUP_ForStart,TopNode->Script.Num());
			EmitDebugInfo(DI_ForEval);
			Writer << EX_JumpIfNot;
			EmitPlaceholderForJumpAddress(TopNest,FIXUP_ForEnd,NAME_None);
			CompileExpr( FPropertyBase(CPT_Bool, CPRT_SimpleReference), TEXT("'For'"), NULL, MAXINT, NULL, FALSE, NULL, TRUE );
		RequireSymbol( TEXT(";"), TEXT("'For'") );

			// Skip the increment expression text but not code.
		// We can't emit a debug info here for the for loop inc, we have to do it in popnest
			InitScriptLocation(TopNest->ForRetry);
			CompileAffector(CPRT_AssignValue, CPRT_DualReference);
			ReturnToLocation(TopNest->ForRetry,1,0);
		RequireSymbol( TEXT(")"), TEXT("'For'") );

		// Handle statements.

		NeedSemicolon=0;
		if( !MatchSymbol(TEXT("{")) )
		{
			CompileStatements();
			PopNest( NEST_For, TEXT("'If'") );
		}

	}
	else if( Token.Matches(NAME_ForEach) )
	{
		CheckAllow( TEXT("'ForEach'"), ALLOW_Cmd );
		PushNest( NEST_ForEach, TEXT(""), NULL );

		FToken TypeToken;
        FScriptLocation LowRetry;
        // compile the expression
        CompileExpr( FPropertyBase(CPT_None,CPRT_AssignmentReference), TEXT("'ForEach'"), &TypeToken, MAXINT, NULL, FALSE, NULL, TRUE );
        FScriptLocation HighRetry;
        // if we got an array
        if (TypeToken.IsDynamicArray())
        {
			// must be an l-value
			if (TypeToken.TokenFunction != NULL)
			{
				ScriptErrorf(SCEL_Restricted, TEXT("Cannot execute dynamic array iterator on the return value of a function"));
			}
			Writer << EX_DynArrayIterator;
			// put the op in front of the expression code
			MoveCompiledCode(LowRetry,HighRetry);
			// compile the remaining part of the expression
			RequireSymbol(TEXT("("), TEXT("'foreach array'"));
			// out item
			FToken OutToken;
			FPropertyBase RequiredType = FPropertyBase(TypeToken.Type, CPRT_AssignValue);
			RequiredType.PropertyClass = TypeToken.PropertyClass;
			RequiredType.MetaClass = TypeToken.MetaClass;
			RequiredType.PropertyFlags |= CPF_OutParm;
			CompileExpr(RequiredType, TEXT("'foreach array'"), &OutToken, MAXINT, NULL, FALSE, NULL, TRUE);
			BYTE IndexByte = 0;
			if (MatchSymbol(TEXT(",")))
			{
				// index is present
				IndexByte = 1;
				Writer << IndexByte;
				// compile the index property expression
				RequiredType = FPropertyBase(CPT_Int, CPRT_AssignValue);
				RequiredType.PropertyFlags |= CPF_OutParm;
				CompileExpr(RequiredType, TEXT("'foreach array'"), &OutToken, MAXINT, NULL, FALSE, NULL, TRUE);
			}
			else
			{
				// index isn't present
				Writer << IndexByte;
				// write out a dummy parm for serialization
				Writer << EX_EmptyParmValue;
			}
			RequireSymbol(TEXT(")"), TEXT("'foreach array'"));
        }
        else
        {
            Writer << EX_Iterator;
            // put the op in front of the expression code
            MoveCompiledCode(LowRetry,HighRetry);
            // make sure we hit an iterator function
            if( TopNest->Allow & ALLOW_Iterator )
            {
                ScriptErrorf(SCEL_Unknown/*SCEL_Parse*/,  TEXT("'ForEach': An iterator expression is required") );
            }
        }

		// Emit end offset.
		EmitPlaceholderForJumpAddress( TopNest, FIXUP_IteratorEnd, NAME_None );

		EmitDebugInfo(DI_EFPIter);

		// Handle statements.
		NeedSemicolon = 0;
		if( !MatchSymbol(TEXT("{")) )
		{
			CompileStatements();
			PopNest( NEST_ForEach, TEXT("'ForEach'") );
		}
	}
	else if( Token.Matches(NAME_Assert) )
	{
		CheckAllow( TEXT("'Assert'"), ALLOW_Cmd );
		EmitDebugInfo(DI_Assert);
		WORD wLine = InputLine;
		Writer << EX_Assert;
		Writer << wLine;
		// record whether we're in debug mode (Assert() only crashes in debug mode, otherwise it's just a log warning)
		BYTE bDebug = BYTE(bEmitDebugInfo);
		Writer << bDebug;
		// write the expression to be evaluated
		CompileExpr( FPropertyBase(CPT_Bool,CPRT_SimpleReference), TEXT("'Assert'") );
	}
	else if( Token.Matches(NAME_Goto) )
	{
		CheckAllow( TEXT("'Goto'"), ALLOW_Label );
		if( TopNest->Allow & ALLOW_StateCmd )
		{
			// Emit virtual state goto.
			Writer << EX_GotoLabel;
			CompileExpr( FPropertyBase(CPT_Name,CPRT_AssignmentReference), TEXT("'Goto'") );
		}
		else
		{
			// Get label list for this nest level.
			INT iNest;
			for( iNest=NestLevel-1; iNest>=2; iNest-- )
			{
				if( Nest[iNest].NestType==NEST_State || Nest[iNest].NestType==NEST_Function || Nest[iNest].NestType==NEST_ForEach )
				{
					break;
				}
			}
			if( iNest < 2 )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Goto is not allowed here") );
			}
			FNestInfo* LabelNest = &Nest[iNest];

			// Get label.
			FToken Label;
			if( !GetToken(Label) )
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Parse*/,  TEXT("Goto: Missing label") );
			}
			if( Label.TokenName == NAME_None )
			{
				Label.TokenName = FName( Label.Identifier );
			}
			if( Label.TokenName == NAME_None )
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Parse*/,  TEXT("Invalid label '%s'"), Label.Identifier );
			}

			// Emit final goto.
			Writer << EX_Jump;
			EmitPlaceholderForJumpAddress( LabelNest, FIXUP_Label, Label.TokenName );
		}
	}
	else if( Token.Matches(NAME_Stop) )
	{
		CheckAllow( TEXT("'Stop'"), ALLOW_StateCmd );
		EmitDebugInfo(DI_PrevStack);
		Writer << EX_Stop;
	}
	else if( Token.Matches(NAME_FilterEditorOnly) )
	{
		CheckAllow( TEXT("'FilterEditorOnly'"), ALLOW_Cmd );
		PushNest( NEST_FilterEditorOnly, TEXT(""), NULL );

		// Jump to next statement if we are not the editor
		EmitDebugInfo(DI_FilterEditorOnly);
		Writer << EX_JumpIfFilterEditorOnly;
		EmitPlaceholderForJumpAddress(TopNest,FIXUP_FilterEditorOnly,NAME_None);

		NeedSemicolon = 0;
		RequireSymbol( TEXT("{"), TEXT("'FilterEditorOnly'") );
	}
	else if( Token.Matches(TEXT("}")) )
	{
		// End of block.
		if( TopNest->NestType==NEST_Class || TopNest->NestType==NEST_Interface )
		{
			ScriptErrorf(SCEL_Class,  TEXT("Unexpected '}' at class scope") );
		}
		else if( TopNest->NestType==NEST_None )
		{
			ScriptErrorf(SCEL_Fatal,  TEXT("Unexpected '}' at global scope") );
		}
		PopNest( NEST_None, TEXT("'}'") );
		NeedSemicolon=0;
	}
	else if( Token.Matches(TEXT(";")) )
	{
		// Extra semicolon.
		NeedSemicolon=0;
	}
	else if( MatchSymbol(TEXT(":")) )
	{
		// A label.
		CheckAllow( TEXT("Label"), ALLOW_Label );

		// Validate label name.
		if( Token.TokenName == NAME_None )
		{
			Token.TokenName = FName( Token.Identifier );
		}
		if( Token.TokenName == NAME_None )
		{
			ScriptErrorf(SCEL_Unknown,  TEXT("Invalid label name '%s'"), Token.Identifier );
		}

		// Handle first label in a state.
		if( !(TopNest->Allow & ALLOW_Cmd ) )
		{
			// This is the first label in a state, so set the code start and enable commands.
			check(TopNest->NestType==NEST_State);
			TopNest->Allow     |= ALLOW_Cmd;
			TopNest->Allow     &= ~(ALLOW_Function | ALLOW_VarDecl | ALLOW_TypeDecl);
		}
		else if ( TopNest->Node->IsA(UState::StaticClass()) )
		{
   			EmitDebugInfo(DI_PrevStackLabel);
		}

		// Get label list for this nest level.
		INT iNest;
		for( iNest=NestLevel-1; iNest>=2; iNest-- )
		{
			if( Nest[iNest].NestType==NEST_State || Nest[iNest].NestType==NEST_Function || Nest[iNest].NestType==NEST_ForEach )
			{
				break;
			}
		}
		if( iNest < 2 )
		{
			ScriptErrorf(SCEL_Restricted,  TEXT("Labels are not allowed here") );
		}
		FNestInfo *LabelNest = &Nest[iNest];

		// Make sure the label is unique here.
		for( FLabelRecord *LabelRec = LabelNest->LabelList; LabelRec; LabelRec=LabelRec->Next )
		{
			if( LabelRec->Name == Token.TokenName )
			{
				ScriptErrorf(SCEL_Restricted,  TEXT("Duplicate label '%s'"), *Token.TokenName.ToString() );
			}
		}

		// Add label.
		LabelNest->LabelList = new(GMainThreadMemStack)FLabelRecord( Token.TokenName, TopNode->Script.Num(), LabelNest->LabelList );
		NeedSemicolon=0;
		if ( bEmitDebugInfo )
		{
			// emit nothing so that this debuginfo will not be read if the object isn't executing state code when GotoState() is called.
			Writer << EX_Nothing;
			EmitDebugInfo(DI_NewStackLabel);
		}
	}
	else
	{
		CheckAllow( TEXT("Expression"), ALLOW_Cmd );
		UngetToken(Token);

		// Try to compile an affector expression or assignment.
		CompileAffector();

	}

	// make sure the struct modification byte list got fully popped
	check(TopNest->StructModificationByteList == NULL);
}

//
// Compile a statement: Either a declaration or a command.
// Returns 1 if success, 0 if end of file.
//
UBOOL FScriptCompiler::CompileStatement()
{
	UBOOL NeedSemicolon = 1;

	// Get a token and compile it.
	FToken Token;
	if( !GetToken(Token,NULL,1) )
	{
		// End of file.
		return 0;
	}
	else if( !CompileDeclaration( Token, NeedSemicolon ) )
	{
		if( Pass == PASS_Parse )
		{
			// Skip this and subsequent commands so we can hit them on next pass.
			if( NestLevel < 3 )
			{
				ScriptErrorf(SCEL_Unknown/*SCEL_Formatting*/,  TEXT("Unexpected '%s'"), Token.Identifier );
			}
			UngetToken(Token);
			PopNest( TopNest->NestType, NestTypeName(TopNest->NestType) );
			SkipStatements( 1, NestTypeName(TopNest->NestType) );
			NeedSemicolon = 0;
		}
		else if ( Pass == PASS_Compile )
		{
			// Compile the command.
			CompileCommand( Token, NeedSemicolon );
		}
	}

	// Make sure no junk is left over.
	if( NeedSemicolon )
	{
		if( !MatchSymbol(TEXT(";")) )
		{
			if( GetToken(Token) )
			{
				ScriptErrorf(SCEL_Unknown,  TEXT("Missing ';' before '%s'"), Token.Identifier );
			}
			else
			{
				ScriptErrorf(SCEL_Unknown,  TEXT("Missing ';'") );
			}
		}
	}
	return 1;
}

//
// Compile multiple statements.
//
void FScriptCompiler::CompileStatements()
{
	INT OriginalNestLevel = NestLevel;
	do
	{
		CompileStatement();
	}
	while( NestLevel > OriginalNestLevel );
}

/*-----------------------------------------------------------------------------
	Probe mask building.
-----------------------------------------------------------------------------*/

//
// Generate probe bitmask for a script.
//
void FScriptCompiler::PostParse( UStruct* Node )
{
	// Allocate defaults.
	UScriptStruct* Struct = Cast<UScriptStruct>(Node);
	if  ( Struct != NULL )
	{
		const INT Size = Align(Struct->GetPropertiesSize(),Struct->GetMinAlignment()) - Struct->GetDefaultsCount();
		if( Size > 0 )
		{
			Struct->GetDefaultArray().AddZeroed(Size);
		}
	}

	// Handle functions.
	UFunction* ThisFunction = Cast<UFunction>(Node, CLASS_IsAUFunction);
	if( ThisFunction )
	{
		ThisFunction->ParmsSize=0;
		for( TFieldIterator<UProperty> It(ThisFunction); It; ++It )
		{
			if( It->PropertyFlags & CPF_Parm )
			{
				ThisFunction->ParmsSize = It->Offset + It->GetSize();
			}
			if( It->PropertyFlags & CPF_ReturnParm )
			{
				ThisFunction->ReturnValueOffset = It->Offset;
			}
		}
	}

	// Accumulate probe masks based on all functions in this state.
	UState* ThisState = Cast<UState>(Node, CLASS_IsAUState);
	if( ThisState )
	{
		for( TFieldIterator<UFunction> Function(ThisState); Function; ++Function )
		{
			if
			(	(Function->GetFName().GetIndex() >= NAME_PROBEMIN)
			&&	(Function->GetFName().GetIndex() <  NAME_PROBEMAX)
			&&  ((Function->FunctionFlags & FUNC_Defined) || (Function->FunctionFlags & FUNC_Native)) )
			{
				ThisState->ProbeMask |= (DWORD)1 << (Function->GetFName().GetIndex() - NAME_PROBEMIN);
			}
		}
	}

	// Recurse with all child states in this class.
	for( TFieldIterator<UStruct> It(Node); It && It.GetStruct()==Node; ++It )
	{
		PostParse( *It );
	}
}

/*-----------------------------------------------------------------------------
	Code skipping.
-----------------------------------------------------------------------------*/

/**
 * Skip over code, honoring { and } pairs.
 *
 * @param	NestCount	number of nest levels to consume. if 0, consumes a single statement
 * @param	ErrorTag	text to use in error message if EOF is encountered before we've done
 */
void FScriptCompiler::SkipStatements( INT NestCount, const TCHAR* ErrorTag  )
{
	FToken Token;

	INT OriginalNestCount = NestCount;

	while( GetToken( Token, NULL, 1 ) )
	{
		if ( Token.Matches(TEXT("{")) )
		{
			NestCount++;
		}
		else if	( Token.Matches(TEXT("}")) )
		{
			NestCount--;
		}
		else if ( Token.Matches(TEXT(";")) && OriginalNestCount == 0 )
		{
			break;
		}

		if ( NestCount < OriginalNestCount || NestCount < 0 )
			break;
	}

	if( NestCount > 0 )
	{
		ScriptErrorf(SCEL_NestLevel,  TEXT("Unexpected end of file at end of %s"), ErrorTag );
	}
	else if ( NestCount < 0 )
	{
		ScriptErrorf(SCEL_Formatting, TEXT("Extraneous closing brace found in %s"), ErrorTag);
	}
}

/*-----------------------------------------------------------------------------
	Main script compiling routine.
-----------------------------------------------------------------------------*/

//
// Perform a second-pass compile on the current class.
//
void FScriptCompiler::CompileSecondPass( UStruct* Node )
{
	// Restore code pointer to where it was saved in the parsing pass.
	INT StartNestLevel = NestLevel;

	// Push this new nesting level.
	ENestType NewNest=NEST_None;
	if( Node->IsA(UFunction::StaticClass()) )
	{
		NewNest = NEST_Function;
	}
	else if( Node == Class )
	{
		// determine if the Class is an interface or a class
		if (Class->ClassFlags & CLASS_Interface)
		{
			NewNest = NEST_Interface;
		}
		else
		{
			NewNest = NEST_Class;
		}
	}
	else if( Node->IsA(UState::StaticClass()) )
	{
		NewNest = NEST_State;
	}

	check(NewNest!=NEST_None);
	PushNest( NewNest, Node->GetFName(), Node );
	check(TopNode==Node);
	TopNode->Script.Empty();

	UFunction* TopFunction = Cast<UFunction>(TopNode, CLASS_IsAUFunction);
	if( TopFunction )
	{
		// Propagate function replication flags down, since they aren't known until the second pass.
		if ( TopFunction->GetSuperFunction() )
		{
			TopFunction->FunctionFlags &= ~FUNC_NetFuncFlags;
			TopFunction->FunctionFlags |= (TopFunction->GetSuperFunction()->FunctionFlags & FUNC_NetFuncFlags);
		}

		FScriptLocation PreviousLocation;
		try
		{
			for ( TFieldIterator<UProperty> It(TopFunction); It; ++It )
			{
				UProperty* prop = *It;
				if ( (prop->PropertyFlags&CPF_OptionalParm) != 0 )
				{
					FTokenData* TokenData = ClassData->FindTokenData(prop);

					if ( TokenData && TokenData->DefaultValue != NULL )
					{
						InputPos = TokenData->DefaultValue->InputPos;
						InputLine = TokenData->DefaultValue->InputLine;
						Writer << EX_DefaultParmValue;

						FScriptLocation DefaultValueStart;

						// emit a placeholder value for the skip offset
						CodeSkipSizeType Placeholder=0;
						Writer << Placeholder;

						CompileParameterValue(prop, TokenData->Token);

						// match the comma or closing paren after the parameter expression
						// this is needed to catch extraneous/invalid code after a valid expression,
						// since CompileFunctionDeclaration() just grabs everything until the next comma/paren
						if (!MatchSymbol(TEXT(",")) && !MatchSymbol(TEXT(")")))
						{
							ScriptErrorf(SCEL_Unknown/*SCEL_Expression*/, TEXT("Missing ',' or ')' after value for optional parameter '%s'"), *prop->GetName());
						}

						// now overwrite the placeholder value with the actual distance between the jump byte and the current location
						*(CodeSkipSizeType*)&TopNode->Script(DefaultValueStart.CodeTop) = TopNode->Script.Num() - (DefaultValueStart.CodeTop + sizeof(CodeSkipSizeType));
					}

					else
					{
						// no default value specified for this optional parameter
						Writer << EX_Nothing;
					}
				}
			}
		}
		// catch the error here so we can continue compiling
		catch (const TCHAR* ErrorMsg)
		{
			if (GEditor->Bootstrapping)
			{
				Class->SetFlags(RF_Marked);
				Warn->Log(NAME_Error, ErrorMsg);
			}
			else
			{
				// Handle compiler error.
				AddResultText(TEXT("Error in %s, Line %i: %s\r\n"), *Class->GetName(), InputLine, ErrorMsg);
			}

			// Invalidate this class.
			Class->ClassFlags &= ~(CLASS_Parsed | CLASS_Compiled);
		}

		ReturnToLocation(PreviousLocation, FALSE, TRUE);
	}

	// If compiling the class node and an input line is specified, it's the replication defs.
	if( Node==Class && Node->Line!=INDEX_NONE )
	{
		// Remember input positions.
		InputPos  = PrevPos  = Node->TextPos;
		InputLine = PrevLine = Node->Line;

		// Compile all replication defs.
		try
		{
			while( 1 )
			{
				// Get Reliable or Unreliable.
				const QWORD PropertyFlags = CPF_Net;
				const DWORD FunctionFlags = FUNC_Net;

				FToken Token;
				GetToken( Token );
				if( Token.Matches( TEXT("}") ) )
				{
					break;
				}
				else if( Token.Matches(NAME_Reliable) || Token.Matches(NAME_Unreliable) )
				{
					ScriptWarnf(SCWL_Level4, TEXT("Reliabe/Unreliable keywords are no longer used in 'replication {}'") );
				}
				else
				{
					UngetToken(Token);
				}

				// Compile conditional expression.
				RequireIdentifier( NAME_If, TEXT("Replication statement") );
				RequireSymbol( TEXT("("), TEXT("Replication condition") );
				WORD RepOffset = TopNode->Script.Num();
				CompileExpr( FPropertyBase(CPT_Bool), TEXT("Replication condition") );
				RequireSymbol( TEXT(")"), TEXT("Replication condition") );

				// Compile list of variables defined in this class and hook them into the
				// replication conditions.
				do
				{
					// Get variable name.
					FToken VarToken;
					if( !GetIdentifier(VarToken) )
					{
						ScriptErrorf(SCEL_Unknown,  TEXT("Missing variable name in replication definition") );
					}
					FName VarName = FName( VarToken.Identifier, FNAME_Find, TRUE );
					if( VarName == NAME_None )
					{
						ScriptErrorf(SCEL_Unknown,  TEXT("Unrecognized variable '%s' name in replication definition"), VarToken.Identifier );
					}

					// Find variable.
					UBOOL Found=0;
					for( TFieldIterator<UProperty> It(Class); It && It.GetStruct()==Class; ++It )
					{
						if( It->GetFName()==VarName )
						{
							// Found it, so make sure it's replicatable.
							if( (It->PropertyFlags & CPF_Net) != 0 )
							{
								ScriptErrorf(SCEL_Restricted,  TEXT("Variable '%s' already has a replication definition"), *VarName.ToString() );
							}

							// Make sure it isn't a dynamic array property
							if ( It->IsA(UArrayProperty::StaticClass()) )
							{
								ScriptErrorf(SCEL_Restricted, TEXT("Dynamic arrays cannot be replicated '%s'"), *VarName.ToString());
							}
							// Make sure it's not a delegate (replicating delegates is insecure, and can't be made secure)
							else if (It->IsA(UDelegateProperty::StaticClass()))
							{
								ScriptErrorf(SCEL_Restricted, TEXT("Delegates cannot be replicated (replicated delegates are insecure) '%s'"), *VarName.ToString());
							}
							// For static arrays, give a warning if the element count exceeds 256 (since only elements 0-255 can be replicated)
							else if (It->ArrayDim > 256)
							{
								ScriptWarnf(SCWL_Level4, TEXT("Static array elements beyond index 255 cannot be replicated"), *VarName.ToString());
							}

							// Set its properties.
							It->PropertyFlags |= PropertyFlags;
							It->RepOffset = RepOffset;
							Found = 1;
							break;
						}
					}
					if( !Found )
					{
						// Find function.
						for( TFieldIterator<UFunction> Function(Class); Function && Function.GetStruct()==Class; ++Function )
						{
							if( Function->GetFName()==VarName )
							{
								ScriptWarnf(SCWL_Level4,TEXT("Functions are no longer handled by 'replication {}', '%s'"),*Function->GetName());
								Found = 1;
								break;
							}
						}
					}
					if( !Found )
					{
						ScriptErrorf(SCEL_Unknown,  TEXT("Bad variable or '%s' in replication definition"), *VarName.ToString() );
					}
				} while( MatchSymbol( TEXT(",") ) );

				// Semicolon.
				RequireSymbol( TEXT(";"), TEXT("Replication definition") );
			}
		}
		// catch the error here so we can continue compiling
		catch (const TCHAR* ErrorMsg)
		{
			if (GEditor->Bootstrapping)
			{
				Class->SetFlags(RF_Marked);
				Warn->Log(NAME_Error, ErrorMsg);
			}
			else
			{
				// Handle compiler error.
				AddResultText(TEXT("Error in %s, Line %i: %s\r\n"), *Class->GetName(), InputLine, ErrorMsg);
			}

			// Invalidate this class.
			Class->ClassFlags &= ~(CLASS_Parsed | CLASS_Compiled);
		}

		for (TFieldIterator<UProperty> Child(Class); Child != NULL && Child.GetStruct() == Class; ++Child)
		{
			if ((Child->PropertyFlags & CPF_RepNotify) && !(Child->PropertyFlags & CPF_Net))
			{
				ScriptWarnf(SCWL_Level4, TEXT("Property '%s' declared with 'repnotify' but is not replicated"), *Child->GetName());
			}
		}
	}

	// Compile all child functions in this class or state.
	for( TFieldIterator<UStruct> Child(Node); Child && Child.GetStruct()==Node; ++Child )
	{
		if( Child->GetClass() != UScriptStruct::StaticClass() )
		{
			CompileSecondPass( *Child );
		}
	}
	check(TopNode==Node);

	// Prepare for function or state compilation.
	UBOOL DoCompile=0;
	if( Node->Line!=INDEX_NONE && Node!=Class )
	{
		// Remember input positions.
		InputPos  = PrevPos  = Node->TextPos;
		PrevLine  = Node->Line;
		InputLine = Node->Line;

		// Emit function parms info into code stream.
		UState* TopState = Cast<UState>(TopNode, CLASS_IsAUState);
		if( TopFunction && !(TopFunction->FunctionFlags & FUNC_Native) )
		{
			// Should we compile any code?
			DoCompile = (TopFunction->FunctionFlags & FUNC_Defined);
			EmitDebugInfo(DI_NewStack);
		}
		else if( TopFunction )
		{
			// Inject native function parameters.
			for( TFieldIterator<UProperty> It(TopFunction); It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
			{
				UProperty* Property = *It;
				Writer << EX_NativeParm;
				Writer << Property;
			}
			Writer << EX_Nothing;
		}
		else if( TopState )
		{
			// Compile the code.
			DoCompile = 1;
		}
	}

	// Compile code.
	if( DoCompile )
	{
		// Compile statements until we get back to our original nest level.
		LinesCompiled -= InputLine;
		try
		{
			while( NestLevel > StartNestLevel )
			{
				if( !CompileStatement() )
				{
					ScriptErrorf(SCEL_Unknown,  TEXT("Unexpected end of code in %s"), *Node->GetClass()->GetName() );
				}
			}
		}
		// attempt to catch the error in this scope so we can continue with other functions in this class
		catch (const TCHAR* ErrorMsg)
		{
			if (GEditor->Bootstrapping)
			{
				Class->SetFlags(RF_Marked);
				Warn->Log(NAME_Error, ErrorMsg);
			}
			else
			{
				// Handle compiler error.
				AddResultText(TEXT("Error in %s, Line %i: %s\r\n"), *Class->GetName(), InputLine, ErrorMsg);
			}

			// Invalidate this class.
			Class->ClassFlags &= ~(CLASS_Parsed | CLASS_Compiled);

			// disable warnings while popping nest level
			EScriptCompilerWarningLevel OldRequiredWarningLevel = RequiredWarningLevel;
			RequiredWarningLevel = EScriptCompilerWarningLevel(MAXINT);
			try
			{
				// return nest level to its previous value
				while (NestLevel > StartNestLevel)
				{
					PopNest(NEST_None, *Node->GetClass()->GetName());
				}
			}
			catch (TCHAR* ErrorMsg)
			{
				// errors in nest popping prevent further compilation of this class,
				// but we need to reset the warning level before passing up the chain
				RequiredWarningLevel = OldRequiredWarningLevel;
				throw ErrorMsg;
			}
			RequiredWarningLevel = OldRequiredWarningLevel;
			//@warning: assumption that no more compiling work will be performed on this node
		}
		LinesCompiled += InputLine;
	}
	else if( Node!=Class )
	{
		// Pop the nesting.
		PopNest( NewNest, *Node->GetClass()->GetName() );
	}
}

//
// Compile the script associated with the specified class.
// Returns 1 if compilation was a success, 0 if any errors occurred.
//
UBOOL FScriptCompiler::CompileScript
(
	FClassTree&			AllClasses,
	UClass*				InClass,
	FMemStack*			InMem,
	UBOOL				InBooting,
	ECompilerPassType	InPass
)
{
	if ( InClass->HasAnyFlags(RF_Marked) )
	{
		return FALSE;
	}
	Booting       = InBooting;
	Class         = InClass;
	Pass	      = InPass;
	Mem           = InMem;
	UBOOL Success  = 0;
	FMemMark Mark(*Mem);
	Warn->SetContext( this );

	ClassData = GScriptHelper->AddClassData(Class);

	//Reset the counter of variables for property window sorting
	OriginalVariableIndex = 0;

	//!! Debugger
	SupressDebugInfo = 0;
	bEmitDebugInfo = ParseParam( appCmdLine(), TEXT("DEBUG") );

	//!! Debugger
	// Message.
	if( ParseParam( appCmdLine(), TEXT("VERBOSE") ) )
	{
		// Message.
		if ( bReparsingClass && Pass == PASS_Parse )
		{
			debugf(TEXT("Scanning %s"), *Class->GetName());
		}
		else
		{
			Warn->Logf( NAME_Log, TEXT("%s %s"), Pass == PASS_Compile ? TEXT("Compiling") : TEXT("Parsing"), *Class->GetName() );
		}
	}

	// Make sure our parent classes is parsed.
	for( UClass* Temp = Class->GetSuperClass(); Temp; Temp=Temp->GetSuperClass() )
	{
		if( !(Temp->ClassFlags & CLASS_Parsed) )
		{
			ScriptErrorf(SCEL_Class,  TEXT("'%s' can't be compiled: Parent class '%s' has errors"), *Class->GetName(), *Temp->GetName() );
		}
	}


	// Init class.
	check( (Class->ClassFlags & CLASS_Compiled) == 0 || bReparsingClass );
	if( Pass == PASS_Parse )
	{
		if ( !bReparsingClass )
		{
			// First pass.
			Class->Script.Empty();

			//@fixme - reset class default object state?

			Class->PropertiesSize = 0;

			// Set class flags and within.
			PreviousClassFlags = Class->ClassFlags;
			Class->ClassFlags &= ~CLASS_RecompilerClear;

			UClass* SuperClass = Class->GetSuperClass();
			if( SuperClass != NULL )
			{
				Class->ClassFlags |= (SuperClass->ClassFlags) & CLASS_ScriptInherit;
				Class->ClassConfigName = SuperClass->ClassConfigName;
				check(SuperClass->ClassWithin);
				if( !Class->ClassWithin )
				{
					Class->ClassWithin = SuperClass->ClassWithin;
				}

				// Copy special categories from parent
				Class->HideCategories = SuperClass->HideCategories;
				Class->AutoExpandCategories = SuperClass->AutoExpandCategories;
				Class->AutoCollapseCategories = SuperClass->AutoCollapseCategories;
				Class->DontSortCategories = SuperClass->DontSortCategories;
				Class->bForceScriptOrder = SuperClass->bForceScriptOrder;
			}

			check(Class->ClassWithin);
		}
	}
	else if ( Pass == PASS_Compile )
	{
		// Second pass.
		// Replace the script.
		Class->Script.Empty();

		// Init the replication defs.
		for( TFieldIterator<UProperty> It(Class); It && It.GetStruct()==Class; ++It )
		{
			It->PropertyFlags &= ~CPF_Net;
			It->RepOffset = MAXWORD;
		}
	}

	// Init compiler variables.
	OriginalPropertiesSize = Class->GetPropertiesSize();
	Input		  = *Class->ScriptText->Text;
	InputLen	  = appStrlen(Input);
	InputPos	  = 0;
	PrevPos		  = 0;
	PrevLine	  = 1;
	InputLine     = 1;
	// Init nesting.
	NestLevel	= 0;
	TopNest		= NULL;
	PushNest( NEST_None, TEXT(""), NULL );

	// Try to compile it, and catch any errors.
	try
	{
		// Compile until we get an error.
		if( Pass == PASS_Parse )
		{
			// Parse entire program.
			while( CompileStatement() )
			{
				// Clear out the previous comment in anticipation of the next statement.
				ClearComment();
				StatementsCompiled++;
			}

			// Precompute info for runtime optimization.
			LinesCompiled += InputLine;

			// Stub out the script.
			if ( !bReparsingClass )
				Class->Script.Empty();

			Class->ClassFlags |= CLASS_Parsed;
		}
		else if ( Pass == PASS_Compile )
		{
			// Compile all uncompiled sections of parsed code.
			CompileSecondPass( Class );
			if (!(Class->ClassFlags & CLASS_Parsed))
			{
				// failed to compile this class
				// this is kind of hacky (could use a return value from CompileSecondPass()) but this maintains consistency with
				// other checks, e.g. the one above that prevents subclasses from being compiled if the base was unsuccessful
				return 0;
			}

			// Mark as compiled.
			Class->ClassFlags |= CLASS_Compiled;
			// Note that we need to import defaultproperties for this class.
			Class->ClassFlags |= CLASS_NeedsDefProps;


			// Sanity check that all functions of this class handle any property references that are editor-only.
			for( UField *Child = Class->Children; Child; Child = Child->Next )
			{
				UFunction *Func = Cast<UFunction>(Child);
				if( Func && (Func->FunctionFlags & FUNC_Defined) )
				{
					TArray<UProperty *> PropReferences;
					TArchiveObjectReferenceCollector<UProperty> Collector(&PropReferences, NULL, FALSE, FALSE);
					Collector.SetFilterEditorOnly(TRUE);
					INT iCode = 0;
					while( iCode < Func->Script.Num() )
					{	
						Func->SerializeExpr( iCode, Collector );
					}

					for( TArray<UProperty *>::TIterator It(PropReferences); It; ++It )
					{
						UProperty *Prop = *It;
						if( Prop->IsEditorOnlyProperty() )
						{
							warnf( NAME_Error, TEXT("Script function %s.%s references editor-only property %s!  Please use \"FilterEditorOnly {...}\" to around the lines."),
								*Class->GetName(),
								*Func->GetName(),
								*Prop->GetPathName() );
						}
					}
				}
			}
		}

		// Make sure the compilation ended with valid nesting.
		if     ( NestLevel==0 )
		{
			ScriptErrorf(SCEL_Fatal, TEXT("Internal nest inconsistency") );
		}
		else if( NestLevel==1 )
		{
			ScriptErrorf(SCEL_Class, TEXT("Missing 'Class' definition") );
		}
		else if( NestLevel==2 )
		{
			// determine if the Class is an interface or a class
			if (Class->ClassFlags & CLASS_Interface)
			{
				PopNest( NEST_Interface, TEXT("'Interface'") );
			}
			else
			{
				PopNest( NEST_Class, TEXT("'Class'") );
			}
		}
		else if( NestLevel >2 )
		{
			ScriptErrorf(SCEL_Class, TEXT("Unexpected end of script in '%s' block"), NestTypeName(TopNest->NestType) );
		}

		// Cleanup after first pass.
		if( Pass == PASS_Parse && !bReparsingClass )
		{
			// Finish parse.
			PostParse( Class );

			//@fixme - verify default object size?

			// Check native size.
			if
			(   Class->HasAnyClassFlags(CLASS_Native)
			&&	OriginalPropertiesSize
			&&	Align(Class->GetDefaultsCount(),PROPERTY_ALIGNMENT)!=OriginalPropertiesSize
			&&	GCheckNatives )
			{
				ScriptWarnf(SCWL_Level4, TEXT("Native class %s size mismatch (script %i, C++ %i)"), *Class->GetName(), Class->GetDefaultsCount(), OriginalPropertiesSize );
				GCheckNatives = 0;
			}

			// Set all optimization ClassFlags based on property types
			for( TFieldIterator<UProperty> It(Class,FALSE); It; ++It )
			{
				if( It->IsLocalized() )
				{
					Class->ClassFlags |= CLASS_Localized;
				}
				if( (It->PropertyFlags & CPF_Config) != 0 )
				{
					Class->ClassFlags |= CLASS_Config;
				}
				if( (It->PropertyFlags & CPF_Component) != 0 )
				{
					Class->ClassFlags |= CLASS_HasComponents;
				}
				if ( It->ContainsInstancedObjectProperty() )
				{
					Class->ClassFlags |= CLASS_HasInstancedProps;
				}
				if( (It->PropertyFlags & CPF_CrossLevel) != 0 )
				{
					Class->ClassFlags |= CLASS_HasCrossLevelRefs;
				}
			}

			// Class needs to specify which ini file is going to be used if it contains config variables.
			if( (Class->ClassFlags & CLASS_Config) && (Class->ClassConfigName == NAME_None) )
			{
				// Inherit config setting from base class.
				Class->ClassConfigName = Class->GetSuperClass() ? Class->GetSuperClass()->ClassConfigName : NAME_None;
				if( Class->ClassConfigName == NAME_None )
				{
					ScriptErrorf(SCEL_Formatting,  TEXT("Classes with config / globalconfig member variables need to specify config file.") );
					Class->ClassConfigName = NAME_Engine;
				}
			}

			// if no state is marked auto, mark class as auto for faster GotoState()
			UBOOL bFoundAuto = 0;
			for (TFieldIterator<UState> It(Class); It; ++It)
			{
				if (It->StateFlags & STATE_Auto)
				{
					if (bFoundAuto && It->GetOuter() == Class)
					{
						ScriptErrorf(SCEL_Restricted, TEXT("Multiple auto states defined"));
					}
					bFoundAuto = 1;
				}
			}
			if (!bFoundAuto)
			{
				Class->StateFlags |= STATE_Auto;
			}

			// First-pass success.
			//@todo: any reason this needs to be here?
//			Class->GetDefaultObject()->InitClassDefaultObject( Class, 1 );
		}
		Success = 1;
	}
	catch( TCHAR* ErrorMsg )
	{
		// All errors are critical when booting.
		if( GEditor->Bootstrapping )
		{
			Class->SetFlags(RF_Marked);
			Warn->Log( NAME_Error, ErrorMsg );
			return 0;
		}

		// Handle compiler error.
		AddResultText( TEXT("Error in %s, Line %i: %s\r\n"), *Class->GetName(), InputLine, ErrorMsg );

		// Invalidate this class.
		Class->ClassFlags &= ~(CLASS_Parsed | CLASS_Compiled);
		Class->Script.Empty();
	}
	// Clean up and exit.
	if ( !bReparsingClass )
		Class->Bind();

	Mark.Pop();
	Warn->SetContext( NULL );
	return Success;
}

/*-----------------------------------------------------------------------------
	FScriptCompiler error handling.
-----------------------------------------------------------------------------*/

//
// Print a formatted debugging message (same format as printf).
//
void VARARGS FScriptCompiler::AddResultText(const TCHAR* Fmt, ...)
{
	TCHAR TempStr[4096];
	GET_VARARGS(TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr) - 1, Fmt, Fmt);
	debugf(NAME_Log, TEXT("%s"), TempStr);
	if (ErrorText != NULL)
	{
		ErrorText->Log(TempStr);
	}
};

/*-----------------------------------------------------------------------------
	Global functions.
-----------------------------------------------------------------------------*/

/**
 * Parse Class's unrealscript and optionally its child classes.  Marks the class as CLASS_Parsed.
 *
 * @param	AllClasses		the class tree containing all classes in the current package
 * @param	Compiler		the script compiler
 * @param	Class			the class to parse
 * @param	MakeAll			whether to parse classes even if they are already parsed
 * @param	Booting			TRUE if we're compiling from a commandlet
 * @param	MakeSubclasses	TRUE if we should compile all child classes of Class
 *
 * @return	TRUE if the class was successfully compiled, FALSE otherwise
 */
static UBOOL ParseScripts( FClassTree& AllClasses, FScriptCompiler& Compiler, UClass* Class, UBOOL MakeAll, UBOOL Booting, UBOOL MakeSubclasses )
{
	check(Class!=NULL);

	if( !Class->ScriptText )
		return TRUE;

	UBOOL bResult = TRUE;

	// Manual dependency support.
	for( INT NameIndex=0; NameIndex<Class->DependentOn.Num(); NameIndex++)
	{
		UClass* OrigDependsOnClass	= FindClass(*Class->DependentOn(NameIndex).ToString());//FindObject<UClass>(ANY_PACKAGE, *Class->DependentOn(NameIndex).ToString());
		UClass* DependsOnClass		= OrigDependsOnClass;
		if( !DependsOnClass )
		{
			Compiler.ScriptErrorf(SCEL_Class, TEXT("Failed to find DependsOn/Implements class '%s' while parsing '%s'"),*Class->DependentOn(NameIndex).ToString(),*Class->GetName() );
			return FALSE;
		}
		else
		{
			// Detect potentially unnecessary usage of dependson (ingore intrinsic classes too - no script text to parse)
			if( DependsOnClass->HasAnyClassFlags(CLASS_Parsed) || DependsOnClass->ScriptText == NULL)
			{
				continue;
			}

			// Treat manual dependency on a base class as an error to detect bad habits early on.
			if( Class->IsChildOf( DependsOnClass ) )
			{
				Compiler.ScriptErrorf(SCEL_Class, TEXT("%s is derived from %s - please remove the DependsOn"),*Class->GetName(),*DependsOnClass->GetName() );
				return FALSE;
			}

			// Check for circular dependency. If the OrigDependsOnClass is dependent on the SubClass, there is one.
			if( Compiler.IsDependentOn( OrigDependsOnClass, Class, AllClasses ) )
			{
				Compiler.ScriptErrorf(SCEL_Class, TEXT("Class %s DependsOn(%s) is a circular dependency."),*OrigDependsOnClass->GetName(),*Class->GetName());
				return FALSE;
			}


			// Find first base class of DependsOnClass that is not a base class of Class.
			TArray<UClass*> ClassesToParse;
			ClassesToParse.AddItem(DependsOnClass);
			// Stop at the first parsed class or intrinsic class
			for ( UClass* ParentClass = DependsOnClass->GetSuperClass(); ParentClass && !ParentClass->HasAnyClassFlags(CLASS_Parsed) && ParentClass->ScriptText; ParentClass = ParentClass->GetSuperClass() )
			{
				ClassesToParse.AddItem(ParentClass);
			}

			while ( ClassesToParse.Num() > 0 )
			{
				UClass* NextClass = ClassesToParse.Pop();
				if ( ParseScripts(AllClasses, Compiler, NextClass, FALSE, Booting, TRUE) )
				{
					break;
				}
				else if ( !ParseScripts(AllClasses, Compiler, NextClass, FALSE, Booting, FALSE) )
				{
					bResult = FALSE;
					break;
				}
			}
// 			while (!Class->IsChildOf(DependsOnClass->GetSuperClass()) && !(DependsOnClass->GetSuperClass()->ClassFlags & CLASS_Parsed))
// 			{
// 				DependsOnClass = DependsOnClass->GetSuperClass();
// 				AllClasses.AddClass(DependsOnClass);
// 			}
// 
// 			if( !ParseScripts( AllClasses, Compiler, DependsOnClass, MakeAll, Booting, FALSE) )
// 			{
// 				bResult = FALSE;
// 			}
		}
	}

	// First-pass compile this class if needed.
	if( MakeAll )
	{
		Class->ClassFlags &= ~CLASS_Parsed;
	}
	if( !(Class->ClassFlags & CLASS_Parsed) )
	{
		UClass* CurrentSuperClass = Class->GetSuperClass();
		if( !Compiler.CompileScript( AllClasses, Class, &GMainThreadMemStack, Booting, PASS_Parse ) )
		{
			// if we couldn't parse this class, we won't be able to parse its children
			return FALSE;
		}
		else if ( CurrentSuperClass != Class->GetSuperClass() )
		{
			// detect a native class that has changed parents and update the tree
			AllClasses.ChangeParentClass(Class);
			AllClasses.Validate();
		}
	}
	check(Class->ClassFlags & CLASS_Parsed);

	// First-pass compile subclasses.
	if ( MakeSubclasses )
	{
		const FClassTree* ClassLeaf = AllClasses.FindNode(Class);
		TArray<const FClassTree*> ChildLeaves;
		ClassLeaf->GetChildClasses(ChildLeaves);

		for( INT ClassIndex=0; ClassIndex<ChildLeaves.Num(); ClassIndex++ )
		{
			const FClassTree* ChildLeaf = ChildLeaves(ClassIndex);
			UClass* SubClass = ChildLeaf->GetClass();

			//note: you must always pass in the root tree node here, since we may add new classes to the tree
			// if an manual dependency (through dependson()) is encountered
			if ( !ParseScripts( AllClasses, Compiler, SubClass, MakeAll, Booting, MakeSubclasses) )
			{
				bResult = FALSE;
			}
		}
	}

	// Success.
	return bResult;
}

/**
 * Hierarchically recompile all scripts.
 *
 * @param	AllClasses		the classtree node associated with Class
 * @param	Compiler		the script compiler
 * @param	Class			the class to parse
 * @param	MakeSubclasses	TRUE if we should compile child classes
 *
 * @return	TRUE if all scripts were successfully compiled.
 */
static UBOOL CompileScripts( FClassTree& AllClasses, FScriptCompiler& Compiler, UClass* Class, UBOOL MakeSubclasses )
{
	UBOOL bResult = TRUE;

	// Compile it.
	if( Class->ScriptText )
	{
		if( !(Class->ClassFlags & CLASS_Compiled) )
		{
			if( !Compiler.CompileScript( AllClasses, Class, &GMainThreadMemStack, 0, PASS_Compile ) )
			{
				return FALSE;
			}
		}
		check(Class->ClassFlags & CLASS_Compiled);
	}

	// Compile subclasses.
	if (MakeSubclasses)
	{
		TArray<FClassTree*> ChildLeaves;
		AllClasses.GetChildClasses(ChildLeaves);

		for( INT ClassIndex=0; ClassIndex<ChildLeaves.Num(); ClassIndex++ )
		{
			FClassTree* ChildLeaf = ChildLeaves(ClassIndex);
			UClass* SubClass = ChildLeaf->GetClass();
			if ( !CompileScripts( *ChildLeaf, Compiler, SubClass, MakeSubclasses ) )
			{
				// remember that we failed, but continue to try to compile other classes
				bResult = FALSE;
			}
		}
	}

	// Success.
	return bResult;
}

static INT CountClassesWithUnparsedDefaults( const FClassTree& AllClasses )
{
	UClass* Class = AllClasses.GetClass();
	INT Result = (Class->ClassFlags&CLASS_NeedsDefProps) ? 1 : 0;

	TArray<const FClassTree*> ChildClasses;
	AllClasses.GetChildClasses(ChildClasses);
	for ( INT ChildIndex = 0; ChildIndex < ChildClasses.Num(); ChildIndex++ )
	{
		const FClassTree* ChildLeaf = ChildClasses(ChildIndex);
		Result += CountClassesWithUnparsedDefaults(*ChildLeaf);
	}

	return Result;
}

static void GetClassesWithUnparsedDefaults( const FClassTree& AllClasses, FString& Result )
{
	UClass* Class = AllClasses.GetClass();
	if ( (Class->ClassFlags&CLASS_NeedsDefProps) != 0 )
	{
		Result += FString::Printf( Result.Len() ? TEXT(", %s") : TEXT("%s"), *Class->GetName() );
	}

	TArray<const FClassTree*> ChildClasses;
	AllClasses.GetChildClasses(ChildClasses);
	for ( INT ChildIndex = 0; ChildIndex < ChildClasses.Num(); ChildIndex++ )
	{
		const FClassTree* ChildLeaf = ChildClasses(ChildIndex);
		GetClassesWithUnparsedDefaults(*ChildLeaf,Result);
	}
}

static FString GetClassesWithUnparsedDefaults( const FClassTree& AllClasses )
{
	FString Result;
	GetClassesWithUnparsedDefaults(AllClasses, Result);
	return Result;
}

/**
 * Clears the constructor link for all classes that are marked as misaligned.  Used to prevent make from crashing
 * when exiting as a result of misaligned classes.
 *
 * @param	AllClasses	the class tree that contains the classes currently being compiled.
 */
static void ClearConstructorLinkForMisalignedClasses( const FClassTree* AllClasses )
{
	check(AllClasses);

	AllClasses = AllClasses->GetRootNode();
	for ( TObjectIterator<UClass> It; It; ++It )
	{
		UClass* Class = *It;
		if ( Class->IsMisaligned() )
		{
			UClass* BaseMisalignedClass;
			for ( BaseMisalignedClass = Class; BaseMisalignedClass; BaseMisalignedClass = BaseMisalignedClass->GetSuperClass() )
			{
				if ( BaseMisalignedClass->GetSuperClass() && !BaseMisalignedClass->GetSuperClass()->HasAnyFlags(RF_MisalignedObject) )
				{
					break;
				}
			}

			if ( BaseMisalignedClass != NULL )
			{
				// if the base-most misaligned class in this class's inheritance chain has a corresponding node in the class tree,
				// it indicates that the base-most misaligned class is part of the package currently being compiled
				if ( AllClasses->FindNode(BaseMisalignedClass) != NULL )
				{
					// unhook the constructor link for this class so that the UObject dtor doesn't attempt to destroy
					// UProperties at incorrect offsets - while this leaks any memory allocated by those properties,
					// it should be reclaimed by the OS as soon as this process exits.
					Class->ConstructorLink = NULL;
				}
			}
		}
	}
}

/**
 * Hierarchically import defaultproperties for all classes (Step 2).  Ensures that the class's defaults have been initialized
 * (all defaults are propagated from its parent class), propagates defaults for all struct properties, and that the class's component map is valid.
 * Then calls ImportDefaultProperties to actually import the default property text into the Defaults address space for the current class.  Finally
 * recursively calls CompileDefaultProperties for the child classes of the current class.
 *
 * @param	AllClasses		the classtree node associated with the current class
 * @param	Compiler		the script compiler
 * @param	MakeSubclasses	TRUE if we should compile child classes
 *
 * @return	TRUE if the class defaults were successfully parsed
 */
UBOOL FScriptCompiler::CompileDefaultProperties( const FClassTree& AllClasses, UBOOL MakeSubclasses )
{
	static UBOOL bVerboseLogging = ParseParam(appCmdLine(),TEXT("VERBOSE"));

	UClass* Class = AllClasses.GetClass();

	// Import its properties
	if( Class->ClassFlags & CLASS_NeedsDefProps )
	{
		if ( bVerboseLogging )
		{
			warnf(TEXT("Importing Defaults for %s"), *Class->GetName() );
		}

		if ( Class->IsMisaligned() && Class->GetDefaultsCount() > 0 )
		{
			// If a class is misaligned and it already has a default object, destructing that object will cause a crash,
			// so let's bail out to ensure that we don't save the .u package which can potentially introduce data corruption.
			ClearConstructorLinkForMisalignedClasses(AllClasses.GetRootNode());
			ScriptErrorf(SCEL_Fatal, LocalizeSecure(LocalizeError(TEXT("NativeRebuildRequired"), TEXT("Core")), *Class->GetName()) );
		}

		UObject* DefaultObject = NULL;
#if !SHIPPING_PC_GAME
		UObjectRedirector* CDORedirector = NULL;
		DefaultObject = Class->ClassDefaultObject;
		if ( DefaultObject != NULL && DefaultObject->GetClass() == UObjectRedirector::StaticClass() )
		{
			CDORedirector = static_cast<UObjectRedirector*>(DefaultObject);
			Class->ClassDefaultObject = NULL;
		}
#endif
		DWORD PreviousHackFlags = GUglyHackFlags;
		GUglyHackFlags |= HACK_DisableSubobjectInstancing;

		DefaultObject = Class->GetDefaultObject(TRUE);
		check(DefaultObject);

#if !SHIPPING_PC_GAME
		if ( CDORedirector != NULL )
		{
			debugf(TEXT("Assigning target on CDO redirector %s for %s"), *CDORedirector->GetName(), *Class->GetName());
			CDORedirector->DestinationObject = DefaultObject;
		}
#endif

		UClass* SuperClass = Class->GetSuperClass();
		DefaultObject->InitClassDefaultObject( Class, 1 );
		if(SuperClass)
		{
			Class->ComponentNameToDefaultObjectMap = SuperClass->ComponentNameToDefaultObjectMap;
		}
		InDefaultPropContext = 1; // error reporting
		Class->PropagateStructDefaults();

		GUglyHackFlags = PreviousHackFlags;

		// the first line of default properties is always the starting line number
		INT LineNumber=0;
		FString LineNumberText;
		const TCHAR* DefaultsText = *Class->DefaultPropText;
		ParseLine(&DefaultsText, LineNumberText, TRUE);
		Parse(*LineNumberText, TEXT("linenumber="), LineNumber);

		// Detect whether any errors were encountered while importing this class's defaults
		INT ErrorCount = GWarn->Errors.Num();

		const TCHAR* ResultBuffer = ImportDefaultProperties(
			Class,
			DefaultsText,
			GWarn,
			0,
			LineNumber);

		UBOOL ImportSuccess = ResultBuffer !=NULL || !Class->DefaultPropText.Len();
		InDefaultPropContext = 0;

		// if import failed, return 1.
		if( !ImportSuccess )
		{
			if ( GWarn->Errors.Num() != ErrorCount )
			{
				Class->ClassFlags &= ~CLASS_NeedsDefProps;
				return 1;
			}

			if ( bVerboseLogging )
			{
				warnf(TEXT("... deferring"));
			}

			return 1;
		}

		AActor* DefaultActor = Cast<AActor>(DefaultObject);
		if (DefaultActor != NULL && DefaultActor->Role != ROLE_Authority)
		{
			ScriptErrorf(SCEL_Restricted, TEXT("Importing defaults for %s: Changing Role in defaultproperties is illegal (was RemoteRole intended?)"), *Class->GetName());
		}
	}

	// Record the fact that we've imported the properties for this class.
	Class->ClassFlags &= ~(CLASS_NeedsDefProps);

	// Import properties in subclasses.
	if (MakeSubclasses)
	{
		TArray<const FClassTree*> ChildLeaves;
		AllClasses.GetChildClasses(ChildLeaves);
		for( INT ClassIndex=0; ClassIndex<ChildLeaves.Num(); ClassIndex++ )
		{
			const FClassTree* ChildLeaf = ChildLeaves(ClassIndex);
			if ( !CompileDefaultProperties( *ChildLeaf, MakeSubclasses ) )
			{
				return 0;
			}
		}
	}

	// Success.
	return 1;
}

/**
 * Hierarchically import defaultproperties for all classes (Step 1).  Determines if any classes
 * failed to import their defaultproperties (by checking whether any classes still need defaults imported)
 *
 * @param	AllClasses		the classtree node associated with Class
 * @param	Compiler		the script compiler
 * @param	MakeSubclasses	TRUE if we should compile child classes
 *
 * @return	TRUE if the class defaults were successfully parsed
 */
UBOOL FScriptCompiler::CompileClassDefaults( const FClassTree& AllClasses, UBOOL MakeSubclasses )
{
	INT InitialErrorCount = GWarn->Errors.Num();
	INT LastCount = CountClassesWithUnparsedDefaults( AllClasses );
	do
	{
		if( !CompileDefaultProperties( AllClasses, MakeSubclasses ) )
		{
			return 0;
		}
		INT Count = CountClassesWithUnparsedDefaults( AllClasses );
		if( Count == LastCount )
		{
			ScriptErrorf(SCEL_Class, TEXT("Compiler encountered circular BEGIN OBJECT dependency: %s"), *GetClassesWithUnparsedDefaults(AllClasses) );
		}

		LastCount = Count;
	} while( LastCount );

	// return success if the error count didn't increase!
	return Warn->Errors.Num() == InitialErrorCount;
}

static UBOOL CompileStructDefaults( TLookupMap<UScriptStruct*>& ScriptStructs, FScriptCompiler& Compiler, UClass* Class );
static void CompileStructDefaults( UScriptStruct* StructToCompile, TLookupMap<UScriptStruct*>& ScriptStructs, FScriptCompiler& Compiler, UClass* Class )
{
	if ( StructToCompile )
	{
		ScriptStructs.RemoveItem(StructToCompile);

		// ok, first thing to do is copy struct defaults from any parent structs 
		UScriptStruct* ParentStruct = Cast<UScriptStruct>(StructToCompile->GetSuperStruct());
		if ( ParentStruct )
		{
			// if the parent struct is still in the ScriptStructs array, then it hasn't imported its defaultproperties yet - do that now
			if ( ScriptStructs.Find(ParentStruct) )
			{
	 			CompileStructDefaults(ParentStruct, ScriptStructs, Compiler, Class);
			}

			// now copy all the values over into this struct's defaults block
			BYTE* ParentDefaults = ParentStruct->GetDefaults();
			BYTE* StructDefaults = StructToCompile->GetDefaults();
			if ( ParentDefaults != NULL && StructDefaults != NULL )
			{
				for ( UProperty* Prop = ParentStruct->PropertyLink; Prop; Prop = Prop->PropertyLinkNext )
				{
					if ( (Prop->PropertyFlags&CPF_Native) == 0 )
					{
						Prop->CopyCompleteValue(StructDefaults + Prop->Offset, ParentDefaults + Prop->Offset);
					}
				}
			}
		}

		Compiler.InDefaultPropContext = 0;

		// next, make sure that any nested structs have already imported their defaults so that we're copying the right values
		// we don't have to worry about overwriting data we copied from the parent struct because we only look at nested structs
		// which have StructToCompile as its Outer.
		for( TFieldIterator<UStructProperty> It(StructToCompile,FALSE); It; ++It )
		{
			UStructProperty* NestedStructProperty = *It;
			if ( (NestedStructProperty->PropertyFlags&CPF_Native) == 0 )
			{
				UScriptStruct* NestedStruct = NestedStructProperty->Struct;
				if ( ScriptStructs.Find(NestedStruct) )
				{
					CompileStructDefaults(NestedStruct, ScriptStructs, Compiler, Class);
				}
			}
		}

		if ( StructToCompile->DefaultStructPropText.Len() > 0 )
		{
			const TCHAR* DefaultsText = *StructToCompile->DefaultStructPropText;

			// first line is always the line number
			FString LineNumberString;
			INT LineNumber=0;
			ParseLine(&DefaultsText, LineNumberString, TRUE);
			Parse(*LineNumberString, TEXT("linenumber="), LineNumber);

			StructToCompile->PropagateStructDefaults();
			ImportObjectProperties( StructToCompile->GetDefaults(), DefaultsText, StructToCompile, NULL, NULL, GWarn, 0, LineNumber );

			StructToCompile->DefaultStructPropText.Empty();
		}
		else
		{
			// this is for structs that do not have structdefaults themselves, but might contain struct properties for structs
			// which have defaults
			StructToCompile->PropagateStructDefaults();
		}
	}
}

static UBOOL CompileStructDefaults( TLookupMap<UScriptStruct*>& ScriptStructs, FScriptCompiler& Compiler, UClass* Class )
{
	INT InitialErrorCount = GWarn->Errors.Num();

	while ( ScriptStructs.Num() > 0 )
	{
		UScriptStruct* Struct = ScriptStructs(0);
		ScriptStructs.Remove(0);

		CompileStructDefaults(Struct, ScriptStructs, Compiler, Class);
	}

	// return success if the error count didn't increase!
	return GWarn->Errors.Num() == InitialErrorCount;
}

static void ClearDefaultPropertyText( const FClassTree& AllClasses )
{
	UClass* Class = AllClasses.GetClass();
	Class->DefaultPropText.Empty();

	TArray<const FClassTree*> ChildLeaves;
	AllClasses.GetChildClasses(ChildLeaves);

	for( INT ClassIndex=0; ClassIndex<ChildLeaves.Num(); ClassIndex++ )
	{
		const FClassTree* ChildLeaf = ChildLeaves(ClassIndex);
		ClearDefaultPropertyText(*ChildLeaf);
	}
}

/**
 * Begins the process of exporting C++ class declarations for native classes in the specified package
 * 
 * @param	CurrentPackage	the package being compiled
 * @param	AllClasses		the class tree for CurrentPackage
 */
static void ExportNativeHeaders( UPackage* CurrentPackage, FClassTree& AllClasses )
{
	// Build a list of header filenames
	TArray<FString>	ClassHeaderFilenames;
	new(ClassHeaderFilenames) FString(TEXT(""));

	UBOOL bExportingHeaders = FALSE;

	// allow for an ini file to override generation of headers
#if SHIPPING_PC_GAME
	const UBOOL bSkipNativeHeaderGeneration = TRUE;
#else
	UBOOL bSkipNativeHeaderGeneration = FALSE;
	GConfig->GetBool(TEXT("UnrealEd.EditorEngine"), TEXT("SkipNativeHeaderGeneration"), bSkipNativeHeaderGeneration, GEngineIni);
#endif

	if (!bSkipNativeHeaderGeneration)
	{
		for( TObjectIterator<UClass> It; It; ++It )
		{
			UClass* Cls = *It;
			if( (CurrentPackage == NULL || Cls->GetOuter()==CurrentPackage) && Cls->ScriptText && Cls->HasAnyClassFlags(CLASS_Native) && !Cls->HasAnyClassFlags(CLASS_NoExport) )
			{
				bExportingHeaders = TRUE;
				break;
			}
		}
	}

	if ( bExportingHeaders )
	{
		const static UBOOL bQuiet = !ParseParam(appCmdLine(),TEXT("VERBOSE"));
		if ( CurrentPackage != NULL )
		{
			if ( bQuiet )
			{
				debugf(TEXT("Exporting native class declarations for %s"), *CurrentPackage->GetName());
			}
			else
			{
				warnf(TEXT("Exporting native class declarations for %s"), *CurrentPackage->GetName());
			}
		}
		else
		{
			if ( bQuiet )
			{
				debugf(TEXT("Exporting native class declarations"));
			}
			else
			{
				warnf(TEXT("Exporting native class declarations"));
			}
		}

		// Export native class definitions to package header files.
		FNativeClassHeaderGenerator( CurrentPackage, AllClasses);
	}
}

IMPLEMENT_COMPARE_POINTER( UClass, UnScrCom, { return appStricmp(*A->GetName(),*B->GetName()); } )

#define LOG_CLASSES 0
#if LOG_CLASSES
static void LogClasses( const FClassTree* Node, INT& Indent )
{
	check(Node);

	warnf(TEXT("%s%s"), appSpc(Indent), *Node->GetClass()->GetName());

	Indent += 3;

	TArray<const FClassTree*> Children;
	Node->GetChildClasses(Children);

	for ( INT i = 0; i < Children.Num(); i++ )
		LogClasses(Children(i), Indent);

	Indent -= 3;
}
#endif

//
// Make all scripts.
// Returns 1 if success, 0 if errors.
// Not recursive.
//
UBOOL UEditorEngine::MakeScripts( UClass* BaseClass, FFeedbackContext* Warn, UBOOL MakeAll, UBOOL Booting, UBOOL MakeSubclasses, UPackage* LimitOuter/* = NULL */, UBOOL bParseOnly/*=0 */, UBOOL bHeaders/*=FALSE*/)
{
	FMemMark Mark(GMainThreadMemStack);
	FTransactionBase* Transaction = Trans ? Trans->CreateInternalTransaction() : NULL;
	FScriptCompiler	Compiler( Warn, bParseOnly );
	Compiler.InDefaultPropContext = 0;

	// Disable this flag during compilation so that creating a new class default object requires passing in TRUE for the bForce parameter of GetDefaultObject()
	DWORD OriginalHackFlags = GUglyHackFlags;
	GUglyHackFlags &= ~HACK_ClassLoadingDisabled;

	// Make list of all classes from this base
	if (!BaseClass)
	{
		BaseClass = UObject::StaticClass();
	}

	// build a class inheritance tree for this package
	FClassTree AllClasses( UObject::StaticClass() );

	for( TObjectIterator<UClass> ObjIt; ObjIt; ++ObjIt )
	{
		if ( (LimitOuter == NULL || ObjIt->IsIn(LimitOuter)) )
		{
			UClass* Class = *ObjIt;
			if ( bParseOnly )
				Class->ClassFlags &= ~CLASS_Parsed;

			AllClasses.AddClass(Class);
		}
	}

	if ( !bParseOnly )
	{
		AllClasses.Validate();
	}

#if LOG_CLASSES
	if ( !bParseOnly )
	{
		INT Indent = 0;
		LogClasses(&AllClasses,Indent);
	}
#endif

	// Do compiling.
	Compiler.StatementsCompiled	= 0;
	Compiler.LinesCompiled		= 0;
	Compiler.ErrorText			= Results;
	if( Compiler.ErrorText )
	{
		Compiler.ErrorText->Text.Empty();
	}

	// Hierarchically parse and compile all classes.
	UBOOL Success = TRUE;
	try
	{
		Success = ParseScripts( AllClasses, Compiler, BaseClass, MakeAll, Booting, MakeSubclasses );
	}
	catch ( TCHAR* ErrorMsg )
	{
		Warn->Log( NAME_Error, ErrorMsg );

		//@todo: is the following necessary?  i.e. do we only want to catch exceptions when running make?
		// if we also want to catch exceptions encountered while importing defaults inside the editor, probably
		// need to also perform some sort of cleanup on the class (unset flags, etc.)
		if ( GEditor->Bootstrapping )
		{
			Success = 0;
		}

		else
		{
			throw ErrorMsg;
		}		
	}
	
	if ( !bParseOnly && Success )
	{
		Success = CompileScripts( AllClasses, Compiler, BaseClass, MakeSubclasses );
		if ( Success )
		{
			// Need to do this after CompileScripts.
			TLookupMap<UScriptStruct*> ScriptStructs;
			for( TObjectIterator<UScriptStruct> ObjIt; ObjIt; ++ObjIt )
			{
				UScriptStruct* Struct = *ObjIt;
				if ( LimitOuter == NULL || Struct->IsIn(LimitOuter) )
				{
					ScriptStructs.AddItem( Struct );
				}
			}

			try
			{
				/*
				Headers must be exported *after* the compilation phase.  Since the compiler bases its determination
				of whether a C++ class matches its script layout on whether the property block of that class's C++ 
				declaration is the same as the version on disk, you cannot export updated header files until you're
				sure that the script compilation can proceed.
				*/ 
				ExportNativeHeaders(LimitOuter, AllClasses);

				if ( !bHeaders )
				{
					Success = CompileStructDefaults( ScriptStructs, Compiler, BaseClass );
					Success = Success && Compiler.CompileClassDefaults( AllClasses, MakeSubclasses );
				}
			}
			catch ( TCHAR* ErrorMsg )
			{
				Warn->Log( NAME_Error, ErrorMsg );

				//@todo: is the following necessary?  i.e. do we only want to catch exceptions when running make?
				// if we also want to catch exceptions encountered while importing defaults inside the editor, probably
				// need to also perform some sort of cleanup on the class (unset flags, etc.)
				if ( GEditor->Bootstrapping )
				{
					Success = 0;
				}

				else
				{
					throw ErrorMsg;
				}
			}
		}
	}

	// Done with make.
	if( Success )
	{
		if ( !bParseOnly )
		{
			// Success.
			if( Compiler.LinesCompiled )
			{
				Compiler.AddResultText( TEXT("Success: Compiled %i line(s), %i statement(s).\r\n"), Compiler.LinesCompiled, Compiler.StatementsCompiled );
			}
			else
			{
				Compiler.AddResultText( TEXT("Success: Everything is up to date") );
			}
		}
	}
	else
	{
		// Restore all classes after compile fails.
		if( Transaction )
		{
			Transaction->Apply( );
		}
	}

	if ( !bParseOnly )
	{
		// Cleanup all exported property text.
		ClearDefaultPropertyText(AllClasses);
	}

	GUglyHackFlags = OriginalHackFlags;
	if( Transaction )
	{
		delete Transaction;
		Transaction = NULL;
	}
	Mark.Pop();

	return Success;
}

FString GetErrorLevelText( EScriptCompilerErrorLevel ErrorLevel )
{
	switch ( ErrorLevel )
	{
	CASE_TEXT(SCEL_Unknown);
	CASE_TEXT(SCEL_Restricted);
	CASE_TEXT(SCEL_Parse);
	CASE_TEXT(SCEL_Limit);
	CASE_TEXT(SCEL_Formatting);
	CASE_TEXT(SCEL_Expression);
	CASE_TEXT(SCEL_NestLevel);
	CASE_TEXT(SCEL_Class);
	CASE_TEXT(SCEL_Fatal);
	}
	return TEXT("");
}

FString GetWarningLevelText( EScriptCompilerWarningLevel WarningLevel )
{
	switch ( WarningLevel )
	{
	CASE_TEXT(SCWL_Level1);
	CASE_TEXT(SCWL_Level2);
	CASE_TEXT(SCWL_Level3);
	CASE_TEXT(SCWL_Level4);
	}
	return TEXT("");
}

const TCHAR* GetPropertyTypeText( EPropertyType Type )
{
	switch ( Type )
	{
		CASE_TEXT(CPT_None);
		CASE_TEXT(CPT_Byte);
		CASE_TEXT(CPT_Int);
		CASE_TEXT(CPT_Bool);
		CASE_TEXT(CPT_Float);
		CASE_TEXT(CPT_ObjectReference);
		CASE_TEXT(CPT_Interface);
		CASE_TEXT(CPT_Name);
		CASE_TEXT(CPT_Delegate);
		CASE_TEXT(CPT_Range);
		CASE_TEXT(CPT_Struct);
		CASE_TEXT(CPT_Vector);
		CASE_TEXT(CPT_Rotation);
		CASE_TEXT(CPT_String);
		CASE_TEXT(CPT_MAX);
	}

	return TEXT("");
}

const TCHAR* GetPropertyRefText( EPropertyReferenceType Type )
{
#ifdef _DEBUG
	switch ( Type )
	{
		CASE_TEXT(CPRT_None);
		CASE_TEXT(CPRT_AssignValue);
		CASE_TEXT(CPRT_SimpleReference);
		CASE_TEXT(CPRT_AssignmentReference);
		CASE_TEXT(CPRT_DualReference);
	}
#endif

	return TEXT("");
}
