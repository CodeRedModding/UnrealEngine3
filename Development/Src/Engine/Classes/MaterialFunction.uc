/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * Material Function - a collection of material expressions that can be reused in different materials
 */
class MaterialFunction extends Object
	native(Material)
	hidecategories(object);

/** Used by materials using this function to know when to recompile. */
var duplicatetransient guid StateId;

/** Used in the material editor, points to the function asset being edited, which this function is just a preview for. */
var transient editoronly MaterialFunction ParentFunction;

/** Description of the function which will be displayed as a tooltip wherever the function is used. */
var() string Description;

/** Whether to list this function in the material function library, which is a window in the material editor that lists categorized functions. */
var() bool bExposeToLibrary;

/** 
 * Categories that this function belongs to in the material function library.  
 * Ideally categories should be chosen carefully so that there are not too many.
 */
var() array<string> LibraryCategories;

/** Array of material expressions, excluding Comments.  Used by the material editor. */
var array<MaterialExpression> FunctionExpressions;

/** Array of comments associated with this material; viewed in the material editor. */
var editoronly array<MaterialExpressionComment> FunctionEditorComments;

/** Transient flag used to track re-entrance in recursive functions like IsDependent. */
var private const transient bool bReentrantFlag;

cpptext
{
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostLoad();

	/** Recursively updates all function call expressions in this function, or in nested functions. */
	void UpdateFromFunctionResource();

	/** Gets the inputs and outputs that this function exposes, for a function call expression to use. */
	void GetInputsAndOutputs(TArray<FFunctionExpressionInput>& OutInputs, TArray<FFunctionExpressionOutput>& OutOutputs) const;

	INT Compile(FMaterialCompiler* Compiler, const FFunctionExpressionOutput& Output, const TArray<FFunctionExpressionInput>& Inputs);

	/** Returns TRUE if this function is dependent on the passed in function, directly or indirectly. */
	UBOOL IsDependent(UMaterialFunction* OtherFunction);

	/** Returns an array of the functions that this function is dependent on, directly or indirectly. */
	void GetDependentFunctions(TArray<UMaterialFunction*>& DependentFunctions) const;
};

defaultproperties
{
	LibraryCategories(0)="Misc"
}
