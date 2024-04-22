/**********************************************************************

Filename    :   ScaleformDataStores.cpp
Content     :   UGFxMoviePlayer class implementation for GFx

Copyright   :   Copyright 2001-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

#include "GFxUI.h"

#include "EngineSequenceClasses.h"
#include "EngineUIPrivateClasses.h"
#include "UnUIMarkupResolver.h"
#include "GFxUIUIPrivateClasses.h"
#include "ScaleformEngine.h"

#if WITH_GFx

void UGFxMoviePlayer::SetPriority ( BYTE NewPriority )
{
	Priority = NewPriority;
	if ( pMovie )
	{
		FGFxEngine* Engine = FGFxEngine::GetEngine();
		Engine->InsertMovie ( pMovie, SDPG_Foreground );
	}
}

// GameDataProvider arguments [callId, Model or View Id, ...]


#else // WITH_GFx = 0


void UGFxMoviePlayer::SetPriority ( BYTE NewPriority )
{
}

#endif // WITH_GFx
