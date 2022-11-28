/*=============================================================================
	UnEdLayer.cpp: Implementation functions for UEdLayer objects

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "EnginePrivate.h"

void UEdLayer::PostEditChange(UProperty* PropertyThatChanged)
{
	GCallback->Send( CALLBACK_RefreshEditor, ERefreshEditor_LayerBrowser );
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
