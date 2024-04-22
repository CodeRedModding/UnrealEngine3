/*=============================================================================
	UnMeshEd.cpp: Skeletal mesh import code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "Factories.h"
#include "SkelImport.h"
#include "EngineAnimClasses.h"
#include "UnSkeletalMeshSorting.h"

/*-----------------------------------------------------------------------------
	Special importers for skeletal data.
-----------------------------------------------------------------------------*/

//
//	USkeletalMeshFactory::StaticConstructor
//
void USkeletalMeshFactory::StaticConstructor()
{
	new(GetClass(), TEXT("bAssumeMayaCoordinates"), RF_Public) UBoolProperty(CPP_PROPERTY(bAssumeMayaCoordinates), TEXT(""), CPF_Edit);
	new(GetClass()->HideCategories) FName(NAME_Object);
}

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void USkeletalMeshFactory::InitializeIntrinsicPropertyValues()
{
	SupportedClass = USkeletalMesh::StaticClass();
#if WITH_ACTORX
	new(Formats)FString(TEXT("psk;Skeletal Mesh"));
#endif
	bCreateNew = 0;
}
//
//	USkeletalMeshFactory::USkeletalMeshFactory
//
USkeletalMeshFactory::USkeletalMeshFactory(FSkelMeshOptionalImportData* InOptionalImportData)
{
	OptionalImportData		= InOptionalImportData;
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

static INT FindBoneIndex( const TArray<FMeshBone>& RefSkeleton, FName BoneName )
{
	for(INT i=0; i<RefSkeleton.Num(); i++)
	{
		if(RefSkeleton(i).Name == BoneName)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

/** Check that root bone is the same, and that any bones that are common have the correct parent. */
UBOOL SkeletonsAreCompatible( const TArray<FMeshBone>& NewSkel, const TArray<FMeshBone>& ExistSkel )
{
	if(NewSkel(0).Name != ExistSkel(0).Name)
	{
		appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("MeshHasDifferentRoot"), *NewSkel(0).Name.ToString(), *ExistSkel(0).Name.ToString()) );
		return false;
	}

	for(INT i=1; i<NewSkel.Num(); i++)
	{
		// See if bone is in both skeletons.
		INT NewBoneIndex = i;
		FName NewBoneName = NewSkel(NewBoneIndex).Name;
		INT BBoneIndex = FindBoneIndex(ExistSkel, NewBoneName);

		// If it is, check parents are the same.
		if(BBoneIndex != INDEX_NONE)
		{
			FName NewParentName = NewSkel( NewSkel(NewBoneIndex).ParentIndex ).Name;
			FName ExistParentName = ExistSkel( ExistSkel(BBoneIndex).ParentIndex ).Name;

			if(NewParentName != ExistParentName)
			{
				appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("MeshHasDifferentRoot"), *NewBoneName.ToString(), *NewParentName.ToString(), *ExistParentName.ToString()) );
				return false;
			}
		}
	}

	return true;
}

/**
* Takes an imported bone name, removes any leading or trailing spaces, and converts the remaining spaces to dashes.
*/
FString FSkeletalMeshBinaryImport::FixupBoneName( ANSICHAR *AnisBoneName )
{
	FString BoneName = AnisBoneName;

	BoneName.Trim();
	BoneName.TrimTrailing();
	BoneName = BoneName.Replace( TEXT( " " ), TEXT( "-" ) );
	
	return( BoneName );
}

/**
* Helper function for filling in the various FSkeletalMeshBinaryImport values from buffer of data
* 
* @param DestArray - destination array of data to allocate and copy to
* @param BufferReadPtr - source data buffer to read from
* @param BufferEnd - end of data buffer
*/
template<typename DataType>
static void CopyMeshDataAndAdvance( TArray<DataType>& DestArray, BYTE*& BufferReadPtr, const BYTE* BufferEnd )
{
	// assume that BufferReadPtr is positioned at next chunk header 
	const VChunkHeader* ChunkHeader = (const VChunkHeader*) BufferReadPtr;
	// advance buffer ptr to data block
	BufferReadPtr += sizeof(VChunkHeader);	
	// make sure we don't overrun the buffer when reading data
	check((sizeof(DataType) * ChunkHeader->DataCount + BufferReadPtr) <= BufferEnd);
	// allocate space in import data
	DestArray.Add( ChunkHeader->DataCount );	
	// copy from buffer
	appMemcpy( &DestArray(0), BufferReadPtr, sizeof(DataType) * ChunkHeader->DataCount );
	// advance buffer
	BufferReadPtr  += sizeof(DataType) * ChunkHeader->DataCount;		
};

// Raw data bone.
struct VBoneNoAlign
{
	ANSICHAR    Name[64];     //
	DWORD		Flags;        // reserved / 0x02 = bone where skin is to be attached...	
	INT 		NumChildren;  // children  // only needed in animation ?
	INT         ParentIndex;  // 0/NULL if this is the root bone.  
	VJointPosNoAlign	BonePos;      // reference position
};

/** This is due to keep the alignment working with ActorX format 
* FQuat in ActorX isn't 16 bit aligned, so when serialize using sizeof, this does not match up, 
*/
void CopyVBone(VBone& Dest, const VBoneNoAlign & Src)
{
	appMemcpy(Dest.Name, Src.Name, sizeof(ANSICHAR)*64);
	appMemcpy(&Dest.Flags, &Src.Flags, sizeof(DWORD)+sizeof(INT)+sizeof(INT));
	Dest.BonePos.Orientation = FQuat(Src.BonePos.Orientation.X, Src.BonePos.Orientation.Y, Src.BonePos.Orientation.Z, Src.BonePos.Orientation.W);
	appMemcpy(&Dest.BonePos.Position, &Src.BonePos.Position, sizeof(FVector)+sizeof(FLOAT)*4);
}

/**
 * Import struct for PSK files.  
 * This omits vertex color data to maintain backwards compatibility with other versions of ActorX which write out vertices in this format
 */
struct VVertexImport
{
	WORD	VertexIndex; // Index to a vertex.
	FLOAT   U,V;         // Scaled to BYTES, rather...-> Done in digestion phase, on-disk size doesn't matter here.
	BYTE    MatIndex;    // At runtime, this one will be implied by the face that's pointing to us.
	BYTE    Reserved;    // Reserved
};

/**
 * Import struct for PSK files.
 * Does not include tangent data that is in VTriangle as PSK files do not export tangent data.
 * This is used to maintain backwards compatability with PSK files.  Do not add anything do this struct unless it exists in the PSK VTriangle that is exported.
 */
struct VTriangleImport
{
	WORD   WedgeIndex[3];	 // Point to three vertices in the vertex list.
	BYTE    MatIndex;	     // Materials can be anything.
	BYTE    AuxMatIndex;     // Second material from exporter (unused)
	DWORD   SmoothingGroups; // 32-bit flag for smoothing groups.
};

/**
* Parse skeletal mesh (psk file) from buffer into raw import data
* 
* @param BufferReadPtr	- start of data to be read
* @param BufferEnd - end of data to be read
* @param bShowSummary - if TRUE then print a summary of what was read to file
*/
void FSkeletalMeshBinaryImport::ImportFromFile( BYTE* BufferReadPtr, const BYTE* BufferEnd, UBOOL bShowSummary )
{
	check(BufferReadPtr);

	// Skip passed main dummy header. 
	BufferReadPtr  += sizeof(VChunkHeader);
  
	// Read the temp skin structures..
	// 3d points "vpoints" datasize*datacount....
	CopyMeshDataAndAdvance<FVector>( Points, BufferReadPtr, BufferEnd );

	//  Wedges (VVertex)	
	TArray<VVertexImport> ImportWedges;
	CopyMeshDataAndAdvance<VVertexImport>( ImportWedges, BufferReadPtr, BufferEnd );

	// Faces (VTriangle)
	TArray<VTriangleImport> ImportTriangles;
	CopyMeshDataAndAdvance<VTriangleImport>( ImportTriangles, BufferReadPtr, BufferEnd );

	// Materials (VMaterial)
	CopyMeshDataAndAdvance<VMaterial>( Materials, BufferReadPtr, BufferEnd );

	/** This is due to keep the alignment working with ActorX format 
	* FQuat in ActorX isn't 16 bit aligned, so when serialize using sizeof, this does not match up, 
	*/
	// A bone: an orientation, and a position, all relative to their parent.
	TArray <VBoneNoAlign>				RefBonesBinaryNoAlign;	// Reference Skeleton

	// Reference skeleton (VBones)
	CopyMeshDataAndAdvance<VBoneNoAlign>( RefBonesBinaryNoAlign, BufferReadPtr, BufferEnd );

	// Now Assign to the real one
	RefBonesBinary.Empty(RefBonesBinaryNoAlign.Num());
	RefBonesBinary.Add(RefBonesBinaryNoAlign.Num());

	// Raw bone influences (VRawBoneInfluence)
	CopyMeshDataAndAdvance<VRawBoneInfluence>( Influences, BufferReadPtr, BufferEnd );
	
	// Y-flip quaternions and translations from Max/Maya/etc space into Unreal space.
	for( INT b=0; b<RefBonesBinary.Num(); b++)
	{
		// Before anything, copy from noalign to real one
		CopyVBone(RefBonesBinary(b), RefBonesBinaryNoAlign(b));

		FQuat Bone = RefBonesBinary(b).BonePos.Orientation;
		Bone.Y = - Bone.Y;
		// W inversion only needed on the parent - since PACKAGE_FILE_VERSION 133.
		// @todo - clean flip out of/into exporters
		if( b==0 ) 
		{
			Bone.W = - Bone.W; 
		}
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

	
	Faces.Empty();
	Faces.Add( ImportTriangles.Num() );
	for( INT f=0; f<Faces.Num(); f++)
	{
		// Generate the actual faces from import data
		VTriangleImport& ImportTri = ImportTriangles(f);
		VTriangle& Tri = Faces(f);
		appMemzero( &Tri, sizeof(VTriangle) );
		
		for( INT I = 0; I < 3; ++I )
		{
			Tri.WedgeIndex[I] = ImportTri.WedgeIndex[I];
		}
		Tri.MatIndex = ImportTri.MatIndex;
		Tri.AuxMatIndex = ImportTri.AuxMatIndex;
		Tri.SmoothingGroups = ImportTri.SmoothingGroups;
		
		Exchange( Faces(f).WedgeIndex[1], Faces(f).WedgeIndex[2] );
	}	

	// Necessary: Fixup face material index from wedge 0 as faces don't always have the proper material index (exporter's task).
	for( INT i=0; i<Faces.Num(); i++)
	{
		Faces(i).MatIndex		= ImportWedges( Faces(i).WedgeIndex[0] ).MatIndex;
		Faces(i).AuxMatIndex	= 0;
	}

	// Try and read vertex color info
	TArray<FColor> VertColors;
	bHasVertexColors = TRUE;
	if( BufferReadPtr == BufferEnd )
	{
		// The pointer is at the end of the file which means there is no vertex color info.  
		// This must be an older version of ActorX
		bHasVertexColors = FALSE;
	}
	else
	{
		// If the read pointer isnt null, check and see if the next header is vertex colors, if so, read them in.
		const VChunkHeader* ChunkHeader = (const VChunkHeader*) BufferReadPtr;
		FString ID = ChunkHeader->ChunkID;
		if( ID == TEXT("VERTEXCOLOR") )
		{
			CopyMeshDataAndAdvance<FColor>( VertColors, BufferReadPtr, BufferEnd );
		}
		else
		{
			bHasVertexColors = FALSE;
		}
	}

	// Try and read extra uv info
	// only read MAX_TEXCOORDS-1 uv sets as the first one is always read
	TArray<FVector2D> UVCoords[MAX_TEXCOORDS-1];
	INT NumExtraUVCoords = 0;
	while( BufferReadPtr != BufferEnd )
	{
		// Since vertex colors should always be at the end of the file, if the read pointer is not at the end of the file
		// we can assume there is color data
		CopyMeshDataAndAdvance<FVector2D>( UVCoords[NumExtraUVCoords], BufferReadPtr, BufferEnd );
		++NumExtraUVCoords;
	}

	// The number of texture coord sets is always one for the first uv set which is always exported plus an extra uv coords in the file.
	NumTexCoords = 1 + NumExtraUVCoords;

	// Default vertex color
	const FColor White(255,255,255);

	// Transfer color data and import vertex data to normal VVertex struct for data manipulation later.
	Wedges.Add( ImportWedges.Num() );
	UBOOL bAllColorsWhite = TRUE;

	for( INT WedgeIdx = 0; WedgeIdx < ImportWedges.Num(); ++WedgeIdx )
	{
		const VVertexImport& ImportWedge = ImportWedges( WedgeIdx );
		VVertex& NewWedge = Wedges( WedgeIdx );

		// if the file didnt have vertex colors just set the color to white.
		NewWedge.Color = bHasVertexColors ? VertColors( WedgeIdx ) : White;

		// Check and see if all vertex colors are white.  If they are, this mesh has no useful vertex colors.
		if( bHasVertexColors && NewWedge.Color != White )
		{
			bAllColorsWhite = FALSE;
		}

		NewWedge.MatIndex = ImportWedge.MatIndex;
		NewWedge.Reserved = ImportWedge.Reserved;
		// Copy the first UV set which is always read from the file.
		NewWedge.UVs[0].X = ImportWedge.U;
		NewWedge.UVs[0].Y = ImportWedge.V;
		NewWedge.VertexIndex = ImportWedge.VertexIndex;

		// Get all extra UV coordinates.
		for( INT UVIndex = 0; UVIndex < NumExtraUVCoords; ++UVIndex )
		{
			NewWedge.UVs[UVIndex+1] = UVCoords[UVIndex](WedgeIdx);
		}

	}
	
	// If we imported vertex colors from the file but they are all white, treat this as there are no vertex colors
	// a default vertex buffer can be used if all vertex colors are white.
	if( bHasVertexColors && bAllColorsWhite )
	{
		bHasVertexColors = FALSE;
	}

	if( bShowSummary )
	{
		// display summary info
		debugf(NAME_Log,TEXT(" * Skeletal skin VPoints            : %i"),Points.Num()			);
		debugf(NAME_Log,TEXT(" * Skeletal skin VVertices          : %i"),Wedges.Num()			);
		debugf(NAME_Log,TEXT(" * Skeletal skin VTriangles         : %i"),Faces.Num()			);
		debugf(NAME_Log,TEXT(" * Skeletal skin VMaterials         : %i"),Materials.Num()		);
		debugf(NAME_Log,TEXT(" * Skeletal skin VBones             : %i"),RefBonesBinary.Num()	);
		debugf(NAME_Log,TEXT(" * Skeletal skin VRawBoneInfluences : %i"),Influences.Num()		);
		debugf(NAME_Log,TEXT(" * Skeletal skin VertexColors		  : %i"),VertColors.Num()		);
		debugf(NAME_Log,TEXT(" * Skeletal skin Num Tex Coords	  : %i"),NumTexCoords			);
	}
}

/**
* Copy mesh data for importing a single LOD
*
* @param LODPoints - vertex data.
* @param LODWedges - wedge information to static LOD level.
* @param LODFaces - triangle/ face data to static LOD level.
* @param LODInfluences - weights/ influences to static LOD level.
*/ 
void FSkeletalMeshBinaryImport::CopyLODImportData( 
					   TArray<FVector>& LODPoints, 
					   TArray<FMeshWedge>& LODWedges,
					   TArray<FMeshFace>& LODFaces,	
					   TArray<FVertInfluence>& LODInfluences )
{
	// Copy vertex data.
	LODPoints.Empty( Points.Num() );
	LODPoints.Add( Points.Num() );
	for( INT p=0; p < Points.Num(); p++ )
	{
		LODPoints(p) = Points(p);
	}

	// Copy wedge information to static LOD level.
	LODWedges.Empty( Wedges.Num() );
	LODWedges.Add( Wedges.Num() );
	for( INT w=0; w < Wedges.Num(); w++ )
	{
		LODWedges(w).iVertex	= Wedges(w).VertexIndex;
		// Copy all texture coordinates
		appMemcpy( LODWedges(w).UVs, Wedges(w).UVs, sizeof(FVector2D) * MAX_TEXCOORDS );
		LODWedges(w).Color	= Wedges( w ).Color;
		
	}

	// Copy triangle/ face data to static LOD level.
	LODFaces.Empty( Faces.Num() );
	LODFaces.Add( Faces.Num() );
	for( INT f=0; f < Faces.Num(); f++)
	{
		FMeshFace Face;
		Face.iWedge[0]			= Faces(f).WedgeIndex[0];
		Face.iWedge[1]			= Faces(f).WedgeIndex[1];
		Face.iWedge[2]			= Faces(f).WedgeIndex[2];
		Face.MeshMaterialIndex	= Faces(f).MatIndex;

        Face.TangentX[0]		= Faces(f).TangentX[0];
        Face.TangentX[1]		= Faces(f).TangentX[1];
        Face.TangentX[2]		= Faces(f).TangentX[2];

        Face.TangentY[0]		= Faces(f).TangentY[0];
        Face.TangentY[1]		= Faces(f).TangentY[1];
        Face.TangentY[2]		= Faces(f).TangentY[2];

        Face.TangentZ[0]		= Faces(f).TangentZ[0];
        Face.TangentZ[1]		= Faces(f).TangentZ[1];
        Face.TangentZ[2]		= Faces(f).TangentZ[2];

        Face.bOverrideTangentBasis = Faces(f).bOverrideTangentBasis;

		LODFaces(f) = Face;
	}			

	// Copy weights/ influences to static LOD level.
	LODInfluences.Empty( Influences.Num() );
	LODInfluences.Add( Influences.Num() );
	for( INT i=0; i < Influences.Num(); i++ )
	{
		LODInfluences(i).Weight		= Influences(i).Weight;
		LODInfluences(i).VertIndex	= Influences(i).VertexIndex;
		LODInfluences(i).BoneIndex	= Influences(i).BoneIndex;
	}
}

/**
* Process and fill in the mesh Materials using the raw binary import data
* 
* @param Materials - [out] array of materials to update
* @param SkelMeshImporter - raw binary import data to process
*/
void ProcessImportMeshMaterials(TArray<UMaterialInterface*>& Materials, FSkeletalMeshBinaryImport& SkelMeshImporter, UBOOL TruncateTags)
{
	TArray <VMaterial>&	MaterialsBinary = SkelMeshImporter.Materials;

	// If direct linkup of materials is requested, try to find them here - to get a texture name from a 
	// material name, cut off anything in front of the dot (beyond are special flags).
	Materials.Empty();
	for( INT m=0; m < MaterialsBinary.Num(); m++)
	{			
		TCHAR MaterialName[128];
		appStrcpy( MaterialName, ANSI_TO_TCHAR( MaterialsBinary(m).MaterialName ) );

		// Terminate string at the dot, or at any double underscore (Maya doesn't allow 
		// anything but underscores in a material name..) Beyond that, the materialname 
		// had tags that are now already interpreted by the exporter to go into flags
		// or order the materials for the .PSK refrence skeleton/skin output.
		if (TruncateTags)
		{
			TCHAR* TagsCutoff = appStrstr( MaterialName , TEXT(".") );
			if(  !TagsCutoff )
			{
				TagsCutoff = appStrstr( MaterialName, TEXT("__"));
			}
			if( TagsCutoff ) 
			{
				*TagsCutoff = 0; 
			}
		}

		UMaterialInterface* Material = FindObject<UMaterialInterface>( ANY_PACKAGE, MaterialName );
		Materials.AddItem(Material);
		MaterialsBinary(m).TextureIndex = m; // Force 'skin' index to point to the exact named material.

		if( Material )
		{
			debugf(TEXT(" Found texture for material %i: [%s] skin index: %i "), m, *Material->GetName(), MaterialsBinary(m).TextureIndex );
		}
		else
		{
			debugf(TEXT(" Mesh material not found among currently loaded ones: %s"), MaterialName );
		}
	}

	// Pad the material pointers.
	while( MaterialsBinary.Num() > Materials.Num() )
	{
		Materials.AddItem( NULL );
	}
}

/**
* Process and fill in the mesh ref skeleton bone hierarchy using the raw binary import data
* 
* @param RefSkeleton - [out] reference skeleton hierarchy to update
* @param SkeletalDepth - [out] depth of the reference skeleton hierarchy
* @param SkelMeshImporter - raw binary import data to process
*/
void ProcessImportMeshSkeleton(TArray<FMeshBone>& RefSkeleton, INT& SkeletalDepth, FSkeletalMeshBinaryImport& SkelMeshImporter)
{
	TArray <VBone>&	RefBonesBinary = SkelMeshImporter.RefBonesBinary;

	// Setup skeletal hierarchy + names structure.
	RefSkeleton.Empty( RefBonesBinary.Num() );
	RefSkeleton.AddZeroed( RefBonesBinary.Num() );

	// Digest bones to the serializable format.
	for( INT b=0; b<RefBonesBinary.Num(); b++ )
	{
		FMeshBone& Bone = RefSkeleton( b );
		VBone& BinaryBone = RefBonesBinary( b );

		Bone.Flags					= 0;
		Bone.BonePos.Position		= BinaryBone.BonePos.Position;     // FVector - Origin of bone relative to parent, or root-origin.
		Bone.BonePos.Orientation	= BinaryBone.BonePos.Orientation;  // FQuat - orientation of bone in parent's Trans.
		Bone.NumChildren			= BinaryBone.NumChildren;
		Bone.ParentIndex			= BinaryBone.ParentIndex;		
		Bone.BoneColor				= FColor(255,255,255);

		FString BoneName = FSkeletalMeshBinaryImport::FixupBoneName( BinaryBone.Name );
		Bone.Name = FName( *BoneName, FNAME_Add, TRUE );
	}

	// Add hierarchy index to each bone and detect max depth.
	SkeletalDepth = 0;
	for( INT b=0; b < RefSkeleton.Num(); b++ )
	{
		INT Parent	= RefSkeleton(b).ParentIndex;
		INT Depth	= 1.0f;

		RefSkeleton(b).Depth	= 1.0f;
		if( Parent != b )
		{
			Depth += RefSkeleton(Parent).Depth;
		}
		if( SkeletalDepth < Depth )
		{
			SkeletalDepth = Depth;
		}
		RefSkeleton(b).Depth = Depth;
	}
}

/**
* Process and update the vertex Influences using the raw binary import data
* 
* @param SkelMeshImporter - raw binary import data to process
*/
void ProcessImportMeshInfluences(FSkeletalMeshBinaryImport& SkelMeshImporter)
{
	TArray <FVector>& Points = SkelMeshImporter.Points;
	TArray <VVertex>& Wedges = SkelMeshImporter.Wedges;
	TArray <VRawBoneInfluence>& Influences = SkelMeshImporter.Influences;

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
	if( Influences.Num() == 0 )
	{
		// warn about no influences
		appMsgf( AMT_OK, *LocalizeUnrealEd("WarningNoSkelInfluences") );
		// add one for each wedge entry
		Influences.Add(Wedges.Num());
		for( INT WedgeIdx=0; WedgeIdx<Wedges.Num(); WedgeIdx++ )
		{	
			Influences(WedgeIdx).VertexIndex = WedgeIdx;
			Influences(WedgeIdx).BoneIndex = 0;
			Influences(WedgeIdx).Weight = 1.0f;
		}		
	}
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
}

//
// FSavedCustomSortSectionInfo - saves and restores custom triangle order for a single section of the skeletal mesh.
//
struct FSavedCustomSortSectionInfo
{
	FSavedCustomSortSectionInfo(USkeletalMesh* ExistingSkelMesh, INT LODModelIndex, INT InSectionIdx)
	:	SavedSectionIdx(InSectionIdx)
	{
		FStaticLODModel& LODModel = ExistingSkelMesh->LODModels(LODModelIndex);
		FSkelMeshSection& Section = LODModel.Sections(SavedSectionIdx);

		// Save the sort mode and number of triangles
		SavedSortOption = Section.TriangleSorting;
		SavedNumTriangles = Section.NumTriangles;

		// Save CustomLeftRightAxis and CustomLeftRightBoneName
		FTriangleSortSettings& TriangleSortSettings = ExistingSkelMesh->LODInfo(LODModelIndex).TriangleSortSettings(SavedSectionIdx);
		SavedCustomLeftRightAxis  = TriangleSortSettings.CustomLeftRightAxis;
		SavedCustomLeftRightBoneName = TriangleSortSettings.CustomLeftRightBoneName;

		if( SavedSortOption == TRISORT_Custom || SavedSortOption == TRISORT_CustomLeftRight )
		{
			// Save the vertices
			TArray<FSoftSkinVertex> Vertices;
			LODModel.GetVertices(Vertices);
			SavedVertices.Add(Vertices.Num());
			for( INT i=0;i<Vertices.Num();i++ )
			{
				SavedVertices(i) = Vertices(i).Position;
			}

			// Save the indices
			INT NumIndices = SavedSortOption == TRISORT_CustomLeftRight ?  Section.NumTriangles * 6 :  Section.NumTriangles * 3;
			SavedIndices.Add(NumIndices);

			if( LODModel.MultiSizeIndexContainer.GetDataTypeSize() == sizeof(WORD) )
			{
				// We cant copy indices directly if the source data is 16 bit.
				for( INT Index = 0; Index < NumIndices; ++Index )
				{
					SavedIndices(Index) = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Get( Section.BaseIndex + Index );
				}
			}
			else
			{
				appMemcpy(&SavedIndices(0), LODModel.MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo(Section.BaseIndex), NumIndices*LODModel.MultiSizeIndexContainer.GetDataTypeSize());
			}
		}
	}

	void Restore(USkeletalMesh* NewSkelMesh, INT LODModelIndex, TArray<INT>& UnmatchedSections)
	{
		FStaticLODModel& LODModel = NewSkelMesh->LODModels(LODModelIndex);
		FSkeletalMeshLODInfo& LODInfo = NewSkelMesh->LODInfo(LODModelIndex);

		// Re-order the UnmatchedSections so the old section index from the previous model is tried first
		INT PrevSectionIndex = UnmatchedSections.FindItemIndex(SavedSectionIdx);
		if( PrevSectionIndex != 0 && PrevSectionIndex != INDEX_NONE )
		{
			Exchange( UnmatchedSections(0), UnmatchedSections(PrevSectionIndex) );
		}

		// Find the strips in the old triangle data.
		TArray< TArray<DWORD> > OldStrips[2];
		for( INT IndexCopy=0; IndexCopy < (SavedSortOption==TRISORT_CustomLeftRight ? 2 : 1); IndexCopy++ )
		{
			const DWORD* OldIndices = &SavedIndices((SavedIndices.Num()>>1)*IndexCopy);
			TArray<UINT> OldTriSet;
			GetConnectedTriangleSets( SavedNumTriangles, OldIndices, OldTriSet );

			// Convert to strips
			INT PrevTriSet = MAXINT;
			for( INT TriIndex=0;TriIndex<SavedNumTriangles; TriIndex++ )
			{
				if( OldTriSet(TriIndex) != PrevTriSet )
				{
					OldStrips[IndexCopy].AddZeroed();
					PrevTriSet = OldTriSet(TriIndex);
				}
				OldStrips[IndexCopy](OldStrips[IndexCopy].Num()-1).AddItem(OldIndices[TriIndex*3+0]);
				OldStrips[IndexCopy](OldStrips[IndexCopy].Num()-1).AddItem(OldIndices[TriIndex*3+1]);
				OldStrips[IndexCopy](OldStrips[IndexCopy].Num()-1).AddItem(OldIndices[TriIndex*3+2]);
			}
		}


		UBOOL bFoundMatchingSection = FALSE; 

		// Try all remaining sections to find a match
		for( INT UnmatchedSectionsIdx=0; !bFoundMatchingSection && UnmatchedSectionsIdx<UnmatchedSections.Num(); UnmatchedSectionsIdx++ )
		{
			// Section of the new mesh to try
			INT SectionIndex = UnmatchedSections(UnmatchedSectionsIdx);
			FSkelMeshSection& Section = LODModel.Sections(SectionIndex);

			TArray<DWORD> Indices;
			LODModel.MultiSizeIndexContainer.GetIndexBuffer( Indices );
			const DWORD* NewSectionIndices = Indices.GetData() + Section.BaseIndex;

			// Build the list of triangle sets in the new mesh's section
			TArray<UINT> TriSet;
			GetConnectedTriangleSets( Section.NumTriangles, NewSectionIndices, TriSet );

			// Mapping from triangle set number to the array of indices that make up the contiguous strip.
			TMap<UINT, TArray<DWORD> > NewStripsMap;
			// Go through each triangle and assign it to the appropriate contiguous strip.
			// This is necessary if the strips in the index buffer are not contiguous.
			INT Index=0;
			for( INT s=0;s<TriSet.Num();s++ )
			{
				// Store the indices for this triangle in the appropriate contiguous set.
				TArray<DWORD>* ThisStrip = NewStripsMap.Find(TriSet(s));
				if( !ThisStrip )
				{
					ThisStrip = &NewStripsMap.Set(TriSet(s),TArray<DWORD>());
				}

				// Add the three indices for this triangle.
				ThisStrip->AddItem(NewSectionIndices[Index++]);
				ThisStrip->AddItem(NewSectionIndices[Index++]);
				ThisStrip->AddItem(NewSectionIndices[Index++]);
			}

			// Get the new vertices
			TArray<FSoftSkinVertex> NewVertices;
			LODModel.GetVertices(NewVertices);

			// Do the processing once for each copy if the index data
			for( INT IndexCopy=0; IndexCopy < (SavedSortOption==TRISORT_CustomLeftRight ? 2 : 1); IndexCopy++ )
			{
				// Copy strips in the new mesh's section into an array. We'll remove items from
				// here as we match to the old strips, so we need to keep a new copy of it each time.
				TArray<TArray<DWORD> > NewStrips;
				for( TMap<UINT, TArray<DWORD> >::TIterator It(NewStripsMap); It; ++It )
				{
					NewStrips.AddItem(It.Value());
				}

				// Match up old strips to new
				INT NumMismatchedStrips = 0;
				TArray<TArray<DWORD> > NewSortedStrips; // output
				for( INT OsIdx=0;OsIdx<OldStrips[IndexCopy].Num();OsIdx++ )
				{
					TArray<DWORD>& OldStripIndices = OldStrips[IndexCopy](OsIdx);

					INT MatchingNewStrip = INDEX_NONE;

					for( INT NsIdx=0;NsIdx<NewStrips.Num() && MatchingNewStrip==INDEX_NONE;NsIdx++ )
					{
						// Check if we have the same number of triangles in the old and new strips.
						if( NewStrips(NsIdx).Num() != OldStripIndices.Num() )
						{
							continue;
						}

						// Make a copy of the indices, as we'll remove them as we try to match triangles.
						TArray<DWORD> NewStripIndices = NewStrips(NsIdx);

						// Check if all the triangles in the new strip closely match those in the old.
						for( INT OldTriIdx=0;OldTriIdx<OldStripIndices.Num();OldTriIdx+=3 )
						{
							// Try to find a match for this triangle in the new strip.
							UBOOL FoundMatch = FALSE;
							for( INT NewTriIdx=0;NewTriIdx<NewStripIndices.Num();NewTriIdx+=3 )
							{
								if( (SavedVertices(OldStripIndices(OldTriIdx+0)) - NewVertices(NewStripIndices(NewTriIdx+0)).Position).SizeSquared() < KINDA_SMALL_NUMBER &&
									(SavedVertices(OldStripIndices(OldTriIdx+1)) - NewVertices(NewStripIndices(NewTriIdx+1)).Position).SizeSquared() < KINDA_SMALL_NUMBER &&
									(SavedVertices(OldStripIndices(OldTriIdx+2)) - NewVertices(NewStripIndices(NewTriIdx+2)).Position).SizeSquared() < KINDA_SMALL_NUMBER )
								{
									// Found a triangle match. Remove the triangle from the new list and try to match the next old triangle.
									NewStripIndices.Remove(NewTriIdx,3);
									FoundMatch = TRUE;
									break;
								}
							}

							// If we didn't find a match for this old triangle, the whole strip doesn't match.
							if( !FoundMatch )
							{
								break;
							}
						}

						if( NewStripIndices.Num() == 0 )
						{
							// strip completely matched
							MatchingNewStrip = NsIdx;
						}
					}

					if( MatchingNewStrip != INDEX_NONE )
					{
						NewSortedStrips.AddItem( NewStrips(MatchingNewStrip) );
						NewStrips.Remove(MatchingNewStrip);
					}
					else
					{
						NumMismatchedStrips++;
					}
				}

				if( IndexCopy == 0 )
				{
					if( 100 * NumMismatchedStrips / OldStrips[0].Num() > 50 )
					{
						// If less than 50% of this section's strips match, we assume this is not the correct section.
						break;
					}

					// This section matches!
					bFoundMatchingSection = TRUE;

					// Warn the user if we couldn't match things up.
					if( NumMismatchedStrips )
					{
						appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("RestoreSortingMismatchedStripsForSection"), TriangleSortOptionToString((ETriangleSortOption)SavedSortOption), SavedSectionIdx, NumMismatchedStrips, OldStrips[0].Num()));
					}

					// Restore the settings saved in the LODInfo (used for the UI)
					FTriangleSortSettings& TriangleSortSettings = LODInfo.TriangleSortSettings(SectionIndex);
					TriangleSortSettings.TriangleSorting = SavedSortOption;
					TriangleSortSettings.CustomLeftRightAxis = SavedCustomLeftRightAxis;
					TriangleSortSettings.CustomLeftRightBoneName = SavedCustomLeftRightBoneName;

					// Restore the sorting mode. For TRISORT_CustomLeftRight, this will also make the second copy of the index data.
					LODModel.SortTriangles(NewSkelMesh, SectionIndex, (ETriangleSortOption)SavedSortOption);
				}

				// Append any strips we couldn't match to the end
				NewSortedStrips += NewStrips;

				// Export the strips out to the index buffer in order
				TArray<DWORD> Indices;
				LODModel.MultiSizeIndexContainer.GetIndexBuffer( Indices );
				DWORD* NewIndices = Indices.GetData() + (Section.BaseIndex + Section.NumTriangles*3*IndexCopy);
				for( INT StripIdx=0;StripIdx<NewSortedStrips.Num();StripIdx++ )
				{
					appMemcpy( NewIndices, &NewSortedStrips(StripIdx)(0), NewSortedStrips(StripIdx).Num() * sizeof(DWORD) );

					// Cache-optimize the triangle order inside the final strip
					CacheOptimizeSortStrip( NewIndices, NewSortedStrips(StripIdx).Num() );

					NewIndices += NewSortedStrips(StripIdx).Num();
				}
				LODModel.MultiSizeIndexContainer.CopyIndexBuffer( Indices );
			}
		}

		if( !bFoundMatchingSection )
		{
			appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("FailedRestoreSortingNoSectionMatch"), TriangleSortOptionToString((ETriangleSortOption)SavedSortOption), SavedSectionIdx));
		}
	}

	INT SavedSectionIdx;
	INT SavedNumTriangles;
	BYTE SavedSortOption;
	BYTE SavedCustomLeftRightAxis;
	FName SavedCustomLeftRightBoneName;
	TArray<FVector> SavedVertices;
	TArray<DWORD> SavedIndices;
};


//
// Class to save and restore the custom sorting for all sections not marked TRISORT_None
//
struct FSavedCustomSortInfo
{
	void Save(USkeletalMesh* ExistingSkelMesh, INT LODModelIndex)
	{
		FStaticLODModel& LODModel = ExistingSkelMesh->LODModels(LODModelIndex);

		for( INT SectionIdx=0;SectionIdx < LODModel.Sections.Num(); SectionIdx++ )
		{
			FSkelMeshSection& Section = LODModel.Sections(SectionIdx);
			if( Section.TriangleSorting != TRISORT_None && Section.NumTriangles > 0 )
			{
				new(SortSectionInfos) FSavedCustomSortSectionInfo(ExistingSkelMesh, LODModelIndex, SectionIdx);
			}
		}
	}

	void Restore(USkeletalMesh* NewSkeletalMesh, INT LODModelIndex)
	{
		FStaticLODModel& LODModel = NewSkeletalMesh->LODModels(LODModelIndex);
		FSkeletalMeshLODInfo& LODInfo = NewSkeletalMesh->LODInfo(LODModelIndex);

		// List of sections in the new model yet to be matched to the sorted sections
		TArray<INT> UnmatchedSections;
		for( INT SectionIdx=0;SectionIdx<LODModel.Sections.Num();SectionIdx++ )
		{
			UnmatchedSections.AddItem(SectionIdx);
		}

		for( INT Idx=0;Idx<SortSectionInfos.Num();Idx++ )
		{
			FSavedCustomSortSectionInfo& SortSectionInfo = SortSectionInfos(Idx);

			if( SortSectionInfo.SavedSortOption == TRISORT_Custom || SortSectionInfo.SavedSortOption == TRISORT_CustomLeftRight )
			{
				// Restore saved custom sort order
				SortSectionInfo.Restore(NewSkeletalMesh, LODModelIndex, UnmatchedSections);
			}
			else
			{
				if( !LODModel.Sections.IsValidIndex(SortSectionInfo.SavedSectionIdx) ||
					!LODInfo.TriangleSortSettings.IsValidIndex(SortSectionInfo.SavedSectionIdx) )
				{
					appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("FailedRestoreSortingForSectionNumber"), TriangleSortOptionToString((ETriangleSortOption)SortSectionInfo.SavedSortOption), SortSectionInfo.SavedSectionIdx));
					continue;
				}

				// Update the UI version of the data.
				FTriangleSortSettings& TriangleSortSettings = LODInfo.TriangleSortSettings(SortSectionInfo.SavedSectionIdx);
				TriangleSortSettings.TriangleSorting = SortSectionInfo.SavedSortOption;
				TriangleSortSettings.CustomLeftRightAxis = SortSectionInfo.SavedCustomLeftRightAxis;
				TriangleSortSettings.CustomLeftRightBoneName = SortSectionInfo.SavedCustomLeftRightBoneName;

				// Just reapply the same sorting method to the section again.
				LODModel.SortTriangles(NewSkeletalMesh, SortSectionInfo.SavedSectionIdx, (ETriangleSortOption)SortSectionInfo.SavedSortOption);
			}
		}		
	}

private:
	TArray<FSavedCustomSortSectionInfo> SortSectionInfos;
};


struct ExistingSkelMeshData
{
	FVector								ExistingOrigin;
	FRotator							ExistingRotOrigin;
	TArray<USkeletalMeshSocket*>		ExistingSockets;
	TArray<FString>						ExistingBoneBreakNames;
	TArray<BYTE>						ExistingBoneBreakOptions;
	TIndirectArray<FStaticLODModel>		ExistingLODModels;
	TArray<FSkeletalMeshLODInfo>		ExistingLODInfo;
	TArray<FMultiSizeIndexContainerData>		ExistingIndexBufferData;
	TArray<FMeshBone>					ExistingRefSkeleton;
	TArray<UMaterialInterface*>			ExistingMaterials;
	TArray<class UApexClothingAsset *>	ExistingClothingAssets;
	TArray<FApexClothingAssetInfo>		ExistingClothingLodMap;
	UFaceFXAsset*						ExistingFaceFXAsset;
	TArray<UMorphTargetSet*>			ExistingPreviewMorphTargetSets;
	UPhysicsAsset*						ExistingBoundsPreviewAsset;
	UBOOL								bExistingForceCPUSkinning;
	TArray<FName>						ExistingPerPolyCollisionBones;
	TArray<FName>						ExistingAddToParentPerPolyCollisionBone;
	QWORD								ExistingRUID;

	BYTE								ExistingClothMovementScaleGenMode;
	UBOOL								bExistingLimitClothToAnimMesh;
	FLOAT								ExistingClothToAnimMeshMaxDist;
	UBOOL								bExistingForceNoWelding;
	TArray<FName>						ExistingClothBones;
	INT									ExistingClothHierarchyLevels;
	UBOOL								bExistingEnableClothBendConstraints;
	UBOOL								bExistingEnableClothDamping;
	UBOOL								bExistingUseClothCOMDamping;
	FLOAT								ExistingClothStretchStiffness;
	FLOAT								ExistingClothBendStiffness;
	FLOAT								ExistingClothDensity;
	FLOAT								ExistingClothThickness;
	FLOAT								ExistingClothDamping;
	INT									ExistingClothIterations;
	INT									ExistingClothHierarchicalIterations;
	FLOAT								ExistingClothFriction;
	FLOAT								ExistingClothRelativeGridSpacing;
	FLOAT								ExistingClothPressure;
	FLOAT								ExistingClothCollisionResponseCoefficient;
	FLOAT								ExistingClothAttachmentResponseCoefficient;
	FLOAT								ExistingClothAttachmentTearFactor;
	FLOAT								ExistingClothSleepLinearVelocity;
	FLOAT								ExistingHardStretchLimitFactor;
	UBOOL								bExistingHardStretchLimit;
	UBOOL								bExistingEnableClothOrthoBendConstraints;
	UBOOL								bExistingEnableClothSelfCollision;
	UBOOL								bExistingEnableClothPressure;
	UBOOL								bExistingEnableClothTwoWayCollision;
	TArray<FClothSpecialBoneInfo>		ExistingClothSpecialBones;
	UBOOL								bExistingEnableClothLineChecks;
	UBOOL								bExistingClothMetal;
	FLOAT								ExistingClothMetalImpulseThreshold;
	FLOAT								ExistingClothMetalPenetrationDepth;
	FLOAT								ExistingClothMetalMaxDeformationDistance;
	UBOOL								bExistingEnableClothTearing;
	UBOOL								ExistingClothTearFactor;
	INT									ExistingClothTearReserve;

	TArray<FName>						ExistingSoftBodyBones;
	TArray<FSoftBodySpecialBoneInfo>	ExistingSoftBodySpecialBones;
	FLOAT								ExistingSoftBodyVolumeStiffness;
	FLOAT								ExistingSoftBodyStretchingStiffness;
	FLOAT								ExistingSoftBodyDensity;
	FLOAT								ExistingSoftBodyParticleRadius;
	FLOAT								ExistingSoftBodyDamping;
	INT									ExistingSoftBodySolverIterations;
	FLOAT								ExistingSoftBodyFriction;
	FLOAT								ExistingSoftBodyRelativeGridSpacing;
	FLOAT								ExistingSoftBodySleepLinearVelocity;
	UBOOL								bExistingEnableSoftBodySelfCollision;
	FLOAT								ExistingSoftBodyAttachmentResponse;
	FLOAT								ExistingSoftBodyCollisionResponse;
	FLOAT								ExistingSoftBodyDetailLevel;
	INT									ExistingSoftBodySubdivisionLevel;
	UBOOL								bExistingSoftBodyIsoSurface;
	UBOOL								bExistingEnableSoftBodyDamping;
	UBOOL								bExistingUseSoftBodyCOMDamping;
	FLOAT								ExistingSoftBodyAttachmentThreshold;
	UBOOL								bExistingEnableSoftBodyTwoWayCollision;
	FLOAT								ExistingSoftBodyAttachmentTearFactor;
	UBOOL								bExistingEnableSoftBodyLineChecks;
	UBOOL								bExistingUseFullPrecisionUVs;

	TArray<FBoneMirrorExport>			ExistingMirrorTable;
	FSavedCustomSortInfo				ExistingSortInfo;

	UBOOL								bExistingUseClothingAssetMaterial;
};

ExistingSkelMeshData* SaveExistingSkelMeshData(USkeletalMesh* ExistingSkelMesh)
{
	struct ExistingSkelMeshData* ExistingMeshDataPtr = NULL;

	if(ExistingSkelMesh)
	{
		ExistingMeshDataPtr = new ExistingSkelMeshData();

		ExistingMeshDataPtr->ExistingSortInfo.Save(ExistingSkelMesh, 0);

		ExistingMeshDataPtr->ExistingSockets = ExistingSkelMesh->Sockets;
		ExistingMeshDataPtr->ExistingBoneBreakNames = ExistingSkelMesh->BoneBreakNames;
		ExistingMeshDataPtr->ExistingBoneBreakOptions = ExistingSkelMesh->BoneBreakOptions;
		ExistingMeshDataPtr->ExistingMaterials = ExistingSkelMesh->Materials;
		ExistingMeshDataPtr->ExistingClothingAssets = ExistingSkelMesh->ClothingAssets;
		ExistingMeshDataPtr->ExistingClothingLodMap = ExistingSkelMesh->ClothingLodMap;

		ExistingMeshDataPtr->ExistingOrigin = ExistingSkelMesh->Origin;
		ExistingMeshDataPtr->ExistingRotOrigin = ExistingSkelMesh->RotOrigin;

		if( ExistingSkelMesh->LODModels.Num() > 0 &&
			ExistingSkelMesh->LODInfo.Num() == ExistingSkelMesh->LODModels.Num() )
		{
			// Remove the zero'th LOD (ie: the LOD being reimported).
			ExistingSkelMesh->LODModels.Remove(0);
			ExistingSkelMesh->LODInfo.Remove(0);

			// Copy off the reamining LODs.
			for ( INT LODModelIndex = 0 ; LODModelIndex < ExistingSkelMesh->LODModels.Num() ; ++LODModelIndex )
			{
				FStaticLODModel& LODModel = ExistingSkelMesh->LODModels(LODModelIndex);
				LODModel.RawPointIndices.Lock( LOCK_READ_ONLY );
				LODModel.LegacyRawPointIndices.Lock( LOCK_READ_ONLY );
			}
			ExistingMeshDataPtr->ExistingLODModels = ExistingSkelMesh->LODModels;
			for ( INT LODModelIndex = 0 ; LODModelIndex < ExistingSkelMesh->LODModels.Num() ; ++LODModelIndex )
			{

				FStaticLODModel& LODModel = ExistingSkelMesh->LODModels(LODModelIndex);
				LODModel.RawPointIndices.Unlock();
				LODModel.LegacyRawPointIndices.Unlock();

				FMultiSizeIndexContainerData ExistingData;
				LODModel.MultiSizeIndexContainer.GetIndexBufferData( ExistingData );
				ExistingMeshDataPtr->ExistingIndexBufferData.AddItem( ExistingData );

			}

			ExistingMeshDataPtr->ExistingLODInfo = ExistingSkelMesh->LODInfo;
			ExistingMeshDataPtr->ExistingRefSkeleton = ExistingSkelMesh->RefSkeleton;
		
		}

		ExistingMeshDataPtr->ExistingFaceFXAsset = ExistingSkelMesh->FaceFXAsset;
		ExistingSkelMesh->FaceFXAsset = NULL;
		if( ExistingMeshDataPtr->ExistingFaceFXAsset )
		{
			ExistingMeshDataPtr->ExistingFaceFXAsset->DefaultSkelMesh = NULL;
#if WITH_FACEFX
			OC3Ent::Face::FxActor* FaceFXActor = ExistingMeshDataPtr->ExistingFaceFXAsset->GetFxActor();
			if( FaceFXActor )
			{
				FaceFXActor->SetShouldClientRelink(FxTrue);
			}
#endif // WITH_FACEFX
		}

		ExistingMeshDataPtr->ExistingBoundsPreviewAsset = ExistingSkelMesh->BoundsPreviewAsset;
		ExistingMeshDataPtr->bExistingForceCPUSkinning = ExistingSkelMesh->bForceCPUSkinning;
		ExistingMeshDataPtr->ExistingPerPolyCollisionBones = ExistingSkelMesh->PerPolyCollisionBones;
		ExistingMeshDataPtr->ExistingAddToParentPerPolyCollisionBone = ExistingSkelMesh->AddToParentPerPolyCollisionBone;

		ExistingMeshDataPtr->ExistingRUID = ExistingSkelMesh->SkelMeshRUID;

		ExistingMeshDataPtr->ExistingClothMovementScaleGenMode = ExistingSkelMesh->ClothMovementScaleGenMode;
		ExistingMeshDataPtr->bExistingLimitClothToAnimMesh = ExistingSkelMesh->bLimitClothToAnimMesh;
		ExistingMeshDataPtr->ExistingClothToAnimMeshMaxDist = ExistingSkelMesh->ClothToAnimMeshMaxDist;
		ExistingMeshDataPtr->bExistingForceNoWelding = ExistingSkelMesh->bForceNoWelding;
		ExistingMeshDataPtr->ExistingClothBones = ExistingSkelMesh->ClothBones;
		ExistingMeshDataPtr->ExistingClothHierarchyLevels = ExistingSkelMesh->ClothHierarchyLevels;
		ExistingMeshDataPtr->bExistingEnableClothBendConstraints = ExistingSkelMesh->bEnableClothBendConstraints;
		ExistingMeshDataPtr->bExistingEnableClothDamping = ExistingSkelMesh->bEnableClothDamping;
		ExistingMeshDataPtr->bExistingUseClothCOMDamping = ExistingSkelMesh->bUseClothCOMDamping;
		ExistingMeshDataPtr->ExistingClothStretchStiffness = ExistingSkelMesh->ClothStretchStiffness;
		ExistingMeshDataPtr->ExistingClothBendStiffness = ExistingSkelMesh->ClothBendStiffness;
		ExistingMeshDataPtr->ExistingClothDensity = ExistingSkelMesh->ClothDensity;
		ExistingMeshDataPtr->ExistingClothThickness = ExistingSkelMesh->ClothThickness;
		ExistingMeshDataPtr->ExistingClothDamping = ExistingSkelMesh->ClothDamping;
		ExistingMeshDataPtr->ExistingClothIterations = ExistingSkelMesh->ClothIterations;
		ExistingMeshDataPtr->ExistingClothHierarchicalIterations = ExistingSkelMesh->ClothHierarchicalIterations;
		ExistingMeshDataPtr->ExistingClothFriction = ExistingSkelMesh->ClothFriction;
		ExistingMeshDataPtr->ExistingClothRelativeGridSpacing = ExistingSkelMesh->ClothRelativeGridSpacing;
		ExistingMeshDataPtr->ExistingClothPressure = ExistingSkelMesh->ClothPressure;
		ExistingMeshDataPtr->ExistingClothCollisionResponseCoefficient = ExistingSkelMesh->ClothCollisionResponseCoefficient;
		ExistingMeshDataPtr->ExistingClothAttachmentResponseCoefficient = ExistingSkelMesh->ClothAttachmentResponseCoefficient;
		ExistingMeshDataPtr->ExistingClothAttachmentTearFactor = ExistingSkelMesh->ClothAttachmentTearFactor;
		ExistingMeshDataPtr->ExistingClothSleepLinearVelocity = ExistingSkelMesh->ClothSleepLinearVelocity;
		ExistingMeshDataPtr->ExistingHardStretchLimitFactor = ExistingSkelMesh->HardStretchLimitFactor;
		ExistingMeshDataPtr->bExistingHardStretchLimit = ExistingSkelMesh->bHardStretchLimit;
		ExistingMeshDataPtr->bExistingEnableClothOrthoBendConstraints = ExistingSkelMesh->bEnableClothOrthoBendConstraints;
		ExistingMeshDataPtr->bExistingEnableClothSelfCollision = ExistingSkelMesh->bEnableClothSelfCollision;
		ExistingMeshDataPtr->bExistingEnableClothPressure = ExistingSkelMesh->bEnableClothPressure;
		ExistingMeshDataPtr->bExistingEnableClothTwoWayCollision = ExistingSkelMesh->bEnableClothTwoWayCollision;
		ExistingMeshDataPtr->ExistingClothSpecialBones = ExistingSkelMesh->ClothSpecialBones;
		ExistingMeshDataPtr->bExistingEnableClothLineChecks = ExistingSkelMesh->bEnableClothLineChecks;
		ExistingMeshDataPtr->bExistingClothMetal = ExistingSkelMesh->bClothMetal;
		ExistingMeshDataPtr->ExistingClothMetalImpulseThreshold = ExistingSkelMesh->ClothMetalImpulseThreshold;
		ExistingMeshDataPtr->ExistingClothMetalPenetrationDepth = ExistingSkelMesh->ClothMetalPenetrationDepth;
		ExistingMeshDataPtr->ExistingClothMetalMaxDeformationDistance = ExistingSkelMesh->ClothMetalMaxDeformationDistance;
		ExistingMeshDataPtr->bExistingEnableClothTearing = ExistingSkelMesh->bEnableClothTearing;
		ExistingMeshDataPtr->ExistingClothTearFactor = ExistingSkelMesh->ClothTearFactor;
		ExistingMeshDataPtr->ExistingClothTearReserve = ExistingSkelMesh->ClothTearReserve;

		ExistingMeshDataPtr->ExistingSoftBodyBones = ExistingSkelMesh->SoftBodyBones;
		ExistingMeshDataPtr->ExistingSoftBodySpecialBones = ExistingSkelMesh->SoftBodySpecialBones;
		ExistingMeshDataPtr->ExistingSoftBodyVolumeStiffness = ExistingSkelMesh->SoftBodyVolumeStiffness;
		ExistingMeshDataPtr->ExistingSoftBodyStretchingStiffness = ExistingSkelMesh->SoftBodyStretchingStiffness;
		ExistingMeshDataPtr->ExistingSoftBodyDensity = ExistingSkelMesh->SoftBodyDensity;
		ExistingMeshDataPtr->ExistingSoftBodyParticleRadius = ExistingSkelMesh->SoftBodyParticleRadius;
		ExistingMeshDataPtr->ExistingSoftBodyDamping = ExistingSkelMesh->SoftBodyDamping;
		ExistingMeshDataPtr->ExistingSoftBodySolverIterations = ExistingSkelMesh->SoftBodySolverIterations;
		ExistingMeshDataPtr->ExistingSoftBodyFriction = ExistingSkelMesh->SoftBodyFriction;
		ExistingMeshDataPtr->ExistingSoftBodyRelativeGridSpacing = ExistingSkelMesh->SoftBodyRelativeGridSpacing;
		ExistingMeshDataPtr->ExistingSoftBodySleepLinearVelocity = ExistingSkelMesh->SoftBodySleepLinearVelocity;
		ExistingMeshDataPtr->bExistingEnableSoftBodySelfCollision = ExistingSkelMesh->bEnableSoftBodySelfCollision;
		ExistingMeshDataPtr->ExistingSoftBodyAttachmentResponse = ExistingSkelMesh->SoftBodyAttachmentResponse;
		ExistingMeshDataPtr->ExistingSoftBodyCollisionResponse = ExistingSkelMesh->SoftBodyCollisionResponse;
		ExistingMeshDataPtr->ExistingSoftBodyDetailLevel = ExistingSkelMesh->SoftBodyDetailLevel;
		ExistingMeshDataPtr->ExistingSoftBodySubdivisionLevel = ExistingSkelMesh->SoftBodySubdivisionLevel;
		ExistingMeshDataPtr->bExistingSoftBodyIsoSurface = ExistingSkelMesh->bSoftBodyIsoSurface;
		ExistingMeshDataPtr->bExistingEnableSoftBodyDamping = ExistingSkelMesh->bEnableSoftBodyDamping;
		ExistingMeshDataPtr->bExistingUseSoftBodyCOMDamping = ExistingSkelMesh->bUseSoftBodyCOMDamping;
		ExistingMeshDataPtr->ExistingSoftBodyAttachmentThreshold = ExistingSkelMesh->SoftBodyAttachmentThreshold;
		ExistingMeshDataPtr->bExistingEnableSoftBodyTwoWayCollision = ExistingSkelMesh->bEnableSoftBodyTwoWayCollision;
		ExistingMeshDataPtr->ExistingSoftBodyAttachmentTearFactor = ExistingSkelMesh->SoftBodyAttachmentTearFactor;
		ExistingMeshDataPtr->bExistingEnableSoftBodyLineChecks = ExistingSkelMesh->bEnableSoftBodyLineChecks;

		ExistingSkelMesh->ExportMirrorTable(ExistingMeshDataPtr->ExistingMirrorTable);

		ExistingMeshDataPtr->ExistingPreviewMorphTargetSets.Empty(ExistingSkelMesh->PreviewMorphSets.Num());
		ExistingMeshDataPtr->ExistingPreviewMorphTargetSets.Append(ExistingSkelMesh->PreviewMorphSets);
		
		ExistingMeshDataPtr->bExistingUseFullPrecisionUVs = ExistingSkelMesh->bUseFullPrecisionUVs;	

		ExistingMeshDataPtr->bExistingUseClothingAssetMaterial = ExistingSkelMesh->bUseClothingAssetMaterial;
	}

	return ExistingMeshDataPtr;
}

// Sorting function for ordering bones (ascending order)
IMPLEMENT_COMPARE_CONSTREF( BYTE, SortBones, { return A - B; })

//extern UBOOL SkeletonsAreCompatible( const TArray<FMeshBone>& NewSkel, const TArray<FMeshBone>& ExistSkel );

void RestoreExistingSkelMeshData(ExistingSkelMeshData* MeshData, USkeletalMesh* SkeletalMesh)
{
	if(MeshData && SkeletalMesh)
	{
		// Fix Materials array to be the correct size.
		if(MeshData->ExistingMaterials.Num() > SkeletalMesh->Materials.Num())
		{
			MeshData->ExistingMaterials.Remove( SkeletalMesh->Materials.Num(), MeshData->ExistingMaterials.Num() - SkeletalMesh->Materials.Num() );
		}
		else if(SkeletalMesh->Materials.Num() > MeshData->ExistingMaterials.Num())
		{
			MeshData->ExistingMaterials.AddZeroed( SkeletalMesh->Materials.Num() - MeshData->ExistingMaterials.Num() );
		}

		SkeletalMesh->Materials = MeshData->ExistingMaterials;

		// Fix ClothingAssets array to be the correct size.
		if(MeshData->ExistingClothingAssets.Num() > SkeletalMesh->ClothingAssets.Num())
		{
			MeshData->ExistingClothingAssets.Remove( SkeletalMesh->ClothingAssets.Num(), MeshData->ExistingClothingAssets.Num() - SkeletalMesh->ClothingAssets.Num() );
		}
		else if(SkeletalMesh->ClothingAssets.Num() > MeshData->ExistingClothingAssets.Num())
		{
			MeshData->ExistingClothingAssets.AddZeroed( SkeletalMesh->ClothingAssets.Num() - MeshData->ExistingClothingAssets.Num() );
		}

		SkeletalMesh->ClothingAssets = MeshData->ExistingClothingAssets;

		SkeletalMesh->ClothingLodMap = MeshData->ExistingClothingLodMap;


		SkeletalMesh->Origin = MeshData->ExistingOrigin;
		SkeletalMesh->RotOrigin = MeshData->ExistingRotOrigin;

		// Assign sockets from old version of this SkeletalMesh.
		// Only copy ones for bones that exist in the new mesh.
		for(INT i=0; i<MeshData->ExistingSockets.Num(); i++)
		{
			const INT BoneIndex = SkeletalMesh->MatchRefBone( MeshData->ExistingSockets(i)->BoneName );
			if(BoneIndex != INDEX_NONE)
			{
				SkeletalMesh->Sockets.AddItem( MeshData->ExistingSockets(i) );
			}
		}

		// Assign bone breaks from old version of this SkeletalMesh.
		// Only copy ones for bones that exist in the new mesh.
		for(INT i=0; i<MeshData->ExistingBoneBreakNames.Num(); i++)
		{
			const INT BoneIndex = SkeletalMesh->MatchRefBone( FName(*MeshData->ExistingBoneBreakNames(i)) );
			if(BoneIndex != INDEX_NONE)
			{
				if (SkeletalMesh->BoneBreakNames.ContainsItem(MeshData->ExistingBoneBreakNames(i)) == FALSE)
				{
					SkeletalMesh->BoneBreakNames.AddItem( MeshData->ExistingBoneBreakNames(i) );
					SkeletalMesh->BoneBreakOptions.AddItem( MeshData->ExistingBoneBreakOptions(i) );
				}
			}
		}

		// make sure size matches up at the end
		check (SkeletalMesh->BoneBreakOptions.Num() == SkeletalMesh->BoneBreakNames.Num());

		// We copy back and fix-up the LODs that still work with this skeleton.
		if( MeshData->ExistingLODModels.Num() > 0 && SkeletonsAreCompatible(SkeletalMesh->RefSkeleton, MeshData->ExistingRefSkeleton) )
		{
			// First create mapping table from old skeleton to new skeleton.
			TArray<INT> OldToNewMap;
			OldToNewMap.Add(MeshData->ExistingRefSkeleton.Num());
			for(INT i=0; i<MeshData->ExistingRefSkeleton.Num(); i++)
			{
				OldToNewMap(i) = FindBoneIndex(SkeletalMesh->RefSkeleton, MeshData->ExistingRefSkeleton(i).Name);
			}

			for(INT i=0; i<MeshData->ExistingLODModels.Num(); i++)
			{
				FStaticLODModel& LODModel = MeshData->ExistingLODModels(i);
				FSkeletalMeshLODInfo& LODInfo = MeshData->ExistingLODInfo(i);

				
				// Fix ActiveBoneIndices array.
				UBOOL bMissingBone = false;
				FName MissingBoneName = NAME_None;
				for(INT j=0; j<LODModel.ActiveBoneIndices.Num() && !bMissingBone; j++)
				{
					INT NewBoneIndex = OldToNewMap( LODModel.ActiveBoneIndices(j) );
					if(NewBoneIndex == INDEX_NONE)
					{
						bMissingBone = true;
						MissingBoneName = MeshData->ExistingRefSkeleton( LODModel.ActiveBoneIndices(j) ).Name;
					}
					else
					{
						LODModel.ActiveBoneIndices(j) = NewBoneIndex;
					}
				}

				// Fix RequiredBones array.
				for(INT j=0; j<LODModel.RequiredBones.Num() && !bMissingBone; j++)
				{
					INT NewBoneIndex = OldToNewMap( LODModel.RequiredBones(j) );
					if(NewBoneIndex == INDEX_NONE)
					{
						bMissingBone = true;
						MissingBoneName = MeshData->ExistingRefSkeleton( LODModel.RequiredBones(j) ).Name;
					}
					else
					{
						LODModel.RequiredBones(j) = NewBoneIndex;
					}
				}

				// Sort ascending for parent child relationship
				Sort<USE_COMPARE_CONSTREF(BYTE, SortBones)>( LODModel.RequiredBones.GetData(), LODModel.RequiredBones.Num() );

				// Fix the chunks' BoneMaps.
				for(INT ChunkIndex = 0;ChunkIndex < LODModel.Chunks.Num();ChunkIndex++)
				{
					FSkelMeshChunk& Chunk = LODModel.Chunks(ChunkIndex);
					for(INT BoneIndex = 0;BoneIndex < Chunk.BoneMap.Num();BoneIndex++)
					{
						INT NewBoneIndex = OldToNewMap( Chunk.BoneMap(BoneIndex) );
						if(NewBoneIndex == INDEX_NONE)
						{
							bMissingBone = true;
							MissingBoneName = MeshData->ExistingRefSkeleton(Chunk.BoneMap(BoneIndex)).Name;
							break;
						}
						else
						{
							Chunk.BoneMap(BoneIndex) = NewBoneIndex;
						}
					}
					if(bMissingBone)
					{
						break;
					}
				}

				if(bMissingBone)
				{
					appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("NewMeshMissingBoneFromLOD"), *MissingBoneName.ToString()) );
				}
				else
				{
					// Assert that the per-section shadow casting array matches the number of sections.
					check( LODInfo.bEnableShadowCasting.Num() == LODModel.Sections.Num() );

					FStaticLODModel* NewLODModel = new(SkeletalMesh->LODModels) FStaticLODModel( LODModel );
			
					NewLODModel->MultiSizeIndexContainer.RebuildIndexBuffer( MeshData->ExistingIndexBufferData(i) );

					SkeletalMesh->LODInfo.AddItem( LODInfo );
				}
			}
		}

		SkeletalMesh->FaceFXAsset = MeshData->ExistingFaceFXAsset;
		if( MeshData->ExistingFaceFXAsset )
		{
			MeshData->ExistingFaceFXAsset->DefaultSkelMesh = SkeletalMesh;
			MeshData->ExistingFaceFXAsset->MarkPackageDirty();
#if WITH_FACEFX
			OC3Ent::Face::FxActor* FaceFXActor = MeshData->ExistingFaceFXAsset->GetFxActor();
			if( FaceFXActor )
			{
				FaceFXActor->SetShouldClientRelink(FxTrue);
			}
#endif // WITH_FACEFX
		}

		SkeletalMesh->BoundsPreviewAsset = MeshData->ExistingBoundsPreviewAsset;
		SkeletalMesh->bForceCPUSkinning = MeshData->bExistingForceCPUSkinning;
		SkeletalMesh->PerPolyCollisionBones = MeshData->ExistingPerPolyCollisionBones;
		SkeletalMesh->AddToParentPerPolyCollisionBone = MeshData->ExistingAddToParentPerPolyCollisionBone;
		SkeletalMesh->SkelMeshRUID = MeshData->ExistingRUID;

		SkeletalMesh->ClothMovementScaleGenMode = MeshData->ExistingClothMovementScaleGenMode;
		SkeletalMesh->bLimitClothToAnimMesh = MeshData->bExistingLimitClothToAnimMesh;
		SkeletalMesh->ClothToAnimMeshMaxDist = MeshData->ExistingClothToAnimMeshMaxDist;
		SkeletalMesh->bForceNoWelding = MeshData->bExistingForceNoWelding;
		SkeletalMesh->ClothBones = MeshData->ExistingClothBones;
		SkeletalMesh->ClothHierarchyLevels = MeshData->ExistingClothHierarchyLevels;
		SkeletalMesh->bEnableClothBendConstraints = MeshData->bExistingEnableClothBendConstraints;
		SkeletalMesh->bEnableClothDamping = MeshData->bExistingEnableClothDamping;
		SkeletalMesh->bUseClothCOMDamping = MeshData->bExistingUseClothCOMDamping;
		SkeletalMesh->ClothStretchStiffness = MeshData->ExistingClothStretchStiffness;
		SkeletalMesh->ClothBendStiffness = MeshData->ExistingClothBendStiffness;
		SkeletalMesh->ClothDensity = MeshData->ExistingClothDensity;
		SkeletalMesh->ClothThickness = MeshData->ExistingClothThickness;
		SkeletalMesh->ClothDamping = MeshData->ExistingClothDamping;
		SkeletalMesh->ClothIterations = MeshData->ExistingClothIterations;
		SkeletalMesh->ClothHierarchicalIterations = MeshData->ExistingClothHierarchicalIterations;
		SkeletalMesh->ClothFriction = MeshData->ExistingClothFriction;
		SkeletalMesh->ClothRelativeGridSpacing = MeshData->ExistingClothRelativeGridSpacing;
		SkeletalMesh->ClothPressure = MeshData->ExistingClothPressure;
		SkeletalMesh->ClothCollisionResponseCoefficient = MeshData->ExistingClothCollisionResponseCoefficient;
		SkeletalMesh->ClothAttachmentResponseCoefficient = MeshData->ExistingClothAttachmentResponseCoefficient;
		SkeletalMesh->ClothAttachmentTearFactor = MeshData->ExistingClothAttachmentTearFactor;
		SkeletalMesh->ClothSleepLinearVelocity = MeshData->ExistingClothSleepLinearVelocity;
		SkeletalMesh->HardStretchLimitFactor = MeshData->ExistingHardStretchLimitFactor;
		SkeletalMesh->bHardStretchLimit = MeshData->bExistingHardStretchLimit;
		SkeletalMesh->bEnableClothOrthoBendConstraints = MeshData->bExistingEnableClothOrthoBendConstraints;
		SkeletalMesh->bEnableClothSelfCollision = MeshData->bExistingEnableClothSelfCollision;
		SkeletalMesh->bEnableClothPressure = MeshData->bExistingEnableClothPressure;
		SkeletalMesh->bEnableClothTwoWayCollision = MeshData->bExistingEnableClothTwoWayCollision;
		SkeletalMesh->ClothSpecialBones = MeshData->ExistingClothSpecialBones;
		SkeletalMesh->bEnableClothLineChecks = MeshData->bExistingEnableClothLineChecks;
		SkeletalMesh->bClothMetal = MeshData->bExistingClothMetal;
		SkeletalMesh->ClothMetalImpulseThreshold = MeshData->ExistingClothMetalImpulseThreshold;
		SkeletalMesh->ClothMetalPenetrationDepth = MeshData->ExistingClothMetalPenetrationDepth;
		SkeletalMesh->ClothMetalMaxDeformationDistance = MeshData->ExistingClothMetalMaxDeformationDistance;
		SkeletalMesh->bEnableClothTearing = MeshData->bExistingEnableClothTearing;
		SkeletalMesh->ClothTearFactor = MeshData->ExistingClothTearFactor;
		SkeletalMesh->ClothTearReserve = MeshData->ExistingClothTearReserve;

		SkeletalMesh->SoftBodyBones = MeshData->ExistingSoftBodyBones;
		SkeletalMesh->SoftBodySpecialBones = MeshData->ExistingSoftBodySpecialBones;
		SkeletalMesh->SoftBodyVolumeStiffness = MeshData->ExistingSoftBodyVolumeStiffness;
		SkeletalMesh->SoftBodyStretchingStiffness = MeshData->ExistingSoftBodyStretchingStiffness;
		SkeletalMesh->SoftBodyDensity = MeshData->ExistingSoftBodyDensity;
		SkeletalMesh->SoftBodyParticleRadius = MeshData->ExistingSoftBodyParticleRadius;
		SkeletalMesh->SoftBodyDamping = MeshData->ExistingSoftBodyDamping ;
		SkeletalMesh->SoftBodySolverIterations = MeshData->ExistingSoftBodySolverIterations;
		SkeletalMesh->SoftBodyFriction = MeshData->ExistingSoftBodyFriction;
		SkeletalMesh->SoftBodyRelativeGridSpacing = MeshData->ExistingSoftBodyRelativeGridSpacing;
		SkeletalMesh->SoftBodySleepLinearVelocity = MeshData->ExistingSoftBodySleepLinearVelocity;
		SkeletalMesh->bEnableSoftBodySelfCollision = MeshData->bExistingEnableSoftBodySelfCollision;
		SkeletalMesh->SoftBodyAttachmentResponse = MeshData->ExistingSoftBodyAttachmentResponse;
		SkeletalMesh->SoftBodyCollisionResponse = MeshData->ExistingSoftBodyCollisionResponse;
		SkeletalMesh->SoftBodyDetailLevel = MeshData->ExistingSoftBodyDetailLevel;
		SkeletalMesh->SoftBodySubdivisionLevel = MeshData->ExistingSoftBodySubdivisionLevel ;
		SkeletalMesh->bSoftBodyIsoSurface = MeshData->bExistingSoftBodyIsoSurface;
		SkeletalMesh->bEnableSoftBodyDamping = MeshData->bExistingEnableSoftBodyDamping;
		SkeletalMesh->bUseSoftBodyCOMDamping = MeshData->bExistingUseSoftBodyCOMDamping;
		SkeletalMesh->SoftBodyAttachmentThreshold = MeshData->ExistingSoftBodyAttachmentThreshold;
		SkeletalMesh->bEnableSoftBodyTwoWayCollision = MeshData->bExistingEnableSoftBodyTwoWayCollision;
		SkeletalMesh->SoftBodyAttachmentTearFactor = MeshData->ExistingSoftBodyAttachmentTearFactor;
		SkeletalMesh->bEnableSoftBodyLineChecks = MeshData->bExistingEnableSoftBodyLineChecks;

		// Copy mirror table.
		SkeletalMesh->ImportMirrorTable(MeshData->ExistingMirrorTable);

		SkeletalMesh->PreviewMorphSets.Empty(MeshData->ExistingPreviewMorphTargetSets.Num());
		SkeletalMesh->PreviewMorphSets.Append(MeshData->ExistingPreviewMorphTargetSets);
		
		SkeletalMesh->bUseFullPrecisionUVs = MeshData->bExistingUseFullPrecisionUVs; 

		SkeletalMesh->bUseClothingAssetMaterial = MeshData->bExistingUseClothingAssetMaterial;

		MeshData->ExistingSortInfo.Restore(SkeletalMesh, 0);
	}
}

UObject* USkeletalMeshFactory::FactoryCreateBinary
(
 UClass*			Class,
 UObject*			InParent,
 FName				Name,
 EObjectFlags		Flags,
 UObject*			Context,
 const TCHAR*		Type,
 const BYTE*&		Buffer,
 const BYTE*		BufferEnd,
 FFeedbackContext*	Warn
 )
{

#if WITH_ACTORX
	// Before importing the new skeletal mesh - see if it already exists, and if so copy any data out we don't want to lose.
	USkeletalMesh* ExistingSkelMesh = FindObject<USkeletalMesh>( InParent, *Name.ToString() );

	UBOOL								bRestoreExistingData = FALSE;
	FVector								ExistingOrigin;
	FRotator							ExistingRotOrigin;
	TArray<USkeletalMeshSocket*>		ExistingSockets;
	TArray<FString>						ExistingBoneBreakNames;
	TIndirectArray<FStaticLODModel>		ExistingLODModels;
	TArray<FSkeletalMeshLODInfo>		ExistingLODInfo;
	TArray<FMeshBone>					ExistingRefSkeleton;
	TArray<UMaterialInterface*>			ExistingMaterials;
	UFaceFXAsset*						ExistingFaceFXAsset = NULL;
	TArray<UMorphTargetSet*>			ExistingPreviewMorphTargetSets;
	UPhysicsAsset*						ExistingBoundsPreviewAsset = NULL;
	UBOOL								bExistingForceCPUSkinning = FALSE;
	TArray<FName>						ExistingPerPolyCollisionBones;
	TArray<FName>						ExistingAddToParentPerPolyCollisionBone;
// 	QWORD								ExistingRUID;

	BYTE								ExistingClothMovementScaleGenMode = 0;
	UBOOL								bExistingLimitClothToAnimMesh = FALSE;
	FLOAT								ExistingClothToAnimMeshMaxDist = FALSE;
	UBOOL								bExistingForceNoWelding = FALSE;
	TArray<FName>						ExistingClothBones;
	INT									ExistingClothHierarchyLevels = FALSE;
	UBOOL								bExistingEnableClothBendConstraints = FALSE;
	UBOOL								bExistingEnableClothDamping = FALSE;
	UBOOL								bExistingUseClothCOMDamping = FALSE;
	FLOAT								ExistingClothStretchStiffness = 0.f;
	FLOAT								ExistingClothBendStiffness = 0.f;
	FLOAT								ExistingClothDensity = 0.f;
	FLOAT								ExistingClothThickness = 0.f;
	FLOAT								ExistingClothDamping = 0.f;
	INT									ExistingClothIterations = 0;
	INT									ExistingClothHierarchicalIterations = 0;
	FLOAT								ExistingClothFriction = 0.f;
	FLOAT								ExistingClothRelativeGridSpacing = 0.f;
	FLOAT								ExistingClothPressure = 0.f;
	FLOAT								ExistingClothCollisionResponseCoefficient = 0.f;
	FLOAT								ExistingClothAttachmentResponseCoefficient = 0.f;
	FLOAT								ExistingClothAttachmentTearFactor = 0.f;
	FLOAT								ExistingClothSleepLinearVelocity = 0.f;
	FLOAT								ExistingHardStretchLimitFactor = 0.f;
	UBOOL								bExistingHardStretchLimit = FALSE;
	UBOOL								bExistingEnableClothOrthoBendConstraints = FALSE;
	UBOOL								bExistingEnableClothSelfCollision = FALSE;
	UBOOL								bExistingEnableClothPressure = FALSE;
	UBOOL								bExistingEnableClothTwoWayCollision = FALSE;
	TArray<FClothSpecialBoneInfo>		ExistingClothSpecialBones;
	UBOOL								bExistingEnableClothLineChecks = FALSE;
	UBOOL								bExistingClothMetal = FALSE;
	FLOAT								ExistingClothMetalImpulseThreshold = 0.f;
	FLOAT								ExistingClothMetalPenetrationDepth = 0.f;
	FLOAT								ExistingClothMetalMaxDeformationDistance = 0.f;
	UBOOL								bExistingEnableClothTearing = FALSE;
	UBOOL								ExistingClothTearFactor = 0.f;
	INT									ExistingClothTearReserve = 0;

	TArray<FName>						ExistingSoftBodyBones;
	TArray<FSoftBodySpecialBoneInfo>	ExistingSoftBodySpecialBones;
	FLOAT								ExistingSoftBodyVolumeStiffness = 0.f;
	FLOAT								ExistingSoftBodyStretchingStiffness = 0.f;
	FLOAT								ExistingSoftBodyDensity = 0.f;
	FLOAT								ExistingSoftBodyParticleRadius = 0.f;
	FLOAT								ExistingSoftBodyDamping = 0.f;
	INT									ExistingSoftBodySolverIterations = 0;
	FLOAT								ExistingSoftBodyFriction = 0.f;
	FLOAT								ExistingSoftBodyRelativeGridSpacing = 0.f;
	FLOAT								ExistingSoftBodySleepLinearVelocity = 0.f;
	UBOOL								bExistingEnableSoftBodySelfCollision = FALSE;
	FLOAT								ExistingSoftBodyAttachmentResponse = 0.f;
	FLOAT								ExistingSoftBodyCollisionResponse = 0.f;
	FLOAT								ExistingSoftBodyDetailLevel = 0.f;
	INT									ExistingSoftBodySubdivisionLevel = 0;
	UBOOL								bExistingSoftBodyIsoSurface = FALSE;
	UBOOL								bExistingEnableSoftBodyDamping= FALSE;
	UBOOL								bExistingUseSoftBodyCOMDamping= FALSE;
	FLOAT								ExistingSoftBodyAttachmentThreshold = 0.f;
	UBOOL								bExistingEnableSoftBodyTwoWayCollision = FALSE;
	FLOAT								ExistingSoftBodyAttachmentTearFactor = 0.f;
	UBOOL								bExistingEnableSoftBodyLineChecks = FALSE;
	UBOOL								bExistingUseFullPrecisionUVs = FALSE;

	TArray<FBoneMirrorExport>			ExistingMirrorTable;
	ExistingSkelMeshData*				ExistingMeshDataPtr = NULL;

	if(ExistingSkelMesh)
	{
		// Free any RHI resources for existing mesh before we re-create in place.
		ExistingSkelMesh->PreEditChange(NULL);
		ExistingMeshDataPtr = SaveExistingSkelMeshData(ExistingSkelMesh);
		
		bRestoreExistingData = TRUE;
	}

	// Create 'empty' mesh.
	USkeletalMesh* SkeletalMesh = CastChecked<USkeletalMesh>( StaticConstructObject( Class, InParent, Name, Flags ) );
	
	// Store the current file path and timestamp for re-import purposes
	SkeletalMesh->SourceFilePath = GFileManager->ConvertToRelativePath( *CurrentFilename );
	FFileManager::FTimeStamp Timestamp;
	if ( GFileManager->GetTimestamp( *CurrentFilename, Timestamp ) )
	{
		SkeletalMesh->SourceFileTimestamp = FString::Printf( TEXT("%04d-%02d-%02d %02d:%02d:%02d"), Timestamp.Year, Timestamp.Month+1, Timestamp.Day, Timestamp.Hour, Timestamp.Minute, Timestamp.Second );        
	}
	
	SkeletalMesh->PreEditChange(NULL);

	if( bAssumeMayaCoordinates )
	{
		SkeletalMesh->RotOrigin = FRotator(0, -16384, 16384);		
	}

	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("ImportingSkeletalMeshFile")), TRUE );

	// Fill with data from buffer - contains the full .PSK file. 	
	FSkeletalMeshBinaryImport SkelMeshImporter;
	SkelMeshImporter.ImportFromFile( (BYTE*)Buffer, BufferEnd );

	// process materials from import data
	ProcessImportMeshMaterials(SkeletalMesh->Materials,SkelMeshImporter, TRUE);

	SkeletalMesh->ClothingAssets.Empty();
	if ( SkeletalMesh->Materials.Num() > 0 )
	{
		SkeletalMesh->ClothingAssets.Add(SkeletalMesh->Materials.Num());
		for (INT i=0; i<SkeletalMesh->Materials.Num(); i++)
		{
			SkeletalMesh->ClothingAssets(i) = NULL;
		}
	}

	// process reference skeleton from import data
	ProcessImportMeshSkeleton(SkeletalMesh->RefSkeleton,SkeletalMesh->SkeletalDepth,SkelMeshImporter);
	debugf( TEXT("Bones digested - %i  Depth of hierarchy - %i"), SkeletalMesh->RefSkeleton.Num(), SkeletalMesh->SkeletalDepth );

	// Build map between bone name and bone index now.
	SkeletalMesh->InitNameIndexMap();

	// process bone influences from import data
	ProcessImportMeshInfluences(SkelMeshImporter);

	check(SkeletalMesh->LODModels.Num() == 0);
	SkeletalMesh->LODModels.Empty();
	new(SkeletalMesh->LODModels)FStaticLODModel();

	SkeletalMesh->LODInfo.Empty();
	SkeletalMesh->LODInfo.AddZeroed();
	SkeletalMesh->LODInfo(0).LODHysteresis = 0.02f;

	// Create initial bounding box based on expanded version of reference pose for meshes without physics assets. Can be overridden by artist.
	FBox BoundingBox( &SkelMeshImporter.Points(0), SkelMeshImporter.Points.Num() );
	FBox Temp = BoundingBox;
	FVector MidMesh		= 0.5f*(Temp.Min + Temp.Max);
	BoundingBox.Min		= Temp.Min + 1.0f*(Temp.Min - MidMesh);
    BoundingBox.Max		= Temp.Max + 1.0f*(Temp.Max - MidMesh);
	// Tuck up the bottom as this rarely extends lower than a reference pose's (e.g. having its feet on the floor).
	// Maya has Y in the vertical, other packages have Z.
	const INT CoordToTuck = bAssumeMayaCoordinates ? 1 : 2;
	BoundingBox.Min[CoordToTuck]	= Temp.Min[CoordToTuck] + 0.1f*(Temp.Min[CoordToTuck] - MidMesh[CoordToTuck]);
	SkeletalMesh->Bounds= FBoxSphereBounds(BoundingBox);

	// copy vertex data needed to generate skinning streams for LOD
	TArray<FVector> LODPoints;
	TArray<FMeshWedge> LODWedges;
	TArray<FMeshFace> LODFaces;
	TArray<FVertInfluence> LODInfluences;
	SkelMeshImporter.CopyLODImportData(LODPoints,LODWedges,LODFaces,LODInfluences);

	// Store whether or not the mesh has vertex colors
	SkeletalMesh->bHasVertexColors = SkelMeshImporter.bHasVertexColors;

	// process optional import data if available and use it when generating the skinning streams
	FSkelMeshExtraInfluenceImportData* ExtraInfluenceDataPtr = NULL;
	FSkelMeshExtraInfluenceImportData ExtraInfluenceData;
	if( OptionalImportData != NULL )
	{
		if( OptionalImportData->RawMeshInfluencesData.Wedges.Num() > 0 )
		{
			// process reference skeleton from import data
			INT TempSkelDepth=0;
			ProcessImportMeshSkeleton(ExtraInfluenceData.RefSkeleton,TempSkelDepth,OptionalImportData->RawMeshInfluencesData);
			// process bone influences from import data
			ProcessImportMeshInfluences(OptionalImportData->RawMeshInfluencesData);
			// copy vertex data needed for processing the extra vertex influences from import data
			OptionalImportData->RawMeshInfluencesData.CopyLODImportData(
				ExtraInfluenceData.Points,
				ExtraInfluenceData.Wedges,
				ExtraInfluenceData.Faces,
				ExtraInfluenceData.Influences
				);
			ExtraInfluenceDataPtr = &ExtraInfluenceData;
			ExtraInfluenceDataPtr->Usage = OptionalImportData->IntendedUsage;
			ExtraInfluenceDataPtr->MaxBoneCountPerChunk = OptionalImportData->MaxBoneCountPerChunk;
		}
	}

	FStaticLODModel& LODModel = SkeletalMesh->LODModels(0);
	// Pass the number of texture coordinate sets to the LODModel
	LODModel.NumTexCoords = SkelMeshImporter.NumTexCoords;

	// Create actual rendering data.
	if( !SkeletalMesh->CreateSkinningStreams(LODInfluences,LODWedges,LODFaces,LODPoints,ExtraInfluenceDataPtr) )
	{
		SkeletalMesh->MarkPendingKill();
		GWarn->EndSlowTask();
		return NULL;
	}

	// Calculate the required bones in this configuration
	SkeletalMesh->CalculateRequiredBones(0);

	// Presize the per-section shadow casting array with the number of sections in the imported LOD.
	const INT NumSections = LODModel.Sections.Num();
	SkeletalMesh->LODInfo(0).bEnableShadowCasting.Empty( NumSections );
	for ( INT SectionIndex = 0 ; SectionIndex < NumSections ; ++SectionIndex )
	{
		SkeletalMesh->LODInfo(0).bEnableShadowCasting.AddItem( TRUE );
	}
	SkeletalMesh->LODInfo(0).TriangleSortSettings.AddZeroed(NumSections);

	// Make RUID.
	SkeletalMesh->SkelMeshRUID = appCreateRuntimeUID();

	if(bRestoreExistingData)
	{
		RestoreExistingSkelMeshData(ExistingMeshDataPtr, SkeletalMesh);
	}

	// End the importing, mark package as dirty so it prompts to save on exit.
	SkeletalMesh->CalculateInvRefMatrices();
	SkeletalMesh->PostEditChange();

	SkeletalMesh->MarkPackageDirty();

	// We have to go and fix any AnimSetMeshLinkup objects that refer to this skeletal mesh, as the reference skeleton has changed.
	for(TObjectIterator<UAnimSet> It;It;++It)
	{
		UAnimSet* AnimSet = *It;

		// Get SkeletalMesh path name
		FName SkelMeshName = FName( *SkeletalMesh->GetPathName() );

		// See if we have already cached this Skeletal Mesh.
		const INT* IndexPtr = AnimSet->SkelMesh2LinkupCache.Find( SkelMeshName );

		if( IndexPtr )
		{
			AnimSet->LinkupCache( *IndexPtr ).BuildLinkup( SkeletalMesh, AnimSet );
		}
	}

	// Now iterate over all skeletal mesh components re-initialising them.
	for(TObjectIterator<USkeletalMeshComponent> It; It; ++It)
	{
		USkeletalMeshComponent* SkelComp = *It;
		if(SkelComp->SkeletalMesh == SkeletalMesh && SkelComp->GetScene())
		{
			FComponentReattachContext ReattachContext(SkelComp);
		}
	}

	GWarn->EndSlowTask();
	return SkeletalMesh;	
#else
	appMsgf( AMT_OK, *LocalizeUnrealEd("Error_ActorXDeprecated") );
	return NULL;
#endif // WITH_ACTORX
}

IMPLEMENT_CLASS(USkeletalMeshFactory);


