/*=============================================================================
 ES2RHIImplementation.cpp: OpenGL ES 2.0 RHI definitions.
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "Engine.h"
#include "ES2RHIPrivate.h"

#if ANDROID
	#include <sys/sysinfo.h>
#endif

#if IPHONE
	#include "IPhoneObjCWrapper.h"
#endif

#if WITH_ES2_RHI

#if IPHONE || ANDROID
	/** Controls and variables for allowing and enabling MSAA */
	UBOOL GMSAAAllowed = TRUE;
	UBOOL GMSAAEnabled = TRUE;
	UBOOL GMSAAToggleRequest = FALSE;
#elif FLASH
	// @todo flash: Main branch thinks FLASH should have MSAA - which is right?
	UBOOL GMSAAAllowed = FALSE;
	UBOOL GMSAAEnabled = FALSE;
	UBOOL GMSAAToggleRequest = FALSE;
#endif

/** Number of ES2 viewports currently initialized */
TArray<FES2Viewport*> FES2Core::ActiveViewports;

/** The most recent viewport that was MakeCurrent'd */
FES2Viewport* FES2Core::CurrentViewport = NULL;


/** 
 * Whether we're currently rendering a depth only pass. 
 * This should be replaced with a pass specific shader once the ES2 RHI supports RHISetBoundShaderState properly instead of keying off of vertex declaration.
 */
UBOOL GMobileRenderingDepthOnly = FALSE;

/** 
 * Whether we're currently rendering a shadow (linear) depth to a shadow buffer. 
 * This should be replaced with a pass specific shader once the ES2 RHI supports RHISetBoundShaderState properly instead of keying off of vertex declaration.
 */
UBOOL GMobileRenderingShadowDepth = FALSE;

/** 
 * Whether we're currently rendering a forward shadow projection.
 * This should be replaced with a pass specific shader once the ES2 RHI supports RHISetBoundShaderState properly instead of keying off of vertex declaration.
 */
UBOOL GMobileRenderingForwardShadowProjections = FALSE;

UBOOL GES2MapBuffer = FALSE;

/*
 *	NVIDIA's nonlinear-depth extension for 16-bit depth only devices
 */
UBOOL GSupports16BitNonLinearDepth = FALSE;

UBOOL GSupportsHalfFloatVertexAttribs = FALSE;

#if ANDROID
	extern UINT GAndroidDeviceMemory;
#endif

// Name of GPU Vendor and Renderer returned by OpenGL
FString GGraphicsVendor = FString(TEXT(""));
FString GGraphicsRenderer = FString(TEXT(""));

/**
 * Checks the OpenGL extensions and sets up global variables according to what the device supports,
 * e.g. GTextureFormatSupport and GSupportsDepthTextures.
 */
void CheckOpenGLExtensions()
{
#if FLASH
	UBOOL bSupportsDXT = TRUE;
	UBOOL bSupportsPVRTC = FALSE;
	UBOOL bSupportsATITC = FALSE;
	UBOOL bSupportsETC = FALSE;
	UBOOL bSupportsDepthTextures = FALSE;
	INT MaxAnisotropy = 1;
#else
	// Get the extension string, adding a space before and after to make sure we have delimiters for everything.
	FString GLExtensions(TEXT(" "));
	GLExtensions += FString( ( const ANSICHAR* )glGetString( GL_EXTENSIONS ) );
	GLExtensions += TEXT(" ");

	// Look for specific extensions in the string.
	//GMSAADiscardSupported = GLExtensions.InStr( TEXT(" GL_EXT_discard_framebuffer "), FALSE, TRUE ) != INDEX_NONE;
	UBOOL bSupportsDXT = GLExtensions.InStr( TEXT(" GL_EXT_texture_compression_s3tc "), FALSE, TRUE ) != INDEX_NONE;
	UBOOL bSupportsPVRTC = GLExtensions.InStr( TEXT(" GL_IMG_texture_compression_pvrtc "), FALSE, TRUE ) != INDEX_NONE;
	UBOOL bSupportsATITC = GLExtensions.InStr( TEXT(" GL_ATI_texture_compression_atitc "), FALSE, TRUE ) != INDEX_NONE;
	UBOOL bSupportsETC = GLExtensions.InStr( TEXT(" GL_OES_compressed_ETC1_RGB8_texture "), FALSE, TRUE ) != INDEX_NONE;

	bSupportsATITC = bSupportsATITC || GLExtensions.InStr( TEXT(" GL_AMD_compressed_ATC_texture "), FALSE, TRUE ) != INDEX_NONE;

	UBOOL bSupportsDepthTextures =
		GLExtensions.InStr( TEXT(" GL_ARB_depth_texture "), FALSE, TRUE ) != INDEX_NONE ||
		GLExtensions.InStr( TEXT(" GL_OES_depth_texture "), FALSE, TRUE ) != INDEX_NONE;

	GPlatformFeatures.bSupportsRendertargetDiscard = GLExtensions.InStr( TEXT(" GL_EXT_discard_framebuffer "), FALSE, TRUE ) != INDEX_NONE;

	GGraphicsVendor = FString( ( const ANSICHAR* )glGetString( GL_VENDOR ) );
	GGraphicsRenderer = FString( ( const ANSICHAR* )glGetString( GL_RENDERER ) );

	debugf( TEXT("Vendor: %s"), *GGraphicsVendor);
	debugf( TEXT("Renderer: %s"), *GGraphicsRenderer);

#if ANDROID
	// Grab the device memory from sysinfo on Android devices
	struct sysinfo SysInfo;
	sysinfo(&SysInfo);
	GAndroidDeviceMemory = SysInfo.totalram * SysInfo.mem_unit;
	debugf( TEXT("Total Device Memory: %u MB"), GAndroidDeviceMemory / 1024 / 1024);
#endif

	// Check for devices that don't support discard (currently just Adreno 205 cards). It looks like newer devices label the Adreno 205 as "Adreno (TM) 205", updating the discard check to accommodate the new name.
	GMobileAllowShaderDiscard = (GGraphicsRenderer != TEXT("Adreno 205")) && (GGraphicsRenderer != TEXT("Adreno (TM) 205"));

	// Check for Mali GPUs, which have issues properly applying uv offsets
	GMobileDeviceAllowBumpOffset = GGraphicsVendor.InStr( TEXT("Mali-400 MP"), FALSE, TRUE ) == INDEX_NONE;

	// Check for devices that are not tiled renderers (currently just NVIDIA GPUs)
	GMobileTiledRenderer = GGraphicsVendor.InStr( TEXT("NVIDIA"), FALSE, TRUE ) == INDEX_NONE;

	// Check for NVIDIA's nonlinear-depth extension for 16-bit depth only devices
	GSupports16BitNonLinearDepth = GLExtensions.InStr( TEXT(" GL_NV_depth_nonlinear "), FALSE, TRUE ) != INDEX_NONE;

	// Check for packed depth stencil support
	GMobileUsePackedDepthStencil = GLExtensions.InStr( TEXT(" OES_packed_depth_stencil "), FALSE, TRUE ) != INDEX_NONE;

	// Check for half float vertex attrib support
	GSupportsHalfFloatVertexAttribs = GLExtensions.InStr( TEXT(" GL_OES_vertex_half_float "), FALSE, TRUE ) != INDEX_NONE;

	// @hack Android Check for devices that may have incorrect glCheckFramebufferStatus implementations (qualcomm) 
	GMobileAllowFramebufferStatusCheck = GGraphicsVendor.InStr( TEXT("Qualcomm"), FALSE, TRUE ) == INDEX_NONE;

	debugf(TEXT("Supports Shader Discard: %s"), GMobileAllowShaderDiscard ? TEXT("TRUE") : TEXT("FALSE"));

	debugf(TEXT("Supports Shader Bump Offset: %s"), GMobileDeviceAllowBumpOffset ? TEXT("TRUE") : TEXT("FALSE"));

	INT MaxAnisotropy = 1;
	UBOOL bSupportsAnisotropy = GLExtensions.InStr( TEXT(" GL_EXT_texture_filter_anisotropic "), FALSE, TRUE ) != INDEX_NONE;
	if ( bSupportsAnisotropy )
	{
		glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &MaxAnisotropy);
		MaxAnisotropy = Max( MaxAnisotropy, 1 );
	}

#if !_WINDOWS
    GES2MapBuffer = GLExtensions.InStr( TEXT(" GL_OES_mapbuffer ") ) != INDEX_NONE;
#endif
#endif

	// Setup the texture format support bit mask.
	GTextureFormatSupport = bSupportsDXT ? TEXSUPPORT_DXT : 0;
	GTextureFormatSupport |= bSupportsPVRTC ? TEXSUPPORT_PVRTC : 0;
	GTextureFormatSupport |= bSupportsATITC ? TEXSUPPORT_ATITC : 0;
	GTextureFormatSupport |= bSupportsETC ? TEXSUPPORT_ETC : 0;

	debugf(TEXT("ES2 Texture Support [ %s %s %s %s ]"), (GTextureFormatSupport & TEXSUPPORT_DXT) ? TEXT("DXT") : TEXT("-"), 
		(GTextureFormatSupport & TEXSUPPORT_PVRTC) ? TEXT("PVRTC") : TEXT("-"), (GTextureFormatSupport & TEXSUPPORT_ATITC) ? TEXT("ATITC") : TEXT("-"),
		(GTextureFormatSupport & TEXSUPPORT_ETC) ? TEXT("ETC") : TEXT("-"));

	GSupportsDepthTextures = bSupportsDepthTextures;

	GPlatformFeatures.MaxTextureAnisotropy = MaxAnisotropy;

#if !FINAL_RELEASE && !FLASH
	// print out some specifics about this implementation...
	GLboolean DriverCompiler = GL_FALSE;
	glGetBooleanv( GL_SHADER_COMPILER, &DriverCompiler );

	GLint NumShaderBinaryFormats = 0;
	glGetIntegerv( GL_NUM_SHADER_BINARY_FORMATS, &NumShaderBinaryFormats );

	debugf(TEXT("Driver Compiler? ... %s, Num Binary Formats = %d"), DriverCompiler ? TEXT("yes") : TEXT("no"), NumShaderBinaryFormats );

	// Log supported GL extensions
	debugf( TEXT("Extensions: %s"), ANSI_TO_TCHAR(glGetString(GL_EXTENSIONS)));

	GLint QueryValue;
	glGetError();	// Reset the error code.
#define PRINT_GL_LIMIT( lim ) glGetIntegerv( lim, &QueryValue ); (glGetError() == GL_NO_ERROR) ? debugf( TEXT("%s = %d"), TEXT( #lim ), QueryValue ) : debugf( TEXT("%s = Not supported"), TEXT( #lim ) )

	PRINT_GL_LIMIT( GL_MAX_VERTEX_UNIFORM_VECTORS );
	PRINT_GL_LIMIT( GL_MAX_VARYING_VECTORS );
	PRINT_GL_LIMIT( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS );
	PRINT_GL_LIMIT( GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS );
	PRINT_GL_LIMIT( GL_MAX_TEXTURE_IMAGE_UNITS );
	PRINT_GL_LIMIT( GL_MAX_FRAGMENT_UNIFORM_VECTORS );
	PRINT_GL_LIMIT( GL_MAX_TEXTURE_SIZE );
	PRINT_GL_LIMIT( GL_MAX_VIEWPORT_DIMS );
	PRINT_GL_LIMIT( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT );
	PRINT_GL_LIMIT( GL_SUBPIXEL_BITS );
#endif

	// Print this one differently, since we want to cache the value
	extern GLint GMaxVertexAttribsGLSL;
#if FLASH
	GMaxVertexAttribsGLSL = 16;
#else
	glGetIntegerv( GL_MAX_VERTEX_ATTRIBS, &GMaxVertexAttribsGLSL );
	debugf( TEXT("%s = %d"), TEXT( "GL_MAX_VERTEX_ATTRIBS" ), GMaxVertexAttribsGLSL );
#endif
}

/**
 * Initializes the ES2 rendering system
 */
void FES2Core::InitES2Core()
{
	GUsingES2RHI = TRUE;
	GUsingMobileRHI = TRUE;

	// TODO: get vertex sharing to work on RHI Resets (however do see much better performance on Tegra2 at least
	// without this on
	if( GAllowFullRHIReset )
	{
		GSystemSettings.bShareVertexShaders = FALSE;
		GSystemSettings.bSharePixelShaders = FALSE;
		GSystemSettings.bShareShaderPrograms = FALSE;
	}

	// we want PP if we want any bloom
	// @todo Make a mobile specific PP setting, this is redundant to a later call where we set the value again
	GMobileAllowPostProcess = GSystemSettings.bAllowBloom || GSystemSettings.bAllowLightShafts || GSystemSettings.bAllowDepthOfField || GSystemSettings.bAllowMobileColorGrading;

	// disable Half2 and Pos3N vertex formats in the engine, as they aren't supported by ES2
	GVertexElementTypeSupport.SetSupported(VET_Half2, GSupportsHalfFloatVertexAttribs);
	GVertexElementTypeSupport.SetSupported(VET_Pos3N, FALSE);
}


/**
 * Shuts down the ES2 renderer
 */
void FES2Core::DestroyES2Core()
{
	check( IsInGameThread() );
	if( GIsRHIInitialized )
	{
		// Shutdown the render manager
		extern FES2RenderManager GRenderManager;
		GRenderManager.ExitRHI();

		// Ask all initialized FRenderResources to release their RHI resources.
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->ReleaseRHI();
		}
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->ReleaseDynamicRHI();
		}

		GIsRHIInitialized = FALSE;
	}
}

/**
 * Called when a new viewport is created
 *
 * @param	Viewport	The newly created viewport
 * @param	NativeWindowHandle Native window handle, retrieved from the FViewport::GetWindow()
 */
void FES2Core::OnViewportCreated( FES2Viewport* Viewport, void* NativeWindowHandle )
{
	extern void PlatformInitializeViewport(FES2Viewport* Viewport, void* NativeWindowHandle);
	PlatformInitializeViewport(Viewport, NativeWindowHandle);

	// only do this the first time
	if (!GIsRHIInitialized)
	{
		SetupPlatformExtensions();

#if FLASH
		// Blob shadows not supported with Flash ES2 yet
		GSystemSettings.bAllowDynamicShadows = FALSE;
#endif // #if !FLASH

		// Disable projected mod shadows if the device doesn't support depth textures.
		if ( !GSupportsDepthTextures && GSystemSettings.bAllowDynamicShadows && GSystemSettings.bMobileModShadows )
		{
			debugf(TEXT("Disabling projected mod shadows because depth textures are not supported on this device."));
			GSystemSettings.bAllowDynamicShadows = FALSE;
			GSystemSettings.bMobileModShadows = FALSE;
		}

#if IPHONE
		FLOAT iPhoneOSVersion = IPhoneGetOSVersion();
		EIOSDevice iPhoneDeviceType = IPhoneGetDeviceType();
		FString iPhoneDeviceName = IPhoneGetDeviceTypeString( iPhoneDeviceType );
		debugf(TEXT("iOS device: %s, OS version: %.2f"), *iPhoneDeviceName, iPhoneOSVersion);
		debugf(TEXT("Mobile settings: %s"), appGetMobileSystemSettingsSectionName());

		// Disable projected mod shadows if the device is using an old OS version.
		if ( iPhoneOSVersion < 5.0f && GSystemSettings.bAllowDynamicShadows && GSystemSettings.bMobileModShadows )
		{
			debugf(TEXT("Disabling projected mod shadows because this device isn't using iOS 5 or later."));
			GSystemSettings.bAllowDynamicShadows = FALSE;
			GSystemSettings.bMobileModShadows = FALSE;
		}

		// Disable light shafts ("god rays") if the device is using an old OS version.
		if ( iPhoneOSVersion < 5.0f && GSystemSettings.bAllowLightShafts )
		{
			debugf(TEXT("Disabling light shafts because this device isn't using iOS 5 or later."));
			GSystemSettings.bAllowLightShafts = FALSE;
		}

		// Drop detail level if the device is a mid-tier device and is also using an old OS version
		// @todo ib2merge: This needs to be documented if we leave it in generically for all games
/*
		if ( iPhoneOSVersion < 5.0f  && GSystemSettings.DetailMode == DM_Medium )
		{
			GSystemSettings.DetailMode--;
			GSystemSettings.ParticleLODBias++;
		}
*/
#endif

		GSystemSettings.MaxAnisotropy = Min( GSystemSettings.MaxAnisotropy, GPlatformFeatures.MaxTextureAnisotropy );

		// Disable MSAA if we're going to use projected mod shadows.  IPhone handles this in EAGLView.mm
#if ANDROID
		if ( GSystemSettings.bAllowDynamicShadows && GSystemSettings.bMobileModShadows && GMSAAAllowed && GMSAAEnabled )
		{
			debugf(TEXT("Disabling MSAA because projected mod shadows are enabled."));
			GMSAAEnabled = FALSE;
		}
#endif

		// Disable projected mod shadows if the device doesn't support depth textures.
		if ( !GSupportsDepthTextures && GSystemSettings.bAllowDynamicShadows && GSystemSettings.bMobileModShadows )
		{
			debugf(TEXT("Disabling projected mod shadows because depth textures are not supported on this device."));
			GSystemSettings.bAllowDynamicShadows = FALSE;
			GSystemSettings.bMobileModShadows = FALSE;
		}

		// Texture streaming is not yet supported with ES2 renderer.  However, by this time it's too late
		// to turn it off.  It must be globally disabled at compile time, via .ini file or command-line.
		check( GUseTextureStreaming == FALSE );

		// no half-pixel offset in GL
		GPixelCenterOffset = 0.0f;

		// Screen door fades aren't supported on mobile platforms yet
		GAllowScreenDoorFade = FALSE;

		// we want PP if we want any bloom
		GMobileAllowPostProcess = GSystemSettings.bAllowBloom || GSystemSettings.bAllowLightShafts || GSystemSettings.bAllowDepthOfField || GSystemSettings.bAllowMobileColorGrading;

#if IPHONE
		//@FIXME: temp until super-slowdown fix is in - disable PP on old OS versions
		if (iPhoneOSVersion < 5.0f)
		{
			GMobileAllowPostProcess = FALSE;
		}
#endif

		// initialize the shadow state (just zero it out)
		appMemzero( &GStateShadow, sizeof(FStateShadow) );

		// Initialize default shadow values for states that we always want to set be at least once
		// the first time they're encountered.  We do this by setting the shadow values to an 
		// out of range index.
		GStateShadow.ColorWriteEnable = INDEX_NONE;
		GStateShadow.Blend.AlphaTest = (ECompareFunction)INDEX_NONE;
		GStateShadow.Rasterizer.FillMode = (ERasterizerFillMode)INDEX_NONE;
		GStateShadow.Rasterizer.CullMode = (ERasterizerCullMode)INDEX_NONE;
		GStateShadow.Depth.bEnableDepthWrite = INDEX_NONE;
		GStateShadow.Depth.DepthTest = (ECompareFunction)INDEX_NONE;

		debugf(TEXT("Viewport resolution: [%dx%d]"), Viewport->SizeX, Viewport->SizeY);

		// make sure that glGetError is initialized to 0 for all future GL_CHECK calls
#if !FLASH
		glGetError();
#endif

		// Initialization the render manager
		extern FES2RenderManager GRenderManager;
		GRenderManager.InitRHI();

		// Initialization of the shader manager
		GShaderManager.InitRHI();

		GLCHECK(glEnable( GL_DEPTH_TEST ));

		// Occlusion query support on ES2 platforms is always disabled for now but can
		// be enabled, per-platform, based on platform-specific conditions
		extern UBOOL GIgnoreAllOcclusionQueries;
		GIgnoreAllOcclusionQueries = TRUE;

		// fixup resources that couldn't be initialized before now
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->InitDynamicRHI();
		}
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->InitRHI();
		}
	}

	MakeCurrent(Viewport);

	// create a surface that wraps the preset BackColorRenderBuffer (the actual displayed buffer on present)
	Viewport->ViewportBackBuffer = new FES2Surface(Viewport->SizeX, Viewport->SizeY, Viewport->BackBufferName);
#if IPHONE
	if( GMSAAAllowed )
	{
		// If MSAA is allowed, we need to create both with and without so we can toggle
		Viewport->ViewportBackBufferMSAA = new FES2Surface(Viewport->SizeX, Viewport->SizeY, Viewport->MSAABackBufferName);
	}
#endif

	// Two reasons to create a depth buffer:
	//     1. If we aren't using offscreen rendering and the BackBufferName is not 0, which means there is no default
	//        depth buffer (iOS vs Android) and the platform needs to have one set up
	//     2. This is the primary (first) viewport - secondary viewports are currently only used for UI/HUD rendering
	// Reason 1 is handled in the if here, reason 2 is handled below
	if (!GMobileAllowPostProcess && Viewport->BackBufferName != 0)
	{
		// create a non-MSAA depth buffer
		if ( GSupportsDepthTextures && ActiveViewports.Num() == 0 )
		{
			Viewport->ViewportDepthBufferTexture = RHICreateTexture2D(Viewport->SizeX,Viewport->SizeY,PF_DepthStencil,1,TexCreate_ResolveTargetable|TexCreate_DepthStencil,NULL);
			Viewport->ViewportDepthBuffer = new FES2Surface(Viewport->ViewportDepthBufferTexture, NULL, 0);
		}
		else
		{
			Viewport->ViewportDepthBuffer =
				// If this is not the primary (first) viewport, create a placeholder surface
				ActiveViewports.Num() == 0 ?
					new FES2Surface(Viewport->SizeX, Viewport->SizeY, PF_DepthStencil, 0) :
					new FES2Surface(Viewport->SizeX, Viewport->SizeY);
		}

		// create a depth buffer
#if IPHONE
		// If MSAA is allowed, we need to create an MSAA depth buffer as well, so we can toggle
		if( GMSAAAllowed )
		{
			//@TODO: Create a resolve target for the MSAA depth buffer, if GSupportsDepthTextures == TRUE.
//			if ( GSupportsDepthTextures )
			{
			    Viewport->ViewportDepthBufferMSAA =
				    // If this is not the primary (first) viewport, create a placeholder surface
				    ActiveViewports.Num() == 0 ?
					    new FES2Surface(Viewport->SizeX, Viewport->SizeY, PF_DepthStencil, 4) :
					    new FES2Surface(Viewport->SizeX, Viewport->SizeY);
			}

			// Now bind the set we want to use
			if( GMSAAEnabled )
			{
				RHISetRenderTarget(Viewport->ViewportBackBufferMSAA, Viewport->ViewportDepthBufferMSAA);
			}
			else
			{ 
				RHISetRenderTarget(Viewport->ViewportBackBuffer, Viewport->ViewportDepthBuffer);
			}
		}
		else
#endif
		{
			// Now bind the color and depth buffers to the current framebuffer
			// @todo: Why is this here?
			RHISetRenderTarget(Viewport->ViewportBackBuffer, Viewport->ViewportDepthBuffer);
		}
	}

	// Did we not create a specific depth buffer?
	if ( Viewport->BackBufferName == 0 && IsValidRef( Viewport->ViewportDepthBuffer ) == FALSE )
	{
		// Create a FES2Surface object that represents the default depth buffer. Won't allocate anything.
		Viewport->ViewportDepthBuffer = new FES2Surface(Viewport->SizeX, Viewport->SizeY, Viewport->BackBufferName);
#if IPHONE
		if( GMSAAAllowed )
		{
			// If MSAA is allowed, we need to create both with and without so we can toggle
			Viewport->ViewportDepthBufferMSAA = new FES2Surface(Viewport->SizeX, Viewport->SizeY, Viewport->MSAABackBufferName);
		}
#endif
	}

	// add this to the list of viewports
	ActiveViewports.AddItem(Viewport);

	// only count the RHI as initialized after the first viewport is made
	GIsRHIInitialized = TRUE;
}

/**
 * Called when a new viewport is destroyed
 *
 * @param	Viewport	The viewport that's currently being destroyed
 */
void FES2Core::OnViewportDestroyed( FES2Viewport* Viewport )
{
	checkSlow(ActiveViewports.ContainsItem(Viewport));
	ActiveViewports.RemoveItem(Viewport);

	extern void PlatformDestroyViewport(FES2Viewport* Viewport);
	PlatformDestroyViewport(Viewport);

	// TODO: these deletes should work fine, but end up crashing on iOS due
	// to a double-free somewhere. Not high-priority, since Instruments
	// indicates that memory remains stable, but it seems like this
	// should be leaking if we allocated offscreen surfaces.

// 	// Free all allocated surfaces
// 	if( Viewport->ViewportBackBuffer )
// 	{
// 		delete Viewport->ViewportBackBuffer;
// 		Viewport->ViewportBackBuffer = NULL;
// 	}
// 	if( Viewport->ViewportDepthBuffer )
// 	{
// 		delete Viewport->ViewportDepthBuffer;
// 		Viewport->ViewportDepthBuffer = NULL;
// 	}
// #if IPHONE || ANDROID
// 	if( Viewport->ViewportBackBufferMSAA )
// 	{
// 		delete Viewport->ViewportBackBufferMSAA;
// 		Viewport->ViewportBackBufferMSAA = NULL;
// 	}
// 	if( Viewport->ViewportDepthBufferMSAA )
// 	{
// 		delete Viewport->ViewportDepthBufferMSAA;
// 		Viewport->ViewportDepthBufferMSAA = NULL;
// 	}
// #endif

	if( ActiveViewports.Num() == 0 )
	{
		// No more viewports
		DestroyES2Core();
	}
}

/**
 * Called when a viewport needs to present it's back buffer
 */
void FES2Core::SwapBuffers(FES2Viewport* Viewport)
{
	// if no viewport passed in, use the first viewport
	if (Viewport == NULL)
	{
		checkSlow(ActiveViewports.Num());
		Viewport = ActiveViewports(0);
	}

	// let the platform deal with it
	extern void PlatformSwapBuffers(FES2Viewport* Viewport);
	PlatformSwapBuffers(Viewport);
}

/**
 * Called when a viewport needs to present it's back buffer
 */
void FES2Core::MakeCurrent(FES2Viewport* Viewport)
{
	// if no viewport passed in, use the first viewport
	if (Viewport == NULL)
	{
		checkSlow(ActiveViewports.Num());
		Viewport = ActiveViewports(0);
	}

	// if already current, do nothing
	if (CurrentViewport == Viewport)
	{
		return;
	}

	// let the platform deal with it
	extern void PlatformMakeCurrent(FES2Viewport* Viewport);
	PlatformMakeCurrent(Viewport);

	CurrentViewport = Viewport;

	// make sure it's currently the back buffer
	RHISetRenderTarget(Viewport->ViewportBackBuffer, Viewport->ViewportDepthBuffer);
}

/**
 * Called when a viewport needs to present it's back buffer
 */
void FES2Core::UnmakeCurrent(FES2Viewport* Viewport)
{
	// if no viewport passed in, use the first viewport
	if (Viewport == NULL)
	{
		checkSlow(ActiveViewports.Num());
		Viewport = ActiveViewports(0);
	}

	if (CurrentViewport == NULL)
	{
		return;
	}

	// let the platform deal with it
	extern void PlatformUnmakeCurrent(FES2Viewport* Viewport);
	PlatformUnmakeCurrent(Viewport);

	// nothing is current
	CurrentViewport = NULL;
}

/**
 * Checks the OpenGL extensions to see what the platform supports.
 */
void FES2Core::SetupPlatformExtensions()
{
	// make sure GTextureFormatSupport has been set up
	CheckOpenGLExtensions();

    GPixelFormats[PF_G8			].PlatformFormat	= GL_LUMINANCE;

	// overload the DXT texture formats with PVRTC formats on the iPhone
	if ( GTextureFormatSupport & TEXSUPPORT_PVRTC )
	{
		GPixelFormats[PF_DXT1			].PlatformFormat	= GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;
		GPixelFormats[PF_DXT1			].BlockBytes		= 8;
		GPixelFormats[PF_DXT1			].BlockSizeX		= 8;
		GPixelFormats[PF_DXT1			].BlockSizeY		= 4;
		GPixelFormats[PF_DXT3			].PlatformFormat	= GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
		GPixelFormats[PF_DXT3			].BlockBytes		= 8;
		GPixelFormats[PF_DXT3			].BlockSizeX		= 4;
		GPixelFormats[PF_DXT3			].BlockSizeY		= 4;
		GPixelFormats[PF_DXT5			].PlatformFormat	= GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
		GPixelFormats[PF_DXT5			].BlockBytes		= 8;
		GPixelFormats[PF_DXT5			].BlockSizeX		= 4;
		GPixelFormats[PF_DXT5			].BlockSizeY		= 4;
		GPixelFormats[PF_A8R8G8B8		].PlatformFormat	= GL_RGBA;

		GES2PixelFormats[PF_DXT1].Setup( GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG, GL_RGBA, GL_UNSIGNED_BYTE, TRUE );
		GES2PixelFormats[PF_DXT3].Setup( GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, GL_RGBA, GL_UNSIGNED_BYTE, TRUE );
		GES2PixelFormats[PF_DXT5].Setup( GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, GL_RGBA, GL_UNSIGNED_BYTE, TRUE );
	}
	else if ( GTextureFormatSupport & TEXSUPPORT_DXT )
	{
		GPixelFormats[PF_DXT1			].PlatformFormat	= GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		GPixelFormats[PF_DXT3			].PlatformFormat	= GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		GPixelFormats[PF_DXT5			].PlatformFormat	= GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		GPixelFormats[PF_A8R8G8B8		].PlatformFormat	= GL_RGBA;

		GES2PixelFormats[PF_DXT1].Setup( GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_RGBA, GL_UNSIGNED_BYTE, TRUE );
		GES2PixelFormats[PF_DXT3].Setup( GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_RGBA, GL_UNSIGNED_BYTE, TRUE );
		GES2PixelFormats[PF_DXT5].Setup( GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_RGBA, GL_UNSIGNED_BYTE, TRUE );
	}
	else if ( GTextureFormatSupport & TEXSUPPORT_ATITC )
	{
		GPixelFormats[PF_DXT1			].PlatformFormat	= GL_ATC_RGB_AMD;
		GPixelFormats[PF_DXT3			].PlatformFormat	= GL_ATC_RGBA_EXPLICIT_ALPHA_AMD;
		GPixelFormats[PF_DXT5			].PlatformFormat	= GL_ATC_RGBA_EXPLICIT_ALPHA_AMD;
		GPixelFormats[PF_A8R8G8B8		].PlatformFormat	= GL_RGBA;

		GES2PixelFormats[PF_DXT1].Setup( GL_ATC_RGB_AMD, GL_RGBA, GL_UNSIGNED_BYTE, TRUE );
		GES2PixelFormats[PF_DXT3].Setup( GL_ATC_RGBA_EXPLICIT_ALPHA_AMD, GL_RGBA, GL_UNSIGNED_BYTE, TRUE );
		GES2PixelFormats[PF_DXT5].Setup( GL_ATC_RGBA_EXPLICIT_ALPHA_AMD, GL_RGBA, GL_UNSIGNED_BYTE, TRUE );
	}
	else if (GTextureFormatSupport & TEXSUPPORT_ETC) 
	{
		GPixelFormats[PF_DXT1			].PlatformFormat	= GL_ETC1_RGB8_OES;
		GPixelFormats[PF_DXT3			].PlatformFormat	= GL_RGBA;
		GPixelFormats[PF_DXT3			].BlockBytes		= 4;
		GPixelFormats[PF_DXT3			].BlockSizeX		= 1;
		GPixelFormats[PF_DXT3			].BlockSizeY		= 1;	
		GPixelFormats[PF_DXT5			].PlatformFormat	= GL_RGBA;
		GPixelFormats[PF_DXT5			].BlockBytes		= 4;
		GPixelFormats[PF_DXT5			].BlockSizeX		= 1;
		GPixelFormats[PF_DXT5			].BlockSizeY		= 1;
		GPixelFormats[PF_A8R8G8B8		].PlatformFormat	= GL_RGBA;

		GES2PixelFormats[PF_DXT1].Setup( GL_ETC1_RGB8_OES, GL_RGBA, GL_UNSIGNED_BYTE, TRUE );
		GES2PixelFormats[PF_DXT3].Setup( GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, FALSE );
		GES2PixelFormats[PF_DXT5].Setup( GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, FALSE );
	}
	else
	{
		appErrorf( TEXT("This platform doesn't support Unreal Engine texture formats!") );
	}
}






#if USE_STATIC_RHI

/**
 * Called at startup to initialize the static ES2 RHI
 *
 * @return The ES2 RHI
 */
FES2RHI* CreateStaticRHI()
{
	// Startup ES2 renderer
	FES2RHIExt* NewRHI = new FES2RHIExt();
	FES2Core::InitES2Core();
	return NewRHI;
}

/** ES2RHI destructor, called before shut down */
FES2RHIExt::~FES2RHIExt()
{
	// Shutdown ES2 renderer
	FES2Core::DestroyES2Core();
}


#endif


#if USE_DYNAMIC_RHI

/**
 * Called at startup to initialize the dynamic ES2 RHI
 *
 * @return The ES2 RHI
 */
FDynamicRHI* ES2CreateRHI()
{
	// Startup ES2 renderer
	FES2RHI* NewRHI = new FES2RHI();
	FES2Core::InitES2Core();
	return NewRHI;
}

/** ES2RHI destructor, called before shut down */
FES2RHI::~FES2RHI()
{
	// Shutdown ES2 renderer
	FES2Core::DestroyES2Core();
}

#endif





/** FES2Viewport constructor */
FES2Viewport::FES2Viewport( void* InWindowHandle, UINT InSizeX, UINT InSizeY, UBOOL bInIsFullscreen )
	: BackBufferName(0)
	, MSAABackBufferName(0)
	, SizeX(InSizeX)
	, SizeY(InSizeY)
	, bIsFullscreen(bInIsFullscreen)
	, ViewportBackBuffer(NULL)
	, ViewportDepthBuffer(NULL)
#if IPHONE || ANDROID
	, ViewportBackBufferMSAA(NULL)
	, ViewportDepthBufferMSAA(NULL)
#endif
{
debugf(TEXT("FES2Viewport::FES2Viewport InWindowHandle is %x"), InWindowHandle);
	FES2Core::OnViewportCreated(this, InWindowHandle);
}



/** FES2Viewport destructor */
FES2Viewport::~FES2Viewport()
{
	FES2Core::OnViewportDestroyed( this );
}



//@todo.JOEW. Remove this include when you implement the real clear touches handling
#if IPHONE
	#include "EngineAIClasses.h"			// needed only for gameframeworkclasses.h
	#include "EngineSequenceClasses.h"		// needed only for gameframeworkclasses.h
	#include "EngineUserInterfaceClasses.h"	// needed only for gameframeworkclasses.h
	#include "EnginePhysicsClasses.h"		// needed only for gameframeworkclasses.h
	#include "GameFrameworkClasses.h"
#endif


void FES2RHI::AcquireThreadOwnership()
{
#if !FLASH
	if( GIsRHIInitialized )
	{
#if ANDROID
		// If this is not the main thread, we must register it with the Java VM
		if( !IsInGameThread() )
		{
			extern bool	RegisterSecondaryThreadForEGL();
			RegisterSecondaryThreadForEGL();
		}
#endif

		// This function is defined elsewhere, for each ES2 platform
		FES2Core::MakeCurrent(NULL);
	}
#endif
}

void FES2RHI::ReleaseThreadOwnership()
{
#if !FLASH
	if( GIsRHIInitialized )
	{
		FES2Core::UnmakeCurrent();

#if ANDROID
		// If this is not the main thread, we must unregister it with the Java VM
		if( !IsInGameThread() )
		{
			extern bool UnRegisterSecondaryThreadFromEGL();
			UnRegisterSecondaryThreadFromEGL();
		}
#endif
	}
#endif
}


void FES2RHI::ReadSurfaceDataMSAA(FSurfaceRHIParamRef SurfaceRHI,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	appErrorf(TEXT("ReadSurfaceDataMSAA is unimplemented for the ES2 RHI."));
} 

INT MobileGetMSAAFactor()
{
#if IPHONE
	if (GMSAAAllowed && GMSAAEnabled)
	{
		return 4;
	}
#endif
	return 1;
}

/////////////////////////////////////////////////
//
// Stub functionality
//
/////////////////////////////////////////////////

FTexture2DRHIRef FES2RHI::GetResolveTarget(FSurfaceRHIParamRef SurfaceRHI) 
{
	if (SurfaceRHI == NULL)
	{
		return NULL;
	}
	DYNAMIC_CAST_ES2RESOURCE(Surface,Surface);
	return Surface->GetResolveTexture();
} 


INT FES2RHI::GetMipTailIdx(FTexture2DRHIParamRef Texture) 
{ 
	return INDEX_NONE;
} 


/** Interleaves the bits from A and B, in the form A[15]B[15]A[14]B[14]...A[1]B[1]A[0]B[0] */
inline DWORD InterleaveBits( DWORD A, DWORD B )
{
	A &= 0xffff;
	A = (A | (A << 8)) & 0x00ff00ff;
	A = (A | (A << 4)) & 0x0f0f0f0f;
	A = (A | (A << 2)) & 0x33333333;
	A = (A | (A << 1)) & 0x55555555;

	B &= 0xffff;
	B = (B | (B << 8)) & 0x00ff00ff;
	B = (B | (B << 4)) & 0x0f0f0f0f;
	B = (B | (B << 2)) & 0x33333333;
	B = (B | (B << 1)) & 0x55555555;

	return (A << 1) | B;
}

/**
*	Returns the offset for the pixel at coordinate (X,Y) relative the base address of a swizzled texture mipmap.
*	In a square texture, the bits in the X and Y coordinates are simply interleaved to create the offset.
*	In a rectangular texture, the shared lower bits are interleaved and the remainder bits are packed together on the left.
*
*	@param Width	Width of the mipmap in pixels
*	@param Height	Height of the mipmap in pixels
*	@param X		X-coordinate (U) in pixels
*	@param Y		Y-coordinate (V) in pixels
*	@return			Offset from the base address where this pixel should be stored (in pixels)
*/
DWORD GetSwizzleOffset( DWORD Width, DWORD Height, DWORD X, DWORD Y, DWORD BitMask, DWORD BitShift )
{
	DWORD Index;
	if ( Width == Height )
	{
		Index = InterleaveBits( Y, X );
	}
	else if ( Width > Height )
	{
		DWORD PackedBits = X & ~BitMask;
		DWORD InterleavedBits = InterleaveBits(Y, X & BitMask);
		Index = (PackedBits << BitShift) | InterleavedBits;
	}
	else
	{
		DWORD PackedBits = Y & ~BitMask;
		DWORD InterleavedBits = InterleaveBits(Y & BitMask, X);
		Index = (PackedBits << BitShift) | InterleavedBits;
	}
	return Index;
}

void FES2RHI::CopyTexture2D(FTexture2DRHIParamRef DstTextureRHI, UINT MipIndex, INT BaseSizeX, INT BaseSizeY, INT Format, const TArray<struct FCopyTextureRegion2D>& Regions) 
{
	DYNAMIC_CAST_ES2RESOURCE(Texture2D,DstTexture);
	check( DstTexture );

	INT ActualBlockSizeX = GPixelFormats[Format].BlockSizeX;
	INT ActualBlockSizeY = GPixelFormats[Format].BlockSizeY;
	INT ActualBlockBytes = GPixelFormats[Format].BlockBytes;

	INT DestSizeX = BaseSizeX >> MipIndex;
	INT DestSizeY = BaseSizeY >> MipIndex;

	// scale the base SizeX,SizeY for the current mip level
	INT DestMipSizeX = Max((INT)ActualBlockSizeX,DestSizeX);
	INT DestMipSizeY = Max((INT)ActualBlockSizeY,DestSizeY);

	// lock the destination texture
	UINT DstStride;
	BYTE* DstData = (BYTE*)RHILockTexture2D( DstTexture, MipIndex, TRUE, DstStride, FALSE );

	DWORD DestBlockSizeX = DestMipSizeX/ActualBlockSizeX;
	DWORD DestBlockSizeY = DestMipSizeY/ActualBlockSizeY;

	//for data swizzling on mobile devices
	DWORD DestBitShift = 0;
	DWORD DestBitMask = 0;
	DWORD DestNumBitsWidth = appFloorLog2(DestBlockSizeX);
	DWORD DestNumBitsHeight = appFloorLog2(DestBlockSizeY);

	//NOTE : These cases have been swapped because on device we have to swizzle x & y
	if ( DestBlockSizeY > DestBlockSizeX )
	{
		// Interleave shared bits to the right, pack the leftovers to the left
		DWORD NumInterleavedBits = DestNumBitsHeight;
		DestBitShift = NumInterleavedBits;		// Amount to shift left to add the packedbits to the interleaved bits
		DestBitMask = (1 << NumInterleavedBits) - 1;	// Mask for the right-most (interleaved) bits in X
	}
	else if ( DestBlockSizeX > DestBlockSizeY )
	{
		// Interleave shared bits to the right, pack the leftovers to the left
		DWORD NumInterleavedBits = DestNumBitsWidth;
		DestBitShift = NumInterleavedBits;		// Amount to shift left to add the packedbits to the interleaved bits
		DestBitMask = (1 << NumInterleavedBits) - 1;	// Mask for the right-most (interleaved) bits in Y
	}

	for( INT RegionIdx=0; RegionIdx < Regions.Num(); RegionIdx++ )
	{
		const FCopyTextureRegion2D& Region = Regions(RegionIdx);

		// Get the source texture for this region
		UObject* BaseTextureObject = (UObject*)Region.SrcTextureObject;
		UTexture2D* SrcTexture = Cast<UTexture2D>(BaseTextureObject);
		check( SrcTexture );

		//source formats must match!
		check(Format == SrcTexture->Format);

		//make sure it has a resource first
		check(SrcTexture->Resource);
		FTexture2DResource* SrcTexture2DResource = (FTexture2DResource*) SrcTexture->Resource;

		//make sure the data exists
		const BYTE* SrcData = (const BYTE*)SrcTexture2DResource->GetRawMipData(MipIndex + Region.FirstMipIdx);
		if (!SrcData)
		{
			debugf(NAME_Warning, TEXT("Source Data Missing from compositing.  Make sure the property NoRHI is set for %s"), *SrcTexture->GetName());
			continue;
		}

		// Calculate source values
		INT SrcSizeX = SrcTexture->SizeX >> (MipIndex + Region.FirstMipIdx);
		INT SrcSizeY = SrcTexture->SizeY >> (MipIndex + Region.FirstMipIdx);
		INT SrcMipSizeX = Max((INT)ActualBlockSizeX,SrcSizeX);
		INT SrcMipSizeY = Max((INT)ActualBlockSizeY,SrcSizeY);
		DWORD SrcBlockSizeX = SrcMipSizeX/ActualBlockSizeX;
		DWORD SrcBlockSizeY = SrcMipSizeY/ActualBlockSizeY;
		DWORD SrcBitShift = 0;
		DWORD SrcBitMask = 0;
		DWORD SrcNumBitsWidth = appFloorLog2(SrcBlockSizeX);
		DWORD SrcNumBitsHeight = appFloorLog2(SrcBlockSizeY);
		if ( SrcBlockSizeY > SrcBlockSizeX )
		{
			// Interleave shared bits to the right, pack the leftovers to the left
			DWORD NumInterleavedBits = SrcNumBitsHeight;
			SrcBitShift = NumInterleavedBits;		// Amount to shift left to add the packedbits to the interleaved bits
			SrcBitMask = (1 << NumInterleavedBits) - 1;	// Mask for the right-most (interleaved) bits in X
		}
		else if ( SrcBlockSizeX > SrcBlockSizeY )
		{
			// Interleave shared bits to the right, pack the leftovers to the left
			DWORD NumInterleavedBits = SrcNumBitsWidth;
			SrcBitShift = NumInterleavedBits;			// Amount to shift left to add the packedbits to the interleaved bits
			SrcBitMask = (1 << NumInterleavedBits) - 1;		// Mask for the right-most (interleaved) bits in Y
		}

		// Source region offsets
		INT SrcRegionOffsetX = (Clamp( Region.OffsetX, 0, SrcMipSizeX - ActualBlockSizeX ) / ActualBlockSizeX) * ActualBlockSizeX;
		INT SrcRegionOffsetY = (Clamp( Region.OffsetY, 0, SrcMipSizeY - ActualBlockSizeY ) / ActualBlockSizeY) * ActualBlockSizeY;


		// Destination region offsets
		INT DestRegionOffsetX = SrcRegionOffsetX;
		if( Region.DestOffsetX >= 0 )
		{
			DestRegionOffsetX = (Clamp( Region.DestOffsetX, 0, DestMipSizeX - ActualBlockSizeX ) / ActualBlockSizeX) * ActualBlockSizeX;
		}
		INT DestRegionOffsetY = SrcRegionOffsetY;
		if( Region.DestOffsetY >= 0 )
		{
			DestRegionOffsetY = (Clamp( Region.DestOffsetY, 0, DestMipSizeY - ActualBlockSizeY ) / ActualBlockSizeY) * ActualBlockSizeY;	
		}

		// scale region size to the current mip level. Size is aligned to the block size
		check(Region.SizeX != 0 && Region.SizeY != 0);
		INT RegionSizeX = Clamp( Align( Region.SizeX, ActualBlockSizeX), 0, SrcMipSizeX );
		INT RegionSizeY = Clamp( Align( Region.SizeY, ActualBlockSizeY), 0, SrcMipSizeY );
		// handle special case for full copy
		if( Region.SizeX == -1 || Region.SizeY == -1 )
		{
			RegionSizeX = SrcMipSizeX;
			RegionSizeY = SrcMipSizeY;
		}

		// size in bytes of an entire row for this mip
		DWORD ActualSrcPitchBytes = (SrcMipSizeX / ActualBlockSizeX) * ActualBlockBytes;
		DWORD ActualDestPitchBytes = (DestMipSizeX / ActualBlockSizeX) * ActualBlockBytes;

		// copy each region row in increments of the block size;
		INT CurDestOffsetY = DestRegionOffsetY;
		for( INT CurSrcOffsetY=SrcRegionOffsetY; CurSrcOffsetY < (SrcRegionOffsetY+RegionSizeY); CurSrcOffsetY += ActualBlockSizeY )
		{
			INT CurSrcBlockOffsetY = CurSrcOffsetY / ActualBlockSizeY;
			INT CurDestBlockOffsetY = CurDestOffsetY / ActualBlockSizeY;
			
			INT CurDestOffsetX = DestRegionOffsetX;
			for (INT CurSrcOffsetX=SrcRegionOffsetX; CurSrcOffsetX < (SrcRegionOffsetX+RegionSizeX); CurSrcOffsetX += ActualBlockSizeX)
			{
				INT CurSrcBlockOffsetX = CurSrcOffsetX / ActualBlockSizeX;
				INT CurDestBlockOffsetX = CurDestOffsetX / ActualBlockSizeX;

				DWORD SrcBlockOffset;
				DWORD DestBlockOffset;
				if (appGetPlatformType() & UE3::PLATFORM_Mobile)
				{
					//Use Z swizzle pattern
					DWORD SwizzledSrcOffset = GetSwizzleOffset(SrcBlockSizeY, SrcBlockSizeX, CurSrcBlockOffsetY, CurSrcBlockOffsetX, SrcBitMask, SrcBitShift);
					SrcBlockOffset = SwizzledSrcOffset*ActualBlockBytes;

					DWORD SwizzledDestOffset = GetSwizzleOffset(DestBlockSizeY, DestBlockSizeX, CurDestBlockOffsetY, CurDestBlockOffsetX, DestBitMask, DestBitShift);
					DestBlockOffset = SwizzledDestOffset*ActualBlockBytes;
				}
				else
				{
					//standard linear texture memory offset
					SrcBlockOffset = (CurSrcBlockOffsetY * ActualSrcPitchBytes) + CurSrcBlockOffsetX*ActualBlockBytes;
					DestBlockOffset = (CurDestBlockOffsetY * ActualDestPitchBytes) + CurDestBlockOffsetX*ActualBlockBytes;
				}
				const BYTE* SrcOffset = SrcData + SrcBlockOffset;
				BYTE* DstOffset = DstData + DestBlockOffset;
				appMemcpy(DstOffset, SrcOffset, ActualBlockBytes);

				CurDestOffsetX += ActualBlockSizeX;
			}

			CurDestOffsetY += ActualBlockSizeY;
		}
	}

	// unlock the destination texture
	RHIUnlockTexture2D( DstTexture, MipIndex, FALSE );
} 


void FES2RHI::CopyMipToMipAsync(FTexture2DRHIParamRef SrcTexture, INT SrcMipIndex, FTexture2DRHIParamRef DestTexture, INT DestMipIndex, INT Size, FThreadSafeCounter& Counter) 
{ 

} 


void FES2RHI::SelectiveCopyMipData(FTexture2DRHIParamRef Texture, BYTE *Src, BYTE *Dst, UINT MemSize, UINT MipIdx) 
{ 
	appMemcpy(Dst, Src, MemSize); 
} 


void FES2RHI::FinalizeAsyncMipCopy(FTexture2DRHIParamRef SrcTexture, INT SrcMipIndex, FTexture2DRHIParamRef DestTexture, INT DestMipIndex) 
{ 

} 


void FES2RHI::SetMRTBlendState(FBlendStateRHIParamRef NewStateRHI, UINT TargetIndex)
{
	// Not supported yet
}


void FES2RHI::SetMRTColorWriteEnable(UBOOL bEnable, UINT TargetIndex)
{
	// Not supported yet
}


void FES2RHI::SetMRTColorWriteMask(UINT ColorWriteMask, UINT TargetIndex)
{
	// Not supported yet
}


UBOOL FES2RHI::UpdateTexture2D( FTexture2DRHIParamRef TextureRHI,UINT MipIndex,UINT n,const FUpdateTextureRegion2D* rects,UINT pitch,UINT sbpp,BYTE* psrc)
{
	// Not supported yet
	return FALSE;
}

FSharedMemoryResourceRHIRef FES2RHI::CreateSharedMemory(EGPUMemoryType MemType,SIZE_T Size) 
{ 
	// create the shared memory resource
	FSharedMemoryResourceRHIRef SharedMemory(NULL);
	return SharedMemory;
} 


FSharedTexture2DRHIRef FES2RHI::CreateSharedTexture2D(UINT SizeX,UINT SizeY,BYTE Format,UINT NumMips,FSharedMemoryResourceRHIParamRef SharedMemory,DWORD Flags) 
{ 
	return FSharedTexture2DRHIRef(); 
} 

FSharedTexture2DArrayRHIRef FES2RHI::CreateSharedTexture2DArray(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,FSharedMemoryResourceRHIParamRef SharedMemoryRHI,DWORD Flags)
{
	// not supported on that platform
	check(0);
	return NULL;
}

FSurfaceRHIRef FES2RHI::CreateTargetableCubeSurface(UINT SizeX,BYTE Format,FTextureCubeRHIParamRef ResolveTargetTexture,ECubeFace CubeFace,DWORD Flags,const TCHAR* UsageStr) 
{ 
	return new FES2Surface(ResolveTargetTexture, CubeFace); 
}

#if !RHI_UNIFIED_MEMORY && !USE_NULL_RHI
void FES2RHI::GetTargetSurfaceSize(FSurfaceRHIParamRef SurfaceRHI, UINT& OutSizeX, UINT& OutSizeY)
{
    if ( SurfaceRHI )
    {
        DYNAMIC_CAST_ES2RESOURCE(Surface,Surface);
        OutSizeX = Surface->GetWidth();
        OutSizeY = Surface->GetHeight();
    }
    else
    {
        OutSizeX = 0;
        OutSizeY = 0;
    }
}
#endif

void FES2RHI::CopyToResolveTarget(FSurfaceRHIParamRef SurfaceRHI, UBOOL bKeepOriginalSurface, const FResolveParams& ResolveParams) 
{
	//@TODO: Support full surface copy if necessary (bKeepOriginalSurface == TRUE).

	if ( SurfaceRHI )
	{
		DYNAMIC_CAST_ES2RESOURCE(Surface,Surface);
		FTexture2DRHIRef ResolveTarget = Surface->GetResolveTexture();
		FTexture2DRHIRef RenderTarget = Surface->GetRenderTargetTexture();

		if ( ResolveTarget && ResolveTarget != RenderTarget )
		{
			Surface->SwapResolveTarget();
		}
	}
} 


void FES2RHI::CopyFromResolveTarget(FSurfaceRHIParamRef DestSurface) 
{ 

} 


void FES2RHI::CopyFromResolveTargetFast(FSurfaceRHIParamRef DestSurface) 
{ 

} 


void FES2RHI::CopyFromResolveTargetRectFast(FSurfaceRHIParamRef DestSurface, FLOAT X1,FLOAT Y1,FLOAT X2,FLOAT Y2) 
{ 

} 

/**
 * Notifies the driver (and our RHI layer) that the content of the specified buffers of the current
 * rendertarget are no longer needed and can be undefined from now on.
 * This allows us to avoid saving an unused renderbuffer to main memory (when used after rendering)
 * or restoring a renderbuffer from memory (when used before rendering). This can be a significant
 * performance cost on some platforms (e.g. tile-based GPUs).
 *
 * @param RenderBufferTypes		Binary bitfield of flags from ERenderBufferTypes
 */
void FES2RHI::DiscardRenderBuffer( DWORD RenderBufferTypes )
{
	if ( GPlatformFeatures.bSupportsRendertargetDiscard )
	{
		// Discard the unused render buffers
		GLenum Attachments[3];
		INT NumAttachments = 0;
		if ( RenderBufferTypes & RBT_Color )
		{
			Attachments[ NumAttachments++ ] = GL_COLOR_ATTACHMENT0;
		}
		if ( RenderBufferTypes & RBT_Depth )
		{
			Attachments[ NumAttachments++ ] = GL_DEPTH_ATTACHMENT;
		}
		if ( RenderBufferTypes & RBT_Stencil )
		{
			Attachments[ NumAttachments++ ] = GL_STENCIL_ATTACHMENT;
		}
		if ( NumAttachments > 0 )
		{
#if IPHONE
			glDiscardFramebufferEXT( GL_READ_FRAMEBUFFER_APPLE, NumAttachments, Attachments );
#else
//			glDiscardFramebufferEXT( GL_FRAMEBUFFER, NumAttachments, Attachments );
#endif
		}
	}
}

void FES2RHI::ReadSurfaceFloatData(FSurfaceRHIParamRef Surface,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<FFloat16Color>& OutData, ECubeFace CubeFace) 
{ 

} 

#if IPHONE && WITH_IOS_5

static GLuint CachedStateOcclusionQuery = 0;

FOcclusionQueryRHIRef FES2RHI::CreateOcclusionQuery()
{
	GLuint QueryID;
	glGenQueriesEXT(1, &QueryID);

	return new FES2OcclusionQuery(QueryID);
}

void FES2RHI::ResetOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQueryRHI)
{
	DYNAMIC_CAST_ES2RESOURCE(OcclusionQuery,OcclusionQuery);

	OcclusionQuery->bResultIsCached = FALSE;
}

void FES2RHI::BeginOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQueryRHI)
{
	DYNAMIC_CAST_ES2RESOURCE(OcclusionQuery,OcclusionQuery);

	if (CachedStateOcclusionQuery != 0)
	{
		glEndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);
	}

	glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, OcclusionQuery->Resource);
	CachedStateOcclusionQuery = OcclusionQuery->Resource;
}

void FES2RHI::EndOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQueryRHI)
{
	glEndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);
	CachedStateOcclusionQuery = 0;
}

UBOOL FES2RHI::GetOcclusionQueryResult(FOcclusionQueryRHIParamRef OcclusionQueryRHI, DWORD& OutNumPixels, UBOOL bWait)
{
	DYNAMIC_CAST_ES2RESOURCE(OcclusionQuery,OcclusionQuery);

	UBOOL bSuccess = TRUE;

	if (!OcclusionQuery->bResultIsCached)
	{
		GLuint Result = 0;

		// If this call is blocking, avoid repeated queries which can cause a performance loss
		if( bWait )
		{
			// This is a blocking call that won't return until the result is available
			glGetQueryObjectuivEXT(OcclusionQuery->Resource, GL_QUERY_RESULT_EXT, &Result);
			OcclusionQuery->Result = Result;
			bSuccess = TRUE;
		}
		else
		{
			// Check if the query is finished
			glGetQueryObjectuivEXT(OcclusionQuery->Resource, GL_QUERY_RESULT_AVAILABLE_EXT, &Result);

			// If it is, get the result now
			if( Result == GL_TRUE )
			{
				glGetQueryObjectuivEXT(OcclusionQuery->Resource, GL_QUERY_RESULT_EXT, &Result);
				OcclusionQuery->Result = Result;
				bSuccess = TRUE;
			}
			else
			{
				OcclusionQuery->Result = 0;
				bSuccess = FALSE;
			}
		}
	}

	OutNumPixels = (DWORD)OcclusionQuery->Result;
	OcclusionQuery->bResultIsCached = bSuccess;

	return bSuccess;
}

#else

FOcclusionQueryRHIRef FES2RHI::CreateOcclusionQuery() 
{ 
	return new FES2OcclusionQuery();
} 

void FES2RHI::ResetOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQuery) 
{ 

} 

void FES2RHI::BeginOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQuery) 
{ 

} 

void FES2RHI::EndOcclusionQuery(FOcclusionQueryRHIParamRef OcclusionQuery) 
{ 

} 

UBOOL FES2RHI::GetOcclusionQueryResult(FOcclusionQueryRHIParamRef OcclusionQuery, DWORD& OutNumPixels, UBOOL bWait) 
{ 
	OutNumPixels = 1;
	return TRUE;
} 

#endif

FSurfaceRHIRef FES2RHI::GetViewportBackBuffer(FViewportRHIParamRef ViewportRHI) 
{ 
	DYNAMIC_CAST_ES2RESOURCE(Viewport,Viewport);

#if IPHONE
	if( GMSAAAllowed && GMSAAEnabled )
	{
		return Viewport->ViewportBackBufferMSAA;
	}
#endif

	// just return the back buffer in the viewport
	return Viewport->ViewportBackBuffer;
} 

FSurfaceRHIRef FES2RHI::GetViewportDepthBuffer(FViewportRHIParamRef ViewportRHI) 
{ 
	DYNAMIC_CAST_ES2RESOURCE(Viewport,Viewport);

#if IPHONE
	if( GMSAAAllowed && GMSAAEnabled )
	{
		return Viewport->ViewportDepthBufferMSAA;
	}
#endif

	// just return the depth buffer in the viewport
	return Viewport->ViewportDepthBuffer;
} 


void FES2RHI::BeginScene() 
{ 
} 

void FES2RHI::EndScene() 
{ 
}


DWORD FES2RHI::GetGPUFrameCycles() 
{ 
	extern DWORD GGPUFrameTime;
	return GGPUFrameTime;
} 

DWORD FES2RHI::GetAvailableTextureMemory() 
{ 
	return 0; 
} 

FViewportRHIRef FES2RHI::CreateViewport(void* WindowHandle,UINT SizeX,UINT SizeY,UBOOL bIsFullscreen) 
{ 
	return new FES2Viewport( WindowHandle, SizeX, SizeY, bIsFullscreen );
} 


void FES2RHI::ResizeViewport(FViewportRHIParamRef Viewport,UINT SizeX,UINT SizeY,UBOOL bIsFullscreen) 
{ 

} 

void FES2RHI::Tick( FLOAT DeltaTime ) 
{ 

} 

void FES2RHI::SetScissorRect(UBOOL bEnable,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY) 
{ 

} 

void FES2RHI::SetDepthBoundsTest(UBOOL bEnable,const FVector4& ClipSpaceNearPos,const FVector4& ClipSpaceFarPos) 
{ 

} 

void FES2RHI::ClearSamplerBias() 
{ 

} 

void FES2RHI::SetVertexShaderBoolParameter(FVertexShaderRHIParamRef VertexShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue) 
{ 

} 

void FES2RHI::SetVertexShaderFloatArray(FVertexShaderRHIParamRef VertexShader,UINT BufferIndex,UINT BaseIndex,UINT NumValues,const FLOAT* FloatValues, INT ParamIndex) 
{ 

} 


void FES2RHI::SetPixelShaderBoolParameter(FPixelShaderRHIParamRef PixelShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue) 
{ 

} 


void FES2RHI::SetRenderTargetBias(FLOAT ColorBias) 
{ 

} 

void FES2RHI::SetShaderRegisterAllocation(UINT NumVertexShaderRegisters,UINT NumPixelShaderRegisters) 
{ 

} 


void FES2RHI::ReduceTextureCachePenalty(FPixelShaderRHIParamRef PixelShader) 
{ 

} 


void FES2RHI::SetMRTRenderTarget(FSurfaceRHIParamRef NewRenderTarget,UINT TargetIndex) 
{ 

} 

void FES2RHI::BeginHiStencilRecord(UBOOL bCompareFunctionEqual, UINT RefValue) 
{ 

} 

void FES2RHI::BeginHiStencilPlayback(UBOOL bFlush) 
{ 

} 

void FES2RHI::EndHiStencil() 
{ 

} 

void FES2RHI::KickCommandBuffer() 
{ 

} 


void FES2RHI::BlockUntilGPUIdle() 
{ 

} 


void FES2RHI::SuspendRendering() 
{ 
} 

void FES2RHI::ResumeRendering() 
{ 
} 

UBOOL FES2RHI::IsRenderingSuspended() 
{ 
	return FALSE; 
}

/**
 *	Copies the contents of the back buffer to specified texture.
 *	@param ResolveParams Required resolve params
 */
void FES2RHI::CopyFrontBufferToTexture( const FResolveParams& ResolveParams )
{
	// Not supported
}

void FES2RHI::RestoreColorDepth(FTexture2DRHIParamRef ColorTexture, FTexture2DRHIParamRef DepthTexture) 
{ 

} 

void FES2RHI::SetTessellationMode(ETessellationMode TessellationMode, FLOAT MinTessellation, FLOAT MaxTessellation) 
{ 

} 


UBOOL FES2RHI::GetAvailableResolutions(FScreenResolutionArray& Resolutions, UBOOL bIgnoreRefreshRate) 
{ 
	// @todo: Enumerate supported resolutions
	FScreenResolutionRHI Res;
	Res.Width = 480;
	Res.Height = 320;
	Res.RefreshRate = 60;
	Resolutions.Push( Res );
	return TRUE; 
} 


void FES2RHI::GetSupportedResolution(UINT& Width,UINT& Height) 
{ 

} 

void FES2RHI::SetLargestExpectedViewportSize( UINT NewLargestExpectedViewportWidth, UINT NewLargestExpectedViewportHeight ) 
{ 

} 

UBOOL FES2RHI::IsBusyTexture2D(FTexture2DRHIParamRef Texture, UINT MipIndex) 
{ 
	return FALSE; 
} 

UBOOL FES2RHI::IsBusyVertexBuffer(FVertexBufferRHIParamRef VertexBuffer) 
{ 
	return FALSE; 
} 

FTexture2DRHIRef FES2RHI::CreateStereoFixTexture()
{
	return NULL;
}

void FES2RHI::UpdateStereoFixTexture(FTexture2DRHIParamRef TextureRHI)
{
}

#if WITH_D3D11_TESSELLATION
// Tessellation is not supported in ES2
FHullShaderRHIRef FES2RHI::CreateHullShader(const TArray<BYTE>& Code) { appErrorf(TEXT("ES2 Render path does not support Hull or Domain shaders for tessellation!")); return NULL; }
FDomainShaderRHIRef FES2RHI::CreateDomainShader(const TArray<BYTE>& Code) { appErrorf(TEXT("ES2 Render path does not support Hull or Domain shaders for tessellation!")); return NULL; }
FBoundShaderStateRHIRef FES2RHI::CreateBoundShaderStateD3D11(FVertexDeclarationRHIParamRef VertexDeclaration, DWORD *StreamStrides, FVertexShaderRHIParamRef VertexShader, FHullShaderRHIParamRef HullShader, FDomainShaderRHIParamRef DomainShader, FPixelShaderRHIParamRef PixelShader, FGeometryShaderRHIParamRef GeometryShader, EMobileGlobalShaderType MobileGlobalShaderType)
{ 
	checkSlow(!HullShader);
	checkSlow(!DomainShader);
	checkSlow(!GeometryShader);

	return CreateBoundShaderState(VertexDeclaration, StreamStrides, VertexShader, PixelShader, MobileGlobalShaderType);
}
void FES2RHI::SetSamplerState(FDomainShaderRHIParamRef DomainShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{ appErrorf(TEXT("ES2 Render path does not support Hull or Domain shaders for tessellation!")); }
void FES2RHI::SetSamplerState(FHullShaderRHIParamRef HullShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{ appErrorf(TEXT("ES2 Render path does not support Hull or Domain shaders for tessellation!")); }
void FES2RHI::SetShaderBoolParameter(FHullShaderRHIParamRef HullShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue)
{ appErrorf(TEXT("ES2 Render path does not support Hull or Domain shaders for tessellation!")); }
void FES2RHI::SetShaderBoolParameter(FDomainShaderRHIParamRef DomainShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue)
{ appErrorf(TEXT("ES2 Render path does not support Hull or Domain shaders for tessellation!")); }
void FES2RHI::SetShaderParameter(FHullShaderRHIParamRef HullShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("ES2 Render path does not support Hull or Domain shaders for tessellation!")); }
void FES2RHI::SetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("ES2 Render path does not support Hull or Domain shaders for tessellation!")); }
void FES2RHI::SetDomainShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("ES2 Render path does not support Hull or Domain shaders for tessellation!")); }
void FES2RHI::SetHullShaderParameter(FHullShaderRHIParamRef HullShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("ES2 Render path does not support Hull or Domain shaders for tessellation!")); }
void FES2RHI::SetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("ES2 Render path does not support Geometry shaders!")); }
FGeometryShaderRHIRef FES2RHI::CreateGeometryShader(const TArray<BYTE>& Code)
{ appErrorf(TEXT("ES2 Render path does not support Geometry shaders!")); return NULL; }
void FES2RHI::SetSamplerState(FGeometryShaderRHIParamRef GeometryShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{ appErrorf(TEXT("ES2 Render path does not support Geometry shaders!")); }
void FES2RHI::SetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex)
{ appErrorf(TEXT("ES2 Render path does not support Compute shaders!")); }
FComputeShaderRHIRef FES2RHI::CreateComputeShader(const TArray<BYTE>& Code)
{ appErrorf(TEXT("ES2 Render path does not support Compute shaders!")); return NULL; }
void FES2RHI::DispatchComputeShader(FComputeShaderRHIParamRef ComputeShader, UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{ appErrorf(TEXT("ES2 Render path does not support Compute shaders!")); }
void FES2RHI::SetSamplerState(FComputeShaderRHIParamRef ComputeShaderRHI,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewStateRHI,FTextureRHIParamRef NewTextureRHI,FLOAT MipBias,FLOAT /*LargestMip*/, FLOAT /*SmallestMip*/, UBOOL /*bForceLinearMinFilter*/)
{ appErrorf(TEXT("ES2 Render path does not support Compute shaders!")); }
void FES2RHI::SetMultipleViewports(UINT Count, FViewPortBounds* Data)
{ appErrorf(TEXT("ES2 Render path does not support multiple Viewports!")); }
void FES2RHI::SetSurfaceParameter(FComputeShaderRHIParamRef ComputeShaderRHI,UINT TextureIndex,FSurfaceRHIParamRef NewTextureRHI)
{ appErrorf(TEXT("ES2 Render path does not support Compute shaders!")); }
void FES2RHI::SetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,UINT TextureIndex,FSurfaceRHIParamRef NewTextureRHI)
{ appErrorf(TEXT("ES2 Render path does not support Compute shaders!")); }
#endif

FBlendStateRHIRef FES2RHI::CreateMRTBlendState(const FMRTBlendStateInitializerRHI&)
{ appErrorf(TEXT("ES2 Render path does not support CreateMRTBlendState!")); return NULL; }


#if !USE_DYNAMIC_RHI

void appDumpTextureMemoryStats(const TCHAR* Message)
{
}

void appDefragmentTexturePool()
{
}

/**
 * Checks if the texture data is allocated within the texture pool or not.
 */
UBOOL appIsPoolTexture( FTextureRHIParamRef TextureRHI )
{
	return FALSE;
}

void appBeginDrawEvent( const FColor& Color, const TCHAR* Text)
{
#ifdef GL_EXT_debug_marker
	glPushGroupMarkerEXT( 0, TCHAR_TO_ANSI(Text) );
#endif
}

void appEndDrawEvent()
{
#ifdef GL_EXT_debug_marker
	glPopGroupMarkerEXT();
#endif
}

void appSetCounterValue( const TCHAR*, float )
{
}

#endif

/**
 * Stats objects for ES2
 */
DECLARE_STATS_GROUP(TEXT("ES2"),STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Draw Calls"),STAT_DrawCalls,STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Draw Calls (UP)"),STAT_DrawCallsUP,STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Num Invalid Meshes"),STAT_NumInvalidMeshes,STATGROUP_ES2);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("ES2 Init Shader"),STAT_ES2InitShaderCacheTime,STATGROUP_ES2);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("ES2 Init Shader Compile"),STAT_ES2InitShaderCacheCompileTime,STATGROUP_ES2);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("ES2 Init Shader Draw"),STAT_ES2InitShaderCacheDrawCallTime,STATGROUP_ES2);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("ES2 VS Avoided"),STAT_ES2ShaderCacheVSAvoided,STATGROUP_ES2);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("ES2 PS Avoided"),STAT_ES2ShaderCachePSAvoided,STATGROUP_ES2);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("ES2 Programs Avoided"),STAT_ES2ShaderCacheProgramsAvoided,STATGROUP_ES2);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("ES2 Shader Compile"),STAT_ES2ShaderCacheCompileTime,STATGROUP_ES2);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("ES2 Shader 1stDraw"),STAT_ES2ShaderCache1stDrawTime,STATGROUP_ES2);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("ES2 VertProg Compile"),STAT_ES2VertexProgramCompileTime,STATGROUP_ES2);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("ES2 PixelProg Compile"),STAT_ES2PixelProgramCompileTime,STATGROUP_ES2);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("ES2 Program Link"),STAT_ES2ProgramLinkTime,STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Triangles Drawn"),STAT_PrimitivesDrawn,STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Triangles Drawn (UP)"),STAT_PrimitivesDrawnUP,STATGROUP_ES2);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("ES2 Program Count"),STAT_ShaderProgramCount,STATGROUP_ES2);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("ES2 Program Count (PP)"),STAT_ShaderProgramCountPP,STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Program Changes"),STAT_ShaderProgramChanges,STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Uniform Updates (Bytes)"),STAT_ShaderUniformUpdates,STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Base Texture Binds"),STAT_BaseTextureBinds,STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Detail Texture Binds"),STAT_DetailTextureBinds,STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Detail2 Texture Binds"),STAT_Detail2TextureBinds,STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Detail3 Texture Binds"),STAT_Detail3TextureBinds,STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Lightmap Texture Binds"),STAT_LightmapTextureBinds,STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Normal Texture Binds"),STAT_NormalTextureBinds,STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Environment Texture Binds"),STAT_EnvironmentTextureBinds,STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Mask Texture Binds"),STAT_MaskTextureBinds,STATGROUP_ES2);
DECLARE_DWORD_COUNTER_STAT(TEXT("ES2 Emissive Texture Binds"),STAT_EmissiveTextureBinds,STATGROUP_ES2);

////////////////////////////////////////////////
// Windows specific EGL implementation
////////////////////////////////////////////////

#if _WINDOWS && WITH_WGL

// A convenience struct to collect all handles for windows, contexts, etc.
// all in one place
struct FWglHandles
{
	void*				WindowHandle;
	HDC					DeviceContext;
	HGLRC				RenderingContext;
};

// Declarations of functions in ES2 that are not in desktop OpenGL, but can be mapped to ones that are
PFNGLDEPTHRANGEPROC glDepthRangef = NULL;
PFNGLCLEARDEPTHPROC glClearDepthf = NULL;

/*
 * Initialize the ES2 functions which are not available in the desktop GL (Earlier versions at least)
 */
void ResolveES2AndDesktopGLConflicts( void * GLDLLHandle )
{
	glClearDepthf = (PFNGLCLEARDEPTHPROC)appGetDllExport( GLDLLHandle, TEXT( "glClearDepth" ) );
	glDepthRangef = (PFNGLDEPTHRANGEPROC)appGetDllExport( GLDLLHandle, TEXT( "glDepthRange" ) );
}

/*
 * Initialize a windows viewport for mobile emulation using OpenGL
 */
void PlatformInitializeViewport(FES2Viewport* Viewport, void* WindowHandle)
{
	void *GLDll = appGetDllHandle(TEXT("opengl32.dll"));
	if (!GLDll)
	{
		appErrorf(TEXT("Couldn't load opengl32.dll"));
		return;
	}

	// Set-up our collection of handy handles for use in other ES2-Called functions
	Viewport->PlatformData = new FWglHandles;
	FWglHandles& WglHandle = *((FWglHandles*)Viewport->PlatformData);
	WglHandle.WindowHandle = WindowHandle;

	WglHandle.DeviceContext = GetDC( (HWND)WglHandle.WindowHandle );

	PIXELFORMATDESCRIPTOR FormatDesc;
	memset(&FormatDesc, 0, sizeof(PIXELFORMATDESCRIPTOR));
	FormatDesc.nSize		= sizeof(PIXELFORMATDESCRIPTOR);
	FormatDesc.nVersion		= 1;
	FormatDesc.dwFlags		= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	FormatDesc.iPixelType	= PFD_TYPE_RGBA;
	FormatDesc.cColorBits	= 32;
	FormatDesc.cDepthBits	= 0;
	FormatDesc.cStencilBits	= 0;
	FormatDesc.iLayerType	= PFD_MAIN_PLANE;

	INT PixelFormat = ChoosePixelFormat( WglHandle.DeviceContext, &FormatDesc );
	if (PixelFormat && SetPixelFormat( WglHandle.DeviceContext, PixelFormat, &FormatDesc ) )
	{
		WglHandle.RenderingContext = wglCreateContext ( WglHandle.DeviceContext );
		if ( WglHandle.RenderingContext )
		{
			wglMakeCurrent( WglHandle.DeviceContext, WglHandle.RenderingContext );

			// Allocate the GL function pointers now that the render context has been created and made current
			LoadWindowsOpenGL();
			ResolveES2AndDesktopGLConflicts( GLDll );
		}
	}
}

/**
 * ES2-called function to Destroy Viewport
 */
void PlatformDestroyViewport(FES2Viewport* Viewport)
{

}

/**
 * ES2-called function to swap the buffers
 */
void PlatformSwapBuffers(FES2Viewport* Viewport) 
{
	if( GThreeTouchMode == ThreeTouchMode_NoSwap )
	{
		glFlush();
	}
	else
	{
		FWglHandles* Handles = (FWglHandles*)Viewport->PlatformData;
		SwapBuffers(Handles->DeviceContext);
	}
}

/**
 * ES2-called function to make the GL context be the current context
 */
void PlatformMakeCurrent(FES2Viewport* Viewport)
{
	FWglHandles* Handles = (FWglHandles*)Viewport->PlatformData;
	verify( wglMakeCurrent( Handles->DeviceContext, Handles->RenderingContext ) );
}

/**
 * ES2-called function to unbind the GL context
 */
void PlatformUnmakeCurrent(FES2Viewport* Viewport)
{
	FWglHandles* Handles = (FWglHandles*)Viewport->PlatformData;
	verify( wglMakeCurrent( NULL, NULL ) );
}

#endif // _WINDOWS && WITH_WGL

#endif // WITH_ES2_RHI
