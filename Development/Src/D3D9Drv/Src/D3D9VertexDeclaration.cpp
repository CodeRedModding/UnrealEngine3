/*=============================================================================
	D3D9VertexBuffer.cpp: D3D vertex declaration RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D9DrvPrivate.h"

IMPLEMENT_COMPARE_CONSTREF(
	D3DVERTEXELEMENT9,
	D3DVertexDeclaration,
	{ return ((INT)A.Offset + A.Stream * MAXWORD) - ((INT)B.Offset + B.Stream * MAXWORD); }
	)

FD3D9VertexDeclarationCache::FKey::FKey(const FVertexDeclarationElementList& InElements)
{
	for(UINT ElementIndex = 0;ElementIndex < InElements.Num();ElementIndex++)
	{
		if( !GVertexElementTypeSupport.IsSupported((EVertexElementType)InElements(ElementIndex).Type) )
		{
			appErrorf(TEXT("Vertex element type %u not supported"),InElements(ElementIndex).Type);
		}
		D3DVERTEXELEMENT9 VertexElement;
		VertexElement.Stream = InElements(ElementIndex).StreamIndex;
		VertexElement.Offset = InElements(ElementIndex).Offset;
		switch(InElements(ElementIndex).Type)
		{
		case VET_Float1:		VertexElement.Type = D3DDECLTYPE_FLOAT1; break;
		case VET_Float2:		VertexElement.Type = D3DDECLTYPE_FLOAT2; break;
		case VET_Float3:		VertexElement.Type = D3DDECLTYPE_FLOAT3; break;
		case VET_Float4:		VertexElement.Type = D3DDECLTYPE_FLOAT4; break;
		case VET_PackedNormal:	VertexElement.Type = D3DDECLTYPE_UBYTE4; break;
		case VET_UByte4:		VertexElement.Type = D3DDECLTYPE_UBYTE4; break;
		case VET_UByte4N:		VertexElement.Type = D3DDECLTYPE_UBYTE4N; break;
		case VET_Color:			VertexElement.Type = D3DDECLTYPE_D3DCOLOR; break;
		case VET_Short2:		VertexElement.Type = D3DDECLTYPE_SHORT2; break;
		case VET_Short2N:		VertexElement.Type = D3DDECLTYPE_SHORT2N; break;
		case VET_Half2:			VertexElement.Type = D3DDECLTYPE_FLOAT16_2; break;
		default: appErrorf(TEXT("Unknown RHI vertex element type %u"),InElements(ElementIndex).Type);
		};
		VertexElement.Method = D3DDECLMETHOD_DEFAULT;
		switch(InElements(ElementIndex).Usage)
		{
		case VEU_Position:			VertexElement.Usage = D3DDECLUSAGE_POSITION; break;
		case VEU_TextureCoordinate:	VertexElement.Usage = D3DDECLUSAGE_TEXCOORD; break;
		case VEU_BlendWeight:		VertexElement.Usage = D3DDECLUSAGE_BLENDWEIGHT; break;
		case VEU_BlendIndices:		VertexElement.Usage = D3DDECLUSAGE_BLENDINDICES; break;
		case VEU_Normal:			VertexElement.Usage = D3DDECLUSAGE_NORMAL; break;
		case VEU_Tangent:			VertexElement.Usage = D3DDECLUSAGE_TANGENT; break;
		case VEU_Binormal:			VertexElement.Usage = D3DDECLUSAGE_BINORMAL; break;
		case VEU_Color:				VertexElement.Usage = D3DDECLUSAGE_COLOR; break;
		};
		VertexElement.UsageIndex = InElements(ElementIndex).UsageIndex;

		VertexElements.AddItem(VertexElement);
	}

	// Sort the D3DVERTEXELEMENTs by stream then offset.
	Sort<USE_COMPARE_CONSTREF(D3DVERTEXELEMENT9,D3DVertexDeclaration)>(GetVertexElements(),InElements.Num());

	// Terminate the vertex element list.
	D3DVERTEXELEMENT9 EndElement = D3DDECL_END();
	VertexElements.AddItem(EndElement);

	Hash = appMemCrc(GetVertexElements(), sizeof(D3DVERTEXELEMENT9) * VertexElements.Num()); 
}

UBOOL FD3D9VertexDeclarationCache::FKey::operator == (const FKey &Other) const
{
	// same number and matching element
	return	VertexElements.Num() == Other.VertexElements.Num() && 
			!appMemcmp(Other.GetVertexElements(), GetVertexElements(), sizeof(D3DVERTEXELEMENT9) * VertexElements.Num());
}

FD3D9VertexDeclaration* FD3D9VertexDeclarationCache::GetVertexDeclaration(const FKey& Declaration)
{ 
	TRefCountPtr<FD3D9VertexDeclaration>* Value = VertexDeclarationMap.Find(Declaration);
	if (!Value)
	{
		TRefCountPtr<IDirect3DVertexDeclaration9> NewDeclaration;
		VERIFYD3D9RESULT(D3DRHI->GetDevice()->CreateVertexDeclaration(Declaration.GetVertexElements(),NewDeclaration.GetInitReference()));
		Value = &VertexDeclarationMap.Set(Declaration, (FD3D9VertexDeclaration*)NewDeclaration.GetReference());
	}
	return *Value;
}

FVertexDeclarationRHIRef FD3D9DynamicRHI::CreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	// Create the vertex declaration.
	FD3D9VertexDeclaration* VertexDeclaration = (FD3D9VertexDeclaration*)VertexDeclarationCache.GetVertexDeclaration(FD3D9VertexDeclarationCache::FKey(Elements));

	return VertexDeclaration;
}

FVertexDeclarationRHIRef FD3D9DynamicRHI::CreateVertexDeclaration(const FVertexDeclarationElementList& Elements, FName DeclName)
{
	return CreateVertexDeclaration(Elements);
}
