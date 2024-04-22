/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionFontSample extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

/** font resource that will be sampled */
var() Font Font;
/** allow access to the various font pages */
var() int FontTexturePage;

cpptext
{
	/** 
	* Generate the compiled material string for this expression
	* @param Compiler - shader material compiler
	* @return index to the new generated expression
	*/
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);

	/**
	* Width of the thumbnail for this expression int he material editor
	* @return size in pixels
	*/
	virtual INT GetWidth() const;

	/**
	* Caption description for this expression
	* @return string caption
	*/
	virtual FString GetCaption() const;

	/**
	* Padding for the text lable
	* @return size in pixels
	*/
	virtual INT GetLabelPadding() { return 8; }

	/**
	 * MatchesSearchQuery: Check this expression to see if it matches the search query
	 * @param SearchQuery - User's search query (never blank)
	 * @return TRUE if the expression matches the search query
     */
	virtual UBOOL MatchesSearchQuery( const TCHAR* SearchQuery );
}

defaultproperties
{
	MenuCategories(0)="Font"
	MenuCategories(1)="Texture"
	Outputs(0)=(OutputName="",Mask=1,MaskR=1,MaskG=1,MaskB=1,MaskA=0)
	Outputs(1)=(OutputName="",Mask=1,MaskR=1,MaskG=0,MaskB=0,MaskA=0)
	Outputs(2)=(OutputName="",Mask=1,MaskR=0,MaskG=1,MaskB=0,MaskA=0)
	Outputs(3)=(OutputName="",Mask=1,MaskR=0,MaskG=0,MaskB=1,MaskA=0)
	Outputs(4)=(OutputName="",Mask=1,MaskR=0,MaskG=0,MaskB=0,MaskA=1)
}
