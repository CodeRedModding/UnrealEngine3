// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "CurveEditorWindow.h"


CurveEditorWindow::CurveEditorWindow( wxWindow* InParent, wxWindowID InID, UObject *CurveObject, FString& Label ) :
	WxTrackableFrame(InParent, InID, TEXT(""), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR, TEXT("Curve Editor")),
	FDockingParent(this),
	ColorRotationIndex(0)
{
	CurveEdSetup = ConstructObject<UInterpCurveEdSetup>( UInterpCurveEdSetup::StaticClass(), CurveObject, NAME_None, RF_NotForClient | RF_NotForServer | RF_Transactional );
	CurveEdSetup->AddToRoot();
	AddCurve(CurveObject, Label);
	CurveEd = new WxCurveEditor( this, -1, CurveEdSetup );
	AddDockingWindow(CurveEd, FDockingParent::DH_Top, TEXT("GenericCurveEditor"), TEXT("GenericCurveEditor"));
}

CurveEditorWindow::~CurveEditorWindow()
{
	CurveEdSetup->RemoveFromRoot();
}

void CurveEditorWindow::AddCurve(UObject *CurveObject, FString& Label)
{
	FColor Colors[] ={FColor(0, 255, 0), FColor(0, 0, 255), FColor(255, 0, 0), FColor(0, 255, 255), FColor(255, 255, 0), FColor(255, 0, 255)};
	CurveEdSetup->AddCurveToCurrentTab(CurveObject, Label, Colors[ColorRotationIndex]);
	ColorRotationIndex = (ColorRotationIndex + 1) % ARRAY_COUNT(Colors);
}

/**
*	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
*  @return A string representing a name to use for this docking parent.
*/
const TCHAR* CurveEditorWindow::GetDockingParentName() const
{
	return TEXT("Curve Editor");
}

/**
* @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
*/
const INT CurveEditorWindow::GetDockingParentVersion() const
{
	return 0;
}
