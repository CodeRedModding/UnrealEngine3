/**************************************************************************
 
   Copyright (c) 1999,2000 Epic MegaGames, Inc. All Rights Reserved.

   SceneIFC.h	- File Exporter class definitions.

   Copyright (c) 1999,2000 Epic MegaGames, Inc. All Rights Reserved.

   Todo: clean up the **@#(*&( mess :)

***************************************************************************/

#ifndef __SceneIFC__H
#define __SceneIFC__H

//===========================================
// Build-specific includes:


//#include "MayaInterface.h"

#include "UnSkeletal.h"
#include "Vertebrate.h"

class SceneIFC;

// extern SceneIFC		OurScene;
// extern VActor       TempActor;

extern LogFile DLog;
extern LogFile AuxLog;

// extern char PluginRegPath[];
/*
extern char DestPath[MAX_PATH];
extern char LogPath[MAX_PATH];
extern char to_path[MAX_PATH];
extern char to_animfile[MAX_PATH];
extern char to_skinfile[MAX_PATH];

extern char animname[MAX_PATH];
extern char framerange[MAXINPUTCHARS];
extern char classname[MAX_PATH];
extern char basename[MAX_PATH];
extern char batchfoldername[MAX_PATH];
*/

//===========================================


class NodeInfo
{
public:
	AXNode*  node;
	int		IsBone;   // in the sense of a valid hierarchical node for physique...
	int     IsCulled;
	int		IsBiped;
	int		IsRoot;
	int     IsSmooth; // Any smooth-skinned mesh.
	int     IsSkin;
	int     IsDummy;
	int     IsMaxBone;
	int     LinksToSkin;
	int     HasMesh;
	int     HasTexture;
	int     IsSelected;
	int     IsRootBone;
	int     NoExport; // untie links from NoExport bones..

	int     InSkeleton;
	int     ModifierIdx; // Modifier for this node (Maya skincluster usage)
	int     IgnoreSubTree;

	// These are enough to recover the hierarchy.
	int  	ParentIndex;
	int     NumChildren;

	//ctor
	NodeInfo()
	{
		memset(this,0,sizeof(NodeInfo));
	}

	//~NodeInfo();
};


// Frame index sequence

class FrameSeq
{
public:
	INT			StartFrame;
	INT			EndFrame;
	TArray<INT> Frames;

	FrameSeq()
		: StartFrame(0), EndFrame(0)
	{
		
	}

	void AddFrame(INT i)
	{
		if( i>=StartFrame && i<=EndFrame) Frames.AddItem(i);
	}

	INT GetFrame(INT index)
	{
		return Frames[index];
	}

	INT GetTotal()
	{
		return Frames.Num();
	}

	void Empty()
	{
		Frames.Empty();
	}
};


struct SkinInf
{
	AXNode*	Node;
	INT		IsSmoothSkinned;
	INT		IsTextured;
	INT		SeparateMesh;
	INT		SceneIndex;
	INT     IsVertexAnimated;
};

class SceneIFC
{
public:
	//const class CUnExportCmd * 	m_client;
	MString			m_logpath;
	MFnIkJoint *	m_ikjoint;

	// Important globals
	int         TreeInitialized;
	
	void		SurveyScene();		
	int			GetSceneInfo(VActor * pTempActor);

	int         DigestSkeleton(VActor *Thing); 
	int         DigestSkin( VActor *Thing );
	int         DigestBrush( VActor *Thing );
	int         FixMaterials( VActor *Thing ); // internal
	int	        ProcessMesh(AXNode* SkinNode, int TreeIndex, VActor *Thing, VSkin& LocalSkin, INT SmoothSkin);
	int         LogSkinInfo( VActor *Thing, const char* SkinName);	
	int			WriteScriptFile( VActor *Thing, char* ScriptName, char* BaseName, char* SkinFileName, char* AnimFileName );
	int         DigestAnim(VActor *Thing, const char* AnimName, int from, int to );
	int         LogAnimInfo( VActor *Thing, char* AnimName);
	void        FixRootMotion( VActor *Thing );

	int			MarkBonesOfSystem(int RIndex);
	int         RecurseValidBones(int RIndex, int &BoneCount);

    // Modifier*   FindPhysiqueModifier(AXNode* nodePtr);
	int			EvaluateBone(int RIndex);
	// int         HasMesh(AXNode* node);
	int         HasTexture(AXNode* node);
	int         EvaluateSkeleton(INT RequireBones);
	int         DigestMaterial(AXNode *node, INT matIndex, TArray<void*> &MatList);

	bool		Depends(MObject const& tgt) const;


	AXNode*			 OurSkin;
	TArray <SkinInf> OurSkins;
	AXNode*      OurRootBone;
	int         RootBoneIndex;
	int         OurBoneTotal;


	// Simple array of nodes for the whole scene tree.
	TArray< NodeInfo > SerialTree; 

	// Physique skin data
	int         PhysiqueNodes;

	// Skeletal links to skin:
	int         LinkedBones;
	int         TotalSkinLinks;

	// Misc evaluation counters
	int         GeomMeshes;
	int         Hierarchies;
	int         TotalBones;
	int         TotalDummies;
	int         TotalBipBones;
	int         TotalMaxBones;
	// More stats:
	int         TotalVerts;
	int         TotalFaces;

	// Animation
	FLOAT   TimeStatic; // for evaluating any objects at '(0)' reference.
	FLOAT	FrameRange;
	FLOAT	FrameStart;
	FLOAT	FrameEnd;

	int			FrameTicks;
	int         OurTotalFrames;
	FLOAT       FrameRate; // frames per second !

	FrameSeq    FrameList; // Total frames == FrameList.Num()

	// Export switches from 'aux' panel.
	UBOOL	DoFixRoot, DoLockRoot, DoExplicitSequences, DoLog, DoPoseZero, CheckPersistSettings, CheckPersistPaths, QuickSaveDisk;
	UBOOL   DoTexGeom, DoSelectedGeom, DoSkipSelectedGeom, DoPhysGeom, DoCullDummies;
	UBOOL   DoTexPCX, DoQuadTex, DoVertexOut, InBatchMode;
	UBOOL   DoSmooth, DoSingleTex, DoForceRate;
	FLOAT   PersistentRate;

	// ctor/dtor
	explicit SceneIFC(MFnIkJoint * ikjoint, MString logpath = MString())
	{
		memset(this,0,sizeof(SceneIFC));
		m_ikjoint = ikjoint;
		m_logpath = logpath;
		TimeStatic = 1.0f; //0.0f ?!?!?
		DoLog = 1;
		DoPhysGeom = 1;		
		DoTexPCX = 1;

	}
	void Cleanup()
	{
		FrameList.Empty();

		SerialTree.Empty();
		OurSkins.Empty();
	}

	~SceneIFC()
	{
		Cleanup();
	}

	int	MatchNodeToIndex(AXNode* ANode)
	{
		for (int t=0; t<SerialTree.Num(); t++)
		{
			if ( (void*)(SerialTree[t].node) == (void*)ANode) return t;
		}
		return -1; // no matching node found.
	}

	int GetChildNodeIndex(int PIndex,int ChildNumber)
	{
		int Sibling = 0;
		for (int t=0; t<SerialTree.Num(); t++)
		{
			if (SerialTree[t].ParentIndex == PIndex) 
			{
				if( Sibling == ChildNumber ) 
				{	
					return t; // return Nth child.
				}
				Sibling++;
			}
		}
		return -1; // no matching node found.		
	}

	int	    GetParentNodeIndex(int CIndex)
	{
		return SerialTree[CIndex].ParentIndex; //-1 if none
	}

	bool	HaveLogPath(void) const	{return m_logpath.length() > 0;}

	private:
	void		StoreNodeTree(AXNode* node);
	int			SerializeSceneTree();
	void        ParseFrameRange(const char* pString, INT startFrame,INT endFrame);

};




#endif // __SceneIFC__H

