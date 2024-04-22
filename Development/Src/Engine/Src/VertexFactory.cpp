/*=============================================================================
	VertexFactory.cpp: Vertex factory implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

DWORD FVertexFactoryType::NextHashIndex = 0;

/**
 * @return The global shader factory list.
 */
TLinkedList<FVertexFactoryType*>*& FVertexFactoryType::GetTypeList()
{
	static TLinkedList<FVertexFactoryType*>* TypeList = NULL;
	return TypeList;
}

/**
 * Finds a FVertexFactoryType by name.
 */
FVertexFactoryType* FVertexFactoryType::GetVFByName(const FString& VFName)
{
	for(TLinkedList<FVertexFactoryType*>::TIterator It(GetTypeList()); It; It.Next())
	{
		FString CurrentVFName = FString(It->GetName());
		if (CurrentVFName == VFName)
		{
			return *It;
		}
	}
	return NULL;
}

FVertexFactoryType::FVertexFactoryType(
	const TCHAR* InName,
	const TCHAR* InShaderFilename,
	UBOOL bInUsedWithMaterials,
	UBOOL bInSupportsStaticLighting,
	UBOOL bInSupportsDynamicLighting,
	UBOOL bInSupportsPrecisePrevWorldPos,
	UBOOL bInUsesLocalToWorld,
	ConstructParametersType InConstructParameters,
	ShouldCacheType InShouldCache,
	ModifyCompilationEnvironmentType InModifyCompilationEnvironment,
	SupportsTessellationShadersType InSupportsTessellationShaders,
	INT InMinPackageVersion,
	INT InMinLicenseePackageVersion
	):
	Name(InName),
	ShaderFilename(InShaderFilename),
	TypeName(InName),
	bUsedWithMaterials(bInUsedWithMaterials),
	bSupportsStaticLighting(bInSupportsStaticLighting),
	bSupportsDynamicLighting(bInSupportsDynamicLighting),
	bSupportsPrecisePrevWorldPos(bInSupportsPrecisePrevWorldPos),
	bUsesLocalToWorld(bInUsesLocalToWorld),
	ConstructParameters(InConstructParameters),
	ShouldCacheRef(InShouldCache),
	ModifyCompilationEnvironmentRef(InModifyCompilationEnvironment),
	SupportsTessellationShadersRef(InSupportsTessellationShaders),
	MinPackageVersion(InMinPackageVersion),
	MinLicenseePackageVersion(InMinLicenseePackageVersion)
{
	// Add this vertex factory type to the global list.
	(new TLinkedList<FVertexFactoryType*>(this))->Link(GetTypeList());

	// Assign the vertex factory type the next unassigned hash index.
	HashIndex = NextHashIndex++;
}

/** Calculates a Hash based on this vertex factory type's source code and includes */
const FSHAHash& FVertexFactoryType::GetSourceHash() const
{
#if CONSOLE
	static FSHAHash CurrentHash = {0};
	return CurrentHash;
#else
	return GetShaderFileHash(GetShaderFilename());
#endif
}

FArchive& operator<<(FArchive& Ar,FVertexFactoryType*& TypeRef)
{
	if(Ar.IsSaving())
	{
		FName TypeName = TypeRef ? FName(TypeRef->GetName()) : NAME_None;
		Ar << TypeName;
	}
	else if(Ar.IsLoading())
	{
		FName TypeName = NAME_None;
		Ar << TypeName;
		TypeRef = FindVertexFactoryType(TypeName);
	}
	return Ar;
}

FVertexFactoryType* FindVertexFactoryType(FName TypeName)
{
	// Search the global vertex factory list for a type with a matching name.
	for(TLinkedList<FVertexFactoryType*>::TIterator VertexFactoryTypeIt(FVertexFactoryType::GetTypeList());VertexFactoryTypeIt;VertexFactoryTypeIt.Next())
	{
		if(VertexFactoryTypeIt->GetFName() == TypeName)
		{
			return *VertexFactoryTypeIt;
		}
	}
	return NULL;
}

FVertexFactory::DataType::DataType()
{
	LightMapStream.Offset = 0,
	LightMapStream.DirectionalStride = sizeof(FQuantizedDirectionalLightSample);
	LightMapStream.SimpleStride = sizeof(FQuantizedSimpleLightSample);
	LightMapStream.bUseInstanceIndex = FALSE;
	NumVerticesPerInstance = 0;
	NumInstances = 1;
}

void FVertexFactory::Set() const
{
	check(IsInitialized());
	for(UINT StreamIndex = 0;StreamIndex < Streams.Num();StreamIndex++)
	{
		const FVertexStream& Stream = Streams(StreamIndex);
		check(Stream.VertexBuffer->IsInitialized());
		RHISetStreamSource(StreamIndex,Stream.VertexBuffer->VertexBufferRHI,Stream.Stride,0,Stream.bUseInstanceIndex,Data.NumVerticesPerInstance,Data.NumInstances);
	}
}

void FVertexFactory::SetPositionStream() const
{
	check(IsInitialized());
	// Set the predefined vertex streams.
	for(UINT StreamIndex = 0;StreamIndex < PositionStream.Num();StreamIndex++)
	{
		const FVertexStream& Stream = PositionStream(StreamIndex);
		check(Stream.VertexBuffer->IsInitialized());
		RHISetStreamSource(StreamIndex,Stream.VertexBuffer->VertexBufferRHI,Stream.Stride,0,Stream.bUseInstanceIndex,Data.NumVerticesPerInstance,Data.NumInstances);
	}
}

void FVertexFactory::SetVertexShadowMap(const FVertexBuffer* ShadowMapVertexBuffer) const
{
	Set();
	check(ShadowMapVertexBuffer->IsInitialized());
	// Set the shadow-map vertex stream.
	RHISetStreamSource(Streams.Num(),ShadowMapVertexBuffer->VertexBufferRHI,sizeof(FLOAT),0,FALSE,0,1);
}

void FVertexFactory::SetVertexLightMap(const FVertexBuffer* LightMapVertexBuffer,UBOOL bUseDirectionalLightMap) const
{
	Set();
	check(LightMapVertexBuffer->IsInitialized());
	// Set the shadow-map vertex stream.
	BYTE LightMapStride = bUseDirectionalLightMap ? Data.LightMapStream.DirectionalStride : Data.LightMapStream.SimpleStride;
	RHISetStreamSource(Streams.Num(),LightMapVertexBuffer->VertexBufferRHI,LightMapStride,0,Data.LightMapStream.bUseInstanceIndex,Data.NumVerticesPerInstance,Data.NumInstances);
}

/**
 * Activates the vertex factory for rendering a vertex light-map and vertex shadow-map at the same time.
 */
void FVertexFactory::SetVertexLightMapAndShadowMap(const FVertexBuffer* LightMapVertexBuffer, const FVertexBuffer* ShadowMapVertexBuffer) const
{
	Set();
	check(LightMapVertexBuffer->IsInitialized());
	check(ShadowMapVertexBuffer->IsInitialized());
	// Set the shadow-map vertex stream.
	const BYTE LightMapStride = Data.LightMapStream.DirectionalStride;
	RHISetStreamSource(Streams.Num(),LightMapVertexBuffer->VertexBufferRHI,LightMapStride,0,Data.LightMapStream.bUseInstanceIndex,Data.NumVerticesPerInstance,Data.NumInstances);
	// Set the shadow-map vertex stream.
	// If the vertex buffer is the null shadowmap vertex buffer, it is being used as a placeholder.
	// Set the stride to 0 so the single value will be replicated to all vertices.
	const INT ShadowMapStride = ShadowMapVertexBuffer == &GNullShadowmapVertexBuffer ? 0 : sizeof(FLOAT);
	RHISetStreamSource(Streams.Num() + 1,ShadowMapVertexBuffer->VertexBufferRHI,ShadowMapStride,0,FALSE,0,1);
}

void FVertexFactory::ReleaseRHI()
{
	Declaration.SafeRelease();
	PositionDeclaration.SafeRelease();
	VertexShadowMapDeclaration.SafeRelease();
	DirectionalVertexLightMapDeclaration.SafeRelease();
	SimpleVertexLightMapDeclaration.SafeRelease();
	DirectionalVertexLightMapAndShadowMapDeclaration.SafeRelease();
	Streams.Empty();
	PositionStream.Empty();
}

/**
* Fill in array of strides from this factory's vertex streams without shadow/light maps
* @param OutStreamStrides - output array of # MaxVertexElementCount stream strides to fill
*/
INT FVertexFactory::GetStreamStrides(DWORD *OutStreamStrides, UBOOL bPadWithZeroes) const
{
	UINT StreamIndex;
	for(StreamIndex = 0;StreamIndex < Streams.Num();++StreamIndex)
	{
		OutStreamStrides[StreamIndex] = Streams(StreamIndex).Stride;
	}
	if (bPadWithZeroes)
	{
		// Pad stream strides with 0's to be safe (they can be used in hashes elsewhere)
		for (;StreamIndex < MaxVertexElementCount;++StreamIndex)
		{
			OutStreamStrides[StreamIndex] = 0;
		}
	}
	return StreamIndex;
}

/**
* Fill in array of strides from this factory's position only vertex streams
* @param OutStreamStrides - output array of # MaxVertexElementCount stream strides to fill
*/
void FVertexFactory::GetPositionStreamStride(DWORD *OutStreamStrides) const
{
	UINT StreamIndex;
	for(StreamIndex = 0;StreamIndex < PositionStream.Num();++StreamIndex)
	{
		OutStreamStrides[StreamIndex] = PositionStream(StreamIndex).Stride;
	}
	// Pad stream strides with 0's to be safe (they can be used in hashes elsewhere)
	for (;StreamIndex < MaxVertexElementCount;++StreamIndex)
	{
		OutStreamStrides[StreamIndex] = 0;
	}
}

/**
* Fill in array of strides from this factory's vertex streams when using shadow-maps
* @param OutStreamStrides - output array of # MaxVertexElementCount stream strides to fill
*/
void FVertexFactory::GetVertexShadowMapStreamStrides(DWORD *OutStreamStrides) const
{
	UINT StreamIndex = GetStreamStrides(OutStreamStrides, FALSE);
	OutStreamStrides[StreamIndex ++] = sizeof(FLOAT);
	// Pad stream strides with 0's to be safe (they can be used in hashes elsewhere)
	for (;StreamIndex < MaxVertexElementCount;++StreamIndex)
	{
		OutStreamStrides[StreamIndex] = 0;
	}
}

/**
* Fill in array of strides from this factory's vertex streams when using light-maps
* @param OutStreamStrides - output array of # MaxVertexElementCount stream strides to fill
*/
void FVertexFactory::GetVertexLightMapStreamStrides(DWORD *OutStreamStrides,UBOOL bUseDirectionalLightMap) const
{
	UINT StreamIndex = GetStreamStrides(OutStreamStrides, FALSE);
	BYTE LightMapStride = bUseDirectionalLightMap ? Data.LightMapStream.DirectionalStride : Data.LightMapStream.SimpleStride;
	OutStreamStrides[StreamIndex ++] = LightMapStride;
	// Pad stream strides with 0's to be safe (they can be used in hashes elsewhere)
	for (;StreamIndex < MaxVertexElementCount;++StreamIndex)
	{
		OutStreamStrides[StreamIndex] = 0;
	}
}

/**
* Fill in array of strides from this factory's vertex streams when using light-maps amd shadow-maps at the same time
* @param OutStreamStrides - output array of # MaxVertexElementCount stream strides to fill
*/
void FVertexFactory::GetVertexLightMapAndShadowMapStreamStrides(DWORD *OutStreamStrides) const
{
	UINT StreamIndex = GetStreamStrides(OutStreamStrides, FALSE);
	BYTE LightMapStride = Data.LightMapStream.DirectionalStride;
	OutStreamStrides[StreamIndex ++] = LightMapStride;
	OutStreamStrides[StreamIndex ++] = sizeof(FLOAT);
	// Pad stream strides with 0's to be safe (they can be used in hashes elsewhere)
	for (;StreamIndex < MaxVertexElementCount;++StreamIndex)
	{
		OutStreamStrides[StreamIndex] = 0;
	}
}

FVertexElement FVertexFactory::AccessStreamComponent(const FVertexStreamComponent& Component,BYTE Usage,BYTE UsageIndex)
{
	FVertexStream VertexStream;
	VertexStream.VertexBuffer = Component.VertexBuffer;
	VertexStream.Stride = Component.Stride;
	VertexStream.bUseInstanceIndex = Component.bUseInstanceIndex;

	return FVertexElement(Streams.AddUniqueItem(VertexStream),Component.Offset,Component.Type,Usage,UsageIndex,Component.bUseInstanceIndex,Data.NumVerticesPerInstance);
}

FVertexElement FVertexFactory::AccessPositionStreamComponent(const FVertexStreamComponent& Component,BYTE Usage,BYTE UsageIndex)
{
	FVertexStream VertexStream;
	VertexStream.VertexBuffer = Component.VertexBuffer;
	VertexStream.Stride = Component.Stride;
	VertexStream.bUseInstanceIndex = Component.bUseInstanceIndex;

	return FVertexElement(PositionStream.AddUniqueItem(VertexStream),Component.Offset,Component.Type,Usage,UsageIndex,Component.bUseInstanceIndex,Data.NumVerticesPerInstance);
}

//TTP:51684
/**
 *	GetVertexElementSize returns the size of data based on its type
 *	it's needed for FixFXCardDeclarator function
 */
static BYTE GetVertexElementSize( BYTE Type )
{
	switch( Type )
	{
		case VET_Float1:
			return 4;
		case VET_Float2:
			return 8;
		case VET_Float3:
			return 12;
		case VET_Float4:
			return 16;
		case VET_PackedNormal:
		case VET_UByte4:
		case VET_UByte4N:
		case VET_Color:
		case VET_Short2:
		case VET_Short2N:
		case VET_Half2:
		case VET_Pos3N:
			return 4;
		default:
			return 0;
	}
}

/**
 * Patches the declaration so vertex stream offsets are unique. This is required for e.g. GeForce FX cards, which don't support redundant 
 * offsets in the declaration. We're unable to make that many vertex elements point to the same offset so the function moves redundant 
 * declarations to higher offsets, pointing to garbage data.
 */
static void PatchVertexStreamOffsetsToBeUnique( FVertexDeclarationElementList& Elements )
{
	// check every vertex element
	for ( UINT e = 0; e < Elements.Num(); e++ )
	{
		// check if there's an element that reads from the same offset
		for ( UINT i = 0; i < Elements.Num(); i++ )
		{
			// but only in the same stream and if it's not the same element
			if ( ( Elements( i ).StreamIndex == Elements( e ).StreamIndex ) && ( Elements( i ).Offset == Elements( e ).Offset ) && ( e != i ) )
			{
				// the id of the highest offset element is stored here (it doesn't need to be the last element in the declarator because the last element may belong to another StreamIndex
				UINT MaxOffsetID = i;

				// find the highest offset element
				for ( UINT j = 0; j < Elements.Num(); j++ )
				{
					if ( ( Elements( j ).StreamIndex == Elements( e ).StreamIndex ) && ( Elements( MaxOffsetID ).Offset < Elements( j ).Offset ) )
					{
						MaxOffsetID = j;
					}
				}

				// get the size of the highest offset element, it's needed for the redundant element new offset
				BYTE PreviousElementSize = GetVertexElementSize( Elements( MaxOffsetID ).Type );

				// prepare a new vertex element
				FVertexElement VertElement;
				VertElement.Offset		= Elements( MaxOffsetID ).Offset + PreviousElementSize;
				VertElement.StreamIndex = Elements( i ).StreamIndex;
				VertElement.Type		= Elements( i ).Type;
				VertElement.Usage		= Elements( i ).Usage;
				VertElement.UsageIndex	= Elements( i ).UsageIndex;
				VertElement.bUseInstanceIndex = Elements(i).bUseInstanceIndex;
				VertElement.NumVerticesPerInstance = Elements(i).NumVerticesPerInstance;

				// remove the old redundant element
				Elements.Remove( i );

				// add a new element with "correct" offset
				Elements.AddItem( VertElement );

				// make sure that when the element has been removed its index is taken by the next element, so we must take care of it too
				i = i == 0 ? 0 : i - 1;
			}
		}
	}
}

void FVertexFactory::InitDeclaration(
	FVertexDeclarationElementList& Elements, 
	const DataType& InData, 
	UBOOL bCreateShadowMapDeclaration, 
	UBOOL bCreateDirectionalLightMapDeclaration, 
	UBOOL bCreateSimpleLightMapDeclaration)
{
	static FName NAME_Decal = FName(TEXT("Decal"));

	// Make a copy of the vertex factory data.
	Data = InData;

	// If GFFX detected, patch up the declarator
	if( !GVertexElementsCanShareStreamOffset )
	{
		PatchVertexStreamOffsetsToBeUnique( Elements );
	}

	FName VertexDeclName = IsDecalFactory() ? NAME_Decal : GetType()->GetFName();

	// Create the vertex declaration for rendering the factory normally.
	Declaration = RHICreateVertexDeclaration(Elements, VertexDeclName);

	// NOTE: For now we only name the rest of the vertex declaration if this is a
	//       decal factory.  This isn't strictly necessary but here to avoid any
	//       unwanted side-effects.

	if (!IsDecalFactory())
	{
		VertexDeclName = NAME_None;
	}

	if (GetType()->SupportsStaticLighting())
	{
		if (bCreateShadowMapDeclaration)
		{
			// Create the vertex declaration for rendering the factory with a vertex shadow-map.
			FVertexDeclarationElementList ShadowMapElements = Elements;
			ShadowMapElements.AddItem(FVertexElement(Streams.Num(),0,VET_Float1,VEU_BlendWeight,0));
			VertexShadowMapDeclaration = RHICreateVertexDeclaration(ShadowMapElements, VertexDeclName);
		}

		if (bCreateDirectionalLightMapDeclaration)
		{
			// Create the vertex declaration for rendering the factory with a directional vertex light-map.
			FVertexDeclarationElementList LightMapElements = Elements;
			for(INT CoefficientIndex = 0;CoefficientIndex < NUM_DIRECTIONAL_LIGHTMAP_COEF;CoefficientIndex++)
			{
				LightMapElements.AddItem(FVertexElement(
					Streams.Num(),
					Data.LightMapStream.Offset + STRUCT_OFFSET(FQuantizedDirectionalLightSample,Coefficients[CoefficientIndex]),
					VET_Color,
					VEU_TextureCoordinate,
					5 + CoefficientIndex,
					Data.LightMapStream.bUseInstanceIndex,
					Data.NumVerticesPerInstance
					));
			}
			DirectionalVertexLightMapDeclaration = RHICreateVertexDeclaration(LightMapElements);

			LightMapElements.AddItem(FVertexElement(Streams.Num() + 1,0,VET_Float1,VEU_BlendWeight,0));
			DirectionalVertexLightMapAndShadowMapDeclaration = RHICreateVertexDeclaration(LightMapElements, VertexDeclName);
		}

		if (bCreateSimpleLightMapDeclaration)
		{
			// Create the vertex declaration for rendering the factory with a simple vertex light-map.
			FVertexDeclarationElementList LightMapElements = Elements;
			LightMapElements.AddItem(FVertexElement(
				Streams.Num(),
				Data.LightMapStream.Offset + STRUCT_OFFSET(FQuantizedSimpleLightSample,Coefficients[0]),
				VET_Color,
				VEU_TextureCoordinate,
				5,
				Data.LightMapStream.bUseInstanceIndex,
				Data.NumVerticesPerInstance
				));
			SimpleVertexLightMapDeclaration = RHICreateVertexDeclaration(LightMapElements, VertexDeclName);
		}
	}
}

void FVertexFactory::InitPositionDeclaration(const FVertexDeclarationElementList& Elements)
{
	// Create the vertex declaration for rendering the factory normally.
	PositionDeclaration = RHICreateVertexDeclaration(Elements);
}

FVertexFactoryParameterRef::FVertexFactoryParameterRef(FVertexFactoryType* InVertexFactoryType,const FShaderParameterMap& ParameterMap, EShaderFrequency InShaderFrequency):
	Parameters(NULL),
	VertexFactoryType(InVertexFactoryType)
{
	Parameters = VertexFactoryType->CreateShaderParameters(InShaderFrequency);
#if !CONSOLE
	VFHash = GetShaderFileHash(VertexFactoryType->GetShaderFilename());
#endif
	if(Parameters)
	{
		Parameters->Bind(ParameterMap);
	}
}

/*
 * Serializer for FVertexFactoryVSParameterRef
 */
UBOOL operator<<(FArchive& Ar,FVertexFactoryVSParameterRef& Ref)
{
	UBOOL bShaderHasOutdatedParameters = FALSE;

	Ar << Ref.VertexFactoryType;

#if CONSOLE
	FSHAHash Dummy;
	Ar << Dummy;
#else
	Ar << Ref.VFHash;
#endif

	if (Ar.IsLoading())
	{
		delete Ref.Parameters;
		if (Ref.VertexFactoryType)
		{
			const FSHAHash& CurrentVFHash = Ref.VertexFactoryType->GetSourceHash();

			if (Ar.Ver() >= Ref.VertexFactoryType->GetMinPackageVersion() 
				&& Ar.LicenseeVer() >= Ref.VertexFactoryType->GetMinLicenseePackageVersion()
#if !CONSOLE
				// Only create the vertex factory shader parameters if the current vertex factory file hash matches the one the shader was compiled with
				&& (!ShouldReloadChangedShaders() || Ref.VFHash == CurrentVFHash)
#endif
				)
			{
				Ref.Parameters = Ref.VertexFactoryType->CreateShaderParameters(SF_Vertex);
			}
			else
			{
				bShaderHasOutdatedParameters = TRUE;
				Ref.Parameters = NULL;
			}
		}
		else
		{
			bShaderHasOutdatedParameters = TRUE;
			Ref.Parameters = NULL;
		}
	}

	// Need to be able to skip over parameters for no longer existing vertex factories.
	INT SkipOffset = Ar.Tell();
	// Write placeholder.
	Ar << SkipOffset;

	if(Ref.Parameters)
	{
		Ref.Parameters->Serialize(Ar);
	}
	else if(Ar.IsLoading())
	{
		Ar.Seek( SkipOffset );
	}
	else if (Ar.IsSaving())
	{
		//saving a NULL FVertexFactoryParameterRef will corrupt the shader cache
		//the shader containing this parameter ref should have been thrown away on load
		const TCHAR* VertexFactoryName = Ref.VertexFactoryType != NULL ? Ref.VertexFactoryType->GetName() : TEXT("NOT FOUND");
		appErrorf(TEXT("Attempting to save a NULL FVertexFactoryParameterRef for VF %s!"), VertexFactoryName);
	}

	if( Ar.IsSaving() )
	{
		INT EndOffset = Ar.Tell();
		Ar.Seek( SkipOffset );
		Ar << EndOffset;
		Ar.Seek( EndOffset );
	}

	return bShaderHasOutdatedParameters;
}

/*
 * Serializer for FVertexFactoryPSParameterRef
 * - Differs from the above in that it supports cases where there are no parameters
 */
UBOOL operator<<(FArchive& Ar,FVertexFactoryPSParameterRef& Ref)
{
	UBOOL bShaderHasOutdatedParameters = FALSE;

	Ar << Ref.VertexFactoryType;

#if CONSOLE
	FSHAHash Dummy;
	Ar << Dummy;
#else
	Ar << Ref.VFHash;
#endif

	// Remember if we have any parameters or not.
	UBOOL HasParameters = TRUE;
	if( Ar.IsSaving() )
	{
		HasParameters = Ref.Parameters != NULL;
	}
	Ar << HasParameters;

	if (Ar.IsLoading())
	{
		delete Ref.Parameters;

		if (Ref.VertexFactoryType)
		{
			const FSHAHash& CurrentVFHash = Ref.VertexFactoryType->GetSourceHash();

			if (Ar.Ver() >= Ref.VertexFactoryType->GetMinPackageVersion() 
				&& Ar.LicenseeVer() >= Ref.VertexFactoryType->GetMinLicenseePackageVersion()
#if !CONSOLE
				// Only create the vertex factory shader parameters if the current vertex factory file hash matches the one the shader was compiled with
				&& (!ShouldReloadChangedShaders() || Ref.VFHash == CurrentVFHash)
#endif
				)
			{
				if( HasParameters )
				{
					Ref.Parameters = Ref.VertexFactoryType->CreateShaderParameters(SF_Pixel);
				}
				else
				{
					Ref.Parameters = NULL;
				}
			}
			else
			{
				bShaderHasOutdatedParameters = TRUE;
				Ref.Parameters = NULL;
			}
		}
		else
		{
			bShaderHasOutdatedParameters = TRUE;
			Ref.Parameters = NULL;
		}
	}

	// Need to be able to skip over parameters for no longer existing vertex factories.
	INT SkipOffset = Ar.Tell();
	// Write placeholder.
	Ar << SkipOffset;

	if(Ref.Parameters)
	{
		Ref.Parameters->Serialize(Ar);
	}
	else if(Ar.IsLoading())
	{
		Ar.Seek( SkipOffset );
	}

	if( Ar.IsSaving() )
	{
		INT EndOffset = Ar.Tell();
		Ar.Seek( SkipOffset );
		Ar << EndOffset;
		Ar.Seek( EndOffset );
	}

	return bShaderHasOutdatedParameters;
}

/** Returns the hash of the vertex factory shader file that this shader was compiled with. */
const FSHAHash& FVertexFactoryParameterRef::GetHash() const 
{ 
#if CONSOLE
	static FSHAHash Dummy = {0};
	return Dummy;
#else
	return VFHash;
#endif
}
