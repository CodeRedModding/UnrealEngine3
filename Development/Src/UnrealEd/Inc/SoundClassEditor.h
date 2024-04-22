/*=============================================================================
	SoundCueEditor.h: SoundCue editing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SOUNDCLASSEDITOR_H__
#define __SOUNDCLASSEDITOR_H__

#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	WxSoundClassEditor
-----------------------------------------------------------------------------*/

class WxSoundClassEditor : public WxLinkedObjEd
{
public:
	WxSoundClassEditor( wxWindow* InParent, wxWindowID InID );
	virtual ~WxSoundClassEditor( void );

	// LinkedObjEditor interface
	virtual void InitEditor( void );

	/**
	 * @return Returns the name of the inherited class, so we can generate .ini entries for all LinkedObjEd instances.
	 */
	virtual const TCHAR* GetConfigName( void ) const
	{
		return TEXT( "SoundClassEditor" );
	}

	/**
     * Creates the controls for this window
     */
	virtual void CreateControls( UBOOL bTreeControl );

	virtual void OpenNewObjectMenu( void );
	virtual void OpenObjectOptionsMenu( void );
	virtual void OpenConnectorOptionsMenu();
	virtual void DrawObjects( FViewport* Viewport, FCanvas* Canvas );
	virtual void UpdatePropertyWindow( void );

	virtual void EmptySelection( void );
	virtual void AddToSelection( UObject* Obj );
	virtual UBOOL IsInSelection( UObject* Obj ) const;
	virtual INT GetNumSelected( void ) const;

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

	// Utils
	void ConnectClasses( USoundClass* ParentClass, INT ChildIndex, USoundClass* ChildClass );
	void DeleteSelectedClasses( void );

	// Context Menu handlers
	void OnContextNewSoundClass( wxCommandEvent& In );
	void OnContextDeleteSoundClass( wxCommandEvent& In );
	void OnContextAddInput( wxCommandEvent& In );
	void OnContextDeleteInput( wxCommandEvent& In );
	void OnContextDeleteNode( wxCommandEvent& In );
	void OnContextBreakLink( wxCommandEvent& In );
	void OnSize( wxSizeEvent& In );

	/**
	 * Used to serialize any UObjects contained that need to be to kept around.
	 *
	 * @param Ar The archive to serialize with
	 */
	virtual void Serialize( FArchive& Ar );

	/** Owning audio device */
	UAudioDevice* AudioDevice;

	/** Root sound class */
	USoundClass* MasterClass;

	// Currently selected USoundClasses
	TArray<class USoundClass*> SelectedClasses;

	// Selected Connector
	UObject* ConnObj; // This is usually a SoundNode, but might be the SoundCue to indicate the 'root' connector.
	INT ConnType;
	INT ConnIndex;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxMBSoundClassEdNewNode
-----------------------------------------------------------------------------*/

class WxMBSoundClassEdNewClass : public wxMenu
{
public:
	WxMBSoundClassEdNewClass( WxSoundClassEditor* ClassEditor );
	~WxMBSoundClassEdNewClass( void );
};

/*-----------------------------------------------------------------------------
	WxMBSoundClassEdClassOptions
-----------------------------------------------------------------------------*/

class WxMBSoundClassEdClassOptions : public wxMenu
{
public:
	WxMBSoundClassEdClassOptions( WxSoundClassEditor* ClassEditor );
	~WxMBSoundClassEdClassOptions( void );
};

/*-----------------------------------------------------------------------------
	WxMBSoundClassEdConnectorOptions
-----------------------------------------------------------------------------*/

class WxMBSoundClassEdConnectorOptions : public wxMenu
{
public:
	WxMBSoundClassEdConnectorOptions( WxSoundClassEditor* ClassEditor );
	~WxMBSoundClassEdConnectorOptions( void );
};

#endif // __SOUNDCLASSEDITOR_H__
