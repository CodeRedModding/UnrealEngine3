/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialInstanceConstant extends MaterialInstance
	native(Material)
	hidecategories(Object)
	collapsecategories;


struct native FontParameterValue
{
	var() name		ParameterName;
	var() Font		FontValue;
	var() int		FontPage;
	var	  guid		ExpressionGUID;
	
	structcpptext
	{
		UBOOL operator==(const FFontParameterValue& Other) const
		{
			return 
				ParameterName == Other.ParameterName &&
				FontValue == Other.FontValue &&
				FontPage == Other.FontPage &&
				ExpressionGUID == Other.ExpressionGUID;
		}
	}
};

struct native ScalarParameterValue
{
	var() name	ParameterName;
	var() float	ParameterValue;
	var	  guid	ExpressionGUID;
	
	structcpptext
	{
		UBOOL operator==(const FScalarParameterValue& Other) const
		{
			return 
				ParameterName == Other.ParameterName &&
				ParameterValue == Other.ParameterValue &&
				ExpressionGUID == Other.ExpressionGUID;
		}
	}
};

struct native TextureParameterValue
{
	var() name		ParameterName;
	var() Texture	ParameterValue;
	var	  guid		ExpressionGUID;
	
	structcpptext
	{
		UBOOL operator==(const FTextureParameterValue& Other) const
		{
			return 
				ParameterName == Other.ParameterName &&
				ParameterValue == Other.ParameterValue &&
				ExpressionGUID == Other.ExpressionGUID;
		}
	}
};

struct native VectorParameterValue
{
	var() name			ParameterName;
	var() LinearColor	ParameterValue;
	var	  guid			ExpressionGUID;
	
	structcpptext
	{
		UBOOL operator==(const FVectorParameterValue& Other) const
		{
			return 
				ParameterName == Other.ParameterName &&
				ParameterValue == Other.ParameterValue &&
				ExpressionGUID == Other.ExpressionGUID;
		}
	}
};



var() const array<FontParameterValue>		FontParameterValues;
var() const array<ScalarParameterValue>		ScalarParameterValues;
var() const array<TextureParameterValue>	TextureParameterValues;
var() const array<VectorParameterValue>		VectorParameterValues;


cpptext
{
	// Constructor.
	UMaterialInstanceConstant();

	// UMaterialInstance interface.
	virtual void InitResources();

	/**
	* Checks if any of the static parameter values are outdated based on what they reference (eg a normalmap has changed format)
	*
	* @param	EditorParameters	The new static parameters. 
	*/
	virtual void CheckStaticParameterValues(FStaticParameterSet* EditorParameters);

	// UMaterialInterface interface.
	virtual UBOOL GetFontParameterValue(FName ParameterName,class UFont*& OutFontValue, INT& OutFontPage);
	virtual UBOOL GetScalarParameterValue(FName ParameterName,FLOAT& OutValue);
	virtual UBOOL GetTextureParameterValue(FName ParameterName,class UTexture*& OutValue);
	virtual UBOOL GetVectorParameterValue(FName ParameterName,FLinearColor& OutValue);

	// UObject interface.
	virtual void PostLoad();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/**
	* Refreshes parameter names using the stored reference to the expression object for the parameter.
	*/
	virtual void UpdateParameterNames();

	/**
	 *	Cleanup the TextureParameter lists in the instance
	 *
	 *	@param	InRefdTextureParamsMap		Map of actual TextureParams used by the parent.
	 *
	 *	NOTE: This is intended to be called only when cooking for stripped platforms!
	 */
	virtual void CleanupTextureParameterReferences(const TMap<FName,UTexture*>& InRefdTextureParamsMap);

	/**
	 *	Setup the mobile properties for this instance
	 */
	virtual void SetupMobileProperties();

	/**
	 *	Set the mobile scalar parameter value to the given value.
	 *
	 *	@param	ParameterName		Name of the parameter to set
	 *	@param	InValue				The scalar value to set it to
	 */
	void SetMobileScalarParameterValue(FName& ParameterName, FLOAT InValue);
	/**
	 *	Set the mobile texture parameter value to the given value.
	 *
	 *	@param	ParameterName		Name of the parameter to set
	 *	@param	InValue				The texture value to set it to
	 */
	void SetMobileTextureParameterValue(FName& ParameterName, UTexture* InValue);
	/**
	 *	Set the mobile vector parameter value to the given value.
	 *
	 *	@param	ParameterName		Name of the parameter to set
	 *	@param	InValue				The vector value to set it to
	 */
	void SetMobileVectorParameterValue(FName& ParameterName, const FLinearColor& InValue);

	/**
	 * This will iterate over MICs in the world and find identical MICs and replace uses of the 
	 * duplicate ones to a single unique MIC. It's based on Parent, overriden parameters, and
	 * the level they are in. This only operates on transient or MICs in a level, it won't
	 * try to mess with content packages.
	 *
	 * @param NumFailuresToPrint If you are looking for a reason why some MICs don't get GC'd, specify a number greater than 0, and the function will tell you why that number aren't getting GC'd (via OBJ REFS)
	 */
	static void CollapseMICs(UINT NumFailuresToPrint=0);
};

// SetParent - Updates the parent.

native function SetParent(MaterialInterface NewParent);

// Set*ParameterValue - Updates the entry in ParameterValues for the named parameter, or adds a new entry.

native function SetScalarParameterValue(name ParameterName, float Value);
native function SetTextureParameterValue(name ParameterName, Texture Value);
native function SetVectorParameterValue(name ParameterName, const out LinearColor Value);

/**
* Sets the value of the given font parameter.  
*
* @param	ParameterName	The name of the font parameter
* @param	OutFontValue	New font value to set for this MIC
* @param	OutFontPage		New font page value to set for this MIC
*/
native function SetFontParameterValue(name ParameterName, Font FontValue, int FontPage);

native function bool GetMobileScalarParameterValue(name ParameterName, out float OutValue);
native function bool GetMobileTextureParameterValue(name ParameterName, out Texture OutValue);
native function bool GetMobileVectorParameterValue(name ParameterName, out LinearColor OutValue);

/** Removes all parameter values */
native function ClearParameterValues();


defaultproperties
{

}