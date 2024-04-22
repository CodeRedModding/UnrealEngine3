/*=============================================================================
	AnimTreeEditor.h: AnimTree editing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ANIMTREEEDITOR_H__
#define __ANIMTREEEDITOR_H__

#include "UnrealEd.h"
#include "TrackableWindow.h"

/*-----------------------------------------------------------------------------
	FAnimTreeEdPreviewVC
-----------------------------------------------------------------------------*/

class FAnimTreeEdPreviewVC : public FEditorLevelViewportClient
{
public:
	class WxAnimTreeEditor*		AnimTreeEd;

	FPreviewScene				PreviewScene;

	/** Helper class that draws common scene elements. */
	FEditorCommonDrawHelper		DrawHelper;

	UBOOL						bDrawingInfoWidget;
	UBOOL						bManipulating;
	EAxis						ManipulateAxis;
	INT							ManipulateWidgetIndex;

	FLOAT						DragDirX;
	FLOAT						DragDirY;
	FVector						WorldManDir;
	FVector						LocalManDir;

	FAnimTreeEdPreviewVC(class WxAnimTreeEditor* InAnimTreeEd);

	// FEditorLevelViewportClient interface

	virtual FSceneInterface* GetScene() { return PreviewScene.GetScene(); }
	virtual FLinearColor GetBackgroundColor();
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas);
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI);
	virtual void Tick(FLOAT DeltaSeconds);

	virtual UBOOL InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT AmountDepressed = 1.f,UBOOL bGamepad=FALSE);
	virtual UBOOL InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad=FALSE);
	virtual void MouseMove(FViewport* Viewport,INT x, INT y);

	virtual void Serialize(FArchive& Ar);
};

/*-----------------------------------------------------------------------------
	WxAnimTreePreview
-----------------------------------------------------------------------------*/

/**
 * wxWindows Holder for FAnimTreeEdPreviewVC
 */
class WxAnimTreePreview : public wxWindow
{
public:
	FAnimTreeEdPreviewVC* AnimTreePreviewVC;

	WxAnimTreePreview(wxWindow* InParent, wxWindowID InID, class WxAnimTreeEditor* InAnimTreeEd, FVector& InViewLocation, FRotator& InViewRotation);
	~WxAnimTreePreview();

private:
	void OnSize( wxSizeEvent& In );

	DECLARE_EVENT_TABLE()
};


/*-----------------------------------------------------------------------------
	WxAnimTreeEditorToolBar
-----------------------------------------------------------------------------*/

class WxAnimTreeEditorToolBar : public WxToolBar
{
public:
	WxComboBox *PreviewMeshListCombo;
	WxComboBox *PreviewSocketListCombo;
	WxComboBox *PreviewAnimSetListCombo;

	WxComboBox *PreviewAnimSetCombo;
	WxComboBox *PreviewAnimSequenceCombo;

	wxSlider *RateSlideBar;

	WxAnimTreeEditorToolBar( wxWindow* InParent, wxWindowID InID );

private:
	WxMaskedBitmap TickTreeB;
	WxMaskedBitmap PreviewNodeB;
	WxMaskedBitmap ShowNodeWeightB;
	WxMaskedBitmap ShowBonesB;
	WxMaskedBitmap ShowBoneNamesB;
	WxMaskedBitmap ShowWireframeB;
	WxMaskedBitmap ShowFloorB;
	WxMaskedBitmap CurvesB;
	WxMaskedBitmap AddMaskedBitmap;
};

/*-----------------------------------------------------------------------------
	WxAnimTreeEditor
-----------------------------------------------------------------------------*/

class WxAnimTreeEditor : public WxTrackableFrame, public FNotifyHook, public FLinkedObjEdNotifyInterface, public FSerializableObject, public FDockingParent
{
public:
	DECLARE_DYNAMIC_CLASS(WxAnimTreeEditor);


	/** AnimTree currently being edited. */
	class UAnimTree* AnimTree;

	/** All nodes in current tree (including the AnimTree itself). */
	TArray<class UAnimObject*> TreeNodes;

	/** Currently selected UAnimNodes. */
	TArray<class UAnimObject*> SelectedNodes;

	/** Array of nodes containing extra information/callbacks for editing custom node types. */
	TArray<class UAnimNodeEditInfo*> AnimNodeEditInfos; 

	/** Object containing selected connector. */
	UObject* ConnObj;

	/** Type of selected connector. */
	INT ConnType;

	/** Index of selected connector. */
	INT ConnIndex;

	/** Whether we should be advancing the animations in the preview window. */
	UBOOL	bTickAnimTree;

	/** If true, draw bones on preview skeleton. */
	UBOOL	bShowSkeleton;

	/** If true, draw bone names on preview skeleton. */
	UBOOL	bShowBoneNames;

	/** If true, draw skeletal mesh in wireframe. */
	UBOOL	bShowWireframe;

	/** If true, draw floor mesh and allow feet to collide with it. */
	UBOOL	bShowFloor;

	/** Show the global percentage weight of each node in the top-right of the node. */
	UBOOL	bShowNodeWeights;

	/** 
	 *	If we are shutting down (will be true after OnClose is called). 
	 *	Needed because wxWindows does not call destructor right away, so things can get ticked after OnClose.
	 */
	UBOOL	bEditorClosing;

	/** Static array of all AnimNode classes. */
	static TArray<UClass*>	AnimNodeClasses;

	/** Static array of all SkelControl classes. */
	static TArray<UClass*>	SkelControlClasses;

	/** Static array of all MorphNode classes. */
	static TArray<UClass*>	MorphNodeClasses;

	/** 
	 *	Indicates if AnimNodeClasses and SkelControlClasses arrays have already been initialised. 
	 *	This is only done once, as new classes cannot be created at runtime. 
	 */
	static UBOOL			bAnimClassesInitialized;

	WxPropertyWindowHost*			PropertyWindow;
	FLinkedObjViewportClient*		LinkedObjVC;
	UTexture2D*						BackgroundTexture;
	class WxAnimTreeEditorToolBar*	ToolBar;


	FAnimTreeEdPreviewVC*		PreviewVC;
	UAnimTreeEdSkelComponent*	PreviewSkelComp;
	UStaticMeshComponent*		FloorComp;
	#if WITH_APEX
	FRBPhysScene*			RBPhysScene;
	#endif
	WxAnimTreeEditor();
	WxAnimTreeEditor( wxWindow* InParent, wxWindowID InID, class UAnimTree* InAnimTree );
	~WxAnimTreeEditor();

	void OnClose( wxCloseEvent& In );
	void RefreshViewport();

	/**
	 * This function is called when the window has been selected from within the ctrl + tab dialog.
	 */
	virtual void OnSelected();
	
	// FLinkedObjViewportClient interface

	virtual void DrawObjects(FViewport* Viewport, FCanvas* Canvas);
	virtual void OpenNewObjectMenu();
	virtual void OpenObjectOptionsMenu();
	virtual void OpenConnectorOptionsMenu();
	virtual void DoubleClickedObject(UObject* Obj);
	virtual void UpdatePropertyWindow();

	virtual void EmptySelection();
	virtual void AddToSelection( UObject* Obj );
	virtual void RemoveFromSelection( UObject* Obj );
	virtual UBOOL IsInSelection( UObject* Obj ) const;
	virtual INT GetNumSelected() const;

	template <class T> 
	INT GetNumSelectedByClass() const
	{
		INT nTotal=0;

		for (INT I=0; I<SelectedNodes.Num(); ++I)
		{
			if (SelectedNodes(I)->IsA(T::StaticClass()))	
			{
				++nTotal;
			}
		}

		return nTotal;
	}

	template< class T >
	INT GetNodeByClass(TArray<T*>& OutList)
	{
		OutList.Empty();

		for (INT I=0; I<TreeNodes.Num(); ++I)
		{
			if (TreeNodes(I)->IsA(T::StaticClass()))
			{
				OutList.AddItem(TreeNodes(I));
			}
		}

		return OutList.Num();
	}

	template< class T >
	INT GetSelectedNodeByClass(TArray<T*>& OutList)
	{
		OutList.Empty();

		for (INT I=0; I<SelectedNodes.Num(); ++I)
		{
			if (SelectedNodes(I)->IsA(T::StaticClass()))
			{
				OutList.AddItem(Cast<T>(SelectedNodes(I)));
			}
		}

		return OutList.Num();
	}

	template< class T >
	T* GetFirstSelectedNodeByClass()
	{
		for (INT I=0; I<SelectedNodes.Num(); ++I)
		{
			if (SelectedNodes(I)->IsA(T::StaticClass()))
			{
				return Cast<T>(SelectedNodes(I));
			}
		}

		return NULL;
	}

	// TODO: ideally, best to cache index and search from there. 
	template< class T >
	T* GetNextSelectedNodeByClass(const T* Current)
	{
		UBOOL bSearch=FALSE;
		for (INT I=0; I<SelectedNodes.Num(); ++I)
		{
			if ( bSearch == FALSE )
			{
				if ( SelectedNodes(I) == Current )
				{
					bSearch = TRUE;
				}
			}	
			else
			{
				if (SelectedNodes(I)->IsA(T::StaticClass()))
				{
					return Cast<T>(SelectedNodes(I));
				}
			}
		}

		return NULL;
	}

	template< class T >
	void AppendToTreeNode(const TArray<T*>& NewList)
	{
		UAnimObject * NewObj;

		for (INT I=0; I<NewList.Num(); ++I)
		{
			NewObj = Cast<UAnimObject>(NewList(I));
			if ( NewObj )
			{
				TreeNodes.AddItem( NewObj );
			}
		}
	}

	virtual void SetSelectedConnector( FLinkedObjectConnector& Connector );
	virtual FIntPoint GetSelectedConnLocation(FCanvas* Canvas);
	virtual INT GetSelectedConnectorType();
	virtual FColor GetMakingLinkColor();

	// Make a connection between selected connector and an object or another connector.
	virtual void MakeConnectionToConnector( FLinkedObjectConnector& Connector );
	virtual void MakeConnectionToObject( UObject* EndObj );

	/**
	 * Called when the user releases the mouse over a link connector and is holding the ALT key.
	 * Commonly used as a shortcut to breaking connections.
	 *
	 * @param	Connector	The connector that was ALT+clicked upon.
	 */
	virtual void AltClickConnector(FLinkedObjectConnector& Connector);

	virtual void MoveSelectedObjects( INT DeltaX, INT DeltaY );
	virtual void EdHandleKeyInput(FViewport* Viewport, FName Key, EInputEvent Event);
	virtual void SpecialDrag( INT DeltaX, INT DeltaY, INT NewX, INT NewY, INT SpecialIndex );
	virtual UBOOL SpecialClick( INT NewX, INT NewY, INT SpecialIndex, FViewport* Viewport, UObject* ProxyObj );

	// FSerializableObject interface
	void Serialize(FArchive& Ar);

	// FNotifyHook interface

	virtual void NotifyDestroy( void* Src );
	virtual void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange );
	virtual void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );
	virtual void NotifyExec( void* Src, const TCHAR* Cmd );

	// WxAnimTreeEditor interface

	void SetPreviewNode(class UAnimNode* NewPreviewNode);
	void TickPreview(FLOAT DeltaSeconds);

	// Menu handlers
	void OnNewAnimNode( wxCommandEvent& In );
	void OnNewSkelControl( wxCommandEvent& In );
	void OnNewMorphNode( wxCommandEvent& In );
	void OnBreakLink( wxCommandEvent& In );
	void OnBreakAllLinks( wxCommandEvent& In );
	void OnAddInput( wxCommandEvent& In );
	void OnRemoveInput( wxCommandEvent& In );
	void OnNameInput( wxCommandEvent& In );
	void OnAddControlHead( wxCommandEvent& In );
	void OnRemoveControlHead( wxCommandEvent& In );
	void OnChangeBoneControlHead(wxCommandEvent& In);
	void OnDeleteObjects( wxCommandEvent& In );

	void OnToggleTickAnimTree( wxCommandEvent& In );
	void OnPreviewSelectedNode( wxCommandEvent& In );
	void OnShowNodeWeights( wxCommandEvent& In );
	void OnShowSkeleton( wxCommandEvent& In );
	void OnShowBoneNames( wxCommandEvent& In );
	void OnShowWireframe( wxCommandEvent& In );
	void OnShowFloor( wxCommandEvent& In );

	void OnCopy( wxCommandEvent &In );
	void OnPaste( wxCommandEvent &In );
	void OnDuplicate( wxCommandEvent &In );
	void OnNewComment( wxCommandEvent &In );
	void OnContextCommentToFront( wxCommandEvent &In );
	void OnContextCommentToBack( wxCommandEvent &In );

	void OnPreviewMeshListCombo( wxCommandEvent &In );
	void OnPreviewAnimSetListCombo( wxCommandEvent &In );
	void OnPreviewSocketListCombo( wxCommandEvent &In );

	void OnPreviewAnimSetCombo( wxCommandEvent &In );
	void OnPreviewAnimSequenceCombo( wxCommandEvent &In );
	void OnPreviewRateChanged( wxScrollEvent& In );

	void UpdatePreviewMeshListCombo();
	void UpdatePreviewAnimSetListCombo();
	void UpdatePreviewSocketListCombo();

	void UpdatePreviewMesh();
	void UpdatePreviewAnimSet();
	void UpdatePreviewSocket();

	/** 
	 * Refresh AnimSet Combo List (called init or post edit of preview animsets)
	 * Triggers UpdatePreviewAnimSequenceCombo
	 */
	void RefreshPreviewAnimSetCombo();

	/**
	 * Refresh AnimSequence List whenever Animset Changes 
	 */
	void RefreshPreviewAnimSequenceCombo();

	/**
	 * Update/Set Currently Selected  sequence name
	 * for AnimSequenceCombo - if AnimNodeSequence is selected
	 *
	 * @param: SequenceName - Sequence name to select
	 */
	void UpdateCurrentlySelectedAnimSequence();

	/**
	* Find Currently Selected  sequence name
	* for AnimSequenceCombo - if AnimNodeSequence is selected
	* This is triggered by add/remove selection code
	* Can't just refresh animset combo - that can create cycling
	*/
	void FindCurrentlySelectedAnimSequence();

	/**
	 * Change sequence name
	 * if AnimNodeSequence is selected
	 *
	 * @param: NewSequenceName - Sequence name to change
	 */
	void ReplaceSequenceName(const FName NewSequenceName);

	/** 
	 * This is used by previewing anim node sequence
	 * This only returns if only one AnimNodeSequence is selected
	 * if multiple, this is not going to return first selected on
	 * Use GetFirstSelectedNodeByClass<UAnimNodeSequence>() 
	 * if you need to get first selected animnodeseuqence instead 
	 */
	UAnimNodeSequence * GetCurrentlySelectedAnimNodeSequence();

	void OnAddNewEntryPreviewSkelMesh( wxCommandEvent& In );
	void OnAddNewEntryPreviewAnimSet( wxCommandEvent& In );
	void OnAddNewEntryPreviewSocket( wxCommandEvent& In );

	// Utils
	void DeleteSelectedObjects();
	void DuplicateSelectedObjects();
	void ReInitAnimTree();
	void BreakLinksToNode(UAnimNode* InNode);
	void BreakLinksToControl(USkelControlBase* InControl);
	void BreakLinksToMorphNode(UMorphNodeBase* InNode);

	UAnimNodeEditInfo* FindAnimNodeEditInfo(UAnimNode* InNode);

	// Socket previewing
	UPrimitiveComponent*	SocketComponent;
	void RecreateSocketPreview();
	void ClearSocketPreview();

	// Static, initialisation stuff
	static void InitAnimClasses();

protected:

	void Copy();
	void Paste();

	INT PasteCount;

	FIntRect CalcBoundingBoxOfSelected();
	FIntRect CalcBoundingBoxOfSequenceObjects(const TArray<UAnimObject*>& InAnimObjects);

	/**
	 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
	 *  @return A string representing a name to use for this docking parent.
	 */
	virtual const TCHAR* GetDockingParentName() const;

	/**
	 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
	 */
	virtual const INT GetDockingParentVersion() const;

	DECLARE_EVENT_TABLE()
};

#endif
