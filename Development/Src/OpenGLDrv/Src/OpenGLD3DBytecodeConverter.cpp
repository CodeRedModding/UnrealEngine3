/*=============================================================================
	OpenGLD3DBytecodeConverter.cpp: Converts D3D binary shaders to GLSL.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

#if _WINDOWS

struct AsmInstructionData;

const int MaxTexcoordRegisters		= 8;
const int MaxTemporaryRegisters		= 32;
const int MaxInputRegisters			= 12;
const int MaxOutputRegisters		= 12;
const int MaxConstIntRegisters		= 16;
const int MaxConstBoolRegisters		= 16;
const int MaxLabels					= 16;
const int MaxFragmentSamplers		= 16;
const int MaxVertexSamplers			= 16;
const int MaxAttributeRegisters		= 16;
const int MaxSamplers				= ( MaxVertexSamplers > MaxFragmentSamplers ) ? MaxVertexSamplers : MaxFragmentSamplers;
const int MaxCombinedSamplers		= MaxVertexSamplers + MaxFragmentSamplers;

enum VaryingRegisters
{
	UnTexcoord8		= 0,
	UnTexcoord9		= 1,

	VaryingRegistersNum	// has to be last
};

struct RegisterMapping
{
	UBOOL	TexcoordRegs[MaxTexcoordRegisters];
	DWORD	TexcoordMask[MaxTexcoordRegisters];

	UBOOL	TemporaryRegs[MaxTemporaryRegisters];
	UBOOL	AddressReg;
	UBOOL	PackedInputRegs[MaxInputRegisters];
	UBOOL	PackedOutputRegs[MaxOutputRegisters];
	UBOOL	AttributeRegs[MaxAttributeRegisters];
	UBOOL	Labels[MaxLabels];
	UBOOL	VaryingRegisters[VaryingRegistersNum];

	UBOOL	InputRegistersUsed[MaxInputRegisters];	// Used in pixel shaders only

	INT		MinimumRelativeOffset;		// Used in vertex shaders only
	INT		MaximumRelativeOffset;		// Used in vertex shaders only

	DWORD	SamplersUsage[MaxSamplers];
	ANSICHAR BumpEnvMatrixSamplerNum;
	ANSICHAR LuminanceParams;

	DWORD	LoopDepth;	// if 0, no loops used

	UBOOL	UsesFog;
	UBOOL	UsesRelConstF;
};

struct Semantic
{
	D3DDECLUSAGE	Usage;
	DWORD			UsageIndex;
	DWORD			Register;
};

struct ShaderConstant
{
	DWORD Index;
	DWORD Value[4];
};

class D3DShaderData
{
public:
	DWORD	ShaderVersion;
	INT		ShaderVersionMajor;
	INT		ShaderVersionMinor;
	UBOOL	bIsPixelShader;
	UBOOL	bHasGlobalConsts;
	UBOOL	bHasBonesConsts;

	DWORD	BytecodeLength;
	DWORD*	Bytecode;

	// Limits for contained program, based on shader version
	// This is filled in during shader InitLimits() method
	DWORD	MaxTemporaries;
	DWORD	MaxTexcoords;
	DWORD	MaxSamplers;
	DWORD	MaxConstInts;
	DWORD	MaxConstFloats;
	DWORD	SafeConstFloats;
	DWORD	MaxConstBools;
	DWORD	MaxAddressRegs;
	DWORD	MaxPackedOutputs;
	DWORD	MaxPackedInputs;
	DWORD	MaxAttributes;
	DWORD	MaxLabels;

	// Registers, attributes, outputs and constants used (calculated in DetermineUsedRegisters() from bytecode)
	RegisterMapping		Regs;
	Semantic			RegsIn[MaxAttributeRegisters];
	Semantic			RegsOut[MaxOutputRegisters];
	ShaderConstant*		ConstantsFloat;
	DWORD				ConstantsFloatNum;
	ShaderConstant*		ConstantsInt;
	DWORD				ConstantsIntNum;
	ShaderConstant*		ConstantsBool;
	DWORD				ConstantsBoolNum;

	DWORD				CurrentLoopDepth;	// used both for REP and LOOP
	DWORD				CurrentLoopRegNum;	// used for LOOP only

	const AsmInstructionData* InstructionsList;	// for vertex or pixel shader

	// This remembers texture formats and samplers used by shader during last compilation; can be used to check if shader needs recompilation.
	DWORD				SampledSamplersNum;
	DWORD				SampledSamplerIndex[MaxCombinedSamplers];
	D3DFORMAT			SampledTextureFormat[MaxCombinedSamplers];

	D3DShaderData(UBOOL InHasGlobalConsts, UBOOL InHasBonesConsts)
	:	ShaderVersion( 0 ), bIsPixelShader( FALSE ), bHasGlobalConsts( InHasGlobalConsts ), bHasBonesConsts( InHasBonesConsts ),
		BytecodeLength( 0 ), Bytecode( 0 ), ConstantsFloat( 0 ), ConstantsFloatNum( 0 ), ConstantsInt( 0 ), ConstantsIntNum( 0 ),
		ConstantsBool( 0 ), ConstantsBoolNum( 0 ), CurrentLoopDepth( 0 ), CurrentLoopRegNum( 0 ),
		InstructionsList( 0 ), SampledSamplersNum( 0 ) {}
	~D3DShaderData( void ) { Clear(); }

	// This method will copy the bytecode into the shader object.
	// It will return true on succesfull parse, and false if parse wasn't possible.
	UBOOL Init( const DWORD* Bytecode );

	void InitLimits( void );
	void DetermineUsedRegisters();

	// This method will reset the object to the initial state after construction.
	void Clear( void )
	{
		if( Bytecode ) { appFree( Bytecode ); Bytecode = 0; }
		if( ConstantsFloat ) { appFree( ConstantsFloat ); ConstantsFloat = 0; }
		if( ConstantsInt ) { appFree( ConstantsInt ); ConstantsInt = 0; }
		if( ConstantsBool ) { appFree( ConstantsBool ); ConstantsBool = 0; }
		BytecodeLength = 0;
		ShaderVersion = 0;
		ShaderVersionMajor = 0;
		ShaderVersionMinor = 0;
		SampledSamplersNum = 0;
	}

	const AsmInstructionData* IdentifyInstruction( DWORD InstructionCode );
	UBOOL FloatConstantIsLocal( DWORD RegNum );

	D3DFORMAT GetTextureFormatForSampler( DWORD SamplerNum )
	{
		// TO DO - we need sampler texture info here
		return D3DFMT_A8R8G8B8;
	}

	DWORD GetTextureTransformFlagsForSampler( DWORD SamplerNum )
	{
		// TO DO - we need sampler texture transform flags info here
		return D3DTTFF_DISABLE;
	}

	void GenerateGLSLDeclarations( FString &TargetBuffer );
	void GenerateOutputRegistersAssignments( FString &TargetBuffer );
	void GenerateInputRegistersAssignments( FString &TargetBuffer );
	void GenerateShaderBody( FString &TargetBuffer );
	void GetAttributeRegName( DWORD RegNum, ANSICHAR* NameBuffer );

protected:

	DWORD ReadParam( const DWORD*& TokenPtr, DWORD& AddressToken );

private:

	void AddConstantFloat( DWORD ConstNum, const DWORD* ConstValue )
	{
		if( !ConstantsFloat )
		{
			ConstantsFloat = (ShaderConstant*)appMalloc(sizeof(ShaderConstant));
			ConstantsFloatNum = 1;
		}
		else
		{
			ConstantsFloatNum++;
			ConstantsFloat = (ShaderConstant*)appRealloc(ConstantsFloat, sizeof(ShaderConstant)*ConstantsFloatNum);
		}
		ConstantsFloat[ConstantsFloatNum-1].Index = ConstNum;
		appMemcpy( ConstantsFloat[ConstantsFloatNum-1].Value, ConstValue, 4 * sizeof(DWORD) );
	}

	void AddConstantInt( DWORD ConstNum, const DWORD* ConstValue )
	{
		if( !ConstantsInt )
		{
			ConstantsInt = (ShaderConstant*)appMalloc(sizeof(ShaderConstant));
			ConstantsIntNum = 1;
		}
		else
		{
			ConstantsIntNum++;
			ConstantsInt = (ShaderConstant*)appRealloc(ConstantsInt, sizeof(ShaderConstant)*ConstantsIntNum);
		}
		ConstantsInt[ConstantsIntNum-1].Index = ConstNum;
		appMemcpy( ConstantsInt[ConstantsIntNum-1].Value, ConstValue, 4 * sizeof(DWORD) );
	}

	void AddConstantBool( DWORD ConstNum, DWORD ConstValue )
	{
		if( !ConstantsBool )
		{
			ConstantsBool = (ShaderConstant*)appMalloc(sizeof(ShaderConstant));
			ConstantsBoolNum = 1;
		}
		else
		{
			ConstantsBoolNum++;
			ConstantsBool = (ShaderConstant*)appRealloc(ConstantsBool, sizeof(ShaderConstant)*ConstantsBoolNum);
		}
		ConstantsBool[ConstantsBoolNum-1].Index = ConstNum;
		ConstantsBool[ConstantsBoolNum-1].Value[0] = ConstValue;
	}
};

// Structure to give to conversion function - all information it needs is in there.
struct ConversionState
{
	D3DShaderData*				Shader;
	const AsmInstructionData*	Instruction;
	DWORD						InstructionCode;
	DWORD						DestToken;
	DWORD						DestAddressToken;
	DWORD						PredicateToken;
	DWORD						SourceToken[4];
	DWORD						SourceAddressToken[4];
	FString						*OutputBuffer;
};

struct SourceParamName
{
	ANSICHAR	RegisterName[128];
	ANSICHAR	SourceExpression[128];	// this will be source param, with all modifiers, converted
};

struct DestParamName
{
	ANSICHAR	RegisterName[128];
	ANSICHAR	MaskString[128];
};

// Function to convert specific instruction to GLSL - prototype
typedef void (*GLSL_CONVERSION_FUNCTION) (ConversionState* );

// Information about specific asm instruction
struct AsmInstructionData
{
	DWORD						Opcode;
	const ANSICHAR*				AsmName;
	UBOOL						HasDestination;
	DWORD						SourceParamsNum;
	GLSL_CONVERSION_FUNCTION	ConversionFunction;
	DWORD						MinShaderVersion;
	DWORD						MaxShaderVersion;
};

static const TCHAR* GShiftInterpretations[] =
{
	TEXT(""),				// 0 (no shift)
	TEXT("2.0 * "),			// 1 (x2)
	TEXT("4.0 * "),			// 2 (x4)
	TEXT("8.0 * "),			// 3 (x8)
	TEXT("16.0 * "),		// 4 (x16)
	TEXT("32.0 * "),		// 5 (x32)
	TEXT("64.0 * "),		// 6 (x64)
	TEXT("128.0 * "),		// 7 (x128)
	TEXT("0.00390625 * "),	// 8 (d256)
	TEXT("0.0078125 * "),	// 9 (d128)
	TEXT("0.015625 * "),	// 10 (d64)
	TEXT("0.03125 * "),		// 11 (d32)
	TEXT("0.0625 * "),		// 12 (d16)
	TEXT("0.125 * "),		// 13 (d8)
	TEXT("0.25 * "),		// 14 (d4)
	TEXT("0.5 * ")			// 15 (d2)
};

static DWORD GetWriteMaskSize( DWORD Mask )
{
	DWORD Size = 0;
	if( Mask & D3DSP_WRITEMASK_0 )	++Size;
	if( Mask & D3DSP_WRITEMASK_1 )	++Size;
	if( Mask & D3DSP_WRITEMASK_2 )	++Size;
	if( Mask & D3DSP_WRITEMASK_3 )	++Size;
	return Size;
}

static inline INT GetRegisterType( DWORD Reg )
{
	return (((Reg & D3DSP_REGTYPE_MASK) >> D3DSP_REGTYPE_SHIFT) | ((Reg & D3DSP_REGTYPE_MASK2) >> D3DSP_REGTYPE_SHIFT2));
}

static UBOOL IsScalarRegister( DWORD Param )
{
	switch(GetRegisterType(Param))
	{
	case D3DSPR_LOOP:
	case D3DSPR_CONSTBOOL:
	case D3DSPR_DEPTHOUT:
	case D3DSPR_PREDICATE:
		return TRUE;
	case D3DSPR_RASTOUT:
		return ((Param & D3DSP_REGNUM_MASK) != 0);
	default:
		return FALSE;
	}
}

static DWORD GetWriteMask( DWORD Param, ANSICHAR* MaskString, UBOOL bIsPixelShader )
{
	ANSICHAR* Ptr = MaskString;
	DWORD Mask = Param & D3DSP_WRITEMASK_ALL;

	if( IsScalarRegister( Param ) )
	{
		Mask = D3DSP_WRITEMASK_0;
	}
	else if( Mask != D3DSP_WRITEMASK_ALL )
	{
		*Ptr++ = '.';
		if( bIsPixelShader )
		{
			if( Param & D3DSP_WRITEMASK_0 )	*Ptr++ = 'r';
			if( Param & D3DSP_WRITEMASK_1 )	*Ptr++ = 'g';
			if( Param & D3DSP_WRITEMASK_2 )	*Ptr++ = 'b';
			if( Param & D3DSP_WRITEMASK_3 )	*Ptr++ = 'a';
		}
		else
		{
			if( Param & D3DSP_WRITEMASK_0 )	*Ptr++ = 'x';
			if( Param & D3DSP_WRITEMASK_1 )	*Ptr++ = 'y';
			if( Param & D3DSP_WRITEMASK_2 )	*Ptr++ = 'z';
			if( Param & D3DSP_WRITEMASK_3 )	*Ptr++ = 'w';
		}
	}
	*Ptr = 0;
	return Mask;
}

static void GetSwizzle( DWORD Param, DWORD Mask, ANSICHAR* SwizzleString, UBOOL bPixelShader )
{
	DWORD Swizzle = ( Param & D3DSP_SWIZZLE_MASK ) >> D3DSP_SWIZZLE_SHIFT;
	const ANSICHAR* Letters = bPixelShader ? "rgba" : "xyzw";
	ANSICHAR* Ptr = SwizzleString;
	if( !IsScalarRegister( Param ) )
	{
		*Ptr++ = '.';
		if( Mask & D3DSP_WRITEMASK_0 )	*Ptr++ = Letters[ Swizzle & 0x03 ];
		if( Mask & D3DSP_WRITEMASK_1 )	*Ptr++ = Letters[ ( Swizzle >> 2 ) & 0x03 ];
		if( Mask & D3DSP_WRITEMASK_2 )	*Ptr++ = Letters[ ( Swizzle >> 4 ) & 0x03 ];
		if( Mask & D3DSP_WRITEMASK_3 )	*Ptr++ = Letters[ ( Swizzle >> 6 ) & 0x03 ];
	}
	*Ptr = 0;
}

static void GenerateSourceParamName( ConversionState& state, DWORD sourceToken, DWORD sourceAddressToken, DWORD sourceMask, SourceParamName& returnName );

static ANSICHAR *GenerateConstFloatArrayName(ConversionState& State, UINT RegNum, DWORD &Adjusted)
{
	if (State.Shader->bHasGlobalConsts && (RegNum >= (State.Shader->bIsPixelShader ? PS_GLOBAL_CONSTANT_BASE_INDEX : VS_GLOBAL_CONSTANT_BASE_INDEX)))
	{
		Adjusted = RegNum - (State.Shader->bIsPixelShader ? PS_GLOBAL_CONSTANT_BASE_INDEX : VS_GLOBAL_CONSTANT_BASE_INDEX);
		return "ConstGlobal";
	}
	else if (State.Shader->bHasBonesConsts && RegNum >= BONE_CONSTANT_BASE_INDEX)
	{
		Adjusted = RegNum - BONE_CONSTANT_BASE_INDEX;
		return "ConstBones";
	}
	else
	{
		Adjusted = RegNum;
		return "ConstFloat";
	}
}

static void GetRegisterName(ConversionState& State, DWORD RegToken, DWORD RegAddressToken, ANSICHAR* OutputName)
{
	static const ANSICHAR* RastoutRegisterNames[] = { "gl_Position", "gl_FogFragCoord", "gl_PointSize" };
	DWORD RegNum = RegToken & D3DSP_REGNUM_MASK;
	DWORD RegType = GetRegisterType( RegToken );

	switch( RegType )
	{
	case D3DSPR_CONST:
	{
		DWORD Adjusted = RegNum;
		ANSICHAR *ConstFloatArray = GenerateConstFloatArrayName(State, RegNum, Adjusted);

		if( RegToken & D3DSHADER_ADDRMODE_RELATIVE )
		{
			const ANSICHAR* AddressString = "Address0.x";
			SourceParamName Address;
			if( State.Shader->ShaderVersionMajor >= 2 )
			{
				GenerateSourceParamName( State, RegAddressToken, 0, D3DSP_WRITEMASK_0, Address );
				AddressString = (ANSICHAR*)&Address.SourceExpression;
			}

			if( RegNum )
			{
				sprintf_s( OutputName, 128, "%c%s[ %s + %u ]", State.Shader->bIsPixelShader?'P':'V', ConstFloatArray, AddressString, Adjusted );
			}
			else
			{
				sprintf_s( OutputName, 128, "%cConstFloat[ %s ]", State.Shader->bIsPixelShader?'P':'V', AddressString );
			}
		}
		else
		{
			if( State.Shader->FloatConstantIsLocal( RegNum ) )
			{
				sprintf_s( OutputName, 128, "LocalConst%u", RegNum );
			}
			else
			{
				sprintf_s( OutputName, 128, "%c%s[%u]", State.Shader->bIsPixelShader?'P':'V', ConstFloatArray, Adjusted );
			}
		}
		break;
	}

	case D3DSPR_CONSTINT:	sprintf_s( OutputName, 128, "%cConstInt[%u]", State.Shader->bIsPixelShader?'P':'V', RegNum );				break;
	case D3DSPR_CONSTBOOL:	sprintf_s( OutputName, 128, "%cConstBool[%u]", State.Shader->bIsPixelShader?'P':'V', RegNum );				break;
	case D3DSPR_TEXTURE:	sprintf_s( OutputName, 128, "%s%u", State.Shader->bIsPixelShader ? "Texture" : "Address" , RegNum );		break;
	case D3DSPR_TEMP:		sprintf_s( OutputName, 128, "Temporary%u", RegNum );														break;
	case D3DSPR_LOOP:		sprintf_s( OutputName, 128, "Loop%u", (DWORD)(State.Shader->CurrentLoopRegNum - 1) );						break;
	case D3DSPR_SAMPLER:	sprintf_s( OutputName, 128, "%cSampler%u", State.Shader->bIsPixelShader?'P':'V', RegNum );					break;
	case D3DSPR_RASTOUT:	strcpy_s( OutputName, 128, RastoutRegisterNames[RegNum] );													break;
	case D3DSPR_DEPTHOUT:	strcpy_s( OutputName, 128, "gl_FragDepth" );																break;
	case D3DSPR_ATTROUT:	strcpy_s( OutputName, 128, RegNum ? "gl_FrontSecondaryColor" : "gl_FrontColor" );							break;
	case D3DSPR_TEXCRDOUT:	sprintf_s( OutputName, 128, "%s[%u]", "OUT",RegNum);														break;
	case D3DSPR_MISCTYPE:
		if( RegNum > 1 )
		{
			appErrorf(TEXT("Misc registers > 1 don't exist!"));
		}
		else if( RegNum == 1 )
		{
			strcpy_s( OutputName, 128, "(gl_FrontFacing?vec4(-1.0):vec4(1.0))" );
		}
		else
		{
			strcpy_s( OutputName, 128, "gl_FragCoord" );
		}
		break;

	case D3DSPR_COLOROUT:
		sprintf_s( OutputName, 128, "gl_FragData[%u]", RegNum );
		break;

	case D3DSPR_INPUT:
		if( State.Shader->bIsPixelShader )
		{
			if( RegToken & D3DSHADER_ADDRMODE_RELATIVE )
			{
				SourceParamName Address;
				GenerateSourceParamName( State, RegAddressToken, 0, D3DSP_WRITEMASK_0, Address );
				if( RegNum )
				{
					sprintf_s( OutputName, 128, "IN[%s + %u]", Address.SourceExpression, RegNum );
				}
				else
				{
					sprintf_s( OutputName, 128, "IN[%s]", Address.SourceExpression );
				}
			}
			else
			{
				sprintf_s( OutputName, 128, "IN[%u]", RegNum );
			}
		}
		else	// vertex shader
		{
			State.Shader->GetAttributeRegName( RegNum, OutputName );
		}
		break;
	}
}

static DWORD GenerateDestParamName( ConversionState& State, DWORD DestToken, DWORD DestAddressToken, DestParamName& ReturnName )
{
	ReturnName.RegisterName[0] = 0;
	ReturnName.MaskString[0] = 0;
	GetRegisterName( State, DestToken, DestAddressToken, ReturnName.RegisterName );
	return GetWriteMask( DestToken, ReturnName.MaskString, State.Shader->bIsPixelShader );
}

static void GenerateSourceParamName( ConversionState& State, DWORD SourceToken, DWORD SourceAddressToken, DWORD SourceMask, SourceParamName& ReturnName )
{
	ReturnName.RegisterName[0] = 0;
	GetRegisterName( State, SourceToken, SourceAddressToken, ReturnName.RegisterName );

	ANSICHAR SwizzleString[6] = { 0 };
	GetSwizzle( SourceToken, SourceMask, SwizzleString, State.Shader->bIsPixelShader );

	ReturnName.SourceExpression[0] = 0;
	if( SourceToken == D3DSIO_TEXKILL )
	{
		return;	// no param string needed
	}

	switch( SourceToken & D3DSP_SRCMOD_MASK )
	{
	case D3DSPSM_NONE:		sprintf_s( ReturnName.SourceExpression, 128, "%s%s", ReturnName.RegisterName, SwizzleString );									break;	// no modification needed
	case D3DSPSM_NOT:		sprintf_s( ReturnName.SourceExpression, 128, "!%s%s", ReturnName.RegisterName, SwizzleString );								break;
	case D3DSPSM_COMP:		sprintf_s( ReturnName.SourceExpression, 128, "(1.0-%s%s)", ReturnName.RegisterName, SwizzleString );							break;
	case D3DSPSM_NEG:		sprintf_s( ReturnName.SourceExpression, 128, "-%s%s", ReturnName.RegisterName, SwizzleString );								break;
	case D3DSPSM_BIAS:		sprintf_s( ReturnName.SourceExpression, 128, "(%s%s-vec4(0.5)%s)", ReturnName.RegisterName, SwizzleString, SwizzleString );	break;
	case D3DSPSM_BIASNEG:	sprintf_s( ReturnName.SourceExpression, 128, "-(%s%s-vec4(0.5)%s)", ReturnName.RegisterName, SwizzleString, SwizzleString );	break;
	case D3DSPSM_SIGN:		sprintf_s( ReturnName.SourceExpression, 128, "(2.0*(%s%s-0.5))", ReturnName.RegisterName, SwizzleString );						break;
	case D3DSPSM_SIGNNEG:	sprintf_s( ReturnName.SourceExpression, 128, "-(2.0*(%s%s-0.5))", ReturnName.RegisterName, SwizzleString );					break;
	case D3DSPSM_X2:		sprintf_s( ReturnName.SourceExpression, 128, "(2.0*%s%s)", ReturnName.RegisterName, SwizzleString );							break;
	case D3DSPSM_X2NEG:		sprintf_s( ReturnName.SourceExpression, 128, "-(2.0*%s%s)", ReturnName.RegisterName, SwizzleString );							break;
	case D3DSPSM_ABS:		sprintf_s( ReturnName.SourceExpression, 128, "abs(%s%s)", ReturnName.RegisterName, SwizzleString );							break;
	case D3DSPSM_ABSNEG:	sprintf_s( ReturnName.SourceExpression, 128, "-abs(%s%s)", ReturnName.RegisterName, SwizzleString );							break;

	case D3DSPSM_DZ:
	case D3DSPSM_DW:	sprintf_s( ReturnName.SourceExpression, 128, "%s%s", ReturnName.RegisterName, SwizzleString );									break;	// need to take care of this in instruction handler :/
	default:			appErrorf(TEXT("Unknown source parameter modifier - %u!!!"), SourceToken & D3DSP_SRCMOD_MASK );			break;
	};
}

static DWORD WriteDestinationToTextBuffer( ConversionState* State, FString &OutputBuffer, const DWORD TokenChanged = 0 )
{
	DestParamName Dest;
	DWORD Mask = GenerateDestParamName( *State, TokenChanged ? TokenChanged : State->DestToken, State->DestAddressToken, Dest );
	if( Mask )
	{
		INT Shift = ( State->DestToken & D3DSP_DSTSHIFT_MASK ) >> D3DSP_DSTSHIFT_SHIFT;
		OutputBuffer += FString::Printf(TEXT("%s%s = %s("), ANSI_TO_TCHAR(Dest.RegisterName), ANSI_TO_TCHAR(Dest.MaskString), GShiftInterpretations[Shift]);
	}
	// else we don't write anything.
	return Mask;
}

static void Convert_mov( ConversionState* State )
{
	DWORD WriteMask = WriteDestinationToTextBuffer( State, *State->OutputBuffer );

	SourceParamName Source;		GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], WriteMask, Source );

	if( State->Instruction->Opcode == D3DSIO_MOVA )
	{
		DWORD MaskSize = GetWriteMaskSize( WriteMask );
		if( MaskSize > 1 )
		{
			*State->OutputBuffer += FString::Printf(TEXT("ivec%d( floor(abs(%s)+vec%d(0.5)) * sign(%s)));\n"), MaskSize, ANSI_TO_TCHAR(Source.SourceExpression), MaskSize, ANSI_TO_TCHAR(Source.SourceExpression));
		}
		else
		{
			*State->OutputBuffer += FString::Printf(TEXT("int( floor(abs(%s)+0.5) * sign(%s)));\n"), ANSI_TO_TCHAR(Source.SourceExpression), ANSI_TO_TCHAR(Source.SourceExpression));
		}
	}
	else
	{
		*State->OutputBuffer += FString::Printf(TEXT("%s);\n"), ANSI_TO_TCHAR(Source.SourceExpression));
	}
}

static void Convert_math( ConversionState* State )
{
	DWORD WriteMask = WriteDestinationToTextBuffer( State, *State->OutputBuffer );

	SourceParamName Source1;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], WriteMask, Source1 );	
	SourceParamName Source2;	GenerateSourceParamName( *State, State->SourceToken[1], State->SourceAddressToken[1], WriteMask, Source2 );

	ANSICHAR MathOperator = ' ';
	switch( State->Instruction->Opcode )
	{
	case D3DSIO_ADD:	MathOperator = '+';	break;
	case D3DSIO_SUB:	MathOperator = '-';	break;
	case D3DSIO_MUL:	MathOperator = '*';	break;
	default:			appErrorf(TEXT("Unhandled opcode in convert_math!"));	break;
	}

	*State->OutputBuffer += FString::Printf(TEXT("%s %c %s);\n"), ANSI_TO_TCHAR(Source1.SourceExpression), MathOperator, ANSI_TO_TCHAR(Source2.SourceExpression));
}

static void Convert_dot( ConversionState* State )
{
	DWORD WriteMask = WriteDestinationToTextBuffer( State, *State->OutputBuffer );
	DWORD WriteMaskSize = GetWriteMaskSize( WriteMask );
	DWORD SourceMask = ( State->Instruction->Opcode == D3DSIO_DP4 ) ? D3DSP_WRITEMASK_ALL : D3DSP_WRITEMASK_0|D3DSP_WRITEMASK_1|D3DSP_WRITEMASK_2;

	SourceParamName Source1;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], SourceMask, Source1 );
	SourceParamName Source2;	GenerateSourceParamName( *State, State->SourceToken[1], State->SourceAddressToken[1], SourceMask, Source2 );

	if( WriteMaskSize > 1 )
	{
		*State->OutputBuffer += FString::Printf(TEXT("vec%d"), WriteMaskSize);
	}
	*State->OutputBuffer += FString::Printf(TEXT("(dot(%s,%s)));\n"), ANSI_TO_TCHAR(Source1.SourceExpression), ANSI_TO_TCHAR(Source2.SourceExpression));
}

static void Convert_matmul( ConversionState* State )
{
	// TO DO: maybe we should use real matrix multiplications here... we'll see

	ConversionState TempState = *State;
	DWORD destTokenNoWriteMask = State->DestToken & ~D3DSP_WRITEMASK_ALL;
	INT NumDots = 0;
	DWORD TempInstruction = 0;

	switch( State->Instruction->Opcode )
	{
	case D3DSIO_M4x4:	TempInstruction = D3DSIO_DP4;	NumDots = 4;	break;
	case D3DSIO_M4x3:	TempInstruction = D3DSIO_DP4;	NumDots = 3;	break;
	case D3DSIO_M3x4:	TempInstruction = D3DSIO_DP3;	NumDots = 4;	break;
	case D3DSIO_M3x3:	TempInstruction = D3DSIO_DP3;	NumDots = 3;	break;
	case D3DSIO_M3x2:	TempInstruction = D3DSIO_DP3;	NumDots = 2;	break;
	default:	appErrorf(TEXT("Unrecognized instruction in convert_matmul!!"));
	};

	TempState.Instruction = State->Shader->IdentifyInstruction( TempInstruction );

	for( INT Index = 0; Index < NumDots; ++Index )
	{
		TempState.DestToken = destTokenNoWriteMask | ( D3DSP_WRITEMASK_0 << Index );
		TempState.SourceToken[1] = State->SourceToken[1] + Index;
		Convert_dot( &TempState );
	}
}

static void Convert_map2gl( ConversionState* State )	// simple 1-1 mapping
{
	static const DWORD Instructions[] = { D3DSIO_MIN, D3DSIO_MAX, D3DSIO_ABS, D3DSIO_FRC, D3DSIO_NRM, D3DSIO_LOGP, D3DSIO_LOG, D3DSIO_EXP, D3DSIO_SGN, D3DSIO_DSX, D3DSIO_DSY };
	static const ANSICHAR* GLSLStrings[] = { "min", "max", "abs", "fract", "normalize", "log2", "log2", "exp2", "sign", "dFdx", "dFdy" };
	static const int NumInstructions = sizeof(Instructions)/sizeof(DWORD);

	DWORD Opcode = State->Instruction->Opcode;
	const ANSICHAR* GLSLInstruction = NULL;

	for( INT Index = 0; Index < NumInstructions; ++Index )
	{
		if( Opcode == Instructions[Index] )
		{
			GLSLInstruction = GLSLStrings[Index];
			break;
		}
	}

	if( !GLSLInstruction )
	{
		appErrorf(TEXT("Unrecognized instruction in convert_map2gl!!"));
	}

	DWORD WriteMask = WriteDestinationToTextBuffer( State, *State->OutputBuffer );

	ANSICHAR ArgumentString[256] = { 0 };
	SourceParamName Source;

	if( State->Instruction->SourceParamsNum )
	{
		GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], WriteMask, Source );
		strcat_s( ArgumentString, 256, Source.SourceExpression );
		for( DWORD i = 1; i < State->Instruction->SourceParamsNum; ++i )
		{
			strcat_s( ArgumentString, 256, ", " );
			GenerateSourceParamName( *State, State->SourceToken[i], State->SourceAddressToken[i], WriteMask, Source );
			strcat_s( ArgumentString, 256, Source.SourceExpression );
		}
	}

	*State->OutputBuffer += FString::Printf(TEXT("%s(%s));\n"), ANSI_TO_TCHAR(GLSLInstruction), ANSI_TO_TCHAR(ArgumentString));
}

static void Convert_compare( ConversionState* State )
{
	DWORD WriteMask = WriteDestinationToTextBuffer( State, *State->OutputBuffer );
	DWORD WriteMaskSize = GetWriteMaskSize( WriteMask );

	SourceParamName Source1;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], WriteMask, Source1 );
	SourceParamName Source2;	GenerateSourceParamName( *State, State->SourceToken[1], State->SourceAddressToken[1], WriteMask, Source2 );

	if( WriteMaskSize > 1 )
	{
		const ANSICHAR* CompareInstruction = "";
		if( State->Instruction->Opcode == D3DSIO_SLT )
		{
			CompareInstruction = "lessThan";			// <
		}
		else if( State->Instruction->Opcode == D3DSIO_SGE )
		{
			CompareInstruction = "greaterThanEqual";	// >=
		}
		else
		{
			appErrorf(TEXT("Unrecognized instruction in convert_compare!!"));
		}

		*State->OutputBuffer += FString::Printf(TEXT("vec%d(%s(%s, %s)));\n"), WriteMaskSize, ANSI_TO_TCHAR(CompareInstruction), ANSI_TO_TCHAR(Source1.SourceExpression), ANSI_TO_TCHAR(Source2.SourceExpression));
	}
	else if( State->Instruction->Opcode == D3DSIO_SLT )	// <
	{
		*State->OutputBuffer += FString::Printf(TEXT("(%s < %s)?1.0:0.0);\n"), ANSI_TO_TCHAR(Source1.SourceExpression), ANSI_TO_TCHAR(Source2.SourceExpression));
	}
	else if( State->Instruction->Opcode == D3DSIO_SGE )	// >=
	{
		*State->OutputBuffer += FString::Printf(TEXT("step(%s, %s));\n"), ANSI_TO_TCHAR(Source2.SourceExpression), ANSI_TO_TCHAR(Source1.SourceExpression));
	}
	else
	{
		appErrorf(TEXT("Unrecognized instruction in convert_compare!! (2)"));
	}
}

static void Convert_mad( ConversionState* State )
{
	DWORD WriteMask = WriteDestinationToTextBuffer( State, *State->OutputBuffer );

	SourceParamName Source1;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], WriteMask, Source1 );
	SourceParamName Source2;	GenerateSourceParamName( *State, State->SourceToken[1], State->SourceAddressToken[1], WriteMask, Source2 );
	SourceParamName Source3;	GenerateSourceParamName( *State, State->SourceToken[2], State->SourceAddressToken[2], WriteMask, Source3 );

	*State->OutputBuffer += FString::Printf(TEXT("(%s * %s) + %s);\n"), ANSI_TO_TCHAR(Source1.SourceExpression), ANSI_TO_TCHAR(Source2.SourceExpression), ANSI_TO_TCHAR(Source3.SourceExpression));
}

static void Convert_rcp( ConversionState* State )
{
	DWORD WriteMask = WriteDestinationToTextBuffer( State, *State->OutputBuffer );
	DWORD WriteMaskSize = GetWriteMaskSize( WriteMask );

	SourceParamName Source1;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], WriteMask, Source1 );

	if( WriteMaskSize > 1 )
	{
		*State->OutputBuffer += FString::Printf(TEXT("vec%d"), WriteMaskSize);
	}

	*State->OutputBuffer += FString::Printf(TEXT("( 1.0 / %s ) );\n"), ANSI_TO_TCHAR(Source1.SourceExpression));
}

static void Convert_expp( ConversionState* State )
{
	SourceParamName Source;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_0, Source );	// we only take x

	DWORD WriteMask = WriteDestinationToTextBuffer( State, *State->OutputBuffer );
	DWORD WriteMaskSize = GetWriteMaskSize( WriteMask );
	if( WriteMaskSize > 1 )
	{
		*State->OutputBuffer += FString::Printf(TEXT("vec%d"), WriteMaskSize );
	}

	*State->OutputBuffer += FString::Printf(TEXT("( exp2( %s ) ) );\n"), ANSI_TO_TCHAR(Source.SourceExpression));
}

static void Convert_lit( ConversionState* State )
{
	WriteDestinationToTextBuffer( State, *State->OutputBuffer );

	ANSICHAR WriteMaskString[6];	// .xyzw\0 = 6
	GetWriteMask( State->DestToken, WriteMaskString, State->Shader->bIsPixelShader );

	SourceParamName SourceX;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_0, SourceX );
	SourceParamName SourceY;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_1, SourceY );
	SourceParamName SourceW;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_3, SourceW );

	*State->OutputBuffer += FString::Printf(TEXT("vec4( 1.0, max( %s, 0.0 ), pow( max( %s, 0.0 ) * step( 0.0, %s ), clamp( %s, -128.0, 128.0 ) ), 1.0 )%s );\n"),
		ANSI_TO_TCHAR(SourceX.SourceExpression), ANSI_TO_TCHAR(SourceY.SourceExpression), ANSI_TO_TCHAR(SourceX.SourceExpression), ANSI_TO_TCHAR(SourceW.SourceExpression), ANSI_TO_TCHAR(WriteMaskString));
}

static void Convert_dst( ConversionState* State )
{
	WriteDestinationToTextBuffer( State, *State->OutputBuffer );

	ANSICHAR WriteMaskString[6];	// .xyzw\0 = 6
	GetWriteMask( State->DestToken, WriteMaskString, State->Shader->bIsPixelShader );

	SourceParamName Source0Y;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_1, Source0Y );
	SourceParamName Source0Z;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_2, Source0Z );
	SourceParamName Source1Y;	GenerateSourceParamName( *State, State->SourceToken[1], State->SourceAddressToken[1], D3DSP_WRITEMASK_1, Source1Y );
	SourceParamName Source1W;	GenerateSourceParamName( *State, State->SourceToken[1], State->SourceAddressToken[1], D3DSP_WRITEMASK_3, Source1W );

	*State->OutputBuffer += FString::Printf(TEXT("vec4( 1.0, %s * %s, %s, %s ) )%s;\n"), ANSI_TO_TCHAR(Source0Y.SourceExpression), ANSI_TO_TCHAR(Source1Y.SourceExpression),
		ANSI_TO_TCHAR(Source0Z.SourceExpression), ANSI_TO_TCHAR(Source1W.SourceExpression), ANSI_TO_TCHAR(WriteMaskString));
}

static void Convert_lrp( ConversionState* State )
{
	DWORD WriteMask = WriteDestinationToTextBuffer( State, *State->OutputBuffer );

	SourceParamName Source1;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], WriteMask, Source1 );
	SourceParamName Source2;	GenerateSourceParamName( *State, State->SourceToken[1], State->SourceAddressToken[1], WriteMask, Source2 );
	SourceParamName Source3;	GenerateSourceParamName( *State, State->SourceToken[2], State->SourceAddressToken[2], WriteMask, Source3 );

	*State->OutputBuffer += FString::Printf(TEXT(" mix(%s, %s, %s) );\n"), ANSI_TO_TCHAR(Source3.SourceExpression), ANSI_TO_TCHAR(Source2.SourceExpression), ANSI_TO_TCHAR(Source1.SourceExpression));
}

static void Convert_pow( ConversionState* State )
{
	DWORD WriteMask = WriteDestinationToTextBuffer( State, *State->OutputBuffer );
	DWORD WriteMaskSize = GetWriteMaskSize( WriteMask );

	SourceParamName Source1;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_0, Source1 );
	SourceParamName Source2;	GenerateSourceParamName( *State, State->SourceToken[1], State->SourceAddressToken[1], D3DSP_WRITEMASK_0, Source2 );

	if( WriteMaskSize > 1 )
	{
		*State->OutputBuffer += FString::Printf(TEXT("vec%d"), WriteMaskSize );
	}

	*State->OutputBuffer += FString::Printf(TEXT("( pow( abs( %s ), %s ) ) );\n"), ANSI_TO_TCHAR(Source1.SourceExpression), ANSI_TO_TCHAR(Source2.SourceExpression));
}

static void Convert_log( ConversionState* State )
{
	DWORD WriteMask = WriteDestinationToTextBuffer( State, *State->OutputBuffer );
	DWORD WriteMaskSize = GetWriteMaskSize( WriteMask );

	SourceParamName Source;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_0, Source );

	if( WriteMaskSize > 1 )
	{
		*State->OutputBuffer += FString::Printf(TEXT("vec%d"), WriteMaskSize);
	}

	*State->OutputBuffer += FString::Printf(TEXT("( log2( abs( %s ) ) ) );\n"), ANSI_TO_TCHAR(Source.SourceExpression));
}

static void Convert_cross( ConversionState* State )
{
	DWORD SourceMask = D3DSP_WRITEMASK_0 | D3DSP_WRITEMASK_1 | D3DSP_WRITEMASK_2;
	ANSICHAR WriteMaskString[6];	// .xyzw\0 = 6

	GetWriteMask( State->DestToken, WriteMaskString, State->Shader->bIsPixelShader );
	WriteDestinationToTextBuffer( State, *State->OutputBuffer );

	SourceParamName Source1;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], SourceMask, Source1 );
	SourceParamName Source2;	GenerateSourceParamName( *State, State->SourceToken[1], State->SourceAddressToken[1], SourceMask, Source2 );

	*State->OutputBuffer += FString::Printf(TEXT("cross(%s, %s)%s );\n"), ANSI_TO_TCHAR(Source1.SourceExpression), ANSI_TO_TCHAR(Source2.SourceExpression), ANSI_TO_TCHAR(WriteMaskString));
}

static void Convert_sincos( ConversionState* State )
{
	DWORD WriteMask = WriteDestinationToTextBuffer( State, *State->OutputBuffer );

	SourceParamName Source1;
	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_0, Source1 );

	// This instruction doesn't touch z and w components, so we leave them as well
	switch( WriteMask )
	{
	case D3DSP_WRITEMASK_0:						*State->OutputBuffer += FString::Printf(TEXT("cos(%s));\n"), ANSI_TO_TCHAR(Source1.SourceExpression));															break;
	case D3DSP_WRITEMASK_1:						*State->OutputBuffer += FString::Printf(TEXT("sin(%s));\n"), ANSI_TO_TCHAR(Source1.SourceExpression));															break;
	case (D3DSP_WRITEMASK_0|D3DSP_WRITEMASK_1):	*State->OutputBuffer += FString::Printf(TEXT("vec2(cos(%s),sin(%s)));\n"), ANSI_TO_TCHAR(Source1.SourceExpression), ANSI_TO_TCHAR(Source1.SourceExpression));	break;
	default:									check(0); break;
	}
}

static void Convert_rsq( ConversionState* State )
{
	DWORD WriteMask = WriteDestinationToTextBuffer( State, *State->OutputBuffer );
	DWORD WriteMaskSize = GetWriteMaskSize( WriteMask );

	SourceParamName Source;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_3, Source );

	if( WriteMaskSize > 1 )
	{
		*State->OutputBuffer += FString::Printf(TEXT("vec%d"), WriteMaskSize);
	}

	*State->OutputBuffer += FString::Printf(TEXT("( inversesqrt( %s ) ) );\n"), ANSI_TO_TCHAR(Source.SourceExpression));
}

static void Convert_cnd( ConversionState* State )
{
	appErrorf(TEXT("cnd conversion not implemented yet for ps1.4 and above, add it."));
}

static void Convert_cmp( ConversionState* State )
{
	// Analyze all parameters
	DWORD DestRegNum = State->DestToken & D3DSP_REGNUM_MASK;
	DWORD SourceRegNum0 = State->SourceToken[0] & D3DSP_REGNUM_MASK;
	DWORD SourceRegNum1 = State->SourceToken[1] & D3DSP_REGNUM_MASK;
	DWORD SourceRegNum2 = State->SourceToken[2] & D3DSP_REGNUM_MASK;
	DWORD DestRegType = GetRegisterType( State->DestToken );
	DWORD SourceRegType0 = GetRegisterType( State->SourceToken[0] );
	DWORD SourceRegType1 = GetRegisterType( State->SourceToken[1] );
	DWORD SourceRegType2 = GetRegisterType( State->SourceToken[2] );

	// We'll use several GLSL instructions as translation of this instruction. But outputs from earlier instructions might overwrite inputs to later instructions.
	// If this can happen, use temporary register.
	UBOOL bUseTemporaryRegister = ( ( ( SourceRegNum0 == DestRegNum ) && ( SourceRegType0 == DestRegType )  ) ||
		( ( SourceRegNum1 == DestRegNum ) && ( SourceRegType1 == DestRegType )  ) ||
		( ( SourceRegNum2 == DestRegNum ) && ( SourceRegType2 == DestRegType )  ) );

	for( DWORD Index = 0; Index < 4; ++Index )
	{
		DWORD WriteMask = 0;
		DWORD CmpChannel = 0;

		for( DWORD j = 0; j < 4; ++j )
		{
			if( ( ( State->SourceToken[0] >> ( D3DSP_SWIZZLE_SHIFT + 2*j ) ) & 0x3 ) == Index )
			{
				CmpChannel = D3DSP_WRITEMASK_0 << j;
				WriteMask |= CmpChannel;
			}
		}

		ANSICHAR WriteMaskString[6];	// .xyzw\0 = 6
		if( bUseTemporaryRegister )
		{
			WriteMask = GetWriteMask( State->DestToken & ( WriteMask | ~D3DSP_SWIZZLE_MASK ), WriteMaskString, State->Shader->bIsPixelShader );
			if( !WriteMask )
			{
				continue;	// this channel isn't set in destination
			}
			*State->OutputBuffer += FString::Printf(TEXT("InstrHelpTemp%s = ( "), ANSI_TO_TCHAR(WriteMaskString));
		}
		else
		{
			WriteMask = WriteDestinationToTextBuffer( State, *State->OutputBuffer, State->DestToken & ( WriteMask | ~D3DSP_SWIZZLE_MASK ) );
			if( !WriteMask )
			{
				continue;	// this channel isn't set in destination
			}
		}

		// Prepare swizzles for all sources
		SourceParamName Source0;
		SourceParamName Source1;
		SourceParamName Source2;
		GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], CmpChannel, Source0 );
		GenerateSourceParamName( *State, State->SourceToken[1], State->SourceAddressToken[1], WriteMask, Source1 );
		GenerateSourceParamName( *State, State->SourceToken[2], State->SourceAddressToken[2], WriteMask, Source2 );

		// Write out a translation line
		*State->OutputBuffer += FString::Printf(TEXT("( %s >= 0.0 ) ? %s : %s );\n"), ANSI_TO_TCHAR(Source0.SourceExpression), ANSI_TO_TCHAR(Source1.SourceExpression), ANSI_TO_TCHAR(Source2.SourceExpression));
	}

	if( bUseTemporaryRegister )		// Move result from temporary register to destination register
	{
		WriteDestinationToTextBuffer( State, *State->OutputBuffer );

		ANSICHAR WriteMaskString[6];	// .xyzw\0 = 6
		GetWriteMask( State->DestToken, WriteMaskString, State->Shader->bIsPixelShader );
		*State->OutputBuffer += FString::Printf(TEXT("InstrHelpTemp%s );\n"), ANSI_TO_TCHAR(WriteMaskString));
	}
}

static void Convert_rep( ConversionState* State )
{
	DWORD RegType = GetRegisterType( State->SourceToken[0] );

	// Try to replace constant with its value, to help the dumb GLSL compiler unroll the loop.
	const DWORD* ConstantValues = NULL;
	if( RegType == D3DSPR_CONSTINT )
	{
		DWORD RegNum = State->SourceToken[0] & D3DSP_REGNUM_MASK;

		for( DWORD Index = 0; Index < State->Shader->ConstantsIntNum; ++Index )
		{
			if( State->Shader->ConstantsInt[Index].Index == RegNum )
			{
				ConstantValues = State->Shader->ConstantsInt[Index].Value; 
				break;
			}
		}
	}

	DWORD CurrentLoopDepth = State->Shader->CurrentLoopDepth;

	if( ConstantValues )
	{
		*State->OutputBuffer += FString::Printf(TEXT("for( LoopTemp%u = 0; LoopTemp%u < %u; LoopTemp%u++ ) {\n"),
			CurrentLoopDepth, CurrentLoopDepth, ConstantValues[0], CurrentLoopDepth );
	}
	else
	{
		SourceParamName Source;
		GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_0, Source );
		*State->OutputBuffer += FString::Printf(TEXT("for( LoopTemp%d = 0; LoopTemp%d < %s; LoopTemp%d++ )\n{\n"),
			CurrentLoopDepth, CurrentLoopDepth, ANSI_TO_TCHAR(Source.SourceExpression), CurrentLoopDepth );
		State->Shader->CurrentLoopDepth++;
	}
}

static void Convert_loop( ConversionState* State )
{
	DWORD RegType = GetRegisterType( State->SourceToken[1] );

	// Try to replace constant with its value, to help the dumb GLSL compiler unroll the loop.
	const DWORD* ConstantValues = NULL;
	if( RegType == D3DSPR_CONSTINT )
	{
		DWORD RegNum = State->SourceToken[1] & D3DSP_REGNUM_MASK;

		for( DWORD Index = 0; Index < State->Shader->ConstantsIntNum; ++Index )
		{
			if( State->Shader->ConstantsInt[Index].Index == RegNum )
			{
				ConstantValues = State->Shader->ConstantsInt[Index].Value; 
				break;
			}
		}
	}

	DWORD CurrentLoopDepth = State->Shader->CurrentLoopDepth;

	if( ConstantValues )
	{
		if( ConstantValues[2] == 0 )
		{
			*State->OutputBuffer += FString::Printf(TEXT("for( Loop%u = %d, LoopTemp%u = 0; LoopTemp%u < %d; LoopTemp%u++ ) {\n"), CurrentLoopDepth, ConstantValues[1], CurrentLoopDepth, CurrentLoopDepth, ConstantValues[0], CurrentLoopDepth );
		}
		else
		{
			ANSICHAR Char = ( ConstantValues[2] > 0 ) ? '<' : '>';
			*State->OutputBuffer += FString::Printf(TEXT("for( Loop%u = %d; Loop%u %c ( %d * %d + %d ); Loop%u += %d ) {\n"), CurrentLoopDepth, ConstantValues[1], CurrentLoopDepth, Char, ConstantValues[0], ConstantValues[2], ConstantValues[1], CurrentLoopDepth, ConstantValues[2] );
		}
	}
	else	// can't help it, no loop unrolling
	{
		SourceParamName Source1;
		GenerateSourceParamName( *State, State->SourceToken[1], State->SourceAddressToken[1], D3DSP_WRITEMASK_ALL, Source1 );

		DWORD CurrentRegNum = State->Shader->CurrentLoopRegNum;
		*State->OutputBuffer += FString::Printf(TEXT("for( LoopTemp%u = 0, Loop%u = %s.y; LoopTemp%u < %s.x; LoopTemp%u++, Loop%u += %s.z ) {\n"),
			CurrentLoopDepth, CurrentRegNum, ANSI_TO_TCHAR(Source1.SourceExpression), CurrentLoopDepth, ANSI_TO_TCHAR(Source1.SourceExpression), CurrentLoopDepth, CurrentRegNum, ANSI_TO_TCHAR(Source1.SourceExpression));
	}

	State->Shader->CurrentLoopRegNum++;
	State->Shader->CurrentLoopDepth++;
}

static void Convert_end( ConversionState* State )
{
	*State->OutputBuffer += TEXT("}\n");

	if( State->Instruction->Opcode == D3DSIO_ENDLOOP )
	{
		State->Shader->CurrentLoopRegNum--;
		State->Shader->CurrentLoopDepth--;
	}
	else if( State->Instruction->Opcode == D3DSIO_ENDREP )
	{
		State->Shader->CurrentLoopDepth--;
	}
}

static void Convert_if( ConversionState* State )
{
	SourceParamName Source;
	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_0, Source );
	*State->OutputBuffer += FString::Printf(TEXT("if( %s )\n{\n"), ANSI_TO_TCHAR(Source.SourceExpression));
}

static void Convert_else( ConversionState* State )
{
	*State->OutputBuffer += TEXT("}\nelse\n{\n");
}

static void Convert_dp2add( ConversionState* State )
{
	ANSICHAR WriteMaskString[6];	// .xyzw\0 = 6
	DWORD WriteMask = GetWriteMask( State->DestToken, WriteMaskString, State->Shader->bIsPixelShader );

	SourceParamName Source0;
	SourceParamName Source1;
	SourceParamName Source2;
	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_0 | D3DSP_WRITEMASK_1, Source0 );
	GenerateSourceParamName( *State, State->SourceToken[1], State->SourceAddressToken[1], D3DSP_WRITEMASK_0 | D3DSP_WRITEMASK_1, Source1 );
	GenerateSourceParamName( *State, State->SourceToken[2], State->SourceAddressToken[2], D3DSP_WRITEMASK_0, Source2 );

	WriteDestinationToTextBuffer( State, *State->OutputBuffer );
	DWORD MaskSize = GetWriteMaskSize( WriteMask );
	if( MaskSize == 1 )
	{
		*State->OutputBuffer += FString::Printf(TEXT("dot( %s, %s ) + %s );\n"), ANSI_TO_TCHAR(Source0.SourceExpression), ANSI_TO_TCHAR(Source1.SourceExpression), ANSI_TO_TCHAR(Source2.SourceExpression));
	}
	else
	{
		*State->OutputBuffer += FString::Printf(TEXT("vec%d( dot( %s, %s ) + %s ) );\n"), MaskSize, ANSI_TO_TCHAR(Source0.SourceExpression), ANSI_TO_TCHAR(Source1.SourceExpression), ANSI_TO_TCHAR(Source2.SourceExpression));
	}
}

static void Convert_texcoord( ConversionState* State )
{
	// WARNING: works only for 2D textures atm.
	WriteDestinationToTextBuffer( State, *State->OutputBuffer );

	ANSICHAR WriteMaskString[6];	// .xyzw\0 = 6
	DWORD WriteMask = GetWriteMask( State->DestToken, WriteMaskString, State->Shader->bIsPixelShader );

	DWORD RegNum = State->DestToken & D3DSP_REGNUM_MASK;
	*State->OutputBuffer += FString::Printf(TEXT("clamp( gl_TexCoord[%u], 0.0, 1.0 )%s );\n"), RegNum, ANSI_TO_TCHAR(WriteMaskString));
}

static void Convert_texbem( ConversionState* State )
{
	appErrorf(TEXT("texbem conversion not implemented yet."));
}

static void Convert_texreg2ar( ConversionState* State )
{
	DWORD SamplerIndex = State->DestToken & D3DSP_REGNUM_MASK;
	ANSICHAR WriteMaskString[6];	// .xyzw\0 = 6
	GetWriteMask( State->DestToken, WriteMaskString, State->Shader->bIsPixelShader );
	WriteDestinationToTextBuffer( State, *State->OutputBuffer );
	SourceParamName Source;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_ALL, Source );
	*State->OutputBuffer += FString::Printf(TEXT("texture2D( PSampler%u, %s.wx )%s );\n"), SamplerIndex, ANSI_TO_TCHAR(Source.SourceExpression), ANSI_TO_TCHAR(WriteMaskString));
}

static void Convert_texreg2gb( ConversionState* State )
{
	DWORD SamplerIndex = State->DestToken & D3DSP_REGNUM_MASK;
	ANSICHAR WriteMaskString[6];	// .xyzw\0 = 6
	GetWriteMask( State->DestToken, WriteMaskString, State->Shader->bIsPixelShader );
	WriteDestinationToTextBuffer( State, *State->OutputBuffer );
	SourceParamName Source;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_ALL, Source );
	*State->OutputBuffer += FString::Printf(TEXT("texture2D( PSampler%u, %s.yz )%s );\n"), SamplerIndex, ANSI_TO_TCHAR(Source.SourceExpression), ANSI_TO_TCHAR(WriteMaskString));
}

static void Convert_texm3x2pad( ConversionState* state )	{ appErrorf(TEXT("texm3x2pad conversion not implemented yet, add it when you need it.")); }
static void Convert_texm3x2tex( ConversionState* state )	{ appErrorf(TEXT("texm3x2tex conversion not implemented yet, add it when you need it.")); }
static void Convert_texm3x3pad( ConversionState* state )	{ appErrorf(TEXT("texm3x3pad conversion not implemented yet, add it when you need it.")); }
static void Convert_texm3x3spec( ConversionState* state )	{ appErrorf(TEXT("texm3x3spec conversion not implemented yet, add it when you need it.")); }
static void Convert_texm3x3vspec( ConversionState* state )	{ appErrorf(TEXT("texm3x3vspec conversion not implemented yet, add it when you need it.")); }
static void Convert_texm3x3tex( ConversionState* state )	{ appErrorf(TEXT("texm3x3tex conversion not implemented yet, add it when you need it.")); }

static void Convert_texkill( ConversionState* State )
{
	DestParamName Dest;
	GenerateDestParamName( *State, State->DestToken, 0, Dest );
	const ANSICHAR* CompareMask = ( State->Shader->ShaderVersionMajor >= 2 ) ? "xyzw" : "xyz";
	*State->OutputBuffer += FString::Printf(TEXT("if( any( lessThan( %s.%s, vec4(0.0) ) ) ) discard;\n"), ANSI_TO_TCHAR(Dest.RegisterName), ANSI_TO_TCHAR(CompareMask));
}

struct SampleFunction
{
	const ANSICHAR* Name;
	DWORD		CoordMask;
};

static SampleFunction GetSampleFunction( DWORD SamplerType, UBOOL bIsProjected )
{
	SampleFunction Func = { NULL, 0 };

	switch( SamplerType )
	{
	case 1:				Func.Name = bIsProjected ? "texture1DProj" : "texture1D";	Func.CoordMask = D3DSP_WRITEMASK_0;											break;
	case D3DSTT_2D:		Func.Name = bIsProjected ? "texture2DProj" : "texture2D";	Func.CoordMask = D3DSP_WRITEMASK_0 | D3DSP_WRITEMASK_1;						break;
	case D3DSTT_CUBE:	Func.Name = "textureCube";									Func.CoordMask = D3DSP_WRITEMASK_0 | D3DSP_WRITEMASK_1 | D3DSP_WRITEMASK_2;	break;
	case D3DSTT_VOLUME:	Func.Name = bIsProjected ? "texture3DProj" : "texture3D";	Func.CoordMask = D3DSP_WRITEMASK_0 | D3DSP_WRITEMASK_1 | D3DSP_WRITEMASK_2;	break;
	default:			appErrorf(TEXT("Unknown sampler type in GetSampleFunction()!"));														break;
	}

	return Func;
}

static void Convert_texldd( ConversionState* State )
{
	ANSICHAR WriteMaskString[6];	// .xyzw\0 = 6
	GetWriteMask( State->DestToken, WriteMaskString, State->Shader->bIsPixelShader );
	WriteDestinationToTextBuffer( State, *State->OutputBuffer );
	SourceParamName Source0;
	SourceParamName Source2;
	SourceParamName Source3;
	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_0 | D3DSP_WRITEMASK_1, Source0 );
	DWORD SamplerIndex = State->SourceToken[1] & D3DSP_REGNUM_MASK;
	GenerateSourceParamName( *State, State->SourceToken[2], State->SourceAddressToken[2], D3DSP_WRITEMASK_0 | D3DSP_WRITEMASK_1, Source2 );
	GenerateSourceParamName( *State, State->SourceToken[3], State->SourceAddressToken[3], D3DSP_WRITEMASK_0 | D3DSP_WRITEMASK_1, Source3 );

	*State->OutputBuffer += FString::Printf(TEXT("textureGrad( PSampler%u, %s, %s, %s )%s );\n"),
		SamplerIndex, 
		ANSI_TO_TCHAR(Source0.SourceExpression), 
		ANSI_TO_TCHAR(Source2.SourceExpression), 
		ANSI_TO_TCHAR(Source3.SourceExpression), 
		ANSI_TO_TCHAR(WriteMaskString));
}

static void Convert_tex( ConversionState* State )
{
	DWORD SamplerIndex;
	DWORD CoordMask = 0;

	WriteDestinationToTextBuffer( State, *State->OutputBuffer );

	// Determine state of texture projection
	UBOOL bIsProjected = FALSE;

	SamplerIndex = State->SourceToken[1] & D3DSP_REGNUM_MASK;
	if( State->InstructionCode & D3DSI_TEXLD_PROJECT )
	{
		CoordMask = D3DSP_WRITEMASK_3;
		bIsProjected = TRUE;
	}

	DWORD SamplerType = State->Shader->Regs.SamplersUsage[SamplerIndex] & D3DSP_TEXTURETYPE_MASK;
	SampleFunction SampleFunction = GetSampleFunction( SamplerType, bIsProjected );
	CoordMask |= SampleFunction.CoordMask;

	ANSICHAR SwizzleString[6];	// .xyzw\0 = 6
	GetSwizzle( State->SourceToken[1], State->DestToken, SwizzleString, State->Shader->bIsPixelShader );

	SourceParamName Source;
	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], CoordMask, Source );
	if( State->InstructionCode & D3DSI_TEXLD_BIAS )
	{
		SourceParamName BiasParam;
		GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_3, BiasParam );
		*State->OutputBuffer += FString::Printf(TEXT("%s( PSampler%u, %s, %s )%s );\n"), ANSI_TO_TCHAR(SampleFunction.Name), SamplerIndex, ANSI_TO_TCHAR(Source.SourceExpression), ANSI_TO_TCHAR(BiasParam.SourceExpression), ANSI_TO_TCHAR(SwizzleString));
	}
	else
	{
		*State->OutputBuffer += FString::Printf(TEXT("%s( PSampler%u, %s )%s );\n"), ANSI_TO_TCHAR(SampleFunction.Name), SamplerIndex, ANSI_TO_TCHAR(Source.SourceExpression), ANSI_TO_TCHAR(SwizzleString));
	}
}

static inline const TCHAR* ConvertToComparisonOperator( const DWORD InstructionCode )
{
	DWORD InstructionCodeControl = ( ( InstructionCode & D3DSP_OPCODESPECIFICCONTROL_MASK ) >> D3DSP_OPCODESPECIFICCONTROL_SHIFT );
	switch( InstructionCodeControl )
	{
	case D3DSPC_GT: return TEXT(">");
	case D3DSPC_EQ: return TEXT("==");
	case D3DSPC_GE: return TEXT(">=");
	case D3DSPC_LT: return TEXT("<");
	case D3DSPC_NE: return TEXT("!=");
	case D3DSPC_LE: return TEXT("<=");
	default:
		appErrorf(TEXT("Unrecognized opcode-specific control, can't choose comparison operator !!!. Opcode 0x%x"), ( InstructionCode & D3DSI_OPCODE_MASK ) );
		return TEXT("");
	}
}

static void Convert_ifc( ConversionState* State )
{
	SourceParamName Source0; GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_0, Source0 );
	SourceParamName Source1; GenerateSourceParamName( *State, State->SourceToken[1], State->SourceAddressToken[1], D3DSP_WRITEMASK_0, Source1 );

	*State->OutputBuffer += FString::Printf(TEXT("if( %s %s %s ) {\n"), ANSI_TO_TCHAR(Source0.SourceExpression), ConvertToComparisonOperator( State->InstructionCode ), ANSI_TO_TCHAR(Source1.SourceExpression));
}

static void Convert_texldl( ConversionState* State )
{
	WriteDestinationToTextBuffer( State, *State->OutputBuffer );

	ANSICHAR SwizzleString[6];	// .xyzw\0 = 6
	GetSwizzle( State->SourceToken[1], State->DestToken, SwizzleString, State->Shader->bIsPixelShader );

	DWORD SamplerIndex = State->SourceToken[1] & D3DSP_REGNUM_MASK;
	DWORD SamplerType = State->Shader->Regs.SamplersUsage[SamplerIndex] & D3DSP_TEXTURETYPE_MASK;
	SampleFunction SampleFunction = GetSampleFunction( SamplerType, false );

	SourceParamName Coord;			GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], SampleFunction.CoordMask, Coord );
	SourceParamName LevelOfDetail;	GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_3, LevelOfDetail );

	*State->OutputBuffer += FString::Printf(TEXT("%sLod( %sSampler%u, %s, %s )%s );\n"), ANSI_TO_TCHAR(SampleFunction.Name), State->Shader->bIsPixelShader ? TEXT("P") : TEXT("V"), SamplerIndex, ANSI_TO_TCHAR(Coord.SourceExpression), ANSI_TO_TCHAR(LevelOfDetail.SourceExpression), ANSI_TO_TCHAR(SwizzleString));
}

static void Convert_breakc( ConversionState* State )
{
	SourceParamName Source0; GenerateSourceParamName( *State, State->SourceToken[0], State->SourceAddressToken[0], D3DSP_WRITEMASK_0, Source0 );
	SourceParamName Source1; GenerateSourceParamName( *State, State->SourceToken[1], State->SourceAddressToken[1], D3DSP_WRITEMASK_0, Source1 );

	*State->OutputBuffer += FString::Printf(TEXT("if( %s %s %s ) break;\n"), ANSI_TO_TCHAR(Source0.SourceExpression), ConvertToComparisonOperator( State->InstructionCode ), ANSI_TO_TCHAR(Source1.SourceExpression));
}

const AsmInstructionData GVertexShaderInstructions[] =
{
	{ D3DSIO_NOP,			"nop",			0,	0,	NULL,					0,					0 },
	{ D3DSIO_MOV,			"mov",			1,	1,	Convert_mov,			0,					0 },
	{ D3DSIO_MOVA,			"mova",			1,	1,	Convert_mov,			D3DVS_VERSION(2,0),	-1 },
	{ D3DSIO_ADD,			"add",			1,	2,	Convert_math,			0,					0 },
	{ D3DSIO_SUB,			"sub",			1,	2,	Convert_math,			0,					0 },
	{ D3DSIO_MUL,			"mul",			1,	2,	Convert_math,			0,					0 },
	{ D3DSIO_DP3,			"dp3",			1,	2,	Convert_dot,			0,					0 },
	{ D3DSIO_DP4,			"dp4",			1,	2,	Convert_dot,			0,					0 },
	{ D3DSIO_M4x4,			"m4x4",			1,	2,	Convert_matmul,			0,					0 },
	{ D3DSIO_M4x3,			"m4x3",			1,	2,	Convert_matmul,			0,					0 },
	{ D3DSIO_M3x4,			"m3x4",			1,	2,	Convert_matmul,			0,					0 },
	{ D3DSIO_M3x3,			"m3x3",			1,	2,	Convert_matmul,			0,					0 },
	{ D3DSIO_M3x2,			"m3x2",			1,	2,	Convert_matmul,			0,					0 },
	{ D3DSIO_MIN,			"min",			1,	2,	Convert_map2gl,			0,					0 },
	{ D3DSIO_MAX,			"max",			1,	2,	Convert_map2gl,			0,					0 },
	{ D3DSIO_ABS,			"abs",			1,	1,	Convert_map2gl,			0,					0 },
	{ D3DSIO_EXP,			"exp",			1,	1,	Convert_map2gl,			0,					0 },	// full
	{ D3DSIO_LOG,			"log",			1,	1,	Convert_map2gl,			0,					0 },
	{ D3DSIO_LOGP,			"logp",			1,	1,	Convert_map2gl,			0,					0 },	// partial
	{ D3DSIO_FRC,			"frc",			1,	1,	Convert_map2gl,			0,					0 },
	{ D3DSIO_SGN,			"sgn",			1,	1,	Convert_map2gl,			0,					0 },
	{ D3DSIO_NRM,			"nrm",			1,	1,	Convert_map2gl,			0,					0 },
	{ D3DSIO_SLT,			"slt",			1,	2,	Convert_compare,		0,					0 },
	{ D3DSIO_SGE,			"sge",			1,	2,	Convert_compare,		0,					0 },
	{ D3DSIO_MAD,			"mad",			1,	3,	Convert_mad,			0,					0 },
	{ D3DSIO_RCP,			"rcp",			1,	1,	Convert_rcp,			0,					0 },
	{ D3DSIO_RSQ,			"rsq",			1,	1,	Convert_rsq,			0,					0 },
	{ D3DSIO_EXPP,			"expp",			1,	1,	Convert_expp,			0,					0 },	// partial
	{ D3DSIO_LIT,			"lit",			1,	1,	Convert_lit,			0,					0 },
	{ D3DSIO_DST,			"dst",			1,	2,	Convert_dst,			0,					0 },
	{ D3DSIO_LRP,			"lrp",			1,	3,	Convert_lrp,			0,					0 },
	{ D3DSIO_POW,			"pow",			1,	2,	Convert_pow,			0,					0 },
	{ D3DSIO_CRS,			"crs",			1,	2,	Convert_cross,			0,					0 },

	{ D3DSIO_SINCOS,		"sincos",		1,	3,	Convert_sincos,			D3DVS_VERSION(2,0),	D3DVS_VERSION(2,1) },	// earlier version
	{ D3DSIO_SINCOS,		"sincos",		1,	1,	Convert_sincos,			D3DVS_VERSION(3,0),	-1 },					// later version

	{ D3DSIO_DCL,			"dcl",			0,	2,	NULL,					0,					0 },	// register declaration
	{ D3DSIO_DEF,			"def",			1,	4,	NULL,					0,					0 },	// constant definition
	{ D3DSIO_DEFB,			"defb",			1,	1,	NULL,					0,					0 },	// constant definition
	{ D3DSIO_DEFI,			"defi",			1,	4,	NULL,					0,					0 },	// constant definition

	// Flow control
	{ D3DSIO_REP,			"rep",			0,	1,	Convert_rep,			D3DVS_VERSION(2,0),	-1 },
	{ D3DSIO_ENDREP,		"endrep",		0,	0,	Convert_end,			D3DVS_VERSION(2,0),	-1 },
	{ D3DSIO_ENDLOOP,		"endloop",		0,	0,	Convert_end,			D3DVS_VERSION(2,0),	-1 },
	{ D3DSIO_IF,			"if",			0,	1,	Convert_if,				D3DVS_VERSION(2,0),	-1 },
	{ D3DSIO_ELSE,			"else",			0,	0,	Convert_else,			D3DVS_VERSION(2,0),	-1 },
	{ D3DSIO_ENDIF,			"endif",		0,	0,	Convert_end,			D3DVS_VERSION(2,0),	-1 },
	{ D3DSIO_LOOP,			"loop",			0,	2,	Convert_loop,			D3DVS_VERSION(2,0),	-1 },
	{ D3DSIO_IFC,			"ifc",			0,	2,	Convert_ifc,			D3DVS_VERSION(2,1),	-1 },
	{ D3DSIO_TEXLDL,		"texldl",		1,	2,	Convert_texldl,			D3DVS_VERSION(3,0),	-1 },
	{ D3DSIO_BREAKC,		"breakc",		0,	2,	Convert_breakc,			D3DVS_VERSION(2,1),	-1 },

	{ 0,					"",				0,	0,	NULL,					0,					0 }	// end token
};

const AsmInstructionData GPixelShaderInstructions[] =
{
	{ D3DSIO_NOP,			"nop",			0,	0,	NULL,					0,					0 },
	{ D3DSIO_MOV,			"mov",			1,	1,	Convert_mov,			0,					0 },
	{ D3DSIO_ADD,			"add",			1,	2,	Convert_math,			0,					0 },
	{ D3DSIO_SUB,			"sub",			1,	2,	Convert_math,			0,					0 },
	{ D3DSIO_MUL,			"mul",			1,	2,	Convert_math,			0,					0 },
	{ D3DSIO_DP3,			"dp3",			1,	2,	Convert_dot,			0,					0 },
	{ D3DSIO_DP4,			"dp4",			1,	2,	Convert_dot,			0,					0 },
	{ D3DSIO_M4x4,			"m4x4",			1,	2,	Convert_matmul,			0,					0 },
	{ D3DSIO_M4x3,			"m4x3",			1,	2,	Convert_matmul,			0,					0 },
	{ D3DSIO_M3x4,			"m3x4",			1,	2,	Convert_matmul,			0,					0 },
	{ D3DSIO_M3x3,			"m3x3",			1,	2,	Convert_matmul,			0,					0 },
	{ D3DSIO_M3x2,			"m3x2",			1,	2,	Convert_matmul,			0,					0 },
	{ D3DSIO_MIN,			"min",			1,	2,	Convert_map2gl,			0,					0 },
	{ D3DSIO_MAX,			"max",			1,	2,	Convert_map2gl,			0,					0 },
	{ D3DSIO_ABS,			"abs",			1,	1,	Convert_map2gl,			0,					0 },
	{ D3DSIO_EXP,			"exp",			1,	1,	Convert_map2gl,			0,					0 },	// full
	{ D3DSIO_LOGP,			"logp",			1,	1,	Convert_map2gl,			0,					0 },	// partial
	{ D3DSIO_FRC,			"frc",			1,	1,	Convert_map2gl,			0,					0 },
	{ D3DSIO_NRM,			"nrm",			1,	1,	Convert_map2gl,			0,					0 },
	{ D3DSIO_SLT,			"slt",			1,	2,	Convert_compare,		0,					0 },
	{ D3DSIO_SGE,			"sge",			1,	2,	Convert_compare,		0,					0 },
	{ D3DSIO_MAD,			"mad",			1,	3,	Convert_mad,			0,					0 },
	{ D3DSIO_RCP,			"rcp",			1,	1,	Convert_rcp,			0,					0 },
	{ D3DSIO_RSQ,			"rsq",			1,	1,	Convert_rsq,			0,					0 },
	{ D3DSIO_LOG,			"log",			1,	1,	Convert_log,			0,					0 },	// full
	{ D3DSIO_EXPP,			"expp",			1,	1,	Convert_expp,			0,					0 },	// partial
	{ D3DSIO_DST,			"dst",			1,	2,	Convert_dst,			0,					0 },
	{ D3DSIO_LRP,			"lrp",			1,	3,	Convert_lrp,			0,					0 },
	{ D3DSIO_POW,			"pow",			1,	2,	Convert_pow,			0,					0 },
	{ D3DSIO_CRS,			"crs",			1,	2,	Convert_cross,			0,					0 },

	{ D3DSIO_SINCOS,		"sincos",		1,	3,	Convert_sincos,			D3DPS_VERSION(2,0),	D3DPS_VERSION(2,1) },	// earlier version
	{ D3DSIO_SINCOS,		"sincos",		1,	1,	Convert_sincos,			D3DPS_VERSION(3,0),	-1 },					// later version

	{ D3DSIO_DCL,			"dcl",			0,	2,	NULL,					0,					0 },	// register declaration
	{ D3DSIO_DEF,			"def",			1,	4,	NULL,					0,					0 },	// constant definition
	{ D3DSIO_DEFB,			"defb",			1,	1,	NULL,					0,					0 },	// constant definition
	{ D3DSIO_DEFI,			"defi",			1,	4,	NULL,					0,					0 },	// constant definition

	{ D3DSIO_CND,			"cnd",			1,	3,	Convert_cnd,			D3DPS_VERSION(1,0),	D3DPS_VERSION(1,4) },
	{ D3DSIO_PHASE,			"phase",		0,	0,	NULL,					0,					0 },
	{ D3DSIO_CMP,			"cmp",			1,	3,	Convert_cmp,			D3DPS_VERSION(1,2),	D3DPS_VERSION(3,0) },
	{ D3DSIO_DP2ADD,		"dp2add",		1,	3,	Convert_dp2add,			D3DPS_VERSION(2,0),	-1 },

	// Texturing instructions
	{ D3DSIO_TEXCOORD,		"texcoord",		1,	0,	Convert_texcoord,		0,					D3DPS_VERSION(1,3) },					// early version
	{ D3DSIO_TEXBEM,		"texbem",		1,	1,	Convert_texbem,			0,					D3DPS_VERSION(1,3) },
	{ D3DSIO_TEX,			"tex",			1,	0,	Convert_tex,			0,					D3DPS_VERSION(1,3) },					// early version

	{ D3DSIO_TEXBEML,		"texbeml",		1,	1,	Convert_texbem,			D3DPS_VERSION(1,0), D3DPS_VERSION(1,3) },
	{ D3DSIO_TEXREG2AR,		"texreg2ar",	1,	1,	Convert_texreg2ar,		D3DPS_VERSION(1,0), D3DPS_VERSION(1,3) },
	{ D3DSIO_TEXREG2GB,		"texreg2gb",	1,	1,	Convert_texreg2gb,		D3DPS_VERSION(1,0), D3DPS_VERSION(1,3) },
	{ D3DSIO_TEXM3x2PAD,	"texm3x2pad",	1,	1,	Convert_texm3x2pad,		D3DPS_VERSION(1,0), D3DPS_VERSION(1,3) },
	{ D3DSIO_TEXM3x2TEX,	"texm3x2tex",	1,	1,	Convert_texm3x2tex,		D3DPS_VERSION(1,0), D3DPS_VERSION(1,3) },
	{ D3DSIO_TEXM3x3PAD,	"texm3x3pad",	1,	1,	Convert_texm3x3pad,		D3DPS_VERSION(1,0), D3DPS_VERSION(1,3) },
	{ D3DSIO_TEXM3x3SPEC,	"texm3x3spec",	1,	2,	Convert_texm3x3spec,	D3DPS_VERSION(1,0), D3DPS_VERSION(1,3) },
	{ D3DSIO_TEXM3x3VSPEC,	"texm3x3vspec",	1,	1,	Convert_texm3x3vspec,	D3DPS_VERSION(1,0), D3DPS_VERSION(1,3) },
	{ D3DSIO_TEXM3x3TEX,	"texm3x3tex",	1,	1,	Convert_texm3x3tex,		D3DPS_VERSION(1,0), D3DPS_VERSION(1,3) },
	{ D3DSIO_TEXKILL,		"texkill",		1,	0,	Convert_texkill,		D3DPS_VERSION(1,0),	D3DPS_VERSION(3,0) },
	{ D3DSIO_TEX,			"texld",		1,	1,	Convert_tex,			D3DPS_VERSION(1,4),	D3DPS_VERSION(1,4) },	// version for ps.1.4
	{ D3DSIO_TEX,			"texld",		1,	2,	Convert_tex,			D3DPS_VERSION(2,0),	-1 },					// late version
	{ D3DSIO_TEXCOORD,		"texcrd",		1,	1,	Convert_texcoord,		D3DPS_VERSION(1,4),	D3DPS_VERSION(1,4) },	// version for ps.1.4
	{ D3DSIO_TEXLDD,		"texldd",		1,	4,	Convert_texldd,			D3DPS_VERSION(3,0),	D3DPS_VERSION(3,0) },

	// Flow control
	{ D3DSIO_REP,			"rep",			0,	1,	Convert_rep,			D3DPS_VERSION(2,1),	-1 },
	{ D3DSIO_ENDREP,		"endrep",		0,	0,	Convert_end,			D3DPS_VERSION(2,1),	-1 },
	{ D3DSIO_IF,			"if",			0,	1,	Convert_if,				D3DPS_VERSION(2,1),	-1 },
	{ D3DSIO_ELSE,			"else",			0,	0,	Convert_else,			D3DPS_VERSION(2,1),	-1 },
	{ D3DSIO_ENDIF,			"endif",		0,	0,	Convert_end,			D3DPS_VERSION(2,1),	-1 },
	{ D3DSIO_ENDLOOP,		"endloop",		0,	0,	Convert_end,			D3DPS_VERSION(3,0),	-1 },
	{ D3DSIO_LOOP,			"loop",			0,	2,	Convert_loop,			D3DPS_VERSION(3,0),	-1 },
	{ D3DSIO_DSX,			"dsx",			1,	1,	Convert_map2gl,			D3DPS_VERSION(2,1),	-1 },
	{ D3DSIO_DSY,			"dsy",			1,	1,	Convert_map2gl,			D3DPS_VERSION(2,1),	-1 },
	{ D3DSIO_IFC,			"ifc",			0,	2,	Convert_ifc,			D3DPS_VERSION(2,1),	-1 },
	{ D3DSIO_TEXLDL,		"texldl",		1,	2,	Convert_texldl,			D3DPS_VERSION(3,0),	-1 },
	{ D3DSIO_BREAKC,		"breakc",		0,	2,	Convert_breakc,			D3DPS_VERSION(2,1),	-1 },

	{ 0,					"",				0,	0,	NULL,					0,					0 }	// end token
};

static DWORD CalcAbsoluteConstOffset( const DWORD Instruction )
{
	DWORD RegNum = Instruction & D3DSP_REGNUM_MASK;
	INT RegType = GetRegisterType( Instruction );

	switch( RegType )
	{
	case D3DSPR_CONST:	return RegNum;
	case D3DSPR_CONST2:	return RegNum+2048;
	case D3DSPR_CONST3:	return RegNum+4096;
	case D3DSPR_CONST4:	return RegNum+6144;
	default:
		appErrorf(TEXT("Unsupported register type for constant offset calculation! : %d"), RegType);
		return 0;
	}
}

UBOOL D3DShaderData::Init( const DWORD* InBytecode )
{
	Clear();

	const DWORD* TokenPtr = InBytecode;
	while( *TokenPtr != D3DSIO_END )
	{
		// Version token?
		DWORD VerToken = *TokenPtr >> 16;
		if( ( VerToken == 0xFFFE ) || ( VerToken == 0xFFFF ) )
		{
			ShaderVersion = *TokenPtr;
			bIsPixelShader = ( VerToken == 0xFFFF );
			ShaderVersionMajor = D3DSHADER_VERSION_MAJOR(ShaderVersion);
			ShaderVersionMinor = D3DSHADER_VERSION_MINOR(ShaderVersion);
			InstructionsList = bIsPixelShader ? GPixelShaderInstructions : GVertexShaderInstructions;
			++TokenPtr;
		}
		else if( ( *TokenPtr & D3DSI_OPCODE_MASK ) == D3DSIO_COMMENT )		// Comment?
		{
			DWORD CommentLen = (*TokenPtr & D3DSI_COMMENTSIZE_MASK ) >> D3DSI_COMMENTSIZE_SHIFT;
			++TokenPtr;
			TokenPtr += CommentLen;
		}
		else
		{
			// Normal instruction, apparently
			DWORD InstructionCode = *TokenPtr++;
			const AsmInstructionData* Instr = IdentifyInstruction( InstructionCode );

			if( !Instr )
			{
				appErrorf(TEXT("Unrecognized instruction when parsing bytecode"));
			}
			else
			{
				switch( Instr->Opcode )
				{
				case D3DSIO_DCL:
					TokenPtr += 2;
					break;

				case D3DSIO_DEF:
					TokenPtr += 5;
					break;

				case D3DSIO_DEFI:
					TokenPtr += 5;
					break;

				case D3DSIO_DEFB:
					TokenPtr += 2;
					break;

				default:
					// Destination param
					if( Instr->HasDestination )
					{
						DWORD AddressToken;
						ReadParam( TokenPtr, AddressToken );
					}

					// Skip predication token, if preset
					if( InstructionCode & D3DSHADER_INSTRUCTION_PREDICATED )
					{
						TokenPtr++;
					}

					// Source params
					for( DWORD Index = 0; Index < Instr->SourceParamsNum; ++Index )
					{
						DWORD AddressToken;
						ReadParam( TokenPtr, AddressToken );
					}
				};
			}
		}
	}

	BytecodeLength = (TokenPtr+1-InBytecode) * sizeof(DWORD);
	Bytecode = (DWORD*)appMalloc( BytecodeLength );
	appMemcpy( Bytecode, InBytecode, BytecodeLength );

	return TRUE;
}

void D3DShaderData::InitLimits( void )
{
	if( bIsPixelShader )
	{
		MaxAttributes		= 0;
		MaxAddressRegs		= 0;	// no address registers in pixel shaders
		MaxPackedOutputs	= 0;

		switch( ShaderVersion )
		{
		case D3DPS_VERSION(1,0):
		case D3DPS_VERSION(1,1):
		case D3DPS_VERSION(1,2):
		case D3DPS_VERSION(1,3):

			MaxTemporaries	= 2;	MaxConstFloats	= 8;	MaxConstInts	= 0;	MaxConstBools	= 0;
			MaxTexcoords	= 4;	MaxSamplers		= 4;	MaxPackedInputs	= 0;	MaxLabels		= 0;
			break;

		case D3DPS_VERSION(1,4):

			MaxTemporaries	= 6;	MaxConstFloats	= 8;	MaxConstInts	= 0;	MaxConstBools	= 0;
			MaxTexcoords	= 6;	MaxSamplers		= 6;	MaxPackedInputs	= 0;	MaxLabels		= 0;
			break;

		case D3DPS_VERSION(2,0):
		case D3DPS_VERSION(2,1):

			MaxTemporaries	= 32;	MaxConstFloats	= 32;	MaxConstInts	= 16;	MaxConstBools	= 16;
			MaxTexcoords	= 8;	MaxSamplers		= 16;	MaxPackedInputs	= 0;	MaxLabels		= 16;
			break;

		case D3DPS_VERSION(3,0):

			MaxTemporaries	= 32;	MaxConstFloats	= 224;	MaxConstInts	= 16;	MaxConstBools	= 16;
			MaxTexcoords	= 0;	MaxSamplers		= 16;	MaxPackedInputs	= 12;	MaxLabels		= 16;
			break;

		default:	// assumming better than previous ones

			warnf(TEXT("Unrecognized pixel shader version: 0x%x"), ShaderVersion);

			MaxTemporaries	= 32;	MaxConstFloats	= 224;	MaxConstInts	= 16;	MaxConstBools	= 16;
			MaxTexcoords	= 0;	MaxSamplers		= 16;	MaxPackedInputs	= 12;	MaxLabels		= 16;
			break;
		};

		SafeConstFloats = MaxConstFloats;
	}
	else
	{
		MaxTexcoords	= 0;	// no texture in vertex shaders
		MaxAttributes	= 16;
		MaxPackedInputs	= 0;

		switch( ShaderVersion )
		{
		case D3DVS_VERSION(1,0):
		case D3DVS_VERSION(1,1):

			MaxTemporaries		= 12;	MaxConstFloats		= 96;	MaxConstBools		= 0;	MaxConstInts		= 0;
			MaxAddressRegs		= 1;	MaxPackedOutputs	= 0;	MaxSamplers			= 0;	MaxLabels			= 0;
			break;

		case D3DVS_VERSION(2,0):
		case D3DVS_VERSION(2,1):

			MaxTemporaries		= 12;	MaxConstFloats		= 256;	MaxConstBools		= 16;	MaxConstInts		= 16;
			MaxAddressRegs		= 1;	MaxPackedOutputs	= 0;	MaxSamplers			= 0;	MaxLabels			= 16;
			break;

		case D3DVS_VERSION(3,0):

			MaxTemporaries		= 32;	MaxConstFloats		= 256;	MaxConstBools		= 32;	MaxConstInts		= 32;
			MaxAddressRegs		= 1;	MaxPackedOutputs	= 12;	MaxSamplers			= 4;	MaxLabels			= 16;
			break;

		default:	// assumming better than previous ones

			warnf(TEXT("Unrecognized vertex shader version: 0x%x"), ShaderVersion);

			MaxTemporaries		= 32;	MaxConstFloats		= 256;	MaxConstBools		= 32;	MaxConstInts		= 32;
			MaxAddressRegs		= 1;	MaxPackedOutputs	= 12;	MaxSamplers			= 4;	MaxLabels			= 16;
			break;
		};

		SafeConstFloats = MaxConstFloats;
	}
}

void D3DShaderData::DetermineUsedRegisters()
{
	DWORD CurrentLoopDepth = 0;
	DWORD MaxLoopDepth = 0;

	// Clear the Regs struct
	appMemzero(&Regs, sizeof(Regs));
	Regs.BumpEnvMatrixSamplerNum = -1;
	Regs.LuminanceParams = -1;

	const DWORD* TokenPtr = Bytecode;

	while( *TokenPtr != D3DSIO_END )
	{
		DWORD VerToken = *TokenPtr >> 16;
		if( ( VerToken == 0xFFFE ) || ( VerToken == 0xFFFF ) )				// Skip version token
		{
			++TokenPtr;
			continue;
		}
		else if( ( *TokenPtr & D3DSI_OPCODE_MASK ) == D3DSIO_COMMENT )		// Skip comments
		{
			DWORD CommentLen = (*TokenPtr & D3DSI_COMMENTSIZE_MASK ) >> D3DSI_COMMENTSIZE_SHIFT;
			++TokenPtr;
			TokenPtr += CommentLen;
			continue;
		}

		DWORD InstructionCode = *TokenPtr++;
		const AsmInstructionData* Instr = IdentifyInstruction( InstructionCode );

		if( !Instr )
		{
			appErrorf(TEXT("Unrecognized instruction when determining registers used in bytecode"));
		}
		else
		{
			switch( Instr->Opcode )
			{
			case D3DSIO_DCL:
				{
					DWORD Usage = *TokenPtr++;
					DWORD Param = *TokenPtr++;
					DWORD RegNum = Param & D3DSP_REGNUM_MASK;

					DWORD MaskedUsage = ( ( Usage >> D3DSP_DCL_USAGE_SHIFT ) & D3DSP_DCL_USAGE_MASK );
					DWORD MaskedUsageIndex = ( Usage & D3DSP_DCL_USAGEINDEX_MASK ) >> D3DSP_DCL_USAGEINDEX_SHIFT;

					INT RegType = GetRegisterType( Param );
					switch( RegType )
					{
					case D3DSPR_INPUT:
						RegsIn[RegNum].Usage = (D3DDECLUSAGE)MaskedUsage;
						RegsIn[RegNum].UsageIndex = MaskedUsageIndex;
						RegsIn[RegNum].Register = Param;
						if( !bIsPixelShader )
						{
							Regs.AttributeRegs[RegNum] = TRUE;
						}
						else
						{
							Regs.PackedInputRegs[RegNum] = TRUE;

							switch( MaskedUsage )
							{
							case D3DDECLUSAGE_TEXCOORD:
								if( MaskedUsageIndex == 8 )
								{
									Regs.VaryingRegisters[UnTexcoord8] = TRUE;
								}
								else if( MaskedUsageIndex == 9 )
								{
									Regs.VaryingRegisters[UnTexcoord9] = TRUE;
								}
								else if( MaskedUsageIndex > 9 )
								{
									appErrorf(TEXT("Add varying for input texcoord > 9!"));
								}
								break;

							default:
								break;
							}
						}
						break;

					case D3DSPR_OUTPUT:
						RegsOut[RegNum].Usage = (D3DDECLUSAGE)MaskedUsage;
						RegsOut[RegNum].UsageIndex = MaskedUsageIndex;
						RegsOut[RegNum].Register = Param;
						Regs.PackedOutputRegs[RegNum] = TRUE;

						switch( MaskedUsage )
						{
						case D3DDECLUSAGE_TEXCOORD:
							if( MaskedUsageIndex == 8 )
							{
								Regs.VaryingRegisters[UnTexcoord8] = TRUE;
							}
							else if( MaskedUsageIndex == 9 )
							{
								Regs.VaryingRegisters[UnTexcoord9] = TRUE;
							}
							else if( MaskedUsageIndex > 9 )
							{
								appErrorf(TEXT("Add varying for output texcoord > 9!"));
							}
							break;

						case D3DDECLUSAGE_FOG:
							Regs.UsesFog = TRUE;

						default:
							break;
						}
						break;

					case D3DSPR_SAMPLER:
						Regs.SamplersUsage[RegNum] = Usage;
						break;
					}
				}
				break;

			case D3DSIO_DEF:
				{
					DWORD RegNum = *TokenPtr & D3DSP_REGNUM_MASK;
					FLOAT Val[4];
					appMemcpy( Val, TokenPtr+1, 4 * sizeof(FLOAT) );

					AddConstantFloat( RegNum, ( DWORD *)Val );
					TokenPtr += Instr->SourceParamsNum;
					if( Instr->HasDestination )
					{
						TokenPtr++;
					}
				}
				break;

			case D3DSIO_DEFI:
				AddConstantInt( *TokenPtr & D3DSP_REGNUM_MASK, TokenPtr+1 );
				TokenPtr += Instr->SourceParamsNum;
				if( Instr->HasDestination )
				{
					TokenPtr++;
				}
				break;

			case D3DSIO_DEFB:
				AddConstantBool( *TokenPtr & D3DSP_REGNUM_MASK, *(TokenPtr+1) );
				TokenPtr += Instr->SourceParamsNum;
				if( Instr->HasDestination )
				{
					TokenPtr++;
				}
				break;

			case D3DSIO_LOOP:
			case D3DSIO_REP:
				CurrentLoopDepth++;
				if( CurrentLoopDepth > MaxLoopDepth )
				{
					MaxLoopDepth = CurrentLoopDepth;
				}
				TokenPtr += Instr->SourceParamsNum;
				if( Instr->HasDestination )
				{
					TokenPtr++;
				}
				break;

			case D3DSIO_ENDLOOP:
			case D3DSIO_ENDREP:
				CurrentLoopDepth--;
				break;

			case D3DSIO_LABEL:
				Regs.Labels[*TokenPtr & D3DSP_REGNUM_MASK] = TRUE;
				TokenPtr += Instr->SourceParamsNum;
				if( Instr->HasDestination )
				{
					TokenPtr++;
				}
				break;

			default:	// rest of instructions - those have meaningful params, so we have to look closely
				{
					switch( Instr->Opcode )
					{
					case D3DSIO_BEM:
					case D3DSIO_TEXBEM:
					case D3DSIO_TEXBEML:
					case D3DSIO_TEX:
					case D3DSIO_TEXDP3TEX:
					case D3DSIO_TEXM3x2TEX:
					case D3DSIO_TEXM3x3SPEC:
					case D3DSIO_TEXM3x3TEX:
					case D3DSIO_TEXM3x3VSPEC:
					case D3DSIO_TEXREG2AR:
					case D3DSIO_TEXREG2GB:
					case D3DSIO_TEXREG2RGB:
					case D3DSIO_DSY:
						break;
					}

					// now analyze the parameters

					INT NumParams = Instr->HasDestination?1:0;
					NumParams += Instr->SourceParamsNum;
					if( InstructionCode & D3DSHADER_INSTRUCTION_PREDICATED )
					{
						++NumParams;
					}

					for( INT Index = 0; Index < NumParams; ++Index )
					{
						DWORD AddressToken;
						DWORD Param = ReadParam( TokenPtr, AddressToken );
						INT RegisterType = GetRegisterType( Param );
						INT RegisterNum = Param & D3DSP_REGNUM_MASK;

						switch( RegisterType )
						{
						case D3DSPR_CONST:
							if( Param & D3DSHADER_ADDRMODE_RELATIVE )
							{
								Regs.UsesRelConstF = TRUE;
								if( !bIsPixelShader )
								{
									if( RegisterNum < Regs.MinimumRelativeOffset )
									{
										Regs.MinimumRelativeOffset = RegisterNum;
									}
									if( RegisterNum > Regs.MaximumRelativeOffset )
									{
										Regs.MaximumRelativeOffset = RegisterNum;
									}
								}
							}
							break;

						case D3DSPR_INPUT:
							if( bIsPixelShader )
							{
								if( Param & D3DSHADER_ADDRMODE_RELATIVE )
								{
									// Have to use all registers - relative addressing can address any of them!
									for( INT Register = 0; Register < MaxInputRegisters; ++Register )
									{
										Regs.InputRegistersUsed[Register] = TRUE;
									}
								}
								else
								{
									Regs.InputRegistersUsed[RegisterNum] = TRUE;
								}
							}
							else
							{
								Regs.AttributeRegs[RegisterNum] = TRUE;	// vertex shaders
							}
							break;

						case D3DSPR_MISCTYPE:
							break;

						case D3DSPR_RASTOUT:
							if( RegisterNum == 1)	// fog register
							{
								Regs.UsesFog = TRUE;
							}
							break;

						case D3DSPR_TEMP:
							Regs.TemporaryRegs[RegisterNum] = TRUE;
							break;

						case D3DSPR_TEXCRDOUT:
							break;

						case D3DSPR_TEXTURE:
							if( bIsPixelShader )
								Regs.TexcoordRegs[RegisterNum] = TRUE;
							else
							{
								if( RegisterNum )
								{
									appErrorf(TEXT("Address register > 0 addressed!"));
								}
								else
								{
									Regs.AddressReg = TRUE;
								}
							}
							break;
						}
					}
				}
				break;
			}
		}
	}

	Regs.LoopDepth = MaxLoopDepth;
}

const AsmInstructionData* D3DShaderData::IdentifyInstruction( DWORD InstructionCode )
{
	const AsmInstructionData* Instr = InstructionsList;

	while( Instr->AsmName[0] )
	{
		if( ( ( InstructionCode & D3DSI_OPCODE_MASK ) == Instr->Opcode ) &&											// if the code is the same,
			( ( !Instr->MinShaderVersion && !Instr->MaxShaderVersion ) ||											// and the instruction is for all shader versions,
			( ( ShaderVersion >= Instr->MinShaderVersion ) && ( ShaderVersion <= Instr->MaxShaderVersion ) ) ) )	// or specifically for our shader version,
			return Instr;
		Instr++;
	}

	appErrorf(TEXT("Unsupported asm opcode in ShaderData::IdentifyInstruction()!!!. Opcode 0x%x, shader version 0x%x"), (InstructionCode & D3DSI_OPCODE_MASK), ShaderVersion);

	return NULL;
}

UBOOL D3DShaderData::FloatConstantIsLocal( DWORD RegNum )
{
	for( DWORD Index = 0; Index < ConstantsFloatNum; ++Index )
	{
		if( ConstantsFloat[Index].Index == RegNum )
		{
			return TRUE;
		}
	}
	return FALSE;
}

void D3DShaderData::GetAttributeRegName( DWORD RegNum, ANSICHAR* NameBuffer )
{
	D3DDECLUSAGE Usage = RegsIn[RegNum].Usage;
	DWORD UsageIndex = RegsIn[RegNum].UsageIndex;
	switch( Usage )
	{
	case D3DDECLUSAGE_POSITION:		sprintf_s( NameBuffer, 128, "Un_AttrPosition%u", UsageIndex );		break;
	case D3DDECLUSAGE_TEXCOORD:		sprintf_s( NameBuffer, 128, "Un_AttrTexCoord%u", UsageIndex );		break;
	case D3DDECLUSAGE_POSITIONT:	sprintf_s( NameBuffer, 128, "Un_AttrPositionT%u", UsageIndex );		break;
	case D3DDECLUSAGE_COLOR:		sprintf_s( NameBuffer, 128, "Un_AttrColor%u", UsageIndex );			break;
	case D3DDECLUSAGE_NORMAL:		sprintf_s( NameBuffer, 128, "Un_AttrNormal%u", UsageIndex );		break;
	case D3DDECLUSAGE_TANGENT:		sprintf_s( NameBuffer, 128, "Un_AttrTangent%u", UsageIndex );		break;
	case D3DDECLUSAGE_BINORMAL:		sprintf_s( NameBuffer, 128, "Un_AttrBinormal%u", UsageIndex );		break;
	case D3DDECLUSAGE_BLENDWEIGHT:	sprintf_s( NameBuffer, 128, "Un_AttrBlendWeight%u", UsageIndex );	break;
	case D3DDECLUSAGE_PSIZE:		sprintf_s( NameBuffer, 128, "Un_AttrPSize%u", UsageIndex );			break;
	case D3DDECLUSAGE_BLENDINDICES:	sprintf_s( NameBuffer, 128, "Un_AttrBlendIndices%u", UsageIndex );	break;
	case D3DDECLUSAGE_TESSFACTOR:	sprintf_s( NameBuffer, 128, "Un_AttrTessFactor%u", UsageIndex );	break;
	case D3DDECLUSAGE_FOG:			sprintf_s( NameBuffer, 128, "Un_AttrFog%u", UsageIndex );			break;
	case D3DDECLUSAGE_DEPTH:		sprintf_s( NameBuffer, 128, "Un_AttrDepth%u", UsageIndex );			break;
	case D3DDECLUSAGE_SAMPLE:		sprintf_s( NameBuffer, 128, "Un_AttrSample%u", UsageIndex );		break;
	default:
		check(0);
		break;
	}
}

// Gets param, and if applicable, also address token. Moves given ptr pass the tokens read.
DWORD D3DShaderData::ReadParam( const DWORD*& TokenPtr, DWORD& AddressToken )
{
	DWORD Param = *(TokenPtr++);
	UBOOL bRelativeToken = ((Param & D3DSHADER_ADDRESSMODE_MASK ) == D3DSHADER_ADDRMODE_RELATIVE);
	AddressToken = bRelativeToken ? (*(TokenPtr++)) : 0;
	return Param;
}

void D3DShaderData::GenerateGLSLDeclarations( FString &TargetBuffer )
{
	TargetBuffer += TEXT("#version 120\n");
	TargetBuffer += TEXT("#extension GL_EXT_bindable_uniform : require\n");
	TargetBuffer += TEXT("#extension GL_ARB_shader_texture_lod : require\n");
	TargetBuffer += TEXT("\n");

	INT AdditionalConstants = 0;
	ANSICHAR Prefix = bIsPixelShader ? 'P' : 'V';

	// Subroutine declarations (for labels)
	for( DWORD Index = 0; Index < MaxLabels; ++Index )
	{
		if( Regs.Labels[Index] )
		{
			TargetBuffer += FString::Printf(TEXT("void LabelFunction%u();\n"), Index);
		}
	}

	// Uniforms declarations
	if( MaxConstFloats )
	{
		TargetBuffer += FString::Printf(TEXT("bindable uniform vec4 %cConstFloat[%u];\n"), Prefix, MaxConstFloats);
	}

	if (bHasGlobalConsts)
	{
		TargetBuffer += FString::Printf(TEXT("bindable uniform vec4 %cConstGlobal[%d];\n"), Prefix, Max<INT>(VS_NUM_GLOBAL_VECTORS, PS_NUM_GLOBAL_VECTORS));
	}

	if (bHasBonesConsts)
	{
		TargetBuffer += TEXT("bindable uniform vec4 VConstBones[225];\n");
	}

	if( MaxConstInts )
	{
		TargetBuffer += FString::Printf(TEXT("uniform ivec4 %cConstInt[%u];\n"), Prefix, MaxConstInts);
	}

	if( MaxConstBools )
	{
		TargetBuffer += FString::Printf(TEXT("bindable uniform bool %cConstBool[%u];\n"), Prefix, MaxConstBools);
	}

	if( bIsPixelShader )
	{
		if( Regs.BumpEnvMatrixSamplerNum != -1 )
		{
			TargetBuffer += TEXT("uniform mat2 BumpEnvMatrix;\n");
			AdditionalConstants++;
			if( Regs.LuminanceParams )
			{
				AdditionalConstants++;
				TargetBuffer += TEXT("uniform float LuminanceScale;\n");
				TargetBuffer += TEXT("uniform float LuminanceOffset;\n");
			}
		}
	}

	// Declarations of texture samplers
	for( DWORD Index = 0; Index < MaxSamplers; ++Index )
	{
		if( Regs.SamplersUsage[Index] )
		{
			DWORD SamplerType = Regs.SamplersUsage[Index] & D3DSP_TEXTURETYPE_MASK;
			const TCHAR* SamplerTypeString;
			switch( SamplerType )
			{
			case 1:				SamplerTypeString = TEXT("1D");	break;
			case D3DSTT_2D:		SamplerTypeString = TEXT("2D");	break;		// no sense in worrying about rect textures, we're on OpenGL 2.0
			case D3DSTT_CUBE:	SamplerTypeString = TEXT("Cube");	break;
			case D3DSTT_VOLUME:	SamplerTypeString = TEXT("3D");	break;
			default:
				appErrorf(TEXT("Unsupported sampler type while processing GLSL declarations! (%x). Fix it."), SamplerType);
				SamplerTypeString = TEXT("_no_idea");
			}
			TargetBuffer += FString::Printf(TEXT("uniform sampler%s %cSampler%u;\n"), SamplerTypeString, Prefix, Index );
		}
	}

	if( Regs.AddressReg )
	{
		TargetBuffer += TEXT("\nivec4 Address0;\n");
	}

	for( DWORD Index = 0; Index < MaxTexcoords; ++Index )
	{
		if( Regs.TexcoordRegs[Index] )
		{
			TargetBuffer += FString::Printf(TEXT("vec4 Texture%u = gl_TexCoord[%u];\n"), Index, Index);
		}
	}

	if( bIsPixelShader )
	{
		TargetBuffer += TEXT("\nvec4 IN[32];\n");
	}

	if( Regs.VaryingRegisters[UnTexcoord8] )
	{
		TargetBuffer += TEXT("\nvarying vec4 Un_Texcoord8;\n");
	}
	if( Regs.VaryingRegisters[UnTexcoord9] )
	{
		TargetBuffer += TEXT("\nvarying vec4 Un_Texcoord9;\n");
	}

	if( MaxPackedOutputs )
	{
		TargetBuffer += FString::Printf(TEXT("\nvec4 OUT[%u];\n"), MaxPackedOutputs );
	}

	for( DWORD Index = 0; Index < MaxTemporaries; ++Index )
	{
		if( Regs.TemporaryRegs[Index] )
		{
			TargetBuffer += FString::Printf(TEXT("vec4 Temporary%u;\n"), Index );
		}
	}

	for( DWORD Index = 0; Index < MaxAttributes; ++Index )
	{
		if( Regs.AttributeRegs[Index] )
		{
			ANSICHAR AttrName[64];
			GetAttributeRegName( Index, AttrName );
			TargetBuffer += FString::Printf(TEXT("attribute vec4 _%s;\n"), ANSI_TO_TCHAR(AttrName));
			TargetBuffer += FString::Printf(TEXT("vec4 %s = _%s;\n"), ANSI_TO_TCHAR(AttrName), ANSI_TO_TCHAR(AttrName) );
		}
	}

	for( DWORD Index = 0; Index < Regs.LoopDepth; ++Index )
	{
		TargetBuffer += FString::Printf(TEXT("int Loop%u;\n"), Index );
		TargetBuffer += FString::Printf(TEXT("int LoopTemp%u;\n"), Index );
	}

	TargetBuffer += TEXT("\nvec4 InstrHelpTemp;\n" );		// to handle strange workings of some instructions

	for( DWORD Index = 0; Index < ConstantsFloatNum; ++Index )
	{
		const FLOAT* Value = (const FLOAT*)ConstantsFloat[Index].Value;
		TargetBuffer += FString::Printf(TEXT("const vec4 LocalConst%u = vec4(%f, %f, %f, %f);\n"), ConstantsFloat[Index].Index, Value[0], Value[1], Value[2], Value[3] );
	}

	// Main program starts here
	TargetBuffer += TEXT("\nvoid main()\n{\n");
}

void D3DShaderData::GenerateShaderBody( FString &TargetBuffer )
{
	const DWORD* TokenPtr = Bytecode;

	ConversionState State;
	State.Shader = this;
	State.OutputBuffer = &TargetBuffer;

	while( *TokenPtr != D3DSIO_END )
	{
		DWORD VerToken = *TokenPtr >> 16;
		if( ( VerToken == 0xFFFE ) || ( VerToken == 0xFFFF ) )				// Skip version token
		{
			++TokenPtr;
			continue;
		}
		else if( ( *TokenPtr & D3DSI_OPCODE_MASK ) == D3DSIO_COMMENT )		// Skip comments
		{
			DWORD CommentLen = (*TokenPtr & D3DSI_COMMENTSIZE_MASK ) >> D3DSI_COMMENTSIZE_SHIFT;
			++TokenPtr;
			TokenPtr += CommentLen;
			continue;
		}

		State.InstructionCode = *TokenPtr++;
		State.Instruction = IdentifyInstruction( State.InstructionCode );

		if( !State.Instruction )
		{
			appErrorf(TEXT("Unrecognized instruction when producing GLSL code\n"));
		}
		else if(	( State.Instruction->Opcode == D3DSIO_DCL ) ||			// Skip declarations and instruction that generate no code
			( State.Instruction->Opcode == D3DSIO_NOP ) ||
			( State.Instruction->Opcode == D3DSIO_DEF ) ||
			( State.Instruction->Opcode == D3DSIO_DEFI ) ||
			( State.Instruction->Opcode == D3DSIO_DEFB ) ||
			( State.Instruction->Opcode == D3DSIO_PHASE ) ||
			( State.Instruction->Opcode == D3DSIO_RET ) )
		{
			// Skip instruction
			TokenPtr += ( State.InstructionCode & D3DSI_INSTLENGTH_MASK ) >> D3DSI_INSTLENGTH_SHIFT;
		}
		else if( !State.Instruction->ConversionFunction )
		{
			appErrorf(TEXT("Can't convert this instruction to GLSL!!!"));
			TokenPtr += ( State.InstructionCode & D3DSI_INSTLENGTH_MASK ) >> D3DSI_INSTLENGTH_SHIFT;
		}
		else
		{
			if( State.Instruction->HasDestination )
			{
				State.DestToken = ReadParam( TokenPtr, State.DestAddressToken );
			}
			State.PredicateToken = ( State.InstructionCode & D3DSHADER_INSTRUCTION_PREDICATED ) ? *TokenPtr++ : 0;
			for( DWORD Index = 0; Index < State.Instruction->SourceParamsNum; ++Index )
			{
				State.SourceToken[Index] = ReadParam( TokenPtr, State.SourceAddressToken[Index] );
			}

			// Call the conversion function
			(*State.Instruction->ConversionFunction)( &State );

			// Add color correction if needed
			{
				INT SamplerIndex = -1;
				switch( State.Instruction->Opcode )
				{
				case D3DSIO_TEX:
					SamplerIndex = State.SourceToken[1] & D3DSP_REGNUM_MASK;
					break;

				case D3DSIO_TEXDP3TEX:
				case D3DSIO_TEXM3x3TEX:
				case D3DSIO_TEXM3x3SPEC:
				case D3DSIO_TEXM3x3VSPEC:
				case D3DSIO_TEXBEM:
				case D3DSIO_TEXREG2AR:
				case D3DSIO_TEXREG2GB:
				case D3DSIO_TEXREG2RGB:
					SamplerIndex = State.DestToken & D3DSP_REGNUM_MASK;
					break;

				case D3DSIO_TEXLDL:
					break;

				default:
					break;	// nothing to fix
				}

				if( SamplerIndex != -1 )
				{
					D3DFORMAT TextureFormat = GetTextureFormatForSampler( SamplerIndex );

					// Remember that we used this sampler, with this texture format
					{
						UBOOL bAlreadyUsed = FALSE;
						for( DWORD i = 0; i < SampledSamplersNum; ++i )
						{
							if( SampledSamplerIndex[i] == (DWORD)SamplerIndex )
							{
								bAlreadyUsed = TRUE;
								break;
							}
						}

						if( !bAlreadyUsed )
						{
							SampledSamplerIndex[SampledSamplersNum] = SamplerIndex;
							SampledTextureFormat[SampledSamplersNum] = TextureFormat;
							SampledSamplersNum++;
						}
					}

					switch( TextureFormat )
					{
					case D3DFMT_X8L8V8U8:
					case D3DFMT_L6V5U5:
					case D3DFMT_Q8W8V8U8:
						appErrorf(TEXT("Texture format 0x%x not supported"), (DWORD)TextureFormat);
						break;
					default:
						break;
					}
				}
			}

			// Process instruction modifiers
			DWORD Mask = State.DestToken & D3DSP_DSTMOD_MASK;

			if( State.Instruction->HasDestination && Mask )
			{
				DestParamName Name;
				GenerateDestParamName( State, State.DestToken, 0, Name );

				if( Mask & D3DSPDM_SATURATE )					// saturation is clamping values to [0;1]
				{
					TargetBuffer += FString::Printf(TEXT("%s%s = clamp(%s%s, 0.0, 1.0);\n"), ANSI_TO_TCHAR(Name.RegisterName), ANSI_TO_TCHAR(Name.MaskString), ANSI_TO_TCHAR(Name.RegisterName), ANSI_TO_TCHAR(Name.MaskString));
				}

				if( Mask & D3DSPDM_MSAMPCENTROID )
				{
					appErrorf(TEXT("_centroid modifier not supported atm, add it please."));
				}
			}
		}
	}
}

void D3DShaderData::GenerateOutputRegistersAssignments( FString &TargetBuffer )
{
	if( bIsPixelShader )
	{
		return;
	}

	for( INT Index = 0; Index < MaxOutputRegisters; ++Index )
	{
		if( Regs.PackedOutputRegs[Index] )
		{
			ANSICHAR WriteMaskString[6];	// .xyzw\0 = 6
			GetWriteMask( RegsOut[Index].Register, WriteMaskString, bIsPixelShader );

			switch( RegsOut[Index].Usage )
			{
			case D3DDECLUSAGE_POSITION:
			case D3DDECLUSAGE_POSITIONT:
				if( RegsOut[Index].UsageIndex > 0 )
				{
					appErrorf(TEXT("Trying to use vertex shader 3.0 output position register > 0."));
				}
				TargetBuffer += FString::Printf(TEXT("gl_Position%s = OUT[%u]%s;\n"), ANSI_TO_TCHAR(WriteMaskString), Index, ANSI_TO_TCHAR(WriteMaskString));
				break;
			case D3DDECLUSAGE_PSIZE:
				appErrorf(TEXT("Trying to use vertex shader 3.0 output register, which has been declared with usage that is never passed to pixel shader."));
				break;
			case D3DDECLUSAGE_TEXCOORD:
				if( RegsOut[Index].UsageIndex > 9 )
				{
					appErrorf(TEXT("Trying to use vertex shader 3.0 output register, which has been declared as texcoord with index > 9."));
				}
				else if( RegsOut[Index].UsageIndex == 9 )
				{
					TargetBuffer += FString::Printf(TEXT("Un_Texcoord9%s = OUT[%u]%s;\n"), ANSI_TO_TCHAR(WriteMaskString), Index, ANSI_TO_TCHAR(WriteMaskString));
				}
				else if( RegsOut[Index].UsageIndex == 8 )
				{
					TargetBuffer += FString::Printf(TEXT("Un_Texcoord8%s = OUT[%u]%s;\n"), ANSI_TO_TCHAR(WriteMaskString), Index, ANSI_TO_TCHAR(WriteMaskString) );
				}
				else
				{
					TargetBuffer += FString::Printf(TEXT("gl_TexCoord[%u]%s = OUT[%u]%s;\n"), RegsOut[Index].UsageIndex, ANSI_TO_TCHAR(WriteMaskString), Index, ANSI_TO_TCHAR(WriteMaskString) );
				}
				break;
			case D3DDECLUSAGE_COLOR:
				if( RegsOut[Index].UsageIndex == 0 )
				{
					TargetBuffer += FString::Printf(TEXT("gl_FrontColor%s = OUT[%u]%s;\n"), ANSI_TO_TCHAR(WriteMaskString), Index, ANSI_TO_TCHAR(WriteMaskString) );
					TargetBuffer += FString::Printf(TEXT("gl_BackColor%s = OUT[%u]%s;\n"), ANSI_TO_TCHAR(WriteMaskString), Index, ANSI_TO_TCHAR(WriteMaskString) );
				}
				else if( RegsOut[Index].UsageIndex == 1 )
				{
					TargetBuffer += FString::Printf(TEXT("gl_FrontSecondaryColor%s = OUT[%u]%s;\n"), ANSI_TO_TCHAR(WriteMaskString), Index, ANSI_TO_TCHAR(WriteMaskString) );
					TargetBuffer += FString::Printf(TEXT("gl_BackSecondaryColor%s = OUT[%u]%s;\n"), ANSI_TO_TCHAR(WriteMaskString), Index, ANSI_TO_TCHAR(WriteMaskString) );
				}
				else
				{
					appErrorf(TEXT("Can't assign OUT to output parameter: dcl_colorX declared, where X > 1!"));
				}
				break;
			case D3DDECLUSAGE_NORMAL:
			case D3DDECLUSAGE_BLENDINDICES:
			case D3DDECLUSAGE_BLENDWEIGHT:
			case D3DDECLUSAGE_TANGENT:
			case D3DDECLUSAGE_BINORMAL:
			case D3DDECLUSAGE_TESSFACTOR:
			case D3DDECLUSAGE_FOG:
				TargetBuffer += FString::Printf(TEXT("gl_FogFragCoord = OUT[%u]%s;\n"), Index, ANSI_TO_TCHAR(WriteMaskString) );
				break;
			case D3DDECLUSAGE_DEPTH:
			case D3DDECLUSAGE_SAMPLE:
			default:
				appErrorf(TEXT("Trying to use vertex shader 3.0 output register, which has been declared with usage for which output GLSL register has not been defined!"));
				break;
			}
		}
	}
}

void D3DShaderData::GenerateInputRegistersAssignments( FString &TargetBuffer )
{
	if( !bIsPixelShader )
		return;

	for( INT Index = 0; Index < MaxInputRegisters; ++Index )
		if( Regs.PackedInputRegs[Index] )
		{
			ANSICHAR WriteMaskString[6];	// .xyzw\0 = 6
			GetWriteMask( RegsIn[Index].Register, WriteMaskString, bIsPixelShader );

			switch( RegsIn[Index].Usage )
			{
			case D3DDECLUSAGE_POSITION:
			case D3DDECLUSAGE_POSITIONT:
			case D3DDECLUSAGE_PSIZE:
				appErrorf(TEXT("Trying to use pixel shader 3.0 input register, which has been declared with usage that is never passed to pixel shader."));
				break;
			case D3DDECLUSAGE_TEXCOORD:
				if( RegsIn[Index].UsageIndex > 9 )
				{
					appErrorf(TEXT("Trying to use pixel shader 3.0 input register, which has been declared as texcoord with index > 9."));
				}
				else if( RegsIn[Index].UsageIndex == 9 )
				{
					TargetBuffer += FString::Printf(TEXT("IN[%u]%s = UnTexcoord9%s;\n"), Index, ANSI_TO_TCHAR(WriteMaskString), ANSI_TO_TCHAR(WriteMaskString) );
				}
				else if( RegsIn[Index].UsageIndex == 8 )
				{
					TargetBuffer += FString::Printf(TEXT("IN[%u]%s = UnTexcoord8%s;\n"), Index, ANSI_TO_TCHAR(WriteMaskString), ANSI_TO_TCHAR(WriteMaskString) );
				}
				else
				{
					TargetBuffer += FString::Printf(TEXT("IN[%u]%s = gl_TexCoord[%u]%s;\n"), Index, ANSI_TO_TCHAR(WriteMaskString), RegsIn[Index].UsageIndex, ANSI_TO_TCHAR(WriteMaskString) );
				}
				break;
			case D3DDECLUSAGE_COLOR:
				if( RegsIn[Index].UsageIndex == 0 )
				{
					TargetBuffer += FString::Printf(TEXT("IN[%u]%s = vec4(gl_Color)%s;\n"), Index, ANSI_TO_TCHAR(WriteMaskString), ANSI_TO_TCHAR(WriteMaskString) );
				}
				else if( RegsIn[Index].UsageIndex == 1 )
				{
					TargetBuffer += FString::Printf(TEXT("IN[%u]%s = vec4(gl_SecondaryColor)%s;\n"), Index, ANSI_TO_TCHAR(WriteMaskString), ANSI_TO_TCHAR(WriteMaskString) );
				}
				else
				{
					warnf(TEXT("dcl_colorX declared, where X > 1! Assigning zero vector to this."));
					TargetBuffer += FString::Printf(TEXT("IN[%u]%s = vec4( 0.0, 0.0, 0.0, 0.0 )%s;\n"), Index, ANSI_TO_TCHAR(WriteMaskString), ANSI_TO_TCHAR(WriteMaskString) );
				}
				break;
			case D3DDECLUSAGE_NORMAL:
			case D3DDECLUSAGE_BLENDINDICES:
			case D3DDECLUSAGE_BLENDWEIGHT:
			case D3DDECLUSAGE_TANGENT:
			case D3DDECLUSAGE_BINORMAL:
			case D3DDECLUSAGE_TESSFACTOR:
			case D3DDECLUSAGE_FOG:
				TargetBuffer += FString::Printf(TEXT("IN[%u]%s = vec4(gl_FogFragCoord, 0.0, 0.0, 0.0)%s;\n"), Index, ANSI_TO_TCHAR(WriteMaskString), ANSI_TO_TCHAR(WriteMaskString) );
				break;
			case D3DDECLUSAGE_DEPTH:
			case D3DDECLUSAGE_SAMPLE:
			default:
				appErrorf(TEXT("Trying to use pixel shader 3.0 input register, which has been declared with usage for which input GLSL register has not been defined. Assigning zero vector."));
				TargetBuffer += FString::Printf(TEXT("IN[%u]%s = vec4( 0.0, 0.0, 0.0, 0.0 )%s;\n"), Index, ANSI_TO_TCHAR(WriteMaskString), ANSI_TO_TCHAR(WriteMaskString) );
			}
		}
}

UBOOL BytecodeVSToGLSLText(const DWORD *Bytecode, FString &OutputShader, UBOOL bHasGlobalConsts, UBOOL bHasBonesConsts)
{
	D3DShaderData Shader(bHasGlobalConsts,bHasBonesConsts);
	Shader.Init(Bytecode);
	Shader.InitLimits();
	Shader.DetermineUsedRegisters();
	Shader.GenerateGLSLDeclarations(OutputShader);
	Shader.GenerateShaderBody(OutputShader);
	Shader.GenerateOutputRegistersAssignments(OutputShader);
	OutputShader += TEXT("}\n");
	return TRUE;
}

UBOOL BytecodePSToGLSLText(const DWORD *Bytecode, FString &OutputShader, UBOOL bHasGlobalConsts)
{
	D3DShaderData Shader(bHasGlobalConsts,FALSE);
	Shader.Init(Bytecode);
	Shader.InitLimits();
	Shader.DetermineUsedRegisters();
	Shader.GenerateGLSLDeclarations(OutputShader);
	Shader.GenerateInputRegistersAssignments(OutputShader);
	Shader.GenerateShaderBody(OutputShader);
	OutputShader += TEXT("}\n");
	return TRUE;
}

#endif // _WINDOWS
