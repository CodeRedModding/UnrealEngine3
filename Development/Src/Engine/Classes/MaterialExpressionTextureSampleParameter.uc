/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionTextureSampleParameter extends MaterialExpressionTextureSample
	native(Material)
	collapsecategories
	abstract
	hidecategories(Object);

var()	name		ParameterName;

/** GUID that should be unique within the material, this is used for parameter renaming. */
var	  const	guid	ExpressionGUID;

/** The name of the parameter Group to display in MaterialInstance Editor. Default is None group */
var() name Group;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;

	virtual UBOOL TextureIsValid( UTexture* InTexture );
	virtual const TCHAR* GetRequirements();
	
	/**
	 *	Sets the default texture if none is set
	 */
	virtual void SetDefaultTexture();

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

	/**
	 * MatchesSearchQuery: Check this expression to see if it matches the search query
	 * @param SearchQuery - User's search query (never blank)
	 * @return TRUE if the expression matches the search query
     */
	virtual UBOOL MatchesSearchQuery( const TCHAR* SearchQuery );

	/**
	 * Called to get list of parameter names for static parameter sets
	 */
	void GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds);
}

defaultproperties
{
	bIsParameterExpression=true
	MenuCategories(0)="Obsolete"
}
