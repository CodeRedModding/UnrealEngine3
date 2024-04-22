/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

#include "UnrealEd.h"
#include "SocketSnapping.h"
#include "ScopedTransaction.h"

BEGIN_EVENT_TABLE( WxSocketSnappingPanel, wxPanel )
	EVT_BUTTON(IDC_SocketSnappingWindow_CancelButton, WxSocketSnappingPanel::OnCancel)
	EVT_BUTTON(IDC_SocketSnappingWindow_OKButton, WxSocketSnappingPanel::OnOkay)
	EVT_UPDATE_UI( IDC_SocketSnappingWindow_OKButton, WxSocketSnappingPanel::OnOkay_UpdateUI)
	EVT_UPDATE_UI( IDC_SocketSnappingWindow_CaptionText, WxSocketSnappingPanel::OnCaption_UpdateUI)
	EVT_LISTBOX_DCLICK(IDC_SocketSnappingWindow_ListCtrl, WxSocketSnappingPanel::OnOkay)
END_EVENT_TABLE()

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
WxSocketSnappingPanel::WxSocketSnappingPanel(USkeletalMeshComponent* SkelMeshComponent, const FVector& LineOrigin, const FVector& LineDirection, wxWindow* Parent, wxWindowID InID, wxPoint Position, wxSize Size)
	:
	wxPanel(Parent, InID, Position, Size),
	SkelMeshComp(SkelMeshComponent)
{
	wxStaticText* CaptionText;

	wxSizer* MainSizer = new wxBoxSizer(wxVERTICAL);
	{
		CaptionText = new wxStaticText(this, IDC_SocketSnappingWindow_CaptionText, MakeCaption(), wxDefaultPosition, wxDefaultSize, 0);//wxALIGN_CENTRE);
		//CaptionText->SetBackgroundColour(wxColour(255, 255, 200));
		CaptionText->SetFont(wxFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
		MainSizer->Add(CaptionText, 0, wxEXPAND | wxALL, 8);

		wxStaticBox* SocketsGroupBox = new wxStaticBox(this, wxID_ANY, *LocalizeUnrealEd("SocketSnapping_SocketsBox"));
		wxStaticBoxSizer* SocketsGroupBoxSizer = new wxStaticBoxSizer(SocketsGroupBox, wxVERTICAL);
		{
			SocketList = new wxListBox(this, IDC_SocketSnappingWindow_ListCtrl, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_SINGLE | wxLB_HSCROLL | wxLB_NEEDED_SB);
			SocketsGroupBoxSizer->Add(SocketList, 1, wxEXPAND | wxALL, 3);
		}
		MainSizer->Add(SocketsGroupBoxSizer, 1, wxEXPAND | wxALL, 3);

		wxSizer* ButtonSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			wxButton* CancelBtn = new wxButton(this, IDC_SocketSnappingWindow_CancelButton, *LocalizeUnrealEd("Cancel"));
			ButtonSizer->Add(CancelBtn, 0, wxRIGHT, 3);

			wxButton* OKBtn = new wxButton(this, IDC_SocketSnappingWindow_OKButton, *LocalizeUnrealEd("OK"));
			OKBtn->Disable();
			ButtonSizer->Add(OKBtn, 0, wxRIGHT, 3);
		}
		MainSizer->Add(ButtonSizer, 0, wxALIGN_RIGHT | wxALL, 3);
	}
	SetSizer(MainSizer);

	CaptionText->Wrap(320);	// NOTE: this is a poor way to set this width but a better way, linked to the actual width of the parent dialog
							// and the actual width of the text box a runtime would be much more complex to write and have the same result
							// given that the parent dialog cannot be resized.

	UpdateSocketList(SkelMeshComponent, LineOrigin, LineDirection);
}

/**
 * Compiles the selection order array by putting every geometry object
 * with a valid selection index into the array, and then sorting it.
 */

static int CDECL SocketPositionCompare(const void *InA, const void *InB)
{
	WxSocketSnappingPanel::FSocketSortRef* SocketRefA = (WxSocketSnappingPanel::FSocketSortRef*)InA;
	WxSocketSnappingPanel::FSocketSortRef* SocketRefB = (WxSocketSnappingPanel::FSocketSortRef*)InB;

	if (0 < SocketRefA->DistToClickLine - SocketRefB->DistToClickLine)
	{
		return 1;
	}
	else if (0 > SocketRefA->DistToClickLine - SocketRefB->DistToClickLine)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

void WxSocketSnappingPanel::UpdateSocketList(USkeletalMeshComponent* SkelMeshComponent, const FVector& LineOrigin, const FVector& LineDirection)
{
	SkelMeshComp = SkelMeshComponent;

	SocketList->Clear();

	check(SkelMeshComp);
	check(SkelMeshComp->SkeletalMesh);

	TArray<FSocketSortRef> Sockets;
	for (TArray<USkeletalMeshSocket*>::TConstIterator SocketIt(SkelMeshComp->SkeletalMesh->Sockets); SocketIt; ++SocketIt)
	{
		USkeletalMeshSocket* Socket = *SocketIt;

		FMatrix SocketTM = FMatrix::Identity;
		Socket->GetSocketMatrix(SocketTM, SkelMeshComp);
		FVector SocketPos = SocketTM.GetOrigin();

		FLOAT LineOriginToSocket = (SocketPos - LineOrigin).Size();
		FVector LineEnd = LineOrigin + LineDirection * LineOriginToSocket;

		FVector ClosestPointOnLineToSocket = ClosestPointOnLine(LineOrigin, LineEnd, SocketPos);
		FLOAT LineToSocket = (SocketPos - ClosestPointOnLineToSocket).Size();

		FSocketSortRef Ref;
		Ref.Socket = *SocketIt;
		Ref.DistToClickLine = LineToSocket;
		Sockets.AddItem(Ref);
	}

	appQsort( &Sockets(0), Sockets.Num(), sizeof(FSocketSortRef), (QSORT_COMPARE)SocketPositionCompare );

	for (TArray<FSocketSortRef>::TConstIterator SocketIt(Sockets); SocketIt; ++SocketIt)
	{
		SocketList->Append(*((*SocketIt).Socket->SocketName.ToString()));
	}

	if (0 < SocketList->GetCount())
	{
		SocketList->SetSelection(0);
	}
}

void WxSocketSnappingPanel::OnCancel(wxCommandEvent& In)
{
	GetParent()->Close();
}

void WxSocketSnappingPanel::OnOkay(wxCommandEvent& In)
{
	FString SelectedSocketName = (const TCHAR*)SocketList->GetStringSelection();
	USkeletalMeshSocket* SelectedSocket = NULL;
	for (TArray<USkeletalMeshSocket*>::TConstIterator SocketIt(SkelMeshComp->SkeletalMesh->Sockets); SocketIt; ++SocketIt)
	{
		if ((*SocketIt)->SocketName.ToString() == SelectedSocketName)
		{
			SelectedSocket = *SocketIt;
			break;
		}
	}

	if (SelectedSocket)
	{
		FMatrix SocketTM;
		if( SelectedSocket->GetSocketMatrix(SocketTM, SkelMeshComp))
		{
			const FScopedTransaction Transaction(*LocalizeUnrealEd("SocketSnapping_SnapTransactionName"));
			// Assign to selected actors the socket that was selected.
			for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				// Don't allow an actor to snap to his own socket!
				AActor* TargetActor = SkelMeshComp->GetOwner();
				if (Actor != TargetActor)
				{
					Actor->Modify();

					Actor->Location = SocketTM.GetOrigin();
					Actor->Rotation = SocketTM.Rotator();

					Actor->Base = TargetActor;
					Actor->BaseSkelComponent = SkelMeshComp;
					Actor->BaseBoneName = SelectedSocket->BoneName;

					Actor->PreEditChange( NULL );
					Actor->PostEditChange();

					GEditor->SetPivot( Actor->Location, FALSE, TRUE );
				}
			}
		}
	}

	GetParent()->Close();
}

void WxSocketSnappingPanel::OnOkay_UpdateUI(wxUpdateUIEvent& In)
{
	wxButton* OKButton = (wxButton*)In.GetEventObject();
	if (OKButton)
	{
		OKButton->Enable(wxNOT_FOUND != SocketList->GetSelection());
	}
}

void WxSocketSnappingPanel::OnCaption_UpdateUI(wxUpdateUIEvent& In)
{
	wxStaticText* TextBox = (wxStaticText*)In.GetEventObject();
	if (TextBox)
	{
		//TextBox->SetLabel(MakeCaption());
		//TextBox->SetToolTip(TextBox->GetLabel());
		//wxSize TextSize = CaptionText->GetSize();
		//CaptionText->SetSize(MainSizer->GetSize().GetWidth() - 4, TextSize.GetHeight());
		//TextBox->Wrap(GetSize().GetWidth() - 10);
	}
}

wxString WxSocketSnappingPanel::MakeCaption()
{
	FString SourceActor;
	if (1 == GEditor->GetSelectedActorCount())
	{
		UObject* SelObj = GEditor->GetSelectedActors()->GetTop<UObject>();
		SourceActor = *SelObj->GetName();
	}
	else
	{
		SourceActor = FString(TEXT("[multiple actors]"));
	}

	FString TargetMesh = SkelMeshComp->SkeletalMesh->GetName();
	FString TargetActor = *SkelMeshComp->GetOwner()->GetName();

	return wxString(*FString::Printf(TEXT("Snapping %s to target %s. The target's SkeletalMesh is %s. Select a socket on the target mesh from the list below and click OK to snap/attach."), *SourceActor, *TargetActor, *TargetMesh));
}

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
WxSocketSnappingDialog::WxSocketSnappingDialog(USkeletalMeshComponent* SkelMeshComponent, const FVector& LineOrigin, const FVector& LineDirection, wxWindow* parent, wxWindowID InID, wxPoint Position, wxSize Size)
	:
wxDialog(parent, InID, *LocalizeUnrealEd("SocketSnapping_DialogCaption"), Position, Size, wxDEFAULT_DIALOG_STYLE | wxSYSTEM_MENU | wxCAPTION)
{
	wxSizer* MainSizer = new wxBoxSizer(wxHORIZONTAL);
	{
		Panel = new WxSocketSnappingPanel(SkelMeshComponent, LineOrigin, LineDirection, this);
		MainSizer->Add(Panel, 1, wxEXPAND);
	}
	SetSizer(MainSizer);
}