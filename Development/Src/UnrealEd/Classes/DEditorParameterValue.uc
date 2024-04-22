/**
 * MaterialEditorInstanceConstant.uc: This base class for material instance editor parameter represenation .
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DEditorParameterValue extends Object
	native
	hidecategories(Object)
	dependson(UnrealEdTypes)
	collapsecategories
	editinlinenew;

var() bool			bOverride;
var() name			ParameterName;
var   Guid			ExpressionId;
