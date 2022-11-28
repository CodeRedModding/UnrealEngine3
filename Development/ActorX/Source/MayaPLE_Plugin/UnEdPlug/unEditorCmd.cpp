//
// Copyright (C) 2002 Secret Level 
// 
// File: unEditorCmd.cpp
//
// MEL Command: unEditor
//
// Author: Maya SDK Wizard
//
// Description: Implementation of plugin interface for unEditor.mll
// 

// Includes everything needed to register a simple MEL command with Maya.
// 
//#include <maya/MSimple.h>
#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include "precompiled.h"

#include "DynArray.h"
#include "Vertebrate.h"

#include "UnrealEdImport.h"

#include "CSetInfo.h"
#include "SceneIFC.h"
#include "maattrval.h"
#include <maya/MStringArray.h>
#include <maya/MEventMessage.h>

#include <maya/MFnMesh.h>
#include "ExportMesh.h"
#include "CStaticMeshGen.h"

// Use helper macro to register a command with Maya.  It creates and
// registers a command that does not support undo or redo.  The 
// created class derives off of MPxCommand.
//

class unEditor : public MPxCommand {							
public:															
	unEditor(void)			
	{}
	virtual ~unEditor(void)	
	{}

	virtual MStatus	doIt ( const MArgList& );					
	static void*	creator();

	void	doAnim( const MArgList& );
	void	doMesh( const MArgList& );

	static void				idleCB(void * data);
	static void				onQuit(void * data);


	static MCallbackId		idleCallbackId;
	static MCallbackId		quitCallbackId;

	static void	onOpen(void);
	static void	onClose(void);

private:
	INT 	GenerateActor(const CSetInfo& setinfo) const;
	INT		GenerateMesh(const MFnMesh& mesh, const CSetInfo& setinfo) const;
	INT 	GenerateMesh(const CSetInfo& setinfo) const;
	void	PrepareOutAnimation(VActor& vactor) const;
	void	TransferSkinToUnreal(VActor& vactor, const CSetInfo& setinfo) const;
	void	TransferAnimToUnreal(VActor& vactor, const CSetInfo& setinfo) const;
};
															
void* unEditor::creator()										
{																
	return new unEditor;										
}																

MStatus initializePlugin( MObject _obj )						
{																
	MFnPlugin	plugin( _obj, "Secret Level", "4.0" );				
	MStatus		stat;											
	unEditor::onOpen(); 
	stat = plugin.registerCommand( "unEditor",					
	                                unEditor::creator );	    
	if ( !stat )												
		stat.perror("registerCommand");							
	return stat;												
}																

// Warning: Currently, if you return MS::kSuccess, Maya crashes. 
// The result here is not much better (Maya closes down), but at least
// there is no erroneous dialog box
MStatus uninitializePlugin( MObject _obj )						
{		
	unEditor::onClose();
	return MS::kFailure;
	/*
	MFnPlugin	plugin( _obj );									
	MStatus		stat;
	unEditor::onClose();
	stat = plugin.deregisterCommand( "unEditor" );				
	if ( !stat )												
		stat.perror("deregisterCommand");						
	return stat;
	*/
}


// Idle calls go through to unreal editor -to make unrealed update its windows, play animations, etc.
void	unEditor::idleCB(void * data)
{
	PollEditor();
}

void	unEditor::onQuit(void * data)
{
	onClose();
}

MCallbackId		unEditor::idleCallbackId = 0;
MCallbackId		unEditor::quitCallbackId = 0;

void	unEditor::onOpen(void)
{
	if (!IsLaunched()) 
	{		
		HMODULE hmodule = GetModuleHandle("UnrealEdDLL");				
		if( hmodule )
		{
			LaunchEditor( hmodule, 0,SW_SHOW);
			idleCallbackId = MEventMessage::addEventCallback("idle",&idleCB,NULL);
			quitCallbackId = MEventMessage::addEventCallback("quitApplication",&onQuit,NULL);
		}
	}
}

void	unEditor::onClose(void)
{
	if (idleCallbackId) 
	{
		MMessage::removeCallback(idleCallbackId);
		idleCallbackId = 0;
		MMessage::removeCallback(quitCallbackId);
		quitCallbackId = 0;
	}
	ShutDownEditor();	
}


MStatus unEditor::doIt( const MArgList& args )
//
//	Description:
//		implements the MEL unEditor command.
//
//	Arguments:
//		args - the argument list that was passes to the command from MEL
//
//	Return Value:
//		MS::kSuccess - command succeeded
//		MS::kFailure - command failed (returning this value will cause the 
//                     MEL script that is being run to terminate unless the
//                     error is caught using a "catch" statement.
//
{
	MStatus stat = MS::kSuccess;

	setResult( "unEditor command executed!\n" );

	if (IsLaunched() && !IsShutdown() && args.length() > 0) 
	{
		if (args.asString(0) == MString("anim")) 
		{
			doAnim(args);
		} 
		else if (args.asString(0) == MString("mesh")) 
		{
			doMesh(args);
		} 
		else if (args.asString(0) == MString("help") || args.asString(0) == MString("-help")) 
		{
			MString res("\nunEditor help\n=============\n");
			res += MString("usage:\n");
			res += MString("\t> unEditor anim (<setname>)*\n");
			res += MString("where each set mentioned contains exactly the main joint\n");
			res += MString("for an IK skeleton.\n");
			res += MString("This set should have a string attribute \"package\"\n");
			res += MString("to indicate which package the skin+animations should\n");
			res += MString("export to. The object name will be the set name.\n");
			res += MString("A string attribute \"animargs\" communicates the animations\n");
			res += MString("to be exported - this string should be trios of\n");
			res += MString("<label> <from> <to> such as \"walk 1 21 run 22 45\".\n");
			res += MString("An optional float attribute \"scale\" (default 1.0)\n");
			res += MString("is used to control the size of the mesh when imported\n");
			res += MString("\n");
			res += MString("\t> unEditor mesh (<setname>)*\n");
			res += MString("Each set here contains a textured mesh.\n");
			res += MString("Scale and package name are determined as for \"unEditor anim\" \n");
			setResult(res);
		}
		
	}
	else 
	{
		MString res("\nunEditor warning\n=============\n");
		res += MString("Once closed, UnrealEd cannot be re-launched\n");
		res += MString("To use this feature, restart Maya.\n");
		setResult(res);
	}
	return stat;
}


void unEditor::doAnim( const MArgList& args)
{
	int ActorCount = 0;

	for (unsigned int i = 1; i < args.length(); ++i) 
	{
		CSetInfo set_i( args.asString(i) );
		if (set_i.GetSet()) 
		{
			if( GenerateActor(set_i) ) ActorCount++;
		}
	}
	
	FinishAnimImport();

	char resultString[1024];
	if( ActorCount )
	{		
		sprintf(resultString," [%i] animating meshes(s) exported.",ActorCount);	
	}
	else
	{
		sprintf(resultString,"unEditor anim : model data not found or main set not linked correctly.\n");
	}

	setResult(resultString);
	
}

void unEditor::doMesh( const MArgList& args )
{
	int MeshCount = 0;

	for (unsigned int i = 1; i < args.length(); ++i) 
	{
		MString err;
		const char * aschar = args.asString(i).asChar();
		CSetInfo set_i( args.asString(i) );
		if (set_i.GetSet()) 
		{			
			if( GenerateMesh(set_i) ) MeshCount++;			
		}
	}

	FinishMeshImport();

	char resultString[1024];
	if( MeshCount )
	{		
		sprintf(resultString," [%i] static meshes(s) exported.",MeshCount);	
	}
	else
	{
		sprintf(resultString,"unEditor mesh : model data not found or main set not linked correctly.\n");
	}	

	setResult(resultString);
}

int	unEditor::GenerateActor(const CSetInfo& setinfo) const
{
	VActor vactor;

	SceneIFC OurScene(setinfo.GetRoot());

	OurScene.SurveyScene();
	
	OurScene.GetSceneInfo(&vactor);

	OurScene.EvaluateSkeleton(1);

	OurScene.DigestSkeleton( &vactor );  // Digest skeleton into tempactor

	// Skin
	OurScene.DigestSkin( &vactor );

	{
		// run check 
		int count = 0;
		for (int i = 0; i < vactor.SkinData.Faces.ArrayNum; ++i) 
		{
			VTriangle const& face_i = vactor.SkinData.Faces[i];
			VVertex& vert = vactor.SkinData.Wedges[ face_i.WedgeIndex[0] ];
			if (vert.MatIndex != face_i.MatIndex) 
			{
				vert.MatIndex = face_i.MatIndex;
			}
		}
	}

	// Collate bones
	vactor.SkinData.RefBones = vactor.RefSkeletonBones;

	INT NumBones = vactor.RefSkeletonBones.ArrayNum;

	TransferSkinToUnreal( vactor, setinfo );
	
	MString animargs;
	if (MGetAttribute(*setinfo.GetNode(),"animargs").GetBasicValue(animargs) == MS::kSuccess) 
	{ // obtain animation arguments
		MStringArray splitargs;
		animargs.split(' ',splitargs);
		for (unsigned int i = 0; i+2 < splitargs.length(); i += 3) 
		{
			MString label = splitargs[i];
			if (splitargs[i+1].isInt() && splitargs[i+2].isInt()) 
			{
				int from = splitargs[i+1].asInt();
				int to = splitargs[i+2].asInt();

				OurScene.DigestAnim(&vactor, label.asChar(), from, to);  

				OurScene.FixRootMotion( &vactor );

				// PopupBox("Digested animation:%s Betakeys: %i Current animation # %i", animname, TempActor.BetaKeys.Num(), TempActor.Animations.Num() );
				if(OurScene.DoForceRate )
					vactor.FrameRate = OurScene.PersistentRate;

				vactor.RecordAnimation();
			}
		}

		PrepareOutAnimation(vactor);

	} // Obtain animation arguments.


	TransferAnimToUnreal( vactor, setinfo );

	return ( (NumBones > 0)? 1 : 0 );
}

void unEditor::PrepareOutAnimation(VActor& vactor) const
{
	int NameUnique = 1;
	int ReplaceIndex = -1;
	int ConsistentBones = 1;

	// Go over animations in 'inbox' and copy or add to outbox.
	for( INT c=0; c<vactor.Animations.Num(); c++)
	{
		INT AnimIdx = c;

		// Name uniqueness check as well as bonenum-check for every sequence; _overwrite_ if a name already exists...
		for( INT t=0;t<vactor.OutAnims.Num(); t++)
		{
			if( strcmp( vactor.OutAnims[t].AnimInfo.Name, vactor.Animations[AnimIdx].AnimInfo.Name ) == 0)
			{
				NameUnique = 0;
				ReplaceIndex = t;
			}

			if( vactor.OutAnims[t].AnimInfo.TotalBones != vactor.Animations[AnimIdx].AnimInfo.TotalBones)
				ConsistentBones = 0;
		}

		// Add or replace.
		if( ConsistentBones )
		{
			INT NewIdx = 0;
			if( NameUnique ) // Add -
			{
				NewIdx = vactor.OutAnims.Num();
				vactor.OutAnims.AddZeroed(1); // Add a single element.
			}
			else // Replace.. delete existing.
			{
				NewIdx = ReplaceIndex;
				vactor.OutAnims[NewIdx].KeyTrack.Empty();
			}

			vactor.OutAnims[NewIdx].AnimInfo = vactor.Animations[AnimIdx].AnimInfo;
			
			vactor.OutAnims[NewIdx].KeyTrack.Append( vactor.Animations[AnimIdx].KeyTrack );
		}
		else 
		{							
			setResult("ERROR !! Aborting the quicksave/move, inconsistent bone counts detected.");
			return;
		}						
	}

	// Delete 'left-box' items.
	if( ConsistentBones )
	{
		// Clear 'left pane' anims..
		{for( INT c=0; c<vactor.Animations.Num(); c++)
		{
			vactor.Animations[c].KeyTrack.Empty();
		}}
		vactor.Animations.Empty();
	}
}


void unEditor::TransferSkinToUnreal(VActor& vactor, const CSetInfo& setinfo) const
{
	// determine parameters
	double scale = 1.0;
	MString package_name("MyPackage");
	MString skin_name(setinfo.GetName());

	if (setinfo.GetNode()) 
	{
		MGetAttribute( *setinfo.GetNode(), "scale" ).GetBasicValue(scale);
		MGetAttribute( *setinfo.GetNode(), "package" ).GetBasicValue(package_name);
	}

	LoadSkin( package_name.asChar(), skin_name.asChar(), &vactor.SkinData, scale );
}

void unEditor::TransferAnimToUnreal(VActor& vactor, const CSetInfo& setinfo) const
{
	// determine parameters
	MString package_name("MyPackage");
	MString skin_name(setinfo.GetName());

	if (setinfo.GetNode()) 
	{
		MGetAttribute( *setinfo.GetNode(), "package" ).GetBasicValue(package_name);
	}

	VAnimationList animlist(&vactor.OutAnims);

	LoadAnimations( package_name.asChar(), skin_name.asChar(), &vactor.SkinData, &animlist );
}

int unEditor::GenerateMesh(const MFnMesh& mesh, const CSetInfo& setinfo) const
{
	CStaticMeshGenerator staticmesh;
	staticmesh.AddMesh(mesh);

	MString package_name("MyPackage");
	MString mesh_name(setinfo.GetName());

	int PackageFound = 0;
	
	bool have_group = false;
	MString group_name;

	if (setinfo.GetNode()) 
	{		
		if( MGetAttribute( *setinfo.GetNode(), "package" ).GetBasicValue(package_name) == MS::kSuccess )
		{
			PackageFound = 1;
		}
		have_group = (MS::kSuccess == MGetAttribute( *setinfo.GetNode(), "group" ).GetBasicValue(group_name));
	}

	if ( have_group && PackageFound ) 
	{
		LoadMesh( package_name.asChar(), group_name.asChar(), mesh_name.asChar(), &staticmesh.GetResult() );
	} 
	else 
	{
		LoadMesh( package_name.asChar(), 0, mesh_name.asChar(), &staticmesh.GetResult() );
	}

	return PackageFound;
}

int unEditor::GenerateMesh(const CSetInfo& setinfo) const
{
	CStaticMeshGenerator staticmesh;

	MSelectionList setMembers;

	// Attempt to get the members of the set.
	if(setinfo.GetSet()->getMembers(setMembers, false) != MS::kSuccess)
	{
		return 0;
	}
	
	for (unsigned int i = 0; i < setMembers.length(); ++i) 
	{ 
		MObject node_i;
		setMembers.getDependNode(i,node_i);
		MString err;
		CExportMesh mesh_i(node_i,err);
		if (mesh_i.GetMesh()) 
		{
			staticmesh.AddMesh(*mesh_i.GetMesh());
		}
	} 

	MString package_name("MyPackage");
	MString mesh_name(setinfo.GetName());
	
	bool have_group = false;
	MString group_name;

	if (setinfo.GetNode()) 
	{
		MGetAttribute( *setinfo.GetNode(), "package" ).GetBasicValue(package_name);
		have_group = (MS::kSuccess == MGetAttribute( *setinfo.GetNode(), "group" ).GetBasicValue(group_name));

		float scale = 1.0f;
		if (MS::kSuccess == MGetAttribute( *setinfo.GetNode(), "scale" ).GetBasicValue(scale))
		{
			staticmesh.Scale(scale);
		}		
	}

	if (have_group) 
	{
		LoadMesh( package_name.asChar(), group_name.asChar(), mesh_name.asChar(), &staticmesh.GetResult() );
	} 
	else 
	{
		LoadMesh( package_name.asChar(), 0, mesh_name.asChar(), &staticmesh.GetResult() );	
	}

	return 1;

}