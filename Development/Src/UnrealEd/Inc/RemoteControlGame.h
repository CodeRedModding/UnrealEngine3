/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __REMOTECONTROLGAME_H__
#define __REMOTECONTROLGAME_H__

// Forward declarations.
class FRemoteControlGameExtension;
class FRemoteControlGameExtensionFactory;

/**
 * An abstract interface for communicating with the game. This allows us
 * to re-use the RemoteControl UI for both the PC game and the console game.
 */
class FRemoteControlGame
{
public:
	FRemoteControlGame();
	virtual ~FRemoteControlGame();

	virtual FRemoteControlGameExtension &GetExtension(const FRemoteControlGameExtensionFactory& InFactory)=0;

	template <class T>
	T &GetTypedExtension(FRemoteControlGameExtensionFactory &InFactory)
	{
		return static_cast<T &>(GetExtension(InFactory));
	}

	/** Callback interface for when RemoteControl is shown. */
	virtual void OnRemoteControlShow() {}
	/** Callback interface for when RemoteControl is hidden. */
	virtual void OnRemoteControlHide() {}

	/** Used to access the game's map extension for the "Open Map" button. */
	virtual FString GetMapExtension() const=0;

	/**
	 * Assembles a list of UProperty names for the specified object's properties.
	 *
	 * @return		FALSE if the named object was not found, TRUE otherwise.
	 */
	virtual UBOOL GetPropertyList(const FString& ObjectName, TArray<FString>& OutPropList)=0;

	/** Gets list of objects from an array property. */
	virtual void GetArrayObjectList(const FString& ObjectName, const FString& PropName, TArray<FString>& OutArrayPropList)=0;

	/** Is this object an actor? */
	virtual UBOOL IsAActor(const FString& ObjectName)=0;

	/** Is this object an array property? */
	virtual UBOOL IsArrayProperty(const FString& OwnerName, const FString& ObjectName)=0;
	virtual UBOOL IsObjectProperty(const FString& OwnerName, const FString& ObjectName)=0;

	// return an object's class name
	virtual FString GetObjectClass(const FString& ObjectName)=0;
	virtual UBOOL GetObjectFromProperty(const FString& ObjectName, const FString& PropertyName, FString& PropertyObjectName)=0;

	///////////////////////
	// Window maintenance.

	/** Get the position of the RemoteControl. */
	virtual wxPoint GetRemoteControlPosition() { return wxDefaultPosition; }
	/** Set the focus to the game. Does nothing for consoles. */
	virtual void SetFocusToGame() {}
	/** Reposition the RemoteControl. Does nothing for consoles. */
	virtual void RepositionRemoteControl() {}
	/** Show an actor editor for the specified actor. */
	virtual UBOOL ShowEditActor(const TCHAR *ActorName)=0;
	/** Refresh actor editor list -- clear out any ones that are no longer valid. */
	virtual void RefreshActorPropertyWindowList()=0;

	/** Actor interface. */
	struct ActorDescription
	{
		FString ClassName;
		FString ActorName;
		FString OwnerName;
	};

	virtual void GetActorList(TArray<ActorDescription> &OutActors, UBOOL bDynamicOnly)=0;
	virtual void SetSelectedActor(const TCHAR *ClassName, const TCHAR *ActorName)=0;
	virtual FString GetSelectedActor() const=0;

	/** @return		A handle to the local player, or NULL if none exists. */
	virtual ULocalPlayer* GetLocalPlayer() const= 0;

	/** @return		A handle to the current play world.  Can handle PIE. */
	virtual UWorld* GetWorld() const=0;

	/////////////////////////
	// Stat interface.

	virtual UBOOL IsStatEnabled(const TCHAR *StatGroup)=0;
	/**
	 * Toggles the named stat group.
	 */
	void ToggleStat(const TCHAR *StatGroup);
	virtual void GetStatList(TArray<FString> &StatGroups)=0;

	/////////////////////////////
	// Local player interface.

	virtual void ExecConsoleCommand(const TCHAR *Command)=0;
	virtual FString GetLocalPlayerObjectName()=0;
	virtual EShowFlags GetShowFlags()=0;
	virtual void GetDisplayInfo(UINT &OutWidth, UINT &OutHeight, UBOOL &OutbFullscreen)=0;
	virtual UBOOL IsInEditor()=0;

	////////////////////////////////
	// Object property interface.

	virtual UBOOL SetObjectProperty(const TCHAR *ClassName, const TCHAR *PropertyName, const TCHAR *ObjectName, const TCHAR *Value)=0;
	virtual UBOOL GetObjectProperty(FString &OutValue, const TCHAR *ClassName, const TCHAR *PropertyName, const TCHAR *ObjectName)=0;

	////////////////////////////////////
	// Convenience SetObjectProperty wrappers.

	/** Convenience function for setting a UBOOL property. Calls SetObjectProperty. */
	UBOOL SetObjectBoolProperty(const TCHAR *ClassName, const TCHAR *PropertyName, const TCHAR *ObjectName, UBOOL bValue);
	/** Convenience function for setting an INT property. Calls SetObjectProperty. */
	UBOOL SetObjectIntProperty(const TCHAR *ClassName, const TCHAR *PropertyName, const TCHAR *ObjectName, INT Value);
	/** Convenience function for setting a FLOAT property. Calls SetObjectProperty.	*/
	UBOOL SetObjectFloatProperty(const TCHAR *ClassName, const TCHAR *PropertyName, const TCHAR *ObjectName, FLOAT Value);

	/////////////////////////////////////
	// Convenience GetObjectProperty wrappers.

	/** Convenience function for getting a UBOOL property.  Calls GetObjectProperty. */
	UBOOL GetObjectBoolProperty(UBOOL &bOutValue, const TCHAR *ClassName, const TCHAR *PropertyName, const TCHAR *ObjectName);
	/** Convenience function for getting an INT property.  Calls GetObjectProperty.	*/
	UBOOL GetObjectIntProperty(INT &OutValue, const TCHAR *ClassName, const TCHAR *PropertyName, const TCHAR *ObjectName);
	/** Convenience function for getting a FLOAT property.  Calls GetObjectProperty. */
	UBOOL GetObjectFloatProperty(FLOAT &OutValue, const TCHAR *ClassName, const TCHAR *PropertyName, const TCHAR *ObjectName);
};

#endif // __REMOTECONTROLGAME_H__
