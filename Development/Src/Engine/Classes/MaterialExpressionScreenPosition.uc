/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionScreenPosition extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

/** applies the divide by w as well as [-1,1]->[1,1] mapping for screen alignment */
var() bool ScreenAlign;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;
}

defaultproperties
{
	MenuCategories(0)="Coordinates"
}
