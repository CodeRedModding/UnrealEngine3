class Material extends MaterialInstance
	native
	hidecategories(object)
	collapsecategories;

enum EBlendMode
{
	BLEND_Opaque,
	BLEND_Masked,
	BLEND_Translucent,
	BLEND_Additive
};

// Material input structs.

struct MaterialInput
{
	var MaterialExpression	Expression;
	var int					Mask,
							MaskR,
							MaskG,
							MaskB,
							MaskA;
	var bool				UseConstant;
};

struct ColorMaterialInput extends MaterialInput
{
	var color	Constant;
};

struct ScalarMaterialInput extends MaterialInput
{
	var float	Constant;
};

struct VectorMaterialInput extends MaterialInput
{
	var vector	Constant;
};

struct Vector2MaterialInput extends MaterialInput
{
	var float	ConstantX,
				ConstantY;
};

// Physics.

var() class<PhysicalMaterial>	PhysicalMaterial;

// Reflection.

var ColorMaterialInput		DiffuseColor;
var ColorMaterialInput		SpecularColor;
var ScalarMaterialInput		SpecularPower;
var VectorMaterialInput		Normal;
var() SphericalHarmonicMap	SHM;

// Emission.

var ColorMaterialInput		EmissiveColor;

// Transmission.

var ScalarMaterialInput		Opacity;
var ScalarMaterialInput		OpacityMask;

/** If BlendMode is BLEND_Masked, the surface is not rendered where OpacityMask < OpacityMaskClipValue. */
var() float					OpacityMaskClipValue;

var Vector2MaterialInput	Distortion;

var() EBlendMode			BlendMode;
var() bool					Unlit;

var ScalarMaterialInput		TwoSidedLightingMask;
var ColorMaterialInput		TwoSidedLightingColor;
var() bool					TwoSided;

var() bool					NonDirectionalLighting;

// Shader interfaces.
struct MaterialPointer
{
	var const native pointer P;
};

struct TextureBasePointer
{
	var const native pointer P;
};

var const native MaterialPointer			MaterialResources[2]; // Deselected and selected FMaterial interfaces.
var const native MaterialInstancePointer	DefaultMaterialInstance;

/** Streamable textures used by this material, filled in via CollectStreamingTextures called from PostLoad/ PostEditChange */
var const native array<TextureBasePointer>	StreamingTextures;

var Guid	PersistentIds[2];

var int		EditorX,
			EditorY,
			EditorPitch,
			EditorYaw;

cpptext
{
	// Constructor.

	UMaterial();

	// UMaterial interface.

	/** Fills in the StreamingTextures array with streamable textures used by this matieral. */
	virtual void CollectStreamingTextures();

	// UMaterialInstance interface.

	virtual UMaterial* GetMaterial() { return this; }
	virtual DWORD GetLayerMask();
	virtual FMaterial* GetMaterialInterface(UBOOL Selected) { return MaterialResources[Selected]; }
	virtual FMaterialInstance* GetInstanceInterface() { return DefaultMaterialInstance; }
	
	// UObject interface.

	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();
	virtual void PostEditChange(UProperty* PropertyThatChanged);
	virtual void Destroy();
}

defaultproperties
{
	BlendMode=BLEND_Opaque
	Unlit=False
	PhysicalMaterial=class'Engine.PhysicalMaterial'
	DiffuseColor=(Constant=(R=128,G=128,B=128))
	SpecularColor=(Constant=(R=128,G=128,B=128))
	SpecularPower=(Constant=15.0)
	Distortion=(ConstantX=0,ConstantY=0)
	Opacity=(Constant=1)
	OpacityMask=(Constant=1)
	OpacityMaskClipValue=0.5
	TwoSidedLightingColor=(Constant=(R=255,G=255,B=255))
}