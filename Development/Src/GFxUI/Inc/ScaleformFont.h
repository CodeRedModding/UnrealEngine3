/**********************************************************************

Filename    :   ScaleformFont.h
Content     :

Copyright   :   (c) 2001-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

#ifndef ScaleformFont_h
#define ScaleformFont_h

#if WITH_GFx

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "GFx/GFX_Loader.h"
#include "Render/Render_Font.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

using namespace Scaleform;

class FGFxUFontProvider : public GFx::FontProvider
{
	public:
		FGFxUFontProvider();
		virtual ~FGFxUFontProvider();

		virtual Render::Font*   CreateFont ( const char* name, unsigned fontFlags );
		virtual void            LoadFontNames ( StringHash<String>& fontnames );
};

#endif
#endif
