/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgCreateMeshProxy.h"

#if ENABLE_SIMPLYGON_MESH_PROXIES

BEGIN_EVENT_TABLE( WxDlgCreateMeshProxy, wxDialog )
	EVT_IDLE( WxDlgCreateMeshProxy::OnIdle )
	EVT_PAINT( WxDlgCreateMeshProxy::OnPaint )
	EVT_BUTTON( ID_DLG_CREATEMESHPROXY_MERGE, WxDlgCreateMeshProxy::OnMerge )
	EVT_BUTTON( ID_DLG_CREATEMESHPROXY_REMERGE, WxDlgCreateMeshProxy::OnRemerge )
	EVT_BUTTON( ID_DLG_CREATEMESHPROXY_UNMERGE, WxDlgCreateMeshProxy::OnUnmerge )
END_EVENT_TABLE()

static INT TextureSizeChoices[] =
{
	2048,
	1024,
	512,
	256,
	128,
	64
};

static const TCHAR* MaterialTypeChoiceStrings[] =
{
	TEXT("Diffuse"),							// MPMT_DiffuseOnly,
	TEXT("Diffuse And Normal")		// MPMT_DiffuseAndNormal,
};

checkAtCompileTime( SimplygonMeshUtilities::MPMT_MAX == ARRAY_COUNT(MaterialTypeChoiceStrings), MaterialTypeChoiceArraysNotSameSize );

WxDlgCreateMeshProxy::WxDlgCreateMeshProxy( wxWindow* InParent, const TCHAR* DialogTitle )
	: wxDialog( InParent, wxID_ANY, DialogTitle, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE )
	, bSetup( FALSE )
	, bDirty( FALSE )
	, MergeButton( NULL )
	, RemergeButton( NULL )
	, UnmergeButton( NULL )
	, OnScreenSizeSlider( NULL )
	, TextureSizeComboBox( NULL )
	, MaterialTypeComboBox( NULL )
	, IgnoreVertexColorsCheckBox( NULL )
	, ErrorText( NULL )
	, SelectedGroup( NULL )
	, Proxy( NULL )
{
	Reset();
}

WxDlgCreateMeshProxy::~WxDlgCreateMeshProxy()
{
	Reset();
}

/**
	* Spawns the elements which make up the dialog
	*/
void WxDlgCreateMeshProxy::Setup()
{
	if ( !bSetup )
	{
		const INT BorderSize = 5;

		// Setup controls
		wxBoxSizer* TopSizer = new wxBoxSizer( wxVERTICAL );
		SetSizer( TopSizer );

		// Load the logo and leave space for it
		SimplygonLogo.Load( TEXT("SimplygonBanner_Sml.png") );
		TopSizer->AddSpacer( BorderSize * 6 );
		{
			wxGridSizer* GridSizer = new wxFlexGridSizer( 2, 2, 0, 0 );
			{
				wxStaticText* Label = new wxStaticText(
					this,						// Parent
					-1,							// ID
					*LocalizeUnrealEd( "MeshProxy_OnScreenSizeLabel" ) );
				AllControls.AddItem( Label );

				GridSizer->Add(
					Label,							// Control window
					0,								// Sizing proportion
					wxALL | wxALIGN_CENTER_VERTICAL,// Flags
					BorderSize );					// Border spacing amount

				OnScreenSizeSlider = new wxSlider( this, -1, 200, 10, 1000, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS );
				AllControls.AddItem( OnScreenSizeSlider );
				wxSize SliderMinSize = OnScreenSizeSlider->GetMinSize();
				OnScreenSizeSlider->SetMinSize( wxSize( Max<INT>( SliderMinSize.GetWidth(), 250 ), SliderMinSize.GetHeight() ) );
				SetOnScreenSize( 200 );

				GridSizer->Add(
					OnScreenSizeSlider,							// Control window
					0,											// Sizing proportion
					wxALL | wxALIGN_CENTER_VERTICAL | wxEXPAND,	// Flags
					BorderSize );								// Border spacing amount
			}
			{
				wxStaticText* Label = new wxStaticText(
					this,						// Parent
					-1,							// ID
					*LocalizeUnrealEd( "MeshProxy_TextureSizeLabel" ) );
				AllControls.AddItem( Label );

				GridSizer->Add(
					Label,							// Control window
					0,								// Sizing proportion
					wxALL | wxALIGN_CENTER_VERTICAL,// Flags
					BorderSize );					// Border spacing amount

				TextureSizeComboBox =
					new WxComboBox(
					this,
					-1,
					TEXT(""),
					wxDefaultPosition,
					wxDefaultSize,
					0,
					NULL,
					wxCB_READONLY );
				AllControls.AddItem( TextureSizeComboBox );

				const INT TextureSizeChoiceCount = ARRAY_COUNT(TextureSizeChoices);
				for ( INT TextureChoiceIndex = 0; TextureChoiceIndex < TextureSizeChoiceCount; ++TextureChoiceIndex )
				{
					TextureSizeComboBox->Append( *FString::Printf(TEXT("%d"), TextureSizeChoices[TextureChoiceIndex] ) );
				}
				SetTextureSize( 1024 );

				GridSizer->Add(
					TextureSizeComboBox,						// Control window
					0,											// Sizing proportion
					wxALL | wxALIGN_CENTER_VERTICAL | wxEXPAND,	// Flags
					BorderSize );								// Border spacing amount
			}
			{
				wxStaticText* Label = new wxStaticText(
					this,						// Parent
					-1,							// ID
					*LocalizeUnrealEd( "MeshProxy_MaterialTypeLabel" ) );
				AllControls.AddItem( Label );

				GridSizer->Add(
					Label,							// Control window
					0,								// Sizing proportion
					wxALL | wxALIGN_CENTER_VERTICAL,// Flags
					BorderSize );					// Border spacing amount

				MaterialTypeComboBox =
					new WxComboBox(
					this,
					-1,
					TEXT(""),
					wxDefaultPosition,
					wxDefaultSize,
					0,
					NULL,
					wxCB_READONLY );
				AllControls.AddItem( MaterialTypeComboBox );

				const INT MaterialTypeChoiceCount = ARRAY_COUNT(MaterialTypeChoiceStrings);
				for ( INT TextureChoiceIndex = 0; TextureChoiceIndex < MaterialTypeChoiceCount; ++TextureChoiceIndex )
				{
					MaterialTypeComboBox->Append( MaterialTypeChoiceStrings[TextureChoiceIndex] );
				}
				SetMaterialType( SimplygonMeshUtilities::MPMT_DiffuseOnly );

				GridSizer->Add(
					MaterialTypeComboBox,						// Control window
					0,											// Sizing proportion
					wxALL | wxALIGN_CENTER_VERTICAL | wxEXPAND,	// Flags
					BorderSize );								// Border spacing amount
			}
			{
				wxStaticText* Label = new wxStaticText(
					this,						// Parent
					-1,							// ID
					*LocalizeUnrealEd( "MeshProxy_IgnoreVertexColorsLabel" ) );
				AllControls.AddItem( Label );

				GridSizer->Add(
					Label,							// Control window
					0,								// Sizing proportion
					wxALL | wxALIGN_CENTER_VERTICAL,// Flags
					BorderSize );					// Border spacing amount

				IgnoreVertexColorsCheckBox =
					new wxCheckBox(
					this,
					-1,
					TEXT("") );
				AllControls.AddItem( IgnoreVertexColorsCheckBox );

				GridSizer->Add(
					IgnoreVertexColorsCheckBox,		// Control window
					0,								// Sizing proportion
					wxALL | wxALIGN_CENTER_VERTICAL,// Flags (don't EXPAND, it's a tickbox)
					BorderSize );					// Border spacing amount
			}
			TopSizer->Add( GridSizer, 0, wxEXPAND | wxALL, BorderSize );
		}
		TopSizer->AddSpacer( BorderSize * 2 );
		{
			wxBoxSizer* ErrorSizer = new wxBoxSizer( wxHORIZONTAL );
			{
				ErrorText = new wxStaticText(
					this,						// Parent
					-1,							// ID
					*FString( "" ) );
				AllControls.AddItem( ErrorText );
				ErrorText->Hide();
				ErrorSizer->Add( ErrorText, 1, wxALIGN_CENTRE, BorderSize );
			}
			TopSizer->Add( ErrorSizer, 0, wxALIGN_CENTRE | wxALIGN_BOTTOM | wxALL, BorderSize );

			wxBoxSizer* ButtonSizer = new wxBoxSizer( wxHORIZONTAL );
			{
				MergeButton = new wxButton( this, ID_DLG_CREATEMESHPROXY_MERGE, *LocalizeUnrealEd( "MeshProxy_Merge" ) );
				AllControls.AddItem( MergeButton );
				ButtonSizer->Add( MergeButton, 1, wxALIGN_RIGHT, BorderSize );
				MergeButton->Hide();
				RemergeButton = new wxButton( this, ID_DLG_CREATEMESHPROXY_REMERGE, *LocalizeUnrealEd( "MeshProxy_Remerge" ) );
				AllControls.AddItem( RemergeButton );
				ButtonSizer->Add( RemergeButton, 1, wxALIGN_RIGHT, BorderSize );
				RemergeButton->Hide();
				UnmergeButton = new wxButton( this, ID_DLG_CREATEMESHPROXY_UNMERGE, *LocalizeUnrealEd( "MeshProxy_Unmerge" ) );
				AllControls.AddItem( UnmergeButton );
				ButtonSizer->Add( UnmergeButton, 1, wxALIGN_RIGHT, BorderSize );
				UnmergeButton->Hide();
				wxButton* CancelButton = new wxButton( this, wxID_CANCEL, *LocalizeUnrealEd( TEXT("&Cancel") ) );
				ButtonSizer->Add( CancelButton, 1, wxALIGN_RIGHT, BorderSize );
			}
			TopSizer->Add( ButtonSizer, 0, wxALIGN_RIGHT | wxALIGN_BOTTOM | wxALL, BorderSize );
		}

		ClientSize = TopSizer->ComputeFittingClientSize( this );
		SetClientSize( ClientSize );

		bSetup = TRUE;
	}
}

/**
	* Clears the result of the group search
	*
	* @param	refresh	If TRUE, refresh the browser
	*/
void WxDlgCreateMeshProxy::Reset(const UBOOL bRefresh)
{
	SelectedGroup = NULL;
	Proxy = NULL;
	ActorsToMerge.Empty();
	ActorsToUnmerge.Empty();
	ComponentsToMerge.Empty();
	ComponentsToUnmerge.Empty();

	if ( bRefresh )
	{
		GCallbackEvent->Send( CALLBACK_RefreshEditor );
	}
}

/**
	* Show or hide the dialog
	*
	* @param	bShow	If TRUE, show the dialog; if FALSE, hide it
	* @return	bool	
	*/
bool WxDlgCreateMeshProxy::Show(bool bShow)
{
	if (bShow)
	{
		// Setup the dialog and have it display the correct options
		EvaluateGroup(NULL);
	}
	return wxDialog::Show(bShow);
}

/**
	* Called when an actor has left a group (and possibly moved to another)
	*
	* @param	JoinedGroup	The group the actor joined (if any)
	* @param	LeftGroup	The group the actor left
	* @param	InActor	The actor that has moved (if not specified, it searches the group for a proxy to revert)
	* @param	bMaintainProxy Special case for when were removing the proxy from the group, but don't want to unmerge it
	* @param	bMaintainGroup Special case for when were reverting the proxy but want to keep it's meshes part of the group
	* @param bIncProxyFlag If TRUE, modify the flag which indicates this was hidden by the proxy
	* @return	bool	If the group contained a proxy
	*/
UBOOL WxDlgCreateMeshProxy::UpdateActorGroup( AGroupActor *JoinedGroup, AGroupActor *LeftGroup, AActor *InActor, const UBOOL bMaintainProxy, const UBOOL bMaintainGroup, const UBOOL bIncProxyFlag )
{
	check( LeftGroup );

	// Make sure this actor is the right type, if no actor specified, check the whole group for the proxy
	UBOOL bRet = FALSE;
	AStaticMeshActor* SMActor = Cast<AStaticMeshActor>( InActor ? InActor : LeftGroup->ContainsProxy() );
	if( SMActor && SMActor->StaticMeshComponent && SMActor->StaticMeshComponent->StaticMesh )
	{
		// Is this actor a proxy? or Did this actor construct the proxy?
		if ( SMActor->IsProxy() || SMActor->IsHiddenByProxy() )
		{
			// Get a list of all the actors we need to resolve from this
			AActor* ToSelect = LeftGroup;
			TArray<AActor*> Actors;
			LeftGroup->GetGroupActors( Actors, FALSE );
			TArray<AStaticMeshActor*> ActorsToMove;
			TArray<AStaticMeshActor*> Proxies;
			bRet = PopulateActors( Actors, &Proxies, NULL, &ActorsToMove, NULL, NULL );
			for ( INT ActorIndex = 0; ActorIndex < Proxies.Num(); ++ActorIndex )
			{
				ActorsToMove.AddUniqueItem( Proxies( ActorIndex ) );	// Add the proxy to the list too (doesn't matter if it's already removed, it needs to be in this list)
			}
			ActorsToMove.AddUniqueItem( SMActor );	// Add the parsed actor, whatever that is too

			// Remove all the associated actors from the group, and move to the other (if specified)
			for ( INT ActorIndex = 0; ActorIndex < ActorsToMove.Num(); ++ActorIndex )
			{
				AStaticMeshActor* SMActorToMove = ActorsToMove( ActorIndex );
				if ( SMActorToMove )
				{
					if ( JoinedGroup )
					{
						JoinedGroup->Add( *SMActorToMove, FALSE );
						ToSelect = JoinedGroup;
					}
					else
					{
						// Since the actor is leaving the group, we need to clean it up and the associated actors too
						if ( ( SMActorToMove->IsProxy() && !bMaintainProxy )
							|| ( SMActorToMove->IsHiddenByProxy() && bMaintainProxy ) )
						{
							// Clear any flags, remove from group and destroy the actor
							SMActorToMove->SetProxy( FALSE );
							SMActorToMove->SetHiddenByProxy( FALSE );
							LeftGroup->Remove( *SMActorToMove, FALSE );
							GWorld->EditorDestroyActor( SMActorToMove, TRUE );
						}
						else	// Must be HiddenByProxy or we're keeping the proxy
						{
							// Revert the actors flags so it becomes shown again
							SMActorToMove->SetProxy( FALSE );
							SMActorToMove->ShowProxy( TRUE, bIncProxyFlag );	// Calls SetHiddenByProxy
							if ( !bMaintainGroup ) // Special case for when were reverting the proxy but want to keep it's meshes part of the group
							{
								LeftGroup->Remove( *SMActorToMove, FALSE );
							}
						}
					}
				}
			}

			// If it was the currently selected group that was affected, refresh the dialog on the next update
			if ( SelectedGroup == LeftGroup || SelectedGroup == JoinedGroup  )
			{
				bDirty = TRUE;
			}

			// Now either select the new group or the old group (depending on which exists)
			GEditor->SelectNone( FALSE, TRUE, FALSE );
			GEditor->SelectActor( ToSelect, TRUE, NULL, FALSE );
			GCallbackEvent->Send( CALLBACK_RefreshEditor );
		}
	}
	return bRet;
}

/**
	* Show or hide the controls
	*
	* @param	bShow	If TRUE, show the controls; if FALSE, hide them
	*/
void WxDlgCreateMeshProxy::ShowControls(const bool bShow)
{
	// Show or hide all the controls in the dialog
	for( INT ControlIndex = 0; ControlIndex < AllControls.Num(); ControlIndex++ )
	{
		wxControl* pControl = AllControls( ControlIndex );
		pControl->Show( bShow );
	}
}

/**
	* Show or hide the actors in the group
	*
	* @param	bShow	If TRUE, show the actors; if FALSE, hide them
	* @param bIncProxyFlag If TRUE, modify the flag which indicates this was hidden by the proxy
	* @param	Actors	An array of actors to either show or hide
	*/
void WxDlgCreateMeshProxy::ShowActors(const UBOOL bShow, const UBOOL bIncProxyFlag, TArray<AStaticMeshActor*> &Actors) const
{
	for ( INT ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex )
	{
		AStaticMeshActor* Actor = Actors( ActorIndex );
		if ( Actor )
		{
			Actor->ShowProxy( bShow, bIncProxyFlag );
		}
	}
}

/**
	* Event handler for when a the window is idle.
	*
	* @param	In	Information about the event.
	*/
void WxDlgCreateMeshProxy::OnIdle(wxIdleEvent &In)
{
	//no need to go through the trouble when it's not visible
	if (bSetup && bDirty && IsShownOnScreen())
	{
		EvaluateGroup(NULL);

		//mark up to date
		bDirty = FALSE;
	}
}

/**
	* Event handler for when a the window is drawn.
	*
	* @param	In	Information about the event.
	*/
void WxDlgCreateMeshProxy::OnPaint(wxPaintEvent &In)
{
	wxBufferedPaintDC dc( this );
	wxRect rc = GetClientRect();

	// Clear background then draw the logo
	wxColour BaseColor = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
	dc.SetBackground( wxBrush( BaseColor, wxSOLID ) );
	dc.Clear();
	dc.DrawBitmap( SimplygonLogo, 4, 4, true);
}

/**
	* Checks the group parsed to see if it can be proxied in any way
	*
	* @param	pSelectedGroup	The group to evaluate the actors from, if NULL looks-up selected)
	* @return	UBOOL TRUE if merge possible
	*/
UBOOL WxDlgCreateMeshProxy::EvaluateGroup(AGroupActor *pSelectedGroup)
{
	// Make sure the dialog has been setup
	if ( !bSetup )
	{
		Setup();
	}

	// Get lists of all the actors to merge
	UBOOL bRet = FALSE;
	bool bCanMerge = false;
	bool bCanRemerge = false;
	bool bCanUnmerge = false;
	FString Error;
	bRet = GetActorsForProxy( pSelectedGroup, bCanMerge, bCanRemerge, bCanUnmerge, Error );

	// Update the dialog
	if ( bRet )
	{
		// Setup the button based on what the user can do
		// Merge controls are available, enable all the controls
		ShowControls( true );
		ErrorText->Show( FALSE );
		MergeButton->Show( bCanMerge );
		RemergeButton->Show( bCanRemerge );
		UnmergeButton->Show( bCanUnmerge );
		SetClientSize( ClientSize );
	}
	else
	{
		// Error occured, print out error
		ShowControls( false );
		ErrorText->SetLabel( *Error );
		ErrorText->Show( TRUE );
		wxSize NewClientSize = GetSizer()->ComputeFittingClientSize( this );
		SetClientSize( ClientSize.x, NewClientSize.y );	// Keep the window the original width
	}
	Layout();	// Refresh the window layout
	Update();	// Weird, if you add EVT_PAINT you need to call this here or it doesn't redraw
	return bRet;
}

/**
	* Loops through the parsed group pulling out the actors we're interested in
	*
	* @param	Actors	The actors to evaluate for compatibility
	* @param	OutP	The proxies actors in the group (if any, typically 0 or 1)
	* @param	OutVA	The visible actors to merge in the group (if any)
	* @param	OutHA	The hidden actors to unmerge in the group (if any)
	* @param	OutVC	The visible components to merge in the group (if any)
	* @param	OutHC	The hidden components to unmerge in the group (if any)
	* @return TRUE if a proxy exists
	*/
UBOOL WxDlgCreateMeshProxy::PopulateActors( TArray<AActor*> &Actors,
																												TArray<AStaticMeshActor*> *OutP,
																												TArray<AStaticMeshActor*> *OutVA,
																												TArray<AStaticMeshActor*> *OutHA,
																												TArray<UStaticMeshComponent*> *OutVC,
																												TArray<UStaticMeshComponent*> *OutHC ) const
{
	if ( OutP ){	OutP->Empty();	}
	if ( OutVA ){	OutVA->Empty();	}
	if ( OutHA ){ OutHA->Empty(); }
	if ( OutVC ){	OutVC->Empty();	}
	if ( OutHC ){	OutHC->Empty(); }

	// Get all the static mesh actors (and components) we wish to merge
	UBOOL bContainsProxy = FALSE;
	for ( INT ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex )
	{
		AActor* Actor = Actors(ActorIndex);
		AStaticMeshActor* SMActor = Cast<AStaticMeshActor>( Actor );
		if( SMActor && SMActor->StaticMeshComponent && SMActor->StaticMeshComponent->StaticMesh )
		{
			// Is this actor a proxy?
			if ( SMActor->IsProxy() )
			{
				bContainsProxy = TRUE;
				if ( OutP ){ OutP->AddItem( SMActor ); }
			}
			// Did this actor construct the proxy?
			else if ( SMActor->IsHiddenByProxy() )
			{
				if ( OutHA ){ OutHA->AddItem( SMActor ); }
				if ( OutHC ){	OutHC->AddItem( SMActor->StaticMeshComponent ); }
			}
			// New actor for proxy (only if not hidden)
			else if ( !SMActor->IsHiddenEd() )
			{			
				if ( OutVA ){	OutVA->AddItem( SMActor ); }
				if ( OutVC ){	OutVC->AddItem( SMActor->StaticMeshComponent ); }
			}
		}
	}
	if ( OutP && OutP->Num() > 1 )
	{
		warnf( NAME_Warning, TEXT("There should only be 1 proxy in a group (%d), if there are more it's either a forced remerge or an error!"), OutP->Num() );
	}
	return bContainsProxy;
}

/**
	* Gets the list of all the actors (and their components) for the merge from the parsed group
	*
	* @param	pSelectedGroup	The group to extract the actors from
	* @param	bCanMerge	Whether or not we can merge the meshes in the group
	* @param	bCanRemerge	Whether or not we can remerge the meshes in the group
	* @param	bCanUnmerge	Whether or not we can unmerge the meshes in the group
	* @param	Error	Localized string if an error occurs
	* @return	UBOOL return TRUE unless an error has occurred
	*/
UBOOL WxDlgCreateMeshProxy::GetActorsForProxy(AGroupActor *pSelectedGroup, bool &bCanMerge, bool &bCanRemerge, bool &bCanUnmerge, FString &Error)
{
	bCanMerge = false;
	bCanRemerge = false;
	bCanUnmerge = false;
	Error.Empty();
	Reset();
	
	// If allow group selection is disabled, disable the dialog too
	if( !GEditor->bGroupingActive )
	{
		Error = LocalizeUnrealEd( "MeshProxy_NoGroupSelection" );
		return FALSE; // Group selection disabled
	}

	// If no group is specified, attempt to grab the selected one
	SelectedGroup = pSelectedGroup;
	if ( !pSelectedGroup )
	{
		// Check how many groups are selected
		const UBOOL bDeepSearch = FALSE;	// Checks for sub groups too - do we want this?
		const INT NumActiveGroups = AGroupActor::NumActiveGroups( TRUE, bDeepSearch );
		if ( NumActiveGroups == 0 )
		{
			Error = LocalizeUnrealEd( "MeshProxy_NoGroupSelected" );
			return FALSE;	// No group selected
		}
		else if ( NumActiveGroups > 1 )
		{
			Error = LocalizeUnrealEd( "MeshProxy_MultipleGroupsSelected" );
			return FALSE;	// Too many groups selected
		}

		// Get the selected group
		SelectedGroup = AGroupActor::GetSelectedGroup( 0, bDeepSearch );
		if ( !SelectedGroup )
		{
			Error = LocalizeUnrealEd( "MeshProxy_GroupNotFound" );
			return FALSE;	// Couldn't find selected group (shouldn't happen!)
		}
	}

	// Grab all the actors that belong to this group
	TArray<AActor*> Actors;
	SelectedGroup->GetGroupActors( Actors, FALSE );

	// Make sure all the actors that are selected, belong to the group thats selected.
	// This is purely so we don't confuse the user by them thinking that all the objects that are selected in the scene 
	// are going to be merged together. We don't HAVE to do this check, it isn't essentially to the functionality 
	// (and it could be slow if there are lots of actors selected, as it's essentially a loop within a loop!)
	if ( !pSelectedGroup )	// We only need to do this if no group was parsed
	{
		for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ); It != NULL; ++It )
		{
			AActor* Actor = Cast<AActor>( *It );
			if ( Actor )
			{
				if ( !Actors.ContainsItem( Actor ) )
				{
					Reset();
					Error = LocalizeUnrealEd( "MeshProxy_MultipleActorsSelected" );
					return FALSE;	// Too many actors selected
				}
			}
		}
	}

	// Get all the static mesh actors (and components) we wish to merge
	TArray<AStaticMeshActor*> Proxies;
	PopulateActors( Actors, &Proxies, &ActorsToMerge, &ActorsToUnmerge, &ComponentsToMerge, &ComponentsToUnmerge );
	Proxy = ( Proxies.Num() > 0 ? Proxies.Last() : NULL );

	// If we have a proxy already
	if ( Proxy )
	{
		// ... and at least 1 actor to do a merge...
		if ( ActorsToMerge.Num() > 0 )
		{
			bCanMerge = true;
		}
		// ... and at least the original actors to do a remerge...
		if ( ActorsToUnmerge.Num() > 0 )
		{
			bCanRemerge = true;
		}
		// ... we can also unmerge
		bCanUnmerge = true;
	}
	// ... otherwise need 0 proxies...
	else
	{
		// ... and at least 2 actors to do a merge...
		if ( ActorsToMerge.Num() > 1 )
		{
			bCanMerge = true;
		}
		// ... and at least the original actors to do a remerge... (special case here, when moving levels we have unmerged actors flagged but no proxy)
		if ( ActorsToUnmerge.Num() > 0 )
		{
			bCanRemerge = true;
		}
		// ... otherwise we can't do anything
		if ( !bCanMerge && !bCanRemerge )
		{
			Reset();
			Error = LocalizeUnrealEd( "MeshProxy_NotEnoughStaticMeshesInGroup" );
			return FALSE;	// Not enough actors in group
		}
	}

	return TRUE;
}

/**
	* When the user has hit the Merge meshes button 
	* (available only when a merged mesh isn't already present)
	*/
void WxDlgCreateMeshProxy::OnMerge(wxCommandEvent& Event)
{
	// If we already have a proxy, remerge
	if ( Proxy )
	{
		Remerge(TRUE, TRUE);
	}
	else
	{
		if ( Merge(TRUE) )
		{
			GEditor->ResetTransaction( *LocalizeUnrealEd( "MeshProxy_Create" ) );	// Temp as it doesn't undo correctly
		}
	}
}
UBOOL WxDlgCreateMeshProxy::Merge(const UBOOL bReset)
{
	// Make sure we've got the actors ready to merge
	UBOOL bRet = FALSE;
	if ( SelectedGroup && ActorsToMerge.Num() > 1 )
	{
		ULevel* Level = ActorsToMerge.Last()->GetLevel();
		if ( Level )
		{
			ULevel* PrevCurrentLevel = GWorld->CurrentLevel;
			GWorld->CurrentLevel = Level;

			UPackage* LevelPackage = Level->GetOutermost();
			UStaticMesh* MeshProxy = NULL;
			FVector ProxyLocation;
			GWarn->BeginSlowTask( *LocalizeUnrealEd( "MeshProxy_CreatingProxy" ), TRUE );
			const SimplygonMeshUtilities::EMeshProxyMaterialType eMaterialType = GetMaterialType();
			const INT iOnScreenSize = GetOnScreenSize();
			const INT iTextureSize = GetTextureSize();
			const SimplygonMeshUtilities::EMeshProxyVertexColorMode eVertexColorMode = ShouldIgnoreVertexColors() ? SimplygonMeshUtilities::MPVCM_Ignore : SimplygonMeshUtilities::MPVCM_ModulateDiffuse;
			if ( SimplygonMeshUtilities::CreateMeshProxy( ComponentsToMerge, eMaterialType, iOnScreenSize, iTextureSize, eVertexColorMode, LevelPackage, &MeshProxy, &ProxyLocation ) )
			{
				check( MeshProxy );
				GEditor->BeginTransaction( *LocalizeUnrealEd( "MeshProxy_Create" ) );
				{
					AStaticMeshActor* MeshProxyActor = CastChecked<AStaticMeshActor>( GWorld->SpawnActor(
						AStaticMeshActor::StaticClass(),
						NAME_None,
						ProxyLocation ) );
					if ( MeshProxyActor )
					{
						FComponentReattachContext ReattachContext( MeshProxyActor->StaticMeshComponent );
						MeshProxyActor->StaticMeshComponent->StaticMesh = MeshProxy;
						MeshProxyActor->SetProxy( TRUE );
						SelectedGroup->Add( *MeshProxyActor );
						SelectedGroup->SetProxyParams( (BYTE)eMaterialType, (BYTE)eVertexColorMode, iOnScreenSize, iTextureSize );
						ShowActors( FALSE, TRUE, ActorsToMerge );
						GEditor->SelectNone( FALSE, TRUE, FALSE );
						GEditor->SelectActor( MeshProxyActor, TRUE, NULL, FALSE );
						bRet = TRUE;
					}
				}
				GEditor->EndTransaction();
			}
			GWarn->EndSlowTask();

			GWorld->CurrentLevel = PrevCurrentLevel;
		}
	}
	if ( bReset )
	{
		Reset( TRUE );
	}
	return bRet;
}

/**
	* When the user has hit the OnRemerge meshes button 
	* (available only when a merged mesh is already present, and there's non-merged meshes present)
	*/
void WxDlgCreateMeshProxy::OnRemerge(wxCommandEvent& Event)
{
	Remerge(TRUE, FALSE);
}
UBOOL WxDlgCreateMeshProxy::Remerge(const UBOOL bReset, const UBOOL bAllActors)
{
	UBOOL bRet = FALSE;
	GWarn->BeginSlowTask( *LocalizeUnrealEd( "MeshProxy_RecreatingProxy" ), TRUE );
	GEditor->BeginTransaction( *LocalizeUnrealEd( "MeshProxy_Recreate" ) );
	{
		// Make a note of the current dialog option choices
		const SimplygonMeshUtilities::EMeshProxyMaterialType eMaterialType = GetMaterialType();
		const INT iOnScreenSize = GetOnScreenSize();
		const INT iTextureSize = GetTextureSize();
		const UBOOL bIgnoreVertexColors = ShouldIgnoreVertexColors();

		// If we have preferred dialog options, from an existing proxy, use them!
		if ( SelectedGroup )
		{
			BYTE Ori_eMaterialType;
			BYTE Ori_eVertexColorMode;
			INT Ori_iOnScreenSize;
			INT Ori_iTextureSize;
			if ( SelectedGroup->GetProxyParams(Ori_eMaterialType, Ori_eVertexColorMode, Ori_iOnScreenSize, Ori_iTextureSize) )
			{
				// Ask the user if they wish to user their original settings or the ones from the dialog
				FString Ori_Settings;
				Ori_Settings += LocalizeUnrealEd("MeshProxy_OnScreenSizeLabel");
				Ori_Settings += FString::Printf(TEXT(" : %d\n"), Ori_iOnScreenSize );
				Ori_Settings += LocalizeUnrealEd("MeshProxy_TextureSizeLabel");
				Ori_Settings += FString::Printf(TEXT(" : %d\n"), Ori_iTextureSize );
				Ori_Settings += LocalizeUnrealEd("MeshProxy_MaterialTypeLabel");
				Ori_Settings += FString::Printf(TEXT(" : %s\n"), MaterialTypeChoiceStrings[ Ori_eMaterialType ] );
				Ori_Settings += LocalizeUnrealEd("MeshProxy_IgnoreVertexColorsLabel");
				Ori_Settings += FString::Printf(TEXT(" : %s\n"), (Ori_eVertexColorMode == SimplygonMeshUtilities::MPVCM_Ignore) ? (*LocalizeUnrealEd("Yes")) : (*LocalizeUnrealEd("No")));
				if (!Proxy || appMsgf(AMT_YesNo, LocalizeSecure(LocalizeUnrealEd("MeshProxy_PreserveOriginalSettings"), *Ori_Settings)))	// Don't ask if there isn't a proxy already (aka post move remerge)
				{
					SetMaterialType((SimplygonMeshUtilities::EMeshProxyMaterialType)Ori_eMaterialType);
					SetOnScreenSize(Ori_iOnScreenSize);
					SetTextureSize(Ori_iTextureSize);
					SetIgnoreVertexColors(Ori_eVertexColorMode == SimplygonMeshUtilities::MPVCM_Ignore);
				}
			}
		}

		bRet |= Unmerge(FALSE);

		// Rather than call EvaluateGroup again, we should already have all the info we need...
		if ( !bAllActors)
		{
			ActorsToMerge.Empty();
			ComponentsToMerge.Empty();
		}
		Proxy = NULL;
		ActorsToMerge += ActorsToUnmerge;
		ComponentsToMerge += ComponentsToUnmerge;
		ActorsToUnmerge.Empty();
		ComponentsToUnmerge.Empty();

		bRet |= Merge(FALSE);

		// Reset the backed up dialog options
		SetMaterialType(eMaterialType);
		SetOnScreenSize(iOnScreenSize);
		SetTextureSize(iTextureSize);
		SetIgnoreVertexColors(bIgnoreVertexColors);
	}
	GEditor->EndTransaction();
	GWarn->EndSlowTask();
	if ( bRet )
	{
		GEditor->ResetTransaction( *LocalizeUnrealEd( "MeshProxy_Recreate" ) );	// Temp as it doesn't undo correctly
	}

	if ( bReset )
	{
		Reset( TRUE );
	}
	return bRet;
}

/**
	* When the user has hit the OnUnmerge meshes button 
	* (available only when a merged mesh is already present)
	*/
void WxDlgCreateMeshProxy::OnUnmerge(wxCommandEvent& Event)
{
	if ( Unmerge(TRUE) )
	{
		GEditor->ResetTransaction( *LocalizeUnrealEd( "MeshProxy_Destroy" ) );	// Temp as it doesn't undo correctly
	}
}
UBOOL WxDlgCreateMeshProxy::Unmerge(const UBOOL bReset)
{
	// Make sure we've got the actors ready to unmerge
	UBOOL bRet = FALSE;
	if ( SelectedGroup && Proxy && ActorsToUnmerge.Num() > 1 )
	{
		GWarn->BeginSlowTask( *LocalizeUnrealEd( "MeshProxy_DestroyingProxy" ), TRUE );
		GEditor->BeginTransaction( *LocalizeUnrealEd( "MeshProxy_Destroy" ) );
		{
			ShowActors( TRUE, TRUE, ActorsToUnmerge );
			SelectedGroup->ResetProxyParams();
			SelectedGroup->Remove( *Proxy, FALSE );
			GWorld->EditorDestroyActor( Proxy, TRUE );
			Proxy = NULL;
			GEditor->SelectNone( FALSE, TRUE, FALSE );
			GEditor->SelectActor( SelectedGroup, TRUE, NULL, FALSE );
			bRet = TRUE;
		}
		GEditor->EndTransaction();
		GWarn->EndSlowTask();
	}
	if ( bReset )
	{
		Reset( TRUE );
	}
	return bRet;
}

/**
	* Retrieves/Fetches the chosen on screen size in pixels.
	*/
INT WxDlgCreateMeshProxy::GetOnScreenSize() const
{
	check( OnScreenSizeSlider );
	return (INT)OnScreenSizeSlider->GetValue();
}
void WxDlgCreateMeshProxy::SetOnScreenSize( const INT iOnScreenSize )
{
	check( OnScreenSizeSlider );
	OnScreenSizeSlider->SetValue(iOnScreenSize);
}

/**
	* Retrieves the chosen texture size.
	*/
INT WxDlgCreateMeshProxy::GetTextureSize() const
{
	check( TextureSizeComboBox );
	INT SelectionIndex = TextureSizeComboBox->GetSelection();
	check( SelectionIndex >= 0 && SelectionIndex < ARRAY_COUNT(TextureSizeChoices) );
	return TextureSizeChoices[SelectionIndex];
}
void WxDlgCreateMeshProxy::SetTextureSize( const INT iTextureSize )
{
	check( TextureSizeComboBox );
	INT SelectionIndex = 0;	// Looking the index for the size
	for ( SelectionIndex = 0; SelectionIndex < ARRAY_COUNT(TextureSizeChoices); SelectionIndex++ )
	{
		if ( TextureSizeChoices[SelectionIndex] <= iTextureSize )	// Rounddown
		{
			break;
		}
	}
	check( SelectionIndex >= 0 && SelectionIndex < ARRAY_COUNT(TextureSizeChoices) );
	TextureSizeComboBox->SetSelection(SelectionIndex);
}

/**
	* Retrieves the chosen material type.
	*/
SimplygonMeshUtilities::EMeshProxyMaterialType WxDlgCreateMeshProxy::GetMaterialType() const
{
	check( MaterialTypeComboBox );
	INT SelectionIndex = MaterialTypeComboBox->GetSelection();
	check( SelectionIndex >= 0 && SelectionIndex < SimplygonMeshUtilities::MPMT_MAX );
	return (SimplygonMeshUtilities::EMeshProxyMaterialType)SelectionIndex;
}
void WxDlgCreateMeshProxy::SetMaterialType( const SimplygonMeshUtilities::EMeshProxyMaterialType eMaterialType )
{
	check( MaterialTypeComboBox );
	check( eMaterialType >= 0 && eMaterialType < SimplygonMeshUtilities::MPMT_MAX );
	MaterialTypeComboBox->SetSelection(eMaterialType);
}

/**
 * Retrieves whether to ignore vertex colors.
 */
UBOOL WxDlgCreateMeshProxy::ShouldIgnoreVertexColors() const
{
	return IgnoreVertexColorsCheckBox->IsChecked();
}

void WxDlgCreateMeshProxy::SetIgnoreVertexColors(UBOOL bShouldIgnoreVertexColors)
{
	IgnoreVertexColorsCheckBox->SetValue(bShouldIgnoreVertexColors == TRUE);
}

#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
