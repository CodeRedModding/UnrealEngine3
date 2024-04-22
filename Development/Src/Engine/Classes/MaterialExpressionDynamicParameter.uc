/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 *	A material expression that routes particle emitter parameters to the
 *	material.
 */
class MaterialExpressionDynamicParameter extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

/** 
 *	The names of the parameters.
 *	These will show up in Cascade when editing a particle system
 *	that uses the material it is in...
 */
var()	editfixedsize	array<string>	ParamNames;

cpptext
{
	// UObject interface.

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// UMaterialExpression interface.

	/**
	 * Replaces references to the passed in expression with references to a different expression or NULL.
	 * @param	OldExpression		Expression to find reference to.
	 * @param	NewExpression		Expression to replace reference with.
	 */
	virtual void SwapReferenceTo(UMaterialExpression* OldExpression,UMaterialExpression* NewExpression = NULL);

    /**
	 * Creates the new shader code chunk needed for the Abs expression
	 *
	 * @param	Compiler - Material compiler that knows how to handle this expression
	 * @return	Index to the new FMaterialCompiler::CodeChunk entry for this expression
	 */	
	virtual INT Compile( FMaterialCompiler* Compiler, INT OutputIndex );

	/**
	 *	Get the outputs supported by this expression.
	 *
	 *	@param	Outputs		The TArray of outputs to fill in.
	 */
	virtual TArray<FExpressionOutput>& GetOutputs();

	/**
	 *	Get the width required by this expression (in the material editor).
	 *
	 *	@return	INT			The width in pixels.
	 */
	virtual INT GetWidth() const;

	/**
	 * Textual description for this material expression
	 *
	 * @return	Caption text
	 */	
	virtual FString GetCaption() const;

	/**
	 *	Returns the amount of padding to use for the label.
	 *
	 *	@return INT			The padding (in pixels).
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
	bShowOutputNameOnPin=true
	bHidePreviewWindow=true

	ParamNames(0)="Param1"
	ParamNames(1)="Param2"
	ParamNames(2)="Param3"
	ParamNames(3)="Param4"

	MenuCategories(0)="Particles"
	MenuCategories(1)="Parameters"

	Outputs(0)=(OutputName="",Mask=1,MaskR=1,MaskG=0,MaskB=0,MaskA=0)
	Outputs(1)=(OutputName="",Mask=1,MaskR=0,MaskG=1,MaskB=0,MaskA=0)
	Outputs(2)=(OutputName="",Mask=1,MaskR=0,MaskG=0,MaskB=1,MaskA=0)
	Outputs(3)=(OutputName="",Mask=1,MaskR=0,MaskG=0,MaskB=0,MaskA=1)
}
