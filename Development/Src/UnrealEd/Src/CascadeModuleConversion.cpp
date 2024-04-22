/*=============================================================================
	CascadeModuleConversion.cpp: 'Cascade' module conversion code
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "Cascade.h"

//
// LOD Creation/Conversion Helpers
//

/**
 *	The type of distribution, used for both Float and Vector
 */
enum EDistributionType
{
	DISTRIBUTIONTYPE_Constant, 
	DISTRIBUTIONTYPE_ConstantCurve,
	DISTRIBUTIONTYPE_Uniform,
	DISTRIBUTIONTYPE_UniformCurve,
	DISTRIBUTIONTYPE_ParticleParam
};

/**
 *	Helper structure for determining FloatDistribution
 */
struct FloatDistributionDetermination
{
	EDistributionType	Type;
	union 
	{
		UDistributionFloatConstant*				FloatConstant;
		UDistributionFloatConstantCurve*		FloatConstantCurve;
		UDistributionFloatUniform*				FloatUniform;
		UDistributionFloatUniformCurve*			FloatUniformCurve;
		UDistributionFloatParticleParameter*	FloatParam;
	};
};

/**
 *	Helper structure for determining VectorDistribution
 */
struct VectorDistributionDetermination
{
	EDistributionType	Type;
	union 
	{
		UDistributionVectorConstant*			VectorConstant;
		UDistributionVectorConstantCurve*		VectorConstantCurve;
		UDistributionVectorUniform*				VectorUniform;
		UDistributionVectorUniformCurve*		VectorUniformCurve;
		UDistributionVectorParticleParameter*	VectorParam;
	};
};

/**
 *	The type of variable for the property
 */
enum EVariableType
{
	VARIABLETYPE_Unknown,
	VARIABLETYPE_Boolean,
	VARIABLETYPE_Float,
	VARIABLETYPE_FColor,
	VARIABLETYPE_FVector,
	VARIABLETYPE_FloatDist,
	VARIABLETYPE_VectorDist
};

/**
 *	Helper structure for determining type of variable
 */
struct VariableDetermination
{
	UParticleModule*	OwnerModule;
	UProperty*			Prop;
	EVariableType		Type;
	union
	{
		UBOOL								BooleanVariable;
		FLOAT								FloatVariable;
		FloatDistributionDetermination		FloatDistVariable;
		VectorDistributionDetermination		VectorDistVariable;
	};
	FColor				FColorVariable;
	FVector				FVectorVariable;
};

/**
 *	Helper macro for printing out enumerations in a switch block
 */
#define CASCADE_ENUM_STRING_CASE(x)		\
	case(x):	return TEXT(#x);

/**
 *	Helper function for printing out variable types
 */
const TCHAR* GetVariableTypeString(EVariableType eType)
{
	switch (eType)
	{
	CASCADE_ENUM_STRING_CASE(VARIABLETYPE_Unknown)
	CASCADE_ENUM_STRING_CASE(VARIABLETYPE_Boolean)
	CASCADE_ENUM_STRING_CASE(VARIABLETYPE_Float)
	CASCADE_ENUM_STRING_CASE(VARIABLETYPE_FColor)
	CASCADE_ENUM_STRING_CASE(VARIABLETYPE_FVector)
	CASCADE_ENUM_STRING_CASE(VARIABLETYPE_FloatDist)
	CASCADE_ENUM_STRING_CASE(VARIABLETYPE_VectorDist)
	}
	return TEXT("UNKNOWN ENUM");
}

/**
 *	Helper macro for printing out distribution types
 */
const TCHAR* GetDistributionTypeString(EDistributionType eType)
{
	switch (eType)
	{
	CASCADE_ENUM_STRING_CASE(DISTRIBUTIONTYPE_Constant)
	CASCADE_ENUM_STRING_CASE(DISTRIBUTIONTYPE_ConstantCurve)
	CASCADE_ENUM_STRING_CASE(DISTRIBUTIONTYPE_Uniform)
	CASCADE_ENUM_STRING_CASE(DISTRIBUTIONTYPE_UniformCurve)
	CASCADE_ENUM_STRING_CASE(DISTRIBUTIONTYPE_ParticleParam)
	}
	return TEXT("UNKNOWN ENUM");
}

/**
 *	Determine the type of float distribution
 *	INTERNAL FUNCTION
 */
UBOOL DetermineFloatDistributionType(UDistributionFloat* FloatDist, FloatDistributionDetermination& DistType)
{
	UBOOL bResult = FALSE;

	if (FloatDist)
	{
		if (FloatDist->IsA(UDistributionFloatConstant::StaticClass()))
		{
			DistType.Type			= DISTRIBUTIONTYPE_Constant;
			DistType.FloatConstant	= Cast<UDistributionFloatConstant>(FloatDist);
			bResult					= TRUE;
		}
		else
		if (FloatDist->IsA(UDistributionFloatConstantCurve::StaticClass()))
		{
			DistType.Type				= DISTRIBUTIONTYPE_ConstantCurve;
			DistType.FloatConstantCurve = Cast<UDistributionFloatConstantCurve>(FloatDist);
			bResult						= TRUE;
		}
		else
		if (FloatDist->IsA(UDistributionFloatUniform::StaticClass()))
		{
			DistType.Type			= DISTRIBUTIONTYPE_Uniform;
			DistType.FloatUniform	= Cast<UDistributionFloatUniform>(FloatDist);
			bResult					= TRUE;
		}
		else
		if (FloatDist->IsA(UDistributionFloatUniformCurve::StaticClass()))
		{
			DistType.Type				= DISTRIBUTIONTYPE_UniformCurve;
			DistType.FloatUniformCurve	= Cast<UDistributionFloatUniformCurve>(FloatDist);
			bResult						= TRUE;
		}
		else
		if (FloatDist->IsA(UDistributionFloatParticleParameter::StaticClass()))
		{
			DistType.Type		= DISTRIBUTIONTYPE_ParticleParam;
			DistType.FloatParam	= Cast<UDistributionFloatParticleParameter>(FloatDist);
			bResult				= TRUE;
		}
	}

	return bResult;
}

/**
 *	Determine the type of vector distribution
 *	INTERNAL FUNCTION
 */
UBOOL DetermineVectorDistributionType(UDistributionVector* VectorDist, VectorDistributionDetermination& DistType)
{
	UBOOL bResult = FALSE;

	if (VectorDist)
	{
		if (VectorDist->IsA(UDistributionVectorConstant::StaticClass()))
		{
			DistType.Type			= DISTRIBUTIONTYPE_Constant;
			DistType.VectorConstant	= Cast<UDistributionVectorConstant>(VectorDist);
			bResult					= TRUE;
		}
		else
		if (VectorDist->IsA(UDistributionVectorConstantCurve::StaticClass()))
		{
			DistType.Type					= DISTRIBUTIONTYPE_ConstantCurve;
			DistType.VectorConstantCurve	= Cast<UDistributionVectorConstantCurve>(VectorDist);
			bResult							= TRUE;
		}
		else
		if (VectorDist->IsA(UDistributionVectorUniform::StaticClass()))
		{
			DistType.Type			= DISTRIBUTIONTYPE_Uniform;
			DistType.VectorUniform	= Cast<UDistributionVectorUniform>(VectorDist);
			bResult					= TRUE;
		}
		else
		if (VectorDist->IsA(UDistributionVectorUniformCurve::StaticClass()))
		{
			DistType.Type				= DISTRIBUTIONTYPE_UniformCurve;
			DistType.VectorUniformCurve	= Cast<UDistributionVectorUniformCurve>(VectorDist);
			bResult						= TRUE;
		}
		else
		if (VectorDist->IsA(UDistributionVectorParticleParameter::StaticClass()))
		{
			DistType.Type			= DISTRIBUTIONTYPE_ParticleParam;
			DistType.VectorParam	= Cast<UDistributionVectorParticleParameter>(VectorDist);
			bResult					= TRUE;
		}
	}

	return bResult;
}

/**
 *	Fill in the variable determination array for the given module
 *	INTERNAL FUNCTION
 */
UBOOL FillModuleVariableArray(UParticleModule* Module, TArray<VariableDetermination>& Variables)
{
	// First, count the number of properties
	INT	PropertyCount	= 0;
	for (TFieldIterator<UProperty> It(Module->GetClass()); It; ++It)
	{
		PropertyCount++;
	}

	// Clear the array
	Variables.InsertZeroed(0, PropertyCount);

	// Fill it...
	INT	PropertyIndex	= 0;
	for (TFieldIterator<UProperty> It(Module->GetClass()); It; ++It)
	{
		UProperty*			Prop		= *It;
		UObjectProperty*	ObjectProp	= Cast<UObjectProperty>(Prop);
		UStructProperty*	StructProp	= Cast<UStructProperty>(Prop);
		UBoolProperty*		BoolProp	= Cast<UBoolProperty>(Prop);
		UFloatProperty*		FloatProp	= Cast<UFloatProperty>(Prop);

		VariableDetermination*	Variable	= &Variables(PropertyIndex++);

		Variable->OwnerModule	= Module;
		Variable->Prop			= Prop;

		if (ObjectProp)
		{
			if (ObjectProp->PropertyClass == UDistributionFloat::StaticClass())
			{
#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
				debugf(TEXT("\tProperty %32s is a DistributionFloat!"), *ObjectProp->GetName());
#endif	//#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
				Variable->Type	= VARIABLETYPE_FloatDist;

				UDistributionFloat* FloatDist = *((UDistributionFloat**)(((BYTE*)Module) + ObjectProp->Offset));	
				if (DetermineFloatDistributionType(FloatDist, Variable->FloatDistVariable) == FALSE)
				{
					// Not good...
					warnf(TEXT("Failed to determine float dist type for %32s"), *ObjectProp->GetName());
				}
			}
			else
			if (ObjectProp->PropertyClass == UDistributionVector::StaticClass())
			{
#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
				debugf(TEXT("\tProperty %32s is a DistributionVector!"), *ObjectProp->GetName());
#endif	//#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
				Variable->Type	= VARIABLETYPE_VectorDist;

				UDistributionVector* VectorDist = *((UDistributionVector**)(((BYTE*)Module) + ObjectProp->Offset));	
				if (DetermineVectorDistributionType(VectorDist, Variable->VectorDistVariable) == FALSE)
				{
					// Not good...
					warnf(TEXT("Failed to determine vector dist type for %32s"), *ObjectProp->GetName());
				}
			}
			else
			{
#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
				debugf(TEXT("\tProperty %32s is UNKNOWN!"), *ObjectProp->GetName());
#endif	//#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
				Variable->Type	= VARIABLETYPE_Unknown;
			}
		}
		else
		if (StructProp)
		{
			if (appStrcmp(*StructProp->GetCPPType(), TEXT("FColor")) == 0)
			{
				Variable->Type	= VARIABLETYPE_FColor;

				FColor*	TestColorPtr = (FColor*)((BYTE*)Module + StructProp->Offset);

				Variable->FColorVariable	= *TestColorPtr;

#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
				debugf(TEXT("\tProperty %32s is a FColor (%s)!"), 
					*StructProp->GetName(), *TestColorPtr->ToString());
#endif	//#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
			}
			else
			if (appStrcmp(*StructProp->GetCPPType(), TEXT("FVector")) == 0)
			{
				Variable->Type	= VARIABLETYPE_FVector;

				FVector*	TestVectorPtr = (FVector*)((BYTE*)Module + StructProp->Offset);

				Variable->FVectorVariable	= *TestVectorPtr;

#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
				debugf(TEXT("\tProperty %32s is a FVector (%s)!"), 
					*StructProp->GetName(), *TestVectorPtr->ToString());
#endif	//#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
			}
			else
			{
#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
				debugf(TEXT("\tProperty %32s is a Structure (%s)!"), *StructProp->GetName(), *StructProp->GetCPPType());
#endif	//#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
				Variable->Type	= VARIABLETYPE_Unknown;
			}
		}
		else
		if (BoolProp)
		{
			Variable->Type				= VARIABLETYPE_Boolean;

			BITFIELD*	Data	= (BITFIELD*)((BYTE*)Module + BoolProp->Offset);
			
			if (*Data & BoolProp->BitMask)
			{
				Variable->BooleanVariable	= TRUE;
			}
			else
			{
				Variable->BooleanVariable	= FALSE;
			}

#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
			debugf(TEXT("\tProperty %32s is a UBOOL (%s)!"), 
				*BoolProp->GetName(), Variable->BooleanVariable == TRUE ? TEXT("TRUE") : TEXT("FALSE"));
#endif	//#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
		}
		else
		if (FloatProp)
		{
			Variable->Type	= VARIABLETYPE_Float;

			FLOAT*	TestFloatPtr = (FLOAT*)((BYTE*)Module + FloatProp->Offset);

			Variable->FloatVariable	= *TestFloatPtr;

#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
			debugf(TEXT("\tProperty %32s is a FLOAT (%8.5f)!"), 
				*FloatProp->GetName(), *TestFloatPtr);
#endif	//#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
		}
	}

	return TRUE;
}

/**
 *	Generate the target boolean variable
 *	INTERNAL FUNCTION
 */
UBOOL GenerateTargetVariableBoolean(VariableDetermination* TargetVariable, 
	VariableDetermination* HighVariable, VariableDetermination* LowVariable,
	FLOAT Percentage)
{
	if (HighVariable->BooleanVariable != LowVariable->BooleanVariable)
	{
#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
		warnf(TEXT("%32s --> Boolean Inequality - High %s, Low %s!"), 
			*TargetVariable->Prop->GetName(),
			HighVariable->BooleanVariable == TRUE ? TEXT("TRUE") : TEXT("FALSE"),
			LowVariable->BooleanVariable == TRUE ? TEXT("TRUE") : TEXT("FALSE"));
#endif	//#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
		if (Percentage >= 0.5f)
		{
			TargetVariable->BooleanVariable	= HighVariable->BooleanVariable;
		}
		else
		{
			TargetVariable->BooleanVariable	= LowVariable->BooleanVariable;
		}
	}
	else
	{
		TargetVariable->BooleanVariable	= HighVariable->BooleanVariable;
	}

#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
	debugf(TEXT("%32s --> Generated %6s from High %6s, Low %6s!"), 
		*TargetVariable->Prop->GetName(),
		TargetVariable->BooleanVariable == TRUE ? TEXT("TRUE") : TEXT("FALSE"),
		HighVariable->BooleanVariable == TRUE ? TEXT("TRUE") : TEXT("FALSE"),
		LowVariable->BooleanVariable == TRUE ? TEXT("TRUE") : TEXT("FALSE"));
#endif	//#if defined(_CASCADE_LOD_GENERATION_DEBUG_)

	return TRUE;
}

/**
 *	Generate the target float variable
 *	INTERNAL FUNCTION
 */
UBOOL GenerateTargetVariableFloat(VariableDetermination* TargetVariable, 
	VariableDetermination* HighVariable, VariableDetermination* LowVariable,
	FLOAT Percentage)
{
	TargetVariable->FloatVariable = 
		(HighVariable->FloatVariable * Percentage) + 
		(LowVariable->FloatVariable * (1.0f - Percentage));

#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
	debugf(TEXT("%32s --> Generated %8.5f from High %8.5f, Low %8.5f!"), 
		*TargetVariable->Prop->GetName(),
		TargetVariable->FloatVariable, 
		HighVariable->FloatVariable,
		LowVariable->FloatVariable);
#endif	//#if defined(_CASCADE_LOD_GENERATION_DEBUG_)

	return TRUE;
}

/**
 *	Generate the target FColor variable
 *	INTERNAL FUNCTION
 */
UBOOL GenerateTargetVariableFColor(VariableDetermination* TargetVariable, 
	VariableDetermination* HighVariable, VariableDetermination* LowVariable,
	FLOAT Percentage)
{
	TargetVariable->FColorVariable.R	= (BYTE)(
		((FLOAT)HighVariable->FColorVariable.R * Percentage) + 
		((FLOAT)LowVariable->FColorVariable.R * (1.0f - Percentage))
		);
	TargetVariable->FColorVariable.G	= (BYTE)(
		((FLOAT)HighVariable->FColorVariable.G * Percentage) + 
		((FLOAT)LowVariable->FColorVariable.G * (1.0f - Percentage))
		);
	TargetVariable->FColorVariable.B	= (BYTE)(
		((FLOAT)HighVariable->FColorVariable.B * Percentage) + 
		((FLOAT)LowVariable->FColorVariable.B * (1.0f - Percentage))
		);
	TargetVariable->FColorVariable.A	= (BYTE)(
		((FLOAT)HighVariable->FColorVariable.A * Percentage) + 
		((FLOAT)LowVariable->FColorVariable.A * (1.0f - Percentage))
		);

#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
	debugf(TEXT("%32s --> Generated %s from High %s, Low %s!"), 
		*TargetVariable->Prop->GetName(),
		*TargetVariable->FColorVariable.ToString(), 
		*HighVariable->FColorVariable.ToString(), 
		*LowVariable->FColorVariable.ToString());
#endif	//#if defined(_CASCADE_LOD_GENERATION_DEBUG_)

	return TRUE;
}

/**
 *	Generate the target FVector variable
 *	INTERNAL FUNCTION
 */
UBOOL GenerateTargetVariableFVector(VariableDetermination* TargetVariable, 
	VariableDetermination* HighVariable, VariableDetermination* LowVariable,
	FLOAT Percentage)
{
	TargetVariable->FVectorVariable	= (HighVariable->FVectorVariable * Percentage) + 
		(LowVariable->FVectorVariable * (1.0f - Percentage));

#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
	debugf(TEXT("%32s --> Generated %s from High %s, Low %s!"), 
		*TargetVariable->Prop->GetName(),
		*TargetVariable->FVectorVariable.ToString(), 
		*HighVariable->FVectorVariable.ToString(), 
		*LowVariable->FVectorVariable.ToString());
#endif	//#if defined(_CASCADE_LOD_GENERATION_DEBUG_)

	return TRUE;
}

//*** FLOAT DISTRIBUTION CONVERSIONS
/**
 *	Generate a constant distribution from two constant distributions
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistConstantFromConstantConstant(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_Constant;

	TargetDD.FloatConstant	= ConstructObject<UDistributionFloatConstant>(UDistributionFloatConstant::StaticClass(), OwnerModule);
	check(TargetDD.FloatConstant);

	TargetDD.FloatConstant->Constant	= 
		(HighDD.FloatConstant->Constant * Percentage) + 
		(LowDD.FloatConstant->Constant * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a constant distribution from a constant dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistConstantFromConstantParticleParam(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_Constant;

	TargetDD.FloatConstant	= ConstructObject<UDistributionFloatConstant>(UDistributionFloatConstant::StaticClass(), OwnerModule);
	check(TargetDD.FloatConstant);

	TargetDD.FloatConstant->Constant	= 
		(HighDD.FloatConstant->Constant * Percentage) + 
		(LowDD.FloatParam->GetValue(0.0f) * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a constant curve dist. from a constant dist. and a constant curve dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistConstantCurveFromConstantConstantCurve(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ConstantCurve;

	TargetDD.FloatConstantCurve	= ConstructObject<UDistributionFloatConstantCurve>(UDistributionFloatConstantCurve::StaticClass(), OwnerModule);
	check(TargetDD.FloatConstantCurve);

	FLOAT	HiValue	= HighDD.FloatConstant->Constant;

	UDistributionFloatConstantCurve* LoCC	= LowDD.FloatConstantCurve;
	UDistributionFloatConstantCurve* NewCC	= TargetDD.FloatConstantCurve;

	for (INT KeyIndex = 0; KeyIndex < LoCC->GetNumKeys(); KeyIndex++)
	{
		INT	NewKeyIndex	= NewCC->CreateNewKey(LoCC->GetKeyIn(KeyIndex));
		for (INT SubIndex = 0; SubIndex < LoCC->GetNumSubCurves(); SubIndex++)
		{
			FLOAT	LoValue		= LoCC->GetKeyOut(SubIndex, KeyIndex);
			FLOAT	NewValue	= (HiValue * Percentage) + (LoValue * (1.0f - Percentage));
			NewCC->SetKeyOut(SubIndex, NewKeyIndex, NewValue);
		}
		NewCC->SetKeyInterpMode(NewKeyIndex, (EInterpCurveMode)(LoCC->GetKeyInterpMode(KeyIndex)));
	}

	return TRUE;
}

/**
 *	Generate a constant curve dist. from two constant curve dists.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistConstantCurveFromConstantCurveConstantCurve(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ConstantCurve;

	TargetDD.FloatConstantCurve	= ConstructObject<UDistributionFloatConstantCurve>(UDistributionFloatConstantCurve::StaticClass(), OwnerModule);
	check(TargetDD.FloatConstantCurve);

	UDistributionFloatConstantCurve* HiCC	= HighDD.FloatConstantCurve;
	UDistributionFloatConstantCurve* LoCC	= LowDD.FloatConstantCurve;
	UDistributionFloatConstantCurve* NewCC	= TargetDD.FloatConstantCurve;

	INT KeyIndex;

	UBOOL	bSampleBoth = (HiCC->GetNumKeys() < LoCC->GetNumKeys()) ? TRUE : FALSE;

	// First, set the points for the high curve
	for (KeyIndex = 0; KeyIndex < HiCC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= HiCC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewCC->CreateNewKey(KeyIn);

		FLOAT	LoValue		= LoCC->GetValue(KeyIn);
		FLOAT	HiValue		= HiCC->GetKeyOut(0, KeyIndex);

		FLOAT	NewValue	= (HiValue * Percentage) + (LoValue * (1.0f - Percentage));
		
		NewCC->SetKeyOut(0, NewKeyIndex, NewValue);
		NewCC->SetKeyInterpMode(NewKeyIndex, (EInterpCurveMode)(HiCC->GetKeyInterpMode(KeyIndex)));
	}

	// If more keys in the low, sample that as well...
	if (bSampleBoth)
	{
		for (KeyIndex = 0; KeyIndex < LoCC->GetNumKeys(); KeyIndex++)
		{
			FLOAT	KeyIn	= LoCC->GetKeyIn(KeyIndex);
			UBOOL	bSetIt	= TRUE;
			for (INT CheckIndex = 0; CheckIndex < NewCC->GetNumKeys(); CheckIndex++)
			{
				if (NewCC->GetKeyIn(CheckIndex) == KeyIn)
				{
					bSetIt = FALSE;
					break;
				}
			}

			if (bSetIt)
			{
				INT		NewKeyIndex	= NewCC->CreateNewKey(KeyIn);
				FLOAT	HiValue		= HiCC->GetValue(KeyIn);
				FLOAT	LoValue		= LoCC->GetKeyOut(0, KeyIndex);

				FLOAT	NewValue	= (HiValue * Percentage) + (LoValue * (1.0f - Percentage));

				NewCC->SetKeyOut(0, NewKeyIndex, NewValue);
				NewCC->SetKeyInterpMode(NewKeyIndex, (EInterpCurveMode)(LoCC->GetKeyInterpMode(KeyIndex)));
			}
		}
	}

	return TRUE;
}

/**
 *	Generate a constant curve dist. from a constant curve dist. and a uniform dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistConstantCurveFromConstantCurveUniform(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ConstantCurve;

	TargetDD.FloatConstantCurve	= ConstructObject<UDistributionFloatConstantCurve>(UDistributionFloatConstantCurve::StaticClass(), OwnerModule);
	check(TargetDD.FloatConstantCurve);

	UDistributionFloatConstantCurve* HiCC	= HighDD.FloatConstantCurve;
	UDistributionFloatUniform*		 LoU	= LowDD.FloatUniform;
	UDistributionFloatConstantCurve* NewCC	= TargetDD.FloatConstantCurve;

	INT KeyIndex;

	FLOAT	LoValue		= (LoU->Max + LoU->Min) / 2.0f;

	// First, set the points for the high curve
	for (KeyIndex = 0; KeyIndex < HiCC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= HiCC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewCC->CreateNewKey(KeyIn);

		FLOAT	HiValue		= HiCC->GetKeyOut(0, KeyIndex);
		FLOAT	NewValue	= (HiValue * Percentage) + (LoValue * (1.0f - Percentage));
		
		NewCC->SetKeyOut(0, NewKeyIndex, NewValue);
		NewCC->SetKeyInterpMode(NewKeyIndex, (EInterpCurveMode)(HiCC->GetKeyInterpMode(KeyIndex)));
	}

	return TRUE;
}

/**
 *	Generate a constant curve dist. from a constant curve dist. and a uniform curve dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistConstantCurveFromConstantCurveUniformCurve(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ConstantCurve;

	TargetDD.FloatConstantCurve	= ConstructObject<UDistributionFloatConstantCurve>(UDistributionFloatConstantCurve::StaticClass(), OwnerModule);
	check(TargetDD.FloatConstantCurve);

	UDistributionFloatConstantCurve*	HiCC	= HighDD.FloatConstantCurve;
	UDistributionFloatUniformCurve*		LoUC	= LowDD.FloatUniformCurve;
	UDistributionFloatConstantCurve*	NewCC	= TargetDD.FloatConstantCurve;

	INT KeyIndex;

	UBOOL	bSampleBoth = (HiCC->GetNumKeys() < LoUC->GetNumKeys()) ? TRUE : FALSE;

	// First, set the points for the high curve
	for (KeyIndex = 0; KeyIndex < HiCC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= HiCC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewCC->CreateNewKey(KeyIn);

		FVector2D	LoVector	= LoUC->GetMinMaxValue(KeyIn);
		FLOAT		LoValue		= (LoVector.X + LoVector.Y) / 2.0f;
		FLOAT		HiValue		= HiCC->GetKeyOut(0, KeyIndex);
		FLOAT		NewValue	= (HiValue * Percentage) + (LoValue * (1.0f - Percentage));
		
		NewCC->SetKeyOut(0, NewKeyIndex, NewValue);
		NewCC->SetKeyInterpMode(NewKeyIndex, (EInterpCurveMode)(HiCC->GetKeyInterpMode(KeyIndex)));
	}

	// If more keys in the low, sample that as well...
	if (bSampleBoth)
	{
		for (KeyIndex = 0; KeyIndex < LoUC->GetNumKeys(); KeyIndex++)
		{
			FLOAT	KeyIn	= LoUC->GetKeyIn(KeyIndex);
			UBOOL	bSetIt	= TRUE;
			for (INT CheckIndex = 0; CheckIndex < NewCC->GetNumKeys(); CheckIndex++)
			{
				if (NewCC->GetKeyIn(CheckIndex) == KeyIn)
				{
					bSetIt = FALSE;
					break;
				}
			}

			if (bSetIt)
			{
				INT			NewKeyIndex	= NewCC->CreateNewKey(KeyIn);
				FLOAT		HiValue		= HiCC->GetValue(KeyIn);
				FVector2D	LoVector	= LoUC->GetMinMaxValue(KeyIn);
				FLOAT		LoValue		= (LoVector.X + LoVector.Y) / 2.0f;
				FLOAT		NewValue	= (HiValue * Percentage) + (LoValue * (1.0f - Percentage));
				NewCC->SetKeyOut(0, NewKeyIndex, NewValue);
				NewCC->SetKeyInterpMode(NewKeyIndex, (EInterpCurveMode)(LoUC->GetKeyInterpMode(KeyIndex)));
			}
		}
	}

	return TRUE;
}

/**
 *	Generate a constant curve dist. from a constant curve dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistConstantCurveFromConstantCurveParticleParam(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ConstantCurve;

	TargetDD.FloatConstantCurve	= ConstructObject<UDistributionFloatConstantCurve>(UDistributionFloatConstantCurve::StaticClass(), OwnerModule);
	check(TargetDD.FloatConstantCurve);

	UDistributionFloatConstantCurve*		HiCC	= HighDD.FloatConstantCurve;
	UDistributionFloatParticleParameter*	LoPP	= LowDD.FloatParam;
	UDistributionFloatConstantCurve*		NewCC	= TargetDD.FloatConstantCurve;

	INT KeyIndex;


	// First, set the points for the high curve
	for (KeyIndex = 0; KeyIndex < HiCC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= HiCC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewCC->CreateNewKey(KeyIn);

		FLOAT	HiValue		= HiCC->GetKeyOut(0, KeyIndex);
		FLOAT	LoValue		= LoPP->GetValue(KeyIn);
		FLOAT	NewValue	= (HiValue * Percentage) + (LoValue * (1.0f - Percentage));
		
		NewCC->SetKeyOut(0, NewKeyIndex, NewValue);
		NewCC->SetKeyInterpMode(NewKeyIndex, (EInterpCurveMode)(HiCC->GetKeyInterpMode(KeyIndex)));
	}

	return TRUE;
}

/**
 *	Generate a uniform dist. from a constant dist. and a uniform dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistUniformFromConstantUniform(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_Uniform;

	TargetDD.FloatUniform	= ConstructObject<UDistributionFloatUniform>(UDistributionFloatUniform::StaticClass(), OwnerModule);
	check(TargetDD.FloatUniform);

	UDistributionFloatConstant*		HiC		= HighDD.FloatConstant;
	UDistributionFloatUniform*		LoU		= LowDD.FloatUniform;
	UDistributionFloatUniform*		NewU	= TargetDD.FloatUniform;

	NewU->Min	= (HiC->Constant * Percentage) + (LoU->Min * (1.0f - Percentage));
	NewU->Max	 =(HiC->Constant * Percentage) + (LoU->Max * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a uniform dist. from two uniform dists.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistUniformFromUniformUniform(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_Uniform;

	TargetDD.FloatUniform	= ConstructObject<UDistributionFloatUniform>(UDistributionFloatUniform::StaticClass(), OwnerModule);
	check(TargetDD.FloatUniform);

	UDistributionFloatUniform*	HiU		= HighDD.FloatUniform;
	UDistributionFloatUniform*	LoU		= LowDD.FloatUniform;
	UDistributionFloatUniform*	NewU	= TargetDD.FloatUniform;

	NewU->Min	= (HiU->Min * Percentage) + (LoU->Min * (1.0f - Percentage));
	NewU->Max	 =(HiU->Max * Percentage) + (LoU->Max * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a uniform dist. from a uniform dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistUniformFromUniformParticleParam(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_Uniform;

	TargetDD.FloatUniform	= ConstructObject<UDistributionFloatUniform>(UDistributionFloatUniform::StaticClass(), OwnerModule);
	check(TargetDD.FloatUniform);

	UDistributionFloatUniform*				HiU		= HighDD.FloatUniform;
	UDistributionFloatParticleParameter*	LoPP	= LowDD.FloatParam;
	UDistributionFloatUniform*				NewU	= TargetDD.FloatUniform;

	FLOAT	LoValue		= LoPP->GetValue(0.0f);

	NewU->Min	= (HiU->Min * Percentage) + (LoValue * (1.0f - Percentage));
	NewU->Max	 =(HiU->Max * Percentage) + (LoValue * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a uniform curve dist. from a constant dist. and a uniform curve dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistUniformCurveFromConstantUniformCurve(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_UniformCurve;

	TargetDD.FloatUniformCurve	= ConstructObject<UDistributionFloatUniformCurve>(UDistributionFloatUniformCurve::StaticClass(), OwnerModule);
	check(TargetDD.FloatUniformCurve);

	UDistributionFloatConstant*		HiC		= HighDD.FloatConstant;
	UDistributionFloatUniformCurve*	LoUC	= LowDD.FloatUniformCurve;
	UDistributionFloatUniformCurve*	NewUC	= TargetDD.FloatUniformCurve;

	INT KeyIndex;

	FLOAT	HiValue	= HiC->Constant;

	// First, set the points for the high curve
	for (KeyIndex = 0; KeyIndex < LoUC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= LoUC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewUC->CreateNewKey(KeyIn);

		FVector2D	LoVector	= LoUC->GetMinMaxValue(KeyIn);

		FLOAT	NewValueMax	= (HiValue * Percentage) + (LoVector.X * (1.0f - Percentage));
		FLOAT	NewValueMin	= (HiValue * Percentage) + (LoVector.Y * (1.0f - Percentage));
		
		NewUC->SetKeyOut(0, NewKeyIndex, NewValueMax);
		NewUC->SetKeyOut(1, NewKeyIndex, NewValueMin);
		NewUC->SetKeyInterpMode(NewKeyIndex, (EInterpCurveMode)(LoUC->GetKeyInterpMode(KeyIndex)));
	}

	return TRUE;
}

/**
 *	Generate a uniform curve dist. from a constant curve dist. and a uniform dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistUniformCurveFromConstantCurveUniform(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_UniformCurve;

	TargetDD.FloatUniformCurve	= ConstructObject<UDistributionFloatUniformCurve>(UDistributionFloatUniformCurve::StaticClass(), OwnerModule);
	check(TargetDD.FloatUniformCurve);

	UDistributionFloatConstantCurve*	HiCC	= HighDD.FloatConstantCurve;
	UDistributionFloatUniform*			LoU		= LowDD.FloatUniform;
	UDistributionFloatUniformCurve*		NewUC	= TargetDD.FloatUniformCurve;

	INT KeyIndex;

	FLOAT	LoValueMin	= LoU->Min;
	FLOAT	LoValueMax	= LoU->Max;

	for (KeyIndex = 0; KeyIndex < HiCC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= HiCC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewUC->CreateNewKey(KeyIn);
		FLOAT	HiValue		= HiCC->GetKeyOut(0, KeyIndex);

		FLOAT	MinValue	= (HiValue * Percentage) + (LoValueMin * (1.0f - Percentage));
		FLOAT	MaxValue	= (HiValue * Percentage) + (LoValueMax * (1.0f - Percentage));
		
		NewUC->SetKeyOut(0, NewKeyIndex, MaxValue);
		NewUC->SetKeyOut(1, NewKeyIndex, MinValue);
		NewUC->SetKeyInterpMode(NewKeyIndex, (EInterpCurveMode)(HiCC->GetKeyInterpMode(KeyIndex)));
	}

	return TRUE;
}

/**
 *	Generate a uniform curve dist. from a constant curve dist. and a uniform curve dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistUniformCurveFromConstantCurveUniformCurve(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_UniformCurve;

	TargetDD.FloatUniformCurve	= ConstructObject<UDistributionFloatUniformCurve>(UDistributionFloatUniformCurve::StaticClass(), OwnerModule);
	check(TargetDD.FloatUniformCurve);

	UDistributionFloatConstantCurve*	HiCC	= HighDD.FloatConstantCurve;
	UDistributionFloatUniformCurve*		LoUC	= LowDD.FloatUniformCurve;
	UDistributionFloatUniformCurve*		NewUC	= TargetDD.FloatUniformCurve;

	INT KeyIndex;

	UBOOL	bSampleBoth = (HiCC->GetNumKeys() < LoUC->GetNumKeys()) ? TRUE : FALSE;

	// First, set the points for the high curve
	for (KeyIndex = 0; KeyIndex < HiCC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= HiCC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewUC->CreateNewKey(KeyIn);

		FVector2D	LoVector	= LoUC->GetMinMaxValue(KeyIn);
		FLOAT		HiValue		= HiCC->GetKeyOut(0, KeyIndex);

		FLOAT		NewValueMax	= (HiValue * Percentage) + (LoVector.X * (1.0f - Percentage));
		FLOAT		NewValueMin	= (HiValue * Percentage) + (LoVector.Y * (1.0f - Percentage));
		
		NewUC->SetKeyOut(0, NewKeyIndex, NewValueMax);
		NewUC->SetKeyOut(1, NewKeyIndex, NewValueMin);
		NewUC->SetKeyInterpMode(NewKeyIndex, (EInterpCurveMode)(HiCC->GetKeyInterpMode(KeyIndex)));
	}

	// If more keys in the low, sample that as well...
	if (bSampleBoth)
	{
		for (KeyIndex = 0; KeyIndex < LoUC->GetNumKeys(); KeyIndex++)
		{
			FLOAT	KeyIn	= LoUC->GetKeyIn(KeyIndex);
			UBOOL	bSetIt	= TRUE;
			for (INT CheckIndex = 0; CheckIndex < NewUC->GetNumKeys(); CheckIndex++)
			{
				if (NewUC->GetKeyIn(CheckIndex) == KeyIn)
				{
					bSetIt = FALSE;
					break;
				}
			}

			if (bSetIt)
			{
				INT			NewKeyIndex	= NewUC->CreateNewKey(KeyIn);
				FLOAT		HiValue		= HiCC->GetValue(KeyIn);
				FVector2D	LoVector	= LoUC->GetMinMaxValue(KeyIn);

				FLOAT		NewValueMax	= (HiValue * Percentage) + (LoVector.X * (1.0f - Percentage));
				FLOAT		NewValueMin	= (HiValue * Percentage) + (LoVector.Y * (1.0f - Percentage));

				NewUC->SetKeyOut(0, NewKeyIndex, NewValueMax);
				NewUC->SetKeyOut(1, NewKeyIndex, NewValueMin);
				NewUC->SetKeyInterpMode(NewKeyIndex, (EInterpCurveMode)(LoUC->GetKeyInterpMode(KeyIndex)));
			}
		}
	}

	return TRUE;
}

/**
 *	Generate a uniform curve dist. from a uniform dist. and a uniform curve dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistUniformCurveFromUniformUniformCurve(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_UniformCurve;

	TargetDD.FloatUniformCurve	= ConstructObject<UDistributionFloatUniformCurve>(UDistributionFloatUniformCurve::StaticClass(), OwnerModule);
	check(TargetDD.FloatUniformCurve);

	UDistributionFloatUniform*		HiU		= HighDD.FloatUniform;
	UDistributionFloatUniformCurve*	LoUC	= LowDD.FloatUniformCurve;
	UDistributionFloatUniformCurve*	NewUC	= TargetDD.FloatUniformCurve;

	INT KeyIndex;
	
	FLOAT	HiValueMax	= HiU->Max;
	FLOAT	HiValueMin	= HiU->Min;

	for (KeyIndex = 0; KeyIndex < LoUC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= LoUC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewUC->CreateNewKey(KeyIn);

		FVector2D	LoVector	= LoUC->GetMinMaxValue(KeyIn);

		FLOAT		NewValueMax	= (HiValueMax * Percentage) + (LoVector.X * (1.0f - Percentage));
		FLOAT		NewValueMin	= (HiValueMin * Percentage) + (LoVector.Y * (1.0f - Percentage));
		
		NewUC->SetKeyOut(0, NewKeyIndex, NewValueMax);
		NewUC->SetKeyOut(1, NewKeyIndex, NewValueMin);
		NewUC->SetKeyInterpMode(NewKeyIndex, (EInterpCurveMode)(LoUC->GetKeyInterpMode(KeyIndex)));
	}

	return TRUE;
}

/**
 *	Generate a uniform curve dist. from a uniform curve dist. and a uniform curve dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistUniformCurveFromUniformCurveUniformCurve(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_UniformCurve;

	TargetDD.FloatUniformCurve	= ConstructObject<UDistributionFloatUniformCurve>(UDistributionFloatUniformCurve::StaticClass(), OwnerModule);
	check(TargetDD.FloatUniformCurve);

	UDistributionFloatUniformCurve*	HiUC	= HighDD.FloatUniformCurve;
	UDistributionFloatUniformCurve*	LoUC	= LowDD.FloatUniformCurve;
	UDistributionFloatUniformCurve*	NewUC	= TargetDD.FloatUniformCurve;

	INT KeyIndex;

	UBOOL	bSampleBoth = (HiUC->GetNumKeys() < LoUC->GetNumKeys()) ? TRUE : FALSE;

	// First, set the points for the high curve
	for (KeyIndex = 0; KeyIndex < HiUC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= HiUC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewUC->CreateNewKey(KeyIn);

		FVector2D	LoVector	= LoUC->GetMinMaxValue(KeyIn);
		FVector2D	HiVector	= HiUC->GetMinMaxValue(KeyIn);

		FLOAT		NewValueMax	= (HiVector.X * Percentage) + (LoVector.X * (1.0f - Percentage));
		FLOAT		NewValueMin	= (HiVector.Y * Percentage) + (LoVector.Y * (1.0f - Percentage));
		
		NewUC->SetKeyOut(0, NewKeyIndex, NewValueMax);
		NewUC->SetKeyOut(1, NewKeyIndex, NewValueMin);
		NewUC->SetKeyInterpMode(NewKeyIndex, (EInterpCurveMode)(HiUC->GetKeyInterpMode(KeyIndex)));
	}

	// If more keys in the low, sample that as well...
	if (bSampleBoth)
	{
		for (KeyIndex = 0; KeyIndex < LoUC->GetNumKeys(); KeyIndex++)
		{
			FLOAT	KeyIn	= LoUC->GetKeyIn(KeyIndex);
			UBOOL	bSetIt	= TRUE;
			for (INT CheckIndex = 0; CheckIndex < NewUC->GetNumKeys(); CheckIndex++)
			{
				if (NewUC->GetKeyIn(CheckIndex) == KeyIn)
				{
					bSetIt = FALSE;
					break;
				}
			}

			if (bSetIt)
			{
				INT			NewKeyIndex	= NewUC->CreateNewKey(KeyIn);
				FVector2D	LoVector	= LoUC->GetMinMaxValue(KeyIn);
				FVector2D	HiVector	= HiUC->GetMinMaxValue(KeyIn);

				FLOAT		NewValueMax	= (HiVector.X * Percentage) + (LoVector.X * (1.0f - Percentage));
				FLOAT		NewValueMin	= (HiVector.Y * Percentage) + (LoVector.Y * (1.0f - Percentage));

				NewUC->SetKeyOut(0, NewKeyIndex, NewValueMax);
				NewUC->SetKeyOut(1, NewKeyIndex, NewValueMin);
				NewUC->SetKeyInterpMode(NewKeyIndex, (EInterpCurveMode)(LoUC->GetKeyInterpMode(KeyIndex)));
			}
		}
	}

	return TRUE;
}

/**
 *	Generate a uniform curve dist. from a uniform curve dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistUniformCurveFromUniformCurveParticleParam(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_UniformCurve;

	TargetDD.FloatUniformCurve	= ConstructObject<UDistributionFloatUniformCurve>(UDistributionFloatUniformCurve::StaticClass(), OwnerModule);
	check(TargetDD.FloatUniformCurve);

	UDistributionFloatUniformCurve*			HiUC	= HighDD.FloatUniformCurve;
	UDistributionFloatParticleParameter*	LoPP	= LowDD.FloatParam;
	UDistributionFloatUniformCurve*			NewUC	= TargetDD.FloatUniformCurve;

	INT KeyIndex;
	

	for (KeyIndex = 0; KeyIndex < HiUC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= HiUC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewUC->CreateNewKey(KeyIn);

		FLOAT		LoValue		= LoPP->GetValue(KeyIn);
		FVector2D	HiVector	= HiUC->GetMinMaxValue(KeyIn);

		FLOAT		NewValueMax	= (HiVector.X * Percentage) + (LoValue * (1.0f - Percentage));
		FLOAT		NewValueMin	= (HiVector.Y * Percentage) + (LoValue * (1.0f - Percentage));
		
		NewUC->SetKeyOut(0, NewKeyIndex, NewValueMax);
		NewUC->SetKeyOut(1, NewKeyIndex, NewValueMin);
		NewUC->SetKeyInterpMode(NewKeyIndex, (EInterpCurveMode)(HiUC->GetKeyInterpMode(KeyIndex)));
	}

	return TRUE;
}

/**
 *	Generate a particle param dist. from a constant dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistParticleParamFromConstantParticleParam(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ParticleParam;

	TargetDD.FloatParam	= ConstructObject<UDistributionFloatParticleParameter>(UDistributionFloatParticleParameter::StaticClass(), OwnerModule);
	check(TargetDD.FloatParam);

	UDistributionFloatConstant*				HiC		= HighDD.FloatConstant;
	UDistributionFloatParticleParameter*	LoPP	= LowDD.FloatParam;
	UDistributionFloatParticleParameter*	NewPP	= TargetDD.FloatParam;

	NewPP->ParameterName	= LoPP->ParameterName;
	NewPP->MinInput			= LoPP->MinInput;
	NewPP->MaxInput			= LoPP->MaxInput;
	NewPP->MinOutput		= LoPP->MinOutput;
	NewPP->MaxOutput		= LoPP->MaxOutput;
	NewPP->ParamMode		= LoPP->ParamMode;

	NewPP->Constant	= (HiC->Constant * Percentage) + (LoPP->Constant * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a particle param dist. from a constant curve dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistParticleParamFromConstantCurveParticleParam(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ParticleParam;

	TargetDD.FloatParam	= ConstructObject<UDistributionFloatParticleParameter>(UDistributionFloatParticleParameter::StaticClass(), OwnerModule);
	check(TargetDD.FloatParam);

	UDistributionFloatConstantCurve*		HiCC	= HighDD.FloatConstantCurve;
	UDistributionFloatParticleParameter*	LoPP	= LowDD.FloatParam;
	UDistributionFloatParticleParameter*	NewPP	= TargetDD.FloatParam;

	NewPP->ParameterName	= LoPP->ParameterName;
	NewPP->MinInput			= LoPP->MinInput;
	NewPP->MaxInput			= LoPP->MaxInput;
	NewPP->MinOutput		= LoPP->MinOutput;
	NewPP->MaxOutput		= LoPP->MaxOutput;
	NewPP->ParamMode		= LoPP->ParamMode;

	FLOAT	MinOut, MaxOut;

	HiCC->GetOutRange(MinOut, MaxOut);

	NewPP->Constant	= (((MinOut + MaxOut) / 2.0f) * Percentage) + (LoPP->Constant * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a particle param dist. from a unfirom dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistParticleParamFromUniformParticleParam(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ParticleParam;

	TargetDD.FloatParam	= ConstructObject<UDistributionFloatParticleParameter>(UDistributionFloatParticleParameter::StaticClass(), OwnerModule);
	check(TargetDD.FloatParam);

	UDistributionFloatUniform*				HiU		= HighDD.FloatUniform;
	UDistributionFloatParticleParameter*	LoPP	= LowDD.FloatParam;
	UDistributionFloatParticleParameter*	NewPP	= TargetDD.FloatParam;

	NewPP->ParameterName	= LoPP->ParameterName;
	NewPP->MinInput			= LoPP->MinInput;
	NewPP->MaxInput			= LoPP->MaxInput;
	NewPP->MinOutput		= LoPP->MinOutput;
	NewPP->MaxOutput		= LoPP->MaxOutput;
	NewPP->ParamMode		= LoPP->ParamMode;

	NewPP->Constant	= (((HiU->Min + HiU->Max) / 2.0f) * Percentage) + (LoPP->Constant * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a particle param dist. from a uniform curve dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistParticleParamFromUniformCurveParticleParam(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ParticleParam;

	TargetDD.FloatParam	= ConstructObject<UDistributionFloatParticleParameter>(UDistributionFloatParticleParameter::StaticClass(), OwnerModule);
	check(TargetDD.FloatParam);

	UDistributionFloatUniformCurve*			HiUC	= HighDD.FloatUniformCurve;
	UDistributionFloatParticleParameter*	LoPP	= LowDD.FloatParam;
	UDistributionFloatParticleParameter*	NewPP	= TargetDD.FloatParam;

	NewPP->ParameterName	= LoPP->ParameterName;
	NewPP->MinInput			= LoPP->MinInput;
	NewPP->MaxInput			= LoPP->MaxInput;
	NewPP->MinOutput		= LoPP->MinOutput;
	NewPP->MaxOutput		= LoPP->MaxOutput;
	NewPP->ParamMode		= LoPP->ParamMode;

	FLOAT	MinOut, MaxOut;

	HiUC->GetOutRange(MinOut, MaxOut);

	NewPP->Constant	= (((MinOut + MaxOut) / 2.0f) * Percentage) + (LoPP->Constant * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a particle param dist. from a particle param dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateFloatDistParticleParamFromParticleParamParticleParam(
	UParticleModule* OwnerModule, 
	FloatDistributionDetermination& TargetDD, 
	FloatDistributionDetermination& HighDD,
	FloatDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ParticleParam;

	TargetDD.FloatParam	= ConstructObject<UDistributionFloatParticleParameter>(UDistributionFloatParticleParameter::StaticClass(), OwnerModule);
	check(TargetDD.FloatParam);

	UDistributionFloatParticleParameter*	HiPP	= HighDD.FloatParam;
	UDistributionFloatParticleParameter*	LoPP	= LowDD.FloatParam;
	UDistributionFloatParticleParameter*	NewPP	= TargetDD.FloatParam;

	if (Percentage <= 0.5f)
	{
		NewPP->ParameterName	= HiPP->ParameterName;
		NewPP->ParamMode		= HiPP->ParamMode;
	}
	else
	{
		NewPP->ParameterName	= LoPP->ParameterName;
		NewPP->ParamMode		= LoPP->ParamMode;
	}
	NewPP->MinInput		= (HiPP->MinInput * Percentage)		+ (LoPP->MinInput * (1.0f - Percentage));
	NewPP->MaxInput		= (HiPP->MaxInput * Percentage)		+ (LoPP->MaxInput * (1.0f - Percentage));
	NewPP->MinOutput	= (HiPP->MinOutput * Percentage)	+ (LoPP->MinOutput * (1.0f - Percentage));
	NewPP->MaxOutput	= (HiPP->MaxOutput * Percentage)	+ (LoPP->MaxOutput * (1.0f - Percentage));
	NewPP->Constant		= (HiPP->Constant * Percentage)		+ (LoPP->Constant * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate the target FloatDistribution variable
 *	INTERNAL FUNCTION
 */
UBOOL GenerateTargetVariableFloatDist(VariableDetermination* TargetVariable, 
	VariableDetermination* HighVariable, VariableDetermination* LowVariable,
	FLOAT Percentage)
{
	UBOOL				bResult	= FALSE;

	// Depending on the types, do the conversion
	// Use the following grid for conversion:
	//
	// -----+-----+-----------------------------+--------------------------------+
	// High | Low |  Percent < 0.5              | Percent >= 0.5                 |
	// -----+-----+-----------------------------+--------------------------------+
	//  C   | C   | C  (H*P+L*(1-P))                                             |
	//  C   | CC  | CC Replicate curve using H*P+L*(1-P) at each point           |
	//  C   | U   | U  Offset the uniform values by the constant                 |
	//  C   | UC  | UC Replicate curve offseting the values by the constant      |
	//  C   | PP  | PP                                                           |
	// -----+-----+-----------------------------+--------------------------------+
	//  CC  | C   | CC Replicate curve using H*P+L*(1-P) at each point           |
	//  CC  | CC  | CC Replicate curve using H*P+L*(1-P) at each point           |
	//  CC  | U   | UC                                                           |
	//  CC  | UC  | UC Replicate curve offseting the values by the constant      |
	//  CC  | PP  | CC                          | PP                             |
	// -----+-----+-----------------------------+--------------------------------+
	//  U   | C   | U  Using H*P+L*(1-P)                                         |
	//  U   | CC  | UC Replicate curve using H*P+L*(1-P) at each point           |
	//  U   | U   | U  Using H*P+L*(1-P)                                         |
	//  U   | UC  | UC Replicate curve offseting the values by the constant      |
	//  U   | PP  | U                           | PP                             |
	// -----+-----+-----------------------------+--------------------------------+
	//  UC  | C   | UC Using H*P+L*(1-P)                                         |
	//  UC  | CC  | UC Replicate curve using H*P+L*(1-P) at each point           |
	//  UC  | U   | UC Using H*P+L*(1-P)                                         |
	//  UC  | UC  | UC Using H*P+L*(1-P)                                         |
	//  UC  | PP  | UC                          | PP                             |
	// -----+-----+-----------------------------+--------------------------------+
	//  PP  | C   | PP                                                           |
	//  PP  | CC  | PP                                                           |
	//  PP  | U   | PP                                                           |
	//  PP  | UC  | PP                                                           |
	//  PP  | PP  | PP                                                           |
	// -----+-----+-----------------------------+--------------------------------+
	switch (HighVariable->FloatDistVariable.Type)
	{
	case DISTRIBUTIONTYPE_Constant:
		switch (LowVariable->FloatDistVariable.Type)
		{
		case DISTRIBUTIONTYPE_Constant:
			bResult = GenerateFloatDistConstantFromConstantConstant(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_ConstantCurve:
			bResult = GenerateFloatDistConstantCurveFromConstantConstantCurve(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_Uniform:
			bResult = GenerateFloatDistUniformFromConstantUniform(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_UniformCurve:
			bResult = GenerateFloatDistUniformCurveFromConstantUniformCurve(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_ParticleParam:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateFloatDistConstantFromConstantParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					Percentage);
			}
			else
			{
				bResult = GenerateFloatDistParticleParamFromConstantParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					Percentage);
			}
			break;
		}
		break;
	case DISTRIBUTIONTYPE_ConstantCurve:
		switch (LowVariable->FloatDistVariable.Type)
		{
		case DISTRIBUTIONTYPE_Constant:
			bResult = GenerateFloatDistConstantCurveFromConstantConstantCurve(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				1.0f - Percentage);
			break;
		case DISTRIBUTIONTYPE_ConstantCurve:
			bResult = GenerateFloatDistConstantCurveFromConstantCurveConstantCurve(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_Uniform:
			bResult = GenerateFloatDistUniformCurveFromConstantCurveUniform(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_UniformCurve:
			bResult = GenerateFloatDistUniformCurveFromConstantCurveUniformCurve(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_ParticleParam:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateFloatDistConstantCurveFromConstantCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					Percentage);
			}
			else
			{
				bResult = GenerateFloatDistParticleParamFromConstantCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					Percentage);
			}
			break;
		}
		break;
	case DISTRIBUTIONTYPE_Uniform:
		switch (LowVariable->FloatDistVariable.Type)
		{
		case DISTRIBUTIONTYPE_Constant:
			bResult = GenerateFloatDistUniformFromConstantUniform(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				1.0f - Percentage);
			break;
		case DISTRIBUTIONTYPE_ConstantCurve:
			bResult = GenerateFloatDistUniformCurveFromConstantCurveUniform(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				1.0f - Percentage);
			break;
		case DISTRIBUTIONTYPE_Uniform:
			bResult = GenerateFloatDistUniformFromUniformUniform(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_UniformCurve:
			bResult = GenerateFloatDistUniformCurveFromUniformUniformCurve(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_ParticleParam:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateFloatDistUniformFromUniformParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					Percentage);
			}
			else
			{
				bResult = GenerateFloatDistParticleParamFromUniformParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					Percentage);
			}
			break;
		}
		break;
	case DISTRIBUTIONTYPE_UniformCurve:
		switch (LowVariable->FloatDistVariable.Type)
		{
		case DISTRIBUTIONTYPE_Constant:
			bResult = GenerateFloatDistUniformCurveFromConstantUniformCurve(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				1.0f - Percentage);
			break;
		case DISTRIBUTIONTYPE_ConstantCurve:
			bResult = GenerateFloatDistUniformCurveFromConstantCurveUniformCurve(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				1.0f - Percentage);
			break;
		case DISTRIBUTIONTYPE_Uniform:
			bResult = GenerateFloatDistUniformCurveFromUniformUniformCurve(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				1.0f - Percentage);
			break;
		case DISTRIBUTIONTYPE_UniformCurve:
			bResult = GenerateFloatDistUniformCurveFromUniformCurveUniformCurve(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_ParticleParam:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateFloatDistUniformCurveFromUniformCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					Percentage);
			}
			else
			{
				bResult = GenerateFloatDistParticleParamFromUniformCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					Percentage);
			}
			break;
		}
		break;
	case DISTRIBUTIONTYPE_ParticleParam:
		switch (LowVariable->FloatDistVariable.Type)
		{
		case DISTRIBUTIONTYPE_Constant:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateFloatDistConstantFromConstantParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					1.0f - Percentage);
			}
			else
			{
				bResult = GenerateFloatDistParticleParamFromConstantParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					1.0f - Percentage);
			}
			break;
		case DISTRIBUTIONTYPE_ConstantCurve:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateFloatDistConstantCurveFromConstantCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					1.0f - Percentage);
			}
			else
			{
				bResult = GenerateFloatDistParticleParamFromConstantCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					1.0f - Percentage);
			}
			break;
		case DISTRIBUTIONTYPE_Uniform:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateFloatDistUniformFromUniformParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					1.0f - Percentage);
			}
			else
			{
				bResult = GenerateFloatDistParticleParamFromUniformParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					1.0f - Percentage);
			}
			break;
		case DISTRIBUTIONTYPE_UniformCurve:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateFloatDistUniformCurveFromUniformCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					1.0f - Percentage);
			}
			else
			{
				bResult = GenerateFloatDistParticleParamFromUniformCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->FloatDistVariable, 
					LowVariable->FloatDistVariable, 
					HighVariable->FloatDistVariable, 
					1.0f - Percentage);
			}
			break;
		case DISTRIBUTIONTYPE_ParticleParam:
			bResult = GenerateFloatDistParticleParamFromParticleParamParticleParam(
				TargetVariable->OwnerModule,
				TargetVariable->FloatDistVariable, 
				HighVariable->FloatDistVariable, 
				LowVariable->FloatDistVariable, 
				Percentage);
			break;
		}
		break;
	default:
		bResult	= FALSE;
		break;
	}

	return bResult;
}

//*** VECTOR DISTRIBUTION CONVERSIONS
/**
 *	Generate a constant distribution from two constant distributions
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistConstantFromConstantConstant(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_Constant;

	TargetDD.VectorConstant	= ConstructObject<UDistributionVectorConstant>(UDistributionVectorConstant::StaticClass(), OwnerModule);
	check(TargetDD.VectorConstant);

	TargetDD.VectorConstant->Constant	= 
		(HighDD.VectorConstant->Constant * Percentage) + 
		(LowDD.VectorConstant->Constant * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a constant distribution from a constant dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistConstantFromConstantParticleParam(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_Constant;

	TargetDD.VectorConstant	= ConstructObject<UDistributionVectorConstant>(UDistributionVectorConstant::StaticClass(), OwnerModule);
	check(TargetDD.VectorConstant);

	TargetDD.VectorConstant->Constant	= 
		(HighDD.VectorConstant->Constant * Percentage) + 
		(LowDD.VectorParam->GetValue(0.0f) * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a constant curve dist. from a constant dist. and a constant curve dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistConstantCurveFromConstantConstantCurve(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ConstantCurve;

	TargetDD.VectorConstantCurve	= ConstructObject<UDistributionVectorConstantCurve>(UDistributionVectorConstantCurve::StaticClass(), OwnerModule);
	check(TargetDD.VectorConstantCurve);

	FVector HiVector	= HighDD.VectorConstant->Constant;

	UDistributionVectorConstantCurve* LoCC	= LowDD.VectorConstantCurve;
	UDistributionVectorConstantCurve* NewCC	= TargetDD.VectorConstantCurve;

	for (INT KeyIndex = 0; KeyIndex < LoCC->GetNumKeys(); KeyIndex++)
	{
		NewCC->CreateNewKey(LoCC->GetKeyIn(KeyIndex));
		for (INT SubIndex = 0; SubIndex < LoCC->GetNumSubCurves(); SubIndex++)
		{
			FLOAT	LoValue		= LoCC->GetKeyOut(SubIndex, KeyIndex);
			FLOAT	NewValue	= (HiVector[SubIndex] * Percentage) + (LoValue * (1.0f - Percentage));
			NewCC->SetKeyOut(SubIndex, KeyIndex, NewValue);
		}
	}

	return TRUE;
}

/**
 *	Generate a constant curve dist. from two constant curve dists.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistConstantCurveFromConstantCurveConstantCurve(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ConstantCurve;

	TargetDD.VectorConstantCurve	= ConstructObject<UDistributionVectorConstantCurve>(UDistributionVectorConstantCurve::StaticClass(), OwnerModule);
	check(TargetDD.VectorConstantCurve);

	UDistributionVectorConstantCurve* HiCC	= HighDD.VectorConstantCurve;
	UDistributionVectorConstantCurve* LoCC	= LowDD.VectorConstantCurve;
	UDistributionVectorConstantCurve* NewCC	= TargetDD.VectorConstantCurve;

	INT KeyIndex;

//	UBOOL	bSampleBoth = (HiCC->GetNumKeys() < LoCC->GetNumKeys()) ? TRUE : FALSE;
	UBOOL	bSampleBoth = TRUE;

	// First, set the points for the high curve
	for (KeyIndex = 0; KeyIndex < HiCC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= HiCC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewCC->CreateNewKey(KeyIn);

//		for (INT SubIndex = 0; SubIndex < HiCC->GetNumSubCurves(); SubIndex++)
		for (INT SubIndex = 0; SubIndex < 3; SubIndex++)
		{
			FVector	LoVector	= LoCC->GetValue(KeyIn);
			FLOAT	HiValue		= HiCC->GetKeyOut(SubIndex, KeyIndex);

			FLOAT	NewValue	= (HiValue * Percentage) + (LoVector[SubIndex] * (1.0f - Percentage));
			
			NewCC->SetKeyOut(SubIndex, NewKeyIndex, NewValue);
		}
	}

	// If more keys in the low, sample that as well...
	if (bSampleBoth)
	{
		for (KeyIndex = 0; KeyIndex < LoCC->GetNumKeys(); KeyIndex++)
		{
			FLOAT	KeyIn	= LoCC->GetKeyIn(KeyIndex);
			UBOOL	bSetIt	= TRUE;
			for (INT CheckIndex = 0; CheckIndex < NewCC->GetNumKeys(); CheckIndex++)
			{
				if (NewCC->GetKeyIn(CheckIndex) == KeyIn)
				{
					bSetIt = FALSE;
					break;
				}
			}

			if (bSetIt)
			{
				INT		NewKeyIndex	= NewCC->CreateNewKey(KeyIn);

//				for (INT SubIndex = 0; SubIndex < LoCC->GetNumSubCurves(); SubIndex++)
				for (INT SubIndex = 0; SubIndex < 3; SubIndex++)
				{
					FVector	HiVector	= HiCC->GetValue(KeyIn);
					FLOAT	LoValue		= LoCC->GetKeyOut(SubIndex, KeyIndex);
					FLOAT	NewValue	= (HiVector[SubIndex] * Percentage) + (LoValue * (1.0f - Percentage));

					NewCC->SetKeyOut(SubIndex, NewKeyIndex, NewValue);
				}
			}
		}
	}

	return TRUE;
}

/**
 *	Generate a constant curve dist. from a constant curve dist. and a uniform dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistConstantCurveFromConstantCurveUniform(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ConstantCurve;

	TargetDD.VectorConstantCurve	= ConstructObject<UDistributionVectorConstantCurve>(UDistributionVectorConstantCurve::StaticClass(), OwnerModule);
	check(TargetDD.VectorConstantCurve);

	UDistributionVectorConstantCurve*	HiCC	= HighDD.VectorConstantCurve;
	UDistributionVectorUniform*			LoU		= LowDD.VectorUniform;
	UDistributionVectorConstantCurve*	NewCC	= TargetDD.VectorConstantCurve;

	INT KeyIndex;

	FVector	LoVector	= (LoU->Max + LoU->Min) / 2.0f;

	// First, set the points for the high curve
	for (KeyIndex = 0; KeyIndex < HiCC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn = HiCC->GetKeyIn(KeyIndex);
		NewCC->CreateNewKey(KeyIn);

//		for (INT SubIndex = 0; SubIndex < HiCC->GetNumSubCurves(); SubIndex++)
		for (INT SubIndex = 0; SubIndex < 3; SubIndex++)
		{
			FLOAT	HiValue		= HiCC->GetKeyOut(SubIndex, KeyIndex);
			FLOAT	NewValue	= (HiValue * Percentage) + (LoVector[SubIndex] * (1.0f - Percentage));
		
			NewCC->SetKeyOut(SubIndex, KeyIndex, NewValue);
		}
	}

	return TRUE;
}

/**
 *	Generate a constant curve dist. from a constant curve dist. and a uniform curve dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistConstantCurveFromConstantCurveUniformCurve(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ConstantCurve;

	TargetDD.VectorConstantCurve	= ConstructObject<UDistributionVectorConstantCurve>(UDistributionVectorConstantCurve::StaticClass(), OwnerModule);
	check(TargetDD.VectorConstantCurve);

	UDistributionVectorConstantCurve*	HiCC	= HighDD.VectorConstantCurve;
	UDistributionVectorUniformCurve*	LoUC	= LowDD.VectorUniformCurve;
	UDistributionVectorConstantCurve*	NewCC	= TargetDD.VectorConstantCurve;

	INT KeyIndex;

//	UBOOL	bSampleBoth = (HiCC->GetNumKeys() < LoUC->GetNumKeys()) ? TRUE : FALSE;
	UBOOL	bSampleBoth = TRUE;

	// First, set the points for the high curve
	for (KeyIndex = 0; KeyIndex < HiCC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= HiCC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewCC->CreateNewKey(KeyIn);

		FTwoVectors	Lo2Vector	= LoUC->GetMinMaxValue(KeyIn);
		FVector		LoVector	= (Lo2Vector.v1 + Lo2Vector.v2) / 2.0f;

//		for (INT SubIndex = 0; SubIndex < HiCC->GetNumSubCurves(); SubIndex++)
		for (INT SubIndex = 0; SubIndex < 3; SubIndex++)
		{
			FLOAT	HiValue		= HiCC->GetKeyOut(SubIndex, KeyIndex);
			FLOAT	NewValue	= (HiValue * Percentage) + (LoVector[SubIndex] * (1.0f - Percentage));
		
			NewCC->SetKeyOut(SubIndex, NewKeyIndex, NewValue);
		}
	}

	// If more keys in the low, sample that as well...
	if (bSampleBoth)
	{
		for (KeyIndex = 0; KeyIndex < LoUC->GetNumKeys(); KeyIndex++)
		{
			FLOAT	KeyIn	= LoUC->GetKeyIn(KeyIndex);
			UBOOL	bSetIt	= TRUE;
			for (INT CheckIndex = 0; CheckIndex < NewCC->GetNumKeys(); CheckIndex++)
			{
				if (NewCC->GetKeyIn(CheckIndex) == KeyIn)
				{
					bSetIt = FALSE;
					break;
				}
			}

			if (bSetIt)
			{
				INT			NewKeyIndex	= NewCC->CreateNewKey(KeyIn);
				FTwoVectors	Lo2Vector	= LoUC->GetMinMaxValue(KeyIn);
				FVector		LoVector	= (Lo2Vector.v1 + Lo2Vector.v2) / 2.0f;

//				for (INT SubIndex = 0; SubIndex < LoUC->GetNumSubCurves(); SubIndex++)
				for (INT SubIndex = 0; SubIndex < 3; SubIndex++)
				{
					FVector	HiVector	= HiCC->GetValue(KeyIn);
					FLOAT	NewValue	= (HiVector[SubIndex] * Percentage) + (LoVector[SubIndex] * (1.0f - Percentage));
					NewCC->SetKeyOut(SubIndex, NewKeyIndex, NewValue);
				}
			}
		}
	}

	return TRUE;
}

/**
 *	Generate a constant curve dist. from a constant curve dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistConstantCurveFromConstantCurveParticleParam(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ConstantCurve;

	TargetDD.VectorConstantCurve	= ConstructObject<UDistributionVectorConstantCurve>(UDistributionVectorConstantCurve::StaticClass(), OwnerModule);
	check(TargetDD.VectorConstantCurve);

	UDistributionVectorConstantCurve*		HiCC	= HighDD.VectorConstantCurve;
	UDistributionVectorParticleParameter*	LoPP	= LowDD.VectorParam;
	UDistributionVectorConstantCurve*		NewCC	= TargetDD.VectorConstantCurve;

	INT KeyIndex;

	// First, set the points for the high curve
	for (KeyIndex = 0; KeyIndex < HiCC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= HiCC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewCC->CreateNewKey(KeyIn);

		FVector	LoVector	= LoPP->GetValue(KeyIn);

//		for (INT SubIndex = 0; SubIndex < HiCC->GetNumSubCurves(); SubIndex++)
		for (INT SubIndex = 0; SubIndex < 3; SubIndex++)
		{
			FLOAT	HiValue		= HiCC->GetKeyOut(SubIndex, KeyIndex);
			FLOAT	NewValue	= (HiValue * Percentage) + (LoVector[SubIndex] * (1.0f - Percentage));
		
			NewCC->SetKeyOut(SubIndex, NewKeyIndex, NewValue);
		}
	}

	return TRUE;
}

/**
 *	Generate a uniform dist. from a constant dist. and a uniform dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistUniformFromConstantUniform(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_Uniform;

	TargetDD.VectorUniform	= ConstructObject<UDistributionVectorUniform>(UDistributionVectorUniform::StaticClass(), OwnerModule);
	check(TargetDD.VectorUniform);

	UDistributionVectorConstant*		HiC		= HighDD.VectorConstant;
	UDistributionVectorUniform*		LoU		= LowDD.VectorUniform;
	UDistributionVectorUniform*		NewU	= TargetDD.VectorUniform;

	NewU->Min	= (HiC->Constant * Percentage) + (LoU->Min * (1.0f - Percentage));
	NewU->Max	 =(HiC->Constant * Percentage) + (LoU->Max * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a uniform dist. from two uniform dists.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistUniformFromUniformUniform(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_Uniform;

	TargetDD.VectorUniform	= ConstructObject<UDistributionVectorUniform>(UDistributionVectorUniform::StaticClass(), OwnerModule);
	check(TargetDD.VectorUniform);

	UDistributionVectorUniform*	HiU		= HighDD.VectorUniform;
	UDistributionVectorUniform*	LoU		= LowDD.VectorUniform;
	UDistributionVectorUniform*	NewU	= TargetDD.VectorUniform;

	NewU->Min	= (HiU->Min * Percentage) + (LoU->Min * (1.0f - Percentage));
	NewU->Max	 =(HiU->Max * Percentage) + (LoU->Max * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a uniform dist. from a uniform dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistUniformFromUniformParticleParam(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_Uniform;

	TargetDD.VectorUniform	= ConstructObject<UDistributionVectorUniform>(UDistributionVectorUniform::StaticClass(), OwnerModule);
	check(TargetDD.VectorUniform);

	UDistributionVectorUniform*				HiU		= HighDD.VectorUniform;
	UDistributionVectorParticleParameter*	LoPP	= LowDD.VectorParam;
	UDistributionVectorUniform*				NewU	= TargetDD.VectorUniform;

	FVector	LoVector	= LoPP->GetValue(0.0f);

	NewU->Min	= (HiU->Min * Percentage) + (LoVector * (1.0f - Percentage));
	NewU->Max	 =(HiU->Max * Percentage) + (LoVector * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a uniform curve dist. from a constant dist. and a uniform curve dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistUniformCurveFromConstantUniformCurve(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_UniformCurve;

	TargetDD.VectorUniformCurve	= ConstructObject<UDistributionVectorUniformCurve>(UDistributionVectorUniformCurve::StaticClass(), OwnerModule);
	check(TargetDD.VectorUniformCurve);

	UDistributionVectorConstant*		HiC		= HighDD.VectorConstant;
	UDistributionVectorUniformCurve*	LoUC	= LowDD.VectorUniformCurve;
	UDistributionVectorUniformCurve*	NewUC	= TargetDD.VectorUniformCurve;

	INT KeyIndex;

	FVector	HiVector	= HiC->Constant;

	// First, set the points for the high curve
	for (KeyIndex = 0; KeyIndex < LoUC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= LoUC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewUC->CreateNewKey(KeyIn);

//		for (INT SubIndex = 0; SubIndex < LoUC->GetNumSubCurves(); SubIndex++)
		for (INT SubIndex = 0; SubIndex < 6; SubIndex++)
		{
			FLOAT	LoValue		= LoUC->GetKeyOut(SubIndex, KeyIndex);
			FLOAT	NewValue;

			if ((SubIndex == 0) || (SubIndex == 1))
			{
				NewValue	= (HiVector.X * Percentage) + (LoValue * (1.0f - Percentage));
			}
			else
			if ((SubIndex == 2) || (SubIndex == 3))
			{
				NewValue	= (HiVector.Y * Percentage) + (LoValue * (1.0f - Percentage));
			}
			else
			{
				NewValue	= (HiVector.Z * Percentage) + (LoValue * (1.0f - Percentage));
			}
			
			NewUC->SetKeyOut(SubIndex, NewKeyIndex, NewValue);
		}
	}

	return TRUE;
}

/**
 *	Generate a uniform curve dist. from a constant curve dist. and a uniform dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistUniformCurveFromConstantCurveUniform(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_UniformCurve;

	TargetDD.VectorUniformCurve	= ConstructObject<UDistributionVectorUniformCurve>(UDistributionVectorUniformCurve::StaticClass(), OwnerModule);
	check(TargetDD.VectorUniformCurve);

	UDistributionVectorConstantCurve*	HiCC	= HighDD.VectorConstantCurve;
	UDistributionVectorUniform*			LoU		= LowDD.VectorUniform;
	UDistributionVectorUniformCurve*	NewUC	= TargetDD.VectorUniformCurve;

	INT KeyIndex;

	FVector	LoVectorMin	= LoU->Min;
	FVector	LoVectorMax	= LoU->Max;

	for (KeyIndex = 0; KeyIndex < HiCC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= HiCC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewUC->CreateNewKey(KeyIn);

//		for (INT SubIndex = 0; SubIndex < NewUC->GetNumSubCurves(); SubIndex++)
		for (INT SubIndex = 0; SubIndex < 6; SubIndex++)
		{
			FLOAT	HiValue = 0.f;
			FLOAT	LoValue = 0.f;

			switch (SubIndex)
			{
			case 0:
				LoValue	= LoVectorMax.X;
				HiValue	= HiCC->GetKeyOut(0, KeyIndex);
				break;
			case 1:
				LoValue	= LoVectorMin.X;
				HiValue	= HiCC->GetKeyOut(0, KeyIndex);
				break;
			case 2:
				LoValue	= LoVectorMax.Y;
				HiValue	= HiCC->GetKeyOut(1, KeyIndex);
				break;
			case 3:
				LoValue	= LoVectorMin.Y;
				HiValue	= HiCC->GetKeyOut(1, KeyIndex);
				break;
			case 4:
				LoValue	= LoVectorMax.Z;
				HiValue	= HiCC->GetKeyOut(2, KeyIndex);
				break;
			case 5:
				LoValue	= LoVectorMin.Z;
				HiValue	= HiCC->GetKeyOut(2, KeyIndex);
				break;
			}

			FLOAT	NewValue	= (HiValue * Percentage) + (LoValue * (1.0f - Percentage));

			NewUC->SetKeyOut(SubIndex, NewKeyIndex, NewValue);
		}
	}

	return TRUE;
}

/**
 *	Generate a uniform curve dist. from a constant curve dist. and a uniform curve dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistUniformCurveFromConstantCurveUniformCurve(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_UniformCurve;

	TargetDD.VectorUniformCurve	= ConstructObject<UDistributionVectorUniformCurve>(UDistributionVectorUniformCurve::StaticClass(), OwnerModule);
	check(TargetDD.VectorUniformCurve);

	UDistributionVectorConstantCurve*	HiCC	= HighDD.VectorConstantCurve;
	UDistributionVectorUniformCurve*	LoUC	= LowDD.VectorUniformCurve;
	UDistributionVectorUniformCurve*	NewUC	= TargetDD.VectorUniformCurve;

	INT KeyIndex;

//	UBOOL	bSampleBoth = (HiCC->GetNumKeys() < LoUC->GetNumKeys()) ? TRUE : FALSE;
	UBOOL	bSampleBoth = TRUE;

	// First, set the points for the high curve
	for (KeyIndex = 0; KeyIndex < HiCC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= HiCC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewUC->CreateNewKey(KeyIn);

		FTwoVectors	Lo2Vector	= LoUC->GetMinMaxValue(KeyIn);

//		for (INT SubIndex = 0; SubIndex < NewUC->GetNumSubCurves(); SubIndex++)
		for (INT SubIndex = 0; SubIndex < 6; SubIndex++)
		{
			FLOAT LoValue = 0.f;
			FLOAT HiValue = 0.f;

			switch (SubIndex)
			{
			case 0:
				LoValue	= Lo2Vector.v1.X;
				HiValue	= HiCC->GetKeyOut(0, KeyIndex);
				break;
			case 1:
				LoValue	= Lo2Vector.v2.X;
				HiValue	= HiCC->GetKeyOut(0, KeyIndex);
				break;
			case 2:
				LoValue	= Lo2Vector.v1.Y;
				HiValue	= HiCC->GetKeyOut(1, KeyIndex);
				break;
			case 3:
				LoValue	= Lo2Vector.v1.Y;
				HiValue	= HiCC->GetKeyOut(1, KeyIndex);
				break;
			case 4:
				LoValue	= Lo2Vector.v1.Z;
				HiValue	= HiCC->GetKeyOut(2, KeyIndex);
				break;
			case 5:
				LoValue	= Lo2Vector.v1.Z;
				HiValue	= HiCC->GetKeyOut(2, KeyIndex);
				break;
			}

			const FLOAT	NewValue	= (HiValue * Percentage) + (LoValue * (1.0f - Percentage));
		
			NewUC->SetKeyOut(SubIndex, NewKeyIndex, NewValue);
		}
	}

	// If more keys in the low, sample that as well...
	if (bSampleBoth)
	{
		for (KeyIndex = 0; KeyIndex < LoUC->GetNumKeys(); KeyIndex++)
		{
			FLOAT	KeyIn	= LoUC->GetKeyIn(KeyIndex);
			UBOOL	bSetIt	= TRUE;
			for (INT CheckIndex = 0; CheckIndex < NewUC->GetNumKeys(); CheckIndex++)
			{
				if (NewUC->GetKeyIn(CheckIndex) == KeyIn)
				{
					bSetIt = FALSE;
					break;
				}
			}

			if (bSetIt)
			{
				INT			NewKeyIndex	= NewUC->CreateNewKey(KeyIn);

				FVector		HiVector	= HiCC->GetValue(KeyIn);
				FTwoVectors	Lo2Vector	= LoUC->GetMinMaxValue(KeyIn);

//				for (INT SubIndex = 0; SubIndex < NewUC->GetNumSubCurves(); SubIndex++)
				for (INT SubIndex = 0; SubIndex < 6; SubIndex++)
				{
					FLOAT	LoValue = 0.f;
					FLOAT	HiValue = 0.f;

					switch (SubIndex)
					{
					case 0:
						LoValue	= Lo2Vector.v1.X;
						HiValue	= HiVector.X;
						break;
					case 1:
						LoValue	= Lo2Vector.v2.X;
						HiValue	= HiVector.X;
						break;
					case 2:
						LoValue	= Lo2Vector.v1.Y;
						HiValue	= HiVector.Y;
						break;
					case 3:
						LoValue	= Lo2Vector.v1.Y;
						HiValue	= HiVector.Y;
						break;
					case 4:
						LoValue	= Lo2Vector.v1.Z;
						HiValue	= HiVector.Z;
						break;
					case 5:
						LoValue	= Lo2Vector.v1.Z;
						HiValue	= HiVector.Z;
						break;
					}

					const FLOAT	NewValue	= (HiValue * Percentage) + (LoValue * (1.0f - Percentage));
				
					NewUC->SetKeyOut(SubIndex, NewKeyIndex, NewValue);
				}
			}
		}
	}

	return TRUE;
}

/**
 *	Generate a uniform curve dist. from a uniform dist. and a uniform curve dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistUniformCurveFromUniformUniformCurve(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_UniformCurve;

	TargetDD.VectorUniformCurve	= ConstructObject<UDistributionVectorUniformCurve>(UDistributionVectorUniformCurve::StaticClass(), OwnerModule);
	check(TargetDD.VectorUniformCurve);

	UDistributionVectorUniform*			HiU		= HighDD.VectorUniform;
	UDistributionVectorUniformCurve*	LoUC	= LowDD.VectorUniformCurve;
	UDistributionVectorUniformCurve*	NewUC	= TargetDD.VectorUniformCurve;

	INT KeyIndex;
	
	FVector	HiVectorMax	= HiU->Max;
	FVector	HiVectorMin	= HiU->Min;

	for (KeyIndex = 0; KeyIndex < LoUC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= LoUC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewUC->CreateNewKey(KeyIn);

		FTwoVectors	Lo2Vector	= LoUC->GetMinMaxValue(KeyIn);

//		for (INT SubIndex = 0; SubIndex < LoUC->GetNumSubCurves(); SubIndex++)
		for (INT SubIndex = 0; SubIndex < 6; SubIndex++)
		{
			FLOAT	LoValue = 0.f;
			FLOAT	HiValue = 0.f;

			switch (SubIndex)
			{
			case 0:
				LoValue	= Lo2Vector.v1.X;
				HiValue	= HiVectorMax.X;
				break;
			case 1:
				LoValue	= Lo2Vector.v2.X;
				HiValue	= HiVectorMin.X;
				break;
			case 2:
				LoValue	= Lo2Vector.v1.Y;
				HiValue	= HiVectorMax.Y;
				break;
			case 3:
				LoValue	= Lo2Vector.v1.Y;
				HiValue	= HiVectorMin.Y;
				break;
			case 4:
				LoValue	= Lo2Vector.v1.Z;
				HiValue	= HiVectorMax.Z;
				break;
			case 5:
				LoValue	= Lo2Vector.v1.Z;
				HiValue	= HiVectorMin.Z;
				break;
			}

			const FLOAT	NewValue	= (HiValue * Percentage) + (LoValue * (1.0f - Percentage));
			NewUC->SetKeyOut(SubIndex, NewKeyIndex, NewValue);
		}
	}

	return TRUE;
}

/**
 *	Generate a uniform curve dist. from a uniform curve dist. and a uniform curve dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistUniformCurveFromUniformCurveUniformCurve(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_UniformCurve;

	TargetDD.VectorUniformCurve	= ConstructObject<UDistributionVectorUniformCurve>(UDistributionVectorUniformCurve::StaticClass(), OwnerModule);
	check(TargetDD.VectorUniformCurve);

	UDistributionVectorUniformCurve*	HiUC	= HighDD.VectorUniformCurve;
	UDistributionVectorUniformCurve*	LoUC	= LowDD.VectorUniformCurve;
	UDistributionVectorUniformCurve*	NewUC	= TargetDD.VectorUniformCurve;

	INT KeyIndex;

//	UBOOL	bSampleBoth = (HiUC->GetNumKeys() < LoUC->GetNumKeys()) ? TRUE : FALSE;
	UBOOL	bSampleBoth = TRUE;

	// First, set the points for the high curve
	for (KeyIndex = 0; KeyIndex < HiUC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= HiUC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewUC->CreateNewKey(KeyIn);

		FTwoVectors	Lo2Vector	= LoUC->GetMinMaxValue(KeyIn);

//		for (INT SubIndex = 0; SubIndex < HiUC->GetNumSubCurves(); SubIndex++)
		for (INT SubIndex = 0; SubIndex < 6; SubIndex++)
		{
			FLOAT	LoValue		= Lo2Vector[SubIndex];
			FLOAT	HiValue		= HiUC->GetKeyOut(SubIndex, KeyIndex);

			FLOAT	NewValue	= (HiValue * Percentage) + (LoValue* (1.0f - Percentage));
		
			NewUC->SetKeyOut(SubIndex, NewKeyIndex, NewValue);
		}
	}

	// If more keys in the low, sample that as well...
	if (bSampleBoth)
	{
		for (KeyIndex = 0; KeyIndex < LoUC->GetNumKeys(); KeyIndex++)
		{
			FLOAT	KeyIn	= LoUC->GetKeyIn(KeyIndex);
			UBOOL	bSetIt	= TRUE;
			for (INT CheckIndex = 0; CheckIndex < NewUC->GetNumKeys(); CheckIndex++)
			{
				if (NewUC->GetKeyIn(CheckIndex) == KeyIn)
				{
					bSetIt = FALSE;
					break;
				}
			}

			if (bSetIt)
			{
				INT			NewKeyIndex	= NewUC->CreateNewKey(KeyIn);

				FTwoVectors	Hi2Vector	= HiUC->GetMinMaxValue(KeyIn);

//				for (INT SubIndex = 0; SubIndex < LoUC->GetNumSubCurves(); SubIndex++)
				for (INT SubIndex = 0; SubIndex < 6; SubIndex++)
				{
					FLOAT	LoValue		= LoUC->GetKeyOut(SubIndex, KeyIndex);
					FLOAT	HiValue		= Hi2Vector[SubIndex];
					FLOAT	NewValue	= (HiValue * Percentage) + (LoValue * (1.0f - Percentage));

					NewUC->SetKeyOut(SubIndex, NewKeyIndex, NewValue);
				}
			}
		}
	}

	return TRUE;
}

/**
 *	Generate a uniform curve dist. from a uniform curve dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistUniformCurveFromUniformCurveParticleParam(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_UniformCurve;

	TargetDD.VectorUniformCurve	= ConstructObject<UDistributionVectorUniformCurve>(UDistributionVectorUniformCurve::StaticClass(), OwnerModule);
	check(TargetDD.VectorUniformCurve);

	UDistributionVectorUniformCurve*		HiUC	= HighDD.VectorUniformCurve;
	UDistributionVectorParticleParameter*	LoPP	= LowDD.VectorParam;
	UDistributionVectorUniformCurve*		NewUC	= TargetDD.VectorUniformCurve;

	INT KeyIndex;
	
	FVector	LoVector	= LoPP->Constant;

	for (KeyIndex = 0; KeyIndex < HiUC->GetNumKeys(); KeyIndex++)
	{
		FLOAT	KeyIn		= HiUC->GetKeyIn(KeyIndex);
		INT		NewKeyIndex	= NewUC->CreateNewKey(KeyIn);

//		for (INT SubIndex = 0; SubIndex < HiUC->GetNumSubCurves(); SubIndex++)
		for (INT SubIndex = 0; SubIndex < 6; SubIndex++)
		{
			FLOAT	LoValue = 0.f;

			switch (SubIndex)
			{
			case 0:
			case 1:
				LoValue	= LoVector.X;
				break;
			case 2:
			case 3:
				LoValue	= LoVector.Y;
				break;
			case 4:
			case 5:
				LoValue	= LoVector.Z;
				break;
			}

			const FLOAT	HiValue		= HiUC->GetKeyOut(SubIndex, KeyIndex);
			const FLOAT	NewValue	= (HiValue * Percentage) + (LoValue * (1.0f - Percentage));
		
			NewUC->SetKeyOut(SubIndex, NewKeyIndex, NewValue);
		}
	}

	return TRUE;
}

/**
 *	Generate a particle param dist. from a constant dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistParticleParamFromConstantParticleParam(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ParticleParam;

	TargetDD.VectorParam	= ConstructObject<UDistributionVectorParticleParameter>(UDistributionVectorParticleParameter::StaticClass(), OwnerModule);
	check(TargetDD.VectorParam);

	UDistributionVectorConstant*			HiC		= HighDD.VectorConstant;
	UDistributionVectorParticleParameter*	LoPP	= LowDD.VectorParam;
	UDistributionVectorParticleParameter*	NewPP	= TargetDD.VectorParam;

	NewPP->ParameterName	= LoPP->ParameterName;
	NewPP->MinInput			= LoPP->MinInput;
	NewPP->MaxInput			= LoPP->MaxInput;
	NewPP->MinOutput		= LoPP->MinOutput;
	NewPP->MaxOutput		= LoPP->MaxOutput;
	NewPP->ParamModes[0]	= LoPP->ParamModes[0];
	NewPP->ParamModes[1]	= LoPP->ParamModes[1];
	NewPP->ParamModes[2]	= LoPP->ParamModes[2];

	NewPP->Constant	= (HiC->Constant * Percentage) + (LoPP->Constant * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a particle param dist. from a constant curve dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistParticleParamFromConstantCurveParticleParam(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ParticleParam;

	TargetDD.VectorParam	= ConstructObject<UDistributionVectorParticleParameter>(UDistributionVectorParticleParameter::StaticClass(), OwnerModule);
	check(TargetDD.VectorParam);

	UDistributionVectorConstantCurve*		HiCC	= HighDD.VectorConstantCurve;
	UDistributionVectorParticleParameter*	LoPP	= LowDD.VectorParam;
	UDistributionVectorParticleParameter*	NewPP	= TargetDD.VectorParam;

	NewPP->ParameterName	= LoPP->ParameterName;
	NewPP->MinInput			= LoPP->MinInput;
	NewPP->MaxInput			= LoPP->MaxInput;
	NewPP->MinOutput		= LoPP->MinOutput;
	NewPP->MaxOutput		= LoPP->MaxOutput;
	NewPP->ParamModes[0]	= LoPP->ParamModes[0];
	NewPP->ParamModes[1]	= LoPP->ParamModes[1];
	NewPP->ParamModes[2]	= LoPP->ParamModes[2];

	FVector	MinOut, MaxOut;

	HiCC->GetRange(MinOut, MaxOut);

	NewPP->Constant	= (((MinOut + MaxOut) / 2.0f) * Percentage) + (LoPP->Constant * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a particle param dist. from a unfirom dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistParticleParamFromUniformParticleParam(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ParticleParam;

	TargetDD.VectorParam	= ConstructObject<UDistributionVectorParticleParameter>(UDistributionVectorParticleParameter::StaticClass(), OwnerModule);
	check(TargetDD.VectorParam);

	UDistributionVectorUniform*				HiU		= HighDD.VectorUniform;
	UDistributionVectorParticleParameter*	LoPP	= LowDD.VectorParam;
	UDistributionVectorParticleParameter*	NewPP	= TargetDD.VectorParam;

	NewPP->ParameterName	= LoPP->ParameterName;
	NewPP->MinInput			= LoPP->MinInput;
	NewPP->MaxInput			= LoPP->MaxInput;
	NewPP->MinOutput		= LoPP->MinOutput;
	NewPP->MaxOutput		= LoPP->MaxOutput;
	NewPP->ParamModes[0]	= LoPP->ParamModes[0];
	NewPP->ParamModes[1]	= LoPP->ParamModes[1];
	NewPP->ParamModes[2]	= LoPP->ParamModes[2];

	NewPP->Constant	= (((HiU->Min + HiU->Max) / 2.0f) * Percentage) + (LoPP->Constant * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a particle param dist. from a uniform curve dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistParticleParamFromUniformCurveParticleParam(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ParticleParam;

	TargetDD.VectorParam	= ConstructObject<UDistributionVectorParticleParameter>(UDistributionVectorParticleParameter::StaticClass(), OwnerModule);
	check(TargetDD.VectorParam);

	UDistributionVectorUniformCurve*			HiUC	= HighDD.VectorUniformCurve;
	UDistributionVectorParticleParameter*	LoPP	= LowDD.VectorParam;
	UDistributionVectorParticleParameter*	NewPP	= TargetDD.VectorParam;

	NewPP->ParameterName	= LoPP->ParameterName;
	NewPP->MinInput			= LoPP->MinInput;
	NewPP->MaxInput			= LoPP->MaxInput;
	NewPP->MinOutput		= LoPP->MinOutput;
	NewPP->MaxOutput		= LoPP->MaxOutput;
	NewPP->ParamModes[0]	= LoPP->ParamModes[0];
	NewPP->ParamModes[1]	= LoPP->ParamModes[1];
	NewPP->ParamModes[2]	= LoPP->ParamModes[2];

	FVector	MinOut, MaxOut;

	HiUC->GetRange(MinOut, MaxOut);

	NewPP->Constant	= (((MinOut + MaxOut) / 2.0f) * Percentage) + (LoPP->Constant * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate a particle param dist. from a particle param dist. and a particle param dist.
 *	INTERNAL FUNCTION
 */
UBOOL GenerateVectorDistParticleParamFromParticleParamParticleParam(
	UParticleModule* OwnerModule, 
	VectorDistributionDetermination& TargetDD, 
	VectorDistributionDetermination& HighDD,
	VectorDistributionDetermination& LowDD,
	FLOAT Percentage)
{
	TargetDD.Type	= DISTRIBUTIONTYPE_ParticleParam;

	TargetDD.VectorParam	= ConstructObject<UDistributionVectorParticleParameter>(UDistributionVectorParticleParameter::StaticClass(), OwnerModule);
	check(TargetDD.VectorParam);

	UDistributionVectorParticleParameter*	HiPP	= HighDD.VectorParam;
	UDistributionVectorParticleParameter*	LoPP	= LowDD.VectorParam;
	UDistributionVectorParticleParameter*	NewPP	= TargetDD.VectorParam;

	if (Percentage <= 0.5f)
	{
		NewPP->ParameterName	= HiPP->ParameterName;
		NewPP->ParamModes[0]	= HiPP->ParamModes[0];
		NewPP->ParamModes[1]	= HiPP->ParamModes[1];
		NewPP->ParamModes[2]	= HiPP->ParamModes[2];
	}
	else
	{
		NewPP->ParameterName	= LoPP->ParameterName;
		NewPP->ParamModes[0]	= LoPP->ParamModes[0];
		NewPP->ParamModes[1]	= LoPP->ParamModes[1];
		NewPP->ParamModes[2]	= LoPP->ParamModes[2];
	}
	NewPP->MinInput		= (HiPP->MinInput * Percentage)		+ (LoPP->MinInput * (1.0f - Percentage));
	NewPP->MaxInput		= (HiPP->MaxInput * Percentage)		+ (LoPP->MaxInput * (1.0f - Percentage));
	NewPP->MinOutput	= (HiPP->MinOutput * Percentage)	+ (LoPP->MinOutput * (1.0f - Percentage));
	NewPP->MaxOutput	= (HiPP->MaxOutput * Percentage)	+ (LoPP->MaxOutput * (1.0f - Percentage));
	NewPP->Constant		= (HiPP->Constant * Percentage)		+ (LoPP->Constant * (1.0f - Percentage));

	return TRUE;
}

/**
 *	Generate the target VectorDistribution variable
 *	INTERNAL FUNCTION
 */
UBOOL GenerateTargetVariableVectorDist(VariableDetermination* TargetVariable, 
	VariableDetermination* HighVariable, VariableDetermination* LowVariable,
	FLOAT Percentage)
{
	UBOOL				bResult	= FALSE;

	// Depending on the types, do the conversion
	// Use the following grid for conversion:
	//
	// -----+-----+-----------------------------+--------------------------------+
	// High | Low |  Percent < 0.5              | Percent >= 0.5                 |
	// -----+-----+-----------------------------+--------------------------------+
	//  C   | C   | C  (H*P+L*(1-P))                                             |
	//  C   | CC  | CC Replicate curve using H*P+L*(1-P) at each point           |
	//  C   | U   | U  Offset the uniform values by the constant                 |
	//  C   | UC  | UC Replicate curve offseting the values by the constant      |
	//  C   | PP  | PP                                                           |
	// -----+-----+-----------------------------+--------------------------------+
	//  CC  | C   | CC Replicate curve using H*P+L*(1-P) at each point           |
	//  CC  | CC  | CC Replicate curve using H*P+L*(1-P) at each point           |
	//  CC  | U   | UC                                                           |
	//  CC  | UC  | UC Replicate curve offseting the values by the constant      |
	//  CC  | PP  | CC                          | PP                             |
	// -----+-----+-----------------------------+--------------------------------+
	//  U   | C   | U  Using H*P+L*(1-P)                                         |
	//  U   | CC  | UC Replicate curve using H*P+L*(1-P) at each point           |
	//  U   | U   | U  Using H*P+L*(1-P)                                         |
	//  U   | UC  | UC Replicate curve offseting the values by the constant      |
	//  U   | PP  | U                           | PP                             |
	// -----+-----+-----------------------------+--------------------------------+
	//  UC  | C   | UC Using H*P+L*(1-P)                                         |
	//  UC  | CC  | UC Replicate curve using H*P+L*(1-P) at each point           |
	//  UC  | U   | UC Using H*P+L*(1-P)                                         |
	//  UC  | UC  | UC Using H*P+L*(1-P)                                         |
	//  UC  | PP  | UC                          | PP                             |
	// -----+-----+-----------------------------+--------------------------------+
	//  PP  | C   | PP                                                           |
	//  PP  | CC  | PP                                                           |
	//  PP  | U   | PP                                                           |
	//  PP  | UC  | PP                                                           |
	//  PP  | PP  | PP                                                           |
	// -----+-----+-----------------------------+--------------------------------+
	switch (HighVariable->VectorDistVariable.Type)
	{
	case DISTRIBUTIONTYPE_Constant:
		switch (LowVariable->VectorDistVariable.Type)
		{
		case DISTRIBUTIONTYPE_Constant:
			bResult = GenerateVectorDistConstantFromConstantConstant(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_ConstantCurve:
			bResult = GenerateVectorDistConstantCurveFromConstantConstantCurve(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_Uniform:
			bResult = GenerateVectorDistUniformFromConstantUniform(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_UniformCurve:
			bResult = GenerateVectorDistUniformCurveFromConstantUniformCurve(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_ParticleParam:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateVectorDistConstantFromConstantParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					Percentage);
			}
			else
			{
				bResult = GenerateVectorDistParticleParamFromConstantParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					Percentage);
			}
			break;
		}
		break;
	case DISTRIBUTIONTYPE_ConstantCurve:
		switch (LowVariable->VectorDistVariable.Type)
		{
		case DISTRIBUTIONTYPE_Constant:
			bResult = GenerateVectorDistConstantCurveFromConstantConstantCurve(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				1.0f - Percentage);
			break;
		case DISTRIBUTIONTYPE_ConstantCurve:
			bResult = GenerateVectorDistConstantCurveFromConstantCurveConstantCurve(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_Uniform:
			bResult = GenerateVectorDistUniformCurveFromConstantCurveUniform(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_UniformCurve:
			bResult = GenerateVectorDistUniformCurveFromConstantCurveUniformCurve(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_ParticleParam:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateVectorDistConstantCurveFromConstantCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					Percentage);
			}
			else
			{
				bResult = GenerateVectorDistParticleParamFromConstantCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					Percentage);
			}
			break;
		}
		break;
	case DISTRIBUTIONTYPE_Uniform:
		switch (LowVariable->VectorDistVariable.Type)
		{
		case DISTRIBUTIONTYPE_Constant:
			bResult = GenerateVectorDistUniformFromConstantUniform(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				1.0f - Percentage);
			break;
		case DISTRIBUTIONTYPE_ConstantCurve:
			bResult = GenerateVectorDistUniformCurveFromConstantCurveUniform(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				1.0f - Percentage);
			break;
		case DISTRIBUTIONTYPE_Uniform:
			bResult = GenerateVectorDistUniformFromUniformUniform(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_UniformCurve:
			bResult = GenerateVectorDistUniformCurveFromUniformUniformCurve(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_ParticleParam:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateVectorDistUniformFromUniformParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					Percentage);
			}
			else
			{
				bResult = GenerateVectorDistParticleParamFromUniformParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					Percentage);
			}
			break;
		}
		break;
	case DISTRIBUTIONTYPE_UniformCurve:
		switch (LowVariable->VectorDistVariable.Type)
		{
		case DISTRIBUTIONTYPE_Constant:
			bResult = GenerateVectorDistUniformCurveFromConstantUniformCurve(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				1.0f - Percentage);
			break;
		case DISTRIBUTIONTYPE_ConstantCurve:
			bResult = GenerateVectorDistUniformCurveFromConstantCurveUniformCurve(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				1.0f - Percentage);
			break;
		case DISTRIBUTIONTYPE_Uniform:
			bResult = GenerateVectorDistUniformCurveFromUniformUniformCurve(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				1.0f - Percentage);
			break;
		case DISTRIBUTIONTYPE_UniformCurve:
			bResult = GenerateVectorDistUniformCurveFromUniformCurveUniformCurve(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				Percentage);
			break;
		case DISTRIBUTIONTYPE_ParticleParam:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateVectorDistUniformCurveFromUniformCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					Percentage);
			}
			else
			{
				bResult = GenerateVectorDistParticleParamFromUniformCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					Percentage);
			}
			break;
		}
		break;
	case DISTRIBUTIONTYPE_ParticleParam:
		switch (LowVariable->VectorDistVariable.Type)
		{
		case DISTRIBUTIONTYPE_Constant:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateVectorDistConstantFromConstantParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					1.0f - Percentage);
			}
			else
			{
				bResult = GenerateVectorDistParticleParamFromConstantParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					1.0f - Percentage);
			}
			break;
		case DISTRIBUTIONTYPE_ConstantCurve:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateVectorDistConstantCurveFromConstantCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					1.0f - Percentage);
			}
			else
			{
				bResult = GenerateVectorDistParticleParamFromConstantCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					1.0f - Percentage);
			}
			break;
		case DISTRIBUTIONTYPE_Uniform:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateVectorDistUniformFromUniformParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					1.0f - Percentage);
			}
			else
			{
				bResult = GenerateVectorDistParticleParamFromUniformParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					1.0f - Percentage);
			}
			break;
		case DISTRIBUTIONTYPE_UniformCurve:
			if (Percentage <= 0.5f)
			{
				bResult = GenerateVectorDistUniformCurveFromUniformCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					1.0f - Percentage);
			}
			else
			{
				bResult = GenerateVectorDistParticleParamFromUniformCurveParticleParam(
					TargetVariable->OwnerModule,
					TargetVariable->VectorDistVariable, 
					LowVariable->VectorDistVariable, 
					HighVariable->VectorDistVariable, 
					1.0f - Percentage);
			}
			break;
		case DISTRIBUTIONTYPE_ParticleParam:
			bResult = GenerateVectorDistParticleParamFromParticleParamParticleParam(
				TargetVariable->OwnerModule,
				TargetVariable->VectorDistVariable, 
				HighVariable->VectorDistVariable, 
				LowVariable->VectorDistVariable, 
				Percentage);
			break;
		}
		break;
	default:
		bResult	= FALSE;
		break;
	}

	return bResult;
}

/**
 *	Generate the target variable
 *	INTERNAL FUNCTION
 */
UBOOL GenerateTargetVariable(VariableDetermination* TargetVariable, 
	VariableDetermination* HighVariable, VariableDetermination* LowVariable,
	FLOAT Percentage)
{
	check(TargetVariable);
	check(HighVariable);
	check(LowVariable);
	check(TargetVariable->Type == HighVariable->Type);
	check(TargetVariable->Type == LowVariable->Type);

	switch (TargetVariable->Type)
	{
	case VARIABLETYPE_Boolean:
		return GenerateTargetVariableBoolean(TargetVariable, HighVariable, LowVariable, Percentage);
	case VARIABLETYPE_Float:
		return GenerateTargetVariableFloat(TargetVariable, HighVariable, LowVariable, Percentage);
	case VARIABLETYPE_FColor:
		return GenerateTargetVariableFColor(TargetVariable, HighVariable, LowVariable, Percentage);
	case VARIABLETYPE_FVector:
		return GenerateTargetVariableFVector(TargetVariable, HighVariable, LowVariable, Percentage);
	case VARIABLETYPE_FloatDist:
		return GenerateTargetVariableFloatDist(TargetVariable, HighVariable, LowVariable, Percentage);
	case VARIABLETYPE_VectorDist:
		return GenerateTargetVariableVectorDist(TargetVariable, HighVariable, LowVariable, Percentage);
	case VARIABLETYPE_Unknown:
		// We don't convert these...
		return TRUE;
	}

	return FALSE;
}

UBOOL FinalizeVariableBoolean(UParticleModule* Module, VariableDetermination* Variable)
{
//	Variable->Prop
//	Variable->BooleanVariable
	BITFIELD*	Data	= (BITFIELD*)((BYTE*)Module + Variable->Prop->Offset);
	if (Variable->BooleanVariable)
	{
		UBoolProperty* BoolProp	= Cast<UBoolProperty>(Variable->Prop);
		*Data |= BoolProp->BitMask;
	}
	else
	{
		UBoolProperty* BoolProp	= Cast<UBoolProperty>(Variable->Prop);
		*Data &= ~(BoolProp->BitMask);
	}

	return TRUE;
}

UBOOL FinalizeVariableFloat(UParticleModule* Module, VariableDetermination* Variable)
{
	FLOAT*	Data	= (FLOAT*)((BYTE*)Module + Variable->Prop->Offset);
	*Data	= Variable->FloatVariable;

	return TRUE;
}

UBOOL FinalizeVariableFColor(UParticleModule* Module, VariableDetermination* Variable)
{
	FColor* Data	= (FColor*)((BYTE*)Module + Variable->Prop->Offset);
	*Data	= Variable->FColorVariable;

	return TRUE;
}

UBOOL FinalizeVariableFVector(UParticleModule* Module, VariableDetermination* Variable)
{
	FVector* Data	= (FVector*)((BYTE*)Module + Variable->Prop->Offset);
	*Data	= Variable->FVectorVariable;

	return TRUE;
}

UBOOL FinalizeVariableFloatDist(UParticleModule* Module, VariableDetermination* Variable)
{
	UDistributionFloat** FloatDistPtr = (UDistributionFloat**)(((BYTE*)Module) + Variable->Prop->Offset);
	switch (Variable->FloatDistVariable.Type)
	{
	case DISTRIBUTIONTYPE_Constant:
		*FloatDistPtr	= Variable->FloatDistVariable.FloatConstant;
		break;
	case DISTRIBUTIONTYPE_ConstantCurve:
		*FloatDistPtr	= Variable->FloatDistVariable.FloatConstantCurve;
		break;
	case DISTRIBUTIONTYPE_Uniform:
		*FloatDistPtr	= Variable->FloatDistVariable.FloatUniform;
		break;
	case DISTRIBUTIONTYPE_UniformCurve:
		*FloatDistPtr	= Variable->FloatDistVariable.FloatUniformCurve;
		break;
	case DISTRIBUTIONTYPE_ParticleParam:
		*FloatDistPtr	= Variable->FloatDistVariable.FloatParam;
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

UBOOL FinalizeVariableVectorDist(UParticleModule* Module, VariableDetermination* Variable)
{
	UDistributionVector** VectorDistPtr = (UDistributionVector**)(((BYTE*)Module) + Variable->Prop->Offset);
	switch (Variable->VectorDistVariable.Type)
	{
	case DISTRIBUTIONTYPE_Constant:
		*VectorDistPtr	= Variable->VectorDistVariable.VectorConstant;
		break;
	case DISTRIBUTIONTYPE_ConstantCurve:
		*VectorDistPtr	= Variable->VectorDistVariable.VectorConstantCurve;
		break;
	case DISTRIBUTIONTYPE_Uniform:
		*VectorDistPtr	= Variable->VectorDistVariable.VectorUniform;
		break;
	case DISTRIBUTIONTYPE_UniformCurve:
		*VectorDistPtr	= Variable->VectorDistVariable.VectorUniformCurve;
		break;
	case DISTRIBUTIONTYPE_ParticleParam:
		*VectorDistPtr	= Variable->VectorDistVariable.VectorParam;
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

/**
 *	Finalize the variable array in the module properties
 *	INTERNAL FUNCTION
 */
UBOOL FinalizeModuleVariables(UParticleModule* Module, TArray<VariableDetermination>& Variables)
{
	UBOOL	bResult	= TRUE;

	for (INT VarIndex = 0; VarIndex < Variables.Num(); VarIndex++)
	{
		VariableDetermination* Variable = &(Variables(VarIndex));

		if (Variable)
		{
			switch (Variable->Type)
			{
			case VARIABLETYPE_Unknown:
				break;
			case VARIABLETYPE_Boolean:
				if (FinalizeVariableBoolean(Module, Variable) == FALSE)
				{
					warnf(TEXT("Failed to finalize UBOOL variable %32s for module %32s"),
						*Variable->Prop->GetName(), *Module->GetName());
					bResult = FALSE;
				}
				break;
			case VARIABLETYPE_Float:
				if (FinalizeVariableFloat(Module, Variable) == FALSE)
				{
					warnf(TEXT("Failed to finalize FLOAT variable %32s for module %32s"),
						*Variable->Prop->GetName(), *Module->GetName());
					bResult = FALSE;
				}
				break;
			case VARIABLETYPE_FColor:
				if (FinalizeVariableFColor(Module, Variable) == FALSE)
				{
					warnf(TEXT("Failed to finalize FColor variable %32s for module %32s"),
						*Variable->Prop->GetName(), *Module->GetName());
					bResult = FALSE;
				}
				break;
			case VARIABLETYPE_FVector:
				if (FinalizeVariableFVector(Module, Variable) == FALSE)
				{
					warnf(TEXT("Failed to finalize FVector variable %32s for module %32s"),
						*Variable->Prop->GetName(), *Module->GetName());
					bResult = FALSE;
				}
				break;
			case VARIABLETYPE_FloatDist:
				if (FinalizeVariableFloatDist(Module, Variable) == FALSE)
				{
					warnf(TEXT("Failed to finalize FloatDist variable %32s for module %32s"),
						*Variable->Prop->GetName(), *Module->GetName());
					bResult = FALSE;
				}
				break;
			case VARIABLETYPE_VectorDist:
				if (FinalizeVariableVectorDist(Module, Variable) == FALSE)
				{
					warnf(TEXT("Failed to VectorDist boolean variable %32s for module %32s"),
						*Variable->Prop->GetName(), *Module->GetName());
					bResult = FALSE;
				}
				break;
			default:
				warnf(TEXT("Module conversion error!"));
				bResult	= FALSE;
				break;
			}
		}
	}

	Module->bEditable	= FALSE;

	return bResult;
}

/**
 *	GenerateLODModuleValues
 *	Default implementation.
 *	Function is intended to generate the required values by interpolating between the given
 *	high and low modules using the given percentage.
 *	
 *	@param	TargetModule	The module to generate the values for
 *	@param	HighModule		The higher values source module
 *	@param	LowModule		The lower values source module
 *	@param	Percentage		The percentage of the source values to set
 *
 *	@return	TRUE	if successful
 *			FALSE	if failed
 */
UBOOL WxCascade::GenerateLODModuleValues(UParticleModule* TargetModule, UParticleModule* HighModule, UParticleModule* LowModule, FLOAT Percentage)
{
#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
	debugf(TEXT("Generating LOD module values for module %s"), *TargetModule->GetName());
#endif	//#if defined(_CASCADE_LOD_GENERATION_DEBUG_)

	check(TargetModule);
	check(HighModule);
	check(LowModule);
	check(TargetModule->StaticClass() == HighModule->StaticClass());
	check(TargetModule->StaticClass() == LowModule->StaticClass());

	FLOAT	HighMultiplier	= Percentage / 100.0f;
	FLOAT	LowMultiplier	= 1.0f - HighMultiplier;

	LowMultiplier	= (LowMultiplier < 0.0f) ? 0.0f : LowMultiplier;

	TArray<VariableDetermination>	TargetVariables;
	TArray<VariableDetermination>	HighVariables;
	TArray<VariableDetermination>	LowVariables;

	// Fill the value arrays
	FillModuleVariableArray(TargetModule, TargetVariables);
	FillModuleVariableArray(HighModule, HighVariables);
	FillModuleVariableArray(LowModule, LowVariables);

	if ((TargetVariables.Num() != HighVariables.Num()) || 
		(TargetVariables.Num() != LowVariables.Num()))
	{
		warnf(TEXT("Module %32s --> Invalid variable counts!"), *TargetModule->GetFullName());
		return FALSE;
	}

	INT VarIndex;

	for (VarIndex = 0; VarIndex < TargetVariables.Num(); VarIndex++)
	{
		VariableDetermination* TargetVariable	= &(TargetVariables(VarIndex));
		VariableDetermination* HighVariable		= &(HighVariables(VarIndex));
		VariableDetermination* LowVariable		= &(LowVariables(VarIndex));

#if defined(_CASCADE_LOD_GENERATION_DEBUG_)
		debugf(TEXT("Property %3d - %32s, %32s, %32s"), VarIndex, 
			GetVariableTypeString(TargetVariable->Type), 
			GetVariableTypeString(HighVariable->Type), 
			GetVariableTypeString(LowVariable->Type));
#endif	//#if defined(_CASCADE_LOD_GENERATION_DEBUG_)

		GenerateTargetVariable(TargetVariable, HighVariable, LowVariable, Percentage);
	}

	// Set the actual property values
	return FinalizeModuleVariables(TargetModule, TargetVariables);
}
