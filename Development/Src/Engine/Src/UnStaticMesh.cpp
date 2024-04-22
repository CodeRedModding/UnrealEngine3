/*=============================================================================
	UnStaticMesh.cpp: Static mesh class implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineDecalClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineProcBuildingClasses.h"
#include "EngineSplineClasses.h"
#include "EngineMeshClasses.h"
#include "EngineFoliageClasses.h"

#include "TessellationRendering.h"

IMPLEMENT_CLASS(AStaticMeshActorBase);
IMPLEMENT_CLASS(AStaticMeshActor);
IMPLEMENT_CLASS(AStaticMeshCollectionActor);
IMPLEMENT_CLASS(ADynamicSMActor);
IMPLEMENT_CLASS(UStaticMesh);
IMPLEMENT_CLASS(AStaticMeshActorBasedOnExtremeContent);
	

/** Package name, that if set will cause only static meshes in that package to be rebuilt based on SM version. */
FName GStaticMeshPackageNameToRebuild = NAME_None;

/*-----------------------------------------------------------------------------
	FStaticMeshTriangleBulkData (FStaticMeshTriangle version of bulk data)
-----------------------------------------------------------------------------*/

/**
 * Returns size in bytes of single element.
 *
 * @return Size in bytes of single element
 */
INT FStaticMeshTriangleBulkData::GetElementSize() const
{
	return sizeof(FStaticMeshTriangle);
} 

/**
 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
 *
 * @warning BulkSerialize: FStaticMeshTriangle is serialized as memory dump
 * See TArray::BulkSerialize for detailed description of implied limitations.
 * 
 * @param Ar			Archive to serialize with
 * @param Data			Base pointer to data
 * @param ElementIndex	Element index to serialize
 */
void FStaticMeshTriangleBulkData::SerializeElement( FArchive& Ar, void* Data, INT ElementIndex )
{
	FStaticMeshTriangle& StaticMeshTriangle = *((FStaticMeshTriangle*)Data + ElementIndex);
	Ar << StaticMeshTriangle.Vertices[0];
	Ar << StaticMeshTriangle.Vertices[1];
	Ar << StaticMeshTriangle.Vertices[2];

	for( INT VertexIndex=0; VertexIndex<3; VertexIndex++ )
	{
		for( INT UVIndex=0; UVIndex<8; UVIndex++ )
		{
			Ar << StaticMeshTriangle.UVs[VertexIndex][UVIndex];
		}
	}

	Ar << StaticMeshTriangle.Colors[0];
	Ar << StaticMeshTriangle.Colors[1];
	Ar << StaticMeshTriangle.Colors[2];
	Ar << StaticMeshTriangle.MaterialIndex;
	if (Ar.Ver() < VER_STATICMESH_FRAGMENTINDEX)
	{
		StaticMeshTriangle.FragmentIndex = 0;
	}
	else
	{
		Ar << StaticMeshTriangle.FragmentIndex;
	}
	Ar << StaticMeshTriangle.SmoothingMask;
	Ar << StaticMeshTriangle.NumUVs;

	if(Ar.Ver() >= VER_STATIC_MESH_EXPLICIT_NORMALS)
	{
		// Serialization fixed up with this package version. bExplicitNormals needs to be serialized last.
		if (Ar.Ver() < VER_STATIC_MESH_SOURCE_DATA_COPY)
		{
			Ar << StaticMeshTriangle.bExplicitNormals;
		}
	}
	else
	{
		StaticMeshTriangle.bExplicitNormals = FALSE;
	}

	if(Ar.Ver() >= VER_OVERRIDETANGENTBASIS)
	{
		Ar << StaticMeshTriangle.TangentX[0];
		Ar << StaticMeshTriangle.TangentX[1];
		Ar << StaticMeshTriangle.TangentX[2];
		Ar << StaticMeshTriangle.TangentY[0];
		Ar << StaticMeshTriangle.TangentY[1];
		Ar << StaticMeshTriangle.TangentY[2];
		Ar << StaticMeshTriangle.TangentZ[0];
		Ar << StaticMeshTriangle.TangentZ[1];
		Ar << StaticMeshTriangle.TangentZ[2];

		Ar << StaticMeshTriangle.bOverrideTangentBasis;
	}
	else if(Ar.IsLoading())
	{
		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
		{
			StaticMeshTriangle.TangentX[VertexIndex] = FVector(0,0,0);
			StaticMeshTriangle.TangentY[VertexIndex] = FVector(0,0,0);
			StaticMeshTriangle.TangentZ[VertexIndex] = FVector(0,0,0);
		}

		if(Ar.Ver() >= VER_STATICMESH_EXTERNAL_VERTEX_NORMALS)
		{
			Ar << StaticMeshTriangle.TangentZ[0];
			Ar << StaticMeshTriangle.TangentZ[1];
			Ar << StaticMeshTriangle.TangentZ[2];

			Ar << StaticMeshTriangle.bOverrideTangentBasis;
		}
		else
		{
			StaticMeshTriangle.bOverrideTangentBasis = FALSE;
		}
	}

	// Serialization fixed up with this package version. bExplicitNormals needs to be serialized last.
	if (Ar.Ver() >= VER_STATIC_MESH_SOURCE_DATA_COPY)
	{
		Ar << StaticMeshTriangle.bExplicitNormals;
	}
}

/**
 * Returns whether single element serialization is required given an archive. This e.g.
 * can be the case if the serialization for an element changes and the single element
 * serialization code handles backward compatibility.
 */
UBOOL FStaticMeshTriangleBulkData::RequiresSingleElementSerialization( FArchive& Ar )
{
	if( Ar.Ver() < VER_STATIC_MESH_SOURCE_DATA_COPY )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/** The implementation of the static mesh vertex data storage type. */
template<typename VertexDataType>
class TStaticMeshVertexData :
	public FStaticMeshVertexDataInterface,
	public TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT>
{
public:

	typedef TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT> ArrayType;

	/**
	* Constructor
	* @param InNeedsCPUAccess - TRUE if resource array data should be CPU accessible
	*/
	TStaticMeshVertexData(UBOOL InNeedsCPUAccess=FALSE)
		:	TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT>(InNeedsCPUAccess)
	{
	}

	/**
	* Resizes the vertex data buffer, discarding any data which no longer fits.
	*
	* @param NumVertices - The number of vertices to allocate the buffer for.
	*/
	virtual void ResizeBuffer(UINT NumVertices)
	{
		if((UINT)ArrayType::Num() < NumVertices)
		{
			// Enlarge the array.
			ArrayType::Add(NumVertices - ArrayType::Num());
		}
		else if((UINT)ArrayType::Num() > NumVertices)
		{
			// Shrink the array.
			ArrayType::Remove(NumVertices,ArrayType::Num() - NumVertices);
		}
	}
	/**
	* @return stride of the vertex type stored in the resource data array
	*/
	virtual UINT GetStride() const
	{
		return sizeof(VertexDataType);
	}
	/**
	* @return BYTE pointer to the resource data array
	*/
	virtual BYTE* GetDataPointer()
	{
		return (BYTE*)&(*this)(0);
	}
	/**
	* @return resource array interface access
	*/
	virtual FResourceArrayInterface* GetResourceArray()
	{
		return this;
	}
	/**
	* Serializer for this class
	*
	* @param Ar - archive to serialize to
	* @param B - data to serialize
	*/
	virtual void Serialize(FArchive& Ar)
	{
		TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT>::BulkSerialize(Ar);
	}
	/**
	* Assignment operator. This is currently the only method which allows for 
	* modifying an existing resource array
	*/
	TStaticMeshVertexData<VertexDataType>& operator=(const TArray<VertexDataType>& Other)
	{
		TResourceArray<VertexDataType,VERTEXBUFFER_ALIGNMENT>::operator=(Other);
		return *this;
	}
};



/*-----------------------------------------------------------------------------
	FPositionVertexBuffer
-----------------------------------------------------------------------------*/

/** The implementation of the static mesh position-only vertex data storage type. */
class FPositionVertexData :
	public TStaticMeshVertexData<FPositionVertex>
{
public:
	FPositionVertexData( UBOOL InNeedsCPUAccess=FALSE )
		: TStaticMeshVertexData<FPositionVertex>( InNeedsCPUAccess )
	{
	}
};


FPositionVertexBuffer::FPositionVertexBuffer():
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{}

FPositionVertexBuffer::~FPositionVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FPositionVertexBuffer::CleanUp()
{
	if (VertexData)
	{
		delete VertexData;
		VertexData = NULL;
	}
}

/**
 * Initializes the buffer with the given vertices, used to convert legacy layouts.
 * @param InVertices - The vertices to initialize the buffer with.
 */
void FPositionVertexBuffer::Init(const TArray<FStaticMeshBuildVertex>& InVertices)
{
	NumVertices = InVertices.Num();

	// Allocate the vertex data storage type.
	AllocateData();

	// Allocate the vertex data buffer.
	VertexData->ResizeBuffer(NumVertices);
	Data = VertexData->GetDataPointer();

	// Copy the vertices into the buffer.
	for(INT VertexIndex = 0;VertexIndex < InVertices.Num();VertexIndex++)
	{
		const FStaticMeshBuildVertex& SourceVertex = InVertices(VertexIndex);
		const UINT DestVertexIndex = VertexIndex;
		VertexPosition(DestVertexIndex) = SourceVertex.Position;
	}
}

/**
 * Initializes this vertex buffer with the contents of the given vertex buffer.
 * @param InVertexBuffer - The vertex buffer to initialize from.
 */
void FPositionVertexBuffer::Init(const FPositionVertexBuffer& InVertexBuffer)
{
	NumVertices = InVertexBuffer.GetNumVertices();
	if ( NumVertices )
	{
		AllocateData();
		check( Stride == InVertexBuffer.GetStride() );
		VertexData->ResizeBuffer(NumVertices);
		Data = VertexData->GetDataPointer();
		const BYTE* InData = InVertexBuffer.Data;
		appMemcpy( Data, InData, Stride * NumVertices );
	}
}

/**
* Removes the cloned vertices used for extruding shadow volumes.
* @param NumVertices - The real number of static mesh vertices which should remain in the buffer upon return.
*/
void FPositionVertexBuffer::RemoveLegacyShadowVolumeVertices(UINT InNumVertices)
{
	check(VertexData);
	VertexData->ResizeBuffer(InNumVertices);
	NumVertices = InNumVertices;

	// Make a copy of the vertex data pointer.
	Data = VertexData->GetDataPointer();
}

/**
* Serializer
*
* @param	Ar				Archive to serialize with
* @param	bNeedsCPUAccess	Whether the elements need to be accessed by the CPU
*/
void FPositionVertexBuffer::Serialize( FArchive& Ar, UBOOL bNeedsCPUAccess )
{
	Ar << Stride << NumVertices;

	if(Ar.IsLoading())
	{
		// Allocate the vertex data storage type.
		AllocateData( bNeedsCPUAccess );
	}

	if(VertexData != NULL)
	{
		// Serialize the vertex data.
		VertexData->Serialize(Ar);

		// Make a copy of the vertex data pointer.
		Data = VertexData->GetDataPointer();
	}
}

/**
* Specialized assignment operator, only used when importing LOD's.  
*/
void FPositionVertexBuffer::operator=(const FPositionVertexBuffer &Other)
{
	//VertexData doesn't need to be allocated here because Build will be called next,
	VertexData = NULL;
}

void FPositionVertexBuffer::InitRHI()
{
	check(VertexData);
	FResourceArrayInterface* ResourceArray = VertexData->GetResourceArray();
	if(ResourceArray->GetResourceDataSize())
	{
		// Create the vertex buffer.
		VertexBufferRHI = RHICreateVertexBuffer(ResourceArray->GetResourceDataSize(),ResourceArray,RUF_Static);
	}
}

void FPositionVertexBuffer::AllocateData( UBOOL bNeedsCPUAccess /*= TRUE*/ )
{
	// Clear any old VertexData before allocating.
	CleanUp();

	VertexData = new FPositionVertexData(bNeedsCPUAccess);
	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}



/*-----------------------------------------------------------------------------
	FColorVertexBuffer
-----------------------------------------------------------------------------*/

/** The implementation of the static mesh color-only vertex data storage type. */
class FColorVertexData :
	public TStaticMeshVertexData<FColor>
{
public:
	FColorVertexData( UBOOL InNeedsCPUAccess=FALSE )
		: TStaticMeshVertexData<FColor>( InNeedsCPUAccess )
	{
	}

	/**
	* Assignment operator. This is currently the only method which allows for 
	* modifying an existing resource array
	*/
	TStaticMeshVertexData<FColor>& operator=(const TArray<FColor>& Other)
	{
		TStaticMeshVertexData<FColor>::operator=(Other);
		return *this;
	}
};


FColorVertexBuffer::FColorVertexBuffer():
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{
}

FColorVertexBuffer::FColorVertexBuffer( const FColorVertexBuffer &rhs ):
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{
	InitFromColorArray(*rhs.VertexData);
}

FColorVertexBuffer::~FColorVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FColorVertexBuffer::CleanUp()
{
	if (VertexData)
	{
		delete VertexData;
		VertexData = NULL;
	}
}

/**
 * Initializes the buffer with the given vertices, used to convert legacy layouts.
 * @param InVertices - The vertices to initialize the buffer with.
 */
void FColorVertexBuffer::Init(const TArray<FStaticMeshBuildVertex>& InVertices)
{
	// First, make sure that there is at least one non-default vertex color in the original data.
	const INT InVertexCount = InVertices.Num();
	UBOOL bAllColorsAreOpaqueWhite = TRUE;
	UBOOL bAllColorsAreEqual = TRUE;

	if( InVertexCount > 0 )
	{
		const FColor FirstColor = InVertices( 0 ).Color;

		for( INT CurVertexIndex = 0; CurVertexIndex < InVertexCount; ++CurVertexIndex )
		{
			const FColor CurColor = InVertices( CurVertexIndex ).Color;

			if( CurColor.R != 255 || CurColor.G != 255 || CurColor.B != 255 || CurColor.A != 255 )
			{
				bAllColorsAreOpaqueWhite = FALSE;
			}

			if( CurColor.R != FirstColor.R || CurColor.G != FirstColor.G || CurColor.B != FirstColor.B || CurColor.A != FirstColor.A )
			{
				bAllColorsAreEqual = FALSE;
			}

			if( !bAllColorsAreEqual && !bAllColorsAreOpaqueWhite )
			{
				break;
			}
		}
	}

	if( bAllColorsAreOpaqueWhite )
	{
		// Ensure no vertex data is allocated.
		CleanUp();

		// Clear the vertex count and stride.
		Stride = 0;
		NumVertices = 0;
	}
	else
	{
		NumVertices = InVertexCount;

		// Allocate the vertex data storage type.
		AllocateData();

		// Allocate the vertex data buffer.
		VertexData->ResizeBuffer(NumVertices);
		Data = VertexData->GetDataPointer();

		// Copy the vertices into the buffer.
		for(INT VertexIndex = 0;VertexIndex < InVertices.Num();VertexIndex++)
		{
			const FStaticMeshBuildVertex& SourceVertex = InVertices(VertexIndex);
			const UINT DestVertexIndex = VertexIndex;
			VertexColor(DestVertexIndex) = SourceVertex.Color;
		}
	}
}

/**
 * Initializes this vertex buffer with the contents of the given vertex buffer.
 * @param InVertexBuffer - The vertex buffer to initialize from.
 */
void FColorVertexBuffer::Init(const FColorVertexBuffer& InVertexBuffer)
{
	NumVertices = InVertexBuffer.GetNumVertices();
	if ( NumVertices )
	{
		AllocateData();
		check( Stride == InVertexBuffer.GetStride() );
		VertexData->ResizeBuffer(NumVertices);
		Data = VertexData->GetDataPointer();
		const BYTE* InData = InVertexBuffer.Data;
		appMemcpy( Data, InData, Stride * NumVertices );
	}
}

/**
* Removes the cloned vertices used for extruding shadow volumes.
* @param NumVertices - The real number of static mesh vertices which should remain in the buffer upon return.
*/
void FColorVertexBuffer::RemoveLegacyShadowVolumeVertices(UINT InNumVertices)
{
	if( VertexData != NULL )
	{
		VertexData->ResizeBuffer(InNumVertices);
		NumVertices = InNumVertices;

		// Make a copy of the vertex data pointer.
		Data = VertexData->GetDataPointer();
	}
}

/**
* Serializer
*
* @param	Ar				Archive to serialize with
* @param	bNeedsCPUAccess	Whether the elements need to be accessed by the CPU
*/
void FColorVertexBuffer::Serialize( FArchive& Ar, UBOOL bNeedsCPUAccess )
{
	// Only allocate render resources if not running/patching/cooking dedicated server
	// Skipped because dedicated cooking clears this data (UStaticMesh::StripData())
#if SUPPORTS_SCRIPTPATCH_CREATION
	UBOOL bShouldLoadVertexResources = Ar.IsLoading() && !GIsSeekFreePCServer && (GPatchingTarget != UE3::PLATFORM_WindowsServer);
#else
	UBOOL bShouldLoadVertexResources = Ar.IsLoading() && !GIsSeekFreePCServer;
#endif
	UBOOL bShouldSaveVertexResources = Ar.IsSaving() && (GCookingTarget != UE3::PLATFORM_WindowsServer);

	// If we are saving, have a non-zero vertex count, yet have no vertex data...
	if ( bShouldSaveVertexResources && NumVertices > 0 && VertexData == NULL )
	{
		// ...serialize as if the vertex count were zero. Else on load serialization breaks.
		// This situation should never occur because VertexData should not be null if NumVertices
		// is greater than zero. So really this should be a checkf but I don't want to crash
		// the Editor when saving a package.
		warnf( TEXT("Color vertex buffer being saved with NumVertices=%d Stride=%d VertexData=NULL. This should never happen."),
			NumVertices, Stride );

		INT SerializedStride = 0;
		INT SerializedNumVertices = 0;
		Ar << SerializedStride << SerializedNumVertices;
	}
	else
	{
	Ar << Stride << NumVertices;

	if( Ar.IsLoading() && NumVertices > 0 )
	{
		// Allocate the vertex data storage type.
		AllocateData( bNeedsCPUAccess );
	}

	if (bShouldLoadVertexResources || bShouldSaveVertexResources || Ar.IsCountingMemory())
	{
		if( VertexData != NULL )
		{
			// Serialize the vertex data.
			VertexData->Serialize(Ar);

			// Make a copy of the vertex data pointer.
			Data = VertexData->GetDataPointer();
		}
	}
}
}


/** Export the data to a string, used for editor Copy&Paste. */
void FColorVertexBuffer::ExportText(FString &ValueStr) const
{
	// the following code only works if there is data and this method should only be called if there is data
	check(NumVertices);

	ValueStr += FString::Printf(TEXT("ColorVertexData(%i)=("), NumVertices);

	// 9 characters per color (ARGB in hex plus comma)
	ValueStr.Reserve(ValueStr.Len() + NumVertices * 9);

	for(UINT i = 0; i < NumVertices; ++i)
	{
		UINT Raw = VertexColor(i).DWColor();
		// does not handle endianess	
		// order: ARGB
		TCHAR ColorString[10];

		// does not use FString::Printf for performance reasons
		appSprintf(ColorString, TEXT("%.8x,"), Raw);
		ValueStr += ColorString;
	}

	// replace , by )
	ValueStr[ValueStr.Len() - 1] = ')';
}



/** Export the data from a string, used for editor Copy&Paste. */
void FColorVertexBuffer::ImportText(const TCHAR* SourceText)
{
	check(SourceText);
	check(!VertexData);

	DWORD VertexCount;

	if (Parse(SourceText, TEXT("ColorVertexData("), VertexCount))
	{
		while(*SourceText && *SourceText != TEXT(')'))
		{
			++SourceText;
		}

		while(*SourceText && *SourceText != TEXT('('))
		{
			++SourceText;
		}

		check(*SourceText == TEXT('('));
		++SourceText;

		NumVertices = VertexCount;
		AllocateData();
		VertexData->ResizeBuffer(NumVertices);
		BYTE *Dst = (BYTE *)VertexData->GetDataPointer();

		// 9 characters per color (ARGB in hex plus comma)
		for( UINT i = 0; i < NumVertices; ++i)
		{
			// does not handle endianess or malformed input
			*Dst++ = ParseHexDigit(SourceText[6]) * 16 + ParseHexDigit(SourceText[7]);
			*Dst++ = ParseHexDigit(SourceText[4]) * 16 + ParseHexDigit(SourceText[5]);
			*Dst++ = ParseHexDigit(SourceText[2]) * 16 + ParseHexDigit(SourceText[3]);
			*Dst++ = ParseHexDigit(SourceText[0]) * 16 + ParseHexDigit(SourceText[1]);
			SourceText += 9;
		}
		check(*(SourceText - 1) == TCHAR(')'));

		// Make a copy of the vertex data pointer.
		Data = VertexData->GetDataPointer();

		BeginInitResource(this);
	}
}

/**
* Specialized assignment operator, only used when importing LOD's.  
*/
void FColorVertexBuffer::operator=(const FColorVertexBuffer &Other)
{
	//VertexData doesn't need to be allocated here because Build will be called next,
	VertexData = NULL;
}

/** Default constructor */
FStaticMeshComponentLODInfo::FStaticMeshComponentLODInfo():
	OverrideVertexColors(NULL)
{
}

/** Copy constructor */
FStaticMeshComponentLODInfo::FStaticMeshComponentLODInfo(const FStaticMeshComponentLODInfo &rhs):
	OverrideVertexColors(NULL)
{
	if(rhs.OverrideVertexColors)
	{
		OverrideVertexColors = new FColorVertexBuffer;
		OverrideVertexColors->InitFromColorArray(&rhs.OverrideVertexColors->VertexColor(0), rhs.OverrideVertexColors->GetNumVertices());
	}
}

/** Destructor */
FStaticMeshComponentLODInfo::~FStaticMeshComponentLODInfo()
{
	// The RT thread has no access to it any more so it's save to delete it.
	CleanUp();
}

void FStaticMeshComponentLODInfo::CleanUp()
{
	if(OverrideVertexColors)
	{
		DEC_DWORD_STAT_BY( STAT_InstVertexColorMemory, OverrideVertexColors->GetAllocatedSize() );
	}

	delete OverrideVertexColors;
	OverrideVertexColors = NULL;

	PaintedVertices.Empty();
}


void FStaticMeshComponentLODInfo::BeginReleaseOverrideVertexColors()
{
	if(OverrideVertexColors)
	{
		// enqueue a rendering command to release
		BeginReleaseResource(OverrideVertexColors);
	}
}

void FStaticMeshComponentLODInfo::ReleaseOverrideVertexColorsAndBlock()
{
	if(OverrideVertexColors)
	{
		// enqueue a rendering command to release
		BeginReleaseResource(OverrideVertexColors);
		// Ensure the RT no longer accessed the data, might slow down
		FlushRenderingCommands();
		// The RT thread has no access to it any more so it's save to delete it.
		CleanUp();
	}
}

/** Load from raw color array */
void FColorVertexBuffer::InitFromColorArray( const FColor *InColors, const UINT Count, const UINT Stride )
{
	check( Count > 0 );

	NumVertices = Count;

	// Allocate the vertex data storage type.
	AllocateData();

	// Copy the colors
	{
		VertexData->Add(Count);

		const BYTE *Src = (const BYTE *)InColors;
		FColor *Dst = (FColor *)VertexData->GetDataPointer();

		for( UINT i = 0; i < Count; ++i)
		{
			*Dst++ = (const FColor &)*Src;

			Src += Stride;
		}
	}

	// Make a copy of the vertex data pointer.
	Data = VertexData->GetDataPointer();
}

UINT FColorVertexBuffer::GetAllocatedSize() const
{
	if(VertexData)
	{
		return VertexData->GetAllocatedSize();
	}
	else
	{
		return 0;
	}
}


/** Load from legacy vertex data */
void FColorVertexBuffer::InitFromLegacyData( const FLegacyStaticMeshVertexBuffer& InLegacyData )
{
	// First, make sure that there is at least one non-default vertex color in the original data.
	const INT LegacyVertexCount = InLegacyData.GetNumVertices();

	UBOOL bAllColorsAreOpaqueWhite = TRUE;
	UBOOL bAllColorsAreEqual = TRUE;
	if( LegacyVertexCount > 0 )
	{
		const FColor FirstColor = InLegacyData.VertexColor( 0 );

		for( INT CurVertexIndex = 0; CurVertexIndex < LegacyVertexCount; ++CurVertexIndex )
		{
			const FColor CurColor = InLegacyData.VertexColor( CurVertexIndex );

			if( CurColor.R != 255 || CurColor.G != 255 || CurColor.B != 255 || CurColor.A != 255 )
			{
				bAllColorsAreOpaqueWhite = FALSE;
			}

			if( CurColor.R != FirstColor.R || CurColor.G != FirstColor.G || CurColor.B != FirstColor.B || CurColor.A != FirstColor.A )
			{
				bAllColorsAreEqual = FALSE;
			}

			if( !bAllColorsAreEqual && !bAllColorsAreOpaqueWhite )
			{
				break;
			}
		}
	}

	if( !bAllColorsAreOpaqueWhite )
	{
		NumVertices = LegacyVertexCount;

		// Allocate the vertex data storage type.
		AllocateData();

		// Make a copy of the vertex data pointer.
		VertexData->ResizeBuffer(NumVertices);
		Data = VertexData->GetDataPointer();


		// Copy the data over
		for( UINT CurVertexIndex = 0; CurVertexIndex < NumVertices; ++CurVertexIndex )
		{
			VertexColor( CurVertexIndex ) = InLegacyData.VertexColor( CurVertexIndex );
		}
	}

}


void FColorVertexBuffer::InitRHI()
{
	if( VertexData != NULL )
	{
		FResourceArrayInterface* ResourceArray = VertexData->GetResourceArray();
		if(ResourceArray->GetResourceDataSize())
		{
			// Create the vertex buffer.
			VertexBufferRHI = RHICreateVertexBuffer(ResourceArray->GetResourceDataSize(),ResourceArray,RUF_Static);
		}
	}
}

void FColorVertexBuffer::AllocateData( UBOOL bNeedsCPUAccess /*= TRUE*/ )
{
	// Clear any old VertexData before allocating.
	CleanUp();

	VertexData = new FColorVertexData(bNeedsCPUAccess);
	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}




/*-----------------------------------------------------------------------------
	FLegacyStaticMeshVertexBuffer
-----------------------------------------------------------------------------*/

FLegacyStaticMeshVertexBuffer::FLegacyStaticMeshVertexBuffer():
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0),
	bUseFullPrecisionUVs(FALSE)
{}

FLegacyStaticMeshVertexBuffer::~FLegacyStaticMeshVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FLegacyStaticMeshVertexBuffer::CleanUp()
{
	delete VertexData;
	VertexData = NULL;
}


/**
* Serializer
*
* @param	Ar				Archive to serialize with
* @param	bNeedsCPUAccess	Whether the elements need to be accessed by the CPU
*/
void FLegacyStaticMeshVertexBuffer::Serialize( FArchive& Ar, UBOOL bNeedsCPUAccess )
{
	Ar << NumTexCoords << Stride << NumVertices;
	Ar << bUseFullPrecisionUVs;								

	if( Ar.IsLoading() )
	{
		// Allocate the vertex data storage type.
		AllocateData( bNeedsCPUAccess );
	}

	// Serialize the vertex data.
	VertexData->Serialize(Ar);

	// Make a copy of the vertex data pointer.
	Data = VertexData->GetDataPointer();
}


void FLegacyStaticMeshVertexBuffer::AllocateData( UBOOL bNeedsCPUAccess /*= TRUE*/ )
{
	// Clear any old VertexData before allocating.
	CleanUp();

	if( !bUseFullPrecisionUVs )
	{
		switch(NumTexCoords)
		{
		case 1: VertexData = new TStaticMeshVertexData< TLegacyStaticMeshFullVertexFloat16UVs<1> >(bNeedsCPUAccess); break;
		case 2: VertexData = new TStaticMeshVertexData< TLegacyStaticMeshFullVertexFloat16UVs<2> >(bNeedsCPUAccess); break;
		case 3: VertexData = new TStaticMeshVertexData< TLegacyStaticMeshFullVertexFloat16UVs<3> >(bNeedsCPUAccess); break;
		case 4: VertexData = new TStaticMeshVertexData< TLegacyStaticMeshFullVertexFloat16UVs<4> >(bNeedsCPUAccess); break;
		default: appErrorf(TEXT("Invalid number of texture coordinates"));
		};		
	}
	else
	{
		switch(NumTexCoords)
		{
		case 1: VertexData = new TStaticMeshVertexData< TLegacyStaticMeshFullVertexFloat32UVs<1> >(bNeedsCPUAccess); break;
		case 2: VertexData = new TStaticMeshVertexData< TLegacyStaticMeshFullVertexFloat32UVs<2> >(bNeedsCPUAccess); break;
		case 3: VertexData = new TStaticMeshVertexData< TLegacyStaticMeshFullVertexFloat32UVs<3> >(bNeedsCPUAccess); break;
		case 4: VertexData = new TStaticMeshVertexData< TLegacyStaticMeshFullVertexFloat32UVs<4> >(bNeedsCPUAccess); break;
		default: appErrorf(TEXT("Invalid number of texture coordinates"));
		};		
	}

	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}


/*-----------------------------------------------------------------------------
	FStaticMeshVertexBuffer
-----------------------------------------------------------------------------*/

FStaticMeshVertexBuffer::FStaticMeshVertexBuffer():
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0),
	bUseFullPrecisionUVs(FALSE)
{}

FStaticMeshVertexBuffer::~FStaticMeshVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FStaticMeshVertexBuffer::CleanUp()
{
	delete VertexData;
	VertexData = NULL;
}

/**
* Initializes the buffer with the given vertices.
* @param InVertices - The vertices to initialize the buffer with.
* @param InNumTexCoords - The number of texture coordinate to store in the buffer.
*/
void FStaticMeshVertexBuffer::Init(const TArray<FStaticMeshBuildVertex>& InVertices,UINT InNumTexCoords)
{
	NumTexCoords = InNumTexCoords;
	NumVertices = InVertices.Num();

	// Allocate the vertex data storage type.
	AllocateData();

	// Allocate the vertex data buffer.
	VertexData->ResizeBuffer(NumVertices);
	Data = VertexData->GetDataPointer();

	// Copy the vertices into the buffer.
	for(INT VertexIndex = 0;VertexIndex < InVertices.Num();VertexIndex++)
	{
		const FStaticMeshBuildVertex& SourceVertex = InVertices(VertexIndex);
		const UINT DestVertexIndex = VertexIndex;
		VertexTangentX(DestVertexIndex) = SourceVertex.TangentX;
		VertexTangentZ(DestVertexIndex) = SourceVertex.TangentZ;

		// store the sign of the determinant in TangentZ.W
		VertexTangentZ(DestVertexIndex).Vector.W = GetBasisDeterminantSignByte( 
			SourceVertex.TangentX, SourceVertex.TangentY, SourceVertex.TangentZ );

		for(UINT UVIndex = 0;UVIndex < NumTexCoords;UVIndex++)
		{
			SetVertexUV(DestVertexIndex,UVIndex,SourceVertex.UVs[UVIndex]);
		}
	}
}

/**
 * Initializes this vertex buffer with the contents of the given vertex buffer.
 * @param InVertexBuffer - The vertex buffer to initialize from.
 */
void FStaticMeshVertexBuffer::Init(const FStaticMeshVertexBuffer& InVertexBuffer)
{
	NumTexCoords = InVertexBuffer.GetNumTexCoords();
	NumVertices = InVertexBuffer.GetNumVertices();
	bUseFullPrecisionUVs = InVertexBuffer.GetUseFullPrecisionUVs();
	if ( NumVertices )
	{
		AllocateData();
		check( GetStride() == InVertexBuffer.GetStride() );
		VertexData->ResizeBuffer(NumVertices);
		Data = VertexData->GetDataPointer();
		const BYTE* InData = InVertexBuffer.GetRawVertexData();
		appMemcpy( Data, InData, Stride * NumVertices );
	}
}

/**
* Removes the cloned vertices used for extruding shadow volumes.
* @param NumVertices - The real number of static mesh vertices which should remain in the buffer upon return.
*/
void FStaticMeshVertexBuffer::RemoveLegacyShadowVolumeVertices(UINT InNumVertices)
{
	check(VertexData);
	VertexData->ResizeBuffer(InNumVertices);
	NumVertices = InNumVertices;

	// Make a copy of the vertex data pointer.
	Data = VertexData->GetDataPointer();
}

/**
* Convert the existing data in this mesh from 16 bit to 32 bit UVs.
* Without rebuilding the mesh (loss of precision)
*/
template<INT NumTexCoordsT>
void FStaticMeshVertexBuffer::ConvertToFullPrecisionUVs()
{
	if( !bUseFullPrecisionUVs )
	{
		check(NumTexCoords == NumTexCoordsT);
		// create temp array to store 32 bit values
		TArray< TStaticMeshFullVertexFloat32UVs<NumTexCoordsT> > DestVertexData;
		// source vertices
		TStaticMeshVertexData< TStaticMeshFullVertexFloat16UVs<NumTexCoordsT> >& SrcVertexData = 
			*(TStaticMeshVertexData< TStaticMeshFullVertexFloat16UVs<NumTexCoordsT> >*)VertexData;
		// copy elements from source vertices to temp data
		DestVertexData.Add(SrcVertexData.Num());
		for( INT VertIdx=0; VertIdx < SrcVertexData.Num(); VertIdx++ )
		{
			TStaticMeshFullVertexFloat32UVs<NumTexCoordsT>& DestVert = DestVertexData(VertIdx);
			TStaticMeshFullVertexFloat16UVs<NumTexCoordsT>& SrcVert = SrcVertexData(VertIdx);		
			appMemcpy(&DestVert,&SrcVert,sizeof(FStaticMeshFullVertex));
			for( INT UVIdx=0; UVIdx < NumTexCoordsT; UVIdx++ )
			{
				DestVert.UVs[UVIdx] = FVector2D(SrcVert.UVs[UVIdx]);
			}
		}
		// force 32 bit UVs
		bUseFullPrecisionUVs = TRUE;

		AllocateData( FALSE );

		*(TStaticMeshVertexData< TStaticMeshFullVertexFloat32UVs<NumTexCoordsT> >*)VertexData = DestVertexData;
		Data = VertexData->GetDataPointer();
		Stride = VertexData->GetStride();
	}
}

/**
* Serializer
*
* @param	Ar				Archive to serialize with
* @param	bNeedsCPUAccess	Whether the elements need to be accessed by the CPU
*/
void FStaticMeshVertexBuffer::Serialize( FArchive& Ar, UBOOL bNeedsCPUAccess )
{
	Ar << NumTexCoords << Stride << NumVertices;
	Ar << bUseFullPrecisionUVs;								

	if( Ar.IsLoading() )
	{
		// Allocate the vertex data storage type.
		AllocateData( bNeedsCPUAccess );
	}

	// Only allocate render resources if not running/patching/cooking dedicated server
	// Skipped because dedicated cooking clears this data (UStaticMesh::StripData())
#if SUPPORTS_SCRIPTPATCH_CREATION
	UBOOL bShouldLoadVertexResources = Ar.IsLoading() && !GIsSeekFreePCServer && (GPatchingTarget != UE3::PLATFORM_WindowsServer);
#else
	UBOOL bShouldLoadVertexResources = Ar.IsLoading() && !GIsSeekFreePCServer;
#endif
	UBOOL bShouldSaveVertexResources = Ar.IsSaving() && (GCookingTarget != UE3::PLATFORM_WindowsServer);
	if (bShouldLoadVertexResources || bShouldSaveVertexResources || Ar.IsCountingMemory())
	{
		if( VertexData != NULL )
		{
			// Serialize the vertex data.
			VertexData->Serialize(Ar);

			// Make a copy of the vertex data pointer.
			Data = VertexData->GetDataPointer();
		}
	}
}


/** Load from legacy vertex data */
void FStaticMeshVertexBuffer::InitFromLegacyData( const FLegacyStaticMeshVertexBuffer& InLegacyData )
{
	NumTexCoords = InLegacyData.GetNumTexCoords();
	NumVertices = InLegacyData.GetNumVertices();
	bUseFullPrecisionUVs = InLegacyData.GetUseFullPrecisionUVs();

	// Allocate the vertex data storage type.
	AllocateData();

	// Make a copy of the vertex data pointer.
	VertexData->ResizeBuffer(NumVertices);
	Data = VertexData->GetDataPointer();

	// Copy the data over
	for( UINT CurVertexIndex = 0; CurVertexIndex < NumVertices; ++CurVertexIndex )
	{
		VertexTangentX( CurVertexIndex ) = InLegacyData.VertexTangentX( CurVertexIndex );
		VertexTangentZ( CurVertexIndex ) = InLegacyData.VertexTangentZ( CurVertexIndex );
		for( UINT CurUVIndex = 0; CurUVIndex < NumTexCoords; ++CurUVIndex )
		{
			SetVertexUV( CurVertexIndex, CurUVIndex, InLegacyData.GetVertexUV( CurVertexIndex, CurUVIndex ) );
		}
	}
}

/**
* Specialized assignment operator, only used when importing LOD's.  
*/
void FStaticMeshVertexBuffer::operator=(const FStaticMeshVertexBuffer &Other)
{
	//VertexData doesn't need to be allocated here because Build will be called next,
	VertexData = NULL;
	bUseFullPrecisionUVs = Other.bUseFullPrecisionUVs;
}

void FStaticMeshVertexBuffer::InitRHI()
{
	check(VertexData);
	FResourceArrayInterface* ResourceArray = VertexData->GetResourceArray();
	if(ResourceArray->GetResourceDataSize())
	{
		// Create the vertex buffer.
		VertexBufferRHI = RHICreateVertexBuffer(ResourceArray->GetResourceDataSize(),ResourceArray,RUF_Static);
	}
}

void FStaticMeshVertexBuffer::AllocateData( UBOOL bNeedsCPUAccess /*= TRUE*/ )
{
	// Clear any old VertexData before allocating.
	CleanUp();

	if( !bUseFullPrecisionUVs )
	{
		switch(NumTexCoords)
		{
		case 1: VertexData = new TStaticMeshVertexData< TStaticMeshFullVertexFloat16UVs<1> >(bNeedsCPUAccess); break;
		case 2: VertexData = new TStaticMeshVertexData< TStaticMeshFullVertexFloat16UVs<2> >(bNeedsCPUAccess); break;
		case 3: VertexData = new TStaticMeshVertexData< TStaticMeshFullVertexFloat16UVs<3> >(bNeedsCPUAccess); break;
		case 4: VertexData = new TStaticMeshVertexData< TStaticMeshFullVertexFloat16UVs<4> >(bNeedsCPUAccess); break;
		default: appErrorf(TEXT("Invalid number of texture coordinates"));
		};		
	}
	else
	{
		switch(NumTexCoords)
		{
		case 1: VertexData = new TStaticMeshVertexData< TStaticMeshFullVertexFloat32UVs<1> >(bNeedsCPUAccess); break;
		case 2: VertexData = new TStaticMeshVertexData< TStaticMeshFullVertexFloat32UVs<2> >(bNeedsCPUAccess); break;
		case 3: VertexData = new TStaticMeshVertexData< TStaticMeshFullVertexFloat32UVs<3> >(bNeedsCPUAccess); break;
		case 4: VertexData = new TStaticMeshVertexData< TStaticMeshFullVertexFloat32UVs<4> >(bNeedsCPUAccess); break;
		default: appErrorf(TEXT("Invalid number of texture coordinates"));
		};		
	}	

	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}

/*-----------------------------------------------------------------------------
	FLegacyShadowExtrusionVertexData
-----------------------------------------------------------------------------*/

/** The implementation of the static mesh position-only vertex data storage type. */
class FLegacyShadowExtrusionVertexData :
	public TStaticMeshVertexData<FLegacyShadowExtrusionVertex>
{
public:
	FLegacyShadowExtrusionVertexData( UBOOL InNeedsCPUAccess=FALSE )
		: TStaticMeshVertexData<FLegacyShadowExtrusionVertex>( InNeedsCPUAccess )
	{
	}
};

FArchive& operator<<(FArchive& Ar,FLegacyExtrusionVertexBuffer& VertexBuffer)
{
	UINT Stride;
	UINT NumVertices;

	Ar << Stride << NumVertices;

	FLegacyShadowExtrusionVertexData LegacyData;
	LegacyData.Serialize(Ar);

	return Ar;
}

/*-----------------------------------------------------------------------------
	FPlatformMeshData
-----------------------------------------------------------------------------*/

FPlatformStaticMeshData::~FPlatformStaticMeshData(void)
{

}

/*-----------------------------------------------------------------------------
	FStaticMeshRenderData
-----------------------------------------------------------------------------*/

FStaticMeshRenderData::FStaticMeshRenderData()
:	bNeedsCPUAccess(TRUE)
,	IndexBuffer(TRUE)	// Needs to be CPU accessible for CPU-skinned decals.
,	AdjacencyIndexBuffer(TRUE)
{
}

/**
 * Special serialize function passing the owning UObject along as required by FUnytpedBulkData
 * serialization.
 *
 * @param	Ar		Archive to serialize with
 * @param	Owner	UObject this structure is serialized within
 * @param	Idx		Index of current array entry being serialized
 */
void FStaticMeshRenderData::Serialize( FArchive& Ar, UObject* Owner, INT Idx )
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>( Owner );
	if( StaticMesh )
	{
		bNeedsCPUAccess = !StaticMesh->bStripkDOPForConsole;
	}

	RawTriangles.Serialize( Ar, Owner );
	
	Ar	<< Elements;
	PositionVertexBuffer.Serialize( Ar, bNeedsCPUAccess );
	if( Ar.Ver() < VER_MESH_PAINT_SYSTEM_ENUM )
	{
		// Backwards compatibility for old static meshes where vertex color was interleaved with
		// tangents and UVs.  We'll load the old format and move the color data to it's own stream.
		check( !Ar.IsSaving() );

		// Load the old data
		FLegacyStaticMeshVertexBuffer LegacyVertexBuffer;
		LegacyVertexBuffer.Serialize( Ar, bNeedsCPUAccess );

		// Convert it to the new format
		VertexBuffer.InitFromLegacyData( LegacyVertexBuffer );
		ColorVertexBuffer.InitFromLegacyData( LegacyVertexBuffer );
	}
	else
	{
		VertexBuffer.Serialize( Ar, bNeedsCPUAccess );

		// Some color buffers have been incorrectly serialized with non-zero vertex counts
		// and no data causing serialization crashes. Fix up these broken assets.
		if ( Ar.Ver() < VER_FIX_BROKEN_COLOR_VERTEX_BUFFERS && Ar.IsLoading() )
		{
			// Save the position in the archive and see what the stride and count of the color buffer will be.
			INT ArPos = Ar.Tell();
			INT ExpectedVertCount = VertexBuffer.GetNumVertices();
			INT ColorBufferVertCount = 0;
			INT ColorBufferStride = 0;
			Ar << ColorBufferStride << ColorBufferVertCount;

			// Speculatively read the stride of the bulk vertex color data.
			INT ColorBulkDataStride = 0;
			if ( Ar.Ver() >= VER_REMOVED_SHADOW_VOLUMES )
			{
				INT ArPos2 = Ar.Tell();
				Ar << ColorBulkDataStride;
				Ar.Seek( ArPos2 );
			}

			if ( ColorBufferVertCount == ExpectedVertCount || ( ColorBufferVertCount > 0 && ColorBulkDataStride == ColorBufferStride ) )
			{
				// The count is what we expect it to be, or at least it looks like the data has been serialized.
				// There is one corner case:
				//   ExpectedVertCount == 4 && ColorBufferVertCount > 0 && ColorBufferVertCount != 4 && ColorBufferStride == 4 && this->NumVertices == 4
				// In this case, it could be a broken buffer but we can't tell for sure so do the conservative thing: try serializing it.
				Ar.Seek( ArPos );
				ColorVertexBuffer.Serialize( Ar, bNeedsCPUAccess );
			}
		}
		else
		{
		// NOTE: This color vertex buffer may actually be empty (zero vertices) for static meshes which
		//		 don't make use of vertex colors
		ColorVertexBuffer.Serialize( Ar, bNeedsCPUAccess );
	}
	}

	if (Ar.Ver() < VER_REMOVED_SHADOW_VOLUMES)
	{
		FLegacyExtrusionVertexBuffer ShadowExtrusionVertexBuffer;
		Ar << ShadowExtrusionVertexBuffer;
	}
	
	Ar	<< NumVertices;

	IndexBuffer.Serialize( Ar, bNeedsCPUAccess );
	Ar << WireframeIndexBuffer;
	if (Ar.Ver() < VER_REMOVED_SHADOW_VOLUMES)
	{
		TArray<FMeshEdge> LegacyEdges;
		LegacyEdges.BulkSerialize( Ar );
		TArray<BYTE> LegacyShadowTriangleDoubleSided;
		Ar << LegacyShadowTriangleDoubleSided;
	}

	if (Ar.Ver() >= VER_CRACK_FREE_DISPLACEMENT_SUPPORT)
	{
		AdjacencyIndexBuffer.Serialize( Ar, bNeedsCPUAccess );
	}

	if (Ar.IsLoading())
	{
		if (PositionVertexBuffer.GetNumVertices() != NumVertices)
		{
			PositionVertexBuffer.RemoveLegacyShadowVolumeVertices(NumVertices);
		}
		if (VertexBuffer.GetNumVertices() != NumVertices)
		{
			VertexBuffer.RemoveLegacyShadowVolumeVertices(NumVertices);
		}
		if (VertexBuffer.GetNumVertices() != NumVertices)
		{
			ColorVertexBuffer.RemoveLegacyShadowVolumeVertices(NumVertices);
		}
	}
}

INT FStaticMeshRenderData::GetTriangleCount() const
{
	INT NumTriangles = 0;
	for(INT ElementIndex = 0;ElementIndex < Elements.Num();ElementIndex++)
	{
		NumTriangles += Elements(ElementIndex).NumTriangles;
	}
	return NumTriangles;
}



/**
 * Configures a vertex factory for rendering this static mesh
 *
 * @param	InOutVertexFactory				The vertex factory to configure
 * @param	InParentMesh					Parent static mesh
 * @param	InOverrideColorVertexBuffer		Optional color vertex buffer to use *instead* of the color vertex stream associated with this static mesh
 */
void FStaticMeshRenderData::SetupVertexFactory( FLocalVertexFactory& InOutVertexFactory, UStaticMesh* InParentMesh, FColorVertexBuffer* InOverrideColorVertexBuffer )
{
	check( InParentMesh != NULL );

	struct InitStaticMeshVertexFactoryParams
	{
		FLocalVertexFactory* VertexFactory;
		FStaticMeshRenderData* RenderData;
		FColorVertexBuffer* OverrideColorVertexBuffer;
		UStaticMesh* Parent;
	} Params;

	Params.VertexFactory = &InOutVertexFactory;
	Params.RenderData = this;
	Params.OverrideColorVertexBuffer = InOverrideColorVertexBuffer;
	Params.Parent = InParentMesh;

	// Initialize the static mesh's vertex factory.
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		InitStaticMeshVertexFactory,
		InitStaticMeshVertexFactoryParams,Params,Params,
		{
			FLocalVertexFactory::DataType Data;
			Data.PositionComponent = FVertexStreamComponent(
				&Params.RenderData->PositionVertexBuffer,
				STRUCT_OFFSET(FPositionVertex,Position),
				Params.RenderData->PositionVertexBuffer.GetStride(),
				VET_Float3
				);
			Data.TangentBasisComponents[0] = FVertexStreamComponent(
				&Params.RenderData->VertexBuffer,
				STRUCT_OFFSET(FStaticMeshFullVertex,TangentX),
				Params.RenderData->VertexBuffer.GetStride(),
				VET_PackedNormal
				);
			Data.TangentBasisComponents[1] = FVertexStreamComponent(
				&Params.RenderData->VertexBuffer,
				STRUCT_OFFSET(FStaticMeshFullVertex,TangentZ),
				Params.RenderData->VertexBuffer.GetStride(),
				VET_PackedNormal
				);

			// Use the "override" color vertex buffer if one was supplied.  Otherwise, the color vertex stream
			// associated with the static mesh is used.
			FColorVertexBuffer* ColorVertexBuffer = &Params.RenderData->ColorVertexBuffer;
			if( Params.OverrideColorVertexBuffer != NULL )
			{
				ColorVertexBuffer = Params.OverrideColorVertexBuffer;
			}
			if( ColorVertexBuffer->GetNumVertices() > 0 )
			{
				Data.ColorComponent = FVertexStreamComponent(
					ColorVertexBuffer,
					0,	// Struct offset to color
					ColorVertexBuffer->GetStride(),
					VET_Color
					);
			}

			Data.TextureCoordinates.Empty();

			if( !Params.RenderData->VertexBuffer.GetUseFullPrecisionUVs() )
			{
				for(UINT UVIndex = 0;UVIndex < Params.RenderData->VertexBuffer.GetNumTexCoords();UVIndex++)
				{
					Data.TextureCoordinates.AddItem(FVertexStreamComponent(
						&Params.RenderData->VertexBuffer,
						STRUCT_OFFSET(TStaticMeshFullVertexFloat16UVs<MAX_TEXCOORDS>,UVs) + sizeof(FVector2DHalf) * UVIndex,
						Params.RenderData->VertexBuffer.GetStride(),
						VET_Half2
						));
				}
				if(	Params.Parent->LightMapCoordinateIndex >= 0 && (UINT)Params.Parent->LightMapCoordinateIndex < Params.RenderData->VertexBuffer.GetNumTexCoords())
				{
					Data.ShadowMapCoordinateComponent = FVertexStreamComponent(
						&Params.RenderData->VertexBuffer,
						STRUCT_OFFSET(TStaticMeshFullVertexFloat16UVs<MAX_TEXCOORDS>,UVs) + sizeof(FVector2DHalf) * Params.Parent->LightMapCoordinateIndex,
						Params.RenderData->VertexBuffer.GetStride(),
						VET_Half2
						);
				}
			}
			else
			{
				for(UINT UVIndex = 0;UVIndex < Params.RenderData->VertexBuffer.GetNumTexCoords();UVIndex++)
				{
					Data.TextureCoordinates.AddItem(FVertexStreamComponent(
						&Params.RenderData->VertexBuffer,
						STRUCT_OFFSET(TStaticMeshFullVertexFloat32UVs<MAX_TEXCOORDS>,UVs) + sizeof(FVector2D) * UVIndex,
						Params.RenderData->VertexBuffer.GetStride(),
						VET_Float2
						));
				}

				if(	Params.Parent->LightMapCoordinateIndex >= 0 && (UINT)Params.Parent->LightMapCoordinateIndex < Params.RenderData->VertexBuffer.GetNumTexCoords())
				{
					Data.ShadowMapCoordinateComponent = FVertexStreamComponent(
						&Params.RenderData->VertexBuffer,
						STRUCT_OFFSET(TStaticMeshFullVertexFloat32UVs<MAX_TEXCOORDS>,UVs) + sizeof(FVector2D) * Params.Parent->LightMapCoordinateIndex,
						Params.RenderData->VertexBuffer.GetStride(),
						VET_Float2
						);
				}
			}	
			Params.VertexFactory->SetData(Data);
		});
}

/**
 * Initializes the LOD's render resources.
 * @param Parent Parent mesh
 */
void FStaticMeshRenderData::InitResources(UStaticMesh* Parent)
{
	if (Parent->bUsedForInstancing && 
		VertexBuffer.GetNumVertices() &&
		Elements.Num() == 1 // You really can't use hardware instancing on the consoles with multiple elements because they share the same index buffer. 
		// The mesh emitter doesn't bother to enforce this; it will just render wrong, even on the PC.
		)
	{
		if (IndexBuffer.Indices.Num())
		{
			IndexBuffer.SetupForInstancing(VertexBuffer.GetNumVertices(), Parent->ConsolePreallocateInstanceCount);
		}

		if (AdjacencyIndexBuffer.Indices.Num())
		{
			AdjacencyIndexBuffer.SetupForInstancing(VertexBuffer.GetNumVertices(), Parent->ConsolePreallocateInstanceCount);
		}
	}

	// Initialize the vertex and index buffers.
	BeginInitResource(&IndexBuffer);
	if( WireframeIndexBuffer.Indices.Num() )
	{
		BeginInitResource(&WireframeIndexBuffer);
	}	
	BeginInitResource(&VertexBuffer);
	BeginInitResource(&PositionVertexBuffer);
	if( ColorVertexBuffer.GetNumVertices() > 0 )
	{
		BeginInitResource(&ColorVertexBuffer);
	}

#if WITH_D3D11_TESSELLATION
	if( GRHIShaderPlatform == SP_PCD3D_SM5 ) 
	{
		BeginInitResource(&AdjacencyIndexBuffer);
	}
#endif

	SetupVertexFactory( VertexFactory, Parent, NULL );
	BeginInitResource(&VertexFactory);

	const DWORD StaticMeshVertexMemory = 
		VertexBuffer.GetStride() * VertexBuffer.GetNumVertices() + 
		PositionVertexBuffer.GetStride() * PositionVertexBuffer.GetNumVertices();
	const DWORD StaticMeshIndexMemory = (IndexBuffer.Indices.Num() + WireframeIndexBuffer.Indices.Num()) * 2 +
#if WITH_D3D11_TESSELLATION
		( (GRHIShaderPlatform == SP_PCD3D_SM5) ? (AdjacencyIndexBuffer.Indices.Num() * 2) : (0) );
#else // #if WITH_D3D11_TESSELLATION
		0;
#endif // #if WITH_D3D11_TESSELLATION
	const DWORD ResourceVertexColorMemory = ColorVertexBuffer.GetStride() * ColorVertexBuffer.GetNumVertices();

#if PS3
	INC_DWORD_STAT_BY( bNeedsCPUAccess == FALSE ? STAT_StaticMeshVideoMemory : STAT_StaticMeshVertexMemory, StaticMeshVertexMemory );
	INC_DWORD_STAT_BY( bNeedsCPUAccess == FALSE ? STAT_ResourceVertexColorVideoMemory : STAT_ResourceVertexColorMemory, ResourceVertexColorMemory );
	INC_DWORD_STAT_BY( bNeedsCPUAccess == FALSE ? STAT_StaticMeshIndexVideoMemory : STAT_StaticMeshIndexMemory, StaticMeshIndexMemory );
#else
	INC_DWORD_STAT_BY( STAT_StaticMeshVertexMemory, StaticMeshVertexMemory );
	INC_DWORD_STAT_BY( STAT_ResourceVertexColorMemory, ResourceVertexColorMemory );
	INC_DWORD_STAT_BY( STAT_StaticMeshIndexMemory, StaticMeshIndexMemory );
#endif // PS3
}

/**
 * Releases the LOD's render resources.
 */
void FStaticMeshRenderData::ReleaseResources()
{
	const DWORD StaticMeshVertexMemory = 
		VertexBuffer.GetStride() * VertexBuffer.GetNumVertices() + 
		PositionVertexBuffer.GetStride() * PositionVertexBuffer.GetNumVertices();
	const DWORD StaticMeshIndexMemory = (IndexBuffer.Indices.Num() + WireframeIndexBuffer.Indices.Num()) * 2 +
#if WITH_D3D11_TESSELLATION
		( (GRHIShaderPlatform == SP_PCD3D_SM5) ? (AdjacencyIndexBuffer.Indices.Num() * 2) : (0) );
#else // #if WITH_D3D11_TESSELLATION
		0;
#endif // #if WITH_D3D11_TESSELLATION
	const DWORD ResourceVertexColorMemory = ColorVertexBuffer.GetStride() * ColorVertexBuffer.GetNumVertices();

#if PS3
	DEC_DWORD_STAT_BY( bNeedsCPUAccess == FALSE ? STAT_StaticMeshVideoMemory : STAT_StaticMeshVertexMemory, StaticMeshVertexMemory );
	DEC_DWORD_STAT_BY( bNeedsCPUAccess == FALSE ? STAT_ResourceVertexColorVideoMemory : STAT_ResourceVertexColorMemory, ResourceVertexColorMemory );
	DEC_DWORD_STAT_BY( bNeedsCPUAccess == FALSE ? STAT_StaticMeshIndexVideoMemory : STAT_StaticMeshIndexMemory, StaticMeshIndexMemory );
#else
	DEC_DWORD_STAT_BY( STAT_StaticMeshVertexMemory, StaticMeshVertexMemory );
	DEC_DWORD_STAT_BY( STAT_ResourceVertexColorMemory, ResourceVertexColorMemory );
	DEC_DWORD_STAT_BY( STAT_StaticMeshIndexMemory, StaticMeshIndexMemory );
#endif // PS3

	// Release the vertex and index buffers.
#if WITH_D3D11_TESSELLATION
	if( GRHIShaderPlatform == SP_PCD3D_SM5 ) 
	{
		BeginReleaseResource(&AdjacencyIndexBuffer);
	}
#endif

	BeginReleaseResource(&IndexBuffer);
	BeginReleaseResource(&WireframeIndexBuffer);
	BeginReleaseResource(&VertexBuffer);
	BeginReleaseResource(&PositionVertexBuffer);
	BeginReleaseResource(&ColorVertexBuffer);

	// Release the vertex factories.
	BeginReleaseResource(&VertexFactory);
}

/*-----------------------------------------------------------------------------
FStaticMeshSourceData
-----------------------------------------------------------------------------*/

FStaticMeshSourceData::FStaticMeshSourceData() : RenderData( NULL )
{}

FStaticMeshSourceData::~FStaticMeshSourceData()
{
	delete RenderData;
	RenderData = NULL;
}

/** Initialize from static mesh render data. */
void FStaticMeshSourceData::Init( FStaticMeshRenderData& InRenderData )
{
	RenderData = new FStaticMeshRenderData();
	RenderData->VertexBuffer.Init( InRenderData.VertexBuffer );
	RenderData->PositionVertexBuffer.Init( InRenderData.PositionVertexBuffer );
	RenderData->ColorVertexBuffer.Init( InRenderData.ColorVertexBuffer );
	RenderData->NumVertices = InRenderData.NumVertices;
	RenderData->IndexBuffer.Indices = InRenderData.IndexBuffer.Indices;
	RenderData->WireframeIndexBuffer.Indices = InRenderData.WireframeIndexBuffer.Indices;
	RenderData->Elements = InRenderData.Elements;

	const INT RawTriangleCount = InRenderData.RawTriangles.GetElementCount();
	if ( RawTriangleCount )
	{
		const BYTE* SrcTriangleData = (const BYTE*)InRenderData.RawTriangles.Lock(LOCK_READ_ONLY);
		RenderData->RawTriangles.Lock(LOCK_READ_WRITE);
		BYTE* DestTriangleData = (BYTE*)RenderData->RawTriangles.Realloc( RawTriangleCount );
		appMemcpy( DestTriangleData, SrcTriangleData, InRenderData.RawTriangles.GetBulkDataSize() );
		RenderData->RawTriangles.Unlock();
		InRenderData.RawTriangles.Unlock();
	}

	ClearMaterialReferences();
}

/** Free source data. */
void FStaticMeshSourceData::Clear()
{
	delete RenderData;
	RenderData = NULL;
}

/** Clear any material references held by the source data. */
void FStaticMeshSourceData::ClearMaterialReferences()
{
	if ( RenderData )
	{
		// Clear references to materials in the source data.
		const INT ElementCount = RenderData->Elements.Num();
		for ( INT ElementIndex = 0; ElementIndex < ElementCount; ++ElementIndex )
		{
			RenderData->Elements( ElementIndex ).Material = 0;
		}
	}
}

/** Serialization. */
FArchive& operator<<( FArchive& Ar, FStaticMeshSourceData& SourceData )
{
	if ( Ar.Ver() >= VER_STATIC_MESH_SOURCE_DATA_COPY )
	{
		if ( Ar.IsLoading() )
		{
			UBOOL bHaveSourceData = FALSE;
			SourceData.RenderData = NULL;
			Ar << bHaveSourceData;
			if ( bHaveSourceData )
			{
				check( SourceData.RenderData == NULL );
				SourceData.RenderData = new FStaticMeshRenderData();
				SourceData.RenderData->Serialize( Ar, NULL, INDEX_NONE );
			}
		}
		else
		{
			UBOOL bHaveSourceData = SourceData.IsInitialized();
			Ar << bHaveSourceData;
			if ( bHaveSourceData )
			{
				SourceData.RenderData->Serialize( Ar, NULL, INDEX_NONE );
			}
		}
	}
	return Ar;
}

/*-----------------------------------------------------------------------------
UStaticMesh
-----------------------------------------------------------------------------*/

/**
 * Mapping of properties' names to their tooltips
 */
TMap<FString, FString> UStaticMesh::PropertyToolTipMap;


/**
 * Initializes the static mesh's render resources.
 */
void UStaticMesh::InitResources()
{
	for(INT LODIndex = 0;LODIndex < LODModels.Num();LODIndex++)
	{
		LODModels(LODIndex).InitResources(this);
	}

#if STATS
	DWORD StaticMeshResourceSize = GetResourceSize();
	INC_DWORD_STAT_BY( STAT_StaticMeshTotalMemory, StaticMeshResourceSize );
	INC_DWORD_STAT_BY( STAT_StaticMeshTotalMemory2, StaticMeshResourceSize );
	INC_DWORD_STAT_BY( STAT_StaticMeshkDOPMemory, GetkDOPTreeSize() );
#endif
}

/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT UStaticMesh::GetResourceSize()
{
	if (GExclusiveResourceSizeMode)
	{
		return 0;
	}
	else
	{
		FArchiveCountMem CountBytesSize( this );
		INT ResourceSize = CountBytesSize.GetNum();
		return ResourceSize;
	}
}

INT UStaticMesh::GetRendererResourceSize() const
{
	INT StaticMeshResourceSize = 0;

	for(INT LODIndex = 0;LODIndex < LODModels.Num();LODIndex++)
	{
		const FStaticMeshRenderData& LODRenderData = LODModels(LODIndex);

		const INT VBSize = LODRenderData.VertexBuffer.GetStride()	* LODRenderData.VertexBuffer.GetNumVertices() + 
			LODRenderData.PositionVertexBuffer.GetStride()			* LODRenderData.PositionVertexBuffer.GetNumVertices() + 
			LODRenderData.ColorVertexBuffer.GetStride()				* LODRenderData.ColorVertexBuffer.GetNumVertices();
		const INT IBSize = (LODRenderData.IndexBuffer.Indices.Num() + LODRenderData.WireframeIndexBuffer.Indices.Num()) * 2;

		StaticMeshResourceSize += VBSize + IBSize;
	}

	return StaticMeshResourceSize;
}

INT UStaticMesh::GetkDOPTreeSize() const
{
	const INT KDOPTreeSize = kDOPTree.Nodes.GetAllocatedSize() + kDOPTree.Triangles.GetAllocatedSize();
	return KDOPTreeSize;
}

/**
 * Releases the static mesh's render resources.
 */
void UStaticMesh::ReleaseResources()
{
#if STATS
	DWORD StaticMeshResourceSize = GetResourceSize();
	DEC_DWORD_STAT_BY( STAT_StaticMeshTotalMemory, StaticMeshResourceSize );
	DEC_DWORD_STAT_BY( STAT_StaticMeshTotalMemory2, StaticMeshResourceSize );
	DEC_DWORD_STAT_BY( STAT_StaticMeshkDOPMemory, GetkDOPTreeSize() );
#endif

	for(INT LODIndex = 0;LODIndex < LODModels.Num();LODIndex++)
	{
		LODModels(LODIndex).ReleaseResources();
	}

	// insert a fence to signal when these commands completed
	ReleaseResourcesFence.BeginFence();
}

//
//	UStaticMesh::StaticConstructor
//
void UStaticMesh::StaticConstructor()
{
#if !CONSOLE && !DEDICATED_SERVER
	new(GetClass()->HideCategories) FName(NAME_Object);
#endif

	UProperty* UseSimpleLineCollisionProp = new(GetClass(),TEXT("UseSimpleLineCollision"), RF_Public)		UBoolProperty( CPP_PROPERTY(UseSimpleLineCollision), TEXT(""), CPF_Edit );
	UProperty* UseSimpleBoxCollisionProp = new(GetClass(),TEXT("UseSimpleBoxCollision"),RF_Public)			UBoolProperty(CPP_PROPERTY(UseSimpleBoxCollision),TEXT(""),CPF_Edit);
	UProperty* UseSimpleRigidBodyCollisionProp = new(GetClass(),TEXT("UseSimpleRigidBodyCollision"),RF_Public)	UBoolProperty(CPP_PROPERTY(UseSimpleRigidBodyCollision),TEXT(""),CPF_Edit);
	UProperty* UseFullPrecisionUVsProp = new(GetClass(),TEXT("UseFullPrecisionUVs"),RF_Public)			UBoolProperty(CPP_PROPERTY(UseFullPrecisionUVs),TEXT(""),CPF_Edit);
	UProperty* UsedForInstancingProp = new(GetClass(),TEXT("bUsedForInstancing"),RF_Public)			UBoolProperty(CPP_PROPERTY(bUsedForInstancing),TEXT(""),CPF_Edit);
	UProperty* PerLODStaticLightingForInstancingProp = new(GetClass(),TEXT("bPerLODStaticLightingForInstancing"),RF_Public)		UBoolProperty(CPP_PROPERTY(bPerLODStaticLightingForInstancing),TEXT(""),CPF_Edit);
	UProperty* ConsolePreallocateInstanceCountProp = new(GetClass(),TEXT("ConsolePreallocateInstanceCount"),RF_Public)		UIntProperty(CPP_PROPERTY(ConsolePreallocateInstanceCount),TEXT(""),CPF_Edit);

	UProperty* UseMaximumStreamingTexelRatioProp = new(GetClass(),TEXT("bUseMaximumStreamingTexelRatio"),RF_Public)UBoolProperty(CPP_PROPERTY(bUseMaximumStreamingTexelRatio),TEXT(""),CPF_Edit);
	UProperty* PartitionForEdgeGeometryProp = new(GetClass(),TEXT("bPartitionForEdgeGeometry"),RF_Public)		UBoolProperty(CPP_PROPERTY(bPartitionForEdgeGeometry),TEXT(""),CPF_Edit);
	UProperty* CanBecomeDynamicProp = new(GetClass(),TEXT("bCanBecomeDynamic"),RF_Public)		UBoolProperty(CPP_PROPERTY(bCanBecomeDynamic),TEXT(""),CPF_Edit);
	UProperty* StripComplexCollisionForConsole = new(GetClass(),TEXT("bStripComplexCollisionForConsole"),RF_Public)	UBoolProperty(CPP_PROPERTY(bStripkDOPForConsole),TEXT(""),CPF_Edit);

	UProperty* LightMapResolutionProp = new(GetClass(),TEXT("LightMapResolution"),RF_Public)			UIntProperty(CPP_PROPERTY(LightMapResolution),TEXT(""),CPF_Edit);
	UProperty* LightMapCoordinateIndexProp = new(GetClass(),TEXT("LightMapCoordinateIndex"),RF_Public)		UIntProperty(CPP_PROPERTY(LightMapCoordinateIndex),TEXT(""),CPF_Edit);
	UProperty* LODDistanceRatioProp = new(GetClass(),TEXT("LODDistanceRatio"),RF_Public)				UFloatProperty(CPP_PROPERTY(LODDistanceRatio),TEXT(""),CPF_Edit);
	UProperty* LODMaxRangeRatioProp = new(GetClass(),TEXT("LODMaxRange"),RF_Public)				UFloatProperty(CPP_PROPERTY(LODMaxRange),TEXT(""),CPF_Edit);
	UProperty* StreamingDistanceMultiplierProp = new(GetClass(),TEXT("StreamingDistanceMultiplier"),RF_Public)UFloatProperty(CPP_PROPERTY(StreamingDistanceMultiplier),TEXT(""),CPF_Edit);

	/**
	 * The following code creates a dynamic array of structs, where the struct contains a dynamic array of MaterialInstances...In unrealscript, this declaration
	 * would look something like this:
	 *
	 *	struct StaticMeshLODInfo
	 *	{
	 *		var() editfixedsize native array<MaterialInterface> EditorMaterials;
	 *	};
	 * 
	 *	var() editfixedsize native array<StaticMeshLODInfo> LODInfo;
	 *
	 */

	//////////////////////////////////////////////////////////////////////////
	// First create StaticMeshLODElement struct
	UScriptStruct* LODElementStruct = new(GetClass(),TEXT("StaticMeshLODElement"),RF_Public|RF_Transient|RF_Native) UScriptStruct(NULL);
	INT StructPropertyOffset = 0;
	new(LODElementStruct,TEXT("Material"),RF_Public)				UObjectProperty(EC_CppProperty,StructPropertyOffset,TEXT(""),CPF_Edit,UMaterialInterface::StaticClass());
	StructPropertyOffset += sizeof(UMaterialInterface*);
	new(LODElementStruct,TEXT("bEnableShadowCasting"),RF_Public)	UBoolProperty(EC_CppProperty,StructPropertyOffset,TEXT(""),CPF_Edit | CPF_Native);
	StructPropertyOffset += sizeof(UBOOL);
//	new(LODElementStruct,TEXT("bSelected"),RF_Transient)			UBoolProperty(EC_CppProperty,StructPropertyOffset,TEXT(""),CPF_Edit | CPF_Native);
	StructPropertyOffset += sizeof(UBOOL);
	new(LODElementStruct,TEXT("bEnableCollision"),RF_Public)		UBoolProperty(EC_CppProperty,StructPropertyOffset,TEXT(""),CPF_Edit | CPF_Native);

	// We're finished adding properties to the FStaticMeshLODElement struct - now we link the struct's properties (which sets the PropertiesSize for the struct) and initialize its defaults
	LODElementStruct->SetPropertiesSize(sizeof(FStaticMeshLODElement));
	LODElementStruct->AllocateStructDefaults();
	FArchive ArDummy0;
	LODElementStruct->Link(ArDummy0,0);

	//////////////////////////////////////////////////////////////////////////
	// Then create the StaticMeshLODInfo struct
	UScriptStruct* LODStruct = new(GetClass(),TEXT("StaticMeshLODInfo"),RF_Public|RF_Transient|RF_Native) UScriptStruct(NULL);

	// Next, create the dynamic array of LODElements - use the struct as the outer for the new array property so that the array is contained by the struct
	UArrayProperty*	ElementsProp = new(LODStruct,TEXT("Elements"),RF_Public) UArrayProperty(EC_CppProperty,0,TEXT(""),CPF_Edit | CPF_EditFixedSize | CPF_Native);

	// Dynamic arrays have an Inner property which corresponds to the array type.
	ElementsProp->Inner	= new(ElementsProp,TEXT("StructProperty1"),RF_Public) UStructProperty(EC_CppProperty,0,TEXT(""),CPF_Edit,LODElementStruct);

	// Link defaults
	LODStruct->SetPropertiesSize(sizeof(FStaticMeshLODInfo));
	LODStruct->AllocateStructDefaults();
	FArchive ArDummy1;
	LODStruct->Link(ArDummy1,0);

	//////////////////////////////////////////////////////////////////////////
	// Finally add array property to the StaticMesh class itself.

	// Next, create the dynamic array of StaticMeshLODInfo structs...same procedure as creating the Materials array above, except that this time, we use the class as the Outer for
	// the array, so that the property becomes a member of the class
	UArrayProperty*	InfoProp = new(GetClass(),TEXT("LODInfo"),RF_Public) UArrayProperty(CPP_PROPERTY(LODInfo),TEXT(""),CPF_Edit | CPF_EditFixedSize | CPF_Native);
	InfoProp->Inner = new(InfoProp,TEXT("StructProperty0"),RF_Public) UStructProperty(EC_CppProperty,0,TEXT(""),CPF_Edit,LODStruct);

	// Add physics body setup
	new(GetClass(),TEXT("BodySetup"),RF_Public)							UObjectProperty(CPP_PROPERTY(BodySetup),TEXT(""),CPF_Edit | CPF_EditInline, URB_BodySetup::StaticClass());

	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UStaticMesh, BodySetup ) ); //@todo rtgc: is this needed seeing that BodySetup is exposed above?

#if WITH_EDITORONLY_DATA
	new(GetClass(),TEXT("FoliageDefaultSettings"),RF_Public)	UObjectProperty(CPP_PROPERTY(FoliageDefaultSettings),TEXT(""),CPF_Edit | CPF_EditInline, UInstancedFoliageSettings::StaticClass());
	new(GetClass(), TEXT("SourceFilePath"), RF_Public)	UStrProperty(CPP_PROPERTY(SourceFilePath),TEXT(""),CPF_Edit|CPF_EditorOnly|CPF_EditConst);
	new(GetClass(), TEXT("SourceFileTimestamp"), RF_Public)	UStrProperty(CPP_PROPERTY(SourceFileTimestamp),TEXT(""),CPF_Edit|CPF_EditorOnly|CPF_EditConst);
#endif // WITH_EDITORONLY_DATA
}

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UStaticMesh::InitializeIntrinsicPropertyValues()
{
	UseSimpleLineCollision		= TRUE;
	UseSimpleBoxCollision		= TRUE;
	UseSimpleRigidBodyCollision = TRUE;
	UseFullPrecisionUVs			= FALSE;
	bUsedForInstancing			= FALSE;
	bUseMaximumStreamingTexelRatio = FALSE;
	bPartitionForEdgeGeometry   = TRUE;
	bCanBecomeDynamic			= FALSE;
	bStripkDOPForConsole		= FALSE;
	bPerLODStaticLightingForInstancing = FALSE;
	ConsolePreallocateInstanceCount = 0;
	LODDistanceRatio			= 1.0;
	LODMaxRange					= 2000;
	ElementToIgnoreForTexFactor	= -1;
	HighResSourceMeshCRC		= 0;
	LightMapResolution			= 0;
	LightMapCoordinateIndex		= 0;
#if WITH_EDITORONLY_DATA
	FoliageDefaultSettings		= NULL;
	SourceFilePath				= "";
	SourceFileTimestamp			= "";
#endif // WITH_EDITORONLY_DATA
	StreamingDistanceMultiplier = 1.0f;
	VertexPositionVersionNumber = 0;
	bRemoveDegenerates			= TRUE;
}

/**
 * Callback used to allow object register its direct object references that are not already covered by
 * the token stream.
 *
 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
 */
void UStaticMesh::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects( ObjectArray );
	for( INT LODIndex=0; LODIndex<LODModels.Num(); LODIndex++ )
	{
		const FStaticMeshRenderData& LODRenderData = LODModels(LODIndex);
		for( INT ElementIndex=0; ElementIndex<LODRenderData.Elements.Num(); ElementIndex++ ) 
		{
			AddReferencedObject( ObjectArray, LODRenderData.Elements(ElementIndex).Material );
		}
	}
}

void UStaticMesh::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	// Release the static mesh's resources.
	ReleaseResources();

	// Flush the resource release commands to the rendering thread to ensure that the edit change doesn't occur while a resource is still
	// allocated, and potentially accessing the UStaticMesh.
	ReleaseResourcesFence.Wait();
}

void UStaticMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	LightMapResolution = Max(LightMapResolution, 0);

	UBOOL	bNeedsRebuild = FALSE;

	const UPackage* ExistingPackage = GetOutermost();
	const UBOOL bHasSourceData = ( LODModels.Num() > 0 && LODModels( 0 ).RawTriangles.GetElementCount() > 0 );
	if( ExistingPackage->PackageFlags & PKG_Cooked || !bHasSourceData )
	{
		// We can't rebuild the mesh because it has been cooked and
		// therefore the raw mesh data has been stripped.
	}
	else
	{
		// If any of the elements have had collision added or removed, rebuild the static mesh.
		// If any of the elements have had materials altered, update here
		for(INT i=0;i<LODModels.Num();i++)
		{
			FStaticMeshRenderData&	RenderData				= LODModels(i);
			FStaticMeshLODInfo& ThisLODInfo = LODInfo(i);

			for(INT ElementIndex = 0;ElementIndex < RenderData.Elements.Num();ElementIndex++)
			{
				if(ElementIndex < ThisLODInfo.Elements.Num())
				{
					FStaticMeshLODElement& ElementInfo = ThisLODInfo.Elements(ElementIndex);
					UMaterialInterface*& EditorMat = ElementInfo.Material;

					// This only NULLs out direct references to materials.  We purposefully do not NULL
					// references to material instances that refer to decal materials, and instead just
					// warn about them during map checking.  The reason is that the side effects of NULLing
					// refs to instances pointing to decal materials would be strange (e.g. changing an
					// instance to refer to a decal material instead would then cause references to the
					// instance to break.
					if ( EditorMat && EditorMat->IsA( UDecalMaterial::StaticClass() ) )
					{
						EditorMat = NULL;
					}

					const UBOOL bEditorEnableShadowCasting = ElementInfo.bEnableShadowCasting;
					const UBOOL bEditorEnableCollision = ElementInfo.bEnableCollision;

					// Copy from UI expose array to 'real' array.
					FStaticMeshElement&	MeshElement				= RenderData.Elements(ElementIndex);

					MeshElement.Material				= EditorMat;
					MeshElement.bEnableShadowCasting	= bEditorEnableShadowCasting;
					MeshElement.EnableCollision			= bEditorEnableCollision;
					if(MeshElement.OldEnableCollision != MeshElement.EnableCollision)
					{
						bNeedsRebuild = TRUE;
						MeshElement.OldEnableCollision = MeshElement.EnableCollision;
					}
				}
			}
		}

		// if UV storage precision has changed then rebuild from source vertex data
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
		if( PropertyThatChanged != NULL && 
			PropertyThatChanged->GetFName() == FName(TEXT("UseFullPrecisionUVs")) )
		{
			if( !UseFullPrecisionUVs && !GVertexElementTypeSupport.IsSupported(VET_Half2) )
			{
				UseFullPrecisionUVs = TRUE;
				warnf(TEXT("16 bit UVs not supported. Reverting to 32 bit UVs"));			
			}
			if( LODModels.Num() > 0 )
			{
				FStaticMeshRenderData& RenderData = LODModels(0);
				if( UseFullPrecisionUVs != RenderData.VertexBuffer.GetUseFullPrecisionUVs() )
				{
					bNeedsRebuild = TRUE;
				}
			}
		}
	}

	if ( PropertyThatChanged->GetName() == TEXT("StreamingDistanceMultiplier") )
	{
		// Allow recalculating the texture factor.
		CachedStreamingTextureFactors.Empty();
		// Recalculate in a few seconds.
		ULevel::TriggerStreamingDataRebuild();
	}

	if(bNeedsRebuild)
	{
		Build();
	}
	else
	{
        // Reinitialize the static mesh's resources.		
		InitResources();

		// Only unbuild lighting for properties which affect static lighting
		UBOOL bUnbuildLighting = FALSE;
		if (PropertyThatChanged &&
			(PropertyThatChanged->GetName() == TEXT("LightMapResolution")
			|| PropertyThatChanged->GetName() == TEXT("LightMapCoordinateIndex")))
		{
			bUnbuildLighting = TRUE;
		}

		FStaticMeshComponentReattachContext(this, bUnbuildLighting);		
	}
	
	// make it has a new guid
	SetLightingGuid();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


/**
 * Called by the editor to query whether a property of this object is allowed to be modified.
 * The property editor uses this to disable controls for properties that should not be changed.
 * When overriding this function you should always call the parent implementation first.
 *
 * @param	InProperty	The property to query
 *
 * @return	TRUE if the property can be modified in the editor, otherwise FALSE
 */
UBOOL UStaticMesh::CanEditChange( const UProperty* InProperty ) const
{
	UBOOL bIsMutable = Super::CanEditChange( InProperty );
	if( bIsMutable && InProperty != NULL )
	{
		// If the mesh source data has been stripped then we'll disable certain property
		// window controls as these settings are now locked in place with the built mesh.
		const UBOOL bHasSourceData = ( LODModels.Num() > 0 && LODModels( 0 ).RawTriangles.GetElementCount() > 0 );

		if( InProperty->GetFName() == TEXT( "bEnableCollision" ) && !bHasSourceData )
		{
			bIsMutable = FALSE;
		}

		if( InProperty->GetFName() == TEXT( "UseFullPrecisionUVs" ) && !bHasSourceData )
		{
			bIsMutable = FALSE;
		}
	}

	return bIsMutable;
}


void UStaticMesh::BeginDestroy()
{
	Super::BeginDestroy();

	if( !GIsUCC && !HasAnyFlags(RF_ClassDefaultObject) )
	{
	ReleaseResources();
	}

	// Free any physics-engine per-poly meshes.
	ClearPhysMeshCache();
}

UBOOL UStaticMesh::IsReadyForFinishDestroy()
{
	return ReleaseResourcesFence.GetNumPendingFences() == 0;
}

//
//	UStaticMesh::Rename
//

UBOOL UStaticMesh::Rename( const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags )
{
	// Rename the static mesh
    return Super::Rename( InName, NewOuter, Flags );
}


/**
 * Called after duplication & serialization and before PostLoad. Used to e.g. make sure UStaticMesh's UModel
 * gets copied as well.
 */
void UStaticMesh::PostDuplicate()
{
	Super::PostDuplicate();
}

/**
 *	UStaticMesh::Serialize
 */
void UStaticMesh::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Bounds;
	Ar << BodySetup;
	if( !Ar.IsLoading() || Ar.Ver() >= VER_COMPACTKDOPSTATICMESH )
	{
		Ar << kDOPTree;
	}
	else if (Ar.IsLoading())
	{
		LegacykDOPTree = new LegacykDOPTreeType();
		Ar << *LegacykDOPTree; // load the old tree for conversion in postload
	}

	if( Ar.IsLoading() )
	{
		Ar << InternalVersion;
	}
	else if( Ar.IsSaving() )
	{
		InternalVersion = STATICMESH_VERSION;
		Ar << InternalVersion;
	}

	// NOTE: The following can be removed after we resave everything and deprecate backwards compatibility
	const INT STATICMESH_VERSION_CONTENT_TAGS = 17;	// Content tags were introduced in SM version 17
	if( InternalVersion >= STATICMESH_VERSION_CONTENT_TAGS &&
		Ar.Ver() < VER_REMOVED_LEGACY_CONTENT_TAGS )
	{
		TArray<FName> IgnoredLegacyContentTags;
		Ar << IgnoredLegacyContentTags;
	}

	if ( Ar.Ver() >= VER_STATIC_MESH_SOURCE_DATA_COPY )
	{
		Ar << SourceData;
		if ( Ar.Ver() >= VER_STORE_MESH_OPTIMIZATION_SETTINGS )
		{
			Ar << OptimizationSettings;
		}
		else if ( Ar.IsLoading() )
		{
			TArray<FLOAT> MaxDeviations;
			Ar << MaxDeviations;
			for ( INT LODIndex = 0; LODIndex < MaxDeviations.Num(); ++LODIndex )
			{
				check( OptimizationSettings.Num() == LODIndex );
				FStaticMeshOptimizationSettings Settings;
				const FLOAT MaxDeviation = MaxDeviations( LODIndex );
				Settings.MaxDeviationPercentage = MaxDeviation / Bounds.SphereRadius;
				OptimizationSettings.AddItem( Settings );
			}
		}
		Ar << bHasBeenSimplified;
	}
	else
	{
		check( Ar.IsLoading() );
		OptimizationSettings.Empty();
		bHasBeenSimplified = FALSE;
	}

	if (Ar.Ver() >= VER_TAG_MESH_PROXIES)
	{
		Ar << bIsMeshProxy;
	}
	else
	{
		check(Ar.IsLoading());
		bIsMeshProxy = FALSE;
	}

	LODModels.Serialize( Ar, this );
	
	Ar << LODInfo;
	
	Ar << ThumbnailAngle;

	Ar << ThumbnailDistance;
	
	if( Ar.IsCountingMemory() )
	{
		Ar << PhysMeshScale3D;

		// Include collision as part of memory used
		if ( BodySetup )
		{
			BodySetup->Serialize( Ar );
		}

		//TODO: Count these members when calculating memory used
		//Ar << kDOPTreeType;
		//Ar << PhysMesh;
		//Ar << ReleaseResourcesFence;
	}

	if( !Ar.IsLoading() || Ar.Ver() >= VER_STATICMESH_VERSION_18 )
	{
		Ar << HighResSourceMeshName;
		Ar << HighResSourceMeshCRC;
	}

	// serialize the lighting guid if it's there
	if (Ar.Ver() < VER_INTEGRATED_LIGHTMASS)
	{
		SetLightingGuid();
	}
	else
	{
		Ar << LightingGuid;
	}

	// Serialize the vertex position version number if it's there
	if ( Ar.Ver() >= VER_PRESERVE_SMC_VERT_COLORS )
	{
		Ar << VertexPositionVersionNumber;
	}
	else
	{
		VertexPositionVersionNumber = 0;
	}

	if ( Ar.Ver() >= VER_DYNAMICTEXTUREINSTANCES )
	{
		Ar << CachedStreamingTextureFactors;
	}
	
	if( Ar.Ver() >= VER_KEEP_STATIC_MESH_DEGENERATES )
	{
		Ar << bRemoveDegenerates;
	}
	else
	{
		bRemoveDegenerates = TRUE;
	}

	if( Ar.Ver() >= VER_INSTANCED_STATIC_MESH_PER_LOD_STATIC_LIGHTING )
	{
		Ar << bPerLODStaticLightingForInstancing;
		Ar << ConsolePreallocateInstanceCount;
	}
	else
	{
		bPerLODStaticLightingForInstancing = FALSE;
		ConsolePreallocateInstanceCount = 0;
}
}



//
//	UStaticMesh::PostLoad
//

void UStaticMesh::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (!GIsGame)
	{
		// Fixup possible element issues
		for (INT LODIdx = 0; LODIdx < LODModels.Num(); LODIdx++)
		{
			FStaticMeshRenderData& LODData = LODModels(LODIdx);
			UBOOL bHasMismatchedIndices = FALSE;
			TArray<INT> MaterialIndices;
			MaterialIndices.Empty(LODData.Elements.Num());
			MaterialIndices.AddZeroed(LODData.Elements.Num());

			// Check each element for: 
			//		Element.MaterialIndex != ElementIndex
			//		(Element.NumTriangle == 0) && (Element.Material != NULL)
			for (INT ElementIdx = 0; ElementIdx < LODData.Elements.Num(); ElementIdx++)
			{
				FStaticMeshElement& Element = LODData.Elements(ElementIdx);
				MaterialIndices(ElementIdx) = Element.MaterialIndex;
				if (Element.MaterialIndex != ElementIdx)
				{
					// We will fix this up next
					bHasMismatchedIndices = TRUE;
				}
				if ((Element.NumTriangles == 0) && (Element.Material != NULL))
				{
					// Tell the user was are clearing this material as there are no triangles.
					// This is to avoid pulling in unused material on cooked content platforms.
					Element.Material = NULL;
				}
			}

			if (bHasMismatchedIndices == TRUE)
			{
				warnf(NAME_Warning, *FString::Printf(LocalizeSecure(LocalizeUnrealEd(TEXT("FixingMismatchedIndicesOnStaticMesh"),TEXT("UnrealEd")), *GetPathName())));
				//@todo. Sort them to be safe?
				INT MaxMaterialIdx = -1;
				INT MaxMaterialIdxIndex = -1;

				// Find the max material index that is used
				for (INT MaterialIdx = 0; MaterialIdx < MaterialIndices.Num(); MaterialIdx++)
				{
					if (MaterialIndices(MaterialIdx) > MaxMaterialIdx)
					{
						MaxMaterialIdx = MaterialIndices(MaterialIdx);
						MaxMaterialIdxIndex = MaterialIdx;
					}
				}
				check(MaxMaterialIdxIndex == MaterialIndices.Num() - 1);

				// Find the 'missing' material indices
				TMap<INT,UBOOL> MissingMaterialIndices;
				for (INT CheckMtrlIdx = 0; CheckMtrlIdx < MaxMaterialIdx + 1; CheckMtrlIdx++)
				{
					INT DummyIdx;
					if (MaterialIndices.FindItem(CheckMtrlIdx, DummyIdx) == FALSE)
					{
						MissingMaterialIndices.Set(CheckMtrlIdx,FALSE);
					}
				}

				// Find any triangle that reference missing indices in the raw data of the mesh
				TMap<INT,INT> RawTriangleMaterialIndices;
				if (MissingMaterialIndices.Num() > 0)
				{
					// See if there are triangles that actually use that index...
					FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*)(LODData.RawTriangles.Lock(LOCK_READ_ONLY));
					if (RawTriangleData != NULL)
					{
						for (INT TriIdx = 0; TriIdx < LODData.RawTriangles.GetElementCount(); TriIdx++)
						{
							FStaticMeshTriangle& Triangle = RawTriangleData[TriIdx];
							INT* Count = RawTriangleMaterialIndices.Find(Triangle.MaterialIndex);
							if (Count == NULL)
							{
								RawTriangleMaterialIndices.Set(Triangle.MaterialIndex,0);
								Count = RawTriangleMaterialIndices.Find(Triangle.MaterialIndex);
							}
							check(Count);
							(*Count)++;
						}
						LODData.RawTriangles.Unlock();
					}
				}

				// Update the max material index w/ those from the RawTriangle data (just to be safe)
				for (TMap<INT,INT>::TIterator RawIt(RawTriangleMaterialIndices); RawIt; ++RawIt)
				{
					INT RawIdx = RawIt.Key();
					if (RawIdx > MaxMaterialIdx)
					{
						MaxMaterialIdx = RawIdx;
					}
				}

				// Make a new element array, and fill in with all the material indices
				TArray<FStaticMeshElement> LODDataElements;
				LODDataElements.Empty(MaxMaterialIdx+1);
				LODDataElements.AddZeroed(MaxMaterialIdx+1);

				for (INT MtrlIdx = MaxMaterialIdx; MtrlIdx >= 0; MtrlIdx--)
				{
					// Find the material index in the actual list...
					FStaticMeshElement* OrigElement = NULL;
					for (INT ElementIdx = 0; ElementIdx < LODData.Elements.Num(); ElementIdx++)
					{
						if (LODData.Elements(ElementIdx).MaterialIndex == MtrlIdx)
						{
							OrigElement = &LODData.Elements(ElementIdx);
						}
					}

					INT* Count = RawTriangleMaterialIndices.Find(MtrlIdx);
					if (OrigElement != NULL)
					{
						LODDataElements(MtrlIdx) = *OrigElement;
					}
					else
					{
						FStaticMeshElement& NewElement = LODDataElements(MtrlIdx);
						NewElement.NumTriangles = Count ? *Count : 0;
						NewElement.MaterialIndex = MtrlIdx;
					}
				}

				// Just remove all empty elements from the end of the list
				UBOOL bFoundNonEmptyElement = FALSE;
				for (INT ElementIdx = LODDataElements.Num() - 1; (ElementIdx >= 0) && (bFoundNonEmptyElement == FALSE); ElementIdx--)
				{
					FStaticMeshElement& Element = LODDataElements(ElementIdx);
					if (Element.NumTriangles == 0)
					{
						// Remove it...
						LODDataElements.Remove(ElementIdx);
					}
					else
					{
						bFoundNonEmptyElement = TRUE;
					}
				}

				// Set the new elements array
				LODData.Elements.Empty();
				LODData.Elements = LODDataElements;

				// Force the mesh to be rebuilt... working around the bOnlyMinorDifferences check below (-2)
				InternalVersion -= 2;
			}
		}

		// Clear material references in source data.
		SourceData.ClearMaterialReferences();
	}

	// Build adjacency information for meshes that have not yet had it built.
#if WITH_D3D11_TESSELLATION
	if (!GUseSeekFreeLoading && !(GCookingTarget & UE3::PLATFORM_Console))
	{
		UBOOL bShouldCheckAdjacencyInformation = FALSE;
		GConfig->GetBool( TEXT("UnrealEd.PropertyFilters"), TEXT("bShowD3D11Properties"), bShouldCheckAdjacencyInformation, GEditorUserSettingsIni );
		if ( bShouldCheckAdjacencyInformation )
	{
		for ( INT LODIndex = 0; LODIndex < LODModels.Num(); ++LODIndex )
		{
			FStaticMeshRenderData& LODModel = LODModels( LODIndex );
			if ( LODModel.AdjacencyIndexBuffer.Indices.Num() == 0 )
			{
				warnf( TEXT("Building adjacency information for static mesh '%s'."), *GetPathName() );
				BuildStaticAdjacencyIndexBuffer( LODModel.PositionVertexBuffer, LODModel.VertexBuffer, LODModel.IndexBuffer.Indices, LODModel.AdjacencyIndexBuffer.Indices );
			}
		}
	}
	}
#endif // #if WITH_D3D11_TESSELLATION
#endif	//#if WITH_EDITOR

	UBOOL bWasBuilt = FALSE;

#if WITH_EDITOR && !WITH_SIMPLYGON
	if ( bHasBeenSimplified )
	{
		// All LODs need to be removed as they may have been generated by Simplygon.
		for ( INT LODIndex = LODModels.Num() - 1; LODIndex > 0; --LODIndex )
		{
			LODModels.Remove( LODIndex );
			if ( LODInfo.IsValidIndex( LODIndex ) )
			{
				LODInfo.Remove( LODIndex );
			}
		}

		// Mesh proxies need to dump LOD 0 too.
		if ( LODModels.Num() >= 1 && bIsMeshProxy )
		{
			FStaticMeshRenderData& BaseModel = LODModels(0);
			BaseModel.RawTriangles.Lock(LOCK_READ_WRITE);
			FStaticMeshTriangle* DegenerateTri = (FStaticMeshTriangle*)BaseModel.RawTriangles.Realloc(1);
			appMemZero(*DegenerateTri);
			BaseModel.RawTriangles.Unlock();
			Build();
			bWasBuilt = TRUE;
		}
		// Rebuild the base LOD from the source triangles.
		else if ( LODModels.Num() >= 1 && SourceData.IsInitialized() )
		{
			FStaticMeshRenderData& SourceModel = GetSourceData();
			FStaticMeshRenderData& BaseModel = LODModels(0);
			const BYTE* SrcTriangles = (const BYTE*)SourceModel.RawTriangles.Lock( LOCK_READ_ONLY );
			BaseModel.RawTriangles.Lock( LOCK_READ_WRITE );
			BYTE* DestTriangles = (BYTE*)BaseModel.RawTriangles.Realloc( SourceModel.RawTriangles.GetElementCount() );
			appMemcpy( DestTriangles, SrcTriangles, SourceModel.RawTriangles.GetBulkDataSize() );
			BaseModel.RawTriangles.Unlock();
			SourceModel.RawTriangles.Unlock();
			Build();
			bWasBuilt = TRUE;
		}

		// Clear optimization settings and the simplified flag.
		bHasBeenSimplified = FALSE;
		bIsMeshProxy = FALSE;
		OptimizationSettings.Empty();

		warnf( LocalizeSecure( LocalizeUnrealEd( TEXT("MeshSimp_LicenseRequired_F") ), *GetPathName() ) );
	}
#endif // #if WITH_EDITOR && !WITH_SIMPLYGON

	// Rebuild static mesh if internal version has been bumped and we allow meshes in this package to be rebuilt. E.g. during package resave we only want to
	// rebuild static meshes in the package we are going to save to cut down on the time it takes to resave all.
	if(InternalVersion < STATICMESH_VERSION && (GStaticMeshPackageNameToRebuild == NAME_None || GStaticMeshPackageNameToRebuild == GetOutermost()->GetFName()) )
	{	
		// Don't bother rebuilding if there are only minor format differences
		UBOOL bOnlyMinorDifferences = FALSE;

		// NOTE: Version 18 only introduced a named reference to a 'high res source mesh name', no structural differences
		const INT STATICMESH_VERSION_SIMPLIFICATION = 18;
		if( STATICMESH_VERSION == STATICMESH_VERSION_SIMPLIFICATION &&
			( InternalVersion == STATICMESH_VERSION - 1 ) )
		{
			bOnlyMinorDifferences = TRUE;
		}

		if( !bOnlyMinorDifferences )
		{
			Build();
			bWasBuilt = TRUE;
		}
	}

	// Revert to using 32 bit Float UVs on hardware that doesn't support rendering with 16 bit Float UVs 
	if( !GIsCooking && !GVertexElementTypeSupport.IsSupported(VET_Half2) )
	{
		for (INT LODIdx = 0; LODIdx < LODModels.Num(); LODIdx++)
		{
			FStaticMeshRenderData& LODData = LODModels(LODIdx);
			FStaticMeshVertexBuffer& VertexBuffer = LODData.VertexBuffer;
			{
				switch(VertexBuffer.GetNumTexCoords())
				{
				case 1:	VertexBuffer.ConvertToFullPrecisionUVs<1>(); break;
				case 2:	VertexBuffer.ConvertToFullPrecisionUVs<2>(); break;
				case 3:	VertexBuffer.ConvertToFullPrecisionUVs<3>(); break;
				case 4:	VertexBuffer.ConvertToFullPrecisionUVs<4>(); break;
				default: appErrorf(TEXT("Invalid number of texture coordinates"));
				}
			}
		}
	}

#if DEDICATED_SERVER
	if (!UseSimpleRigidBodyCollision && GIsSeekFreePCServer)
	{
		for( INT LODIdx=0; LODIdx < LODModels.Num(); LODIdx++ )
		{
			FStaticMeshRenderData& LODModel = LODModels(LODIdx);

			// Strip Index buffers we were unable to strip during cook.  See UStaticMesh::StripData()
 			LODModel.IndexBuffer.Indices.Empty();
		}
	}
#endif

	if (LegacykDOPTree && !bWasBuilt && LODModels.Num() )
	{
		DOUBLE StartTime = appSeconds();
		TArray<FkDOPBuildCollisionTriangle<WORD> > kDOPBuildTriangles;
		FPositionVertexBuffer* PositionVertexBuffer = &LODModels(0).PositionVertexBuffer;
		for (INT TriangleIndex = 0; TriangleIndex < LegacykDOPTree->Triangles.Num(); TriangleIndex++)
		{
			FkDOPCollisionTriangle<WORD>& OldTriangle = LegacykDOPTree->Triangles(TriangleIndex);
			new (kDOPBuildTriangles) FkDOPBuildCollisionTriangle<WORD>(
				OldTriangle.v1,
				OldTriangle.v2,
				OldTriangle.v3,
				OldTriangle.MaterialIndex,
				PositionVertexBuffer->VertexPosition(OldTriangle.v1),
				PositionVertexBuffer->VertexPosition(OldTriangle.v2),
				PositionVertexBuffer->VertexPosition(OldTriangle.v3));
		}
		kDOPTree.Build(kDOPBuildTriangles);
//		debugf(TEXT("Rebuilt kDop into compact format in %6.2fms"),FLOAT(appSeconds() - StartTime)*1000.0f);
	}
	else if (GetLinkerVersion() < VER_KDOP_ONE_NODE_FIX && kDOPTree.Nodes.Num() == 2 && !bWasBuilt && LODModels.Num() )
	{
		TArray<FkDOPBuildCollisionTriangle<WORD> > kDOPBuildTriangles;
		FPositionVertexBuffer* PositionVertexBuffer = &LODModels(0).PositionVertexBuffer;
		for (INT TriangleIndex = 0; TriangleIndex < kDOPTree.Triangles.Num(); TriangleIndex++)
		{
			FkDOPCollisionTriangle<WORD>& OldTriangle = kDOPTree.Triangles(TriangleIndex);
			new (kDOPBuildTriangles) FkDOPBuildCollisionTriangle<WORD>(
				OldTriangle.v1,
				OldTriangle.v2,
				OldTriangle.v3,
				OldTriangle.MaterialIndex,
				PositionVertexBuffer->VertexPosition(OldTriangle.v1),
				PositionVertexBuffer->VertexPosition(OldTriangle.v2),
				PositionVertexBuffer->VertexPosition(OldTriangle.v3));
		}
		kDOPTree.Build(kDOPBuildTriangles);
	}
	// we are done with the old tree
	delete LegacykDOPTree; 
	LegacykDOPTree = 0;


#if !CONSOLE
	// Strip away loaded Editor-only data if we're a client and never care about saving.
	if( GIsClient && !GIsEditor && !GIsUCC )
	{
		// Console platform is not a mistake, this ensures that as much as possible will be tossed.
		StripData( (UE3::EPlatformType)(UE3::PLATFORM_Console | UE3::PLATFORM_WindowsConsole), FALSE );
	}
#endif

	if( !GIsUCC && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		InitResources();
	}
	check(LODModels.Num() == LODInfo.Num());
	if( GIsEditor )
	{
		UScriptStruct* LODElementStruct = CastChecked<UScriptStruct>( StaticFindObject( UScriptStruct::StaticClass(), ANY_PACKAGE, TEXT("StaticMeshLODElement")) );
		for( TFieldIterator<UProperty> It( LODElementStruct ); It; ++It )
		{
			UProperty* Prop = *It;

			// Set the property "display name" for bEnableCollision
			if (Prop->GetName() == TEXT("bEnableCollision"))
			{
				FString KeyName = TEXT("DisplayName");
				if (!Prop->HasMetaData(*KeyName))
				{
					Prop->SetMetaData(*KeyName, TEXT("Enable Per Poly Collision"));
				}
			}
		}
	}
}

/**
 * Used by various commandlets to purge editor only and platform-specific data from various objects
 * 
 * @param PlatformsToKeep Platforms for which to keep platform-specific data
 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
 */
void UStaticMesh::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData); 

#if WITH_EDITORONLY_DATA
	if (bStripLargeEditorData)
	{
		// RawTriangles is only used in the Editor and for rebuilding static meshes.
		for( INT i=0; i<LODModels.Num(); i++ )
		{
			FStaticMeshRenderData& LODModel = LODModels(i);
			LODModel.RawTriangles.RemoveBulkData();
			LODModel.WireframeIndexBuffer.Indices.Empty();
		}

		// HighResSourceMeshName is used for mesh simplification features in the editor
		HighResSourceMeshName.Empty();

		// Source data and optimization settings are only needed for mesh simplification in the editor.
		SourceData.Clear();
		OptimizationSettings.Empty();
	}

	// Only Windows needs the adjacency index buffer. If D3D11 tessellation is disabled, always strip it.
#if WITH_D3D11_TESSELLATION
	if( !(PlatformsToKeep & (UE3::PLATFORM_Windows | UE3::PLATFORM_WindowsConsole)) )
#endif // #if WITH_D3D11_TESSELLATION
	{
		for( INT LODIdx=0; LODIdx < LODModels.Num(); LODIdx++ )
		{
			FStaticMeshRenderData& LODModel = LODModels(LODIdx);
			LODModel.AdjacencyIndexBuffer.Indices.Empty();
		}
	}

	// Dedicated servers don't need vertex/index buffer data, so
	// toss if the only platform to keep is WindowsServer
	if (!(PlatformsToKeep & ~UE3::PLATFORM_WindowsServer))
	{
		for( INT LODIdx=0; LODIdx < LODModels.Num(); LODIdx++ )
		{
			FStaticMeshRenderData& LODModel = LODModels(LODIdx);

			// Strip Index/Vertex/Color buffers
			if (UseSimpleRigidBodyCollision)
			{
				// Keep index data if we need to generate Novodex collision (per poly collision)
				// We'll strip this in PostLoad()
				LODModel.IndexBuffer.Indices.Empty();
			}
			LODModel.AdjacencyIndexBuffer.Indices.Empty();
			LODModel.VertexBuffer.CleanUp();
			LODModel.ColorVertexBuffer.CleanUp();
		}
	}
#endif // WITH_EDITORONLY_DATA

	// Strip an unwanted complex collision data aka kDOP tree.
	if( bStripkDOPForConsole && ((PlatformsToKeep & UE3::PLATFORM_PS3) || (PlatformsToKeep & UE3::PLATFORM_Xbox360)) )
	{
		kDOPTree.Nodes.Empty();
		kDOPTree.Triangles.Empty();
	}
}

/**
* Used by the cooker to pre cache the convex data for a given static mesh.  
* This data is stored with the level.
* @param Level - the level the data is cooked for
* @param TotalScale3D - the scale to cook the data at
* @param Owner - owner of this mesh for debug purposes (can be NULL)
* @param TriByteCount - running total of memory usage for per-tri collision
* @param TriMeshCount - running count of per-tri collision cache
* @param HullByteCount - running total of memory usage for hull cache
* @param HullCount - running count of hull cache
*/
void UStaticMesh::CookPhysConvexDataForScale(ULevel* Level, const FVector& TotalScale3D, const AActor* Owner, INT& TriByteCount, INT& TriMeshCount, INT& HullByteCount, INT& HullCount)
{
	// If we are doing per-tri collision..
	if(!UseSimpleRigidBodyCollision)
	{
        check(Level);

		// See if we already have cached data for this mesh at this scale.
		FKCachedPerTriData* TestData = Level->FindPhysPerTriStaticMeshCachedData(this, TotalScale3D);
		if(!TestData)
		{
			// If not, cook it now and add to store and map
			INT NewPerTriDataIndex = Level->CachedPhysPerTriSMDataStore.AddZeroed();
			FKCachedPerTriData* NewPerTriData = &Level->CachedPhysPerTriSMDataStore(NewPerTriDataIndex);

			FCachedPerTriPhysSMData NewCachedData;
			NewCachedData.Scale3D = TotalScale3D;
			NewCachedData.CachedDataIndex = NewPerTriDataIndex;

			FString DebugName = FString::Printf(TEXT("%s %s"), *Level->GetName(), *GetName() );
			MakeCachedPerTriMeshDataForStaticMesh( NewPerTriData, this, TotalScale3D, *DebugName );

			// Log to memory used total.
			TriByteCount += NewPerTriData->CachedPerTriData.Num();
			TriMeshCount++;

			Level->CachedPhysPerTriSMDataMap.Add( this, NewCachedData );

			//debugf( TEXT("Added PER-TRI: %s @ [%f %f %f]"), *GetName(), TotalScale3D.X, TotalScale3D.Y, TotalScale3D.Z );
		}
	}
	// If we have simplified collision..
	// And it has some convex bits
	else if(BodySetup && BodySetup->AggGeom.ConvexElems.Num() > 0)
	{
		check(Level);

		// First see if its already in the cache
		FKCachedConvexData* TestData = Level->FindPhysStaticMeshCachedData(this, TotalScale3D);

		// If not, cook it and add it.
		if(!TestData)
		{
			// Create new struct for the cache
			INT NewConvexDataIndex = Level->CachedPhysSMDataStore.AddZeroed();
			FKCachedConvexData* NewConvexData = &Level->CachedPhysSMDataStore(NewConvexDataIndex);

			FCachedPhysSMData NewCachedData;
			NewCachedData.Scale3D = TotalScale3D;
			NewCachedData.CachedDataIndex = NewConvexDataIndex;

			// Cook the collision geometry at the scale its used at in-level.
			FString DebugName = FString::Printf(TEXT("%s %s"), Owner ? *Owner->GetName() : TEXT("NoOwner"), *GetName() );
			MakeCachedConvexDataForAggGeom( NewConvexData, BodySetup->AggGeom.ConvexElems, TotalScale3D, *DebugName );

			// Add to memory used total.
			for(INT HullIdx = 0; HullIdx < NewConvexData->CachedConvexElements.Num(); HullIdx++)
			{
				FKCachedConvexDataElement& Hull = NewConvexData->CachedConvexElements(HullIdx);
				HullByteCount += Hull.ConvexElementData.Num();
				HullCount++;
			}

			// And add to the cache.
			Level->CachedPhysSMDataMap.Add( this, NewCachedData );

			//debugf( TEXT("Added SIMPLE: %d - %s @ [%f %f %f]"), NewConvexDataIndex, *GetName(), TotalScale3D.X, TotalScale3D.Y, TotalScale3D.Z );
		}
	}
}
//
//	UStaticMesh::GetDesc
//

/** 
 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
 */
FString UStaticMesh::GetDesc()
{
	//@todo: Handle LOD descs here?
	return FString::Printf( TEXT("%d Tris, %d Verts"), LODModels(0).IndexBuffer.Indices.Num() / 3, LODModels(0).NumVertices );
}

/** 
 * Returns detailed info to populate listview columns
 */
FString UStaticMesh::GetDetailedDescription( INT InIndex )
{
	FString Description = TEXT( "" );
	
	if(LODModels.Num() == 0)
	{
		Description = TEXT("No Render Data!");
	}
	else
	{
		switch( InIndex )
		{
		case 0:
			Description = FString::Printf( TEXT( "%d triangles" ), LODModels(0).IndexBuffer.Indices.Num() / 3 );
			break;
		case 1: 
			Description = FString::Printf( TEXT( "%d vertices" ), LODModels(0).NumVertices );
			break;
		case 2: 
			// Bounds
			Description = FString::Printf( TEXT( "%.2f x %.2f x %.2f" ),
				Bounds.BoxExtent.X * 2.0f,
				Bounds.BoxExtent.Y * 2.0f,
				Bounds.BoxExtent.Z * 2.0f );
			break;
		}	
	}
	
	return( Description );
}

#if USE_GAMEPLAY_PROFILER
/** 
 * This function actually does the work for the GetProfilerAssetObject and is virtual.  
 * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
 **/
UObject* UStaticMeshComponent::GetProfilerAssetObjectInternal() const
{
	return StaticMesh;
}
#endif

/**
 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
 * you have a component of interest but what you really want is some characteristic that you can use to track
 * down where it came from.  
 *
 */
FString UStaticMeshComponent::GetDetailedInfoInternal() const
{
	FString Result;  

	if( StaticMesh != NULL )
	{
		Result = StaticMesh->GetPathName( NULL );
	}
	else
	{
		Result = TEXT("No_StaticMesh");
	}

	return Result;  
}

#if USE_GAMEPLAY_PROFILER
/** 
 * This function actually does the work for the GetProfilerAssetObject and is virtual.  
 * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
 */
UObject* ADynamicSMActor::GetProfilerAssetObjectInternal() const
{
	if (StaticMeshComponent != NULL)
	{
		return StaticMeshComponent->GetProfilerAssetObjectInternal();
	}
	return NULL;
}
#endif

FString ADynamicSMActor::GetDetailedInfoInternal() const
{
	FString Result;  

	if( StaticMeshComponent != NULL )
	{
		Result = StaticMeshComponent->GetDetailedInfoInternal();
	}
	else
	{
		Result = TEXT("No_StaticMeshComponent");
	}

	return Result;  
}

#if USE_GAMEPLAY_PROFILER
/** 
 * This function actually does the work for the GetProfilerAssetObject and is virtual.  
 * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
 */
UObject* AStaticMeshActor::GetProfilerAssetObjectInternal() const
{
	if (StaticMeshComponent != NULL)
	{
		return StaticMeshComponent->GetProfilerAssetObjectInternal();
	}
	return NULL;
}
#endif

FString AStaticMeshActor::GetDetailedInfoInternal() const
{
	FString Result;  

	if( StaticMeshComponent != NULL )
	{
		Result = StaticMeshComponent->GetDetailedInfoInternal();
	}
	else
	{
		Result = TEXT("No_StaticMeshComponent");
	}

	return Result;  
}

/** Code here to handle special case */
void AStaticMeshActor::SetBase(AActor *NewBase, FVector NewFloor, INT bNotifyActor, USkeletalMeshComponent* SkelComp, FName BoneName )
{
	Super::SetBase(NewBase, NewFloor, bNotifyActor, SkelComp, BoneName);

	// See what our new LOD 'replacement' primitive should be
	UPrimitiveComponent* NewReplacementPrim = NULL;
	if(GIsEditor && !GIsGame)
	{
		// Look for the 'base most' building we are attached to
		AProcBuilding* BaseBuilding = Cast<AProcBuilding>(Base);
		if(BaseBuilding)
		{
			AProcBuilding* BaseMostBuilding = BaseBuilding->GetBaseMostBuilding();
			if(BaseMostBuilding->SimpleMeshComp)
			{
				NewReplacementPrim = BaseMostBuilding->SimpleMeshComp;
			}
			else if(BaseMostBuilding->LowLODPersistentActor && BaseMostBuilding->LowLODPersistentActor->StaticMeshComponent)
			{
				NewReplacementPrim = BaseMostBuilding->LowLODPersistentActor->StaticMeshComponent;
			}
		}
	}

	// See if the new LOD primitive is different from what we currently have. 
	// If so, set it, and update components to note the change
	if(NewReplacementPrim != StaticMeshComponent->ReplacementPrimitive)
	{
		StaticMeshComponent->ReplacementPrimitive = NewReplacementPrim;
		ForceUpdateComponents(FALSE, FALSE);
	}
}

void AStaticMeshActor::PostEditMove( UBOOL bFinished )
{
	Super::PostEditMove( bFinished );

	if( bFinished )
	{
		GCallbackEvent->Send(CALLBACK_BaseSMActorOnProcBuilding, this);
	}
}

#if WITH_EDITOR
/**
	* Show or hide the actors in the group
	*
	* @param	bShow		If TRUE, show the actors; if FALSE, hide them
	* @param bIncProxyFlag If TRUE, set the flag which indicates this was hidden by the proxy
	*/
void AStaticMeshActor::ShowProxy(const UBOOL bShow, const UBOOL bIncProxyFlag)
{
	const UBOOL bHide = !bShow;
	bHiddenEd = bHide;
	bHiddenEdLayer = bHide;
	bHiddenEdScene = bHide;
	bHiddenEdTemporary = bHide;
	if ( StaticMeshComponent )
	{
		StaticMeshComponent->SetHiddenEditor( bHide );
		StaticMeshComponent->SetHiddenGame( bHide );
	}
	if ( bIncProxyFlag )
	{
		SetHiddenByProxy( bHide );
	}
	if ( bHide )
	{
		SetFlags(RF_NotForClient|RF_NotForServer);
	}
	else
	{
		ClearFlags(RF_NotForClient|RF_NotForServer);
	}
	ForceUpdateComponents( FALSE, FALSE );
}

/**
	* Flag that this mesh is a proxy
	*/
void AStaticMeshActor::SetProxy(const UBOOL Proxy)
{
	bProxy = Proxy;
}

/**
	* Get whether this is a proxy
	*/
UBOOL AStaticMeshActor::IsProxy() const
{
	return bProxy;
}

/**
	* Flag that this mesh is hidden because we have a proxy in it's place
	*/
void AStaticMeshActor::SetHiddenByProxy(const UBOOL HiddenByProxy)
{
	if ( bHiddenByProxy != HiddenByProxy )
	{
		bHiddenByProxy = HiddenByProxy;
		if ( StaticMeshComponent )
		{
			if ( bHiddenByProxy )	// Turn off anything that shouldn't be done when the mesh is hidden by proxy
			{
				OldCastShadow = StaticMeshComponent->CastShadow;
				OldAcceptsLights = StaticMeshComponent->bAcceptsLights;
				OldCollisionType = CollisionType;

				StaticMeshComponent->CastShadow = FALSE;
				StaticMeshComponent->bAcceptsLights = FALSE;
				SetCollisionType( COLLIDE_NoCollision );
			}
			else	// Restore the settings once the mesh is unmerged
			{
				StaticMeshComponent->CastShadow = OldCastShadow;
				StaticMeshComponent->bAcceptsLights = OldAcceptsLights;
				OldCastShadow = FALSE;
				OldAcceptsLights = FALSE;

				SetCollisionType( OldCollisionType );
				OldCollisionType = COLLIDE_NoCollision;
			}
		}
	}
}

/**
	* Get whether this is a hidden because we have a proxy
	*/
UBOOL AStaticMeshActor::IsHiddenByProxy() const
{
	return bHiddenByProxy;
}

#endif // WITH_EDITOR

/**
 * Attempts to load this mesh's high res source static mesh
 *
 * @return The high res source mesh, or NULL if it failed to load
 */
UStaticMesh* UStaticMesh::LoadHighResSourceMesh() const
{
	UStaticMesh* HighResSourceStaticMesh = NULL;

	const UBOOL bIsAlreadySimplified = ( HighResSourceMeshName.Len() > 0 );
	if( bIsAlreadySimplified )
	{
		// Mesh in editor is already simplified.  We'll need to track down the original source mesh.

		// HighResSourceMesh name is a *path name*, so it has the package and the name in a single string
		INT DotPos = HighResSourceMeshName.InStr( TEXT( "." ) );
		check( DotPos > 0 );

		// Grab everything before the dot; this is the package name
		FString PackageName = HighResSourceMeshName.Left( DotPos );

		// Grab everything after the dot; this is the object name
		FString ObjectName = HighResSourceMeshName.Right( HighResSourceMeshName.Len() - ( DotPos + 1 ) );

		check( PackageName.Len() > 0 );
		check( ObjectName.Len() > 0 );


		// OK, now load the source mesh from the package
		// NOTE: Unless the package is already loaded, this will fail!
		HighResSourceStaticMesh =
			LoadObject<UStaticMesh>( NULL, *HighResSourceMeshName, NULL, LOAD_None, NULL );
		if( HighResSourceStaticMesh == NULL )
		{
			// @todo: We probably shouldn't need to have two separate loading paths here

			// Load the package the source mesh is in
			// NOTE: Until the package is SAVED, this will fail!
			UPackage* SourceMeshPackage = UObject::LoadPackage( NULL, *PackageName, 0 );
			if( SourceMeshPackage != NULL )
			{
				// OK, now load the source mesh from the package
				HighResSourceStaticMesh = LoadObject<UStaticMesh>( SourceMeshPackage, *ObjectName, NULL, LOAD_None, NULL );
			}
		}
	}

	return HighResSourceStaticMesh;
}


/**
 * Computes a CRC for this mesh to be used with mesh simplification tests.
 *
 * @return Simplified CRC for this mesh
 */
DWORD UStaticMesh::ComputeSimplifiedCRCForMesh() const
{
	TArray< BYTE > MeshBytes;

	// @todo: Ideally we'd check more aspects of the mesh, like material bindings

	if( LODModels.Num() > 0 )
	{
		const FStaticMeshRenderData& LOD = LODModels( 0 );

		// Position vertex data
		{
			const INT PosVertexBufferDataSize =
				LOD.PositionVertexBuffer.GetStride() * LOD.PositionVertexBuffer.GetNumVertices();
			const void* PosVertexBufferDataPtr =
				&LOD.PositionVertexBuffer.VertexPosition( 0 );

			const INT CurMeshBytes = MeshBytes.Num();
			MeshBytes.Add( PosVertexBufferDataSize );
			void* DestPtr = &MeshBytes( CurMeshBytes );
			
			appMemcpy( DestPtr, PosVertexBufferDataPtr, PosVertexBufferDataSize );
		}

		// Color vertex data
		{
			if( LOD.ColorVertexBuffer.GetNumVertices() > 0 )
			{
				const INT ColorVertexBufferDataSize =
					LOD.ColorVertexBuffer.GetStride() * LOD.ColorVertexBuffer.GetNumVertices();
				const void* ColorVertexBufferDataPtr =
					&LOD.ColorVertexBuffer.VertexColor( 0 );

				const INT CurMeshBytes = MeshBytes.Num();
				MeshBytes.Add( ColorVertexBufferDataSize );
				void* DestPtr = &MeshBytes( CurMeshBytes );
				
				appMemcpy( DestPtr, ColorVertexBufferDataPtr, ColorVertexBufferDataSize );
			}
		}

		// Other vertex data
		{
			const INT VertexBufferDataSize =
				LOD.VertexBuffer.GetStride() * LOD.VertexBuffer.GetNumVertices();
			const void* VertexBufferDataPtr =
				LOD.VertexBuffer.GetRawVertexData();

			const INT CurMeshBytes = MeshBytes.Num();
			MeshBytes.Add( VertexBufferDataSize );
			void* DestPtr = &MeshBytes( CurMeshBytes );
			
			appMemcpy( DestPtr, VertexBufferDataPtr, VertexBufferDataSize );
		}

		// Indices
		{
			const INT IndexBufferDataSize =
				LOD.IndexBuffer.Indices.GetResourceDataSize();
			const void* IndexBufferDataPtr =
				LOD.IndexBuffer.Indices.GetResourceData();

			const INT CurMeshBytes = MeshBytes.Num();
			MeshBytes.Add( IndexBufferDataSize );
			void* DestPtr = &MeshBytes( CurMeshBytes );
			
			appMemcpy( DestPtr, IndexBufferDataPtr, IndexBufferDataSize );
		}
	}


	// Compute CRC
	DWORD CRCValue = 0;
	if( MeshBytes.Num() > 0 )
	{
		CRCValue = appMemCrc( MeshBytes.GetData(), MeshBytes.Num(), 0 );
	}

	return CRCValue;
}


UBOOL UStaticMesh::RemoveZeroTriangleElements(UStaticMesh* InStaticMesh, UBOOL bUserPrompt)
{
	UBOOL bModifiedMesh = FALSE;

	if (InStaticMesh != NULL)
	{
		for (INT LODIdx = InStaticMesh->LODModels.Num() - 1; LODIdx >= 0; LODIdx--)
		{
			TArray<INT> ElementIdxToRemove;
			UBOOL bHasRemovals = FALSE;
			// See if there are empty elements
			FStaticMeshRenderData& LODModel = InStaticMesh->LODModels(LODIdx);
			FStaticMeshLODInfo& LODInfo = InStaticMesh->LODInfo(LODIdx);

			if (LODModel.Elements.Num() < LODInfo.Elements.Num())
			{
				LODInfo.Elements.Remove(LODModel.Elements.Num() - 1, LODInfo.Elements.Num() - LODModel.Elements.Num());
			}
			if (LODModel.Elements.Num() > LODInfo.Elements.Num())
			{
				LODInfo.Elements.AddZeroed(LODModel.Elements.Num() - LODInfo.Elements.Num());
			}

			check(LODModel.Elements.Num() == LODInfo.Elements.Num());

			for (INT ElementIdx = LODModel.Elements.Num() - 1; ElementIdx >= 0; ElementIdx--)
			{
				FStaticMeshElement& SMElement = LODModel.Elements(ElementIdx);
				FStaticMeshLODElement& SMLODElement = LODInfo.Elements(ElementIdx);

				if (SMElement.NumTriangles == 0)
				{
					// These will be in last-to-first order which is how we want to
					// remove them... When we use this array later, we will traverse
					// it from 0..NUM since the entries will already be sorted.
					ElementIdxToRemove.AddItem(ElementIdx);
					bHasRemovals = TRUE;
				}
				// NULL material should *not* remove
			}

			if (bHasRemovals)
			{
				UBOOL bPerformRemovals = TRUE;
				if (bUserPrompt == TRUE)
				{
					FString OutputMessage;

					OutputMessage = LocalizeUnrealEd("RemovingElementsPrompt");
					OutputMessage += TEXT("\n");
					for (INT RemoveIdx = 0; RemoveIdx < ElementIdxToRemove.Num(); RemoveIdx++)
					{
						INT ElementIdx = ElementIdxToRemove(RemoveIdx);

						OutputMessage += TEXT("  ");
						OutputMessage += FString::Printf(LocalizeSecure(LocalizeUnrealEd("RemovingElementsFormatString_NoTriangles"), ElementIdx));
						OutputMessage += TEXT("\n");
					}
					OutputMessage += LocalizeUnrealEd("RemovingElementsPrompt_Confirm");

					bPerformRemovals = appMsgf(AMT_YesNo, *OutputMessage);
				}

				if (bPerformRemovals == TRUE)
				{
					// Detach all instances of the static mesh while the mesh is being modified
					// This is slow, but faster than FGlobalComponentReattachContext because it only operates on static mesh components
					FStaticMeshComponentReattachContext	ComponentReattachContext(InStaticMesh);

					// PreEditChange of UStaticMesh releases all rendering resources and blocks until the rendering thread has processed the commands, which must be done before modifying
					InStaticMesh->PreEditChange(NULL);

					FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*)(LODModel.RawTriangles.Lock(LOCK_READ_WRITE));
					// Remove all the empty ones
					// To prevent issues w/ multiple removals, we have to do this one at a time
					for (INT RemoveIdx = 0; RemoveIdx < ElementIdxToRemove.Num(); RemoveIdx++)
					{
						INT RemoveElementIdx = ElementIdxToRemove(RemoveIdx);
						INT PrevElementCount = LODModel.Elements.Num();

						debugf(TEXT("Removing element %2d (out of %2d)"), RemoveElementIdx, PrevElementCount);

						LODModel.Elements.Remove(RemoveElementIdx);
						LODInfo.Elements.Remove(RemoveElementIdx);

						TMap<INT,INT> RemappedMaterialIndices;
						// Reset the material indices on the elements...
						for (INT ResetIdx = RemoveElementIdx; ResetIdx < LODModel.Elements.Num(); ResetIdx++)
						{
							// Just shift it down by one
							INT NewMtrlIndex = LODModel.Elements(ResetIdx).MaterialIndex - 1;
							check(NewMtrlIndex >= 0);
							check(NewMtrlIndex == ResetIdx);
							RemappedMaterialIndices.Set(LODModel.Elements(ResetIdx).MaterialIndex, NewMtrlIndex);
							LODModel.Elements(ResetIdx).MaterialIndex = NewMtrlIndex;
						}

						// Reset the raw triangle data w/ the remapped indices...
						if (RawTriangleData != NULL)
						{
							for (INT TriIdx = 0; TriIdx < LODModel.RawTriangles.GetElementCount(); TriIdx++)
							{
								FStaticMeshTriangle* Triangle = &RawTriangleData[TriIdx];
								INT* RemappedIdx = RemappedMaterialIndices.Find(Triangle->MaterialIndex);
								if (RemappedIdx != NULL)
								{
									Triangle->MaterialIndex = *RemappedIdx;
								}
								else
								{
									// Every index should be in the Remapped map...
									// Non-remapped ones will just point to themselves.
								}
							}
						}

						bModifiedMesh = TRUE;
					}
					LODModel.RawTriangles.Unlock();

					// Call PEC on the static mesh so it knows it has changed, and so it reinitializes rendering resources
					// This is necessary when changing any aspect of the mesh that affects how it is exported to Lightmass, since the exported static mesh is cached
					InStaticMesh->PostEditChange();
				}
			}
		}
	}

	return bModifiedMesh;
}

/**
 * Returns vertex color data by position.
 * For matching to reimported meshes that may have changed or copying vertex paint data from mesh to mesh.
 *
 *	@param	VertexColorData		(out)A map of vertex position data and its color. The method fills this map.
 */
void UStaticMesh::GetVertexColorData(TMap<FVector, FColor>& VertexColorData)
{
	VertexColorData.Empty();

	// What LOD to get vertex colors from.  
	// Currently mesh painting only allows for painting on the first lod.
	const UINT PaintingMeshLODIndex = 0;
	const FStaticMeshRenderData& RenderData = LODModels(PaintingMeshLODIndex);

	// Nothing to copy if there are no colors stored.
	if (RenderData.ColorVertexBuffer.GetNumVertices() > 0)
	{
		// Build a mapping of vertex positions to vertex colors.  Using a TMap will allow for fast lookups so we can match new static mesh vertices with existing colors 
		const FPositionVertexBuffer& ExistingPositionVertexBuffer = RenderData.PositionVertexBuffer;
		for (UINT PosIdx = 0; PosIdx < ExistingPositionVertexBuffer.GetNumVertices(); ++PosIdx)
		{
			const FVector& VertexPos = ExistingPositionVertexBuffer.VertexPosition(PosIdx);

			// Make sure this vertex doesn't already have an assigned color.  
			if (NULL == VertexColorData.Find(VertexPos))
			{
				VertexColorData.Set(VertexPos, RenderData.ColorVertexBuffer.VertexColor(PosIdx));
			}
		}
	}	
}

/**
 * Sets vertex color data by position.
 * Map of vertex color data by position is matched to the vertex position in the mesh
 * and nearest matching vertex color is used.
 *
 *	@param	VertexColorData		A map of vertex position data and color.
 */
void UStaticMesh::SetVertexColorData(const TMap<FVector, FColor>& VertexColorData)
{
	// What LOD to get vertex colors from.  
	// Currently mesh painting only allows for painting on the first lod.
	const UINT PaintingMeshLODIndex = 0;

	INT TriCount = LODModels(PaintingMeshLODIndex).RawTriangles.GetElementCount();
	FStaticMeshTriangle* RawTriangleData =
		(FStaticMeshTriangle*)(LODModels(PaintingMeshLODIndex).RawTriangles.Lock(LOCK_READ_WRITE));

	for (INT TriIdx = 0; TriIdx < TriCount; TriIdx++)
	{
		FStaticMeshTriangle& Triangle = RawTriangleData[TriIdx];

		for (INT VertIdx = 0; VertIdx < 3; VertIdx++)
		{
			// try to match this triangle's current vertex with one in VertexColorData.
			// This is a find in a TMap which uses a fast hash table lookup.
			const FColor* PaintedColor = VertexColorData.Find(Triangle.Vertices[VertIdx]);
			if (PaintedColor)
			{
				// A matching color for this vertex was found
				Triangle.Colors[VertIdx] = *PaintedColor;
			}
			else
			{
				// A matching color for this vertex could not be found. Make this vertex white
				Triangle.Colors[VertIdx] = FColor(255,255,255,255);
			}
		}
	}

	LODModels(PaintingMeshLODIndex).RawTriangles.Unlock();
}

/**
 * Static: Processes the specified static mesh for light map UV problems
 *
 * @param	InStaticMesh					Static mesh to process
 * @param	InOutAssetsWithMissingUVSets	Array of assets that we found with missing UV sets
 * @param	InOutAssetsWithBadUVSets		Array of assets that we found with bad UV sets
 * @param	InOutAssetsWithValidUVSets		Array of assets that we found with valid UV sets
 * @param	bInVerbose						If TRUE, log the items as they are found
 */
void UStaticMesh::CheckLightMapUVs( UStaticMesh* InStaticMesh, TArray< FString >& InOutAssetsWithMissingUVSets, TArray< FString >& InOutAssetsWithBadUVSets, TArray< FString >& InOutAssetsWithValidUVSets, UBOOL bInVerbose )
{

	struct
	{
		/**
		 * Checks to see if a point overlaps a triangle
		 *
		 * @param	P	Point
		 * @param	A	First triangle vertex
		 * @param	B	Second triangle vertex
		 * @param	C	Third triangle vertex
		 *
		 * @return	TRUE if the point overlaps the triangle
		 */
		UBOOL PointInTriangle( const FVector& P, const FVector& A, const FVector& B, const FVector& C, const FLOAT Epsilon )
		{
			struct
			{
				UBOOL SameSide( const FVector& P1, const FVector& P2, const FVector& A, const FVector& B, const FLOAT Epsilon )
				{
					const FVector Cross1( ( B - A ) ^ ( P1 - A ) );
					const FVector Cross2( ( B - A ) ^ ( P2 - A ) );
					return ( Cross1 | Cross2 ) >= -Epsilon;
				}
			} Local;

			return ( Local.SameSide( P, A, B, C, Epsilon ) &&
					 Local.SameSide( P, B, A, C, Epsilon ) &&
					 Local.SameSide( P, C, A, B, Epsilon ) );
		}


		/**
		 * Tests to see if the specified 2D triangles overlap
		 *
		 * @param	TriAVert0	First vertex of first triangle
		 * @param	TriAVert1	Second vertex of first triangle
		 * @param	TriAVert2	Third vertex of first triangle
		 * @param	TriBVert0	First vertex of second triangle
		 * @param	TriBVert1	Second vertex of second triangle
		 * @param	TriBVert2	Third vertex of second triangle
		 *
		 * @return	TRUE if the triangles overlap
		 */
		UBOOL Are2DTrianglesOverlapping( const FVector2D& TriAVert0, const FVector2D& TriAVert1, const FVector2D& TriAVert2, const FVector2D& TriBVert0, const FVector2D& TriBVert1, const FVector2D& TriBVert2, const FLOAT Epsilon )
		{
			struct
			{
				inline FLOAT Vec2DOrient( const FVector2D& Vec0, const FVector2D& Vec1, const FVector2D& Vec2 )
				{
					return ( Vec0[ 0 ] - Vec2[ 0 ] ) * ( Vec1[ 1 ] - Vec2[ 1 ] ) -
						   ( Vec0[ 1 ] - Vec2[ 1 ] ) * ( Vec1[ 0 ] - Vec2[ 0 ] );
				}


				inline UBOOL CheckVertexIntersection( const FVector2D& TriAVert0, const FVector2D& TriAVert1, const FVector2D& TriAVert2, const FVector2D& TriBVert0, const FVector2D& TriBVert1, const FVector2D& TriBVert2, const FLOAT Epsilon )
				{
					if( Vec2DOrient( TriBVert2, TriBVert0, TriAVert1 ) >= -Epsilon )
					{
						if( Vec2DOrient( TriBVert2, TriBVert1, TriAVert1 ) <= Epsilon )
						{
							if( Vec2DOrient( TriAVert0, TriBVert0, TriAVert1 ) > -Epsilon ) 
							{
								return ( Vec2DOrient( TriAVert0, TriBVert1, TriAVert1 ) <= Epsilon );
							}

							if( Vec2DOrient( TriAVert0, TriBVert0, TriAVert2 ) >= -Epsilon )
							{
								return ( Vec2DOrient( TriAVert1, TriAVert2, TriBVert0 ) >= -Epsilon );
							}
						}
						else 
						{
							if( Vec2DOrient( TriAVert0, TriBVert1, TriAVert1 ) <= Epsilon )
							{
								if( Vec2DOrient( TriBVert2, TriBVert1, TriAVert2 ) <= Epsilon )
								{
									return ( Vec2DOrient( TriAVert1, TriAVert2, TriBVert1 ) >= -Epsilon );
								}
							}
						}
					}
					else
					{
						if( Vec2DOrient( TriBVert2, TriBVert0, TriAVert2 ) >= -Epsilon ) 
						{
							if( Vec2DOrient( TriAVert1, TriAVert2, TriBVert2 ) >= -Epsilon )
							{
								return ( Vec2DOrient( TriAVert0, TriBVert0, TriAVert2 ) >= -Epsilon );
							}

							if( Vec2DOrient( TriAVert1, TriAVert2, TriBVert1 ) >= -Epsilon ) 
							{
								return ( Vec2DOrient( TriBVert2, TriAVert2, TriBVert1 ) >= -Epsilon );
							}
						}
					}

					return FALSE;
				};



				inline UBOOL CheckEdgeIntersection( const FVector2D& TriAVert0, const FVector2D& TriAVert1, const FVector2D& TriAVert2, const FVector2D& TriBVert0, const FVector2D& TriBVert1, const FVector2D& TriBVert2, const FLOAT Epsilon )
				{ 
					if( Vec2DOrient( TriBVert2, TriBVert0, TriAVert1 ) >= -Epsilon ) 
					{
						if( Vec2DOrient( TriAVert0, TriBVert0, TriAVert1 ) >= -Epsilon ) 
						{ 
							return ( Vec2DOrient( TriAVert0, TriAVert1, TriBVert2 ) >= -Epsilon );
						} 

						if( Vec2DOrient( TriAVert1, TriAVert2, TriBVert0 ) >= -Epsilon )
						{ 
							return ( Vec2DOrient( TriAVert2, TriAVert0, TriBVert0 ) >= -Epsilon );
						} 
					}
					else 
					{
						if( Vec2DOrient( TriBVert2, TriBVert0, TriAVert2 ) >= -Epsilon ) 
						{
							if( Vec2DOrient( TriAVert0, TriBVert0, TriAVert2 ) >= -Epsilon ) 
							{
								if( Vec2DOrient( TriAVert0, TriAVert2, TriBVert2 ) >= -Epsilon )
								{
									return TRUE;
								}

								return ( Vec2DOrient( TriAVert1, TriAVert2, TriBVert2 ) >= -Epsilon );
							}
						}
					}

					return FALSE;
				}



				inline UBOOL CheckTriangleOverlapCCW( const FVector2D& TriAVert0, const FVector2D& TriAVert1, const FVector2D& TriAVert2, const FVector2D& TriBVert0, const FVector2D& TriBVert1, const FVector2D& TriBVert2, const FLOAT Epsilon ) 
				{
					if( Vec2DOrient( TriBVert0, TriBVert1, TriAVert0 ) >= -Epsilon ) 
					{
						if( Vec2DOrient( TriBVert1, TriBVert2, TriAVert0 ) >= -Epsilon ) 
						{
							if( Vec2DOrient( TriBVert2, TriBVert0, TriAVert0 ) >= -Epsilon )
							{
								return TRUE;
							}

							return CheckEdgeIntersection( TriAVert0, TriAVert1, TriAVert2, TriBVert0, TriBVert1, TriBVert2, Epsilon );
						} 

						if( Vec2DOrient( TriBVert2, TriBVert0, TriAVert0 ) >= -Epsilon ) 
						{
							return CheckEdgeIntersection( TriAVert0, TriAVert1, TriAVert2, TriBVert2, TriBVert0, TriBVert1, Epsilon );
						}

						return CheckVertexIntersection( TriAVert0, TriAVert1, TriAVert2, TriBVert0, TriBVert1, TriBVert2, Epsilon );
					}

					if( Vec2DOrient( TriBVert1, TriBVert2, TriAVert0 ) >= -Epsilon ) 
					{
						if( Vec2DOrient( TriBVert2, TriBVert0, TriAVert0 ) >= -Epsilon ) 
						{
							return CheckEdgeIntersection( TriAVert0, TriAVert1, TriAVert2, TriBVert1, TriBVert2, TriBVert0, Epsilon );
						}

						return CheckVertexIntersection( TriAVert0, TriAVert1, TriAVert2, TriBVert1, TriBVert2, TriBVert0, Epsilon );
					}

					return CheckVertexIntersection( TriAVert0, TriAVert1, TriAVert2, TriBVert2, TriBVert0, TriBVert1, Epsilon );
				};

			} Local;


			if( Local.Vec2DOrient( TriAVert0, TriAVert1, TriAVert2 ) < Epsilon )
			{
				if( Local.Vec2DOrient( TriBVert0, TriBVert1, TriBVert2 ) < Epsilon )
				{
					return Local.CheckTriangleOverlapCCW( TriAVert0, TriAVert2, TriAVert1, TriBVert0, TriBVert2, TriBVert1, Epsilon );
				}

				return Local.CheckTriangleOverlapCCW( TriAVert0, TriAVert2, TriAVert1, TriBVert0, TriBVert1, TriBVert2, Epsilon );
			}

			if( Local.Vec2DOrient( TriBVert0, TriBVert1, TriBVert2 ) < Epsilon )
			{
				return Local.CheckTriangleOverlapCCW( TriAVert0, TriAVert1, TriAVert2, TriBVert0, TriBVert2, TriBVert1, Epsilon );
			}

			return Local.CheckTriangleOverlapCCW( TriAVert0, TriAVert1, TriAVert2, TriBVert0, TriBVert1, TriBVert2, Epsilon );
		};

	} Local;


	check( InStaticMesh != NULL );

	TArray< INT > TriangleOverlapCounts;

	// For each LOD model in the mesh
	for( INT CurLODModelIndex = 0; CurLODModelIndex < InStaticMesh->LODModels.Num(); ++CurLODModelIndex )
	{
		FStaticMeshRenderData& LODRenderData = InStaticMesh->LODModels( CurLODModelIndex );


		// Does this LOD have any triangles?
		const INT TriangleCount = LODRenderData.RawTriangles.GetElementCount();
		if( TriangleCount > 0 )
		{
			FStaticMeshTriangle* LODTriangleData = ( FStaticMeshTriangle* )LODRenderData.RawTriangles.Lock( LOCK_READ_ONLY );

			TriangleOverlapCounts.Reset();
			TriangleOverlapCounts.AddZeroed( TriangleCount );


			INT OverlappingLightMapUVTriangleCount = 0;
			INT OutOfBoundsTriangleCount = 0;
			bool bHasLightMapUVSet = FALSE;



			const FStaticMeshTriangle& FirstTriangle = LODTriangleData[ 0 ];
			const INT LODUVSetCount = Min( FirstTriangle.NumUVs, ( INT )LODRenderData.VertexBuffer.GetNumTexCoords() );

			// We expect the light map texture coordinate to be greater than zero, as the first UV set
			// should never really be used for light maps, unless this mesh was exported as a lightmapped uv set.
			INT LightMapTextureCoordinateIndex = InStaticMesh->LightMapCoordinateIndex;
			if( LightMapTextureCoordinateIndex <= 0 && LODUVSetCount > 1 )
			{
				LightMapTextureCoordinateIndex = 1;
			}

			if( LODUVSetCount > LightMapTextureCoordinateIndex ) 
			{
				bHasLightMapUVSet = TRUE;

				// For each mesh triangle
				for( INT CurTriangleIndex = 0; CurTriangleIndex < TriangleCount; ++CurTriangleIndex )
				{
					const FStaticMeshTriangle& CurTriangle = LODTriangleData[ CurTriangleIndex ];



					// Test for UVs outside of the 0.0 to 1.0 range (wrapped/clamped)
					UBOOL bAnyUVsOutOfBounds = FALSE;
					for( INT CurTriVertIndex = 0; CurTriVertIndex < 3; ++CurTriVertIndex )
					{
						const FVector2D& CurVertUV = CurTriangle.UVs[ CurTriVertIndex ][ LightMapTextureCoordinateIndex ];
						const FLOAT TestEpsilon = 0.001f;
						for( INT CurDimIndex = 0; CurDimIndex < 2; ++CurDimIndex )
						{
							if( CurVertUV[ CurDimIndex ] < ( 0.0f - TestEpsilon ) ||
								CurVertUV[ CurDimIndex ] > ( 1.0f + TestEpsilon ) )
							{
								bAnyUVsOutOfBounds = TRUE;
								break;
							}
						}
					}

					if( bAnyUVsOutOfBounds )
					{
						++OutOfBoundsTriangleCount;
					}

					
					// We use the centroid vs. triangle test instead of full overlap test to reduce false-positives
					if( 1 )
					{
						// For every *other* mesh triangle
						for( INT OtherTriangleIndex = 0; OtherTriangleIndex < TriangleCount; ++OtherTriangleIndex )
						{
							if( CurTriangleIndex != OtherTriangleIndex )
							{
								const FStaticMeshTriangle& OtherTriangle = LODTriangleData[ OtherTriangleIndex ];

								FVector2D CurTriangleUVCentroid = 
										( CurTriangle.UVs[ 0 ][ LightMapTextureCoordinateIndex ] +
										  CurTriangle.UVs[ 1 ][ LightMapTextureCoordinateIndex ] +
										  CurTriangle.UVs[ 2 ][ LightMapTextureCoordinateIndex ] ) / 3.0f;

								// Bias toward non-overlapping so sliver triangles won't overlap their adjoined neighbors
								const FLOAT TestEpsilon = -0.001f;

								// Test for overlap
								if( Local.PointInTriangle(
										FVector( CurTriangleUVCentroid, 0.0f ),
										FVector( OtherTriangle.UVs[ 0 ][ LightMapTextureCoordinateIndex ], 0.0f ),
										FVector( OtherTriangle.UVs[ 1 ][ LightMapTextureCoordinateIndex ], 0.0f ),
										FVector( OtherTriangle.UVs[ 2 ][ LightMapTextureCoordinateIndex ], 0.0f ),
										TestEpsilon ) )
								{
									if( TriangleOverlapCounts( CurTriangleIndex ) == 0 )
									{
										++OverlappingLightMapUVTriangleCount;
									}
									++TriangleOverlapCounts( CurTriangleIndex );

									if( TriangleOverlapCounts( OtherTriangleIndex ) == 0 )
									{
										++OverlappingLightMapUVTriangleCount;
									}
									++TriangleOverlapCounts( OtherTriangleIndex );
								}
							}
						}
					}
					else
					{
						// For every *other* mesh triangle.  We start at our current triangle index because the
						// previous triangles have already been tested in a prior loop iteration
						for( INT OtherTriangleIndex = CurTriangleIndex + 1; OtherTriangleIndex < TriangleCount; ++OtherTriangleIndex )
						{
							check( CurTriangleIndex != OtherTriangleIndex );
							const FStaticMeshTriangle& OtherTriangle = LODTriangleData[ OtherTriangleIndex ];

							// Bias toward non-overlapping so triangles with shared edges are not considered overlapping
							const FLOAT TestEpsilon = -0.01f;

							// Test for overlap
							if( Local.Are2DTrianglesOverlapping(
									CurTriangle.UVs[ 0 ][ LightMapTextureCoordinateIndex ],
									CurTriangle.UVs[ 1 ][ LightMapTextureCoordinateIndex ],
									CurTriangle.UVs[ 2 ][ LightMapTextureCoordinateIndex ],
									OtherTriangle.UVs[ 0 ][ LightMapTextureCoordinateIndex ],
									OtherTriangle.UVs[ 1 ][ LightMapTextureCoordinateIndex ],
									OtherTriangle.UVs[ 2 ][ LightMapTextureCoordinateIndex ],
									TestEpsilon ) )
							{
								if( TriangleOverlapCounts( CurTriangleIndex ) == 0 )
								{
									++OverlappingLightMapUVTriangleCount;
								}
								++TriangleOverlapCounts( CurTriangleIndex );

								if( TriangleOverlapCounts( OtherTriangleIndex ) == 0 )
								{
									++OverlappingLightMapUVTriangleCount;
								}
								++TriangleOverlapCounts( OtherTriangleIndex );
							}
						}
					}
				}
			}
			else
			{
				// LOD doesn't contain a light map texture coordinate set.  Skip on by.
			}



			LODRenderData.RawTriangles.Unlock();


			// Report warnings
			{
				if( !bHasLightMapUVSet )
				{
					if (bInVerbose == TRUE)
					{
						warnf( NAME_Warning, TEXT( "[%s, LOD %i] missing light map UVs (Res %i, CoordIndex %i)" ), *InStaticMesh->GetName(), CurLODModelIndex, InStaticMesh->LightMapResolution, InStaticMesh->LightMapCoordinateIndex );
					}
					InOutAssetsWithMissingUVSets.AddItem( InStaticMesh->GetFullName() );
				}
				else if( OverlappingLightMapUVTriangleCount > 0 || OutOfBoundsTriangleCount > 0 )
				{
					if (bInVerbose == TRUE)
					{
						if( OverlappingLightMapUVTriangleCount > 0 )
						{
							warnf( NAME_Warning, TEXT( "[%s, LOD %i] %i triangles with overlapping UVs (of %i) (UV set %i)" ), *InStaticMesh->GetName(), CurLODModelIndex, OverlappingLightMapUVTriangleCount, TriangleCount, LightMapTextureCoordinateIndex );
						}

						if( OutOfBoundsTriangleCount > 0 )
						{
							warnf( NAME_Warning, TEXT( "[%s, LOD %i] %i triangles with out-of-bound UVs (of %i) (UV set %i)" ), *InStaticMesh->GetName(), CurLODModelIndex, OutOfBoundsTriangleCount, TriangleCount, LightMapTextureCoordinateIndex );
						}
					}
					InOutAssetsWithBadUVSets.AddItem( InStaticMesh->GetFullName() );
				}
				else
				{
					// Good to go!
					InOutAssetsWithValidUVSets.AddItem( InStaticMesh->GetFullName() );
					if (bInVerbose == TRUE)
					{
						warnf( NAME_Log, TEXT( "[%s, LOD %i] light map UVs OK" ), *InStaticMesh->GetName(), CurLODModelIndex );
					}
				}
			}
		}
		else
		{
			// LOD doesn't contain any triangles.  Skip on by.
			if (bInVerbose == TRUE)
			{
				warnf( NAME_Warning, TEXT( "[%s, LOD %i] doesn't have any triangles" ), *InStaticMesh->GetName(), CurLODModelIndex );
			}
		}
	}
}

/**
 * Ensures the original mesh data is saved and returns a pointer to it. This method should
 * always be called before making changes to the mesh's triangle data.
 *
 * @return The source mesh data.
 */
FStaticMeshRenderData& UStaticMesh::PreModifyMesh()
{
	if ( !SourceData.IsInitialized() && LODModels.Num() )
	{
		SourceData.Init( LODModels(0) );
	}
	check( SourceData.IsInitialized() );
	return *SourceData.GetRenderData();
}

/**
 * Returns the source data imported for LOD0 of this mesh.
 */
FStaticMeshRenderData& UStaticMesh::GetSourceData()
{
	check( LODModels.Num() );
	if ( SourceData.IsInitialized() )
	{
		return *SourceData.GetRenderData();
	}
	return LODModels(0);
}

/**
 * Returns the render data to use for exporting the specified LOD. This method should always
 * be called when exporting a static mesh.
 */
FStaticMeshRenderData* UStaticMesh::GetLODForExport( INT LODIndex )
{
	check( LODModels.Num() );
	LODIndex = Clamp<INT>( LODIndex, 0, LODModels.Num()-1 );
#if UDK
	if (bIsMeshProxy)
	{
		return NULL;
	}
	else if (bHasBeenSimplified)
	{
		check( SourceData.IsInitialized() );
		return SourceData.GetRenderData();
	}
#endif // #if UDK
	return &(LODModels(LODIndex));
}

/*-----------------------------------------------------------------------------
	UStaticMeshComponent
-----------------------------------------------------------------------------*/

/**
 * Determines whether any of the component's LODs require override vertex color fixups
 *
 * @param	OutLODIndices	Indices of the LODs requiring fixup, if any
 *
 * @return	TRUE if any LODs require override vertex color fixups
 */
UBOOL UStaticMeshComponent::RequiresOverrideVertexColorsFixup( TArray<INT>& OutLODIndices )
{
	OutLODIndices.Empty();
	UBOOL bFixupRequired = FALSE;

#if WITH_EDITORONLY_DATA
	if ( GIsEditor && !GIsCooking && StaticMesh && StaticMesh->VertexPositionVersionNumber != VertexPositionVersionNumber )
	{
		// Iterate over each LOD to confirm which ones, if any, actually need to have their colors updated
		for ( TArray<FStaticMeshComponentLODInfo>::TConstIterator LODIter( LODData ); LODIter; ++LODIter )
		{
			const FStaticMeshComponentLODInfo& CurCompLODInfo = *LODIter;

			// Confirm that the LOD has override colors and cached vertex data. If it doesn't have both, it can't be fixed up.
			if ( CurCompLODInfo.OverrideVertexColors && 
				CurCompLODInfo.OverrideVertexColors->GetNumVertices() > 0 && 
				CurCompLODInfo.PaintedVertices.Num() > 0 && 
				StaticMesh->LODModels.IsValidIndex( LODIter.GetIndex() ) )
			{
				OutLODIndices.AddItem( LODIter.GetIndex() );
				bFixupRequired = TRUE;
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	return bFixupRequired;
}

/** Save off the vertex positions of the source mesh per LOD if necessary */
void UStaticMeshComponent::CachePaintedDataIfNecessary()
{
#if WITH_EDITORONLY_DATA
	// Only cache the vertex positions if we're in the editor and not cooking (because cooking wants this data stripped)
	if ( GIsEditor && !GIsCooking && StaticMesh )
	{
		// Iterate over each component LOD info checking for the existence of override colors
		for ( TArray<FStaticMeshComponentLODInfo>::TIterator LODIter( LODData ); LODIter; ++LODIter )
		{
			FStaticMeshComponentLODInfo& CurCompLODInfo = *LODIter;

			// Workaround for a copy-paste bug. If the number of painted vertices is <= 1 we know the data is garbage.
			if ( CurCompLODInfo.PaintedVertices.Num() <= 1 )
			{
				CurCompLODInfo.PaintedVertices.Empty();
			}

			// If the mesh has override colors but no cached vertex positions, then the current vertex positions should be cached to help preserve instanced vertex colors during mesh tweaks
			// NOTE: We purposefully do *not* cache the positions if cached positions already exist, as this would result in the loss of the ability to fixup the component if the source mesh
			// were changed multiple times before a fix-up operation was attempted
			if ( CurCompLODInfo.OverrideVertexColors && 
				 CurCompLODInfo.OverrideVertexColors->GetNumVertices() > 0 &&
				 CurCompLODInfo.PaintedVertices.Num() == 0 &&
				 StaticMesh->LODModels.IsValidIndex( LODIter.GetIndex() ) ) 
			{
				FStaticMeshRenderData* CurRenderData = &(StaticMesh->LODModels( LODIter.GetIndex() ));
				if ( LODIter.GetIndex() == 0 && CurRenderData->NumVertices != CurCompLODInfo.OverrideVertexColors->GetNumVertices() )
				{
					// If LOD0 does not match the number of vertices in the override array, try the source data.
					CurRenderData = &(StaticMesh->GetSourceData());
				}
				if ( CurRenderData->NumVertices == CurCompLODInfo.OverrideVertexColors->GetNumVertices() )
				{
					// Cache the data.
					CurCompLODInfo.PaintedVertices.Reserve( CurRenderData->NumVertices );
					for ( UINT VertIndex = 0; VertIndex < CurRenderData->NumVertices; ++VertIndex )
					{
						FPaintedVertex* Vertex = new( CurCompLODInfo.PaintedVertices ) FPaintedVertex;
						Vertex->Position = CurRenderData->PositionVertexBuffer.VertexPosition( VertIndex );
						Vertex->Normal = CurRenderData->VertexBuffer.VertexTangentZ( VertIndex );
						Vertex->Color = CurCompLODInfo.OverrideVertexColors->VertexColor( VertIndex );
					}
				}
				else
				{
					warnf( TEXT("Unable to cache painted data for mesh component. Vertex color overrides will be lost if the mesh is modified. %s %s LOD%d."), *GetFullName(), *StaticMesh->GetFullName(), LODIter.GetIndex() );
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/** Helper struct for the mesh component vert position octree */
struct FStaticMeshComponentVertPosOctreeSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	/**
	 * Get the bounding box of the provided octree element. In this case, the box
	 * is merely the point specified by the element.
	 *
	 * @param	Element	Octree element to get the bounding box for
	 *
	 * @return	Bounding box of the provided octree element
	 */
	FORCEINLINE static FBoxCenterAndExtent GetBoundingBox( const FPaintedVertex& Element )
	{
		return FBoxCenterAndExtent( Element.Position, FVector( 0.0f, 0.0f, 0.0f ) );
	}

	/**
	 * Determine if two octree elements are equal
	 *
	 * @param	A	First octree element to check
	 * @param	B	Second octree element to check
	 *
	 * @return	TRUE if both octree elements are equal, FALSE if they are not
	 */
	FORCEINLINE static UBOOL AreElementsEqual( const FPaintedVertex& A, const FPaintedVertex& B )
	{
		return ( A.Position == B.Position && A.Normal == B.Normal && A.Color == B.Color );
	}

	/** Ignored for this implementation */
	FORCEINLINE static void SetElementId( const FPaintedVertex& Element, FOctreeElementId Id )
	{
	}
};

typedef TOctree<FPaintedVertex, FStaticMeshComponentVertPosOctreeSemantics> TSMCVertPosOctree;

/** Update the vertex override colors if necessary (i.e. vertices from source mesh have changed from override colors) */
UBOOL UStaticMeshComponent::FixupOverrideColorsIfNecessary()
{
#if WITH_EDITORONLY_DATA
	// Detect if there is a version mismatch between the source mesh and the component. If so, the component's LODs potentially
	// need to have their override colors updated to match changes in the source mesh.
	TArray<INT> LODsToUpdate;
	if ( RequiresOverrideVertexColorsFixup( LODsToUpdate ) )
	{
		// Detach this component because rendering changes are about to be applied
		FComponentReattachContext ReattachContext( this );
		for ( TArray<INT>::TConstIterator LODIdxIter( LODsToUpdate ); LODIdxIter; ++LODIdxIter )
		{
			FStaticMeshComponentLODInfo& CurCompLODInfo = LODData( *LODIdxIter );
			FStaticMeshRenderData& CurRenderData = StaticMesh->LODModels( *LODIdxIter );

			// Find the extents formed by the cached vertex positions in order to optimize the octree used later
			FVector MinExtents( CurCompLODInfo.PaintedVertices( 0 ).Position );
			FVector MaxExtents( CurCompLODInfo.PaintedVertices( 0 ).Position );

			for ( INT VertIndex = 0; VertIndex < CurCompLODInfo.PaintedVertices.Num(); ++VertIndex )
			{
				const FVector& CurVector = CurCompLODInfo.PaintedVertices( VertIndex ).Position;
				
				MinExtents.X = Min<FLOAT>( MinExtents.X, CurVector.X );
				MinExtents.Y = Min<FLOAT>( MinExtents.Y, CurVector.Y );
				MinExtents.Z = Min<FLOAT>( MinExtents.Z, CurVector.Z );
				
				MaxExtents.X = Max<FLOAT>( MaxExtents.X, CurVector.X );
				MaxExtents.Y = Max<FLOAT>( MaxExtents.Y, CurVector.Y );
				MaxExtents.Z = Max<FLOAT>( MaxExtents.Z, CurVector.Z );
			}

			// Create an octree which spans the extreme extents of the old and new vertex positions in order to quickly query for the colors
			// of the new vertex positions
			FBox OldBounds( MinExtents, MaxExtents );
			OldBounds += StaticMesh->Bounds.GetBox();
			TSMCVertPosOctree VertPosOctree( OldBounds.GetCenter(), OldBounds.GetExtent().GetMax() );

			// Add each old vertex to the octree
			for ( INT PaintedVertexIndex = 0; PaintedVertexIndex < CurCompLODInfo.PaintedVertices.Num(); ++PaintedVertexIndex )
			{
				VertPosOctree.AddElement( CurCompLODInfo.PaintedVertices( PaintedVertexIndex ) );
			}

			// If the override color buffer isn't already the required size to house the number of vertices on the source
			// mesh, it needs to be destroyed and re-allocated
			if ( CurCompLODInfo.OverrideVertexColors->GetNumVertices() != CurRenderData.NumVertices )
			{
				CurCompLODInfo.BeginReleaseOverrideVertexColors();
				FlushRenderingCommands();
				CurCompLODInfo.OverrideVertexColors->InitFromSingleColor( FColor( 255, 255, 255 ), CurRenderData.NumVertices );
			}
			else
			{
				CurCompLODInfo.BeginReleaseOverrideVertexColors();
				FlushRenderingCommands();
			}
	
			// Iterate over each new vertex position, attempting to find the old vertex it is closest to, applying
			// the color of the old vertex to the new position if possible.
			for ( UINT NewVertIndex = 0; NewVertIndex < CurRenderData.PositionVertexBuffer.GetNumVertices(); ++NewVertIndex )
			{
				TArray<FPaintedVertex> PointsToConsider;
				TSMCVertPosOctree::TConstIterator<> OctreeIter( VertPosOctree );
				const FVector& CurPosition = CurRenderData.PositionVertexBuffer.VertexPosition( NewVertIndex );
				const FVector CurNormal = CurRenderData.VertexBuffer.VertexTangentZ( NewVertIndex );

				// Iterate through the octree attempting to find the vertices closest to the current new point
				while ( OctreeIter.HasPendingNodes() )
				{
					const TSMCVertPosOctree::FNode& CurNode = OctreeIter.GetCurrentNode();
					const FOctreeNodeContext& CurContext = OctreeIter.GetCurrentContext();

					// Find the child of the current node, if any, that contains the current new point
					FOctreeChildNodeRef ChildRef = CurContext.GetContainingChild( FBoxCenterAndExtent( CurPosition, FVector::ZeroVector ) );
					
					if ( !ChildRef.IsNULL() )
					{
						const TSMCVertPosOctree::FNode* ChildNode = CurNode.GetChild( ChildRef );

						// If the specified child node exists and contains any of the old vertices, push it to the iterator for future consideration
						if ( ChildNode && ChildNode->GetInclusiveElementCount() > 0 )
						{
							OctreeIter.PushChild( ChildRef );
						}
						// If the child node doesn't have any of the old vertices in it, it's not worth pursuing any further. In an attempt to find
						// anything to match vs. the new point, add all of the children of the current octree node that have old points in them to the
						// iterator for future consideration.
						else
						{
							FOREACH_OCTREE_CHILD_NODE( ChildRef )
							{
								if( CurNode.HasChild( ChildRef ) && CurNode.GetChild( ChildRef )->GetInclusiveElementCount() > 0 )
								{
									OctreeIter.PushChild( ChildRef );
								}
							}
						}
					}
					
					// Add all of the elements in the current node to the list of points to consider for closest point calculations
					PointsToConsider.Append( CurNode.GetElements() );
					OctreeIter.Advance();
				}

				// If any points to consider were found, iterate over each and find which one is the closest to the new point 
				if ( PointsToConsider.Num() > 0 )
				{
					const FLOAT DistanceOverNormalThreshold = KINDA_SMALL_NUMBER;
					FPaintedVertex BestVertex = PointsToConsider(0);
					FLOAT BestDistanceSquared = ( BestVertex.Position - CurPosition ).SizeSquared();
					FLOAT BestNormalDot = FVector( BestVertex.Normal ) | CurNormal;

					for ( INT ConsiderationIndex = 1; ConsiderationIndex < PointsToConsider.Num(); ++ConsiderationIndex )
					{
						FPaintedVertex& Vertex = PointsToConsider( ConsiderationIndex );
						const FLOAT DistSqrd = ( Vertex.Position - CurPosition ).SizeSquared();
						const FLOAT NormalDot = FVector( Vertex.Normal ) | CurNormal;
						if ( DistSqrd < BestDistanceSquared - DistanceOverNormalThreshold )
						{
							BestVertex = Vertex;
							BestDistanceSquared = DistSqrd;
							BestNormalDot = NormalDot;
						}
						else if ( DistSqrd < BestDistanceSquared + DistanceOverNormalThreshold && NormalDot > BestNormalDot )
						{
							BestVertex = Vertex;
							BestDistanceSquared = DistSqrd;
							BestNormalDot = NormalDot;
						}
					}

					CurCompLODInfo.OverrideVertexColors->VertexColor( NewVertIndex ) = BestVertex.Color;
				}
			}

			// Initialize the vert. colors
			BeginInitResource( CurCompLODInfo.OverrideVertexColors );
		}
		VertexPositionVersionNumber = StaticMesh->VertexPositionVersionNumber;
	}
	return LODsToUpdate.Num() > 0 ? TRUE : FALSE;
#else
	return TRUE;
#endif // WITH_EDITORONLY_DATA
}

void UStaticMeshComponent::InitResources()
{
	for(INT LODIndex = 0; LODIndex < LODData.Num(); LODIndex++)
	{
		FStaticMeshComponentLODInfo &LODInfo = LODData(LODIndex);
		if(LODInfo.OverrideVertexColors)
		{
			BeginInitResource(LODInfo.OverrideVertexColors);
			INC_DWORD_STAT_BY( STAT_InstVertexColorMemory, LODInfo.OverrideVertexColors->GetAllocatedSize() );
		}

		// Create the light-map resources.
		if(LODData(LODIndex).LightMap != NULL)
		{
			LODData(LODIndex).LightMap->InitResources();
		}
	}
}



void UStaticMeshComponent::ReleaseResources()
{
	for(INT LODIndex = 0;LODIndex < LODData.Num();LODIndex++)
	{
		LODData(LODIndex).BeginReleaseOverrideVertexColors();
	}

	DetachFence.BeginFence();
}

void UStaticMeshComponent::BeginDestroy()
{
	Super::BeginDestroy();
	ReleaseResources();
}

void UStaticMeshComponent::ExportCustomProperties(FOutputDevice& Out, UINT Indent)
{
	for(INT LODIdx=0; LODIdx < LODData.Num(); LODIdx++)
	{
		Out.Logf( TEXT("%sCustomProperties "), appSpc(Indent) );

		FStaticMeshComponentLODInfo& LODInfo = LODData(LODIdx);
		
		if(LODInfo.OverrideVertexColors)
		{
			Out.Logf( TEXT("CustomLODData LOD=%d "), LODIdx );

			FString	Value;
			LODInfo.OverrideVertexColors->ExportText(Value);

			Out.Log(Value);
		}
		Out.Logf( TEXT("\r\n"));
	}
}

void UStaticMeshComponent::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	check(SourceText);
	check(Warn);

	if(ParseCommand(&SourceText,TEXT("CustomLODData")))
	{
		QWORD LODIndex;
		if (Parse(SourceText, TEXT("LOD="), LODIndex))
		{
			while(*SourceText && (!appIsWhitespace(*SourceText)))
			{
				++SourceText;
			}

			check(*SourceText);

			if(LODIndex >= LODData.Num() || *SourceText == 0)
			{
				Warn->Logf( *LocalizeError( TEXT( "CustomProperties Syntax Error" ), TEXT( "Core" ) ) );
				return;
			}

			FStaticMeshComponentLODInfo& LODInfo = LODData(LODIndex);

			check(!LODInfo.OverrideVertexColors);

			while(appIsWhitespace(*SourceText))
			{
				++SourceText;
			}

			LODInfo.OverrideVertexColors = new FColorVertexBuffer;
			LODInfo.OverrideVertexColors->ImportText(SourceText);
		}
		else check(0);
	}
}

void UStaticMeshComponent::PostEditUndo()
{
	// The component's light-maps are loaded from the transaction, so their resources need to be reinitialized.
	InitResources();

	Super::PostEditUndo();
}

/**
 * Called after all objects referenced by this object have been serialized. Order of PostLoad routed to 
 * multiple objects loaded in one set is not deterministic though ConditionalPostLoad can be forced to
 * ensure an object has been "PostLoad"ed.
 */
void UStaticMeshComponent::PostLoad()
{
	Super::PostLoad();

#if MOBILE
	// on mobile platforms, the detail level doesn't change at runtime
	// so if the component will be culled by detail level, clear content references to restore some memory
	if (DetailMode > GSystemSettings.DetailMode)
	{
		StaticMesh = NULL;
		Materials.Empty();
	}
#endif

	if ( GIsEditor && !GIsCooking && StaticMesh )
	{
		CachePaintedDataIfNecessary();
		if ( FixupOverrideColorsIfNecessary() )
		{
			GWarn->MapCheck_Add( MCTYPE_INFO, GetOuter(), *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_RepairedPaintedVertexColors" ), *GetName() ) ), TEXT( "RepairedPaintedVertexColors" ) );
		}
	}

	// make sure the component doesn't have more LOD levels than the mesh itself, as it can 
	// confuse cooking for PS3 
	if (StaticMesh && LODData.Num() > StaticMesh->LODModels.Num())
	{
		LODData.Remove(StaticMesh->LODModels.Num(), LODData.Num() - StaticMesh->LODModels.Num());
	}

	// Initialize the resources for the freshly loaded component.
	InitResources();
}

/**
 * Used by various commandlets to purge editor only and platform-specific data from various objects
 * 
 * @param PlatformsToKeep Platforms for which to keep platform-specific data
 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
 */
void UStaticMeshComponent::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData); 

#if WITH_EDITORONLY_DATA
	// toss if the only platform to keep is WindowsServer
	if (!(PlatformsToKeep & ~UE3::PLATFORM_WindowsServer))
	{
		for( INT LODIdx=0; LODIdx < LODData.Num(); LODIdx++ )
		{
			FStaticMeshComponentLODInfo& LODInfo = LODData(LODIdx);
			LODInfo.LightMap = NULL;
			LODInfo.ShadowMaps.Empty();
			LODInfo.ShadowVertexBuffers.Empty();
			LODInfo.ReleaseOverrideVertexColorsAndBlock();
		}
	}

	// On any platform we're cooking/stripping data for, strip out all of the vertex color
	// positions, as they're only used for mesh fix-ups that shouldn't be done in game
	if ( (PlatformsToKeep & UE3::PLATFORM_Stripped ) != 0 || GIsCookingForDemo )
	{
		for ( INT LodIdx = 0; LodIdx < LODData.Num(); ++LodIdx )
		{
			FStaticMeshComponentLODInfo& LODInfo = LODData( LodIdx );
			LODInfo.PaintedVertices.Empty();

			if ( LODInfo.OverrideVertexColors &&
				 ( StaticMesh == NULL ||
				   LodIdx >= StaticMesh->LODModels.Num() ||
				   LODInfo.OverrideVertexColors->GetNumVertices() != StaticMesh->LODModels( LodIdx ).NumVertices ) )
			{
				LODInfo.ReleaseOverrideVertexColorsAndBlock();
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/** Change the StaticMesh used by this instance. */
UBOOL UStaticMeshComponent::SetStaticMesh(UStaticMesh* NewMesh, UBOOL bForce)
{
	// Do nothing if we are already using the supplied static mesh
	if(NewMesh == StaticMesh && !bForce)
	{
		return FALSE;
	}

	// Don't allow changing static meshes if the owner is "static".
	if( !Owner || !Owner->IsStatic() )
	{
		// Terminate rigid-body data for this StaticMeshComponent
		TermComponentRBPhys(NULL);

		// Force the recreate context to be destroyed by going out of scope after setting the new static-mesh.
		{
			FComponentReattachContext ReattachContext(this);
			StaticMesh = NewMesh;
		}

		// Re-init the rigid body info.
		UBOOL bFixed = TRUE;
		if(!Owner || Owner->Physics == PHYS_RigidBody)
		{
			bFixed = FALSE;
		}

		if(IsAttached())
		{
			InitComponentRBPhys(bFixed);
		}

		// Notify the streaming system. Don't use Update(), because this may be the first time the mesh has been set
		// and the component may have to be added to the streaming system for the first time.
		GStreamingManager->NotifyPrimitiveAttached( this, DPT_Spawned );
		return TRUE;
	}
	return FALSE;
}

/** Script version of SetStaticMesh. */
void UStaticMeshComponent::execSetStaticMesh(FFrame& Stack, RESULT_DECL)
{
	P_GET_OBJECT(UStaticMesh, NewMesh);
	P_GET_UBOOL_OPTX(bForceSet,FALSE);
	P_FINISH;

	*(UBOOL*)Result = SetStaticMesh(NewMesh,bForceSet);
}

/**
* Changes the value of bForceStaticDecals.
* @param bInForceStaticDecals - The value to assign to bForceStaticDecals.
*/
void UStaticMeshComponent::SetForceStaticDecals(UBOOL bInForceStaticDecals)
{
	if( bForceStaticDecals != bInForceStaticDecals )
	{
		bForceStaticDecals = bInForceStaticDecals;
		FComponentReattachContext ReattachContext(this);
	}
}

/** Script version of SetForceStaticDecals. */
void UStaticMeshComponent::execSetForceStaticDecals(FFrame& Stack, RESULT_DECL)
{
	P_GET_UBOOL(bInForceStaticDecals);
	P_FINISH;

	SetForceStaticDecals(bInForceStaticDecals);
}

UBOOL UStaticMeshComponent::CanBecomeDynamic()
{
	return StaticMesh && StaticMesh->bCanBecomeDynamic && !bNeverBecomeDynamic && BlockRigidBody && !bDisableAllRigidBody;
}

/** Script version of SetForceStaticDecals. */
void UStaticMeshComponent::execCanBecomeDynamic(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	*(UBOOL*)Result = CanBecomeDynamic();
}

/**
 * Returns whether this primitive only uses unlit materials.
 *
 * @return TRUE if only unlit materials are used for rendering, false otherwise.
 */
UBOOL UStaticMeshComponent::UsesOnlyUnlitMaterials() const
{
	if( StaticMesh )
	{
		// Figure out whether any of the sections has a lit material assigned.
		UBOOL bUsesOnlyUnlitMaterials = TRUE;
		for( INT ElementIndex=0; ElementIndex<StaticMesh->LODModels(0).Elements.Num(); ElementIndex++ )
		{
			UMaterialInterface*	MaterialInterface	= GetMaterial(ElementIndex);
			UMaterial*			Material			= MaterialInterface ? MaterialInterface->GetMaterial() : NULL;

			if( !Material || Material->LightingModel != MLM_Unlit )
			{
				bUsesOnlyUnlitMaterials = FALSE;
				break;
			}
		}
		return bUsesOnlyUnlitMaterials;
	}
	else
	{
		return FALSE;
	}
}

/**
 * Returns the lightmap resolution used for this primivite instnace in the case of it supporting texture light/ shadow maps.
 * 0 if not supported or no static shadowing.
 *
 * @param	Width	[out]	Width of light/shadow map
 * @param	Height	[out]	Height of light/shadow map
 *
 * @return	UBOOL			TRUE if LightMap values are padded, FALSE if not
 */
UBOOL UStaticMeshComponent::GetLightMapResolution( INT& Width, INT& Height ) const
{
	UBOOL bPadded = FALSE;
	if( StaticMesh )
	{
		// Use overriden per component lightmap resolution.
		if( bOverrideLightMapRes )
		{
			Width	= OverriddenLightMapRes;
			Height	= OverriddenLightMapRes;
		}
		// Use the lightmap resolution defined in the static mesh.
		else
		{
			Width	= StaticMesh->LightMapResolution;
			Height	= StaticMesh->LightMapResolution;
		}
		bPadded = TRUE;
	}
	// No associated static mesh!
	else
	{
		Width	= 0;
		Height	= 0;
	}

	return bPadded;
}

/**
 *	Returns the lightmap resolution used for this primitive instance in the case of it supporting texture light/ shadow maps.
 *	This will return the value assuming the primitive will be automatically switched to use texture mapping.
 *
 *	@param Width	[out]	Width of light/shadow map
 *	@param Height	[out]	Height of light/shadow map
 */
void UStaticMeshComponent::GetEstimatedLightMapResolution(INT& Width, INT& Height) const
{
	if (StaticMesh)
	{
		ELightMapInteractionType LMIType = GetStaticLightingType();

		UBOOL bUseSourceMesh = FALSE;

		// It's currently set to vertex mapping
		if (LMIType == LMIT_Vertex)
		{
			if (bOverrideLightMapRes == TRUE)
			{
				// In this case, it is assumed that OverridenLightMapRes is 0...
				// (Unless the lightmap UVs are not valid)
				bUseSourceMesh = TRUE;
			}
			else
			{
				if (OverriddenLightMapRes != 0)
				{
					Width	= OverriddenLightMapRes;
					Height	= OverriddenLightMapRes;
				}
				else
				{
					bUseSourceMesh = TRUE;
				}
			}
		}
		else
		{
			// Use overriden per component lightmap resolution.
			// If the overridden LM res is > 0, then this is what would be used...
			if (bOverrideLightMapRes == TRUE)
			{
				if (OverriddenLightMapRes != 0)
				{
					Width	= OverriddenLightMapRes;
					Height	= OverriddenLightMapRes;
				}
			}
			else
			{
				bUseSourceMesh = TRUE;
			}
		}

		// Use the lightmap resolution defined in the static mesh.
		if (bUseSourceMesh == TRUE)
		{
			Width	= StaticMesh->LightMapResolution;
			Height	= StaticMesh->LightMapResolution;
		}

		// If it was not set by anything, give it a default value...
		if (Width == 0)
		{
			INT TempInt = 0;
			verify(GConfig->GetInt(TEXT("DevOptions.StaticLighting"), TEXT("DefaultStaticMeshLightingRes"), TempInt, GLightmassIni));

			Width	= TempInt;
			Height	= TempInt;
		}
	}
	else
	{
		Width	= 0;
		Height	= 0;
	}
}

/**
 *	Returns the static lightmap resolution used for this primitive.
 *	0 if not supported or no static shadowing.
 *
 * @return	INT		The StaticLightmapResolution for the component
 */
INT UStaticMeshComponent::GetStaticLightMapResolution() const
{
	INT Width, Height;
	GetLightMapResolution(Width, Height);
	return Max<INT>(Width, Height);
}

/**
 *	Returns TRUE if the component uses texture lightmaps
 *	
 *	@param	InWidth		[in]	The width of the light/shadow map
 *	@param	InHeight	[in]	The width of the light/shadow map
 *
 *	@return	UBOOL				TRUE if texture lightmaps are used, FALSE if not
 */
UBOOL UStaticMeshComponent::UsesTextureLightmaps(INT InWidth, INT InHeight) const
{
	return (
		(StaticMesh != NULL) &&
		(InWidth > 0) && 
		(InHeight > 0) &&
		(StaticMesh->LightMapCoordinateIndex >= 0) &&	
		((UINT)StaticMesh->LightMapCoordinateIndex < StaticMesh->LODModels(0).VertexBuffer.GetNumTexCoords())
		);
}

/**
 *	Returns TRUE if the static mesh the component uses has valid lightmap texture coordinates
 */
UBOOL UStaticMeshComponent::HasLightmapTextureCoordinates() const
{
	if ((StaticMesh != NULL) &&
		(StaticMesh->LightMapCoordinateIndex >= 0) &&
		((UINT)StaticMesh->LightMapCoordinateIndex < StaticMesh->LODModels(0).VertexBuffer.GetNumTexCoords()))
	{
		return TRUE;
	}
	return FALSE;
}

/**
 *	Get the memory used for texture-based light and shadow maps of the given width and height
 *
 *	@param	InWidth						The desired width of the light/shadow map
 *	@param	InHeight					The desired height of the light/shadow map
 *	@param	OutLightMapMemoryUsage		The resulting lightmap memory used
 *	@param	OutShadowMapMemoryUsage		The resulting shadowmap memory used
 */
void UStaticMeshComponent::GetTextureLightAndShadowMapMemoryUsage(INT InWidth, INT InHeight, INT& OutLightMapMemoryUsage, INT& OutShadowMapMemoryUsage) const
{
	// Stored in texture.
	const FLOAT MIP_FACTOR = 1.33f;
	OutShadowMapMemoryUsage = appTrunc(MIP_FACTOR * InWidth * InHeight); // G8
	const UINT NumLightMapCoefficients = GSystemSettings.bAllowDirectionalLightMaps ? NUM_DIRECTIONAL_LIGHTMAP_COEF : NUM_SIMPLE_LIGHTMAP_COEF;
	OutLightMapMemoryUsage = appTrunc(NumLightMapCoefficients * MIP_FACTOR * InWidth * InHeight / 2); // DXT1
}

/**
 *	Get the memory used for vertex-based light and shadow maps
 *
 *	@param	OutLightMapMemoryUsage		The resulting lightmap memory used
 *	@param	OutShadowMapMemoryUsage		The resulting shadowmap memory used
 */
void UStaticMeshComponent::GetVertexLightAndShadowMapMemoryUsage(INT& OutLightMapMemoryUsage, INT& OutShadowMapMemoryUsage) const
{
	// Stored in vertex buffer.
	if (StaticMesh != NULL)
	{
		OutShadowMapMemoryUsage = sizeof(FLOAT) * StaticMesh->LODModels(0).NumVertices;
		const UINT LightMapSampleSize = GSystemSettings.bAllowDirectionalLightMaps ? sizeof(FQuantizedDirectionalLightSample) : sizeof(FQuantizedSimpleLightSample);
		OutLightMapMemoryUsage = LightMapSampleSize * StaticMesh->LODModels(0).NumVertices;
	}
	else
	{
		OutShadowMapMemoryUsage = 0;
		OutLightMapMemoryUsage = 0;
	}
}

/**
 * Returns the light and shadow map memory for this primite in its out variables.
 *
 * Shadow map memory usage is per light whereof lightmap data is independent of number of lights, assuming at least one.
 *
 * @param [out] LightMapMemoryUsage		Memory usage in bytes for light map (either texel or vertex) data
 * @param [out]	ShadowMapMemoryUsage	Memory usage in bytes for shadow map (either texel or vertex) data
 */
void UStaticMeshComponent::GetLightAndShadowMapMemoryUsage( INT& LightMapMemoryUsage, INT& ShadowMapMemoryUsage ) const
{
	// Zero initialize.
	ShadowMapMemoryUsage = 0;
	LightMapMemoryUsage = 0;

	// Cache light/ shadow map resolution.
	INT LightMapWidth = 0;
	INT	LightMapHeight = 0;
	GetLightMapResolution(LightMapWidth, LightMapHeight);

	// Determine whether static mesh/ static mesh component has static shadowing.
	if (HasStaticShadowing() && StaticMesh)
	{
		// Determine whether we are using a texture or vertex buffer to store precomputed data.
		if (UsesTextureLightmaps(LightMapWidth, LightMapHeight) == TRUE)
		{
			GetTextureLightAndShadowMapMemoryUsage(LightMapWidth, LightMapHeight, LightMapMemoryUsage, ShadowMapMemoryUsage);
		}
		else
		{
			GetVertexLightAndShadowMapMemoryUsage(LightMapMemoryUsage, ShadowMapMemoryUsage);
		}
	}
}

/**
 * Returns the light and shadow map memory for this primite in its out variables.
 *
 * Shadow map memory usage is per light whereof lightmap data is independent of number of lights, assuming at least one.
 *
 *	@param [out]	TextureLightMapMemoryUsage		Estimated memory usage in bytes for light map texel data
 *	@param [out]	TextureShadowMapMemoryUsage		Estimated memory usage in bytes for shadow map texel data
 *	@param [out]	VertexLightMapMemoryUsage		Estimated memory usage in bytes for light map vertex data
 *	@param [out]	VertexShadowMapMemoryUsage		Estimated memory usage in bytes for shadow map vertex data
 *	@param [out]	StaticLightingResolution		The StaticLightingResolution used for Texture estimates
 *	@param [out]	bIsUsingTextureMapping			Set to TRUE if the mesh is using texture mapping currently; FALSE if vertex
 *	@param [out]	bHasLightmapTexCoords			Set to TRUE if the mesh has the proper UV channels
 *
 *	@return			UBOOL							TRUE if the mesh has static lighting; FALSE if not
 */
UBOOL UStaticMeshComponent::GetEstimatedLightAndShadowMapMemoryUsage( 
	INT& TextureLightMapMemoryUsage, INT& TextureShadowMapMemoryUsage,
	INT& VertexLightMapMemoryUsage, INT& VertexShadowMapMemoryUsage,
	INT& StaticLightingResolution, UBOOL& bIsUsingTextureMapping, UBOOL& bHasLightmapTexCoords) const
{
	TextureLightMapMemoryUsage = 0;
	TextureShadowMapMemoryUsage = 0;
	VertexLightMapMemoryUsage = 0;
	VertexShadowMapMemoryUsage = 0;
	bIsUsingTextureMapping = FALSE;
	bHasLightmapTexCoords = FALSE;

	// Cache light/ shadow map resolution.
	INT LightMapWidth		= 0;
	INT	LightMapHeight		= 0;
	GetEstimatedLightMapResolution(LightMapWidth, LightMapHeight);
	StaticLightingResolution = LightMapWidth;

	INT TrueLightMapWidth	= 0;
	INT	TrueLightMapHeight	= 0;
	GetLightMapResolution(TrueLightMapWidth, TrueLightMapHeight);

	// Determine whether static mesh/ static mesh component has static shadowing.
	if (HasStaticShadowing() && StaticMesh)
	{
		// Determine whether we are using a texture or vertex buffer to store precomputed data.
		bHasLightmapTexCoords = HasLightmapTextureCoordinates();
		// Determine whether we are using a texture or vertex buffer to store precomputed data.
		bIsUsingTextureMapping = UsesTextureLightmaps(TrueLightMapWidth, TrueLightMapHeight);
		// Stored in texture.
		GetTextureLightAndShadowMapMemoryUsage(LightMapWidth, LightMapHeight, TextureLightMapMemoryUsage, TextureShadowMapMemoryUsage);
		// Stored in vertex buffer.
		GetVertexLightAndShadowMapMemoryUsage(VertexLightMapMemoryUsage, VertexShadowMapMemoryUsage);

		return TRUE;
	}

	return FALSE;
}

INT UStaticMeshComponent::GetNumElements() const
{
	if(StaticMesh)
	{
		check(StaticMesh->LODModels.Num() >= 1);
		return StaticMesh->LODModels(0).Elements.Num();
	}
	else
	{
		return 0;
	}
}

/**
 *	UStaticMeshComponent::GetMaterial
 * @param MaterialIndex Index of material
 * @return Material instance for this component at index
 */
UMaterialInterface* UStaticMeshComponent::GetMaterial(INT MaterialIndex) const
{
	// Call GetMateiral using the base (zeroth) LOD.
	return GetMaterial( MaterialIndex, 0 );
}

/**
*	UStaticMeshComponent::GetMaterial
* @param MaterialIndex Index of material
* @param LOD Lod level to query from
* @return Material instance for this component at index
*/
UMaterialInterface* UStaticMeshComponent::GetMaterial(INT MaterialIndex, INT LOD) const
{
	// If we have a base materials array, use that
	if(MaterialIndex < Materials.Num() && Materials(MaterialIndex))
	{
		return Materials(MaterialIndex);
	}
	// Otherwise get from static mesh lod
	else
	{
		if(StaticMesh && LOD < StaticMesh->LODModels.Num())
		{
			// Find the first element that has the corresponding material index, and use its material.
			for(INT ElementIndex = 0;ElementIndex < StaticMesh->LODModels(LOD).Elements.Num();ElementIndex++)
			{
				const FStaticMeshElement& Element = StaticMesh->LODModels(LOD).Elements(ElementIndex);
				if(Element.MaterialIndex == MaterialIndex)
				{
					return Element.Material;
				}
			}
		}

		// If there wasn't a static mesh element with the given material index, return NULL.
		return NULL;
	}
}

void UStaticMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const
{
	if( StaticMesh )
	{
		for (INT LODIndex = 0; LODIndex < StaticMesh->LODModels.Num(); LODIndex++)
		{
			for (INT ElementIndex = 0; ElementIndex < StaticMesh->LODModels(LODIndex).Elements.Num(); ElementIndex++)
			{
				// Get the material for each element at the current lod index
				OutMaterials.AddItem(GetMaterial(ElementIndex, LODIndex));
			}
		}
	}
}

UStaticMeshComponent::UStaticMeshComponent()
{
}

void UStaticMeshComponent::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects(ObjectArray);
	for(INT LODIndex = 0;LODIndex < LODData.Num();LODIndex++)
	{
		if(LODData(LODIndex).LightMap != NULL)
		{
			LODData(LODIndex).LightMap->AddReferencedObjects(ObjectArray);
		}
	}
}


//
//	UStaticMeshComponent::Serialize
//
void UStaticMeshComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << LODData;

	if (Ar.Ver() < VER_INTEGRATED_LIGHTMASS)
	{
		bOverrideLightMapRes = bOverrideLightMapResolution_DEPRECATED;
		OverriddenLightMapRes = OverriddenLightMapResolution_DEPRECATED;
	}

	// Serialize out the vert. position version number
	if( Ar.Ver() < VER_DEPRECATE_DOUBLY_SERIALISED_SMC )
	{
		if ( Ar.Ver() >= VER_PRESERVE_SMC_VERT_COLORS  )
		{
			INT Dummy = -1;
			Ar << Dummy;
			check( GIsCooking || !GIsEditor || Dummy == VertexPositionVersionNumber );
		}
		else
		{
			VertexPositionVersionNumber = 0;
		}
	}
}


/**
 * Returns whether native properties are identical to the one of the passed in component.
 *
 * @param	Other	Other component to compare against
 *
 * @return TRUE if native properties are identical, FALSE otherwise
 */
UBOOL UStaticMeshComponent::AreNativePropertiesIdenticalTo( UComponent* Other ) const
{
	UBOOL bNativePropertiesAreIdentical = Super::AreNativePropertiesIdenticalTo( Other );
	UStaticMeshComponent* OtherSMC = CastChecked<UStaticMeshComponent>(Other);

	if( bNativePropertiesAreIdentical )
	{
		// Components are not identical if they have lighting information.
		if( LODData.Num() || OtherSMC->LODData.Num() )
		{
			bNativePropertiesAreIdentical = FALSE;
		}
	}
	
	return bNativePropertiesAreIdentical;
}

void UStaticMeshComponent::PreEditUndo()
{
	Super::PreEditUndo();

	// Undo can result in a resize of LODData which can calls ~FStaticMeshComponentLODInfo.
	// To safely delete FStaticMeshComponentLODInfo::OverrideVertexColors we
	// need to make sure the RT thread has no access to it any more.
	check(!IsAttached());
	ReleaseResources();
	DetachFence.Wait();
}

void UStaticMeshComponent::PreSave()
{
	Super::PreSave();
#if WITH_EDITORONLY_DATA
	CachePaintedDataIfNecessary();
#endif // WITH_EDITORONLY_DATA
}

/** 
 * Called when any property in this object is modified in UnrealEd
 *
 * @param	PropertyThatChanged		changed property
 */
void UStaticMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Ensure that OverriddenLightMapRes is either 0 or a power of two.
	if( OverriddenLightMapRes > 0 )
	{
		OverriddenLightMapRes = Max(OverriddenLightMapRes + 3 & ~3,4);
	}
	else
	{
		OverriddenLightMapRes = 0;
	}

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (((PropertyThatChanged->GetName().InStr(TEXT("OverriddenLightMapRes"), FALSE, TRUE) != INDEX_NONE) && (bOverrideLightMapRes == TRUE)) ||
			(PropertyThatChanged->GetName().InStr(TEXT("bOverrideLightMapRes"), FALSE, TRUE) != INDEX_NONE))
		{
			InvalidateLightingCache();
		}

		if ( PropertyThatChanged->GetName().InStr(TEXT("bIgnoreInstanceForTextureStreaming"), FALSE, TRUE) != INDEX_NONE ||
			 PropertyThatChanged->GetName().InStr(TEXT("StreamingDistanceMultiplier"), FALSE, TRUE) != INDEX_NONE )
		{
			ULevel::TriggerStreamingDataRebuild();
		}
	}

	// This only NULLs out direct references to materials.  We purposefully do not NULL
	// references to material instances that refer to decal materials, and instead just
	// warn about them during map checking.  The reason is that the side effects of NULLing
	// refs to instances pointing to decal materials would be strange (e.g. changing an
	// instance to refer to a decal material instead would then cause references to the
	// instance to break.
	for( INT MaterialIndex = 0 ; MaterialIndex < Materials.Num() ; ++MaterialIndex )
	{
		UMaterialInterface*&	MaterialInterface = Materials(MaterialIndex);
		if( MaterialInterface && MaterialInterface->IsA( UDecalMaterial::StaticClass() ) )
		{
			MaterialInterface = NULL;
		}
	}

	LightmassSettings.EmissiveBoost = Max(LightmassSettings.EmissiveBoost, 0.0f);
	LightmassSettings.DiffuseBoost = Max(LightmassSettings.DiffuseBoost, 0.0f);
	LightmassSettings.SpecularBoost = Max(LightmassSettings.SpecularBoost, 0.0f);

	// Ensure properties are in sane range.
	SubDivisionStepSize = Clamp( SubDivisionStepSize, 1, 128 );

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#if WITH_EDITOR
void UStaticMeshComponent::CheckForErrors()
{
	Super::CheckForErrors();

	// Get the mesh owner's name.
	FString OwnerName(GNone);
	if ( Owner )
	{
		OwnerName = Owner->GetName();
	}

	// Warn about direct or indirect references to decal materials.
	for( INT MaterialIndex = 0 ; MaterialIndex < Materials.Num() ; ++MaterialIndex )
	{
		UMaterialInterface* MaterialInterface = Materials(MaterialIndex);
		if ( MaterialInterface )
		{
			if( MaterialInterface->IsA( UDecalMaterial::StaticClass() ) )
			{
				GWarn->MapCheck_Add( MCTYPE_WARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_DecalMaterialStaticMesh" ), *GetName(), *OwnerName, *MaterialInterface->GetName() ) ), TEXT( "DecalMaterialStaticMesh" ) );
			}

			const UMaterial* ReferencedMaterial = MaterialInterface->GetMaterial();
			if( ReferencedMaterial && ReferencedMaterial->IsA( UDecalMaterial::StaticClass() ) )
			{
				GWarn->MapCheck_Add( MCTYPE_WARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_DecalMaterialInstanceStaticMesh" ), *GetName(), *OwnerName, *MaterialInterface->GetName(), *ReferencedMaterial->GetName() ) ), TEXT( "DecalMaterialInstanceStaticMesh" ) );
			}
		}
	}


	// Make sure any simplified meshes can still find their high res source mesh
	if( StaticMesh != NULL )
	{
		const UBOOL bIsAlreadySimplified = ( StaticMesh->HighResSourceMeshName.Len() > 0 );
		if( bIsAlreadySimplified )
		{
			// Mesh in editor is already simplified.  We'll need to track down the original source mesh.
			UStaticMesh* HighResSourceStaticMesh = StaticMesh->LoadHighResSourceMesh();

			if( HighResSourceStaticMesh != NULL )
			{
				// Compute CRC of high resolution mesh
				const DWORD HighResMeshCRC = HighResSourceStaticMesh->ComputeSimplifiedCRCForMesh();

				if( HighResMeshCRC != StaticMesh->HighResSourceMeshCRC )
				{
					// It looks like the high res mesh has changed since we were generated.  We'll need
					// to let the user know about this.
					GWarn->MapCheck_Add( MCTYPE_WARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_MeshSimplificationNeeded" ), *HighResSourceStaticMesh->GetName(), *StaticMesh->GetName() ) ), TEXT( "MeshSimplificationNeeded" ) );
				}
			}
			else
			{
				// Can't find the high res mesh?
				GWarn->MapCheck_Add( MCTYPE_WARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_CantLoadHiresMesh" ), *StaticMesh->HighResSourceMeshName, *StaticMesh->GetName() ) ), TEXT( "CantLoadHiresMesh" ) );
			}
		}

		// Check for element material index/material mismatches
		if (StaticMesh->LODModels.Num() > 0)
		{
			INT MaxMaterialIdx = -1;
			INT ElementCount = 0;
			INT ZeroTriangleElements = 0;
			// Only checking LOD 0 at the moment...
			FStaticMeshRenderData& LODData = StaticMesh->LODModels(0);
			ElementCount = LODData.Elements.Num();
			for (INT ElementIdx = 0; ElementIdx < ElementCount; ElementIdx++)
			{
				FStaticMeshElement& Element = LODData.Elements(ElementIdx);
				if (Element.MaterialIndex > MaxMaterialIdx)
				{
					MaxMaterialIdx = Element.MaterialIndex;
				}
				if (Element.NumTriangles == 0)
				{
					ZeroTriangleElements++;
				}
			}

			if (MaxMaterialIdx != -1)
			{
				if (Materials.Num() > ElementCount)
				{
					GWarn->MapCheck_Add( MCTYPE_WARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_MoreMaterialsThanElements" ), Materials.Num(), ElementCount, *StaticMesh->GetName() ) ), TEXT( "MoreMaterialsThanElements" ) );
				}
				else if (Materials.Num() > (MaxMaterialIdx + 1))
				{
					GWarn->MapCheck_Add( MCTYPE_WARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_MoreMaterialsThanReferenced" ), Materials.Num(), MaxMaterialIdx+1, *StaticMesh->GetName() ) ), TEXT( "MoreMaterialsThanReferenced" ) );
				}
			}
			if (ZeroTriangleElements > 0)
			{
				GWarn->MapCheck_Add( MCTYPE_WARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_ElementsWithZeroTriangles" ), ZeroTriangleElements, *StaticMesh->GetName() ) ), TEXT( "ElementsWithZeroTriangles" ) );
			}
		}
	}

	// Make sure any non uniform scaled meshes have appropriate collision
	if ( (ShouldCollide() || BlockRigidBody) && StaticMesh != NULL && StaticMesh->BodySetup != NULL && Owner != NULL )
	{
		// Overall scale factor for this mesh.
		const FVector& TotalScale3D = Scale * Owner->DrawScale * Scale3D * Owner->DrawScale3D;
		if ( !TotalScale3D.IsUniform() &&
			 (StaticMesh->BodySetup->AggGeom.BoxElems.Num() > 0   ||
			  StaticMesh->BodySetup->AggGeom.SphylElems.Num() > 0 ||
			  StaticMesh->BodySetup->AggGeom.SphereElems.Num() > 0) )

		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_SimpleCollisionButNonUniformScale" ), *StaticMesh->GetName() ) ), TEXT( "SimpleCollisionButNonUniformScale" ) );
		}
	}

	// Any simple lightmap modification texture should be marked as such in its compression settings
	if( SimpleLightmapModificationTexture != NULL )
	{
		if( SimpleLightmapModificationTexture->CompressionSettings != TC_SimpleLightmapModification )
		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_SimpleLightmapModTextureButIncorrectCompression" ), *StaticMesh->GetName() ) ), TEXT( "SimpleLightmapModTextureButIncorrectCompression" ) );
		}
	}

	FLightingChannelContainer NonDefaultChannels;
	NonDefaultChannels.SetAllChannels();
	NonDefaultChannels.BSP = FALSE;
	NonDefaultChannels.Static = FALSE;
	NonDefaultChannels.Dynamic = FALSE;
	NonDefaultChannels.CompositeDynamic = FALSE;

	if ( !HiddenGame  // usually if a SMC is hidden it will stay hidden (e.g. a collision SMC)
		&& bAcceptsLights 
		&& AlwaysLoadOnClient 
		&& AlwaysLoadOnServer
		&& !IsA(UFracturedSkinnedMeshComponent::StaticClass())
		&& !bUsePrecomputedShadows 
		&& (!LightEnvironment || !LightEnvironment->IsEnabled())
		// Don't warn for meshes with custom lighting channels (often using cinematic lighting)
		&& !LightingChannels.OverlapsWith(NonDefaultChannels))
	{
		GWarn->MapCheck_Add( MCTYPE_PERFORMANCEWARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_NotLightmappedOrUsingALightEnv" ), *GetDetailedInfo() ) ), TEXT( "NotLightmappedOrUsingALightEnv" ) );
	}
}
#endif

void UStaticMeshComponent::UpdateBounds()
{
	if(StaticMesh)
	{
		// Graphics bounds.
		FBoxSphereBounds OriginalBounds = Bounds;
		Bounds = StaticMesh->Bounds.TransformBy(LocalToWorld);
		
		// Add bounds of collision geometry (if present).
		if(StaticMesh->BodySetup)
		{
			FMatrix Transform;
			FVector Scale3D;
			GetTransformAndScale(Transform, Scale3D);

			FBox AggGeomBox = StaticMesh->BodySetup->AggGeom.CalcAABB(Transform, Scale3D);
			if (AggGeomBox.IsValid)
			{
				Bounds = LegacyUnion(Bounds,FBoxSphereBounds(AggGeomBox));
			}
		}

		// Takes into account that the static mesh collision code nudges collisions out by up to 1 unit.
		Bounds.BoxExtent += FVector(1,1,1);
		Bounds.SphereRadius += 1.0f;
		Bounds.BoxExtent *= BoundsScale;
		Bounds.SphereRadius *= BoundsScale;

#if !CONSOLE
		if ( !GIsGame && Scene->GetWorld() == GWorld &&
			(OriginalBounds.Origin.Equals(Bounds.Origin) == FALSE || OriginalBounds.BoxExtent.Equals(Bounds.BoxExtent) == FALSE) )
		{
			ULevel::TriggerStreamingDataRebuild();
		}
#endif
	}
	else
	{
		Super::UpdateBounds();
	}
}

UBOOL UStaticMeshComponent::IsValidComponent() const
{
	return StaticMesh != NULL && StaticMesh->LODModels.Num() && StaticMesh->LODModels(0).NumVertices > 0 && Super::IsValidComponent();
}

void UStaticMeshComponent::Attach()
{
	// Check that the static-mesh hasn't been changed to be incompatible with the cached light-map.
	for(INT i=0;i<LODData.Num();i++)
	{
		FStaticMeshComponentLODInfo& LODInfo = LODData(i);
		if (!bUsePrecomputedShadows)
		{
			LODInfo.LightMap = NULL;
			LODInfo.ShadowMaps.Empty();
			LODInfo.ShadowVertexBuffers.Empty();
		}
		else if (LODInfo.LightMap)
		{
			const FLightMap1D* LightMap1D = LODData(i).LightMap->GetLightMap1D();
			if(	StaticMesh->LODModels.Num() != LODData.Num() || 
				(LightMap1D && LightMap1D->NumSamples() != StaticMesh->LODModels(i).NumVertices))
			{
				// If the vertex light-map doesn't have the same number of elements as the static mesh has vertices, discard the light-map.
				LODData(i).LightMap = NULL;
			}
		}
	}

	// Change the tick group based upon blocking or not
	TickGroup = TickGroup < TG_PostAsyncWork ? (BlockActors ? TG_PreAsyncWork : TG_DuringAsyncWork) : TG_PostAsyncWork;

	Super::Attach();
}

void UStaticMeshComponent::GetStaticTriangles(FPrimitiveTriangleDefinitionInterface* PTDI) const
{
	if(StaticMesh && StaticMesh->LODModels.Num() > 0)
	{
		const INT LODIndex = 0;
		const FStaticMeshRenderData& LODRenderData = StaticMesh->LODModels(LODIndex);

		// Compute the primitive transform's inverse transpose, for transforming normals.
		const FMatrix LocalToWorldInverseTranspose = LocalToWorld.Inverse().Transpose();

		// Reverse triangle vertex windings if the primitive is negatively scaled.
		const UBOOL bReverseWinding = LocalToWorldDeterminant < 0.0f;

		// Iterate over the static mesh's triangles.
		const INT NumTriangles = LODRenderData.GetTriangleCount();
		for(INT TriangleIndex = 0;TriangleIndex < NumTriangles;TriangleIndex++)
		{
			// Lookup the triangle's vertices;
			FPrimitiveTriangleVertex Vertices[3];
			for(INT TriangleVertexIndex = 0;TriangleVertexIndex < 3;TriangleVertexIndex++)
			{
				const WORD VertexIndex = LODRenderData.IndexBuffer.Indices(TriangleIndex * 3 + TriangleVertexIndex);
				FPrimitiveTriangleVertex& Vertex = Vertices[bReverseWinding ? 2 - TriangleVertexIndex : TriangleVertexIndex];

				Vertex.WorldPosition = LocalToWorld.TransformFVector(LODRenderData.PositionVertexBuffer.VertexPosition(VertexIndex));
				Vertex.WorldTangentX = LocalToWorld.TransformNormal(LODRenderData.VertexBuffer.VertexTangentX(VertexIndex)).SafeNormal();
				Vertex.WorldTangentY = LocalToWorld.TransformNormal(LODRenderData.VertexBuffer.VertexTangentY(VertexIndex)).SafeNormal();
				Vertex.WorldTangentZ = LocalToWorldInverseTranspose.TransformNormal(LODRenderData.VertexBuffer.VertexTangentZ(VertexIndex)).SafeNormal();
			}

			// Pass the triangle to the caller's interface.
			PTDI->DefineTriangle(
				Vertices[0],
				Vertices[1],
				Vertices[2]
				);
		}
	}
}

void UStaticMeshComponent::GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
{
	if ( StaticMesh && !bIgnoreInstanceForTextureStreaming )
	{
		UBOOL bHasValidLightmapCoordinates = ((StaticMesh->LightMapCoordinateIndex >= 0) &&
			StaticMesh->LODModels.Num() > 0 &&
			((UINT)StaticMesh->LightMapCoordinateIndex < StaticMesh->LODModels(0).VertexBuffer.GetNumTexCoords()));

		// We need to come up with a compensation factor for spline deformed meshes
		FLOAT SplineDeformFactor = 1.f;
		const USplineMeshComponent* SplineComp = ConstCast<USplineMeshComponent>(this);
		if (SplineComp)
		{
			// We do this by looking at the ratio between current bounds (including deformation) and undeformed (straight from staticmesh)
			FBoxSphereBounds UndeformedBounds = StaticMesh->Bounds.TransformBy(LocalToWorld);
			FVector DeformFactor = Bounds.BoxExtent/UndeformedBounds.BoxExtent;
			SplineDeformFactor = DeformFactor.GetMax();
		}

		const FSphere BoundingSphere	= Bounds.GetSphere();
		const FLOAT LocalTexelFactor	= StaticMesh->GetStreamingTextureFactor(0) * StreamingDistanceMultiplier;
		const FLOAT LocalLightmapFactor	= bHasValidLightmapCoordinates ? StaticMesh->GetStreamingTextureFactor(StaticMesh->LightMapCoordinateIndex) : 1.0f;
		const FLOAT WorldTexelFactor	= SplineDeformFactor * LocalTexelFactor * LocalToWorld.GetMaximumAxisScale();
		const FLOAT WorldLightmapFactor	= SplineDeformFactor * LocalLightmapFactor * LocalToWorld.GetMaximumAxisScale();

		// Process each material applied to the top LOD of the mesh. TODO: Handle lower LODs?
		for( INT ElementIndex = 0; ElementIndex < StaticMesh->LODModels(0).Elements.Num(); ElementIndex++ )
		{
			const FStaticMeshElement& Element = StaticMesh->LODModels(0).Elements(ElementIndex);
			UMaterialInterface* Material = GetMaterial(Element.MaterialIndex);
			if(!Material)
			{
				Material = GEngine->DefaultMaterial;
			}

			// Enumerate the textures used by the material.
			TArray<UTexture*> Textures;

			Material->GetUsedTextures(Textures, MSQ_UNSPECIFIED, TRUE);

			// Add each texture to the output with the appropriate parameters.
			// TODO: Take into account which UVIndex is being used.
			for(INT TextureIndex = 0;TextureIndex < Textures.Num();TextureIndex++)
			{
				FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
				StreamingTexture.Bounds = BoundingSphere;
				StreamingTexture.TexelFactor = WorldTexelFactor;
				StreamingTexture.Texture = Textures(TextureIndex);
			}
		}

		// Add in the lightmaps and shadowmaps.
		if ( LODData.Num() > 0 && bHasValidLightmapCoordinates )
		{
			const FStaticMeshComponentLODInfo& LODInfo = LODData(0);
			FLightMap2D* Lightmap = LODInfo.LightMap ? LODInfo.LightMap->GetLightMap2D() : NULL;

			UINT CoefficientIndexMin = GSystemSettings.bAllowDirectionalLightMaps ? 0 : SIMPLE_LIGHTMAP_COEF_INDEX;
			UINT CoefficientIndexMax = GSystemSettings.bAllowDirectionalLightMaps ? NUM_DIRECTIONAL_LIGHTMAP_COEF : NUM_STORED_LIGHTMAP_COEF;

			for ( UINT CoefficientIndex = CoefficientIndexMin; CoefficientIndex < CoefficientIndexMax; CoefficientIndex++ )
			{
				if ( Lightmap != NULL && Lightmap->IsValid(CoefficientIndex) )
				{
					const FVector2D& Scale		= Lightmap->GetCoordinateScale();
					if ( Scale.X > SMALL_NUMBER && Scale.Y > SMALL_NUMBER )
					{
						FLOAT LightmapFactorX		 = WorldLightmapFactor / Scale.X;
						FLOAT LightmapFactorY		 = WorldLightmapFactor / Scale.Y;
						FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
						StreamingTexture.Bounds		 = BoundingSphere;
						StreamingTexture.TexelFactor = Max(LightmapFactorX, LightmapFactorY);
						StreamingTexture.Texture	 = Lightmap->GetTexture(CoefficientIndex);
					}
				}
			}

			TArray<UShadowMap2D*> ShadowMaps = LODInfo.ShadowMaps;
			for ( INT ShadowMapIndex=0; ShadowMapIndex < ShadowMaps.Num(); ++ShadowMapIndex )
			{
				UShadowMap2D* ShadowMap2D = ShadowMaps( ShadowMapIndex );
				if( ShadowMap2D != NULL && ShadowMap2D->IsValid() )
				{
					const FVector2D& Scale = ShadowMap2D->GetCoordinateScale();
					if ( Scale.X > SMALL_NUMBER && Scale.Y > SMALL_NUMBER )
					{
						FLOAT ShadowmapFactorX		 = WorldLightmapFactor / Scale.X;
						FLOAT ShadowmapFactorY		 = WorldLightmapFactor / Scale.Y;
						FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
						StreamingTexture.Bounds		 = BoundingSphere;
						StreamingTexture.TexelFactor = Max(ShadowmapFactorX, ShadowmapFactorY);
						StreamingTexture.Texture	 = ShadowMap2D->GetTexture();
					}
				}
			}
		}
	}
}

/* ==========================================================================================================
	AStaticMeshCollectionActor
========================================================================================================== */
/* === UObject interface === */
/**
 * Serializes the LocalToWorld transforms for the StaticMeshComponents contained in this actor.
 */
void AStaticMeshCollectionActor::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	if (!HasAnyFlags(RF_ClassDefaultObject) && Ar.GetLinker() != NULL && Ar.IsLoading())
		{
			Components = (TArrayNoInit<UActorComponent*>&)StaticMeshComponents;
			StaticMeshComponents.Empty();
		}
}

/** 
  * Used by Octree ActorRadius check to determine whether to return a component even if the actor owning the component has already been returned.
  * Make sure all static mesh components which can become dynamic are returned
  */
UBOOL AStaticMeshCollectionActor::ForceReturnComponent(UPrimitiveComponent* TestPrimitive)
{
	return Cast<UStaticMeshComponent>(TestPrimitive) && ((UStaticMeshComponent *)TestPrimitive)->CanBecomeDynamic();
}

// EOF




