/**
 * MaterialEditorInstanceConstant.uc: This is derived class for material instance editor parameter represenation.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DEditorVectorParameterValue extends DEditorParameterValue
	native
	hidecategories(Object)
	dependson(UnrealEdTypes)
	collapsecategories
	editinlinenew;

var() LinearColor	ParameterValue;
