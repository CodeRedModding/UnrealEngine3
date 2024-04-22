/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "RemoteControlGamePC.h"
#include "RemoteControlFrame.h"
#include "RemoteControlGameExtension.h"
#include "PropertyWindow.h"

namespace
{
	/**
	 * Make an FName from a string.
	 */
	FName MakeFName(const TCHAR* InStr)
	{
		if(NULL == InStr || appStricmp(InStr, TEXT("")) == 0)
		{
			return NAME_None;
		}
		else
		{
			return FName(InStr, FNAME_Find);
		}
	}

	HWND GetGameWindow(FRemoteControlGamePC* Game)
	{
		ULocalPlayer* LocalPlayer = Game->GetLocalPlayer();
		if( LocalPlayer && LocalPlayer->ViewportClient->Viewport )
		{
			const HWND hwnd = (HWND)LocalPlayer->ViewportClient->Viewport->GetWindow();
			if( ::IsWindow(hwnd) )
			{
				return hwnd;
			}
		}
		return NULL;
	}
}

inline DWORD GetTypeHash(const FRemoteControlGameExtensionFactory* A)
{
	return PointerHash( A );
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FRemoteControlGamePC
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FRemoteControlGamePC::FRemoteControlGamePC()
	:	EditorWorld( NULL )
	,	Frame( NULL )
	,	PlayWorld( NULL )
	,	bRemoteControlShown( FALSE )
{
}

FRemoteControlGamePC::~FRemoteControlGamePC()
{
	for (FExtensionMapIterator It(Extensions); It; ++It)
	{
		delete It.Value();
	}
	Extensions.Empty();
}

void FRemoteControlGamePC::SetFrame(WxRemoteControlFrame* InFrame)
{
	Frame = InFrame;
}

void FRemoteControlGamePC::SetPlayWorld(UWorld* InPlayWorld)
{
	PlayWorld = InPlayWorld;
}

/**
 * Given a static factory for RemoteControl extensions, returns the extension
 * interface. Creates the extension interface if this is the first request.
 */
FRemoteControlGameExtension& FRemoteControlGamePC::GetExtension(const FRemoteControlGameExtensionFactory& InFactory)
{
	FRemoteControlGameExtension* Extension = Extensions.FindRef(&InFactory);
	if (Extension == NULL)
	{
		Extension = InFactory.CreateExtension(this);
		Extensions.Set(&InFactory,Extension);
	}
	return *Extension;
}

/**
 * @return		A handle to the current play world.  Can handle PIE.
 */
UWorld *FRemoteControlGamePC::GetWorld() const
{
	// Safely get the world regardless of whether we are play-in-editor or in a normal game.
	if ( GIsEditor )
	{
		// PIE.
		check( PlayWorld );
		return PlayWorld;
	}
	else
	{
		// Normal game.
		return GWorld;
	}
}

/**
 * Destroy all property windows. Called when PIE ends.
 */
void FRemoteControlGamePC::DestroyPropertyWindows()
{
	for(TMap<FString, WxPropertyWindowFrame*>::TIterator It(ActorEditors); It; ++It)
	{
		delete It.Value();
	}
	ActorEditors.Empty();
}

/**
 * Render in-game stuff. Used to fake actor selection.
 */
void FRemoteControlGamePC::RenderInGame()
{
	if( bRemoteControlShown && SelectedActorName.Len() > 0 )
	{
		AActor* Actor = FindActor(*SelectedActorName);
		if( Actor )
		{
			const FBox Box = Actor->GetComponentsBoundingBox();
			DrawWireBox(GetWorld()->LineBatcher, Box, FColor(0, 128, 255),SDPG_World);
		}
	}
}

/**
 * Callback for when RemoteControl is shown.
 */
void FRemoteControlGamePC::OnRemoteControlShow()
{
	bRemoteControlShown = TRUE;
}

/**
 * Callback for when RemoteControl is hidden.
 */
void FRemoteControlGamePC::OnRemoteControlHide()
{
	bRemoteControlShown = FALSE;
}

/**
 * This allows us to lock the RemoteControl to the game window in PC
 * but use a default position when hooked up to a console
 */
wxPoint FRemoteControlGamePC::GetRemoteControlPosition()
{
	// @todo: handle multi-monitor case.
	HWND hwnd = GetGameWindow( this );
	if( hwnd )
	{
		// lock to top right
		RECT gameWindowRect;

		::GetWindowRect(hwnd, &gameWindowRect);

		return wxPoint(gameWindowRect.right, gameWindowRect.top);
	}
	else
	{
		return wxDefaultPosition;
	}
}

/**
 * Set the control focus to the game. Does nothing when running against a console build.
 */
void FRemoteControlGamePC::SetFocusToGame()
{
	HWND hwnd = GetGameWindow( this );
	if( hwnd )
	{
		SetActiveWindow(hwnd);
	}
}

/**
 * Reposition the RemoteControl relative to the game window.
 */
void FRemoteControlGamePC::RepositionRemoteControl()
{
	if( Frame )
	{
		Frame->Reposition();
	}
}

/**
 * Show the actor editor for the specified actor.
 */
UBOOL FRemoteControlGamePC::ShowEditActor(const TCHAR* ActorName)
{
	WxPropertyWindowFrame* PropertyWindowFrame = ActorEditors.FindRef( ActorName );

	if( !PropertyWindowFrame )
	{
		// Make sure the actor exists before creating a property window for it.
		AActor* Actor = FindActor( ActorName );
		if( Actor )
		{
			// Create a property window.
			PropertyWindowFrame = new WxPropertyWindowFrame;
			PropertyWindowFrame->Create( NULL, -1);
			PropertyWindowFrame->DisallowClose();
			PropertyWindowFrame->SetObject( Actor, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories );
			ActorEditors.Set( ActorName, PropertyWindowFrame );
		}
		else
		{
			// The actor no longer exists.
			return FALSE;
		}
	}

	PropertyWindowFrame->Show();
	PropertyWindowFrame->Raise();
	return TRUE;
}

/**
    refresh actor editor list -- clear out any ones that are no longer valid
*/
void FRemoteControlGamePC::RefreshActorPropertyWindowList()
{
	for(TMap<FString, WxPropertyWindowFrame*>::TIterator It(ActorEditors); It; ++It)
	{
		// Find the actor.
		AActor* Actor = FindActor( *It.Key() );

		if( !Actor )
		{
			// Delete the window.
			delete It.Value();
			It.RemoveCurrent();
		}
		else
		{
			WxPropertyWindow::TObjectIterator ObjectIterator = It.Value()->ObjectIterator();
			check( ObjectIterator );
			if( *ObjectIterator != Actor )
			{
				// Refresh.
				It.Value()->SetObject( Actor, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories );
			}
		}
	}
}

namespace
{
	/**
	 * Helper function for GetActorList.
	 */
	template <class IteratorType>
		void GetActorListHelper(TArray<FRemoteControlGame::ActorDescription>& Actors, IteratorType& It)
	{
		for( ; It ; ++It )
		{
			AActor* Actor = *It;
			FRemoteControlGame::ActorDescription ActorDesc;
			ActorDesc.ClassName = Actor->GetClass()->GetName();
			ActorDesc.ActorName = Actor->GetPathName();
			if( Actor->Owner )
			{
				ActorDesc.OwnerName = Actor->Owner->GetName();
			}
			Actors.AddItem( ActorDesc );
		}
	}
}

/**
    Return the list of actors in the game.
*/
void FRemoteControlGamePC::GetActorList(TArray<ActorDescription>& Actors, UBOOL bDynamicOnly)
{
	Actors.Empty();

	UWorld* OldGWorld = NULL;
	// Use the PlayWorld as the GWorld if we're using PIE, but not if we are executing from an exec IN the PIE world
	if( PlayWorld && !GIsPlayInEditorWorld)
	{
		OldGWorld = SetPlayInEditorWorld( PlayWorld );
	}

	if( bDynamicOnly )
	{
		FDynamicActorIterator It;
		GetActorListHelper( Actors, It );
	}
	else
	{
		FActorIterator It;
		GetActorListHelper( Actors, It );
	}

	if (OldGWorld)
	{
		// Pop the world.
		RestoreEditorWorld( OldGWorld );
	}
}

/**
    Set the actor to highlight in the viewport.
*/
void FRemoteControlGamePC::SetSelectedActor(const TCHAR* ClassName, const TCHAR* ActorName)
{
	// Turn into an FName.
	SelectedActorName = ActorName;
}

FString FRemoteControlGamePC::GetSelectedActor() const
{
	return SelectedActorName;
}

/**
 * @return		A handle to the local player, or NULL if none exists.
 */
ULocalPlayer* FRemoteControlGamePC::GetLocalPlayer() const
{
	ULocalPlayer* LocalPlayer = NULL;
	if( GEngine && GEngine->GamePlayers.Num() > 0 )
	{
		LocalPlayer = GEngine->GamePlayers(0);
	}
	return LocalPlayer;
}

/**
 * Return whether a stat group is being shown or not.
 */
UBOOL FRemoteControlGamePC::IsStatEnabled(const TCHAR* StatGroup)
{
	UBOOL bStatEnabled = FALSE;
#if 0 // @todo: connect to new stats interface.
	// Grab the first local player.
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if( LocalPlayer )
	{
		bStatEnabled = LocalPlayer->ViewportClient->EnabledStats.ContainsItem( FString(StatGroup) );
	}
#endif
	return bStatEnabled;
}

/**
 * Enumerate the list of stats that are available.
 */
void FRemoteControlGamePC::GetStatList(TArray<FString>& StatGroups)
{
	StatGroups.Empty();
#if 0 // @todo: connect to new stats interface.
	for( FStatGroupOld* Group = GFirstStatGroup;Group;Group = Group->NextGroup )
	{
		StatGroups.AddItem(FString(Group->Label));
	}
#endif
}

/**
 * Execute a command as if the player had typed it on the console.
 */
void FRemoteControlGamePC::ExecConsoleCommand(const TCHAR *Command)
{
	// Grab the first local player.
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if( LocalPlayer )
	{
		LocalPlayer->Exec( Command, *GLog );
	}
}

/**
 * Get the *object* name of the local player.
 */
FString FRemoteControlGamePC::GetLocalPlayerObjectName() 
{
	FString LocalPlayerObjectName;

	// Grab the first local player.
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if( LocalPlayer )
	{
		LocalPlayerObjectName = LocalPlayer->GetName();
	}
	return LocalPlayerObjectName;
}

/**
 * Return the show flags for the current player viewport.
 */
EShowFlags FRemoteControlGamePC::GetShowFlags()
{
	EShowFlags ShowFlags = 0;

	// Grab the first local player.
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if( LocalPlayer )
	{
		ShowFlags = LocalPlayer->ViewportClient->ShowFlags;
	}

	return ShowFlags;
}

/**
 * Retrieve the display info about the player viewport.
 */
void FRemoteControlGamePC::GetDisplayInfo(UINT& Width, UINT& Height, UBOOL& bFullscreen)
{
	// Grab the first local player.
	ULocalPlayer* LocalPlayer = GetLocalPlayer();

	if( LocalPlayer && LocalPlayer->ViewportClient && LocalPlayer->ViewportClient->Viewport )
	{
		Width = LocalPlayer->ViewportClient->Viewport->GetSizeX();
		Height = LocalPlayer->ViewportClient->Viewport->GetSizeY();
		bFullscreen = LocalPlayer->ViewportClient->Viewport->IsFullscreen();
	}
	else
	{
		Width = 0;
		Height = 0;
		bFullscreen = FALSE;
	}
}

/**
 * Set a property on one or more objects.
 */
UBOOL FRemoteControlGamePC::SetObjectProperty(const TCHAR* InClassName, const TCHAR* InPropertyName, const TCHAR* InObjectName, const TCHAR* Value)
{
	const FName ClassName(MakeFName(InClassName));
	const FName PropertyName(MakeFName(InPropertyName));
	const FName ObjectName(MakeFName(InObjectName));

	UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassName.ToString());

	while( *Value == ' ' )
	{
		Value++;
	}

	UBOOL bFoundOne = FALSE;
	if( Class )
	{
		UProperty* Property = FindField<UProperty>(Class, PropertyName);

		if( Property )
		{
			if( ObjectName == NAME_None )
			{
				// code based on UObject::GlobalSetProperty
				for( FObjectIterator It ; It ; ++It )
				{
					if( It->IsA(Class) )
					{
						It->PreEditChange(Property);
						Property->ImportText(Value, (BYTE *) *It + Property->Offset, PPF_Localized, *It);
						FPropertyChangedEvent PropertyEvent(Property);
						It->PostEditChangeProperty(PropertyEvent);
						bFoundOne = TRUE;
					}
				}
			}
			else
			{
				UObject* Object = UObject::StaticFindObject(Class, ANY_PACKAGE, *ObjectName.ToString());

				if( Object )
				{
					Object->PreEditChange(Property);
					Property->ImportText(Value, (BYTE *) Object + Property->Offset, PPF_Localized, Object);
					FPropertyChangedEvent PropertyEvent(Property);
					Object->PostEditChangeProperty(PropertyEvent);
					bFoundOne = TRUE;
				}
			}
		}
	}
	return bFoundOne;
}

/**
    Get a property
*/
UBOOL FRemoteControlGamePC::GetObjectProperty(FString& OutValue, const TCHAR* InClassName, const TCHAR* InPropertyName, const TCHAR* InObjectName)
{
	FName ClassName(MakeFName(InClassName));
	FName PropertyName(MakeFName(InPropertyName));
	FName ObjectName(MakeFName(InObjectName));
	UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassName.ToString());

	if( !Class )
	{
		return FALSE;
	}

	UProperty* Property = FindField<UProperty>(Class, PropertyName);

	if( !Property )
	{
		return FALSE;
	}

	UObject *Object=NULL;

	if(ObjectName == NAME_None)
	{
		// Just look for first one?
		for(FObjectIterator It; It; ++It)
		{
			if(It->IsA(Class))
			{
				Object = *It;
				break;
			}
		}
	}
	else
	{
		// Try the world first.
		Object = FindObject<UObject>(GetWorld(), *ObjectName.ToString());
		if( !Object )
		{
			// Try any package.
			Object = FindObject<UObject>(ANY_PACKAGE, *ObjectName.ToString());
		}
	}

	UBOOL bFoundObject = FALSE;
	if( Object )
	{
		Property->ExportText(0, OutValue, (BYTE*)Object, (BYTE*)Object, Class, PPF_Localized);
		bFoundObject = TRUE;
	}
	return bFoundObject;
}

/**
 * Finds an actor by name.
 */
AActor* FRemoteControlGamePC::FindActor(const TCHAR* ActorName) const
{
	return Cast<AActor>( UObject::StaticFindObject(AActor::StaticClass(), NULL, ActorName) );
}

/**
 Get an UObject by name
*/
UObject* FRemoteControlGamePC::GetObject(const FString& ObjectName)
{
	UObject* Object = FindObject<UObject>(ANY_PACKAGE, *ObjectName);
	return Object;
}

/**
 Get a property from an object by name
 */
UProperty* FRemoteControlGamePC::GetProperty(const FString& OwnerName, const FString& PropertyName)
{
	UObject* OwnerObject = GetObject(OwnerName);
	UProperty* Property = FindField<UProperty>(OwnerObject->GetClass(), *PropertyName);

	return Property;
}

FString FRemoteControlGamePC::GetMapExtension() const
{
	return FURL::DefaultMapExt;
}

/**
	Returns the name of the UObject associated with an UObjectProperty
*/
UBOOL FRemoteControlGamePC::GetObjectFromProperty(const FString& ObjectName,
												  const FString& PropertyName,
												  FString& PropertyObjectName)
{
	UObject* Object = GetObject(ObjectName);
	check(Object);
	UProperty* Property = FindField<UProperty>(Object->GetClass(), *PropertyName);
	check(Property);
	UObject* PropertyObject = *((UObject**)(((BYTE*)Object) + Property->Offset));

	UBOOL bResult = FALSE;
	if ( PropertyObject && PropertyObject->GetFName() != FName(NAME_None) )
	{
		PropertyObjectName = PropertyObject->GetFName().ToString();
		bResult = TRUE;
	}

	return bResult;
}

/**
 * Assembles a list of UProperty names for the specified object's properties.
 *
 * @return		FALSE if the named object was not found, TRUE otherwise.
 */
UBOOL FRemoteControlGamePC::GetPropertyList(const FString& ObjectName, TArray<FString>& OutPropList)
{
	OutPropList.Empty();

	UBOOL bFoundObject = FALSE;
	UObject* Object = GetObject( ObjectName );
	if( Object )
	{
		bFoundObject = TRUE;
		for (TFieldIterator<UProperty> It(Object->GetClass()); It; ++It)
		{
			UProperty* Property = *It;
			OutPropList.AddItem( Property->GetFName().ToString() );
		}
	}

	return bFoundObject;
}

/**
 * Gets list of objects from an array property.
 */
void FRemoteControlGamePC::GetArrayObjectList(const FString& ObjectName, 
											  const FString& PropName,
											  TArray<FString>& OutArrayObjectList)
{
	UObject* Object = GetObject(ObjectName);
	UArrayProperty* pProp = Cast<UArrayProperty>(GetProperty(ObjectName, PropName));
	check(pProp);
	OutArrayObjectList.Empty();
	if (pProp->Inner->IsA(UObjectProperty::StaticClass()))
	{
		FScriptArray* ArrayProp = (FScriptArray*)(((BYTE*)Object) + pProp->Offset);
		INT elementSize = pProp->Inner->ElementSize;

		for (INT i=0; i < ArrayProp->Num(); i++)
		{
			UObject* element = *(UObject**)(((BYTE*)ArrayProp->GetData()) + i*elementSize);
			if (element)
			{
                OutArrayObjectList.AddItem(element->GetFName().ToString());
			}
		}
	}
}

/**
Is named object an actor?
*/
UBOOL FRemoteControlGamePC::IsAActor(const FString& ObjectName)
{
	UObject* Object = GetObject( ObjectName );
	AActor* Actor = Cast<AActor>(Object);
	return Actor != NULL;
}

/**
Is named object an ArrayProperty?
*/
UBOOL FRemoteControlGamePC::IsArrayProperty(const FString& OwnerName, const FString& ObjectName)
{
	UProperty* Property = GetProperty(OwnerName, ObjectName);
	return Cast<UArrayProperty>(Property) != NULL;
}

/**
Is named object an ObjectProperty?
*/
UBOOL FRemoteControlGamePC::IsObjectProperty(const FString& OwnerName, const FString& ObjectName)
{
	UProperty* Property = GetProperty(OwnerName, ObjectName);
	return Cast<UObjectProperty>(Property) != NULL;
}

/**
Get Object's class
*/
FString FRemoteControlGamePC::GetObjectClass(const FString& ObjectName)
{
	UObject* Object = GetObject(ObjectName);
	return Object ? Object->GetClass()->GetFName().ToString() : FString(TEXT("NULL"));
}
