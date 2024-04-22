/**********************************************************************

Filename    :   GFxRawData.uc
Content     :   Unreal Scaleform GFx integration

Copyright   :   (c) 2006-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :   Since 'ucc' will prefix all class names with 'U'
                there is not conflict with GFx file / class naming.

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING 
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/


class GFxRawData extends Object
	native
	hidecategories(Object)
	editinlinenew;

var const array<byte>              RawData;
/** A list of weak references to Swfs needed by this SwfMovie */
var() editconst array<String> ReferencedSwfs;
var() editconst array<Object> References;
var() array<Object> UserReferences;

cpptext
{
	// Accessors
	void SetRawData(const BYTE *data, UINT size);
}

defaultproperties
{
}
