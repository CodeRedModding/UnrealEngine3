/**
 * MaterialEditorInstanceConstant.uc: This class is used by the material instance editor to hold a set of inherited parameters which are then pushed to a material instance.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialEditorInstanceConstant extends Object
	native
	hidecategories(Object)
	dependson(UnrealEdTypes)
	collapsecategories;

 struct native EditorParameterGroup
 {
	var name  GroupName;
	var() const editfixedsize editinline array<DEditorParameterValue> Parameters;
 };

struct native EditorParameterValue
{
	var() bool			bOverride;
	var() name			ParameterName;
	var   Guid			ExpressionId;
};

struct native EditorVectorParameterValue extends EditorParameterValue
{
	var() LinearColor	ParameterValue;
};

struct native EditorScalarParameterValue extends EditorParameterValue
{
	var() float		ParameterValue;
};

struct native EditorTextureParameterValue extends EditorParameterValue
{
    var() Texture	ParameterValue;
};

struct native EditorFontParameterValue extends EditorParameterValue
{
    var() Font		FontValue;
	var() int		FontPage;
};

struct native EditorStaticSwitchParameterValue extends EditorParameterValue
{
    var() bool		ParameterValue;

structcpptext
{
	/** Constructor */
	FEditorStaticSwitchParameterValue(const FStaticSwitchParameter& InParameter) : ParameterValue(InParameter.Value)
	{
		//initialize base class members
		bOverride = InParameter.bOverride;
		ParameterName = InParameter.ParameterName;
		ExpressionId = InParameter.ExpressionGUID;
	}
}
};

struct native ComponentMaskParameter
{
	var() bool R;
	var() bool G;
	var() bool B;
	var() bool A;

structcpptext
{
	/** Constructor */
	FComponentMaskParameter(UBOOL InR, UBOOL InG, UBOOL InB, UBOOL InA) :
		R(InR),
		G(InG),
		B(InB),
		A(InA)
	{
	}
}
};

struct native EditorStaticComponentMaskParameterValue extends EditorParameterValue
{
    var() ComponentMaskParameter		ParameterValue;

structcpptext
{
	/** Constructor */
	FEditorStaticComponentMaskParameterValue(const FStaticComponentMaskParameter& InParameter) : ParameterValue(InParameter.R, InParameter.G, InParameter.B, InParameter.A)
	{
		//initialize base class members
		bOverride = InParameter.bOverride;
		ParameterName = InParameter.ParameterName;
		ExpressionId = InParameter.ExpressionGUID;
	}
}
};

/** Physical material to use for this graphics material. Used for sounds, effects etc.*/
var() PhysicalMaterial									PhysMaterial;

/** Physical material mask settings to use. */
var() PhysicalMaterialMaskSettings PhysicalMaterialMask;

// since the Parent may point across levels and the property editor needs to import this text, it must be marked crosslevel so it doesn't set itself to NULL in FindImportedObject
var() crosslevelpassive MaterialInterface				Parent;

var() editfixedsize editinline array<EditorParameterGroup> ParameterGroups;

/** Mobile parameters */
var() editfixedsize editinline array<EditorParameterGroup> MobileParameterGroups;

var	  MaterialInstanceConstant							SourceInstance;
var const transient duplicatetransient	  array<Guid>	VisibleExpressions;

var deprecated texture									FlattenedTexture;
/** Mobile base (diffuse) texture override */
var deprecated texture MobileBaseTexture;

/** Mobile emissive texture override.  For emissive to be visible, the base material's emissive color source must be set to 'Emissive Texture' */
var deprecated texture MobileEmissiveTexture;

/** Detail texture to use for blending the base texture */
var deprecated texture MobileDetailTexture;

/** Spherical environment map texture.  When specified, spherical environment mapping will be enabled for this material. */
var deprecated texture MobileEnvironmentTexture;

/** Normal map texture.  If specified, this enables per pixel lighting when used in combination with other material features. */
var deprecated texture MobileNormalTexture;

/** General purpose mask texture used for bump offset amount, texture blending, etc. */
var deprecated texture MobileMaskTexture;


/** The Lightmass override settings for this object. */
var(Lightmass)	LightmassParameterizedMaterialSettings	LightmassSettings;

/** Should we use old style typed arrays for unassigned parameters instead of a None group (new style)? */
var() bool bUseOldStyleMICEditorGroups;

cpptext
{
	/**Fix up for deprecated properties*/
	virtual void PostLoad();

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
	void SetSourceInstance(UMaterialInstanceConstant* MaterialInterface);

	/** 
	 *  Returns group for parameter. Creates one if needed. 
	 *
	 * @param	InParameterGroupName	Name to be looked for.
	 * @param	InParameterGroups		The array of groups to look in
	 */
	FEditorParameterGroup& GetParameterGroup(FName& InParameterGroupName, TArrayNoInit<struct FEditorParameterGroup>& InParameterGroups);

	/** 
	 *  Creates/adds value to group retrieved from parent material . 
	 *
	 * @param ParentMaterial		Name of material to search for groups.
	 * @param ParameterValue		Current data to be grouped
	 */
	void AssignParameterToGroup(UMaterial* ParentMaterial, UDEditorParameterValue * ParameterValue);

	/** Regenerates the mobile parameter arrays. */
	void RegenerateMobileArrays();

	/** 
	 *	Generate the mobile parameter entries for the given group.
	 *
	 *	@param	InGroupName		The group to generate
	 */
	UBOOL GenerateMobileParameterEntries(FName& InGroupName);

	/** Copies the mobile parameter array values back to the source instance. */
	void CopyMobileParametersToSourceInstance();
}
