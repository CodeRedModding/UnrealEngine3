/**
 * MaterialEditorInstanceConstant.uc: This is derived class for material instance editor parameter represenation.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DEditorStaticComponentMaskParameterValue extends DEditorParameterValue
	native
	hidecategories(Object)
	dependson(UnrealEdTypes)
	collapsecategories;

struct native DComponentMaskParameter
{


structcpptext
{
	public:
	/** Constructor */
	FDComponentMaskParameter(UBOOL InR, UBOOL InG, UBOOL InB, UBOOL InA) :
		R(InR),
		G(InG),
		B(InB),
		A(InA)
	{
	};
	FDComponentMaskParameter(){};
}
	var() bool R;
	var() bool G;
	var() bool B;
	var() bool A;
};
var() DComponentMaskParameter		ParameterValue;
cpptext
{
	/** Constructor */
	UDEditorStaticComponentMaskParameterValue(const FStaticComponentMaskParameter& InParameter) : ParameterValue(InParameter.R, InParameter.G, InParameter.B, InParameter.A)
	{
		//initialize base class members
		bOverride = InParameter.bOverride;
		ParameterName = InParameter.ParameterName;
		ExpressionId = InParameter.ExpressionGUID;
	}
	NO_DEFAULT_CONSTRUCTOR(UDEditorStaticComponentMaskParameterValue)

}
