/*=============================================================================
	UnScript.h: UnrealScript execution engine.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Native functions.
-----------------------------------------------------------------------------*/

/** The type of a native function callable by script */
typedef void (UObject::*Native)( FFrame& TheStack, RESULT_DECL );

//
// Native function table.
//
extern Native GNatives[];
BYTE GRegisterNative( INT iNative, const Native& Func );

extern Native GCasts[];
BYTE GRegisterCast( INT CastCode, const Native& Func );


//
// Registering a native function.
//
#define IMPLEMENT_FUNCTION(cls,num,func) \
	extern "C" { Native int##cls##func = (Native)&cls::func; } \
	static BYTE cls##func##Temp = GRegisterNative( num, int##cls##func );


/**
 * Property data type enums.
 * @warning: if values in this enum are modified, you must update:
 * - these are also used in the GConversions table, indexed by enum value
 * - FPropertyBase::GetSize() hardcodes the sizes for each property type
 */
enum EPropertyType
{
	CPT_None			= 0,
	CPT_Byte			= 1,
	CPT_Int				= 2,
	CPT_Bool			= 3,
	CPT_Float			= 4,
	CPT_ObjectReference	= 5,
	CPT_Name			= 6,
	CPT_Delegate		= 7,
	CPT_Interface		= 8,
	CPT_Range           = 9,
	CPT_Struct			= 10,
	CPT_Vector          = 11,
	CPT_Rotation        = 12,
	CPT_String          = 13,
	CPT_Map				= 14,

	// when you add new property types, make sure you add the corresponding entry
	// in the PropertyTypeToNameMap array in UnScrCom.cpp!!
	CPT_MAX				= 15,
};

/*-----------------------------------------------------------------------------
	FFrame implementation.
-----------------------------------------------------------------------------*/

inline FFrame::FFrame( UObject* InObject )
:	Node		(InObject ? InObject->GetClass() : NULL)
,	Object		(InObject)
,	Code		(NULL)
,	Locals		(NULL)
,	PreviousFrame	(NULL)
,	OutParms	(NULL)
{}
inline FFrame::FFrame( UObject* InObject, UStruct* InNode, INT CodeOffset, void* InLocals, FFrame* InPreviousFrame )
:	Node		(InNode)
,	Object		(InObject)
,	Code		(&InNode->Script(CodeOffset))
,	Locals		((BYTE*)InLocals)
,	PreviousFrame	(InPreviousFrame)
,	OutParms	(NULL)
{}
inline INT FFrame::ReadInt()
{
	INT Result;
#ifdef REQUIRES_ALIGNED_INT_ACCESS
	appMemcpy(&Result, Code, sizeof(INT));
#else
	Result = *(INT*)Code;
#endif
	Code += sizeof(INT);
	return Result;
}
inline UObject* FFrame::ReadObject()
{
	// we always pull 64-bits of data out, which is really a UObject* in some representation (depending on platform)
	ScriptPointerType TempCode;

#ifdef REQUIRES_ALIGNED_INT_ACCESS
	appMemcpy(&TempCode, Code, sizeof(ScriptPointerType));
#else
	TempCode = *(ScriptPointerType*)Code;
#endif

	// turn that DWORD into a UObject pointer
	UObject* Result = (UObject*)appSPtrToPointer(TempCode);
	Code += sizeof(ScriptPointerType);
	return Result;
}
inline FLOAT FFrame::ReadFloat()
{
	FLOAT Result;
#ifdef REQUIRES_ALIGNED_ACCESS
	appMemcpy(&Result, Code, sizeof(FLOAT));
#else
	Result = *(FLOAT*)Code;
#endif
	Code += sizeof(FLOAT);
	return Result;
}
inline INT FFrame::ReadWord()
{
	INT Result;
#ifdef REQUIRES_ALIGNED_INT_ACCESS
	WORD Temporary;
	appMemcpy(&Temporary, Code, sizeof(WORD));
	Result = Temporary;
#else
	Result = *(WORD*)Code;
#endif
	Code += sizeof(WORD);
	return Result;
}
/**
 * Reads a value from the bytestream, which represents the number of bytes to advance
 * the code pointer for certain expressions.
 */
inline CodeSkipSizeType FFrame::ReadCodeSkipCount()
{
	CodeSkipSizeType Result;

#ifdef REQUIRES_ALIGNED_INT_ACCESS
	appMemcpy(&Result, Code, sizeof(CodeSkipSizeType));
#else
	Result = *(CodeSkipSizeType*)Code;
#endif

	Code += sizeof(CodeSkipSizeType);
	return Result;
}

inline VariableSizeType FFrame::ReadVariableSize( UField** ExpressionField/*=NULL*/ )
{
	VariableSizeType Result=0;

	UObject* Field = ReadObject();
	BYTE NullPropertyType=*Code++;

	if ( Field != NULL )
	{
		UProperty* Property = Cast<UProperty>(Field);
		if ( Property != NULL )
		{
			Result = Property->GetSize();
		}
		else
		{
			UEnum* ExplicitEnumValue = Cast<UEnum>(Field);
			if ( ExplicitEnumValue != NULL )
			{
				Result = 1;
			}
			else
			{
				UFunction* FunctionRef = Cast<UFunction>(Field);
				if ( FunctionRef != NULL )
				{
					Result = sizeof(ScriptPointerType);
				}
			}
		}
	}
	else
	{
		switch ( NullPropertyType )
		{
		case CPT_None:
			// nothing...
			break;
		case CPT_Byte:		Result = sizeof(BYTE);
			break;
		case CPT_Int:		Result = sizeof(INT);
			break;
		case CPT_Bool:		Result = sizeof(UBOOL);
			break;
		case CPT_Float:		Result = sizeof(FLOAT);
			break;
		case CPT_Name:		Result = sizeof(FName);
			break;
		case CPT_Vector:	Result = sizeof(FVector);
			break;
		case CPT_Rotation:	Result = sizeof(FRotator);
			break;
		case CPT_Delegate:	Result = sizeof(FScriptDelegate);
			break;
		default:
			appErrorf(TEXT("Unhandled property type in FFrame::ReadVariableSize(): %u"), NullPropertyType);
			break;
		}
	}

	if ( ExpressionField != NULL )
	{
		*ExpressionField = Cast<UField>(Field);
	}

	return Result;
}
inline FName FFrame::ReadName()
{
	FName Result;
#ifdef REQUIRES_ALIGNED_ACCESS
	appMemcpy(&Result, Code, sizeof(FName));
#else
	Result = *(FName*)Code;
#endif
	Code += sizeof(FName);
	return Result;
}

void GInitRunaway();
FORCEINLINE void FFrame::Step(UObject *Context, RESULT_DECL)
{
	INT B = *Code++;
	(Context->*GNatives[B])(*this,Result);
}


/**
 * This will return the StackTrace of the current callstack from .uc land
 **/
inline FString FFrame::GetStackTrace() const
{
	FString Retval;

	// travel down the stack recording the frames
	TArray<const FFrame*> FrameStack;
	const FFrame* CurrFrame = this;
	while( CurrFrame != NULL )
	{
		FrameStack.AddItem(CurrFrame);
		CurrFrame = CurrFrame->PreviousFrame;
	}
	
	// and then dump them to a string
	Retval += FString( TEXT("Script call stack:\n") );
	for( INT idx = FrameStack.Num() - 1; idx >= 0; idx-- )
	{
		Retval += FString::Printf( TEXT("\t%s\n"), *FrameStack(idx)->Node->GetFullName() );
	}

	return Retval;
}


/*-----------------------------------------------------------------------------
	FStateFrame implementation.
-----------------------------------------------------------------------------*/

inline FStateFrame::FStateFrame(UObject* InObject)
:	FFrame		( InObject )
,	StateNode	( InObject->GetClass() )
,	ProbeMask	( ~(DWORD)0 )
,	bContinuedState(FALSE)
,	LocalVarsOwner(NULL)
{}

inline FString FStateFrame::Describe()
{
	return Node ? Node->GetFullName() : TEXT("None");
}

/**
 * Allocate space for all state local variables.
 *
 * @param	InClass		Class object in which the states for this StateFrame reside in
 */
inline void FStateFrame::InitLocalVars(UClass* InClass)
{
	checkSlow(InClass != NULL);
	checkSlow(LocalVarsOwner == NULL || InClass == LocalVarsOwner);
	if (Locals == NULL)
	{
		INT LocalVarSize = 0;

		for (TFieldIterator<UState> State(InClass); State; ++State)
		{
			if (State->StateFlags & STATE_HasLocals)
			{
				LocalVarSize += State->PropertiesSize;
			}
		}

		if (LocalVarSize > 0)
		{
			Locals = (BYTE*)appMalloc(LocalVarSize);
			appMemzero(Locals, LocalVarSize);
			LocalVarsOwner = InClass;
		}
	}
}

inline void FStateFrame::ClearLocalVars()
{
	// during the exit purge, classes may be destroyed before their instances, so we cannot safely iterate states/properties
	if (Locals != NULL && !GExitPurge)
	{
		INT LocalVarSize = 0;
		for (TFieldIterator<UState> State(LocalVarsOwner); State; ++State)
		{
			if (State->StateFlags & STATE_HasLocals)
			{
				for (UProperty* Prop = State->ConstructorLink; Prop != NULL; Prop = Prop->ConstructorLinkNext)
				{
					Prop->DestroyValue(Locals + Prop->Offset);
				}
				LocalVarSize += State->PropertiesSize;
			}
		}
		appMemzero(Locals, LocalVarSize);
	}
}
