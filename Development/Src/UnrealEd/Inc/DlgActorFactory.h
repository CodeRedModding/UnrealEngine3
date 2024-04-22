/*=============================================================================
	DlgActorFactory.h: UnrealEd General Purpose Actor Factory Dialog
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGACTORFACTORY_H__
#define __DLGACTORFACTORY_H__

/**
  * UnrealEd General Purpose Actor Factory Dialog.
  */
class WxDlgActorFactory : public wxDialog
{
public:
	WxDlgActorFactory();
	~WxDlgActorFactory();

	/** 
	  * Displays the dialog using the passed in factory's properties.
	  *
	  * @param InFactory  Actor factory to use for displaying properties.
	  */
	void ShowDialog(UActorFactory* InFactory, UBOOL bInReplace = FALSE);

private:
	UActorFactory* Factory;
	WxPropertyWindowHost* PropertyWindow;
	wxStaticText *NameText;
	/** whether this option is a "Replace Actor" (TRUE) or "Add Actor" (FALSE) */
	UBOOL bIsReplaceOp;

	void OnOK( wxCommandEvent& In );
	void OnClose( wxCloseEvent& CloseEvent );

	DECLARE_EVENT_TABLE()
};



#endif

