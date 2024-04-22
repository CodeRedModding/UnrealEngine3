/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionFunctionInput extends MaterialExpression
	native(Material)
	hidecategories(object);

/** Used for previewing when editing the function, also temporarily used to connect to the outside material when compiling that material. */
var ExpressionInput Preview;

/** The input's name, which will be drawn on the connector in function call expressions that use this function. */
var() string InputName;

/** The input's description, which will be used as a tooltip on the connector in function call expressions that use this function. */
var() string Description;

/** Id of this input, used to maintain references through name changes. */
var const guid Id;

/** Supported input types */
enum EFunctionInputType
{
	FunctionInput_Scalar,
	FunctionInput_Vector2,
	FunctionInput_Vector3,
	FunctionInput_Vector4,
	FunctionInput_Texture2D,
	FunctionInput_TextureCube,
	FunctionInput_StaticBool
};

/** 
 * Type of this input.  
 * Input code chunks will be cast to this type, and a compiler error will be emitted if the cast fails.
 */
var() EFunctionInputType InputType;

/** Value used to preview this input when editing the material function. */
var() vector4 PreviewValue;

/** Whether to use the preview value or texture as the default value for this input. */
var() bool bUsePreviewValueAsDefault;

/** Controls where the input is displayed relative to the other inputs. */
var() int SortPriority;

/** 
 * TRUE when this expression is being compiled in a function preview, 
 * FALSE when this expression is being compiled into a material that uses the function.
 * Only valid in Compile()
 */
var transient bool bCompilingFunctionPreview;

cpptext
{
	// UObject interface
	virtual void PostLoad();
	virtual void PostDuplicate();
	virtual void PostEditImport();
	virtual void PreEditChange(UProperty* PropertyAboutToChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual UBOOL CanEditChange( const UProperty* InProperty ) const;

	// UMaterialExpression interface
	virtual FString GetCaption() const;
	/**
	 * MatchesSearchQuery: Check this expression to see if it matches the search query
	 * @param SearchQuery - User's search query (never blank)
	 * @return TRUE if the expression matches the search query
     */
	virtual UBOOL MatchesSearchQuery( const TCHAR* SearchQuery );
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip);
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual INT CompilePreview(FMaterialCompiler* Compiler, INT OutputIndex);

	/**
	 * Replaces references to the passed in expression with references to a different expression or NULL.
	 * @param	OldExpression		Expression to find reference to.
	 * @param	NewExpression		Expression to replace reference with.
	 */
	virtual void SwapReferenceTo(UMaterialExpression* OldExpression,UMaterialExpression* NewExpression = NULL);

	/** Generates the Id for this input. */
	void ConditionallyGenerateId(UBOOL bForce);

	/** Validates InputName.  Must be called after InputName is changed to prevent duplicate inputs. */
	void ValidateName();

private:

	/** Helper function which compiles this expression for previewing. */
	INT CompilePreviewValue(FMaterialCompiler* Compiler);
};

defaultproperties
{
	bCompilingFunctionPreview=True
	InputType=FunctionInput_Vector3
	InputName="In"
	MenuCategories(0)="Functions"
	BorderColor=(R=185,G=255,B=172)
}
