/*=============================================================================
	UnStaticMesh.cpp: Static mesh class implementation.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"


IMPLEMENT_CLASS(AStaticMeshActor);
IMPLEMENT_CLASS(UStaticMesh);

//
//	FStaticMeshPositionVertexBuffer::Update
//

void FStaticMeshPositionVertexBuffer::Update()
{
	Stride = sizeof(FShadowVertex);
	Size = StaticMesh->Vertices.Num() * Stride * 2;
	GResourceManager->UpdateResource(this);
}

//
//	FStaticMeshPositionVertexBuffer::GetData
//

void FStaticMeshPositionVertexBuffer::GetData(void* Buffer)
{
	check(Size == StaticMesh->Vertices.Num() * sizeof(FShadowVertex) * 2);

	FShadowVertex*	DestVertex = (FShadowVertex*)Buffer;
	for(UINT VertexIndex = 0;VertexIndex < (UINT)StaticMesh->Vertices.Num();VertexIndex++)
		*DestVertex++ = FShadowVertex(StaticMesh->Vertices(VertexIndex).Position,0.0f);
	for(UINT VertexIndex = 0;VertexIndex < (UINT)StaticMesh->Vertices.Num();VertexIndex++)
		*DestVertex++ = FShadowVertex(StaticMesh->Vertices(VertexIndex).Position,1.0f);

}

//
//	FStaticMeshTangentVertexBuffer::Update
//

void FStaticMeshTangentVertexBuffer::Update()
{
	Stride = sizeof(FPackedNormal) * 3;
	Size = StaticMesh->Vertices.Num() * Stride;
	GResourceManager->UpdateResource(this);
}

//
//	FStaticMeshTangentVertexBuffer::GetData
//

void FStaticMeshTangentVertexBuffer::GetData(void* Buffer)
{
	check(Size == StaticMesh->Vertices.Num() * sizeof(FPackedNormal) * 3);

	FPackedNormal*	DestNormal = (FPackedNormal*)Buffer;
	for(UINT VertexIndex = 0;VertexIndex < (UINT)StaticMesh->Vertices.Num();VertexIndex++)
	{
		FStaticMeshVertex&	Vertex = StaticMesh->Vertices(VertexIndex);
		*DestNormal++ = Vertex.TangentX;
		*DestNormal++ = Vertex.TangentY;
		*DestNormal++ = Vertex.TangentZ;
	}

}

//
//	UStaticMesh::UStaticMesh
//

UStaticMesh::UStaticMesh():
	PositionVertexBuffer(this),
	TangentVertexBuffer(this)
{
}

//
//	UStaticMesh::StaticConstructor
//

void UStaticMesh::StaticConstructor()
{
	new(GetClass()->HideCategories) FName(NAME_Object);

	new(GetClass(),TEXT("UseSimpleLineCollision"),RF_Public)		UBoolProperty(CPP_PROPERTY(UseSimpleLineCollision),TEXT(""),CPF_Edit);
	new(GetClass(),TEXT("UseSimpleBoxCollision"),RF_Public)			UBoolProperty(CPP_PROPERTY(UseSimpleBoxCollision),TEXT(""),CPF_Edit);
	new(GetClass(),TEXT("UseSimpleKarmaCollision"),RF_Public)		UBoolProperty(CPP_PROPERTY(UseSimpleKarmaCollision),TEXT(""),CPF_Edit);
	new(GetClass(),TEXT("UseVertexColor"),RF_Public)				UBoolProperty(CPP_PROPERTY(UseVertexColor),TEXT(""),CPF_Edit);
	new(GetClass(),TEXT("ForceDoubleSidedShadowVolumes"),RF_Public)	UBoolProperty(CPP_PROPERTY(DoubleSidedShadowVolumes),TEXT(""),CPF_Edit);

	new(GetClass(),TEXT("LightMapResolution"),RF_Public)			UIntProperty(CPP_PROPERTY(LightMapResolution),TEXT(""),CPF_Edit);
	new(GetClass(),TEXT("LightMapCoordinateIndex"),RF_Public)		UIntProperty(CPP_PROPERTY(LightMapCoordinateIndex),TEXT(""),CPF_Edit);

	FArchive ArDummy;
	UStruct* MaterialStruct = new(GetClass(),TEXT("StaticMeshMaterial")) UStruct(NULL);

	new(MaterialStruct,TEXT("Material"),RF_Public)	UObjectProperty(EC_CppProperty,STRUCT_OFFSET(FStaticMeshMaterial,Material),TEXT(""),CPF_Edit,UMaterialInstance::StaticClass());

	new(MaterialStruct,TEXT("EnableCollision"),RF_Public)	UBoolProperty(EC_CppProperty,STRUCT_OFFSET(FStaticMeshMaterial,EnableCollision),TEXT(""),CPF_Edit);
	MaterialStruct->SetPropertiesSize(sizeof(FStaticMeshMaterial));
	MaterialStruct->AllocateStructDefaults();
	MaterialStruct->Link(ArDummy,0);

	UArrayProperty*	A	= new(GetClass(),TEXT("Materials"),RF_Public)	UArrayProperty(CPP_PROPERTY(Materials),TEXT(""),CPF_Edit | CPF_EditConstArray | CPF_Native);
	A->Inner			= new(A,TEXT("StructProperty0"),RF_Public)		UStructProperty(EC_CppProperty,0,TEXT(""),CPF_Edit,MaterialStruct);

	new(GetClass(),TEXT("BodySetup"),RF_Public)	UObjectProperty(CPP_PROPERTY(BodySetup),TEXT(""),CPF_Edit | CPF_EditInline, URB_BodySetup::StaticClass());

	UseSimpleLineCollision = 0;
	UseSimpleBoxCollision = 1;
	UseSimpleKarmaCollision = 1;
	UseVertexColor = 0;

}

//
//	UStaticMesh::PostEditChange
//

void UStaticMesh::PostEditChange(UProperty* PropertyThatChanged)
{
	Super::PostEditChange(PropertyThatChanged);

	LightMapResolution = 1 << appCeilLogTwo( LightMapResolution );

	// If any of the materials have had collision added or removed, rebuild the static mesh.

	UBOOL	NeedsRebuild = 0;
	for(UINT MaterialIndex = 0;MaterialIndex < (UINT)Materials.Num();MaterialIndex++)
	{	
		if(Materials(MaterialIndex).OldEnableCollision != Materials(MaterialIndex).EnableCollision)
		{
			NeedsRebuild = 1;
			Materials(MaterialIndex).OldEnableCollision = Materials(MaterialIndex).EnableCollision;
		}
	}

	if(NeedsRebuild)
	{
		Build();
	}
	else
	{
		FStaticMeshComponentRecreateContext(this);
	}
}

//
// UStaticMesh::Destroy
//

void UStaticMesh::Destroy()
{
	//@warning: the below is merged from UPrimitive - need to re-evaluate that code
	// Destroy any inner's of this skeletal mesh. This should only happen in the editor i think.
	if(!GIsGarbageCollecting)
	{
		if(BodySetup)
		{
			delete BodySetup;
			BodySetup = NULL;
		}
	}

	Super::Destroy();
}

//
//	UStaticMesh::Rename
//

void UStaticMesh::Rename( const TCHAR* InName, UObject* NewOuter )
{
	// Also rename the collision mode (if present), if the outer has changed.
    if( NewOuter && CollisionModel && CollisionModel->GetOuter() == GetOuter() )
		CollisionModel->Rename( CollisionModel->GetName(), NewOuter );

	// Rename the static mesh
    Super::Rename( InName, NewOuter );

}

//
//	FStaticMeshTriangle serializer.
//

FArchive& operator<<(FArchive& Ar,FStaticMeshTriangle& T)
{

	Ar	<< T.Vertices[0]
		<< T.Vertices[1]
		<< T.Vertices[2]
		<< T.NumUVs;

	for(INT UVIndex = 0;UVIndex < T.NumUVs;UVIndex++)
		Ar	<< T.UVs[0][UVIndex]
			<< T.UVs[1][UVIndex]
			<< T.UVs[2][UVIndex];

	Ar	<< T.Colors[0]
		<< T.Colors[1]
		<< T.Colors[2];

	Ar << T.MaterialIndex;
	Ar << T.SmoothingMask;
	
	return Ar;
}

//
//	UStaticMesh::Serialize
//

void UStaticMesh::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Bounds;
	Ar << BodySetup;
	Ar << CollisionModel;
	Ar << kDOPTree;

	if(Ar.IsSaving() && !RawTriangles.Num())
		RawTriangles.Load();
	Ar << RawTriangles;

	if( Ar.IsLoading() )
    {
		Ar << InternalVersion;
	}
    else if( Ar.IsSaving() )
    {
		InternalVersion = STATICMESH_VERSION;
		Ar << InternalVersion;
    }

	Ar << Materials;	
	Ar << Vertices;

	PositionVertexBuffer.Update();
	TangentVertexBuffer.Update();

	Ar << UVBuffers << IndexBuffer << WireframeIndexBuffer << Edges;
	Ar << ShadowTriangleDoubleSided;
	Ar << ThumbnailAngle;

	if( Ar.Ver() < 182 )
	{
		INT DeprecatedStreamingTextureFactor;
		Ar << DeprecatedStreamingTextureFactor;
	}
}

//
//	UStaticMesh::PostLoad
//

void UStaticMesh::PostLoad()
{
	Super::PostLoad();

	if(InternalVersion < STATICMESH_VERSION)
		Build();
}

//
//	UStaticMesh::GetDesc
//

FString UStaticMesh::GetDesc()
{
	RawTriangles.Load();
	return FString::Printf( TEXT("%d Triangles"), RawTriangles.Num() );
}

//
//	UStaticMeshComponent::GetMaterial
//

UMaterialInstance* UStaticMeshComponent::GetMaterial(INT MaterialIndex) const
{
	if(MaterialIndex < Materials.Num() && Materials(MaterialIndex))
		return Materials(MaterialIndex);
	else if(StaticMesh && MaterialIndex < StaticMesh->Materials.Num() && StaticMesh->Materials(MaterialIndex).Material)
		return StaticMesh->Materials(MaterialIndex).Material;
	else
		return GEngine->DefaultMaterial;
}


// Deprecated static mesh lightmap required by UStaticMeshComponent::Serialize.
struct FDeprecatedStaticMeshLightMap : FTexture2D
{
	ULightComponent*		Light;
	TLazyArray<BYTE>		Visibility;

	void GetData(void* Buffer,UINT MipIndex,UINT StrideY){}

	friend FArchive& operator<<(FArchive& Ar,FDeprecatedStaticMeshLightMap& L)
	{
		Ar << L.Light;
		Ar << L.Visibility;
		Ar << L.SizeX << L.SizeY;
		return Ar;
	}
};

//
//	UStaticMeshComponent::Serialize
//

void UStaticMeshComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << StaticLights;

	if( Ar.Ver() < 183 )
	{
		TIndirectArray<FDeprecatedStaticMeshLightMap> OldLightMaps;
		Ar << OldLightMaps;
	}
	else
	{
		Ar << StaticLightMaps;
	}
}

//
//	UStaticMeshComponent::UpdateBounds
//

void UStaticMeshComponent::UpdateBounds()
{
	if(StaticMesh)
	{
		FBoxSphereBounds	LocalBounds = StaticMesh->Bounds;

		if(StaticMesh->CollisionModel)
			LocalBounds = LocalBounds + StaticMesh->CollisionModel->Bounds;

		Bounds = LocalBounds.TransformBy(LocalToWorld);
		
		// Takes into account that the static mesh collision code nudges collisions out by up to 1 unit.

		Bounds.BoxExtent += FVector(1,1,1);
		Bounds.SphereRadius += 1.0f;
	}
	else
		Super::UpdateBounds();
}

//
//	UStaticMeshComponent::IsValidComponent
//

UBOOL UStaticMeshComponent::IsValidComponent() const
{
	return StaticMesh != NULL && StaticMesh->Vertices.Num() > 0 && Super::IsValidComponent();
}
