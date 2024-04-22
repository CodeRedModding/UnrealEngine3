/**
 * MaterialEditorInstanceConstant.uc: This is derived class for material instance editor parameter represenation.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DEditorStaticSwitchParameterValue extends DEditorParameterValue
	native
	hidecategories(Object)
	dependson(UnrealEdTypes)
	collapsecategories;

var() bool		ParameterValue;
cpptext
{
	/** Constructor */
	UDEditorStaticSwitchParameterValue(const FStaticSwitchParameter& InParameter) : ParameterValue(InParameter.Value)
	{
		//initialize base class members
		bOverride = InParameter.bOverride;
		ParameterName = InParameter.ParameterName;
		ExpressionId = InParameter.ExpressionGUID;
	}
	NO_DEFAULT_CONSTRUCTOR(UDEditorStaticSwitchParameterValue)
}
