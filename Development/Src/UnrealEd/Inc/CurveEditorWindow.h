// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#ifndef __CURVEEDITORWINDOW_H__
#define __CURVEEDITORWINDOW_H__

#include "UnrealEd.h"
#include "CurveEd.h"
#include "TrackableWindow.h"

class CurveEditorWindow : public WxTrackableFrame, public FNotifyHook, FDockingParent, FCurveEdNotifyInterface
{
public:
	CurveEditorWindow( wxWindow* InParent, wxWindowID InID, UObject *CurveObject, FString& Label);
	virtual ~CurveEditorWindow();

	void AddCurve(UObject *CurveObject, FString& Label);

	/**
	*	Public on purpose. This is just a helper class, if someone needs advanced functionalities he can access
	*	the underlying objects directly.
	*/
	UInterpCurveEdSetup *CurveEdSetup;
	WxCurveEditor *CurveEd;
protected:
	/**
	*	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
	*  @return A string representing a name to use for this docking parent.
	*/
	virtual const TCHAR* GetDockingParentName() const;

	/**
	* @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
	*/
	virtual const INT GetDockingParentVersion() const;
private:
	INT ColorRotationIndex;
};

#endif // __CURVEEDITORWINDOW_H__
