/*=============================================================================
	MaterialShared.h: Shared material definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

extern EShaderPlatform GRHIShaderPlatform;
/** Shader platform to cook for, not meaningful unless GIsCooking is TRUE */
extern EShaderPlatform GCookingShaderPlatform;
/** Material quality to cook for, not meaningful unless GIsCooking is TRUE */
extern EMaterialShaderQuality GCookingMaterialQuality;

/** Force minimal shader compiling 
 *  Experimental: This is used for commandlets that do not need physical shaders for anything.
**/
#if CONSOLE
#define GForceMinimalShaderCompilation (0)
#else
extern UBOOL GForceMinimalShaderCompilation;
#endif

#define ME_CAPTION_HEIGHT		18
#define ME_STD_VPADDING			16
#define ME_STD_HPADDING			32
#define ME_STD_BORDER			8
#define ME_STD_THUMBNAIL_SZ		96
#define ME_PREV_THUMBNAIL_SZ	256
#define ME_STD_LABEL_PAD		16
#define ME_STD_TAB_HEIGHT		21

/**
 * The maximum number of texture samplers usable on materials that have bUsedWithStaticLighting==TRUE in the Material Editor by an artist.
 * If an artist uses more samples, the Material Editor will display a warning.
 *
 * The value of this constant assumes two samplers are currently used for directional lightmaps, and one for a static shadow map in the base pass on consoles, 
 * And one more for receiving dynamic shadows in the base pass shader; any changes to texture sampler usage must be reflected here!
 *
 * Note: If a material has bUsedWithScreenDoorFadeEnabled, the effective maximum sampler count will be reduced by one.
 */
#define MAX_ME_STATICLIGHTING_PIXELSHADER_SAMPLERS 12

/**
 * The maximum number of texture samplers usable on materials that have bUsedWithStaticLighting==FALSE in the Material Editor by an artist.
 * If an artist uses more samples, the Material Editor will display a warning.
 *
 * This must be updated if any passes used for dynamic lighting use more than one texture sampler.
 *
 * Note: If a material has bUsedWithScreenDoorFadeEnabled, the effective maximum sampler count will be reduced by one.
 */
#define MAX_ME_DYNAMICLIGHTING_PIXELSHADER_SAMPLERS 15

/**
 * The minimum package version which stores valid material compilation output.  Increasing this version will cause all materials saved in
 * older versions to generate their code on load.  Additionally, material shaders cached before the version will be discarded.
 * Warning: Because this version will invalidate old materials, bumping it requires a content resave! (otherwise shaders will be rebuilt on every run)
 */
#define VER_MIN_COMPILEDMATERIAL VER_DIFFUSEPOWER_DEFAULT
/** same as VER_MIN_COMPILEDMATERIAL but for UDecalMaterial */
#define VER_MIN_COMPILEDMATERIAL_DECAL VER_DECAL_MATERIAL_IDENDITY_NORMAL_XFORM

/** Same as VER_MIN_COMPILEDMATERIAL, but compared against the licensee package version. */
#define LICENSEE_VER_MIN_COMPILEDMATERIAL	0
/** same as LICENSEE_VER_MIN_COMPILEDMATERIAL but for UDecalMaterial */
#define LICENSEE_VER_MIN_COMPILEDMATERIAL_DECAL	0

/**
 * The types which can be used by materials.
 */
enum EMaterialValueType
{
	/** 
	 * A scalar float type.  
	 * Note that MCT_Float1 will not auto promote to any other float types, 
	 * So use MCT_Float instead for scalar expression return types.
	 */
	MCT_Float1		= 1,
	MCT_Float2		= 2,
	MCT_Float3		= 4,
	MCT_Float4		= 8,

	/** 
	 * Any size float type by definition, but this is treated as a scalar which can auto convert (by replication) to any other size float vector.
	 * Use this as the type for any scalar expressions.
	 */
	MCT_Float		= 8|4|2|1,
	MCT_Texture2D	= 16,
	MCT_TextureCube	= 32,
	MCT_Texture		= 16|32,
	MCT_StaticBool	= 64,
	MCT_Unknown		= 128
};

/** Controls discarding of shaders whose source .usf file has changed since the shader was compiled. */
UBOOL ShouldReloadChangedShaders();

/**
 * Represents a subclass of FMaterialUniformExpression.
 */
class FMaterialUniformExpressionType
{
public:

	typedef class FMaterialUniformExpression* (*SerializationConstructorType)();

	/**
	 * @return The global uniform expression type list.  The list is used to temporarily store the types until
	 *			the name subsystem has been initialized.
	 */
	static TLinkedList<FMaterialUniformExpressionType*>*& GetTypeList();

	/**
	 * Should not be called until the name subsystem has been initialized.
	 * @return The global uniform expression type map.
	 */
	static TMap<FName,FMaterialUniformExpressionType*>& GetTypeMap();

	/**
	 * Minimal initialization constructor.
	 */
	FMaterialUniformExpressionType(const TCHAR* InName,SerializationConstructorType InSerializationConstructor);

	/**
	 * Serializer for references to uniform expressions.
	 */
	friend FArchive& operator<<(FArchive& Ar,class FMaterialUniformExpression*& Ref);
	friend FArchive& operator<<(FArchive& Ar,class FMaterialUniformExpressionTexture*& Ref);

	const TCHAR* GetName() const { return Name; }

private:

	const TCHAR* Name;
	SerializationConstructorType SerializationConstructor;
};

#define DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(Name) \
	public: \
	static FMaterialUniformExpressionType StaticType; \
	static FMaterialUniformExpression* SerializationConstructor() { return new Name(); } \
	virtual FMaterialUniformExpressionType* GetType() const { return &StaticType; }

#define IMPLEMENT_MATERIALUNIFORMEXPRESSION_TYPE(Name) \
	FMaterialUniformExpressionType Name::StaticType(TEXT(#Name),&Name::SerializationConstructor);

/**
 * Represents an expression which only varies with uniform inputs.
 */
class FMaterialUniformExpression : public FRefCountedObject
{
public:

	virtual ~FMaterialUniformExpression() {}

	virtual FMaterialUniformExpressionType* GetType() const = 0;
	virtual void Serialize(FArchive& Ar) = 0;
	virtual void GetNumberValue(const struct FMaterialRenderContext& Context,FLinearColor& OutValue) const {}
	virtual class FMaterialUniformExpressionTexture* GetTextureUniformExpression() { return NULL; }
	virtual UBOOL IsConstant() const { return FALSE; }
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const { return FALSE; }

	friend FArchive& operator<<(FArchive& Ar,class FMaterialUniformExpression*& Ref);
};

/**
 * A texture expression.
 */
class FMaterialUniformExpressionTexture: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTexture);
public:

	FMaterialUniformExpressionTexture();

	FMaterialUniformExpressionTexture(UTexture* InDefaultValue);

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar);
	virtual void GetTextureValue(const FMaterialRenderContext& Context,const FMaterial& Material,const FTexture*& OutValue) const;
	/** Accesses the texture used for rendering this uniform expression. */
	virtual void GetGameThreadTextureValue(class UMaterialInterface* MaterialInterface,const FMaterial& Material,UTexture*& OutValue,UBOOL bAllowOverride=TRUE) const;
	/** 
	 * Returns the texture that this uniform expression references, 
	 * If the expression was created before uniform expressions were moved to the shader cache.
	 */
	UTexture* GetLegacyReferencedTexture() const { return LegacyTexture; }
	UTexture* GetDefaultTextureValue() const { return DefaultValueDuringCompile; }
	void ClearDefaultTextureValueReference() { DefaultValueDuringCompile = NULL; }
	virtual class FMaterialUniformExpressionTexture* GetTextureUniformExpression() { return this; }
	void SetTransientOverrideTextureValue( UTexture* InOverrideTexture )
	{
		TransientOverrideValue = InOverrideTexture;
	}
	void SetTextureIndex(INT InTextureIndex) 
	{ 
		TextureIndex = InTextureIndex; 
	}

	virtual UBOOL IsConstant() const
	{
		return FALSE;
	}
	virtual UBOOL IsIdentical(const FMaterialUniformExpression* OtherExpression) const;

	friend FArchive& operator<<(FArchive& Ar,class FMaterialUniformExpressionTexture*& Ref);

protected:
	/** Index into FMaterial::UniformExpressionTextures of the texture referenced by this expression. */
	INT TextureIndex;
	/** Default value of the expression, only set during compilation before TextureIndex is known. */
	UTexture* DefaultValueDuringCompile;
	/** Default value for legacy uniform expressions before they were moved to the shader cache. */
	UTexture* LegacyTexture;
	/** Texture that may be used in the editor for overriding the texture but never saved to disk */
	UTexture* TransientOverrideValue;
};

/** Uniform expressions for a single shader frequency. */
class FShaderFrequencyUniformExpressions
{
public:
	TArray<TRefCountPtr<FMaterialUniformExpression> > UniformVectorExpressions;
	TArray<TRefCountPtr<FMaterialUniformExpression> > UniformScalarExpressions;
	TArray<TRefCountPtr<FMaterialUniformExpressionTexture> > Uniform2DTextureExpressions;

	friend FArchive& operator<<(FArchive& Ar,FShaderFrequencyUniformExpressions& Parameters)
	{
		Ar << Parameters.UniformVectorExpressions;
		Ar << Parameters.UniformScalarExpressions;
		Ar << Parameters.Uniform2DTextureExpressions;
		return Ar;
	}

	UBOOL IsEmpty() const;
	UBOOL operator==(const FShaderFrequencyUniformExpressions& ReferenceSet) const;
	void ClearDefaultTextureValueReferences();
	void GetInputsString(EShaderFrequency Frequency, FString& InputsString) const;
};

/** Stores all uniform expressions for a material generated from a material translation. */
class FUniformExpressionSet : public FRefCountedObject
{
public:
	FUniformExpressionSet() {}

	void Serialize(FArchive& Ar);
	UBOOL IsEmpty() const;
	UBOOL operator==(const FUniformExpressionSet& ReferenceSet) const;
	void ClearDefaultTextureValueReferences();
	FString GetSummaryString() const;
	void GetInputsString(FString& InputsString) const;
	FShaderFrequencyUniformExpressions& GetExpresssions(EShaderFrequency Frequency);
	const FShaderFrequencyUniformExpressions& GetExpresssions(EShaderFrequency Frequency) const;

protected:

	/** Uniform expressions used by the pixel shader. */
	FShaderFrequencyUniformExpressions PixelExpressions;
	TArray<TRefCountPtr<FMaterialUniformExpressionTexture> > UniformCubeTextureExpressions;

	/** Uniform expressions used by the vertex shader. */
	FShaderFrequencyUniformExpressions VertexExpressions;

#if WITH_D3D11_TESSELLATION
	FShaderFrequencyUniformExpressions HullExpressions;
	FShaderFrequencyUniformExpressions DomainExpressions;
#endif

	friend class FMaterial;
	friend class FHLSLMaterialTranslator;
	friend class FMaterialPixelShaderParameters;
	friend class FMaterialVertexShaderParameters;
	friend class FCachedUniformExpressionValues;
	friend class FMaterialShaderMap;
#if WITH_D3D11_TESSELLATION
	friend class FMaterialDomainShaderParameters;
	friend class FMaterialHullShaderParameters;
#endif
};

//
//	EMaterialProperty
//

enum EMaterialProperty
{
	MP_EmissiveColor = 0,
	MP_Opacity,
	MP_OpacityMask,
	MP_Distortion,
	MP_TwoSidedLightingMask,
	MP_DiffuseColor,
	MP_DiffusePower,
	MP_SpecularColor,
	MP_SpecularPower,
	MP_Normal,
	MP_CustomLighting,
	MP_CustomLightingDiffuse,
	MP_AnisotropicDirection,
	MP_WorldPositionOffset,
	MP_WorldDisplacement,
	MP_TessellationMultiplier,
	MP_SubsurfaceAbsorptionColor,
	MP_SubsurfaceInscatteringColor,
	MP_SubsurfaceScatteringRadius,
	MP_MAX
};

/** 
 * Uniquely identifies a material expression output. 
 * Used by the material compiler to keep track of which output it is compiling.
 */
class FMaterialExpressionKey
{
public:
	UMaterialExpression* Expression;
	INT OutputIndex;

	FMaterialExpressionKey(UMaterialExpression* InExpression, INT InOutputIndex) :
		Expression(InExpression),
		OutputIndex(InOutputIndex)
	{}

	friend UBOOL operator==(const FMaterialExpressionKey& X, const FMaterialExpressionKey& Y)
	{
		return X.Expression == Y.Expression && X.OutputIndex == Y.OutputIndex;
	}

	friend DWORD GetTypeHash(const FMaterialExpressionKey& ExpressionKey)
	{
		return PointerHash(ExpressionKey.Expression);
	}
};

/** Function specific compiler state. */
class FMaterialFunctionCompileState
{
public:

	class UMaterialExpressionMaterialFunctionCall* FunctionCall;

	// Stack used to avoid re-entry within this function
	TArray<FMaterialExpressionKey> ExpressionStack;

	/** A map from material expression to the index into CodeChunks of the code for the material expression. */
	TMap<FMaterialExpressionKey,INT> ExpressionCodeMap[MP_MAX];

	explicit FMaterialFunctionCompileState(UMaterialExpressionMaterialFunctionCall* InFunctionCall) :
		FunctionCall(InFunctionCall)
	{}
};

//
//	FMaterialCompiler
//

struct FMaterialCompiler
{
	virtual void SetMaterialProperty(EMaterialProperty InProperty) = 0;
	virtual INT Error(const TCHAR* Text) = 0;
	INT Errorf(const TCHAR* Format,...);
	virtual EMaterialShaderQuality GetShaderQuality() = 0;

	virtual INT CallExpression(FMaterialExpressionKey ExpressionKey,FMaterialCompiler* InCompiler) = 0;

	virtual EMaterialValueType GetType(INT Code) = 0;

	/** 
	 * Casts the passed in code to DestType, or generates a compile error if the cast is not valid. 
	 * This will truncate a type (float4 -> float3) but not add components (float2 -> float3), however a float1 can be cast to any float type by replication. 
	 */
	virtual INT ValidCast(INT Code,EMaterialValueType DestType) = 0;
	virtual INT ForceCast(INT Code,EMaterialValueType DestType,UBOOL bExactMatch=FALSE,UBOOL bReplicateValue=FALSE) = 0;

	/** Pushes a function onto the compiler's function stack, which indicates that compilation is entering a function. */
	virtual void PushFunction(const FMaterialFunctionCompileState& FunctionState) {}

	/** Pops a function from the compiler's function statck, which indicates that compilation is leaving a function. */
	virtual FMaterialFunctionCompileState PopFunction() { return FMaterialFunctionCompileState(NULL); }

	virtual INT VectorParameter(FName ParameterName,const FLinearColor& DefaultValue) = 0;
	virtual INT ScalarParameter(FName ParameterName,FLOAT DefaultValue) = 0;
	virtual INT FlipBookOffset(UTexture* InFlipBook) = 0;

	virtual INT Constant(FLOAT X) = 0;
	virtual INT Constant2(FLOAT X,FLOAT Y) = 0;
	virtual INT Constant3(FLOAT X,FLOAT Y,FLOAT Z) = 0;
	virtual INT Constant4(FLOAT X,FLOAT Y,FLOAT Z,FLOAT W) = 0;

	virtual INT GameTime() = 0;
	virtual INT RealTime() = 0;
	virtual INT PeriodicHint(INT PeriodicCode) { return PeriodicCode; }

	virtual INT Sine(INT X) = 0;
	virtual INT Cosine(INT X) = 0;

	virtual INT Floor(INT X) = 0;
	virtual INT Ceil(INT X) = 0;
	virtual INT Frac(INT X) = 0;
	virtual INT Fmod(INT A, INT B) = 0;
	virtual INT Abs(INT X) = 0;

	virtual INT ReflectionVector() = 0;
	virtual INT CameraVector() = 0;
	virtual INT CameraWorldPosition() = 0;
	virtual INT LightVector() = 0;

	virtual INT ScreenPosition( UBOOL bScreenAlign ) = 0;
	virtual INT ActorWorldPosition() = 0;
	virtual INT WorldPosition() = 0;
	virtual INT ObjectWorldPosition() = 0;
	virtual INT ObjectRadius() = 0;
	virtual INT ParticleMacroUV(UBOOL bUseViewSpace) = 0;

	virtual INT If(INT A,INT B,INT AGreaterThanB,INT AEqualsB,INT ALessThanB) = 0;

	virtual INT TextureCoordinate(UINT CoordinateIndex, UBOOL UnMirrorU, UBOOL UnMirrorV) = 0;
	virtual INT TextureSample(INT Texture,INT Coordinate) = 0;

	virtual INT Texture(UTexture* Texture) = 0;
	virtual INT TextureParameter(FName ParameterName,UTexture* DefaultTexture) = 0;

	virtual INT BiasNormalizeNormalMap(INT Texture, BYTE CompressionSettings) = 0;

	virtual	INT SceneTextureSample( BYTE TexType, INT CoordinateIdx, UBOOL ScreenAlign )=0;
	virtual	INT SceneTextureDepth( UBOOL bNormalize, INT CoordinateIdx)=0;
	virtual	INT PixelDepth(UBOOL bNormalize)=0;
	virtual	INT DestColor()=0;
	virtual	INT DestDepth(UBOOL bNormalize)=0;
	virtual INT DepthBiasedAlpha( INT SrcAlphaIdx, INT BiasIdx, INT BiasScaleIdx )=0;
	virtual INT DepthBiasedBlend( INT SrcColorIdx, INT BiasIdx, INT BiasScaleIdx )=0;
	virtual INT FluidDetailNormal(INT TextureCoordinate)=0;

	virtual INT StaticBool(UBOOL bValue) { return INDEX_NONE; }
	virtual UBOOL GetStaticBoolValue(INT BoolIndex, UBOOL& bSucceeded) { bSucceeded = FALSE; return FALSE; }
	virtual INT VertexColor() = 0;

	virtual INT Add(INT A,INT B) = 0;
	virtual INT Sub(INT A,INT B) = 0;
	virtual INT Mul(INT A,INT B) = 0;
	virtual INT Div(INT A,INT B) = 0;
	virtual INT Dot(INT A,INT B) = 0;
	virtual INT Cross(INT A,INT B) = 0;

	virtual INT Power(INT Base,INT Exponent) = 0;
	virtual INT SquareRoot(INT X) = 0;
	virtual INT Length(INT X) = 0;

	virtual INT Lerp(INT X,INT Y,INT A) = 0;
	virtual INT Min(INT A,INT B) = 0;
	virtual INT Max(INT A,INT B) = 0;
	virtual INT Clamp(INT X,INT A,INT B) = 0;

	virtual INT ComponentMask(INT Vector,UBOOL R,UBOOL G,UBOOL B,UBOOL A) = 0;
	virtual INT AppendVector(INT A,INT B) = 0;
	virtual INT TransformVector(BYTE SourceCoordType,BYTE DestCoordType,INT A) = 0;
	virtual INT TransformPosition(BYTE SourceCoordType,BYTE DestCoordType,INT A) = 0;

	virtual INT LensFlareIntesity() = 0;
	virtual INT LensFlareOcclusion() = 0;
	virtual INT LensFlareRadialDistance() = 0;
	virtual INT LensFlareRayDistance() = 0;
	virtual INT LensFlareSourceDistance() = 0;

	virtual INT DynamicParameter() = 0;
	virtual INT LightmapUVs() = 0;

	virtual INT LightmassReplace(INT Realtime, INT Lightmass) = 0;
	virtual INT ObjectOrientation() = 0;
	virtual INT WindDirectionAndSpeed() = 0;
	virtual INT FoliageImpulseDirection() = 0;
	virtual INT FoliageNormalizedRotationAxisAndAngle() = 0;
	virtual INT RotateAboutAxis(INT NormalizedRotationAxisAndAngleIndex, INT PositionOnAxisIndex, INT PositionIndex) = 0;
	virtual INT TwoSidedSign() = 0;
	virtual INT WorldNormal() = 0;

	virtual INT CustomExpression( class UMaterialExpressionCustom* Custom, TArray<INT>& CompiledInputs ) = 0;

	virtual INT OcclusionPercentage() = 0;
	virtual INT DDX(INT X) = 0;
	virtual INT DDY(INT X) = 0;

	virtual INT PerInstanceRandom() = 0;
	virtual INT AntialiasedTextureMask(INT Tex, INT UV, float Threshold, BYTE Channel) = 0;
	virtual INT DepthOfFieldFunction(INT Depth, INT FunctionValueIndex) = 0;
	virtual INT PerInstanceSelectionMask() = 0;
	virtual INT ScreenSize() = 0;
	virtual INT SceneTexelSize() = 0;
};

//
//	FProxyMaterialCompiler - A proxy for the compiler interface which by default passes all function calls unmodified.
//

struct FProxyMaterialCompiler: FMaterialCompiler
{
	FMaterialCompiler*	Compiler;

	// Constructor.

	FProxyMaterialCompiler(FMaterialCompiler* InCompiler):
		Compiler(InCompiler)
	{}

	// Simple pass through all other material operations unmodified.

	virtual void SetMaterialProperty(EMaterialProperty InProperty) { Compiler->SetMaterialProperty(InProperty); }
	virtual INT Error(const TCHAR* Text) { return Compiler->Error(Text); }
	virtual EMaterialShaderQuality GetShaderQuality() { return Compiler->GetShaderQuality(); }

	virtual INT CallExpression(FMaterialExpressionKey ExpressionKey,FMaterialCompiler* InCompiler) { return Compiler->CallExpression(ExpressionKey,InCompiler); }

	virtual EMaterialValueType GetType(INT Code) { return Compiler->GetType(Code); }
	virtual INT ValidCast(INT Code,EMaterialValueType DestType) { return Compiler->ValidCast(Code, DestType); }
	virtual INT ForceCast(INT Code,EMaterialValueType DestType,UBOOL bExactMatch=FALSE,UBOOL bReplicateValue=FALSE) 
	{ return Compiler->ForceCast(Code,DestType,bExactMatch,bReplicateValue); }

	virtual INT VectorParameter(FName ParameterName,const FLinearColor& DefaultValue) { return Compiler->VectorParameter(ParameterName,DefaultValue); }
	virtual INT ScalarParameter(FName ParameterName,FLOAT DefaultValue) { return Compiler->ScalarParameter(ParameterName,DefaultValue); }

	virtual INT Constant(FLOAT X) { return Compiler->Constant(X); }
	virtual INT Constant2(FLOAT X,FLOAT Y) { return Compiler->Constant2(X,Y); }
	virtual INT Constant3(FLOAT X,FLOAT Y,FLOAT Z) { return Compiler->Constant3(X,Y,Z); }
	virtual INT Constant4(FLOAT X,FLOAT Y,FLOAT Z,FLOAT W) { return Compiler->Constant4(X,Y,Z,W); }

	virtual INT GameTime() { return Compiler->GameTime(); }
	virtual INT RealTime() { return Compiler->RealTime(); }

	virtual INT PeriodicHint(INT PeriodicCode) { return Compiler->PeriodicHint(PeriodicCode); }

	virtual INT Sine(INT X) { return Compiler->Sine(X); }
	virtual INT Cosine(INT X) { return Compiler->Cosine(X); }

	virtual INT Floor(INT X) { return Compiler->Floor(X); }
	virtual INT Ceil(INT X) { return Compiler->Ceil(X); }
	virtual INT Frac(INT X) { return Compiler->Frac(X); }
	virtual INT Fmod(INT A, INT B) { return Compiler->Fmod(A,B); }
	virtual INT Abs(INT X) { return Compiler->Abs(X); }

	virtual INT ReflectionVector() { return Compiler->ReflectionVector(); }
	virtual INT CameraVector() { return Compiler->CameraVector(); }
	virtual INT CameraWorldPosition() { return Compiler->CameraWorldPosition(); }
	virtual INT LightVector() { return Compiler->LightVector(); }

	virtual INT ScreenPosition( UBOOL bScreenAlign ) { return Compiler->ScreenPosition( bScreenAlign ); }
	virtual INT ActorWorldPosition() { return Compiler->ActorWorldPosition(); }
	virtual INT WorldPosition() { return Compiler->WorldPosition(); }
	virtual INT ObjectWorldPosition() { return Compiler->ObjectWorldPosition(); }
	virtual INT ObjectRadius() { return Compiler->ObjectRadius(); }
	virtual INT ParticleMacroUV(UBOOL bUseViewSpace) { return Compiler->ParticleMacroUV(bUseViewSpace); }

	virtual INT If(INT A,INT B,INT AGreaterThanB,INT AEqualsB,INT ALessThanB) { return Compiler->If(A,B,AGreaterThanB,AEqualsB,ALessThanB); }

	virtual INT TextureSample(INT InTexture,INT Coordinate) { return Compiler->TextureSample(InTexture,Coordinate); }
	virtual INT TextureCoordinate(UINT CoordinateIndex, UBOOL UnMirrorU, UBOOL UnMirrorV) { return Compiler->TextureCoordinate(CoordinateIndex, UnMirrorU, UnMirrorV); }

	virtual INT Texture(UTexture* InTexture) { return Compiler->Texture(InTexture); }
	virtual INT TextureParameter(FName ParameterName,UTexture* DefaultValue) { return Compiler->TextureParameter(ParameterName,DefaultValue); }

	virtual INT BiasNormalizeNormalMap(INT Texture, BYTE CompressionSettings) { return Compiler->BiasNormalizeNormalMap(Texture, CompressionSettings); }

	virtual	INT SceneTextureSample(BYTE TexType,INT CoordinateIdx,UBOOL ScreenAlign) { return Compiler->SceneTextureSample(TexType,CoordinateIdx,ScreenAlign);	}
	virtual	INT SceneTextureDepth( UBOOL bNormalize, INT CoordinateIdx) { return Compiler->SceneTextureDepth(bNormalize,CoordinateIdx);	}
	virtual	INT PixelDepth(UBOOL bNormalize) { return Compiler->PixelDepth(bNormalize);	}
	virtual	INT DestColor() { return Compiler->DestColor(); }
	virtual	INT DestDepth(UBOOL bNormalize) { return Compiler->DestDepth(bNormalize); }
	virtual INT DepthBiasedAlpha( INT SrcAlphaIdx, INT BiasIdx, INT BiasScaleIdx ) { return Compiler->DepthBiasedAlpha(SrcAlphaIdx,BiasIdx,BiasScaleIdx); }
	virtual INT DepthBiasedBlend( INT SrcColorIdx, INT BiasIdx, INT BiasScaleIdx ) { return Compiler->DepthBiasedBlend(SrcColorIdx,BiasIdx,BiasScaleIdx); }
	virtual INT FluidDetailNormal(INT TextureCoordinate) { return Compiler->FluidDetailNormal(TextureCoordinate); }

	virtual INT StaticBool(UBOOL bValue) { return Compiler->StaticBool(bValue); }
	virtual UBOOL GetStaticBoolValue(INT BoolIndex, UBOOL& bSucceeded) { return Compiler->GetStaticBoolValue(BoolIndex, bSucceeded); }
	virtual INT VertexColor() { return Compiler->VertexColor(); }

	virtual INT Add(INT A,INT B) { return Compiler->Add(A,B); }
	virtual INT Sub(INT A,INT B) { return Compiler->Sub(A,B); }
	virtual INT Mul(INT A,INT B) { return Compiler->Mul(A,B); }
	virtual INT Div(INT A,INT B) { return Compiler->Div(A,B); }
	virtual INT Dot(INT A,INT B) { return Compiler->Dot(A,B); }
	virtual INT Cross(INT A,INT B) { return Compiler->Cross(A,B); }

	virtual INT Power(INT Base,INT Exponent) { return Compiler->Power(Base,Exponent); }
	virtual INT SquareRoot(INT X) { return Compiler->SquareRoot(X); }
	virtual INT Length(INT X) { return Compiler->Length(X); }

	virtual INT Lerp(INT X,INT Y,INT A) { return Compiler->Lerp(X,Y,A); }
	virtual INT Min(INT A,INT B) { return Compiler->Min(A,B); }
	virtual INT Max(INT A,INT B) { return Compiler->Max(A,B); }
	virtual INT Clamp(INT X,INT A,INT B) { return Compiler->Clamp(X,A,B); }

	virtual INT ComponentMask(INT Vector,UBOOL R,UBOOL G,UBOOL B,UBOOL A) { return Compiler->ComponentMask(Vector,R,G,B,A); }
	virtual INT AppendVector(INT A,INT B) { return Compiler->AppendVector(A,B); }
	virtual INT TransformVector(BYTE SourceCoordType,BYTE DestCoordType,INT A) { return Compiler->TransformVector(SourceCoordType,DestCoordType,A); }
	virtual INT TransformPosition(BYTE SourceCoordType,BYTE DestCoordType,INT A) { return Compiler->TransformPosition(SourceCoordType,DestCoordType,A); }

	virtual INT DynamicParameter() { return Compiler->DynamicParameter(); }
	virtual INT LightmapUVs() { return Compiler->LightmapUVs(); }

	virtual INT LightmassReplace(INT Realtime, INT Lightmass) { return Realtime; }
	virtual INT ObjectOrientation() { return Compiler->ObjectOrientation(); }
	virtual INT WindDirectionAndSpeed() { return Compiler->WindDirectionAndSpeed(); }
	virtual INT FoliageImpulseDirection() { return Compiler->FoliageImpulseDirection(); }
	virtual INT FoliageNormalizedRotationAxisAndAngle() { return Compiler->FoliageNormalizedRotationAxisAndAngle(); }
	virtual INT RotateAboutAxis(INT NormalizedRotationAxisAndAngleIndex, INT PositionOnAxisIndex, INT PositionIndex)
	{
		return Compiler->RotateAboutAxis(NormalizedRotationAxisAndAngleIndex, PositionOnAxisIndex, PositionIndex);
	}
	virtual INT TwoSidedSign() { return Compiler->TwoSidedSign(); }
	virtual INT WorldNormal() { return Compiler->WorldNormal(); }

	virtual INT CustomExpression( class UMaterialExpressionCustom* Custom, TArray<INT>& CompiledInputs ) { return Compiler->CustomExpression(Custom,CompiledInputs); }
	virtual INT DDX(INT X) { return Compiler->DDX(X); }
	virtual INT DDY(INT X) { return Compiler->DDY(X); }

	virtual INT AntialiasedTextureMask(INT Tex, INT UV, float Threshold, BYTE Channel)
	{
		return Compiler->AntialiasedTextureMask(Tex, UV, Threshold, Channel);
	}
	virtual INT DepthOfFieldFunction(INT Depth, INT FunctionValueIndex)
	{
		return Compiler->DepthOfFieldFunction(Depth, FunctionValueIndex);
	}
	virtual INT PerInstanceRandom() { return Compiler->PerInstanceRandom(); }
	virtual INT PerInstanceSelectionMask() { return Compiler->PerInstanceSelectionMask(); }
	virtual INT ScreenSize() { return Compiler->ScreenSize(); }
	virtual INT SceneTexelSize() { return Compiler->SceneTexelSize(); }
};


/**
 * @return	TRUE if the property is active and should be shown, FALSE if the property should be hidden.
 */
extern UBOOL IsActiveMaterialProperty(const UMaterial* Material, const EMaterialProperty Property);

/**
 * @return The type of value expected for the given material property.
 */
extern EMaterialValueType GetMaterialPropertyType(EMaterialProperty Property);

/** Returns the shader frequency corresponding to the given material input. */
extern EShaderFrequency GetMaterialPropertyShaderFrequency(EMaterialProperty Property);

/** Returns whether the given expression class is allowed. */
extern UBOOL IsAllowedExpressionType(UClass* Class, UBOOL bMaterialFunction);

/** Parses a string into multiple lines, for use with tooltips. */
extern void ConvertToMultilineToolTip(const FString& InToolTip, INT TargetLineLength, TArray<FString>& OutToolTip);

/** transform types usable by a material shader */
enum ECoordTransformUsage
{
	// no transforms used
	UsedCoord_None		=0,
	// local to world used
	UsedCoord_World		=1<<0,
	// local to view used
	UsedCoord_View		=1<<1,
	// local to local used
	UsedCoord_Local		=1<<2,
	// World Position used
	UsedCoord_WorldPos	=1<<3
};

/**
 * A material.
 */
class FMaterial
{
	friend class FMaterialPixelShaderParameters;
	friend class FMaterialVertexShaderParameters;
	friend class FHLSLMaterialTranslator;
#if WITH_D3D11_TESSELLATION
	friend class FMaterialDomainShaderParameters;
	friend class FMaterialHullShaderParameters;
#endif
public:

	/**
	 * Minimal initialization constructor.
	 */
	FMaterial():
		MaxTextureDependencyLength(0),
		ShaderMap(NULL),
		Id(0,0,0,0),
		LegacyUniformExpressions(NULL),
		UsingTransforms(UsedCoord_None),
		bUsesSceneColor(FALSE),
		bUsesSceneDepth(FALSE),
		bUsesDynamicParameter(FALSE),
		bUsesLightmapUVs(FALSE),
		bUsesMaterialVertexPositionOffset(FALSE),
		bValidCompilationOutput(FALSE)
	{}

	/**
	 * Destructor
	 */
	virtual ~FMaterial();

	/** Populates OutEnvironment with defines needed to compile shaders for this material. */
	void SetupMaterialEnvironment(
		EShaderPlatform Platform,
		FVertexFactoryType* VertexFactoryType,
		FShaderCompilerEnvironment& OutEnvironment
		) const;

	/**
	* Compiles this material for Platform, storing the result in OutShaderMap
	*
	* @param StaticParameters - the set of static parameters to compile
	* @param Platform - the platform to compile for
	* @param Quality - Quality level to compile for (for any quality switches)
	* @param OutShaderMap - the shader map to compile
	* @param bForceCompile - force discard previous results 
	* @param bDebugDump - Dump out the preprocessed and disassembled shader for debugging.
	* @return - TRUE if compile succeeded or was not necessary (shader map for StaticParameters was found and was complete)
	*/
	virtual UBOOL Compile(
		class FStaticParameterSet* StaticParameters, 
		EShaderPlatform Platform, 
		EMaterialShaderQuality Quality, 
		TRefCountPtr<class FMaterialShaderMap>& OutShaderMap, 
		UBOOL bForceCompile=FALSE, 
		UBOOL bDebugDump=FALSE);

private:
	/**
	* Compiles OutShaderMap using the shader code from MaterialShaderCode on Platform
	*
	* @param StaticParameters - the set of static parameters to compile
	* @param Platform - the platform to compile for
	* @param OutShaderMap - the shader map to compile
	* @param MaterialShaderCode - a filled out instance of MaterialTemplate.usf to compile
	* @param bForceCompile - force discard previous results 
	* @param bSilent - indicates that no error message should be outputted on shader compile failure
	* @param bDebugDump - Dump out the preprocessed and disassembled shader for debugging.
	* @return - TRUE if compile succeeded or was not necessary (shader map for StaticParameters was found and was complete)
	*/
	UBOOL CompileShaderMap( 
		const class FStaticParameterSet* StaticParameters, 
		EShaderPlatform Platform, 
		const FUniformExpressionSet& UniformExpressionSet,
		TRefCountPtr<FMaterialShaderMap>& OutShaderMap, 
		const FString& MaterialShaderCode,
		UBOOL bForceCompile,
		UBOOL bDebugDump = FALSE);

public:

	/**
	* Caches the material shaders for this material with no static parameters on the given platform.
	*/
	UBOOL CacheShaders(EShaderPlatform Platform=GRHIShaderPlatform, EMaterialShaderQuality Quality=MSQ_UNSPECIFIED, UBOOL bFlushExistingShaderMap=TRUE);

	/**
	* Caches the material shaders for the given static parameter set and platform
	*/
	UBOOL CacheShaders(class FStaticParameterSet* StaticParameters, EShaderPlatform Platform, EMaterialShaderQuality Quality, UBOOL bFlushExistingShaderMap, UBOOL bDebugDump = FALSE);

	/**
	 * Should the shader for this material with the given platform, shader type and vertex 
	 * factory type combination be compiled
	 *
	 * @param Platform		The platform currently being compiled for
	 * @param ShaderType	Which shader is being compiled
	 * @param VertexFactory	Which vertex factory is being compiled (can be NULL)
	 *
	 * @return TRUE if the shader should be compiled
	 */
	virtual UBOOL ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const;

	/** Whether shaders should be compiled right away in CompileShaderMap or deferred until later. */
	virtual UBOOL DeferFinishCompiling() const { return FALSE; }

	/**
	 * Called by the material compilation code with a map of the compilation errors.
	 * Note that it is called even if there were no errors, but it passes an empty error map in that case.
	 * @param Errors - A set of expression error pairs.
	 */
	virtual void HandleMaterialErrors(const TMultiMap<UMaterialExpression*,FString>& Errors) {}

	/** Adds objects referenced by the material resource to an array. */
	void AddReferencedObjects(TArray<UObject*>& ObjectArray);

	/** Serializes the material. */
	virtual void Serialize(FArchive& Ar);

	/** Initializes the material's shader map. */
	UBOOL InitShaderMap(EShaderPlatform Platform, EMaterialShaderQuality Quality);

	/** Initializes the material's shader map. */
	UBOOL InitShaderMap(FStaticParameterSet* StaticParameters, EShaderPlatform Platform, EMaterialShaderQuality Quality);

	/** Flushes this material's shader map from the shader cache if it exists. */
	void FlushShaderMap();

	/**
	 * Null any material expression references for this material
	 */
	void RemoveExpressions();

	// Material properties.
	/** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
	virtual INT CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler) const = 0;
	virtual UBOOL IsTwoSided() const = 0;
	virtual UBOOL RenderTwoSidedSeparatePass() const = 0;
	virtual UBOOL RenderLitTranslucencyPrepass() const = 0;
	virtual UBOOL RenderLitTranslucencyDepthPostpass() const = 0;
	virtual UBOOL NeedsDepthTestDisabled() const = 0;
	virtual UBOOL AllowsFog() const { return TRUE; }
	virtual UBOOL UsesOneLayerDistortion() const { return FALSE; }
	virtual UBOOL IsUsedWithFogVolumes() const = 0;
	virtual UBOOL IsLightFunction() const = 0;
	virtual UBOOL IsWireframe() const = 0;
	virtual UBOOL IsDistorted() const = 0;
	virtual UBOOL HasSubsurfaceScattering() const = 0;
	virtual UBOOL HasSeparateTranslucency() const = 0;
	virtual UBOOL IsSpecialEngineMaterial() const = 0;
	virtual UBOOL IsTerrainMaterial() const = 0;
	virtual UBOOL IsSpecularAllowed() const { return TRUE; }
	virtual UBOOL IsLightmapSpecularAllowed() const { return IsSpecularAllowed(); }
	virtual UBOOL IsDecalMaterial() const = 0;
	virtual UBOOL IsUsedWithSkeletalMesh() const { return FALSE; }
	virtual UBOOL IsUsedWithTerrain() const { return FALSE; }
	virtual UBOOL IsUsedWithLandscape() const { return FALSE; }
	virtual UBOOL IsUsedWithMobileLandscape() const { return FALSE; }
	virtual UBOOL IsUsedWithFracturedMeshes() const { return FALSE; }
	virtual UBOOL IsUsedWithSpeedTree() const { return FALSE; }
	virtual UBOOL IsUsedWithParticleSystem() const { return FALSE; }
	virtual UBOOL IsUsedWithParticleSprites() const { return FALSE; }
	virtual UBOOL IsUsedWithBeamTrails() const { return FALSE; }
	virtual UBOOL IsUsedWithParticleSubUV() const { return FALSE; }
	virtual UBOOL IsUsedWithStaticLighting() const { return FALSE; }
	virtual UBOOL IsUsedWithLensFlare() const { return FALSE; }
	virtual UBOOL IsUsedWithGammaCorrection() const { return FALSE; }
	virtual UBOOL IsUsedWithInstancedMeshParticles() const { return FALSE; }
	virtual UBOOL IsUsedWithFluidSurfaces() const { return FALSE; }
	virtual UBOOL IsUsedWithMaterialEffect() const { return FALSE; }
	virtual UBOOL IsUsedWithDecals() const { return FALSE; }
	virtual	UBOOL IsUsedWithMorphTargets() const { return FALSE; }
	virtual UBOOL IsUsedWithRadialBlur() const { return FALSE; }
	virtual UBOOL IsUsedWithInstancedMeshes() const { return FALSE; }
	virtual UBOOL IsUsedWithSplineMeshes() const { return FALSE; }
	virtual UBOOL IsUsedWithAPEXMeshes() const { return FALSE; }
	virtual UBOOL IsUsedWithScreenDoorFade() const { return FALSE; }
	virtual enum EMaterialTessellationMode GetD3D11TessellationMode() const;
	virtual UBOOL IsCrackFreeDisplacementEnabled() const { return FALSE; }
	/** @return TRUE if the material should only be used with simple static lighting */
	virtual UBOOL RequiresSimpleStaticLighting() const { return FALSE; }
	virtual UBOOL IsMasked() const = 0;
	virtual UBOOL UsesImageBasedReflections() const { return FALSE; }
	virtual UBOOL UsesMaskedAntialiasing() const { return FALSE; }
	virtual FLOAT GetImageReflectionNormalDampening() const { return 1.0f; }
	virtual FLOAT GetShadowDepthBias() const { return 0.0f; }
	virtual UBOOL UsesPerPixelCameraVector() const { return FALSE; }
	virtual UBOOL CastLitTranslucencyShadowAsMasked() const = 0;
	virtual UBOOL TranslucencyInheritDominantShadowsFromOpaque() const { return FALSE; }
	virtual enum EBlendMode GetBlendMode() const = 0;
	virtual enum EMaterialLightingModel GetLightingModel() const = 0;
#if WITH_MOBILE_RHI
	virtual void FillMobileMaterialVertexParams(FMobileMaterialVertexParams& OutVertexParams) const {};
	virtual void FillMobileMaterialPixelParams(FMobileMaterialPixelParams& OutPixelParams) const {};
	virtual FProgramKey GetMobileMaterialSortKey (void) const { return FProgramKey(); }
#endif
	virtual FLOAT GetOpacityMaskClipValue() const = 0;
	virtual FString GetFriendlyName() const = 0;
#if WITH_MOBILE_RHI
	/**
	 * Internal helper functions to fill in the vertex params struct
	 * @param InMaterial - The Material to draw the parameters from
	 * @param OutVertexParams - Vertex parameter structure to pass to the shader system
	 */
	virtual void FillMobileMaterialVertexParams (const UMaterialInterface* InMaterial, FMobileMaterialVertexParams& OutVertexParams, const UMaterialInterface* MobileTextureMaterial=NULL) const;
	/**
	 * Internal helper functions to fill in the vertex params struct
	 * @param InMaterial - The Material to draw the parameters from
	 * @param OutVertexParams - Vertex parameter structure to pass to the shader system
	 */
	virtual void FillMobileMaterialPixelParams (const UMaterialInterface* InMaterial, FMobileMaterialPixelParams& OutVertexParams, const UMaterialInterface* MobileTextureMaterial=NULL) const;
#endif
protected:
	virtual UBOOL HasNormalmapConnected() const { return FALSE; }
	virtual UBOOL AllowTranslucencyDoF() const { return FALSE; }
	virtual UBOOL TranslucencyReceiveDominantShadowsFromStatic() const { return FALSE; }
	virtual UBOOL HasVertexPositionOffsetConnected() const { return FALSE; }


public:
	/**
	 * Should shaders compiled for this material be saved to disk?
	 */
	virtual UBOOL IsPersistent() const = 0;

	// Accessors.
	const TArray<FString>& GetCompileErrors() const { return CompileErrors; }
	const TArray<UMaterialExpression*>& GetErrorExpressions() const { return ErrorExpressions; }
	void SetCompileErrors(const TArray<FString>& InCompileErrors) { CompileErrors = InCompileErrors; }
	void SetUsesSceneColor(UBOOL bInUsesSceneColor) { bUsesSceneColor = bInUsesSceneColor; }
	const TMap<UMaterialExpression*,INT>& GetTextureDependencyLengthMap() const { return TextureDependencyLengthMap; }
	INT GetMaxTextureDependencyLength() const { return MaxTextureDependencyLength; }
	const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& GetUniform2DTextureExpressions() const;
	const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& GetUniformCubeTextureExpressions() const;
	class FMaterialShaderMap* GetShaderMap() const { return ShaderMap; }
	/** This function is intended only for use when cooking in CoderMode! */
	void SetShaderMap(FMaterialShaderMap* InMaterialShaderMap);
	const FGuid& GetId() const { return Id; }
	void SetId(const FGuid& NewId)
	{
		Id = NewId;
	}
	DWORD GetTransformsUsed() const { return UsingTransforms; }
	UINT GetUserTexCoordsUsed() const { return NumUserTexCoords; }
	/** Boolean indicators of using SceneColorTexture or SceneDepthTexture	*/
	UBOOL GetUsesSceneColor() const { return bUsesSceneColor; }
	UBOOL GetUsesSceneDepth() const { return bUsesSceneDepth; }
	/** Boolean indicating using DynamicParameter */
	UBOOL GetUsesDynamicParameter() const { return bUsesDynamicParameter; }
	UBOOL UsesMaterialVertexPositionOffset() const { return bUsesMaterialVertexPositionOffset; }

	UBOOL MaterialModifiesMeshPosition() const;

	/** Returns an array of both textures referenced by the uniform expressions of this material and textures used for rendering. */
	const TArray<UTexture*>& GetTextures() const { return UniformExpressionTextures; }

	/** 
	 * Empties the uniform expression texture references
	 */
	void RemoveUniformExpressionTextures() 
	{ 
		UniformExpressionTextures.Empty();
	}


	/** Information about one texture lookup. */
	struct FTextureLookup
	{
		void	Serialize(FArchive& Ar);
		INT		TexCoordIndex;
		INT		TextureIndex;			// Index into Uniform2DTextureExpressions

		/** Horizontal multiplier that can be different from 1.0f if the artist uses tiling */
		FLOAT	UScale;

		/** Vertical multiplier */
		FLOAT VScale;
	};
	typedef TArray<FTextureLookup> FTextureLookupInfo;

	/** Returns information about all texture lookups. */
	const FTextureLookupInfo& GetTextureLookupInfo() const	{ return TextureLookups; }

	/**
	 * Finds the shader matching the template type and the passed in vertex factory, asserts if not found.
	 * Note - Only implemented for FMeshMaterialShaderTypes
	 */
	template<typename ShaderType>
	ShaderType* GetShader(FVertexFactoryType* VertexFactoryType) const
	{
		return (ShaderType*)GetShader(&ShaderType::StaticType, VertexFactoryType);
	}

	/** Returns a string that describes the material's usage for debugging purposes. */
	virtual FString GetMaterialUsageDescription() const = 0;

	/**
	* Get user source code for the material, with a list of code snippets to highlight representing the code for each MaterialExpression
	* @param OutSource - generated source code
	* @param OutHighlightMap - source code highlight list
	* @param Quality - the material quality level to get the source for
	* @return - TRUE on Success
	*/
	UBOOL GetMaterialExpressionSource( FString& OutSource, TMap<FMaterialExpressionKey,INT>* OutExpressionCodeMap, EMaterialShaderQuality Quality );

	/**
	 * For UMaterials, this will return the flattened texture for platforms that don't 
	 * have full material support
	 *
	 * @return the FTexture object that represents the dominant texture for this material (can be NULL)
	 */
	virtual class FTexture* GetMobileTexture(const INT MobileTextureUnit) const
	{
		return NULL;
	}

	/** 
	 * Adds an FMaterial to the global list.
	 * Any FMaterials that don't belong to a UMaterialInterface need to be registered in this way to work correctly with runtime recompiling of outdated shaders.
	 */
	static void AddEditorLoadedMaterialResource(FMaterial* Material)
	{
		EditorLoadedMaterialResources.Add(Material);
	}

	/** Recompiles any materials in the EditorLoadedMaterialResources list if they are not complete. */
	static void UpdateEditorLoadedMaterialResources();

protected:
	void SetUsesDynamicParameter(UBOOL bInUsesDynamicParameter) { bUsesDynamicParameter = bInUsesDynamicParameter; }

	/** Rebuilds the information about all texture lookups. */
	void RebuildTextureLookupInfo( UMaterial *Material );

	/** Returns the index to the Expression in the Expressions array, or -1 if not found. */
	INT	FindExpression( const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >&Expressions, const FMaterialUniformExpressionTexture &Expression );

	void AddLegacyTextures(const TArray<UTexture*>& InTextures);
	void AddReferencedTextures(const TArray<UTexture*>& InTextures);

	UBOOL HasLegacyUniformExpressions() const { return LegacyUniformExpressions != NULL; }

	/** Useful for debugging. */
	virtual FString GetBaseMaterialPathName() const { return TEXT(""); }

	/**
	 * Maintains references to UTextures for this FMaterial.  This serves two purposes:
	 * It allows soft texture references from uniform expressions (which are saved in the shader cache) to be reassociated on load,
	 * And it allows the material to emit RTGC references so that no used textures get garbage collected in game.
	 */
	TArray<UTexture*> UniformExpressionTextures;

private:

	/** 
	 * Tracks FMaterials without a corresponding UMaterialInterface in the editor, for example FExpressionPreviews.
	 * Used to handle the 'recompileshaders changed' command in the editor.
	 * This doesn't have to use a reference counted pointer because materials are removed on destruction.
	 */
	static TSet<FMaterial*> EditorLoadedMaterialResources;

	/** List of material expressions which generated a compiler error during the last compile. */
	TArray<UMaterialExpression*> ErrorExpressions;

	TArray<FString> CompileErrors;

	/** The texture dependency lengths for the materials' expressions. */
	TMap<UMaterialExpression*,INT> TextureDependencyLengthMap;

	/** The maximum texture dependency length for the material. */
	INT MaxTextureDependencyLength;

	TRefCountPtr<FMaterialShaderMap> ShaderMap;

	FGuid Id;

	/** If non-NULL, contains legacy uniform expressions. */
	FUniformExpressionSet* LegacyUniformExpressions;

	/** Information about each texture lookup in the pixel shader. */
	FTextureLookupInfo	TextureLookups;

	UINT NumUserTexCoords;

	/** combination of ECoordTransformUsage flags used by this shader */
	DWORD UsingTransforms;

	/** Boolean indicators of using SceneColorTexture or SceneDepthTexture	*/
	BITFIELD bUsesSceneColor : 1;
	BITFIELD bUsesSceneDepth : 1;

	/** Boolean indicating using DynamicParameter */
	BITFIELD bUsesDynamicParameter : 1;

	/** Boolean indicating using LightmapUvs */
	BITFIELD bUsesLightmapUVs : 1;

	/** Indicates whether the material uses the vertex position offset material input. */
	BITFIELD bUsesMaterialVertexPositionOffset : 1;

	/**
	* False if the material's persistent compilation output was loaded from an archive older than VER_MIN_COMPILEDMATERIAL.
	* (VER_MIN_COMPILEDMATERIAL is defined in MaterialShared.cpp)
	*/
	BITFIELD bValidCompilationOutput : 1;

	friend class UDecalMaterial;

	/**
	 * Finds the shader matching the template type and the passed in vertex factory, asserts if not found.
	 */
	FShader* GetShader(class FMeshMaterialShaderType* ShaderType, FVertexFactoryType* VertexFactoryType) const;

	friend class UMaterial;
	friend class UMaterialInstance;
	friend class FMaterialShaderMap;
	friend class FDrawTranslucentMeshAction;
	friend class FShaderCompilingThreadManager;
};
/** Cached uniform expression values for a single shader frequency. */
class FShaderFrequencyUniformExpressionValues
{
public:
	/** Stores the frame numbers that these values were last cached on. */
	UINT CachedFrameNumber;

	/** 
	 * Cached uniform expression values.
	 * @todo - can we use the scene rendering allocator for these since they are recached each frame? 
	 */
	TArray<FVector4> CachedScalarParameters;
	TArray<FVector4> CachedVectorParameters;
	TArray<const FTexture*> CachedTexture2DParameters;

	FShaderFrequencyUniformExpressionValues() :
		CachedFrameNumber(UINT_MAX)
	{}

	/** Evaluates uniform expression chains if needed and stores the results in this cache object. */
	void Update(
		const FShaderFrequencyUniformExpressions& UniformExpressions,
		const FMaterialRenderContext& MaterialRenderContext, 
		UBOOL bForceUpdate);
};

/** Stores cached uniform expression values and provides a mechanism to update the cached values. */
class FCachedUniformExpressionValues
{
public:
	/** Uniform expression values cached for the pixel shader. */
	FShaderFrequencyUniformExpressionValues PixelValues;

	/** Uniform expression values cached for the vertex shader. */
	FShaderFrequencyUniformExpressionValues VertexValues;

#if WITH_D3D11_TESSELLATION
	FShaderFrequencyUniformExpressionValues HullValues;
	FShaderFrequencyUniformExpressionValues DomainValues;
#endif
};

/**
 * A material render proxy used by the renderer.
 */
class FMaterialRenderProxy
{
public:

	UBOOL bCacheable;
#if WITH_EDITOR
	//Editor only way of knowing if this proxy is specifically for mobile emulation
	UBOOL bForMobileEmulation;
#endif


	/** The uniform expression values that have been cached for this material. */
	mutable FCachedUniformExpressionValues UniformParameterCache;

	// These functions should only be called by the rendering thread.
	virtual const class FMaterial* GetMaterial() const = 0;
	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const = 0;
	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const = 0;
	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const = 0;
	virtual FLOAT GetDistanceFieldPenumbraScale() const { return 1.0f; }
	virtual UBOOL IsSelected() const { return FALSE; }
	virtual UBOOL IsHovered() const { return FALSE; }
	FMaterialRenderProxy() :
		bCacheable(TRUE)
#if WITH_EDITOR
		, bForMobileEmulation(FALSE)
#endif
	{}
	virtual ~FMaterialRenderProxy() {}
#if WITH_MOBILE_RHI
	virtual FTexture* GetMobileTexture(const INT MobileTextureUnit) const { return GetMaterial()->GetMobileTexture(MobileTextureUnit); }
	virtual void FillMobileMaterialVertexParams (FMobileMaterialVertexParams& OutVertexParams) const { return GetMaterial()->FillMobileMaterialVertexParams( OutVertexParams ); }
	virtual void FillMobileMaterialPixelParams (FMobileMaterialPixelParams& OutPixelParams) const {  return GetMaterial()->FillMobileMaterialPixelParams( OutPixelParams ); }
#endif
};

/**
 * The context of a material being rendered.
 */
struct FMaterialRenderContext
{
	/** material instance used for the material shader */
	const FMaterialRenderProxy* MaterialRenderProxy;
	/** Material resource to use. */
	const FMaterial& Material;
	/** current scene time */
	FLOAT CurrentTime;
	/** The current real-time */
	FLOAT CurrentRealTime;
	/** view matrix used for transform expression */
	const FSceneView* View;
	/** Whether uniform expression values can be cached when using this render context. */
	UBOOL bAllowUniformParameterCaching;
	/** Whether to modify sampler state set with this context to work around mip map artifacts that show up in deferred passes. */
	UBOOL bWorkAroundDeferredMipArtifacts;

	/** 
	* Constructor
	*/
	FMaterialRenderContext(
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterial,
		FLOAT InCurrentTime,
		FLOAT InCurrentRealTime,
		const FSceneView* InView,
		UBOOL bInAllowUniformParameterCaching = TRUE,
		UBOOL bInWorkAroundDeferredMipArtifacts = FALSE)
		:	
		MaterialRenderProxy(InMaterialRenderProxy),
		Material(InMaterial),
		CurrentTime(InCurrentTime),
		CurrentRealTime(InCurrentRealTime),
		View(InView),
		bAllowUniformParameterCaching(bInAllowUniformParameterCaching),
		bWorkAroundDeferredMipArtifacts(bInWorkAroundDeferredMipArtifacts)
	{
		checkSlow(View);
	}
};

/**
 * An material render proxy which overrides the material's Color vector parameter.
 */
class FColoredMaterialRenderProxy : public FMaterialRenderProxy
{
public:

	const FMaterialRenderProxy* const Parent;
	const FLinearColor Color;

	/** Initialization constructor. */
	FColoredMaterialRenderProxy(const FMaterialRenderProxy* InParent,const FLinearColor& InColor):
		Parent(InParent),
		Color(InColor)
	{}

	// FMaterialRenderProxy interface.
	virtual const class FMaterial* GetMaterial() const;
	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const;
	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const;
	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const;
};

/**
 * An material render proxy which overrides the material's Color and Lightmap resolution vector parameter.
 */
class FLightingDensityMaterialRenderProxy : public FColoredMaterialRenderProxy
{
public:
	const FVector2D LightmapResolution;

	/** Initialization constructor. */
	FLightingDensityMaterialRenderProxy(const FMaterialRenderProxy* InParent,const FLinearColor& InColor, const FVector2D& InLightmapResolution) :
		FColoredMaterialRenderProxy(InParent, InColor), 
		LightmapResolution(InLightmapResolution)
	{}

	// FMaterialRenderProxy interface.
	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const;
};

/**
 * An material render proxy which overrides the material's Color vector parameter.
 */
class FScalarReplacementMaterialRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const FName ScalarName;
	const FLOAT ScalarValue;

	/** Initialization constructor. */
	FScalarReplacementMaterialRenderProxy(const FMaterialRenderProxy* InParent,const FName& InScalarName,FLOAT InScalarValue):
	Parent(InParent),
	ScalarName(InScalarName),
	ScalarValue(InScalarValue)
	{}

	// FMaterialRenderProxy interface.
	virtual const class FMaterial* GetMaterial() const;
	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const;
	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const;
	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const;
};

/**
 * A material render proxy which overrides a texture and vector parameter;
 */
class FTexturedMaterialRenderProxy : public FMaterialRenderProxy
{
public:

	const FMaterialRenderProxy* Parent;
	const FTexture* Texture;
	FLinearColor Color;

	FTexturedMaterialRenderProxy() :
		Parent(NULL),
		Texture(NULL),
		Color(FLinearColor::Black)
	{}

	/** Initialization constructor. */
	FTexturedMaterialRenderProxy(const FMaterialRenderProxy* InParent,const FTexture* InTexture,const FLinearColor& InColor):
		Parent(InParent),
		Texture(InTexture),
		Color(InColor)
	{}

	// FMaterialRenderProxy interface.
	virtual const class FMaterial* GetMaterial() const;
	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const;
	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const;
	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const;
};

/**
* A material render proxy for font rendering
*/
class FFontMaterialRenderProxy : public FMaterialRenderProxy
{
public:

	/** parent material instance for fallbacks */
	const FMaterialRenderProxy* const Parent;
	/** font which supplies the texture pages */
	const class UFont* Font;
	/** index to the font texture page to use by the intance */
	const INT FontPage;
	/** font parameter name for finding the matching parameter */
	const FName& FontParamName;

	/** Initialization constructor. */
	FFontMaterialRenderProxy(const FMaterialRenderProxy* InParent,const class UFont* InFont,const INT InFontPage, const FName& InFontParamName)
	:	Parent(InParent)
	,	Font(InFont)
	,	FontPage(InFontPage)
	,	FontParamName(InFontParamName)
	{
		check(Parent);
		check(Font);
	}

	// FMaterialRenderProxy interface.
	virtual const class FMaterial* GetMaterial() const;
	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const;
	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const;
	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const;
};

/**
 * A material render proxy which overrides the selection color
 */
class FOverrideSelectionColorMaterialRenderProxy : public FMaterialRenderProxy
{
public:

	const FMaterialRenderProxy* const Parent;
	const FLinearColor SelectionColor;

	/** Initialization constructor. */
	FOverrideSelectionColorMaterialRenderProxy(const FMaterialRenderProxy* InParent,const FLinearColor& InSelectionColor) :
		Parent(InParent), 
		SelectionColor(InSelectionColor)
	{}

	// FMaterialRenderProxy interface.
	virtual const class FMaterial* GetMaterial() const;
	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const;
	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const;
	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const;
};

//
//	FExpressionInput
//

//@warning: FExpressionInput is mirrored in MaterialExpression.uc and manually "subclassed" in Material.uc (FMaterialInput)
struct FExpressionInput
{
	/** Material expression that this input is connected to, or NULL if not connected. */
	class UMaterialExpression*	Expression;

	/** Index into Expression's outputs array that this input is connected to. */
	INT							OutputIndex;

	/** 
	 * Optional name of the input.  
	 * Note that this is the only member which is not derived from the output currently connected. 
	 */
	FString						InputName;

	UBOOL						Mask,
								MaskR,
								MaskG,
								MaskB,
								MaskA;
	DWORD						GCC64Padding; // @todo 64: if the C++ didn't mismirror this structure, we might not need this

	INT Compile(FMaterialCompiler* Compiler);

	/**
	 * Tests if the input has a material expression connected to it
	 *
	 * @return	TRUE if an expression is connected, otherwise FALSE
	 */
	UBOOL IsConnected() const { return (NULL != Expression); }
};

//
//	FMaterialInput
//

template<class InputType> struct FMaterialInput: FExpressionInput
{
	BITFIELD	UseConstant:1;
	InputType	Constant;
};

struct FColorMaterialInput: FMaterialInput<FColor>
{
	INT Compile(FMaterialCompiler* Compiler,const FColor& Default);
};
struct FScalarMaterialInput: FMaterialInput<FLOAT>
{
	INT Compile(FMaterialCompiler* Compiler,FLOAT Default);
};

struct FVectorMaterialInput: FMaterialInput<FVector>
{
	INT Compile(FMaterialCompiler* Compiler,const FVector& Default);
};

struct FVector2MaterialInput: FMaterialInput<FVector2D>
{
	INT Compile(FMaterialCompiler* Compiler,const FVector2D& Default);
};

//
//	FExpressionOutput
//

struct FExpressionOutput
{
	FString	OutputName;
	UBOOL	Mask,
			MaskR,
			MaskG,
			MaskB,
			MaskA;

	FExpressionOutput(UBOOL InMask = 0,UBOOL InMaskR = 0,UBOOL InMaskG = 0,UBOOL InMaskB = 0,UBOOL InMaskA = 0):
		Mask(InMask),
		MaskR(InMaskR),
		MaskG(InMaskG),
		MaskB(InMaskB),
		MaskA(InMaskA)
	{}
};

/**
 * @return True if BlendMode is translucent (should be part of the translucent rendering).
 */
extern UBOOL IsTranslucentBlendMode(enum EBlendMode BlendMode);

/**
 * A resource which represents UMaterial to the renderer.
 */
class FMaterialResource : public FMaterial
{
public:

	FRenderCommandFence ReleaseFence;

	FMaterialResource(UMaterial* InMaterial);
	virtual ~FMaterialResource() {}

	void SetMaterial(UMaterial* InMaterial)
	{
		Material = InMaterial;
	}

	/** Returns the number of samplers used in this material. */
	INT GetSamplerUsage() const;

	virtual FString GetMaterialUsageDescription() const;

	// FMaterial interface.
	/** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
	virtual INT CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler) const;
	virtual UBOOL IsTwoSided() const;
	virtual UBOOL RenderTwoSidedSeparatePass() const;
	virtual UBOOL RenderLitTranslucencyPrepass() const;
	virtual UBOOL RenderLitTranslucencyDepthPostpass() const;
	virtual UBOOL NeedsDepthTestDisabled() const;
	virtual UBOOL AllowsFog() const;
	virtual UBOOL UsesOneLayerDistortion() const;
	virtual UBOOL IsUsedWithFogVolumes() const;
	virtual UBOOL IsLightFunction() const;
	virtual UBOOL IsWireframe() const;
	virtual UBOOL IsSpecialEngineMaterial() const;
	virtual UBOOL IsTerrainMaterial() const;
	virtual UBOOL IsLightmapSpecularAllowed() const;
	virtual UBOOL IsDecalMaterial() const;
	virtual UBOOL IsUsedWithSkeletalMesh() const;
	virtual UBOOL IsUsedWithTerrain() const;
	virtual UBOOL IsUsedWithLandscape() const;
	virtual UBOOL IsUsedWithMobileLandscape() const;
	virtual UBOOL IsUsedWithFracturedMeshes() const;
	virtual UBOOL IsUsedWithSpeedTree() const;
	virtual UBOOL IsUsedWithParticleSystem() const;
	virtual UBOOL IsUsedWithParticleSprites() const;
	virtual UBOOL IsUsedWithBeamTrails() const;
	virtual UBOOL IsUsedWithParticleSubUV() const;
	virtual UBOOL IsUsedWithStaticLighting() const;
	virtual UBOOL IsUsedWithLensFlare() const;
	virtual UBOOL IsUsedWithGammaCorrection() const;
	virtual UBOOL IsUsedWithInstancedMeshParticles() const;
	virtual UBOOL IsUsedWithFluidSurfaces() const;
	virtual UBOOL IsUsedWithMaterialEffect() const;
	virtual UBOOL IsUsedWithDecals() const;
	virtual	UBOOL IsUsedWithMorphTargets() const;
	virtual	UBOOL IsUsedWithRadialBlur() const;
	virtual	UBOOL IsUsedWithInstancedMeshes() const;
	virtual	UBOOL IsUsedWithSplineMeshes() const;
	virtual UBOOL IsUsedWithAPEXMeshes() const;
	virtual UBOOL IsUsedWithScreenDoorFade() const;
	virtual enum EMaterialTessellationMode GetD3D11TessellationMode() const;
	virtual UBOOL IsCrackFreeDisplacementEnabled() const;
	virtual enum EBlendMode GetBlendMode() const;
	virtual enum EMaterialLightingModel GetLightingModel() const;
#if WITH_MOBILE_RHI
	virtual void FillMobileMaterialVertexParams (FMobileMaterialVertexParams& OutVertexParams) const;
	virtual void FillMobileMaterialPixelParams (FMobileMaterialPixelParams& OutPixelParams) const;
	virtual FProgramKey GetMobileMaterialSortKey (void) const;
#endif //WITH_MOBILE_RHI
	virtual FLOAT GetOpacityMaskClipValue() const;
	virtual UBOOL IsDistorted() const;
	virtual UBOOL HasSubsurfaceScattering() const;
	virtual UBOOL HasSeparateTranslucency() const;
	virtual UBOOL IsMasked() const;
	virtual UBOOL UsesImageBasedReflections() const;
	virtual UBOOL UsesMaskedAntialiasing() const;
	virtual FLOAT GetImageReflectionNormalDampening() const;
	virtual FLOAT GetShadowDepthBias() const;
	virtual UBOOL UsesPerPixelCameraVector() const;
	virtual UBOOL CastLitTranslucencyShadowAsMasked() const;
	virtual UBOOL TranslucencyInheritDominantShadowsFromOpaque() const;
	virtual FString GetFriendlyName() const;
protected:
	virtual UBOOL HasNormalmapConnected() const;
	virtual UBOOL AllowTranslucencyDoF() const;
	virtual UBOOL TranslucencyReceiveDominantShadowsFromStatic() const;
	virtual UBOOL HasVertexPositionOffsetConnected() const;
	/** Useful for debugging. */
	virtual FString GetBaseMaterialPathName() const;
public:
	/**
	 * Should shaders compiled for this material be saved to disk?
	 */
	virtual UBOOL IsPersistent() const;

	/** Allows the resource to do things upon compile. */
	virtual UBOOL Compile( class FStaticParameterSet* StaticParameters, EShaderPlatform Platform, EMaterialShaderQuality Quality, TRefCountPtr<FMaterialShaderMap>& OutShaderMap, UBOOL bForceCompile=FALSE, UBOOL bDebugDump=FALSE);

	/**
	 * Gets instruction counts that best represent the likely usage of this material based on lighting model and other factors.
	 * @param Descriptions - an array of descriptions to be populated
	 * @param InstructionCounts - an array of instruction counts matching the Descriptions.  
	 *		The dimensions of these arrays are guaranteed to be identical and all values are valid.
	 */
	void GetRepresentativeInstructionCounts(TArray<FString> &Descriptions, TArray<INT> &InstructionCounts) const;

	/**
	 * For UMaterials, this will return the flattened texture for platforms that don't 
	 * have full material support
	 *
	 * @return the FTexture object that represents the flattened texture for this material (can be NULL)
	 */
	virtual class FTexture* GetMobileTexture(const INT MobileTextureUnit) const;

	virtual void Serialize(FArchive& Ar);

	enum EBlendMode BlendModeOverrideValue;
	UBOOL bIsBlendModeOverrided;
	UBOOL bIsMaskedOverrideValue;
protected:
	UMaterial* Material;
};


/**
* Holds the information for a static switch parameter
*/
class FStaticSwitchParameter
{
public:
	FName ParameterName;
	UBOOL Value;
	UBOOL bOverride;
	FGuid ExpressionGUID;

	FStaticSwitchParameter() :
	ParameterName(TEXT("None")),
		Value(FALSE),
		bOverride(FALSE),
		ExpressionGUID(0,0,0,0)
	{ }

	FStaticSwitchParameter(FName InName, UBOOL InValue, UBOOL InOverride, FGuid InGuid) :
	ParameterName(InName),
		Value(InValue),
		bOverride(InOverride),
		ExpressionGUID(InGuid)
	{ }

	friend FArchive& operator<<(FArchive& Ar,FStaticSwitchParameter& P)
	{
		Ar << P.ParameterName << P.Value << P.bOverride;
		Ar << P.ExpressionGUID;
		return Ar;
	}
};

/**
* Holds the information for a static component mask parameter
*/
class FStaticComponentMaskParameter
{
public:
	FName ParameterName;
	UBOOL R, G, B, A;
	UBOOL bOverride;
	FGuid ExpressionGUID;

	FStaticComponentMaskParameter() :
	ParameterName(TEXT("None")),
		R(FALSE),
		G(FALSE),
		B(FALSE),
		A(FALSE),
		bOverride(FALSE),
		ExpressionGUID(0,0,0,0)
	{ }

	FStaticComponentMaskParameter(FName InName, UBOOL InR, UBOOL InG, UBOOL InB, UBOOL InA, UBOOL InOverride, FGuid InGuid) :
	ParameterName(InName),
		R(InR),
		G(InG),
		B(InB),
		A(InA),
		bOverride(InOverride),
		ExpressionGUID(InGuid)
	{ }

	friend FArchive& operator<<(FArchive& Ar,FStaticComponentMaskParameter& P)
	{
		Ar << P.ParameterName << P.R << P.G << P.B << P.A << P.bOverride;
		Ar << P.ExpressionGUID;
		return Ar;
	}
};

/**
* Holds the information for a normal texture parameter
*/
class FNormalParameter
{
public:
	FName ParameterName;
	BYTE CompressionSettings;
	UBOOL bOverride;
	FGuid ExpressionGUID;

	FNormalParameter() :
	ParameterName(TEXT("None")),
		CompressionSettings(1/*TC_Normalmap*/),
		bOverride(FALSE),
		ExpressionGUID(0,0,0,0)
	{ }

	FNormalParameter(FName InName, BYTE InCompressionSettings, UBOOL InOverride, FGuid InGuid) :
	ParameterName(InName),
		CompressionSettings(InCompressionSettings),
		bOverride(InOverride),
		ExpressionGUID(InGuid)
	{ }

	friend FArchive& operator<<(FArchive& Ar,FNormalParameter& P)
	{
		Ar << P.ParameterName << P.CompressionSettings << P.bOverride;
		Ar << P.ExpressionGUID;
		return Ar;
	}
};

/**
* Holds the information for a static switch parameter
*/
class FStaticTerrainLayerWeightParameter
{
public:
	FName ParameterName;
	UBOOL bOverride;
	FGuid ExpressionGUID;

	INT WeightmapIndex;

	FStaticTerrainLayerWeightParameter() :
	ParameterName(TEXT("None")),
		bOverride(FALSE),
		ExpressionGUID(0,0,0,0),
		WeightmapIndex(INDEX_NONE)
	{ }

	FStaticTerrainLayerWeightParameter(FName InName, INT InWeightmapIndex, UBOOL InOverride, FGuid InGuid) :
	ParameterName(InName),
		bOverride(InOverride),
		ExpressionGUID(InGuid),
		WeightmapIndex(InWeightmapIndex)
	{ }

	friend FArchive& operator<<(FArchive& Ar,FStaticTerrainLayerWeightParameter& P)
	{
		Ar << P.ParameterName << P.WeightmapIndex << P.bOverride;
		Ar << P.ExpressionGUID;
		return Ar;
	}
};

/**
* Contains all the information needed to identify a FMaterialShaderMap.  
*/
class FStaticParameterSet
{
public:
	/** 
	* The Id of the base material's FMaterialResource.  This, along with the static parameters,
	* are used to identify identical shader maps so that material instances with static permutations can share shader maps.
	*/
	FGuid BaseMaterialId;

	/**
	 *	NOTE: When adding a new item to the StaticParameterSet, make sure that the expression(s) that
	 *	generate them are tagged "bUsedByStaticParameterSet=true" and the ClearInputExpressions function
	 *	is implemented to ensure that the CleanupMaterials function does not remove expressions needed
	 *	to find the shadermap for compiled material instances.
	 */

	/** An array of static switch parameters in this set */
	TArray<FStaticSwitchParameter> StaticSwitchParameters;

	/** An array of static component mask parameters in this set */
	TArray<FStaticComponentMaskParameter> StaticComponentMaskParameters;

	/** An array of texture sample normal parameters in this set */
	TArray<FNormalParameter> NormalParameters;
	
	/** An array of terrain layer weight parameters in this set */
	TArray<FStaticTerrainLayerWeightParameter> TerrainLayerWeightParameters;

	FStaticParameterSet() :
		BaseMaterialId(0, 0, 0, 0)
	{
	}

	FStaticParameterSet(FGuid InBaseId) :
		BaseMaterialId(InBaseId)
	{
	}

	/** 
	* Checks if this set contains any parameters
	* 
	* @return	TRUE if this set has no parameters
	*/
	UBOOL IsEmpty()
	{
		return StaticSwitchParameters.Num() == 0 && StaticComponentMaskParameters.Num() == 0 && NormalParameters.Num() == 0 && TerrainLayerWeightParameters.Num() == 0;
	}

	void Serialize(FArchive& Ar)
	{
		Ar << BaseMaterialId << StaticSwitchParameters << StaticComponentMaskParameters;
		if( Ar.Ver() >= VER_ADD_NORMAL_PARAMETERS )
		{
			Ar << NormalParameters;
		}
		if( Ar.Ver() >= VER_ADD_TERRAINLAYERWEIGHT_PARAMETERS )
		{
			Ar << TerrainLayerWeightParameters;
		}
	}

	/** 
	* Indicates whether this set is equal to another, copying override settings.
	* 
	* @param ReferenceSet	The set to compare against
	* @return				TRUE if the sets are not equal
	*/
	UBOOL ShouldMarkDirty(const FStaticParameterSet* ReferenceSet);

	friend DWORD GetTypeHash(const FStaticParameterSet &Ref)
	{
		return Ref.BaseMaterialId.A;
	}

	/** 
	* Tests this set against another for equality, disregarding override settings.
	* 
	* @param ReferenceSet	The set to compare against
	* @return				TRUE if the sets are equal
	*/
	UBOOL operator==(const FStaticParameterSet &ReferenceSet) const;

	FString GetSummaryString() const;
};

#if WITH_EDITOR
/**
 *	Mobile Emulation Material Manager
 */
class FMobileEmulationMaterialManager : public FSerializableObject
{
public:
	/** Constructor */
	FMobileEmulationMaterialManager();
	/** Destructor */
	~FMobileEmulationMaterialManager();

	/** 
	 *	GetMobileEmulationMaterialManager
	 *	Singleton interface for MEMtrlManager
	 *
	 *	@return	FMobileEmulationMaterialManager*		The manager
	 */
	static FMobileEmulationMaterialManager* GetManager()
	{
		if (MobileEmulationMaterialManager == NULL)
		{
			MobileEmulationMaterialManager = new FMobileEmulationMaterialManager();
		}
		return MobileEmulationMaterialManager;
	}

	/**
	 *	Shutdown the manager
	 */
	void Shutdown();

	/**
	 *	Clear the cached MobileEmu materials
	 */
	void ClearCachedMaterials();

	/**
	 *	Delete the generated MobileEngineMaterials
	 */
	void UpdateMobileEngineMaterials();

	/**
	 *	Update the cached materials
	 *	This should be called on map load, toggling the mode, etc. to ensure all 
	 *	required materials are cached...
	 *
	 *	@param	bRegenerateAll			If TRUE, toss *all* and regenerate all found ones.
	 *	@param	bResetParameters		If TRUE, update material parameters even if the material is already cached.
	 */
	void UpdateCachedMaterials(UBOOL bRegenerateAll, UBOOL bResetParameters);

	/** FSerializableObject interface */
	virtual void Serialize(FArchive& Ar);

	/**
	 * Helper function to get the correct name of the master material
	 *
	 * @param LightingModel - Lit or unlit
	 * @param BlendMode - Translucent, Additive, Opaque, masked, etc
	 */
	FString GetMobileEngineMaterialName(BYTE LightingModel, BYTE BlendMode, BYTE bTwoSided, BYTE bDecal);

	/**
	 * Helper function to get the correct version of the master material
	 *
	 * @param LightingModel - Lit or unlit
	 * @param BlendMode - Translucent, Additive, Opaque, masked, etc
	 * @param bCreateAsNeeded - Creates a duplicate of the master material and sets the lighting model and blend mode
	 */
	UMaterial* GetMobileEngineMaterial(BYTE LightingModel, BYTE BlendMode, BYTE bTwoSided, BYTE bDecal, UBOOL bCreateAsNeeded, UBOOL bForceRecreate);

	/**
	 *	Get the mobile key for the given material interface.
	 *
	 *	@param	InMaterialInterface		The source material interface
	 *	@param	OptOutProgramKeyData	Optional output ProgramKeyData struct to fill in
	 *
	 *	@return							The compressed material program key
	 */
	FProgramKey GetMaterialMobileKey(
		class UMaterialInterface* InMaterialInterface,
		struct FProgramKeyData* OptOutProgramKeyData);

	/**
	 *	Update the mobile representation of the give material interface.
	 *	This includes adding it to the manager if it does not exist.
	 *
	 *	@param	InMaterialInterface			The source material interface
	 *	@param	bInForce					If TRUE, then forcibly recreate the mobile emulation material
	 *	@param	bResetParameters			If TRUE, reset parameters even if the mobile emulation material is already cached
	 */
	void UpdateMaterialInterface(class UMaterialInterface* InMaterialInterface, UBOOL bInForce, UBOOL bResetParameters);

	/**
	 *	Helper function to set the MobileParticleTime field for mobile emulation rendering.
	 * 
	 *  @param InProxy                      The material that is going to be used for emulation rendering of a mesh particle
	 *	@param MobileParticleTime			Time value taken from the Dynamic Parameter Time
	 */
	void SetupMobileDynamicTimeParameter(FMaterialRenderProxy* InProxy, FLOAT MobileParticleTime);

	/**
	 *	Release the mobile representation of the give material interface.
	 *
	 *	@param	InMaterialInterface			The source material interface
	 */
	void ReleaseMaterialInterface(class UMaterialInterface* InMaterialInterface);

	/**
	 *	Get the mobile emulation material for the given MaterialInterface
	 *
	 *	@param	InMaterialInterface			The source material interface
	 *	
	 *	@return	const UMaterialInstance*	The material instance to be used for rendering
	 */
	class UMaterialInstance* GetMobileMaterialInstance(const class UMaterialInterface* InMaterialInterface);

	/**
	 *	Get the mobile emulation material for the given MaterialInterface
	 *
	 *	@param	InMaterialInterface			The source material interface
	 *	@param	bInSelected					TRUE if the object being rendered is selected
	 *	@param	bInHovered					TRUE if the object being rendered is being 'hovered' over
	 *	
	 *	@return	FMaterialInstanceResource*	The material instance resource to render with
	 */
	class FMaterialInstanceResource* GetInstanceResource(const class UMaterialInterface* InMaterialInterface, UBOOL bInSelected, UBOOL bInHovered);

	/**
	 *	Log out all materials in the manager
	 *
	 */
	void LogMobileEmulationMaterials();

	/**
	 *	Flush out all materials in the manager and recache them
	 *
	 */
	void FlushMobileEmulationMaterials();

	/** 
	 *	Set the rendering thumbnails mode
	 *
	 *	@param	bInRenderingThumbnails		Whether thumbnails are being rendered or not
	 */
	void SetRenderingThumbnails(UBOOL bInRenderingThumbnails)
	{
		bRenderingThumbnails = bInRenderingThumbnails;
	}

	/** 
	 *	Allow the rendering of material thumbnails w/ the mobile emulation material
	 *
	 *	@param	bInAllowMobileEmulationThumbnails		Whether mobile emulation thumbnails are allowed
	 */
	void SetAllowMobileEmulationThumbnails(UBOOL bInAllowMobileEmulationThumbnails)
	{
		bAllowMobileEmulationThumbnails = bInAllowMobileEmulationThumbnails;
	}

	/**
	 * Helper function to track transient shader values
	 */
	void SetMobileFogParams(const UBOOL bInEnabled, const FLOAT InFogStart, const FLOAT InFogEnd, const FColor& InFogColor);

	/**
	 * Sets the color grading parameters on mobile platforms.
	 */
	void SetMobileColorGradingParams(const struct FMobileColorGradingParams& Params);

	/**
	 * Set override color for mesh particles
	 *
	 * @param InProxy - The material that is going to be used for emulation rendering of a mesh particle
	 * @param InMeshParticleColor - The color of the particle specified by the emitter
	 */
	void SetMeshParticleColor(FMaterialRenderProxy* InProxy, const FLinearColor& InMeshParticleColor);

	/** 
	 * Set whether gamma correction is being used
	 */
	void SetGammaCorrectionEnabled(UBOOL bEnabled) 
	{
		bIsGammaCorrectionEnabled = bEnabled;
	}

	/** 
	 * Get whether gamma correction is enabled
	 */
	UBOOL GetGammaCorrectionEnabled(void)
	{
		return bIsGammaCorrectionEnabled;
	}

private:
	/**
	 * Clear Mobile Material Settings
	 *
	 * Resets the shared material settings, this is so as to ensure they are re assigned correctly on map load
	 */
	void ClearMobileMaterialSettings();

protected:
	/** The singleton */
	static FMobileEmulationMaterialManager* MobileEmulationMaterialManager;

	/** TRUE if rendering Thumbnails */
	UBOOL bRenderingThumbnails;
	/** If TRUE, allow rendering Thumbnails with mobile emulation material */
	UBOOL bAllowMobileEmulationThumbnails;
	/** TRUE if we are rendering with gamma correction enabled */
	UBOOL bIsGammaCorrectionEnabled;

	/** Material to the mobile emulation material instance it uses */
	TMap<const UMaterialInterface*, UMaterialInstance*> UniqueMICMap;

	/** Transient Fog Settings */
	FLOAT FogStart;
	FLOAT FogEnd;
	FLinearColor FogColor;

	/* Mobile Color Grading Settings */
	FLOAT MobileColorGradingBlend;
	FLOAT MobileColorGradingDesaturation;
	FLinearColor MobileColorGradingHighlights;
	FLinearColor MobileColorGradingMidTones;
	FLinearColor MobileColorGradingShadows;
};
#endif


#if WITH_MOBILE_RHI

/** 
 * Helper function that fills in the mobile texture transform array based on the Material passed in
 * @param InMaterial - Material with the parameters set
 * @param MaterialTime - Time to be used in calculation of texture transform.
 * @param OutTransform - The Transform to fill in with panner and rotator information
 */
void GetMobileTextureTransformHelper(const UMaterialInterface* InMaterial, FLOAT MaterialTime, TMatrix<3,3>& OutTransform);

#endif //WITH_MOBILE_RHI
