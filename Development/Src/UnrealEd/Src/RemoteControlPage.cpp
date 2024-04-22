/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "RemoteControlPage.h"
#include "RemoteControlGame.h"

WxRemoteControlPage::WxRemoteControlPage(FRemoteControlGame *InGame)
	:	Game(InGame)
{
}

WxRemoteControlPage::~WxRemoteControlPage()
{
}

/**
 * Returns the current FRemoteControlGame.
 */
FRemoteControlGame *WxRemoteControlPage::GetGame() const
{
	return Game;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Helper functions to go back and forth between wx and Unreal for boolean values.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the specified boolean choice control.
 */
void WxRemoteControlPage::SetupBooleanChoiceUI(wxChoice& ChoiceControl)
{
	ChoiceControl.Append( TEXT("True") );
	ChoiceControl.Append( TEXT("False") );
}

/**
 * Updates a boolean choice from an Unreal property.
 */
void WxRemoteControlPage::UpdateBooleanChoiceUI(wxChoice& ChoiceControl
												, const TCHAR *InClassName
												, const TCHAR *InPropertyName
												, const TCHAR *InObjectName)
{
	UBOOL bValue=FALSE;
	Game->GetObjectBoolProperty( bValue, InClassName, InPropertyName, InObjectName );

	const wxString StringValue( bValue ? TEXT("True") : TEXT("False") );
	ChoiceControl.SetStringSelection( StringValue );
}

/**
 * Sets a UBOOL property value from the UI value.
 */
void WxRemoteControlPage::SetBooleanPropertyFromChoiceUI(const wxChoice& ChoiceControl
														 , const TCHAR *InClassName
														 , const TCHAR *InPropertyName
														 , const TCHAR *InObjectName)
{
	UBOOL bValue = FALSE;
	const wxString strSelect = ChoiceControl.GetStringSelection();
	if( strSelect.CmpNoCase(TEXT("True")) == 0 )
	{
		bValue = TRUE;
	}
	Game->SetObjectBoolProperty( InClassName, InPropertyName, InObjectName, bValue );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Helper functions to go back and forth between wx and Unreal for resolution values.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @return		TRUE if the specified value is a power of 2.
 */
static UBOOL IsPowerOfTwo(INT Val)
{
	SQWORD Comparator = 1;
	while ( Comparator < SQWORD( MAXINT ) )
	{
		if ( Comparator == SQWORD( Val ) )
		{
			return TRUE;
		}
		Comparator *= 2;
	}
	return FALSE;
}

/**
 * @return		The nearest ower of to lequal to the specified value.
 */
static INT RoundDownToPowerOfTwo(INT Val)
{
	SQWORD Comparator = 2;
	while ( Comparator < SQWORD( MAXINT ) )
	{
		if ( Val < Comparator )
		{
			return Comparator / 2;
		}
		Comparator *= 2;
	}
	return 0;
}

/**
 * Initializes a control with the specified resolution.
 */
void WxRemoteControlPage::SetupResolutionChoiceUI(wxChoice& ChoiceControl, INT MinPowerOf2, INT MaxPowerOf2)
{
	if( !IsPowerOfTwo(MinPowerOf2) )
	{
		MinPowerOf2 = RoundDownToPowerOfTwo( MinPowerOf2 );
	}

	if( !IsPowerOfTwo(MaxPowerOf2) )
	{
		MaxPowerOf2 = RoundDownToPowerOfTwo( MaxPowerOf2 );
	}

	for(INT i = MaxPowerOf2; i >= MinPowerOf2; i/=2)
	{
		FString str = FString::Printf(TEXT("%d x %d"), i, i);
		ChoiceControl.Append(*str);
	}
}

/**
 * Updates a control containing a resolution from a resolution Unreal property.
 */
void WxRemoteControlPage::UpdateResolutionChoiceUI(wxChoice& ChoiceControl
												   , const TCHAR *InClassName
												   , const TCHAR *InPropertyName
												   , const TCHAR *InObjectName)
{
	INT Value = 0;

    // Special case for maximum texture size.
    Game->GetObjectIntProperty( Value, InClassName, InPropertyName, InObjectName );

	if ( !IsPowerOfTwo(Value) )
	{
		Value = RoundDownToPowerOfTwo( Value );
	}
	ChoiceControl.SetStringSelection( *FString::Printf(TEXT("%d x %d"), Value, Value ) );
}

/**
 * Sets a resolution property value from the UI value.
 */
void WxRemoteControlPage::SetResolutionPropertyFromChoiceUI(const wxChoice& ChoiceControl
															, const TCHAR *InClassName
															, const TCHAR *InPropertyName
															, const TCHAR *InObjectName)
{
	// Get the string selected on the control.
	const wxString StrSelect( ChoiceControl.GetStringSelection() );

	// Search for resolution sepearator.
	int Loc = StrSelect.Find('x');
	checkMsg(Loc != -1, "Missing 'x' in resolution string");

	// Get the value on the left side of the resolution separator.
	long Value;
	const wxString Token( StrSelect.Left( (size_t)Loc ) );
	Token.ToLong( &Value );

    Game->SetObjectIntProperty(InClassName, InPropertyName, InObjectName, (INT)Value );
}

/**
 * Helper function to load a masked bitmap.
 * @param	InBitMapName	The bitmap filename, excluding extension.
 * @param	OutBitmap		[out] Bitmap object receiving the loaded bitmap.
 * @param	InMaskColor		[opt] The color to use as a transparency mask; default is (0,128,128).
 */
void WxRemoteControlPage::LoadMaskedBitmap(const TCHAR *InBitmapName, WxBitmap& OutBitmap, const wxColor& MaskColor)
{
	OutBitmap = WxBitmap( InBitmapName );
	OutBitmap.SetMask(new wxMask(OutBitmap, MaskColor));
	/*
	WxBitmap bitmap( InBitmapName );
	bitmap.SetMask(new wxMask(bitmap, MaskColor));
	return bitmap;
	*/
}
