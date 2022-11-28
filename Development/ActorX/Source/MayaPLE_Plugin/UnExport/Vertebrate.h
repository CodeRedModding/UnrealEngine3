/**********************************************************************
	
	Vertebrate.h - Binary structures for digesting and exporting skeleton, skin & animation data.

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

#ifndef VERTHDR_H
#define VERTHDR_H

#include "DynArray.h"
#include "UnSkeletal.h"
#include "FastFile.h"

//#include "Win32IO.h"
//#include <assert.h>
//#include <stdio.h>


// Enforce the default packing, entrenched in the exporter/importer code for PSA & PSK files...
#pragma pack( push, 4)

// Bitflags describing effects for a (classic Unreal & Unreal Tournament) mesh triangle.
enum EJSMeshTriType
{
	// Triangle types. Pick ONE AND ONLY ONE of these.
	MTT_Normal				= 0x00,	// Normal one-sided.
	MTT_NormalTwoSided      = 0x01,    // Normal but two-sided.
	MTT_Translucent			= 0x02,	// Translucent two-sided.
	MTT_Masked				= 0x03,	// Masked two-sided.
	MTT_Modulate			= 0x04,	// Modulation blended two-sided.
	MTT_Placeholder			= 0x08,	// Placeholder triangle for positioning weapon. Invisible.

	// Bit flags. Add any of these you want.
	MTT_Unlit				= 0x10,	// Full brightness, no lighting.
	MTT_Flat				= 0x20,	// Flat surface, don't do bMeshCurvy thing.
	MTT_Alpha				= 0x20,	// This material has per-pixel alpha.
	MTT_Environment			= 0x40,	// Environment mapped.
	MTT_NoSmooth			= 0x80,	// No bilinear filtering on this poly's texture.
	
};

// Unreal engine internal / T3D polyflags
enum EPolyFlags
{
	// Regular in-game flags.
	PF_Invisible		= 0x00000001,	// Poly is invisible.
	PF_Masked			= 0x00000002,	// Poly should be drawn masked.
	PF_Translucent	 	= 0x00000004,	// Poly is transparent.
	PF_NotSolid			= 0x00000008,	// Poly is not solid, doesn't block.
	PF_Environment   	= 0x00000010,	// Poly should be drawn environment mapped.
	PF_ForceViewZone	= 0x00000010,	// Force current iViewZone in OccludeBSP (reuse Environment flag)
	PF_Semisolid	  	= 0x00000020,	// Poly is semi-solid = collision solid, Csg nonsolid.
	PF_Modulated 		= 0x00000040,	// Modulation transparency.
	PF_FakeBackdrop		= 0x00000080,	// Poly looks exactly like backdrop.
	PF_TwoSided			= 0x00000100,	// Poly is visible from both sides.
	PF_AutoUPan		 	= 0x00000200,	// Automatically pans in U direction.
	PF_AutoVPan 		= 0x00000400,	// Automatically pans in V direction.
	PF_NoSmooth			= 0x00000800,	// Don't smooth textures.
	PF_BigWavy 			= 0x00001000,	// Poly has a big wavy pattern in it.
	PF_SpecialPoly		= 0x00001000,	// Game-specific poly-level render control (reuse BigWavy flag)
	PF_AlphaTexture		= 0x00001000,	// Honor texture alpha (reuse BigWavy and SpecialPoly flags)
	PF_SmallWavy		= 0x00002000,	// Small wavy pattern (for water/enviro reflection).
	PF_DetailTexture	= 0x00002000,	// Render as detail texture.
	PF_Flat				= 0x00004000,	// Flat surface.
	PF_LowShadowDetail	= 0x00008000,	// Low detail shadows.
	PF_NoMerge			= 0x00010000,	// Don't merge poly's nodes before lighting when rendering.
	PF_SpecularBump		= 0x00020000,	// Poly is specular bump mapped.
	PF_DirtyShadows		= 0x00040000,	// Dirty shadows.
	PF_BrightCorners	= 0x00080000,	// Brighten convex corners.
	PF_SpecialLit		= 0x00100000,	// Only speciallit lights apply to this poly.
	PF_Gouraud			= 0x00200000,	// Gouraud shaded.
	PF_NoBoundRejection = 0x00200000,	// Disable bound rejection in OccludeBSP (reuse Gourard flag)
	PF_Wireframe		= 0x00200000,	// Render as wireframe
	PF_Unlit			= 0x00400000,	// Unlit.
	PF_HighShadowDetail	= 0x00800000,	// High detail shadows.
	PF_Portal			= 0x04000000,	// Portal between iZones.
	PF_Mirrored			= 0x08000000,	// Reflective surface.

	// Editor flags.
	PF_Memorized     	= 0x01000000,	// Editor: Poly is remembered.
	PF_Selected      	= 0x02000000,	// Editor: Poly is selected.
	PF_Highlighted      = 0x10000000,	// Editor: Poly is highlighted.   
	PF_FlatShaded		= 0x40000000,	// FPoly has been split by SplitPolyWithPlane.   

	// Internal.
	PF_EdProcessed 		= 0x40000000,	// FPoly was already processed in editorBuildFPolys.
	PF_EdCut       		= 0x80000000,	// FPoly has been split by SplitPolyWithPlane.  
	PF_RenderFog		= 0x40000000,	// Render with fogmapping.
	PF_Occlude			= 0x80000000,	// Occludes even if PF_NoOcclude.
	PF_RenderHint       = 0x01000000,   // Rendering optimization hint.

	// Combinations of flags.
	PF_NoOcclude		= PF_Masked | PF_Translucent | PF_Invisible | PF_Modulated | PF_AlphaTexture,
	PF_NoEdit			= PF_Memorized | PF_Selected | PF_EdProcessed | PF_NoMerge | PF_EdCut,
	PF_NoImport			= PF_NoEdit | PF_NoMerge | PF_Memorized | PF_Selected | PF_EdProcessed | PF_EdCut,
	PF_AddLast			= PF_Semisolid | PF_NotSolid,
	PF_NoAddToBSP		= PF_EdCut | PF_EdProcessed | PF_Selected | PF_Memorized,
	PF_NoShadows		= PF_Unlit | PF_Invisible | PF_Environment | PF_FakeBackdrop,
	PF_Transient		= PF_Highlighted,
};

//
// Most of these structs are mirrored in Unreal's "UnSkeletal.h" and need to stay binary compatible with Unreal's 
// skeletal data import routines.
//

// File header structure. 
struct VChunkHdr
{
	char		ChunkID[20];  // string ID of up to 19 chars (usually zero-terminated?)
	int			TypeFlag;     // Flags/reserved/Version number...
    int         DataSize;     // size per struct following;
	int         DataCount;    // number of structs/
};


// A Material
struct VMaterial
{
	char		MaterialName[64]; // Straightforward ascii array, for binary input.
	int         TextureIndex;     // multi/sub texture index 
	DWORD		PolyFlags;        // all poly's with THIS material will have this flag.
	int         AuxMaterial;      // index into another material, eg. alpha/detailtexture/shininess/whatever
	DWORD		AuxFlags;		  // reserved: auxiliary flags 
	INT			LodBias;          // material-specific lod bias
	INT			LodStyle;         // material-specific lod style

	// Necessary to determine material uniqueness
	UBOOL operator==( const VMaterial& M ) const
	{
		UBOOL Match = true;
		if (TextureIndex != M.TextureIndex) Match = false;
		if (PolyFlags != M.PolyFlags) Match = false;
		if (AuxMaterial != M.AuxMaterial) Match = false;
		if (AuxFlags != M.AuxFlags) Match = false;
		if (LodBias != M.LodBias) Match = false;
		if (LodStyle != M.LodStyle) Match = false;

		for(INT c=0; c<64; c++)
		{
			if( MaterialName[c] != M.MaterialName[c] )
			{
				Match = false;
				break;
			}
		}
		return Match;
	}	

	// Copy a name and properly zero-terminate it.
	void SetName( char* NewName)
	{
		for(INT c=0; c<64; c++)
		{
			MaterialName[c]=NewName[c];
			if( MaterialName[c] == 0 ) 
				break;
		}
		// fill out
		while( c<64)
		{
			MaterialName[c] = 0;
			c++;
		}	
	}
};

struct VBitmapOrigin
{
	char RawBitmapName[MAX_PATH];
	char RawBitmapPath[MAX_PATH];
};

// A bone: an orientation, and a position, all relative to their parent.
struct VJointPos
{
	FQuat   	Orientation;  //
	FVector		Position;     //  needed or not ?

	FLOAT       Length;       //  For collision testing / debugging drawing...
	FLOAT       XSize;	      //
	FLOAT       YSize;        //
	FLOAT       ZSize;        //
};

//struct FNamedBoneBinary
struct VBone
{
	char	   Name[64];     // ANSICHAR   Name[64];	// Bone's name
	DWORD      Flags;		 // reserved
	INT        NumChildren;  //
	INT		   ParentIndex;	 // 0/NULL if this is the root bone.  
	VJointPos  BonePos;	     //
};

struct AnimInfoBinary
{
	AnimInfoBinary(void)	{ memset(this,0,sizeof(*this)); }
	ANSICHAR Name[64];     // Animation's name
	ANSICHAR Group[64];    // Animation's group name	

	INT TotalBones;           // TotalBones * NumRawFrames is number of animation keys to digest.

	INT RootInclude;          // 0 none 1 included 		
	INT KeyCompressionStyle;  // Reserved: variants in tradeoffs for compression.
	INT KeyQuotum;            // Max key quotum for compression	
	FLOAT KeyReduction;       // desired 
	FLOAT TrackTime;          // explicit - can be overridden by the animation rate
	FLOAT AnimRate;           // frames per second.
	INT StartBone;            // - Reserved: for partial animations.
	INT FirstRawFrame;        //
	INT NumRawFrames;         //
};

struct VQuatAnimKey
{
	VQuatAnimKey(void)	{ memset(this,0,sizeof(*this)); }
	FVector		Position;           // relative to parent.
	FQuat       Orientation;        // relative to parent.
	FLOAT       Time;				// The duration until the next key (end key wraps to first...)
};


struct VBoneInfIndex // ,, ,, contains Index, number of influences per bone (+ N detail level sizers! ..)
{
	_WORD WeightIndex;
	_WORD Detail0;  // how many to process if we only handle 1 master bone per vertex.
	_WORD Detail1;  // how many to process if we're up to 2 max influences
	_WORD Detail2;  // how many to process if we're up to full 3 max influences 

};

struct VBoneInfluence // Weight and vertex number
{
	_WORD PointIndex; // 3d vertex
	_WORD BoneWeight; // 0..1 scaled influence
};

struct VRawBoneInfluence // Just weight, vertex, and Bone, sorted later.
{
	FLOAT Weight;
	INT   PointIndex;
	INT   BoneIndex;
};

//
// Points: regular FVectors (for now..)
//
struct VPoint
{	
	FVector			Point;             //  change into packed integer later IF necessary, for 3x size reduction...
};


//
// Vertex with texturing info, akin to Hoppe's 'Wedge' concept.
//
struct VVertex
{
	WORD	PointIndex;	 // Index to a point.
	FLOAT   U,V;         // Engine may choose to store these as floats, words,bytes - but raw PSK file has full floats.
	BYTE    MatIndex;    // At runtime, this one will be implied by the vertex that's pointing to us.
	BYTE    Reserved;    // Top secret.
};

//
// Textured triangle.
//
struct VTriangle
{
	WORD    WedgeIndex[3];	 // point to three vertices in the vertex list.
	BYTE    MatIndex;	     // Materials can be anything.
	BYTE    AuxMatIndex;     // Second material (eg. damage skin, shininess, detail texture / detail mesh...
	DWORD   SmoothingGroups; // 32-bit flag for smoothing groups AND Lod-bias calculation.
};


//
// Physique (or other) skin.
//
struct VSkin
{
	TArray <VMaterial>			Materials; // Materials
	TArray <VPoint>				Points;    // 3D Points
	TArray <VVertex>			Wedges;  // Wedges
	TArray <VTriangle>			Faces;       // Faces
	TArray <VBone>				RefBones;   // reference skeleton
	TArray <void*>				RawMaterials; // alongside Materials - to access extra data //Mtl*
	TArray <VRawBoneInfluence>	RawWeights;  // Raw bone weights..
	TArray <VBitmapOrigin>      RawBMPaths;  // Full bitmap Paths for logging

	// Brushes: UV must embody the material size...
	TArray <INT>				MaterialUSize;
	TArray <INT>				MaterialVSize;
	
	int NumBones; // Explicit number of bones (?)
};



//
// Complete Animation for a skeleton.
// Independent-sized key tracks for each part of the skeleton.
//
struct VAnimation
{
	AnimInfoBinary        AnimInfo;   //  Generic animation info as saved for the engine
	TArray <VQuatAnimKey> KeyTrack;   //  Animation track - keys * bones
	VAnimation()
	{}
};

struct VAnimationList
{
	explicit VAnimationList(TArray<VAnimation> * list = NULL)
		: AnimList(list)
	{}

	TArray<VAnimation> *	AnimList;
};


#pragma pack( pop )

//
// The internal animation class, which can contain various amounts of
// (mesh+reference skeleton), and (skeletal animation sequences).
//
// Knows how to save a subset of the data as a valid Unreal2
// model/skin/skeleton/animation file.
//

class VActor
{
public:

	//
	// Some globals from the scene probe. 
	//
	INT		NodeCount;
	INT     MeshCount;

	FLOAT	FrameTotalTicks; 
	FLOAT   FrameRate; 

	// file stuff
	char*	LogFileName;
	char*   OutFileName;

	// Multiple skins: because not all skins have to be Physique skins; and
	// we also want hierarchical actors/animation (robots etc.)

	VSkin             SkinData;

	// Single skeleton digestion (reference)
	TArray <VBone>		RefSkeletonBones;
	TArray <AXNode*>	RefSkeletonNodes;     // The node* array for each bone index....

	// Raw animation, 'betakeys' structure
	int                   BetaNumBones;
	int                   BetaNumFrames; //#debug
	TArray<VQuatAnimKey>  BetaKeys;
	char                  BetaAnimName[64];


	// Array of digested Animations - all same bone count, variable frame count.
	TArray  <VAnimation> Animations; //digested;
	TArray  <VAnimation> OutAnims;   //queued for output.
	int		             AnimationBoneNumber; // Bone number consistency check..

public:
	int	MatchNodeToSkeletonIndex(AXNode* ANode);
	int IsSameVertex( VVertex &A, VVertex &B )
	{
		if (A.PointIndex != B.PointIndex) return 0;
		if (A.MatIndex != B.MatIndex) return 0;
		if (A.V != B.V) return 0;
		if (A.U != B.U) return 0;
		return 1;
	}


	// Ctor
	VActor();
	~VActor();
	void Cleanup();

	//
	// Save the actor: Physique mesh, plus reference skeleton.
	//

	int SerializeActor(FastFileClass &OutFile);

	// Save the Output animations. ( 'Reference' skeleton is just all the bone names.)
	int SerializeAnimation(FastFileClass &OutFile);

	// Load the Output animations. ( 'Reference' skeleton is just all the bone names.)
	int LoadAnimation(FastFileClass &InFile);

	// Add a current 'betakeys' animation data to our TempActor's in-memory repertoire.
	int RecordAnimation();

	DWORD FlagsFromName(char* pName );

	int ReflagMaterials();

	// Write out a brush to the file. Everything's in SkinData - ignore any weights etc.
	int WriteBrush(FastFileClass &OutFile, INT DoSmooth, INT OneTexture );
};

#endif






