/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpression extends Object
	native(Material)
	abstract
	hidecategories(Object);

//@warning: FExpressionInput is mirrored in MaterialShared.h and manually "subclassed" in Material.uc (FMaterialInput)
struct ExpressionInput
{
	/** Material expression that this input is connected to, or NULL if not connected. */
	var MaterialExpression	Expression;

	/** Index into Expression's outputs array that this input is connected to. */
	var int					OutputIndex;

	/** 
	 * Optional name of the input.  
	 * Note that this is the only member which is not derived from the output currently connected. 
	 */
	var string				InputName;
	var int					Mask,
							MaskR,
							MaskG,
							MaskB,
							MaskA;
	var int					GCC64_Padding; // @todo 64: if the C++ didn't mismirror this structure (with MaterialInput), we might not need this
};

/** Struct that represents an expression's output. */
struct ExpressionOutput
{
	var string				OutputName;
	var int					Mask,
							MaskR,
							MaskG,
							MaskB,
							MaskA;
};

/** This variable is conlficting with Materia var, making new ones (MaterialExpressionEditor), and then deprecating this **/
var deprecated int	EditorX,
					EditorY;

var editoronly int		MaterialExpressionEditorX,
						MaterialExpressionEditorY;

/** Set to TRUE by RecursiveUpdateRealtimePreview() if the expression's preview needs to be updated in realtime in the material editor. */
var bool					bRealtimePreview;

/** If TRUE, we should update the preview next render. This is set when changing bRealtimePreview. */
var transient bool			bNeedToUpdatePreview;

/** Indicates that this is a 'parameter' type of expression and should always be loaded (ie not cooked away) because we might want the default parameter. */
var bool					bIsParameterExpression;

/** 
 * The material that this expression is currently being compiled in.  
 * This is not necessarily the object which owns this expression, for example a preview material compiling a material function's expressions.
 */
var const Material Material;

/** 
 * The material function that this expression is being used with, if any.
 * This will be NULL if the expression belongs to a function that is currently being edited, 
 */
var const MaterialFunction Function;

/** A description that level designers can add (shows in the material editor UI). */
var() string				Desc;

/** Color of the expression's border outline. */
var color BorderColor;

/** If TRUE, use the output name as the label for the pin */
var bool bShowOutputNameOnPin;
/** If TRUE, do not render the preview window for the expression */
var bool bHidePreviewWindow;

/** Whether to draw the expression's inputs. */
var bool bShowInputs;

/** Whether to draw the expression's outputs. */
var bool bShowOutputs;

/** Categories to sort this expression into... */
var array<name>	MenuCategories;

/** The expression's outputs, which are set in default properties by derived classes. */
var array<ExpressionOutput> Outputs;

/** 
 *	If TRUE, this expression is used when generating the StaticParameterSet.
 *	It is important to set this correctly if the cooker is using the CleanupMaterials functionality.
 *	If it is not set correctly, the cleanup code will remove the expression and the StaticParameterSet
 *	will mismatch when verifying the shader map.
 *	The ClearInputExpression function should also be implement on expressions that set this as it
 *	will be called by the CleanupMaterials function to remove unrequired expressions.
 */
var bool bUsedByStaticParameterSet;

cpptext
{
	// UObject interface.
	virtual void PostLoad();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// UMaterialExpression interface.

	/**
	 * Replaces references to the passed in expression with references to a different expression or NULL.
	 * @param	OldExpression		Expression to find reference to.
	 * @param	NewExpression		Expression to replace reference with.
	 */
	virtual void SwapReferenceTo(UMaterialExpression* OldExpression,UMaterialExpression* NewExpression = NULL) {}

	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex) { return INDEX_NONE; }
	virtual INT CompilePreview(FMaterialCompiler* Compiler, INT OutputIndex) { return Compile(Compiler, OutputIndex); }
	virtual TArray<FExpressionOutput>& GetOutputs();
	virtual const TArray<FExpressionInput*> GetInputs();
	virtual FExpressionInput* GetInput(INT InputIndex);
	virtual FString GetInputName(INT InputIndex) const;
	virtual UBOOL IsInputConnectionRequired(INT InputIndex) const { return TRUE; }
	virtual INT GetWidth() const;
	virtual INT GetHeight() const;
	virtual UBOOL UsesLeftGutter() const;
	virtual UBOOL UsesRightGutter() const;
	virtual FString GetCaption() const;

	/** Gets a tooltip for the specified connector. */
	virtual void GetConnectorToolTip(INT InputIndex, INT OutputIndex, TArray<FString>& OutToolTip);

	/** Gets a tooltip for the expression itself. */
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) {}

	virtual int GetLabelPadding() { return 0; }

	virtual INT CompilerError(FMaterialCompiler* Compiler, const TCHAR* pcMessage);

	virtual void Serialize(FArchive& Ar);

	/**
	 * @return TRUE if the expression preview needs realtime update
     */
	virtual UBOOL NeedsRealtimePreview() { return FALSE; }

	/**
	 * MatchesSearchQuery: Check this expression to see if it matches the search query
	 * @param SearchQuery - User's search query (never blank)
	 * @return TRUE if the expression matches the search query
     */
	virtual UBOOL MatchesSearchQuery( const TCHAR* SearchQuery );

	/** Called before applying a transaction to the object.  Default implementation simply calls PreEditChange. */
	virtual void PreEditUndo();

	/** Called after applying a transaction to the object.  Default implementation simply calls PostEditChange. */
	virtual void PostEditUndo();

#if WITH_EDITOR
	/**
	 *	Called by the CleanupMaterials function, this will clear the inputs of the expression.
	 *	This only needs to be implemented by expressions that have bUsedByStaticParameterSet set to TRUE.
	 */
	virtual void ClearInputExpressions() {}
#endif

	/**
	 * Sets overrides in the material expression's static parameters
	 *
	 * @param	Permutation		The set of static parameters to override and their values	
	 */
	virtual void SetStaticParameterOverrides(const FStaticParameterSet* Permutation) {}

	/**
	 * Clears static parameter overrides so that static parameter expression defaults will be used
	 *	for subsequent compiles.
	 */
	virtual void ClearStaticParameterOverrides() {}

	/**
	 * Copies the SrcExpressions into the specified material.  Preserves internal references.
	 * New material expressions are created within the specified material.
	 */
	static void CopyMaterialExpressions(const TArray<class UMaterialExpression*>& SrcExpressions, const TArray<class UMaterialExpressionComment*>& SrcExpressionComments, 
		class UMaterial* Material, class UMaterialFunction* Function, TArray<class UMaterialExpression*>& OutNewExpressions, TArray<class UMaterialExpression*>& OutNewComments);
}

defaultproperties
{
	Outputs(0)=(OutputName="")
	bShowInputs=true
	bShowOutputs=true
}