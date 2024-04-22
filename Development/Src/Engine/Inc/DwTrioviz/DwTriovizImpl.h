/******************************************************************************/
/*                                                                            */
/*      Copyright DARKWORKS, 2008-2009. Patent-pending technology by TRIOVIZ. */
/*                                                                            */
/******************************************************************************/

#ifndef __DWTRIOVIZSDK_DWTRIOVIZIMPL_H_INCLUDED__
#define __DWTRIOVIZSDK_DWTRIOVIZIMPL_H_INCLUDED__

// This class is a UObject class, and it will be put into an auto-generated header file,
// without any #if DWTRIOVIZSDK wrapper, so we must have it around for all builds/platforms
class UDwTriovizImplEffect : public UPostProcessEffect
{
public:
    DECLARE_CLASS_INTRINSIC(UDwTriovizImplEffect,UPostProcessEffect,0,Engine)

#if DWTRIOVIZSDK
	// DwTriovizImplEffect interface

	/**
	 * Creates a proxy to represent the render info for a post process effect
	 * @param WorldSettings - The world's post process settings for the view.
	 * @return The proxy object.
	 */
	virtual class FPostProcessSceneProxy* CreateSceneProxy(const FPostProcessSettings* WorldSettings);

	/**
	 * @param View - current view
	 * @return true if the effect should be rendered
	 */
	virtual UBOOL IsShown(const FSceneView* View) const;

	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

#endif	// DWTRIOVIZSDK
};

#if DWTRIOVIZSDK
struct FDwSceneViewCache
{
	FLOAT	DeltaWorldTime;
	UINT	NumView;
	FLOAT	X;
	FLOAT	Y;
	FLOAT	SizeX;
	FLOAT	SizeY;
	UINT	RenderTargetX;
	UINT	RenderTargetY;
	UINT	RenderTargetSizeX;
	UINT	RenderTargetSizeY;
	FLOAT	DisplayGamma;
	FLOAT	DOF_FocusDistance;
	FVector4 InvDeviceZToWorldZTransform;
	FMatrix ProjectionMatrix;
	FLinearColor OverlayColor;
	FLinearColor ColorScale;
	UBOOL	bUseLDRSceneColor;
	UBOOL	bResolveScene;
	FSurfaceRHIRef RenderTargetSurface;
	UBOOL	bIsLastView;
	UBOOL	bUseNormalDevSpace;
	UBOOL	bUseMoviePackingWithDisparity;
	UBOOL	bUseZFromAlpha;
	UBOOL	bUseSmoothTransition;
	UBOOL	bNoEffect;

	FDwSceneViewCache();
	FDwSceneViewCache(FViewInfo * ViewInfo);
};

enum ETrioviz3DMode
{
	ETrioviz3DMode_Off,
	ETrioviz3DMode_SideBySide,
	ETrioviz3DMode_TopBottom,	
	ETrioviz3DMode_ColorFilter,
#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280 || DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
	ETrioviz3DMode_FramePacking,
#endif
	ETrioviz3DMode_max,
};

#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280
extern FPS3RHISurface * GDwLeftEyeStereoSurface;
extern FPS3RHISurface * GDwRightEyeStereoSurface;
extern FSurfaceRHIRef GDwLeftEyeStereoSurfaceRef;
extern FSurfaceRHIRef GDwRightEyeStereoSurfaceRef;

class FDwSceneRenderTargetBackbufferStereoProxy : public FRenderTarget
{
public:

	// FRenderTarget interface

	/**
	* Set SizeX and SizeY of proxy and re-allocate scene targets as needed
	*
	* @param InSizeX - scene render target width requested
	* @param InSizeY - scene render target height requested
	*/
	void SetSizes(UINT InSizeX,UINT InSizeY)
	{
		SizeX = InSizeX;
		SizeY = InSizeY;
	}

	/**
	* Set current eye
	*/
	void SetStereo(UINT Eye)
	{
		EyeIndex = Eye;
	}

	/**
	* @return width of the scene render target this proxy will render to
	*/
	virtual UINT GetSizeX() const
	{
		return GScreenWidth;
	}

	/**
	* @return height of the scene render target this proxy will render to
	*/
	virtual UINT GetSizeY() const
	{
		return GScreenHeight;
	}

	/**
	* @return gamma this render target should be rendered with
	*/
	virtual FLOAT GetDisplayGamma() const
	{
		return 1.0f;
	}

	/**
	* @return RHI surface for setting the render target
	*/

	const FSurfaceRHIRef&	GetLeftEye()		const { return GDwLeftEyeStereoSurfaceRef; }
	const FSurfaceRHIRef&	GetRightEye()		const { return GDwRightEyeStereoSurfaceRef; }

	virtual const FSurfaceRHIRef& GetRenderTargetSurface() const;

private:

	/** scene render target width requested */
	UINT SizeX;
	/** scene render target height requested */
	UINT SizeY;
	/** EyeIndex */
	UINT EyeIndex;
};
#endif

#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
extern FSharedTexture2DRHIRef g_DwFrontBuffer;
extern FSharedMemoryResourceRHIRef g_DwMemoryFrontBuffer;
extern FTexture2DRHIRef g_DwStereoFrontBuffer;
extern IDirect3DTexture9* g_DwStereoFrontBufferDx9;
extern IDirect3DTexture9* g_DwStereoSecondFrontBufferDx9;
extern IDirect3DSurface9* g_DwStereoBackBuffer;
extern D3DPRESENT_PARAMETERS g_DwPresentParameters;
FSharedTexture2DRHIRef DwCreateSharedTexture2D(UINT SizeX,UINT SizeY,D3DFORMAT Format,UINT NumMips,FSharedMemoryResourceRHIParamRef SharedMemory,DWORD Flags);
#endif

void DwTriovizImpl_Initialize();
void DwTriovizImpl_Release();
void DwTriovizImpl_Render(FDwSceneViewCache * View, UBOOL FinalEffectInGroup, DWORD UsageFlagsLDR, UBOOL RenderRightOnly = FALSE);
void DwTriovizImpl_ResetRenderingStates();
UBOOL DwTriovizImpl_NeedsFinalize(void);
UBOOL DwTriovizImpl_IsSideBySide(void);
void DwTriovizImpl_RenderTriovizSplitscreen(void);
UBOOL DwTriovizImpl_IsTriovizAllowed();
UBOOL DwTriovizImpl_IsTriovizActive();
void DwTriovizImpl_SwitchCaptureDepth();
UBOOL DwTriovizImpl_IsTriovizCapturingDepth();
INT DwTriovizImpl_RenderMenu(FCanvas* pCanvas, INT X, INT Y);
UBOOL DwTriovizImpl_ProcessMenuInput(INT ControllerId, FName const InputKeyName, BYTE EventType);
void DwTriovizImpl_ToggleMenu();
#if USE_BINK_CODEC
#if XBOX
void DwTriovizImpl_BinkRender(UBOOL ApplyEffect, FSurfaceRHIRef Backbuffer, int RenderTargetX, int RenderTargetY, int RenderTargetSizeX, int RenderTargetSizeY);
#else
void DwTriovizImpl_BinkRender(UBOOL ApplyEffect, FSurfaceRHIRef Backbuffer, int RenderTargetX, int RenderTargetY, int RenderTargetSizeX, int RenderTargetSizeY);
#endif
//void DwTriovizImpl_FinalizeBink(const FTexture2DRHIRef& SceneColorTexture, UINT SizeX, UINT SizeY, FSurfaceRHIRef RenderTarget = NULL);
#endif//USE_BINK_CODEC
#if WITH_GFx_FULLSCREEN_MOVIE
void DwTriovizImpl_GfxRender(const FTexture2DRHIRef& SceneColorTexture, UINT SizeX, UINT SizeY, FSurfaceRHIRef RenderTarget = NULL);
void DwTriovizImpl_FinalizeGfxVideo(const FTexture2DRHIRef& SceneColorTexture, UINT SizeX, UINT SizeY, FSurfaceRHIRef RenderTarget = NULL);
#endif//WITH_GFx_FULLSCREEN_MOVIE
#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280 || DWTRIOVIZSDK_PS3_FRAMEPACKING_1280
void DwTriovizImpl_SwitchFramePacking3D(void);
void DwTriovizImpl_Strong3D(void);
void DwTriovizImpl_Subtle3D(void);
#endif//DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280 || DWTRIOVIZSDK_PS3_FRAMEPACKING_1280
#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
void DwTriovizImpl_XboxFramePackingCopyBackbuffer_RenderThread(INT EyeIndex, UBOOL UseTrioviz=TRUE, UBOOL ApplyLeftEyeEffect=FALSE);
void DwTriovizImpl_XboxFramePackingCopyBackbuffer(INT EyeIndex);
#if USE_BINK_CODEC
void DwTriovizImpl_RenderBink(UBOOL ApplyEffect);
#endif
#endif//DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
bool DwTriovizImpl_IsRenderingLeftEye();
void DwTriovizImpl_SetWhichEyeRendering(bool bLeftEye);
class FCanvas;
bool DwTriovizImpl_SetCanvasForUIRendering(FCanvas * Canvas, UINT nSizeX, UINT nSizeY);
class GFxMovieView;
bool DwTriovizImpl_SetViewportForGFxUIRendering(GFxMovieView * movie, UINT nSizeX, UINT nSizeY);
void DwTriovizImpl_ResetGfxViewport(GFxMovieView * movie);
void DwTriovizImpl_Disable3DForUI();
void DwTriovizImpl_Enable3DForUI(bool bEnable);
void DwTriovizImpl_Switch3DForUI();
void DwTriovizImpl_Set3DMode(int eDwTrioviz3DMode);
void DwTriovizImpl_Set3DIntesity(float DwTriovizIntensityValue);
void DwTriovizImpl_UseHDRSurface(UBOOL bUseHDR);
float DwTriovizImpl_GetCurrentProfileForegroundDisparityFactor();
float DwTriovizImpl_GetCurrentProfileMultiplier();
void DwTriovizImpl_SetCurrentProfileMultiplier(float fDwMultiplier);
void DwTriovizImpl_ToggleCinematicMode(UBOOL Cinematic = FALSE);
void DwTriovizImpl_SetFocalDistanceCinematicMode(FLOAT Distance);
void DwTriovizImpl_SetInvDeviceZToWorldZTransform(const FVector4& InvDeviceZToWorldZTransform);
UBOOL DwTriovizImpl_IsCinematicMode(void);
void DwTriovizImpl_PrepareCapturing(UCanvas* CanvasObject, UBOOL DwUseCinematicMode);
#if WIN32
void DwTriovizImpl_CaptureDepth(UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<BYTE>& OutData);
void DwTriovizImpl_SaveTGA(TCHAR * File, INT Width, INT Height, FColor* Data);
#endif //WIN32

// EPICSTART
#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280
UINT DwTriovizImpl_GetPS3DepthBufferHeight(UINT BufferSizeY);
#endif
UBOOL DwTriovizImpl_Exec(const TCHAR* Cmd, FOutputDevice& Ar);

// some additional Trioviz 
#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
#define DWTRIOVIZSDK_XBOX_FRAMEPACKING_VSYNC 1	//Force VSync but allocate more memory (8MB)
extern UBOOL g_DwUseFramePacking;
extern UBOOL g_Dw720p3DAvailable;
extern XGSTEREOPARAMETERS g_DwStereoParams;
#endif

// EPICEND

void DwTriovizImpl_fake3DRendering();
void DwTriovizImpl_Set3dMovieHackery(UBOOL bIsMovieBlurred);
UBOOL DwTriovizImpl_Is3dMovieHackeryNeeded();

#endif//DWTRIOVIZSDK

#endif