/*=============================================================================
	DialogueManager.cpp: Sound event handling/editing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "DialogueManager.h"
#include "EngineSequenceClasses.h"
#include "UnLinkedObjDrawUtils.h"

WxDialogueManager::WxDialogueManager( wxWindow* InParent, wxWindowID InID, USequence* InSequence ) :
	RootSequence( InSequence ), 
	WxKismet( InParent, InID, InSequence, TEXT("DialogueManager") )
{
}

WxDialogueManager::~WxDialogueManager()
{

}

USequence* WxDialogueManager::GetRootSequence()
{
	return RootSequence;
}

void WxDialogueManager::SetRootSequence( USequence* Seq )
{
	RootSequence = Seq;
}
