//
// Copyright (C) 2002 Secret Level 
// 
// File: unExportCmd.cpp
//
// MEL Command: unExport
//
// Author: Maya SDK Wizard
//

// Includes everything needed to register a simple MEL command with Maya.
// 
#include "precompiled.h"
#include "unExportCmd.h"

#include <maya/MGlobal.h>


CUnExportCmd::CUnExportCmd(MString const& name)
: m_name(name), m_set(0), m_setroot(0)
{
	m_arguments[setname].SetIndicator("-setname");
	m_arguments[logpath].SetIndicator("-logpath");
	m_arguments[to_path].SetIndicator("-path");
	m_arguments[animfile].SetIndicator("-animfile");
	m_arguments[skinfile].SetIndicator("-skinfile");
}


void	CUnExportCmd::ParseForNames( const MArgList& args )
{
	DestroySetPointers();

	// clear all first
	for (unsigned int j = 0; j < num_args; j++) {
		m_arguments[j].ClearValue();
	}


	// arg[0] is the command name
	for (unsigned int i = 0; i < args.length(); i++) { // i
		MString arg_i = args.asString(i);
		bool found = false;
		for (unsigned int j = 0; !found && j < num_args; j++) { // j
			if (arg_i == m_arguments[j].GetIndicator()) {
				m_arguments[j].SetValue( args.asString(++i) );
				found = true;
			}
		} // j
	} // i

	// user-friendly stuff
	//----------------------------------------------------------------
	MString path = GetToPath();
	if (path.length() > 0 ) {
		// check final character of path
		unsigned int len = path.length();
		char last_char = path.asChar()[len-1];
		if (last_char != '/' && last_char != '\\') {
			path += "/";
			SetToPath(path);
		}
	}

	MString logpath = GetLogPath();
	if (logpath.length() > 0 ) {
		// check final character of path
		unsigned int len = logpath.length();
		char last_char = logpath.asChar()[len-1];
		if (last_char != '/' && last_char != '\\') {
			logpath += "/";
			SetLogPath(logpath);
		}
	}

	MString animfile = GetAnimFile();
	// removing trailing .psa
	if (animfile.length() > 4) {
		unsigned int len = animfile.length();
		if (animfile.substring(len-4,len-1) == ".psa") {
			animfile = animfile.substring(0,len-5);
			SetAnimFile(animfile);
		}
	}

	MString skinfile = GetSkinFile();
	// removing trailing .psk
	if (skinfile.length() > 4) {
		unsigned int len = skinfile.length();
		MString msubstr = skinfile.substring(len-4,len-1);
		const char * substr = msubstr.asChar();
		if (skinfile.substring(len-4,len-1) == ".psk") {
			skinfile = skinfile.substring(0,len-5);
			SetSkinFile(skinfile);
		}
	}
}


MStatus	CUnExportCmd::DetermineSetInfo(void)
{
	DestroySetPointers();
	if (GetSetName().length() == 0) {
		// want full monty
		return MS::kSuccess;
	}

	//
	// Iterate over all nodes in the dependency graph. Only some dependency graph nodes are/have actual DAG nodes.
	// 	

	MSelectionList selectionList;
	selectionList.add(GetSetName());
	if(selectionList.length() != 1)
	{
		// TODO: report the error that we didn't get any one thing that
		//  matches the search string
		setResult("no object matches " + GetSetName());
		return MS::kSuccess;
	}
	MObject setObject;
	if(selectionList.getDependNode(0, setObject) != MS::kSuccess)
	{
		// TODO: report that we failed to get the node
		setResult("failed to make selectionList");
		return MS::kSuccess;
	}

	MStatus stat;

	// attempt 
	MFnSet set(setObject, &stat);
	if(stat != MS::kSuccess)
	{
		// TODO: report that the specified item is not, in fact a set
		setResult("failed to make MFnSet");
		return MS::kSuccess;
	}

	// define the set
	m_set = new MFnSet(set);

	MSelectionList setMembers;
	// attempt to get the members of the set
	if(set.getMembers(setMembers, false) != MS::kSuccess)
	{
		// TODO: report failure
		setResult("failed to make MSelectionList");
		return MS::kSuccess;
	}

	if(setMembers.length() != 1)
	{
		// TODO: report failure to have one and only one joint node in the set
		setResult("setMembers.length() != 1");
		return MS::kSuccess;
	}

	MObject memberObject;
	// extract the single joint node in the set
	setMembers.getDependNode(0, memberObject);

	MFnIkJoint rootJoint(memberObject, &stat);
	if(stat != MS::kSuccess)
	{
		// TODO: report that the object in the set was not actually a joint!
		setResult("member is not a root joint");
		return MS::kSuccess;
	}

	setResult("have correct set-up");

	// define the set root
	m_setroot = new MFnIkJoint(rootJoint);

	OrientRoot();

	return MS::kSuccess;
}

void CUnExportCmd::DestroySetPointers(void)
{
	if (m_set) {
		delete m_set;
	}
	m_set = 0;
	if (m_setroot) {
		delete m_setroot;
	}
	m_setroot = 0;
}


MStatus CAboutCmd::doIt( const MArgList& args )
{
	const char * blurb [] =
	{
		"unExport",
		"Modification of ActorXTool",
		"Copyright (c) 2000, 2001, 2002 Epic MegaGames, Inc. ",
		"By Secret Level",
		NULL
	};

	MString res;

	const char ** line = blurb;
	while (*line) {
		if (res.length() > 0) {
			res += "\n   Result: ";
		}
		res += *line;
		line++;
	}

	setResult(res);
	return MS::kSuccess;
}




MStatus CHelpCmd::doIt( const MArgList& args )
{
	const char * blurb [] =
	{
		"unExport is a modification by Secret Level of ActorXTool (by Epic)",
		"that exports meshes and animations from Maya into a format friendly",
		"to UnReal. unExport operates from the command-line.",
		"USAGE: unExport [<mode>] ",
		"         [-setname <setname>]",
		"         [-logpath <logpath>]",
		"         [-path <path>]",
		"         [-skinfile <skinfile>]",
		"         [-animfile <animfile>]",
		"         (-digest <label> <from> <to>)*",
		" where <mode> is \"about\", \"auto\", \"skin\", \"anim\" (default is \"auto\").",
		"For more information - use \"unExport about <keyword>\" where <keyword>",
		"is \"skin\", \"anim\", \"auto\", \"flags\".",
		NULL
	};

	const char * topic_skin [] =
	{
		"===================",
		"unExport help skin",
		"===================",
		"Exports a mesh to a .psk file",
		"The file will be in location \"<path><skinfile>.psk\"",
		"The choice of mesh will be from a root joint that is the",
		"sole joint member of set <setname>. In the instance that no",
		"set has been specified, the first available root will be used.",
		"Mandatory information is <path> and <skinfile>.",
		NULL
	};

	const char * topic_anim [] =
	{
		"===================",
		"unExport help anim",
		"===================",
		"Exports an animation to a .psa file",
		"The file will be in location \"<path><animfile>.psk\"",
		"The choice of mesh being animated will be from a root joint",
		"that is the sole joint member of set <setname>. In the instance",
		"that no set has been specified, the first available root will",
		"be used.",
		"The frame ranges to be used are indicated by an arbitrary number",
		"of (-digest <label> <from> <to>) combinations. Each exports the",
		"indicated frame range (<from> - <to>) with the label <label>.",
		"Mandatory information is <path> and <animfile>.",
		NULL
	};

	const char * topic_auto [] =
	{
		"===================",
		"unExport help auto",
		"===================",
		"Scans the scene for sets with boolean attributes \"pskExport\"",
		"and/or \"psaExport\", indicating export of a \".psk\" or \".psa\"",
		"respectively.",
		"",
		" <set>	       - pskExport == true / false            ** mandatory for .psk",
		"              - psaExport == true / false	          ** mandatory for .psa",
		"              - animargs == (<label> <from> <to>)*   ** mandatory for .psa",
		"              - the root-joint for the mesh being exported",
		"A set with attribute \"pskExport\" == \"true\" executes the command-line",
		"> unExport skin -setname <set> -skinfile <set>",
		"A set with attribute \"psaExport\" == \"true\" executes the command-line",
		"> unExport anim -setname <set> -animfile <set> <digest-flags>",
		"where <digest-flags> are of the form (-digest <label> <from> <to>)",
		"corresponding to the (<label> <from> <to>) trios in attribute \"animargs\".",
		"NOTE: Any arguments stored in the set other than \"animargs\" is overridden by the",
		"corresponding value specified from the command-line. This is so that the batched",
		"version of this command can override attributes placed for ease of use while",
		"prototyping.",
		NULL
	};

	const char * topic_flags [] =
	{
		"====================",
		"unExport help flags",
		"====================",
		"-setname <setname>:",
		"    Indicates a set that contains the root-joint of the mesh to be",
		"    exported - and no other joints. If a scene contains only one",
		"    root-joint, <setname> may be omitted, otherwise, the first root-joint",
		"    found will be used which is perhaps unpredictable.",
		"-logpath <logpath>:",
		"    A location for temporary log-files to be placed, if debugging is",
		"    required through the export process.",
		"-path <path>:",
		"    The path where corresponding .psa or .psk files should be placed",
		"    Default value is \".\\\"",
		"-skinfile <skinfile>:",
		"    The name of the .psk (mesh) file to be generated. Ignored in auto",
		"-animfile <animfile>:",
		"    The name of the .psa (animation) file to be generated. Ignored in auto",
		"-digest <label> <from> <to>:",
		"    Trios indicating an animation frame range, and the label it is",
		"    to be given on export.",
		NULL
	};
	
	MString res;
	
	for (unsigned int i = 0; i < args.length(); i++) { // i
		MString arg_i = args.asString(i);
		const char ** topic = 0;
		if (arg_i == "skin") {
			topic = topic_skin;
		} else if (arg_i == "anim") {
			topic = topic_anim;
		} else if (arg_i == "auto") {
			topic = topic_auto;
		} else if (arg_i == "flags") {
			topic = topic_flags;
		}
		if (topic) {
			while (*topic) {
				if (res.length() > 0) {
					res += "\n   Result: ";
				}
				res += *topic;
				topic++;
			}
		}
	}

	if (res.length() == 0) {
		const char ** line = blurb;
		while (*line) {
			if (res.length() > 0) {
				res += "\n   Result: ";
			}
			res += *line;
			line++;
		}
	}

	setResult(res);
	return MS::kSuccess;
}


void		CUnExportCmd::OrientRoot(void)
{
	if (GetSetRoot() == 0)
		return;
	
	// construct the list of commands
	const char * root_name = GetSetRoot()->name().asChar();
	const char format_cmd [] =
		"// set to frame 0\n"
		"currentTime -edit 0;\n"
		"// must have root joint selected to revert bindpose\n"
		"select -r \"%s\";\n"
		"// get the bindPose node\n"
		"string $bindPose[]=`dagPose -q -bp \"%s\"`;\n"
		"// check if it's already at bindpose\n"
		"string $isAtPose[]=`dagPose -q -ap $bindPose[0]`;\n"
		"// if not, revert to bindpose\n"
		"if (`size($isAtPose)` > 0)\n"
		"dagPose -r -g -bp;\n"
		"// set rotations to orient Z-up\n"
		"setAttr (\"%s\" + \".rx\") 90;\n"
		"setAttr (\"%s\" + \".rz\") 90;\n";
	char bind_pose_cmd[1024];
	_snprintf(bind_pose_cmd,1024,format_cmd,root_name,root_name,root_name,root_name);

	MStatus res = MGlobal::executeCommand(bind_pose_cmd);
}
