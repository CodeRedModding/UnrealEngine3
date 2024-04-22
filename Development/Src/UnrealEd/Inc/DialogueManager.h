/*=============================================================================
	DialogueManager.h: Dialogue event handling/editing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DIALOGUEMANAGER_H__
#define __DIALOGUEMANAGER_H__

#include "Kismet.h"
#include "UnrealEd.h"

class WxDialogueManager: public WxKismet // WxLinkedObjEd
{
public:
	// Sequence stored passed in (Stored in package) when editor is created.
	class USequence* RootSequence;

	WxDialogueManager( wxWindow* InParent, wxWindowID InID, class USequence* InSequence );
	~WxDialogueManager();

	virtual USequence*	GetRootSequence();
	virtual void		SetRootSequence( USequence* Seq );
};

#endif
