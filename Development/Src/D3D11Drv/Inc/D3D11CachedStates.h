/*=============================================================================
	D3D11CachedStates.h: Helpers for caching d3d11 pipeline state
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * Keeps track of SamplerState combinations
 */
class FSamplerKey
{
public:
	D3D11_SAMPLER_DESC SamplerDesc;

public:
	FSamplerKey()
	{}
	FSamplerKey(FD3D11SamplerState* InState)
	{
		SamplerDesc = InState->SamplerDesc;
	}
	UBOOL operator==( const FSamplerKey& Other ) const
	{
		return 0 == appMemcmp(&SamplerDesc,&Other.SamplerDesc,sizeof(D3D11_SAMPLER_DESC));
	}
	UBOOL operator!=( const FSamplerKey& Other ) const
	{
		return appMemcmp(&SamplerDesc,&Other.SamplerDesc,sizeof(D3D11_SAMPLER_DESC));
	}
	FSamplerKey& operator=( const FSamplerKey& Other )
	{
		SamplerDesc = Other.SamplerDesc;
		return *this;
	}
	DWORD GetHash() const
	{
		// Try to hash off of the highest frequency member
		return (DWORD)SamplerDesc.Filter;
	}
	friend DWORD GetTypeHash( const FSamplerKey K )
	{
		return K.GetHash();
	}
};

/**
 * Keeps track of InputLayout combinations
 */
class FInputLayoutKey
{
public:
	ID3D11InputLayout* OriginalInputLayout;
	ID3D11VertexShader* OriginalVertexShader;

public:
	FInputLayoutKey() : OriginalInputLayout(NULL)
		, OriginalVertexShader(NULL)
	{}
	FInputLayoutKey(ID3D11InputLayout* InOriginalInputLayout,
		ID3D11VertexShader* InOriginalVertexShader) : OriginalInputLayout(InOriginalInputLayout)
		, OriginalVertexShader(InOriginalVertexShader)
	{}
	UBOOL operator==( const FInputLayoutKey& Other ) const
	{
		return OriginalInputLayout == Other.OriginalInputLayout && OriginalVertexShader == Other.OriginalVertexShader;
	}
	UBOOL operator!=( const FInputLayoutKey& Other ) const
	{
		return OriginalInputLayout != Other.OriginalInputLayout || OriginalVertexShader != Other.OriginalVertexShader;
	}
	FInputLayoutKey& operator=( const FInputLayoutKey& Other )
	{
		OriginalInputLayout = Other.OriginalInputLayout;
		OriginalVertexShader = Other.OriginalVertexShader;
		return *this;
	}
	DWORD GetHash() const
	{
		// Try to hash off of the highest frequency member
		return PointerHash( OriginalVertexShader );
	}
	friend DWORD GetTypeHash( const FInputLayoutKey K )
	{
		return K.GetHash();
	}
};

/**
 * Keeps track of DepthStencil combinations
 */
class FDepthStencilKey
{
public:
	D3D11_DEPTH_STENCIL_DESC DepthState;
	D3D11_DEPTH_STENCIL_DESC StencilState;

public:
	FDepthStencilKey()
	{}
	FDepthStencilKey(
		const D3D11_DEPTH_STENCIL_DESC& InDepthState,
		const D3D11_DEPTH_STENCIL_DESC& InStencilState
		):
		DepthState(InDepthState),
		StencilState(InStencilState)
	{
	}
	UBOOL operator==( const FDepthStencilKey& Other ) const
	{
		return 0 == appMemcmp(&DepthState,&Other.DepthState,sizeof(D3D11_DEPTH_STENCIL_DESC))
			&& 0 == appMemcmp(&StencilState,&Other.StencilState,sizeof(D3D11_DEPTH_STENCIL_DESC));
	}
	UBOOL operator!=( const FDepthStencilKey& Other ) const
	{
		return appMemcmp(&DepthState,&Other.DepthState,sizeof(D3D11_DEPTH_STENCIL_DESC))
			|| appMemcmp(&StencilState,&Other.StencilState,sizeof(D3D11_DEPTH_STENCIL_DESC));
	}
	FDepthStencilKey& operator=( const FDepthStencilKey& Other )
	{
		DepthState = Other.DepthState;
		StencilState = Other.StencilState;
		return *this;
	}
	DWORD GetHash() const
	{
		// Try to hash off of the highest frequency member
		return (DWORD)StencilState.FrontFace.StencilDepthFailOp;
	}

	friend DWORD GetTypeHash( const FDepthStencilKey K )
	{
		return K.GetHash();
	}
};

/**
 * Keeps track of RasterizerState combinations
 */
class FRasterizerKey
{
public:
	D3D11_RASTERIZER_DESC RasterizerState;
	INT DepthBias;
	UBOOL bScissorEnable;
	UBOOL bMultisampleEnable;

public:
	FRasterizerKey() : RasterizerState(), DepthBias(0), bScissorEnable(FALSE), bMultisampleEnable(FALSE)
	{}
	FRasterizerKey(
		const D3D11_RASTERIZER_DESC& InRasterizerState,
		INT InDepthBias,
		UBOOL bInScissorEnable,
		UBOOL bInMultisampleEnable
		):
		RasterizerState(InRasterizerState),
		DepthBias(InDepthBias),
		bScissorEnable(bInScissorEnable),
		bMultisampleEnable(bInMultisampleEnable)
	{
	}
	UBOOL operator==( const FRasterizerKey& Other ) const
	{
		return 0 == appMemcmp(&RasterizerState,&Other.RasterizerState,sizeof(D3D11_RASTERIZER_DESC)) 
			&& DepthBias == Other.DepthBias
			&& bScissorEnable == Other.bScissorEnable
			&& bMultisampleEnable == Other.bMultisampleEnable;
	}
	UBOOL operator!=( const FRasterizerKey& Other ) const
	{
		return appMemcmp(&RasterizerState,&Other.RasterizerState,sizeof(D3D11_RASTERIZER_DESC)) 
			|| DepthBias != Other.DepthBias
			|| bScissorEnable != Other.bScissorEnable
			|| bMultisampleEnable != Other.bMultisampleEnable;
	}
	FRasterizerKey& operator=( const FRasterizerKey& Other )
	{
		RasterizerState = Other.RasterizerState;
		DepthBias = Other.DepthBias;
		bScissorEnable = Other.bScissorEnable;
		bMultisampleEnable = Other.bMultisampleEnable;
		return *this;
	}
	DWORD GetHash() const
	{
		// Try to hash off of the highest frequency member
		return (DWORD)RasterizerState.CullMode;
	}
	friend DWORD GetTypeHash( const FRasterizerKey K )
	{
		return K.GetHash();
	}
};

/**
 * Keeps track of BlendState combinations
 */
class FBlendKey
{
public:
	D3D11_BLEND_DESC BlendState;
	TStaticArray<UINT8,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> EnabledStateValue;

public:
	FBlendKey()
	{
		EnabledStateValue = MakeUniformStaticArray<UINT8,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>(0);
		EnabledStateValue[0] = D3D11_COLOR_WRITE_ENABLE_ALL;
	}
	FBlendKey(
		const D3D11_BLEND_DESC& InBlendState,
		const TStaticArray<UINT8,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>& InEnabledStateValue
		) :
		BlendState(InBlendState),
		EnabledStateValue(InEnabledStateValue)
	{
	}
	UBOOL operator==( const FBlendKey& Other ) const
	{
		return 0 == appMemcmp(&BlendState,&Other.BlendState,sizeof(D3D11_BLEND_DESC))
			&& EnabledStateValue == Other.EnabledStateValue;
	}
	UBOOL operator!=( const FBlendKey& Other ) const
	{
		return appMemcmp(&BlendState,&Other.BlendState,sizeof(D3D11_BLEND_DESC))
			|| EnabledStateValue != Other.EnabledStateValue;
	}
	FBlendKey& operator=( const FBlendKey& Other )
	{
		BlendState = Other.BlendState;
		EnabledStateValue = Other.EnabledStateValue;
		return *this;
	}
	DWORD GetHash() const
	{
		// Try to hash off of the highest frequency member
		return (DWORD)BlendState.RenderTarget[0].SrcBlend;
	}
	friend DWORD GetTypeHash( const FBlendKey K )
	{
		return K.GetHash();
	}
};

// Set this to 1 to let D3D11 handle the state caching for us.  This can have an effect on PIX
// captures as it will capture all of the D3D11 state objects created.
#define LET_D3D11_CACHE_STATE 0
