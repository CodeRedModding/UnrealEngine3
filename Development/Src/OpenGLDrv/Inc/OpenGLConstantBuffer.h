/*=============================================================================
	OpenGLConstantBuffer.h: Public OpenGL Constant Buffer definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#define MAX_CONSTANT_BUFFER_SIZE (2560)
#define MIN_CONSTANT_BUFFER_SIZE (Max<INT>(VSR_MaxVertexShaderRegister,PSR_MaxPixelShaderRegister)*sizeof(FVector4))
#define BONE_CONSTANT_BUFFER_SIZE (MAX_GPUSKIN_BONES*3*sizeof(FVector4))
#define BOOL_CONSTANT_BUFFER_SIZE (32 * sizeof(UINT))

enum EOpenGLShaderOffsetBuffer
{
	LOCAL_CONSTANT_BUFFER = 0,
	GLOBAL_CONSTANT_BUFFER = 1,

	VS_BONE_CONSTANT_BUFFER=2,
	VS_BOOL_CONSTANT_BUFFER=3,

	PS_BOOL_CONSTANT_BUFFER=2,

	MAX_VS_CONSTANT_BUFFER_SLOTS = 4,
	MAX_PS_CONSTANT_BUFFER_SLOTS = 3
};

struct FOpenGLOffsetConstantBufferContentsVS
{
	FMatrix ViewProjectionMatrix;
	FVector4 ViewOrigin;
	FVector4 PreViewTranslation;
};

struct FOpenGLOffsetConstantBufferContentsPS
{
	FVector4 ScreenPositionScaleBias;
	FVector4 MinZ_MaxZRatio;
	FVector4 NvStereoEnabled;
	FVector4 DiffuseOverrideParameter;
	FVector4 SpecularOverrideParameter;
	FVector4 ViewOrigin;
	FVector4 ScreenAndTexelSize;
};

// VS_GLOBAL_CONSTANT_BASE_INDEX and PS_GLOBAL_CONSTANT_BASE_INDEX must match base indices in Common.usf
// and place common shader constants at the very end of const tables. It's done this way so even if shader
// compiler optimizes some of the common constants out, local shader constants have a predictable base index
// (0, to be specific). Thanks to this, bytecode-to-GLSL converter knows what uniform table to assign the
// consts to.
#define VS_NUM_GLOBAL_VECTORS (sizeof(FOpenGLOffsetConstantBufferContentsVS)/sizeof(FVector4))
#define PS_NUM_GLOBAL_VECTORS (sizeof(FOpenGLOffsetConstantBufferContentsPS)/sizeof(FVector4))
#define VS_GLOBAL_CONSTANT_BASE_INDEX (256-VS_NUM_GLOBAL_VECTORS-1) // -1 as FOpenGLOffsetConstantBufferContentsVS does not include unused TemporalAA data
#define PS_GLOBAL_CONSTANT_BASE_INDEX (224-PS_NUM_GLOBAL_VECTORS-1) // -1 as FOpenGLOffsetConstantBufferContentsPS does not include unused PSR_ColorBiasFactor
#define BONE_CONSTANT_BASE_INDEX (VS_GLOBAL_CONSTANT_BASE_INDEX-MAX_GPUSKIN_BONES*3)

class FOpenGLConstantBuffer : public FRenderResource, public FRefCountedObject
{
public:

	FOpenGLConstantBuffer(UINT InSize,UINT InBaseOffset = 0,UINT SubBuffers = 1);
	~FOpenGLConstantBuffer();

	// FRenderResource interface.
	virtual void	InitDynamicRHI();
	virtual void	ReleaseDynamicRHI();

	void			UpdateConstant(const BYTE* Data, WORD Offset, WORD Size);
	UBOOL			CommitConstantsToDevice(UBOOL bDiscardSharedConstants);
#if OPENGL_USE_BINDABLE_UNIFORMS
	FOpenGLUniformBuffer*	GetUniformBuffer() const { return Buffers[CurrentSubBuffer]; }
#endif
	BYTE*			GetData() const { return ShadowData; }
	UINT			GetUpdateSize() const { return TotalUpdateSize; }

private:
#if OPENGL_USE_BINDABLE_UNIFORMS
	TRefCountPtr<FOpenGLUniformBuffer>* Buffers;
#endif
	UINT	MaxSize;
	UINT	BaseOffset;
	UBOOL	bIsDirty;
	BYTE*	ShadowData;
	UINT	CurrentSubBuffer;
	UINT	NumSubBuffers;

	/** Size of all constants that has been updated since the last call to Commit. */
	UINT	CurrentUpdateSize;

	/**
	 * Size of all constants that has been updated since the last Discard.
	 * Includes "shared" constants that don't necessarily gets updated between every Commit.
	 */
	UINT	TotalUpdateSize;
};
