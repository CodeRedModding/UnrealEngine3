/*=============================================================================
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ContentBrowserCLR_h__
#define __ContentBrowserCLR_h__

#ifdef _MSC_VER
#pragma once
#endif

// NOTE: This file is included as MANAGED for CLR .cpp files, but UNMANAGED for the rest of the app

#include "InteropShared.h"
#include "SourceControl.h"

#ifdef __cplusplus_cli

// ... MANAGED ONLY definitions go here ...

ref class MContentBrowserControl;


#else // #ifdef __cplusplus_cli

// ... NATIVE ONLY definitions go here ...

#endif // #else

// forward decl.
struct FSelectedAssetInfo;

/**
 * Content browser class (shared between native and managed code)
 */
class FContentBrowser
	: public FSerializableObject
	, public FTickableObject
	, public FCallbackEventDevice
#if HAVE_SCC
	, public FSourceControlEventListener
#endif
{

public:

	/** Static: Allocate and initialize content browser */
	static FContentBrowser* CreateContentBrowser( class WxContentBrowserHost* InParentBrowser, const HWND InParentWindowHandle );

	/** Static: Returns true if at least one content browser has been allocated */
	static UBOOL IsInitialized( const INT InstanceIndex = 0 )
	{
		return ( ContentBrowserInstances.Num() > InstanceIndex );
	}

	/** Static: Access instance of content browser that the user focused last */
	static FContentBrowser& GetActiveInstance( )
	{
		for ( int CBIndex = 0; CBIndex < ContentBrowserInstances.Num(); ++CBIndex )
		{
			if ( ContentBrowserInstances(CBIndex)->bIsActiveInstance )
			{
				return *ContentBrowserInstances( CBIndex );
			}
		}

		// In case we do not find an appropriate instance of content browser, return the default one.
		check( ContentBrowserInstances.Num() > 0 );
		return *ContentBrowserInstances( 0 );
	}

	static UBOOL ShouldUseContentBrowser()
	{
		static UBOOL bShouldUseContentBrowser = TRUE;
		static UBOOL bInitialized = FALSE;
		if (!bInitialized)
		{
			UBOOL bUseContentBrowserFromIni;
			const UBOOL bReadSucceeded = GConfig->GetBool(TEXT("BrowserWindows"), TEXT("UseContentBrowser"), bUseContentBrowserFromIni, GEditorUserSettingsIni);
			bShouldUseContentBrowser = !ParseParam( appCmdLine(), TEXT("NoContentBrowser") ) && (!bReadSucceeded || bUseContentBrowserFromIni);
			bInitialized = TRUE;
		}
		return bShouldUseContentBrowser;
	}

protected:
	/** Static: Access an global instance of the specified content browser */
	static FContentBrowser& GetInstance( const INT InstanceIndex = 0 )
	{
		check( ContentBrowserInstances.Num() > InstanceIndex );
		return *ContentBrowserInstances( InstanceIndex );
	}


public:

	/** Destructor */
	virtual ~FContentBrowser();

	/** Resize the window */
	void Resize( HWND hWndParent, int x, int y, int Width, int Height );

	/**
	 * Propagates focus from the wxWidgets framework to the WPF framework.
	 */
	void SetFocus();

	/**
	 * Places focus straight into the Search filter
	 */
	void GoToSearch();

	/**
	 * Display the specified objects in the content browser
	 *
	 * @param	InObjects	One or more objects to display
	 */
	void SyncToObjects( TArray< UObject* >& InObjects );


	/** 
	 * Select the specified packages in the content browser
	 * 
	 * @param	InPackages	One or more packages to select
	 */
	void SyncToPackages( const TArray< UPackage* >& InPackages );


	/** 
	 * Creates a local, unsaved collection
	 * 
	 * @param	InCollectionName	The name of the collection to create
	 */
	void CreateLocalCollection( const FString& InCollectionName );


	/** 
	 * Destroys a local collection
	 * 
	 * @param	InCollectionName	The name of the collection to destroy
	 */
	void DestroyLocalCollection( const FString& InCollectionName );

	
	/** 
	 * Adds assets to a local collection
	 * 
	 * @param	InCollectionName	Name of the collection where assets should be added
	 * @param	AssetsToAdd			List of assets to add to the collection
	 *
	 * @return TRUE is succeeded, FALSE if failed. 
	 */
	bool AddAssetsToLocalCollection( const FString& InCollectionName, const TArray<UObject*>& AssetsToAdd );

	/** 
	 * Remove assets from a local collection
	 * 
	 * @param	InCollectionName	Name of the collection to remove assets from
	 * @param	AssetsToRemvoe		List of assets to remove from the collection
	 */
	void RemoveAssetsFromLocalCollection( const FString& InCollectionName, const TArray<UObject*>& AssetsToRemove );

	/** 
	 * Selects the collection in the content browser
	 * 
	 * @param	InCollectionToSelect	Name of the collection to select
	 * @param	InCollectionType		Type of the collection which should be selected
	 */
	void SelectCollection( const FString& InCollectionToSelect, EGADCollection::Type InCollectionType );

	/**
	 * Generate a list of assets from a marshaled string.
	 * 
	 * @param	MarshaledAssetString		a string containing references one or more assets.
	 * @param	out_SelectedAssets			receieves the list of assets parsed from the string.
	 * 
	 * @return	TRUE if at least one asset item was parsed from the string.
	 */
	static UBOOL UnmarshalAssetItems( const FString& MarshaledAssetString, TArray<FSelectedAssetInfo>& out_ClassPathPairs );

	/**
	 * Wrapper method for building a string containing a list of class name + path names delimited by tokens.
	 *
	 * @param	out_ResultString	receives the value of the tokenized string containing references to the selected assets.
	 *
	 * @return	the number of currently selected assets
	 */
	INT GenerateSelectedAssetString( FString* out_ResultString );

	/**
	 * Returns whether saving the specified package is allowed
	 */
	UBOOL AllowPackageSave( UPackage* PackageToSave );

	/**
	 * Wrapper for determining whether an asset is eligible to be loaded on its own.
	 * 
	 * @param	AssetPathName	the fully qualified [Unreal] pathname of the asset to check
	 * 
	 * @return	true if the specified asset can be loaded on its own
	 */
	static UBOOL IsAssetValidForLoading( const FString& AssetPathName );

	/**
	 * Wrapper for determining whether an asset is eligible to be placed in a level.
	 * 
	 * @param	AssetPathName	the fully qualified [Unreal] pathname of the asset to check
	 * 
	 * @return	true if the specified asset can be placed in a level
	 */
	static UBOOL IsAssetValidForPlacing( const FString& AssetPathName );

	const TArray<UClass*>* GetSharedThumbnailClasses() const;


	/** Returns the map of classes to browsable object types */
	const TMap< UClass*, TArray< UGenericBrowserType* > >& GetBrowsableObjectTypeMap() const;



protected:

	/** Constructor */
	FContentBrowser();

	/**
	 * Initialize the content browser
	 *
	 * @param	InParentBrowser			Parent browser window (or NULL if we're not parented to a browser.)
	 * @param	InParentWindowHandle	Parent window handle
	 *
	 * @return	TRUE if successful
	 */
	UBOOL InitContentBrowser( WxContentBrowserHost* InParentBrowser, const HWND InParentWindowHandle );

	/** FSerializableObject: Serialize object references for garbage collector */
	virtual void Serialize( FArchive& Ar );


	/**
	 * Used to determine if an object should be ticked when the game is paused.
	 * Defaults to false, as that mimics old behavior.
	 *
	 * @return TRUE if it should be ticked when paused, FALSE otherwise
	 */
	virtual UBOOL IsTickableWhenPaused() const
	{
		return TRUE;
	}


	/**
	 * Used to determine whether the object should be ticked in the editor.  Defaults to FALSE since
	 * that is the previous behavior.
	 *
	 * @return	TRUE if this tickable object can be ticked in the editor
	 */
	virtual UBOOL IsTickableInEditor() const
	{
		return TRUE;
	}


	/**
	 * Used to determine whether an object is ready to be ticked. This is 
	 * required for example for all UObject derived classes as they might be
	 * loaded async and therefore won't be ready immediately.
	 *
	 * @return	TRUE if class is ready to be ticked, FALSE otherwise.
	 */
	virtual UBOOL IsTickable() const
	{
		return TRUE;
	}


	/**
	 * Called from within UnLevTic.cpp after ticking all actors or from
	 * the rendering thread (depending on bIsRenderingThreadObject)
	 *
	 * @param DeltaTime	Game time passed since the last call.
	 */
	virtual void Tick( FLOAT DeltaTime );

public:
	/** FCallbackEventDevice: Called when a parameterless global event we've registered for is fired */
	virtual void Send( ECallbackEventType Event );

	/**
	 * Called for map change events.
	 *
	 * @param InType the event that was fired
	 * @param InFlags the flags for this event
	 */
	virtual void Send(ECallbackEventType InType,DWORD InFlags);


	/** FCallbackEventDevice: Called when a parameterless global event we've registered for is fired */
	virtual void Send( ECallbackEventType Event, UObject* InObject );

	/**
	 * Called for content browser updates.
	 */
	virtual void Send( const FCallbackEventParameters& Parms );

#if HAVE_SCC
	/** Callback when a command is done executing */
	virtual void SourceControlCallback(FSourceControlCommand* InCommand);
#endif

protected:

	/** Managed reference to the actual window control */
	GCRoot( MContentBrowserControl^ ) WindowControl;

	/** Static: List of currently-active Content Browser instances */
	static TArray< FContentBrowser* > ContentBrowserInstances;

public:
	/** Find an appropriate content browser and make it the active one */
	void MakeAppropriateContentBrowserActive();

protected:
	/** Make this ContentBrowser the sole active content browser */
	void MakeActive();

	/** Notifies an instance of the Content Browser that it is now responsible for the current selection */
	void OnBecameSelectionAuthority();

	/** Notify an instance of the Content Browser that it is giving up being the selection authority */
	void OnYieldSelectionAuthority();

	/** Whether this instance of the content browser is authority over selection */
	UBOOL bIsActiveInstance;
};



#endif	// __ContentBrowserCLR_h__

