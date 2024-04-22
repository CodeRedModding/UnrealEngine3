/**********************************************************************

Copyright   :   (c) 2006-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING 
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

/* Used by GFxEvent_FSCommand to pass fscommands to kismet events. */

class GFxFSCmdHandler_Kismet extends GFxFSCmdHandler
	native(UISequence);

native event bool FSCommand(GFxMoviePlayer movie, GFxEvent_FSCommand Event, string cmd, string Arg);
