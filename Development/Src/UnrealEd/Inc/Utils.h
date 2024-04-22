/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef UNREALED_UTILS_H
#define UNREALED_UTILS_H

enum EWidgetMovementMode
{
	WMM_Rotate,
	WMM_Translate,
	WMM_Scale
};

struct HWidgetUtilProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HWidgetUtilProxy,HHitProxy);

	INT						Info1;
	INT						Info2;
	EAxis					Axis;
	FMatrix					WidgetMatrix;
	EWidgetMovementMode		Mode;

	HWidgetUtilProxy(INT InInfo1, INT InInfo2, EAxis InAxis, const FMatrix& InWidgetMatrix, EWidgetMovementMode bInMode):
	HHitProxy(HPP_UI),
		Info1(InInfo1),
		Info2(InInfo2),
		Axis(InAxis),
		WidgetMatrix(InWidgetMatrix),
		Mode(bInMode)
	{}

	void CalcVectors(FSceneView* SceneView, const FViewportClick& Click, FVector& LocalManDir, FVector& WorldManDir, FLOAT& DragDirX, FLOAT& DragDirY);
};

class FUnrealEdUtils
{
public:
	static void DrawWidget(const FSceneView* View,FPrimitiveDrawInterface* PDI, const FMatrix& WidgetMatrix, INT InInfo1, INT InInfo2, EAxis HighlightAxis, EWidgetMovementMode bInMode);
	static void DrawWidget(const FSceneView* View,FPrimitiveDrawInterface* PDI, const FMatrix& WidgetMatrix, INT InInfo1, INT InInfo2, EAxis HighlightAxis, EWidgetMovementMode bInMode, UBOOL bHitTesting);
};


/**
 * Basic sizing utilities for windows.
 */
class FWindowUtil
{
public:
	/** Returns the width of InA as a percentage in relation to InB. */
	static FLOAT GetWidthPct( const wxRect& InA, const wxRect& InB );

	/** Returns the height of InA as a percentage in relation to InB. */
	static FLOAT GetHeightPct( const wxRect& InA, const wxRect& InB );

	/** Returns the real client area of this window, minus any toolbars and other docked controls.
	*
	* @param	InThis			The window to get a rect from.
	* @param	wxToolBar		An optional pointer to the window's toolbar or docked control.
	* @return					The net area of the window.
	*/
	static wxRect GetClientRect( const wxWindow& InThis, const wxToolBar* InToolBar=NULL );

	/**
	* Loads the position/size and other information about InWindow from the INI file and applies them.
	* The default position and size gets used if the INI label could not be read.
	*
	* @param	InName			The INI label name.
	* @param	InWindow		The window to load.  Must be a valid pointer.
	* @param	InX				Default X pos.
	* @param	InY				Default Y pos.
	* @param	InW				Default width.
	* @param	InH				Default height.
	*/
	static void LoadPosSize( const FString& InName, wxTopLevelWindow* InWindow, INT InX = -1, INT InY = -1, INT InW = -1, INT InH = -1 );

	/**
	* If vLeft or vTop are under the windows toolbar, they are moved out from under it
	*
	* @param	vLeft			The default left position.
	* @param	vTop			The default top position
	*/
	static void ForceTopLeftPosOffToolbar( INT& vLeft, INT& vTop );

	/**
	* Saves the position/size and other relevant info about InWindow to the INI file.
	*
	* @param	InName			The INI label name.
	* @param	InWindow		The window to save.  Must be a valid pointer.
	*/
	static void SavePosSize( const FString& InName, const wxTopLevelWindow* InWindow );

private:
	/** @name Prevent creation */
	//@{
	FWindowUtil();
	~FWindowUtil();
	//@}
};

/**
* Utility class for creating popup menus.
*/
class FTrackPopupMenu
{
public:
	/** Inputs must be valid pointers.  FTrackPopupMenu does not assume ownership of InWindow or InMenu. */
	FTrackPopupMenu( wxWindow* InWindow, wxMenu* InMenu );

	/**
	* Display the popup menu at the specified position.  If either of InX or InY is less than zero,
	* the menu appears at the current mouse position.
	*/
	void Show( INT InX = -1, INT InY = -1 );

private:
	/** The parent window. */
	wxWindow*	Window;
	/** The popup menu. */
	wxMenu*		Menu;
};

/**
* Wrapper class for handing object pointers to wxWindows. Ensures that all UObject references are
* serialized and provides accessor functions to avoid potentially unsafe casting.
*/
class WxTreeObjectWrapper : public wxTreeItemData, FSerializableObject
{
public:
	/**
	* Constructor, keeping track of passed in Object.
	*
	* @param InObject	Object to wrap
	*/
	WxTreeObjectWrapper( UObject* InObject )
	{
		Object = InObject;
		if( Object && (Object->GetOutermost()->PackageFlags & PKG_PlayInEditor) )
		{
			appErrorf( TEXT("Editor is holding onto '%s' residing in PIE package"), *Object->GetFullName() );		
		}
	}

	/**
	* Templatized accessor that returns the object of the template
	* type if it could be cast and NULL otherwise.
	*
	* @return	current Object if it could be cast to type T, NULL otherwise
	*/
	template< class T > T* GetObject()
	{
		return Cast<T>(Object);
	}

	/**
	* Templatized accessor that returns the object of the template
	* type if it could be cast and asserts otherwise.
	*
	* @return	Object cast to type T (asserts if this is not possible)
	*/
	template< class T > T* GetObjectChecked()
	{
		return CastChecked<T>(Object);
	}

	/**
	* Serialize our UObject reference.
	*
	* @param Ar The archive to serialize with
	*/
	virtual void Serialize(FArchive& Ar)
	{
		check( !Ar.IsLoading() && !Ar.IsSaving() );
		Ar << Object;
	}

private:
	/**
	* Private default constructor. Hidden on purpose.
	*/
	WxTreeObjectWrapper()
	{ 
		check(0);	
	}

	/** Object to store and serialize */
	UObject* Object;
};

/**
 * Localizes the labels of a window and all of its child windows.
 *
 * @param	InWin		The window to localize.  Must be valid.
 * @param	bOptional	TRUE to skip controls which do not have localized text (otherwise the <?int?bleh.bleh> string is assigned to the label)
 * @param	bFixUnderscores	TRUE to replace & characters with _ characters (workaround for .wxrc loading bug)
 */
void FLocalizeWindow( wxWindow* InWin, UBOOL bOptional=FALSE, UBOOL bFixUnderscores=FALSE );

/**
 * Localizes a window Label.
 *
 * @param	InWin		The window to localize.  Must be valid.
 * @param	bOptional	TRUE to skip controls which do not have localized text (otherwise the <?int?bleh.bleh> string is assigned to the label)
 * @param	bFixUnderscores	TRUE to replace & characters with _ characters (workaround for .wxrc loading bug)
 */
void FLocalizeWindowLabel( wxWindow* InWin, UBOOL bOptional=FALSE, UBOOL bFixUnderscores=FALSE );

/**
 * Localizes the child controls within a window, but not the window itself.
 *
 * @param	InWin		The window whose children to localize.  Must be valid.
 * @param	bOptional	TRUE to skip controls which do not have localized text (otherwise the <?int?bleh.bleh> string is assigned to the label)
 * @param	bFixUnderscores	TRUE to replace & characters with _ characters (workaround for .wxrc loading bug)
 */
void FLocalizeChildren( wxWindow* InWin, UBOOL bOptional=FALSE, UBOOL bFixUnderscores=FALSE );

/** Util to find currently loaded fractured versions of a particular StaticMesh. */
TArray<UFracturedStaticMesh*> FindFracturedVersionsOfMesh(UStaticMesh* InMesh);

/**
 * Utility class for creating balloon notifications in the taskbar.
 */
class FShowBalloonNotification
{
private:
	/** The number of active or queued balloon notifications */
	static INT NumActiveNotifications;
public:
	/** 
	 * Shows a balloon notification in the task bar 
	 *
	 * @param Title		The title of the message
	 * @param Message	The actual message to display. Note: Windows limits this value to 256 chars.  Any strings larger than 256 will be truncated.
	 * @param NotifyID	The ID of the balloon notification for responding to messages.
	 * @param Timeout	The Timeout in milliseconds.  After this amount of time has elapsed the balloon will disappear.  Note: This is ignored on Windows Vista and above (see remarks in the function)
	 */
	static UBOOL ShowNotification( const FString& Title, const FString& InMessage, UINT NotifyID, UINT Timeout = 15000 );
	
	/**
	 * Deletes the balloon and Icon from the task bar if no balloons are currently active or queued
	 */
	static void DeleteNotification();
};

#endif

