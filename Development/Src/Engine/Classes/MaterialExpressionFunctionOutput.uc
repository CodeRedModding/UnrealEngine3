/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionFunctionOutput extends MaterialExpression
	native(Material)
	hidecategories(object);

/** The output's name, which will be drawn on the connector in function call expressions that use this function. */
var() string OutputName;

/** The output's description, which will be used as a tooltip on the connector in function call expressions that use this function. */
var() string Description;

/** Controls where the output is displayed relative to the other outputs. */
var() int SortPriority;

/** Stores the expression in the material function connected to this output. */
var ExpressionInput	A;

/** Whether this output was previewed the last time this function was edited. */
var bool bLastPreviewed;

/** Id of this input, used to maintain references through name changes. */
var const guid Id;

cpptext
{
	// UObject interface
	virtual void PostLoad();
	virtual void PostDuplicate();
	virtual void PostEditImport();
	virtual void PreEditChange(UProperty* PropertyAboutToChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// UMaterialExpression interface
	virtual FString GetCaption() const;
	/**
	 * MatchesSearchQuery: Check this expression to see if it matches the search query
	 * @param SearchQuery - User's search query (never blank)
	 * @return TRUE if the expression matches the search query
     */
	virtual UBOOL MatchesSearchQuery( const TCHAR* SearchQuery );
	virtual FString GetInputName(INT InputIndex) const
	{
		return TEXT("");
	}
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip);
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);

	/** Generates the Id for this input. */
	void ConditionallyGenerateId(UBOOL bForce);

	/** Validates OutputName.  Must be called after OutputName is changed to prevent duplicate outputs. */
	void ValidateName();
};

defaultproperties
{
	bShowOutputs=False
	OutputName="Result"
	MenuCategories(0)="Functions"
	BorderColor=(R=255,G=155,B=0)
}
