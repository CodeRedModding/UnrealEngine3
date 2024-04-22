/**
 * These need to be in here due to gcc's explicit specialization in non-namespace scope demands
 *
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


#ifndef _MATERIAL_INSTANCE_TIME_VARYING_HELPERS_H_
#define _MATERIAL_INSTANCE_TIME_VARYING_HELPERS_H_



template< typename ARRAY_TYPE >
void UpdateParameterValueOverTime( ARRAY_TYPE& ToModify, UBOOL InbLoop, UBOOL InbAutoActivate, FLOAT InCycleTime, UBOOL InbNormalizeTime, FLOAT InOffsetTime, UBOOL InbOffsetFromEnd )
{
	ToModify.bLoop = InbLoop;
	ToModify.bAutoActivate = InbAutoActivate;
	ToModify.CycleTime = InCycleTime;
	ToModify.bNormalizeTime = InbNormalizeTime;

	ToModify.OffsetTime = InOffsetTime;
	ToModify.bOffsetFromEnd = InbOffsetFromEnd;

}

template< typename MI_TYPE, typename ARRAY_TYPE > 
TArrayNoInit<ARRAY_TYPE>& GetArray( MI_TYPE* );

// 	template<> void GetArray<UMaterialInstanceTimeVarying, FFontParameterValueOverTime>( const UMaterialInstanceTimeVarying* MITV, TArray<FFontParameterValueOverTime>& TheArray ) { TheArray = MITV->FontParameterValues; }
// 
template<> TArrayNoInit<FFontParameterValueOverTime>& GetArray<UMaterialInstanceTimeVarying, FFontParameterValueOverTime>( UMaterialInstanceTimeVarying* MITV ) { return *&MITV->FontParameterValues; }

template<> TArrayNoInit<FScalarParameterValueOverTime>& GetArray<UMaterialInstanceTimeVarying, FScalarParameterValueOverTime>( UMaterialInstanceTimeVarying* MITV ) { return *&MITV->ScalarParameterValues; }

template<> TArrayNoInit<FTextureParameterValueOverTime>& GetArray<UMaterialInstanceTimeVarying, FTextureParameterValueOverTime>( UMaterialInstanceTimeVarying* MITV ) { return *&MITV->TextureParameterValues; }

template<> TArrayNoInit<FVectorParameterValueOverTime>& GetArray<UMaterialInstanceTimeVarying, FVectorParameterValueOverTime>( UMaterialInstanceTimeVarying* MITV ) { return *&MITV->VectorParameterValues; }

template<> TArrayNoInit<FLinearColorParameterValueOverTime>& GetArray<UMaterialInstanceTimeVarying, FLinearColorParameterValueOverTime>( UMaterialInstanceTimeVarying* MITV ) { return *&MITV->LinearColorParameterValues; }


template< typename MI_TYPE, typename ARRAY_TYPE >
void UpdateParameterValueOverTimeValues( MI_TYPE* MI, const FName& ParameterName, UBOOL InbLoop, UBOOL InbAutoActivate, FLOAT InCycleTime, UBOOL InbNormalizeTime, FLOAT InOffsetTime, UBOOL InbOffsetFromEnd )
{
	TArrayNoInit<ARRAY_TYPE>& TheArray = GetArray<MI_TYPE, ARRAY_TYPE>( MI );

	// Check for an existing value for the named parameter in the array.
	for( INT ValueIndex = 0; ValueIndex < TheArray.Num(); ++ValueIndex )
	{
		if( TheArray(ValueIndex).ParameterName == ParameterName )
		{
			UpdateParameterValueOverTime<ARRAY_TYPE>( TheArray(ValueIndex), InbLoop, InbAutoActivate, InCycleTime, InbNormalizeTime, InOffsetTime, InbOffsetFromEnd );
			break;
		}
	}
}


#endif // _MATERIAL_INSTANCE_TIME_VARYING_HELPERS_H_


