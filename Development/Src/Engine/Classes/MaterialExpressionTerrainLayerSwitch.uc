/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionTerrainLayerSwitch extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

//the override that will be set when this expression is being compiled from a static permutation
var const native transient pointer InstanceOverride{const FStaticTerrainLayerWeightParameter};

var ExpressionInput	LayerUsed;
var ExpressionInput	LayerNotUsed;

var() Name ParameterName;
var() bool PreviewUsed;

/** GUID that should be unique within the material, this is used for parameter renaming. */
var	  const	guid	ExpressionGUID;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;
	
	/**
	 * MatchesSearchQuery: Check this expression to see if it matches the search query
	 * @param SearchQuery - User's search query (never blank)
	 * @return TRUE if the expression matches the search query
     */
	virtual UBOOL MatchesSearchQuery( const TCHAR* SearchQuery );

	/** 
	 * Generates a GUID for this expression if one doesn't already exist. 
	 *
	 * @param bForceGeneration	Whether we should generate a GUID even if it is already valid.
	 */
	void ConditionallyGenerateGUID(UBOOL bForceGeneration=FALSE);

	/** Tries to generate a GUID. */
	virtual void PostLoad();

	/** Tries to generate a GUID. */
	virtual void PostDuplicate();

	/** Tries to generate a GUID. */
	virtual void PostEditImport();

#if WITH_EDITOR
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
	PreviewUsed=true
}
