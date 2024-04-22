/**********************************************************************

Copyright   :   (c) 2006-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

class GFxReimportCommandlet extends Commandlet
	native;

cpptext
{
	virtual INT Main(const FString& Params);
}

/**
 * Re-imports assets found  in specified packages, or all packages if no arguments are specified.
 *
 * @param Params the string containing the parameters for the commandlet
 */
event int Main(string Params);

