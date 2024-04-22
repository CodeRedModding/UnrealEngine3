/**********************************************************************

Filename    :   SwfMovie.uc
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


class SwfMovie extends GFxRawData
	native
	AutoExpandCategories(Import);


enum FlashTextureRescale
{
	FlashTextureScale_High,
	FlashTextureScale_Low,
	FlashTextureScale_NextLow,
	FlashTextureScale_Mult4,
	FlashTextureScale_None
};

cpptext
{
	/** Set sRGB = OFF on all referenced Texture2Ds */
	virtual void PostLoad( void );
}

var() bool               bUsesFontlib;

var(Import) editoronly string  SourceFile;

var(Import) editconst bool bSetSRGBOnImportedTextures <Tooltip=Mark textures as sRGB when importing.>;
var(Import) bool bPackTextures;
var(Import) int PackTextureSize <editcondition=bPackTextures | ClampMin=256 | Multiple=32>;
var(Import) bool bForceSquarePacking <editcondition=bPackTextures>;
var(Import) FlashTextureRescale TextureRescale;
var(Import) editconst string TextureFormat;
/** Date/Time-stamp of the file from the last import */
var(Import) editconst editoronly string SourceFileTimestamp;

// @todo: Expose these as user-modifyable properties?
var int RTTextures;
var int RTVideoTextures;

/** Time stamp set upon import (or re-import) of this Swf movie.  Used to force GFx to ignore already-loaded content and use the re-imported data within a single editor session. */
var editoronly const transient qword ImportTimeStamp;



defaultproperties
{
	RTTextures=24
	RTVideoTextures=2
	bPackTextures=false
	bSetSRGBOnImportedTextures=false
	bForceSquarePacking=false
}
