/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionStaticBoolParameter extends MaterialExpressionParameter
	native(Material)
	collapsecategories
	hidecategories(Object);

var() bool	DefaultValue;
var() bool	ExtendedCaptionDisplay;

//the override that will be set when this expression is being compiled from a static permutation
var const native transient pointer InstanceOverride{const FStaticSwitchParameter};

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual INT CompilePreview(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;
	virtual void SetStaticParameterOverrides(const FStaticParameterSet* Permutation);
	virtual void ClearStaticParameterOverrides();
}

defaultproperties
{
	bUsedByStaticParameterSet=true

	MenuCategories(0)="Parameters"
}
