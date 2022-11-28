/*=============================================================================
	UnFont.cpp: Unreal font code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"


//
// Get a character's dimensions.
//
void UFont::GetCharSize( TCHAR InCh, INT& Width, INT& Height )
{
	Width = 0;
	Height = 0;
	INT Ch = (TCHARU)RemapChar(InCh);

	if( Ch < Characters.Num() )
	{
		FFontCharacter& Char = Characters(Ch);
		Width = Char.USize;
		Height = Char.VSize;
	}
}

//
//	UFont::Serialize
//

void UFont::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar << Characters << Textures << Kerning;

	if( !GLazyLoad )
    {
		for( INT t=0;t<Textures.Num();t++ )
			if( Textures(t) )
				for( INT i=0; i<Textures(t)->Mips.Num(); i++ )
					Textures(t)->Mips(i).Data.Load();
    }

	Ar << CharRemap << IsRemapped;
}
IMPLEMENT_CLASS(UFont);

/*------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------*/

