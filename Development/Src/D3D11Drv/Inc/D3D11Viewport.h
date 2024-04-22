/*=============================================================================
	D3D11Viewport.h: D3D viewport RHI definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

class FD3D11Viewport : public FRefCountedObject, public TDynamicRHIResource<RRT_Viewport>
{
public:

	FD3D11Viewport(class FD3D11DynamicRHI* InD3DRHI,HWND InWindowHandle,UINT InSizeX,UINT InSizeY,UBOOL bInIsFullscreen);
	~FD3D11Viewport();

	void Resize(UINT InSizeX,UINT InSizeY,UBOOL bInIsFullscreen);

	/**
	 * If the swap chain has been invalidated by DXGI, resets the swap chain to the expected state; otherwise, does nothing.
	 * Called once/frame by the game thread on all viewports.
	 * @param bIgnoreFocus - Whether the reset should happen regardless of whether the window is focused.
     */
	void ConditionalResetSwapChain(UBOOL bIgnoreFocus);

	/** Presents the swap chain. */
	void Present(UBOOL bLockToVsync);

	// Accessors.
	UINT GetSizeX() const { return SizeX; }
	UINT GetSizeY() const { return SizeY; }
	FD3D11Surface* GetBackBuffer() const { return BackBuffer; }

private:
	FD3D11DynamicRHI* D3DRHI;
	HWND WindowHandle;
	UINT SizeX;
	UINT SizeY;
	UBOOL bIsFullscreen;
	UBOOL bIsValid;
	TRefCountPtr<IDXGISwapChain> SwapChain;
	TRefCountPtr<FD3D11Surface> BackBuffer;
};

