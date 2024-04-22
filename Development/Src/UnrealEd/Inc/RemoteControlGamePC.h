/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __REMOTECONTROLGAMEPC_H__
#define __REMOTECONTROLGAMEPC_H__

#include "RemoteControlGame.h"

// Foward declarations.
class WxRemoteControlFrame;

/**
 * Concrete implementation of FRemoteControlGame on the PC.
 */
class FRemoteControlGamePC : public FRemoteControlGame
{
public:
	FRemoteControlGamePC();
	virtual ~FRemoteControlGamePC();

	void SetFrame(WxRemoteControlFrame* InFrame);
	void SetPlayWorld(UWorld* InPlayWorld);

	void DestroyPropertyWindows();
	void RenderInGame();

	/** Finds an actor by name.	*/
	AActor* FindActor(const TCHAR* ActorName) const;

	/**
	 * Given a static factory for RemoteControl extensions, returns the extension
	 * interface. Creates the extension interface if this is the first request.
	 */
	virtual FRemoteControlGameExtension& GetExtension(const FRemoteControlGameExtensionFactory& factory);

	/** Used to access the game's map extension for the "Open Map" button. */
	virtual FString GetMapExtension() const;

	/**
	 * Assembles a list of UProperty names for the specified object's properties.
	 *
	 * @return		FALSE if the named object was not found, TRUE otherwise.
	 */
	virtual UBOOL GetPropertyList(const FString& ObjectName, TArray<FString>& OutPropList);

	/** Gets list of objects from an array property. */
	virtual void GetArrayObjectList(const FString& ObjectName, const FString& PropName, TArray<FString>& OutArrayPropList);

	// Is this object an actor?
	virtual UBOOL IsAActor(const FString& ObjectName);

	// Is this object and array property?
	virtual UBOOL IsArrayProperty(const FString& OwnerName, const FString& ObjectName);
	virtual UBOOL IsObjectProperty(const FString& OwnerName, const FString& ObjectName);

	// return an object's class name
	virtual FString GetObjectClass(const FString& ObjectName);
	virtual UBOOL GetObjectFromProperty(const FString& ObjectName, const FString& PropertyName, FString& PropertyObjectName);

	/** Callback interface for when RemoteControl is shown. */
	virtual void OnRemoteControlShow();
	/** Callback interface for when RemoteControl is hidden. */
	virtual void OnRemoteControlHide();

	// window maintenance
	virtual wxPoint GetRemoteControlPosition();
	virtual void SetFocusToGame();
	virtual void RepositionRemoteControl();
	// show an actor editor for the specified actor. 
	virtual UBOOL ShowEditActor(const TCHAR* ActorName);
	// refresh actor editor list -- clear out any ones that are no longer valid
	virtual void RefreshActorPropertyWindowList();
	
	// actor interface
	virtual void GetActorList(TArray<ActorDescription>& Actors, UBOOL bDynamicOnly);
	virtual void SetSelectedActor(const TCHAR* ClassName, const TCHAR* ActorName);
	virtual FString GetSelectedActor() const;

	/** @return		A handle to the local player, or NULL if none exists. */
	virtual ULocalPlayer* GetLocalPlayer() const;

	/** @return		A handle to the current play world.  Can handle PIE. */
	virtual UWorld* GetWorld() const;

	// stat interface
	virtual UBOOL IsStatEnabled(const TCHAR* StatGroup);
	virtual void GetStatList(TArray<FString>& StatGroups);

	// local player interface
	virtual void ExecConsoleCommand(const TCHAR *Command);
	virtual FString GetLocalPlayerObjectName();
	virtual EShowFlags GetShowFlags();
	virtual void GetDisplayInfo(UINT& Width, UINT& Height, UBOOL& bFullscreen);

	virtual UBOOL IsInEditor()
	{
		return GIsEditor;
	}

	// object property interface
	virtual UBOOL SetObjectProperty(const TCHAR* ClassName, const TCHAR* PropertyName, const TCHAR* ObjectName, const TCHAR* Value);
	virtual UBOOL GetObjectProperty(FString& value, const TCHAR* ClassName, const TCHAR* PropertyName, const TCHAR* ObjectName);

private:
	// Get an UObject by name
	virtual UObject* GetObject(const FString& ObjectName);

	// Get a property from an object by name
	virtual UProperty* GetProperty(const FString& OwnerName, const FString& PropertyName);

	UWorld* EditorWorld;

	WxRemoteControlFrame* Frame;
	UWorld* PlayWorld;
	TMap<FString, class WxPropertyWindowFrame*> ActorEditors;

	typedef TMap<const FRemoteControlGameExtensionFactory*,FRemoteControlGameExtension*> FExtensionMap;
	typedef TMap<const FRemoteControlGameExtensionFactory*,FRemoteControlGameExtension*>::TIterator FExtensionMapIterator;
	FExtensionMap Extensions;

	FString SelectedActorName;
	UBOOL  bRemoteControlShown;
};

#endif // __REMOTECONTROLGAMEPC_H__
