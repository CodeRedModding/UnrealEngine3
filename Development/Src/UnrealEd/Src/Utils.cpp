/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnFracturedStaticMesh.h"

#if _WINDOWS
#pragma pack(push,8)
// Needed for showing balloon messages
#include <ShellAPI.h>
#pragma pack(pop)
#endif

extern INT GEditorIcon;

FLOAT UnrealEd_WidgetSize = 0.15f; // Proportion of the viewport the widget should fill

/** Utility for calculating drag direction when you click on this widget. */
void HWidgetUtilProxy::CalcVectors(FSceneView* SceneView, const FViewportClick& Click, FVector& LocalManDir, FVector& WorldManDir, FLOAT& DragDirX, FLOAT& DragDirY)
{
	if(Axis == AXIS_X)
	{
		WorldManDir = WidgetMatrix.GetAxis(0);
		LocalManDir = FVector(1,0,0);
	}
	else if(Axis == AXIS_Y)
	{
		WorldManDir = WidgetMatrix.GetAxis(1);
		LocalManDir = FVector(0,1,0);
	}
	else
	{
		WorldManDir = WidgetMatrix.GetAxis(2);
		LocalManDir = FVector(0,0,1);
	}

	FVector WorldDragDir = WorldManDir;

	if(Mode == WMM_Rotate)
	{
		if( Abs(Click.GetDirection() | WorldManDir) > KINDA_SMALL_NUMBER ) // If click direction and circle plane are parallel.. can't resolve.
		{
			// First, find actual position we clicking on the circle in world space.
			const FVector ClickPosition = FLinePlaneIntersection(	Click.GetOrigin(),
																	Click.GetOrigin() + Click.GetDirection(),
																	WidgetMatrix.GetOrigin(),
																	WorldManDir );

			// Then find Radial direction vector (from center to widget to clicked position).
			FVector RadialDir = ( ClickPosition - WidgetMatrix.GetOrigin() );
			RadialDir.Normalize();

			// Then tangent in plane is just the cross product. Should always be unit length again because RadialDir and WorlManDir should be orthogonal.
			WorldDragDir = RadialDir ^ WorldManDir;
		}
	}

	// Transform world-space drag dir to screen space.
	FVector ScreenDir = SceneView->ViewMatrix.TransformNormal(WorldDragDir);
	ScreenDir.Z = 0.0f;

	if( ScreenDir.IsZero() )
	{
		DragDirX = 0.0f;
		DragDirY = 0.0f;
	}
	else
	{
		ScreenDir.Normalize();
		DragDirX = ScreenDir.X;
		DragDirY = ScreenDir.Y;
	}
}

/** 
 *	Utility function for drawing manipulation widget in a 3D viewport. 
 *	If we are hit-testing will create HWidgetUtilProxys for each axis, filling in InInfo1 and InInfo2 as passed in by user. 
 */
void FUnrealEdUtils::DrawWidget(const FSceneView* View,FPrimitiveDrawInterface* PDI, const FMatrix& WidgetMatrix, INT InInfo1, INT InInfo2, EAxis HighlightAxis, EWidgetMovementMode bInMode)
{
	DrawWidget( View, PDI, WidgetMatrix, InInfo1, InInfo2, HighlightAxis, bInMode, PDI->IsHitTesting() );
}

void FUnrealEdUtils::DrawWidget(const FSceneView* View,FPrimitiveDrawInterface* PDI, const FMatrix& WidgetMatrix, INT InInfo1, INT InInfo2, EAxis HighlightAxis, EWidgetMovementMode bInMode, UBOOL bHitTesting)
{
	const FVector WidgetOrigin = WidgetMatrix.GetOrigin();

	// Calculate size to draw widget so it takes up the same screen space.
	const FLOAT ZoomFactor = Min<FLOAT>(View->ProjectionMatrix.M[0][0], View->ProjectionMatrix.M[1][1]);
	const FLOAT WidgetRadius = View->Project(WidgetOrigin).W * (UnrealEd_WidgetSize / ZoomFactor);

	// Choose its color. Highlight manipulated axis in yellow.
	FColor XColor(255, 0, 0);
	FColor YColor(0, 255, 0);
	FColor ZColor(0, 0, 255);

	if(HighlightAxis == AXIS_X)
		XColor = FColor(255, 255, 0);
	else if(HighlightAxis == AXIS_Y)
		YColor = FColor(255, 255, 0);
	else if(HighlightAxis == AXIS_Z)
		ZColor = FColor(255, 255, 0);

	const FVector XAxis = WidgetMatrix.GetAxis(0); 
	const FVector YAxis = WidgetMatrix.GetAxis(1); 
	const FVector ZAxis = WidgetMatrix.GetAxis(2);

	if(bInMode == WMM_Rotate)
	{
		if(bHitTesting) PDI->SetHitProxy( new HWidgetUtilProxy(InInfo1, InInfo2, AXIS_X, WidgetMatrix, bInMode) );
		DrawCircle(PDI,WidgetOrigin, YAxis, ZAxis, XColor, WidgetRadius, 24, SDPG_Foreground);
		if(bHitTesting) PDI->SetHitProxy( NULL );

		if(bHitTesting) PDI->SetHitProxy( new HWidgetUtilProxy(InInfo1, InInfo2, AXIS_Y, WidgetMatrix, bInMode) );
		DrawCircle(PDI,WidgetOrigin, XAxis, ZAxis, YColor, WidgetRadius, 24, SDPG_Foreground);
		if(bHitTesting) PDI->SetHitProxy( NULL );

		if(bHitTesting) PDI->SetHitProxy( new HWidgetUtilProxy(InInfo1, InInfo2, AXIS_Z, WidgetMatrix, bInMode) );
		DrawCircle(PDI,WidgetOrigin, XAxis, YAxis, ZColor, WidgetRadius, 24, SDPG_Foreground);
		if(bHitTesting) PDI->SetHitProxy( NULL );
	}
	else
	{
		FMatrix WidgetTM;

		// Draw the widget arrows.
		if(bHitTesting) PDI->SetHitProxy( new HWidgetUtilProxy(InInfo1, InInfo2, AXIS_X, WidgetMatrix, bInMode) );
		WidgetTM = FMatrix(XAxis, YAxis, ZAxis, WidgetOrigin);
		DrawDirectionalArrow(PDI,WidgetTM, XColor, WidgetRadius, 1.f, SDPG_Foreground);
		if(bHitTesting) PDI->SetHitProxy( NULL );

		if(bHitTesting) PDI->SetHitProxy( new HWidgetUtilProxy(InInfo1, InInfo2, AXIS_Y, WidgetMatrix, bInMode) );
		WidgetTM = FMatrix(YAxis, ZAxis, XAxis, WidgetOrigin);
		DrawDirectionalArrow(PDI,WidgetTM, YColor, WidgetRadius, 1.f, SDPG_Foreground);
		if(bHitTesting) PDI->SetHitProxy( NULL );

		if(bHitTesting) PDI->SetHitProxy( new HWidgetUtilProxy(InInfo1, InInfo2, AXIS_Z, WidgetMatrix, bInMode) );
		WidgetTM = FMatrix(ZAxis, XAxis, YAxis, WidgetOrigin);
		DrawDirectionalArrow(PDI,WidgetTM, ZColor, WidgetRadius, 1.f, SDPG_Foreground);
		if(bHitTesting) PDI->SetHitProxy( NULL );

		if(bInMode == WMM_Scale)
		{
			FVector AlongX = WidgetOrigin + (XAxis * WidgetRadius * 0.3f);
			FVector AlongY = WidgetOrigin + (YAxis * WidgetRadius * 0.3f);
			FVector AlongZ = WidgetOrigin + (ZAxis * WidgetRadius * 0.3f);

			PDI->DrawLine(AlongX, AlongY, FColor(255,255,255), SDPG_Foreground);
			PDI->DrawLine(AlongY, AlongZ, FColor(255,255,255), SDPG_Foreground);
			PDI->DrawLine(AlongZ, AlongX, FColor(255,255,255), SDPG_Foreground);
		}
	}
}

/**
 * Localizes a window and all of its child windows.
 *
 * @param	InWin	The window to localize
 * @param	bOptional	TRUE to skip controls which do not have localized text (otherwise the <?int?bleh.bleh> string is assigned to the label)
 * @param	bFixUnderscores	TRUE to replace & characters with _ characters (workaround for .wxrc loading bug)
 */
void FLocalizeWindow( wxWindow* InWin, UBOOL bOptional, UBOOL bFixUnderscores )
{
	FLocalizeWindowLabel( InWin, bOptional, bFixUnderscores );
	FLocalizeChildren( InWin, bOptional, bFixUnderscores );
}

/**
 * Localizes a window Label.
 *
 * @param	InWin		The window to localize.  Must be valid.
 * @param	bOptional	TRUE to skip controls which do not have localized text (otherwise the <?int?bleh.bleh> string is assigned to the label)
 * @param	bFixUnderscores	TRUE to replace & characters with _ characters (workaround for .wxrc loading bug)
 */
void FLocalizeWindowLabel( wxWindow* InWin, UBOOL bOptional, UBOOL bFixUnderscores )
{
	check( InWin );
	FString label = InWin->GetLabel().c_str();
	if( label.Len() )
	{
		if( bFixUnderscores )
		{
			// Replace '&' characters with '_' characters, since they get mangled by
			// WxWidgets when loading from .xrc files
			label.ReplaceInline( TEXT( "&" ), TEXT( "_" ) );
		}
		FString WindowLabel = Localize( "UnrealEd", TCHAR_TO_ANSI( *label ), GPackage, NULL, bOptional );

		// if bOptional is TRUE, and this control's label isn't localized, WindowLabel should be empty
		if ( WindowLabel.Len() > 0 || !bOptional )
		{
			InWin->SetLabel( *WindowLabel );
		}
	}
}

/**
 * Localizes the child controls within a window, but not the window itself.
 *
 * @param	InWin		The window whose children to localize.  Must be valid.
 * @param	bOptional	TRUE to skip controls which do not have localized text (otherwise the <?int?bleh.bleh> string is assigned to the label)
 * @param	bFixUnderscores	TRUE to replace & characters with _ characters (workaround for .wxrc loading bug)
 */
void FLocalizeChildren( wxWindow* InWin, UBOOL bOptional, UBOOL bFixUnderscores )
{
	wxWindow* child;

    for( wxWindowList::compatibility_iterator node = InWin->GetChildren().GetFirst() ; node ; node = node->GetNext() )
    {
        child = node->GetData();
		FLocalizeWindowLabel( child, bOptional, bFixUnderscores );

		if( child->GetChildren().GetFirst() != NULL )
		{
			FLocalizeChildren( child, bOptional, bFixUnderscores );
		}
    }
}



/*-----------------------------------------------------------------------------
	FWindowUtil.
-----------------------------------------------------------------------------*/

// Returns the width of InA as a percentage in relation to InB.
FLOAT FWindowUtil::GetWidthPct( const wxRect& InA, const wxRect& InB )
{
	check( (FLOAT)InB.GetWidth() );
	return InA.GetWidth() / (FLOAT)InB.GetWidth();
}

// Returns the height of InA as a percentage in relation to InB.
FLOAT FWindowUtil::GetHeightPct( const wxRect& InA, const wxRect& InB )
{
	check( (FLOAT)InB.GetHeight() );
	return InA.GetHeight() / (FLOAT)InB.GetHeight();
}

// Returns the real client area of this window, minus any toolbars and other docked controls.
wxRect FWindowUtil::GetClientRect( const wxWindow& InThis, const wxToolBar* InToolBar )
{
	wxRect rc = InThis.GetClientRect();

	if( InToolBar )
	{
		rc.y += InToolBar->GetClientRect().GetHeight();
		rc.height -= InToolBar->GetClientRect().GetHeight();
	}

	return rc;
}

// Loads the position/size and other information about InWindow from the INI file
// and applies them.

void FWindowUtil::LoadPosSize( const FString& InName, wxTopLevelWindow* InWindow, INT InX, INT InY, INT InW, INT InH )
{
	check( InWindow );

	FString Wk;
	GConfig->GetString( TEXT("WindowPosManager"), *InName, Wk, GEditorUserSettingsIni );

	TArray<FString> Args;
	wxRect rc(InX,InY,InW,InH);
	UBOOL Maximized = FALSE, Minimized = FALSE;
	if( Wk.ParseIntoArray( &Args, TEXT(","), 0 ) >= 3 )
	{
		// Break out the arguments

		INT X = appAtoi( *Args(0) );
		INT Y = appAtoi( *Args(1) );
		const INT W = appAtoi( *Args(2) );
		const INT H = appAtoi( *Args(3) );

		FString MaxName = InName+TEXT("Maximized");
		FString MinName = InName+TEXT("Minimized");
		GConfig->GetBool( TEXT("WindowPosManager"), *MaxName, Maximized, GEditorUserSettingsIni );
		GConfig->GetBool( TEXT("WindowPosManager"), *MinName, Minimized, GEditorUserSettingsIni );
		
		// Make sure that the window is going to be on the visible screen
		const INT Threshold = 20;
		INT vleft = ::GetSystemMetrics( SM_XVIRTUALSCREEN )-Threshold;
		INT vtop = ::GetSystemMetrics( SM_YVIRTUALSCREEN )-Threshold;
		ForceTopLeftPosOffToolbar( vleft, vtop );
		const INT vright = vleft + ::GetSystemMetrics( SM_CXVIRTUALSCREEN )+Threshold;
		const INT vbottom = vtop + ::GetSystemMetrics( SM_CYVIRTUALSCREEN )+Threshold;

		if( X < vleft || X >= vright )		X = vleft;
		if( Y < vtop || Y >= vbottom )		Y = vtop;

		// Set the windows attributes

		rc.SetX( X );
		rc.SetY( Y );
		rc.SetWidth( W );
		rc.SetHeight( H );
	}

	InWindow->SetSize( rc );
	if( Maximized )
	{
		InWindow->Maximize();
	}
	else if( Minimized )
	{
		InWindow->Iconize( TRUE );
	}
}

/**
* If vLeft or vTop are under the windows toolbar, they are moved out from under it
*
* @param	vLeft			The default left position.
* @param	vTop			The default top position
*/
void FWindowUtil::ForceTopLeftPosOffToolbar( INT& vLeft, INT& vTop )
{
	// Get the window's current rectangle ...
	HWND Desktop = GetDesktopWindow();
	RECT rcWnd;
	GetWindowRect( Desktop, &rcWnd );

	// Figure out the best monitor for that window.
	HMONITOR hBestMonitor = MonitorFromRect( &rcWnd, MONITOR_DEFAULTTONEAREST );

	// Get information about that monitor
	MONITORINFO MonitorInfo;
	MonitorInfo.cbSize = sizeof(MonitorInfo);
	GetMonitorInfo( hBestMonitor, &MonitorInfo);

	// Figure out the work area not covered by taskbar
	RECT rcWorkArea = MonitorInfo.rcWork;

	vLeft =  ( (vLeft < rcWorkArea.left) && (vLeft >= 0) ) ? rcWorkArea.left : vLeft;
	vTop =  ( (vTop < rcWorkArea.top)  && (vTop >= 0) ) ? rcWorkArea.top : vTop;
}

// Saves the position/size and other relevant info about InWindow to the INI file.
void FWindowUtil::SavePosSize( const FString& InName, const wxTopLevelWindow* InWindow )
{
	check( InWindow );

	UBOOL Maximized = InWindow->IsMaximized() ? TRUE: FALSE;
	UBOOL Minimized = InWindow->IsIconized() ? TRUE: FALSE;

	FString MaxName = InName+TEXT("Maximized");
	FString MinName = InName+TEXT("Minimized");

	GConfig->SetBool( TEXT("WindowPosManager"), *MaxName, Maximized, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("WindowPosManager"), *MinName, Minimized, GEditorUserSettingsIni );

	//If the window is not Maximized or Minimized save position and dimensions of window
	if( !Minimized && !Maximized )
	{
		wxRect rc = InWindow->GetRect();
		FString Wk = *FString::Printf( TEXT("%d,%d,%d,%d"), rc.GetX(), rc.GetY(), rc.GetWidth(), rc.GetHeight() );
		GConfig->SetString( TEXT("WindowPosManager"), *InName, *Wk, GEditorUserSettingsIni );
	}
}

/*-----------------------------------------------------------------------------
	FTrackPopupMenu.
-----------------------------------------------------------------------------*/

FTrackPopupMenu::FTrackPopupMenu( wxWindow* InWindow, wxMenu* InMenu ):
Window( InWindow ),
Menu( InMenu )
{
	check( Window );
	check( Menu );
}

void FTrackPopupMenu::Show( INT InX, INT InY )
{
	wxPoint pt( InX, InY );

	// Display at the current mouse position?
	if( InX < 0 || InY < 0 )
	{
		pt = Window->ScreenToClient( wxGetMousePosition() );
	}

	// Prevent context menu from locking cursor within bounds of the window
	ClipCursor(NULL);

	Window->PopupMenu( Menu, pt );
}

/** Util to find currently loaded fractured versions of a particular StaticMesh. */
TArray<UFracturedStaticMesh*> FindFracturedVersionsOfMesh(UStaticMesh* InMesh)
{
	TArray<UFracturedStaticMesh*> Result;

	if(InMesh)
	{
		// If we actually passed in a FSM, then it is itself a fractured version - so use it!
		UFracturedStaticMesh* InFracMesh = Cast<UFracturedStaticMesh>(InMesh);
		if(InFracMesh)
		{
			Result.AddUniqueItem(InFracMesh);
		}
		// Otherwise iterate over all FSMs to find ones based on supplied mesh
		else
		{
			for( TObjectIterator<UFracturedStaticMesh> It; It; ++It )
			{
				UFracturedStaticMesh* FracMesh = *It;
				if(FracMesh && FracMesh->SourceStaticMesh == InMesh)
				{
					Result.AddUniqueItem(FracMesh);
				}
			}
		}
	}

	return Result;
}

INT FShowBalloonNotification::NumActiveNotifications = 0;

/** 
 * Shows a balloon notification in the task bar 
 *
 * @param Title		The title of the message
 * @param Message	The actual message to display. Note: Windows limits this value to 256 chars.
 * @param NotifyID	The ID of the balloon notification for responding to messages.
 * @param Timeout	The Timeout in milliseconds.  After this amount of time has elapsed the balloon will disappear.  Note: This is ignored on Windows Vista and above (see remarks in the function)
 */
UBOOL FShowBalloonNotification::ShowNotification( const FString& Title, const FString& InMessage, UINT NotifyID, UINT Timeout )
{
	UBOOL Result = FALSE;
#if _WINDOWS
	NOTIFYICONDATA NotifyData;
	appMemzero( &NotifyData, sizeof(NOTIFYICONDATA) );

	// Copy the message, we may need to modify it if its too long
	FString Message = InMessage;
	const UINT MaxMessageSize = ARRAYSIZE(NotifyData.szInfo);
	if( Message.Len() > MaxMessageSize )
	{
		// Truncate the message to the max size minus 4 extra elements to add a "..." and the required null terminator.
		Message = Message.Left( MaxMessageSize - 4 );
		const INT NewlineIndex = Message.InStr( "\n", TRUE );
		if( NewlineIndex != INDEX_NONE )
		{
			Message = Message.Left( NewlineIndex + 1 );
		}
		Message += "...\0";
	}

	// Display a balloon (NIF_INFO) and set up a callback message (NIF_MESSAGE)
	UINT NotifyFlags = NIF_INFO | NIF_MESSAGE;
	
	if( NumActiveNotifications == 0 )
	{
		// If there are no active balloons (an icon will already be there), show an icon so the balloon isn't floating off in space.
		NotifyFlags |= NIF_ICON;
		// Load the icon default editor icon to display in the task bar.
		NotifyData.hIcon = LoadIcon(hInstance,MAKEINTRESOURCE(GEditorIcon));
	}

	// Size of the structure
	NotifyData.cbSize = sizeof(NOTIFYICONDATA);
	// Parent window
	NotifyData.hWnd = (HWND)GApp->EditorFrame->GetHWND();
	// Flags defining how the notification is shown
	NotifyData.uFlags = NotifyFlags;
	// Timeout for when the balloon should disappear. 
	// Note: Has no effect in windows vista or later.  The system accessibility settings define the timeout on these OS'es
	NotifyData.uTimeout = Timeout;
	// Display an "information" icon
	NotifyData.dwInfoFlags = NIIF_INFO;
	// The id for this notification.
	NotifyData.uID = ID_BALLOON_NOTIFY_ID;
	// The callback ID for trapping custom messages from the balloon
	NotifyData.uCallbackMessage = NotifyID;

	// Set the title and information to display
	appStrcpy(NotifyData.szInfo, MaxMessageSize, *Message );
	appStrcpy(NotifyData.szInfoTitle, ARRAYSIZE(NotifyData.szInfoTitle), *Title );
	
	// Show the notification.
	if( NumActiveNotifications == 0 )
	{
		// A balloon with this ID has never been seen
		Result = Shell_NotifyIcon(NIM_ADD, &NotifyData);
		NotifyData.uVersion = NOTIFYICON_VERSION;
		Shell_NotifyIcon(NIM_SETVERSION, &NotifyData);
	}
	else
	{
		// We are modifying an already existing balloon
		Result = Shell_NotifyIcon(NIM_MODIFY, &NotifyData);
	}
	DWORD LastError = GetLastError();

	
	if( Result == TRUE )
	{
		++NumActiveNotifications;
	}
#endif
	return Result;
}

/**
 * Deletes the balloon and Icon from the task bar 
 */
void FShowBalloonNotification::DeleteNotification()
{
#if _WINDOWS
	if( NumActiveNotifications > 0 )
	{
		--NumActiveNotifications;

		if( NumActiveNotifications <= 0)
		{
			// There are no active or pending notifications so delete the icon.

			// Setup important info for the notify data and delete the icon.
			NOTIFYICONDATA NotifyData;
			appMemzero( &NotifyData, sizeof(NOTIFYICONDATA) );

			NotifyData.cbSize = sizeof( NOTIFYICONDATA );
			NotifyData.hWnd = (HWND)GApp->EditorFrame->GetHWND();
			NotifyData.uID = ID_BALLOON_NOTIFY_ID;

			Shell_NotifyIcon( NIM_DELETE, &NotifyData );
			DWORD LastError = GetLastError();

			// Account for the possiblity that DeleteNotification might be called more times than ShowNotification
			// This value should never be negative.
			NumActiveNotifications = 0;
		}
	}
#endif
}


