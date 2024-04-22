/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ConvertersCLR_h__
#define __ConvertersCLR_h__

#ifdef _MSC_VER
	#pragma once
#endif

#ifndef __cplusplus_cli
	#error "This file must be compiled as managed code using the /clr compiler option."
#endif

using namespace System;
using namespace System::Globalization;
using namespace System::Windows::Data;

/** Negates a boolean value */
[ValueConversion(bool::typeid, bool::typeid)]
ref class NegatedBooleanConverter : public IValueConverter
{

public:
    
	virtual Object^ Convert( Object^ value, Type^ targetType, Object^ parameter, Globalization::CultureInfo^ culture )
    {
		return !(bool)value;
    }

    virtual Object^ ConvertBack( Object^ value, Type^ targetType, Object^ parameter, Globalization::CultureInfo^ culture )
    {
		return !(bool)value;
    }
};

/** Converts doubles to strings and back */
[ValueConversion(double::typeid, String::typeid)]
ref class DoubleToStringConverter : public IValueConverter
{

public:
    
	virtual Object^ Convert( Object^ value, Type^ targetType, Object^ parameter, Globalization::CultureInfo^ culture )
    {
		double Val = (Double)value;
		return Val.ToString( "F2" );
    }

    virtual Object^ ConvertBack( Object^ value, Type^ targetType, Object^ parameter, Globalization::CultureInfo^ culture )
    {
        String^ Str = (String^)value;
        double Val = 0.0f;
		double::TryParse( Str, Val );
		return Val;
    }
};

/** Converts doubles to rounded integers and back */
[ValueConversion(int::typeid, double::typeid)]
ref class RoundedIntToDoubleConverter : public IValueConverter
{

public:
    
	virtual Object^ Convert( Object^ value, Type^ targetType, Object^ parameter, Globalization::CultureInfo^ culture )
    {
		return (double)(safe_cast<int>(value));
    }

    virtual Object^ ConvertBack( Object^ value, Type^ targetType, Object^ parameter, Globalization::CultureInfo^ culture )
    {
		return (int)(Math::Round(safe_cast<Double>(value)));
    }
};

/** Converts an integer to another integer but applies arithmetic */
[ValueConversion(Int32::typeid, Int32::typeid)]
ref class IntToIntOffsetConverter : public IValueConverter
{

public:

	IntToIntOffsetConverter( int InOffset )
		: Offset( InOffset )
	{
	}
    
	virtual Object^ Convert( Object^ value, Type^ targetType, Object^ parameter, Globalization::CultureInfo^ culture )
    {
		Int32 Val = (Int32)value;
		return Val + Offset;
    }

    virtual Object^ ConvertBack( Object^ value, Type^ targetType, Object^ parameter, Globalization::CultureInfo^ culture )
    {
        Int32 Val = (Int32)value;
		return Val - Offset;
    }


private:

	/** Offset to add when converting to the type, or subtract when converting back */
	int Offset;

};

#endif	//__ConvertersCLR_h__
