/*=============================================================================
	D3DMaterialShader.cpp: Unreal Direct3D material shader implementation.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "D3DDrv.h"

//
//	FD3DMaterialUserInputTime
//

struct FD3DMaterialUserInputTime: FD3DMaterialUserInput
{
	UBOOL	Absolute;

	// Constructor.

	FD3DMaterialUserInputTime(UBOOL InAbsolute):
		Absolute(InAbsolute)
	{}

	// FD3DMaterialUserInput interface.

	virtual FLinearColor GetValue(FMaterialInstance* MaterialInstance,FLOAT ObjectTime)
    { 
        return FLinearColor(Absolute && GD3DSceneRenderer ? GD3DSceneRenderer->Scene->GetSceneTime() : ObjectTime,0,0,0);
    }
};

//
//	FD3DMaterialUserInputParameter
//

struct FD3DMaterialUserInputParameter: FD3DMaterialUserInput
{
	FName	ParameterName;
	UBOOL	Vector;

	// Constructor.

	FD3DMaterialUserInputParameter(FName InParameterName,UBOOL InVector):
		ParameterName(InParameterName),
		Vector(InVector)
	{}

	// FD3DMaterialUserInput interface.

	virtual FLinearColor GetValue(FMaterialInstance* MaterialInstance,FLOAT ObjectTime)
	{
		if(MaterialInstance)
		{
			if(Vector)
			{
				FLinearColor*	Value = MaterialInstance->VectorValueMap.Find(ParameterName);
				if(Value)
					return FLinearColor(*Value);
			}
			else
			{
				FLOAT*	Value = MaterialInstance->ScalarValueMap.Find(ParameterName);
				if(Value)
					return FLinearColor(*Value,0,0,0);
			}
		}
		return FLinearColor::Black;
	}
	virtual UBOOL ShouldEmbedCode() { return 0; }
};

//
//	FD3DMaterialUserInputConstant
//

struct FD3DMaterialUserInputConstant: FD3DMaterialUserInput
{
	FLinearColor	Value;
	UINT			NumComponents;

	// Constructor.

	FD3DMaterialUserInputConstant(const FLinearColor& InValue,UINT InNumComponents):
		Value(InValue),
		NumComponents(InNumComponents)
	{}

	// FD3DMaterialUserInput interface.

	virtual FLinearColor GetValue(FMaterialInstance* MaterialInstance,FLOAT ObjectTime) { return Value; }
	virtual UBOOL ShouldEmbedCode() { return 1; }
};

//
//	FD3DMaterialUserInputSine
//

struct FD3DMaterialUserInputSine: FD3DMaterialUserInput
{
	FD3DMaterialUserInput*	X;
	UBOOL					Cosine;

	// Constructor.

	FD3DMaterialUserInputSine(FD3DMaterialUserInput* InX,UBOOL InCosine):
		X(InX),
		Cosine(InCosine)
	{}

	// FD3DMaterialUserInput interface.

	virtual FLinearColor GetValue(FMaterialInstance* MaterialInstance,FLOAT ObjectTime) { return FLinearColor(Cosine ? appCos(X->GetValue(MaterialInstance,ObjectTime).R) : appSin(X->GetValue(MaterialInstance,ObjectTime).R),0,0,0); }
	virtual UBOOL ShouldEmbedCode() { return X->ShouldEmbedCode(); }
};

//
//	FD3DMaterialUserInputSquareRoot
//

struct FD3DMaterialUserInputSquareRoot: FD3DMaterialUserInput
{
	FD3DMaterialUserInput*	X;

	// Constructor.

	FD3DMaterialUserInputSquareRoot(FD3DMaterialUserInput* InX):
		X(InX)
	{}

	// FD3DMaterialUserInput interface.

	virtual FLinearColor GetValue(FMaterialInstance* MaterialInstance,FLOAT ObjectTime) { return FLinearColor(appSqrt(X->GetValue(MaterialInstance,ObjectTime).R),0,0,0); }
	virtual UBOOL ShouldEmbedCode() { return X->ShouldEmbedCode(); }
};

enum ED3DFoldedMathOperation
{
	FMO_Add,
	FMO_Sub,
	FMO_Mul,
	FMO_Div,
	FMO_Dot
};

//
//	FD3DMaterialUserInputFoldedMath
//

struct FD3DMaterialUserInputFoldedMath: FD3DMaterialUserInput
{
	FD3DMaterialUserInput*	A;
	FD3DMaterialUserInput*	B;
	ED3DFoldedMathOperation	Op;

	// Constructor.

	FD3DMaterialUserInputFoldedMath(FD3DMaterialUserInput* InA,FD3DMaterialUserInput* InB,ED3DFoldedMathOperation InOp):
		A(InA),
		B(InB),
		Op(InOp)
	{}

	// FD3DMaterialUserInput interface.

	virtual FLinearColor GetValue(FMaterialInstance* MaterialInstance,FLOAT ObjectTime)
	{
		FLinearColor	ValueA = A->GetValue(MaterialInstance,ObjectTime);
		FLinearColor	ValueB = B->GetValue(MaterialInstance,ObjectTime);
		switch(Op)
		{
		case FMO_Add: return ValueA + ValueB;
		case FMO_Sub: return ValueA - ValueB;
		case FMO_Mul: return ValueA * ValueB;
		case FMO_Div: return FLinearColor(ValueA.R / ValueB.R,ValueA.G / ValueB.G,ValueA.B / ValueB.B,ValueA.A / ValueB.A);
		case FMO_Dot: return ValueA.R * ValueB.R + ValueA.G * ValueB.G + ValueA.B * ValueB.B + ValueA.A * ValueB.A;
		default: appErrorf(TEXT("Unknown folded math operation: %08x"),Op);
		};
		return FLinearColor(0,0,0,0);
	}
	virtual UBOOL ShouldEmbedCode() { return A->ShouldEmbedCode() && B->ShouldEmbedCode(); }
};

//
//	FD3DMaterialUserInputPeriodic - A hint that only the fractional part of this input matters.
//

struct FD3DMaterialUserInputPeriodic: FD3DMaterialUserInput
{
	FD3DMaterialUserInput*	A;

	// Constructor.

	FD3DMaterialUserInputPeriodic(FD3DMaterialUserInput* InA):
		A(InA)
	{}

	// FD3DMaterialUserInput interface.

	virtual FLinearColor GetValue(FMaterialInstance* MaterialInstance,FLOAT ObjectTime)
	{
		FLinearColor	Value = A->GetValue(MaterialInstance,ObjectTime);
		return FLinearColor(
				Value.R - appFloor(Value.R),
				Value.G - appFloor(Value.G),
				Value.B - appFloor(Value.B),
				Value.A - appFloor(Value.A)
				);
	}
};

//
//	FD3DMaterialUserInputAppendVector
//

struct FD3DMaterialUserInputAppendVector: FD3DMaterialUserInput
{
	FD3DMaterialUserInput*	A;
	FD3DMaterialUserInput*	B;
	UINT					NumComponentsA;

	// Constructor.

	FD3DMaterialUserInputAppendVector(FD3DMaterialUserInput* InA,FD3DMaterialUserInput* InB,UINT InNumComponentsA):
		A(InA),
		B(InB),
		NumComponentsA(InNumComponentsA)
	{}

	// FD3DMaterialUserInput interface.

	virtual FLinearColor GetValue(FMaterialInstance* MaterialInstance,FLOAT ObjectTime)
	{
		FLinearColor	ValueA = A->GetValue(MaterialInstance,ObjectTime),
						ValueB = B->GetValue(MaterialInstance,ObjectTime);
		return FLinearColor(
				NumComponentsA >= 1 ? ValueA.R : (&ValueB.R)[0 - NumComponentsA],
				NumComponentsA >= 2 ? ValueA.G : (&ValueB.R)[1 - NumComponentsA],
				NumComponentsA >= 3 ? ValueA.B : (&ValueB.R)[2 - NumComponentsA],
				NumComponentsA >= 4 ? ValueA.A : (&ValueB.R)[3 - NumComponentsA]
				);
	}
	virtual UBOOL ShouldEmbedCode() { return A->ShouldEmbedCode() && B->ShouldEmbedCode(); }
};

//
//	FD3DMaterialUserInputMin
//

struct FD3DMaterialUserInputMin: FD3DMaterialUserInput
{
	FD3DMaterialUserInput*	A;
	FD3DMaterialUserInput*	B;

	// Constructor.

	FD3DMaterialUserInputMin(FD3DMaterialUserInput* InA,FD3DMaterialUserInput* InB):
		A(InA),
		B(InB)
	{}

	// FD3DMaterialUserInput interface.

	virtual FLinearColor GetValue(FMaterialInstance* MaterialInstance,FLOAT ObjectTime)
	{
		FLinearColor	ValueA = A->GetValue(MaterialInstance,ObjectTime);
		FLinearColor	ValueB = B->GetValue(MaterialInstance,ObjectTime);
		return FLinearColor(Min(ValueA.R,ValueB.R),Min(ValueA.G,ValueB.G),Min(ValueA.B,ValueB.B),Min(ValueA.A,ValueB.A));
	}
	virtual UBOOL ShouldEmbedCode() { return A->ShouldEmbedCode() && B->ShouldEmbedCode(); }
};

//
//	FD3DMaterialUserInputMax
//

struct FD3DMaterialUserInputMax: FD3DMaterialUserInput
{
	FD3DMaterialUserInput*	A;
	FD3DMaterialUserInput*	B;

	// Constructor.

	FD3DMaterialUserInputMax(FD3DMaterialUserInput* InA,FD3DMaterialUserInput* InB):
		A(InA),
		B(InB)
	{}

	// FD3DMaterialUserInput interface.

	virtual FLinearColor GetValue(FMaterialInstance* MaterialInstance,FLOAT ObjectTime)
	{
		FLinearColor	ValueA = A->GetValue(MaterialInstance,ObjectTime);
		FLinearColor	ValueB = B->GetValue(MaterialInstance,ObjectTime);
		return FLinearColor(Max(ValueA.R,ValueB.R),Max(ValueA.G,ValueB.G),Max(ValueA.B,ValueB.B),Max(ValueA.A,ValueB.A));
	}
	virtual UBOOL ShouldEmbedCode() { return A->ShouldEmbedCode() && B->ShouldEmbedCode(); }
};

//
//	FD3DMaterialUserInputClamp
//

struct FD3DMaterialUserInputClamp: FD3DMaterialUserInput
{
	FD3DMaterialUserInput*	Input;
	FD3DMaterialUserInput*	Min;
	FD3DMaterialUserInput*	Max;

	// Constructor.

	FD3DMaterialUserInputClamp(FD3DMaterialUserInput* InInput,FD3DMaterialUserInput* InMin,FD3DMaterialUserInput* InMax):
		Input(InInput),
		Min(InMin),
		Max(InMax)
	{}

	// FD3DMaterialUserInput interface.

	virtual FLinearColor GetValue(FMaterialInstance* MaterialInstance,FLOAT ObjectTime)
	{
		FLinearColor	ValueMin = Min->GetValue(MaterialInstance,ObjectTime),
						ValueMax = Max->GetValue(MaterialInstance,ObjectTime),
						ValueInput = Input->GetValue(MaterialInstance,ObjectTime);
		return FLinearColor(
				Clamp(ValueInput.R,ValueMin.R,ValueMax.R),
				Clamp(ValueInput.G,ValueMin.G,ValueMax.G),
				Clamp(ValueInput.B,ValueMin.B,ValueMax.B),
				Clamp(ValueInput.A,ValueMin.A,ValueMax.A)
				);
	}
	virtual UBOOL ShouldEmbedCode() { return Input->ShouldEmbedCode() && Min->ShouldEmbedCode() && Max->ShouldEmbedCode(); }
};

//
//	FD3DMaterialUserInputFloor
//

struct FD3DMaterialUserInputFloor: FD3DMaterialUserInput
{
	FD3DMaterialUserInput*	X;

	// Constructor.

	FD3DMaterialUserInputFloor(FD3DMaterialUserInput* InX):
		X(InX)
	{}

	// FD3DMaterialUserInput interface.

	virtual FLinearColor GetValue(FMaterialInstance* MaterialInstance,FLOAT ObjectTime)
	{
		FLinearColor	ValueX = X->GetValue(MaterialInstance,ObjectTime);
		return FLinearColor(
				appFloor(ValueX.R),
				appFloor(ValueX.G),
				appFloor(ValueX.B),
				appFloor(ValueX.A)
				);
	}
	virtual UBOOL ShouldEmbedCode() { return X->ShouldEmbedCode(); }
};

//
//	FD3DMaterialUserInputCeil
//

struct FD3DMaterialUserInputCeil: FD3DMaterialUserInput
{
	FD3DMaterialUserInput*	X;

	// Constructor.

	FD3DMaterialUserInputCeil(FD3DMaterialUserInput* InX):
		X(InX)
	{}

	// FD3DMaterialUserInput interface.

	virtual FLinearColor GetValue(FMaterialInstance* MaterialInstance,FLOAT ObjectTime)
	{
		FLinearColor	ValueX = X->GetValue(MaterialInstance,ObjectTime);
		return FLinearColor(
				appCeil(ValueX.R),
				appCeil(ValueX.G),
				appCeil(ValueX.B),
				appCeil(ValueX.A)
				);
	}
	virtual UBOOL ShouldEmbedCode() { return X->ShouldEmbedCode(); }
};

//
//	FD3DMaterialUserInputFrac
//

struct FD3DMaterialUserInputFrac: FD3DMaterialUserInput
{
	FD3DMaterialUserInput*	X;

	// Constructor.

	FD3DMaterialUserInputFrac(FD3DMaterialUserInput* InX):
		X(InX)
	{}

	// FD3DMaterialUserInput interface.

	virtual FLinearColor GetValue(FMaterialInstance* MaterialInstance,FLOAT ObjectTime)
	{
		FLinearColor	ValueX = X->GetValue(MaterialInstance,ObjectTime);
		return FLinearColor(
				ValueX.R - appFloor(ValueX.R),
				ValueX.G - appFloor(ValueX.G),
				ValueX.B - appFloor(ValueX.B),
				ValueX.A - appFloor(ValueX.A)
				);
	}
	virtual UBOOL ShouldEmbedCode() { return X->ShouldEmbedCode(); }
};

//
//	FD3DMaterialShader::FD3DMaterialShader
//

FD3DMaterialShader::FD3DMaterialShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction,INT InMaterialResourceIndex,FGuid InPersistentMaterialId):
	FD3DShader(InSet,InFunction),
	PersistentMaterialId(InPersistentMaterialId),
	MaterialResourceIndex(InMaterialResourceIndex),
	SHBasisTexture(NULL),
	SHSkyBasisTexture(NULL),
	UserTexturesParameter(this,TEXT("UserTextures")),
	SHBasisTextureParameter(this,TEXT("SHBasisTexture")),
	SHSkyBasisTextureParameter(this,TEXT("SHSkyBasisTexture")),
	UserVectorInputsParameter(this,TEXT("UserVectorInputs")),
	UserScalarInputsParameter(this,TEXT("UserScalarInputs"))
{}

//
//	FD3DMaterialShader::~FD3DMaterialShader
//

FD3DMaterialShader::~FD3DMaterialShader()
{
	for(UINT TextureIndex = 0;TextureIndex < (UINT)UserTextures.Num();TextureIndex++)
		UserTextures(TextureIndex)->RemoveRef();
	for(UINT InputIndex = 0;InputIndex < (UINT)UserInputs.Num();InputIndex++)
		delete UserInputs(InputIndex);
	if(SHBasisTexture)
		SHBasisTexture->RemoveRef();
	if(SHSkyBasisTexture)
		SHSkyBasisTexture->RemoveRef();
}

//
//	FD3DMaterialCompiler
//

enum EShaderCodeChunkFlags
{
	SCF_RGBE_4BIT_EXPONENT			= 1,
	SCF_RGBE_8BIT_EXPONENT			= 2,
	SCF_RGBE						= SCF_RGBE_4BIT_EXPONENT | SCF_RGBE_8BIT_EXPONENT,
	SCF_REQUIRES_GAMMA_CORRECTION	= 4,
};

struct FD3DMaterialCompiler: FMaterialCompiler
{
	// FShaderCodeChunk

	struct FShaderCodeChunk
	{
		FString					Code;
		FD3DMaterialUserInput*	Input;
		EMaterialCodeType		Type;
		DWORD					Flags;

		FShaderCodeChunk(const TCHAR* InCode,EMaterialCodeType InType,DWORD InFlags):
			Code(InCode),
			Input(NULL),
			Type(InType),
			Flags(InFlags)
		{}

		FShaderCodeChunk(FD3DMaterialUserInput* InInput,const TCHAR* InCode,EMaterialCodeType InType,DWORD InFlags):
			Input(InInput),
			Code(InCode),
			Type(InType),
			Flags(InFlags)
		{}
	};

	TArray<FShaderCodeChunk>				CodeChunks;
	TArray<FTextureBase*>					Textures;
	UINT									NumUserTexCoords;
	TArray<FD3DMaterialUserInput*>			UserInputs;
	TArray<FD3DMaterialUserInputConstant*>	UserConstants;
	TArray<FD3DMaterialUserInput*>			UserVectorInputs;
	TArray<FD3DMaterialUserInput*>			UserScalarInputs;

	TArray<FMaterialCompilerGuard*>	GuardStack;

	// Constructor.

	FD3DMaterialCompiler():
		NumUserTexCoords(0)
	{}

	// DescribeType

	const TCHAR* DescribeType(EMaterialCodeType Type) const
	{
		switch(Type)
		{
		case MCT_Float1:		return TEXT("float1");
		case MCT_Float2:		return TEXT("float2");
		case MCT_Float3:		return TEXT("float3");
		case MCT_Float4:		return TEXT("float4");
		case MCT_Float:			return TEXT("float");
		case MCT_Texture2D:		return TEXT("texture2D");
		case MCT_TextureCube:	return TEXT("textureCube");
		case MCT_Texture3D:		return TEXT("texture3D");
		default: appErrorf(TEXT("Trying to describe invalid type: %u"),(UINT)Type);
		};
		return NULL;
	}

	// AddCodeChunk - Adds a formatted code string to the Code array and returns its index.

	INT AddCodeChunk(EMaterialCodeType Type,DWORD Flags,const TCHAR* Format,...)
	{
		INT		BufferSize		= 256;
		TCHAR*	FormattedCode	= NULL;
		INT		Result			= -1;

		while(Result == -1)
		{
			FormattedCode = (TCHAR*) appRealloc( FormattedCode, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT(FormattedCode,BufferSize - 1,Format,Format,Result);
			BufferSize *= 2;
		};
		FormattedCode[Result] = 0;

		INT	CodeIndex = CodeChunks.Num();
		new(CodeChunks) FShaderCodeChunk(FormattedCode,Type,Flags);
		
		appFree(FormattedCode);
		return CodeIndex;
	}

	// AddUserInput - Adds an input to the Code array and returns its index.

	INT AddUserInput(FD3DMaterialUserInput* UserInput,EMaterialCodeType Type,const TCHAR* Format,...)
	{
		INT		BufferSize		= 256;
		TCHAR*	FormattedCode	= NULL;
		INT		Result			= -1;

		while(Result == -1)
		{
			FormattedCode = (TCHAR*) appRealloc( FormattedCode, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT(FormattedCode,BufferSize - 1,Format,Format,Result);
			BufferSize *= 2;
		};
		FormattedCode[Result] = 0;

		UserInputs.AddItem(UserInput);

		INT	CodeIndex = CodeChunks.Num();
		new(CodeChunks) FShaderCodeChunk(UserInput,FormattedCode,Type,0);

		appFree(FormattedCode);
		return CodeIndex;
	}

	// AddUserConstant - Adds an input for a constant, reusing existing instances of the constant.

	INT AddUserConstant(const FLinearColor& Constant,EMaterialCodeType Type,const TCHAR* Format,...)
	{
		INT		BufferSize		= 256;
		TCHAR*	FormattedCode	= NULL;
		INT		Result			= -1;

		while(Result == -1)
		{
			FormattedCode = (TCHAR*) appRealloc( FormattedCode, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT(FormattedCode,BufferSize - 1,Format,Format,Result);
			BufferSize *= 2;
		};
		FormattedCode[Result] = 0;

		FD3DMaterialUserInputConstant*	UserInput = NULL;
		for(UINT ConstantIndex = 0;ConstantIndex < (UINT)UserConstants.Num();ConstantIndex++)
		{
			if(UserConstants(ConstantIndex)->Value == Constant)
			{
				UserInput = UserConstants(ConstantIndex);
				break;
			}
		}

		if(!UserInput)
		{
			UserInput = new FD3DMaterialUserInputConstant(Constant,Type == MCT_Float4 ? 4 : (Type == MCT_Float3 ? 3 : (Type == MCT_Float2 ? 2 : 1)));
			UserInputs.AddItem(UserInput);
			UserConstants.AddItem(UserInput);
		}

		INT	CodeIndex = CodeChunks.Num();
		new(CodeChunks) FShaderCodeChunk(UserInput,FormattedCode,Type,0);
		
		appFree(FormattedCode);
		return CodeIndex;
	}

	// AccessUserInput - Adds code to access an input to the Code array and returns its index.

	INT AccessUserInput(INT Index)
	{
		check(Index >= 0 && Index < CodeChunks.Num());

		const FShaderCodeChunk&	CodeChunk = CodeChunks(Index);

		check(CodeChunk.Input);

		TCHAR*	FormattedCode = (TCHAR*) appMalloc( 65536 * sizeof(TCHAR) );
		if(CodeChunk.Type == MCT_Float)
		{
			INT	ScalarInputIndex = UserScalarInputs.AddUniqueItem(CodeChunk.Input);
			appSprintf(FormattedCode,TEXT("UserScalarInputs[%u][%u]"),ScalarInputIndex / 4,ScalarInputIndex & 3);
		}
		else if(CodeChunk.Type & MCT_Float)
		{
			INT				VectorInputIndex = UserVectorInputs.AddUniqueItem(CodeChunk.Input);
			const TCHAR*	Mask;
			switch(CodeChunk.Type)
			{
			case MCT_Float:
			case MCT_Float1: Mask = TEXT(".r"); break;
			case MCT_Float2: Mask = TEXT(".rg"); break;
			case MCT_Float3: Mask = TEXT(".rgb"); break;
			default: Mask = TEXT(""); break;
			};
			appSprintf(FormattedCode,TEXT("UserVectorInputs[%u]%s"),VectorInputIndex,Mask);
		}
		else
			appErrorf(TEXT("User input of unknown type: %s"),DescribeType(CodeChunk.Type));

		INT	CodeIndex = CodeChunks.Num();
		new(CodeChunks) FShaderCodeChunk(FormattedCode,CodeChunks(Index).Type,0);

		appFree(FormattedCode);
		return CodeIndex;
	}

	// GetParameterCode

	const TCHAR* GetParameterCode(INT Index)
	{
		check(Index >= 0 && Index < CodeChunks.Num());
		if(!CodeChunks(Index).Input || CodeChunks(Index).Input->ShouldEmbedCode())
			return *CodeChunks(Index).Code;
		else
			return *CodeChunks(AccessUserInput(Index)).Code;
	}

	// CoerceParameter

	FString CoerceParameter(INT Index,EMaterialCodeType DestType)
	{
		check(Index >= 0 && Index < CodeChunks.Num());

		const FShaderCodeChunk&	CodeChunk = CodeChunks(Index);

		if(CodeChunk.Type & DestType)
			return GetParameterCode(Index);
		else
		{
			Errorf(TEXT("Coercion failed: %s: %s -> %s"),*CodeChunk.Code,DescribeType(CodeChunk.Type),DescribeType(DestType));
			return TEXT("");
		}
	}
	
	// GetFixedParameterCode

	const TCHAR* GetFixedParameterCode(INT Index) const
	{
		check(Index >= 0 && Index < CodeChunks.Num());
		check(!CodeChunks(Index).Input || CodeChunks(Index).Input->ShouldEmbedCode());
		return *CodeChunks(Index).Code;
	}

	// GetParameterType

	EMaterialCodeType GetParameterType(INT Index) const
	{
		check(Index >= 0 && Index < CodeChunks.Num());
		return CodeChunks(Index).Type;
	}

	// GetParameterInput

	FD3DMaterialUserInput* GetParameterInput(INT Index) const
	{
		check(Index >= 0 && Index < CodeChunks.Num());
		return CodeChunks(Index).Input;
	}

	// GetParameterFlags

	DWORD GetParameterFlags(INT Index) const
	{
		check(Index >= 0 && Index < CodeChunks.Num());
		return CodeChunks(Index).Flags;
	}

	// GetArithmeticResultType

	EMaterialCodeType GetArithmeticResultType(INT A,INT B)
	{
		check(A >= 0 && A < CodeChunks.Num());
		check(B >= 0 && B < CodeChunks.Num());

		EMaterialCodeType	TypeA = CodeChunks(A).Type,
							TypeB = CodeChunks(B).Type;

		if(!(TypeA & MCT_Float) || !(TypeB & MCT_Float))
			Errorf(TEXT("Attempting to perform arithmetic on non-numeric types: %s %s"),DescribeType(TypeA),DescribeType(TypeB));

		if(TypeA == TypeB)
			return TypeA;
		else if(TypeA & TypeB)
		{
			if(TypeA == MCT_Float)
				return TypeB;
			else
			{
				check(TypeB == MCT_Float);
				return TypeA;
			}
		}
		else
		{
			Errorf(TEXT("Arithmetic between types %s and %s are undefined"),DescribeType(TypeA),DescribeType(TypeB));
			return MCT_Float;
		}
	}

	// GetNumComponents - Returns the number of components in a vector type.

	UINT GetNumComponents(EMaterialCodeType Type)
	{
		switch(Type)
		{
			case MCT_Float:
			case MCT_Float1: return 1;
			case MCT_Float2: return 2;
			case MCT_Float3: return 3;
			case MCT_Float4: return 4;
			default: Errorf(TEXT("Attempting to get component count of non-vector type %s"),DescribeType(Type));
		}
		return 0;
	}

	// GetVectorType - Returns the vector type containing a given number of components.

	EMaterialCodeType GetVectorType(UINT NumComponents)
	{
		switch(NumComponents)
		{
		case 1: return MCT_Float;
		case 2: return MCT_Float2;
		case 3: return MCT_Float3;
		case 4: return MCT_Float4;
		default: Errorf(TEXT("Requested %u component vector type does not exist"),NumComponents);
		};
		return MCT_Unknown;
	}

	// FMaterialCompiler interface.

	virtual INT Error(const TCHAR* Text)
	{
		if(GuardStack.Num())
			GuardStack(GuardStack.Num() - 1)->Error(Text);
		throw Text;
		return INDEX_NONE;
	}

	virtual void EnterGuard(FMaterialCompilerGuard* Guard)
	{
		GuardStack.AddItem(Guard);
	}

	virtual void ExitGuard()
	{
		check(GuardStack.Num());
		GuardStack.Remove(GuardStack.Num() - 1);
	}

	virtual EMaterialCodeType GetType(INT Code)
	{
		return GetParameterType(Code);
	}

	virtual INT ForceCast(INT Code,EMaterialCodeType DestType)
	{
		if(GetParameterInput(Code) && !GetParameterInput(Code)->ShouldEmbedCode())
			return ForceCast(AccessUserInput(Code),DestType);

		EMaterialCodeType	SourceType = GetParameterType(Code);

		if(SourceType & DestType)
			return Code;
		else if((SourceType & MCT_Float) && (DestType & MCT_Float))
		{
			UINT	NumSourceComponents = GetNumComponents(SourceType),
					NumDestComponents = GetNumComponents(DestType);

			if(NumSourceComponents > NumDestComponents) // Use a mask to select the first NumDestComponents components from the source.
			{
				const TCHAR*	Mask;
				switch(NumDestComponents)
				{
				case 1: Mask = TEXT(".r"); break;
				case 2: Mask = TEXT(".rg"); break;
				case 3: Mask = TEXT(".rgb"); break;
				default: appErrorf(TEXT("Should never get here!")); return INDEX_NONE;
				};

				return AddCodeChunk(DestType,0,TEXT("%s%s"),GetParameterCode(Code),Mask);
			}
			else if(NumSourceComponents < NumDestComponents) // Pad the source vector up to NumDestComponents.
			{
				UINT	NumPadComponents = NumDestComponents - NumSourceComponents;
				return AddCodeChunk(
					DestType,
					0,
					TEXT("%s(%s%s%s%s)"),
					DescribeType(DestType),
					GetParameterCode(Code),
					NumPadComponents >= 1 ? TEXT(",0") : TEXT(""),
					NumPadComponents >= 2 ? TEXT(",0") : TEXT(""),
					NumPadComponents >= 3 ? TEXT(",0") : TEXT("")
					);
			}
			else
				return Code;
		}
		else
			return Errorf(TEXT("Cannot force a cast between non-numeric types."));
	}

	virtual INT VectorParameter(FName ParameterName)
	{
		return AddUserInput(new FD3DMaterialUserInputParameter(ParameterName,1),MCT_Float4,TEXT(""));
	}
	
	virtual INT ScalarParameter(FName ParameterName)
	{
		return AddUserInput(new FD3DMaterialUserInputParameter(ParameterName,0),MCT_Float,TEXT(""));
	}

	virtual INT Constant(FLOAT X)
	{
		return AddUserConstant(FLinearColor(X,0,0,0),MCT_Float,TEXT("(%0.8f)"),X);
	}

	virtual INT Constant2(FLOAT X,FLOAT Y)
	{
		return AddUserConstant(FLinearColor(X,Y,0,0),MCT_Float2,TEXT("float2(%0.8f,%0.8f)"),X,Y);
	}

	virtual INT Constant3(FLOAT X,FLOAT Y,FLOAT Z)
	{
		return AddUserConstant(FLinearColor(X,Y,Z,0),MCT_Float3,TEXT("float3(%0.8f,%0.8f,%0.8f)"),X,Y,Z);
	}

	virtual INT Constant4(FLOAT X,FLOAT Y,FLOAT Z,FLOAT W)
	{
		return AddUserConstant(FLinearColor(X,Y,Z,W),MCT_Float4,TEXT("float4(%0.8f,%0.8f,%0.8f,%0.8f)"),X,Y,Z,W);
	}

	virtual INT Texture(FTextureBase* Texture)
	{
		INT					TextureIndex = Textures.AddUniqueItem(Texture);
		FResourceType*		ResourceType = Texture->Type();
		EMaterialCodeType	ShaderType;
		if(ResourceType->IsA(FTexture2D::StaticType))
			ShaderType = MCT_Texture2D;
		else if(ResourceType->IsA(FTextureCube::StaticType))
			ShaderType = MCT_TextureCube;
		else if(ResourceType->IsA(FTexture3D::StaticType))
			ShaderType = MCT_Texture3D;
		else
		{
			Errorf(TEXT("Unsupported texture type used: %s"),ResourceType->Name);
			return INDEX_NONE;
		}

		DWORD Flags = 0;
		if( Texture->IsRGBE() )
			Flags |= Texture->Format == PF_A8R8G8B8 ? SCF_RGBE_8BIT_EXPONENT : SCF_RGBE_4BIT_EXPONENT;
		if( (GPixelFormats[Texture->Format].PlatformFlags & D3DPF_REQUIRES_GAMMA_CORRECTION) && Texture->IsGammaCorrected() )
			Flags |= SCF_REQUIRES_GAMMA_CORRECTION;

		return AddCodeChunk(ShaderType,Flags,TEXT("UserTextures[%u]"),TextureIndex);
	}

	virtual INT ObjectTime()
	{
		return AddUserInput(new FD3DMaterialUserInputTime(0),MCT_Float,TEXT("ObjectTime"));
	}

	virtual INT SceneTime()
	{
		return AddUserInput(new FD3DMaterialUserInputTime(1),MCT_Float,TEXT("SceneTime"));
	}

	virtual INT PeriodicHint(INT PeriodicCode)
	{
		if(GetParameterInput(PeriodicCode))
			return AddUserInput(new FD3DMaterialUserInputPeriodic(GetParameterInput(PeriodicCode)),GetParameterType(PeriodicCode),TEXT("%s"),GetParameterCode(PeriodicCode));
		else
			return PeriodicCode;
	}

	virtual INT Sine(INT X)
	{
		if(GetParameterInput(X))
			return AddUserInput(new FD3DMaterialUserInputSine(GetParameterInput(X),0),MCT_Float,TEXT("sin(%s)"),*CoerceParameter(X,MCT_Float));
		else
			return AddCodeChunk(MCT_Float,0,TEXT("sin(%s)"),*CoerceParameter(X,MCT_Float));
	}
	
	virtual INT Cosine(INT X)
	{
		if(GetParameterInput(X))
			return AddUserInput(new FD3DMaterialUserInputSine(GetParameterInput(X),1),MCT_Float,TEXT("cos(%s)"),*CoerceParameter(X,MCT_Float));
		else
			return AddCodeChunk(MCT_Float,0,TEXT("cos(%s)"),*CoerceParameter(X,MCT_Float));
	}

	virtual INT Floor(INT X)
	{
		if(GetParameterInput(X))
			return AddUserInput(new FD3DMaterialUserInputFloor(GetParameterInput(X)),GetParameterType(X),TEXT("floor(%s)"),GetParameterCode(X));
		else
			return AddCodeChunk(GetParameterType(X),0,TEXT("floor(%s)"),GetParameterCode(X));
	}

	virtual INT Ceil(INT X)
	{
		if(GetParameterInput(X))
			return AddUserInput(new FD3DMaterialUserInputCeil(GetParameterInput(X)),GetParameterType(X),TEXT("ceil(%s)"),GetParameterCode(X));
		else
			return AddCodeChunk(GetParameterType(X),0,TEXT("ceil(%s)"),GetParameterCode(X));
	}

	virtual INT Frac(INT X)
	{
		if(GetParameterInput(X))
			return AddUserInput(new FD3DMaterialUserInputFrac(GetParameterInput(X)),GetParameterType(X),TEXT("frac(%s)"),GetParameterCode(X));
		else
			return AddCodeChunk(GetParameterType(X),0,TEXT("frac(%s)"),GetParameterCode(X));
	}

	virtual INT ReflectionVector()
	{
		return AddCodeChunk(MCT_Float3,0,TEXT("Input.TangentReflectionVector"));
	}

	virtual INT CameraVector()
	{
		return AddCodeChunk(MCT_Float3,0,TEXT("Input.TangentCameraVector"));
	}

	virtual INT TextureCoordinate(UINT CoordinateIndex)
	{
		NumUserTexCoords = ::Max(CoordinateIndex + 1,NumUserTexCoords);
		return AddCodeChunk(MCT_Float2,0,TEXT("Input.UserTexCoords[%u]"),CoordinateIndex);
	}

	virtual INT TextureSample(INT TextureIndex,INT CoordinateIndex)
	{
		EMaterialCodeType	TextureType = GetParameterType(TextureIndex);
		DWORD				Flags		= GetParameterFlags(TextureIndex);

		FString				SampleCode;

		switch(TextureType)
		{
		case MCT_Texture2D:
			SampleCode = TEXT("tex2D(%s,%s)");
			break;
		case MCT_TextureCube:
			SampleCode = TEXT("texCUBE(%s,%s)");
			break;
		case MCT_Texture3D:
			SampleCode = TEXT("tex3D(%s,%s)");
			break;		
		}

		if( Flags & SCF_RGBE_4BIT_EXPONENT )
			SampleCode = FString::Printf( TEXT("expandCompressedRGBE(%s)"), *SampleCode );

		if( Flags & SCF_RGBE_8BIT_EXPONENT )
			SampleCode = FString::Printf( TEXT("expandRGBE(%s)"), *SampleCode );

		if( Flags & SCF_REQUIRES_GAMMA_CORRECTION )
			SampleCode = FString::Printf( TEXT("gammaCorrect(%s)"), *SampleCode );

		switch(TextureType)
		{
		case MCT_Texture2D:
			return AddCodeChunk(
					MCT_Float4,
					0,
					*SampleCode,
					*CoerceParameter(TextureIndex,MCT_Texture2D),
					*CoerceParameter(CoordinateIndex,MCT_Float2)
					);
		case MCT_TextureCube:
			return AddCodeChunk(
					MCT_Float4,
					0,
					*SampleCode,
					*CoerceParameter(TextureIndex,MCT_TextureCube),
					*CoerceParameter(CoordinateIndex,MCT_Float3)
					);
		case MCT_Texture3D:
			return AddCodeChunk(
					MCT_Float4,
					0,
					*SampleCode,
					*CoerceParameter(TextureIndex,MCT_Texture3D),
					*CoerceParameter(CoordinateIndex,MCT_Float3)
					);
		default:
			Errorf(TEXT("Sampling unknown texture type: %s"),DescribeType(TextureType));
			return INDEX_NONE;
		};
	}

	virtual INT VertexColor()
	{
		return AddCodeChunk(MCT_Float4,0,TEXT("Input.UserVertexColor"));
	}

	virtual INT Add(INT A,INT B)
	{
		if(GetParameterInput(A) && GetParameterInput(B))
			return AddUserInput(new FD3DMaterialUserInputFoldedMath(GetParameterInput(A),GetParameterInput(B),FMO_Add),GetArithmeticResultType(A,B),TEXT("(%s + %s)"),GetParameterCode(A),GetParameterCode(B));
		else
			return AddCodeChunk(GetArithmeticResultType(A,B),0,TEXT("(%s + %s)"),GetParameterCode(A),GetParameterCode(B));
	}

	virtual INT Sub(INT A,INT B)
	{
		if(GetParameterInput(A) && GetParameterInput(B))
			return AddUserInput(new FD3DMaterialUserInputFoldedMath(GetParameterInput(A),GetParameterInput(B),FMO_Sub),GetArithmeticResultType(A,B),TEXT("(%s - %s)"),GetParameterCode(A),GetParameterCode(B));
		else
			return AddCodeChunk(GetArithmeticResultType(A,B),0,TEXT("(%s - %s)"),GetParameterCode(A),GetParameterCode(B));
	}

	virtual INT Mul(INT A,INT B)
	{
		if(GetParameterInput(A) && GetParameterInput(B))
			return AddUserInput(new FD3DMaterialUserInputFoldedMath(GetParameterInput(A),GetParameterInput(B),FMO_Mul),GetArithmeticResultType(A,B),TEXT("(%s * %s)"),GetParameterCode(A),GetParameterCode(B));
		else
			return AddCodeChunk(GetArithmeticResultType(A,B),0,TEXT("(%s * %s)"),GetParameterCode(A),GetParameterCode(B));
	}

	virtual INT Div(INT A,INT B)
	{
		if(GetParameterInput(A) && GetParameterInput(B))
			return AddUserInput(new FD3DMaterialUserInputFoldedMath(GetParameterInput(A),GetParameterInput(B),FMO_Div),GetArithmeticResultType(A,B),TEXT("(%s / %s)"),GetParameterCode(A),GetParameterCode(B));
		else
			return AddCodeChunk(GetArithmeticResultType(A,B),0,TEXT("(%s / %s)"),GetParameterCode(A),GetParameterCode(B));
	}

	virtual INT Dot(INT A,INT B)
	{
		if(GetParameterInput(A) && GetParameterInput(B))
			return AddUserInput(new FD3DMaterialUserInputFoldedMath(GetParameterInput(A),GetParameterInput(B),FMO_Dot),GetArithmeticResultType(A,B),TEXT("dot(%s,%s)"),GetParameterCode(A),GetParameterCode(B));
		else
			return AddCodeChunk(MCT_Float,0,TEXT("dot(%s,%s)"),GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
	}

	virtual INT Cross(INT A,INT B)
	{
		return AddCodeChunk(MCT_Float3,0,TEXT("cross(%s,%s)"),*CoerceParameter(A,MCT_Float3),*CoerceParameter(B,MCT_Float3));
	}

	virtual INT SquareRoot(INT X)
	{
		if(GetParameterInput(X))
			return AddUserInput(new FD3DMaterialUserInputSquareRoot(GetParameterInput(X)),MCT_Float,TEXT("sqrt(%s)"),*CoerceParameter(X,MCT_Float1));
		else
			return AddCodeChunk(MCT_Float,0,TEXT("sqrt(%s)"),*CoerceParameter(X,MCT_Float1));
	}

	virtual INT Lerp(INT X,INT Y,INT A)
	{
		return AddCodeChunk(GetArithmeticResultType(X,Y),0,TEXT("lerp(%s,%s,%s)"),GetParameterCode(X),GetParameterCode(Y),*CoerceParameter(A,MCT_Float1));
	}

	virtual INT Min(INT A,INT B)
	{
		if(GetParameterInput(A) && GetParameterInput(B))
			return AddUserInput(new FD3DMaterialUserInputMin(GetParameterInput(A),GetParameterInput(B)),GetParameterType(A),TEXT("min(%s,%s)"),GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		else
			return AddCodeChunk(GetParameterType(A),0,TEXT("min(%s,%s)"),GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
	}

	virtual INT Max(INT A,INT B)
	{
		if(GetParameterInput(A) && GetParameterInput(B))
			return AddUserInput(new FD3DMaterialUserInputMax(GetParameterInput(A),GetParameterInput(B)),GetParameterType(A),TEXT("max(%s,%s)"),GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
		else
			return AddCodeChunk(GetParameterType(A),0,TEXT("max(%s,%s)"),GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
	}

	virtual INT Clamp(INT X,INT A,INT B)
	{
		if(GetParameterInput(X) && GetParameterInput(A) && GetParameterInput(B))
			return AddUserInput(new FD3DMaterialUserInputClamp(GetParameterInput(X),GetParameterInput(A),GetParameterInput(B)),GetParameterType(X),TEXT("clamp(%s,%s,%s)"),GetParameterCode(X),*CoerceParameter(A,GetParameterType(X)),*CoerceParameter(B,GetParameterType(X)));
		else
			return AddCodeChunk(GetParameterType(X),0,TEXT("clamp(%s,%s,%s)"),GetParameterCode(X),*CoerceParameter(A,GetParameterType(X)),*CoerceParameter(B,GetParameterType(X)));
	}

	virtual INT ComponentMask(INT Vector,UBOOL R,UBOOL G,UBOOL B,UBOOL A)
	{
		EMaterialCodeType	VectorType = GetParameterType(Vector);

		if(	A && (VectorType & MCT_Float) < MCT_Float4 ||
			B && (VectorType & MCT_Float) < MCT_Float3 ||
			G && (VectorType & MCT_Float) < MCT_Float2 ||
			R && (VectorType & MCT_Float) < MCT_Float1)
			Errorf(TEXT("Not enough components in (%s: %s) for component mask %u%u%u%u"),GetParameterCode(Vector),DescribeType(GetParameterType(Vector)),R,G,B,A);

		EMaterialCodeType	ResultType;
		switch((R ? 1 : 0) + (G ? 1 : 0) + (B ? 1 : 0) + (A ? 1 : 0))
		{
		case 1: ResultType = MCT_Float; break;
		case 2: ResultType = MCT_Float2; break;
		case 3: ResultType = MCT_Float3; break;
		case 4: ResultType = MCT_Float4; break;
		default: Errorf(TEXT("Couldn't determine result type of component mask %u%u%u%u"),R,G,B,A); return INDEX_NONE;
		};

		return AddCodeChunk(
			ResultType,
			0,
			TEXT("%s.%s%s%s%s"),
			GetParameterCode(Vector),
			R ? TEXT("r") : TEXT(""),
			G ? TEXT("g") : TEXT(""),
			B ? TEXT("b") : TEXT(""),
			A ? TEXT("a") : TEXT("")
			);
	}

	virtual INT AppendVector(INT A,INT B)
	{
		INT					NumResultComponents = GetNumComponents(GetParameterType(A)) + GetNumComponents(GetParameterType(B));
		EMaterialCodeType	ResultType = GetVectorType(NumResultComponents);

		if(GetParameterInput(A) && GetParameterInput(B))
			return AddUserInput(new FD3DMaterialUserInputAppendVector(GetParameterInput(A),GetParameterInput(B),GetNumComponents(GetParameterType(A))),ResultType,TEXT("float%u(%s,%s)"),NumResultComponents,GetParameterCode(A),GetParameterCode(B));
		else
			return AddCodeChunk(ResultType,0,TEXT("float%u(%s,%s)"),NumResultComponents,GetParameterCode(A),GetParameterCode(B));
	}
};

//
//	GenerateMaterialCode
//

FString GenerateMaterialCode(FMaterial* Material,FD3DMaterialCompiler& Compiler,TArray<FMaterialError>& OutErrors)
{
	try
	{
		FString	UserShaderTemplate = GD3DRenderDevice->GetShaderSource(TEXT("UserShaderTemplate.hlsl"));

		INT	NormalChunk = Compiler.ForceCast(Material->CompileProperty(MP_Normal,&Compiler),MCT_Float3),
			EmissiveColorChunk = Compiler.ForceCast(Material->CompileProperty(MP_EmissiveColor,&Compiler),MCT_Float3),
			DiffuseColorChunk = Compiler.ForceCast(Material->CompileProperty(MP_DiffuseColor,&Compiler),MCT_Float3),
			SpecularColorChunk = Compiler.ForceCast(Material->CompileProperty(MP_SpecularColor,&Compiler),MCT_Float3),
			SpecularPowerChunk = Compiler.ForceCast(Material->CompileProperty(MP_SpecularPower,&Compiler),MCT_Float1),
			OpacityChunk = Compiler.ForceCast(Material->CompileProperty(MP_Opacity,&Compiler),MCT_Float1),
			MaskChunk = Compiler.ForceCast(Material->CompileProperty(MP_OpacityMask,&Compiler),MCT_Float1),
			DistortionChunk = Compiler.ForceCast(Material->CompileProperty(MP_Distortion,&Compiler),MCT_Float2),
			TwoSidedLightingMaskChunk = Compiler.ForceCast(Material->CompileProperty(MP_TwoSidedLightingMask,&Compiler),MCT_Float3),
			SHMChunk = Compiler.ForceCast(Material->CompileProperty(MP_SHM,&Compiler),MCT_Float4);

		return FString::Printf(
			*UserShaderTemplate,
			Compiler.Textures.Num(),
			Compiler.NumUserTexCoords,
			Compiler.UserVectorInputs.Num(),
			Compiler.UserScalarInputs.Num(),
			Compiler.GetFixedParameterCode(NormalChunk),
			Compiler.GetFixedParameterCode(EmissiveColorChunk),
			Compiler.GetFixedParameterCode(DiffuseColorChunk),
			Compiler.GetFixedParameterCode(SpecularColorChunk),
			Compiler.GetFixedParameterCode(SpecularPowerChunk),
			Compiler.GetFixedParameterCode(OpacityChunk),
			Compiler.GetFixedParameterCode(MaskChunk),
			*FString::Printf(TEXT("%.5f"),Material->OpacityMaskClipValue),
			Compiler.GetFixedParameterCode(DistortionChunk),
			Compiler.GetFixedParameterCode(TwoSidedLightingMaskChunk),
			Compiler.GetFixedParameterCode(SHMChunk)
			);
	}
	catch(const TCHAR* ErrorText)
	{
		debugf(TEXT("Failed to generate material code: %s"),ErrorText);

		// If any errors occured while generating the code, log them and use the next fallback material.
		FMaterialError	Error;
		Error.Description = ErrorText;
		new(OutErrors) FMaterialError(Error);
		return TEXT("");
	}
}

//
//	AppendErrors
//

void AppendErrors(TArray<FMaterialError>& Errors,const TArray<FMaterialError>& OtherErrors)
{
	for(INT OtherErrorIndex = 0;OtherErrorIndex < OtherErrors.Num();OtherErrorIndex++)
	{
		UBOOL	Duplicate = 0;
		for(INT ErrorIndex = 0;ErrorIndex < Errors.Num();ErrorIndex++)
		{
			if(!appStricmp(*Errors(ErrorIndex).Description,*OtherErrors(OtherErrorIndex).Description))
			{
				Duplicate = 1;
				break;
			}
		}
		if(!Duplicate)
		{
			FMaterialError*	Error = new(Errors) FMaterialError;
			Error->Description = OtherErrors(OtherErrorIndex).Description;
		}
	}
}

//
//	FD3DMaterialShader::Cache
//

void FD3DMaterialShader::Cache(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions,FMaterial* Material)
{
	FMaterial*	FallbackMaterials[] =
	{
		Material,
		GEngine->DefaultMaterial->MaterialResources[0]
	};
	UINT		NumFallbacks = 2;

	Errors.Empty();

	for(UINT FallbackIndex = 0;FallbackIndex < NumFallbacks;FallbackIndex++)
	{
		// Generate the user shader code.

		FD3DMaterialCompiler	Compiler;

		Compiler.NumUserTexCoords = GetMaxCoordinateIndex();

		AddDefinition(Definitions,TEXT("SHADERBLENDING_SOLID"),TEXT("%u"),FallbackMaterials[FallbackIndex]->BlendMode == BLEND_Opaque);
		AddDefinition(Definitions,TEXT("SHADERBLENDING_MASKED"),TEXT("%u"),FallbackMaterials[FallbackIndex]->BlendMode == BLEND_Masked);
		AddDefinition(Definitions,TEXT("SHADERBLENDING_TRANSLUCENT"),TEXT("%u"),FallbackMaterials[FallbackIndex]->BlendMode == BLEND_Translucent);
		AddDefinition(Definitions,TEXT("SHADERBLENDING_ADDITIVE"),TEXT("%u"),FallbackMaterials[FallbackIndex]->BlendMode == BLEND_Additive);

		AddDefinition(Definitions,TEXT("SHADER_TWOSIDED"),TEXT("%u"),FallbackMaterials[FallbackIndex]->TwoSided);
		AddDefinition(Definitions,TEXT("SHADER_UNLIT"),TEXT("%u"),FallbackMaterials[FallbackIndex]->Unlit);
		AddDefinition(Definitions,TEXT("SHADER_NONDIRECTIONALLIGHTING"),TEXT("%u"),FallbackMaterials[FallbackIndex]->NonDirectionalLighting);
		AddDefinition(Definitions,TEXT("SHADER_SHM"),TEXT("%u"),FallbackMaterials[FallbackIndex]->SourceSHM != NULL);

		TArray<FMaterialError>	NewErrors;
		FString TempUserShaderCode = GenerateMaterialCode(FallbackMaterials[FallbackIndex],Compiler,NewErrors);

		if(NewErrors.Num())
		{
			AppendErrors(Errors,NewErrors);
			continue;
		}

		// Cache the textures used by the generated code.

		for(UINT TextureIndex = 0;TextureIndex < (UINT)Compiler.Textures.Num();TextureIndex++)
		{
			FTextureBase*	Texture			= Compiler.Textures(TextureIndex);
			FD3DTexture*	CachedTexture	= (FD3DTexture*)GD3DRenderDevice->Resources.GetCachedResource(Texture);
			CachedTexture->AddRef();
			UserTextures.AddItem(CachedTexture);
		}

		if(FallbackMaterials[FallbackIndex]->SourceSHM)
		{
			SHBasisTexture		= (FD3DTextureCube*)GD3DRenderDevice->Resources.GetCachedResource(&FallbackMaterials[FallbackIndex]->SourceSHM->OptimizedBasisCubemaps(0));
			SHSkyBasisTexture	= (FD3DTextureCube*)GD3DRenderDevice->Resources.GetCachedResource(&FallbackMaterials[FallbackIndex]->SourceSHM->OptimizedBasisCubemaps(0));

			SHBasisTexture->AddRef();
			SHSkyBasisTexture->AddRef();
		}

		NumUserTexCoords	= Compiler.NumUserTexCoords;
		UserInputs			= Compiler.UserInputs;
		UserVectorInputs	= Compiler.UserVectorInputs;
		UserScalarInputs	= Compiler.UserScalarInputs;

		// Compile the shader.
		UserShaderCode = TempUserShaderCode;
		TArray<FString>	CompileErrors = FD3DShader::Cache(ShaderFilename,VertexShaderFunction,PixelShaderFunction,Definitions);
#if !ALLOW_DUMP_SHADERS
		UserShaderCode = TEXT("");
#endif

		for(UINT ErrorIndex = 0;ErrorIndex < (UINT)CompileErrors.Num();ErrorIndex++)
		{
			FMaterialError	Error;
			Error.Description = CompileErrors(ErrorIndex);
			new(Errors) FMaterialError(Error);
		}

		// Compilation succeeded, don't fallback.

		MaterialName = Material->DescribeResource();

		if(!CompileErrors.Num())
			return;
	}

	FString	ErrorText = FString::Printf(TEXT("Failed to compile fallback material for %s - %s"),*Description,*GetTypeDescription());
	for(UINT ErrorIndex = 0;ErrorIndex < (UINT)Errors.Num();ErrorIndex++)
		ErrorText = ErrorText + TEXT("\r\n") + Errors(ErrorIndex).Description;

	appErrorf(*ErrorText);
}

//
//	FD3DMaterialShader::Set
//

void FD3DMaterialShader::Set(FMaterialInstance* MaterialInstance,FLOAT ObjectTime)
{
	FD3DShader::Set();

	for(UINT TextureIndex = 0;TextureIndex < (UINT)UserTextures.Num() && TextureIndex < UserTexturesParameter.NumSamplers;TextureIndex++)
		UserTexturesParameter.SetUserTexture(UserTextures(TextureIndex),TextureIndex);

	for(UINT InputIndex = 0;InputIndex < (UINT)UserVectorInputs.Num() && InputIndex < UserVectorInputsParameter.NumPixelRegisters;InputIndex++)
		UserVectorInputsParameter.SetColor(UserVectorInputs(InputIndex)->GetValue(MaterialInstance,ObjectTime),InputIndex);

	for(INT InputIndex = 0;InputIndex < UserScalarInputs.Num() && InputIndex / 4 < (INT)UserScalarInputsParameter.NumPixelRegisters;InputIndex += 4)
	{
		UserScalarInputsParameter.SetVector(
			(InputIndex + 0 < UserScalarInputs.Num()) ? UserScalarInputs(InputIndex + 0)->GetValue(MaterialInstance,ObjectTime).R : 0.0f,
			(InputIndex + 1 < UserScalarInputs.Num()) ? UserScalarInputs(InputIndex + 1)->GetValue(MaterialInstance,ObjectTime).R : 0.0f,
			(InputIndex + 2 < UserScalarInputs.Num()) ? UserScalarInputs(InputIndex + 2)->GetValue(MaterialInstance,ObjectTime).R : 0.0f,
			(InputIndex + 3 < UserScalarInputs.Num()) ? UserScalarInputs(InputIndex + 3)->GetValue(MaterialInstance,ObjectTime).R : 0.0f,
			InputIndex / 4
			);
	}

	if(SHBasisTexture)
		SHBasisTextureParameter.SetTexture(SHBasisTexture->Texture,D3DTEXF_LINEAR);

	if(SHSkyBasisTexture)
		SHSkyBasisTextureParameter.SetTexture(SHSkyBasisTexture->Texture,D3DTEXF_LINEAR);
}

//
//	FD3DMaterialShader::GetShaderSource
//

FString FD3DMaterialShader::GetShaderSource(const TCHAR* Filename)
{
	if(!appStricmp(Filename,TEXT("UserShader.hlsl")))
		return UserShaderCode;
	else
		return FD3DShader::GetShaderSource(Filename);
}

//
//	FD3DMaterialShader::Dump
//

void FD3DMaterialShader::Dump(FOutputDevice& Ar)
{
	Ar.Logf(TEXT("%s: MaterialResourceIndex=%i, Function=%u, Cached=%u, NumRefs=%u"),*GetTypeDescription(),MaterialResourceIndex,(UINT)Function,Cached,NumRefs);
	//Ar.Logf(TEXT("%s"),*UserShaderCode);
}

//
//	FD3DEmissiveTileShader::Set
//

void FD3DEmissiveTileShader::Set(FMaterialInstance* MaterialInstance)
{
	FD3DMaterialShader::Set(MaterialInstance,appSeconds());

	FPlane IdColor = FColor(GD3DRenderInterface.CurrentHitProxyIndex).Plane();
	HitProxyIdParameter.SetVector(IdColor.X,IdColor.Y,IdColor.Z,IdColor.W);
}

//
//	FD3DEmissiveTileShader::CompileShaders
//

TArray<FString> FD3DEmissiveTileShader::CompileShaders(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions,INT& VertexShaderCode,INT& PixelShaderCode,TArray<FD3DUnmappedShaderParameter>& UnmappedParameters)
{
	TArray<FString>	Errors;

	TMap<FGuid,FD3DCompiledShader>&	CompiledShaderMap = 
		HitProxy ? 
			GD3DRenderDevice->CompiledEmissiveTileHitProxyShaders :
			GD3DRenderDevice->CompiledEmissiveTileShaders;

	if(PersistentMaterialId != FGuid(0,0,0,0))
	{
		FD3DCompiledShader*	CachedCompiledShader = CompiledShaderMap.Find(PersistentMaterialId);

		if(CachedCompiledShader)
		{
			VertexShaderCode = CachedCompiledShader->VertexShaderCode;
			PixelShaderCode = CachedCompiledShader->PixelShaderCode;
			UnmappedParameters = CachedCompiledShader->UnmappedParameters;
			return Errors;
		}
	}

	Errors = FD3DShader::CompileShaders(ShaderFilename,VertexShaderFunction,PixelShaderFunction,Definitions,VertexShaderCode,PixelShaderCode,UnmappedParameters);

	if(PersistentMaterialId != FGuid(0,0,0,0) && !Errors.Num())
	{
		FD3DCompiledShader	CompiledShader;
		CompiledShader.VertexShaderCode = VertexShaderCode;
		CompiledShader.PixelShaderCode = PixelShaderCode;
		CompiledShader.UnmappedParameters = UnmappedParameters;
		CompiledShaderMap.Set(PersistentMaterialId,CompiledShader);
	}

	return Errors;
}

//
//	UD3DRenderDevice::CompileMaterial
//

void AppendShaderInfo(TArray<FMaterialError>& OutErrors,UBOOL IncludeCost,FD3DMaterialShader* Shader)
{
	if(IncludeCost)
	{
		(new(OutErrors) FMaterialError)->Description = FString::Printf(TEXT("%s: %u instructions"),*Shader->GetTypeDescription(),Shader->NumPixelShaderInstructions);
	}
	AppendErrors(OutErrors,Shader->Errors);
}

TArray<FMaterialError> UD3DRenderDevice::CompileMaterial(FMaterial* Material)
{
	TArray<FMaterialError>	Errors;

	// Attempt to generate the user shader code.

	FD3DMaterialCompiler	Compiler;
	FString					UserShaderCode = GenerateMaterialCode(Material,Compiler,Errors);

	if(!Errors.Num())
	{
		// Attempt to compile all possible shaders using the material.

		AppendErrors(Errors,EmissiveTileShaders.GetCachedShader(Material)->Errors);

		FResourceType*	VertexFactoryTypes[] =
		{
			&FLocalVertexFactory::StaticType
		};

		for(UINT TypeIndex = 0;TypeIndex < ARRAY_COUNT(VertexFactoryTypes);TypeIndex++)
		{
			for(INT Translucent = 0;Translucent < ((Material->BlendMode == BLEND_Translucent || Material->BlendMode == BLEND_Additive) ? 2 : 1);Translucent++)
			{
				DWORD	OpaqueFlag = Translucent ? 0 : SF_OpaqueLayer,
						FunctionFlags = UseFPBlending ? SF_UseFPBlending : 0;

				AppendShaderInfo(Errors,!Translucent,MeshShaders.GetCachedShader((ED3DShaderFunction)(SF_Emissive | FunctionFlags | OpaqueFlag),Material,VertexFactoryTypes[TypeIndex]));
				AppendShaderInfo(Errors,0,MeshShaders.GetCachedShader((ED3DShaderFunction)(SF_VertexLighting | FunctionFlags | OpaqueFlag),Material,VertexFactoryTypes[TypeIndex]));
				AppendShaderInfo(Errors,0,MeshShaders.GetCachedShader((ED3DShaderFunction)(SF_ShadowDepth | FunctionFlags),Material,VertexFactoryTypes[TypeIndex]));
				//AppendShaderInfo(Errors,MeshShaders.GetCachedShader(SF_LightFunction,Material,VertexFactoryTypes[TypeIndex]));

				ED3DStaticLightingType	StaticLightingTypes[]=
				{
					SLT_Dynamic,
					SLT_VertexMask,
					SLT_TextureMask
				};
				for(UINT StaticLightingTypeIndex = 0;StaticLightingTypeIndex < ARRAY_COUNT(StaticLightingTypes);StaticLightingTypeIndex++)
				{
					AppendShaderInfo(Errors,!StaticLightingTypeIndex && !Translucent,MeshLightShaders.GetCachedShader((ED3DShaderFunction)(SF_PointLight | FunctionFlags),Material,VertexFactoryTypes[TypeIndex],StaticLightingTypes[StaticLightingTypeIndex]));
					AppendShaderInfo(Errors,0,MeshLightShaders.GetCachedShader((ED3DShaderFunction)(SF_DirectionalLight | FunctionFlags),Material,VertexFactoryTypes[TypeIndex],StaticLightingTypes[StaticLightingTypeIndex]));
					AppendShaderInfo(Errors,0,MeshLightShaders.GetCachedShader((ED3DShaderFunction)(SF_SpotLight | FunctionFlags),Material,VertexFactoryTypes[TypeIndex],StaticLightingTypes[StaticLightingTypeIndex]));
				}
			}
		}
	}

	return Errors;
}
