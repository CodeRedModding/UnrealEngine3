/*=============================================================================
StartupTipDialog.cpp: Startup Tip dialog window
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "FConfigCacheIni.h"
#include "StartupTipDialog.h"

/*-----------------------------------------------------------------------------
WxLocalizedTipProvider
-----------------------------------------------------------------------------*/
WxLocalizedTipProvider::WxLocalizedTipProvider() : 
TipSection(NULL)
{
	// We allow various .inis to contribute multiple paths for localization files.
	UBOOL bFoundFile =  FALSE;
	for (INT LangIndex = 0; LangIndex < 2 && !bFoundFile; LangIndex++)
	{
		// first pass use use language, second pass use english (only does second pass if first wasn't found)
		const TCHAR* LangExt = LangIndex ? TEXT("int") : UObject::GetLanguage();

		for( INT PathIndex=0; PathIndex < GSys->LocalizationPaths.Num(); PathIndex++ )
		{
			FString LocPath = FString::Printf(TEXT("%s") PATH_SEPARATOR TEXT("%s"), *GSys->LocalizationPaths(PathIndex), LangExt);
			FFilename SearchPath = FString::Printf(TEXT("%s") PATH_SEPARATOR TEXT("EditorTips.%s"), *LocPath, LangExt);

			TArray<FString> LocFileNames;
			GFileManager->FindFiles(LocFileNames, *SearchPath, TRUE, FALSE);

			if(LocFileNames.Num())
			{
				TipFilename = LocPath * LocFileNames(0);
				GConfig->LoadFile(*TipFilename);
				bFoundFile = TRUE;
				break;
			}
		}
	}
}

wxString WxLocalizedTipProvider::GetTip(INT &TipNumber)
{
	wxString Tip;

	if(TipFilename != TEXT(""))
	{
		FConfigFile* TipFile = GConfig->FindConfigFile( *TipFilename);
		if(TipFile != NULL)
		{
			TipSection = TipFile->Find(TEXT("Tips"));
		}
	}


	if(TipSection != NULL)
	{
		INT NumTips = TipSection->Num();
		TipNumber = TipNumber % NumTips;

		FConfigSection::TIterator TipIterator(*TipSection);

		for(INT Idx=0;Idx<TipNumber;Idx++)
		{
			++TipIterator;
		}

		Tip = *Localize(TEXT("Tips"),*TipIterator.Key().ToString(), TEXT("EditorTips"));

		TipNumber++;
	}

	return Tip;
}


/*-----------------------------------------------------------------------------
WxStartupTipDialog
-----------------------------------------------------------------------------*/
BEGIN_EVENT_TABLE(WxStartupTipDialog, wxDialog)
	EVT_BUTTON(IDB_NEXT_TIP, WxStartupTipDialog::OnNextTip)
	EVT_TEXT_URL(wxID_ANY, WxStartupTipDialog::LaunchURL)
END_EVENT_TABLE()

WxStartupTipDialog::WxStartupTipDialog(wxWindow* Parent) : 
wxDialog(Parent, wxID_ANY, *LocalizeUnrealEd("StartupTipDialog_Title"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
CurrentTip(0),
bIsMouseDownOverURL(FALSE)
{
	SetMinSize(wxSize(400,300));

	wxSizer* MainSizer = new wxBoxSizer(wxVERTICAL);
	{
		wxSizer* HSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			// Bitmap Panel
			wxPanel* BitmapPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER);
			{
				wxSizer* BitmapSizer = new wxBoxSizer(wxHORIZONTAL);
				{
					wxStaticBitmap *Bitmap = new wxStaticBitmap(BitmapPanel, wxID_ANY, WxBitmap(TEXT("TipOfTheDay")));
					BitmapSizer->Add(Bitmap, 0, wxALIGN_BOTTOM);
				}
				BitmapPanel->SetSizer(BitmapSizer);
				BitmapPanel->SetBackgroundColour(wxColour(0,0,0));
			}
			HSizer->Add(BitmapPanel, 0, wxEXPAND | wxALL, 5);

			// Textboxes
			wxSizer* TextSizer = new wxBoxSizer(wxVERTICAL);
			{
				// Static Label
				wxStaticText *Label = new wxStaticText(this, wxID_ANY, *LocalizeUnrealEd("StartupTipDialog_StartupTip"));
				wxFont Font = Label->GetFont();
				Font.SetPointSize(16);
				Font.SetWeight(wxFONTWEIGHT_BOLD);
				Label->SetFont(Font);
				TextSizer->Add(Label, 0, wxEXPAND | wxBOTTOM, 5);

				// Tip Text
				TipText = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_AUTO_URL);
				TipText->SetEditable(false);

				TextSizer->Add(TipText, 1, wxEXPAND | wxTOP, 5);
			}
			HSizer->Add(TextSizer, 1, wxEXPAND | wxALL, 5);
		}
		MainSizer->Add(HSizer, 1, wxEXPAND | wxALL, 5);

		wxSizer* ButtonSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			CheckStartupShow = new wxCheckBox(this, wxID_ANY, *LocalizeUnrealEd("StartupTipDialog_ShowTipsAtStartup"));
			ButtonSizer->Add(CheckStartupShow, 1, wxALIGN_LEFT | wxEXPAND);

			NextTip = new wxButton(this, IDB_NEXT_TIP, *LocalizeUnrealEd("StartupTipDialog_NextTip"));
			ButtonSizer->Add(NextTip, 0, wxEXPAND | wxLEFT, 5);

			wxButton* OKButton = new wxButton(this, wxID_OK,*LocalizeUnrealEd("OK"));
			ButtonSizer->Add(OKButton, 0, wxEXPAND | wxLEFT, 5);
		}
		MainSizer->Add(ButtonSizer, 0, wxEXPAND | wxALL, 5);
	}
	SetSizer(MainSizer);

	// Load options for the dialog.
	LoadOptions();

	// Display a tip.
	UpdateTip();
}

WxStartupTipDialog::~WxStartupTipDialog()
{
	SaveOptions();
}

/** Saves options for the tip box. */
void WxStartupTipDialog::SaveOptions() const
{
	// Show at startup.
	GConfig->SetBool(TEXT("StartupTipDialog"), TEXT("ShowAtStartup"), CheckStartupShow->GetValue(), GEditorUserSettingsIni);

	// Current Tip
	GConfig->SetInt(TEXT("StartupTipDialog"), TEXT("CurrentTip"), CurrentTip, GEditorUserSettingsIni);
}

/** Loads options for the tip box. */
void WxStartupTipDialog::LoadOptions()
{
	// Always display the startup tip dialog in the center of the screen
	Centre();

	// Show at startup.
	UBOOL bShowAtStartup = TRUE;
	if( !GConfig->GetBool(TEXT("StartupTipDialog"), TEXT("ShowAtStartup"), bShowAtStartup, GEditorUserSettingsIni) )
	{
		// Backwards compatibility with old preference name
		GConfig->GetBool(TEXT("DlgTipOfTheDay"), TEXT("ShowAtStartup"), bShowAtStartup, GEditorUserSettingsIni);
	}
	CheckStartupShow->SetValue(bShowAtStartup == TRUE);

	// Current Tip
	UBOOL bLoadedTipCount = GConfig->GetInt(TEXT("StartupTipDialog"), TEXT("CurrentTip"), CurrentTip, GEditorUserSettingsIni);
	if( !bLoadedTipCount )
	{
		// Backwards compatibility with old preference name
		bLoadedTipCount = GConfig->GetInt(TEXT("DlgTipOfTheDay"), TEXT("CurrentTip"), CurrentTip, GEditorUserSettingsIni);
	}

	if(bLoadedTipCount == FALSE)
	{
		CurrentTip = 0;
	}
}

/** Updates the currently displayed tip with a new randomized tip. */
void WxStartupTipDialog::UpdateTip()
{
	TipText->SetValue(TipProvider.GetTip(CurrentTip));
}

/** Updates the tip text with a new tip. */
void WxStartupTipDialog::OnNextTip(wxCommandEvent &Event)
{
	UpdateTip();
}

/** Launch Browser with URL specified*/
void WxStartupTipDialog::LaunchURL(wxTextUrlEvent&Event)
{
	if (!Event.GetMouseEvent().m_leftDown)
	{
		bIsMouseDownOverURL = FALSE;
	}
	else
	{
		//left is down, but was it last time
		if (!bIsMouseDownOverURL)
		{
			//get URL
			INT StartIndex = Event.GetURLStart();
			INT EndIndex = Event.GetURLEnd();
			FString URLString = TipText->GetRange(StartIndex, EndIndex).c_str();

			appLaunchURL(*URLString);
			bIsMouseDownOverURL = TRUE;
		}
	}
}




