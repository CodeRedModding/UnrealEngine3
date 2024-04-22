/*=============================================================================
	SoundCueEditor.h: SoundCue editing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SOUNDCUEEDITOR_H__
#define __SOUNDCUEEDITOR_H__

#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	WxSoundCueEdToolBar
-----------------------------------------------------------------------------*/

class WxSoundCueEdToolBar : public WxToolBar
{
public:
	WxSoundCueEdToolBar( wxWindow* InParent, wxWindowID InID );
	~WxSoundCueEdToolBar();

	WxMaskedBitmap PlayCueB, PlayNodeB, StopB;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxSoundCueEditor
-----------------------------------------------------------------------------*/
class WxSoundCueEditorEditMenu;

class WxSoundCueEditor : public WxLinkedObjEd
{
public:
	WxSoundCueEditor( wxWindow* InParent, wxWindowID InID, class USoundCue* InSoundCue );
	virtual ~WxSoundCueEditor();

	// LinkedObjEditor interface
	virtual void InitEditor();

	/**
	 * @return Returns the name of the inherited class, so we can generate .ini entries for all LinkedObjEd instances.
	 */
	virtual const TCHAR* GetConfigName() const
	{
		return TEXT( "SoundCueEditor" );
	}

	/**
     * Creates the controls for this window
     */
	virtual void CreateControls( UBOOL bTreeControl );

	virtual void OpenNewObjectMenu();
	virtual void OpenObjectOptionsMenu();
	virtual void OpenConnectorOptionsMenu();
	virtual void DrawObjects( FViewport* Viewport, FCanvas* Canvas );
	virtual void UpdatePropertyWindow();

	virtual void EmptySelection();
	virtual void AddToSelection( UObject* Obj );
	virtual UBOOL IsInSelection( UObject* Obj ) const;
	virtual INT GetNumSelected() const;

	virtual void SetSelectedConnector( FLinkedObjectConnector& Connector );
	virtual FIntPoint GetSelectedConnLocation( FCanvas* Canvas );

	// Make a connection between selected connector and an object or another connector.
	virtual void MakeConnectionToConnector( FLinkedObjectConnector& Connector );
	virtual void MakeConnectionToObject( UObject* EndObj );

	/**
	 * Called when the user releases the mouse over a link connector and is holding the ALT key.
	 * Commonly used as a shortcut to breaking connections.
	 *
	 * @param	Connector	The connector that was ALT+clicked upon.
	 */
	virtual void AltClickConnector( FLinkedObjectConnector& Connector );

	virtual void MoveSelectedObjects( INT DeltaX, INT DeltaY );
	virtual void EdHandleKeyInput( FViewport* Viewport, FName Key, EInputEvent Event );

	// Static, initialiasation stuff
	static void InitSoundNodeClasses();

	// Utils
	void ConnectNodes( USoundNode* ParentNode, INT ChildIndex, USoundNode* ChildNode );
	void DeleteSelectedNodes();

	/**
	* Uses the global Undo transactor to redo changes, update viewports etc.
	*/
	void Undo();

	/**
	* Uses the global Redo transactor to undo changes, update viewports etc.
	*/
	void Redo();

	/** 
	 * Copy the current selection
	 */
	void Copy();

	/** 
	 * Cut the current selection (just a copy and a delete)
	 */
	void Cut();

	/** 
	 * Paste the copied selection
	 */
	void Paste();

	// Context Menu handlers
	void OnContextNewSoundNode( wxCommandEvent& In );
	void OnContextNewWave( wxCommandEvent& In );
	void OnContextNewRandom( wxCommandEvent& In );
	void OnContextAddInput( wxCommandEvent& In );
	void OnContextDeleteInput( wxCommandEvent& In );
	void OnContextDeleteNode( wxCommandEvent& In );
	void OnContextPlaySoundNode( wxCommandEvent& In );
	void OnContextSyncInBrowser( wxCommandEvent& In );
	void OnContextPlaySoundCue( wxCommandEvent& In );
	void OnContextStopPlaying( wxCommandEvent& In );
	void OnContextBreakLink( wxCommandEvent& In );
	// Edit Menu handlers
	void OnMenuCut( wxCommandEvent& In );
	void OnMenuCopy( wxCommandEvent& In );
	void OnMenuPaste( wxCommandEvent& In );
	void OnSize( wxSizeEvent& In );

	/**
	 * Used to serialize any UObjects contained that need to be to kept around.
	 *
	 * @param Ar The archive to serialize with
	 */
	virtual void Serialize( FArchive& Ar );

	/**
	 * Sets all Sounds in the SelectedPackages to the SoundClass specified by SoundClassID
	 * 
	 * @param SelectedPackages	Packages to search inside for SoundCues.
	 * @param SoundClassID		The ID of the sound class to set all sounds to.
	 */
	static void BatchProcessSoundClass( const TArray<UPackage*>& SelectedPackages, INT SoundClassID );

	/**
	 * Clusters sounds into a cue through a random node, based on wave name, and optionally an attenuation node.
	 * 
	 * @param SelectedPackages			Packages to search inside for SoundNodeWaves 
	 * @param bIncludeAttenuationNode	If true an attenuation node will be created in the SoundCues
	 */
	static void BatchProcessClusterSounds( const TArray< UPackage*>& SelectedPackages, UBOOL bIncludeAttenuationNode );

	/**
	 * Sets up all sound cues in the given packages to play a radio chirp if possible. 
	 * All sound cues must have an attenuation node and wave nodes to be eligible. 
	 *
	 * @param	SelectedPackages	The set of packages containing sound cues to add chirps. 
	 */
	static void BatchProcessInsertRadioChirp( const TArray< UPackage*>& SelectedPackages );

	/**
	 * Sets up all passed in sound cues to play a radio chirp if possible. 
	 * All sound cues must have an attenuation node and wave nodes to be eligible. 
	 *
	 * @param	SelectedObjects	The list of sound cues to add radio chirps to
	 */
	static void BatchProcessInsertRadioChirp( const TArray< UObject*>& SelectedObjects );

	/**
	 * Sets up all passed in sound cues to have a mature and non-mature version if possible
	 * It does this by inserting a mature node into the cue with a mature and non mature version of the wave already in the cue
	 * All sound cues must have an attenuation node and wave nodes to be eligible. 
	 *
	 * @param	SelectedObjects	The list of sound cues to add mature nodes to.
	 */
	static void BatchProcessInsertMatureNode( const TArray< UObject*>& SelectedObjects );

protected:

	/**
	 * One or more objects changed, so mark the package as dirty
	 */
	virtual void NotifyObjectsChanged();

public:

	WxSoundCueEdToolBar* ToolBar;

	// SoundCue currently being edited
	class USoundCue* SoundCue;

	// Currently selected USoundNodes
	TArray<class USoundNode*> SelectedNodes;

	// Selected Connector
	UObject* ConnObj; // This is usually a SoundNode, but might be the SoundCue to indicate the 'root' connector.
	INT ConnType;
	INT ConnIndex;
	
	WxSoundCueEditorEditMenu* MenuBar;

	// Static list of all SequenceObject classes.
	static TArray<UClass*>	SoundNodeClasses;
	static UBOOL			bSoundNodeClassesInitialized;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxMBSoundCueEdNewNode
-----------------------------------------------------------------------------*/

class WxMBSoundCueEdNewNode : public wxMenu
{
public:
	WxMBSoundCueEdNewNode( WxSoundCueEditor* CueEditor );
	~WxMBSoundCueEdNewNode();
};

/*-----------------------------------------------------------------------------
	WxMBSoundCueEdNodeOptions
-----------------------------------------------------------------------------*/

class WxMBSoundCueEdNodeOptions : public wxMenu
{
public:
	WxMBSoundCueEdNodeOptions( WxSoundCueEditor* CueEditor );
	~WxMBSoundCueEdNodeOptions();
};

/*-----------------------------------------------------------------------------
	WxMBSoundCueEdConnectorOptions
-----------------------------------------------------------------------------*/

class WxMBSoundCueEdConnectorOptions : public wxMenu
{
public:
	WxMBSoundCueEdConnectorOptions( WxSoundCueEditor* CueEditor );
	~WxMBSoundCueEdConnectorOptions();
};

#endif // __SOUNDCUEEDITOR_H__
