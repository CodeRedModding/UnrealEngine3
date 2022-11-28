/*=============================================================================
	EdHook.cpp: UnrealEd VB hooks.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

// Includes.
#include "UnrealEd.h"
#include "..\..\Core\Inc\UnMsg.h"

// Thread exchange.
HANDLE			hEngineThreadStarted;
HANDLE			hEngineThread;
HWND			hWndEngine;
DWORD			EngineThreadId;
const TCHAR*	GTopic;
const TCHAR*	GItem;
const TCHAR*	GValue;
TCHAR*			GCommand;

extern int GLastScroll;

// Misc.
UEngine* Engine;

/*-----------------------------------------------------------------------------
	Editor hook exec.
-----------------------------------------------------------------------------*/

void UUnrealEdEngine::NotifyDestroy( void* Src )
{
	INT idx = ActorProperties.FindItemIndex( (WxPropertyWindowFrame*)Src );
	if( idx != INDEX_NONE )
	{
		ActorProperties.Remove( idx );
	}
	if( Src==LevelProperties )
		LevelProperties = NULL;
	if( Src==UseDest )
		UseDest = NULL;
	if( Src==GApp->ObjectPropertyWindow )
		GApp->ObjectPropertyWindow = NULL;
}
void UUnrealEdEngine::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{
	Trans->Begin( TEXT("Edit Properties") );
}
void UUnrealEdEngine::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	Trans->End();

	if( ActorProperties.FindItemIndex( (WxPropertyWindowFrame*)Src ) != INDEX_NONE )
	{
		GCallback->Send( CALLBACK_ActorPropertiesChange );
	}
	GCallback->Send( CALLBACK_RedrawAllViewports );
	GEditorModeTools.GetCurrentMode()->ActorPropChangeNotify();
	RedrawLevel( Level );
}
AUTOREGISTER_TOPIC(TEXT("Obj"),ObjTopicHandler);
void ObjTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	if( ParseCommand(&Item,TEXT("QUERY")) )
	{
		UClass* Class;
		if( ParseObject<UClass>(Item,TEXT("TYPE="),Class,ANY_PACKAGE) )
		{
			UPackage* BasePackage;
			UPackage* RealPackage;
			TArray<UObject*> Results;
			if( !ParseObject<UPackage>( Item, TEXT("PACKAGE="), BasePackage, NULL ) )
			{
				// Objects in any package.
				for( FObjectIterator It; It; ++It )
					if( It->IsA(Class) )
						Results.AddItem( *It );
			}
			else if( !ParseObject<UPackage>( Item, TEXT("GROUP="), RealPackage, BasePackage ) )
			{
				// All objects beneath BasePackage.
				for( FObjectIterator It; It; ++It )
					if( It->IsA(Class) && It->IsIn(BasePackage) )
						Results.AddItem( *It );
			}
			else
			{
				// All objects within RealPackage.
				for( FObjectIterator It; It; ++It )
					if( It->IsA(Class) && It->IsIn(RealPackage) )
						Results.AddItem( *It );
			}
			for( INT i=0; i<Results.Num(); i++ )
			{
				if( i )
					Ar.Log( TEXT(" ") );
				Ar.Log( Results(i)->GetName() );
			}
		}
	}
	else if( ParseCommand(&Item,TEXT("DELETE")) )
	{
		UPackage* Pkg=ANY_PACKAGE;
		UClass*   Class;
		UObject*  Object;
		ParseObject<UPackage>( Item, TEXT("PACKAGE="), Pkg, NULL );
		if( !ParseObject<UClass>( Item,TEXT("CLASS="), Class, ANY_PACKAGE )
				|| !ParseObject(Item,TEXT("OBJECT="),Class,Object,Pkg) )
			Ar.Logf( TEXT("Object not found") );
		else
			if( UObject::IsReferenced( Object, RF_Native | RF_Public, 0 ) )
				Ar.Logf( TEXT("%s is in use"), *Object->GetFullName() );
			else
			{
				UObject* Pkg = Object;
				while( Pkg->GetOuter() )
					Pkg = Pkg->GetOuter();
				if( Pkg && Pkg->IsA(UPackage::StaticClass() ) )
					((UPackage*)Pkg)->bDirty = 1;
				delete Object;
			}
	}
	else if( ParseCommand(&Item,TEXT("GROUPS")) )
	{
		UClass* Class;
		UPackage* Pkg;
		if
		(	ParseObject<UPackage>(Item,TEXT("PACKAGE="),Pkg,NULL)
		&&	ParseObject<UClass>(Item,TEXT("CLASS="),Class,ANY_PACKAGE) )
		{
			TArray<UObject*> List;
			for( FObjectIterator It; It; ++It )
				if( It->IsA(Class) && It->GetOuter() && It->GetOuter()->GetOuter()==Pkg )
					List.AddUniqueItem( It->GetOuter() );
			for( INT i=0; i<List.Num(); i++ )
			{
				if( i )
					Ar.Log( TEXT(",") );
				Ar.Log( List(i)->GetName() );
			}
		}
	}
	else if( ParseCommand(&Item,TEXT("BROWSECLASS")) )
	{
		Ar.Log( GUnrealEd->BrowseClass->GetName() );
	}
}
void ObjTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Data )
{
	if( ParseCommand(&Item,TEXT("NOTECURRENT")) )
	{
		UClass* Class;
		UObject* Object;
		if
		(	GUnrealEd->UseDest
		&&	ParseObject<UClass>( Data, TEXT("CLASS="), Class, ANY_PACKAGE )
		&&	ParseObject( Data, TEXT("OBJECT="), Class, Object, ANY_PACKAGE ) )
		{
			TCHAR Temp[256];
			appSprintf( Temp, TEXT("%s'%s'"), Object->GetClass()->GetName(), *Object->GetPathName() );
			GUnrealEd->UseDest->SetValue( Temp );
		}
	}
}
void UUnrealEdEngine::NotifyExec( void* Src, const TCHAR* Cmd )
{
	if( ParseCommand(&Cmd,TEXT("BROWSECLASS")) )
	{
		ParseObject( Cmd, TEXT("CLASS="), BrowseClass, ANY_PACKAGE );
		UseDest = (WProperties*)Src;
		GCallback->Send( CALLBACK_Browse );
	}
	else if( ParseCommand(&Cmd,TEXT("USECURRENT")) )
	{
		ParseObject( Cmd, TEXT("CLASS="), BrowseClass, ANY_PACKAGE );
		UseDest = (WProperties*)Src;
		GCallback->Send( CALLBACK_UseCurrent );
	}
	else if( ParseCommand(&Cmd,TEXT("NEWOBJECT")) )
	{
		FTreeItem* Dest = (FTreeItem*)Src;
		UClass* Cls, *ParentCls;
		ParseObject( Cmd, TEXT("CLASS="), Cls, ANY_PACKAGE );
		ParseObject( Cmd, TEXT("PARENTCLASS="), ParentCls, ANY_PACKAGE );
		UObject* Parent = NULL;
		ParseObject( Cmd, TEXT("PARENT="), ParentCls, Parent, NULL );
		if( Cls && Parent )
		{
			UObject* NewObject = NULL;
			if( Cls->IsChildOf(AActor::StaticClass()) )
			{
				if( Parent->IsA(AActor::StaticClass()) )
					NewObject = ((AActor*)Parent)->XLevel->SpawnActor( Cls );
				else
					debugf( TEXT("Warning: Can't instance actor of class %s, parent is %s"), Cls->GetName(), *Parent->GetFullName() );
			}			
			else
			{
				UObject* UseOuter = (Cls->IsChildOf(UComponent::StaticClass()) || Parent->IsA(UClass::StaticClass()) ? Parent : Parent->GetOuter());
				NewObject = StaticConstructObject( Cls, UseOuter, NAME_None, RF_Public|((Cls->ClassFlags&CLASS_Localized) ? RF_PerObjectLocalized : 0) );
			}
			if( NewObject )
				Dest->SetValue( *NewObject->GetPathName() );
		}
	}
}
void UUnrealEdEngine::UpdatePropertyWindows()
{
	TArray<UObject*> SelectedActors;
	for( INT i=0; i<Level->Actors.Num(); i++ )
		if( Level->Actors(i) && GSelectionTools.IsSelected( Level->Actors(i) ) )
			SelectedActors.AddItem( Level->Actors(i) );

	// Update all unlocked windows

	for( INT x = 0 ; x < ActorProperties.Num() ; ++x )
	{
		if( !ActorProperties(x)->IsLocked() )
		{
			ActorProperties(x)->PropertyWindow->SetObjectArray( &SelectedActors, 0,1,1 );
		}
	}
	
	// Validate the contents of any locked windows.  Any object that is being viewed inside
	// of a locked property window but is no longer valid needs to be removed.

	for( INT x = 0 ; x < ActorProperties.Num() ; ++x )
	{
		if( ActorProperties(x)->IsLocked() )
		{
			for( INT o = 0 ; o < ActorProperties(x)->PropertyWindow->GetRoot()->Objects.Num() ; ++o )
			{
				AActor* Actor = Cast<AActor>(ActorProperties(x)->PropertyWindow->GetRoot()->Objects(o));

				if( Level->Actors.FindItemIndex(Actor) == INDEX_NONE )
				{
					ActorProperties(x)->PropertyWindow->GetRoot()->Objects.RemoveItem( Actor );
					ActorProperties(x)->PropertyWindow->GetRoot()->Finalize();
					o = -1;
				}
			}
		}
	}

	// Tell all property windows to refresh themselves.

	for( INT i=0; i<WProperties::PropertiesWindows.Num(); i++ )
	{
		WProperties* Properties=WProperties::PropertiesWindows(i);
		Properties->ForceRefresh();
	}
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
