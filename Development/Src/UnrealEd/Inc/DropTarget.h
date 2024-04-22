/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


#ifndef __DROPTARGET_H__
#define __DROPTARGET_H__

#include "HModel.h"
#include "AssetSelection.h"

class FEditorDropTarget
{
public:
	FEditorDropTarget( struct FEditorLevelViewportClient* InViewportClient )
	: ViewportClient(InViewportClient)
	{
	}

protected:
	FEditorLevelViewportClient* ViewportClient;
};

class WxEditorTextDropTarget : public FEditorDropTarget, public wxTextDropTarget
{
public:
	WxEditorTextDropTarget( struct FEditorLevelViewportClient* InViewportClient );

	/**
	 * Called when the user releases the mouse button after a drag-n-drop operation in cases where the payload is text data.
	 *
	 * @param	MouseX	x position of the mouse, in canvas coordinates (i.e. 0,0 => SizeX,SizeY)
	 * @param	MouseY	y position of the mouse, in canvas coordinates (i.e. 0,0 => SizeX,SizeY)
	 *
	 * @return	TRUE if the viewport accepted the drop-operation.
	 */
	virtual bool OnDropText( wxCoord MouseX, wxCoord MouseY, const wxString& Text )=0;
};

/**
 * A class for receiving drag-n-drop operations which contain object path names.
 */
class WxObjectPathNameDropTarget : public WxEditorTextDropTarget
{
public:
	/**
	 * Constructor
	 *
	 * @param	InViewportClient	the VC for the viewport that will receive the drop
	 */
	WxObjectPathNameDropTarget( FEditorLevelViewportClient* InViewportClient );

	/** @hack: hook to fill-in the drop-target's data member with the data from the drop-source */
	virtual bool IsAcceptedData(IDataObject *pIDataSource) const;

	// called when the mouse enters the window (only once until OnLeave())
	virtual wxDragResult OnEnter( wxCoord x, wxCoord y, wxDragResult def );
 
	// called when mouse leaves the window: might be used to remove the feedback which was given in OnEnter()
    virtual void OnLeave();

	/**
	 * Called when the mouse moves in the window - shouldn't take long to execute or otherwise mouse movement would be too slow
	 *
	 * @param	x	x position of cursor
	 * @param	y	y position of cursor
	 * @param	def	result to return if dropping is allowed at this coordinate
	 *
	 * @return	value indicating whether this coordinate is a valid drop location
	 */
	virtual wxDragResult OnDragOver( wxCoord x, wxCoord y, wxDragResult def );

	/**
	 * Called when the user releases the mouse button after dragging an asset from the content browser.  Places a new instance
	 * of the appropriate asset/class at the location of the mouse.
	 *
	 * @param	MouseX	x position of the mouse, in canvas coordinates (i.e. 0,0 => SizeX,SizeY)
	 * @param	MouseY	y position of the mouse, in canvas coordinates (i.e. 0,0 => SizeX,SizeY)
	 * @param	Text	a string containing a pipe-delimited list of UObject pathnames
	 *
	 * @return	TRUE if the viewport accepted the drop-operation.
	 */
	virtual bool OnDropText( wxCoord MouseX, wxCoord MouseY, const wxString& Text );

protected:

	/** Cached list of data about the assets that are part of the drag-n-drop op */
	TArray<FSelectedAssetInfo> DroppedAssets;

	/**
	 * Parses the string passed in by the drop-source data object to generate a list of FSelectedAssetInfo
	 */
	void ParseDroppedAssetString();

	/**
	 * Clears the list of FDroppedObjectData
	 */
	void ClearDroppedAssetData();

	/**
	 * Called when the user drags an asset from the content browser into a level viewport.
	 *
	 * @param	DropX			X location (relative to the viewport's origin) where the user dropped the objects
	 * @param	DropY			Y location (relative to the viewport's origin) where the user dropped the objects
	 * @param	DroppedObjects	list of asset object that were dragged from the content browser
	 *
	 * @return	TRUE if the viewport accepted the drop operation
	 */
	UBOOL ProcessDropIntoViewport( INT DropX, INT DropY, const TArray<UObject*>& DroppedObjects );

	/**
	 * Called when an asset is dropped onto the blank area of a viewport.
	 *
	 * @param	View				The SceneView for the dropped-in viewport
	 * @param	ViewportMousePos	Mouse cursor location
	 * @param	DroppedObjects		Array of objects dropped into the viewport
	 *
	 * @return	TRUE if the drop operation was successfully handled; FALSE otherwise
	 */
	UBOOL OnDropToBackground( FSceneView* View, FViewportCursorLocation& ViewportMousePos, const TArray<UObject*>& DroppedObjects );

	/**
	 * Called when an asset is dropped upon an existing actor.
	 *
	 * @param	View				The SceneView for the dropped-in viewport
	 * @param	ViewportMousePos	Mouse cursor location
	 * @param	DroppedObjects		Array of objects dropped into the viewport
	 * @param	TargetProxy			Hit proxy representing the dropped upon actor
	 *
	 * @return	TRUE if the drop operation was successfully handled; FALSE otherwise
	 */
	UBOOL OnDropToActor( FSceneView* View, FViewportCursorLocation& ViewportMousePos, const TArray<UObject*>& DroppedObjects, HActor* TargetProxy );

	/**
	 * Called when an asset is dropped upon a BSP surface.
	 *
	 * @param	View				The SceneView for the dropped-in viewport
	 * @param	ViewportMousePos	Mouse cursor location
	 * @param	DroppedObjects		Array of objects dropped into the viewport
	 * @param	TargetProxy			Hit proxy representing the dropped upon model
	 *
	 * @return	TRUE if the drop operation was successfully handled; FALSE otherwise
	 */
	UBOOL OnDropToBSPSurface( FSceneView* View, FViewportCursorLocation& ViewportMousePos, const TArray<UObject*>& DroppedObjects, HModel* TargetProxy );

	/**
	 * Called when an asset is dropped upon a manipulation widget.
	 *
	 * @param	View				The SceneView for the dropped-in viewport
	 * @param	ViewportMousePos	Mouse cursor location
	 * @param	DroppedObjects		Array of objects dropped into the viewport
	 * @param	TargetProxy			Hit proxy representing the dropped upon manipulation widget
	 *
	 * @return	TRUE if the drop operation was successfully handled; FALSE otherwise
	 */
	UBOOL OnDropToWidget( FSceneView* View, FViewportCursorLocation& ViewportMousePos, const TArray<UObject*>& DroppedObjects, HWidgetAxis* TargetProxy );
};

/**
 * Context menu to handle advanced drag-and-drop operations, granting the user additional flexibility beyond standard
 * drag-and-drop
 */
class WxDragDropContextMenu : public wxMenu
{
public:
	/**
	 * Construct a WxDragDropContextMenu
	 *
	 * @param	DroppedObjects	Array of objects that have been dropped (currently only the first object is used)
	 * @param	HitProxy		HitProxy at the location of the user drop, if any
	 * @param	SceneView		SceneView of the viewport where the user performed the drop
	 * @param	CursorLocation	Location of the cursor where the user performed the drop
	 */ 
	WxDragDropContextMenu( TArray<UObject*> DroppedObjects, HHitProxy* HitProxy, const FSceneView& SceneView, const FViewportCursorLocation& CursorLocation );
	
	/**
	 * Destroy a WxDragDropContextMenu
	 */
	~WxDragDropContextMenu();

	/**
	 * Checks whether the context menu "handled" the drag-and-drop operation
	 *
	 * @return	TRUE if the menu handled the operation (via actor creation, etc.); FALSE if the user closed the menu or it
	 *			did not handle the operation for some reason
	 */
	UBOOL WasDropHandled() const;

private:
	/** Array of the dropped assets */
	TArray<FSelectedAssetInfo>		DroppedAssets;
	
	/** Reference to the Scene View of the dropped in viewport */
	const FSceneView&				DroppedInSceneView;

	/** Reference to the cursor location of the drop */
	const FViewportCursorLocation&	DroppedCursorLocation;

	/** Hit proxy at the drop location, if any */
	HHitProxy*						DroppedUponProxy;

	/** Index of the BSP surface dropped upon in its model's array, in the event of a drop to surface */
	UINT							DroppedUponSurfaceIndex;
	
	/** TRUE if the context menu handled the drag-drop operation */
	UBOOL							bHandledDrop;

	/** Helper function designed to append any valid add actor/replace with actor actor factory options */
	void AppendFactoryOptions();

	/** Helper function designed to append any valid "Apply actor as ____ to ____" options */
	void AppendApplyToOptions();

	/** Helper function designed to append any miscellaneous options */
	void AppendMiscOptions();

	/** Helper function to correctly set GEditor click locations, etc. based upon the type of drop operation */
	void ConfigureClickInformation();

	/**
	 * Called in response to the user selecting a menu option to add an actor of the dropped object from one of
	 * the valid actor factories
	 *
	 * @param	In	Event automatically generated by wxWidgets when the user selects a menu option
	 */
	void OnDropCreateActor( wxCommandEvent& In );

	/**
	 * Called in response to the user selecting a menu option to replace all selected actors with an actor of the
	 * dropped object from one of the valid actor factories
	 *
	 * @param	In	Event automatically generated by wxWidgets when the user selects a menu option
	 */
	void OnDropReplaceActor( wxCommandEvent& In );

	/**
	 * Called in response to the user selecting a menu option to apply the dropped object(s) to the dropped upon
	 * object (such as a material to a BSP surface, etc.)
	 *
	 * @param	In	Event automatically generated by wxWidgets when the user selects a menu option
	 */
	void OnDropApplyObjectToDroppedUpon( wxCommandEvent& In );

	/**
	 * Called in response to the user selecting a menu option to drop the dropped object as a prefab instance.
	 *
	 * @param	In	Event automatically generated by wxWidgets when the user selects a menu option
	 */
	void OnDropCreatePrefabInstance( wxCommandEvent& In );

	DECLARE_EVENT_TABLE();
};

// class WxAssetDataObject : public wxDataObjectSimple
// {
// public:
// 	void SetAssetObject( UObject* InAssetObject );
// 
// 	UObject* GetAssetObject() const;
// 
// 	/** === wxDataObjectSimple interface === */
// 	// if you don't specify the format in the ctor, you can still use
// 	// SetFormat() later
// 	WxAssetDataObject();
// 
// 	// get the size of our data
// 	virtual size_t GetDataSize() const;
// 	// copy our data to the buffer
// 	virtual bool GetDataHere(void* buf) const;
// 
// 	// copy data from buffer to our data
// 	virtual bool SetData(size_t len, const void* buf);
// 
// private:
// 	UObject* AssetObject;
// };

// class WxPlaceActorDropTarget : public FEditorDropTarget, public wxSimpleDropTarget
// {
// public:
// 	WxPlaceActorDropTarget();
// 
// 	// called when the mouse enters the window (only once until OnLeave())
// 	virtual wxDragResult OnEnter(wxCoord x, wxCoord y, wxDragResult def);
// 
// 	// called when the mouse moves in the window - shouldn't take long to
// 	// execute or otherwise mouse movement would be too slow
// 	virtual wxDragResult OnDragOver(wxCoord x, wxCoord y, wxDragResult def);
// 
// 	// this function is called when data is dropped at position (x, y) - if it
// 	// returns true, OnData() will be called immediately afterwards which will
// 	// allow to retrieve the data dropped.
// 	virtual bool OnDrop(wxCoord x, wxCoord y);
// 
// 	// called after OnDrop() returns TRUE: you will usually just call
// 	// GetData() from here and, probably, also refresh something to update the
// 	// new data and, finally, return the code indicating how did the operation
// 	// complete (returning default value in case of success and wxDragError on
// 	// failure is usually ok)
// 	virtual wxDragResult OnData(wxCoord x, wxCoord y, wxDragResult def);
// 
// 	// may be called *only* from inside OnData() and will fill m_dataObject
// 	// with the data from the drop source if it returns true
// 	virtual bool GetData();
// };


#endif // __DROPTARGET_H__
