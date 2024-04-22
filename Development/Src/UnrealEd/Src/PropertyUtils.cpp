/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "PropertyUtils.h"
#include "ObjectsPropertyNode.h"
#include "PropertyWindow.h"
#include "PropertyWindowManager.h"
#include "DlgPropertyTextEditBox.h"
#include "ItemPropertyNode.h"
#include "ContentBrowserHost.h"
#include "NewMaterialEditor.h"
#if WITH_MANAGED_CODE
	#include "ContentBrowserShared.h"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Equation evaluation.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Converts a string to it's numeric equivalent, ignoring whitespace.
 * "123  45" - becomes 12,345
 *
 * @param	Value	The string to convert.
 * @return			The converted value.
 */
FLOAT Val(const FString& Value)
{
	FLOAT RetValue = 0;

	for( INT x = 0 ; x < Value.Len() ; x++ )
	{
		FString Char = Value.Mid(x, 1);

		if( Char >= TEXT("0") && Char <= TEXT("9") )
		{
			RetValue *= 10;
			RetValue += appAtoi( *Char );
		}
		else 
		{
			if( Char != TEXT(" ") )
			{
				break;
			}
		}
	}

	return RetValue;
}

FString GrabChar( FString* pStr )
{
	FString GrabChar;
	if( pStr->Len() )
	{
		do
		{		
			GrabChar = pStr->Left(1);
			*pStr = pStr->Mid(1);
		} while( GrabChar == TEXT(" ") );
	}
	else
	{
		GrabChar = TEXT("");
	}

	return GrabChar;
}

UBOOL SubEval( FString* pStr, FLOAT* pResult, INT Prec )
{
	FString c;
	FLOAT V, W, N;

	V = W = N = 0.0f;

	c = GrabChar(pStr);

	if( (c >= TEXT("0") && c <= TEXT("9")) || c == TEXT(".") )	// Number
	{
		V = 0;
		while(c >= TEXT("0") && c <= TEXT("9"))
		{
			V = V * 10 + Val(c);
			c = GrabChar(pStr);
		}

		if( c == TEXT(".") )
		{
			N = 0.1f;
			c = GrabChar(pStr);

			while(c >= TEXT("0") && c <= TEXT("9"))
			{
				V = V + N * Val(c);
				N = N / 10.0f;
				c = GrabChar(pStr);
			}
		}
	}
	else if( c == TEXT("("))									// Opening parenthesis
	{
		if( !SubEval(pStr, &V, 0) )
		{
			return 0;
		}
		c = GrabChar(pStr);
	}
	else if( c == TEXT("-") )									// Negation
	{
		if( !SubEval(pStr, &V, 1000) )
		{
			return 0;
		}
		V = -V;
		c = GrabChar(pStr);
	}
	else if( c == TEXT("+"))									// Positive
	{
		if( !SubEval(pStr, &V, 1000) )
		{
			return 0;
		}
		c = GrabChar(pStr);
	}
	else if( c == TEXT("@") )									// Square root
	{
		if( !SubEval(pStr, &V, 1000) )
		{
			return 0;
		}

		if( V < 0 )
		{
			debugf(TEXT("Expression Error : Can't take square root of negative number"));
			return 0;
		}
		else
		{
			V = appSqrt(V);
		}

		c = GrabChar(pStr);
	}
	else														// Error
	{
		debugf(TEXT("Expression Error : No value recognized"));
		return 0;
	}
PrecLoop:
	if( c == TEXT("") )
	{
		*pResult = V;
		return 1;
	}
	else if( c == TEXT(")") )
	{
		*pStr = FString(TEXT(")")) + *pStr;
		*pResult = V;
		return 1;
	}
	else if( c == TEXT("+") )
	{
		if( Prec > 1 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
		{
			if( SubEval(pStr, &W, 2) )
			{
				V = V + W;
				c = GrabChar(pStr);
				goto PrecLoop;
			}
		}
	}
	else if( c == TEXT("-") )
	{
		if( Prec > 1 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
		{
			if( SubEval(pStr, &W, 2) )
			{
				V = V - W;
				c = GrabChar(pStr);
				goto PrecLoop;
			}
		}
	}
	else if( c == TEXT("/") )
	{
		if( Prec > 2 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
		{
			if( SubEval(pStr, &W, 3) )
			{
				if( W == 0 )
				{
					debugf(TEXT("Expression Error : Division by zero isn't allowed"));
					return 0;
				}
				else
				{
					V = V / W;
					c = GrabChar(pStr);
					goto PrecLoop;
				}
			}
		}
	}
	else if( c == TEXT("%") )
	{
		if( Prec > 2 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
		{
			if( SubEval(pStr, &W, 3) )
			{
				if( W == 0 )
				{
					debugf(TEXT("Expression Error : Modulo zero isn't allowed"));
					return 0;
				}
				else
				{
					V = (INT)V % (INT)W;
					c = GrabChar(pStr);
					goto PrecLoop;
				}
			}
		}
	}
	else if( c == TEXT("*") )
	{
		if( Prec > 3 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
		{
			if( SubEval(pStr, &W, 4) )
			{
				V = V * W;
				c = GrabChar(pStr);
				goto PrecLoop;
			}
		}
	}
	else
	{
		debugf(TEXT("Expression Error : Unrecognized Operator"));
	}

	*pResult = V;
	return 1;
}

/**
 * Evaluates a numerical equation.
 *
 * Operators and precedence: 1:+- 2:/% 3:* 4:^ 5:&|
 * Unary: -
 * Types: Numbers (0-9.), Hex ($0-$f)
 * Grouping: ( )
 *
 * @param	Str			String containing the equation.
 * @param	pResult		Pointer to storage for the result.
 * @return				1 if successful, 0 if equation fails.
 */
UBOOL Eval( FString Str, FLOAT* pResult )
{
	FLOAT Result;

	// Check for a matching number of brackets right up front.
	INT Brackets = 0;
	for( INT x = 0 ; x < Str.Len() ; x++ )
	{
		if( Str.Mid(x,1) == TEXT("(") )
		{
			Brackets++;
		}
		if( Str.Mid(x,1) == TEXT(")") )
		{
			Brackets--;
		}
	}

	if( Brackets != 0 )
	{
		debugf(TEXT("Expression Error : Mismatched brackets"));
		Result = 0;
	}

	else
	{
		if( !SubEval( &Str, &Result, 0 ) )
		{
			debugf(TEXT("Expression Error : Error in expression"));
			Result = 0;
		}
	}

	*pResult = Result;

	return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Convenience definitions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

typedef FObjectPropertyNode::TObjectIterator		TPropObjectIterator;
typedef FObjectPropertyNode::TObjectConstIterator	TPropObjectConstIterator;

static FORCEINLINE INT GetButtonSize()
{
	return 15;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Property drawing
//
///////////////////////////////////////////////////////////////////////////////////////////////////

/*-----------------------------------------------------------------------------
	UPropertyDrawProxy
-----------------------------------------------------------------------------*/
/**
 * @return	Returns the height required by the input proxy.
 */
INT UPropertyDrawProxy::GetProxyHeight() const
{
	return PROP_DefaultItemHeight;
}

void UPropertyDrawProxy::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy )
{
	// Extract text from the property . . .
	FString Str;
	InProperty->ExportText( 0, Str, InReadAddress - InProperty->Offset, InReadAddress - InProperty->Offset, NULL, PPF_Localized|PPF_PropertyWindow );
	UByteProperty* EnumProperty = Cast<UByteProperty>(InProperty);
	if ( EnumProperty != NULL && EnumProperty->Enum != NULL )
	{
		// see if we have alternate text to use for displaying the value
		UMetaData* PackageMetaData = EnumProperty->GetOutermost()->GetMetaData();
		if ( PackageMetaData )
		{
			FName AltDisplayName = FName(*(Str+TEXT(".DisplayName")));
			FString ValueText = PackageMetaData->GetValue(EnumProperty->Enum, AltDisplayName);
			if ( ValueText.Len() > 0 )
			{
				// render the alternate text for this enum value
				Str = ValueText;
			}
		}
	}

	
	// To be consistent with how UPropertyInputCombo displays values, we'll check to see if this is class property,
	// and if so, we'll omit the full object path and just display the value
	UClassProperty* ClassProp = Cast<UClassProperty>( InProperty );
	if( ClassProp != NULL )
	{
		// Trim everything up to the last dot character
		INT DotPos = Str.InStr( ".", TRUE );
		if( DotPos != -1 )
		{
			Str = Str.Right( Str.Len() - DotPos - 1 );
		}

		// Eliminate trailing ' character if it has one
		while( Str.Len() > 0 && Str.GetCharArray()( Str.Len() - 1 ) == TCHAR( '\'' ) )
		{
			Str = Str.Left( Str.Len() - 1 );
		}
	}


	// . . . and draw it.
	wxCoord W, H;
	InDC->GetTextExtent( *Str, &W, &H );
	InDC->DrawText( *Str, InRect.x, InRect.y+((InRect.GetHeight() - H) / 2) );
}

void UPropertyDrawProxy::DrawUnknown( wxDC* InDC, wxRect InRect )
{
	// Draw an empty string to the property window.
	const FString Str = LocalizeUnrealEd( "MultiplePropertyValues" );

	wxCoord W, H;
	InDC->GetTextExtent( *Str, &W, &H );
	InDC->DrawText( *Str, InRect.x, InRect.y+((InRect.GetHeight() - H) / 2) );
}

/*-----------------------------------------------------------------------------
	UPropertyDrawNumeric
-----------------------------------------------------------------------------*/

UBOOL UPropertyDrawNumeric::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	const FPropertyNode* ParentNode = InTreeNode->GetParentNode();
	check(ParentNode);
	const UProperty* ParentProperty = ParentNode->GetProperty();
	const UProperty* NodeProperty   = InTreeNode->GetProperty();

	// Supported if an atomic type that is not a UStructProperty named NAME_Rotator or editconst.
	if(		( NodeProperty->IsA(UFloatProperty::StaticClass()) || NodeProperty->IsA(UIntProperty::StaticClass())
		||	( NodeProperty->IsA(UByteProperty::StaticClass()) && ConstCast<UByteProperty>(NodeProperty)->Enum == NULL) )
		&& !( ConstCast<UStructProperty>(ParentProperty,CLASS_IsAUStructProperty) && ConstCast<UStructProperty>(ParentProperty,CLASS_IsAUStructProperty)->Struct->GetFName() == NAME_Rotator )
		&& !InTreeNode->IsEditConst() )
	{
		return TRUE;
	}

	return FALSE;
}

void UPropertyDrawNumeric::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy )
{
	// By default, draw the equation.  If it's empty, draw the actual value.

	FString Str = ((UPropertyInputNumeric*)InInputProxy)->Equation;
	if( Str.Len() == 0 )
	{
		InProperty->ExportText( 0, Str, InReadAddress - InProperty->Offset, InReadAddress - InProperty->Offset, NULL, PPF_Localized );
	}

	// Draw it.
	wxCoord W, H;
	InDC->GetTextExtent( *Str, &W, &H );
	InDC->DrawText( *Str, InRect.x, InRect.y+((InRect.GetHeight() - H) / 2) );
}

/*-----------------------------------------------------------------------------
	FRangedNumeric 
-----------------------------------------------------------------------------*/
class FRangedNumeric
{
public:
	FRangedNumeric (void);

	/*
	 * Set the range
	 */
	void InitRange  (const UBOOL bInIsInteger, const FString& InUIMinString, const FString& InUIMaxString, const FString& InClampMinString, const FString& InClampMaxString);

	/*
	 * Get the numeric value as a string
	 */
	FString GetStringValue(void) const;
	/*
	 * Get the numeric value as a string
	 */
	FLOAT GetFloatValue(void) const;
	/*
	 * Set the numeric value as a string
	 */
	void SetValue(const FString& InValueString);
	/*
	 * Set the numeric value based on number of ticks
	 */
	void MoveByIncrements (const INT InIncrementCount);
	/*
	 * Set the numeric value based on percent
	 */
	void MoveByPercent (const FLOAT InPercent);

	/**
	 * returns range percent of the value from UIMin to UIMax
	 *@retval - Percent from 0.0 to 1.0 
	 */
	FLOAT GetPercent(void) const;
private:

	union 
	{
		struct {
			INT IntValue;
			INT UIMinIntValue;
			INT UIMaxIntValue;
			INT ClampMinIntValue;
			INT ClampMaxIntValue;
		};
		struct {
			FLOAT FloatValue;
			FLOAT UIMinFloatValue;
			FLOAT UIMaxFloatValue;
			FLOAT ClampMinFloatValue;
			FLOAT ClampMaxFloatValue;
		};
	};
	UBOOL bIsInteger;
};

FRangedNumeric::FRangedNumeric(void)
{
	IntValue = 0;
	UIMinIntValue = MININT;
	UIMaxIntValue = MAXINT;
	ClampMinIntValue = MININT;
	ClampMaxIntValue = MAXINT;
	bIsInteger = TRUE;
}

/*
 * Set the range
 */
void FRangedNumeric::InitRange (const UBOOL bInIsInteger, const FString& InUIMinString, const FString& InUIMaxString, const FString& InClampMinString, const FString& InClampMaxString)
{
	bIsInteger = bInIsInteger;

	//if they didn't specify a UI Values just Clamp numbers, default UI Range to clamp range
	FString UIMinString = InUIMinString.Len() ? InUIMinString : InClampMinString;
	FString UIMaxString = InUIMaxString.Len() ? InUIMaxString : InClampMaxString;

	if (bIsInteger)
	{
		//Get clamp values
		ClampMinIntValue = (InClampMinString.Len()) ? appAtoi(*InClampMinString) : MININT;
		ClampMaxIntValue = (InClampMaxString.Len()) ? appAtoi(*InClampMaxString) : MAXINT;
		//ensure range is valid
		if (ClampMinIntValue >= ClampMaxIntValue)
		{
			warnf(NAME_Warning, TEXT("Clamp Min (%s) >= Clamp Max (%s) for Ranged Numeric"), *InClampMinString, *InClampMaxString );
		}
		//Get UI min max
		UIMinIntValue = appAtoi(*UIMinString);
		UIMaxIntValue = appAtoi(*UIMaxString);
		//make sure they conform to clamping values
		UIMinIntValue = Max(UIMinIntValue, ClampMinIntValue);
		UIMaxIntValue = Min(UIMaxIntValue, ClampMaxIntValue);
		//ensure range is valid
		if (UIMinIntValue >= UIMaxIntValue)
		{
			warnf(NAME_Warning, TEXT("UI Min (%s) >= UI Max (%s) for Ranged Numeric"), *UIMinString, *UIMaxString );
		}
	}
	else
	{
		//Get clamp values
		ClampMinFloatValue = (InClampMinString.Len()) ? appAtof(*InClampMinString) : -BIG_NUMBER;
		ClampMaxFloatValue = (InClampMaxString.Len()) ? appAtof(*InClampMaxString) : BIG_NUMBER;
		//ensure range is valid
		if (ClampMinFloatValue >= ClampMaxFloatValue)
		{
			warnf(NAME_Warning, TEXT("Clamp Min (%s) >= Clamp Max (%s) for Ranged Numeric"), *InClampMinString, *InClampMaxString );
		}
		//Get UI min max
		UIMinFloatValue = appAtof(*UIMinString);
		UIMaxFloatValue = appAtof(*UIMaxString);
		//make sure they conform to clamping values
		UIMinFloatValue = Max(UIMinFloatValue, ClampMinFloatValue);
		UIMaxFloatValue = Min(UIMaxFloatValue, ClampMaxFloatValue);
		//ensure range is valid
		if (UIMinFloatValue >= UIMaxFloatValue)
		{
			warnf(NAME_Warning, TEXT("UI Min (%s) >= UI Max (%s) for Ranged Numeric"), *UIMinString, *UIMaxString );
		}
	}
}
/*
 * Get the numeric value as a string
 */
FString FRangedNumeric::GetStringValue(void) const
{
	FString StringValue;
	if (bIsInteger)
	{
		StringValue = FString::Printf( TEXT("%i"), IntValue);
	}
	else
	{
		StringValue = FString::Printf( TEXT("%f"), FloatValue);
	}
	return StringValue;
}
/*
 * Get the numeric value as a float
 */
FLOAT FRangedNumeric::GetFloatValue(void) const
{
	FLOAT Retval;
	if (bIsInteger)
	{
		Retval = IntValue;
	}
	else
	{
		Retval = FloatValue;
	}
	return Retval;
}
/*
 * Set the numeric value as a string
 */
void FRangedNumeric::SetValue(const FString& InValueString)
{
	if (bIsInteger)
	{
		IntValue = appAtoi(*InValueString);
		IntValue = Clamp<INT>(IntValue, ClampMinIntValue, ClampMaxIntValue);
	}
	else
	{
		FloatValue = appAtof(*InValueString);
		FloatValue = Clamp<FLOAT>(FloatValue, ClampMinFloatValue, ClampMaxFloatValue);
	}
}

/*
 * Set the numeric value based on number of ticks
 */
void FRangedNumeric::MoveByIncrements (const INT InIncrementCount)
{
	if (bIsInteger)
	{
		//default to increment by 1
		IntValue += InIncrementCount;
		IntValue = Clamp<INT>(IntValue, UIMinIntValue, UIMaxIntValue);
	}
	else
	{
		//default for now to increment by .01
		FloatValue += InIncrementCount*.01f;
		FloatValue = Clamp<FLOAT>(FloatValue, UIMinFloatValue, UIMaxFloatValue);
	}
}

/*
 * Set the numeric value based on percent
 */
void FRangedNumeric::MoveByPercent (const FLOAT InPercent)
{
	if (bIsInteger)
	{
		if (UIMinIntValue < UIMaxIntValue)
		{
			IntValue = appRound(InPercent*(UIMaxIntValue - UIMinIntValue)) + UIMinIntValue;			//default to increment by 1
			IntValue = Clamp<INT>(IntValue, UIMinIntValue, UIMaxIntValue);
		}
	}
	else
	{
		if (UIMinFloatValue < UIMaxFloatValue)
		{
			FloatValue = InPercent*(UIMaxFloatValue - UIMinFloatValue) + UIMinFloatValue;			//default to increment by 1
			FloatValue = Clamp<FLOAT>(FloatValue, UIMinFloatValue, UIMaxFloatValue);
		}
	}
}

/**
 * returns range percent of the value from UIMin to UIMax
 *@retval - Percent from 0.0 to 1.0 
 */
FLOAT FRangedNumeric::GetPercent(void) const
{
	FLOAT Percent = 0.0f;
	if (bIsInteger)
	{
		if (UIMinIntValue < UIMaxIntValue)
		{
			Percent = (IntValue - UIMinIntValue) / FLOAT(UIMaxIntValue - UIMinIntValue);
		}
	}
	else
	{
			Percent = (FloatValue - UIMinFloatValue) / FLOAT(UIMaxFloatValue - UIMinFloatValue);
	}
	Percent = Clamp<FLOAT>(Percent, 0.0f, 1.0f);
	return Percent;
}

/*-----------------------------------------------------------------------------
	Ranged Numeric Render Helper
-----------------------------------------------------------------------------*/
void DrawRangedSlider (wxBufferedPaintDC& InDC, const wxRect& InSliderRect, const FString& InTitle, const FLOAT InPercent)
{
	INT CurveConstant = 2;

	//background rect
	wxRect BackgroundRect = wxRect(InSliderRect.x,InSliderRect.y+1, InSliderRect.width, InSliderRect.height-2);
	InDC.SetBrush( wxBrush( wxColour(160,160,160), wxSOLID ) );
	InDC.SetPen( *wxMEDIUM_GREY_PEN );
	InDC.DrawRoundedRectangle( BackgroundRect.x, BackgroundRect.y, BackgroundRect.GetWidth(), BackgroundRect.GetHeight(), CurveConstant);

	//percent rect
	wxRect HighlightRect = wxRect(BackgroundRect.x,BackgroundRect.y, appTrunc(BackgroundRect.GetWidth()*InPercent), BackgroundRect.GetHeight());
	InDC.SetBrush( wxBrush( wxColour(128,128,96), wxSOLID ) );
	InDC.SetPen( *wxBLACK_PEN );
	InDC.DrawRoundedRectangle( HighlightRect.x, HighlightRect.y, HighlightRect.GetWidth(), HighlightRect.GetHeight(), CurveConstant);

	//Draw Title
	InDC.SetTextForeground( *wxWHITE );

	wxCoord W, H;
	InDC.GetTextExtent( *InTitle, &W, &H );
	INT LeftX = InSliderRect.GetLeft()+4;
	INT CenterY = InSliderRect.GetTop() + ((InSliderRect.GetHeight()-H)>>1);
	InDC.DrawText( *InTitle, LeftX, CenterY );
}



/*-----------------------------------------------------------------------------
	UPropertyDrawRangedNumeric
-----------------------------------------------------------------------------*/

UBOOL UPropertyDrawRangedNumeric::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	const FPropertyNode* ParentNode = InTreeNode->GetParentNode();
	check(ParentNode);
	const UProperty* ParentProperty = ParentNode->GetProperty();
	const UProperty* NodeProperty   = InTreeNode->GetProperty();

	// Supported if an atomic type that is not a UStructProperty named NAME_Rotator or editconst.
	if(		( NodeProperty->IsA(UFloatProperty::StaticClass()) || NodeProperty->IsA(UIntProperty::StaticClass())
		||	( NodeProperty->IsA(UByteProperty::StaticClass()) && ConstCast<UByteProperty>(NodeProperty)->Enum == NULL) )
		&& !( ConstCast<UStructProperty>(ParentProperty,CLASS_IsAUStructProperty) && ConstCast<UStructProperty>(ParentProperty,CLASS_IsAUStructProperty)->Struct->GetFName() == NAME_Rotator )
		&& !InTreeNode->IsEditConst() )
	{
		const FString& UIMinString = NodeProperty->GetMetaData(TEXT("UIMin"));
		const FString& UIMaxString = NodeProperty->GetMetaData(TEXT("UIMax"));
		const FString& ClampMinString = NodeProperty->GetMetaData(TEXT("ClampMin"));
		const FString& ClampMaxString = NodeProperty->GetMetaData(TEXT("ClampMax"));
		if ((UIMinString.Len() || ClampMinString.Len()) && (UIMaxString.Len() || ClampMaxString.Len()))
		{
			return TRUE;
		}
	}

	return FALSE;
}

void UPropertyDrawRangedNumeric::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy )
{
	check(InProperty);
	check(InDC);

	FString Str;
	InProperty->ExportText( 0, Str, InReadAddress - InProperty->Offset, InReadAddress - InProperty->Offset, NULL, PPF_Localized );

	//use internal ranged value to get percent
	FRangedNumeric RangedNumeric;

	const UBOOL bIsFloat = InProperty->IsA(UFloatProperty::StaticClass());
	const FString& UIMinString = InProperty->GetMetaData(TEXT("UIMin"));
	const FString& UIMaxString = InProperty->GetMetaData(TEXT("UIMax"));
	const FString& ClampMinString = InProperty->GetMetaData(TEXT("ClampMin"));
	const FString& ClampMaxString = InProperty->GetMetaData(TEXT("ClampMax"));
	RangedNumeric.InitRange(!bIsFloat, UIMinString, UIMaxString, ClampMinString, ClampMaxString);
	RangedNumeric.SetValue(Str);

	//Draw the ranged slider using the helper function
	FLOAT Percent = RangedNumeric.GetPercent();

	wxBufferedPaintDC *BufferedPaintDC = wxDynamicCast(InDC, wxBufferedPaintDC);
	check(BufferedPaintDC);
	DrawRangedSlider (*BufferedPaintDC, InRect, RangedNumeric.GetStringValue(), Percent);
}

/*-----------------------------------------------------------------------------
	UPropertyDrawColor
-----------------------------------------------------------------------------*/

UBOOL UPropertyDrawColor::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	const UProperty* NodeProperty = InTreeNode->GetProperty();
	// Supported if the property is a UStructProperty named NAME_Color or NAME_LinearColor.
	return ( ConstCast<UStructProperty>(NodeProperty,CLASS_IsAUStructProperty) &&
			(ConstCast<UStructProperty>(NodeProperty,CLASS_IsAUStructProperty)->Struct->GetFName()==NAME_Color ||
			ConstCast<UStructProperty>(NodeProperty,CLASS_IsAUStructProperty)->Struct->GetFName()==NAME_LinearColor) );
}

void UPropertyDrawColor::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy )
{
	FColor	Color;

	if( Cast<UStructProperty>(InProperty,CLASS_IsAUStructProperty)->Struct->GetFName() == NAME_Color )
	{
		Color = *(FColor*)InReadAddress;
	}
	else
	{
		check( Cast<UStructProperty>(InProperty,CLASS_IsAUStructProperty)->Struct->GetFName() == NAME_LinearColor );
		Color = FColor(*(FLinearColor*)InReadAddress);
	}

	const wxColour WkColor( Color.R, Color.G, Color.B );
	InDC->SetPen( *wxBLACK_PEN );
	InDC->SetBrush( wxBrush( WkColor, wxSOLID ) );

	check( InInputProxy );
	const INT NumButtons = InInputProxy->GetNumButtons();//InInputProxy ? InInputProxy->GetNumButtons() : 0;
	InDC->DrawRectangle( InRect.x+4,InRect.y+4, InRect.GetWidth()-(NumButtons*GetButtonSize())-12,InRect.GetHeight()-8 );
}

/*-----------------------------------------------------------------------------
	UPropertyDrawRotation
-----------------------------------------------------------------------------*/

UBOOL UPropertyDrawRotation::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	const FPropertyNode* ParentNode = InTreeNode->GetParentNode();
	check(ParentNode);
	const UProperty* ParentProperty = ParentNode->GetProperty();

	// Supported if the property is a UStructProperty named NAME_Rotator.
	return (ConstCast<UStructProperty>(ParentProperty,CLASS_IsAUStructProperty) &&
			ConstCast<UStructProperty>(ParentProperty,CLASS_IsAUStructProperty)->Struct->GetFName() == NAME_Rotator);
}

void UPropertyDrawRotation::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy )
{
	FLOAT Val = *((INT*)InReadAddress);
	Val = 360.f * (Val / 65536.f);

	FString Wk;
	if( Abs(Val) > 359.f )
	{
		const INT Revolutions = Val / 360.f;
		Val -= Revolutions * 360;
		Wk = FString::Printf( TEXT("%.2f%c %s %d"), Val, 176, (Revolutions < 0)?TEXT("-"):TEXT("+"), abs(Revolutions) );
	}
	else
	{
		Wk = FString::Printf( TEXT("%.2f%c"), Val, 176 );
	}

	// note : The degree symbol is ASCII code 248 (char code 176)
	InDC->DrawText( *Wk, InRect.x,InRect.y+1 );
}

/*-----------------------------------------------------------------------------
	UPropertyDrawRotationHeader
-----------------------------------------------------------------------------*/

UBOOL UPropertyDrawRotationHeader::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	const UProperty* NodeProperty = InTreeNode->GetProperty();
	// Supported if the property is a UStructProperty named NAME_Rotator.
	return( ConstCast<UStructProperty>(NodeProperty,CLASS_IsAUStructProperty) &&
			ConstCast<UStructProperty>(NodeProperty,CLASS_IsAUStructProperty)->Struct->GetFName() == NAME_Rotator );
}

void UPropertyDrawRotationHeader::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy )
{
	FString Str;
	InProperty->ExportText( 0, Str, InReadAddress - InProperty->Offset, InReadAddress - InProperty->Offset, NULL, PPF_Localized|PPF_PropertyWindow|PPF_ExportAsFriendlyRotation );
	InDC->DrawText( *Str, InRect.x,InRect.y+1 );
}

/*-----------------------------------------------------------------------------
	UPropertyDrawBool
-----------------------------------------------------------------------------*/

UBOOL UPropertyDrawBool::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	const UProperty* NodeProperty = InTreeNode->GetProperty();
	// Supported if the property is a UBoolProperty.
	return ( ConstCast<UBoolProperty>(NodeProperty) != NULL );
}

void UPropertyDrawBool::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy )
{
	const WxMaskedBitmap& bmp = (*(BITFIELD*)InReadAddress & Cast<UBoolProperty>(InProperty)->BitMask) ? GPropertyWindowManager->CheckBoxOnB : GPropertyWindowManager->CheckBoxOffB;
	InDC->DrawBitmap( bmp, InRect.x, InRect.y+((InRect.GetHeight() - 13) / 2), 1 );
}

void UPropertyDrawBool::DrawUnknown( wxDC* InDC, wxRect InRect )
{
	const WxMaskedBitmap& bmp = GPropertyWindowManager->CheckBoxUnknownB;
	InDC->DrawBitmap( bmp, InRect.x, InRect.y+((InRect.GetHeight() - 13) / 2), 1 );
}

/*-----------------------------------------------------------------------------
	UPropertyDrawArrayHeader
-----------------------------------------------------------------------------*/

UBOOL UPropertyDrawArrayHeader::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	const UProperty* NodeProperty = InTreeNode->GetProperty();
	// Supported if the property is a UArrayProperty
	return ( ( NodeProperty->ArrayDim != 1 && InArrayIdx == -1 ) || ConstCast<UArrayProperty>(NodeProperty) );
}

void UPropertyDrawArrayHeader::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy )
{
	FString Str;
	if ( InProperty != NULL && Cast<UArrayProperty>(InProperty) != NULL )
	{
		Str = FString::Printf(TEXT("%s (%d)"), *LocalizeUnrealEd( TEXT("ArrayPropertyHeader") ), ((FScriptArray*)InReadAddress)->Num());
	}
	else
	{
		Str = *LocalizeUnrealEd( TEXT("ArrayPropertyHeader") );
	}
	wxCoord W, H;
	InDC->GetTextExtent( *Str, &W, &H );
	InDC->DrawText( wxString( *Str ), InRect.x, InRect.y+((InRect.GetHeight() - H) / 2) );
}

/*-----------------------------------------------------------------------------
UPropertyDrawTextEditBox
-----------------------------------------------------------------------------*/

UBOOL UPropertyDrawTextEditBox::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	const UProperty* NodeProperty = InTreeNode->GetProperty();
	// Supported if string property with CPF_TextEditBox set
	if(	NodeProperty->IsA(UStrProperty::StaticClass()) && (NodeProperty->PropertyFlags & CPF_EditTextBox) )
	{
		return TRUE;
	}
	return FALSE;
}

void UPropertyDrawTextEditBox::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy )
{
	FString Str;
	InProperty->ExportText( 0, Str, InReadAddress - InProperty->Offset, InReadAddress - InProperty->Offset, NULL, PPF_Localized );

	// Strip carriage returns.
	Str.ReplaceInline(TEXT("\n"), TEXT(" "));

	// Draw it.
	wxCoord W, H;
	InDC->GetTextExtent( *Str, &W, &H );
	InDC->DrawText( *Str, InRect.x, InRect.y+((InRect.GetHeight() - H) / 2) );
}

/*-----------------------------------------------------------------------------
UPropertyDrawMultilineTextBox
-----------------------------------------------------------------------------*/

UBOOL UPropertyDrawMultilineTextBox::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	const UProperty* NodeProperty = InTreeNode->GetProperty();
	// Supported if we have the appropriate metadata
	if(NodeProperty->HasMetaData(TEXT("MultilineWithMaxRows")))
	{
		return TRUE;
	}
	return FALSE;
}

/**
 * @return	Returns the height required by the draw proxy.
 */
INT UPropertyDrawMultilineTextBox::GetProxyHeight() const
{
	if(PropertyControl)
	{
		return PropertyControl->InputProxy->GetProxyHeight();
	}

	return PROP_DefaultItemHeight;
}

/**
 * Associates this draw proxy with a given control so we can access the
 * input proxy if needed.
 * @param	InControl	The property control to associate.
 */
void UPropertyDrawMultilineTextBox::AssociateWithControl(WxPropertyControl* InControl)
{
	PropertyControl = InControl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Property input
//
///////////////////////////////////////////////////////////////////////////////////////////////////

/*-----------------------------------------------------------------------------
WxPropertyItemButton
-----------------------------------------------------------------------------*/
IMPLEMENT_DYNAMIC_CLASS(WxPropertyItemButton, wxBitmapButton);

BEGIN_EVENT_TABLE(WxPropertyItemButton, wxBitmapButton)
EVT_ERASE_BACKGROUND(WxPropertyItemButton::OnEraseBackground)
END_EVENT_TABLE()

/**
 * Constructor.
 */
WxPropertyItemButton::WxPropertyItemButton()
{

}

/**
 * Constructor.
 */
WxPropertyItemButton::WxPropertyItemButton(wxWindow *parent,
											wxWindowID id,
											const wxBitmap& bitmap,
											const wxPoint& pos,
											const wxSize& size,
											long style,
											const wxValidator& validator,
											const wxString& name)
											: wxBitmapButton(parent, id, bitmap, pos, size, style, validator, name)
{

}

/** Overloads the erase background event to prevent flickering */
void WxPropertyItemButton::OnEraseBackground(wxEraseEvent &Event)
{

}

/*-----------------------------------------------------------------------------
	UPropertyInputProxy private interface
-----------------------------------------------------------------------------*/

// Adds a button to the right side of the item window.
void UPropertyInputProxy::AddButton( WxPropertyControl* InItem, ePropButton InButton, const wxRect& InRC )
{
	// when not in the editor, don't create buttons that rely on the generic or level browsers
	if (GUnrealEd != NULL || (InButton != PB_Browse && InButton != PB_Use && InButton != PB_Find))
	{
		WxMaskedBitmap* bmp = NULL;
		FString ToolTip;
		INT ID = 0;

		switch( InButton )
		{
			case PB_Add:
				bmp = &GPropertyWindowManager->Prop_AddNewItemB;
				ToolTip = *LocalizeUnrealEd("PropertyWindow_AddNewItem");
				ID = ID_PROP_PB_ADD;
				break;

			case PB_Empty:
				bmp = &GPropertyWindowManager->Prop_RemoveAllItemsFromArrayB;
				ToolTip = *LocalizeUnrealEd("PropertyWindow_RemoveAllItemsFromArray");
				ID = ID_PROP_PB_EMPTY;
				break;

			case PB_Insert:
				bmp = &GPropertyWindowManager->Prop_InsertNewItemHereB;
				ToolTip = *LocalizeUnrealEd("PropertyWindow_InsertNewItemHere");
				ID = ID_PROP_PB_INSERT;
				break;

			case PB_Delete:
				bmp = &GPropertyWindowManager->Prop_DeleteItemB;
				ToolTip = *LocalizeUnrealEd("PropertyWindow_DeleteItem");
				ID = ID_PROP_PB_DELETE;
				break;

			case PB_Browse:
				bmp = &GPropertyWindowManager->Prop_ShowGenericBrowserB;
				ToolTip = *LocalizeUnrealEd("PropertyWindow_FindObjectInContentBrowser");
				ID = ID_PROP_PB_BROWSE;
				break;

			case PB_Clear:
				bmp = &GPropertyWindowManager->Prop_ClearAllTextB;
				ToolTip = *LocalizeUnrealEd("PropertyWindow_ClearAllText");
				ID = ID_PROP_PB_CLEAR;
				break;

			case PB_Find:
				bmp = &GPropertyWindowManager->Prop_UseMouseToPickActorB;
				ToolTip = *LocalizeUnrealEd("PropertyWindow_UseMouseToPickActorFromViewport");
				ID = ID_PROP_PB_FIND;
				break;

			case PB_Use:
				bmp = &GPropertyWindowManager->Prop_UseCurrentBrowserSelectionB;
				ToolTip = *LocalizeUnrealEd("PropertyWindow_UseCurrentSelectionInBrowser");
				ID = ID_PROP_PB_USE;
				break;

			case PB_NewObject:
				bmp = &GPropertyWindowManager->Prop_NewObjectB;
				ToolTip = *LocalizeUnrealEd("PropertyWindow_CreateANewObject");
				ID = ID_PROP_PB_NEWOBJECT;
				break;

			case PB_Duplicate:
				bmp = &GPropertyWindowManager->Prop_DuplicateB;
				ToolTip = *LocalizeUnrealEd("PropertyWindow_DuplicateThisItem");
				ID = ID_PROP_PB_DUPLICATE;
				break;

			case PB_CurveEdit:
				bmp = &GPropertyWindowManager->Prop_CurveEditB;
				ToolTip = *LocalizeUnrealEd("PropertyWindow_CurveEdit");
				ID = ID_PROP_PB_CURVEEDIT;
				break;

			default:
				checkMsg( 0, "Unknown button type" );
				break;
		}

		// Create a new button and add it to the button array.
		WxPropertyItemButton* button = new WxPropertyItemButton( InItem, ID, *bmp, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxBU_AUTODRAW);
		button->SetToolTip( *ToolTip );
		Buttons.AddItem( button );

		// Notify the parent.
		ParentResized( InItem, InRC );
	}
}

// Creates any controls that are needed to edit the property.
void UPropertyInputProxy::CreateButtons( WxPropertyControl* InItem, const wxRect& InRC )
{
	check(InItem);
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode ();
	check(PropertyTreeNode);
	UProperty* NodeProperty = PropertyTreeNode->GetProperty();

	// Clear out existing buttons.
	DeleteButtons();

	// If no property is bound, don't create any buttons.
	if( !NodeProperty )
	{
		return;
	}

	// If property is const, don't create any buttons.
	if (PropertyTreeNode->IsEditConst())
	{
		return;
	}

	// If the property is an item of a const array, don't create any buttons.
	const UArrayProperty* ArrayProp = ConstCast<UArrayProperty>( NodeProperty->GetOuter() );
	if (ArrayProp != NULL && (ArrayProp->PropertyFlags & CPF_EditConst))
	{
		return;
	}

	// If the property is a 'curve', then show the CurveEditor prompt
	if (NodeProperty->IsA(UObjectProperty::StaticClass()))
	{
		const UObjectProperty* ObjectProperty	= ConstCast<UObjectProperty>( NodeProperty,CLASS_IsAUObjectProperty);
		if (ObjectProperty)
		{
			if ((ObjectProperty->GetName().InStr(TEXT("InterpCurve")) != INDEX_NONE) &&
				(ObjectProperty->GetName().InStr(TEXT("InterpCurvePoint")) == INDEX_NONE))
			{
				FString ObjectPropCategory = ObjectProperty->Category.ToString();
				if ((ObjectPropCategory.InStr(TEXT("DistributionFloat"), FALSE, TRUE) == INDEX_NONE) &&
					(ObjectPropCategory.InStr(TEXT("DistributionVector"), FALSE, TRUE) == INDEX_NONE))
				{
					AddButton( InItem, PB_CurveEdit, InRC );
				}
			}
		}
	}

	//////////////////////////////
	// Handle an array property.

	if( NodeProperty->IsA(UArrayProperty::StaticClass() ) )
	{
		if( !(NodeProperty->PropertyFlags & CPF_EditFixedSize) )
		{
			const UArrayProperty* ArrayProp = ConstCast<UArrayProperty>( NodeProperty );
			if (ArrayProp)
			{
				//if this array supports actors
				UObjectProperty* ObjProp = Cast<UObjectProperty>( ArrayProp->Inner );
				if( ObjProp )
				{
					UClass* ObjPropClass = ObjProp->PropertyClass;
					if ( ObjPropClass->IsChildOf( AActor::StaticClass() ) )
					{
						AddButton( InItem, PB_Use, InRC );
					}
				}
			}

			AddButton( InItem, PB_Add, InRC );
			AddButton( InItem, PB_Empty, InRC );
		}
	}

	if( ArrayProp )
	{
		if( PropertyTreeNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly) && !(ArrayProp->PropertyFlags & CPF_EditFixedSize) )
		{
			AddButton( InItem, PB_Insert, InRC );
			AddButton( InItem, PB_Delete, InRC );
			AddButton( InItem, PB_Duplicate, InRC );
		}

		// If this is coming from an array property and we are using a combobox for editing,
		// we don't want any other buttons beyond these two.
		if( ComboBox != NULL )
		{
			return;
		}
	}

	//////////////////////////////
	// Handle an object property.

	if( NodeProperty->IsA(UObjectProperty::StaticClass()) || NodeProperty->IsA(UInterfaceProperty::StaticClass()))
	{
		//ignore this node if the consistency check should happen for the children
		UBOOL bStaticSizedArray = (NodeProperty->ArrayDim > 1) && (PropertyTreeNode->GetArrayIndex() == -1);
		if (!bStaticSizedArray)
		{
			TArray<BYTE*> ReadAddresses;
			// Only add buttons if read addresses are all NULL or non-NULL.
			PropertyTreeNode->GetReadAddress( PropertyTreeNode, FALSE, ReadAddresses, FALSE );
			{
				if( PropertyTreeNode->HasNodeFlags(EPropertyNodeFlags::EditInline) )
				{
					if ( !PropertyTreeNode->HasNodeFlags(EPropertyNodeFlags::EditInlineUse) )
					{
						// editinline objects can be created at runtime from within the property window
						AddButton( InItem, PB_NewObject, InRC );
					}

					// hmmm, seems like this code could be removed and the code inside the 'if <UClassProperty>' check
					// below could be moved outside the else....but is there a reason to allow class properties to have the
					// following buttons if the class property is marked 'editinline' (which is effectively what this logic is doing)
					if( !(NodeProperty->PropertyFlags & CPF_NoClear) )
					{
						AddButton( InItem, PB_Clear, InRC );
					}

					// this object could also be assigned to some resource from a content package
					if ( PropertyTreeNode->HasNodeFlags(EPropertyNodeFlags::EditInlineUse) )
					{
						// add a button for displaying the generic browser
						AddButton( InItem, PB_Browse, InRC );

						// add button for filling the value of this item with the selected object from the GB
						AddButton( InItem, PB_Use, InRC );
					}
				}
				else
				{
					// ignore class properties
					if( ConstCast<UClassProperty>( NodeProperty ) == NULL )
					{
						// reference to object resource that isn't dynamically created (i.e. some content package)
						if( !(NodeProperty->PropertyFlags & CPF_NoClear) )
						{
							// add button to clear the text
							AddButton( InItem, PB_Clear, InRC );
						}

						//@fixme ?? this will always evaluate to true (!InItem->bEditInline)
						if( !PropertyTreeNode->HasNodeFlags(EPropertyNodeFlags::EditInline) || PropertyTreeNode->HasNodeFlags(EPropertyNodeFlags::EditInlineUse) )
						{
							// add button to display the generic browser
							AddButton( InItem, PB_Browse, InRC );

							// add button for filling the value of this item with the selected object from the GB
							AddButton( InItem, PB_Use, InRC );
						}
					}
				}
			}
		}
	}

	// handle a string property
	if ( NodeProperty->IsA(UStrProperty::StaticClass()) &&
		(NodeProperty->HasMetaData(TEXT("IsPathName")) || (ArrayProp != NULL && ArrayProp->HasMetaData(TEXT("IsPathName")))) )
	{
		//Allow assignment of selected object path to this property
		AddButton( InItem, PB_Use, InRC );
		// since this property is expected to contain an object path name, allow looking up that path in the generic browser
		AddButton(InItem, PB_Browse, InRC);
	}
}

// Deletes any buttons that were created.
void UPropertyInputProxy::DeleteButtons()
{
	for( INT x = 0 ; x < Buttons.Num() ; ++x )
	{
		Buttons(x)->Destroy();
	}

	Buttons.Empty();
}

/*-----------------------------------------------------------------------------
	UPropertyInputProxy public interface
-----------------------------------------------------------------------------*/

/**
 * @return	Returns the height required by the input proxy.
 */
INT UPropertyInputProxy::GetProxyHeight() const
{
	return PROP_DefaultItemHeight;
}

// Allows the created controls to react to the parent window being resized.
void UPropertyInputProxy::ParentResized( WxPropertyControl* InItem, const wxRect& InRC )
{
	INT XPos = InRC.x + InRC.GetWidth() - GetButtonSize();
	INT YPos = InRC.height / 2 - GetButtonSize() / 2;

	for( INT x = 0 ; x < Buttons.Num() ; ++x )
	{
		Buttons(x)->SetSize(XPos, YPos, GetButtonSize(), GetButtonSize());
		XPos -= GetButtonSize();
	}
}

// Sends a text string to the selected objects.
void UPropertyInputProxy::SendTextToObjects( WxPropertyControl* InItem, const FString& InText )
{
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();

	// If more than one object is selected, an empty field indicates their values for this property differ.
	// Don't send it to the objects value in this case (if we did, they would all get set to None which isn't good).
	if( ObjectNode->GetNumObjects() > 1 && !InText.Len() )
	{
		return;
	}

	// Build up a list of objects to modify.
	TArray<FObjectBaseAddress> ObjectsToModify;
	for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
	{
		UObject*	Object = *Itor;
		BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );
		ObjectsToModify.AddItem( FObjectBaseAddress( Object, Addr ) );
	}

	const FString Value( InText );
	ImportText( InItem, ObjectsToModify, Value );
}

void UPropertyInputProxy::ImportText(WxPropertyControl* InItem,
									 const TArray<FObjectBaseAddress>& InObjects,
									 const FString& InValue)
{
	TArray<FString> Values;
	for ( INT ObjectIndex = 0 ; ObjectIndex < InObjects.Num() ; ++ObjectIndex )
	{
		Values.AddItem( InValue );
	}
	if (Values.Num() > 0)
	{
		ImportText( InItem, InObjects, Values );
	}
}

void UPropertyInputProxy::ImportText(WxPropertyControl* InItem,
									 const TArray<FObjectBaseAddress>& InObjects,
									 const TArray<FString>& InValues)
{
	check(InItem);
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode ();
	check(PropertyTreeNode);
	UProperty* NodeProperty = PropertyTreeNode->GetProperty();

	////////////////
	FReloadObjectArc* OldMemoryArchive = GMemoryArchive;
	FArchetypePropagationArc* PropagationArchive;
	GMemoryArchive = PropagationArchive = new FArchetypePropagationArc();

	UWorld* OldGWorld = NULL;

	// If the object we are modifying is in the PIE world, than make the PIE world the active
	// GWorld.  Assumes all objects managed by this property window belong to the same world.
	UPackage* ObjectPackage = InObjects(0).Object->GetOutermost();
	UBOOL bIsPIEPackage = ObjectPackage->PackageFlags & PKG_PlayInEditor;
	if ( GUnrealEd && GUnrealEd->PlayWorld && bIsPIEPackage)
	{
		OldGWorld = SetPlayInEditorWorld(GUnrealEd->PlayWorld);
	}
	///////////////

	// Send the values and assemble a list of preposteditchange values.
	TArray<FString> Befores;
	UBOOL bNotifiedPreChange = FALSE;
	UObject *NotifiedObj = NULL;
	TArray< TMap<FString,INT> > ArrayIndicesPerObject;

	for ( INT ObjectIndex = 0 ; ObjectIndex < InObjects.Num() ; ++ObjectIndex )
	{	
		const FObjectBaseAddress& Cur = InObjects( ObjectIndex );
		if (Cur.BaseAddress == NULL)
		{
			InItem->RequestDataValidation();
			//Fully abort this procedure.  The data has changed out from under the object
			return;
		}

		// Cache the value of the property before modifying it.
		FString PreviousValue;
		NodeProperty->ExportText( 0, PreviousValue, Cur.BaseAddress - NodeProperty->Offset, Cur.BaseAddress - NodeProperty->Offset, NULL, PPF_Localized );

		// Check if we need to call PreEditChange on all objects.
		// Remove quotes from the original value because FName properties  
		// are wrapped in quotes before getting here. This causes the 
		// string comparison to fail even when the name is unchanged. 
		if ( !bNotifiedPreChange && appStrcmp( *InValues(ObjectIndex).TrimQuotes(), *PreviousValue ) )
		{
			bNotifiedPreChange = TRUE;
			NotifiedObj = Cur.Object;
			InItem->NotifyPreChange( NodeProperty, Cur.Object );
		}

		// Set the new value.
		const TCHAR* NewValue = *InValues(ObjectIndex);
		NodeProperty->ImportText( NewValue, Cur.BaseAddress, PPF_Localized, Cur.Object );

		// Cache the value of the property after having modified it.
		FString ValueAfterImport;
		NodeProperty->ExportText( 0, ValueAfterImport, Cur.BaseAddress - NodeProperty->Offset, Cur.BaseAddress - NodeProperty->Offset, NULL, PPF_Localized );
		Befores.AddItem( ValueAfterImport );

		// If the values before and after setting the property differ, mark the object dirty.
		if ( appStrcmp( *PreviousValue, *ValueAfterImport ) != 0 )
		{
			Cur.Object->MarkPackageDirty();
		}

		// I apologize for the nasty #ifdefs, this is just to keep multiple versions of the code until we decide on one
#ifdef PROPAGATE_EXACT_PROPERTY_ONLY
		// tell the propagator to pass the change along (InData - InParent should be the byte offset into the object of the actual property)
		GObjectPropagator->OnPropertyChange(Cur.Object, NodeProperty, Cur.BaseAddress - NodeProperty->Offset);
#else
		// we need to go up to the highest property that we can and send the entire contents (whole struct, array, etc)
		FPropertyNode* Item = PropertyTreeNode;
		while (Item->GetParentNode()->GetProperty())
		{
			Item = Item->GetParentNode();
		}
		GObjectPropagator->OnPropertyChange(Cur.Object, Item->GetProperty(), Item->GetProperty()->Offset);
#endif

		//add on array index so we can tell which entry just changed
		ArrayIndicesPerObject.AddItem(TMap<FString,INT>());
		GenerateArrayIndexMapToObjectNode(ArrayIndicesPerObject(ObjectIndex), PropertyTreeNode);
	}

	GMemoryArchive->ActivateReader();

	// Note the current property window so that CALLBACK_ObjectPropertyChanged
	// doesn't destroy the window out from under us.
	WxPropertyWindow* MainWindow = InItem->GetPropertyWindow();
	check(MainWindow);
	MainWindow->ChangeActiveCallbackCount(1);

	// If PreEditChange was called, so should PostEditChange.
	if ( bNotifiedPreChange )
	{
		// Call PostEditChange on all objects.
		const UBOOL bTopologyChange = FALSE;
		FPropertyChangedEvent ChangeEvent(NodeProperty, bTopologyChange, EPropertyChangeType::ValueSet);
		ChangeEvent.SetArrayIndexPerObject(ArrayIndicesPerObject);
		InItem->NotifyPostChange( ChangeEvent );
	}

	// Unset, effectively making this property window updatable by CALLBACK_ObjectPropertyChanged.
	MainWindow->ChangeActiveCallbackCount(-1);

	// If PreEditChange was called, so was PostEditChange.
	if ( bNotifiedPreChange )
	{
		// Look at values after PostEditChange and refresh the property window as necessary.
		for ( INT ObjectIndex = 0 ; ObjectIndex < InObjects.Num() ; ++ObjectIndex )
		{	
			const FObjectBaseAddress& Cur = InObjects( ObjectIndex );

			// Cur.BaseAddress could be stale as part of NotifyPostChange process, don't trust it
			BYTE* Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Cur.Object );

			FString After;
			NodeProperty->ExportText( 0, After, Addr - NodeProperty->Offset, Addr - NodeProperty->Offset, NULL, PPF_Localized );

			// the value of this property might have been modified when PostEditChange was called, so check for that now.
			if( Befores(ObjectIndex) != After || !Cast<UPropertyInputRotation>( this ) )
			{
				RefreshControlValue( NodeProperty, Addr );
			}
		}
	}


	// if GMemoryArchive is still pointing to the one we created, restore it to the previous value
	if ( GMemoryArchive == PropagationArchive )
	{
		GMemoryArchive = OldMemoryArchive;
	}

	// clean up the FArchetypePropagationArc we created
	delete PropagationArchive;
	PropagationArchive = NULL;

	if (OldGWorld)
	{
		// restore the original (editor) GWorld
		RestoreEditorWorld( OldGWorld );
	}

	// Redraw
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

void UPropertyInputProxy::ImportText(WxPropertyControl* InItem,
									 UProperty* InProperty,
									 const TCHAR* InBuffer,
									 BYTE* InData,
									 UObject* InParent)
{
	FReloadObjectArc* OldMemoryArchive = GMemoryArchive;
	FArchetypePropagationArc* PropagationArchive;
	GMemoryArchive = PropagationArchive = new FArchetypePropagationArc();

	UWorld* OldGWorld = NULL;
	// if the object we are modifying is in the PIE world, than make the PIE world
	// the active GWorld
	UPackage* ObjectPackage = InParent->GetOutermost();
	UBOOL bIsPIEPackage = ObjectPackage->PackageFlags & PKG_PlayInEditor;
	if ( GUnrealEd && GUnrealEd->PlayWorld && bIsPIEPackage)
	{
		OldGWorld = SetPlayInEditorWorld(GUnrealEd->PlayWorld);
	}

	InItem->NotifyPreChange( InProperty, InParent );

	GMemoryArchive->ActivateReader();

	// Cache the value of the property before modifying it.
	FString PreviousValue;
	InProperty->ExportText( 0, PreviousValue, InData - InProperty->Offset, InData - InProperty->Offset, NULL, PPF_Localized );

	// Set the new value.
	InProperty->ImportText( InBuffer, InData, PPF_Localized, InParent );

	// Cache the value of the property after having modified it.
	FString ValueAfterImport;
	InProperty->ExportText( 0, ValueAfterImport, InData - InProperty->Offset, InData - InProperty->Offset, NULL, PPF_Localized );

	// If the values before and after setting the property differ, mark the object dirty.
	if ( appStrcmp( *PreviousValue, *ValueAfterImport ) != 0 )
	{
		InParent->MarkPackageDirty();
	}

	// Note the current property window so that CALLBACK_ObjectPropertyChanged
	// doesn't destroy the window out from under us.
	WxPropertyWindow* MainWindow = InItem->GetPropertyWindow();
	check(MainWindow);
	MainWindow->ChangeActiveCallbackCount(1);

	const UBOOL bTopologyChange = FALSE;
	FPropertyChangedEvent ChangeEvent(InProperty, bTopologyChange, EPropertyChangeType::ValueSet);
	InItem->NotifyPostChange( ChangeEvent );

	// Unset, effectively making this property window updatable by CALLBACK_ObjectPropertyChanged.
	MainWindow->ChangeActiveCallbackCount(-1);

	// I apologize for the nasty #ifdefs, this is just to keep multiple versions of the code until we decide on one
#ifdef PROPAGATE_EXACT_PROPERTY_ONLY
    // tell the propagator to pass the change along (InData - InParent should be the byte offset into the object of the actual property)
	GObjectPropagator->OnPropertyChange(InParent, InProperty, InData - (BYTE*)InParent);
#else
	// we need to go up to the highest property that we can and send the entire contents (whole struct, array, etc)
	FPropertyNode* TestTreeNode = InItem->GetPropertyNode ();
	check(TestTreeNode);
	while (TestTreeNode->GetParentNode()->GetProperty())
	{
		TestTreeNode = TestTreeNode->GetParentNode();
	}
	UProperty* ParentProperty = TestTreeNode->GetProperty();
	GObjectPropagator->OnPropertyChange(InParent, ParentProperty, ParentProperty->Offset);
#endif

	FString After;
	InProperty->ExportText( 0, After, InData - InProperty->Offset, InData - InProperty->Offset, NULL, PPF_Localized );

	if( ValueAfterImport != After || !Cast<UPropertyInputRotation>( this ) )
	{
		RefreshControlValue( InProperty, InData );
	}

	// if GMemoryArchive is still pointing to the one we created, restore it to the previous value
	if ( GMemoryArchive == PropagationArchive )
	{
		GMemoryArchive = OldMemoryArchive;
	}

	// clean up the FArchetypePropagationArc we created
	delete PropagationArchive;
	PropagationArchive = NULL;

	if (OldGWorld)
	{
		// restore the original (editor) GWorld
		RestoreEditorWorld( OldGWorld );
	}

	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );

	//@todo: need a clean way to notify archetype actor property windows to update value of property
	// when value in archetype is modified!
}

/**
 * Called when the "Use selected" property item button is clicked.
 */
void UPropertyInputProxy::OnUseSelected(WxPropertyControl* InItem)
{
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	check(PropertyTreeNode);
	UProperty* NodeProperty = PropertyTreeNode->GetProperty();

	UObjectProperty* ObjProp = Cast<UObjectProperty>( NodeProperty );
	UInterfaceProperty* IntProp = Cast<UInterfaceProperty>( NodeProperty );
	UStrProperty* StrProp = Cast<UStrProperty>( NodeProperty );
	if( ObjProp || IntProp || StrProp)
	{
		GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);

		UClass* ObjPropClass = NULL;
		if (ObjProp) 
		{
			ObjPropClass = ObjProp->PropertyClass;
		}
		else if (IntProp)
		{
			ObjPropClass = IntProp->InterfaceClass;
		}
		else if (StrProp)
		{
			//String Property can point to any object type
			ObjPropClass = UObject::StaticClass();
		}
		USelection* SelectedSet = GEditor->GetSelectedSet( ObjPropClass );

		// If an object of that type is selected, use it.
		UObject* SelectedObject = SelectedSet->GetTop( ObjPropClass );
		if( SelectedObject )
		{
			const FString SelectedObjectPathName = SelectedObject->GetPathName();
			FString CompareName;
			if (StrProp)
			{
				//StrProperties simply store the path name without the class prefix
				CompareName = SelectedObjectPathName;
			}
			else
			{
				CompareName = FString::Printf( TEXT("%s'%s'"), *SelectedObject->GetClass()->GetName(), *SelectedObjectPathName );
			}
			SendTextToObjects( InItem, *SelectedObjectPathName );
			InItem->Refresh();

			// Read values back and report any failures.
			TArray<FObjectBaseAddress> ObjectsThatWereModified;
			FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();
			UBOOL bAllObjectPropertyValuesMatch = TRUE;
			for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
			{
				UObject*	Object = *Itor;
				BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );
				FObjectBaseAddress Cur( Object, Addr );
				FString PropertyValue;
				NodeProperty->ExportText( 0, PropertyValue, Cur.BaseAddress - NodeProperty->Offset, Cur.BaseAddress - NodeProperty->Offset, NULL, PPF_Localized );
				if ( PropertyValue != CompareName )
				{
					bAllObjectPropertyValuesMatch = FALSE;
					break;
				}
			}

			// Warn that some object assignments failed.
			if ( !bAllObjectPropertyValuesMatch )
			{
				appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("ObjectAssignmentsFailed"), *CompareName, *InItem->GetDisplayName()) );
			}
		}
	}
}

/**
 * Recurse up to the next object node, adding all array indices into a map according to their property name
 * @param ArrayIndexMap - for the current object, what properties use which array offsets
 * @param InNode - node to start adding array offsets for.  This function will move upward until it gets to an object node
 */
void UPropertyInputProxy::GenerateArrayIndexMapToObjectNode(TMap<FString,INT>& OutArrayIndexMap, FPropertyNode* InNode)
{
	OutArrayIndexMap.Empty();
	for (FPropertyNode* IterationNode = InNode; (IterationNode != NULL) && (IterationNode->GetObjectNode() == NULL); IterationNode = IterationNode->GetParentNode())
	{
		UProperty* Property = IterationNode->GetProperty();
		if (Property)
		{
			//since we're starting from the lowest level, we have to take the first entry.  In the case of an array, the entries and the array itself have the same name, except the parent has an array index of -1
			if (!OutArrayIndexMap.HasKey(Property->GetName()))
			{
				OutArrayIndexMap.Set(Property->GetName(), IterationNode->GetArrayIndex());;
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	UPropertyInputBool
-----------------------------------------------------------------------------*/

/**
 * Wrapper method for determining whether a class is valid for use by this property item input proxy.
 *
 * @param	InItem			the property window item that contains this proxy.
 * @param	CheckClass		the class to verify
 * @param	bAllowAbstract	TRUE if abstract classes are allowed
 *
 * @return	TRUE if CheckClass is valid to be used by this input proxy
 */
UBOOL UPropertyInputArrayItemBase::IsClassAllowed( WxPropertyControl* InItem,  UClass* CheckClass, UBOOL bAllowAbstract ) const
{
	check(CheckClass);

	return !CheckClass->HasAnyClassFlags(CLASS_Hidden|CLASS_HideDropDown|CLASS_Deprecated)
		&&	(bAllowAbstract || !CheckClass->HasAnyClassFlags(CLASS_Abstract));
}

void UPropertyInputArrayItemBase::ButtonClick( WxPropertyControl* InItem, ePropButton InButton )
{
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	check(PropertyTreeNode);
	UProperty* NodeProperty = PropertyTreeNode->GetProperty();
	FPropertyNode* ParentNode = PropertyTreeNode->GetParentNode();
	check(ParentNode);
	WxPropertyControl* ParentWindow = ParentNode->GetNodeWindow();
	check(ParentWindow);

	switch( InButton )
	{
		case PB_Insert:
			{
				TArray<BYTE*> Addresses;
				FScriptArray* Addr = NULL;
				if ( ParentNode->GetReadAddress( ParentNode, ParentNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses ) )
				{
					Addr = (FScriptArray*)Addresses(0);
				}

				if( Addr )
				{
					// Create an FArchetypePropagationArc to propagate the updated property values from archetypes to instances of that archetype
					FReloadObjectArc* OldMemoryArchive = GMemoryArchive;
					FArchetypePropagationArc* PropagationArchive;
					GMemoryArchive = PropagationArchive = new FArchetypePropagationArc();

					ParentWindow->NotifyPreChange( ParentNode->GetProperty() );

					// now change the propagation archive to read mode
					PropagationArchive->ActivateReader();

					Addr->Insert( PropertyTreeNode->GetArrayIndex(), 1, NodeProperty->GetSize() );	

					//set up indices for the coming events
					TArray< TMap<FString,INT> > ArrayIndicesPerObject;
					for (INT ObjectIndex = 0; ObjectIndex < Addresses.Num(); ++ObjectIndex)
					{
						//add on array index so we can tell which entry just changed
						ArrayIndicesPerObject.AddItem(TMap<FString,INT>());
						GenerateArrayIndexMapToObjectNode(ArrayIndicesPerObject(ObjectIndex), PropertyTreeNode);
					}

					BYTE* Dest = (BYTE*)Addr->GetData() + PropertyTreeNode->GetArrayIndex()*NodeProperty->GetSize();
					appMemzero( Dest, NodeProperty->GetSize() );

					// Apply struct defaults.
					UStructProperty* StructProperty	= Cast<UStructProperty>( NodeProperty,CLASS_IsAUStructProperty);
					if( StructProperty )
					{
						UScriptStruct* InnerStruct = StructProperty->Struct;
						check(InnerStruct);

						// if this struct has defaults, copy them over
						if ( InnerStruct->GetDefaultsCount() && StructProperty->HasValue(InnerStruct->GetDefaults()) )
						{
							check( Align(InnerStruct->GetDefaultsCount(),InnerStruct->MinAlignment) == NodeProperty->GetSize() );
							StructProperty->CopySingleValue( Dest, InnerStruct->GetDefaults() );
						}
					}

					const UBOOL bExpand = TRUE;
					const UBOOL bRecurse = FALSE;
					ParentNode->SetExpanded(bExpand, bRecurse);
					const UBOOL bTopologyChange = TRUE;
					FPropertyChangedEvent ChangeEvent(ParentNode->GetProperty(), bTopologyChange, EPropertyChangeType::ArrayAdd);
					ChangeEvent.SetArrayIndexPerObject(ArrayIndicesPerObject);
					ParentWindow->NotifyPostChange( ChangeEvent );

					// if GMemoryArchive is still pointing to the one we created, restore it to the previous value
					if ( GMemoryArchive == PropagationArchive )
					{
						GMemoryArchive = OldMemoryArchive;
					}

					// clean up the FArchetypePropagationArc we created
					delete PropagationArchive;
					PropagationArchive = NULL;
				}

			}
			break;

		case PB_Delete:
			{
				TArray<BYTE*> Addresses;
				if ( ParentNode->GetReadAddress( ParentNode, ParentNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses ) )
				{
					// Create an FArchetypePropagationArc to propagate the updated property values from archetypes to instances of that archetype
					FReloadObjectArc* OldMemoryArchive = GMemoryArchive;
					FArchetypePropagationArc* PropagationArchive;
					GMemoryArchive = PropagationArchive = new FArchetypePropagationArc();

					ParentWindow->NotifyPreChange( NodeProperty );

					// perform the operation on the array for all selected objects
					for ( INT i = 0 ; i < Addresses.Num() ; ++i )
					{
						FScriptArray* Addr = (FScriptArray*)Addresses(i);
						Addr->Remove( PropertyTreeNode->GetArrayIndex(), 1, NodeProperty->GetSize() );
					}

					// now change the propagation archive to read mode
					PropagationArchive->ActivateReader();

					const UBOOL bExpand = TRUE;
					const UBOOL bRecurse = FALSE;
					ParentNode->SetExpanded(bExpand, bRecurse);
					const UBOOL bTopologyChange = TRUE;
					FPropertyChangedEvent ChangeEvent(ParentNode->GetProperty(), bTopologyChange);
					ParentWindow->NotifyPostChange( ChangeEvent );

					// if GMemoryArchive is still pointing to the one we created, restore it to the previous value
					if ( GMemoryArchive == PropagationArchive )
					{
						GMemoryArchive = OldMemoryArchive;
					}

					// clean up the FArchetypePropagationArc we created
					delete PropagationArchive;
					PropagationArchive = NULL;
				}
			}
			break;

		case PB_Duplicate:
			{
				TArray<BYTE*> Addresses;
				FScriptArray* Addr = NULL;
				if ( ParentNode->GetReadAddress( ParentNode, ParentNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses ) )
				{
					Addr = (FScriptArray*)Addresses(0);
				}

				if( Addr )
				{
					// Create an FArchetypePropagationArc to propagate the updated property values from archetypes to instances of that archetype
					FReloadObjectArc* OldMemoryArchive = GMemoryArchive;
					FArchetypePropagationArc* PropagationArchive;
					GMemoryArchive = PropagationArchive = new FArchetypePropagationArc();

					ParentWindow->NotifyPreChange( ParentNode->GetProperty() );

					// now change the propagation archive to read mode
					PropagationArchive->ActivateReader();

					Addr->Insert( PropertyTreeNode->GetArrayIndex(), 1, NodeProperty->GetSize() );	
					BYTE* Dest = (BYTE*)Addr->GetData() + PropertyTreeNode->GetArrayIndex()*NodeProperty->GetSize();
					appMemzero( Dest, NodeProperty->GetSize() );

					// Copy the selected item's value to the new item.
					// Find the object that owns the array and pass it to the SubObjectRoot and DestObject arguments
					// of CopyCompleteValue so that the instanced objects will be correctly duplicated.
					FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();
					UObject* ArrayOwner = NULL;
					for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor && !ArrayOwner ; ++Itor )
					{
						ArrayOwner = *Itor;
					}

					FObjectInstancingGraph InstanceGraph( ArrayOwner );
					NodeProperty->CopyCompleteValue(Dest, (BYTE*)Addr->GetData() + (PropertyTreeNode->GetArrayIndex() + 1) * NodeProperty->GetSize(), ArrayOwner, ArrayOwner, &InstanceGraph);
					
					// get a list of the objects that were instanced during the CCV
					TArray<UObject*> ObjectInstances;
					InstanceGraph.RetrieveObjectInstances(ArrayOwner, ObjectInstances, TRUE );

					// now make sure that the any objects that were created during the copying (copying a struct with an object 
					// pointer could duplicate the object, and then use this original as the archetype, instead of the 
					// original's archetype)
					for (INT InstanceIndex = 0; InstanceIndex < ObjectInstances.Num(); InstanceIndex++)
					{
						UObject* InstancedObject = ObjectInstances(InstanceIndex);
						UObject* OriginalArchetype = InstancedObject->GetArchetype()->GetArchetype();
						InstancedObject->SetArchetype( OriginalArchetype );
					}

					const UBOOL bExpand = TRUE;
					const UBOOL bRecurse = FALSE;
					ParentNode->SetExpanded(bExpand, bRecurse);
					const UBOOL bTopologyChange = TRUE;
					FPropertyChangedEvent ChangeEvent(ParentNode->GetProperty(), bTopologyChange, EPropertyChangeType::ValueSet);
					ParentWindow->NotifyPostChange( ChangeEvent );

					// if GMemoryArchive is still pointing to the one we created, restore it to the previous value
					if ( GMemoryArchive == PropagationArchive )
					{
						GMemoryArchive = OldMemoryArchive;
					}

					// clean up the FArchetypePropagationArc we created
					delete PropagationArchive;
					PropagationArchive = NULL;
				}

			}
			break;

		default:
			Super::ButtonClick( InItem, InButton );
			break;
	}
}

/*-----------------------------------------------------------------------------
	UPropertyInputBool
-----------------------------------------------------------------------------*/

// Determines if this input proxy supports the specified UProperty.
UBOOL UPropertyInputBool::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	if (InTreeNode->IsEditConst())
	{
		return FALSE;
	}

	return ( ConstCast<UBoolProperty>(InTreeNode->GetProperty()) != NULL );
}

// Allows a customized response to a user double click on the item in the property window.
void UPropertyInputBool::DoubleClick(WxPropertyControl* InItem, const TArray<FObjectBaseAddress>& InObjects, const TCHAR* InValue, UBOOL bForceValue)
{
	if ( InObjects.Num() == 0 )
	{
		return;
	}

	WxPropertyWindow* MainPropertyWindow = InItem->GetPropertyWindow();
	if ( MainPropertyWindow && MainPropertyWindow->HasFlags(EPropertyWindowFlags::ReadOnly) )
	{
		return;
	}

	// Assemble a list of target values.
	TArray<FString> Results;
	const FString ForceResult( InValue );

	for ( INT ObjectIndex = 0 ; ObjectIndex < InObjects.Num() ; ++ObjectIndex )
	{	
		if( bForceValue )
		{
			Results.AddItem( ForceResult );
		}
		else
		{
			const FObjectBaseAddress& Cur = InObjects( ObjectIndex );
			FString TempResult;

			FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
			check(PropertyTreeNode);
			UProperty* NodeProperty = PropertyTreeNode->GetProperty();

			NodeProperty->ExportTextItem( TempResult, Cur.BaseAddress, NULL, NULL, 0 );

			if( TempResult == TEXT("True") || TempResult == GTrue )
			{
				TempResult = TEXT("False");
			}
			else
			{
				TempResult = TEXT("True");
			}
			Results.AddItem( TempResult );
		}
	}

	ImportText( InItem, InObjects, Results );
}

// Allows special actions on left mouse clicks.  Return TRUE to stop normal processing.
UBOOL UPropertyInputBool::LeftClick( WxPropertyControl* InItem, INT InX, INT InY )
{
	WxPropertyWindow* MainPropertyWindow = InItem->GetPropertyWindow();
	if (MainPropertyWindow && MainPropertyWindow->HasFlags(EPropertyWindowFlags::ReadOnly))
		return TRUE;

	// If clicking on top of the checkbox graphic, toggle it's value and return.
	InX -= MainPropertyWindow->GetSplitterPos();

	if( InX > 0 && InX < GPropertyWindowManager->CheckBoxOnB.GetWidth() )
	{
		FString Value = InItem->GetPropertyText();
		UBOOL bForceValue = 0;
		if( !Value.Len() )
		{
			Value = GTrue;
			bForceValue = 1;
		}

		TArray<FObjectBaseAddress> ObjectsToModify;

		FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
		FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();
		for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
		{
			UObject*	Object = *Itor;
			BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );
			ObjectsToModify.AddItem( FObjectBaseAddress( Object, Addr ) );
		}

		DoubleClick( InItem, ObjectsToModify, *Value, bForceValue );
		//DoubleClick( InItem, Object, InItem->GetValueBaseAddress( (BYTE*) Object ), *Value, bForceValue );

		InItem->Refresh();

		return 1;
	}

	return 0;
}

/*-----------------------------------------------------------------------------
	UPropertyInputColor
-----------------------------------------------------------------------------*/

/**
* Generates a text string based from the color passed in, based on the color type of the property passed in.
*
* @param InItem	Property that we are basing our string type off of.
* @param InColor	Color to use to generate the string.
* @param OutString Output for generated string.
*/
void UPropertyInputColor::GenerateColorString( const WxPropertyControl* InItem, const FColor &InColor, FString& OutString )
{
	const FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	check(PropertyTreeNode);
	const UProperty* NodeProperty = PropertyTreeNode->GetProperty();
	if( ConstCast<UStructProperty>(NodeProperty,CLASS_IsAUStructProperty)->Struct->GetFName() == NAME_Color )
	{
		OutString = FString::Printf( TEXT("(R=%i,G=%i,B=%i)"), (INT)InColor.R, (INT)InColor.G, (INT)InColor.B );
	}
	else
	{
		check( ConstCast<UStructProperty>(NodeProperty,CLASS_IsAUStructProperty)->Struct->GetFName() == NAME_LinearColor );
		const FLinearColor lc( InColor );
		OutString = FString::Printf( TEXT("(R=%f,G=%f,B=%f)"), lc.R, lc.G, lc.B );
	}
}

// Determines if this input proxy supports the specified UProperty.
UBOOL UPropertyInputColor::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	if (InTreeNode->IsEditConst())
	{
		return FALSE;
	}

	const UProperty* NodeProperty = InTreeNode->GetProperty();
	return ( ConstCast<UStructProperty>(NodeProperty,CLASS_IsAUStructProperty) && (ConstCast<UStructProperty>(NodeProperty,CLASS_IsAUStructProperty)->Struct->GetFName()==NAME_Color || ConstCast<UStructProperty>(NodeProperty,CLASS_IsAUStructProperty)->Struct->GetFName()==NAME_LinearColor) );
}


// Allows special actions on left mouse clicks.  Return TRUE to stop normal processing.
UBOOL UPropertyInputColor::LeftUnclick( WxPropertyControl* InItem, INT InX, INT InY )
{
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	check(PropertyTreeNode);
	UProperty* NodeProperty = PropertyTreeNode->GetProperty();
	WxPropertyWindow* MainPropertyWindow = InItem->GetPropertyWindow();
	check(MainPropertyWindow);

	if (MainPropertyWindow->HasFlags(EPropertyWindowFlags::ReadOnly))
	{
		return TRUE;
	}

	// If clicking on top of the checkbox graphic, toggle it's value and return.
	InX -= MainPropertyWindow->GetSplitterPos();
	if( InX > 0 )
	{
		//default to a dword color
		UBOOL bIsLinearColor = FALSE;

		//what type of color node is this
		if(ConstCast<UStructProperty>(NodeProperty,CLASS_IsAUStructProperty)->Struct->GetFName() == NAME_LinearColor)
		{
			bIsLinearColor = TRUE;
		}
		else
		{
			check( ConstCast<UStructProperty>(NodeProperty,CLASS_IsAUStructProperty)->Struct->GetFName() == NAME_Color );
		}

		// Get the parent object before opening the picker dialog, in case the dialog deselects.
		FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();

		wxWindow* TopWindow = MainPropertyWindow->GetParent();
		//make sure internal callbacks don't rebuild the window
		//MainPropertyWindow->ChangeActiveCallbackCount(1);

		FPickColorStruct PickColorStruct;
		PickColorStruct.PropertyWindow = MainPropertyWindow;
		PickColorStruct.PropertyNode = PropertyTreeNode;
		PickColorStruct.RefreshWindows.AddItem(MainPropertyWindow);
		PickColorStruct.bSendEventsOnlyOnMouseUp = TRUE;
		for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
		{
			PickColorStruct.ParentObjects.AddItem(*Itor);
			if (bIsLinearColor)
			{
				PickColorStruct.FLOATColorArray.AddItem((FLinearColor*)(PropertyTreeNode->GetValueBaseAddress((BYTE*)(*Itor))));
			}
			else
			{
				PickColorStruct.DWORDColorArray.AddItem((FColor*)(PropertyTreeNode->GetValueBaseAddress((BYTE*)(*Itor))));
			}
		}

		//modeless, just spawns the window
		PickColor(PickColorStruct);

		return 1;
	}

	return 0;
}

/*-----------------------------------------------------------------------------
	UPropertyInputArray
-----------------------------------------------------------------------------*/

// Determines if this input proxy supports the specified UProperty.
UBOOL UPropertyInputArray::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	if (InTreeNode->IsEditConst())
	{
		return FALSE;
	}
	const UProperty* NodeProperty = InTreeNode->GetProperty();


	return ( ( NodeProperty->ArrayDim != 1 && InArrayIdx == -1 ) || ConstCast<UArrayProperty>(NodeProperty) );
}

// Handles button clicks from within the item.
void UPropertyInputArray::ButtonClick( WxPropertyControl* InItem, ePropButton InButton )
{
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	check(PropertyTreeNode);
	UProperty* NodeProperty = PropertyTreeNode->GetProperty();

	switch( InButton )
	{
		case PB_Empty:
		{
			FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();

			TArray<BYTE*> Addresses;
			const UBOOL bSuccess = PropertyTreeNode->GetReadAddress( PropertyTreeNode, PropertyTreeNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses,
															TRUE, //bComparePropertyContents
															FALSE, //bObjectForceCompare
															TRUE ); //bArrayPropertiesCanDifferInSize
			if ( bSuccess )
			{
				// determines whether we actually changed any values (if the user clicks the "emtpy" button when the array is already empty,
				// we don't want the objects to be marked dirty)
				UBOOL bNotifiedPreChange = FALSE;

				// these archives are used to propagate property value changes from archetypes to instances of that archetype
				FReloadObjectArc* OldMemoryArchive = GMemoryArchive;
				FArchetypePropagationArc* PropagationArchive = NULL;

				UArrayProperty* Array = CastChecked<UArrayProperty>( NodeProperty );
				for ( INT i = 0 ; i < Addresses.Num() ; ++i )
				{
					BYTE* Addr = Addresses(i);
					if( Addr )
					{
						if ( !bNotifiedPreChange )
						{
							bNotifiedPreChange = TRUE;

							// Create an FArchetypePropagationArc to propagate the updated property values from archetypes to instances of that archetype
							GMemoryArchive = PropagationArchive = new FArchetypePropagationArc();

							// send the PreEditChange notification to all selected objects
							InItem->NotifyPreChange( NodeProperty );
						}

						((FScriptArray*)Addr)->Empty( 0, Array->Inner->ElementSize );
					}
				}

				if ( bNotifiedPreChange )
				{

					const UBOOL bExpand = TRUE;
					const UBOOL bRecurse  = FALSE;
					PropertyTreeNode->SetExpanded(bExpand, bRecurse);

					// now change the propagation archive to read mode
					PropagationArchive->ActivateReader();

					// send the PostEditChange notification; it will be propagated to all selected objects
					const UBOOL bTopologyChange = TRUE;
					FPropertyChangedEvent ChangeEvent(NodeProperty, bTopologyChange, EPropertyChangeType::ValueSet);
					InItem->NotifyPostChange( ChangeEvent );
				}

				// if GMemoryArchive is still pointing to the one we created, restore it to the previous value
				if ( GMemoryArchive == PropagationArchive )
				{
					GMemoryArchive = OldMemoryArchive;
				}

				// clean up the FArchetypePropagationArc we created
				delete PropagationArchive;
				PropagationArchive = NULL;
			}
		}
		break;

		case PB_Add:
		{
			FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();

			TArray<BYTE*> Addresses;
			const UBOOL bSuccess = PropertyTreeNode->GetReadAddress( PropertyTreeNode, PropertyTreeNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses, TRUE, FALSE, TRUE );
			if ( bSuccess )
			{
				// determines whether we actually changed any values (if the user clicks the "emtpy" button when the array is already empty,
				// we don't want the objects to be marked dirty)
				UBOOL bNotifiedPreChange = FALSE;

				// these archives are used to propagate property value changes from archetypes to instances of that archetype
				FReloadObjectArc* OldMemoryArchive = GMemoryArchive;
				FArchetypePropagationArc* PropagationArchive = NULL;

				TArray< TMap<FString,INT> > ArrayIndicesPerObject;

				UArrayProperty* Array = CastChecked<UArrayProperty>( NodeProperty );
				for ( INT i = 0 ; i < Addresses.Num() ; ++i )
				{
					BYTE* Addr = Addresses(i);
					if( Addr )
					{
						if ( !bNotifiedPreChange )
						{
							bNotifiedPreChange = TRUE;

							// Create an FArchetypePropagationArc to propagate the updated property values from archetypes to instances of that archetype
							GMemoryArchive = PropagationArchive = new FArchetypePropagationArc();
							GMemoryArchive->ActivateWriter();

							// send the PreEditChange notification to all selected objects
							InItem->NotifyPreChange( NodeProperty );
						}

						//add on array index so we can tell which entry just changed
						ArrayIndicesPerObject.AddItem(TMap<FString,INT>());
						GenerateArrayIndexMapToObjectNode(ArrayIndicesPerObject(i), PropertyTreeNode);

						const INT ArrayIndex = ((FScriptArray*)Addr)->AddZeroed( 1, Array->Inner->ElementSize );
						ArrayIndicesPerObject(i).Set(NodeProperty->GetName(), ArrayIndex);

						// Apply struct defaults.
						UStructProperty* StructProperty = Cast<UStructProperty>(Array->Inner,CLASS_IsAUStructProperty);
						if( StructProperty && StructProperty->Struct->GetDefaultsCount() && StructProperty->HasValue(StructProperty->Struct->GetDefaults()) )
						{
							check( Align(StructProperty->Struct->GetDefaultsCount(),StructProperty->Struct->MinAlignment) == Array->Inner->ElementSize );
							BYTE* Dest = (BYTE*)((FScriptArray*)Addr)->GetData() + ArrayIndex * Array->Inner->ElementSize;
							StructProperty->CopySingleValue( Dest, StructProperty->Struct->GetDefaults() );
						}
					}
				}

				if ( bNotifiedPreChange )
				{

					const UBOOL bExpand   = TRUE;
					const UBOOL bRecurse  = FALSE;
					PropertyTreeNode->SetExpanded(bExpand, bRecurse);					// re-expand the property items corresponding to the array elements

					// now change the propagation archive to read mode
					PropagationArchive->ActivateReader();

					// send the PostEditChange notification; it will be propagated to all selected objects
					const UBOOL bTopologyChange = TRUE;
					FPropertyChangedEvent ChangeEvent(NodeProperty, bTopologyChange, EPropertyChangeType::ArrayAdd);
					ChangeEvent.SetArrayIndexPerObject(ArrayIndicesPerObject);
					InItem->NotifyPostChange( ChangeEvent );
				}

				// if GMemoryArchive is still pointing to the one we created, restore it to the previous value
				if ( GMemoryArchive == PropagationArchive )
				{
					GMemoryArchive = OldMemoryArchive;
				}

				// clean up the FArchetypePropagationArc we created
				delete PropagationArchive;
				PropagationArchive = NULL;
			}
		}
		break;
		case PB_Use:
		{
			//must be an array of actors
			FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();
			TArray<BYTE*> Addresses;
			const UBOOL bSuccess = PropertyTreeNode->GetReadAddress( PropertyTreeNode, PropertyTreeNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses, TRUE, FALSE, TRUE );
			if ( bSuccess )
			{
				// determines whether we actually changed any values (if the user clicks the "emtpy" button when the array is already empty,
				// we don't want the objects to be marked dirty)
				UBOOL bNotifiedPreChange = FALSE;

				// send the PreEditChange notification to all selected objects
				InItem->NotifyPreChange( NodeProperty );

				// If an object of that type is selected, use it.
				UArrayProperty* Array = CastChecked<UArrayProperty>( NodeProperty );
				for ( INT i = 0 ; i < Addresses.Num() ; ++i )
				{
					TArray<AActor*> CurrentListOfActors;

					FScriptArray* Addr = (FScriptArray*)Addresses(i);
					check(Addr);
					//Get the guts
					AActor** InternalArray = (AActor**)(Addr->GetData());
					if (InternalArray)
					{
						check(InternalArray);
						for (INT CurrentActorIndex = 0; CurrentActorIndex < Addr->Num(); ++CurrentActorIndex)
						{
							AActor* CurrentActor = InternalArray[CurrentActorIndex];
							CurrentListOfActors.AddUniqueItem(CurrentActor);
						}
					}
					
					//Assemble List of selected actors
					USelection* SelectedSet = GEditor->GetSelectedSet( AActor::StaticClass() );
					for ( USelection::TObjectIterator It( SelectedSet->ObjectItor() ) ; It ; ++It )
					{
						UObject* Object = *It;
						check(Object);
						AActor* NewActor = Cast<AActor>(Object);
						check(NewActor);
						CurrentListOfActors.AddUniqueItem(NewActor);
					}

					//clear the array
					Addr->Empty(Addr->Num(), sizeof(AActor*));
					//now re-add all entries back to the array
					Addr->Add( CurrentListOfActors.Num(), sizeof(AActor*));
					//Get the new guts
					InternalArray = (AActor**)(Addr->GetData());
					if (InternalArray)
					{
						for (INT NewArrayIndex = 0; NewArrayIndex < CurrentListOfActors.Num(); ++NewArrayIndex)
						{
							InternalArray[NewArrayIndex] = CurrentListOfActors(NewArrayIndex);
						}
					}
				}

				//force expansion
				const UBOOL bExpand   = TRUE;
				const UBOOL bRecurse  = FALSE;
				PropertyTreeNode->SetExpanded(bExpand, bRecurse);
				// send the PostEditChange notification; it will be propagated to all selected objects
				const UBOOL bTopologyChange = TRUE;
				FPropertyChangedEvent ChangeEvent(NodeProperty, bTopologyChange, EPropertyChangeType::ArrayAdd);
				//ChangeEvent.SetArrayIndexPerObject(ArrayIndicesPerObject);
				InItem->NotifyPostChange( ChangeEvent );
			}
		}
		break;

		default:
			Super::ButtonClick( InItem, InButton );
			break;
	}
}

/*-----------------------------------------------------------------------------
	UPropertyInputArrayItem
-----------------------------------------------------------------------------*/

// Determines if this input proxy supports the specified UProperty.
UBOOL UPropertyInputArrayItem::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	const WxPropertyControl* PropertyWindowBase = InTreeNode->GetNodeWindow();
	if (InTreeNode->IsEditConst())
	{
		return FALSE;
	}

	const UProperty* NodeProperty = InTreeNode->GetProperty();

	return !ConstCast<UClassProperty>( NodeProperty ) &&
			ConstCast<UArrayProperty>( NodeProperty->GetOuter() ) &&
			InTreeNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly) &&
			!(ConstCast<UArrayProperty>(NodeProperty->GetOuter())->PropertyFlags & CPF_EditConst);
}

// Handles button clicks from within the item.
void UPropertyInputArrayItem::ButtonClick( WxPropertyControl* InItem, ePropButton InButton )
{
	switch( InButton )
	{
		case PB_Browse:
		{
			if ( GUnrealEd )
			{
#if WITH_MANAGED_CODE
				WxContentBrowserHost* ContentBrowser = NULL;
				if ( FContentBrowser::IsInitialized() )
				{
					ContentBrowser = GUnrealEd->GetBrowser<WxContentBrowserHost>(TEXT("ContentBrowser"));
					if ( ContentBrowser != NULL )
					{
						GUnrealEd->GetBrowserManager()->ShowWindow(ContentBrowser->GetDockID(), TRUE);
					}
				}
#endif
			}
			break;
		}

		case PB_Clear:
			SendTextToObjects( InItem, TEXT("None") );
			InItem->Refresh();
			break;

		case PB_Use:
			OnUseSelected( InItem );
			break;

		default:
			Super::ButtonClick( InItem, InButton );
			break;
	}
}

/*-----------------------------------------------------------------------------
 WxPropertyPanel
-----------------------------------------------------------------------------*/

/**
 * This is a overloaded wxPanel class that passes certain events back up to the wxPropertyWindow_Base parent class.
 */
class WxPropertyPanel : public wxPanel
{
public:
	WxPropertyPanel(WxPropertyControl* InParent);

protected:
	/** Keypress event handler, just routes the event back up to the parent. */
	void OnChar(wxKeyEvent &Event);

	/** Pointer to the parent property window that created this class. */
	WxPropertyControl* Parent;

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(WxPropertyPanel, wxPanel)
	EVT_CHAR(WxPropertyPanel::OnChar)
END_EVENT_TABLE()

WxPropertyPanel::WxPropertyPanel(WxPropertyControl* InParent) : 
wxPanel(InParent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER | wxWANTS_CHARS),
Parent(InParent)
{
	
}

/** Keypress event handler, just routes the event back up to the parent. */
void WxPropertyPanel::OnChar(wxKeyEvent &Event)
{
	// Make sure this event gets propagated up to the parent.  We need to use the add pending event function
	// because the parent may destroy this panel in its event handler.  This way the event will be processed
	// later on, when this class can safely be deleted.
	Event.SetId(Parent->GetId());
	Parent->GetEventHandler()->AddPendingEvent(Event);
}

/*-----------------------------------------------------------------------------
	UPropertyInputText::WxPropertyItemTextCtrl
-----------------------------------------------------------------------------*/

IMPLEMENT_DYNAMIC_CLASS(UPropertyInputText::WxPropertyItemTextCtrl, WxTextCtrl);

BEGIN_EVENT_TABLE(UPropertyInputText::WxPropertyItemTextCtrl, WxTextCtrl)
EVT_SET_FOCUS(UPropertyInputText::WxPropertyItemTextCtrl::OnChangeFocus)
EVT_KILL_FOCUS(UPropertyInputText::WxPropertyItemTextCtrl::OnChangeFocus)
EVT_LEFT_DOWN(UPropertyInputText::WxPropertyItemTextCtrl::OnLeftMouseDown)
EVT_ENTER_WINDOW(UPropertyInputText::WxPropertyItemTextCtrl::OnMouseEnter)
EVT_KEY_DOWN(UPropertyInputText::WxPropertyItemTextCtrl::OnChar)
END_EVENT_TABLE()

/**
 * Constructor.
 */
UPropertyInputText::WxPropertyItemTextCtrl::WxPropertyItemTextCtrl()
{

}

/**
 * Constructor.
 */
UPropertyInputText::WxPropertyItemTextCtrl::WxPropertyItemTextCtrl( wxWindow* parent,
					   wxWindowID id,
					   const wxString& value,
					   const wxPoint& pos,
					   const wxSize& size,
					   long style)
					   : WxTextCtrl(parent, id, value, pos, size, style)
{
	wxEvtHandler::Connect(wxID_ANY, ID_SELECT_ALL_TEXT, wxCommandEventHandler(UPropertyInputText::WxPropertyItemTextCtrl::OnSelectAllText));
}

/**
 * Event handler for the focus event. This function maintains proper selection state within the parent property window.
 *
 * @param	Event	Information about the event.
 */
void UPropertyInputText::WxPropertyItemTextCtrl::OnChangeFocus(wxFocusEvent &Event)
{
	WxPropertyControl *ParentItem = (WxPropertyControl*)GetGrandParent();
	WxPropertyWindow *MainWindow = ParentItem->GetPropertyWindow();
	check(MainWindow);
	WxPropertyWindowHost* HostWindow = MainWindow->GetParentHostWindow();
	check(HostWindow);

	MainWindow->Freeze();
	if (Event.GetEventType() == wxEVT_SET_FOCUS)
	{
		SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

		// We've received focus, so make sure all text gets selected
		wxCommandEvent TxtEvent;
		TxtEvent.SetEventObject(this);
		TxtEvent.SetEventType(ID_SELECT_ALL_TEXT);
		wxEvtHandler::AddPendingEvent(TxtEvent);

		HostWindow->ClearActiveFocus();
		HostWindow->SetActiveFocus(ParentItem, TRUE);

		MainWindow->SetLastFocused(ParentItem);
	}
	else if (Event.GetEventType() == wxEVT_KILL_FOCUS)
	{
		check(ParentItem);
		ParentItem->InputProxy->SendToObjects( ParentItem );
		ParentItem->InputProxy->OnKillFocus(ParentItem);

		SetSelection(0, 0);
	}
	// Set this node as being in focus.
	MainWindow->Thaw();
	MainWindow->Refresh();
}

/**
* Event handler for the kill focus event. This function brings the cursor back to index 0 within the text control.
*
* @param	Event	Information about the event.
*/
void UPropertyInputText::WxPropertyItemTextCtrl::OnKillFocus(wxFocusEvent& Event)
{
	SetSelection(0, 0);
}

/**
 * Event handler for the left mouse button down event. This handler is needed because when the text control is clicked and it doesn't have focus it cannot
 * select all of the text in the control due to the fact that the control thinks it is in the middle of a drag/selection operation. This only applies when
 * all property item buttons are being rendered because that's the only time when all text control's are created and visible.
 */
void UPropertyInputText::WxPropertyItemTextCtrl::OnLeftMouseDown(wxMouseEvent &Event)
{
	//If all buttons are being rendered that means the text control already existed
	if(wxWindow::FindFocus() != this)
	{
		//so me must not let the base class handle the event because it'll think we're in the middle of a drag/selection operation
		Event.Skip(false);

		//instead set focus ourselves and prevent any further operations
		SetFocus();
	}
	else
	{
		//otherwise just let things continue on as normal
		Event.Skip(true);
	}
}

/**
 * Event handler for the select all text event.
 *
 * @param	Event	Information about the event.
 */
void UPropertyInputText::WxPropertyItemTextCtrl::OnSelectAllText(wxCommandEvent &Event)
{
	SetSelection(-1, -1);
}

/**
 * This event handler sets the default cursor when the mouse cursor enters the text box. This prevents the resize cursor from being permanent until you move the mouse outside of the text box.
 *
 * @param	Event	Information about the event.
 */
void UPropertyInputText::WxPropertyItemTextCtrl::OnMouseEnter(wxMouseEvent &Event)
{
	SetCursor(wxCursor(wxCURSOR_DEFAULT));
}

/**
 * Handles key down events.
 *
 * @param	Event	Information about the event.
 */
void UPropertyInputText::WxPropertyItemTextCtrl::OnChar( wxKeyEvent& In )
{
	UBOOL bHandled(FALSE);
	// Since the text box may grow as the user types we need to check on every
	// key down event.  To save time only do the refresh if it's a multiline box.
	WxPropertyControl *ParentItem = (WxPropertyControl*)GetGrandParent();
	if(ParentItem)
	{
		UPropertyInputText* InputProperty = static_cast<UPropertyInputText*>(ParentItem->InputProxy);
		if(InputProperty && InputProperty->MaxRowCount != 0)
		{
			bHandled = TRUE;

			// If this is a multi-line box we need to handle return specially because we don't want wx to add a newline
			// to the string in the textbox.
			if(In.GetRawKeyCode() == WXK_RETURN)
			{
				WxItemPropertyControl* ControlInst = wxDynamicCast(ParentItem, WxItemPropertyControl );

				wxCommandEvent CommandEvent;

				ControlInst->OnInputTextEnter(CommandEvent);
			}
			else
			{
				In.Skip();
			}

			WxPropertyWindow *MainWindow = ParentItem->GetPropertyWindow();
			check(MainWindow);

			MainWindow->RefreshEntireTree();
		}
	}

	if(!bHandled)
	{
		In.Skip();
	}
}

/*-----------------------------------------------------------------------------
UPropertyInputText
-----------------------------------------------------------------------------*/

/**
* Clears any selection in the text control.
*/
void UPropertyInputText::ClearTextSelection()
{
	if ( TextCtrl )
	{
		TextCtrl->SetSelection( -1, -1 );
	}
}

// Determines if this input proxy supports the specified UProperty.
UBOOL UPropertyInputText::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	if (InTreeNode->IsEditConst())
	{
		return FALSE;
	}
	const UProperty* NodeProperty = InTreeNode->GetProperty();
	const WxPropertyControl* PropertyWindowBase = InTreeNode->GetNodeWindow();

	if(	!InTreeNode->HasNodeFlags(EPropertyNodeFlags::EditInline)
			&&	( (NodeProperty->IsA(UNameProperty::StaticClass()) && NodeProperty->GetFName() != NAME_InitialState)
			||	NodeProperty->IsA(UStrProperty::StaticClass())
			||	NodeProperty->IsA(UObjectProperty::StaticClass()) 
			||	NodeProperty->IsA(UInterfaceProperty::StaticClass()))
			&&	!NodeProperty->IsA(UComponentProperty::StaticClass()) )
	{
		return TRUE;
	}

	return FALSE;
}

// Allows the created controls to react to the parent window being resized.  Derived classes
// that implement this method should to call up to their parent.
void UPropertyInputText::CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue )
{
	PropertyPanel = new WxPropertyPanel(InItem);

	// Create any buttons for the control.
	Super::CreateControls( InItem, InBaseClass, InRC, InValue );

	UProperty* PropertyNode = InItem->GetProperty();
	MaxRowCount = 0;
	if(PropertyNode->HasMetaData(TEXT("MultilineWithMaxRows")))
	{
		MaxRowCount = appAtoi(*PropertyNode->GetMetaData(TEXT("MultilineWithMaxRows")));
	}

	PropertySizer = new wxBoxSizer(wxHORIZONTAL);
	{
		// NOTE: We specify wxTE_PROCESS_ENTER and wxTE_PROCESS_TAB so that the edit control will receive
		//    WM_CHAR messages for VK_RETURN and VK_TAB.  This is so that we can use these keys for navigation
		//    between properties, etc.  Without specifying these flags, property windows embedding inside of
		//    dialog frames would not receive WM_CHAR messages for these keys.
		DWORD WindowStyle = wxNO_BORDER | wxTE_PROCESS_TAB | (MaxRowCount != 0 ? wxTE_MULTILINE : 0);
		WxPropertyWindow *MainWindow = InItem->GetPropertyWindow();
		if( MainWindow != NULL && MainWindow->HasFlags(EPropertyWindowFlags::AllFlags))
		{
			WindowStyle |= wxTE_PROCESS_ENTER;
		}
		TextCtrl = new WxPropertyItemTextCtrl(PropertyPanel, ID_PROPWINDOW_EDIT, InValue, wxDefaultPosition, wxDefaultSize, WindowStyle);
		TextCtrl->SetSelection(0,0);
		TextCtrl->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));
		PropertySizer->Add(TextCtrl,1, wxEXPAND|wxALL, 0);
	}
	PropertyPanel->SetSizer(PropertySizer);
	PropertyPanel->SetAutoLayout(true);

	ParentResized( InItem, InRC );
}

// Deletes any controls which were created for editing.
void UPropertyInputText::DeleteControls()
{
	// Delete any buttons on the control.
	Super::DeleteControls();

	if( PropertyPanel )
	{
		//Save off pointer to window instead of just deleting it, because it was causing set focus events to get called and therefore using the destroyed panel
		wxPanel* OldPropertyPanel = PropertyPanel;

		PropertyPanel = NULL;
		TextCtrl = NULL;

		OldPropertyPanel->Destroy();

	}
}

// Sends the current value in the input control to the selected objects.
void UPropertyInputText::SendToObjects( WxPropertyControl* InItem )
{
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	check(PropertyTreeNode);
	UProperty* NodeProperty = PropertyTreeNode->GetProperty();

	if( !TextCtrl )
	{
		return;
	}

	FString Value = (const TCHAR*)TextCtrl->GetValue();

	// Strip any leading underscores and spaces from names.
	if( NodeProperty->IsA( UNameProperty::StaticClass() ) )
	{
		while ( true )
		{
			if ( Value.StartsWith( TEXT("_") ) )
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("NamesCantBeingWithUnderscore") );
				// Strip leading underscores.
				do
				{
					Value = Value.Right( Value.Len()-1 );
				} while ( Value.StartsWith( TEXT("_") ) );
			}
			else if ( Value.StartsWith( TEXT(" ") ) )
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("NamesCantBeingWithSpace") );
				// Strip leading spaces.
				do
				{
					Value = Value.Right( Value.Len()-1 );
				} while ( Value.StartsWith( TEXT(" ") ) );
			}
			else
			{
				// Starting with something valid -- break.
				break;
			}
		}

		// Ensure the name is enclosed with quotes.
		if(Value.Len())
		{
			Value = FString::Printf(TEXT("\"%s\""), *Value.TrimQuotes());
		}
	}

	// If more than one object is selected, an empty field indicates their values for this property differ.
	// Don't send it to the objects value in this case (if we did, they would all get set to None which isn't good).
	FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();

	if( ObjectNode->GetNumObjects() == 1 || Value.Len() )
	{
		// Build up a list of objects to modify.
		TArray<FObjectBaseAddress> ObjectsToModify;
		for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
		{
			UObject*	Object = *Itor;
			BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );
			ObjectsToModify.AddItem( FObjectBaseAddress( Object, Addr ) );
		}

		ImportText( InItem, ObjectsToModify, Value );
	}
}

// Allows the created controls to react to the parent window being resized.
void UPropertyInputText::ParentResized( WxPropertyControl* InItem, const wxRect& InRC )
{
	Super::ParentResized( InItem, InRC );

	if( PropertyPanel )
	{
		PropertyPanel->SetSize( InRC.x, 1, InRC.GetWidth() + 2 - (Buttons.Num() * GetButtonSize()), InRC.GetHeight() - 1);
		PropertyPanel->Layout();
	}
}

/**
 * Syncs the generic browser to the object specified by the given property.
 */
static void SyncGenericBrowser(WxPropertyControl* InItem)
{
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	check(PropertyTreeNode);
	UProperty* NodeProperty = PropertyTreeNode->GetProperty();

	UObjectProperty* ObjectProperty = Cast<UObjectProperty>( NodeProperty );
	UInterfaceProperty* IntProp = Cast<UInterfaceProperty>( NodeProperty );
	UStrProperty* StrProp = Cast<UStrProperty>(NodeProperty);
	if (ObjectProperty != NULL || IntProp != NULL || StrProp != NULL)
	{
		UClass* PropertyClass;
		if (ObjectProperty != NULL)
		{
			PropertyClass = ObjectProperty->PropertyClass;
		}
		else if (IntProp != NULL)
		{
			PropertyClass = IntProp->InterfaceClass;
		}
		else
		{
			PropertyClass = UObject::StaticClass();
		}

		// Get a list of addresses for objects handled by the property window.
		TArray<BYTE*> Addresses;
		const UBOOL bWasGetReadAddressSuccessful = PropertyTreeNode->GetReadAddress( PropertyTreeNode, PropertyTreeNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses, FALSE );
		check( bWasGetReadAddressSuccessful );

		// Create a list of object names.
		TArray<FString> ObjectNames;
		ObjectNames.Empty( Addresses.Num() );

		// Copy each object's object proper tyname off into the name list.
		for ( INT i = 0 ; i < Addresses.Num() ; ++i )
		{
			new( ObjectNames ) FString();
			NodeProperty->ExportText( 0, ObjectNames(i), Addresses(i) - NodeProperty->Offset, Addresses(i) - NodeProperty->Offset, NULL, PPF_Localized );
		}

		// Create a list of objects to sync the generic browser to.
		TArray<UObject*> Objects;
		for ( INT i = 0 ; i < ObjectNames.Num() ; ++i )
		{
			UObject* Object = UObject::StaticFindObject( PropertyClass, ANY_PACKAGE, *ObjectNames(i), FALSE );
			// if it's a string property, the object might not be loaded, so try that if it can't be found
			if (Object == NULL && StrProp != NULL)
			{
				Object = UObject::StaticLoadObject(PropertyClass, NULL, *ObjectNames(i), NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
			}
			if ( Object )
			{
				Objects.AddItem( Object );
			}
		}

		GApp->EditorFrame->SyncBrowserToObjects(Objects);
	}
}

// Handles button clicks from within the item.
void UPropertyInputText::ButtonClick( WxPropertyControl* InItem, ePropButton InButton )
{
	switch( InButton )
	{
		case PB_Browse:
			if ( GUnrealEd )
			{
				// Sync the generic browser to the object(s) specified by the given property.
				SyncGenericBrowser( InItem );
			}
			break;

		case PB_Clear:
			SendTextToObjects( InItem, TEXT("None") );
			InItem->Refresh();
			break;

		case PB_Use:
			OnUseSelected( InItem );
			break;

		default:
			Super::ButtonClick( InItem, InButton );
			break;
	}
}

// Refreshes the value in the controls created for this property.
void UPropertyInputText::RefreshControlValue( UProperty* InProperty, BYTE* InData )
{
	FString Wk;
	InProperty->ExportText( 0, Wk, InData - InProperty->Offset, InData - InProperty->Offset, NULL, PPF_Localized );

	UByteProperty* EnumProperty = Cast<UByteProperty>(InProperty);
	if ( EnumProperty != NULL && EnumProperty->Enum != NULL )
	{
		// see if we have alternate text to use for displaying the value
		UMetaData* PackageMetaData = EnumProperty->GetOutermost()->GetMetaData();
		if ( PackageMetaData )
		{
			FName AltValueName = FName(*(Wk+TEXT(".DisplayName")));
			FString ValueText = PackageMetaData->GetValue(EnumProperty->Enum, AltValueName);
			if ( ValueText.Len() > 0 )
			{
				// render the alternate text for this enum value
				Wk = ValueText;
			}
		}
	}

	if( TextCtrl )
	{
		TextCtrl->SetValue( *Wk );
	}
}

/**
* Allows a customized response to the set focus event of the property item.
* @param	InItem			The property window node.
*/
void UPropertyInputText::OnSetFocus(WxPropertyControl* InItem)
{
	if(TextCtrl)
	{
		TextCtrl->SetFocus();
	}
}

/**
* Allows a customized response to the kill focus event of the property item.
* @param	InItem			The property window node.
*/
void UPropertyInputText::OnKillFocus(WxPropertyControl* InItem)
{
	if(TextCtrl)
	{
		TextCtrl->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));
	}
}

/**
* @return	Returns the height required by the input proxy.
*/
INT UPropertyInputText::GetProxyHeight() const
{
	INT LinesInText = 1;//default to a single line
	if (TextCtrl && (MaxRowCount > 0))
	{
		LinesInText = TextCtrl->GetNumberOfLines();
	}
	INT MaxLines = Max(MaxRowCount, 1);

	INT NumDisplayableLines = Clamp(LinesInText, 1, MaxLines);
	INT ExtraSpace = (NumDisplayableLines-1)*PROP_TextLineSpacing;
	INT Height = PROP_DefaultItemHeight * NumDisplayableLines - ExtraSpace;

	return Height;
}

/**
 *	This is a spinner control specific to numeric properties.  It creates the spinner buttons and
 *  emulates a 3DSMax style spinner control.
 */ 
class WxNumericPropertySpinner : public wxPanel
{
public:
	WxNumericPropertySpinner(wxWindow* InParent, class UPropertyInputNumeric* InPropertyInput, WxPropertyControl* InItem);

	/** Event handler for mouse events. */
	void OnMouseEvent(wxMouseEvent& InEvent);

	/** Event handler for the spin buttons. */
	void OnSpin(wxCommandEvent& InEvent);

	/**
	 * Sets a fixed increment amount to use instead of 1% of the current value.
	 * @param IncrementAmount	The increment amount to use instead of defaulting to 1% of the starting value.
	 */
	void SetFixedIncrementAmount(FLOAT IncrementAmount)
	{
		FixedIncrementAmount = IncrementAmount;
		bFixedIncrement = TRUE;
	}

private:
	/**
	 * Updates the numeric property text box and sends the new value to all objects.
	 * @param NewValue New value for the property.
	 */
	void UpdatePropertyValue(FLOAT NewValue);

	/**
	 * Copies to the specified output array the property value for all objects being edited.
	 */
	void CapturePropertyValues(TArray<FLOAT>& OutValues);

	/** @return Whether or not we should be operating as if there are objects with multiple values selected. */
	UBOOL ShouldSpinMultipleValues();

	/** Parent Property Input Proxy. */
	class UPropertyInputNumeric* PropertyInput;

	/** Parent Property to send value updates to. */
	WxPropertyControl* PropertyItem;

	/** Change since mouse was captured. */
	INT MouseDelta;

	wxPoint MouseStartPoint;

	/** Starting value of the property when we capture the mouse. */
	FLOAT StartValue;

	TArray<FLOAT> StartValues;

	/** Flag for whether or not we should be using a fixed increment amount. */
	UBOOL bFixedIncrement;

	/** Fixed increment amount. */
	FLOAT FixedIncrementAmount;

	/** Since Wx has no way of hiding cursors normally, we need to create a blank cursor to use to hide our cursor. */
	wxCursor BlankCursor;

	/** TRUE if spinning a property for multiple objects, in which case spinning affects a relative change independently for each object. */
	UBOOL bSpinningMultipleValues;

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(WxNumericPropertySpinner, wxPanel)
	EVT_MOUSE_EVENTS(OnMouseEvent)
	EVT_BUTTON(IDB_SPIN_UP, WxNumericPropertySpinner::OnSpin)
	EVT_BUTTON(IDB_SPIN_DOWN, WxNumericPropertySpinner::OnSpin)
END_EVENT_TABLE()

WxNumericPropertySpinner::WxNumericPropertySpinner(wxWindow* InParent, class UPropertyInputNumeric* InPropertyInput, WxPropertyControl* InItem) : 
wxPanel(InParent),
PropertyInput(InPropertyInput),
PropertyItem(InItem),
bFixedIncrement(FALSE),
FixedIncrementAmount(1.0f)
{
	// Create spinner buttons
	wxSizer* Sizer = new wxBoxSizer(wxVERTICAL);
	{
		{
			WxMaskedBitmap bmp;
			bmp.Load(TEXT("Spinner_Up"));
			WxPropertyItemButton* SpinButton = new WxPropertyItemButton( this, IDB_SPIN_UP, bmp, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxBU_AUTODRAW);
	
			SpinButton->SetMinSize(wxSize(12,-1));
			Sizer->Add(SpinButton, 1, wxEXPAND);
		}

		Sizer->AddSpacer(4);

		{
			WxMaskedBitmap bmp;
			bmp.Load(TEXT("Spinner_Down"));
			WxPropertyItemButton* SpinButton = new WxPropertyItemButton( this, IDB_SPIN_DOWN, bmp, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxBU_AUTODRAW);

			SpinButton->SetMinSize(wxSize(12,-1));
			Sizer->Add(SpinButton, 1, wxEXPAND);
		}
	}
	SetSizer(Sizer);

	// Create a Blank Cursor
	WxMaskedBitmap bitmap(TEXT("blank"));
	wxImage BlankImage = bitmap.ConvertToImage();
	BlankImage.SetMaskColour(192,192,192);
	BlankCursor = wxCursor(BlankImage);

	// If more than one object is selected, spinning affects a relative change independently for each object.
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();
	bSpinningMultipleValues = ObjectNode->GetNumObjects() > 1;
}

/**
 * Copies to the specified output array the property value for all objects being edited.
 */
void WxNumericPropertySpinner::CapturePropertyValues(TArray<FLOAT>& OutValues)
{
	PropertyInput->GetValues(PropertyItem, OutValues);
}

/** @return Whether or not we should be operating as if there are objects with multiple values selected. */
UBOOL WxNumericPropertySpinner::ShouldSpinMultipleValues()
{
	FObjectPropertyNode* ObjectNode = PropertyItem->GetPropertyNode()->FindObjectItemParent();

	TArray<BYTE*> OutAddresses;
	const UBOOL bSpinMultiple = ObjectNode->GetReadAddress(PropertyItem->GetPropertyNode(), FALSE, OutAddresses, TRUE) == FALSE;

	return bSpinMultiple;
}

void WxNumericPropertySpinner::OnMouseEvent(wxMouseEvent& InEvent)
{
	const UBOOL bHasCapture = HasCapture();

	// See if we should be capturing the mouse.
	if(InEvent.LeftDown())
	{
		CaptureMouse();

		MouseDelta = 0;
		MouseStartPoint = InEvent.GetPosition();

		// Update whether or not we are spinning multiple values by comparing the properties values.
		bSpinningMultipleValues = ShouldSpinMultipleValues();


		if ( bSpinningMultipleValues )
		{
			// Spinning multiple values affects a relative change independently for each object.
			StartValue = 100.0f;

			// Assemble a list of starting values for objects manipulated by this property window.
			CapturePropertyValues( StartValues );
		}
		else
		{
			// Solve for the equation value and pass that value back to the property
			StartValue = PropertyInput->GetValue();
		}
		
		// Change the cursor and background color for the panel to indicate that we are dragging.
		wxSetCursor(BlankCursor);

		SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNHIGHLIGHT));
		Refresh();
	}
	else if(InEvent.LeftUp() && bHasCapture)
	{
		WarpPointer(MouseStartPoint.x, MouseStartPoint.y);
		ReleaseMouse();

		// clear the text control if we are spinning multiple values.
		if(bSpinningMultipleValues)
		{
			PropertyInput->Equation = TEXT("");
			PropertyInput->TextCtrl->SetValue(TEXT(""));
		}

		// Change the cursor back to normal and the background color of the panel back to normal.
		wxSetCursor(wxNullCursor);

		SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));
		Refresh();
	}
	else if(InEvent.Dragging() && bHasCapture)
	{
		const INT MoveDelta = InEvent.GetY() - MouseStartPoint.y;
		const INT DeltaSmoother = 3;

		// To keep the movement from being completely twitchy, we use a DeltaSmoother which means the mouse needs to move at least so many pixels before registering a change.
		if(Abs<INT>(MoveDelta) >= DeltaSmoother)
		{
			// Add contribution of delta to the total mouse delta, we need to invert it because Y is 0 at the top of the screen.
			MouseDelta += -(MoveDelta) / DeltaSmoother;

			// Solve for the equation value and pass that value back to the property, we always increment by 1% of the original value.
			FLOAT ChangeAmount;

			if(bFixedIncrement == FALSE)
			{
				ChangeAmount = Abs(StartValue) * 0.01f;
				const FLOAT SmallestChange = 0.01f;
				if(ChangeAmount < SmallestChange)
				{
					ChangeAmount = SmallestChange;
				}
			}
			else
			{
				ChangeAmount = FixedIncrementAmount;
			}

			WarpPointer(MouseStartPoint.x, MouseStartPoint.y);

			if ( bSpinningMultipleValues )
			{
				FString DeltaText;
				TArray<FLOAT> NewValues;
				if ( !bFixedIncrement )
				{
					// e.g. floats.
					const FLOAT FinalValue = ChangeAmount * MouseDelta + StartValue;
					const FLOAT FinalScale = FinalValue/100.0f;
					for ( INT i = 0 ; i < StartValues.Num() ; ++i )
					{
						NewValues.AddItem( StartValues(i) * FinalScale );
					}

					DeltaText = FString::Printf(TEXT("%.2f%c"), FinalScale*100, '%');
				}
				else
				{
					// e.g. integers or rotations.
					for ( INT i = 0 ; i < StartValues.Num() ; ++i )
					{
						NewValues.AddItem( ChangeAmount * MouseDelta + StartValues(i) );
					}

					DeltaText = FString::Printf(TEXT("%i"), appTrunc(ChangeAmount * MouseDelta) );
				}
				
				PropertyInput->TextCtrl->Freeze();
				{
					PropertyInput->SetValues( NewValues, PropertyItem );

					// Set some text to let the user know what the current is.
					PropertyInput->Equation = DeltaText;
					PropertyInput->TextCtrl->SetValue(*DeltaText);
				}
				PropertyInput->TextCtrl->Thaw();
			}
			else
			{
				// We're spinning for a single value, so just set the new value.
				const FLOAT FinalValue = ChangeAmount * MouseDelta + StartValue;
				PropertyInput->SetValue( FinalValue, PropertyItem );
			}
		}
	}
	else if(InEvent.Moving())
	{
		// Change the cursor to a NS drag cursor when the user mouses over it so that they can tell that there is a drag opportunity here.
		wxSetCursor(wxCURSOR_SIZENS);
	}
}

void WxNumericPropertySpinner::OnSpin(wxCommandEvent& InEvent)
{
	// Calculate how much to change the value by.  If there is no fixed amount set,
	// then use 1% of the current value.  Otherwise, use the fixed value that was set.

	const int WidgetId = InEvent.GetId();
	const FLOAT DirectionScale = (WidgetId == IDB_SPIN_UP) ? 1.0f : -1.0f;

	// Update whether or not we are spinning multiple values by comparing the properties values.
	bSpinningMultipleValues = ShouldSpinMultipleValues();

	if ( bFixedIncrement )
	{
		const FLOAT ChangeAmount = FixedIncrementAmount * DirectionScale;
		if ( bSpinningMultipleValues )
		{
			// Assemble a list of starting values for objects manipulated by this property window.
			CapturePropertyValues( StartValues );

			for ( INT i = 0 ; i < StartValues.Num() ; ++i )
			{
				StartValues(i) += ChangeAmount;
			}
			PropertyInput->SetValues( StartValues, PropertyItem );
			PropertyInput->Equation = TEXT("");
			PropertyInput->TextCtrl->SetValue(TEXT(""));
		}
		else
		{
			// We're spinning for a single value, so just solve for
			// the equation value and pass that value back to the property.
			const FLOAT EqResult = PropertyInput->GetValue();
			PropertyInput->TextCtrl->Freeze();
			{
				PropertyInput->SetValue( EqResult + ChangeAmount, PropertyItem );
			}
			PropertyInput->TextCtrl->Thaw();
			
		}
	}
	else
	{
		const FLOAT SmallestChange = 0.01f;

		if ( bSpinningMultipleValues )
		{
			// Assemble a list of starting values for objects manipulated by this property window.
			CapturePropertyValues( StartValues );

			for ( INT i = 0 ; i < StartValues.Num() ; ++i )
			{
				FLOAT ChangeAmount = Abs(StartValues(i)) * 0.01f;
				if( ChangeAmount < SmallestChange )
				{
					ChangeAmount = SmallestChange;
				}
				StartValues(i) += ChangeAmount * DirectionScale;
			}

			PropertyInput->SetValues( StartValues, PropertyItem );
			PropertyInput->Equation = TEXT("");
			PropertyInput->TextCtrl->SetValue(TEXT(""));
		}
		else
		{
			const FLOAT EqResult = PropertyInput->GetValue();

			FLOAT ChangeAmount = Abs(EqResult) * 0.01f;
			if( ChangeAmount < SmallestChange )
			{
				ChangeAmount = SmallestChange;
			}

			// We're spinning for a single value, so just solve for
			// the equation value and pass that value back to the property.
			PropertyInput->TextCtrl->Freeze();
			{
				PropertyInput->SetValue( EqResult + ChangeAmount*DirectionScale, PropertyItem );
			}
			PropertyInput->TextCtrl->Thaw();
		}
	}
}

/*-----------------------------------------------------------------------------
	UPropertyInputNumeric (with support for equations)
-----------------------------------------------------------------------------*/

// Determines if this input proxy supports the specified UProperty.
UBOOL UPropertyInputNumeric::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	if (InTreeNode->IsEditConst())
	{
		return FALSE;
	}

	const WxPropertyControl* PropertyWindowBase= InTreeNode->GetNodeWindow();
	check(PropertyWindowBase);
	const UProperty* NodeProperty = InTreeNode->GetProperty();

	if(	!InTreeNode->HasNodeFlags(EPropertyNodeFlags::EditInline) && NodeProperty->IsA(UFloatProperty::StaticClass()) )
	{
		return TRUE;
	}

	return FALSE;
}

// Allows the created controls to react to the parent window being resized.  Derived classes
// that implement this method should to call up to their parent.
void UPropertyInputNumeric::CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue )
{
	// Set the equation initially to be the same as the numeric value in the property.
	//if( Equation.Len() == 0 )
	//{
	Equation = InValue;
	//}

	// Create any buttons for the control.
	Super::CreateControls( InItem, InBaseClass, InRC, *Equation );

	// Add a spin button to the property sizer.
	SpinButton = new WxNumericPropertySpinner( PropertyPanel, this, InItem );
	PropertySizer->Add(SpinButton, 0, wxEXPAND | wxTOP | wxBOTTOM | wxRIGHT, 1);

	ParentResized(InItem, InRC);
}

// Sends the current value in the input control to the selected objects.
void UPropertyInputNumeric::SendToObjects( WxPropertyControl* InItem )
{
	if( !TextCtrl )
	{
		return;
	}

	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	check(PropertyTreeNode);
	FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();

	// If more than one object is selected, an empty field indicates their values for this property differ.
	// Don't send it to the objects value in this case (if we did, they would all get set to None which isn't good).
	if( ObjectNode->GetNumObjects() == 1 || Equation.Len() || TextCtrl->GetValue().Len() )
	{
		FLOAT EqResult = GetValue();

		UProperty* Property = InItem->GetProperty();
		check(Property);
		const FString& MinString = Property->GetMetaData(TEXT("ClampMin"));
		if (MinString.Len())
		{
			check(MinString.IsNumeric());
			FLOAT MinValue = appAtof(*MinString);
			EqResult = Max(MinValue, EqResult);
		}
		//Enforce max 
		const FString& MaxString = Property->GetMetaData(TEXT("ClampMax"));
		if (MaxString.Len())
		{
			check(MaxString.IsNumeric());
			FLOAT MaxValue = appAtof(*MaxString);
			EqResult = Min(MaxValue, EqResult);
		}

		const FString Value( FString::Printf( TEXT("%f"), EqResult ) );
		//Set the value back into the control, in case of clamping
		TextCtrl->SetValue(*Value);
		Equation = Value;

		// Build up a list of objects to modify.
		TArray<FObjectBaseAddress> ObjectsToModify;
		for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
		{
			UObject*	Object = *Itor;
			BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );
			ObjectsToModify.AddItem( FObjectBaseAddress( Object, Addr ) );
		}

		ImportText( InItem, ObjectsToModify, Value );
	}
}

/**
 * Refreshes the value in the controls created for this property.
 */
void UPropertyInputNumeric::RefreshControlValue(UProperty* InProperty, BYTE* InData)
{
	Super::RefreshControlValue( InProperty, InData );

	if( TextCtrl != NULL )
	{
		// Update the equation stored at this property.
		Equation = (const TCHAR*)TextCtrl->GetValue();
	}
}

/**
 * @return Returns the numeric value of the property.
 */
FLOAT UPropertyInputNumeric::GetValue()
{
	FLOAT EqResult;

	Equation = (const TCHAR*)TextCtrl->GetValue();
	Eval( Equation, &EqResult );

	return EqResult;
}

/**
 * Gets the values for each of the objects that a property item is affecting.
 * @param	InItem		Property item to get objects from.
 * @param	Values		Array of FLOATs to store values.
 */
void UPropertyInputNumeric::GetValues(WxPropertyControl* InItem, TArray<FLOAT> &Values)
{
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	check(PropertyTreeNode);
	UProperty* NodeProperty = PropertyTreeNode->GetProperty();

	FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();

	Values.Empty();

	for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
	{
		UObject*	Object = *Itor;
		BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );

		const FObjectBaseAddress Cur( Object, Addr );

		FString PreviousValue;
		NodeProperty->ExportText( 0, PreviousValue, Cur.BaseAddress - NodeProperty->Offset, Cur.BaseAddress - NodeProperty->Offset, NULL, PPF_Localized );

		FLOAT EqResult;
		Eval( PreviousValue, &EqResult );

		Values.AddItem( EqResult );
	}
}

/**
 * Updates the numeric property text box and sends the new value to all objects.
 * @param NewValue New value for the property.
 */
void UPropertyInputNumeric::SetValue(FLOAT NewValue, WxPropertyControl* InItem)
{
	wxString NewTextValue;
	NewTextValue.Printf(TEXT("%.4f"), NewValue);
	TextCtrl->SetValue(NewTextValue);

	SendToObjects(InItem);
}

/**
 * Sends the new values to all objects.
 * @param NewValues		New values for the property.
 */
void UPropertyInputNumeric::SetValues(const TArray<FLOAT>& NewValues, WxPropertyControl* InItem)
{
	TArray<FString> Values;
	for ( INT i = 0 ; i < NewValues.Num() ; ++i )
	{
		Values.AddItem( FString::Printf( TEXT("%f"), NewValues(i) ) );
	}

	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	// Build up a list of objects to modify.
	FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();
	TArray<FObjectBaseAddress> ObjectsToModify;
	for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
	{
		UObject*	Object = *Itor;
		BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );
		ObjectsToModify.AddItem( FObjectBaseAddress( Object, Addr ) );
	}

	ImportText( InItem, ObjectsToModify, Values );
}

/*-----------------------------------------------------------------------------
	UPropertyInputInteger (with support for equations)
-----------------------------------------------------------------------------*/

// Determines if this input proxy supports the specified UProperty.
UBOOL UPropertyInputInteger::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	const WxPropertyControl* PropertyWindowBase = InTreeNode->GetNodeWindow();
	if (InTreeNode->IsEditConst())
	{
		return FALSE;
	}

	const UProperty* NodeProperty = InTreeNode->GetProperty();

	const UBOOL bIsInteger = NodeProperty->IsA(UIntProperty::StaticClass());
	const UBOOL bIsNonEnumByte = ( NodeProperty->IsA(UByteProperty::StaticClass()) && ConstCast<UByteProperty>(NodeProperty)->Enum == NULL);

	if(	!InTreeNode->HasNodeFlags(EPropertyNodeFlags::EditInline) && (bIsInteger || bIsNonEnumByte) )
	{
		return TRUE;
	}

	return FALSE;
}

// Allows the created controls to react to the parent window being resized.  Derived classes
// that implement this method should to call up to their parent.
void UPropertyInputInteger::CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue )
{
	// Create any buttons for the control.
	Super::CreateControls( InItem, InBaseClass, InRC, InValue );
	UProperty* Property = InItem->GetProperty();
	check(Property);

	//default value
	FLOAT MetadataIncrementAmount = 1.0f;

	const UBOOL bSpecifiesFixedIncrement = Property->HasMetaData(TEXT("FixedIncrement"));
	if (bSpecifiesFixedIncrement)
	{
		MetadataIncrementAmount = Property->GetFLOATMetaData(TEXT("FixedIncrement"));
	}

	SpinButton->SetFixedIncrementAmount(MetadataIncrementAmount);
}

// Sends the current value in the input control to the selected objects.
void UPropertyInputInteger::SendToObjects( WxPropertyControl* InItem )
{
	if( !TextCtrl )
	{
		return;
	}

	FObjectPropertyNode* ObjectNode = InItem->GetPropertyNode()->FindObjectItemParent();
	

	// If more than one object is selected, an empty field indicates their values for this property differ.
	// Don't send it to the objects value in this case (if we did, they would all get set to None which isn't good).
	if( ObjectNode->GetNumObjects() == 1 || Equation.Len() || TextCtrl->GetValue().Len() )
	{
		INT EqResult = appTrunc(GetValue());

		//clamp based on metadata
		EqResult = ClampValueFromMetaData(InItem, EqResult);

		const FString Value( FString::Printf( TEXT("%i"), EqResult ) );
		//Set the value back into the control, in case of clamping
		TextCtrl->SetValue(*Value);
		Equation = Value;

		FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
		// Build up a list of objects to modify.
		TArray<FObjectBaseAddress> ObjectsToModify;
		for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
		{
			UObject*	Object = *Itor;
			BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );
			ObjectsToModify.AddItem( FObjectBaseAddress( Object, Addr ) );
		}

		ImportText( InItem, ObjectsToModify, Value );
	}
}


/**
* Updates the numeric property text box and sends the new value to all objects.
* @param NewValue New value for the property.
*/
void UPropertyInputInteger::SetValue(FLOAT NewValue, WxPropertyControl* InItem)
{
	wxString NewTextValue;
	NewTextValue.Printf(TEXT("%i"), appTrunc(NewValue));
	TextCtrl->SetValue(NewTextValue);

	SendToObjects(InItem);
}

/**
 * Sends the new values to all objects.
 * @param NewValues		New values for the property.
 */
void UPropertyInputInteger::SetValues(const TArray<FLOAT>& NewValues, WxPropertyControl* InItem)
{
	TArray<FString> Values;
	for ( INT i = 0 ; i < NewValues.Num() ; ++i )
	{
		const INT NewVal = appTrunc(NewValues(i));
		Values.AddItem( FString::Printf( TEXT("%i"), NewVal ) );
	}

	// Build up a list of objects to modify.
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();
	TArray<FObjectBaseAddress> ObjectsToModify;
	for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
	{
		UObject*	Object = *Itor;
		BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );
		ObjectsToModify.AddItem( FObjectBaseAddress( Object, Addr ) );
	}

	ImportText( InItem, ObjectsToModify, Values );
}

/**
 * Clamps a passed in value to the bounds of the metadata of the object.  
 * Supports "ClampMin" (Numeric), "ClampMax" (Numeric), and "ArrayClamp".  Note, ArrayClamp must reference an array with the same outer so it can be found
 * @param InItem - The property window that we are trying to clamp the value of
 * @param InValue - The requested value to set this property to prior to clamping
 * @return The clamped value to assign directly into the property by ImportText
 */

INT UPropertyInputInteger::ClampValueFromMetaData(WxPropertyControl* InItem, INT InValue)
{
	UProperty* Property = InItem->GetProperty();
	check(Property);

	INT Retval = InValue;

	//if there is "Multiple" meta data, the selected number is a multiple
	const FString& MultipleString = Property->GetMetaData(TEXT("Multiple"));
	if (MultipleString.Len())
	{
		check(MultipleString.IsNumeric());
		INT MultipleValue = appAtoi(*MultipleString);
		if (MultipleValue!=0)
		{
			Retval -= Retval%MultipleValue;
		}
	}

	//enforce min
	const FString& MinString = Property->GetMetaData(TEXT("ClampMin"));
	if (MinString.Len())
	{
		check(MinString.IsNumeric());
		INT MinValue = appAtoi(*MinString);
		Retval = Max(MinValue, Retval);
	}
	//Enforce max 
	const FString& MaxString = Property->GetMetaData(TEXT("ClampMax"));
	if (MaxString.Len())
	{
		check(MaxString.IsNumeric());
		INT MaxValue = appAtoi(*MaxString);
		Retval = Min(MaxValue, Retval);
	}
	//enforce array bounds
	const FString& ArrayClampString = Property->GetMetaData(TEXT("ArrayClamp"));
	if (ArrayClampString.Len())
	{
		//ensure that multi-select isn't being used
		WxPropertyWindow* MainWindow = InItem->GetPropertyWindow();
		check(MainWindow);
		if (MainWindow->GetNumObjects() == 1)
		{
			FPropertyNode* PropertyNode = InItem->GetPropertyNode();
			check(PropertyNode);

			FObjectPropertyNode* ObjectNode = PropertyNode->FindObjectItemParent();

			INT LastValidIndex = GetArrayPropertyLastValidIndex(ObjectNode, ArrayClampString);
			Retval = Clamp<INT>(Retval, 0, LastValidIndex);
		}
		else
		{
			GWarn->Logf( LocalizeSecure(LocalizeUnrealEd("PropertyWindow_Error_ArrayClampInvalidInMultiSelect"), *Property->GetName()));
		}
	}
	return Retval;
}

/**
 * Gets the max valid index for a array property of an object
 * @param InObjectNode - The parent of the variable being clamped
 * @param InArrayName - The array name we're hoping to clamp to the extents of
 * @return LastValidEntry in the array (if the array is found)
 */
INT UPropertyInputInteger::GetArrayPropertyLastValidIndex(FObjectPropertyNode* InObjectNode, const FString& InArrayName)
{
	INT ClampMax = MAXINT;

	check(InObjectNode->GetNumObjects()==1);
	UObject* ParentObject = InObjectNode->GetObject(0);

	//find the associated property
	UProperty* FoundProperty = NULL;
	for( TFieldIterator<UProperty> It(ParentObject->GetClass()); It; ++It )
	{
		UProperty* CurProp = *It;
		if (CurProp->GetName()==InArrayName)
		{
			FoundProperty = CurProp;
			break;
		}
	}

	if (FoundProperty && (FoundProperty->ArrayDim == 1))
	{
		UArrayProperty* ArrayProperty = CastChecked<UArrayProperty>( FoundProperty );
		if (ArrayProperty)
		{
			BYTE* PropertyAddressBase = (BYTE*)ParentObject + ArrayProperty->Offset;
			ClampMax = ((FScriptArray*)PropertyAddressBase)->Num() - 1;
		}
		else
		{
			GWarn->Logf( LocalizeSecure(LocalizeUnrealEd("PropertyWindow_Error_ArrayClampPropertyIsNotAnArray"), *InArrayName));
		}
	}
	else
	{
		GWarn->Logf( LocalizeSecure(LocalizeUnrealEd("PropertyWindow_Error_ArrayClampPropertyNotFound"), *InArrayName));
	}

	return ClampMax;
}


///////////////////////////////////////////////////////////////////////////////
//
// Ranged Numeric Text Control
//
///////////////////////////////////////////////////////////////////////////////
class WxRangedNumericTextCtrl : public wxTextCtrl, public FDeferredInitializationWindow
{
public :
	DECLARE_DYNAMIC_CLASS(WxRangedNumericTextCtrl);
	virtual ~WxRangedNumericTextCtrl(void) {};
	/**
	 *	Initialize this text ctrl.  Must be the first function called after creation.
	 *
	 * @param	parent			The parent window.
	 */
	virtual void Create( wxWindow* InParent, wxWindowID InID);
	/**
	 * Callback used for setting the "search"/"cancel" button and adjusting text properly
	 */
	void OnChangeFocus( wxFocusEvent& In );
	/**
	 * To pass event back up to the Host window to control tab ordering
	 */
	void OnChar( wxKeyEvent& In );

	DECLARE_EVENT_TABLE();
};

IMPLEMENT_DYNAMIC_CLASS(WxRangedNumericTextCtrl,wxTextCtrl);

BEGIN_EVENT_TABLE( WxRangedNumericTextCtrl, wxTextCtrl )
EVT_SET_FOCUS ( WxRangedNumericTextCtrl::OnChangeFocus )
EVT_KILL_FOCUS( WxRangedNumericTextCtrl::OnChangeFocus )
EVT_CHAR( WxRangedNumericTextCtrl::OnChar )
END_EVENT_TABLE()

/**
 *	Initialize this text ctrl.  Must be the first function called after creation.
 *
 * @param	parent			The parent window.
 */
void WxRangedNumericTextCtrl::Create( wxWindow* InParent, wxWindowID InID)
{
	check(InParent);

	/** Text box*/
	wxTextCtrl::Create(InParent, InID, TEXT(""), wxDefaultPosition, wxDefaultSize, wxCLIP_CHILDREN | wxWANTS_CHARS | wxTE_PROCESS_ENTER | wxTE_PROCESS_TAB);
}

/**
 * Callback used for setting the "search"/"cancel" button and adjusting text properly
 */
void WxRangedNumericTextCtrl::OnChangeFocus( wxFocusEvent& In )
{
	WxPropertyControl *ParentItem = (WxPropertyControl*)GetGrandParent();
	WxPropertyWindow *Window = ParentItem->GetPropertyWindow();

	Window->Freeze();
	if (In.GetEventType() == wxEVT_SET_FOCUS)
	{
		SetSelection(-1, -1);

		Window->SetLastFocused(ParentItem);
	}
	else if (In.GetEventType() == wxEVT_KILL_FOCUS)
	{
		check(ParentItem);
		ParentItem->InputProxy->SendToObjects( ParentItem );
		ParentItem->InputProxy->OnKillFocus(ParentItem);
	}
	// Set this node as being in focus.
	Window->Thaw();
	Window->Refresh();
}

/**
 * To pass event back up to the Host window to control tab ordering
 */
void WxRangedNumericTextCtrl::OnChar( wxKeyEvent& In )
{
	UINT KeyCode = In.GetKeyCode();
	if (!IsShown() || (KeyCode == WXK_TAB) || (KeyCode == WXK_DOWN) || (KeyCode == WXK_UP))
	{
		//In.SetEventObject(this);
		In.ResumePropagation(wxEVENT_PROPAGATE_MAX);
		In.Skip();	//let parent deal with this
		TryParent(In);
	} 
	else if (KeyCode == WXK_RETURN)
	{
		WxPropertyControl *ParentItem = (WxPropertyControl*)GetGrandParent();
		check(ParentItem);
		ParentItem->InputProxy->SendToObjects( ParentItem );
		ParentItem->InputProxy->OnKillFocus(ParentItem);
		ParentItem->InputProxy->OnSetFocus(ParentItem);
		SetSelection(-1, -1);
	}
	else if (KeyCode == WXK_ESCAPE)
	{
		Hide();
	}
	else
	{
		wxTextCtrl::OnChar(In);
	}
}

/*-----------------------------------------------------------------------------
	WxRangedSlider 
 *	This is a slider/editbox control specific to numeric properties.  It will use the UIMin/UIMax specificied in script to determine slider range, 
 * but uses ClampMin/ClampMax to constrict the range.  If manual changes are needed, clicking on the number will bring up a text ctrl.
 */ 
class WxRangedSlider : public wxPanel
{
public:
	WxRangedSlider(WxPropertyControl* InParent, class UPropertyInputRangedNumeric* InPropertyInput, const TCHAR* InValue);
	~WxRangedSlider();

	/**
	 * Mouse message for the RangedSlider
	 */
	void OnMouseEvent( wxMouseEvent& In );
	/**
	 *	Empty background clear, so wx doesn't clear our draw proxy work
	 */
	void OnDrawBackground(wxEraseEvent& In) {}
	/**
	 * Custom character handling
	 */
	virtual void OnChar (wxKeyEvent& In);

	/**
	 * Show and give the text ctrl focus
	 */
	void ShowTextCtrl(void);
	/**
	 * If the text control is shown, commits the value and hides the text ctrl
	 */
	void CommitValues();


	FString GetStringValue(void) const;
	/**
	 * Accessor to get the value as a float (for int's as well)
	 */
	FLOAT GetFloatValue(void) const;
	/**
	 * Accessor to set the value as a string
	 */
	void SetValue (const FString& InValue);

	/**
	 * Used from the UPropertyInputRangedNumeric when reset to default value.  In this case, the textctrl should be insta-hidden so it can't stomp over the reset value
	 */
	void RefreshControlValue (UProperty* InProperty, BYTE* InData);

	/**
	 * Adjust layout and size according to parent
	 */
	void ParentResize();
private:

	/** Parent Property Input Proxy. */
	class UPropertyInputRangedNumeric* PropertyInput;

	/** Parent Property to send value updates to. */
	WxPropertyControl* PropertyItem;

	/** Point where they clicked down. */
	wxPoint StartMousePoint;
	/** Change since mouse was captured. */
	wxPoint LastMousePoint;

	/** Have we started dragging the mouse yet */
	UBOOL bStartedDragging;

	/** Since Wx has no way of hiding cursors normally, we need to create a blank cursor to use to hide our cursor. */
	wxCursor BlankCursor;

	FRangedNumeric RangedNumeric;

	/** For custom typing of value */
	WxRangedNumericTextCtrl *TextCtrl;

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(WxRangedSlider, wxPanel)
	EVT_MOUSE_EVENTS(WxRangedSlider::OnMouseEvent)
	EVT_ERASE_BACKGROUND(WxRangedSlider::OnDrawBackground)
	EVT_CHAR(WxRangedSlider::OnChar)
END_EVENT_TABLE()

/**
 * Construct for WxRangedSlider
 * @param InParent, the Parent Window
 */
WxRangedSlider::WxRangedSlider(WxPropertyControl* InParent, class UPropertyInputRangedNumeric* InPropertyInput, const TCHAR* InValue ) : 
	wxPanel(InParent),
	PropertyInput(InPropertyInput),
	PropertyItem(InParent)
{
	SetWindowStyle(GetWindowStyle() | wxTRANSPARENT_WINDOW | wxNO_BORDER | wxWANTS_CHARS);

	// Create a Blank Cursor
	WxMaskedBitmap bitmap(TEXT("blank"));
	wxImage BlankImage = bitmap.ConvertToImage();
	BlankImage.SetMaskColour(192,192,192);
	BlankCursor = wxCursor(BlankImage);

	// If more than one object is selected, spinning affects a relative change independently for each object.
	FPropertyNode* PropertyTreeNode = InParent->GetPropertyNode();
	FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();
	//bSpinningMultipleValues = ObjectNode->GetNumObjects() > 1;

	//Setup the internal ranges
	UProperty* Property = InParent->GetProperty();
	const UBOOL bIsFloat = Property->IsA(UFloatProperty::StaticClass());
	const FString& UIMinString = Property->GetMetaData(TEXT("UIMin"));
	const FString& UIMaxString = Property->GetMetaData(TEXT("UIMax"));
	const FString& ClampMinString = Property->GetMetaData(TEXT("ClampMin"));
	const FString& ClampMaxString = Property->GetMetaData(TEXT("ClampMax"));

	RangedNumeric.InitRange(!bIsFloat, UIMinString, UIMaxString, ClampMinString, ClampMaxString);

	FString ValueString = InValue;
	//just in case there were multiple values, let's set it to the value of the first one.
	if (ValueString.Len()==0)
	{
		TArray<FLOAT> Values;
		InPropertyInput->GetValues(InParent, Values);
		check(Values.Num() > 0);
		ValueString = FString::Printf( TEXT("%f"), Values(0) );
	}

	RangedNumeric.SetValue(ValueString);

	//Set up the text control for future use
	TextCtrl = new WxRangedNumericTextCtrl();
	TextCtrl->Create(this, -1);
	TextCtrl->Show(FALSE);

	wxSizer* TextSizer = new wxBoxSizer(wxVERTICAL);
	TextSizer->Add(TextCtrl, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
	SetSizer(TextSizer);
}

/**
 * Destructor for WxRangedSlider
 */
WxRangedSlider::~WxRangedSlider()
{
	if ( TextCtrl )
	{
		TextCtrl->Destroy();
		TextCtrl = NULL;
	}
}

/**
 * Mouse message for the RangedSlider
 */
void WxRangedSlider::OnMouseEvent( wxMouseEvent& InEvent )
{
	check(PropertyItem);

	const UBOOL bHasCapture = HasCapture();

	// See if we should be capturing the mouse.
	if(InEvent.LeftDown())
	{
		//make sure if all properties buttons are on, that this becomes set
		PropertyItem->SetFocus();
		check(TextCtrl);
		TextCtrl->Hide();

		CaptureMouse();

		LastMousePoint  = InEvent.GetPosition();
		StartMousePoint = LastMousePoint;
		bStartedDragging = FALSE;

		// Change the cursor and background color for the panel to indicate that we are dragging.
		wxSetCursor(BlankCursor);

		PropertyItem->Refresh();
	}
	else if(InEvent.LeftUp() && bHasCapture)
	{
		ReleaseMouse();

		//move mouse back to the beginning
		//WPF Implementation
		//WarpPointer(StartMousePoint.x, StartMousePoint.y);
		// Change the cursor back to normal and the background color of the panel back to normal.
		wxSetCursor(wxNullCursor);
		PropertyItem->Refresh();

		//throw up the text control
		if (bStartedDragging == FALSE)
		{
			ShowTextCtrl();
		}
	}
	else if(InEvent.Dragging() && bHasCapture)
	{
		//ensure dead zone
		if (Abs<INT>(InEvent.GetX() - StartMousePoint.x) >= 3)
		{
			bStartedDragging = TRUE;
			/** WPF Implementation
			INT DeltaX = InEvent.GetX() - LastMousePoint.x;
			LastMousePoint = InEvent.GetPosition();

			if (DeltaX != 0)
			{
				RangedNumeric.MoveByIncrements (DeltaX*Abs(DeltaX));
				PropertyInput->SetValue( RangedNumeric.GetStringValue(), PropertyItem );
				PropertyItem->Refresh();
				PropertyItem->Update();
			}
			*/
			wxSetCursor(wxCURSOR_SIZEWE);
			wxRect ClientRect = GetClientRect();
			//check in case of parent sizing
			if (ClientRect.GetWidth() > 0)
			{
				FLOAT Percent = (InEvent.GetX()-ClientRect.x) / (FLOAT)(ClientRect.GetWidth());
				RangedNumeric.MoveByPercent(Percent);
				PropertyInput->SetValue( RangedNumeric.GetStringValue(), PropertyItem );
				PropertyItem->Refresh();
				PropertyItem->Update();
			}
		}
	}
}

/**
 * Custom character handling
 */
void WxRangedSlider::OnChar (wxKeyEvent& In)
{
	UBOOL bChangedValue = FALSE;
	UINT KeyCode = In.GetKeyCode();
	if (KeyCode == WXK_RIGHT)
	{
		RangedNumeric.MoveByIncrements(1);
		bChangedValue = TRUE;
	}
	else if (KeyCode == WXK_LEFT)
	{
		RangedNumeric.MoveByIncrements(-1);
		bChangedValue = TRUE;
	}
	else
	{
		In.ResumePropagation(wxEVENT_PROPAGATE_MAX);
		In.Skip();	//let parent deal with this
		TryParent(In);
	}

	if (bChangedValue)
	{
		PropertyInput->SetValue( RangedNumeric.GetStringValue(), PropertyItem );
		PropertyItem->Refresh();
		PropertyItem->Update();
	}
}

/**
 * Show and give the text ctrl focus
 */
void WxRangedSlider::ShowTextCtrl(void)
{
	check(TextCtrl);
	TextCtrl->Show(TRUE);
	Layout();
	TextCtrl->SetValue(*RangedNumeric.GetStringValue());
	TextCtrl->SetFocus();
}

/**
 * If the text control is shown, commits the value and hides the text ctrl
 */
void WxRangedSlider::CommitValues()
{
	//if the text ctrl WAS shown and we're now focused on something else.
	if (TextCtrl->IsShown())
	{
		//hide the window, no longer required
		if (wxWindow::FindFocus()!=TextCtrl)
		{
			TextCtrl->Show(FALSE);
		}

		RangedNumeric.SetValue (TextCtrl->GetValue().c_str());
		PropertyInput->SetValue( RangedNumeric.GetStringValue(), PropertyItem );
	}
}


/**
 * Accessor to get the value as a string
 */
FString WxRangedSlider::GetStringValue(void) const
{
	return RangedNumeric.GetStringValue();
}

/**
 * Accessor to get the value as a float (for int's as well)
 */
FLOAT WxRangedSlider::GetFloatValue(void) const
{
	return RangedNumeric.GetFloatValue();
}

/**
 * Accessor to set the value as a string
 */
void WxRangedSlider::SetValue (const FString& InValue)
{
	RangedNumeric.SetValue(InValue);	
}

/**
 * Used from the UPropertyInputRangedNumeric when reset to default value.  In this case, the textctrl should be insta-hidden so it can't stomp over the reset value
 */
void WxRangedSlider::RefreshControlValue (UProperty* InProperty, BYTE* InData)
{
	FString ValueString;
	InProperty->ExportText( 0, ValueString, InData - InProperty->Offset, InData - InProperty->Offset, NULL, PPF_Localized );
	RangedNumeric.SetValue(ValueString);

	check(TextCtrl);
	TextCtrl->Hide();
}

/**
 * Adjust layout and size according to parent
 */
void WxRangedSlider::ParentResize()
{
	Layout();
}

/*-----------------------------------------------------------------------------
	UPropertyInputRangedNumeric
-----------------------------------------------------------------------------*/

// Allows the created controls to react to the parent window being resized.  Derived classes
// that implement this method should to call up to their parent.
void UPropertyInputRangedNumeric::CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue )
{
	// Create any buttons for the control.
	Super::CreateControls( InItem, InBaseClass, InRC, InValue );

	// Add a spin button to the property sizer.

	Slider = new WxRangedSlider( InItem, this, InValue);

	ParentResized(InItem, InRC);
}

// Deletes any controls which were created for editing.
void UPropertyInputRangedNumeric::DeleteControls()
{
	// Delete any buttons on the control.
	Super::DeleteControls();

	if( Slider )
	{
		Slider->Destroy();
		Slider = NULL;
	}
}

// Determines if this input proxy supports the specified UProperty.
UBOOL UPropertyInputRangedNumeric::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	if (InTreeNode->IsEditConst())
	{
		return FALSE;
	}

	const WxPropertyControl* PropertyWindowBase= InTreeNode->GetNodeWindow();
	check(PropertyWindowBase);
	const UProperty* NodeProperty = InTreeNode->GetProperty();

	const UBOOL bIsInteger = NodeProperty->IsA(UIntProperty::StaticClass());
	const UBOOL bIsNonEnumByte = ( NodeProperty->IsA(UByteProperty::StaticClass()) && ConstCast<UByteProperty>(NodeProperty)->Enum == NULL);
	const UBOOL bIsFloat = NodeProperty->IsA(UFloatProperty::StaticClass());

	if(	!InTreeNode->HasNodeFlags(EPropertyNodeFlags::EditInline) && (bIsInteger || bIsNonEnumByte || bIsFloat))
	{
		const FString& UIMinString = NodeProperty->GetMetaData(TEXT("UIMin"));
		const FString& UIMaxString = NodeProperty->GetMetaData(TEXT("UIMax"));
		const FString& ClampMinString = NodeProperty->GetMetaData(TEXT("ClampMin"));
		const FString& ClampMaxString = NodeProperty->GetMetaData(TEXT("ClampMax"));
		if ((UIMinString.Len() || ClampMinString.Len()) && (UIMaxString.Len() || ClampMaxString.Len()))
		{
			return TRUE;
		}
	}

	return FALSE;
}
// Sends the current value in the input control to the selected objects.
void UPropertyInputRangedNumeric::SendToObjects( WxPropertyControl* InItem )
{
	if( !Slider )
	{
		return;
	}

	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	check(PropertyTreeNode);
	FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();

	// If more than one object is selected, an empty field indicates their values for this property differ.
	// Don't send it to the objects value in this case (if we did, they would all get set to None which isn't good).
	FString NewValue = Slider->GetStringValue();
	if( ObjectNode->GetNumObjects() == 1 || NewValue.Len() )
	{
		// Build up a list of objects to modify.
		TArray<FObjectBaseAddress> ObjectsToModify;
		for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
		{
			UObject*	Object = *Itor;
			BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );
			ObjectsToModify.AddItem( FObjectBaseAddress( Object, Addr ) );
		}

		ImportText( InItem, ObjectsToModify, NewValue );
	}
}

/**
 * @return Returns the numeric value of the property.
 */
FLOAT UPropertyInputRangedNumeric::GetValue()
{
	check(Slider);
	return Slider->GetFloatValue();
}

/**
 * Gets the values for each of the objects that a property item is affecting.
 * @param	InItem		Property item to get objects from.
 * @param	Values		Array of FLOATs to store values.
 */
void UPropertyInputRangedNumeric::GetValues(WxPropertyControl* InItem, TArray<FLOAT> &Values)
{
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	check(PropertyTreeNode);
	UProperty* NodeProperty = PropertyTreeNode->GetProperty();

	FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();

	Values.Empty();

	for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
	{
		UObject*	Object = *Itor;
		BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );

		const FObjectBaseAddress Cur( Object, Addr );

		FString PreviousValue;
		NodeProperty->ExportText( 0, PreviousValue, Cur.BaseAddress - NodeProperty->Offset, Cur.BaseAddress - NodeProperty->Offset, NULL, PPF_Localized );

		FLOAT EqResult = appAtof(*PreviousValue);
		Values.AddItem( EqResult );
	}
}

/**
 * Updates the numeric property text box and sends the new value to all objects.
 * @param NewValue New value for the property.
 */
void UPropertyInputRangedNumeric::SetValue(const FString& NewValue, WxPropertyControl* InItem)
{
	check(Slider);
	Slider->SetValue(NewValue);

	SendToObjects(InItem);
}


// Allows the created controls to react to the parent window being resized.
void UPropertyInputRangedNumeric::ParentResized( WxPropertyControl* InItem, const wxRect& InRC )
{
	if (Slider)
	{
		check(InItem);

		const wxRect rc = InItem->GetClientRect();
		const INT SplitterPos = InItem->GetPropertyWindow()->GetSplitterPos();
		const wxRect SliderRect = wxRect( rc.x + SplitterPos, rc.y, rc.width - SplitterPos, rc.GetHeight());

		Slider->SetSize(SliderRect.x, SliderRect.y, SliderRect.GetWidth(), SliderRect.GetHeight());
		Slider->ParentResize();
	}
}

/**
 * Allows a customized response to the set focus event of the property item.
 * @param	InItem			The property window node.
 */
void UPropertyInputRangedNumeric::OnSetFocus(WxPropertyControl* InItem) 
{
	if (Slider)
	{
		Slider->ShowTextCtrl();
	}
}

/**
* Allows a customized response to the kill focus event of the property item.
* @param	InItem			The property window node.
*/
void UPropertyInputRangedNumeric::OnKillFocus(WxPropertyControl* InItem)
{
	check(Slider);
	Slider->CommitValues();
}

/**
 * Updates the control value with the property value.
 *
 * @param	InProperty		The property from which to export.
 * @param	InData			The parent object read address.
 */
void UPropertyInputRangedNumeric::RefreshControlValue( UProperty* InProperty, BYTE* InData )
{
	//if reset was selected, we don't want the text ctrl to re-apply its value.  Hide it!
	check(Slider);
	Slider->RefreshControlValue(InProperty, InData);
}

/*-----------------------------------------------------------------------------
	UPropertyInputRotation
-----------------------------------------------------------------------------*/

// Determines if this input proxy supports the specified UProperty.
UBOOL UPropertyInputRotation::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	if (InTreeNode->IsEditConst())
	{
		return FALSE;
	}

	const FPropertyNode* ParentNode = InTreeNode->GetParentNode();
	check(ParentNode);
	const UProperty* ParentProperty = ParentNode->GetProperty();

	if( ConstCast<UStructProperty>(ParentProperty,CLASS_IsAUStructProperty) && ConstCast<UStructProperty>(ParentProperty,CLASS_IsAUStructProperty)->Struct->GetFName() == NAME_Rotator )
	{
		return TRUE;
	}
	else if (ExactCastConst<UIntProperty>(InTreeNode->GetProperty()) != NULL && InTreeNode->GetProperty()->HasMetaData(TEXT("EditAsDegrees")))
	{
		return TRUE;
	}

	return FALSE;
}

// Allows the created controls to react to the parent window being resized.  Derived classes
// that implement this method should to call up to their parent.
void UPropertyInputRotation::CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue )
{
	FString Result;

	// Set the equation initially to be the same as the numeric value in the property.
	Equation = InValue;

	if(Equation.Len() > 0)
	{
		FLOAT Val = appAtof( *Equation );

		Val = 360.f * (Val / 65536.f);

		FString Wk;
		if( Abs(Val) > 359.f )
		{
			INT Revolutions = Val / 360.f;
			Val -= Revolutions * 360;
			Wk = FString::Printf( TEXT("%.2f%c %s %d"), Val, 176, (Revolutions < 0)?TEXT("-"):TEXT("+"), abs(Revolutions) );
		}
		else
		{
			Wk = FString::Printf( TEXT("%.2f%c"), Val, 176 );
		}

		Result = Wk;
	}


	// Create any buttons for the control.
	Super::CreateControls( InItem, InBaseClass, InRC, *Result );

	TextCtrl->SetValue(*Result);
	TextCtrl->SetSelection(-1,-1);

	// Set a fixed increment amount.
	SpinButton->SetFixedIncrementAmount(1.0f);

}

// Sends the current value in the input control to the selected objects.
void UPropertyInputRotation::SendToObjects( WxPropertyControl* InItem )
{
	if( !TextCtrl )
	{
		return;
	}

	FObjectPropertyNode* ObjectNode = InItem->GetPropertyNode()->FindObjectItemParent();

	// If more than one object is selected, an empty field indicates their values for this property differ.
	// Don't send it to the objects value in this case (if we did, they would all get set to None which isn't good).
	if( ObjectNode->GetNumObjects() == 1 || Equation.Len() || TextCtrl->GetValue().Len() )
	{
		// Parse the input back from the control (i.e. "80.0o- 2")
		FLOAT Val = GetValue();

		// Convert back to Unreal units
		Val = 65536.f * (Val / 360.f);

		
		const FString Value = *FString::Printf( TEXT("%f"), Val );

		// Build up a list of objects to modify.
		FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
		TArray<FObjectBaseAddress> ObjectsToModify;
		for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
		{
			UObject*	Object = *Itor;
			BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );
			ObjectsToModify.AddItem( FObjectBaseAddress( Object, Addr ) );
		}

		ImportText( InItem, ObjectsToModify, Value );
	}
}

/**
 * @return Returns the numeric value of the property in DEGREES, NOT Unreal Units!
 */
FLOAT UPropertyInputRotation::GetValue()
{
	// Parse the input back from the control (i.e. "80.0o- 2")
	Equation = (const TCHAR*)TextCtrl->GetValue();
	FLOAT Val;

	TArray<FString> Split;
	if( Equation.ParseIntoArray( &Split, TEXT(" "), 0 ) == 3 )
	{
		const INT Sign = (Split(1) == TEXT("+")) ? +1 : -1;
		const INT Revolutions = appAtof( *Split(2) );

		Val = appAtof( *Split(0) ) + (360 * (Revolutions * Sign));
	}
	else
	{
		Val = appAtof( *Equation );
	}

	return Val;
}

/**
 * Gets the values for each of the objects that a property item is affecting.
 * @param	InItem		Property item to get objects from.
 * @param	Values		Array of FLOATs to store values.
 */
void UPropertyInputRotation::GetValues(WxPropertyControl* InItem, TArray<FLOAT> &Values)
{
	Super::GetValues(InItem, Values);

	// Convert rotation values from unreal units to degrees.
	for(INT ItemIdx=0; ItemIdx<Values.Num(); ItemIdx++)
	{
		const FLOAT Val = Values(ItemIdx);

		Values(ItemIdx) = 360.f * (Val / 65536.f);
	}
}

/**
 * Updates the numeric property text box and sends the new value to all objects, note that this takes values IN DEGREES.  NOT Unreal Units!
 * @param NewValue New value for the property.
 */
void UPropertyInputRotation::SetValue(FLOAT Val, WxPropertyControl* InItem)
{
	FString Wk;
	if( Abs(Val) > 359.f )
	{
		const INT Revolutions = Val / 360.f;
		Val -= Revolutions * 360;
		Wk = FString::Printf( TEXT("%.2f%c %s %d"), Val, 176, (Revolutions < 0)?TEXT("-"):TEXT("+"), abs(Revolutions) );
	}
	else
	{
		Wk = FString::Printf( TEXT("%.2f%c"), Val, 176 );
	}

	TextCtrl->SetValue(*Wk);

	SendToObjects(InItem);
}

/**
 * Sends the new values to all objects.
 * @param NewValues		New values for the property.
 */
void UPropertyInputRotation::SetValues(const TArray<FLOAT>& NewValues, WxPropertyControl* InItem)
{
	TArray<FString> Values;
	for ( INT i = 0 ; i < NewValues.Num() ; ++i )
	{
		FLOAT Val = NewValues(i);

		// Convert back to Unreal units
		Val = 65536.f * (Val / 360.f);

		const FString Wk = *FString::Printf( TEXT("%f"), Val );

		Values.AddItem( Wk );
	}

	// Build up a list of objects to modify.
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();
	TArray<FObjectBaseAddress> ObjectsToModify;
	for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
	{
		UObject*	Object = *Itor;
		BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );
		ObjectsToModify.AddItem( FObjectBaseAddress( Object, Addr ) );
	}

	ImportText( InItem, ObjectsToModify, Values );
}

/*-----------------------------------------------------------------------------
	WxPassNavigationComboBox
-----------------------------------------------------------------------------*/
class WxPassNavigationComboBox : public WxComboBox
{
	DECLARE_DYNAMIC_CLASS(WxPassNavigationComboBox);
	/**
	 * Ctor to match wxComboBox
	 */
	WxPassNavigationComboBox(void) : WxComboBox()
	{
	}
	/**
	 * Ctor to match wxComboBox
	 */
	WxPassNavigationComboBox(wxWindow *parent, wxWindowID id,
		const wxString& value = wxEmptyString,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		int n = 0, const wxString choices[] = NULL,
		long style = 0,
		const wxValidator& validator = wxDefaultValidator,
		const wxString& name = wxComboBoxNameStr) :
	WxComboBox(parent,id,value,pos,size,n,choices,style,validator,name)
	{
	}

	/**
	 * To pass event back up to the Host window to control tab ordering
	 */
	void OnChar( wxKeyEvent& In )
	{
		if (In.GetKeyCode() == WXK_TAB )
		{
			//just go up to the parent control
			In.ResumePropagation(1);
			TryParent(In);
			In.ResumePropagation(0);	//now that we've hand propagated, don't bother trying in the base case.
		}
		In.Skip();
	}

	/**
	 * To pass event back up to the Host window to control tab ordering
	 */
	void WxPassNavigationComboBox::OnLeftClick( wxMouseEvent& In )
	{
		//just go up to the parent control
		In.ResumePropagation(1);
		//allows the control to take focus and highlight correctly
		TryParent(In);
		In.ResumePropagation(0);	//now that we've hand propagated, don't bother trying in the base case.
		if (m_parent)
		{
			//try to set this as the "active" window according to the property window
			m_parent->SetFocus();
		}
		In.Skip();
	}

	DECLARE_EVENT_TABLE();
};

IMPLEMENT_DYNAMIC_CLASS(WxPassNavigationComboBox,WxComboBox);

BEGIN_EVENT_TABLE( WxPassNavigationComboBox, WxComboBox )
EVT_CHAR( WxPassNavigationComboBox::OnChar )
EVT_LEFT_DOWN( WxPassNavigationComboBox::OnLeftClick )
END_EVENT_TABLE()


/*-----------------------------------------------------------------------------
UPropertyInput_DynamicCombo
-----------------------------------------------------------------------------*/

// Determines if this input proxy supports the specified UProperty.
UBOOL UPropertyInput_DynamicCombo::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	if (InTreeNode->IsEditConst())
	{
		return FALSE;
	}

	const WxPropertyControl* PropertyWindowBase = InTreeNode->GetNodeWindow();
	check(PropertyWindowBase);
	const UProperty* NodeProperty = InTreeNode->GetProperty();


	if (NodeProperty->GetFName() == TEXT("Group"))
	{
		return  TRUE;
	}
	return FALSE;
}

// Allows the created controls to react to the parent window being resized.  Derived classes
// that implement this method should to call up to their parent.
void UPropertyInput_DynamicCombo::CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue )
{
	check(InItem);

	// Create any buttons for the control.
	Super::CreateControls( InItem, InBaseClass, InRC, InValue );

	FString CurrentTextValue = InValue;
	UProperty* NodeProperty = InItem->GetProperty();


	wxRect ComboRect = GetComboRect(InRC);

	ComboBox = new WxComboBox(InItem, ID_PROPWINDOW_COMBOBOX, TEXT(""), ComboRect.GetPosition(), ComboRect.GetSize(), -1, NULL, wxCB_DROPDOWN|wxTE_PROCESS_ENTER|wxTE_PROCESS_TAB);
	//use this as the last rect size that was set
	LastResizeRect = ComboRect;

	// Freeze the combo box while we populate it to reduce the performance hit
	ComboBox->Freeze();

	FillComboFromMaterialEditor(ComboBox, InItem, CurrentTextValue);

	ParentResized( InItem, InRC );

	// Thaw the combo box after we've populated its contents
	ComboBox->Thaw();
}

// Deletes any controls which were created for editing.
void UPropertyInput_DynamicCombo::DeleteControls()
{
	// Delete any buttons on the control.
	Super::DeleteControls();

	if( ComboBox )
	{
		ComboBox->Destroy();
		ComboBox = NULL;
	}
}

// Sends the current value in the input control to the selected objects.
void UPropertyInput_DynamicCombo::SendToObjects( WxPropertyControl* InItem )
{
	if( !ComboBox )
	{
		return;
	}

	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	check(PropertyTreeNode);
	UProperty* NodeProperty = PropertyTreeNode->GetProperty();


	FString Value;

	Value = ComboBox->GetValue().c_str();

	// Build up a list of objects to modify.
	TArray<FObjectBaseAddress> ObjectsToModify;

	FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();
	for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
	{
		UObject*	Object = *Itor;
		BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );
		ObjectsToModify.AddItem( FObjectBaseAddress( Object, Addr ) );
	}

	ImportText( InItem, ObjectsToModify, Value );
	ComboBox->Freeze();
	FillComboFromMaterialEditor(ComboBox,InItem, Value);
	ComboBox->Thaw();
}

// Allows the created controls to react to the parent window being resized.
void UPropertyInput_DynamicCombo::ParentResized( WxPropertyControl* InItem, const wxRect& InRC )
{
	Super::ParentResized( InItem, InRC );

	if( ComboBox )
	{
		//prevents calling resize over and over on the children
		wxRect NewComboRect = GetComboRect(InRC);
		if (NewComboRect != LastResizeRect)
		{
			if ((NewComboRect.GetWidth() == LastResizeRect.GetWidth()) && (NewComboRect.GetHeight() == LastResizeRect.GetHeight()))
			{
				ComboBox->Move(NewComboRect.GetLeft(), NewComboRect.GetTop());
			}
			else
			{
				ComboBox->SetSize( NewComboRect);
			}
			LastResizeRect = NewComboRect;
		}
	}
}

/**Shared function to get the Rect that we want to set this combo to from the parent window size*/
wxRect UPropertyInput_DynamicCombo::GetComboRect(const wxRect& InRect)
{
	wxRect NewComboRect(InRect.x - 2, 0, InRect.GetWidth() + 2 - (Buttons.Num() * GetButtonSize()), InRect.GetHeight());
	return NewComboRect;
}

/** Repopulates the combo box if it's been created*/
void UPropertyInput_DynamicCombo::RefreshControlValue( UProperty* InProperty, BYTE* InData )
{
	FString Wk;
	InProperty->ExportText( 0, Wk, InData - InProperty->Offset, InData - InProperty->Offset, NULL, PPF_Localized );

	if (ComboBox)
	{
		INT Index = 0;
		Index = ComboBox->FindString( *Wk );
		ComboBox->SetValue(*Wk);	
		ComboBox->SetSelection(Index);
	}
}

/**
* Fills the combobox with the contents of material editor group names
* @param InComboBox - the combo box to fill in with strings
* @param InItem - node to get retrieve all information
* @param InDefaultValue - legacy default value
*/
IMPLEMENT_COMPARE_CONSTREF(FString, DynamicComboSort, 
{ 
	FString AName = A.ToLower();
	FString BName = B.ToLower();
	if (AName == TEXT("none"))
	{
		return 1;
	}
	if (BName == TEXT("none"))
	{
		return 0;
	}

	return appStricmp( *AName, *BName ); 
})
FString UPropertyInput_DynamicCombo::FillComboFromMaterialEditor(wxComboBox* InComboBox, WxPropertyControl* InItem, const FString& InDefaultValue)
{
	FString CurrentTextValue = InDefaultValue;
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	const WxPropertyWindowHost* PropertyWindowHost = InItem->GetPropertyWindow()->GetParentHostWindow();
	check(PropertyWindowHost);
	const wxWindow *HostParent   = PropertyWindowHost->GetParent();
	const WxMaterialEditor *Win = wxDynamicCast( HostParent, WxMaterialEditor );
	UBOOL bDisableCtrl = FALSE;
	check(ComboBox);
	ComboBox->Clear();
	
	// Get Material editor
	if (Win == NULL)
	{
		wxAuiFloatingFrame *OwnerFloatingFrame = wxDynamicCast( HostParent, wxAuiFloatingFrame );
		check(OwnerFloatingFrame);
		wxAuiManager *AuiManager = OwnerFloatingFrame->GetOwnerManager();
		Win = wxDynamicCast(AuiManager->GetManagedWindow(), WxMaterialEditor);
	}
	check(Win);
	TArray<FString> ExistingGroups;
	ExistingGroups.AddUniqueItem(TEXT("None"));
	UMaterial* MaterialPrev = Cast<UMaterial>(Win->MaterialInterface);
	if (MaterialPrev)
	{
		for( INT MaterialExpressionIndex = 0 ; MaterialExpressionIndex < MaterialPrev->Expressions.Num() ; ++MaterialExpressionIndex )
		{

			UMaterialExpression* MaterialExpression = MaterialPrev->Expressions( MaterialExpressionIndex );
			UMaterialExpressionParameter *Switch = Cast<UMaterialExpressionParameter>(MaterialExpression);
			UMaterialExpressionTextureSampleParameter *TextureS = Cast<UMaterialExpressionTextureSampleParameter>(MaterialExpression);
			UMaterialExpressionFontSampleParameter  *FontS = Cast<UMaterialExpressionFontSampleParameter>(MaterialExpression);
			if(Switch)
			{
				ExistingGroups.AddUniqueItem(Switch->Group.ToString());
			}
			if(TextureS)
			{
				ExistingGroups.AddUniqueItem(TextureS->Group.ToString());
			}
			if(FontS)
			{
				ExistingGroups.AddUniqueItem(FontS->Group.ToString());
			}
		}
		Sort<USE_COMPARE_CONSTREF(FString, DynamicComboSort)>( ExistingGroups.GetTypedData(), ExistingGroups.Num() );
		TArray<BYTE*> Addresses;
		if ( PropertyTreeNode->GetReadAddress( PropertyTreeNode, PropertyTreeNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses ) )
		{
			FName* Param = (FName*)((BYTE*)Addresses(0));
			ComboBox->SetValue(*Param->ToString());	
			CurrentTextValue = Param->ToString();

		}

		for( INT i = 0 ; i <ExistingGroups.Num() ; ++i )
		{
			ComboBox->Append( *ExistingGroups(i));			
		}
	}
	

	ComboBox->SetValue(*InDefaultValue);
	INT Index = ComboBox->FindString( *InDefaultValue );
	ComboBox->SetSelection(Index);
	return CurrentTextValue;
}
/*-----------------------------------------------------------------------------
	UPropertyInputCombo
-----------------------------------------------------------------------------*/


// Determines if this input proxy supports the specified UProperty.
UBOOL UPropertyInputCombo::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	if (InTreeNode->IsEditConst())
	{
		return FALSE;
	}

	const WxPropertyControl* PropertyWindowBase = InTreeNode->GetNodeWindow();
	check(PropertyWindowBase);
	const UProperty* NodeProperty = InTreeNode->GetProperty();


	if(	((NodeProperty->IsA(UByteProperty::StaticClass()) && ConstCast<UByteProperty>(NodeProperty)->Enum)
			||	(NodeProperty->IsA(UNameProperty::StaticClass()) && NodeProperty->GetFName() == NAME_InitialState)
			||	(NodeProperty->IsA(UStrProperty::StaticClass()) && NodeProperty->HasMetaData(TEXT("Enum")))
			||  (ConstCast<UClassProperty>(NodeProperty)))
			||	(NodeProperty->HasMetaData(TEXT("DynamicList")))
			&&	( ( InArrayIdx == -1 && NodeProperty->ArrayDim == 1 ) || ( InArrayIdx > -1 && NodeProperty->ArrayDim > 0 ) ) )
	{
		return TRUE;
	}

	return FALSE;
}

// Allows the created controls to react to the parent window being resized.  Derived classes
// that implement this method should to call up to their parent.
void UPropertyInputCombo::CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue )
{
	long style;

	// Create any buttons for the control.
	Super::CreateControls( InItem, InBaseClass, InRC, InValue );

	FString CurrentTextValue = InValue;
	UProperty* NodeProperty = InItem->GetProperty();

	UByteProperty* ByteProperty = Cast<UByteProperty>(NodeProperty);
	if ( ByteProperty != NULL )
	{
		style = wxCB_READONLY;
		if(ByteProperty->GetOwnerProperty()->HasMetaData(TEXT("Sorted")))
		{
			style |= wxCB_SORT;
		}
	}
	else
	{
		style = wxCB_READONLY|wxCB_SORT;
	}

	wxRect ComboRect = GetComboRect(InRC);
	ComboBox = new WxPassNavigationComboBox( InItem, ID_PROPWINDOW_COMBOBOX, TEXT(""), ComboRect.GetPosition(), ComboRect.GetSize(), -1, NULL, style | wxWANTS_CHARS );
	//use this as the last rect size that was set
	LastResizeRect = ComboRect;

	// Freeze the combo box while we populate it to reduce the performance hit
	ComboBox->Freeze();

	if( ByteProperty != NULL )
	{
		check(ByteProperty->Enum);

		UEnum* Enum = ByteProperty->Enum;
		CurrentTextValue = FillComboFromEnum(ComboBox, Enum, CurrentTextValue);
	}
	else if( NodeProperty->IsA(UNameProperty::StaticClass()) && NodeProperty->GetFName() == NAME_InitialState )
	{
		TArray<FName> States;

		if( InBaseClass )
		{
			for( TFieldIterator<UState> StateIt( InBaseClass ) ; StateIt ; ++StateIt )
			{
				if( StateIt->StateFlags & STATE_Editable )
				{
					States.AddUniqueItem( StateIt->GetFName() );
				}
			}
		}

		ComboBox->Append( *FName(NAME_None).ToString() );
		for( INT i = 0 ; i < States.Num() ; i++ )
		{
			ComboBox->Append( *States(i).ToString() );
		}
	}
	else if( Cast<UClassProperty>(NodeProperty) )			
	{
		UClassProperty* ClassProp = static_cast<UClassProperty*>(NodeProperty);
		ComboBox->Append( TEXT("None") );
		const UBOOL bAllowAbstract = ClassProp->GetOwnerProperty()->HasMetaData(TEXT("AllowAbstract"));
		for( TObjectIterator<UClass> It ; It ; ++It )
		{
			if ( It->IsChildOf(ClassProp->MetaClass) && IsClassAllowed(InItem, *It, bAllowAbstract) )
			{
				ComboBox->Append( *It->GetName() );
			}
		}
	} 
	else if (NodeProperty->IsA(UStrProperty::StaticClass()) && NodeProperty->HasMetaData(TEXT("Enum")))
	{
		FString EnumName = NodeProperty->GetMetaData(TEXT("Enum"));
		UEnum* Enum = FindObject<UEnum>(NULL, *EnumName, TRUE);
		CurrentTextValue = FillComboFromEnum (ComboBox, Enum, CurrentTextValue);
	}
	else if (NodeProperty->HasMetaData(TEXT("DynamicList")))
	{
		CurrentTextValue = FillComboFromDynamicList(InItem, CurrentTextValue);
	}

	// This is hacky and terrible, but I don't see a way around it.

	if( CurrentTextValue.Left( 6 ) == TEXT("Class'") )
	{
		CurrentTextValue = CurrentTextValue.Mid( 6, CurrentTextValue.Len()-7 );
		CurrentTextValue = CurrentTextValue.Right( CurrentTextValue.Len() - CurrentTextValue.InStr( ".", 1 ) - 1 );
	}

	INT Index = 0;
	TArray<BYTE*> Addresses;
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	if ( PropertyTreeNode->GetReadAddress( PropertyTreeNode, PropertyTreeNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses ) )
	{
		bContainsMultipleValueString = FALSE;
		Index = ComboBox->FindString( *CurrentTextValue );
	}
	else
	{
		bContainsMultipleValueString = TRUE;
		ComboBox->Insert(*LocalizeUnrealEd("MultiplePropertyValues"), 0);
	}

	ComboBox->Select( Index >= 0 ? Index : 0 );

	ParentResized( InItem, InRC );

	// Thaw the combo box after we've populated its contents
	ComboBox->Thaw();
}

// Deletes any controls which were created for editing.
void UPropertyInputCombo::DeleteControls()
{
	// Delete any buttons on the control.
	Super::DeleteControls();

	if( ComboBox )
	{
		ComboBox->Destroy();
		ComboBox = NULL;
	}
}

// Sends the current value in the input control to the selected objects.
void UPropertyInputCombo::SendToObjects( WxPropertyControl* InItem )
{
	if( !ComboBox )
	{
		return;
	}

	// if we're displaying the "multiple value" string and that is what is selected, don't modify the objects
	if ( bContainsMultipleValueString )
	{
		if ( ComboBox->GetSelection() == 0 )
		{
			return;
		}
		else
		{
			ComboBox->Delete(0);
			bContainsMultipleValueString = FALSE;
		}
	}

	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	check(PropertyTreeNode);
	UProperty* NodeProperty = PropertyTreeNode->GetProperty();


	FString Value;
	if ( bUsesAlternateDisplayValues && !NodeProperty->IsA(UStrProperty::StaticClass()))
	{
		// currently only enum properties can use alternate display values; this might change, so assert here so that if support is expanded to other property types
		// without updating this block of code, we'll catch it quickly
		UEnum* Enum = CastChecked<UByteProperty>(NodeProperty)->Enum;
		check(Enum);

		INT SelectedItem = ComboBox->GetSelection();
		check(SelectedItem<Enum->NumEnums());

		Value = Enum->GetEnum(SelectedItem).ToString();
	}
	else
	{
		Value = ComboBox->GetStringSelection().c_str();
	}

	// Build up a list of objects to modify.
	TArray<FObjectBaseAddress> ObjectsToModify;

	FObjectPropertyNode* ObjectNode = PropertyTreeNode->FindObjectItemParent();
	for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
	{
		UObject*	Object = *Itor;
		BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );
		ObjectsToModify.AddItem( FObjectBaseAddress( Object, Addr ) );
	}

	ImportText( InItem, ObjectsToModify, Value );
}

// Allows the created controls to react to the parent window being resized.
void UPropertyInputCombo::ParentResized( WxPropertyControl* InItem, const wxRect& InRC )
{
	Super::ParentResized( InItem, InRC );

	if( ComboBox )
	{
		//prevents calling resize over and over on the children
		wxRect NewComboRect = GetComboRect(InRC);
		if (NewComboRect != LastResizeRect)
		{
			if ((NewComboRect.GetWidth() == LastResizeRect.GetWidth()) && (NewComboRect.GetHeight() == LastResizeRect.GetHeight()))
			{
				ComboBox->Move(NewComboRect.GetLeft(), NewComboRect.GetTop());
			}
			else
			{
				ComboBox->SetSize( NewComboRect);
			}
			LastResizeRect = NewComboRect;
		}
	}
}

/**Shared function to get the Rect that we want to set this combo to from the parent window size*/
wxRect UPropertyInputCombo::GetComboRect(const wxRect& InRect)
{
	wxRect NewComboRect(InRect.x - 2, 0, InRect.GetWidth() + 2 - (Buttons.Num() * GetButtonSize()), InRect.GetHeight());
	return NewComboRect;
}

/** Repopulates the combo box if it's been created*/
void UPropertyInputCombo::RefreshControlValue( UProperty* InProperty, BYTE* InData )
{
	FString Wk;
	InProperty->ExportText( 0, Wk, InData - InProperty->Offset, InData - InProperty->Offset, NULL, PPF_Localized );

	if (ComboBox)
	{
		INT Index = 0;
		Index = ComboBox->FindString( *Wk );
		if (Index >= 0)
		{
			ComboBox->Select( Index );
		}
	}
}

/**
 * Fills the combobox with the contents of the enumeration
 * @param InComboBox - the combo box to fill in with strings
 * @param InEnum - the enumeration to fill the combo box with
 */
FString UPropertyInputCombo::FillComboFromEnum (wxComboBox* InComboBox, UEnum* InEnum, const FString& InDefaultValue)
{
	FString CurrentTextValue = InDefaultValue;
	if (InEnum)
	{
		UBOOL bReplacedInValue=FALSE;

		// Names.Num() - 1, because the last item in an enum is the _MAX item
		for( INT i=0; i< InEnum->NumEnums() - 1; i++ )
		{
			// this is the name of the enum value (i.e. ROLE_Authority)
			FString EnumValueName = InEnum->GetEnum(i).ToString();

			// see if we specified an alternate name for this value using metadata
			const FString& AlternateEnumValueName = InEnum->GetMetaData( TEXT("DisplayName"), i );
			if ( AlternateEnumValueName.Len() == 0 )
			{
				ComboBox->Append( *EnumValueName );
			}
			else
			{
				// CurrentTextValue will be the actual enum value name; if this enum value is the one corresponding to the current property value,
				// change CurrentTextValue to the alternate name text.
				if ( !bReplacedInValue && EnumValueName == CurrentTextValue )
				{
					bReplacedInValue = TRUE;
					CurrentTextValue = AlternateEnumValueName;
				}

				bUsesAlternateDisplayValues = TRUE;
				ComboBox->Append( *AlternateEnumValueName );
			}
		}
	}
	return CurrentTextValue;
}

FString UPropertyInputCombo::FillComboFromDynamicList(WxPropertyControl* InItem, const FString& InDefaultValue)
{
	UProperty* NodeProperty = InItem->GetProperty();
	if (! NodeProperty || ! NodeProperty->HasMetaData(TEXT("DynamicList")))
	{
		return TEXT("");
	}

	// get list name from meta data, it will be used to obtain values
	FString ListName = NodeProperty->GetMetaData(TEXT("DynamicList"));
	TArray<FString> Values;
	FString CurrentTextValue = InDefaultValue;

	FPropertyNode* PropertyNode = InItem->GetPropertyNode();
	check(PropertyNode);
	FPropertyNode* ParentNode = PropertyNode->GetParentNode();
	check(ParentNode);

	// iterate through parent objects until one of them provide values for current list
	FObjectPropertyNode* ObjectNode = ParentNode->FindObjectItemParent();
	for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
	{
		if (UObject* Obj = *Itor)
		{
			Obj->GetDynamicListValues(ListName, Values);
			if (Values.Num() != 0)
			{
				// always add "None" item to list
				const FString& NoneText = TEXT("None");
				Values.AddUniqueItem(NoneText);

				// fill list items
				ComboBox->Clear();
				for (INT i = 0; i < Values.Num(); ++ i)
				{
					ComboBox->Append( *(Values(i)) );
				}

				// if list doesn't contain text value displayed for control, change it to None
				if (!Values.ContainsItem(CurrentTextValue))
				{
					CurrentTextValue = NoneText;
				}

				break;
			}
		}
	}

	return CurrentTextValue;
}

void UPropertyInputCombo::OnSetFocus(WxPropertyControl* InItem)
{
	Super::OnSetFocus(InItem);
	
	// refresh dynamic list when control gets focus
	UProperty* NodeProperty = InItem->GetProperty();
	if (NodeProperty && NodeProperty->HasMetaData(TEXT("DynamicList")))
	{
		FString CurrentTextValue = InItem->GetPropertyText();
		CurrentTextValue = FillComboFromDynamicList(InItem, CurrentTextValue);

		// select item for displayed text
		INT Index = ComboBox->FindString( *CurrentTextValue );
		ComboBox->Select( Index >= 0 ? Index : 0 );
	}
}

/*-----------------------------------------------------------------------------
	UPropertyInputEditInline
-----------------------------------------------------------------------------*/

// Determines if this input proxy supports the specified UProperty.
UBOOL UPropertyInputEditInline::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	if (InTreeNode->IsEditConst())
	{
		return FALSE;
	}

	const WxPropertyControl* PropertyWindowBase = InTreeNode->GetNodeWindow();
	
	return InTreeNode->HasNodeFlags(EPropertyNodeFlags::EditInline);
}

// Handles button clicks from within the item.
void UPropertyInputEditInline::ButtonClick( WxPropertyControl* InItem, ePropButton InButton )
{
	FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
	check(PropertyTreeNode);
	UProperty* NodeProperty = PropertyTreeNode->GetProperty();

	switch( InButton )
	{
		case PB_NewObject:
		{
			UObjectProperty* ObjProp = Cast<UObjectProperty>( NodeProperty );
			UInterfaceProperty* IntProp = Cast<UInterfaceProperty>( NodeProperty );
			check( ObjProp || IntProp);

			wxMenu Menu;

			// Create a popup menu containing the classes that can be created for this property.
			// Add all editinlinenew classes which derive from PropertyClass to the menu.
			// At the same time, build a map of enums to classes which will be used by
			// InItem->OnEditInlineNewClass(...) to identify the selected class.

			INT id = ID_PROP_CLASSBASE_START;
			InItem->ClassMap.Empty();
			for( TObjectIterator<UClass> It ; It ; ++It )
			{
				const UBOOL bChildOfObjectClass = ObjProp && It->IsChildOf(ObjProp->PropertyClass);
				const UBOOL bDerivedInterfaceClass = IntProp && It->ImplementsInterface(IntProp->InterfaceClass);
				if( (bChildOfObjectClass || bDerivedInterfaceClass) && IsClassAllowed(InItem, *It, FALSE) )
				{
					UBOOL bValidEditinlineClass = TRUE;
					for ( TPropObjectIterator Itor( PropertyTreeNode->FindObjectItemParent()->ObjectIterator() ); bValidEditinlineClass && Itor ; ++Itor )
					{
						UObject* OwnerObject = *Itor;
						bValidEditinlineClass = OwnerObject->IsA(It->ClassWithin);
					}

					if ( bValidEditinlineClass )
					{
						if ( id < ID_PROP_CLASSBASE_END )
						{
							InItem->ClassMap.Set( id, *It );
							Menu.Append( id, *It->GetName() );
						}
						++id;
					}
				}
			}


			// also see if an archetype of this class is selected in the GB (if we're in the editor)
			if (ObjProp && GEditor != NULL)
			{
				GCallbackEvent->Send(CALLBACK_LoadSelectedAssetsIfNeeded);
				USelection* SelectedSet = GEditor->GetSelectedSet( ObjProp->PropertyClass );

				if (SelectedSet)
				{
					// If an object of that type is selected, use it.
					UObject* SelectedObject = SelectedSet->GetTop( ObjProp->PropertyClass );
					if (SelectedObject)
					{
						if (SelectedObject->HasAnyFlags(RF_ArchetypeObject))
						{
							InItem->NewObjArchetype = SelectedObject;
							InItem->ClassMap.Set( id, NULL );
							Menu.Append(id, *(FString(TEXT("Instance of ")) + SelectedObject->GetName()));
						}
					}
				}

			}


			checkf(id <= ID_PROP_CLASSBASE_END,TEXT("No more ids available for 'select class' menu items - try increasing the number reserved by ID_PROP_CLASSBASE_END (overflowed by %i IDs)"), id - ID_PROP_CLASSBASE_END);

			// Show the menu to the user, sending messages to InItem.
			FTrackPopupMenu tpm( InItem, &Menu );
			tpm.Show();
		}
		break;

		case PB_Clear:
		{

			FString CurrValue = InItem->GetPropertyText(); // this is used to check for a Null component and if we find one we do not want to pop up the dialog

			if( CurrValue == TEXT("None") )
			{
				return;
			}
			
			// Prompt the user that this action will eliminate existing data.
			const UBOOL bDoReplace = appMsgf( AMT_YesNo, *LocalizeUnrealEd("EditInlineNewAreYouSure") );

			if( bDoReplace )
			{
				SendTextToObjects( InItem, TEXT("None") );
				const UBOOL bExpand = FALSE;
				const UBOOL bRecurse = FALSE;
				PropertyTreeNode->SetExpanded(bExpand, bRecurse);
			}
		}
		break;

		case PB_Use:
			OnUseSelected( InItem );
			break;

		default:
			Super::ButtonClick( InItem, InButton );
			break;
	}
}

/**
 * Wrapper method for determining whether a class is valid for use by this property item input proxy.
 *
 * @param	InItem			the property window item that contains this proxy.
 * @param	CheckClass		the class to verify
 * @param	bAllowAbstract	TRUE if abstract classes are allowed
 *
 * @return	TRUE if CheckClass is valid to be used by this input proxy
 */
UBOOL UPropertyInputEditInline::IsClassAllowed( WxPropertyControl* InItem,  UClass* CheckClass, UBOOL bAllowAbstract ) const
{
	return Super::IsClassAllowed(InItem, CheckClass, bAllowAbstract) && CheckClass->HasAnyClassFlags(CLASS_EditInlineNew);
}


/*-----------------------------------------------------------------------------
UPropertyInputTextEditBox
-----------------------------------------------------------------------------*/

// Determines if this input proxy supports the specified UProperty.
UBOOL UPropertyInputTextEditBox::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{
	const WxPropertyControl* PropertyWindowBase = InTreeNode->GetNodeWindow();
	check(PropertyWindowBase);
	const UProperty* NodeProperty = InTreeNode->GetProperty();

	if (InTreeNode->IsEditConst())
	{
		return FALSE;
	}

	if(	!InTreeNode->HasNodeFlags(EPropertyNodeFlags::EditInline) && NodeProperty->IsA(UStrProperty::StaticClass()) && (NodeProperty->PropertyFlags & CPF_EditTextBox) )
	{
		return TRUE;
	}

	return FALSE;
}

void UPropertyInputTextEditBox::ButtonClick( WxPropertyControl* InItem, ePropButton InButton )
{
	switch( InButton )
	{
		case PB_TextBox:
			if( DlgTextEditBox == NULL )
			{
				UProperty* NodeProperty = InItem->GetProperty();
				DlgTextEditBox = new WxDlgPropertyTextEditBox( InItem, this, *NodeProperty->GetName(), *InitialValue );
			}
			DlgTextEditBox->Show();
			break;
		default:
			Super::ButtonClick( InItem, InButton );
			break;
	}
}

// Allows the created controls to react to the parent window being resized.  Derived classes
// that implement this method should to call up to their parent.
void UPropertyInputTextEditBox::CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue )
{
	InitialValue = InValue;

	// Create any buttons for the control.
	Super::CreateControls( InItem, InBaseClass, InRC, InValue );

	// Create a new button and add it to the button array.
	if( InValue[0] != 0 )
	{
		TextButton = new wxButton( InItem, ID_PROP_PB_TEXTBOX, InValue, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxBU_AUTODRAW | wxBU_LEFT );
	}
	else
	{
		TextButton = new wxButton( InItem, ID_PROP_PB_TEXTBOX, *LocalizeUnrealEd("PropertyWindow_TextBox"), wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxBU_AUTODRAW );
	}
	TextButton->SetToolTip( *LocalizeUnrealEd("PropertyWindow_TextBox") );
	
	// Notify the parent.
	ParentResized( InItem, InRC );
}

// Deletes any controls which were created for editing.
void UPropertyInputTextEditBox::DeleteControls()
{
	// Delete any buttons on the control.
	Super::DeleteControls();

	if( DlgTextEditBox )
	{
		DlgTextEditBox->Destroy();
		DlgTextEditBox = NULL;
	}

	if( TextButton )
	{
		TextButton->Destroy();
		TextButton = NULL;
	}
}

// Allows the created controls to react to the parent window being resized.
void UPropertyInputTextEditBox::ParentResized( WxPropertyControl* InItem, const wxRect& InRC )
{
	Super::ParentResized(InItem, InRC);

	if( TextButton )
	{
		INT YPos = InRC.height / 2 - GetButtonSize() / 2;
		TextButton->SetSize(InRC.x, YPos, InRC.GetWidth(), GetButtonSize());
	}
}

// Sends the current value in the input control to the selected objects.
void UPropertyInputTextEditBox::SendToObjects( WxPropertyControl* InItem )
{
	if( !DlgTextEditBox )
	{
		return;
	}

	FString Value = DlgTextEditBox->GetValue();

	// If more than one object is selected, an empty field indicates their values for this property differ.
	// Don't send it to the objects value in this case (if we did, they would all get set to None which isn't good).
	FObjectPropertyNode* ObjectNode = InItem->GetPropertyNode()->FindObjectItemParent();

	if( ObjectNode->GetNumObjects() == 1 || Value.Len() )
	{
		FPropertyNode* PropertyTreeNode = InItem->GetPropertyNode();
		// Build up a list of objects to modify.
		TArray<FObjectBaseAddress> ObjectsToModify;
		for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
		{
			UObject*	Object = *Itor;
			BYTE*		Addr = PropertyTreeNode->GetValueBaseAddress( (BYTE*) Object );
			ObjectsToModify.AddItem( FObjectBaseAddress( Object, Addr ) );
		}

		ImportText( InItem, ObjectsToModify, Value );
	}
}

// Refreshes the value in the controls created for this property.
void UPropertyInputTextEditBox::RefreshControlValue( UProperty* InProperty, BYTE* InData )
{
	FString Wk;
	InProperty->ExportText( 0, Wk, InData - InProperty->Offset, InData - InProperty->Offset, NULL, PPF_Localized );
	if( DlgTextEditBox )
	{
		DlgTextEditBox->SetValue( *Wk );
	}
	if( TextButton )
	{
		if( Wk.Len() > 0 )
		{
			TextButton->SetWindowStyle( wxWANTS_CHARS | wxBU_AUTODRAW | wxBU_LEFT );
			TextButton->SetLabel( *Wk );
		}
		else
		{
			TextButton->SetWindowStyle( wxWANTS_CHARS | wxBU_AUTODRAW );
			TextButton->SetLabel( *LocalizeUnrealEd("PropertyWindow_TextBox") );
		}		
	}
}

void UPropertyInputTextEditBox::NotifyHideShowChildren(UBOOL bShow)
{
	if( !bShow && DlgTextEditBox )
	{
		DlgTextEditBox->Close();
	}
}

/*-----------------------------------------------------------------------------
	Class implementations.
	
	Done here to control the order they are iterated later on.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UPropertyDrawArrayHeader);
IMPLEMENT_CLASS(UPropertyDrawProxy);
IMPLEMENT_CLASS(UPropertyDrawNumeric);
IMPLEMENT_CLASS(UPropertyDrawRangedNumeric);
IMPLEMENT_CLASS(UPropertyDrawColor);
IMPLEMENT_CLASS(UPropertyDrawRotation);
IMPLEMENT_CLASS(UPropertyDrawRotationHeader);
IMPLEMENT_CLASS(UPropertyDrawBool);
IMPLEMENT_CLASS(UPropertyDrawTextEditBox);
IMPLEMENT_CLASS(UPropertyDrawMultilineTextBox);

IMPLEMENT_CLASS(UPropertyInputProxy);
IMPLEMENT_CLASS(UPropertyInputArray);
IMPLEMENT_CLASS(UPropertyInputArrayItemBase);
IMPLEMENT_CLASS(UPropertyInputColor);
IMPLEMENT_CLASS(UPropertyInputBool);
IMPLEMENT_CLASS(UPropertyInputArrayItem);
IMPLEMENT_CLASS(UPropertyInputNumeric);
IMPLEMENT_CLASS(UPropertyInputRangedNumeric);
IMPLEMENT_CLASS(UPropertyInputText);
IMPLEMENT_CLASS(UPropertyInputTextEditBox);
IMPLEMENT_CLASS(UPropertyInputRotation);
IMPLEMENT_CLASS(UPropertyInputInteger);
IMPLEMENT_CLASS(UPropertyInputEditInline);
IMPLEMENT_CLASS(UPropertyInputCombo);
IMPLEMENT_CLASS(UPropertyInput_DynamicCombo);