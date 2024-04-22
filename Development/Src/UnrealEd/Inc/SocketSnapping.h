/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_SOCKETSNAPPING
#define _INC_SOCKETSNAPPING

/**
 * A socket snapping panel control. 
 */
class WxSocketSnappingPanel : public wxPanel
{
public:
	/** Temp structure for sorting sockets by distance */
	struct FSocketSortRef
	{
		USkeletalMeshSocket* Socket;
		FLOAT DistToClickLine;
	};

	/**
	 * Create a new panel.
	 *
	 * @param    SkelMeshComponent - the component from which sockets are listed for snapping
	 *
	 * @param    RootPoint - the clicked point from which sockets are sorted by distance in the list
	 *
	 * @param    Parent - the parent window
	 *
	 * @param    InID - an identifier for the panel
	 *
	 * @param    Position - the panel position
	 *
	 * @param    Size - the panel size
	 */
	WxSocketSnappingPanel(USkeletalMeshComponent* SkelMeshComponent, const FVector& LineOrigin, const FVector& LineDirection, wxWindow* Parent, wxWindowID InID=wxID_ANY, wxPoint Position=wxDefaultPosition, wxSize Size=wxDefaultSize);

private:
	/** Skeletal mesh component of the target actor to which the panel is snapping actors */
	USkeletalMeshComponent* SkelMeshComp;
	
	/** The main listbox control on the panel showing the sockets available */
	wxListBox* SocketList;

	void UpdateSocketList(USkeletalMeshComponent* SkelMeshComponent, const FVector& LineOrigin, const FVector& LineDirection);
	void OnCancel(wxCommandEvent& In);
	void OnOkay(wxCommandEvent& In);
	void OnOkay_UpdateUI(wxUpdateUIEvent& In);
	void OnCaption_UpdateUI(wxUpdateUIEvent& In);
	wxString MakeCaption();

	DECLARE_EVENT_TABLE()
};

/**
 * A socket snapping dialog window frame containing a WxSocketSnappingPanel control. 
 */
class WxSocketSnappingDialog : public wxDialog
{
public:
	/**
	 * Create a new dialog.
	 *
	 * @param    SkelMeshComponent - the component from which sockets are listed for snapping
	 *
	 * @param    RootPoint - the clicked point from which sockets are sorted by distance in the list
	 *
	 * @param    Parent - the parent window
	 *
	 * @param    InID - an identifier for the panel
	 *
	 * @param    Position - the panel position
	 *
	 * @param    Size - the panel size
	 */
	WxSocketSnappingDialog(USkeletalMeshComponent* SkelMeshComponent, const FVector& LineOrigin, const FVector& LineDirection, wxWindow* Parent, wxWindowID InID=wxID_ANY, wxPoint Position=wxDefaultPosition, wxSize Size=wxDefaultSize);

private:
	/** The main panel - the only control on the dialog */
	WxSocketSnappingPanel* Panel;
};

#endif