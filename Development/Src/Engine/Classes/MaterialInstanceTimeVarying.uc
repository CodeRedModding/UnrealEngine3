/**
 *  When adding new functionality to this you will (sadly) need to touch a number of places:
 *
 *  MaterialInstanceTimeVarying.uc  for the actual data that will be used in the game
 *  MaterialEditorInstanceTimeVarying.uc for the editor property dialog that will be used to edit the data you just added
 *  
 * MaterialInstanceTimeVaryingHelpers.h  void UMaterialEditorInstanceTimeVarying::CopyToSourceInstance()
 *     template< typename MI_TYPE, typename ARRAY_TYPE >    (this copies
 *     void UpdateParameterValueOverTimeValues(
 *
 * MaterialInstanceTimeVaryingEditor.cpp  void UMaterialEditorInstanceTimeVarying::RegenerateArrays()  (each of the different types when it sets each param (ughh!))
 *  
 * MaterialInstanceTimeVarying.cpp  void UMaterialInstanceTimeVarying::Set___   (to set the default values)
 *
 * MaterialInstanceConstant.cpp  static void UpdateMICResources(UMaterialInstanceTimeVarying* Instance)   (to send the data over to the rendering thread (if it needs it) (hopefully most data can be encapsulated in the 'struct native ParameterValueOverTime' which all of the specialized data structs derrive from
 *
 * MaterialInstance.h struct FTimeVaryingDataTypeBase 
 *
 * BaseEditior.ini  need to look at the [UnrealEd.CustomPropertyItemBindings] and add a new entry for any new types that you create
 *                 (i.e. if you want a specialized PropertySheet view/editing)
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialInstanceTimeVarying extends MaterialInstance
	native(Material);



struct native ParameterValueOverTime
{
	var guid ExpressionGUID;

	/** when this is parameter is to start "ticking" then this value will be set to the current game time **/
	var float StartTime;

	var() name ParameterName;

	/** if true, then the CycleTime is the loop time and time loops **/
	var() bool bLoop;

	/** This will auto activate this param **/
	var() bool bAutoActivate;

	/** this controls time normalization and the loop time **/
	var() float	CycleTime;

	/** if true, then the CycleTime is used to scale time so all keys are between zero and one **/
	var() bool bNormalizeTime;

	/** How much time this will wait before actually firing off.  This is useful for keeping the curves being just the data for controlling the param and not a bunch of slack in the beginning (e.g. to wait N seconds then start fading) **/
	var() float OffsetTime;

	/** When using OffsetTime it is nice to be able to offset from the end of the decal's lifetime (e.g. you want to fade out the decal, you want to change the color of the decal before it fades away etc.) **/
	var() bool bOffsetFromEnd; 

	structcpptext
	{
		/** Constructors */
		FParameterValueOverTime() {}
		FParameterValueOverTime(EEventParm)
		{
			appMemzero(this, sizeof(FParameterValueOverTime));
		}
		void InitToDefaults()
		{
			appMemzero(this, sizeof(FParameterValueOverTime));
			StartTime=-1.0f;
			bLoop=FALSE;
			bAutoActivate=FALSE;
			CycleTime=1.0f;
			bNormalizeTime=FALSE;
		}
		FParameterValueOverTime(ENativeConstructor)
		{
			InitToDefaults();
		}
	}

	structdefaultproperties
	{
		StartTime=-1.0f
		bLoop=FALSE
		bAutoActivate=FALSE
		CycleTime=1.0f
		bNormalizeTime=FALSE
	}
};


struct native FontParameterValueOverTime extends ParameterValueOverTime
{
	var() Font		FontValue;
	var() int		FontPage;
};

struct native ScalarParameterValueOverTime extends ParameterValueOverTime
{
	/** This allows MITVs to have both single scalar and curve values **/
	var() float	ParameterValue;

	/** This will automatically be used if there are any values in this Curve **/
	var() InterpCurveFloat ParameterValueCurve;
};

struct native TextureParameterValueOverTime extends ParameterValueOverTime
{
	var() Texture	ParameterValue;
};

struct native VectorParameterValueOverTime extends ParameterValueOverTime
{
	var() LinearColor	ParameterValue;

	/** This will automatically be used if there are any values in this Curve **/
	var() InterpCurveVector ParameterValueCurve;
};

struct native LinearColorParameterValueOverTime extends ParameterValueOverTime
{
	var() LinearColor	ParameterValue;

	/** This will automatically be used if there are any values in this Curve **/
	var() InterpCurveLinearColor ParameterValueCurve;
};

/** causes all parameters to start playing immediately **/
var() bool bAutoActivateAll;

/** This sets how long the MITV will be around (i.e. this MITV is owned by a decal which lasts N seconds).  It is used for bOffsetFromEnd functionality **/
var transient float Duration;

var() array<FontParameterValueOverTime>		FontParameterValues;
var() array<ScalarParameterValueOverTime>		ScalarParameterValues;
var() array<TextureParameterValueOverTime>	TextureParameterValues;
var() array<VectorParameterValueOverTime>		VectorParameterValues;
/** LinearColor Values apply to Vector Parameters allowing modification of RGBA values **/
var() array<LinearColorParameterValueOverTime>	LinearColorParameterValues;
cpptext
{
	// Constructor.
	UMaterialInstanceTimeVarying();

	// UMaterialInstance interface.
	virtual void InitResources();

	// UMaterialInterface interface.
	virtual UBOOL GetFontParameterValue(FName ParameterName,class UFont*& OutFontValue, INT& OutFontPage);
	/**
	 * For MITVs you can utilize both single Scalar values and InterpCurve values.
	 *
	 * If there is any data in the InterpCurve, then the MITV will utilize that. Else it will utilize the Scalar value
	 * of the same name.
	 **/
	virtual UBOOL GetScalarParameterValue(FName ParameterName,FLOAT& OutValue);
	virtual UBOOL GetScalarCurveParameterValue(FName ParameterName,FInterpCurveFloat& OutValue);
	virtual UBOOL GetTextureParameterValue(FName ParameterName,class UTexture*& OutValue);
	virtual UBOOL GetVectorParameterValue(FName ParameterName,FLinearColor& OutValue);
	virtual UBOOL GetVectorCurveParameterValue(FName ParameterName,FInterpCurveVector &OutValue);
	virtual UBOOL GetLinearColorParameterValue(FName ParameterName,FLinearColor& OutValue);
	virtual UBOOL GetLinearColorCurveParameterValue(FName ParameterName,FInterpCurveLinearColor &OutValue);

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
};



// SetParent - Updates the parent.

native function SetParent(MaterialInterface NewParent);

// Set*ParameterValue - Updates the entry in ParameterValues for the named parameter, or adds a new entry.

/**
 * For MITVs you can utilize both single Scalar values and InterpCurve values.
 *
 * If there is any data in the InterpCurve, then the MITV will utilize that. Else it will utilize the Scalar value
 * of the same name.
 **/
native function SetScalarParameterValue(name ParameterName, float Value);
native function SetScalarCurveParameterValue(name ParameterName, const out InterpCurveFloat Value);

/** This sets how long after the MITV has been spawned to start "ticking" the named Scalar InterpCurve **/
native function SetScalarStartTime(name ParameterName, float Value);

/** This sets how long the MITV will be around (i.e. this MITV is owned by a decal which lasts N seconds) **/
native function SetDuration(float Value);


native function SetTextureParameterValue(name ParameterName, Texture Value);
native function SetVectorParameterValue(name ParameterName, const out LinearColor Value);
native function SetVectorCurveParameterValue(name ParameterName, const out InterpCurveVector Value);
native function SetLinearColorParameterValue(name ParameterName, const out LinearColor Value);
native function SetLinearColorCurveParameterValue(name ParameterName, const out InterpCurveLinearColor Value);

/** This sets how long after the MITV has been spawned to start "ticking" the named Scalar InterpCurve **/
native function SetVectorStartTime(name ParameterName, float Value);

/**
* Sets the value of the given font parameter.
*
* @param	ParameterName	The name of the font parameter
* @param	OutFontValue	New font value to set for this MIC
* @param	OutFontPage		New font page value to set for this MIC
*/
native function SetFontParameterValue(name ParameterName, Font FontValue, int FontPage);

/** Removes all parameter values */
native function ClearParameterValues();

/** This will interrogate all of the parameter and see what the max duration needed for them is.  Useful for setting the Duration / or knowing how long this MITV will take **/
native function float GetMaxDurationFromAllParameters();

//**This will check for conflict between LinearColor and Vector Parameters **/
native function bool CheckForVectorParameterConflicts(name ParameterName);

defaultproperties
{
	bAutoActivateAll=FALSE
	Duration=0.0f
}
