/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ATTACHMENTEDITOR_H__
#define __ATTACHMENTEDITOR_H__

#include "UnLinkedObjEditor.h"

/** Helper struct for storing drawing information for one actor in an attachment graph(s) */
struct FAttachEdDrawInfo
{
	FAttachEdDrawInfo()
	{
		Actor = NULL;
		DrawX = 0;
		DrawY = 0;
		Depth = 0;
		TotalHeight = 0;
		NodeHeight = 0;
		InputY = 0;
		bUserMoved = FALSE;
	}

	~FAttachEdDrawInfo()
	{
		// Free child entries
		ReleaseChildInfos();
	}

	/** Empty/free all child info's owned by this one */
	void ReleaseChildInfos();

	/** Calculate the total height of this node, plus all child nodes */
	INT CalcTotalHeight();

	/** Update the internal position of this node, and all children */
	void CalcPosition(INT InDepth, FLOAT YPos);

	/** Draw this node onto the canvas, and then draw all child nodes */
	void DrawInfo(FCanvas* Canvas, UBOOL bDrawInput);

	/** Sets a custom user draw position for this info */
	void SetUserDrawPos( INT InX, INT InY );

	/** Generate a FLinkedObjDrawInfo struct, based on the contents of this node */
	FLinkedObjDrawInfo MakeLinkObjDrawInfo(UBOOL bDrawInput);

	/** 
	 *	See if any actors in ChildActors are a direct child of the Actor of this DrawInfo. If so, create a ChildInfo entry 
	 *	For each newly created ChildIndo entry, call this function on it in turn, to build out the whole tree
	 *  
	 *	@param ChildActors	The actors to check for direct children
	 *  @param ActorToUserDrawPosMap	Optional mapping of actors to their position in the graph.  
	 *									This is used to preserve moved draw infos in the editor when they are destroyed during a refresh
	 */
	void AddChildActors(const TArray<AActor*>& ChildActors, const TMap<AActor*, FIntPoint>* ActorToUserDrawPosMap = NULL );

	/** Get all actors ref'd by this node, and its children */
	void GetAllActors(TArray<AActor*>& OutActors);
public:
	/** Pointer to actor that this node represents */
	AActor* Actor;
	/** X position to draw this node */
	INT DrawX;
	/** Y position to draw this node */
	INT DrawY;
	/** Total y height of this node, and all its children */
	INT TotalHeight;
	/** Array of nodes that are children (ie attached) to this one */
	TArray<FAttachEdDrawInfo*> ChildInfos;
	/** An array of output connector positions so we can determine where to start drawing new connector links */
	TArray<FIntPoint> OutputPositions;
	/** Y position of the single input on the left of this node */
	INT InputY;
private:
	/** How deep in the tree this node is */
	INT Depth;
	/** Y height of just this node */
	INT NodeHeight;
	/** If true the user manually moved this info and we should not auto arrange it by default */
	UBOOL bUserMoved;
};

/** Toolbar panel, placed at top of Attachment Editor */
class WxAttachmentEditorToolBar : public wxPanel
{
public:
	WxAttachmentEditorToolBar( wxWindow* InParent, wxWindowID InID );
	~WxAttachmentEditorToolBar();

	/** Bitmap for 'add actor' button */
	WxMaskedBitmap AddActorB;
	/** Bitmap for 'clear' button */
	WxMaskedBitmap ClearB;
	/** Bitmap for 'attach' button */
	WxMaskedBitmap AttachB;
	/** Bitmap for 'refresh' button */
	WxMaskedBitmap RefreshB;
	/** Bitmap for 'auto arrange' button*/
	WxMaskedBitmap AutoArrangeB;

	/** 'Add actor' button */
	WxBitmapCheckButton *AddActorButton;
	/** 'Clear' button */
	WxBitmapCheckButton *ClearButton;
	/** 'Attach' button */
	WxBitmapCheckButton *AttachButton;
	/** 'Refresh' button */
	WxBitmapCheckButton *RefreshButton;
	/** 'Auto arrange' button */
	WxBitmapCheckButton *AutoArrangeButton;
};

/** Window that displays a graph showing attached actors, and allows you to edit them */
class WxAttachmentEditor : public WxBrowser, public FSerializableObject, public FLinkedObjEdNotifyInterface
{
public:
	DECLARE_DYNAMIC_CLASS(WxAttachmentEditor);

	WxAttachmentEditor();
	~WxAttachmentEditor();

	virtual void Serialize(FArchive& Ar);

	virtual void Create(INT DockID, const TCHAR* FriendlyName, wxWindow* Parent);

	/** Called when the browser is getting activated (becoming the visible window in it's dockable frame). */
	void Activated(void);

	/** Tells the browser to update itself*/
	void Update(void);

	/** Returns the key to use when looking up values */
	virtual const TCHAR* GetLocalizationKey(void) const
	{
		return TEXT("AttachmentEditor");
	}

	/**
	 * Callback interface; Check for map change callbacks and clear out the attachment editor
	 *
	 * @param	InType	Callback type
	 * @param	Flag	Flag associated with the provided callback
	 */
	virtual void Send( ECallbackEventType InType, DWORD Flag );

	/** Called when editor is resized */
	void OnSize( wxSizeEvent& In );
	/** Handler for 'add actor' button. Adds selected Actor(s), along with everything they are attached to, into the editor */
	void OnAddSelectedActor( wxCommandEvent& In );
	/** Handler for 'clear' button. Remove all actors from attachment editor window */
	void OnClearActors( wxCommandEvent& In );
	/** Handler for 'attach actors' button. Attaches selected actors together. */
	void OnAttachActors( wxCommandEvent& In );
	/** Handler for 'refresh' button. Regenerates the graph view */
	void OnRefreshGraph( wxCommandEvent& In );
	/** Handle for 'auto arrange' button. Auto arranges graph nodes in the editor window */
	void OnAutoArrange( wxCommandEvent& In );
	/** Handler for 'break parent link' menu option. */
	void OnBreakParent( wxCommandEvent& In );

	/** Handler for 'Remove attachment graph' menu option.*/
	void OnRemoveGraph( wxCommandEvent& In );

	/**
	 * Handler for 'Break All Attachments of Selected and Remove' menu option.
	 *
	 * @param	In	Event generated by wxWidgets when the menu option is selected
	 */
	void OnDetachAllSelectedAndRemove( wxCommandEvent& In );

	/** Handler for selecting downstream graph nodes */
	void OnSelectDownsteamNodes( wxCommandEvent& In );

	/** Handler for selecting upstream graph nodes */
	void OnSelectUpsteamNodes( wxCommandEvent& In );

	/** Redraw the attachment viewport */
	void RefreshViewport();

	/** Update all position information of nodes */
	void RecalcDrawPositions();

	/** See if an actor is already added to the editor */
	UBOOL IsActorAdded(AActor* Actor);

	/** Add a particular actor's graph to the editor (if its not already there) */
	void AddActorToEditor(AActor* Actor);

	/** Attempt to add all selected actors to the attachment editor */
	void AddSelectedToEditor();

	/** Attach the selected actors together */
	void AttachSelected();

	/** Clears all of the actors from the attachment editor */
	void ClearActors();

	/**
	 * Remove the selected actors' graphs from the editor, breaking their attachments in the process
	 */
	void DetachAndRemoveSelected();

	/**
	 * Break all of the attachments for the actor of the provided info, setting its base to NULL, and un-attaching anything based on the actor
	 *
	 * @param	ActorDrawInfo		Info containing the actor to break all attachments for
	 * @param	bShouldRefreshGraph	Optional parameter, if TRUE, will refresh the graph after breaking attachments
	 */
	void BreakAllAttachmentsForInfo( FAttachEdDrawInfo& ActorDrawInfo, UBOOL bShouldRefreshGraph = TRUE );

	/** 
	 * Regenerates attachment graph, without changing the set visible
	 *
	 * @param bResetDrawPositions	If true, reset the user draw positions of all nodes in the graph.  This will auto arrange all nodes
 	 */
	void RefreshGraph( UBOOL bResetDrawPositions = FALSE );

	// FLinkedObjEdNotifyInterface interface
	virtual void DrawObjects(FViewport* Viewport, FCanvas* Canvas);
	virtual void EdHandleKeyInput(FViewport* Viewport, FName Key, EInputEvent Event);
	
	/** Called when the user right-clicks in the editor background; shows a context menu */
	virtual void OpenNewObjectMenu();
	virtual void OpenObjectOptionsMenu();
	
	virtual void AltClickConnector( struct FLinkedObjectConnector& Connector );
	virtual void ClickedLine(struct FLinkedObjectConnector &Src, struct FLinkedObjectConnector &Dest);
	virtual void SetSelectedConnector( struct FLinkedObjectConnector& Connector );
	virtual void MoveSelectedObjects( INT DeltaX, INT DeltaY );
	virtual FIntPoint GetSelectedConnLocation( FCanvas* Canvas );
	virtual INT GetSelectedConnectorType() { return SelectedConnectorType; }
	virtual void MakeConnectionToConnector( struct FLinkedObjectConnector& Connector );
	virtual void MakeConnectionToObject( UObject* Obj );

	/**
	 * Called when an object in the attachment editor is double clicked
	 */
	virtual void DoubleClickedObject( UObject *Obj );

	virtual void EmptySelection();
	virtual void AddToSelection( UObject* Obj );
	virtual void RemoveFromSelection( UObject* Obj );
	virtual UBOOL IsInSelection( UObject* Obj ) const;
	virtual INT GetNumSelected() const;

	/**
	 * Attaches an actor to a skeletal mesh, prompting the user to select a socket or bone to attach to.
	 * 
	 * @param SkelMeshActor	The skeletal mesh actor to attach to
	 * @param ActorToAttach The actor being attached to the skeletal mesh
	 */
	void AttachToSkeletalMesh( class ASkeletalMeshActor* SkelMeshActor, AActor* ActorToAttach );

	/**
	 * Selects all children of the passed in actor (Actors based to it)
	 *
	 * @param InActor	The actor which may have other actors based to it.
	 */
	void SelectAllChildren( AActor& InActor );

	/**
	 * Finds the corresponding draw info for an actor. 
	 * 
 	 * @param	ActorToFind		The actor whose info should be found
	 * @return	A draw info struct, or null if the a node representing the actor is not being drawn in the attachment editor
	 */
	FAttachEdDrawInfo* FindDrawInfoForActor( const AActor& ActorToFind );

	/**
	 * Finds corresponding draw info for an actor, if any, as well as the draw info array that draw info is located in.
	 *
	 * @param	ActorToFind			The actor whose info should be found
	 * @param	OutFoundInfo		Pointer which is set to the draw info corresponding to the actor, if found; NULL otherwise
	 * @param	OutFoundInfoArray	Pointer which is set to the draw info array containing the found draw info, if found; NULL otherwise
	 */
	void FindDrawInfoForActor( const AActor& ActorToFind, FAttachEdDrawInfo*& OutFoundInfo, TArray<FAttachEdDrawInfo*>*& OutFoundInfoArray );

	/** Container window for the 'graph' viewport.  This is usually a dockable window that wraps LinkedObjVC. */
	WxLinkedObjVCHolder* GraphWindow;
	/** Main panel, holds toolbar and viewport */
	wxPanel* BigPanel;
	/** Pointer to viewport client used to draw attachment graph */
	FLinkedObjViewportClient* LinkedObjVC;
	/** Texture used as background for graph view */
	UTexture2D*	BackgroundTexture;
	/** Set of 'roots' of attachment trees */
	TArray<FAttachEdDrawInfo*>	DrawInfos;
	/** Toolbar panel, at top of window */
	WxAttachmentEditorToolBar* ToolBar;
	/** A mapping of actors in this editor to their draw info positions*/
	TMap<AActor*, FIntPoint> ActorToDrawPosMap;

	// Data for remembering the currently selected connector
	UObject* SelectedConnectorObj;
	BYTE SelectedConnectorType;
	INT SelectedConnectorIndex;

	// Note: The following macro changes the access specifier to protected
	DECLARE_EVENT_TABLE()
};

#endif // __ATTACHMENTEDITOR_H__
