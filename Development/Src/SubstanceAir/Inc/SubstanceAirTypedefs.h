//! @file SubstanceAirTypedefs.h
//! @brief Typedefs for the UE3 Integration
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_INTEGRATION_TYPEDEFS_H
#define _SUBSTANCE_AIR_INTEGRATION_TYPEDEFS_H

#include "Engine.h"

#include "framework/std_wrappers.h"
#include "SubstanceAirUncopyable.h"

#include <vector>

#define SBS_DEFAULT_MEMBUDGET_MB (32)
#define SBS_DFT_COOKED_MIPS_NB (5)

#if !FINAL_RELEASE
#define SBS_VERBOSE 1
#endif

//! A few forward declaration
namespace SubstanceAir
{
	typedef std::vector<UINT> Uids;

	struct FPackage;

	struct FGraphDesc;
	struct FOutputDesc;
	struct FInputDescBase;
	struct FImageInputDesc;

	struct FGraphInstance;
	struct FOutputInstance;
	struct FInputInstanceBase;
	struct FNumericalInputInstanceBase;
	struct FImageInputInstance;

	struct FPreset;

	//! @brief The list of channels roles available in Designer
	enum ChannelUse
	{
		CHAN_Undef				=0,
		CHAN_Diffuse,
		CHAN_Opacity,
		CHAN_Emissive,
		CHAN_Ambient,
		CHAN_AmbientOcclusion,
		CHAN_Mask,
		CHAN_Normal,
		CHAN_Bump,
		CHAN_Height,
		CHAN_Displacement,
		CHAN_Specular,
		CHAN_SpecularLevel,
		CHAN_SpecularColor,
		CHAN_Glossiness,
		CHAN_Roughness,
		CHAN_AnisotropyLevel,
		CHAN_AnisotropyAngle,
		CHAN_Transmissive,
		CHAN_Reflection,
		CHAN_Refraction,
		CHAN_Environment,
		CHAN_IOR,
		CU_SCATTERING0,
		CU_SCATTERING1,
		CU_SCATTERING2,
		CU_SCATTERING3,
		CHAN_MAX
	};
} // namespace SubstanceAir


//! @brief Pair class
template<class T, class U>
struct pair_t
{
	typedef std::pair<T, U> Type;
};


//! @brief Aligned allocator
template <class T>
struct aligned_allocator : std::allocator<T>
{
	template<class U>
	struct rebind { typedef aligned_allocator<U> other; };
	
	aligned_allocator()
	{
	}
	
	template<class Other>
	aligned_allocator(const aligned_allocator<Other>&)
	{
	}
	
	aligned_allocator(const aligned_allocator<T>&)
	{
	}

	typedef std::allocator<T> base;

	typedef typename base::pointer pointer;
	typedef typename base::size_type size_type;

	pointer allocate(size_type n)
	{
		return (pointer)_aligned_malloc(n*sizeof(T),16);
	}

	pointer allocate(size_type n, void const*)
	{
		return this->allocate(n);
	}

	void deallocate(pointer p, size_type)
	{
		_aligned_free(p);
	}
};


struct FIntVector2
{
	INT X, Y;
	FIntVector2()
		:	X( 0 )
		,	Y( 0 )
	{}
	FIntVector2( INT InX, INT InY )
		:	X( InX )
		,	Y( InY )
	{}
	static FIntVector2 ZeroValue()
	{
		return FIntVector2(0,0);
	}
	const INT& operator()( INT i ) const
	{
		return (&X)[i];
	}
	INT& operator()( INT i )
	{
		return (&X)[i];
	}
	const INT& operator[]( INT i ) const
	{
		return (&X)[i];
	}
	INT& operator[]( INT i )
	{
		return (&X)[i];
	}
	static INT Num()
	{
		return 2;
	}
	UBOOL operator==( const FIntVector2& Other ) const
	{
		return X==Other.X && Y==Other.Y;
	}
	UBOOL operator!=( const FIntVector2& Other ) const
	{
		return X!=Other.X || Y!=Other.Y;
	}
	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FIntVector2& V )
	{
		return Ar << V.X << V.Y;
	}
};


struct FIntVector3
{
	INT X, Y, Z;
	FIntVector3()
		:	X( 0 )
		,	Y( 0 )
		,	Z( 0 )
	{}
	FIntVector3( INT InX, INT InY, INT InZ )
		:	X( InX )
		,	Y( InY )
		,	Z( InZ )
	{}
	static FIntVector3 ZeroValue()
	{
		return FIntVector3(0,0,0);
	}
	const INT& operator()( INT i ) const
	{
		return (&X)[i];
	}
	INT& operator()( INT i )
	{
		return (&X)[i];
	}
	INT& operator[]( INT i )
	{
		return (&X)[i];
	}
	const INT& operator[]( INT i ) const
	{
		return (&X)[i];
	}
	static INT Num()
	{
		return 3;
	}
	UBOOL operator==( const FIntVector3& Other ) const
	{
		return X==Other.X && Y==Other.Y && Z==Other.Z;
	}
	UBOOL operator!=( const FIntVector3& Other ) const
	{
		return X!=Other.X || Y!=Other.Y || Z!=Other.Z;
	}
	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FIntVector3& V )
	{
		return Ar << V.X << V.Y << V.Z;
	}
};


struct FIntVector4
{
	INT X, Y, Z, W;
	FIntVector4()
		:	X( 0 )
		,	Y( 0 )
		,	Z( 0 )
		,	W( 0 )
	{}
	FIntVector4( INT InX, INT InY, INT InZ, INT InW )
		:	X( InX )
		,	Y( InY )
		,	Z( InZ )
		,	W( InW )
	{}
	static FIntVector4 ZeroValue()
	{
		return FIntVector4(0,0,0,0);
	}
	const INT& operator()( INT i ) const
	{
		return (&X)[i];
	}
	INT& operator()( INT i )
	{
		return (&X)[i];
	}
	INT& operator[]( INT i )
	{
		return (&X)[i];
	}
	const INT& operator[]( INT i ) const 
	{
		return (&X)[i];
	}
	static INT Num()
	{
		return 4;
	}
	UBOOL operator==( const FIntVector4& Other ) const
	{
		return X==Other.X && Y==Other.Y && Z==Other.Z && W==Other.W;
	}
	UBOOL operator!=( const FIntVector4& Other ) const
	{
		return X!=Other.X || Y!=Other.Y || Z!=Other.Z || W!=Other.W;
	}
	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FIntVector4& V )
	{
		return Ar << V.X << V.Y << V.Z << V.W;
	}
};


typedef INT int_t;
//! @note DWORD because UINT is not allowed as a key in TMap
typedef DWORD uint_t;
typedef FLOAT float_t;
typedef UBOOL bool_t;

typedef FGuid guid_t;

struct guid_t_comp {
	bool operator() (const guid_t& lhs, const guid_t& rhs) const
	{
		return GetTypeHash(lhs)<GetTypeHash(rhs);
	}
};

typedef FString input_hash_t;
typedef TArray<BYTE> binary_t;
//! @brief Only used during runtime to store sbsbin
typedef std::vector< unsigned char, aligned_allocator < unsigned char > > aligned_binary_t;

typedef FVector2D vec2float_t;
typedef FVector vec3float_t;
typedef FVector4 vec4float_t;

//! @brief Using float vector for the moment
typedef FIntVector2 vec2int_t;
typedef FIntVector3 vec3int_t;
typedef FIntVector4 vec4int_t;

typedef struct SubstanceAir::FGraphDesc	graph_desc_t;
typedef struct SubstanceAir::FGraphInstance	graph_inst_t;

typedef struct SubstanceAir::FOutputDesc output_desc_t;
typedef struct SubstanceAir::FInputDescBase	input_desc_t;

typedef std::tr1::shared_ptr<input_desc_t> input_desc_ptr;

typedef struct SubstanceAir::FOutputInstance output_inst_t;
typedef struct SubstanceAir::FInputInstanceBase	input_inst_t;
typedef struct SubstanceAir::FNumericalInputInstanceBase num_input_inst_t;
typedef struct SubstanceAir::FImageInputInstance img_input_inst_t;

typedef struct SubstanceAir::FPackage package_t;

typedef struct SubstanceAir::FPreset preset_t;
//! @brief Container of presets
typedef TArray<preset_t> presets_t;

typedef std::tr1::shared_ptr< input_inst_t > input_inst_t_ptr;

//! @brief Serialization functions for the UE3
FArchive& operator<<(FArchive& Ar, std::tr1::shared_ptr< input_inst_t >& I);
FArchive& operator<<(FArchive& Ar, output_inst_t& O);
FArchive& operator<<(FArchive& Ar, graph_inst_t& G);
FArchive& operator<<(FArchive& Ar, output_desc_t& O);
FArchive& operator<<(FArchive& Ar, std::tr1::shared_ptr< input_desc_t >& I);
FArchive& operator<<(FArchive& Ar, package_t*& P);
FArchive& operator<<(FArchive& Ar, graph_desc_t*& G);
FArchive& operator<<(FArchive& Ar, graph_desc_t& G);

#define _LINENAME_CONCAT( _name_, _line_ ) _name_##_line_
#define _LINENAME(_name_, _line_) _LINENAME_CONCAT(_name_,_line_)
#define _UNIQUE_VAR(_name_) _LINENAME(_name_,__LINE__)

/* This is a lightweight foreach loop */
#define SBS_VECTOR_FOREACH(var, vec) \
	for (size_t _UNIQUE_VAR(__i)=0, _UNIQUE_VAR(__j)=0; _UNIQUE_VAR(__i)<vec.size(); _UNIQUE_VAR(__i)++) for (var = vec[_UNIQUE_VAR(__i)]; _UNIQUE_VAR(__j)==_UNIQUE_VAR(__i); _UNIQUE_VAR(__j)++)

#define SBS_VECTOR_REVERSE_FOREACH(var, vec) \
	for (size_t _UNIQUE_VAR(__i)=vec.size(), _UNIQUE_VAR(__j)=vec.size(); _UNIQUE_VAR(__i)!=0; _UNIQUE_VAR(__i)--) for (var = vec[_UNIQUE_VAR(__i)-1]; _UNIQUE_VAR(__j)==_UNIQUE_VAR(__i); _UNIQUE_VAR(__j)--)

#endif //_SUBSTANCE_AIR_INTEGRATION_TYPEDEFS_H
