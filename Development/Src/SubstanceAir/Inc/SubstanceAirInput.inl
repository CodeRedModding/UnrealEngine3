//! @file SubstanceAirInput.inl
//! @brief Substance Air inline implementation
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#pragma pack ( push, 8 )
#include <substance/substance.h>
#pragma pack ( pop )

namespace SubstanceAir
{

template < typename T > void FNumericalInputInstanceBase::GetValue(TArray< T >& OutValue)
{
	switch((SubstanceInputType)Type)
	{
	case Substance_IType_Float:
		{
			FNumericalInputInstance<float_t>* Input = 
				(FNumericalInputInstance<float_t>*)this;
			OutValue.Push(Input->Value);
		}
		break;
	case Substance_IType_Float2:
		{
			FNumericalInputInstance<vec2float_t>* Input = 
				(FNumericalInputInstance<vec2float_t>*)this;
			OutValue.Push(Input->Value.X);
			OutValue.Push(Input->Value.Y);
		}
		break;
	case Substance_IType_Float3:
		{
			FNumericalInputInstance<vec3float_t>* Input = 
				(FNumericalInputInstance<vec3float_t>*)this;
			OutValue.Push(Input->Value.X);
			OutValue.Push(Input->Value.Y);
			OutValue.Push(Input->Value.Z);
		}
		break;
	case Substance_IType_Float4:
		{
			FNumericalInputInstance<vec4float_t>* Input = 
				(FNumericalInputInstance<vec4float_t>*)this;
			OutValue.Push(Input->Value.X);
			OutValue.Push(Input->Value.Y);
			OutValue.Push(Input->Value.Z);
			OutValue.Push(Input->Value.W);
		}
		break;
	case Substance_IType_Integer:
		{
			FNumericalInputInstance<int_t>* Input = 
				(FNumericalInputInstance<int_t>*)this;
			OutValue.Push(Input->Value);			
		}
		break;
	case Substance_IType_Integer2:
		{
			FNumericalInputInstance<vec2int_t>* Input = 
				(FNumericalInputInstance<vec2int_t>*)this;
			OutValue.Push(Input->Value.X);
			OutValue.Push(Input->Value.Y);
		}
		break;
	case Substance_IType_Integer3:
		{
			FNumericalInputInstance<vec3int_t>* Input = 
				(FNumericalInputInstance<vec3int_t>*)this;
			OutValue.Push(Input->Value.X);
			OutValue.Push(Input->Value.Y);
			OutValue.Push(Input->Value.Z);
		}
		break;
	case Substance_IType_Integer4:
		{
			FNumericalInputInstance<vec4int_t>* Input = 
				(FNumericalInputInstance<vec4int_t>*)this;
			OutValue.Push(Input->Value.X);
			OutValue.Push(Input->Value.Y);
			OutValue.Push(Input->Value.Z);
			OutValue.Push(Input->Value.W);
		}
		break;
	default:
		break;
	}
}


template < typename T > void FNumericalInputInstanceBase::SetValue(const TArray< T > & InValue)
{
	switch((SubstanceInputType)Type)
	{
	case Substance_IType_Float:
		{
			FNumericalInputInstance<float_t>* Input = 
				(FNumericalInputInstance<float_t>*)this;

			if (InValue.Num() >= 1)
			{
				Input->Value = InValue(0);
			}			
		}
		break;
	case Substance_IType_Float2:
		{
			FNumericalInputInstance<vec2float_t>* Input = 
				(FNumericalInputInstance<vec2float_t>*)this;

			if (InValue.Num() >= 2)
			{
				Input->Value.X = InValue(0);
				Input->Value.Y = InValue(1);
			}
		}
		break;
	case Substance_IType_Float3:
		{
			FNumericalInputInstance<vec3float_t>* Input = 
				(FNumericalInputInstance<vec3float_t>*)this;

			if (InValue.Num() >= 3)
			{
				Input->Value.X = InValue(0);
				Input->Value.Y = InValue(1);
				Input->Value.Z = InValue(2);
			}
		}
		break;
	case Substance_IType_Float4:
		{
			FNumericalInputInstance<vec4float_t>* Input = 
				(FNumericalInputInstance<vec4float_t>*)this;

			if (InValue.Num() >= 4)
			{
				Input->Value.X = InValue(0);
				Input->Value.Y = InValue(1);
				Input->Value.Z = InValue(2);
				Input->Value.W = InValue(3);
			}
		}
		break;
	case Substance_IType_Integer:
		{
			FNumericalInputInstance<int_t>* Input = 
				(FNumericalInputInstance<int_t>*)this;

			if (InValue.Num() >= 1)
			{
				Input->Value = (int_t) InValue(0);
			}
		}
		break;
	case Substance_IType_Integer2:
		{
			FNumericalInputInstance<vec2int_t>* Input = 
				(FNumericalInputInstance<vec2int_t>*)this;

			if (InValue.Num() >= 2)
			{
				Input->Value.X = (int_t) InValue(0);
				Input->Value.Y = (int_t) InValue(1);
			}
		}
		break;
	case Substance_IType_Integer3:
		{
			FNumericalInputInstance<vec3int_t>* Input = 
				(FNumericalInputInstance<vec3int_t>*)this;

			if (InValue.Num() >= 3)
			{
				Input->Value.X = (int_t) InValue(0);
				Input->Value.Y = (int_t) InValue(1);
				Input->Value.Z = (int_t) InValue(2);
			}
		}
		break;
	case Substance_IType_Integer4:
		{
			FNumericalInputInstance<vec4int_t>* Input = 
				(FNumericalInputInstance<vec4int_t>*)this;

			if (InValue.Num() >= 4)
			{
				Input->Value.X = (int_t) InValue(0);
				Input->Value.Y = (int_t) InValue(1);
				Input->Value.Z = (int_t) InValue(2);
				Input->Value.W = (int_t) InValue(3);
			}
		}
		break;
	default:
		break;
	}
}

} // namespace SubstanceAir
