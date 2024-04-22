/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionLandscapeLayerBlend extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

enum ELandscapeLayerBlendType
{
	LB_AlphaBlend,
	LB_HeightBlend,
};

struct native LayerBlendInput
{
	var() name LayerName;
	var() ELandscapeLayerBlendType BlendType;
	var edithide ExpressionInput LayerInput;
	var edithide ExpressionInput HeightInput;
	var() float PreviewWeight;
	//the override that will be set when this expression is being compiled from a static permutation
	var const native transient pointer InstanceOverride{const FStaticTerrainLayerWeightParameter};
};

var() array<LayerBlendInput> Layers;

/** GUID that should be unique within the material, this is used for parameter renaming. */
var	  const	guid	ExpressionGUID;

cpptext
{
	// UMaterialExpression interface
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;
	void SwapReferenceTo( UMaterialExpression* OldExpression, UMaterialExpression* NewExpression );
	virtual const TArray<FExpressionInput*> GetInputs();
	virtual FExpressionInput* GetInput(INT InputIndex);
	virtual FString GetInputName(INT InputIndex) const;

	/** GUID generation. */
	void ConditionallyGenerateGUID(UBOOL bForceGeneration=FALSE);
	virtual void PostLoad();
	virtual void PostDuplicate();
	virtual void PostEditImport();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/**
	 *	Called by the CleanupMaterials function, this will clear the inputs of the expression.
	 *	This only needs to be implemented by expressions that have bUsedByStaticParameterSet set to TRUE.
	 */
	virtual void ClearInputExpressions();
#endif

	/**
	 * Called to get list of parameter names for static parameter sets
	 */
	void GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds);

	/**
	 * Sets overrides in the material expression's static parameters
	 *
	 * @param	Permutation		The set of static parameters to override and their values	
	 */
	virtual void SetStaticParameterOverrides(const FStaticParameterSet* Permutation);

	/**
	 * Clears static parameter overrides so that static parameter expression defaults will be used
	 *	for subsequent compiles.
	 */
	virtual void ClearStaticParameterOverrides();
}

defaultproperties
{
	bIsParameterExpression=true
	bUsedByStaticParameterSet=true
	MenuCategories(0)="Terrain"
	MenuCategories(1)="WorldPosOffset"
}
