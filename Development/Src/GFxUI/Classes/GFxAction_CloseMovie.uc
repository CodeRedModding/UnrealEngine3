/**********************************************************************

Copyright   :   (c) 2006-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING 
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/
class GFxAction_CloseMovie extends SequenceAction
	native(UISequence);

var GFxMoviePlayer              Movie;
var() bool                  bUnload;

cpptext
{
	virtual void Activated();
}

event bool IsValidLevelSequenceObject() { return true; }

DefaultProperties
{
    bUnload=TRUE

	ObjName="Close GFx Movie"
	ObjCategory="GFx UI"

    VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Movie Player",bWriteable=false)
}
