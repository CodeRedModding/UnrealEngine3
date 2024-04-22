/*=============================================================================
	D3D11ConstantBuffer.h: Public D3D Constant Buffer definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/** Size of the default constant buffer. */
#define MAX_GLOBAL_CONSTANT_BUFFER_SIZE		4096

/** Size of the image reflection constant buffers. */
#define MAX_IR_CONSTANT_BUFFER_SIZE			4096

/** Size of the bone constant buffer. */
#define BONE_CONSTANT_BUFFER_SIZE			(75*4*4*3)

// !!! These offsets must match the cbuffer register definitions in Common.usf !!!
enum ED3D11ShaderOffsetBuffer
{
	/** Default constant buffer. */
	GLOBAL_CONSTANT_BUFFER_INDEX = 0,
	/** Vertex shader view-dependent constants set in RHISetViewParameters. */
	VS_VIEW_CONSTANT_BUFFER_INDEX,
	PS_VIEW_CONSTANT_BUFFER_INDEX,
	VS_BONE_CONSTANT_BUFFER_INDEX,
	HS_VIEW_CONSTANT_BUFFER_INDEX,
	DS_VIEW_CONSTANT_BUFFER_INDEX,
	IMAGE_REFLECTION_CONSTANT_BUFFER1,
	IMAGE_REFLECTION_CONSTANT_BUFFER2,
	MAX_CONSTANT_BUFFER_SLOTS
};

/** Sizes of constant buffers defined in ED3D11ShaderOffsetBuffer. */
extern const UINT GConstantBufferSizes[MAX_CONSTANT_BUFFER_SLOTS];

// !!! These must match the cbuffer definitions in Common.usf !!!
struct FVertexShaderOffsetConstantBufferContents
{
	FMatrix ViewProjectionMatrix;
	FVector4 ViewOrigin;
	FVector4 PreViewTranslation;
#if WITH_REALD
	FVector4 VSRRealDCoefficients1;
#endif
};
struct FPixelShaderOffsetConstantBufferContents
{
	FVector4 ScreenPositionScaleBias;
	FVector4 MinZ_MaxZRatio;
	FLOAT NvStereoEnabled;
	FVector4 DiffuseOverrideParameter;
	FVector4 SpecularOverrideParameter;
	FVector4 ViewOrigin;
	FVector4 ScreenAndTexelSize;
#if WITH_REALD
	FLOAT PSRRealDCoefficients1;
#endif
};
struct FHullShaderOffsetConstantBufferContents
{
	FMatrix ViewProjectionMatrixHS;
	FLOAT AdaptiveTessellationFactor;
	FLOAT ProjectionScaleY;
};
struct FDomainShaderOffsetConstantBufferContents
{
	FMatrix ViewProjectionMatrixDS;
	FVector4 CameraPositionDS;
};

/**
 * A D3D constant buffer
 */
class FD3D11ConstantBuffer : public FRenderResource, public FRefCountedObject
{
public:

	FD3D11ConstantBuffer(FD3D11DynamicRHI* InD3DRHI,UINT InSize = 0,UINT SubBuffers = 1);
	~FD3D11ConstantBuffer();

	// FRenderResource interface.
	virtual void	InitDynamicRHI();
	virtual void	ReleaseDynamicRHI();

	void			UpdateConstant(const BYTE* Data, WORD Offset, WORD Size);
	UBOOL			CommitConstantsToDevice( UBOOL bDiscardSharedConstants );
	ID3D11Buffer*	GetConstantBuffer();

private:
	FD3D11DynamicRHI* D3DRHI;
	TRefCountPtr<ID3D11Buffer>* Buffers;
	UINT	MaxSize;
	UBOOL	IsDirty;
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
