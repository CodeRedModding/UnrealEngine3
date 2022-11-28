/*=============================================================================
	UnClient.h: Interface definition for platform specific client code.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

//
//	Definitions.
//

#define MAX_TEXCOORDS	4

//
//	EMouseCursor
//

enum EMouseCursor
{
	MC_Arrow,
	MC_Cross,
	MC_SizeAll,
	MC_SizeUpRightDownLeft,
	MC_SizeUpLeftDownRight,
	MC_SizeLeftRight,
	MC_SizeUpDown,
};

//
//	EPixelFormat
//

// @warning: When you update this, you must add an entry to GPixelFormats(see UnTex.cpp) and also update XeTools.cpp.

enum EPixelFormat
{
	PF_Unknown			= 0,
	PF_A32B32G32R32F	= 1,
	PF_A8R8G8B8			= 2,
	PF_G8				= 3,
	PF_G16				= 4,
	PF_DXT1				= 5,
	PF_DXT3				= 6,
	PF_DXT5				= 7,
	PF_UYVY				= 8,
};

//
//	FSurface
//

struct FSurface: FResource
{
	DECLARE_RESOURCE_TYPE(FSurface,FResource);

	UINT	SizeX,
			SizeY;
	BYTE	Format;
	
	// Constructor.

	FSurface(): SizeX(0), SizeY(0), Format(PF_Unknown) {}
};

//
//	FTextureBase
//

struct FTextureBase: FSurface
{
	DECLARE_RESOURCE_TYPE(FTextureBase,FSurface);

	UINT	SizeZ;
	UINT	NumMips;
	UINT	CurrentMips;
	
	// Constructor.

	FTextureBase()
	:	SizeZ(0), 
		NumMips(0), 
		CurrentMips(0)
	{}

	// GetUnpackMin

	virtual FPlane GetUnpackMin() { return FPlane(0,0,0,0); }

	// GetUnpackMax

	virtual FPlane GetUnpackMax() { return FPlane(1,1,1,1); }

	// IsGammaCorrected

	virtual UBOOL IsGammaCorrected() { return 0; }

	// IsRGBE

	virtual UBOOL IsRGBE() { return 0; }

	// GetClampX

	virtual UBOOL GetClampX() { return 0; }

	// GetClampY

	virtual UBOOL GetClampY() { return 0; }

	// GetClampZ

	virtual UBOOL GetClampZ() { return 0; }

	/**
	 * Returns the UTexture interface associated (or NULL).
	 *
	 * @return Returns the associated UTexture interface or NULL if no such thing exists.
	 */
	virtual class UTexture* GetUTexture() { return NULL; }
};

//
//	FTexture2D
//

struct FTexture2D: FTextureBase
{
	DECLARE_RESOURCE_TYPE(FTexture2D,FTextureBase);

	// GetData - Fills a buffer with the texture data.
	virtual void GetData(void* Buffer,UINT MipIndex,UINT StrideY) = 0;
};

//
//	FTexture3D
//

struct FTexture3D: FTextureBase
{
	DECLARE_RESOURCE_TYPE(FTexture3D,FTextureBase);

	// GetData - Fills a buffer with the texture data.
	virtual void GetData(void* Buffer,UINT MipIndex,UINT StrideY,UINT StrideZ) = 0;
};

//
//	FTextureCube
//

struct FTextureCube: FTextureBase
{
	DECLARE_RESOURCE_TYPE(FTextureCube,FTextureBase);

	// GetData - Fills a buffer with the texture data for one of the cubemap faces.
	virtual void GetData(void* Buffer,UINT FaceIndex,UINT MipIndex,UINT StrideY) = 0;
};

//
//	FMaterialCompilerGuard
//

struct FMaterialCompilerGuard
{
	virtual void Error(const TCHAR* Text) = 0;
};

// EMaterialCodeType

enum EMaterialCodeType
{
	MCT_Float1		= 1,
	MCT_Float2		= 2,
	MCT_Float3		= 4,
	MCT_Float4		= 8,
	MCT_Float		= 8|4|2|1,
	MCT_Texture2D	= 16,
	MCT_TextureCube	= 32,
	MCT_Texture3D	= 64,
	MCT_Unknown		= 128
};

//
//	FMaterialCompiler
//

struct FMaterialCompiler
{
	virtual INT Error(const TCHAR* Text) = 0;
	INT Errorf(const TCHAR* Format,...);

	virtual void EnterGuard(FMaterialCompilerGuard* Guard) = 0;
	virtual void ExitGuard() = 0;

	virtual EMaterialCodeType GetType(INT Code) = 0;
	virtual INT ForceCast(INT Code,EMaterialCodeType DestType) = 0;

	virtual INT VectorParameter(FName ParameterName) = 0;
	virtual INT ScalarParameter(FName ParameterName) = 0;

	virtual INT Constant(FLOAT X) = 0;
	virtual INT Constant2(FLOAT X,FLOAT Y) = 0;
	virtual INT Constant3(FLOAT X,FLOAT Y,FLOAT Z) = 0;
	virtual INT Constant4(FLOAT X,FLOAT Y,FLOAT Z,FLOAT W) = 0;
	virtual INT Texture(FTextureBase* Texture) = 0;

	virtual INT ObjectTime() = 0;
	virtual INT SceneTime() = 0;
	virtual INT PeriodicHint(INT PeriodicCode) { return PeriodicCode; }

	virtual INT Sine(INT X) = 0;
	virtual INT Cosine(INT X) = 0;

	virtual INT Floor(INT X) = 0;
	virtual INT Ceil(INT X) = 0;
	virtual INT Frac(INT X) = 0;

	virtual INT ReflectionVector() = 0;
	virtual INT CameraVector() = 0;

	virtual INT TextureCoordinate(UINT CoordinateIndex) = 0;
	virtual INT TextureSample(INT Texture,INT Coordinate) = 0;

	virtual INT VertexColor() = 0;

	virtual INT Add(INT A,INT B) = 0;
	virtual INT Sub(INT A,INT B) = 0;
	virtual INT Mul(INT A,INT B) = 0;
	virtual INT Div(INT A,INT B) = 0;
	virtual INT Dot(INT A,INT B) = 0;
	virtual INT Cross(INT A,INT B) = 0;

	virtual INT SquareRoot(INT X) = 0;

	virtual INT Lerp(INT X,INT Y,INT A) = 0;
	virtual INT Min(INT A,INT B) = 0;
	virtual INT Max(INT A,INT B) = 0;
	virtual INT Clamp(INT X,INT A,INT B) = 0;

	virtual INT ComponentMask(INT Vector,UBOOL R,UBOOL G,UBOOL B,UBOOL A) = 0;
	virtual INT AppendVector(INT A,INT B) = 0;
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

	virtual INT Error(const TCHAR* Text) { return Compiler->Error(Text); }

	virtual void EnterGuard(FMaterialCompilerGuard* Guard) { Compiler->EnterGuard(Guard); }
	virtual void ExitGuard() { Compiler->ExitGuard(); }

	virtual EMaterialCodeType GetType(INT Code) { return Compiler->GetType(Code); }
	virtual INT ForceCast(INT Code,EMaterialCodeType DestType) { return Compiler->ForceCast(Code,DestType); }

	virtual INT VectorParameter(FName ParameterName) { return Compiler->VectorParameter(ParameterName); }
	virtual INT ScalarParameter(FName ParameterName) { return Compiler->ScalarParameter(ParameterName); }

	virtual INT Constant(FLOAT X) { return Compiler->Constant(X); }
	virtual INT Constant2(FLOAT X,FLOAT Y) { return Compiler->Constant2(X,Y); }
	virtual INT Constant3(FLOAT X,FLOAT Y,FLOAT Z) { return Compiler->Constant3(X,Y,Z); }
	virtual INT Constant4(FLOAT X,FLOAT Y,FLOAT Z,FLOAT W) { return Compiler->Constant4(X,Y,Z,W); }
	virtual INT Texture(FTextureBase* Texture) { return Compiler->Texture(Texture); }

	virtual INT ObjectTime() { return Compiler->ObjectTime(); }
	virtual INT SceneTime() { return Compiler->SceneTime(); }

	virtual INT PeriodicHint(INT PeriodicCode) { return Compiler->PeriodicHint(PeriodicCode); }

	virtual INT Sine(INT X) { return Compiler->Sine(X); }
	virtual INT Cosine(INT X) { return Compiler->Cosine(X); }

	virtual INT Floor(INT X) { return Compiler->Floor(X); }
	virtual INT Ceil(INT X) { return Compiler->Ceil(X); }
	virtual INT Frac(INT X) { return Compiler->Frac(X); }

	virtual INT ReflectionVector() { return Compiler->ReflectionVector(); }
	virtual INT CameraVector() { return Compiler->CameraVector(); }

	virtual INT TextureSample(INT Texture,INT Coordinate) { return Compiler->TextureSample(Texture,Coordinate); }
	virtual INT TextureCoordinate(UINT CoordinateIndex) { return Compiler->TextureCoordinate(CoordinateIndex); }

	virtual INT VertexColor() { return Compiler->VertexColor(); }

	virtual INT Add(INT A,INT B) { return Compiler->Add(A,B); }
	virtual INT Sub(INT A,INT B) { return Compiler->Sub(A,B); }
	virtual INT Mul(INT A,INT B) { return Compiler->Mul(A,B); }
	virtual INT Div(INT A,INT B) { return Compiler->Div(A,B); }
	virtual INT Dot(INT A,INT B) { return Compiler->Dot(A,B); }
	virtual INT Cross(INT A,INT B) { return Compiler->Cross(A,B); }

	virtual INT SquareRoot(INT X) { return Compiler->SquareRoot(X); }

	virtual INT Lerp(INT X,INT Y,INT A) { return Compiler->Lerp(X,Y,A); }
	virtual INT Min(INT A,INT B) { return Compiler->Min(A,B); }
	virtual INT Max(INT A,INT B) { return Compiler->Max(A,B); }
	virtual INT Clamp(INT X,INT A,INT B) { return Compiler->Clamp(X,A,B); }

	virtual INT ComponentMask(INT Vector,UBOOL R,UBOOL G,UBOOL B,UBOOL A) { return Compiler->ComponentMask(Vector,R,G,B,A); }
	virtual INT AppendVector(INT A,INT B) { return Compiler->AppendVector(A,B); }
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
	MP_SpecularColor,
	MP_SpecularPower,
	MP_Normal,
	MP_SHM,
	MP_MAX
};

//
//	FMaterial
//

struct FMaterial: FResource
{
	DECLARE_RESOURCE_TYPE(FMaterial,FResource);

	class USphericalHarmonicMap*	SourceSHM;

	UBOOL	TwoSided,
			NonDirectionalLighting,
			Unlit;
	INT		BlendMode;
	FLOAT	OpacityMaskClipValue;

	// Constructor.

	FMaterial():
		SourceSHM(NULL),
		TwoSided(0),
		NonDirectionalLighting(0),
		Unlit(0),
		BlendMode(0),
		OpacityMaskClipValue(0.0f)
	{}

	// CompileProperty

	virtual INT CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler) = 0;

	// GetPropertyType

	static EMaterialCodeType GetPropertyType(EMaterialProperty Property);
};

//
//	FMaterialInstance
//

struct FMaterialInstance
{
	TMap<FName,FLinearColor>	VectorValueMap;
	TMap<FName,FLOAT>			ScalarValueMap;
};

//
//	FMaterialError
//

struct FMaterialError
{
	FString	Description;
};

//
//	FIndexBuffer
//

struct FIndexBuffer: FResource
{
	DECLARE_RESOURCE_TYPE(FIndexBuffer,FResource);

	UINT	Stride;
	UINT	Size;

	// Constructor.

	FIndexBuffer(): Stride(0), Size(0) {}

	// Num - Returns the number of indices in the buffer.

	UINT Num() const { return Size / Stride; }

	// GetData

	virtual void GetData(void* Buffer) = 0;
};

//
//	FRawIndexBuffer
//

class FRawIndexBuffer : public FIndexBuffer
{
public:

	TArray<_WORD>	Indices;

	// Constructor.

	FRawIndexBuffer()
	{
		Stride = sizeof(_WORD);
		Size = 0;
	}

	// Stripify - Converts a triangle list into a triangle strip.

	INT Stripify();

	// CacheOptimize - Orders a triangle list for better vertex cache coherency.

	void CacheOptimize();

	// Serialization.

	friend FArchive& operator<<(FArchive& Ar,FRawIndexBuffer& I);

	// FIndexBuffer interface.

	virtual void GetData(void* Data);
};

//
//	FRawIndexBuffer32
//

class FRawIndexBuffer32 : public FIndexBuffer
{
public:

	TArray<UINT>	Indices;

	// Constructor.

	FRawIndexBuffer32()
	{
		Stride = sizeof(UINT);
		Size = 0;
	}

	// Serialization.

	friend FArchive& operator<<(FArchive& Ar,FRawIndexBuffer32& I);

	// FIndexBuffer interface.

	virtual void GetData(void* Data);
};

//
//	FVertexBuffer
//

struct FVertexBuffer: FResource
{
	DECLARE_RESOURCE_TYPE(FVertexBuffer,FResource);

	UINT	Stride;
	UINT	Size;

	// Constructor.

	FVertexBuffer(): Stride(0), Size(0) {}

	// Num - Returns the number of vertices in the buffer.

	UINT Num() const { return Size / Stride; }

	// GetData

	virtual void GetData(void* Buffer) = 0;
};

//
//	FPackedNormal
//

struct FPackedNormal
{
	union
	{
		struct
		{
#if __INTEL_BYTE_ORDER__
			BYTE	X,
					Y,
					Z,
					W;
#else
			BYTE	W,
					Z,
					Y,
					X;
#endif
		};
		DWORD		Packed;
	}				Vector;

	// Constructors.

	FPackedNormal() { Vector.Packed = 0; }
	FPackedNormal( DWORD InPacked ) { Vector.Packed = InPacked; }
	FPackedNormal(const FVector& InVector) { *this = InVector; }

	// Conversion operators.

	void operator=(const FVector& InVector);
	operator FVector() const;

	// Equality operator.

	UBOOL operator==(const FPackedNormal& B) const
	{
		if(Vector.Packed != B.Vector.Packed)
			return 0;

		FVector	V1 = *this,
				V2 = B;

		if(Abs(V1.X - V2.X) > THRESH_NORMALS_ARE_SAME * 4.0f)
			return 0;

		if(Abs(V1.Y - V2.Y) > THRESH_NORMALS_ARE_SAME * 4.0f)
			return 0;

		if(Abs(V1.Z - V2.Z) > THRESH_NORMALS_ARE_SAME * 4.0f)
			return 0;

		return 1;
	}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FPackedNormal& N)
	{
		return Ar << N.Vector.Packed;
	}
};

//
//	EVertexComponentType
//

enum EVertexComponentType
{
	VCT_None,
	VCT_Float1,
	VCT_Float2,
	VCT_Float3,
	VCT_Float4,
	VCT_PackedNormal,	// FPackedNormal
	VCT_UByte4,
	VCT_UShort2,
	VCT_Short2
};

//
//	FVertexStreamComponent
//

struct FVertexStreamComponent
{
	FVertexBuffer*			VertexBuffer;
	UINT					Offset;
	EVertexComponentType	Type;

	FVertexStreamComponent() { VertexBuffer = NULL; Offset = 0; Type = VCT_None; }
	FVertexStreamComponent(FVertexBuffer* InVertexBuffer,UINT InOffset,EVertexComponentType InType): VertexBuffer(InVertexBuffer), Offset(InOffset), Type(InType) {}
};

//
//	FVertexFactory
//

struct FVertexFactory: FResource
{
	DECLARE_RESOURCE_TYPE(FVertexFactory,FResource);

	FVertexBuffer*	DynamicVertexBuffer;

	// Constructor.

	FVertexFactory(): DynamicVertexBuffer(NULL) {}
};

//
//	FLocalVertexFactory
//

struct FLocalVertexFactory: FVertexFactory
{
	DECLARE_RESOURCE_TYPE(FLocalVertexFactory,FVertexFactory);

	FMatrix									LocalToWorld;
	FMatrix									WorldToLocal;

	FVertexStreamComponent					PositionComponent;
	FVertexStreamComponent					TangentBasisComponents[3];
	TStaticArray<FVertexStreamComponent,8>	TextureCoordinates;
	FVertexStreamComponent					LightMaskComponent;
	FVertexStreamComponent					StaticLightingComponent;
	FVertexStreamComponent					LightMapCoordinateComponent;

	// Constructor.

	FLocalVertexFactory(): LocalToWorld(FMatrix::Identity), WorldToLocal(FMatrix::Identity) {}
};

//
//	FLocalShadowVertexFactory
//

struct FLocalShadowVertexFactory: FVertexFactory
{
	DECLARE_RESOURCE_TYPE(FLocalShadowVertexFactory,FVertexFactory);

	FMatrix					LocalToWorld;
	FVertexStreamComponent	PositionComponent;
	FVertexStreamComponent	ExtrusionComponent;

	// Constructor.

	FLocalShadowVertexFactory(): LocalToWorld(FMatrix::Identity) {}
};

//
//	FTerrainVertexFactory
//

struct FTerrainVertexFactory: FVertexFactory
{
	DECLARE_RESOURCE_TYPE(FTerrainVertexFactory,FVertexFactory);

	FMatrix					LocalToWorld;
	INT						SectionSizeX,
							SectionSizeY,
							MaxTesselationLevel,
							TerrainSizeX,
							TerrainSizeY,
							SectionBaseX,
							SectionBaseY,
							WeightMapResolution,
							LightMapResolution;
	FLOAT					TerrainHeightScale;

	FVertexStreamComponent	PositionComponent;		// Packed DWORD: Z in upper 16 bits, X,Y in lower 16 bits.
	FVertexStreamComponent	DisplacementComponent;	// float, displaces vertex along normal
	FVertexStreamComponent	GradientsComponent;		// Used to calculate tangent space: GradientX, GradientY

	// Constructor.

	FTerrainVertexFactory(
		INT InSectionSizeX,
		INT InSectionSizeY,
		INT InMaxTesselationLevel,
		INT InTerrainSizeX,
		INT InTerrainSizeY,
		INT InSectionBaseX,
		INT InSectionBaseY,
		INT InWeightMapResolution,
		INT InLightMapResolution,
		FLOAT InTerrainHeightScale
		):
		LocalToWorld(FMatrix::Identity),
		SectionSizeX(InSectionSizeX),
		SectionSizeY(InSectionSizeY),
		MaxTesselationLevel(InMaxTesselationLevel),
		TerrainSizeX(InTerrainSizeX),
		TerrainSizeY(InTerrainSizeY),
		SectionBaseX(InSectionBaseX),
		SectionBaseY(InSectionBaseY),
		WeightMapResolution(InWeightMapResolution),
		LightMapResolution(InLightMapResolution),
		TerrainHeightScale(InTerrainHeightScale)
	{}
};

//
//	FWindPointSource
//

struct FWindPointSource
{
	FVector	SourceLocation;
	FLOAT	Strength,
			Phase,
			Frequency,
			InvRadius,
			InvDuration;

	// GetWind

	FVector GetWind(const FVector& Location) const;
};

//
//	FWindDirectionSource
//

struct FWindDirectionSource
{
	FVector	Direction;
	FLOAT	Strength,
			Phase,
			Frequency;

	// GetWind

	FVector GetWind(const FVector& Location) const;
};

//
//	FFoliageVertexFactory
//

struct FFoliageVertexFactory: FVertexFactory
{
	DECLARE_RESOURCE_TYPE(FFoliageVertexFactory,FVertexFactory);

	TStaticArray<FWindPointSource,100>		WindPointSources;
	TStaticArray<FWindDirectionSource,4>	WindDirectionalSources;
	FLOAT									WindSpeed;

	FVertexStreamComponent	PositionComponent;
	FVertexStreamComponent	TangentBasisComponents[3];
	FVertexStreamComponent	SwayStrengthComponent;
	FVertexStreamComponent	PointSourcesComponent;
	FVertexStreamComponent	TextureCoordinateComponent;
	FVertexStreamComponent	LightMapCoordinateComponent;
	FVertexStreamComponent	StaticLightingComponent;

	// Constructor.

	FFoliageVertexFactory() {}
};

//
//	FParticleVertexFactory
//

struct FParticleVertexFactory: FVertexFactory
{
	DECLARE_RESOURCE_TYPE(FParticleVertexFactory,FVertexFactory);

	FMatrix					LocalToWorld;
	FVertexStreamComponent	PositionComponent;
	FVertexStreamComponent	OldPositionComponent;
	FVertexStreamComponent	SizeComponent;
	FVertexStreamComponent	UVComponent;
	FVertexStreamComponent	RotationComponent;
	FVertexStreamComponent	ColorComponent;

	BYTE					ScreenAlignment;
};

struct FParticleSubUVVertexFactory : FParticleVertexFactory
{
	DECLARE_RESOURCE_TYPE(FParticleSubUVVertexFactory,FParticleVertexFactory);

	FVertexStreamComponent	UVComponent2;
	FVertexStreamComponent  Interpolation;
	FVertexStreamComponent  SizeScaler;
};

struct FParticleTrailVertexFactory : FParticleVertexFactory
{
	DECLARE_RESOURCE_TYPE(FParticleTrailVertexFactory,FParticleVertexFactory);
};

//
//	HHitProxy - Base class for detecting user-interface hits.
//

struct HHitProxy
{
	UINT	Order; // Set when passed to SetHitProxy to the order this hit proxy was rendered in.

	HHitProxy(): Order(0) {}
	virtual ~HHitProxy() {}
	virtual const TCHAR* GetName() const
	{
		return TEXT("HHitProxy");
	}
	virtual UBOOL IsA(const TCHAR* Str) const
	{
		return appStricmp(TEXT("HHitProxy"),Str)==0;
	}
	virtual void Serialize(FArchive& Ar) {}

	/**
		Override to change the mouse based on what it is hovering over.
	*/
	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Arrow;
	}
};

// Hit proxy declaration macro.

#define DECLARE_HIT_PROXY(cls,parent) \
	virtual const TCHAR* GetName() const \
		{ return TEXT(#cls); } \
	virtual UBOOL IsA( const TCHAR* Str ) const \
		{ return appStricmp(TEXT(#cls),Str)==0 || parent::IsA(Str); } \

//
//	FRenderInterface
//

struct FRenderInterface
{
	FRenderInterface() :
		Zoom2D(1.f)
	{
	}

	virtual void Clear(const FColor& Color) = 0;

	virtual void DrawScene(const struct FSceneContext& Context) = 0;

	virtual void DrawLine2D(INT X1,INT Y1,INT X2,INT Y2,FColor Color) = 0;

	virtual void DrawTile(INT X,INT Y,UINT SizeX,UINT SizeY,FLOAT U,FLOAT V,FLOAT W,FLOAT SizeU,FLOAT SizeV,const FLinearColor& Color,FTexture3D* Texture = NULL,UBOOL AlphaBlend = 1) = 0;
	virtual void DrawTile(INT X,INT Y,UINT SizeX,UINT SizeY,FLOAT U,FLOAT V,FLOAT SizeU,FLOAT SizeV,const FLinearColor& Color,FTexture2D* Texture = NULL,UBOOL AlphaBlend = 1) = 0;
	virtual void DrawTile(INT X,INT Y,UINT SizeX,UINT SizeY,FLOAT U,FLOAT V,FLOAT SizeU,FLOAT SizeV,FMaterial* Material,FMaterialInstance* MaterialInstance) = 0;

	virtual void DrawTriangle2D(
		const FIntPoint& Vert0, FLOAT U0, FLOAT V0, 
		const FIntPoint& Vert1, FLOAT U1, FLOAT V1, 
		const FIntPoint& Vert2, FLOAT U2, FLOAT V2,
		const FLinearColor& Color, FTexture2D* Texture = NULL, UBOOL AlphaBlend = 1) = 0;
	virtual void DrawTriangle2D(
		const FIntPoint& Vert0, FLOAT U0, FLOAT V0, 
		const FIntPoint& Vert1, FLOAT U1, FLOAT V1, 
		const FIntPoint& Vert2, FLOAT U2, FLOAT V2,
		FMaterial* Material,FMaterialInstance* MaterialInstance) = 0;

	INT DrawStringCentered(INT StartX,INT StartY,const TCHAR* Text,class UFont* Font,const FLinearColor& Color);
	INT DrawString(INT StartX,INT StartY,const TCHAR* Text,class UFont* Font,const FLinearColor& Color);
	INT DrawShadowedString(INT StartX,INT StartY,const TCHAR* Text,class UFont* Font,const FLinearColor& Color);
	void StringSize(UFont* Font,INT& XL,INT& YL,const TCHAR* Format,...);

	virtual void SetHitProxy(HHitProxy* HitProxy) = 0;
	virtual UBOOL IsHitTesting() = 0;

	void SetOrigin2D( const FIntPoint& InOrigin ) { Origin2D = InOrigin; }
	void SetOrigin2D( INT InX, INT InY ) { Origin2D = FIntPoint( InX, InY ); }

	void SetZoom2D( FLOAT NewZoom2D ) { Zoom2D = NewZoom2D; }

	FIntPoint Origin2D; // All 2D drawing code should add this into their X/Y drawing coordinates.  Makes scrolling in the editor much easier.
	FLOAT Zoom2D;
};

//
//	FChildViewport
//

struct FChildViewport
{
	struct FViewportClient*	ViewportClient;

	// FChildViewport interface.

	virtual struct FViewport* GetViewport() { return NULL; }

	virtual void* GetWindow() = 0;

	virtual UBOOL CaptureMouseInput(UBOOL Capture) = 0;	// ViewportClient only gets mouse click notifications when input isn't captured.
	virtual void LockMouseToWindow(UBOOL bLock) = 0;
	virtual UBOOL KeyState(FName Key) = 0;

	virtual UBOOL CaptureJoystickInput(UBOOL Capture) = 0;

	UBOOL IsCtrlDown();
	UBOOL IsShiftDown();
	UBOOL IsAltDown();

	virtual INT GetMouseX() = 0;
	virtual INT GetMouseY() = 0;

	virtual UBOOL MouseHasMoved() = 0; // Returns true if the mouse has moved since the last mouse button was pressed.

	virtual UINT GetSizeX() = 0;
	virtual UINT GetSizeY() = 0;

	virtual UBOOL IsFullscreen() = 0;
	
	virtual void Draw(UBOOL Synchronous = 0) = 0;
	virtual UBOOL ReadPixels(TArray<FColor>& OutputBuffer) = 0;
	virtual void InvalidateDisplay() = 0;

	// GetHitProxyMap - Copies the hit proxies from an area of the screen into a buffer.  (MinX,MinY)->(MaxX,MaxY) must be entirely within the viewport's client area.

	virtual void GetHitProxyMap(UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<HHitProxy*>& OutMap) = 0;

	// Invalidate - Invalidates cached hit proxies and the display.

	virtual void Invalidate() = 0;

	// GetHitProxy - Returns the dominant hit proxy at a given point.  If X,Y are outside the client area of the viewport, returns NULL.
	// Caution is required as calling Invalidate after this will free the returned HHitProxy.

	HHitProxy* GetHitProxy(INT X,INT Y);
};

//
//	FViewport
//

struct FViewport: public FChildViewport
{
	// FChildViewport interface.

	virtual FViewport* GetViewport() { return this; }

	// FViewport interface.

	virtual const TCHAR* GetName() = 0;
	virtual void SetName(const TCHAR* NewName) = 0;
	virtual void Resize(UINT NewSizeX,UINT NewSizeY,UBOOL NewFullscreen) = 0;
};

//
//	FPrecacheInterface
//

struct FPrecacheInterface
{
	virtual void CacheResource(FResource* Resource) = 0;
};

//
//	EInputEvent
//

enum EInputEvent
{
	IE_Pressed		= 0,
	IE_Released		= 1,
	IE_Repeat		= 2,
	IE_DoubleClick	= 3,
	IE_Axis			= 4,
};

//
//	FViewportClient
//

struct FViewportClient
{
	// FViewportClient interface.

	virtual void Precache(FChildViewport* Viewport,FPrecacheInterface* P) {}

	virtual void RedrawRequested(FChildViewport* Viewport) { Viewport->Draw(); }

	virtual void Draw(FChildViewport* Viewport,FRenderInterface* RI) {}

	virtual void InputKey(FChildViewport* Viewport,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f) {}
	virtual void InputAxis(FChildViewport* Viewport,FName Key,FLOAT Delta,FLOAT DeltaTime) {}
	virtual void InputChar(FChildViewport* Viewport,TCHAR Character) {}
	/** Returns the platform specific forcefeedback manager associated with this viewport */
	virtual class UForceFeedbackManager* GetForceFeedbackManager(void) { return NULL; }
	virtual void MouseMove(FChildViewport* Viewport,INT X,INT Y) {}

	virtual EMouseCursor GetCursor(FChildViewport* Viewport,INT X,INT Y) { return MC_Arrow; }

	virtual void LostFocus(FChildViewport* Viewport) {}
	virtual void ReceivedFocus(FChildViewport* Viewport) {}

	virtual void CloseRequested(FViewport* Viewport) {}

	virtual UBOOL RequiresHitProxyStorage() { return 1; }
};

//
//	UClient - Interface to platform-specific code.
//

class UClient : public UObject, FExec
{
	DECLARE_ABSTRACT_CLASS(UClient,UObject,CLASS_Config,Engine);
public:

	// Configuration.

	FLOAT		MinDesiredFrameRate;

	BITFIELD	StartupFullscreen,
				EditorPreviewFullscreen;
	INT			StartupResolutionX,
				StartupResolutionY;

	// UObject interface.

	void StaticConstructor();

	// UClient interface.

	virtual void Init(UEngine* InEngine) = 0;
	virtual void Flush() = 0;
	virtual void Tick(FLOAT DeltaTime) = 0;
	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar=*GLog) { return 0; }
	/**
	 * PC only, debugging only function to prevent the engine from reacting to OS messages. Used by e.g. the script
	 * debugger to avoid script being called from within message loop (input).
	 *
	 * @param InValue	If FALSE disallows OS message processing, otherwise allows OS message processing (default)
	 */
	virtual void AllowMessageProcessing( UBOOL InValue ) = 0;
	
	virtual FViewport* CreateViewport(FViewportClient* ViewportClient,const TCHAR* Name,UINT SizeX,UINT SizeY,UBOOL Fullscreen = 0) = 0;
	virtual FChildViewport* CreateWindowChildViewport(FViewportClient* ViewportClient,void* ParentWindow,UINT SizeX=0,UINT SizeY=0) = 0;
	virtual void CloseViewport(FChildViewport* Viewport) = 0;

	virtual class URenderDevice* GetRenderDevice() = 0;
	virtual class UAudioDevice* GetAudioDevice() = 0;
};

//
//	URenderDevice
//

class URenderDevice : public USubsystem
{
	DECLARE_ABSTRACT_CLASS(URenderDevice,USubsystem,0,WinDrv);
public:
	/** Maximum texture size */
	UINT	MaxTextureSize;

	// URenderDevice interface.

	virtual void Init() = 0;
	virtual void Flush() = 0;

	virtual void CreateViewport(FChildViewport* Viewport) = 0;
	virtual void ResizeViewport(FChildViewport* Viewport) = 0;
	virtual void DestroyViewport(FChildViewport* Viewport) = 0;

	virtual void DrawViewport(FChildViewport* Viewport,UBOOL Synchronous) = 0;
	virtual void ReadPixels(FChildViewport* Viewport,FColor* OutputBuffer) = 0;
	
	virtual void GetHitProxyMap(FChildViewport* Viewport,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<HHitProxy*>& OutMap) = 0;
	virtual void InvalidateHitProxies(FChildViewport* Viewport) = 0;

	virtual TArray<FMaterialError> CompileMaterial(FMaterial* Material) = 0;
};

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
