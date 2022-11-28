/*=============================================================================
	UnMeshEd.cpp: Skeletal mesh import code.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
		* Major cleanups by Daniel Vogel
=============================================================================*/

#include "EditorPrivate.h"
#include "SkelImport.h"

/*-----------------------------------------------------------------------------
	Special importers for skeletal data.
-----------------------------------------------------------------------------*/

//
//	USkeletalMeshFactory::StaticConstructor
//
void USkeletalMeshFactory::StaticConstructor()
{
	SupportedClass = USkeletalMesh::StaticClass();
	new(Formats)FString(TEXT("psk;Skeletal Mesh"));
	bCreateNew = 0;
	new(GetClass(), TEXT("bAssumeMayaCoordinates"), RF_Public) UBoolProperty(CPP_PROPERTY(bAssumeMayaCoordinates), TEXT(""), CPF_Edit);
	new(GetClass()->HideCategories) FName(TEXT("Object"));
}

//
//	USkeletalMeshFactory::USkeletalMeshFactory
//
USkeletalMeshFactory::USkeletalMeshFactory()
{
	bEditorImport			= 1;
	bAssumeMayaCoordinates	= false;
}

IMPLEMENT_COMPARE_CONSTREF( VRawBoneInfluence, UnMeshEd, 
{
	if		( A.VertexIndex > B.VertexIndex	) return  1;
	else if ( A.VertexIndex < B.VertexIndex	) return -1;
	else if ( A.Weight      < B.Weight		) return  1;
	else if ( A.Weight      > B.Weight		) return -1;
	else if ( A.BoneIndex   > B.BoneIndex	) return  1;
	else if ( A.BoneIndex   < B.BoneIndex	) return -1;
	else									  return  0;	
}
)

UObject* USkeletalMeshFactory::FactoryCreateBinary
(
 UClass*			Class,
 UObject*			InParent,
 FName				Name,
 DWORD				Flags,
 UObject*			Context,
 const TCHAR*		Type,
 const BYTE*&		Buffer,
 const BYTE*		BufferEnd,
 FFeedbackContext*	Warn
 )
{
	// Create 'empty' mesh.
	USkeletalMesh* SkeletalMesh = CastChecked<USkeletalMesh>( StaticConstructObject( Class, InParent, Name, Flags ));
	
	// Fill with data from buffer - contains the full .PSK file. 
	
	GWarn->BeginSlowTask( TEXT("Importing Skeletal Mesh File"), 1);

	BYTE*						BufferReadPtr = (BYTE*) Buffer;
	VChunkHeader*				ChunkHeader;

	TArray <VMaterial>			Materials;		// Materials
	TArray <FVector>			Points;			// 3D Points
	TArray <VVertex>			Wedges;			// Wedges
	TArray <VTriangle>			Faces;			// Faces
	TArray <VBone>				RefBonesBinary;	// Reference Skeleton
	TArray <VRawBoneInfluence>	Influences;		// Influences

	// Main dummy header.
	ChunkHeader		= (VChunkHeader*)BufferReadPtr;
	BufferReadPtr  += sizeof(VChunkHeader);
  
	// Read the temp skin structures..
	// 3d points "vpoints" datasize*datacount....
	ChunkHeader		= (VChunkHeader*)BufferReadPtr;
	BufferReadPtr  += sizeof(VChunkHeader);
	Points.Add(ChunkHeader->DataCount);
	appMemcpy( &Points(0), BufferReadPtr, sizeof(VPoint) * ChunkHeader->DataCount );
	BufferReadPtr  += sizeof(VPoint) * ChunkHeader->DataCount;

	//  Wedges (VVertex)	
	ChunkHeader		= (VChunkHeader*)BufferReadPtr;
	BufferReadPtr  += sizeof(VChunkHeader);
	Wedges.Add(ChunkHeader->DataCount);
	appMemcpy( &Wedges(0), BufferReadPtr, sizeof(VVertex) * ChunkHeader->DataCount );
	BufferReadPtr  += sizeof(VVertex) * ChunkHeader->DataCount;

	// Faces (VTriangle)
	ChunkHeader		= (VChunkHeader*)BufferReadPtr;
	BufferReadPtr  += sizeof(VChunkHeader);
	Faces.Add(ChunkHeader->DataCount);
	appMemcpy( &Faces(0), BufferReadPtr, sizeof(VTriangle) * ChunkHeader->DataCount );
	BufferReadPtr  += sizeof(VTriangle) * ChunkHeader->DataCount;

	// Materials (VMaterial)
	ChunkHeader		= (VChunkHeader*)BufferReadPtr;
	BufferReadPtr  += sizeof(VChunkHeader);
	Materials.Add(ChunkHeader->DataCount);
	appMemcpy( &Materials(0), BufferReadPtr, sizeof(VMaterial) * ChunkHeader->DataCount );
	BufferReadPtr  += sizeof(VMaterial) * ChunkHeader->DataCount;

	// Reference skeleton (VBones)
	ChunkHeader		= (VChunkHeader*)BufferReadPtr;
	BufferReadPtr  += sizeof(VChunkHeader);
	RefBonesBinary.Add(ChunkHeader->DataCount);
	appMemcpy( &RefBonesBinary(0), BufferReadPtr, sizeof(VBone) * ChunkHeader->DataCount );
	BufferReadPtr  += sizeof(VBone) * ChunkHeader->DataCount;

	// Raw bone influences (VRawBoneInfluence)
	ChunkHeader		= (VChunkHeader*)BufferReadPtr;
	BufferReadPtr  += sizeof(VChunkHeader);
	Influences.Add(ChunkHeader->DataCount);
	appMemcpy( &Influences(0), BufferReadPtr, sizeof(VRawBoneInfluence) * ChunkHeader->DataCount );
	BufferReadPtr  += sizeof(VRawBoneInfluence) * ChunkHeader->DataCount;

	// Y-flip quaternions and translations from Max/Maya/etc space into Unreal space.
	for( INT b=0; b<RefBonesBinary.Num(); b++)
	{
		FQuat Bone = RefBonesBinary(b).BonePos.Orientation;
		Bone.Y = - Bone.Y;
		// W inversion only needed on the parent - since PACKAGE_FILE_VERSION 133.
		// @todo - clean flip out of/into exporters
		if( b==0 ) 
			Bone.W = - Bone.W; 
		RefBonesBinary(b).BonePos.Orientation = Bone;

		FVector Pos = RefBonesBinary(b).BonePos.Position;
		Pos.Y = - Pos.Y;
		RefBonesBinary(b).BonePos.Position = Pos;
	}

	// Y-flip skin, and adjust handedness
	for( INT p=0; p<Points.Num(); p++ )
	{
		Points(p).Y *= -1;
	}
	for( INT f=0; f<Faces.Num(); f++)
	{
		Exchange( Faces(f).WedgeIndex[1], Faces(f).WedgeIndex[2] );
	}	

	if( bAssumeMayaCoordinates )
	{
		SkeletalMesh->RotOrigin = FRotator(0, -16384, 16384);		
	}

	// If direct linkup of materials is requested, try to find them here - to get a texture name from a 
	// material name, cut off anything in front of the dot (beyond are special flags).
	SkeletalMesh->Materials.Empty();
	for( INT m=0; m< Materials.Num(); m++)
	{			
		TCHAR MaterialName[128];
		appStrcpy( MaterialName, ANSI_TO_TCHAR( Materials(m).MaterialName ) );

		// Terminate string at the dot, or at any double underscore (Maya doesn't allow 
		// anything but underscores in a material name..) Beyond that, the materialname 
		// had tags that are now already interpreted by the exporter to go into flags
		// or order the materials for the .PSK refrence skeleton/skin output.
		TCHAR* TagsCutoff = appStrstr( MaterialName , TEXT(".") );
		if(  !TagsCutoff )
			TagsCutoff = appStrstr( MaterialName, TEXT("__"));
		if( TagsCutoff ) 
			*TagsCutoff = 0; 

		UMaterialInstance* Material = FindObject<UMaterialInstance>( ANY_PACKAGE, MaterialName );
		SkeletalMesh->Materials.AddItem(Material);
		Materials(m).TextureIndex = m; // Force 'skin' index to point to the exact named material.

		if( Material )
		{
			debugf(TEXT(" Found texture for material %i: [%s] skin index: %i "), m, Material->GetName(), Materials(m).TextureIndex );
		}
		else
		{
			debugf(TEXT(" Mesh material not found among currently loaded ones: %s"), MaterialName );
		}
	}

	// Pad the material pointers.
	while( Materials.Num() > SkeletalMesh->Materials.Num() )
		SkeletalMesh->Materials.AddItem( NULL );

	// display summary info
	debugf(NAME_Log,TEXT(" * Skeletal skin VPoints            : %i"),Points.Num()			);
	debugf(NAME_Log,TEXT(" * Skeletal skin VVertices          : %i"),Wedges.Num()			);
	debugf(NAME_Log,TEXT(" * Skeletal skin VTriangles         : %i"),Faces.Num()			);
	debugf(NAME_Log,TEXT(" * Skeletal skin VMaterials         : %i"),Materials.Num()		);
	debugf(NAME_Log,TEXT(" * Skeletal skin VBones             : %i"),RefBonesBinary.Num()	);
	debugf(NAME_Log,TEXT(" * Skeletal skin VRawBoneInfluences : %i"),Influences.Num()		);

	// Necessary: Fixup face material index from wedge 0 as faces don't always have the proper material index (exporter's task).
	for( INT i=0; i<Faces.Num(); i++)
	{
		Faces(i).MatIndex		= Wedges( Faces(i).WedgeIndex[0] ).MatIndex;
		Faces(i).AuxMatIndex	= 0;
	}

	// Setup skeletal hierarchy + names structure.
	SkeletalMesh->RefSkeleton.Empty( RefBonesBinary.Num() );
	SkeletalMesh->RefSkeleton.AddZeroed( RefBonesBinary.Num() );

	// Digest bones to the serializable format.
    for( INT b=0; b<RefBonesBinary.Num(); b++ )
	{
		appTrimSpaces( &RefBonesBinary(b).Name[0] );

		FMeshBone& Bone = SkeletalMesh->RefSkeleton(b);
		
		Bone.Flags					= 0;
		Bone.BonePos.Position		= RefBonesBinary(b).BonePos.Position;     // FVector - Origin of bone relative to parent, or root-origin.
		Bone.BonePos.Orientation	= RefBonesBinary(b).BonePos.Orientation;  // FQuat - orientation of bone in parent's Trans.
		Bone.BonePos.Length			= 0;
		Bone.BonePos.XSize			= 0;
		Bone.BonePos.YSize			= 0;
		Bone.BonePos.ZSize			= 0;
		Bone.NumChildren			= RefBonesBinary(b).NumChildren;
		Bone.ParentIndex			= RefBonesBinary(b).ParentIndex;		
		Bone.Name					= FName( ANSI_TO_TCHAR(&RefBonesBinary(b).Name[0]) );		
	}

	// Add hierarchy index to each bone and detect max depth.
	SkeletalMesh->SkeletalDepth = 0;
	for( INT b=0; b<SkeletalMesh->RefSkeleton.Num(); b++ )
	{
		INT Parent	= SkeletalMesh->RefSkeleton(b).ParentIndex;
		INT Depth	= 1.0f;
		
		SkeletalMesh->RefSkeleton(b).Depth	= 1.0f;
		if( Parent != b )
		{
			Depth += SkeletalMesh->RefSkeleton(Parent).Depth;
		}
		if( SkeletalMesh->SkeletalDepth < Depth )
		{
			SkeletalMesh->SkeletalDepth = Depth;
		}

		SkeletalMesh->RefSkeleton(b).Depth = Depth;
	}
	debugf( TEXT("Bones digested - %i  Depth of hierarchy - %i"), SkeletalMesh->RefSkeleton.Num(), SkeletalMesh->SkeletalDepth );

	// Sort influences by vertex index.
	Sort<USE_COMPARE_CONSTREF(VRawBoneInfluence,UnMeshEd)>( &Influences(0), Influences.Num() );

	// Remove more than allowed number of weights by removing least important influences (setting them to 0). 
	// Relies on influences sorted by vertex index and weight and the code actually removing the influences below.
	INT LastVertexIndex		= INDEX_NONE;
	INT InfluenceCount		= 0;
	for(  INT i=0; i<Influences.Num(); i++ )
	{		
		if( ( LastVertexIndex != Influences(i).VertexIndex ) )
		{
			InfluenceCount	= 0;
			LastVertexIndex	= Influences(i).VertexIndex;
		}

		InfluenceCount++;

		if( InfluenceCount > 4 || LastVertexIndex >= Points.Num() )
		{
			Influences(i).Weight = 0.f;
		}
	}

	// Remove influences below a certain threshold.
	INT RemovedInfluences	= 0;
	const FLOAT MINWEIGHT	= 0.01f; // 1%
	for( INT i=Influences.Num()-1; i>=0; i-- )
	{
		if( Influences(i).Weight < MINWEIGHT )
		{
			Influences.Remove(i);
			RemovedInfluences++;
		}
	}
	
	// Renormalize influence weights.
	INT	LastInfluenceCount	= 0;
	InfluenceCount			= 0;
	LastVertexIndex			= INDEX_NONE;
	FLOAT TotalWeight		= 0.f;
	for( INT i=0; i<Influences.Num(); i++ )
	{
		if( LastVertexIndex != Influences(i).VertexIndex )
		{
			LastInfluenceCount	= InfluenceCount;
			InfluenceCount		= 0;
			
			// Normalize the last set of influences.
			if( LastInfluenceCount && (TotalWeight != 1.0f) )
			{				
				FLOAT OneOverTotalWeight = 1.f / TotalWeight;
				for( int r=0; r<LastInfluenceCount; r++)
				{
					Influences(i-r-1).Weight *= OneOverTotalWeight;
				}
			}
			TotalWeight		= 0.f;				
			LastVertexIndex = Influences(i).VertexIndex;							
		}
		InfluenceCount++;
		TotalWeight	+= Influences(i).Weight;			
	}

	// Ensure that each vertex has at least one influence as e.g. CreateSkinningStream relies on it.
	// The below code relies on influences being sorted by vertex index.
	LastVertexIndex = -1;
	InfluenceCount	= 0;
	for( INT i=0; i<Influences.Num(); i++ )
	{
		INT CurrentVertexIndex = Influences(i).VertexIndex;

		if( LastVertexIndex != CurrentVertexIndex )
		{
			for( INT j=LastVertexIndex+1; j<CurrentVertexIndex; j++ )
			{
				// Add a 0-bone weight if none other present (known to happen with certain MAX skeletal setups).
				Influences.Insert(i,1);
				Influences(i).VertexIndex	= j;
				Influences(i).BoneIndex		= 0;
				Influences(i).Weight		= 1.f;
			}
			LastVertexIndex = CurrentVertexIndex;
		}
	}

	//@todo: allow importing different LOD levels.
	SkeletalMesh->LODModels.Empty();
	new(SkeletalMesh->LODModels)FStaticLODModel();

	FStaticLODModel& LODModel = SkeletalMesh->LODModels(0); 

	// Copy vertex data to static LOD level.
	LODModel.Points.Add( Points.Num() );
	LODModel.Points.Detach();
	for( INT p=0; p<Points.Num(); p++ )
	{
		LODModel.Points(p) = Points(p);
	}

	// Copy wedge information to static LOD level.
	LODModel.Wedges.Add( Wedges.Num() );
	LODModel.Wedges.Detach();
	for( INT w=0; w<Wedges.Num(); w++ )
	{
		LODModel.Wedges(w).iVertex	= Wedges(w).VertexIndex;
		LODModel.Wedges(w).U		= Wedges(w).U;
		LODModel.Wedges(w).V		= Wedges(w).V;
	}

	// Copy triangle/ face data to static LOD level.
	LODModel.Faces.Add( Faces.Num() );
	LODModel.Faces.Detach();
	for( INT f=0; f<Faces.Num(); f++)
	{
		FMeshFace Face;
		Face.iWedge[0]			= Faces(f).WedgeIndex[0];
		Face.iWedge[1]			= Faces(f).WedgeIndex[1];
		Face.iWedge[2]			= Faces(f).WedgeIndex[2];
		Face.MeshMaterialIndex	= Faces(f).MatIndex;
		
		LODModel.Faces(f)		= Face;
	}			

	// Copy weights/ influences to static LOD level.
	LODModel.Influences.Add( Influences.Num() );
	LODModel.Influences.Detach();
	for( INT i=0; i<Influences.Num(); i++ )
	{
		LODModel.Influences(i).Weight		= Influences(i).Weight;
		LODModel.Influences(i).VertIndex	= Influences(i).VertexIndex;
		LODModel.Influences(i).BoneIndex	= Influences(i).BoneIndex;
	}

	// Create initial bounding box based on expanded version of reference pose for meshes without physics assets. Can be overridden by artist.
	FBox BoundingBox( &LODModel.Points(0), LODModel.Points.Num() );
	FBox Temp = BoundingBox;
	FVector MidMesh		= 0.5f*(Temp.Min + Temp.Max);
	BoundingBox.Min		= Temp.Min + 1.0f*(Temp.Min - MidMesh);
    BoundingBox.Max		= Temp.Max + 1.0f*(Temp.Max - MidMesh);
	// Tuck up the bottom as this rarely extends lower than a reference pose's (e.g. having its feet on the floor).
	BoundingBox.Min.Z	= Temp.Min.Z + 0.1f*(Temp.Min.Z - MidMesh.Z);
	SkeletalMesh->Bounds= FBoxSphereBounds(BoundingBox);

	// Create actual rendering data.
	SkeletalMesh->CreateSkinningStreams();

	SkeletalMesh->PostLoad();
	SkeletalMesh->MarkPackageDirty();

	GWarn->EndSlowTask();
	return SkeletalMesh;
}

IMPLEMENT_CLASS(USkeletalMeshFactory);


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
