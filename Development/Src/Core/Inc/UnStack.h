/*=============================================================================
	UnStack.h: UnrealScript execution stack definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

class UStruct;

/*-----------------------------------------------------------------------------
	Constants & types.
-----------------------------------------------------------------------------*/

// Sizes.
enum {MAX_STRING_CONST_SIZE		= 1024              }; 
/** this is the size of the buffer used by the script interpreter for unused simple (not constructed) return values.
 * Larger return values use the constructed value path (EX_EatReturnValue/execEatReturnValue)
 *NOTE - Changed usage to dwords to ensure 4 byte alignment
 */
enum {MAX_SIMPLE_RETURN_VALUE_SIZE_IN_DWORDS = (64/sizeof(DWORD))}; 

/**
 * a typedef for the size of an UnrealScript variable; typedef'd because this value must be synchronized between the
 * script compiler and the script VM
 */
typedef WORD VariableSizeType;


/**
 * a typedef for the number of bytes to skip-over when certain expressions are evaluated by the VM (e.g. context expressions that resolve to NULL, etc.)
 * typedef'd because this type must be synchronized between the script compiler and the script VM
 */
typedef WORD CodeSkipSizeType;

/**
 * This is the largest possible size that a single variable can be; a variables size is determined by multiplying the
 * size of the type by the variables ArrayDim (always 1 unless it's a static array).
 */
enum {MAX_VARIABLE_SIZE = 0x0FFF };

//
// UnrealScript intrinsic return value declaration.
//
#define RESULT_DECL void*const Result


//
// State flags.
//
enum EStateFlags
{
	// State flags.
	STATE_Editable		= 0x00000001,	// State should be user-selectable in UnrealEd.
	STATE_Auto			= 0x00000002,	// State is automatic (the default state).
	STATE_Simulated     = 0x00000004,   // State executes on client side.
	STATE_HasLocals		= 0x00000008,	// State has local variables.
};

//
// Function flags.
//
enum EFunctionFlags
{
	// Function flags.
	FUNC_Final				= 0x00000001,	// Function is final (prebindable, non-overridable function).
	FUNC_Defined			= 0x00000002,	// Function has been defined (not just declared).
	FUNC_Iterator			= 0x00000004,	// Function is an iterator.
	FUNC_Latent				= 0x00000008,	// Function is a latent state function.
	FUNC_PreOperator		= 0x00000010,	// Unary operator is a prefix operator.
	FUNC_Singular			= 0x00000020,   // Function cannot be reentered.
	FUNC_Net				= 0x00000040,   // Function is network-replicated.
	FUNC_NetReliable		= 0x00000080,   // Function should be sent reliably on the network.
	FUNC_Simulated			= 0x00000100,	// Function executed on the client side.
	FUNC_Exec				= 0x00000200,	// Executable from command line.
	FUNC_Native				= 0x00000400,	// Native function.
	FUNC_Event				= 0x00000800,   // Event function.
	FUNC_Operator			= 0x00001000,   // Operator function.
	FUNC_Static				= 0x00002000,   // Static function.
	FUNC_HasOptionalParms	= 0x00004000,	// Function has optional parameters
	FUNC_Const				= 0x00008000,   // Function doesn't modify this object.
	//						= 0x00010000,	// unused
	FUNC_Public				= 0x00020000,	// Function is accessible in all classes (if overridden, parameters much remain unchanged).
	FUNC_Private			= 0x00040000,	// Function is accessible only in the class it is defined in (cannot be overriden, but function name may be reused in subclasses.  IOW: if overridden, parameters don't need to match, and Super.Func() cannot be accessed since it's private.)
	FUNC_Protected			= 0x00080000,	// Function is accessible only in the class it is defined in and subclasses (if overridden, parameters much remain unchanged).
	FUNC_Delegate			= 0x00100000,	// Function is actually a delegate.
	FUNC_NetServer			= 0x00200000,	// Function is executed on servers (set by replication code if passes check)
	FUNC_HasOutParms		= 0x00400000,	// function has out (pass by reference) parameters
	FUNC_HasDefaults		= 0x00800000,	// function has structs that contain defaults
	FUNC_NetClient			= 0x01000000,	// function is executed on clients
	FUNC_DLLImport			= 0x02000000,	// function is imported from a DLL

	// Combinations of flags.
	FUNC_FuncInherit        = FUNC_Exec | FUNC_Event,
	FUNC_FuncOverrideMatch	= FUNC_Exec | FUNC_Final | FUNC_Latent | FUNC_PreOperator | FUNC_Iterator | FUNC_Static | FUNC_Public | FUNC_Protected | FUNC_Const,
	FUNC_NetFuncFlags       = FUNC_Net | FUNC_NetReliable | FUNC_NetServer | FUNC_NetClient,

	FUNC_AllFlags		= 0xFFFFFFFF,
};


enum ERuntimeUCFlags
{
	RUC_ArrayLengthSet			=	0x01,	//setting the length of an array, not an element of the array
	RUC_SkippedOptionalParm		=	0x02,	// value for an optional parameter was not included in the function call
	RUC_NeverExpandDynArray		=	0x04,	// flag for execDynArrayElement() to prevent it increasing the array size for index out of bounds
};

//
// Evaluatable expression item types.
//
enum EExprToken
{
	// Variable references.
	EX_LocalVariable		= 0x00,	// A local variable.
	EX_InstanceVariable		= 0x01,	// An object variable.
	EX_DefaultVariable		= 0x02,	// Default variable for a concrete object.
	EX_StateVariable		= 0x03, // State local variable.

	// Tokens.
	EX_Return				= 0x04,	// Return from function.
	EX_Switch				= 0x05,	// Switch.
	EX_Jump					= 0x06,	// Goto a local address in code.
	EX_JumpIfNot			= 0x07,	// Goto if not expression.
	EX_Stop					= 0x08,	// Stop executing state code.
	EX_Assert				= 0x09,	// Assertion.
	EX_Case					= 0x0A,	// Case.
	EX_Nothing				= 0x0B,	// No operation.
	EX_LabelTable			= 0x0C,	// Table of labels.
	EX_GotoLabel			= 0x0D,	// Goto a label.
	EX_EatReturnValue       = 0x0E, // destroy an unused return value
	EX_Let					= 0x0F,	// Assign an arbitrary size value to a variable.
	EX_DynArrayElement      = 0x10, // Dynamic array element.!!
	EX_New                  = 0x11, // New object allocation.
	EX_ClassContext         = 0x12, // Class default metaobject context.
	EX_MetaCast             = 0x13, // Metaclass cast.
	EX_LetBool				= 0x14, // Let boolean variable.
	EX_EndParmValue			= 0x15,	// end of default value for optional function parameter
	EX_EndFunctionParms		= 0x16,	// End of function call parameters.
	EX_Self					= 0x17,	// Self object.
	EX_Skip					= 0x18,	// Skippable expression.
	EX_Context				= 0x19,	// Call a function through an object context.
	EX_ArrayElement			= 0x1A,	// Array element.
	EX_VirtualFunction		= 0x1B,	// A function call with parameters.
	EX_FinalFunction		= 0x1C,	// A prebound function call with parameters.
	EX_IntConst				= 0x1D,	// Int constant.
	EX_FloatConst			= 0x1E,	// Floating point constant.
	EX_StringConst			= 0x1F,	// String constant.
	EX_ObjectConst		    = 0x20,	// An object constant.
	EX_NameConst			= 0x21,	// A name constant.
	EX_RotationConst		= 0x22,	// A rotation constant.
	EX_VectorConst			= 0x23,	// A vector constant.
	EX_ByteConst			= 0x24,	// A byte constant.
	EX_IntZero				= 0x25,	// Zero.
	EX_IntOne				= 0x26,	// One.
	EX_True					= 0x27,	// Bool True.
	EX_False				= 0x28,	// Bool False.
	EX_NativeParm           = 0x29, // Native function parameter offset.
	EX_NoObject				= 0x2A,	// NoObject.

	EX_IntConstByte			= 0x2C,	// Int constant that requires 1 byte.
	EX_BoolVariable			= 0x2D,	// A bool variable which requires a bitmask.
	EX_DynamicCast			= 0x2E,	// Safe dynamic class casting.
	EX_Iterator             = 0x2F, // Begin an iterator operation.
	EX_IteratorPop          = 0x30, // Pop an iterator level.
	EX_IteratorNext         = 0x31, // Go to next iteration.
	EX_StructCmpEq          = 0x32,	// Struct binary compare-for-equal.
	EX_StructCmpNe          = 0x33,	// Struct binary compare-for-unequal.
	EX_UnicodeStringConst   = 0x34, // Unicode string constant.
	EX_StructMember         = 0x35, // Struct member.
	EX_DynArrayLength		= 0x36,	// A dynamic array length for setting/getting
	EX_GlobalFunction		= 0x37, // Call non-state version of a function.
	EX_PrimitiveCast		= 0x38,	// A casting operator for primitives which reads the type as the subsequent byte
	EX_DynArrayInsert		= 0x39,	// Inserts into a dynamic array
	EX_ReturnNothing		= 0x3A, // failsafe for functions that return a value - returns the zero value for a property and logs that control reached the end of a non-void function
	EX_EqualEqual_DelDel	= 0x3B,	// delegate comparison for equality
	EX_NotEqual_DelDel		= 0x3C, // delegate comparison for inequality
	EX_EqualEqual_DelFunc	= 0x3D,	// delegate comparison for equality against a function
	EX_NotEqual_DelFunc		= 0x3E,	// delegate comparison for inequality against a function
	EX_EmptyDelegate		= 0x3F,	// delegate 'None'
	EX_DynArrayRemove		= 0x40,	// Removes from a dynamic array
	EX_DebugInfo			= 0x41,	//DEBUGGER Debug information
	EX_DelegateFunction		= 0x42, // Call to a delegate function
	EX_DelegateProperty		= 0x43, // Delegate expression
	EX_LetDelegate			= 0x44, // Assignment to a delegate
	EX_Conditional			= 0x45, // tertiary operator support
	EX_DynArrayFind			= 0x46, // dynarray search for item index
	EX_DynArrayFindStruct	= 0x47, // dynarray<struct> search for item index
	EX_LocalOutVariable		= 0x48, // local out (pass by reference) function parameter
	EX_DefaultParmValue		= 0x49,	// default value of optional function parameter
	EX_EmptyParmValue		= 0x4A,	// unspecified value for optional function parameter
	EX_InstanceDelegate		= 0x4B,	// const reference to a delegate or normal function object




	EX_InterfaceContext		= 0x51,	// Call a function through a native interface variable
	EX_InterfaceCast		= 0x52,	// Converting an object reference to native interface variable
	EX_EndOfScript			= 0x53, // Last byte in script code
	EX_DynArrayAdd			= 0x54, // Add to a dynamic array
	EX_DynArrayAddItem		= 0x55, // Add an item to a dynamic array
	EX_DynArrayRemoveItem	= 0x56, // Remove an item from a dynamic array
	EX_DynArrayInsertItem	= 0x57, // Insert an item into a dynamic array
	EX_DynArrayIterator		= 0x58, // Iterate through a dynamic array
	EX_DynArraySort			= 0x59,	// Sort a list in place
	EX_JumpIfFilterEditorOnly	= 0x5A,	// Skip the code block if the editor is not present.

	// Natives.
	EX_ExtendedNative		= 0x60,
	EX_FirstNative			= 0x70,
	EX_Max					= 0x1000,
};


enum ECastToken
{
	CST_InterfaceToObject	= 0x36,
	CST_InterfaceToString	= 0x37,
	CST_InterfaceToBool		= 0x38,
	CST_RotatorToVector		= 0x39,
	CST_ByteToInt			= 0x3A,
	CST_ByteToBool			= 0x3B,
	CST_ByteToFloat			= 0x3C,
	CST_IntToByte			= 0x3D,
	CST_IntToBool			= 0x3E,
	CST_IntToFloat			= 0x3F,
	CST_BoolToByte			= 0x40,
	CST_BoolToInt			= 0x41,
	CST_BoolToFloat			= 0x42,
	CST_FloatToByte			= 0x43,
	CST_FloatToInt			= 0x44,
	CST_FloatToBool			= 0x45,
	CST_ObjectToInterface	= 0x46,
	CST_ObjectToBool		= 0x47,
	CST_NameToBool			= 0x48,
	CST_StringToByte		= 0x49,
	CST_StringToInt			= 0x4A,
	CST_StringToBool		= 0x4B,
	CST_StringToFloat		= 0x4C,
	CST_StringToVector		= 0x4D,
	CST_StringToRotator		= 0x4E,
	CST_VectorToBool		= 0x4F,
	CST_VectorToRotator		= 0x50,
	CST_RotatorToBool		= 0x51,
	CST_ByteToString		= 0x52,
	CST_IntToString			= 0x53,
	CST_BoolToString		= 0x54,
	CST_FloatToString		= 0x55,
	CST_ObjectToString		= 0x56,
	CST_NameToString		= 0x57,
	CST_VectorToString		= 0x58,
	CST_RotatorToString		= 0x59,
	CST_DelegateToString	= 0x5A,
// 	CST_StringToDelegate	= 0x5B,
	CST_StringToName		= 0x60,
	CST_Max					= 0xFF,
};


//
// Latent functions.
//
enum EPollSlowFuncs
{
	EPOLL_Sleep				= 384,
	EPOLL_FinishAnim		= 385
};

/*-----------------------------------------------------------------------------
	Execution stack helpers.
-----------------------------------------------------------------------------*/

//
// Information remembered about an Out parameter.
//
struct FOutParmRec
{
	UProperty* Property;
	BYTE*      PropAddr;
	FOutParmRec* NextOutParm;
};

//
// Information about script execution at one stack level.
//
struct FFrame : public FOutputDevice
{	
	// Variables.
	UStruct*	Node;
	UObject*	Object;
	BYTE*		Code;
	BYTE*		Locals;

	/** Previous frame on the stack */
	FFrame* PreviousFrame;
	/** contains information on any out parameters */
	FOutParmRec* OutParms;

	// Constructors.
	FFrame( UObject* InObject );
	FFrame( UObject* InObject, UStruct* InNode, INT CodeOffset, void* InLocals, FFrame* InPreviousFrame = NULL );

	virtual ~FFrame()
	{}

	// Functions.
	FORCEINLINE void Step( UObject* Context, RESULT_DECL );
	void Serialize( const TCHAR* V, enum EName Event );
	
	INT ReadInt();
	FLOAT ReadFloat();
	FName ReadName();
	UObject* ReadObject();
	INT ReadWord();

	/**
	 * Reads a value from the bytestream, which represents the number of bytes to advance
	 * the code pointer for certain expressions.
	 *
	 * @param	ExpressionField		receives a pointer to the field representing the expression; used by various execs
	 *								to drive VM logic
	 */
	CodeSkipSizeType ReadCodeSkipCount();

	/**
	 * Reads a value from the bytestream which represents the number of bytes that should be zero'd out if a NULL context
	 * is encountered
	 *
	 * @param	ExpressionField		receives a pointer to the field representing the expression; used by various execs
	 *								to drive VM logic
	 */
	VariableSizeType ReadVariableSize( UField** ExpressionField=NULL );

	/**
 	 * This will return the StackTrace of the current callstack from .uc land
	 **/
	FString GetStackTrace() const;
};

/**
 * Contains information about script execution at the main stack
 * level.  This part of an object's script state is saveable at
 * any time.
 */
struct FStateFrame : public FFrame
{
	/** Current state node */
	UState* StateNode;

	/** Mask for all functions the current state is probing */
	DWORD ProbeMask;

	/** Current latent action, 0 for no active latent */
	WORD     LatentAction;

	/** set during the time between when a state is continued (via a higher state being popped via PopState) 
	 * until the object's state code is next executed
	 * used to update the script debugger's view of the script stack
	 */
	BYTE bContinuedState;

	/** Contains information needed for returning to a previous state */
	struct FPushedState
	{
	public:
		UState* State;
		UStruct* Node;
		BYTE* Code;
		
		FPushedState()
		: State(NULL)
		, Node(NULL)
		, Code(NULL)
		{
		}
	};
	friend FArchive& operator<<(FArchive& Ar,FPushedState& PushedState);

	/** List of the currently pushed states */
	TArray<FPushedState> StateStack;

	/** UClass from which LocalVars was created (needed for safe destruction) */
	UClass* LocalVarsOwner;

	// copy constructor needs to avoid copying state locals, since that would result in double freeing constructed variables
	//@warning: assumes the StateFrame on UObject is never created via this constructor (currently only used in ProcessState() for state transition safety)
	FStateFrame(const FStateFrame& Other)
		: FFrame(Other), StateNode(Other.StateNode), ProbeMask(Other.ProbeMask), LatentAction(Other.LatentAction), bContinuedState(Other.bContinuedState),
			StateStack(Other.StateStack), LocalVarsOwner(NULL)
	{
		Locals = NULL;
	}
	FStateFrame& operator=(const FStateFrame& Other)
	{
		FFrame::operator=(Other);
		StateNode = Other.StateNode;
		ProbeMask = Other.ProbeMask;
		LatentAction = Other.LatentAction;
		bContinuedState = Other.bContinuedState;
		StateStack = Other.StateStack;
		Locals = NULL;
		LocalVarsOwner = NULL;
		return *this;
	}

	virtual ~FStateFrame()
	{
		if (Locals != NULL)
		{
			ClearLocalVars();
			appFree(Locals);
			Locals = NULL;
			LocalVarsOwner = NULL;
		}
	}

	// Functions.
	FStateFrame(UObject* InObject);
	FString Describe();
	/**
	 * Allocate space for all state local variables.
	 *
	 * @param	InClass		Class object in which the states for this StateFrame reside in
	 */
	void InitLocalVars(UClass* InClass);
	/** safely destructs and clears all state local variables */
	void ClearLocalVars();
};

/*-----------------------------------------------------------------------------
	Script execution helpers.
-----------------------------------------------------------------------------*/

//
// Base class for UnrealScript iterator lists.
//
struct FIteratorList
{
	FIteratorList* Next;
	FIteratorList() {}
	FIteratorList( FIteratorList* InNext ) : Next( InNext ) {}
	FIteratorList* GetNext() { return (FIteratorList*)Next; }
};


