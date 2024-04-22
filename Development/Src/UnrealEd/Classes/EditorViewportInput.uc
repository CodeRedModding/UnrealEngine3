/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class EditorViewportInput extends Input
	transient
	config(Input)
	native;

var EditorEngine	Editor;

cpptext
{
	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);
}