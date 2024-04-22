/**
 * MaterialEditorInstanceTimeVaryingTimeVarying.uc: This class is used by the material instance editor to hold a set of inherited parameters which are then pushed to a material instance.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialEditorInstanceTimeVarying extends Object
	native
	hidecategories(Object)
	collapsecategories;

struct native EditorParameterValueOverTime
{
	var guid ExpressionId;

	var() bool bOverride;
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
	var() bool bOffsetFromEnd; // end of decal's lifespan


	structdefaultproperties
	{
		bLoop=FALSE
		bAutoActivate=FALSE
		CycleTime=1.0f
		bNormalizeTime=FALSE
	}
};

struct native EditorVectorParameterValueOverTime extends EditorParameterValueOverTime
{
	/** This allows MITVs to have both single scalar and curve values **/
	var() LinearColor ParameterValue;

	/** This will automatically be used if there are any values in this Curve **/
	var() InterpCurveVector ParameterValueCurve; 
};

struct native EditorLinearColorParameterValueOverTime extends EditorParameterValueOverTime
{
	/** This allows MITVs to have both single scalar and curve values **/
	var() LinearColor ParameterValue;

	/** This will automatically be used if there are any values in this Curve **/
	var() InterpCurveLinearColor ParameterValueCurve; 
};

struct native EditorScalarParameterValueOverTime extends EditorParameterValueOverTime
{
	/** This allows MITVs to have both single scalar and curve values **/
	var() float ParameterValue;

	/** This will automatically be used if there are any values in this Curve **/
	var() InterpCurveFloat ParameterValueCurve;
};

struct native EditorTextureParameterValueOverTime extends EditorParameterValueOverTime
{
    var() Texture ParameterValue;
};

struct native EditorFontParameterValueOverTime extends EditorParameterValueOverTime
{
    var() Font FontValue;
	var() int FontPage;
};

struct native EditorStaticSwitchParameterValueOverTime extends EditorParameterValueOverTime
{
    var() bool ParameterValue;
};

struct native ComponentMaskParameterOverTime
{
	var() bool R;
	var() bool G;
	var() bool B;
	var() bool A;
};

struct native EditorStaticComponentMaskParameterValueOverTime extends EditorParameterValueOverTime
{
    var() ComponentMaskParameterOverTime ParameterValue;
};

/** Physical material to use for this graphics material. Used for sounds, effects etc.*/
var() PhysicalMaterial PhysMaterial;

/** Phsical material mask settings to use. */
var() PhysicalMaterialMaskSettings PhysicalMaterialMask;

var() MaterialInterface											Parent;


/** causes all parameters to start playing immediately **/
var() bool bAutoActivateAll;

/** LinearColor values apply to Vector Parameters allowing you to specify RGBA values **/
var() array<EditorLinearColorParameterValueOVerTime>			LinearColorParameterValues;

var() array<EditorVectorParameterValueOverTime>					VectorParameterValues;
var() array<EditorScalarParameterValueOverTime>					ScalarParameterValues;
var() array<EditorTextureParameterValueOverTime>				TextureParameterValues;
var() array<EditorFontParameterValueOverTime>					FontParameterValues;
var() array<EditorStaticSwitchParameterValueOverTime>			StaticSwitchParameterValues;
var() array<EditorStaticComponentMaskParameterValueOverTime>	StaticComponentMaskParameterValues;
var	  MaterialInstanceTimeVarying								SourceInstance;
var const transient duplicatetransient	  array<Guid>			VisibleExpressions;

/** The Lightmass override settings for this object. */
var(Lightmass)	LightmassParameterizedMaterialSettings	LightmassSettings;

cpptext
{
	// UObject interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/** Regenerates the parameter arrays. */
	void RegenerateArrays();

	/** Copies the parameter array values back to the source instance. */
	void CopyToSourceInstance();

	/** Copies static parameters to the source instance, which will be marked dirty if a compile was necessary */
	void CopyStaticParametersToSourceInstance();

	/** 
	 * Sets the source instance for this object and regenerates arrays. 
	 *
	 * @param MaterialInterface		Instance to use as the source for this material editor instance.
	 */
	void SetSourceInstance(UMaterialInstanceTimeVarying* MaterialInterface);
}


defaultproperties
{

}
