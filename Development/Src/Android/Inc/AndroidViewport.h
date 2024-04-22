/*=============================================================================
	AndroidViewport.h: Unreal Android viewport interface
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ANDROIDVIEWPORT_H__
#define __ANDROIDVIEWPORT_H__

#include "AndroidInput.h"

//
//	FAndroidViewport
//
struct FKdViewport: public FViewportFrame, public FViewport
{
	// Viewport state.
	class UKdClient* Client;

	UINT SizeX;
	UINT SizeY;

	static void StaticInit();
	// Constructor/destructor.
	FKdViewport(UKdClient* InClient, FViewportClient* InViewportClient, UINT InSizeX, UINT InSizeY);

	// FViewportFrame interface.
	virtual void* GetWindow() { return NULL; }

	virtual void LockMouseToWindow(UBOOL bLock) {}
	virtual UBOOL KeyState(FName Key) const;

	virtual UBOOL CaptureJoystickInput(UBOOL Capture) { return TRUE; }

	virtual INT GetMouseX();
	virtual INT GetMouseY();
	virtual void GetMousePos( FIntPoint& MousePosition );
	virtual void SetMouse(INT x, INT y);

	virtual UBOOL MouseHasMoved() { return TRUE; }

	virtual UINT GetSizeX() const { return SizeX; }
	virtual UINT GetSizeY() const { return SizeY; }

	virtual void InvalidateDisplay() {}

	virtual void GetHitProxyMap(UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<HHitProxy*>& OutMap) {}
	virtual void Invalidate() {}

	// FViewport interface.
	virtual FViewport* GetViewport() { return this; }
	virtual UBOOL IsFullscreen() { return TRUE; }
	virtual void SetName(const TCHAR* NewName) {}
	virtual void Resize(UINT NewSizeX,UINT NewSizeY,UBOOL NewFullscreen,INT InPosX = -1, INT InPosY = -1);
	virtual FViewportFrame* GetViewportFrame();
	void ProcessInput(FLOAT DeltaTime);

	/**
	* @return whether or not this Controller has Tilt Turned on
	**/
	virtual UBOOL IsControllerTiltActive( INT ControllerID ) const;

	/**
	* sets whether or not the Tilt functionality is turned on
	**/
	virtual void SetControllerTiltActive( INT ControllerID, UBOOL bActive );

	/**
	* sets whether or not to ONLY use the tilt input controls
	**/
	virtual void SetOnlyUseControllerTiltInput( INT ControllerID, UBOOL bActive );

	/**
	* sets whether or not to use the tilt forward and back input controls
	**/
	virtual void SetUseTiltForwardAndBack( INT ControllerID, UBOOL bActive );

	/**
	* @return whether or not this Controller has a keyboard available to be used
	**/
	virtual UBOOL IsKeyboardAvailable( INT ControllerID ) const;

	/**
	* @return whether or not this Controller has a mouse available to be used
	**/
	virtual UBOOL IsMouseAvailable( INT ControllerID ) const;

	// FAndroidViewport interface.	
	void Destroy();

	/**
	* recalibrate the tilt based on its current orientation
	**/
	virtual void CalibrateTilt();

private:

	/** TRUE if we want to use the tilt functionality to look around */
	UBOOL bIsTiltActive;

	/** The center values for tilt calibration */
	FLOAT TiltCenterYaw, TiltCenterPitch;
};

#endif
