/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MeshPaintWindowShared_h__
#define __MeshPaintWindowShared_h__

#ifdef _MSC_VER
	#pragma once
#endif


// NOTE: This file is included as MANAGED for CLR .cpp files, but UNMANAGED for the rest of the app

#include "InteropShared.h"



#ifdef __cplusplus_cli


// ... MANAGED ONLY definitions go here ...

ref class MMeshPaintWindow;
ref class MMeshPaintFrame;
ref class MWPFFrame;
ref class MImportColorsScreenPanel;
#else // #ifdef __cplusplus_cli


// ... NATIVE ONLY definitions go here ...


#endif // #else

/**
* Window class for importing Vertex colors from texture used by Mesh Paint window class (shared between native and managed code)
*/
class FImportColorsScreen : public FCallbackEventDevice
{
public:

	/** Display the ImportColors screen */
	static void DisplayImportColorsScreen();

	/** Shut down the ImportColors screen singleton */
	static void Shutdown();

protected:
	/** Override from FCallbackEventDevice to handle events */
	virtual void Send( ECallbackEventType Event );

private:
	/** Constructor */
	FImportColorsScreen();

	/** Destructor */
	~FImportColorsScreen();

	// Copy constructor and assignment operator intentionally left unimplemented
	FImportColorsScreen( const FImportColorsScreen& );
	FImportColorsScreen& operator=( const FImportColorsScreen& );

	/**
	* Return internal singleton instance of the class
	*
	* @return	Reference to the internal singleton instance of the class
	*/
	static FImportColorsScreen& GetInternalInstance();

	/** Frame used for the ImportColors screen */
	GCRoot( MWPFFrame^ ) ImportColorsScreenFrame;

	/** Panel used for the ImportColors screen */
	GCRoot( MImportColorsScreenPanel^ ) ImportColorsScreenPanel;

	/** Singleton instance of the class */
	static FImportColorsScreen* Instance;
};


/**
 * Mesh Paint window class (shared between native and managed code)
 */
class FMeshPaintWindow
	: public FCallbackEventDevice
{

public:

	/** Static: Allocate and initialize mesh paint window */
	static FMeshPaintWindow* CreateMeshPaintWindow( class FEdModeMeshPaint* InMeshPaintSystem );


public:

	/** Constructor */
	FMeshPaintWindow();

	/** Destructor */
	virtual ~FMeshPaintWindow();

	/** Initialize the mesh paint window */
	UBOOL InitMeshPaintWindow( FEdModeMeshPaint* InMeshPaintSystem );

	/** Refresh all properties */
	void RefreshAllProperties();

	/** Saves window settings to the Mesh Paint settings structure */
	void SaveWindowSettings();

	/** Returns true if the mouse cursor is over the mesh paint window */
	UBOOL IsMouseOverWindow();

	/** FCallbackEventDevice: Called when a parameterless global event we've registered for is fired */
	void Send( ECallbackEventType Event );

	/** FCallbackEventDevice: Called when a global event we've registered for is fired with an attached object*/
	void Send( ECallbackEventType Event, UObject* EventObject );

    /** FCallbackEventDevice: Called when the viewport has been resized. */
    void Send( ECallbackEventType Event, class FViewport* EventViewport, UINT InMessage);

	/** FCallbackEventDevice: Notifies all observers that are registered for this event type */
	void Send(ECallbackEventType InType,const FString& InString, UObject* InObject);

	/** Called from edit mode when actor selection is changed. */
	void RefreshTextureTargetList();

	/** Called from edit mode after painting to ensure texture properties are up to date. */
	void RefreshTextureTargetListProperties();

	/** Called from edit mode when the transaction buffer size grows too large. */
	void TransactionBufferSizeBreech(bool bIsBreeched);

protected:

	/** Managed reference to the actual window control */
	//AutoGCRoot( MMeshPaintWindow^ ) WindowControl;
	AutoGCRoot( MMeshPaintFrame^ ) MeshPaintFrame;
	AutoGCRoot( MMeshPaintWindow^ ) MeshPaintPanel;

	FPickColorStruct PaintColorStruct;
	FPickColorStruct EraseColorStruct;
};



#endif	// __MeshPaintWindowShared_h__