/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionCustomTexture extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

var() Texture		Texture;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual INT CompilePreview(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual INT GetWidth() const;
	virtual FString GetCaption() const;
	virtual INT GetLabelPadding() { return 8; }
	virtual UBOOL MatchesSearchQuery( const TCHAR* SearchQuery );
}

defaultproperties
{
	MenuCategories(0)="Custom"
}
