/*=============================================================================
	UnScrCom.cpp: UnrealScript compiler.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney
	* 11/8/00 Steve Polge integrated enhancements by:
		Paul Dubois - Infinite Machine
		Aaron Leiby - Legend Entertainment
		Chris Hargrove - Legend Entertainment
=============================================================================*/

#include "EditorPrivate.h"
#include "UnScrCom.h"
#include "OpCode.h"

/*-----------------------------------------------------------------------------
	Constants & declarations.
-----------------------------------------------------------------------------*/

enum {MAX_ARRAY_SIZE=2048};
static UBOOL GCheckNatives=1;

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
			/*   None      Byte                Int                 Bool              Float                Object             Name             Delegate                                               Struct           Vector               Rotator              String              */
			/*   --------  ------------------  ------------------  ----------------  -------------------  -----------------  ---------------  -------------------  ---------------  ---------------  ---------------- -------------------  -------------------- -----------------   */
/* None     */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,             CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_Max,             },
/* Byte     */ { CST_Max,  CST_Max,            CST_IntToByte|TAC,  CST_BoolToByte,	 CST_FloatToByte|TAC, CST_Max,           CST_Max,         CST_Max,             CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_StringToByte,    },
/* Int      */ { CST_Max,  CST_ByteToInt|AC,   CST_Max,            CST_BoolToInt,    CST_FloatToInt|TAC,  CST_Max,           CST_Max,         CST_Max,             CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_StringToInt,     },
/* Bool     */ { CST_Max,  CST_ByteToBool,     CST_IntToBool,      CST_Max,          CST_FloatToBool,     CST_ObjectToBool,  CST_NameToBool,  CST_Max,             CST_Max,         CST_Max,         CST_Max,         CST_VectorToBool,    CST_RotatorToBool,   CST_StringToBool,    },
/* Float    */ { CST_Max,  CST_ByteToFloat|AC, CST_IntToFloat|AC,  CST_BoolToFloat,  CST_Max,             CST_Max,           CST_Max,         CST_Max,	           CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_StringToFloat,   },
/* Object   */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,             CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_Max,             },
/* Name     */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,             CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_StringToName,    },
/*          */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,             CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_Max,             },
/*          */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,             CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_Max,             },
/*          */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,             CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_Max,             },
/* Struct   */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,             CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_Max,             CST_Max,             },
/* Vector   */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,			   CST_Max,         CST_Max,         CST_Max,         CST_Max,             CST_RotatorToVector, CST_StringToVector,  },
/* Rotator  */ { CST_Max,  CST_Max,            CST_Max,            CST_Max,          CST_Max,             CST_Max,           CST_Max,         CST_Max,			   CST_Max,         CST_Max,         CST_Max,         CST_VectorToRotator, CST_Max,             CST_StringToRotator, },
/* String   */ { CST_Max,  CST_ByteToString,   CST_IntToString,    CST_BoolToString, CST_FloatToString,   CST_ObjectToString,CST_NameToString,CST_Max,			   CST_Max,         CST_Max,         CST_Max,         CST_VectorToString,  CST_RotatorToString, CST_Max,             },
};
#undef AC
#undef TAC
	INT DestType = Dest.IsVector() ? CPT_Vector : Dest.IsRotator() ? CPT_Rotation : Dest.Type;
	INT SrcType  = Src .IsVector() ? CPT_Vector : Src .IsRotator() ? CPT_Rotation : Src.Type;
	return GConversions[DestType][SrcType];
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
	FString Path = appBaseDir(), Result;
	if( Path.Right(1)==PATH_SEPARATOR )
		Path = Path.LeftChop( 1 );
	while( Path.Len() && Path.Right(1)!=PATH_SEPARATOR )
		Path = Path.LeftChop( 1 );
	if (InDefaultPropContext)
		return FString::Printf
		(
			TEXT("%sDevelopment\\Src\\%s\\Classes\\%s.uc(1) : defaultproperties"),
			*Path,
			Class->GetOuter()->GetName(),
			Class->GetName()
		);
	return FString::Printf
	(
		TEXT("%sDevelopment\\Src\\%s\\Classes\\%s.uc(%i)"),
		*Path,
		Class->GetOuter()->GetName(),
		Class->GetName(),
		InputLine
	);
}

/*-----------------------------------------------------------------------------
	Single-character processing.
-----------------------------------------------------------------------------*/

//
// Get a single character from the input stream and return it, or 0=end.
//
TCHAR FScriptCompiler::GetChar( UBOOL Literal )
{
	int CommentCount=0;

	PrevPos  = InputPos;
	PrevLine = InputLine;

	Loop:
	TCHAR c = Input[InputPos++];
	if( c==0x0a )
	{
		InputLine++;
	}
	else if( !Literal && c=='/' && Input[InputPos]=='*' )
	{
		CommentCount++;
		InputPos++;
		goto Loop;
	}
	else if( !Literal && c=='*' && Input[InputPos]=='/' )
	{
		if( --CommentCount < 0 )
			appThrowf( TEXT("Unexpected '*/' outside of comment") );
		InputPos++;
		goto Loop;
	}
	if( CommentCount > 0 )
	{
		if( c==0 )
			appThrowf( TEXT("End of script encountered inside comment") );
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
	Skip1: do c=GetChar(); while( c==0x20 || c==0x09 || c==0x0d || c==0x0a );
	if( c=='/' && Input[InputPos]=='/' )
	{
		// Comment, so skip to start of next line.
		do c=GetChar(1); while( c!=0x0d && c!=0x0a && c!=0x00 );
		goto Skip1;
	}
	return c;
}

//
// Return 1 if input as a valid end-of-line character, or 0 if not.
// EOL characters are: Comment, CR, linefeed, 0 (end-of-file mark)
//
INT FScriptCompiler::IsEOL( TCHAR c )
{
	return c==0x0d || c==0x0a || c==0;
}

/*-----------------------------------------------------------------------------
	Code emitting.
-----------------------------------------------------------------------------*/

//!! Yoda Debugger
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

INT FScriptCompiler::EmitOuterContext()
{
	static UProperty* OuterProp=NULL;

	// Initialize OuterProp to UProperty Engine.Object.Outer (only needs to be done once since it never needs to change).
	if( !OuterProp )
		for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(UObject::StaticClass()); It; ++It )
			if( It->GetFName()==NAME_Outer )
				{ OuterProp = *It; break; }

	Writer << EX_Context;
	Writer << EX_InstanceVariable;
	Writer << OuterProp;
	
	_WORD wSize=5;		// hardcoded value: since we're always emiting Outer here, we shouldn't have to worry about this ever changing.
	Writer << wSize;
	
	BYTE bSize=0;		// byte at this fixup address must be patched with actual size afterward.
	Writer << bSize;
	
	INT FixupPos = TopNode->Script.Num()-1;
	
	return FixupPos;
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
				_WORD OutCh;
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
					appThrowf( TEXT("Illegal actor constant") );
				Writer << EX_ObjectConst;
				Writer << Token.Object;
			}
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
				appThrowf( TEXT("Not yet implemented") );
			}
			break;
		}
		default:
		{
			appThrowf( TEXT("Internal EmitConstant token type error %i"), (BYTE)Token.Type );
		}
	}
}

//
// Emit the function corresponding to a stack node link.
//
void FScriptCompiler::EmitStackNodeLinkFunction( UFunction* Node, UBOOL ForceFinal, UBOOL Global, UBOOL bSuper, UProperty *DelegateProp )
{
	UBOOL IsFinal = (Node->FunctionFlags & FUNC_Final) || ForceFinal;

	// Emit it according to function type.
	if( IsFinal && Node->iNative && Node->iNative<256 )
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
	else if( Node->FunctionFlags & FUNC_Delegate )
	{
		// Delegate function
		Writer << EX_DelegateFunction;
		// if a NULL property was passed in, assume that this is the default delegate property
		if (DelegateProp == NULL)
		{
			DelegateProp = CastChecked<UProperty>(FindField( CastChecked<UStruct>(Node->GetOuter()), *FString::Printf(TEXT("__%s__Delegate"), Node->GetName()) ));
		}
		BYTE bLocalProp = DelegateProp->GetOuter()->IsA(UFunction::StaticClass());
		Writer << bLocalProp;
		Writer << DelegateProp;
		FName N(Node->GetFName());
		Writer << N;
	}
	else
	{
		// Virtual function.
		Writer << EX_VirtualFunction;
		// write out the super byte
		BYTE Super = bSuper ? 1 : 0;
		Writer << Super;
		FName N(Node->GetFName());
		Writer << N;
	}
}

//
// Emit a code offset which the script compiler will fix up in the
// proper PopNest call.
//
void FScriptCompiler::EmitAddressToFixupLater( FNestInfo* Nest, EFixupType Type, FName Name )
{
	// Add current code address to the nest level's fixup list.
	Nest->FixupList = new(GMem)FNestFixupRequest( Type, TopNode->Script.Num(), Name, Nest->FixupList );

	// Emit a dummy code offset as a placeholder.
	_WORD Temp=0;
	Writer << Temp;

}

//
// Emit a code offset which should be chained to later.
//
void FScriptCompiler::EmitAddressToChainLater( FNestInfo* Nest )
{
	// Note chain address in nest info.
	Nest->iCodeChain = TopNode->Script.Num();

	// Emit a dummy code offset as a placeholder.
	_WORD Temp=0;
	Writer << Temp;

}

//
// Update and reset the nest info's chain address.
//
void FScriptCompiler::EmitChainUpdate( FNestInfo* Nest )
{
	// If there's a chain address, plug in the current script offset.
	if( Nest->iCodeChain != INDEX_NONE )
	{
		check((INT)Nest->iCodeChain+(INT)sizeof(_WORD)<=TopNode->Script.Num());
		*(_WORD*)&TopNode->Script( Nest->iCodeChain ) = TopNode->Script.Num();
		Nest->iCodeChain = INDEX_NONE;
	}
}

//
// Emit a variable size, making sure it's within reason.
//
void FScriptCompiler::EmitSize( INT Size, const TCHAR* Tag )
{
	BYTE B = Size;
	if( B != Size )
		appThrowf( TEXT("%s: Variable is too large (%i bytes, 255 max)"), Tag, Size );
	Writer << B;

}

//
// Emit an assignment.
//
void FScriptCompiler::EmitLet( const FPropertyBase& Type, const TCHAR *Tag )
{
	// Validate the required type.
	if( Type.PropertyFlags & CPF_Const )
		appThrowf( TEXT("Can't assign Const variables") );
	if( Type.ArrayDim > 1 )
		appThrowf( TEXT("Can only assign individual elements, not arrays") );

	//!! Yoda Debugger
	EmitDebugInfo(DI_Let);
	
	if( Type.Type==CPT_Bool )
		Writer << EX_LetBool;
	else
	if( Type.Type==CPT_Delegate )
		Writer << EX_LetDelegate;
	else
		Writer << EX_Let;

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

//
// Get the next token from the input stream, set *Token to it.
// Returns 1 if processed a token, 0 if end of line.
//
// If you initialize the token's Type info prior to calling this
// function, we perform special logic that tries to evaluate Token in the
// context of that type. This is how we distinguish enum tags.
//
INT FScriptCompiler::GetToken( FToken& Token, const FPropertyBase* Hint, INT NoConsts )
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
			if( Length > NAME_SIZE )
				appThrowf( TEXT("Identifer length exceeds maximum of %i"), (INT)NAME_SIZE );
			c = GetChar();
		} while( ((c>='A')&&(c<='Z')) || ((c>='a')&&(c<='z')) || ((c>='0')&&(c<='9')) || (c=='_') );
		UngetChar();
		Token.Identifier[Length]=0;

		// Assume this is an identifier unless we find otherwise.
		Token.TokenType = TOKEN_Identifier;

		// Lookup the token's global name.
		Token.TokenName = FName( Token.Identifier, FNAME_Find );

		// See if the idenfitifier is part of a vector, rotation, or object constant.
		if( Token.TokenName==NAME_Vect && !NoConsts && MatchSymbol(TEXT("(")) )
		{
			// This is a vector constant.
			FVector V = FVector( 0.f, 0.f, 0.f );
			if(!GetConstFloat(V.X))		appThrowf( TEXT("Missing X component of vector" ));
			if(!MatchSymbol(TEXT(","))) appThrowf( TEXT("Missing ',' in vector"         ));
			if(!GetConstFloat(V.Y))		appThrowf( TEXT("Missing Y component of vector" ));
			if(!MatchSymbol(TEXT(","))) appThrowf( TEXT("Missing ',' in vector"         ));
			if(!GetConstFloat(V.Z))		appThrowf( TEXT("Missing Z component of vector" ));
			if(!MatchSymbol(TEXT(")"))) appThrowf( TEXT("Missing ')' in vector"         ));

			Token.SetConstVector(V);
			return 1;
		}
		if( Token.TokenName==NAME_Rot && !NoConsts && MatchSymbol(TEXT("(")) )
		{
			// This is a rotation constant.
			FRotator R;
			if(!GetConstInt(R.Pitch))   appThrowf( TEXT("Missing Pitch component of rotation" ));
			if(!MatchSymbol(TEXT(","))) appThrowf( TEXT("Missing ',' in rotation"             ));
			if(!GetConstInt(R.Yaw))     appThrowf( TEXT("Missing Yaw component of rotation"   ));
			if(!MatchSymbol(TEXT(","))) appThrowf( TEXT("Missing ',' in rotation"               ));
			if(!GetConstInt(R.Roll))    appThrowf( TEXT("Missing Roll component of rotation"  ));
			if(!MatchSymbol(TEXT(")")))	appThrowf( TEXT("Missing ')' in rotation"               ));

			Token.SetConstRotation(R);
			return 1;
		}
		if( Token.TokenName==NAME_True && !NoConsts )
		{
			Token.SetConstBool(1);
			return 1;
		}
		if( Token.TokenName==NAME_False && !NoConsts )
		{
			Token.SetConstBool(0);
			return 1;
		}
		if( Token.TokenName==NAME_ArrayCount && !NoConsts )
		{
			FToken TypeToken;
			RequireSizeOfParm( TypeToken, TEXT("'ArrayCount'") );
			if( TypeToken.ArrayDim<=1 )
				appThrowf( TEXT("ArrayCount argument is not an array") );
			Token.SetConstInt( TypeToken.ArrayDim );
			return 1;
		}
		if( Token.Matches(TEXT("None")) && !NoConsts )
		{
			Token.SetConstObject( NULL );
			return 1;
		}

		// See if this is an enum, which we can only evaluate with knowledge
		// about the specified type.
		if( Hint && Hint->Type==CPT_Byte && Hint->Enum && Token.TokenName!=NAME_None && !NoConsts )
		{
			// Find index into the enumeration.
			INT EnumIndex=INDEX_NONE;
			if( Hint->Enum->Names.FindItem( Token.TokenName, EnumIndex ) )
			{
				Token.SetConstByte(Hint->Enum,EnumIndex);
				return 1;
			}
		}

		// See if this is a general object constant.
		if( !NoConsts && PeekSymbol(TEXT("'")) )
		{
			UClass* Type = FindObject<UClass>( ANY_PACKAGE, Token.Identifier );
			if
			(	Type
			&&	!Type->IsChildOf( AActor::StaticClass() )
			&&	!NoConsts
			&&	MatchSymbol(TEXT("'")) )
			{
				// This is an object constant.
				FString Str			= TEXT("");
				// GetToken doesn't handle '-' as an alphanumeric character so we need to manually fudge to allow fully qualified names to contain '-'.
				UBOOL	TokenIsDash = false;
				CheckInScope( Type );
				do
				{
					FToken NameToken;
					if( !GetIdentifier(NameToken,1) )
						appThrowf( TEXT("Missing %s name"), Type->GetName() );
					if( Str!=TEXT("") && !TokenIsDash )
						Str += TEXT(".");
					Str += NameToken.Identifier;
					TokenIsDash = MatchSymbol(TEXT("-"));
					if( TokenIsDash )
						Str += TEXT("-");
				}
				while( MatchSymbol(TEXT(".")) || TokenIsDash );

				// Find object.
				UObject* Ob = UObject::StaticFindObject( Type, ANY_PACKAGE, *Str );
				if( Ob == NULL )
					Ob = UObject::StaticLoadObject( Type, NULL, *Str, NULL, LOAD_NoWarn | LOAD_FindIfFail, NULL );
				if( Ob == NULL )
					warnf( NAME_Warning, TEXT("Unresolved reference to %s '%s'"), Type->GetName(), *Str );
				if( !MatchSymbol(TEXT("'")) )
					appThrowf( TEXT("Missing single quote after %s name"), Type->GetName() );
				CheckInScope( Ob );

				// Got a constant object.
				Token.SetConstObject( Ob );
				return 1;
			}
		}
		return 1;
	}
	else if( (c>='0' && c<='9') || ((c=='+' || c=='-') && (p>='0' && p<='9')) && !NoConsts )
	{
		// Integer or floating point constant.
		int  IsFloat = 0;
		int  Length  = 0;
		int  IsHex   = 0;
		do
		{
			if( c=='.' ) IsFloat = 1;
			if( c=='X' ) IsHex   = 1;

			Token.Identifier[Length++] = c;
			if( Length >= NAME_SIZE )
				appThrowf( TEXT("Number length exceeds maximum of %i "), (INT)NAME_SIZE );
			c = appToUpper(GetChar());
		} while( (c>='0' && c<='9') || c=='.' || c=='X' || (c>='A' && c<='F') );

		Token.Identifier[Length]=0;
		UngetChar();

		if( IsFloat )
		{
			Token.SetConstFloat( appAtof(Token.Identifier) );
		}
		else if( IsHex )
		{
			TCHAR* End = Token.Identifier + appStrlen(Token.Identifier);
			Token.SetConstInt( appStrtoi(Token.Identifier,&End,0) );
		}
		else
		{
			Token.SetConstInt( appAtoi(Token.Identifier) );
		}
		return 1;
	}
	else if( c=='\'' && !NoConsts)
	{
		// Name constant.
		int Length=0;
		c = GetChar();
		while( (c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || (c=='_') || (c==' ') )
		{
			Token.Identifier[Length++] = c;
			if( Length > NAME_SIZE )
				appThrowf( TEXT("Name length exceeds maximum of %i"), (INT)NAME_SIZE );
			c = GetChar();
		}
		if( c != '\'' )
			appThrowf( TEXT("Illegal character in name") );
		Token.Identifier[Length]=0;

		// Make constant name.
		Token.SetConstName( FName(Token.Identifier) );
		return 1;
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
					break;
			}
			Temp[Length++] = c;
			if( Length >= MAX_STRING_CONST_SIZE )
				appThrowf( TEXT("String constant exceeds maximum of %i characters"), (INT)MAX_STRING_CONST_SIZE );
			c = GetChar(1);
		}
		if( c!='"' )
			appThrowf( TEXT("Unterminated string constant") );

		Temp[Length]=0;

		Token.SetConstString(Temp);
		return 1;
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
		Token.TokenName = FName( Token.Identifier, FNAME_Find );

		return 1;
	}
	return 0;
}

//
// Get a raw token until we reach end of line.
//
INT FScriptCompiler::GetRawToken( FToken& Token )
{
	// Get token after whitespace.
	TCHAR Temp[MAX_STRING_CONST_SIZE];
	INT Length=0;
	TCHAR c = GetLeadingChar();
	while( !IsEOL(c) )
	{
		if( (c=='/' && PeekChar()=='/') || (c=='/' && PeekChar()=='*') )
			break;
		Temp[Length++] = c;
		if( Length >= MAX_STRING_CONST_SIZE )
			appThrowf( TEXT("Identifier exceeds maximum of %i characters"), (INT)MAX_STRING_CONST_SIZE );
		c = GetChar(1);
	}
	UngetChar();

	// Get rid of trailing whitespace.
	while( Length>0 && (Temp[Length-1]==' ' || Temp[Length-1]==9 ) )
		Length--;
	Temp[Length]=0;

	Token.SetConstString(Temp);

	return Length>0;
}

//
// Get an identifier token, return 1 if gotten, 0 if not.
//
INT FScriptCompiler::GetIdentifier( FToken& Token, INT NoConsts )
{
	if( !GetToken( Token, NULL, NoConsts ) )
		return 0;

	if( Token.TokenType == TOKEN_Identifier )
		return 1;

	UngetToken(Token);
	return 0;
}

//
// Get a symbol token, return 1 if gotten, 0 if not.
//
INT FScriptCompiler::GetSymbol( FToken& Token )
{
	if( !GetToken(Token) )
		return 0;

	if( Token.TokenType == TOKEN_Symbol )
		return 1;

	UngetToken(Token);
	return 0;
}

//
// Get an integer constant, return 1 if gotten, 0 if not.
//
INT FScriptCompiler::GetConstInt( INT& Result, const TCHAR* Tag, UStruct* Scope )
{
	FToken Token;
	if( GetToken(Token) )
	{
		if( Token.GetConstInt(Result) ) return 1;
		else if( Scope != 0 )
		{
			UField* Field=FindField( Scope, Token.Identifier, UConst::StaticClass() );
			if( Field != NULL )
			{
				UConst* Const = CastChecked<UConst>( Field );
				FToken ConstToken;
				FRetryPoint Retry; InitRetry(Retry);

				// Kludge: use GetToken to parse the const's value
				Input = *Const->Value;
				InputPos = 0;
				if( !GetToken( ConstToken ) || ConstToken.TokenType!=TOKEN_Const )
					appThrowf( TEXT("Error in constant") );
				PerformRetry(Retry,0,1);

				if( ConstToken.GetConstInt(Result) ) return 1;
				else UngetToken(Token);
			}
			else UngetToken(Token);
		}
		else                            UngetToken(Token);
	}

	if( Tag ) appThrowf( TEXT("%s: Missing constant integer") );
	return 0;

}

//
// Get a real number, return 1 if gotten, 0 if not.
//
INT FScriptCompiler::GetConstFloat( FLOAT& Result, const TCHAR* Tag )
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
		appThrowf( TEXT("%s: Missing constant integer") );

	return 0;
}

//
// Get a specific identifier and return 1 if gotten, 0 if not.
// This is used primarily for checking for required symbols during compilation.
//
INT	FScriptCompiler::MatchIdentifier( FName Match )
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
INT	FScriptCompiler::MatchSymbol( const TCHAR* Match )
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
INT FScriptCompiler::PeekSymbol( const TCHAR* Match )
{
	FToken Token;
	if( !GetToken(Token,NULL,1) )
		return 0;
	UngetToken(Token);

	return Token.TokenType==TOKEN_Symbol && appStricmp(Token.Identifier,Match)==0;
}

//
// Peek ahead and see if an identifier follows in the stream.
//
INT FScriptCompiler::PeekIdentifier( FName Match )
{
	FToken Token;
	if( !GetToken(Token,NULL,1) )
		return 0;
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
		appThrowf( TEXT("Missing '%s' in %s"), Match, Tag );
}

//
// Require an identifier.
//
void FScriptCompiler::RequireIdentifier( FName Match, const TCHAR* Tag )
{
	if( !MatchIdentifier(Match) )
		appThrowf( TEXT("Missing '%s' in %s"), *Match, Tag );
}

//
// Require a SizeOf-style parenthesis-enclosed type.
//
void FScriptCompiler::RequireSizeOfParm( FToken& TypeToken, const TCHAR* Tag )
{
	// Setup a retry point.
	FRetryPoint Retry;
	InitRetry( Retry );

	// Get leading paren.
	RequireSymbol( TEXT("("), Tag );

	// Get an expression.
	if( !CompileExpr( FPropertyBase(CPT_None), Tag, &TypeToken ) )
		appThrowf( TEXT("Bad or missing expression in %s"), Tag );

	// Get trailing paren.
	RequireSymbol( TEXT(")"), Tag );

	// Return binary code pointer (not script text) to where it was.
	PerformRetry( Retry, 1, 0 );

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
			break;
		appStrncat( ClassName, Token.Identifier, ARRAY_COUNT(ClassName) );
		if( !MatchSymbol(TEXT(".")) )
			break;
		appStrncat( ClassName, TEXT("."), ARRAY_COUNT(ClassName) );
	}
	if( ClassName[0] )
	{
		Result = FindObject<UClass>( ANY_PACKAGE, ClassName );
		if( !Result )
			appThrowf( TEXT("Class '%s' not found"), ClassName );
	}
	else if( Thing )
	{
		appThrowf( TEXT("%s: Missing class name"), Thing );
	}
	return Result;
}

/*-----------------------------------------------------------------------------
	Fields.
-----------------------------------------------------------------------------*/

UField* FScriptCompiler::FindField
(
	UStruct*		Scope,
	const TCHAR*	InIdentifier,
	UClass*			FieldClass,
	const TCHAR*	Thing
)
{
	check(InIdentifier);
	FName InName( InIdentifier, FNAME_Find );
	if( InName!=NAME_None )
	{
		for( Scope; Scope; Scope=Cast<UStruct>( Scope->GetOuter()) )
		{
			for( TFieldIterator<UField> It(Scope); It; ++It )
			{
				if( It->GetFName()==InName )
				{
					if( !It->IsA(FieldClass) )
					{
						if( Thing )
							appThrowf( TEXT("%s: expecting %s, got %s"), Thing, FieldClass->GetName(), It->GetClass()->GetName() );
						return NULL;
					}
					return *It;
				}
			}
		}
	}
	return NULL;
}

// Check if a field obsscures a field in an outer scope.
void FScriptCompiler::CheckObscures( UStruct* Scope, FToken& Token )
{
	UStruct* BaseScope = Scope->GetInheritanceSuper();
	while( BaseScope )
	{
		UField* Existing = FindField( BaseScope, Token.Identifier );
		if( Existing && Existing->GetOuter()==BaseScope )
			Warn->Logf( NAME_Warning, TEXT("'%s' obscures '%s' defined in base class."), *Token.TokenName, *Token.TokenName );
		BaseScope = BaseScope->GetInheritanceSuper();
	}
}

/*-----------------------------------------------------------------------------
	Variables.
-----------------------------------------------------------------------------*/

//
// Compile an enumeration definition.
//
UEnum* FScriptCompiler::CompileEnum( UStruct* Scope )
{
	check(Scope);

	// Make sure enums can be declared here.
	if( TopNest->NestType!=NEST_Class )
		appThrowf( TEXT("Enums can only be declared in class or struct scope") );

	// Get enumeration name.
	FToken EnumToken;
	if( !GetIdentifier(EnumToken) )
		appThrowf( TEXT("Missing enumeration name") );

	// Verify that the enumeration definition is unique within this scope.
	UField* Existing = FindField( Scope, EnumToken.Identifier );
	if( Existing && Existing->GetOuter()==Scope )
		appThrowf( TEXT("enum: '%s' already defined here"), *EnumToken.TokenName );

	CheckObscures( Scope, EnumToken );

	// Get opening brace.
	RequireSymbol( TEXT("{"), TEXT("'Enum'") );

	// Create enum definition.
	UEnum* Enum = new( Scope, EnumToken.Identifier, RF_Public )UEnum( NULL );
	Enum->Next = Scope->Children;
	Scope->Children = Enum;

	// Parse all enums tags.
	FToken TagToken;
	while( GetIdentifier(TagToken) )
	{
		INT iFound;
		FName NewTag(TagToken.Identifier);
		if( Enum->Names.FindItem(NewTag,iFound) )
			appThrowf( TEXT("Duplicate enumeration tag %s"), TagToken.Identifier );
		Enum->Names.AddItem( NewTag );
		if( Enum->Names.Num() > 255 )
			appThrowf( TEXT("Exceeded maximum of 255 enumerators") );
		if( !MatchSymbol(TEXT(",")) )
			break;
	}
	if( !Enum->Names.Num() )
		appThrowf( TEXT("Enumeration must contain at least one enumerator") );

	// Trailing brace.
	RequireSymbol( TEXT("}"), TEXT("'Enum'") );

	return Enum;
}

//
// Compile a struct definition.
//
UStruct* FScriptCompiler::CompileStruct( UStruct* Scope )
{
	check(Scope);

	// Make sure enums can be declared here.
	if( TopNest->NestType!=NEST_Class )
		appThrowf( TEXT("Enums can only be declared in class or struct scope") );

	// Get struct name.
	FToken StructToken;

	UBOOL IsNative = 0, IsExport = 0;
	
	for(;;)
	{
		if( !GetIdentifier(StructToken) )
			appThrowf( TEXT("Missing struct name") );
	    if( StructToken.TokenName == NAME_Native )
		    IsNative = 1;
	    else
		if( StructToken.TokenName == NAME_Export )
			IsExport = 1;
	    else
	        break;
	}

	// Verify uniqueness.
	UField* Existing = FindField( Scope, StructToken.Identifier );
	if( Existing && Existing->GetOuter()==Scope )
		appThrowf( TEXT("struct: '%s' already defined here"), *StructToken.TokenName );

	CheckObscures( Scope, StructToken );

	// Get optional superstruct.
	UStruct* BaseStruct = NULL;
	if( MatchIdentifier(NAME_Extends) )
	{
		FToken ParentScope, ParentName;
		if( !GetIdentifier( ParentScope ) )
			appThrowf( TEXT("'struct': Missing parent struct after 'Extends'") );
		if( !MatchSymbol(TEXT(".")) )
		{
		    UField* Field = FindField( Scope, ParentScope.Identifier, UStruct::StaticClass(), TEXT("'extends'") );
            if( Field && !Field->IsA( UStruct::StaticClass() ) )
		        appThrowf( TEXT("'struct': Can't find parent struct class '%s'"), ParentScope.Identifier );
            BaseStruct = CastChecked<UStruct>( Field );
		}
		else
		{
			if( !GetIdentifier( ParentName ) )
				appThrowf( TEXT("'struct': Missing parent struct type after '%s.'"), ParentScope.Identifier );
            UClass* TempClass = FindObject<UClass>( ANY_PACKAGE, ParentScope.Identifier );
            if( !TempClass )
				appThrowf( TEXT("'struct': Can't find parent struct class '%s'"), ParentScope.Identifier );
            UField* Field = FindField( TempClass, ParentName.Identifier, UStruct::StaticClass(), TEXT("'extends'") );
            if( Field && !Field->IsA( UStruct::StaticClass() ) )
		        appThrowf( TEXT("'struct': Can't find parent struct class '%s.%s'"), ParentScope.Identifier, ParentName.Identifier );
            BaseStruct = CastChecked<UStruct>( Field );
		}
	}

	// Create.
	UStruct* Struct = new( Scope, StructToken.Identifier, RF_Public )UStruct(BaseStruct);
	Struct->Next = Scope->Children;
	Scope->Children = Struct;

	if (IsNative)
		Struct->StructFlags |= STRUCT_Native;
	if (IsExport)
		Struct->StructFlags |= STRUCT_Export;

	// Get opening brace.
	RequireSymbol( TEXT("{"), TEXT("'struct'") );

	// Parse all struct variables.
	INT NumElements=0;
	FToken Token;
	do
	{
		GetToken( Token );
		if( Token.Matches(NAME_Struct) )
		{
			CompileStruct( Struct );		
			RequireSymbol( TEXT(";"), TEXT("'struct'") );
		}
		else if
		(	Token.Matches(NAME_Const) )
		{
			CompileConst( Struct );
			RequireSymbol( TEXT(";"), TEXT("'const'") );
		}
		else if
		(	Token.Matches( NAME_Var ) )
		{
			// Get editability.
			QWORD EdFlags = 0;
			if( MatchSymbol(TEXT("(")) )
			{
				EdFlags |= CPF_Edit;
				RequireSymbol( TEXT(")"), TEXT("Editable 'struct' member variable") );
			}

			// Get variable type.
			DWORD ObjectFlags=0;
			FPropertyBase OriginalProperty(CPT_None);
			GetVarType( Struct, OriginalProperty, ObjectFlags, CPF_ParmFlags, TEXT("'struct' member variable") );
			OriginalProperty.PropertyFlags |= EdFlags;
			FName Category = (EdFlags & CPF_Edit) ? FName( StructToken.Identifier, FNAME_Find ) : NAME_None;

			// Validate.
			if( OriginalProperty.PropertyFlags & CPF_ParmFlags )
				appThrowf( TEXT("Illegal type modifiers in variable") );

			// Process all variables of this type.
			do
			{
				FPropertyBase	Property = OriginalProperty;
				UProperty*		NewProperty = GetVarNameAndDim( Struct, Property, ObjectFlags, 0, 0, NULL, TEXT("Variable declaration"), Category, 0 );
				if(NewProperty->PropertyFlags & CPF_Component)
					Struct->StructFlags |= STRUCT_HasComponents;
			} while( MatchSymbol(TEXT(",")) );

			// Expect a semicolon.
			RequireSymbol( TEXT(";"), TEXT("'struct'") );
			NumElements++;

			// Propagate alignment restrictions.
			if( (OriginalProperty.Type == CPT_Struct) && OriginalProperty.Struct && (OriginalProperty.ArrayDim != 0) )
				Struct->MinAlignment = Max( Struct->MinAlignment, OriginalProperty.Struct->MinAlignment );
		}
		else if
		( Token.Matches( TEXT("structdefaults") ) )
		{
			// Get default properties text.
			FString				StrLine;
			const TCHAR*		Str;
			const TCHAR*		Buffer = &Input[Token.StartPos];
			FStringOutputDevice DefaultStructPropText;

			while( ParseLine(&Buffer,StrLine,1) )
			{
				Str = *StrLine;
				ParseNext( &Str );
				if( *Str=='}' )
					break;
				DefaultStructPropText.Logf( TEXT("%s\r\n"), *StrLine );
			}
			Struct->DefaultStructPropText = DefaultStructPropText;

			do
			{
				GetToken( Token );
			}
			while( !Token.Matches( TEXT("}") ) );
			GetToken( Token );
		}
		else if
		( !Token.Matches( TEXT("}") ) )
		{
			appThrowf( TEXT("'struct': Expecting 'Var', got '%s'"), Token.Identifier );
		}
	} while( !Token.Matches( TEXT("}") ) );
	
	// Make sure matrices are 16 byte aligned.
	if( Struct->GetFName() == NAME_Matrix )
		Struct->MinAlignment = Max( Struct->MinAlignment, 16 );

	FArchive DummyAr;
	Struct->Link( DummyAr, 1 );
	return Struct;
}

//
// Compile a constant definition.
//
void FScriptCompiler::CompileConst( UStruct* Scope )
{
	check(Scope);

	// Get varible name.
	FToken ConstToken;
	if( !GetIdentifier(ConstToken) )
		appThrowf( TEXT("Missing constant name") );

	// Verify uniqueness.
	FName ConstName = FName( ConstToken.Identifier );
	UField* Existing = FindField( Scope, ConstToken.Identifier );
	if( Existing && Existing->GetOuter()==Scope )
		appThrowf( TEXT("const: '%s' already defined"), *ConstToken.Identifier );

	CheckObscures( Scope, ConstToken );

	// Get equals and value.
	RequireSymbol( TEXT("="), TEXT("'const'") );
	const TCHAR* Start = Input+InputPos;
	FToken ValueToken;
	if( !GetToken(ValueToken) )
		appThrowf( TEXT("const %s: Missing value"), *ConstName );
#if 0 // language enhancement from Paul Dubois - infinite machines
	if( ValueToken.TokenType != TOKEN_Const )
		appThrowf( TEXT("const %s: Value is not constant"), *ConstName );
#endif
	// Format constant.
	TCHAR Value[1024];
	if( ValueToken.TokenType != TOKEN_Const )
	{
		if (ValueToken.Matches(TEXT("sizeof")))
		{
			FToken ClassToken;
			UClass* Class;

			RequireSymbol( TEXT("("), TEXT("'sizeof'") );
			if( !GetIdentifier(ClassToken) )
				appThrowf( TEXT("Missing class name") );
			Class = FindObject<UClass>( ANY_PACKAGE, ClassToken.Identifier );
			if( !Class )
				appThrowf( TEXT("Bad class name '%s'"), ClassToken.Identifier );
			RequireSymbol( TEXT(")"), TEXT("'sizeof'") );
			
			// Format constant
			appSprintf( Value, TEXT("%d"), Class->GetPropertiesSize() );
			warnf(TEXT("Assigning %s to %s"), Value, ConstToken.Identifier);
		}
		else
		{
			appThrowf( TEXT("const %s: Value is not constant"), *ConstName );
		}
	}
	else
	{
		// Format constant.
		appStrncpy( Value, Start, Min(1024,Input+InputPos-Start+1) );
	}

	// Create constant.
	UConst* NewConst = new(Scope,ConstName)UConst(NULL,Value);
	NewConst->Next = Scope->Children;
	Scope->Children = NewConst;

}

/*-----------------------------------------------------------------------------
	Retry management.
-----------------------------------------------------------------------------*/

//
// Remember the current compilation points, both in the source being
// compiled and the object code being emitted.  Required because
// UnrealScript grammar isn't quite LALR-1.
//
void FScriptCompiler::InitRetry( FRetryPoint& Retry )
{
	Retry.Input     = Input;
	Retry.InputPos	= InputPos;
	Retry.InputLine	= InputLine;
	Retry.CodeTop	= TopNode->Script.Num();

}

//
// Return to a previously-saved retry point.
//
void FScriptCompiler::PerformRetry( FRetryPoint& Retry, UBOOL Binary, UBOOL Text )
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

//
// Insert the code in the interval from Retry2-End into the code stream
// beginning at Retry1.
//
void FScriptCompiler::CodeSwitcheroo( FRetryPoint& LowRetry, FRetryPoint& HighRetry )
{
	FMemMark Mark(GMem);
	INT HighSize = TopNode->Script.Num() - HighRetry.CodeTop;
	INT LowSize  = HighRetry.CodeTop   - LowRetry.CodeTop;

	BYTE* Temp = new(GMem,HighSize)BYTE;
	appMemcpy ( Temp,                                          &TopNode->Script(HighRetry.CodeTop),HighSize);
	appMemmove( &TopNode->Script(LowRetry.CodeTop + HighSize), &TopNode->Script(LowRetry.CodeTop), LowSize );
	appMemcpy ( &TopNode->Script(LowRetry.CodeTop           ), Temp,                             HighSize);

	Mark.Pop();
}

/*-----------------------------------------------------------------------------
	Functions.
-----------------------------------------------------------------------------*/

//
// Try to compile a complete function call or field expression with a field name
// matching Token.  Returns 1 if a function call was successfully parsed, or 0 if no matching function
// was found.  Handles the error condition where the function was called but the
// specified parameters didn't match, or there was an error in a parameter
// expression.
//
// The function to call must be accessible within the current scope.
//
// This also handles unary operators identically to functions, but not binary
// operators.
//
// Sets ResultType to the function's return type.
//
UBOOL FScriptCompiler::CompileFieldExpr
(
	UStruct*		Scope,
	FPropertyBase&	RequiredType,
	FToken			Token,
	FToken&			ResultType,
	UBOOL			IsSelf,
	UBOOL			IsConcrete
)
{
	FRetryPoint Retry; InitRetry(Retry);
	UBOOL    ForceFinal = 0;
	UBOOL    Global     = 0;
	UBOOL    Super      = 0;
	UBOOL    Default    = 0;
	UBOOL    Static     = 0;
	UClass*  FieldClass = NULL;

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
		debugf(TEXT("looking for const %s"),Token.Identifier);
	}
	else if( Token.Matches(NAME_Global) )
	{
		// Call the highest global, final (non-state) version of a function.
		if( !IsSelf )
			appThrowf( TEXT("Can only use 'global' with self") );
		if( !IsConcrete )
			appThrowf( TEXT("Can only use 'global' with concrete objects") );
		Scope      = Scope->GetOwnerClass();
		FieldClass = UFunction::StaticClass();
		Global     = 1;
		IsSelf     = 0;
		RequireSymbol( TEXT("."), TEXT("'global'") );
		GetToken(Token);
	}
	else if( Token.Matches(NAME_Super) && !PeekSymbol(TEXT("(")) )
	{
		// Call the superclass version of the function.
		if( !IsSelf )
			appThrowf( TEXT("Can only use 'super' with self") );

		// find the containing state/class for this function
		while( Cast<UFunction>(Scope) != NULL )
		{
			Scope = CastChecked<UStruct>( Scope->GetOuter() );
		}
		if( !Scope )
			appThrowf( TEXT("Can't use 'super': no superclass") );
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
		//ForceFinal = 1;
		IsSelf     = 0;
		Super      = 1;
		RequireSymbol( TEXT("."), TEXT("'super'") );
		GetToken(Token);
	}
	else if( Token.Matches(NAME_Super) )
	{
		// Call the final version of a function residing at or below a certain class.
		if( !IsSelf )
			appThrowf( TEXT("Can only use 'super(classname)' with self") );
		RequireSymbol( TEXT("("), TEXT("'super'") );
		FToken ClassToken;
		if( !GetIdentifier(ClassToken) )
			appThrowf( TEXT("Missing class name") );
		Scope = FindObject<UClass>( ANY_PACKAGE, ClassToken.Identifier );//!!should scope the class search
		if( !Scope )
			appThrowf( TEXT("Bad class name '%s'"), ClassToken.Identifier );
		if( !Scope->IsChildOf( Scope->GetOwnerClass() ) )
			appThrowf( TEXT("'Super(classname)': class '%s' does not expand '%s'"), ClassToken.Identifier, Scope->GetName(), Scope->GetName() );
		RequireSymbol( TEXT(")"), TEXT("'super(classname)'") );
		RequireSymbol( TEXT("."), TEXT("'super(classname)'") );
		FieldClass = UFunction::StaticClass();
		ForceFinal = 1;
		Super = 1;
		IsSelf     = 0;
		GetToken(Token);
	}

	// Handle field type.
	UField* Field = FindField( Scope, Token.Identifier, FieldClass );
	// If we can't find the field, check our outer class.
	UBOOL bUseOuter=false;
	if( !Field )
	{
		UClass* OuterClass = Scope->GetOwnerClass()->ClassWithin;
		if( OuterClass && OuterClass!=UObject::StaticClass() )
		{
			Field = FindField( OuterClass, Token.Identifier, FieldClass );
			bUseOuter = Field!=NULL;
		}
	}

    UnReferencedLocal *prevLocal=NULL, *curLocal=unReferencedLocals;
    while( curLocal )
    {
        if( curLocal->property == Field )
        {
            if( prevLocal == NULL )
                unReferencedLocals = curLocal->next;
            else
                prevLocal->next = curLocal->next;

            delete curLocal;
            break;
        }
        else
        {
            prevLocal = curLocal;
            curLocal = curLocal->next;
        }
    }

	UDelegateProperty *delegateProp = NULL;
	if (Field && Cast<UDelegateProperty>(Field) && PeekSymbol(TEXT("(")))
	{
		// delegate call
		debugf(TEXT("Attempting to call delegate from property %s"),Field->GetName());
		// save this delegate property for later reference
		delegateProp = ((UDelegateProperty*)Field);
		// and replace the field with the actual function for compilation down below
		Field = delegateProp->Function;
	}
	if( Field && Field->GetClass()==UFunction::StaticClass() && (((UFunction*)Field)->FunctionFlags&FUNC_Delegate) && !PeekSymbol(TEXT("(")) )
	{
		// Convert a Delegate function to its corresponding UDelegateProperty if this isn't a function call.
		Field = FindField( CastChecked<UStruct>(Field->GetOuter()), *FString::Printf(TEXT("__%s__Delegate"), Field->GetName()) );
	}
	if( Field && Field->GetClass()==UEnum::StaticClass() && MatchSymbol(TEXT(".")) )
	{
		// Enum constant.
		UEnum* Enum = CastChecked<UEnum>( Field );
		INT EnumIndex = INDEX_NONE;
		if( !GetToken(Token) )
			appThrowf( TEXT("Missing enum tag after '%s'"), Enum->GetName() );
		else if( Token.TokenName==NAME_EnumCount )
			EnumIndex = Enum->Names.Num();
		else if( Token.TokenName==NAME_None || !Enum->Names.FindItem( Token.TokenName, EnumIndex ) )
			appThrowf( TEXT("Missing enum tag after '%s'"), Enum->GetName() );
		ResultType.SetConstByte( Enum, EnumIndex );
		ResultType.PropertyFlags &= ~CPF_OutParm;
		EmitConstant( ResultType );
		return 1;
	}
	else if( Field && Cast<UProperty>(Field) )
	{
		// Check validity.
		UProperty* Property = CastChecked<UProperty>( Field );
		if( !(Property->GetFlags() & RF_Public) && ((Property->GetFlags() & RF_Final) ? Property->GetOwnerClass()!=Class : !Class->IsChildOf( Property->GetOwnerClass() )) )
			appThrowf( TEXT("Can't access private variable '%s' in '%s'"), Property->GetName(), Property->GetOwnerClass()->GetName() );
		if( Default && Property->GetOuter()->GetClass()!=UClass::StaticClass() )
			appThrowf( TEXT("You can't access the default value of static and local variables") );
		if( !IsConcrete && !Default && Property->GetOuter()->GetClass()!=UFunction::StaticClass() )
			appThrowf( TEXT("You can only access default values of variables here") );
		if( (Property->PropertyFlags & CPF_Deprecated) )
			Warn->Logf(NAME_ExecWarning, TEXT("Reference to deprecated property '%s'"), Property->GetName());

		INT OuterSizeFixup=0;
		if( bUseOuter )
			OuterSizeFixup=EmitOuterContext();

		// Process the variable we found.
		if( Cast<UBoolProperty>(Property) )
			Writer << EX_BoolVariable;
		EExprToken ExprToken = Default ? EX_DefaultVariable : Property->GetOuter()->GetClass()==UFunction::StaticClass() ? EX_LocalVariable : EX_InstanceVariable;
		Writer << ExprToken;
		Writer << Property;

		// Return the type.
		ResultType = FPropertyBase( Property );
		ResultType.PropertyFlags |= CPF_OutParm;

		// Specialize 'Object.Class' metaclass, and 'Object.Outer' class.
		if( Property->GetFName()==NAME_Class && Property->GetOuter()==UObject::StaticClass() )
			ResultType.MetaClass = Scope->GetOwnerClass();
		if( Property->GetFName()==NAME_Outer && Property->GetOuter()==UObject::StaticClass() )
			ResultType.PropertyClass = Scope->GetOwnerClass()->ClassWithin;

		if( OuterSizeFixup )
			TopNode->Script(OuterSizeFixup) = (BYTE)ResultType.GetSize();
		return 1;
	}
	else if( Field && Field->GetClass()==UFunction::StaticClass() && RequiredType.Type == CPT_Delegate )
	{
		// An expression requiring a delegate.
		UFunction* Function = ((UFunction*) Field );

		//!!DELEGATES - check if the function is callable from here (eg protected)

		// check return type and params
		if
		(	RequiredType.Function->NumParms!=Function->NumParms
		||	(!RequiredType.Function->GetReturnProperty())!=(!Function->GetReturnProperty()) )
			appThrowf( TEXT("'%s' mismatches delegate '%s'"), *Function->FriendlyName, *RequiredType.Function->FriendlyName );

		// Check all individual parameters.
		INT Count=0;
		for( TFieldIterator<UProperty,CLASS_IsAUProperty> It1(RequiredType.Function),It2(Function); Count<Function->NumParms; ++It1,++It2,++Count )
		{
			if( !FPropertyBase(*It1).MatchesType(FPropertyBase(*It2), 1) )
			{
				appThrowf( TEXT("'%s' mismatches delegate '%s'"), *Function->FriendlyName, *RequiredType.Function->FriendlyName );
				break;
			}
		}
		
		// get to the context
		INT OuterSizeFixup=0;
		if( bUseOuter )
			OuterSizeFixup=EmitOuterContext();

		Writer << EX_DelegateProperty;
		Writer << Function->FriendlyName;

		if( OuterSizeFixup )
			TopNode->Script(OuterSizeFixup) = (BYTE)ResultType.GetSize();

		ResultType = FToken(FPropertyBase(CPT_Delegate));	
		return 1;		
	}
	else if( Field && Field->GetClass()==UFunction::StaticClass() && MatchSymbol(TEXT("(")) )
	{
		// Function.
		UFunction* Function = ((UFunction*) Field );
		GotAffector = 1;

		// Verify that the function is callable here.
		// NOTE[aleiby]: Obviously, a better way would be to have an IsInScope( Scope, FunctionName ) helper function, or an AttemptFunctionCall( Scope, FunctionName ) that automatically spits out the appropriate error messages.
		if( (Function->FunctionFlags & FUNC_Private) && Function->GetOwnerClass()!=Class )
			appThrowf( TEXT("Can't access private function '%s' in '%s'"), Function->GetName(), Function->GetOwnerClass()->GetName() );
		if(	Function->FunctionFlags & FUNC_Protected )
		{
			//Warn->Logf( TEXT("%s calling %s::%s in scope %s"), Class->GetName(), Function->GetOwnerClass()->GetName(), Function->GetName(), Scope->GetName() );
			if( !Class->IsChildOf( Function->GetOwnerClass() ) )														// if we are not a subclass of the function's class.
			{
				if( Function->GetOwnerClass()->IsChildOf( Class ) )														// if we are a superclass of the function's class.
				{
					UFunction* ScopedFunction = Cast<UFunction>( FindField( Class, Function->GetName(), UFunction::StaticClass(), TEXT("'function'") ) );	// Fix ARL: Use correct scope to account for functions in states.
					if( ScopedFunction==NULL )																			// if the function is not in the superclass's scope.
					{
						appThrowf( TEXT("Can't access protected function '%s' in '%s' (not in scope)"), Function->GetName(), Function->GetOwnerClass()->GetName() );
					}
					else
					{
						//Warn->Logf( TEXT("    %s::%s is %s"), ScopedFunction->GetOwnerClass()->GetName(), ScopedFunction->GetName(), (ScopedFunction->FunctionFlags & FUNC_Private) ? TEXT("PRIVATE") : (ScopedFunction->FunctionFlags & FUNC_Public) ? TEXT("PUBLIC") : (ScopedFunction->FunctionFlags & FUNC_Protected) ? TEXT("PROTECTED") : TEXT("INVALID") );
						if( (ScopedFunction->FunctionFlags & FUNC_Private) && ScopedFunction->GetOwnerClass()!=Class )	// if a function with the same name is in scope but is private.
						{
							appThrowf( TEXT("Can't access private function '%s' in '%s' (%s::%s is not in scope)"), ScopedFunction->GetName(), ScopedFunction->GetOwnerClass()->GetName(), Function->GetOwnerClass()->GetName(), Function->GetName() );
						}
					}
				}
				else
				{
					appThrowf( TEXT("Can't access protected function '%s' in '%s'"), Function->GetName(), Function->GetOwnerClass()->GetName() );
				}
			}
		}

		
		if( Function->FunctionFlags & FUNC_Latent )
			CheckAllow( Function->GetName(), ALLOW_StateCmd );
		if( !(Function->FunctionFlags & FUNC_Static) && !IsConcrete )
			appThrowf( TEXT("Can't call instance functions from within static functions") );
		if( Static && !(Function->FunctionFlags & FUNC_Static) )
			appThrowf( TEXT("Function '%s' is not static"), Function->GetName() );
		if( (Function->FunctionFlags & FUNC_Iterator) )
		{
			CheckAllow( Function->GetName(), ALLOW_Iterator );
			TopNest->Allow &= ~ALLOW_Iterator;
		}

		INT OuterSizeFixup=0;
		if( bUseOuter )
			OuterSizeFixup=EmitOuterContext();

		// Emit the function call.
		EmitStackNodeLinkFunction( Function, ForceFinal, Global, Super, delegateProp );

		// See if this is an iterator with automatic casting of parm 2 object to the parm 1 class.
		UBOOL IsIteratorCast = 0;
		if
		(	(Function->FunctionFlags & FUNC_Iterator)
		&&	(Function->NumParms>=2) )
		{
			TFieldIterator<UProperty,CLASS_IsAUProperty> It(Function);
			UObjectProperty* A=Cast<UObjectProperty>(*It); ++It;
			UObjectProperty* B=Cast<UObjectProperty>(*It);
			if( A && B && A->PropertyClass==UClass::StaticClass() )
				IsIteratorCast=1;
		}
		UClass* IteratorClass = NULL;

		// Parse the parameters.
		FToken ParmToken[MAX_FUNC_PARMS];
		INT Count=0;
		TFieldIterator<UProperty,CLASS_IsAUProperty> It(Function);
		for( It; It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It,++Count )
		{
			// Get parameter.
			FPropertyBase Parm = FPropertyBase( *It );
			if( Parm.PropertyFlags & CPF_ReturnParm )
				break;

			// If this is an iterator, automatically adjust the second parameter's type.
			if( Count==1 && IteratorClass )
				Parm.PropertyClass = IteratorClass;

			// Get comma parameter delimiter.
			if( Count!=0 && !MatchSymbol(TEXT(",")) )
			{
				// Failed to get a comma.
				if( !(Parm.PropertyFlags & CPF_OptionalParm) )
					appThrowf( TEXT("Call to '%s': missing or bad parameter %i"), Function->GetName(), Count+1 );

				// Ok, it was optional.
				break;
			}

			INT Result = CompileExpr( Parm, NULL, &ParmToken[Count] );
			if( Result == -1 )
			{
				// Type mismatch.
				appThrowf( TEXT("Call to '%s': type mismatch in parameter %i"), Token.Identifier, Count+1 );
			}
			else if( Result == 0 )
			{
				// Failed to get an expression.
				if( !(Parm.PropertyFlags & CPF_OptionalParm) )
					appThrowf( TEXT("Call to '%s': bad or missing parameter %i"), Token.Identifier, Count+1 );
				if( PeekSymbol(TEXT(")")) )
					break;
				else
					Writer << EX_Nothing;
			}
			else if( IsIteratorCast && Count==0 )
				ParmToken[Count].GetConstObject( UClass::StaticClass(), *(UObject**)&IteratorClass );
		}
		for( It; It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
			check(It->PropertyFlags & CPF_OptionalParm);

		// Get closing paren.
		FToken Temp;
		if( !GetToken(Temp) || Temp.TokenType!=TOKEN_Symbol )
			appThrowf( TEXT("Call to '%s': Bad expression or missing ')'"), Token.Identifier );
		if( !Temp.Matches(TEXT(")")) )
			appThrowf( TEXT("Call to '%s': Bad '%s' or missing ')'"), Token.Identifier, Temp.Identifier );

		// Emit end-of-function-parms tag.
		Writer << EX_EndFunctionParms;
		//!! Yoda Debugger
		if( Function->FunctionFlags & FUNC_Latent )
		{			
			EmitDebugInfo( DI_PrevStackLatent );
		}
		else
		{ 
			EmitDebugInfo(DI_EFP);
		}

		if( Function->FunctionFlags & FUNC_Latent )
		{
			// Emit EX_Nothing or LATENTNEWSTACK will be processed immediately, instead of when the function returns
			if ( bEmitDebugInfo )
			{
				Writer << EX_Nothing;
				EmitDebugInfo( DI_NewStackLatent );
			}
		}

		// Check return value.
		if( It && (It->PropertyFlags & CPF_ReturnParm) )
		{
			// Has a return value.
			ResultType = FPropertyBase( *It );
			ResultType.PropertyFlags &= ~CPF_OutParm;

			// Spawn special case: Make return type the same as a constant class passed to it.
			if( Token.Matches(NAME_Spawn) )
				ResultType.PropertyClass = ParmToken[0].MetaClass;
		}
		else
		{
			// No return value.
			ResultType = FToken(FPropertyBase(CPT_None));
		}

		// Returned value is an r-value.
		ResultType.PropertyFlags &= ~CPF_OutParm;

		if( OuterSizeFixup )
			TopNode->Script(OuterSizeFixup) = (BYTE)ResultType.GetSize();

		return 1;
	}
	else if( Field && Field->GetClass()==UConst::StaticClass() )
	{
		// Named constant.
		UConst* Const = CastChecked<UConst>( Field );
		// read the const value into a token
		FRetryPoint ConstRetry; InitRetry(ConstRetry);
		Input = *Const->Value;
		InputPos = 0;
		if( !GetToken( ResultType, &RequiredType ) || ResultType.TokenType!=TOKEN_Const )
			appThrowf( TEXT("Error in constant") );
		// reset the compile point to the previous node
		PerformRetry(ConstRetry,0,1);
		// and write out the constant token
		ResultType.AttemptToConvertConstant( RequiredType );
		ResultType.PropertyFlags &= ~CPF_OutParm;
		EmitConstant( ResultType );
		return 1;
	}
	else
	{
		// Nothing.
		if( FieldClass )
			appThrowf( TEXT("Unknown %s '%s' in '%s'"), FieldClass->GetName(), Token.Identifier, *Scope->GetFullName() );
		PerformRetry( Retry );
		return 0;
	}
}

/*-----------------------------------------------------------------------------
	Type conversion.
-----------------------------------------------------------------------------*/

//
// Return the cost of converting a type from Source to Dest:
//    0 if types are identical.
//    1 if dest is more precise than source.
//    2 if converting integral to float.
//    3 if dest is less precise than source, or a generalization of source.
//    MAXINT if the types are incompatible.
//
int FScriptCompiler::ConversionCost
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
		int Result = 1;
		if( Source.Type==CPT_ObjectReference && Source.PropertyClass!=NULL )
		{
			// The fewer classes traversed in this conversion, the better the quality.
			check(Dest.Type==CPT_ObjectReference);
			check(Dest.PropertyClass!=NULL);
			UClass* Test;
			for( Test=Source.PropertyClass; Test && Test!=Dest.PropertyClass; Test=Test->GetSuperClass() )
				Result++;
			check(Test!=NULL);
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
	else if( Dest.Type==CPT_ObjectReference && Dest.PropertyClass!=NULL )
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
		return 104;
	}
	else if( (Source.Type==CPT_Int || Source.Type==CPT_Byte) && Dest.Type==CPT_Float )
	{
		// Conversion to float.
		//AddResultText("ConvertToFloat\r\n");
		return 103;
	}
	else
	{
		// Expansion.
		//AddResultText("Expansion\r\n");
		return 101;
	}
}

//
// Compile a dynamic object upcast expression.
//
UBOOL FScriptCompiler::CompileDynamicCast( const FToken& Token, FToken& ResultType )
{
	FRetryPoint LowRetry; InitRetry(LowRetry);
	if( !MatchSymbol(TEXT("(")) && !PeekSymbol(TEXT("<")) )
		return 0;
	UClass* DestClass = FindObject<UClass>( ANY_PACKAGE, Token.Identifier );
	if( !DestClass )
	{
		UEnum* DestEnum = FindObject<UEnum>( ANY_PACKAGE, Token.Identifier );
		if (DestEnum)
		{
			// Get expression to cast, and ending paren.
			FToken TempType;
			FPropertyBase RequiredType( CPT_Byte );
			if
			(	CompileExpr(RequiredType, NULL, &TempType)!=1
			||	!MatchSymbol(TEXT(")")) )
			{
				// No ending paren, therefore it is probably a function call.
				PerformRetry( LowRetry );
				return 0;
			}

			ResultType = FToken( FPropertyBase( DestEnum ) );
			return 1;
		}
		PerformRetry( LowRetry );
		return 0;
	}
	UClass* MetaClass = UObject::StaticClass();
	if( DestClass==UClass::StaticClass() )
	{
		if( MatchSymbol(TEXT("<")) )
		{
			FToken Token;
			if( !GetIdentifier(Token) )
				return 0;
			MetaClass = FindObject<UClass>( ANY_PACKAGE, Token.Identifier );
			if( !MetaClass || !MatchSymbol(TEXT(">")) || !MatchSymbol(TEXT("(")) )
			{
				PerformRetry( LowRetry );
				return 0;
			}
		}
	}

	// Get expression to cast, and ending paren.
	FToken TempType;
	FPropertyBase RequiredType( UObject::StaticClass() );
	if
	(	CompileExpr(RequiredType, NULL, &TempType)!=1
	||	!MatchSymbol(TEXT(")")) )
	{
		// No ending paren, therefore it is probably a function call.
		PerformRetry( LowRetry );
		return 0;
	}
	CheckInScope( DestClass );

	// See what kind of conversion this is.
	if
	(	(!TempType.PropertyClass || TempType.PropertyClass->IsChildOf(DestClass))
	&&	(TempType.PropertyClass!=UClass::StaticClass() || DestClass!=UClass::StaticClass() || TempType.MetaClass->IsChildOf(MetaClass) ) )
	{
		// Redundent conversion.
		appThrowf
		(
			TEXT("Cast from '%s' to '%s' is unnecessary"),
			TempType.PropertyClass ? TempType.PropertyClass->GetName() : TEXT("None"),
			DestClass->GetName()
		);
	}
	else if
	(	(DestClass->IsChildOf(TempType.PropertyClass))
	&&	(DestClass!=UClass::StaticClass() || TempType.MetaClass==NULL || MetaClass->IsChildOf(TempType.MetaClass) ) )
	{
		// Dynamic cast, must be handled at runtime.
		FRetryPoint HighRetry; InitRetry(HighRetry);
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
		CodeSwitcheroo(LowRetry,HighRetry);
	}
	else
	{
		// The cast will always fail.
		appThrowf( TEXT("Cast from '%s' to '%s' will always fail"), TempType.PropertyClass->GetName(), DestClass->GetName() );
	}

	// A cast is no longer an l-value.
	ResultType = FToken( FPropertyBase( DestClass ) );
	if( ResultType.PropertyClass == UClass::StaticClass() )
		ResultType.MetaClass = MetaClass;
	return 1;
}

/*-----------------------------------------------------------------------------
	Expressions.
-----------------------------------------------------------------------------*/

//
// Compile a top-level expression.
//
// Returns:
//		 0	if no expression was parsed.
//		 1	if an expression matching RequiredType (or any type if CPT_None) was parsed.
//		-1	if there was a type mismatch.
//
UBOOL FScriptCompiler::CompileExpr
(
	FPropertyBase	RequiredType,
	const TCHAR*	ErrorTag,
	FToken*			ResultToken,
	INT				MaxPrecedence,
	FPropertyBase*	HintType
)
{
	FRetryPoint LowRetry; InitRetry(LowRetry);

	FPropertyBase P(CPT_None);
	FToken Token(P);
	if( !GetToken( Token, HintType ? HintType : &RequiredType ) )
	{
		// This is an empty expression.
		(FPropertyBase&)Token = FPropertyBase( CPT_None );
	}
	else if( Token.TokenName == NAME_None && RequiredType.Type == CPT_Delegate )
	{
		// Assigning None to delegate
		Writer << EX_DelegateProperty;
		Writer << Token.TokenName;
		Token.Type = CPT_Delegate;
	}
	else if( Token.TokenType == TOKEN_Const )
	{
		// This is some kind of constant.
		Token.AttemptToConvertConstant( RequiredType );
		Token.PropertyFlags &= ~CPF_OutParm;
		EmitConstant( Token );
	}
	else if( Token.Matches(TEXT("(")) )
	{
		// Parenthesis. Recursion will handle all error checking.
		if( !CompileExpr( RequiredType, NULL, &Token ) )
			appThrowf( TEXT("Bad or missing expression in parenthesis") );
		RequireSymbol( TEXT(")"), TEXT("expression") );
		if( Token.Type == CPT_None )
			appThrowf( TEXT("Bad or missing expression in parenthesis") );
	}
	else if
	((	Token.TokenName==NAME_Byte
	||	Token.TokenName==NAME_Int
	||	Token.TokenName==NAME_Bool
	||	Token.TokenName==NAME_Float
	||	Token.TokenName==NAME_Name
	||	Token.TokenName==NAME_String
	||	Token.TokenName==NAME_Button
	||	Token.TokenName==NAME_Struct
	||	Token.TokenName==NAME_Vector
	||	Token.TokenName==NAME_Rotator )
	&&	MatchSymbol(TEXT("(")) )
	{
		// An explicit type conversion, so get source type.
		FPropertyBase ToType(CPT_None);
		if( Token.TokenName==NAME_Vector )
		{
			ToType = FPropertyBase( FindObjectChecked<UStruct>( ANY_PACKAGE, TEXT("Vector") ));
		}
		else if( Token.TokenName==NAME_Rotator )
		{
			ToType = FPropertyBase( FindObjectChecked<UStruct>( ANY_PACKAGE, TEXT("Rotator") ));
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
			:	Token.TokenName==NAME_Button			? CPT_String
			:	Token.TokenName==NAME_Struct			? CPT_Struct
			:   CPT_None;
			check(T!=CPT_None);
			ToType = FPropertyBase( T );
		}
		//caveat: General struct constants aren't supported.!!

		// Get destination type.
		FToken FromType;
		CompileExpr( FPropertyBase(CPT_None), *Token.TokenName, &FromType );
		if( FromType.Type == CPT_None )
			appThrowf( TEXT("'%s' conversion: Bad or missing expression"), *Token.TokenName );

		// Can we perform this explicit conversion?
		DWORD Conversion = GetConversion( ToType, FromType );
		if( Conversion != CST_Max )
		{
			// Perform conversion.
			FRetryPoint HighRetry; InitRetry(HighRetry);
			Writer << EX_PrimitiveCast;
			Writer << (ECastToken)(Conversion & CONVERT_MASK);
			CodeSwitcheroo(LowRetry,HighRetry);
			Token = FToken(ToType);
		}
		else if( ToType.Type == FromType.Type )
			appThrowf( TEXT("No need to cast '%s' to itself"), *FName((EName)ToType.Type) );
		else
			appThrowf( TEXT("Can't convert '%s' to '%s'"), *FName((EName)FromType.Type), *FName((EName)ToType.Type) );

		// The cast is no longer an l-value.
		Token.PropertyFlags &= ~CPF_OutParm;
		if( !MatchSymbol(TEXT(")")) )
			appThrowf( TEXT("Missing ')' in type conversion") );
	}
	else if( Token.TokenName==NAME_Self )
	{
		// Special Self context expression.
		CheckAllow( TEXT("'self'"), ALLOW_Instance );		
		Writer << EX_Self;
		Token = FToken(FPropertyBase( Class ));
	}
	else if( Token.TokenName==NAME_New )
	{
		// Special Self context expression.
		Writer << EX_New;
		UBOOL Paren = MatchSymbol(TEXT("(")) && !MatchSymbol(TEXT(")")), Going = Paren;
		UClass* ParentClass = Class;

		// Parent expression.
		if( Going )
		{
			FToken NewParent;
			CompileExpr( FPropertyBase(UObject::StaticClass()), TEXT("'new' parent object"), &NewParent );
			if( !NewParent.PropertyClass )//!!??
				NewParent.PropertyClass = UObject::StaticClass();
			ParentClass = NewParent.PropertyClass;
			//!!oldver, delete this: if( !ParentClass )
			//	appThrowf( TEXT("'new': Invalid outer class") );
			Going = MatchSymbol(TEXT(","));
		}
		else Writer << EX_Nothing;

		// Name expression.
		if( Going )
		{
			CompileExpr( FPropertyBase(CPT_String), TEXT("'new' name") );
			Going = MatchSymbol(TEXT(","));
		}
		else Writer << EX_Nothing;

		// Flags expression.
		if( Going )
		{
			CompileExpr( FPropertyBase(CPT_Int), TEXT("'new' flags") );
		}
		else Writer << EX_Nothing;
		if( Paren )
			RequireSymbol( TEXT(")"), TEXT("'new'") );

		// New class name.
		FToken NewClass;
		CompileExpr( FPropertyBase(UClass::StaticClass(),UObject::StaticClass()), TEXT("'new'"), &NewClass );
		if( !NewClass.MetaClass )
			appThrowf( TEXT("'new': Invalid class") );
		check(NewClass.MetaClass->ClassWithin);
		Token = FToken(NewClass.MetaClass);

		// Validate parent.
		if( !ParentClass || !ParentClass->IsChildOf(NewClass.MetaClass->ClassWithin) )
			appThrowf( TEXT("'new': %s objects must reside in %s objects, not %s objects"), NewClass.MetaClass->GetName(), NewClass.MetaClass->ClassWithin->GetName(), ParentClass->GetName() );

		// Constructor parameters.
		if( MatchSymbol( TEXT("(")) )
			RequireSymbol( TEXT(")"), TEXT("'new' constructor parameters") );
	}
	else if( CompileDynamicCast( FToken(Token), Token) )
	{
		// Successfully compiled a dynamic object cast.
	}
	else if( CompileFieldExpr( TopNode, RequiredType, Token, Token, 1, (TopNest->Allow&ALLOW_Instance)!=0 ) )
	{
		// We successfully parsed a variable or function expression.
	}
	else
	{
		// This doesn't match an expression, so put it back.
		UngetToken( Token );
		(FPropertyBase&)Token = FPropertyBase( CPT_None );
	}

	// Intercept member selection operator for objects.
	for( ; ; )
	{
		//dynarray
		if( Token.ArrayDim==0 && MatchSymbol(TEXT(".")) )
		{
			if( MatchIdentifier(NAME_Length) )
			{
				FRetryPoint HighRetry; InitRetry(HighRetry);
				Writer << EX_DynArrayLength;
				CodeSwitcheroo( LowRetry, HighRetry );

				//for correct 'matchups' done in a later assignment, when it needs to know that length is an int
				Token   = FPropertyBase(CPT_Int);
				Token.ArrayDim = 1;
				Token.PropertyFlags |= CPF_OutParm;
			}
			//NOTE: Insert and Remove are done as special bytecodes of their own
			//   this is so that a dynamic array of something with the CPF_NeedCtorLink flag set
			//   will not incur major speed penalties on insert/remove
			else if( MatchIdentifier(NAME_Insert) )
			{
				FRetryPoint HighRetry; InitRetry(HighRetry);
				Writer << EX_DynArrayInsert;
				CodeSwitcheroo( LowRetry, HighRetry );

				RequireSymbol( TEXT("("), TEXT("'insert(...)'") );
				CompileExpr( FPropertyBase(CPT_Int), TEXT("'insert(...)'") );
				RequireSymbol( TEXT(","), TEXT("'insert(...)'") );
				CompileExpr( FPropertyBase(CPT_Int), TEXT("'insert(...)'") );
				RequireSymbol( TEXT(")"), TEXT("'insert(...)'") );
				Token   = FToken(FPropertyBase(CPT_None));
				GotAffector = 1;
				break;
			}
			else if( MatchIdentifier(NAME_Remove) )
			{
				FRetryPoint HighRetry; InitRetry(HighRetry);
				Writer << EX_DynArrayRemove;

				CodeSwitcheroo( LowRetry, HighRetry );
				RequireSymbol( TEXT("("), TEXT("'remove(...)'") );
				CompileExpr( FPropertyBase(CPT_Int), TEXT("'remove(...)'") );
				RequireSymbol( TEXT(","), TEXT("'remove(...)'") );
				CompileExpr( FPropertyBase(CPT_Int), TEXT("'remove(...)'") );
				RequireSymbol( TEXT(")"), TEXT("'remove(...)'") );
				Token   = FToken(FPropertyBase(CPT_None));
				GotAffector = 1;
				break;
			} else
				appThrowf( TEXT("Invalid property or function call on a dynamic array"));

		}
		else if( Token.ArrayDim!=1 )
		{
			// If no array handler, we're done.
			if( !MatchSymbol(TEXT("[")) )
				break;

			// Emit array token.
			FRetryPoint HighRetry; InitRetry(HighRetry);
			if( Token.ArrayDim>1 )
				Writer << EX_ArrayElement;
			else
				Writer << EX_DynArrayElement;

			// Emit index expression.
			Token.ArrayDim = 1;
			CompileExpr( FPropertyBase(CPT_Int), TEXT("array index") );
			if( !MatchSymbol(TEXT("]")) )
				appThrowf( TEXT("%s is an array; expecting ']'"), Token.Identifier );

			// Emit element size and array dimension.
			CodeSwitcheroo( LowRetry, HighRetry );
		}
		else if( Token.Type==CPT_Struct && MatchSymbol(TEXT(".")) )
		{
			// Get member.
			check(Token.Struct!=NULL);
			FToken Tag; GetToken(Tag);
			UProperty* Member=NULL;
			for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Token.Struct); It && !Member; ++It )
				if( It->GetFName() == Tag.TokenName )
					Member = *It;
			if( !Member )
				appThrowf( TEXT("Unknown member '%s' in struct '%s'"), Tag.Identifier, Token.Struct->GetName() );
			QWORD OriginalFlags      = Token.PropertyFlags;
			(FPropertyBase&)Token    = FPropertyBase( Member );
			Token.PropertyFlags      = OriginalFlags | (Token.PropertyFlags & CPF_PropagateFromStruct);

			// Write struct info.
			FRetryPoint HighRetry; InitRetry(HighRetry);
			if( Member->IsA(UBoolProperty::StaticClass()) )
				Writer << EX_BoolVariable;
			Writer << EX_StructMember;
			Writer << Member;
			CodeSwitcheroo( LowRetry, HighRetry );
		}
		else if( Token.Type==CPT_ObjectReference && MatchSymbol(TEXT(".")) )
		{
			// Compile an object context expression.
			check(Token.PropertyClass!=NULL);
			FToken OriginalToken = Token;

			// Handle special class default, const, and static members.
			if( Token.PropertyClass==UClass::StaticClass() )
			{
				check(Token.MetaClass);
				if ( PeekIdentifier(NAME_Const) )
				{
					// reset the compiled code, since there's crap not needed for consts
					PerformRetry(LowRetry,1,0);
					// read the const token
					GetToken(Token);
					// and attempt to compile the context const reference
					if( !CompileFieldExpr( Token.MetaClass, RequiredType, FToken(Token), Token, 0, 0 ) )
					{
						appThrowf( TEXT("'%s': Bad const context expression"), Token.MetaClass->GetName() );
					}
					continue;
				}
				else
				if( PeekIdentifier(NAME_Default) || PeekIdentifier(NAME_Static) )
				{
					// Write context token.
					FRetryPoint HighRetry; InitRetry(HighRetry);
					Writer << EX_ClassContext;
					CodeSwitcheroo( LowRetry, HighRetry );

					// Compile class default context expression.
					GetToken(Token);
					FRetryPoint ContextStart; InitRetry(ContextStart);
					if( !CompileFieldExpr( Token.MetaClass, RequiredType, FToken(Token), Token, 0, 0 ) )
						appThrowf( TEXT("'%s': Bad context expression"), Token.MetaClass->GetName() );

					// Insert skipover info for handling null contexts.
					FRetryPoint ContextEnd; InitRetry(ContextEnd);
					_WORD wSkip = TopNode->Script.Num() - ContextStart.CodeTop; Writer << wSkip;
					EmitSize( Token.GetSize(), TEXT("Context expression") );//!!hardcoded size
					CodeSwitcheroo( ContextStart, ContextEnd );
					continue;
				}
			}

			// Emit object context override token.
			FRetryPoint HighRetry; InitRetry(HighRetry);
			Writer << EX_Context;
			CodeSwitcheroo( LowRetry, HighRetry );

			// Get the context variable or expression.
			FToken TempToken;
			GetToken(TempToken);

			// Compile a variable or function expression.
			FRetryPoint ContextStart; InitRetry(ContextStart);
			if(	!CompileFieldExpr( OriginalToken.PropertyClass, RequiredType, FToken(TempToken), Token, 0, 1 ) )
				appThrowf( TEXT("Unrecognized member '%s' in class '%s'"), TempToken.Identifier, OriginalToken.PropertyClass->GetName() );

			// Insert skipover info for handling null contexts.
			FRetryPoint ContextEnd; InitRetry(ContextEnd);
			_WORD wSkip = TopNode->Script.Num() - ContextStart.CodeTop; Writer << wSkip;
			EmitSize( Token.GetSize(), TEXT("Context expression") );//!!hardcoded size
			CodeSwitcheroo( ContextStart, ContextEnd );
		}
		else break;
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
			// Build a list of matching operators.
			for( INT i=NestLevel-1; i>=1; i-- )
			{
				for( TFieldIterator<UFunction> It(Nest[i].Node); It; ++It )
				{
					if
					(	It->FriendlyName==OperToken.TokenName
					&&	RequiredFunctionFlags==(It->FunctionFlags & (FUNC_PreOperator|FUNC_Operator)) )
					{
						// Add this operator to the list.
						OperLinks.AddItem( *It );
						Precedence = It->OperPrecedence;
						NumParms   = Min(NumParms,(INT)It->NumParms);
					}
				}
			}

			// See if we got a valid operator, and if we want to handle it at the current precedence level.
			if( OperLinks.Num()>0 && Precedence<MaxPrecedence )
			{
				// Compile the second expression.
				FRetryPoint MidRetry; InitRetry(MidRetry);
				FPropertyBase NewRequiredType(CPT_None);
				FToken NewResultType;
				if( NumParms==3 || IsPreOperator )
				{
					TCHAR NewErrorTag[80];
					appSprintf( NewErrorTag, TEXT("Following '%s'"), *OperToken.TokenName );
					CompileExpr( NewRequiredType, NewErrorTag, &NewResultType, Precedence, &Token );
					if( NewResultType.Type == CPT_None )
						appThrowf( TEXT("Bad or missing expression after '%s'"), *OperToken.TokenName );
				}

				// Figure out which operator overload is best.
				BestOperLink = NULL;
				//AddResultText("Oper %s:\r\n",OperLinks[0]->Node().Name());
				UBOOL AnyLeftValid=0, AnyRightValid=0;
				for( INT i=0; i<OperLinks.Num(); i++ )
				{
					// See how good a match the first parm is.
					UFunction*  Node      = OperLinks(i);
					INT			ThisMatch = 0;
					TFieldIterator<UProperty,CLASS_IsAUProperty> It(Node);

					if( Node->NumParms==3 || !IsPreOperator )
					{
						// Check match of first parm.
						UProperty* Parm1 = *It; ++It;
						//AddResultText("Left  (%s->%s): ",FName(Token.Type)(),FName(Parm1.Type)());
						INT Cost         = ConversionCost(FPropertyBase(Parm1),Token);
						ThisMatch        = Cost;
						AnyLeftValid     = AnyLeftValid || Cost!=MAXINT;
					}

					if( Node->NumParms == 3 || IsPreOperator )
					{
						// Check match of second parm.
						UProperty* Parm2 = *It; ++It;
						//AddResultText("Right (%s->%s): ",FName(NewResultType.Type)(),FName(Parm2.Type)());
						INT Cost         = ConversionCost(FPropertyBase(Parm2),NewResultType);
						ThisMatch        = Max(ThisMatch,Cost);
						AnyRightValid    = AnyRightValid || Cost!=MAXINT;
					}

					if( (!BestOperLink || ThisMatch<BestMatch) && (Node->NumParms==NumParms) )
					{
						// This is the best match.
						BestOperLink = OperLinks(i);
						BestMatch    = ThisMatch;
						Matches      = 1;
					}
					else if( ThisMatch == BestMatch ) Matches++;
				}
				if( BestMatch == MAXINT )
				{
					if
					(	(appStrcmp(*OperToken.TokenName,TEXT("=="))==0 || appStrcmp(*OperToken.TokenName,TEXT("!="))==0)
					&&	Token.Type==CPT_Struct
					&&	NewResultType.Type==CPT_Struct
					&&	Token.Struct==NewResultType.Struct )
					{
						// Special-case struct binary comparison operators.
						FRetryPoint HighRetry; InitRetry(HighRetry);
						if( appStrcmp(*OperToken.TokenName,TEXT("=="))==0 )
							Writer << EX_StructCmpEq;
						else
							Writer << EX_StructCmpNe;
						Writer << Token.Struct;
						CodeSwitcheroo( LowRetry, HighRetry );
						Token = FToken(FPropertyBase(CPT_Bool));
						goto Test;
					}
					else if( AnyLeftValid && !AnyRightValid )
						appThrowf( TEXT("Right type is incompatible with '%s'"), *OperToken.TokenName );
					else if( AnyRightValid && !AnyLeftValid )
						appThrowf( TEXT("Left type is incompatible with '%s'"), *OperToken.TokenName );
					else
						appThrowf( TEXT("Types are incompatible with '%s'"), *OperToken.TokenName );
				}
				else if( Matches > 1 )
				{
					appThrowf( TEXT("Operator '%s': Can't resolve overload (%i matches of quality %i)"), *OperToken.TokenName, Matches, BestMatch );
				}
				else
				{
					// Now BestOperLink points to the operator we want to use, and the code stream
					// looks like:
					//
					//       |LowRetry| Expr1 |MidRetry| Expr2
					//
					// Here we carefully stick any needed expression conversion operators into the
					// code stream, and swap everything until we end up with:
					//
					// |LowRetry| Oper [Conv1] Expr1 |MidRetry| [Conv2] Expr2 EX_EndFunctionParms

					// Get operator parameter pointers.
					check(BestOperLink);
					TFieldIterator<UProperty,CLASS_IsAUProperty> It(BestOperLink);
					UProperty* OperParm1 = *It;
					if( !IsPreOperator )
						++It;
					UProperty* OperParm2 = *It;
					UProperty* OperReturn = BestOperLink->GetReturnProperty();
					check(OperReturn);

					// Convert Expr2 if necessary.
					if( BestOperLink->NumParms==3 || IsPreOperator )
					{
						if( OperParm2->PropertyFlags & CPF_OutParm )
						{
							// Note that this expression has a side-effect.
							GotAffector = 1;
						}
						if( NewResultType.Type != FPropertyBase(OperParm2).Type )
						{
							// Emit conversion.
							FRetryPoint HighRetry; InitRetry(HighRetry);
							Writer << EX_PrimitiveCast;
							Writer << (ECastToken)(GetConversion(FPropertyBase(OperParm2),NewResultType) & CONVERT_MASK);					
							CodeSwitcheroo(MidRetry,HighRetry);
						}
						if( OperParm2->PropertyFlags & CPF_SkipParm )
						{
							// Emit skip expression for short-circuit operators.
							FRetryPoint HighRetry; InitRetry(HighRetry);
							_WORD wOffset = 1 + HighRetry.CodeTop - MidRetry.CodeTop;
							Writer << EX_Skip;
							Writer << wOffset;
							CodeSwitcheroo(MidRetry,HighRetry);
						}
					}

					// Convert Expr1 if necessary.
					if( !IsPreOperator )
					{
						if( OperParm1->PropertyFlags & CPF_OutParm )
						{
							// Note that this expression has a side-effect.
							GotAffector = 1;
						}
						if( Token.Type != FPropertyBase(OperParm1).Type  )
						{
							// Emit conversion.
							FRetryPoint HighRetry; InitRetry(HighRetry);
							Writer << EX_PrimitiveCast;
							Writer << (ECastToken)(GetConversion(FPropertyBase(OperParm1),Token) & CONVERT_MASK);
							CodeSwitcheroo(LowRetry,HighRetry);
						}
					}

					// Emit the operator function call.			
					FRetryPoint HighRetry; InitRetry(HighRetry);
					EmitStackNodeLinkFunction( BestOperLink, 1, 0, 0 );
					CodeSwitcheroo(LowRetry,HighRetry);

					// End of call.
					Writer << EX_EndFunctionParms;

					//!! Yoda Debugger
					// Since each debuginfo in the stream must be correctly read out of the stream
					// we can't serialize one with skippable operators, since there's a good chance
					// it will just get skipped and screw things up.
					if ( !(OperParm2->PropertyFlags & CPF_SkipParm) )
						EmitDebugInfo(DI_EFPOper);
					
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
		if( ErrorTag )
			appThrowf( TEXT("Bad or missing expression in %s"), ErrorTag );
		if( ResultToken )
			*ResultToken = Token;
		return 0;
	}

	// Make sure the type is correct.
	if( !RequiredType.MatchesType(Token,0) )
	{
		// Can we perform an automatic conversion?
		DWORD Conversion = GetConversion( RequiredType, Token );
		if( RequiredType.PropertyFlags & CPF_OutParm )
		{
			// If the caller wants an l-value, we can't do any conversion.
			if( ErrorTag )
			{
				if( Token.TokenType == TOKEN_Const )
					appThrowf( TEXT("Expecting a variable, not a constant") );
				else if( Token.PropertyFlags & CPF_Const )
					appThrowf( TEXT("Const mismatch in Out variable %s"), ErrorTag );
				else
					appThrowf( TEXT("Type mismatch in Out variable %s"), ErrorTag );
			}
			if( ResultToken )
				*ResultToken = Token;
			return -1;
		}
		else if( RequiredType.ArrayDim!=1 || Token.ArrayDim!=1 )
		{
			// Type mismatch, and we can't autoconvert arrays.
			if( ErrorTag )
				appThrowf( TEXT("Array mismatch in %s"), ErrorTag );
			if( ResultToken )
				*ResultToken = Token;
			return -1;
		}
		else if
		(	( (RequiredType.PropertyFlags & CPF_CoerceParm) ? (Conversion!=CST_Max) : (Conversion & AUTOCONVERT) )
		&&	(RequiredType.Type!=CPT_Byte || RequiredType.Enum==NULL) )
		{
			// Perform automatic conversion or coercion.
			FRetryPoint HighRetry; InitRetry(HighRetry);
			Writer << EX_PrimitiveCast;
			Writer << (ECastToken)(GetConversion( RequiredType, Token ) & CONVERT_MASK);
			CodeSwitcheroo(LowRetry,HighRetry);
			Token.PropertyFlags &= ~CPF_OutParm;
			Token = FToken(FPropertyBase((EPropertyType)RequiredType.Type));
		}
		else
		{
			// Type mismatch.
			if( ErrorTag )
				appThrowf( TEXT("Type mismatch in %s"), ErrorTag );
			if( ResultToken )
				*ResultToken = Token;
			return -1;
		}
	}

	if( ResultToken )
		*ResultToken = Token;
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
		case NEST_State:	return TEXT("State");
		case NEST_Function:	return TEXT("Function");
		case NEST_If:		return TEXT("If");
		case NEST_Loop:		return TEXT("Loop");
		case NEST_Switch:	return TEXT("Switch");
		case NEST_For:		return TEXT("For");
		case NEST_ForEach:	return TEXT("ForEach");
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
			appThrowf( TEXT("%s is not allowed before the Class definition"), Thing );
		}
		else
		{
			appThrowf( TEXT("%s is not allowed here"), Thing );
		}
	}
	if( AllowFlags & ALLOW_Cmd )
	{
		// Don't allow variable declarations after commands.
		TopNest->Allow &= ~(ALLOW_VarDecl | ALLOW_Function | ALLOW_Ignores);
	}
}

//
// Check that a specified object is accessible from
// this object's scope.
//
void FScriptCompiler::CheckInScope( UObject* Obj )
{
	//check(Obj);
	//if( Class->PackageImports.FindItemIndex( Obj->GetOuter()->GetFName(), i )==INDEX_NONE )
	//	appThrowf(" '%s' is not accessible to this script", Obj->GetFullName() );
}

/*-----------------------------------------------------------------------------
	Nest management.
-----------------------------------------------------------------------------*/

//
// Increase the nesting level, setting the new top nesting level to
// the one specified.  If pushing a function or state and it overrides a similar
// thing declared on a lower nesting level, verifies that the override is legal.
//
void FScriptCompiler::PushNest( ENestType NestType, FName ThisName, UStruct* InNode )
{
	// Defaults.
	UStruct* PrevTopNode = TopNode;
	UStruct* PrevNode = NULL;
	DWORD PrevAllow = 0;

	if( Pass==0 && (NestType==NEST_State || NestType==NEST_Function) )
		for( TFieldIterator<UField> It(TopNode); It && It->GetOuter()==TopNode; ++It )
			if( It->GetFName()==ThisName )
				appThrowf( TEXT("'%s' conflicts with '%s'"), *ThisName, *It->GetFullName() );

	// Update pointer to top nesting level.
	TopNest					= &Nest[NestLevel++];
	TopNode					= NULL;
	TopNest->Node			= InNode;
	TopNest->NestType		= NestType;
	TopNest->iCodeChain		= INDEX_NONE;
	TopNest->SwitchType		= FPropertyBase(CPT_None);
	TopNest->FixupList		= NULL;
	TopNest->LabelList		= NULL;
	TopNest->NestFlags		= 0;  //NEW: CDH... missing return value warning

	// Init fixups.
	for( INT i=0; i<FIXUP_MAX; i++ )
		TopNest->Fixups[i] = MAXWORD;

	// Prevent overnesting.
	if( NestLevel >= MAX_NEST_LEVELS )
		appThrowf( TEXT("Maximum nesting limit exceeded") );

	// Inherit info from stack node above us.
	INT IsNewNode = NestType==NEST_Class || NestType==NEST_State || NestType==NEST_Function;
	if( NestLevel > 1 )
	{
		if( Pass == 1 )
		{
			if( !IsNewNode )
				TopNest->Node = TopNest[-1].Node;
			TopNode = TopNest[0].Node;
		}
		else
		{
			if( IsNewNode )
			{
				// Create a new stack node.
				if( NestType==NEST_Class )
				{
					TopNest->Node = TopNode = Class;
					Class->ProbeMask		= 0;
					Class->IgnoreMask		= ~(QWORD)0;
					Class->LabelTableOffset = MAXWORD;
				}
				else if( NestType==NEST_State )
				{
					UState* State;
					TopNest->Node = TopNode = State = new(PrevTopNode ? (UObject*)PrevTopNode : (UObject*)Class, ThisName, RF_Public)UState( NULL );
					State->ProbeMask		= 0;
					State->IgnoreMask		= ~(QWORD)0;
					State->LabelTableOffset = MAXWORD;
				}
				else if( NestType==NEST_Function )
				{
					UFunction* Function;
					TopNest->Node = TopNode = Function = new(PrevTopNode ? (UObject*)PrevTopNode : (UObject*)Class, ThisName, RF_Public)UFunction( NULL );
					Function->RepOffset         = MAXWORD;
					Function->ReturnValueOffset = MAXWORD;
					Function->FriendlyName = ThisName;
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
			TopNest->Allow = ALLOW_VarDecl | ALLOW_Function | ALLOW_State | ALLOW_Ignores | ALLOW_Instance;
			break;

		case NEST_State:
			check(ThisName!=NAME_None);
			check(PrevNode!=NULL);
			TopNest->Allow = ALLOW_Function | ALLOW_Label | ALLOW_StateCmd | ALLOW_Ignores | ALLOW_Instance;
			if( Pass==0 )
			{
				TopNode->Next = PrevNode->Children;
				PrevNode->Children = TopNode;
			}
			break;

		case NEST_Function:
			check(ThisName!=NAME_None);
			check(PrevNode!=NULL);
			TopNest->Allow = ALLOW_VarDecl | ALLOW_Return | ALLOW_Cmd | ALLOW_Label;
			if( !(((UFunction*)TopNode)->FunctionFlags & FUNC_Static) )
				TopNest->Allow |= ALLOW_Instance;
			if( Pass==0 )
			{
				TopNode->Next = PrevNode->Children;
				PrevNode->Children = TopNode;
			}
			else
			{
				UFunction *func = (UFunction*)TopNode;
				// add the new function to the state/class func map
				UState *topState = Cast<UState>(PrevTopNode);
				if (topState == NULL)
				{
					topState = Cast<UState>(Class);
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

		default:
			appThrowf( TEXT("Internal error in PushNest, type %i"), (BYTE)NestType );
			break;
	}
}

//
// Decrease the nesting level and handle any errors that result.
//
void FScriptCompiler::PopNest( ENestType NestType, const TCHAR* Descr )
{
	if( TopNode )
		if( TopNode->Script.Num() > 65534 )
			appThrowf( TEXT("Code space for %s overflowed by %i bytes"), TopNode->GetName(), TopNode->Script.Num() - 65534 );

	// Validate the nesting state.
	if( NestLevel <= 0 )
		appThrowf( TEXT("Unexpected '%s' at global scope"), Descr, NestTypeName(NestType) );
	else if( NestType==NEST_None )
		NestType = TopNest->NestType;
	else if( TopNest->NestType!=NestType )
		appThrowf( TEXT("Unexpected end of %s in '%s' block"), Descr, NestTypeName(TopNest->NestType) );

	// Pass-specific handling.
	if( Pass == 0 )
	{
		// Remember code position.
		if( NestType==NEST_State || NestType==NEST_Function )
		{
			TopNode->TextPos    = InputPos;
			TopNode->Line       = InputLine;
		}
		else if( NestType!=NEST_Class )
		{
			appErrorf( TEXT("Bad first pass NestType %i"), (BYTE)NestType );
		}
		FArchive DummyAr;
		TopNode->Link( DummyAr, 1 );
	}
	else
	{
		// If ending a state, process labels.
		if( NestType==NEST_State )
		{
			// Emit stop command.

			//!! Yoda Debugger
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
				Writer << EX_Nothing;
				EmitDebugInfo(DI_PrevStack);

                if ( TopNode )
                {
			        for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(TopNode); It; ++It )
                    {
                        UProperty *property = *It;
                        UnReferencedLocal *prevLocal=NULL, *curLocal = unReferencedLocals;

                        while( curLocal )
                        {
                            if( curLocal->property == property )
                            {
                                INT realInputLine = InputLine;
                                if( prevLocal == NULL )
                                    unReferencedLocals = curLocal->next;
                                else
                                    prevLocal->next = curLocal->next;
                    	        InputLine = curLocal->declarationLine;
        			            Warn->Logf( NAME_Warning, TEXT("'%s' : unreferenced local variable"), curLocal->property->GetName ());
                                InputLine = realInputLine;
                                delete curLocal;
                                break;
                            }
                            else
                            {
                                prevLocal = curLocal;
                                curLocal = curLocal->next;
                            }
                        }
                    }
                }

				if ((TopNest->NestType==NEST_Function)
				 && (TopFunction->FunctionFlags & FUNC_Defined)
				 && (TopFunction->GetReturnProperty())
				 && (!(TopNest->NestFlags & NESTF_ReturnValueFound)))
					Warn->Logf( NAME_Warning, TEXT("%s: Missing return value"), TopFunction->GetName());
			}
		}
		else if( NestType==NEST_Switch )
		{
			if( TopNest->Allow & ALLOW_Case )
			{
				// No default was specified, so emit end-of-case marker.
				EmitChainUpdate(TopNest);
				Writer << EX_Case;
				_WORD W=MAXWORD; Writer << W;
			}

			// Here's the big end.
			TopNest->SetFixup(FIXUP_SwitchEnd,TopNode->Script.Num());
		}
		else if( NestType==NEST_If )
		{
			if( MatchIdentifier(NAME_Else) )
			{
				// Send current code to the end of the if block.
				Writer << EX_Jump;
				EmitAddressToFixupLater( TopNest, FIXUP_IfEnd, NAME_None );

				// Update previous If's next-address.
				EmitChainUpdate( TopNest );
				if( MatchIdentifier(NAME_If) )
				{
					// ElseIf.
					CheckAllow( TEXT("'Else If'"), ALLOW_ElseIf );

					// Jump to next evaluator if expression is false.
					//!! Yoda Debugger
					EmitDebugInfo(DI_SimpleIf);
					Writer << EX_JumpIfNot;
					EmitAddressToChainLater( TopNest );

					// Compile boolean expr.
					RequireSymbol( TEXT("("), TEXT("'Else If'") );
					CompileExpr( FPropertyBase(CPT_Bool), TEXT("'Else If'") );
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
				EmitChainUpdate( TopNest );

				// Here's the big end.
				TopNest->SetFixup( FIXUP_IfEnd, TopNode->Script.Num() );
			}
		}
		else if( NestType==NEST_For )
		{
			// Compile the incrementor expression here.
			TopNest->SetFixup(FIXUP_ForInc,TopNode->Script.Num());
			FRetryPoint Retry; InitRetry(Retry);
			
			PerformRetry(TopNest->ForRetry,0,1);		
				EmitDebugInfo(DI_ForInc);
				CompileAffector();
			PerformRetry(Retry,0,1);

			// Jump back to start of loop.
			Writer << EX_Jump;
			EmitAddressToFixupLater(TopNest,FIXUP_ForStart,NAME_None);

			// Here's the end of the loop.
			TopNest->SetFixup(FIXUP_ForEnd,TopNode->Script.Num());
		}
		else if( NestType==NEST_ForEach )
		{
			// Perform next iteration.
			Writer << EX_IteratorNext;

			// Here's the end of the loop.
			TopNest->SetFixup( FIXUP_IteratorEnd, TopNode->Script.Num() );
			Writer << EX_IteratorPop;
		}
		else if( NestType==NEST_Loop )
		{
			TopNest->SetFixup( FIXUP_LoopPostCond, TopNode->Script.Num() );
			if( MatchIdentifier(NAME_Until) )
			{
				// Jump back to start of loop.
				Writer << EX_JumpIfNot;
				EmitAddressToFixupLater( TopNest, FIXUP_LoopStart, NAME_None );

				// Compile boolean expression.
				RequireSymbol( TEXT("("), TEXT("'Until'") );
				CompileExpr( FPropertyBase(CPT_Bool), TEXT("'Until'") );
				RequireSymbol( TEXT(")"), TEXT("'Until'") );

				// Here's the end of the loop.
				TopNest->SetFixup( FIXUP_LoopEnd, TopNode->Script.Num() );
			}
			else if( PeekIdentifier(NAME_While) && !(TopNest->Allow & ALLOW_InWhile) )
			{
				// Not allowed here.
				appThrowf( TEXT("The loop syntax is do...until, not do...while") );
			}
			else
			{
				// Jump back to start of loop.
				Writer << EX_Jump;
				EmitAddressToFixupLater( TopNest, FIXUP_LoopStart, NAME_None );

				// Here's the end of the loop.
				TopNest->SetFixup( FIXUP_LoopEnd, TopNode->Script.Num() );
			}
		}
		// Perform all code fixups.
		for( FNestFixupRequest* Fixup = TopNest->FixupList; Fixup!=NULL; Fixup=Fixup->Next )
		{
			if( Fixup->Type == FIXUP_Label )
			{
				// Fixup a local label.
				FLabelRecord* LabelRecord;
				for( LabelRecord = TopNest->LabelList; LabelRecord; LabelRecord=LabelRecord->Next )
				{
					if( LabelRecord->Name == Fixup->Name )
					{
						*(_WORD*)&TopNode->Script(Fixup->iCode) = LabelRecord->iCode;
						break;
					}
				}
				if( LabelRecord == NULL )
					appThrowf( TEXT("Label '%s' not found in this block of code"), *Fixup->Name );
			}
			else
			{
				// Fixup a code structure address.
				if( TopNest->Fixups[Fixup->Type] == MAXWORD )
					appThrowf( TEXT("Internal fixup error %i"), (BYTE)Fixup->Type );
				*(_WORD*)&TopNode->Script(Fixup->iCode) = TopNest->Fixups[Fixup->Type];
			}
		}
	}

	// Make sure there's no dangling chain.
	check(TopNest->iCodeChain==INDEX_NONE);

	// Pop the nesting level.
	NestType = TopNest->NestType;
	NestLevel--;
	TopNest--;
	TopNode	= TopNest->Node;

	// Update allow-flags.
	if( NestType==NEST_Function )
	{
		// Don't allow variable declarations after functions.
		TopNest->Allow &= ~(ALLOW_VarDecl);
	}
	else if( NestType == NEST_State )
	{
		// Don't allow variable declarations after states.
		TopNest->Allow &= ~(ALLOW_VarDecl);
	}
}

//
// Find the highest-up nest info of a certain type.
// Used (for example) for associating Break statements with their Loops.
//
INT FScriptCompiler::FindNest( ENestType NestType )
{
	for( int i=NestLevel-1; i>0; i-- )
		if( Nest[i].NestType == NestType )
			return i;
	return -1;
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
		appThrowf( TEXT("Missing compiler directive after '#'") );
	}
	else if( Directive.Matches(TEXT("Error")) )
	{
		appThrowf( TEXT("#Error directive encountered") );
	}
	else
	{
		appThrowf( TEXT("Unrecognized compiler directive %s"), Directive.Identifier );
	}

	// Skip to end of line.
	TCHAR c;
	while( !IsEOL( c=GetChar() ) );
	if( c==0 ) UngetChar();
}

/*-----------------------------------------------------------------------------
	Variable declaration parser.
-----------------------------------------------------------------------------*/

//
// Parse one variable declaration and set up its properties in VarProperty.
// Returns pointer to the class property if success, or NULL if there was no variable
// declaration to parse. Called during variable 'Dim' declaration and function
// parameter declaration.
//
// If you specify a hard-coded name, that name will be used as the variable name (this
// is used for function return values, which are automatically called "ReturnValue"), and
// a default value is not allowed.
//
UBOOL FScriptCompiler::GetVarType
(
	UStruct*		Scope,
	FPropertyBase&	VarProperty,
	DWORD&			ObjectFlags,
	QWORD			Disallow,
	const TCHAR*	Thing
)
{
	check(Scope);
	FPropertyBase TypeDefType(CPT_None);
	UClass* TempClass;
	UBOOL IsVariable = 0;

	// Get flags.
	ObjectFlags=RF_Public;
	QWORD Flags=0;
	for( ; ; )
	{
		FToken Specifier;
		GetToken(Specifier);
		if( Specifier.Matches(NAME_Const) )
		{
			Flags      |= CPF_Const;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_Config) )
		{
			Flags      |= CPF_Config;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_GlobalConfig) )
		{
			Flags      |= CPF_GlobalConfig | CPF_Config;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_Localized) )
		{
			Flags      |= CPF_Localized | CPF_Const;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_Travel) )
		{
			Flags      |= CPF_Travel;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_Private) )
		{
			ObjectFlags &= ~RF_Public;
			ObjectFlags |= RF_Final;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_Protected) )
		{
			ObjectFlags &= ~RF_Public;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_Public) )
		{
			ObjectFlags |= RF_Public;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_EditConst) )
		{
			Flags      |= CPF_EditConst;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_EditConstArray) )
		{
			Flags      |= CPF_EditConstArray;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_Input) )
		{
			Flags      |= CPF_Input;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_Transient) )
		{
			Flags      |= CPF_Transient;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_Native) )
		{
			Flags      |= CPF_Native;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_NoExport) )
		{
			Flags      |= CPF_NoExport;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_Out) )
		{
			Flags      |= CPF_OutParm;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_Export) )
		{
			Flags      |= CPF_ExportObject;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_EditInline) )
		{
			Flags      |= CPF_EditInline;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_EdFindable) )
		{
			Flags      |= CPF_EdFindable;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_EditInlineUse) )
		{
			Flags      |= CPF_EditInline | CPF_EditInlineUse;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_NoClear) )
		{
			Flags      |= CPF_NoClear;
			IsVariable  = 1;
		}
		else if( Specifier.Matches(NAME_EditInlineNotify) )
		{
			Flags      |= CPF_EditInline | CPF_EditInlineNotify;
			IsVariable  = 1;
		}
		else if (Specifier.Matches(NAME_RepNotify) )
		{
			Flags	   |= CPF_RepNotify;
			IsVariable = 1;
		}
		else if (Specifier.Matches(NAME_Interp))
		{
			Flags |= CPF_Edit;
			Flags |= CPF_Interp;
			IsVariable = 1;
		}
		else if( Specifier.Matches(NAME_NonTransactional) )
		{
			Flags |= CPF_NonTransactional;
			IsVariable = 1;
		}
		else if( Specifier.Matches(NAME_Deprecated) )
		{
			Flags      |= CPF_Deprecated;
			IsVariable  = 1;
		}		
		else if( Specifier.Matches(NAME_Skip) )
		{
			Flags     |= CPF_SkipParm;
			IsVariable = 1;
		}
		else if( Specifier.Matches(NAME_Coerce) )
		{
			Flags     |= CPF_CoerceParm;
			IsVariable = 1;
		}
		else if( Specifier.Matches(NAME_Optional) )
		{
			Flags     |= CPF_OptionalParm;
			IsVariable = 1;
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
			return 0;	
		appThrowf( TEXT("%s: Missing variable type"), Thing );
	}
	else if( VarType.Matches(NAME_Enum) )
	{
		// Compile an Enum definition and variable declaration here.
		VarProperty = FPropertyBase( CompileEnum(Scope) );
	}
	else if( VarType.Matches(NAME_Struct) )
	{
		// Compile a Struct definition and variable declaration here.
		VarProperty = FPropertyBase( CompileStruct(Scope) );
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
		GetVarType( Scope, VarProperty, ObjectFlags, Disallow, TEXT("'array'") );
		if( VarProperty.ArrayDim==0 )
			appThrowf( TEXT("Arrays within arrays not supported") );
		VarProperty.ArrayDim = 0;
		RequireSymbol( TEXT(">"), TEXT("'array'") );
	}
	else if( VarType.Matches(NAME_Map) )
	{
		appThrowf( TEXT("Map are not supported in UnrealScript yet") );
		RequireSymbol( TEXT("<"), TEXT("'map'") );
		//!!GetVarType( Scope, VarProperty, ObjectFlags, Disallow, TEXT("'map'") );
		RequireSymbol( TEXT(","), TEXT("'map'") );
		//!!GetVarType( Scope, VarProperty, ObjectFlags, Disallow, TEXT("'map'") );
		RequireSymbol( TEXT(">"), TEXT("'map'") );
	}
	else if( VarType.Matches(NAME_String) )
	{
		if( MatchSymbol(TEXT("[")) )
		{
			INT StringSize=0;
			if( !GetConstInt(StringSize) )
				appThrowf( TEXT("%s: Missing string size"),Thing?Thing:TEXT("Declaration") );
			if( !MatchSymbol(TEXT("]")) )
				appThrowf( TEXT("%s: Missing ']'"), Thing ? Thing : TEXT("Declaration") );
			Warn->Logf( NAME_Warning, TEXT("String sizes are now obsolete; all strings are dynamically sized") );
			//appThrowf( TEXT("String sizes are now obsolete; all strings are dynamically sized") );
		}
		VarProperty = FPropertyBase(CPT_String);
	}
	else if ( VarType.Matches(NAME_Delegate) )
	{
		RequireSymbol(TEXT("<"),TEXT("'delegate'"));
		FToken delegateType;
		if (!GetToken(delegateType) || delegateType.TokenType != TOKEN_Identifier)
		{
			appThrowf(TEXT("%s: Failed to read delegate name"),Thing!=NULL?Thing:TEXT("Declaration"));

		}
		RequireSymbol(TEXT(">"),TEXT("'delegate'"));
		VarProperty = FPropertyBase(CPT_Delegate);
		VarProperty.DelegateName = FName(delegateType.Identifier);
	}
	else if( (TempClass = FindObject<UClass>( ANY_PACKAGE, VarType.Identifier ) ) != NULL )
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
					appThrowf( TEXT("'class': Missing class limitor") );
				VarProperty.MetaClass = FindObject<UClass>( ANY_PACKAGE, Limitor.Identifier );
				if( !VarProperty.MetaClass )
					appThrowf( TEXT("'class': Limitor '%s' is not a class name"), Limitor.Identifier );
				RequireSymbol( TEXT(">"), TEXT("'class limitor'") );
			}
			else VarProperty.MetaClass = UObject::StaticClass();
		}

		// allow enums/structs from classes outside the current hierarchy
		if( MatchSymbol(TEXT(".")) )
		{
			FToken ElemIdent;
			if( !GetIdentifier(ElemIdent) )
				appThrowf( TEXT("'%s': Missing class member type after '.'"),
						VarType.Identifier );
			if( (Field=FindField( TempClass, ElemIdent.Identifier ))!=NULL )
			{
				if( Field->GetClass()==UEnum::StaticClass() )
					VarProperty = FPropertyBase( CastChecked<UEnum>(Field) );
				else if( Field->GetClass()==UStruct::StaticClass() )
				{
					UStruct*	Struct = CastChecked<UStruct>(Field);
					VarProperty = FPropertyBase( Struct );
					if((Struct->StructFlags & STRUCT_HasComponents) && !(Disallow & CPF_Component))
						VarProperty.PropertyFlags |= CPF_Component;
				}
				else
					appThrowf( TEXT("Unrecognized type '%s' within '%s'"),
						ElemIdent.Identifier, VarType.Identifier );
			}
			else
				appThrowf( TEXT("Unrecognized type '%s' within '%s'"),
						ElemIdent.Identifier, VarType.Identifier );
		}

		if( TempClass->IsChildOf(UComponent::StaticClass()) )
			Flags |= ((CPF_Component|CPF_ExportObject) & (~Disallow));
		else
		if( Flags & CPF_Component )
			appThrowf( TEXT("Component modifier not allowed for non-UComponent object references") );
	}
	else if( (Field = FindObject<UEnum>( ANY_PACKAGE, VarType.Identifier ))!=NULL )
	{
		// In-scope enumeration or struct.
		VarProperty = FPropertyBase( CastChecked<UEnum>(Field) );
	}
    else if( (Field = FindObject<UStruct>( ANY_PACKAGE, VarType.Identifier ))!=NULL )
	{
		UStruct*	Struct = CastChecked<UStruct>(Field);
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
	else appThrowf( TEXT("Unrecognized type '%s'"), VarType.Identifier );

	// Set FPropertyBase info.
	VarProperty.PropertyFlags |= Flags;

	// Make sure the overrides are allowed here.
	if( VarProperty.PropertyFlags & Disallow )
		appThrowf( TEXT("Specified type modifiers not allowed here") );

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
static inline UProperty* GroupProperty( UStruct* Scope, UProperty* Previous, DWORD PropertyFlags, FName ClassName )
{
	UBOOL Movable = 1;

	// Can't move properties in a class that is definied as noexport. Additionally we only group properties
	// in UClasses as UFunction properties can't be grouped and "structs" are usually mirrored manually so
	// we rather not group there either.
	UClass* Class = Cast<UClass>(Scope);
	if( !Class || (Class->ClassFlags & CLASS_NoExport) )
		Movable = 0;

	// Can't safely move variables that are marked as noexport.
	if( PropertyFlags & CPF_NoExport )
		Movable = 0;

	// Iterate over all properties and find the proper insertion point.
	if( Movable )
	{
		for(TFieldIterator<UProperty,CLASS_IsAUProperty> It(Scope); It && It.GetStruct()==Scope; ++It)
		{
			UProperty* Property = *It;

			// Make sure we don't insert anything between noexport variables.
			if( Property->PropertyFlags & CPF_NoExport )
				break;

			// Make sure the type and transient behaviour are identical.
			UBOOL	SamePropertyType	= Property->GetClass()->GetFName() == ClassName,
					SameGroup			= (Property->PropertyFlags & CPF_Transient) == (PropertyFlags & CPF_Transient);
			
			if( SamePropertyType && SameGroup )
				Previous = Property;
		}
	}

	return Previous;
}

UProperty* FScriptCompiler::GetVarNameAndDim
(
	UStruct*		Scope,
	FPropertyBase&	VarProperty,
	DWORD			ObjectFlags,
	UBOOL			NoArrays,
	UBOOL			IsFunction,
	const TCHAR*	HardcodedName,
	const TCHAR*	Thing,
	FName			Category,
	UBOOL			Skip
)
{
	check(Scope);

	// Get varible name.
	FToken VarToken;
	if( HardcodedName )
	{
		// Hard-coded variable name, such as with return value.
		VarToken.TokenType = TOKEN_Identifier;
		appStrcpy( VarToken.Identifier, HardcodedName );
	}
	else if( !GetIdentifier(VarToken) )
		appThrowf( TEXT("Missing variable name") );

	// Make sure it doesn't conflict.
	if( !Skip )
	{
		UField* Existing = FindField( Scope, VarToken.Identifier );
		if( Existing && Existing->GetOuter()==Scope )
			appThrowf( TEXT("%s: '%s' already defined"), Thing, VarToken.Identifier );
	}

	// Get optional dimension immediately after name.
	if( MatchSymbol(TEXT("[")) )
	{
		if( NoArrays )
			appThrowf( TEXT("Arrays aren't allowed in this context") );

		if( VarProperty.Type == CPT_Bool )
			appThrowf( TEXT("Bool arrays are not allowed") );

		// Default to dynamic array!!
		if( !PeekSymbol(TEXT("]")) )
		{
			if( !GetConstInt(VarProperty.ArrayDim, 0, Scope) )
			{
				FToken my_token;
				if ( GetIdentifier(my_token) )
				{
					FName ConstName = FName( my_token.Identifier );
					UField* Existing = FindField( Scope, my_token.Identifier );
					if( Existing && Existing->IsA( UConst::StaticClass()) )
					{
						UConst* pconst = (UConst*)Existing;
						VarProperty.ArrayDim = appAtoi( *(pconst->Value) );
					}
				}
				else
					appThrowf( TEXT("%s %s: Bad or missing array size"), Thing, VarToken.Identifier );
			}

			if( VarProperty.ArrayDim<=1 || VarProperty.ArrayDim>MAX_ARRAY_SIZE )
				appThrowf( TEXT("%s %s: Illegal array size %i"), Thing, VarToken.Identifier, VarProperty.ArrayDim );
		}

		if( !MatchSymbol(TEXT("]")) )
			appThrowf( TEXT("%s %s: Missing ']'"), Thing, VarToken.Identifier );
	}
	else if( MatchSymbol(TEXT("(")) )
	{
		appThrowf( TEXT("Use [] for arrays, not ()") );
	}

	// Add property.
	UProperty* NewProperty=NULL;
	if( !Skip )
	{
		UProperty* Prev=NULL, *Array=NULL;
		UObject* NewScope=Scope;
	    for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Scope); It && It.GetStruct()==Scope; ++It )
		    Prev = *It;
		if( VarProperty.ArrayDim==0 )
		{
			Array = new(Scope,VarToken.Identifier,ObjectFlags)UArrayProperty;
			NewScope = Array;
			VarProperty.ArrayDim = 1;
			ObjectFlags = RF_Public;
		}
		if( VarProperty.Type==CPT_Byte )
		{
			Prev = GroupProperty( Scope, Prev, VarProperty.PropertyFlags, UByteProperty::StaticClass()->GetFName() );

			NewProperty = new(NewScope,VarToken.Identifier,ObjectFlags)UByteProperty;
			Cast<UByteProperty>(NewProperty)->Enum = VarProperty.Enum;
		}
		else if( VarProperty.Type==CPT_Int )
		{
			NewProperty = new(NewScope,VarToken.Identifier,ObjectFlags)UIntProperty;
		}
		else if( VarProperty.Type==CPT_Bool )
		{
			Prev = GroupProperty( Scope, Prev, VarProperty.PropertyFlags, UBoolProperty::StaticClass()->GetFName() );

			NewProperty = new(NewScope,VarToken.Identifier,ObjectFlags)UBoolProperty;
		}
		else if( VarProperty.Type==CPT_Float )
		{
			NewProperty = new(NewScope,VarToken.Identifier,ObjectFlags)UFloatProperty;
		}
		else if( VarProperty.Type==CPT_ObjectReference )
		{
			check(VarProperty.PropertyClass);
			if( VarProperty.PropertyClass==UClass::StaticClass() )
			{
				NewProperty = new(NewScope,VarToken.Identifier,ObjectFlags)UClassProperty;
				Cast<UClassProperty>(NewProperty)->MetaClass = VarProperty.MetaClass;
			}
			else
			if( VarProperty.PropertyClass->IsChildOf(UComponent::StaticClass()) )
			{
				NewProperty = new(NewScope,VarToken.Identifier,ObjectFlags)UComponentProperty;
				VarProperty.PropertyFlags |= CPF_EditInline;
			}
			else
				NewProperty = new(NewScope,VarToken.Identifier,ObjectFlags)UObjectProperty;

			Cast<UObjectProperty>(NewProperty)->PropertyClass = VarProperty.PropertyClass;
		}
		else if( VarProperty.Type==CPT_Name )
		{
			NewProperty = new(NewScope,VarToken.Identifier,ObjectFlags)UNameProperty;
		}
		else if( VarProperty.Type==CPT_String )
		{
			NewProperty = new(NewScope,VarToken.Identifier,ObjectFlags)UStrProperty;
		}
		else if( VarProperty.Type==CPT_Struct )
		{
			NewProperty = new(NewScope,VarToken.Identifier,ObjectFlags)UStructProperty;
			Cast<UStructProperty>(NewProperty)->Struct = VarProperty.Struct;		
		}
		else if ( VarProperty.Type==CPT_Delegate )
		{
			NewProperty = new(NewScope,VarToken.Identifier,ObjectFlags)UDelegateProperty;
			UDelegateProperty *delegateProperty = (UDelegateProperty*)NewProperty;
			delegateProperty->DelegateName = VarProperty.DelegateName;
			delegateProperty->Function = Cast<UFunction>(FindField(Scope,*VarProperty.DelegateName,UFunction::StaticClass(),NULL));
			debugf(TEXT("Created new delegate property %s <%s> (%s)"),
				   NewProperty->GetName(),
				   *VarProperty.DelegateName,
				   delegateProperty->Function?delegateProperty->Function->GetName():TEXT("NULL"));
		}
		else appErrorf( TEXT("Unknown property type %i"), (BYTE)VarProperty.Type );
		if( Array )
		{
			CastChecked<UArrayProperty>(Array)->Inner = NewProperty;
			// Copy some of the property flags to the inner property.
			NewProperty->PropertyFlags |= (VarProperty.PropertyFlags&CPF_PropagateToArrayInner);
			NewProperty = Array;
		}
		NewProperty->ArrayDim      = VarProperty.ArrayDim;
		NewProperty->PropertyFlags = VarProperty.PropertyFlags;
		NewProperty->Category      = Category;
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
	return NewProperty;
}

//
// Compile a variable assignment statement.
//
void FScriptCompiler::CompileAssignment( const TCHAR* Tag )
{
	// Set up.
	FRetryPoint LowRetry; InitRetry(LowRetry);
	FToken RequiredType, VarToken;

	// Compile l-value expression.
	CompileExpr( FPropertyBase(CPT_None), TEXT("Assignment"), &RequiredType );
	if( RequiredType.Type == CPT_None )
		appThrowf( TEXT("%s assignment: Missing left value"), Tag );
	else if( !(RequiredType.PropertyFlags & CPF_OutParm) )
		appThrowf( TEXT("%s assignment: Left value is not a variable"), Tag );
	else if( !MatchSymbol(TEXT("=")) )
		appThrowf( TEXT("%s: Missing '=' after %s"), Tag );

	// Emit let.
	FRetryPoint HighRetry; InitRetry(HighRetry);
	EmitLet( RequiredType, Tag );

	// Switch around.
	CodeSwitcheroo(LowRetry,HighRetry);

	// Compile right value.
	RequiredType.PropertyFlags &= ~CPF_OutParm;
	CompileExpr( RequiredType, Tag );

}

//
// Try to compile an affector expression or assignment.
//
void FScriptCompiler::CompileAffector()
{
	// Try to compile an affector expression or assignment.
	FRetryPoint LowRetry; InitRetry(LowRetry);
	GotAffector = 0;

	// Try to compile an expression here.
	FPropertyBase RequiredType(CPT_None);
	FToken ResultType;
	if( CompileExpr( RequiredType, NULL, &ResultType ) < 0 )
	{
		FToken Token;
		GetToken(Token);
		appThrowf( TEXT("'%s': Bad command or expression"), Token.Identifier );
	}

	// Did we get a function call or a varible assignment?
	if( MatchSymbol(TEXT("=")) )
	{
		// Variable assignment.
		if( !(ResultType.PropertyFlags & CPF_OutParm) )
			appThrowf( TEXT("'=': Left value is not a variable") );

		// Compile right value.
		RequiredType = ResultType;
		RequiredType.PropertyFlags &= ~CPF_OutParm;
		CompileExpr( RequiredType, TEXT("'='") );

		// Emit the let.
		FRetryPoint HighRetry; InitRetry(HighRetry);
		EmitLet( ResultType, TEXT("'='") );
		CodeSwitcheroo(LowRetry,HighRetry);
	}
	else if( GotAffector )
	{
		// Function call or operators with outparms.
		if( ResultType.Type==CPT_String )
		{
			FRetryPoint HighRetry; InitRetry(HighRetry);
			Writer << EX_EatString;
			CodeSwitcheroo(LowRetry,HighRetry);
		}
	}
	else if( ResultType.Type != CPT_None )
	{
		// Whatever expression we parsed had no effect.
		FToken Token;
		GetToken(Token);
		appThrowf( TEXT("'%s': Expression has no effect"), Token.Identifier );
	}
	else
	{
		// Didn't get anything, so throw an error at the appropriate place.
		FToken Token;
		GetToken(Token);
		appThrowf( TEXT("'%s': Bad command or expression"), Token.Identifier );
	}
}

/*-----------------------------------------------------------------------------
	Statement compiler.
-----------------------------------------------------------------------------*/

//
// Compile a declaration in Token. Returns 1 if compiled, 0 if not.
//
INT FScriptCompiler::CompileDeclaration( FToken& Token, UBOOL& NeedSemicolon )
{
	if( Token.Matches(NAME_Class) && (TopNest->Allow & ALLOW_Class) )
	{
		// Start of a class block.
		CheckAllow( TEXT("'class'"), ALLOW_Class );

		// Class name.
		if( !GetToken(Token) )
			appThrowf( TEXT("Missing class name") );
        if( appIsDigit( **FString(Token.Identifier).Right(1) ) )
        	Warn->Logf( NAME_Warning, TEXT("Class names shouldn't end in a digit"), Token.Identifier );
		if( !Token.Matches(Class->GetName()) )
			appThrowf( TEXT("Class must be named %s, not %s"), Class->GetName(), Token.Identifier );

		// Get parent class.
		if( MatchIdentifier(NAME_Extends) )
		{
			// Set the superclass.
			UClass* TempClass = GetQualifiedClass( TEXT("'extends'") );
			if( Class->GetSuperClass() == NULL )
				Class->SuperField = TempClass;
			else if( Class->GetSuperClass() != TempClass )
				appThrowf( TEXT("%s's superclass must be %s, not %s"), *Class->GetPathName(), *Class->GetSuperClass()->GetPathName(), *TempClass->GetPathName() );
			Class->MinAlignment = Max(Class->MinAlignment,TempClass->MinAlignment);
		}
		else if( Class->GetSuperClass() )
			appThrowf( TEXT("class: missing 'Extends %s'"), Class->GetSuperClass()->GetName() );

		// Get outer class.
		if( MatchIdentifier(NAME_Within) )
		{
			// Set the outer class.
			UClass* TempClass = GetQualifiedClass( TEXT("'within'") );
			if( Class->ClassWithin == NULL || Class->ClassWithin==UObject::StaticClass() )
				Class->ClassWithin = TempClass;
			else if( Class->ClassWithin != TempClass )
				appThrowf( TEXT("%s must be within %s, not %s"), *Class->GetPathName(), *Class->ClassWithin->GetPathName(), *TempClass->GetPathName() );
		}
		else Class->ClassWithin = Class->GetSuperClass() ? Class->GetSuperClass()->ClassWithin : UObject::StaticClass();

		UClass* ExpectedWithin = Class->GetSuperClass() ? Class->GetSuperClass()->ClassWithin : UObject::StaticClass();
		if( !Class->ClassWithin->IsChildOf(ExpectedWithin) )
			appThrowf( TEXT("'within': within %s is not a generalization of superclass within '%s'"), Class->ClassWithin->GetName(), ExpectedWithin->GetName() );

		// Keep track of whether "config(ini)" was used.
		UBOOL DeclaresConfigFile = 0;

		// Class attributes.
		FToken Token;
		for( ; ; )
		{
			GetToken(Token);
			if( Token.Matches(NAME_Intrinsic) || Token.Matches(NAME_Native) )//oldver
			{
				// Note that this class has C++ code dependencies.
				if( Class->GetSuperClass() && !(Class->GetSuperClass()->GetFlags() & RF_Native) )
					appThrowf( TEXT("Native classes cannot expand non-native classes") );
				Class->SetFlags( RF_Native );

				// Parse an optional class header filename.
				if( MatchSymbol(TEXT("(")) )
				{
					FToken Token;
					if( !GetIdentifier(Token, 0) )
						appThrowf( TEXT("native: Missing native header filename") );
					Class->ClassHeaderFilename = Token.Identifier;
					RequireSymbol(TEXT(")"), TEXT("native") );
				}
			}
			else if( Token.Matches(NAME_NoExport) )
			{
				// Don't export to C++ header.
				Class->ClassFlags |= CLASS_NoExport;
			}
			else if( Token.Matches(NAME_EditInlineNew) )
			{
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
				// Allow the class to be placed in the editor.
				if ( Class->ClassFlags & CLASS_Placeable )
					Class->ClassFlags -= CLASS_Placeable;
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
                if( !GetIdentifier(Token, 0) )
                    appThrowf( TEXT("dependsOn: Missing dependent name") );
                RequireSymbol(TEXT(")"), TEXT("dependsOn") );
			}
			else if( Token.Matches(NAME_PerObjectConfig) )
			{
				// Don't export to C++ header.
				Class->ClassFlags |= CLASS_PerObjectConfig;
			}
			else if( Token.Matches(NAME_Abstract) )
			{
				// Hide all editable properties.
				Class->ClassFlags |= CLASS_Abstract;
			}
			else if ( Token.Matches(NAME_Deprecated) )
			{
				Class->ClassFlags |= CLASS_Deprecated;
			}
			else if( Token.Matches(NAME_Guid) )
			{
				// Get the class's GUID.
				RequireSymbol( TEXT("("), TEXT("'Guid'") );
					GetConstInt(*(INT*)&Class->ClassGuid.A);
				RequireSymbol( TEXT(","), TEXT("'Guid'") );
					GetConstInt(*(INT*)&Class->ClassGuid.B);
				RequireSymbol( TEXT(","), TEXT("'Guid'") );
					GetConstInt(*(INT*)&Class->ClassGuid.C);
				RequireSymbol( TEXT(","), TEXT("'Guid'") );
					GetConstInt(*(INT*)&Class->ClassGuid.D);
				RequireSymbol( TEXT(")"), TEXT("'Guid'") );
			}
			else if( Token.Matches(NAME_Transient) )
			{
				// Transient class.
				Class->ClassFlags |= CLASS_Transient;
			}
			else if( Token.Matches(NAME_Localized) )
			{
				// Localized class.
				//oldver
				appThrowf( TEXT("Class 'localized' keyword is no longer required") );
			}
			else if( Token.Matches(NAME_Config) )
			{
				// Transient class.
				if( MatchSymbol(TEXT("(")) )
				{
					FToken Token;
					if( !GetIdentifier(Token, 0) )
						appThrowf( TEXT("config: Missing configuration name") );
					Class->ClassConfigName = Token.Identifier;
					RequireSymbol(TEXT(")"), TEXT("config") );
					DeclaresConfigFile = 1;
				}
				else
					appThrowf( TEXT("config: Missing configuration name") );
			}
			else if( Token.Matches(NAME_SafeReplace) )
			{
				// Safely replaceable.
				Class->ClassFlags |= CLASS_SafeReplace;
			}
			else if( Token.Matches(NAME_HideCategories) )
			{
				RequireSymbol( TEXT("("), TEXT("'HideCategories'") );
				do
				{
					FToken Category;
					if( !GetIdentifier( Category, 1 ) )	
						appThrowf( TEXT("HideCategories: Expected category name") );
					Class->HideCategories.AddItem( FName( Category.Identifier ) );
				}
				while( MatchSymbol(TEXT(",")) );
				RequireSymbol( TEXT(")"), TEXT("'HideCategories'") );
			}
			else if( Token.Matches(NAME_AutoExpandCategories) )
			{
				RequireSymbol( TEXT("("), TEXT("'AutoExpandCategories'") );
				do
				{
					FToken Category;
					if( !GetIdentifier( Category, 1 ) )	
						appThrowf( TEXT("AutoExpandCategories: Expected category name") );
					Class->AutoExpandCategories.AddItem( FName( Category.Identifier ) );
				}
				while( MatchSymbol(TEXT(",")) );
				RequireSymbol( TEXT(")"), TEXT("'AutoExpandCategories'") );
			}
			else if( Token.Matches(NAME_ShowCategories) )
			{
				RequireSymbol( TEXT("("), TEXT("'ShowCategories'") );
				do
				{
					FToken Category;
					if( !GetIdentifier( Category, 1 ) )	
						appThrowf( TEXT("ShowCategories: Expected category name") );
					Class->HideCategories.RemoveItem( FName( Category.Identifier ) );
				}
				while( MatchSymbol(TEXT(",")) );
				RequireSymbol( TEXT(")"), TEXT("'ShowCategories'") );
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
			else
			{
				UngetToken(Token);
				break;
			}
		}
		// Validate.
		if( (Class->ClassFlags&CLASS_NoExport) && !(Class->GetFlags()&RF_Native) )
			appThrowf( TEXT("'noexport': Only valid for native classes") );

		// Invalidate config name if not specifically declared.
		if( !DeclaresConfigFile )
			Class->ClassConfigName = NAME_None;

		// Get semicolon.
		RequireSymbol( TEXT(";"), TEXT("'Class'") );
		NeedSemicolon=0;

		// Init variables.
		Class->Script.Empty();
		Class->Children			= NULL;
		Class->Next				= NULL;
		Class->ProbeMask        = 0;
		Class->IgnoreMask       = 0;
		Class->StateFlags       = 0;
		Class->LabelTableOffset = 0;
		Class->NetFields.Empty();

		// Make visible outside the package.
		Class->ClearFlags( RF_Transient );
		check(Class->GetFlags()&RF_Public);
		check(Class->GetFlags()&RF_Standalone);

		// Setup initial package imports to include the packages and the package imports
		// of all base classes.
		Class->PackageImports.Empty();
		for( UClass* C = Class; C; C=C->GetSuperClass() )
		{
			Class->PackageImports.AddUniqueItem( C->GetOuter()->GetFName() );
			for( INT i=0; i<C->PackageImports.Num(); i++ )
				Class->PackageImports.AddUniqueItem( C->PackageImports( i ) );
		}

		// Copy properties from parent class.
		Class->Defaults.Empty();
		if( Class->GetSuperClass() )
		{
			Class->SetPropertiesSize( Class->GetSuperClass()->GetPropertiesSize() );
			Class->Defaults = Class->GetSuperClass()->Defaults;
		}

		// Push the class nesting.
		PushNest( NEST_Class, Class->GetName(), NULL );
	}
	else if( Token.Matches(NAME_Import) )
	{
		CheckAllow( TEXT("'Uses'"), ALLOW_VarDecl );
		if( TopNest->NestType != NEST_Class )
			appThrowf( TEXT("'Uses' is are only allowed at class scope") );

		// Get thing to import.
		FToken ImportThing;
		GetToken( ImportThing );
		if( !ImportThing.Matches(NAME_Enum) && !ImportThing.Matches(NAME_Package) )
			appThrowf( TEXT("'import': Missing 'enum', 'struct', or 'package'") );

		// Get name to import.
		FToken ImportName;
		if( !GetIdentifier(ImportName) )
			appThrowf( TEXT("'import': Missing package, enum, or struct name to import") );

		// Handle package, enum, or struct.
		if( ImportThing.Matches(NAME_Package) )
		{
			// Import a package.
			Class->PackageImports.AddUniqueItem( FName(ImportName.Identifier) );
		}
		else if( ImportThing.Matches(NAME_Enum) )
		{
			// From package.
			UPackage* Pkg = CastChecked<UPackage>(Class->GetOuter());
			if( MatchIdentifier(NAME_From) )
			{
				FToken ImportPackage;
				if(	GetIdentifier( ImportPackage ) )
				{
					Pkg = FindObject<UPackage>( NULL, ImportPackage.Identifier );
					if( !Pkg )
						appThrowf( TEXT("'Uses': Unrecognized package '%s'"), ImportPackage.Identifier );
				}
			}
			else appThrowf( TEXT("'Uses': Unrecognized '%s'"), ImportThing.Identifier );
			new( Pkg, ImportName.Identifier )UEnum( NULL );
		}

	}
	else if
	(	Token.Matches(NAME_Function)
	||	Token.Matches(NAME_Operator)
	||	Token.Matches(NAME_PreOperator)
	||	Token.Matches(NAME_PostOperator)
	||	Token.Matches(NAME_Intrinsic) //oldver
	||	Token.Matches(NAME_Native)
	||	Token.Matches(NAME_Final)
	||	Token.Matches(NAME_Private)
	||	Token.Matches(NAME_Protected)
	||	Token.Matches(NAME_Public)
	||	Token.Matches(NAME_Latent)
	||	Token.Matches(NAME_Iterator)
	||	Token.Matches(NAME_Singular)
	||	Token.Matches(NAME_Static)
	||	Token.Matches(NAME_Exec)
	||  Token.Matches(NAME_Delegate)
	|| (Token.Matches(NAME_Event) && (TopNest->Allow & ALLOW_Function) )
	|| (Token.Matches(NAME_Simulated) && !PeekIdentifier(NAME_State)) )
	{
		// Function or operator.
		const TCHAR* NestName = NULL;
		FRetryPoint FuncNameRetry;
		FFuncInfo FuncInfo;
		FuncInfo.FunctionFlags = FUNC_Public;
		UStruct* Scope = TopNode;

		// Process all specifiers.
		for( ;; )
		{
			InitRetry(FuncNameRetry);
			if( Token.Matches(NAME_Function) )
			{
				// Get function name.
				CheckAllow( TEXT("'Function'"),ALLOW_Function);
				NestName = TEXT("function");
			}
			else if( Token.Matches(NAME_Operator) )
			{
				// Get operator name or symbol.
				CheckAllow( TEXT("'Operator'"), ALLOW_Function );
				NestName = TEXT("operator");
				FuncInfo.FunctionFlags |= FUNC_Operator;
				FuncInfo.ExpectParms = 3;

				if( !MatchSymbol(TEXT("(")) )
					appThrowf( TEXT("Missing '(' and precedence after 'Operator'") );
				else if( !GetConstInt(FuncInfo.Precedence) )
					appThrowf( TEXT("Missing precedence value") );
				else if( FuncInfo.Precedence<0 || FuncInfo.Precedence>255 )
					appThrowf( TEXT("Bad precedence value") );
				else if( !MatchSymbol(TEXT(")")) )
					appThrowf( TEXT("Missing ')' after operator precedence") );
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
			else if( Token.Matches(NAME_Intrinsic) || Token.Matches(NAME_Native) )//oldver
			{
				// Get internal id.
				FuncInfo.FunctionFlags |= FUNC_Native;
				if( MatchSymbol(TEXT("(")) )
				{
					if( !GetConstInt(FuncInfo.iNative) )
						appThrowf( TEXT("Missing native id") );
					else if( !MatchSymbol(TEXT(")")) )
						appThrowf( TEXT("Missing ')' after internal id") );
				}
			}
			else if( Token.Matches(NAME_Event) )
			{
				CheckAllow( TEXT("'Function'"), ALLOW_Function );
				NestName = TEXT("event");
				FuncInfo.FunctionFlags |= FUNC_Event;
			}
			else if( Token.Matches(NAME_Static) )
			{
				FuncInfo.FunctionFlags |= FUNC_Static;
				if( TopNode->GetClass()==UState::StaticClass() )
					appThrowf( TEXT("Static functions cannot exist in a state") );
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
			}
			else if( Token.Matches(NAME_Delegate) )
			{
				CheckAllow( TEXT("'Function'"),ALLOW_Function);
				NestName = TEXT("delegate");
				FuncInfo.FunctionFlags |= FUNC_Delegate;
			}
			else if( Token.Matches(NAME_Final) )
			{
				// This is a final (prebinding, non-overridable) function or operator.
				FuncInfo.FunctionFlags |= FUNC_Final;
			}
			else if( Token.Matches(NAME_Private) )
			{
				FuncInfo.FunctionFlags &= ~FUNC_Public;
				FuncInfo.FunctionFlags |= FUNC_Private;
			}
			else if( Token.Matches(NAME_Protected) )
			{
				FuncInfo.FunctionFlags &= ~FUNC_Public;
				FuncInfo.FunctionFlags |= FUNC_Protected;
			}
			else if( Token.Matches(NAME_Public) )
			{
				FuncInfo.FunctionFlags |= FUNC_Public;
			}
			else break;
			GetToken(Token);
		}
		UngetToken(Token);

		// Make sure we got a function.
		if( !NestName )
			appThrowf( TEXT("Missing 'function'") );

		// Warn if native doesn't actually exist.
#if CHECK_NATIVE_MATCH
		if( FuncInfo.iNative!=0 )
			if( FuncInfo.iNative<EX_FirstNative || FuncInfo.iNative>EX_Max || GNatives[FuncInfo.iNative]==execUndefined )
				appThrowf( TEXT("Bad native function id %i\r\n"),FuncInfo.iNative);
#endif

		// Get return type.
		FRetryPoint Start; InitRetry(Start);
		DWORD ObjectFlags = 0;
		FPropertyBase ReturnType( CPT_None );
		FToken TestToken;
		UBOOL HasReturnValue = 0;
		if( GetIdentifier(TestToken,1) )
		{
			if( !PeekSymbol(TEXT("(")) )
			{
				PerformRetry( Start );
				HasReturnValue = GetVarType( TopNode, ReturnType, ObjectFlags, ~0, NULL );
			}
			else PerformRetry( Start );
		}

		// Get function or operator name.
		if( !GetIdentifier(FuncInfo.Function) && (!(FuncInfo.FunctionFlags&FUNC_Operator) || !GetSymbol(FuncInfo.Function)) )
			appThrowf( TEXT("Missing %s name"), NestName );
		if( !MatchSymbol(TEXT("(")) )
			appThrowf( TEXT("Bad %s definition"), NestName );

		// Validate flag combinations.
		if( FuncInfo.FunctionFlags & FUNC_Native )
		{
			if( FuncInfo.iNative && !(FuncInfo.FunctionFlags & FUNC_Final) )
				appThrowf( TEXT("Numbered native functions must be final") );
		}
		else
		{
			if( FuncInfo.FunctionFlags & FUNC_Latent )
				appThrowf( TEXT("Only native functions may use 'Latent'") );
			if( FuncInfo.FunctionFlags & FUNC_Iterator )
				appThrowf( TEXT("Only native functions may use 'Iterator'") );
		}

		// If operator, figure out the function signature.
		TCHAR Signature[NAME_SIZE]=TEXT("");
		if( FuncInfo.FunctionFlags & FUNC_Operator )
		{
			// Name.
			const TCHAR* In = FuncInfo.Function.Identifier;
			while( *In )
				appStrncat( Signature, CppTags[*In++-32], NAME_SIZE );

			// Parameter signature.
			FRetryPoint Retry;
			InitRetry( Retry );
			if( !MatchSymbol(TEXT(")")) )
			{
				// Parameter types.
				appStrncat( Signature, TEXT("_"), NAME_SIZE );
				if( FuncInfo.FunctionFlags & FUNC_PreOperator )
					appStrncat( Signature, TEXT("Pre"), NAME_SIZE );
				do
				{
					// Get parameter type.
					FPropertyBase Property(CPT_None);
					DWORD ObjectFlags;
					GetVarType( TopNode, Property, ObjectFlags, ~CPF_ParmFlags, TEXT("Function parameter") );
					GetVarNameAndDim( TopNode, Property, ObjectFlags, 0, 1, NULL, TEXT("Function parameter"), NAME_None, 1 );

					// Add to signature.
					if
					(	Property.Type==CPT_ObjectReference
					||	Property.Type==CPT_Struct )
					{
						appStrncat( Signature, Property.PropertyClass->GetName(), NAME_SIZE );
					}
					else
					{
						TCHAR Temp[NAME_SIZE];
						appStrcpy( Temp, *FName((EName)Property.Type) );
						if( appStrstr( Temp, TEXT("Property") ) )
							*appStrstr( Temp, TEXT("Property") ) = 0;
						appStrncat( Signature, Temp, NAME_SIZE );
					}
				} while( MatchSymbol(TEXT(",")) );
				RequireSymbol( TEXT(")"), TEXT("parameter list") );
			}
			PerformRetry( Retry, 1, 1 );
		}
		else
		{
			appStrcpy( Signature, FuncInfo.Function.Identifier );
		}

		// Allocate local property frame, push nesting level and verify
		// uniqueness at this scope level.
		PushNest( NEST_Function, Signature, NULL );
		UFunction* TopFunction = ((UFunction*)TopNode);
		TopFunction->FunctionFlags  |= FuncInfo.FunctionFlags;
		TopFunction->OperPrecedence  = FuncInfo.Precedence;
		TopFunction->iNative         = FuncInfo.iNative;
		TopFunction->FriendlyName    = FName( FuncInfo.Function.Identifier, FNAME_Add );

		// Get parameter list.
		if( !MatchSymbol(TEXT(")")) )
		{
			UBOOL Optional=0;
			do
			{
				// Get parameter type.
				FPropertyBase Property(CPT_None);
				DWORD ObjectFlags;
				GetVarType( TopNode, Property, ObjectFlags, ~CPF_ParmFlags, TEXT("Function parameter") );
				Property.PropertyFlags |= CPF_Parm;
				UProperty* Prop = GetVarNameAndDim( TopNode, Property, ObjectFlags, 0, 1, NULL, TEXT("Function parameter"), NAME_None, 0 );
				TopFunction->NumParms++;

				// Check parameters.
				if( (FuncInfo.FunctionFlags & FUNC_Operator) && (Property.PropertyFlags & ~CPF_ParmFlags) )
					appThrowf( TEXT("Operator parameters may not have modifiers") );
				else if( Property.Type==CPT_Bool && (Property.PropertyFlags & CPF_OutParm) )
					appThrowf( TEXT("Booleans may not be out parameters") );
				else if
				(	(Property.PropertyFlags & CPF_SkipParm)
				&&	(!(TopFunction->FunctionFlags&FUNC_Native) || !(TopFunction->FunctionFlags&FUNC_Operator) || TopFunction->NumParms!=2) )
					appThrowf( TEXT("Only parameter 2 of native operators may be 'Skip'") );

				// Default value.
				if( MatchSymbol( TEXT("=") ) )
				{
					Prop->PropertyFlags |= CPF_OptionalParm;
				}

				// Handle optionality.
				if( Prop->PropertyFlags & CPF_OptionalParm )
					Optional = 1;
				else if( Optional )
					appThrowf( TEXT("After an optional parameters, all other parmeters must be optional") );
			} while( MatchSymbol(TEXT(",")) );
			RequireSymbol( TEXT(")"), TEXT("parameter list") );
		}

		// Get return type, if any.
		if( HasReturnValue )
		{
			ReturnType.PropertyFlags |= CPF_Parm | CPF_OutParm | CPF_ReturnParm;
			GetVarNameAndDim( TopNode, ReturnType, ObjectFlags, 1, 1, TEXT("ReturnValue"), TEXT("Function return type"), NAME_None, 0 );
			TopFunction->NumParms++;
		}

		// Check overflow.
		if( TopFunction->NumParms > MAX_FUNC_PARMS )
			appThrowf( TEXT("'%s': too many parameters"), TopNode->GetName() );

		// For operators, verify that: the operator is either binary or unary, there is
		// a return value, and all parameters have the same type as the return value.
		if( FuncInfo.FunctionFlags & FUNC_Operator )
		{
			INT n = TopFunction->NumParms;
			if( n != FuncInfo.ExpectParms )
				appThrowf( TEXT("%s must have %i parameters"), NestName, FuncInfo.ExpectParms-1 );

			if( !TopFunction->GetReturnProperty() )
				appThrowf( TEXT("Operator must have a return value") );

			if( !(FuncInfo.FunctionFlags & FUNC_Final) )
				appThrowf( TEXT("Operators must be declared as 'Final'") );
		}

		// Make new UDelegateProperty for delegate
		if( TopFunction->FunctionFlags & FUNC_Delegate )
		{
			UProperty* Prev=NULL;
			for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Scope); It && It.GetStruct()==Scope; ++It )
			{
				Prev = *It;
			}

			// check for any delegate properties that reference this delegate
			for (TObjectIterator<UDelegateProperty> It; It; ++It)
			{
				UDelegateProperty *delegateProp = Cast<UDelegateProperty>(*It);
				if (delegateProp != NULL &&
					delegateProp->Function == NULL &&
					delegateProp->DelegateName == TopFunction->FriendlyName)
				{
					// and fix up the function reference
					delegateProp->Function = TopFunction;
					debugf(TEXT("Fixed up delegate reference %s to %s"),delegateProp->GetName(),*TopFunction->FriendlyName);
				}
			}

			FName PropName = FName(*FString::Printf(TEXT("__%s__Delegate"),*TopFunction->FriendlyName), FNAME_Add);
			UProperty* NewProperty = new(Scope, PropName, RF_Public)UDelegateProperty;
			Cast<UDelegateProperty>(NewProperty)->Function =  TopFunction;

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

		// Detect whether the function is being defined or declared.
		if( PeekSymbol(TEXT(";")) )
		{
			// Function is just being declared, not defined.
			check( (TopFunction->FunctionFlags & FUNC_Defined)==0 );
		}
		else
		{
			// Function is being defined.
			TopFunction->FunctionFlags |= FUNC_Defined;
			if( TopFunction->FunctionFlags & FUNC_Native )
				appThrowf( TEXT("Native functions may only be declared, not defined") );

			// Require bracket.
			RequireSymbol( TEXT("{"), NestName );
			
			NeedSemicolon=0;
		}

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
					TopNode->SuperField = NULL;
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
						appThrowf( TEXT("Overloaded operator differs in precedence") );

					// See if all parameters match.
					if
					(	TopFunction->NumParms!=Function->NumParms
					||	(!TopFunction->GetReturnProperty())!=(!Function->GetReturnProperty()) )
						appThrowf( TEXT("Redefinition of '%s %s' differs from original"), NestName, FuncInfo.Function.Identifier );

					// Check all individual parameters.
					INT Count=0;
					for( TFieldIterator<UProperty,CLASS_IsAUProperty> It1(TopFunction),It2(*Function); Count<Function->NumParms; ++It1,++It2,++Count )
					{
						if( !FPropertyBase(*It1).MatchesType(FPropertyBase(*It2), 1) )
						{
							if( It1->PropertyFlags & CPF_ReturnParm )
								appThrowf( TEXT("Redefinition of %s %s differs only by return type"), NestName, FuncInfo.Function.Identifier );
							else if( !(FuncInfo.FunctionFlags & FUNC_Operator) )
								appThrowf( TEXT("Redefinition of '%s %s' differs from original"), NestName, FuncInfo.Function.Identifier );
							break;
						}
					}
					if( Count<TopFunction->NumParms )
						continue;

					// Function flags to copy from parent.
					FuncInfo.FunctionFlags |= (Function->FunctionFlags & FUNC_FuncInherit);

					// Balk if required specifiers differ.
					if( (Function->FunctionFlags&FUNC_FuncOverrideMatch) != (FuncInfo.FunctionFlags&FUNC_FuncOverrideMatch) )
						appThrowf( TEXT("Function '%s' specifiers differ from original"), Function->GetName() );

					// Are we overriding a function?
					if( TopNode==Function->GetOuter() )
					{
						// Duplicate.
						PerformRetry( FuncNameRetry );
						appThrowf( TEXT("Duplicate function '%s'"), Function->GetName() );
					}
					else
					{
						// Overriding an existing function.
						if( Function->FunctionFlags & FUNC_Final )
						{
							PerformRetry(FuncNameRetry);
							appThrowf( TEXT("%s: Can't override a 'final' function"), Function->GetName() );
						}
					}

					// Here we have found the original.
					TopNode->SuperField = *Function;
					goto Found;
				}
			}
		}
		Found:

		// Bind the function.
		TopFunction->Bind();

		// If declaring a function, end the nesting.
		if( !(TopFunction->FunctionFlags & FUNC_Defined) )
			PopNest( NEST_Function, NestName );

	}
	else if( Token.Matches(NAME_Const) )
	{
		CompileConst( Class );
	}
	else if( Token.Matches(NAME_Var) || Token.Matches(NAME_Local) )
	{
		// Variable definition.
		QWORD Disallow;
		if( Token.Matches(NAME_Var) )
		{
			// Declaring per-object variables.
			CheckAllow( TEXT("'Var'"), ALLOW_VarDecl );
			if( TopNest->NestType != NEST_Class )
				appThrowf( TEXT("Instance variables are only allowed at class scope (use 'local'?)") );
			Disallow = CPF_ParmFlags;
		}
		else
		{
			// Declaring local variables.
			CheckAllow( TEXT("'Local'"), ALLOW_VarDecl );
			if( TopNest->NestType == NEST_Class )
				appThrowf( TEXT("Local variables are only allowed in functions") );
			Disallow	= ~0;
		}

		// Get category, if any.
		FName EdCategory = NAME_None;
		QWORD EdFlags    = 0;
		if( MatchSymbol(TEXT("(")) )
		{
			// Get optional property editing category.
			EdFlags |= CPF_Edit;
			FToken Category;
			if( GetIdentifier( Category, 1 ) )	EdCategory = FName( Category.Identifier );
			else								EdCategory = Class->GetFName();
			
			if( !MatchSymbol(TEXT(")")) )
				appThrowf( TEXT("Missing ')' after editable category") );
		}

		// Compile the variable type.		
		FPropertyBase OriginalProperty(CPT_None);
		DWORD ObjectFlags=0;
		GetVarType( TopNode, OriginalProperty, ObjectFlags, Disallow, TEXT("Variable declaration") );
		OriginalProperty.PropertyFlags |= EdFlags;

		// If editable but no category was specified, the category name is our class name.
		if( (OriginalProperty.PropertyFlags & CPF_Edit) && (EdCategory==NAME_None) )
			EdCategory = Class->GetFName();

		// Validate combinations.
		if( (OriginalProperty.PropertyFlags & (CPF_Transient|CPF_Native)) && TopNest->NestType!=NEST_Class )
			appThrowf( TEXT("Static and local variables may not be transient or native") );
		if( OriginalProperty.PropertyFlags & CPF_ParmFlags )
			appThrowf( TEXT("Illegal type modifiers in variable") );

		// Process all variables of this type.
		do
		{
            UProperty* newProperty;
			FPropertyBase Property = OriginalProperty;
			newProperty = GetVarNameAndDim( TopNode, Property, ObjectFlags, 0, 0, NULL, TEXT("Variable declaration"), EdCategory, 0 );
            if( newProperty )
            {
                UnReferencedLocal *newLocal = new UnReferencedLocal;
                newLocal->next = unReferencedLocals;
                newLocal->property = newProperty;
                newLocal->declarationLine = InputLine;
                unReferencedLocals = newLocal;
            }
		} while( MatchSymbol(TEXT(",")) );
		
		// Propagate alignment restrictions.
		if( Token.Matches(NAME_Var) && (OriginalProperty.Type == CPT_Struct) && OriginalProperty.Struct && (OriginalProperty.ArrayDim != 0) )
			Class->MinAlignment = Max( Class->MinAlignment, OriginalProperty.Struct->MinAlignment );
	}
	else if( Token.Matches(NAME_Enum) )
	{
		// Enumeration definition.
		CheckAllow( TEXT("'Enum'"), ALLOW_VarDecl );

		// Compile enumeration.
		CompileEnum( Class );

	}
	else if( Token.Matches(NAME_Struct) )
	{
		// Struct definition.
		CheckAllow( TEXT("'struct'"), ALLOW_VarDecl );

		// Compile struct.
		CompileStruct( Class );
	}
	else if
	(	Token.Matches(NAME_State)
	||	Token.Matches(NAME_Auto)
	||	Token.Matches(NAME_Simulated) )
	{
		// State block.
		check(TopNode!=NULL);
		CheckAllow( TEXT("'State'"), ALLOW_State );
		DWORD StateFlags=0, GotState=0;

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
			appThrowf( TEXT("Missing 'State'") );

		// Get name and default parent state.
		FToken NameToken;
		if( !GetIdentifier(NameToken) )
			appThrowf( TEXT("Missing state name") );
		UState* ParentState = Cast<UState>(FindField( TopNode, NameToken.Identifier, UState::StaticClass(), TEXT("'state'") ));
		if( ParentState && ParentState->GetOwnerClass()==Class )
			appThrowf( TEXT("Duplicate state '%s'"), NameToken.Identifier );

		// Check for 'extends' keyword.
		if( MatchIdentifier(NAME_Extends) )
		{
			FToken ParentToken;
			if( ParentState )
				appThrowf( TEXT("'Extends' not allowed here: state '%s' overrides version in parent class"), NameToken.Identifier );
			if( !GetIdentifier(ParentToken) )
				appThrowf( TEXT("Missing parent state name") );
			ParentState = Cast<UState>(FindField( TopNode, ParentToken.Identifier, UState::StaticClass(), TEXT("'state'") ));
			if( !ParentState )
				appThrowf( TEXT("'extends': Parent state '%s' not found"), ParentToken.Identifier );
		}

		// Begin the state block.
		PushNest( NEST_State, NameToken.Identifier, NULL );
		UState* State = CastChecked<UState>( TopNode );
		State->StateFlags |= StateFlags;
		State->SuperField = ParentState;
		RequireSymbol( TEXT("{"), TEXT("'State'") );
		NeedSemicolon=0;
		SupressDebugInfo = -1;
	}
	else if( Token.Matches(NAME_Ignores) )
	{
		// Probes to ignore in this state.
		CheckAllow( TEXT("'Ignores'"), ALLOW_Ignores );
		for( ; ; )
		{
			FToken IgnoreFunction;
			if( !GetToken(IgnoreFunction) )
				appThrowf( TEXT("'Ignores': Missing probe function name") );
			if
			(	IgnoreFunction.TokenName==NAME_None
			||	IgnoreFunction.TokenName.GetIndex() <  NAME_PROBEMIN
			||	IgnoreFunction.TokenName.GetIndex() >= NAME_PROBEMAX )
			{
				for( INT i=NestLevel-2; i>=1; i-- )
				{
					for( TFieldIterator<UFunction> Function(Nest[i].Node); Function; ++Function )
					{
						if( Function->GetFName()==IgnoreFunction.TokenName )
						{
							// Verify that function is ignoreable.
							if( Function->FunctionFlags & FUNC_Final )
								appThrowf( TEXT("'%s': Cannot ignore final functions"), Function->GetName() );

							// Insert empty function definition to intercept the call.
							PushNest( NEST_Function, Function->GetName(), NULL );
							UFunction* TopFunction = ((UFunction*) TopNode );
							TopFunction->FunctionFlags    |= (Function->FunctionFlags & FUNC_FuncOverrideMatch);
							TopFunction->NumParms          = Function->NumParms;
							TopFunction->SuperField        = *Function;

							// Copy parameters.
							UField** PrevLink = &TopFunction->Children;
							check(*PrevLink==NULL);
							for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(*Function); It && (It->PropertyFlags & CPF_Parm); ++It )
							{
								UProperty* NewProperty=NULL;
								if( It->IsA(UByteProperty::StaticClass()) )
								{
									NewProperty = new(TopFunction,It->GetName(),RF_Public)UByteProperty;
									Cast<UByteProperty>(NewProperty)->Enum = Cast<UByteProperty>(*It)->Enum;
								}
								else if( It->IsA(UIntProperty::StaticClass()) )
								{
									NewProperty = new(TopFunction,It->GetName(),RF_Public)UIntProperty;
								}
								else if( It->IsA(UBoolProperty::StaticClass()) )
								{
									NewProperty = new(TopFunction,It->GetName(),RF_Public)UBoolProperty;
								}
								else if( It->IsA(UFloatProperty::StaticClass()) )
								{
									NewProperty = new(TopFunction,It->GetName(),RF_Public)UFloatProperty;
								}
								else if( It->IsA(UClassProperty::StaticClass()) )
								{
									NewProperty = new(TopFunction,It->GetName(),RF_Public)UClassProperty;
									Cast<UObjectProperty>(NewProperty)->PropertyClass = Cast<UObjectProperty>(*It)->PropertyClass;
									Cast<UClassProperty>(NewProperty)->MetaClass = Cast<UClassProperty>(*It)->MetaClass;
								}
								else if( It->IsA(UObjectProperty::StaticClass()) )
								{
									NewProperty = new(TopFunction,It->GetName(),RF_Public)UObjectProperty;
									Cast<UObjectProperty>(NewProperty)->PropertyClass = Cast<UObjectProperty>(*It)->PropertyClass;
								}
								else if( It->IsA(UNameProperty::StaticClass()) )
								{
									NewProperty = new(TopFunction,It->GetName(),RF_Public)UNameProperty;
								}
								else if( It->IsA(UStrProperty::StaticClass()) )
								{
									NewProperty = new(TopFunction,It->GetName(),RF_Public)UStrProperty;
								}
								else if( It->IsA(UStructProperty::StaticClass()) )
								{
									NewProperty = new(TopFunction,It->GetName(),RF_Public)UStructProperty;
									Cast<UStructProperty>(NewProperty)->Struct = Cast<UStructProperty>(*It)->Struct;
								}
								else appErrorf( TEXT("Unknown property type %s"), It->GetClass()->GetName() );
								NewProperty->ArrayDim = It->ArrayDim;
								NewProperty->PropertyFlags = It->PropertyFlags;
								*PrevLink              = NewProperty;
								PrevLink = &(*PrevLink)->Next;
							}

							// Finish up.
							PopNest( NEST_Function, TEXT("Ignores") );
							goto FoundIgnore;
						}
					}
				}
				appThrowf( TEXT("'Ignores': '%s' is not a function"), IgnoreFunction.Identifier );
				FoundIgnore:;
			}
			else
			{
				// Ignore a probe function.
				UState* State = CastChecked<UState>( TopNode );
				State->IgnoreMask &= ~((QWORD)1 << (IgnoreFunction.TokenName.GetIndex() - NAME_PROBEMIN));
			}

			// More?
			if( !MatchSymbol(TEXT(",")) )
				break;
		}
	}
	else if( Token.Matches(NAME_Replication) )
	{
		// Network replication definition.
		if( TopNest->NestType != NEST_Class )
			appThrowf( TEXT("'Replication' is not allowed here") );
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
	else
	{
		// Not a declaration.
		return 0;
	}
	return 1;
}

//
// Compile a command in Token. Handles any errors that may occur.
//
void FScriptCompiler::CompileCommand( FToken& Token, UBOOL& NeedSemicolon )
{
	check(Pass==1);
	if( Token.Matches(NAME_Switch) )
	{
		// Switch.
		CheckAllow( TEXT("'Switch'"), ALLOW_Cmd );
		PushNest( NEST_Switch, TEXT(""), NULL );

		// Compile the select-expression.
		//!! Yoda Debugger
		EmitDebugInfo(DI_Switch);
		Writer << EX_Switch;
		FRetryPoint LowRetry; InitRetry(LowRetry);
		CompileExpr( FPropertyBase(CPT_None), TEXT("'Switch'"), &TopNest->SwitchType );
		if( TopNest->SwitchType.ArrayDim != 1 )
			appThrowf( TEXT("Can't switch on arrays") );
		FRetryPoint HighRetry; InitRetry(HighRetry);
		EmitSize( TopNest->SwitchType.GetSize(), TEXT("'Switch'") );//!!hardcoded size
		CodeSwitcheroo(LowRetry,HighRetry);
		TopNest->SwitchType.PropertyFlags &= ~(CPF_OutParm);

		// Get bracket.
		RequireSymbol( TEXT("{"), TEXT("'Switch'") );
		NeedSemicolon=0;

	}
	else if( Token.Matches(NAME_Case) )
	{
		CheckAllow( TEXT("'Class'"), ALLOW_Case );

		// Update previous Case's chain address.
		EmitChainUpdate(TopNest);

		// Insert this case statement and prepare to chain it to the next one.
		Writer << EX_Case;
		EmitAddressToChainLater(TopNest);
		CompileExpr( TopNest->SwitchType, TEXT("'Case'") );
		RequireSymbol( TEXT(":"), TEXT("'Case'") );
		NeedSemicolon=0;

		TopNest->Allow |= ALLOW_Cmd | ALLOW_Label | ALLOW_Break;
	}
	else if( Token.Matches(NAME_Default) && (TopNest->Allow & ALLOW_Case) )
	{
		// Default case.
		CheckAllow( TEXT("'Default'"), ALLOW_Case );
		
		// Update previous Case's chain address.
		EmitChainUpdate(TopNest);

		// Emit end-of-case marker.
		Writer << EX_Case;
		_WORD W=MAXWORD; Writer << W;
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
				break;
			else if( Nest[i].NestType==NEST_ForEach )
				Writer << EX_IteratorPop;
		}
		if( i <= 0 )
			appThrowf( TEXT("Internal consistency error on 'Return'") );
		UFunction* Function = CastChecked<UFunction>(Nest[i].Node);
		UProperty* Return = Function->GetReturnProperty();
		//!! Yoda Debugger
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
			FPropertyBase Property(Return);
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
		//!! Yoda Debugger
		EmitDebugInfo(DI_PrevStack);
	}
	else if( Token.Matches(NAME_If) )
	{
		// If.
		CheckAllow( TEXT("'If'"), ALLOW_Cmd );
		PushNest( NEST_If, TEXT(""), NULL );

		// Jump to next evaluator if expression is false.
		//!! Yoda Debugger
		EmitDebugInfo(DI_SimpleIf);
		Writer << EX_JumpIfNot;
		EmitAddressToChainLater(TopNest);

		// Compile boolean expr.
		RequireSymbol( TEXT("("), TEXT("'If'") );
		CompileExpr( FPropertyBase(CPT_Bool), TEXT("'If'") );
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
		TopNest->SetFixup(FIXUP_LoopStart,TopNode->Script.Num());

		// Evaluate expr and jump to end of loop if false.
		//!! Yoda Debugger
		EmitDebugInfo(DI_While);		
		Writer << EX_JumpIfNot;
		EmitAddressToFixupLater(TopNest,FIXUP_LoopEnd,NAME_None);

		// Compile boolean expr.
		RequireSymbol( TEXT("("), TEXT("'While'") );
		CompileExpr( FPropertyBase(CPT_Bool), TEXT("'While'") );
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

		TopNest->SetFixup(FIXUP_LoopStart,TopNode->Script.Num());

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

		//!! Yoda Debugger
		if( Nest[iNest].NestType == NEST_Switch )
		{
			EmitDebugInfo(DI_BreakSwitch);
		}
		else if ( Nest[iNest].NestType == NEST_ForEach )
		{
			EmitDebugInfo(DI_BreakForEach);
		}
		else if ( Nest[iNest].NestType == NEST_For )
		{
			EmitDebugInfo(DI_BreakFor);
		}
		else if ( Nest[iNest].NestType == NEST_Loop )
		{
			EmitDebugInfo(DI_BreakLoop);
		}


		// Jump to the loop's end.
		Writer << EX_Jump;
		if     ( Nest[iNest].NestType == NEST_Loop    ) EmitAddressToFixupLater( &Nest[iNest], FIXUP_LoopEnd,     NAME_None );
		else if( Nest[iNest].NestType == NEST_For     ) EmitAddressToFixupLater( &Nest[iNest], FIXUP_ForEnd,      NAME_None );
		else if( Nest[iNest].NestType == NEST_ForEach ) EmitAddressToFixupLater( &Nest[iNest], FIXUP_IteratorEnd, NAME_None );
		else if( Nest[iNest].NestType == NEST_Switch  ) EmitAddressToFixupLater( &Nest[iNest], FIXUP_SwitchEnd,   NAME_None );
		else                                            EmitAddressToFixupLater(TopNest,FIXUP_SwitchEnd,NAME_None);

	}
	else if( Token.Matches(NAME_Continue) )
	{
		CheckAllow( TEXT("'Continue'"), ALLOW_Continue );

		// Find the nearest For or Loop.
		INT iNest = FindNest(NEST_Loop);
		iNest     = Max(iNest,FindNest(NEST_For    ));
		iNest     = Max(iNest,FindNest(NEST_ForEach));
		check(iNest>0);

		//!! Yoda Debugger
		if ( Nest[iNest].NestType == NEST_ForEach )
		{
			EmitDebugInfo(DI_ContinueForeach);
		}
		else if ( Nest[iNest].NestType == NEST_For )
		{
			EmitDebugInfo(DI_ContinueFor);
		}
		else if ( Nest[iNest].NestType == NEST_Loop )
		{
			EmitDebugInfo(DI_ContinueLoop);
		}


		// Jump to the loop's start.
		if( Nest[iNest].NestType == NEST_Loop )
		{
			Writer << EX_Jump;
			EmitAddressToFixupLater( &Nest[iNest], FIXUP_LoopPostCond, NAME_None );
		}
		else if( Nest[iNest].NestType == NEST_For )
		{
			Writer << EX_Jump;
			EmitAddressToFixupLater( &Nest[iNest], FIXUP_ForInc, NAME_None );
		}
		else if( Nest[iNest].NestType == NEST_ForEach )
		{
			Writer << EX_IteratorNext;
			Writer << EX_Jump;
			EmitAddressToFixupLater( &Nest[iNest], FIXUP_IteratorEnd, NAME_None );
		}

	}
	else if(Token.Matches(NAME_For))
	{
		CheckAllow( TEXT("'For'"), ALLOW_Cmd );
		PushNest( NEST_For, TEXT(""), NULL );

		//!! Yoda Debugger
		// Compile for parms.
		RequireSymbol( TEXT("("), TEXT("'For'") );
			EmitDebugInfo(DI_ForInit);
			// Supress the EX_Let's debug info.
			SupressDebugInfo++;
			CompileAffector();
		RequireSymbol( TEXT(";"), TEXT("'For'") );
			TopNest->SetFixup(FIXUP_ForStart,TopNode->Script.Num());
			EmitDebugInfo(DI_ForEval);
			Writer << EX_JumpIfNot;
			EmitAddressToFixupLater(TopNest,FIXUP_ForEnd,NAME_None);
			CompileExpr( FPropertyBase(CPT_Bool), TEXT("'For'") );
		RequireSymbol( TEXT(";"), TEXT("'For'") );
		
			// Skip the increment expression text but not code.
		// We can't emit a debug info here for the for loop inc, we have to do it in popnest
			InitRetry(TopNest->ForRetry);
			CompileAffector();
			PerformRetry(TopNest->ForRetry,1,0);
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

		// Emit iterator token.
		Writer << EX_Iterator;

		// Compile the iterator expression.
		
		//!! Yoda Debugger
		// Suppress the standard EFP
		SupressDebugInfo++;
		FToken TypeToken;
		CompileExpr( FPropertyBase(CPT_None), TEXT("'ForEach'") );
		if( TopNest->Allow & ALLOW_Iterator )
			appThrowf( TEXT("'ForEach': An iterator expression is required") );

		// Emit end offset.
		EmitAddressToFixupLater( TopNest, FIXUP_IteratorEnd, NAME_None );

		//!! Yoda Debugger
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
		//!! Yoda Debugger
		EmitDebugInfo(DI_Assert);
		_WORD wLine = InputLine;
		Writer << EX_Assert;
		Writer << wLine;
		CompileExpr( FPropertyBase(CPT_Bool), TEXT("'Assert'") );
	}
	else if( Token.Matches(NAME_Goto) )
	{
		CheckAllow( TEXT("'Goto'"), ALLOW_Label );
		if( TopNest->Allow & ALLOW_StateCmd )
		{
			// Emit virtual state goto.
			Writer << EX_GotoLabel;
			CompileExpr( FPropertyBase(CPT_Name), TEXT("'Goto'") );
		}
		else
		{
			// Get label list for this nest level.
			INT iNest;
			for( iNest=NestLevel-1; iNest>=2; iNest-- )
				if( Nest[iNest].NestType==NEST_State || Nest[iNest].NestType==NEST_Function || Nest[iNest].NestType==NEST_ForEach )
					break;
			if( iNest < 2 )
				appThrowf( TEXT("Goto is not allowed here") );
			FNestInfo* LabelNest = &Nest[iNest];

			// Get label.
			FToken Label;
			if( !GetToken(Label) )
				appThrowf( TEXT("Goto: Missing label") );
			if( Label.TokenName == NAME_None )
				Label.TokenName = FName( Label.Identifier );
			if( Label.TokenName == NAME_None )
				appThrowf( TEXT("Invalid label '%s'"), Label.Identifier );

			// Emit final goto.
			Writer << EX_Jump;
			EmitAddressToFixupLater( LabelNest, FIXUP_Label, Label.TokenName );
		}
	}
	else if( Token.Matches(NAME_Stop) )
	{
		CheckAllow( TEXT("'Stop'"), ALLOW_StateCmd );
		EmitDebugInfo(DI_PrevStack);
		Writer << EX_Stop;
	}
	else if( Token.Matches(TEXT("}")) )
	{
		// End of block.
		if( TopNest->NestType==NEST_Class )
			appThrowf( TEXT("Unexpected '}' at class scope") );
		else if( TopNest->NestType==NEST_None )
			appThrowf( TEXT("Unexpected '}' at global scope") );
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
			Token.TokenName = FName( Token.Identifier );
		if( Token.TokenName == NAME_None )
			appThrowf( TEXT("Invalid label name '%s'"), Token.Identifier );

		// Handle first label in a state.
		if( !(TopNest->Allow & ALLOW_Cmd ) )
		{
			// This is the first label in a state, so set the code start and enable commands.
			check(TopNest->NestType==NEST_State);
			TopNest->Allow     |= ALLOW_Cmd;
			TopNest->Allow     &= ~(ALLOW_Function | ALLOW_VarDecl);
		}
		else if ( TopNest->Node->IsA(UState::StaticClass()) )
		{         
   			EmitDebugInfo(DI_PrevStackLabel);
		}

		// Get label list for this nest level.
		INT iNest;
		for( iNest=NestLevel-1; iNest>=2; iNest-- )
			if( Nest[iNest].NestType==NEST_State || Nest[iNest].NestType==NEST_Function || Nest[iNest].NestType==NEST_ForEach )
				break;
		if( iNest < 2 )
			appThrowf( TEXT("Labels are not allowed here") );
		FNestInfo *LabelNest = &Nest[iNest];

		// Make sure the label is unique here.
		for( FLabelRecord *LabelRec = LabelNest->LabelList; LabelRec; LabelRec=LabelRec->Next )
			if( LabelRec->Name == Token.TokenName )
				appThrowf( TEXT("Duplicate label '%s'"), *Token.TokenName );

		// Add label.
		LabelNest->LabelList = new(GMem)FLabelRecord( Token.TokenName, TopNode->Script.Num(), LabelNest->LabelList );
		NeedSemicolon=0;
		//!! Yoda Debugger
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
		if( Pass == 0 )
		{
			// Skip this and subsequent commands so we can hit them on next pass.
			if( NestLevel < 3 )
				appThrowf( TEXT("Unexpected '%s'"), Token.Identifier );
			UngetToken(Token);
			PopNest( TopNest->NestType, NestTypeName(TopNest->NestType) );
			SkipStatements( 1, NestTypeName(TopNest->NestType) );
			NeedSemicolon = 0;
		}
		else
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
				appThrowf( TEXT("Missing ';' before '%s'"), Token.Identifier );
			else
				appThrowf( TEXT("Missing ';'") );
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
	do CompileStatement();
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
	INT Size = Node->GetPropertiesSize() - Node->Defaults.Num();
	if( Size > 0 )
		Node->Defaults.AddZeroed( Size );

	// Handle functions.
	UFunction* ThisFunction = Cast<UFunction>( Node );
	if( ThisFunction )
	{
		ThisFunction->ParmsSize=0;
		for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(ThisFunction); It; ++It )
		{
			if( It->PropertyFlags & CPF_Parm )
				ThisFunction->ParmsSize = It->Offset + It->GetSize();
			if( It->PropertyFlags & CPF_ReturnParm )
				ThisFunction->ReturnValueOffset = It->Offset;
		}
	}

	// Accumulate probe masks based on all functions in this state.
	UState* ThisState = Cast<UState>( Node );
	if( ThisState )
	{
		for( TFieldIterator<UFunction> Function(ThisState); Function; ++Function )
		{
			if
			(	(Function->GetFName().GetIndex() >= NAME_PROBEMIN)
			&&	(Function->GetFName().GetIndex() <  NAME_PROBEMAX)
			&&  (Function->FunctionFlags & FUNC_Defined) )
				ThisState->ProbeMask |= (QWORD)1 << (Function->GetFName().GetIndex() - NAME_PROBEMIN);
		}
	}

	// Recurse with all child states in this class.
	for( TFieldIterator<UStruct> It(Node); It && It.GetStruct()==Node; ++It )
		PostParse( *It );
}

/*-----------------------------------------------------------------------------
	Code skipping.
-----------------------------------------------------------------------------*/

//
// Skip over code, honoring { and } pairs.
//
void FScriptCompiler::SkipStatements( int SubCount, const TCHAR* ErrorTag  )
{
	FToken Token;
	while( SubCount>0 && GetToken( Token, NULL, 1 ) )
	{
		if		( Token.Matches(TEXT("{")) ) SubCount++;
		else if	( Token.Matches(TEXT("}")) ) SubCount--;
	}
	if( SubCount > 0 )
		appThrowf( TEXT("Unexpected end of file at end of %s"), ErrorTag );
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
		NewNest = NEST_Function;
	else if( Node == Class )
		NewNest = NEST_Class;
	else if( Node->IsA(UState::StaticClass()) )
		NewNest = NEST_State;
	check(NewNest!=NEST_None);
	PushNest( NewNest, Node->GetName(), Node );
	check(TopNode==Node);
	TopNode->Script.Empty();

	// Propagate function replication flags down, since they aren't known until the second pass.
	UFunction* TopFunction = Cast<UFunction>( TopNode );
	if( TopFunction && TopFunction->GetSuperFunction() )
	{
		TopFunction->FunctionFlags &= ~FUNC_NetFuncFlags;
		TopFunction->FunctionFlags |= (TopFunction->GetSuperFunction()->FunctionFlags & FUNC_NetFuncFlags);
	}

	// If compiling the class node and an input line is specified, it's the replication defs.
	if( Node==Class && Node->Line!=INDEX_NONE )
	{
		// Remember input positions.
		InputPos  = PrevPos  = Node->TextPos;
		InputLine = PrevLine = Node->Line;

		// Compile all replication defs.
		while( 1 )
		{
			// Get Reliable or Unreliable.
			QWORD PropertyFlags = CPF_Net;
			DWORD FunctionFlags = FUNC_Net;
			FToken Token;
			GetToken( Token );
			if( Token.Matches( TEXT("}") ) )
			{
				break;
			}
			else if( Token.Matches(NAME_Reliable) )
			{
				FunctionFlags |= FUNC_NetReliable;
			}
			else if( !Token.Matches(NAME_Unreliable) )
			{
				appThrowf( TEXT("Missing 'Reliable' or 'Unreliable'") );
			}

			// Compile conditional expression.
			RequireIdentifier( NAME_If, TEXT("Replication statement") );
			RequireSymbol( TEXT("("), TEXT("Replication condition") );
			_WORD RepOffset = TopNode->Script.Num();
			CompileExpr( FPropertyBase(CPT_Bool), TEXT("Replication condition") );
			RequireSymbol( TEXT(")"), TEXT("Replication condition") );

			// Compile list of variables defined in this class and hook them into the
			// replication conditions.
			do
			{
				// Get variable name.
				FToken VarToken;
				if( !GetIdentifier(VarToken) )
					appThrowf( TEXT("Missing variable name in replication definition") );
				FName VarName = FName( VarToken.Identifier, FNAME_Find );
				if( VarName == NAME_None )
					appThrowf( TEXT("Unrecognized variable '%s' name in replication definition"), VarToken.Identifier );

				// Find variable.
				UBOOL Found=0;
				for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Class); It && It.GetStruct()==Class; ++It )
				{
					if( It->GetFName()==VarName )
					{
						// Found it, so make sure it's replicatable.
						if( It->PropertyFlags & CPF_Net )
							appThrowf( TEXT("Variable '%s' already has a replication definition"), *VarName );

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
							// Found it, so make sure it's replicable.
							if( Function->GetSuperFunction() )
								appThrowf( TEXT("Function '%s' is defined in base class '%s'"), *VarName, Function->GetSuperFunction()->GetOwnerClass()->GetName() );
							if( Function->FunctionFlags & FUNC_Net )
								appThrowf( TEXT("Function '%s' already has a replication definition"), *VarName );
							if( (Function->FunctionFlags&FUNC_Native) && (Function->FunctionFlags&FUNC_Final) )
								appThrowf( TEXT("Native final functions may not be replicated") );

							// Set its properties.
							Function->FunctionFlags  |= FunctionFlags;
							Function->RepOffset       = RepOffset;
							Found                     = 1;
							break;
						}
					}
				}
				if( !Found )
					appThrowf( TEXT("Bad variable or function '%s' in replication definition"), *VarName );
			} while( MatchSymbol( TEXT(",") ) );

			// Semicolon.
			RequireSymbol( TEXT(";"), TEXT("Replication definition") );
		}
	}

	// Compile all child functions in this class or state.
	for( TFieldIterator<UStruct> Child(Node); Child && Child.GetStruct()==Node; ++Child )
		if( Child->GetClass() != UStruct::StaticClass() )
			CompileSecondPass( *Child );
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
		UFunction* TopFunction = Cast<UFunction>( TopNode );
		UState*    TopState    = Cast<UState   >( TopNode );
		if( TopFunction && !(TopFunction->FunctionFlags & FUNC_Native) )
		{
			// Should we compile any code?
			DoCompile = (TopFunction->FunctionFlags & FUNC_Defined);
			//!! Yoda Debugger
			EmitDebugInfo(DI_NewStack);
		}
		else if( TopFunction )
		{
			// Inject native function parameters.
			for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(TopFunction); It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
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
		while( NestLevel > StartNestLevel )
			if( !CompileStatement() )
				appThrowf( TEXT("Unexpected end of code in %s"), Node->GetClass()->GetName() );
		LinesCompiled += InputLine;
	}
	else if( Node!=Class )
	{
		// Pop the nesting.
		PopNest( NewNest, Node->GetClass()->GetName() );
	}
}

void InitReplication( UClass* Class, UStruct* Node )
{
	UFunction* Function = Cast<UFunction>( Node );
	if( Function )
	{
		Function->FunctionFlags &= ~(FUNC_NetFuncFlags);
		Function->RepOffset = MAXWORD;
	}
	for( TFieldIterator<UStruct> Child(Node); Child && Child.GetStruct()==Node; ++Child )
	{
		InitReplication( Class, *Child );
	}
}

//
// Compile the script associated with the specified class.
// Returns 1 if compilation was a success, 0 if any errors occured.
//
UBOOL FScriptCompiler::CompileScript
(
	UClass*		InClass,
	FMemStack*	InMem,
	UBOOL		InBooting,
	INT			InPass
)
{
	Booting       = InBooting;
	Class         = InClass;
	Pass	      = InPass;
	Mem           = InMem;
	UBOOL Success  = 0;
	FMemMark Mark(*Mem);
	Warn->SetContext( this );

	//!! Debugger
	SupressDebugInfo = 0;
	bEmitDebugInfo = ParseParam( appCmdLine(), TEXT("DEBUG") );

	//!! Debugger
	// Message.
	if( !ParseParam( appCmdLine(), TEXT("SILENTBUILD") ) )
	{
		// Message.
		Warn->Logf( NAME_Log, TEXT("%s %s"), Pass ? TEXT("Compiling") : TEXT("Parsing"), Class->GetName() );
	}

	// Make sure our parent classes is parsed.
	for( UClass* Temp = Class->GetSuperClass(); Temp; Temp=Temp->GetSuperClass() )
		if( !(Temp->ClassFlags & CLASS_Parsed) )
			appThrowf( TEXT("'%s' can't be compiled: Parent class '%s' has errors"), Class->GetName(), Temp->GetName() );

	// Init class.
	check(!(Class->ClassFlags & CLASS_Compiled));
	if( Pass == 0 )
	{
		// First pass.
		Class->Script.Empty();
		Class->Defaults.Empty();
		Class->PropertiesSize = 0;

		// Set class flags and within.
		Class->ClassFlags &= ~CLASS_RecompilerClear;
		if( Class->GetSuperClass() )
		{
			Class->ClassFlags |= (Class->GetSuperClass()->ClassFlags) & CLASS_ScriptInherit;
			Class->ClassConfigName = Class->GetSuperClass()->ClassConfigName;
			check(Class->GetSuperClass()->ClassWithin);
			if( !Class->ClassWithin )
				Class->ClassWithin = Class->GetSuperClass()->ClassWithin;

			// Copy special categories from parent
			Class->HideCategories = Class->GetSuperClass()->HideCategories;
			Class->AutoExpandCategories = Class->GetSuperClass()->AutoExpandCategories;
		}

		check(Class->ClassWithin);
	}
	else
	{
		// Second pass.
		// Replace the script.
		Class->Script.Empty();

		// Init the replication defs.
		for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Class); It && It.GetStruct()==Class; ++It )
		{
			It->PropertyFlags &= ~CPF_Net;
			It->RepOffset = MAXWORD;
		}
		InitReplication( Class, Class );
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
	TopNest		= &Nest[-1];
	PushNest( NEST_None, TEXT(""), NULL );
	// Try to compile it, and catch any errors.
	try
	{
		// Compile until we get an error.
		if( Pass == 0 )
		{
			// Parse entire program.
			while( CompileStatement() )
				StatementsCompiled++;
	
			// Precompute info for runtime optimization.
			LinesCompiled += InputLine;

			// Stub out the script.
			Class->Script.Empty();
			Class->ClassFlags |= CLASS_Parsed;
		}
		else
		{
			// Compile all uncompiled sections of parsed code.
			CompileSecondPass( Class );

			// Mark as compiled.
			Class->ClassFlags |= CLASS_Compiled;
			// Note that we need to import defaultproperties for this class.
			Class->ClassFlags |= CLASS_NeedsDefProps;
		}

		// Make sure the compilation ended with valid nesting.
		if     ( NestLevel==0 )	appThrowf ( TEXT("Internal nest inconsistency") );
		else if( NestLevel==1 )	appThrowf ( TEXT("Missing 'Class' definition") );
		else if( NestLevel==2 ) PopNest( NEST_Class, TEXT("'Class'") );
		else if( NestLevel >2 ) appThrowf ( TEXT("Unexpected end of script in '%s' block"), NestTypeName(TopNest->NestType) );

		// Cleanup after first pass.
		if( Pass == 0 )
		{
			// Finish parse.
			PostParse( Class );
			Class->PackageImports.Shrink();
			Class->Defaults.Shrink();
			check(Class->GetPropertiesSize()==Class->Defaults.Num());

			// Check native size.
			if
			(  (Class->GetFlags() & RF_Native)
			&&	OriginalPropertiesSize
			&&	Align(Class->Defaults.Num(),PROPERTY_ALIGNMENT)!=OriginalPropertiesSize
			&&	GCheckNatives )
			{
				Warn->Logf( NAME_Warning, TEXT("Native class %s size mismatch (script %i, C++ %i)"), Class->GetName(), Class->Defaults.Num(), OriginalPropertiesSize );
				GCheckNatives = 0;
			}

			// Set config and localized flags.
			for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Class); It && It->GetOuter()==Class; ++It )
			{
				if( It->PropertyFlags & CPF_Localized )
					Class->ClassFlags |= CLASS_Localized;
				if( It->PropertyFlags & CPF_Config )
					Class->ClassFlags |= CLASS_Config;
				if( It->PropertyFlags & CPF_Component )
					Class->ClassFlags |= CLASS_HasComponents;
			}

			// Class needs to specify which ini file is going to be used if it contains config variables.
			if( (Class->ClassFlags & CLASS_Config) && (Class->ClassConfigName == NAME_None) )
			{
				// Inherit config setting from base class.
				Class->ClassConfigName = Class->GetSuperClass() ? Class->GetSuperClass()->ClassConfigName : NAME_None;
				if( Class->ClassConfigName == NAME_None )
					appThrowf( TEXT("Classes with config/ globalconfig member variables need to specify config file.") );
			}

			// First-pass success.
			Class->GetDefaultObject()->InitClassDefaultObject( Class, 1 );
		}
		Success = 1;
	}
	catch( TCHAR* ErrorMsg )
	{
		// All errors are critical when booting.
		if( GEditor->Bootstrapping )
		{
			Warn->Log( NAME_Error, ErrorMsg );
			return 0;
		}

		// Handle compiler error.
		AddResultText( TEXT("Error in %s, Line %i: %s\r\n"), Class->GetName(), InputLine, ErrorMsg );

		// Invalidate this class.
		Class->ClassFlags &= ~(CLASS_Parsed | CLASS_Compiled);
		Class->Script.Empty();
	}
	// Clean up and exit.
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
void VARARGS FScriptCompiler::AddResultText( TCHAR* Fmt, ... )
{
	TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt , Fmt );
	debugf( NAME_Log, TempStr );
	if( ErrorText )
		ErrorText->Log( TempStr );
}

void VARARGS FScriptCompiler::AddResultText( const TCHAR* Fmt, ... )
{
	TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt, Fmt );
	debugf( NAME_Log, TempStr );
	if( ErrorText )
		ErrorText->Log( TempStr );
}

/*-----------------------------------------------------------------------------
	Global functions.
-----------------------------------------------------------------------------*/

//
// Guarantee that Class and all its child classes are CLASS_Compiled and return 1,
// or 0 if error.
//
static UBOOL ParseScripts( TArray<UClass*>& AllClasses, FScriptCompiler& Compiler, UClass* Class, UBOOL MakeAll, UBOOL Booting, UBOOL MakeSubclasses )
{
	check(Class!=NULL);
	if( !Class->ScriptText )
		return 1;

	// First-pass compile this class if needed.
	if( MakeAll )
		Class->ClassFlags &= ~CLASS_Parsed;
	if( !(Class->ClassFlags & CLASS_Parsed) )
		if( !Compiler.CompileScript( Class, &GMem, Booting, 0 ) )
			return 0;
	check(Class->ClassFlags & CLASS_Parsed);

	// First-pass compile subclasses.
	if (MakeSubclasses)
	{
		for( TArray<UClass*>::TIterator It(AllClasses); It; ++It )
        {
			if( (*It)->GetSuperClass()==Class)
            {
				UClass* SubClass = *It;

                // Manual dependency support.
                for( INT NameIdx=0; NameIdx<SubClass->DependentOn.Num(); NameIdx++)
                {
					UClass* OrigDependsOnClass	= FindObject<UClass>(ANY_PACKAGE, *SubClass->DependentOn(NameIdx));
                    UClass* DependsOnClass		= OrigDependsOnClass;
                    if( !DependsOnClass )
                    {
						warnf(NAME_Error,TEXT("Unknown class %s used in conjunction with dependson"), *SubClass->DependentOn(NameIdx));
						return 0;
                    }
                    else
                    {
						// Treat manual depedency on a base class as an error to detect bad habits early on.
						if( Class->IsChildOf( DependsOnClass ) )
						{
							warnf(NAME_Error,TEXT("%s is derived from %s - please remove the dependson"), *SubClass->DependentOn(NameIdx), DependsOnClass );
							return 0;
						}
						
						// Find first base class of DependsOnClass that is not a base class of Class.
						while( !Class->IsChildOf( DependsOnClass->GetSuperClass() ) )
						{
							DependsOnClass = DependsOnClass->GetSuperClass();
						}

						// Detect circular dependencies and unnecessary usage of dependson
						if( OrigDependsOnClass->ClassFlags & CLASS_Parsed )
						{
							warnf(NAME_Error,TEXT("Class %s dependson(%s) either is a circular dependency or unnecessary usage of dependson."),SubClass->GetName(),*SubClass->DependentOn(NameIdx));
							return 0;
						}
						
						if (!ParseScripts( AllClasses, Compiler, DependsOnClass, MakeAll, Booting, MakeSubclasses))
							return 0;
					}
                }

				if (!ParseScripts( AllClasses, Compiler, SubClass, MakeAll, Booting, MakeSubclasses))
				    return 0;
            }
        }
	}

	// Success.
	return 1;
}

//
// Hierarchically recompile all scripts.
//
static UBOOL CompileScripts( TArray<UClass*>& AllClasses, FScriptCompiler& Compiler, UClass* Class, UBOOL MakeSubclasses )
{
	// Compile it.
	if( Class->ScriptText )
	{
		if( !(Class->ClassFlags & CLASS_Compiled) )
			if( !Compiler.CompileScript( Class, &GMem, 0, 1 ) )
				return 0;
		check(Class->ClassFlags & CLASS_Compiled);
	}

	// Compile subclasses.
	if (MakeSubclasses)
	{
		for( TArray<UClass*>::TIterator It(AllClasses); It; ++It )
			if( (*It)->GetSuperClass()==Class && !CompileScripts( AllClasses, Compiler, *It, MakeSubclasses ) )
				return 0;
	}

	// Success.
	return 1;
}

//
// Hierarchically import properties for all scripts.
//
static INT ClassesNeedingImporting( TArray<UClass*>& AllClasses )
{
	INT Result = 0;
	for( TArray<UClass*>::TIterator It(AllClasses); It; ++It )
		if( (*It)->ClassFlags & CLASS_NeedsDefProps )
			Result++;
	return Result;
}

static FString ClassesNeedingImportingList( TArray<UClass*>& AllClasses )
{
	FString Result;
	for( TArray<UClass*>::TIterator It(AllClasses); It; ++It )
		if( (*It)->ClassFlags & CLASS_NeedsDefProps )
			Result = Result + FString::Printf( Result.Len() ? TEXT(", %s") : TEXT("%s"), (*It)->GetName() );
	return Result;
}


static UBOOL ImportPropertiesScriptsWorker( TArray<UClass*>& AllClasses, FScriptCompiler& Compiler, UClass* Class, UBOOL MakeSubclasses )
{
	// Import its properties
	if( Class->ClassFlags & CLASS_NeedsDefProps )
	{
		warnf(TEXT("Importing Defaults for %s"), Class->GetName() );
		Class->GetDefaultObject()->InitClassDefaultObject( Class, 1 );
		if(Class->GetSuperClass())
		{
			Class->ComponentClassToNameMap = Class->GetSuperClass()->ComponentClassToNameMap;
			Class->ComponentNameToDefaultObjectMap = Class->GetSuperClass()->ComponentNameToDefaultObjectMap;
		}
		Compiler.InDefaultPropContext = 1; // error reporting
		Class->PropagateStructDefaults();
		UBOOL ImportSuccess = ImportDefaultProperties(Class,*Class->DefaultPropText,GWarn,0)!=NULL || !Class->DefaultPropText.Len();
		Compiler.InDefaultPropContext = 0;

		// if import failed, return 1.
		if( !ImportSuccess )
		{
			warnf(TEXT("... deferring"));
			return 1;
		}
	}

	// Record the fact that we've imported the properties for this class.
	Class->ClassFlags &= ~(CLASS_NeedsDefProps);

	// Import properties in subclasses.
	if (MakeSubclasses)
	{
		for( TArray<UClass*>::TIterator It(AllClasses); It; ++It )
			if( (*It)->GetSuperClass()==Class && !ImportPropertiesScriptsWorker( AllClasses, Compiler, *It, MakeSubclasses ) )
				return 0;
	}

	// Success.
	return 1;
}

static UBOOL ImportPropertiesScripts( TArray<UClass*>& AllClasses, FScriptCompiler& Compiler, UClass* Class, UBOOL MakeSubclasses )
{
	INT InitialErrorCount = GWarn->ErrorCount;
	INT LastCount = ClassesNeedingImporting( AllClasses );
	do
	{
		if( !ImportPropertiesScriptsWorker( AllClasses, Compiler, Class, MakeSubclasses ) )
			return 0;
		INT Count = ClassesNeedingImporting( AllClasses );
		if( Count == LastCount )
			appThrowf(TEXT("Compiler encountered circular BEGIN OBJECT dependency: %s"), *ClassesNeedingImportingList(AllClasses) );
		LastCount = Count;
	} while( LastCount );

	// return success if the error count didn't increase!
	return GWarn->ErrorCount == InitialErrorCount;
}

static UBOOL ImportPropertiesStructs( TArray<UStruct*>& RealStructs, FScriptCompiler& Compiler, UClass* Class, UBOOL MakeSubclasses )
{
	INT InitialErrorCount = GWarn->ErrorCount;

	for( INT i=0; i<RealStructs.Num(); i++ )
	{
		UStruct* Struct = RealStructs(i);
		if( Struct )
		{
			Compiler.InDefaultPropContext = 0;
			ImportObjectProperties( Struct, &Struct->Defaults(0), NULL, *Struct->DefaultStructPropText, NULL, Class, GWarn, 0, Class );
		}
	}

	// return success if the error count didn't increase!
	return GWarn->ErrorCount == InitialErrorCount;
}

//
// Save fields transactionally.
//
static void SaveRecursiveFields( FTransactionBase* Tr, UField* Field )
{
	Tr->SaveObject( Field );
	UStruct* ThisStruct = Cast<UStruct>( Field );
	if( ThisStruct )
		for( TFieldIterator<UField> It(ThisStruct); It && It.GetStruct()==ThisStruct; ++It )
			SaveRecursiveFields( Tr, *It );
}

IMPLEMENT_COMPARE_POINTER( UClass, UnScrCom, { return appStricmp(A->GetName(),B->GetName()); } )

//
// Make all scripts.
// Returns 1 if success, 0 if errors.
// Not recursive.
//
UBOOL UEditorEngine::MakeScripts( UClass* BaseClass, FFeedbackContext* Warn, UBOOL MakeAll, UBOOL Booting, UBOOL MakeSubclasses )
{
	FMemMark Mark(GMem);
	FTransactionBase* Transaction = Trans ? Trans->CreateInternalTransaction() : NULL;
	FScriptCompiler	Compiler( Warn );
	Compiler.InDefaultPropContext = 0;

	// Make list of all classes from this base
	if (!BaseClass)
		BaseClass = UObject::StaticClass();
	TArray<UClass*> AllClasses;
	for( TObjectIterator<UClass> ObjIt; ObjIt; ++ObjIt )
	{
		if ((*ObjIt == BaseClass) || (MakeSubclasses && ObjIt->IsChildOf(BaseClass)))
			AllClasses.AddItem( *ObjIt );
	}

	// Make script compilation deterministic by sorting by name.
	Sort<USE_COMPARE_POINTER(UClass,UnScrCom)>( &AllClasses(0), AllClasses.Num() );

	// Do compiling.
	Compiler.ShowDep            = ParseParam( appCmdLine(), TEXT("SHOWDEP") );
	Compiler.StatementsCompiled	= 0;
	Compiler.LinesCompiled		= 0;
	Compiler.ErrorText			= Results;
	if( Compiler.ErrorText )
		Compiler.ErrorText->Text.Empty();

	// Hierarchically parse and compile all classes.
	UBOOL Success = 1;
	Success = Success && ParseScripts( AllClasses, Compiler, BaseClass, MakeAll, Booting, MakeSubclasses );
	Success = Success && CompileScripts( AllClasses, Compiler, BaseClass, MakeSubclasses );

	// Need to do this after CompileScripts.
	TArray<UStruct*> AllRealStructs;
	for( TObjectIterator<UStruct> ObjIt; ObjIt; ++ObjIt )
	{
		UStruct* Struct = *ObjIt;
		if( Struct->DefaultStructPropText != TEXT("") )
			AllRealStructs.AddItem( Struct );
	}

	Success = Success && ImportPropertiesStructs( AllRealStructs, Compiler, BaseClass, MakeSubclasses );
	Success = Success && ImportPropertiesScripts( AllClasses, Compiler, BaseClass, MakeSubclasses );
	// Done with make.
	if( Success )
	{
		// Success.
		if( Compiler.LinesCompiled )
			Compiler.AddResultText( TEXT("Success: Compiled %i line(s), %i statement(s).\r\n"), Compiler.LinesCompiled, Compiler.StatementsCompiled );
		else
			Compiler.AddResultText( TEXT("Success: Everything is up to date") );
	}
	else
	{
		// Restore all classes after compile fails.
		if( Transaction )
			Transaction->Apply();
	}
	for( TArray<UClass*>::TIterator It(AllClasses); It; ++It )
	{
		// Cleanup all exported property text.
		(*It)->DefaultPropText=TEXT("");
	}
	if( Transaction )
		delete Transaction;
	Mark.Pop();
	return Success;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
