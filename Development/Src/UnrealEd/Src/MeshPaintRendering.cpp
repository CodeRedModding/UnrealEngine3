/*================================================================================
	MeshPaintRendering.cpp: Mesh texture paint brush rendering
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/

#include "UnrealEd.h"
#include "../../Engine/Src/ScenePrivate.h"
#include "MeshPaintRendering.h"


namespace MeshPaintRendering
{

	/** Mesh paint vertex shader */
	class TMeshPaintVertexShader
		: public FGlobalShader
	{
		DECLARE_SHADER_TYPE( TMeshPaintVertexShader, Global );

	public:

		static UBOOL ShouldCache( EShaderPlatform Platform )
		{
			return TRUE;
		}

		static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
		{
	//		OutEnvironment.Definitions.Set(TEXT("NUM_SAMPLES"),*FString::Printf(TEXT("%u"),NumSamples));
		}

		/** Default constructor. */
		TMeshPaintVertexShader() {}

		/** Initialization constructor. */
		TMeshPaintVertexShader( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
			: FGlobalShader( Initializer )
		{
			TransformParameter.Bind( Initializer.ParameterMap, TEXT( "c_Transform" ) );
		}

		/** Serializer */
		virtual UBOOL Serialize( FArchive& Ar )
		{
			UBOOL bShaderHasOutdatedParameters = FShader::Serialize( Ar );
			Ar << TransformParameter;
			return bShaderHasOutdatedParameters;
		}

		/** Sets shader parameter values */
		void SetParameters( const FMatrix& InTransform )
		{
			SetVertexShaderValue( GetVertexShader(), TransformParameter, InTransform );
		}


	private:

		FShaderParameter TransformParameter;
	};


	IMPLEMENT_SHADER_TYPE( , TMeshPaintVertexShader, TEXT( "MeshPaintVertexShader" ), TEXT( "Main" ), SF_Vertex, 0, 0 );



	/** Mesh paint pixel shader */
	class TMeshPaintPixelShader
		: public FGlobalShader
	{
		DECLARE_SHADER_TYPE( TMeshPaintPixelShader, Global );

	public:

		static UBOOL ShouldCache(EShaderPlatform Platform)
		{
			return TRUE;
		}

		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
	// 		OutEnvironment.Definitions.Set(TEXT("NUM_SAMPLES"),*FString::Printf(TEXT("%u"),NumSamples));
		}

		/** Default constructor. */
		TMeshPaintPixelShader() {}

		/** Initialization constructor. */
		TMeshPaintPixelShader( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
			: FGlobalShader( Initializer )
		{
			// @todo MeshPaint: These shader params should not be optional (remove TRUE)
			CloneTextureParameter.Bind( Initializer.ParameterMap, TEXT( "s_CloneTexture" ), TRUE );
			WorldToBrushMatrixParameter.Bind( Initializer.ParameterMap, TEXT( "c_WorldToBrushMatrix" ), TRUE );
			BrushMetricsParameter.Bind( Initializer.ParameterMap, TEXT( "c_BrushMetrics" ), TRUE );
			BrushStrengthParameter.Bind( Initializer.ParameterMap, TEXT( "c_BrushStrength" ), TRUE );
			BrushColorParameter.Bind( Initializer.ParameterMap, TEXT( "c_BrushColor" ), TRUE );
			ChannelFlagsParameter.Bind( Initializer.ParameterMap, TEXT( "c_ChannelFlags"), TRUE);
			GenerateMaskFlagParameter.Bind( Initializer.ParameterMap, TEXT( "c_GenerateMaskFlag"), TRUE);
			GammaParameter.Bind( Initializer.ParameterMap, TEXT( "c_Gamma" ), TRUE );
		}

		/** Serializer */
		virtual UBOOL Serialize(FArchive& Ar)
		{
			UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
			Ar << CloneTextureParameter;
			Ar << WorldToBrushMatrixParameter;
			Ar << BrushMetricsParameter;
			Ar << BrushStrengthParameter;
			Ar << BrushColorParameter;
			Ar << ChannelFlagsParameter;
			Ar << GenerateMaskFlagParameter;
			Ar << GammaParameter;
			return bShaderHasOutdatedParameters;
		}

		/** Sets shader parameter values */
		void SetParameters( const FLOAT InGamma, const FMeshPaintShaderParameters& InShaderParams )
		{
			SetTextureParameter(
				GetPixelShader(),
				CloneTextureParameter,
				TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI(),
				InShaderParams.CloneTexture->GetRenderTargetResource()->TextureRHI );
				

			SetPixelShaderValue( GetPixelShader(), WorldToBrushMatrixParameter, InShaderParams.WorldToBrushMatrix );

			FVector4 BrushMetrics;
			BrushMetrics.X = InShaderParams.BrushRadius;
			BrushMetrics.Y = InShaderParams.BrushRadialFalloffRange;
			BrushMetrics.Z = InShaderParams.BrushDepth;
			BrushMetrics.W = InShaderParams.BrushDepthFalloffRange;
			SetPixelShaderValue( GetPixelShader(), BrushMetricsParameter, BrushMetrics );

			FVector4 BrushStrength4( InShaderParams.BrushStrength, 0.0f, 0.0f, 0.0f );
			SetPixelShaderValue( GetPixelShader(), BrushStrengthParameter, BrushStrength4 );

			SetPixelShaderValue( GetPixelShader(), BrushColorParameter, InShaderParams.BrushColor );

			FVector4 ChannelFlags;
			ChannelFlags.X = InShaderParams.RedChannelFlag;
			ChannelFlags.Y = InShaderParams.GreenChannelFlag;
			ChannelFlags.Z = InShaderParams.BlueChannelFlag;
			ChannelFlags.W = InShaderParams.AlphaChannelFlag;
			SetPixelShaderValue( GetPixelShader(), ChannelFlagsParameter, ChannelFlags );
			
			FLOAT MaskVal = InShaderParams.GenerateMaskFlag ? 1.0f : 0.0f;
			SetPixelShaderValue( GetPixelShader(), GenerateMaskFlagParameter, MaskVal );

			// @todo MeshPaint
			SetPixelShaderValue( GetPixelShader(), GammaParameter, InGamma );
			//RHISetRenderTargetBias(appPow(2.0f,GCurrentColorExpBias));
		}


	private:

		/** Texture that is a clone of the destination render target before we start drawing */
		FShaderResourceParameter CloneTextureParameter;
		
		/** Texture that stores the paint delta info.  Difference between this and clone will show paint area */

		/** Brush -> World matrix */
		FShaderParameter WorldToBrushMatrixParameter;

		/** Brush metrics: x = radius, y = falloff range, z = depth, w = depth falloff range */
		FShaderParameter BrushMetricsParameter;

		/** Brush strength */
		FShaderParameter BrushStrengthParameter;

		/** Brush color */
		FShaderParameter BrushColorParameter;

		/** Flags that control paining individual channels: x = Red, y = Green, z = Blue, w = Alpha */
		FShaderParameter ChannelFlagsParameter;
		
		/** Flag to control brush mask generation or paint blending */
		FShaderParameter GenerateMaskFlagParameter;

		/** Gamma */
		// @todo MeshPaint: Remove this?
		FShaderParameter GammaParameter;
	};


	IMPLEMENT_SHADER_TYPE( , TMeshPaintPixelShader, TEXT( "MeshPaintPixelShader" ), TEXT( "Main" ), SF_Pixel, 0, 0 );


	/** Mesh paint dilate vertex shader */
	class TMeshPaintDilateVertexShader
		: public FGlobalShader
	{
		DECLARE_SHADER_TYPE( TMeshPaintDilateVertexShader, Global );

	public:

		static UBOOL ShouldCache( EShaderPlatform Platform )
		{
			return TRUE;
		}

		static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
		{
			//		OutEnvironment.Definitions.Set(TEXT("NUM_SAMPLES"),*FString::Printf(TEXT("%u"),NumSamples));
		}

		/** Default constructor. */
		TMeshPaintDilateVertexShader() {}

		/** Initialization constructor. */
		TMeshPaintDilateVertexShader( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
			: FGlobalShader( Initializer )
		{
			TransformParameter.Bind( Initializer.ParameterMap, TEXT( "c_Transform" ) );
		}

		/** Serializer */
		virtual UBOOL Serialize( FArchive& Ar )
		{
			UBOOL bShaderHasOutdatedParameters = FShader::Serialize( Ar );
			Ar << TransformParameter;
			return bShaderHasOutdatedParameters;
		}

		/** Sets shader parameter values */
		void SetParameters( const FMatrix& InTransform )
		{
			SetVertexShaderValue( GetVertexShader(), TransformParameter, InTransform );
		}


	private:

		FShaderParameter TransformParameter;
	};


	IMPLEMENT_SHADER_TYPE( , TMeshPaintDilateVertexShader, TEXT( "MeshPaintDilateVertexShader" ), TEXT( "Main" ), SF_Vertex, 0, 0 );



	/** Mesh paint pixel shader */
	class TMeshPaintDilatePixelShader
		: public FGlobalShader
	{
		DECLARE_SHADER_TYPE( TMeshPaintDilatePixelShader, Global );

	public:

		static UBOOL ShouldCache(EShaderPlatform Platform)
		{
			return TRUE;
		}

		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
		}

		/** Default constructor. */
		TMeshPaintDilatePixelShader() {}

		/** Initialization constructor. */
		TMeshPaintDilatePixelShader( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
			: FGlobalShader( Initializer )
		{
			Texture0Parameter.Bind( Initializer.ParameterMap, TEXT( "s_Texture0" ), TRUE );
			Texture1Parameter.Bind( Initializer.ParameterMap, TEXT( "s_Texture1"), TRUE );
			Texture2Parameter.Bind( Initializer.ParameterMap, TEXT( "s_Texture2"), TRUE );
			WidthPixelOffsetParameter.Bind( Initializer.ParameterMap, TEXT( "c_WidthPixelOffset"), TRUE );
			HeightPixelOffsetParameter.Bind( Initializer.ParameterMap, TEXT( "c_HeightPixelOffset"), TRUE );
			//ChannelFlagsParameter.Bind( Initializer.ParameterMap, TEXT( "c_ChannelFlags"), TRUE);
			GammaParameter.Bind( Initializer.ParameterMap, TEXT( "c_Gamma" ), TRUE );
		}

		/** Serializer */
		virtual UBOOL Serialize(FArchive& Ar)
		{
			UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
			Ar << Texture0Parameter;
			Ar << Texture1Parameter;
			Ar << Texture2Parameter;
			Ar << WidthPixelOffsetParameter;
			Ar << HeightPixelOffsetParameter;
			//Ar << ChannelFlagsParameter;
			Ar << GammaParameter;
			return bShaderHasOutdatedParameters;
		}

		/** Sets shader parameter values */
		void SetParameters( const FLOAT InGamma, const FMeshPaintDilateShaderParameters& InShaderParams )
		{
			SetTextureParameter(
				GetPixelShader(),
				Texture0Parameter,
				TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI(),
				InShaderParams.Texture0->GetRenderTargetResource()->TextureRHI );

			SetTextureParameter(
				GetPixelShader(),
				Texture1Parameter,
				TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI(),
				InShaderParams.Texture1->GetRenderTargetResource()->TextureRHI );

			SetTextureParameter(
				GetPixelShader(),
				Texture2Parameter,
				TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI(),
				InShaderParams.Texture2->GetRenderTargetResource()->TextureRHI );

			SetPixelShaderValue( GetPixelShader(), WidthPixelOffsetParameter, InShaderParams.WidthPixelOffset );
			SetPixelShaderValue( GetPixelShader(), HeightPixelOffsetParameter, InShaderParams.HeightPixelOffset );


			SetPixelShaderValue( GetPixelShader(), GammaParameter, InGamma );
		}


	private:

		/** Texture0 */
		FShaderResourceParameter Texture0Parameter;

		/** Texture1 */
		FShaderResourceParameter Texture1Parameter;

		/** Texture2 */
		FShaderResourceParameter Texture2Parameter;

		/** Pixel size width */
		FShaderParameter WidthPixelOffsetParameter;
		
		/** Pixel size height */
		FShaderParameter HeightPixelOffsetParameter;

		/** Gamma */
		// @todo MeshPaint: Remove this?
		FShaderParameter GammaParameter;
	};


	IMPLEMENT_SHADER_TYPE( , TMeshPaintDilatePixelShader, TEXT( "MeshPaintDilatePixelShader" ), TEXT( "Main" ), SF_Pixel, 0, 0 );

	//TODO: end*********************************************************************************************************************






	/** Mesh paint vertex format */
	typedef FSimpleElementVertex FMeshPaintVertex;
// 	{
// 		FVector4 Position;
// 		FVector2D UV;
// 	};


	/** Mesh paint vertex declaration resource */
	typedef FSimpleElementVertexDeclaration FMeshPaintVertexDeclaration;
// 		: public FRenderResource
// 	{
// 	public:
// 		FVertexDeclarationRHIRef VertexDeclarationRHI;
// 
// 		/** Destructor. */
// 		virtual ~FMeshPaintVertexDeclaration() {}
// 
// 		virtual void InitRHI()
// 		{
// 			FVertexDeclarationElementList Elements;
// 			Elements.AddItem(FVertexElement(0,STRUCT_OFFSET(FMeshPaintVertex,Position),VET_Float4,VEU_Position,0));
// 			Elements.AddItem(FVertexElement(0,STRUCT_OFFSET(FMeshPaintVertex,TextureCoordinate),VET_Float2,VEU_TextureCoordinate,0));
// 			VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
// 		}
// 
// 		virtual void ReleaseRHI()
// 		{
// 			VertexDeclarationRHI.SafeRelease();
// 		}
// 	};




	/** Global mesh paint vertex declaration resource */
	TGlobalResource< FMeshPaintVertexDeclaration > GMeshPaintVertexDeclaration;



	typedef FSimpleElementVertex FMeshPaintDilateVertex;
	typedef FSimpleElementVertexDeclaration FMeshPaintDilateVertexDeclaration;
	TGlobalResource< FMeshPaintDilateVertexDeclaration > GMeshPaintDilateVertexDeclaration;


	/** Binds the mesh paint vertex and pixel shaders to the graphics device */
	void SetMeshPaintShaders_RenderThread( const FMatrix& InTransform,
										   const FLOAT InGamma,
										   const FMeshPaintShaderParameters& InShaderParams )
	{
		TShaderMapRef< TMeshPaintVertexShader > VertexShader( GetGlobalShaderMap() );

		// Set vertex shader parameters
		VertexShader->SetParameters( InTransform );

		TShaderMapRef< TMeshPaintPixelShader > PixelShader( GetGlobalShaderMap() );

		// Set pixel shader parameters
		PixelShader->SetParameters( InGamma, InShaderParams );


		// @todo MeshPaint: Make sure blending/color writes are setup so we can write to ALPHA if needed!

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState( BoundShaderState, GMeshPaintVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof( FMeshPaintVertex ) );
	}

	/** Binds the mesh paint vertex and pixel shaders to the graphics device */
	void SetMeshPaintDilateShaders_RenderThread( const FMatrix& InTransform,
		const FLOAT InGamma,
		const FMeshPaintDilateShaderParameters& InShaderParams )
	{
		TShaderMapRef< TMeshPaintDilateVertexShader > VertexShader( GetGlobalShaderMap() );

		// Set vertex shader parameters
		VertexShader->SetParameters( InTransform );

		TShaderMapRef< TMeshPaintDilatePixelShader > PixelShader( GetGlobalShaderMap() );

		// Set pixel shader parameters
		PixelShader->SetParameters( InGamma, InShaderParams );

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState( BoundShaderState, GMeshPaintDilateVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof( FMeshPaintDilateVertex ) );
	}

}
