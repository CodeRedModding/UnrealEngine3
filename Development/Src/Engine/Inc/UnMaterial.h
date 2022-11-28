/*=============================================================================
	UnMaterial.h: Material definitions.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

#define ME_CAPTION_HEIGHT		18
#define ME_STD_VPADDING			16
#define ME_STD_HPADDING			32
#define ME_STD_BORDER			8
#define ME_STD_THUMBNAIL_SZ		96
#define ME_PREV_THUMBNAIL_SZ	256
#define ME_STD_LABEL_PAD		16
#define ME_STD_TAB_HEIGHT		21

//
//	FExpressionInput
//

//@warning: FExpressionInput is mirrored in MaterialExpression.uc and manually "subclassed" in Material.uc (FMaterialInput)
struct FExpressionInput
{
	class UMaterialExpression*	Expression;
	UBOOL						Mask,
								MaskR,
								MaskG,
								MaskB,
								MaskA;

	INT Compile(FMaterialCompiler* Compiler);
};

//
//	FMaterialInput
//

template<class InputType> struct FMaterialInput: FExpressionInput
{
	BITFIELD	UseConstant:1	GCC_PACK(4);
	InputType	Constant;
};

struct FColorMaterialInput: FMaterialInput<FColor>
{
	INT Compile(FMaterialCompiler* Compiler,const FColor& Default);
};
struct FScalarMaterialInput: FMaterialInput<FLOAT>
{
	INT Compile(FMaterialCompiler* Compiler,FLOAT Default);
};

struct FVectorMaterialInput: FMaterialInput<FVector>
{
	INT Compile(FMaterialCompiler* Compiler,const FVector& Default);
};

struct FVector2MaterialInput: FMaterialInput<FVector2D>
{
	INT Compile(FMaterialCompiler* Compiler,const FVector2D& Default);
};

//
//	FExpressionOutput
//

struct FExpressionOutput
{
	FString	Name;
	UBOOL	Mask,
			MaskR,
			MaskG,
			MaskB,
			MaskA;

	FExpressionOutput(UBOOL InMask,UBOOL InMaskR = 0,UBOOL InMaskG = 0,UBOOL InMaskB = 0,UBOOL InMaskA = 0):
		Mask(InMask),
		MaskR(InMaskR),
		MaskG(InMaskG),
		MaskB(InMaskB),
		MaskA(InMaskA)
	{}
};

//
//	FMaterialPointer
//

typedef FMaterial*	FMaterialPointer;

/** Used to be able mirror TArray<FBaseTexture*> in script */
typedef FTextureBase* FTextureBasePointer;

//
//	FMaterialInstancePointer
//

typedef FMaterialInstance*	FMaterialInstancePointer;