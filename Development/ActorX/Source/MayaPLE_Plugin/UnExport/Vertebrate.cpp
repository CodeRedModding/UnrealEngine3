/**********************************************************************
	
	Vertebrate.cpp - Binary structures for digesting and exporting skeleton, skin & animation data.

  	Copyright (c) 1999,2000 Epic MegaGames, Inc. All Rights Reserved.

	New model limits (imposed by the binary format and/or engine)
		256		materials.
		65535  'wedges' = amounts to approx 20000 triangles/vertices, depending on texturing complexity.
	A realistic upper limit, with LOD in mind, is 10000 wedges.

	Todo: 
	 Smoothing group support !
	-> Unreal's old non-lod code had structures in place for all adjacent triangles-to-a-vertex,
	   to compute the vertex normal. 
    -> LODMeshes only needs the connectivity of each triangle to its three vertices.
	-> Facet-shading as opposed to gouraud/vertex shading would be a very nice option.
	-> Normals should be exported per influence & transformed along with vertices for faster drawing/lighting pipeline in UT ?!
		 
************************************************************************/

#include "precompiled.h"
#include "Vertebrate.h"


// Ctor
VActor::VActor()
{
	NodeCount = 0;
	MeshCount = 0;

	FrameTotalTicks = 0.0f; 
	FrameRate = 0.0f; 

	// file stuff
	LogFileName = 0;
	OutFileName = 0;

	BetaNumBones = 0;
	BetaNumFrames = 0;
	BetaAnimName[0] = 0;
	AnimationBoneNumber = 0;
};
		
void VActor::Cleanup()
{
	BetaKeys.Empty();
	RefSkeletonNodes.Empty();
	RefSkeletonBones.Empty();

	// Clean up all dynamic-array-allocated ones - can't count on automatic destructor calls for multi-dimension arrays...
	{for(INT i=0; i<Animations.Num(); i++)
	{
		Animations[i].KeyTrack.Empty();
	}}
	Animations.Empty();

	{for(INT i=0; i<OutAnims.Num(); i++)
	{
		OutAnims[i].KeyTrack.Empty();
	}}
	OutAnims.Empty();
}

VActor::~VActor()
{
	Cleanup();
}

int	VActor::MatchNodeToSkeletonIndex(AXNode* ANode)
{
	for (int t=0; t<RefSkeletonBones.Num(); t++)
	{
		if ( RefSkeletonNodes[t] == ANode) return t;
	}

	return -1; // no matching node found.
}


//
// Save the actor: Physique mesh, plus reference skeleton.
//

int VActor::SerializeActor(FastFileClass &OutFile)
{
	// Header
	VChunkHdr ChunkHdr;
	_tcscpy(ChunkHdr.ChunkID,"ACTRHEAD");
	ChunkHdr.DataCount = 0;
	ChunkHdr.DataSize  = 0;
	ChunkHdr.TypeFlag  = 1999801; // ' 1 august 99' 
	OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));
	////////////////////////////////////////////

	//TCHAR MessagePopup[512];
	//sprintf(MessagePopup, "Writing Skin file, 3d vertices : %i",SkinData.Points.Num());
	//PopupBox(GetActiveWindow(),MessagePopup, "Saving", MB_OK);				

	// Skin: 3D Points
	_tcscpy(ChunkHdr.ChunkID,("PNTS0000"));
	ChunkHdr.DataCount = SkinData.Points.Num();
	ChunkHdr.DataSize  = sizeof ( VPoint );
	OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));

	/////////////////////////////////////////////
	for( int i=0; i < (SkinData.Points.Num()); i++ )
	{
		OutFile.Write( &SkinData.Points[i], sizeof (VPoint));
	}

	// Skin: VERTICES (wedges)
	_tcscpy(ChunkHdr.ChunkID,("VTXW0000"));
	ChunkHdr.DataCount = SkinData.Wedges.Num();
	ChunkHdr.DataSize  = sizeof (VVertex);
	OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));
	//
	for( i=0; i<SkinData.Wedges.Num(); i++)
	{
		OutFile.Write( &SkinData.Wedges[i], sizeof (VVertex));
	}

	// Skin: TRIANGLES (faces)
	_tcscpy(ChunkHdr.ChunkID,("FACE0000"));
	ChunkHdr.DataCount = SkinData.Faces.Num();
	ChunkHdr.DataSize  = sizeof( VTriangle );
	OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));
	//
	for( i=0; i<SkinData.Faces.Num(); i++)
	{
		OutFile.Write( &SkinData.Faces[i], sizeof (VTriangle));
	}

	// Skin: Materials
	_tcscpy(ChunkHdr.ChunkID,("MATT0000"));
	ChunkHdr.DataCount = SkinData.Materials.Num();
	ChunkHdr.DataSize  = sizeof( VMaterial );
	OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));
	//
	for( i=0; i<SkinData.Materials.Num(); i++)
	{
		OutFile.Write( &SkinData.Materials[i], sizeof (VMaterial));
	}

	// Reference Skeleton: Refskeleton.TotalBones times a VBone.
	_tcscpy(ChunkHdr.ChunkID,("REFSKELT"));
	ChunkHdr.DataCount = RefSkeletonBones.Num();
	ChunkHdr.DataSize  = sizeof ( VBone ) ;
	OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));
	//
	for( i=0; i<RefSkeletonBones.Num(); i++)
	{
		OutFile.Write( &RefSkeletonBones[i], sizeof (VBone));
	}

	// Reference Skeleton: Refskeleton.TotalBones times a VBone.
	_tcscpy(ChunkHdr.ChunkID,("RAWWEIGHTS"));
	ChunkHdr.DataCount = SkinData.RawWeights.Num(); 
	ChunkHdr.DataSize  = sizeof ( VRawBoneInfluence ) ;
	OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));
		
	for( i=0; i< SkinData.RawWeights.Num(); i++)
	{
		OutFile.Write( &SkinData.RawWeights[i], sizeof (VRawBoneInfluence));
	}
	
	return OutFile.GetError();
};


// Save the Output animations. ( 'Reference' skeleton is just all the bone names.)
int VActor::SerializeAnimation(FastFileClass &OutFile)
{
	// Header :
	VChunkHdr ChunkHdr;
	_tcscpy(ChunkHdr.ChunkID,("ANIMHEAD"));
	ChunkHdr.DataCount = 0;
	ChunkHdr.DataSize  = 0;
	OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));

	// Bone names (+flags) list:
	_tcscpy(ChunkHdr.ChunkID,("BONENAMES"));
	ChunkHdr.DataCount = RefSkeletonBones.Num(); 
	ChunkHdr.DataSize  = sizeof ( VBone ); 
	OutFile.Write(&ChunkHdr, sizeof (ChunkHdr));
	for(int b = 0; b < RefSkeletonBones.Num(); b++)
	{
		OutFile.Write( &RefSkeletonBones[b], sizeof (VBone) );
	}

	INT TotalAnimKeys = 0;
	INT TotalAnimFrames = 0;
	// Add together all frames to get the count.
	for(INT i = 0; i<OutAnims.Num(); i++)
	{
		OutAnims[i].AnimInfo.FirstRawFrame = TotalAnimKeys / RefSkeletonBones.Num();
		TotalAnimKeys   += OutAnims[i].KeyTrack.Num();
		TotalAnimFrames += OutAnims[i].AnimInfo.NumRawFrames;			
	}

	_tcscpy(ChunkHdr.ChunkID,("ANIMINFO"));
	ChunkHdr.DataCount = OutAnims.Num();
	ChunkHdr.DataSize  = sizeof( AnimInfoBinary  ); // heap of angaxis/pos/length, 8 floats #debug
	OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));
	for( i = 0; i<OutAnims.Num(); i++)
	{
		OutFile.Write( &OutAnims[i].AnimInfo, sizeof( AnimInfoBinary ) );
	}
	
	_tcscpy(ChunkHdr.ChunkID,("ANIMKEYS"));
	ChunkHdr.DataCount = TotalAnimKeys;            // RefSkeletonBones.Num() * BetaNumFrames; 
	ChunkHdr.DataSize  = sizeof( VQuatAnimKey );   // Heap of angaxis/pos/length, 8 floats #debug
	OutFile.Write( &ChunkHdr, sizeof (ChunkHdr));

	// Save out all in our 'digested' array.
	for( INT a = 0; a<OutAnims.Num(); a++ )
	{	
		OutFile.Write( OutAnims[a].KeyTrack.Data, OutAnims[a].KeyTrack.Num() * sizeof ( VQuatAnimKey ));
	}

	return 1;
};

// Load the Output animations. ( 'Reference' skeleton is just all the bone names.)
int VActor::LoadAnimation(FastFileClass &InFile)
{
	//
	// Animation layout:  
	//
	// name        variable										type
	//
	// ANIMHEAD
	// BONENAMES   RefSkeletonBones								FNamedBoneBinary
	// ANIMINFO    OutAnims										AnimInfoBinary
	// ANIMKEYS    OutAnims[0-.KeyTrack.Num()].KeyTrack[i]....  VQuatAnimKey
	//

	// Animation header.
	VChunkHdr ChunkHdr;
	// Output error message if not found.
	INT ReadBytes = InFile.Read(&ChunkHdr,sizeof(ChunkHdr));	

	// Bones
	InFile.Read(&ChunkHdr,sizeof(ChunkHdr));
	RefSkeletonBones.Empty();		
	RefSkeletonBones.Add( ChunkHdr.DataCount ); 
	for( INT i=0; i<RefSkeletonBones.Num(); i++ )
	{
		InFile.Read( &RefSkeletonBones[i], sizeof(VBone) );			
	}		
	// Animation info
	InFile.Read(&ChunkHdr,sizeof(ChunkHdr));

	// Proper cleanup: de-linking of tracks! -> because they're 'an array of arrays' and our 
	// dynamic array stuff is dumb and doesn't call individual element-destructors, which would usually carry this responsibility.
	for( i=0; i<OutAnims.Num();i++)
	{
		OutAnims[i].KeyTrack.Empty(); // Empty should really unlink any data !
	}
	OutAnims.Empty();

	OutAnims.AddZeroed( ChunkHdr.DataCount ); // Zeroed... necessary, the KeyTrack is a dynamic array.

	for( i = 0; i<OutAnims.Num(); i++)
	{	
		InFile.Read( &OutAnims[i].AnimInfo, sizeof(AnimInfoBinary));
	}		

	// Key tracks.
	InFile.Read(&ChunkHdr,sizeof(ChunkHdr));
	// verify if total matches read keys...
	INT TotalKeys = ChunkHdr.DataCount;
	INT ReadKeys = 0;

	//PopupBox(" Start loading Keytracks, number: %i OutAnims: %i ", ChunkHdr.DataCount , OutAnims.Num() );
	for( i = 0; i<OutAnims.Num(); i++)
	{	
		INT TrackKeys = OutAnims[i].AnimInfo.NumRawFrames * OutAnims[i].AnimInfo.TotalBones;
		OutAnims[i].KeyTrack.Empty();
		OutAnims[i].KeyTrack.Add(TrackKeys);
		InFile.Read( &(OutAnims[i].KeyTrack[0]), TrackKeys * sizeof(VQuatAnimKey) );

		ReadKeys += TrackKeys;
	}

#pragma message("TODO:Popup")
	//if( OutAnims.Num() &&  (AnimationBoneNumber > 0) && (AnimationBoneNumber != OutAnims[0].AnimInfo.TotalBones ) )
	//	PopupBox(" ERROR !! Loaded animation bone number [%i] \n inconsistent with digested bone number [%i] ",OutAnims[0].AnimInfo.TotalBones,AnimationBoneNumber);

	return 1;
}

// Add a current 'betakeys' animation data to our TempActor's in-memory repertoire.
int VActor::RecordAnimation()
{		
	AnimInfoBinary TempInfo;
	TempInfo.FirstRawFrame = 0; // Fixed up at write time
	TempInfo.NumRawFrames =  BetaNumFrames; // BetaKeys.Num() 
	TempInfo.StartBone = 0; // 
	TempInfo.TotalBones = BetaNumBones; // OurBoneTotal;
	TempInfo.TrackTime = BetaNumFrames; // FrameRate;
	TempInfo.AnimRate =  FrameRate; // BetaNumFrames;
	TempInfo.RootInclude = 0;
	TempInfo.KeyReduction = 1.0;
	TempInfo.KeyQuotum = BetaKeys.Num(); // NumFrames * BetaNumBones; //Set to full size...
	TempInfo.KeyCompressionStyle = 0;

	if( BetaNumFrames && BetaNumBones ) 
	{
		INT ThisIndex = Animations.Num();

		// Very necessary - see if we're adding inconsistent skeletons to the (raw) frame memory !
		if( ThisIndex==0) // First one to add...
		{
			AnimationBoneNumber = BetaNumBones;
		}
		else if ( AnimationBoneNumber != BetaNumBones )
		{				
#pragma message("TODO:Popup")
//				PopupBox("ERROR !! Inconsistent number of bones detected: %i instead of %i",BetaNumBones,AnimationBoneNumber );
			return 0;
		}

		Animations.AddZeroed(1);
		Animations[ThisIndex].AnimInfo = TempInfo;


		//for(INT t=0; t< BetaKeys.Num(); t++)
		//{
		//	Animations[ThisIndex].KeyTrack.AddItem( BetaKeys[t] ); // mikearno
		//}
		Animations[ThisIndex].KeyTrack.Append( BetaKeys );

		// get name
		_tcscpy( Animations[ThisIndex].AnimInfo.Name, BetaAnimName );
		// get group name
		_tcscpy( Animations[ThisIndex].AnimInfo.Group, ("None") );
		
		INT TotalKeys = Animations[ThisIndex].KeyTrack.Num();					
		BetaNumFrames = 0;
		BetaKeys.Empty();

		return TotalKeys;
	}
	else 
	{
		BetaNumFrames = 0;
		BetaKeys.Empty();
		return 0;
	}
}

DWORD VActor::FlagsFromName(char* pName )
{

	BOOL	two=FALSE;
	BOOL	translucent=FALSE;
	BOOL	weapon=FALSE;
	BOOL	unlit=FALSE;
	BOOL	enviro=FALSE;
	BOOL	nofiltering=FALSE;
	BOOL	modulate=FALSE;
	BOOL	masked=FALSE;
	BOOL	flat=FALSE;
	BOOL	alpha=FALSE;

	// Substrings in arbitrary positions enforce flags.
	if (CheckSubString(pName,_T("twosid")))    two=true;
	if (CheckSubString(pName,_T("doublesid"))) two=true;
	if (CheckSubString(pName,_T("weapon"))) weapon=true;
	if (CheckSubString(pName,_T("modul"))) modulate=true;
	if (CheckSubString(pName,_T("mask")))  masked=true;
	if (CheckSubString(pName,_T("flat")))  flat=true;
	if (CheckSubString(pName,_T("envir"))) enviro=true;
	if (CheckSubString(pName,_T("mirro")))  enviro=true;
	if (CheckSubString(pName,_T("nosmo")))  nofiltering=true;
	if (CheckSubString(pName,_T("unlit")))  unlit=true;
	if (CheckSubString(pName,_T("bright"))) unlit=true;
	if (CheckSubString(pName,_T("trans")))  translucent=true;
	if (CheckSubString(pName,_T("opaque"))) translucent=false;
	if (CheckSubString(pName,_T("alph"))) alpha=true;

	BYTE MatFlags= MTT_Normal;
	
	if (two)
		MatFlags|= MTT_NormalTwoSided;
	
	if (translucent)
		MatFlags|= MTT_Translucent;
	
	if (masked)
		MatFlags|= MTT_Masked;
	
	if (modulate)
		MatFlags|= MTT_Modulate;
	
	if (unlit)
		MatFlags|= MTT_Unlit;
	
	if (flat)
		MatFlags|= MTT_Flat;
	
	if (enviro)
		MatFlags|= MTT_Environment;
	
	if (nofiltering)
		MatFlags|= MTT_NoSmooth;

	if (alpha)
		MatFlags|= MTT_Alpha;
	
	if (weapon)
		MatFlags= MTT_Placeholder;

	return (DWORD) MatFlags;
}


int VActor::ReflagMaterials()
{
	for(INT t=0; SkinData.Materials.Num(); t++)
	{
		SkinData.Materials[t].PolyFlags = FlagsFromName(SkinData.Materials[t].MaterialName);
	}	
	return 1;
}

// Write out a brush to the file. Everything's in SkinData - ignore any weights etc.
int VActor::WriteBrush(FastFileClass &OutFile, INT DoSmooth, INT OneTexture )
{		
	//for(INT m=0; m<SkinData.Materials.Num(); m++)
	//	PopupBox("MATERIAL OUTPUT SIZES: [%i] %i %i",m,SkinData.MaterialUSize[m],SkinData.MaterialVSize[m]);


	OutFile.Print("Begin PolyList\r\n");
	// Write all faces.
	for(INT i=0; i<SkinData.Faces.Num(); i++)
	{
		
		FVector Base;
		FVector Normal;
		FVector TextureU;
		FVector TextureV;
		INT PointIdx[3];
		FVector Vertex[3];
		FLOAT U[3],V[3];

		INT TexIndex = SkinData.Faces[i].MatIndex;
		if ( OneTexture ) TexIndex = 0;

		for(INT v=0; v<3; v++)
		{
			PointIdx[v]= SkinData.Wedges[ SkinData.Faces[i].WedgeIndex[v] ].PointIndex;
			Vertex[v] = SkinData.Points[ PointIdx[v]].Point;
			U[v] = SkinData.Wedges[ SkinData.Faces[i].WedgeIndex[v] ].U;
			V[v] = SkinData.Wedges[ SkinData.Faces[i].WedgeIndex[v] ].V;
		}

		// Compute Unreal-style texture coordinate systems:
		//FLOAT MaterialWidth  = 1024; //SkinData.MaterialUSize[TexIndex]; //256.0f
		//FLOAT MaterialHeight = -1024; //SkinData.MaterialVSize[TexIndex]; //256.0f

		FLOAT MaterialWidth  = SkinData.MaterialUSize[TexIndex]; 
		FLOAT MaterialHeight = -SkinData.MaterialVSize[TexIndex]; 			

		FTexCoordsToVectors
		(
			Vertex[0], FVector( U[0],V[0],0.0f) * FVector( MaterialWidth, MaterialHeight, 1),
			Vertex[1], FVector( U[1],V[1],0.0f) * FVector( MaterialWidth, MaterialHeight, 1),
			Vertex[2], FVector( U[2],V[2],0.0f) * FVector( MaterialWidth, MaterialHeight, 1),
			&Base, &TextureU, &TextureV 
		);

		// Need to flip the one texture vector ?
		TextureV *= -1;
		// Pre-flip ???
		FVector Flip(-1,1,1);
		Vertex[0] *= Flip;
		Vertex[1] *= Flip;
		Vertex[2] *= Flip;			
		Base *= Flip;
		TextureU *= Flip;
		TextureV *= Flip;

		// Maya: need to flip everything 'upright' -Y vs Z?
		
		// Write face
		OutFile.Print("	Begin Polygon");

		

		OutFile.Print("	Texture=%s", SkinData.Materials[TexIndex].MaterialName );
		if( DoSmooth )
			OutFile.Print(" Smooth=%u", SkinData.Faces[i].SmoothingGroups);
		// Flags = NOTE-translate to Unreal in-engine style flags

		DWORD PolyFlags = 0;
		// SkinData.Materials[TexIndex].PolyFlags
		DWORD TriFlags = SkinData.Materials[TexIndex].PolyFlags;
		if( TriFlags)
		{		
			// Set style based on triangle type.
			if     ( (TriFlags&15)==MTT_Normal         ) PolyFlags |= 0;
			else if( (TriFlags&15)==MTT_NormalTwoSided ) PolyFlags |= PF_TwoSided;
			else if( (TriFlags&15)==MTT_Modulate       ) PolyFlags |= PF_Modulated;
			else if( (TriFlags&15)==MTT_Translucent    ) PolyFlags |= PF_Translucent;
			else if( (TriFlags&15)==MTT_Masked         ) PolyFlags |= PF_Masked;
			else if( (TriFlags&15)==MTT_Placeholder    ) PolyFlags |= PF_Invisible;

			// Handle effects.
			if     ( TriFlags&MTT_Unlit             ) PolyFlags |= PF_Unlit;
			if     ( TriFlags&MTT_Flat              ) PolyFlags |= PF_Flat;
			if     ( TriFlags&MTT_Environment       ) PolyFlags |= PF_Environment;
			if     ( TriFlags&MTT_NoSmooth          ) PolyFlags |= PF_NoSmooth;

			// per-pixel Alpha flag ( Reuses Flatness triangle tag and PF_AlphaTexture engine tag...)
			if     ( TriFlags&MTT_Flat				) PolyFlags |= PF_AlphaTexture; 
		}

		OutFile.Print(" Flags=%i",PolyFlags );

		OutFile.Print(" Link=%u", i);
		OutFile.Print("\r\n");

		OutFile.Print("		Origin   %+013.6f,%+013.6f,%+013.6f\r\n", Base.X, Base.Y, Base.Z );			
		OutFile.Print("		TextureU %+013.6f,%+013.6f,%+013.6f\r\n", TextureU.X, TextureU.Y, TextureU.Z );
		OutFile.Print("		TextureV %+013.6f,%+013.6f,%+013.6f\r\n", TextureV.X, TextureV.Y, TextureV.Z );
		for( v=0; v<3; v++ )
		OutFile.Print("		Vertex   %+013.6f,%+013.6f,%+013.6f\r\n", Vertex[v].X, Vertex[v].Y, Vertex[v].Z );
		OutFile.Print("	End Polygon\r\n");
	}		
	OutFile.Print("End PolyList\r\n");

	return 0;
}
