/*=============================================================================
	D3D9Viewport.h: D3D viewport RHI definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

class FD3D9Viewport : public FRefCountedObject, public TDynamicRHIResource<RRT_Viewport>
{
public:

	FD3D9Viewport(class FD3D9DynamicRHI* InD3DRHI,void* InWindowHandle,UINT InSizeX,UINT InSizeY,UBOOL bInIsFullscreen);
	~FD3D9Viewport();

	void Resize(UINT InSizeX,UINT InSizeY,UBOOL bInIsFullscreen);

	// Accessors.
	void* GetWindowHandle() const { return WindowHandle; }
	UINT GetSizeX() const { return SizeX; }
	UINT GetSizeY() const { return SizeY; }
	UBOOL IsFullscreen() const { return bIsFullscreen; }

private:
	FD3D9DynamicRHI* D3DRHI;
	void* WindowHandle;
	UINT SizeX;
	UINT SizeY;
	UBOOL bIsFullscreen;
};


