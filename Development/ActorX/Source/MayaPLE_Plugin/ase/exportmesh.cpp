//
// Copyright (C) 2002 Secret Level 
// 
// File: exportmesh.cpp
//
// Author: Michael Arnold
//
// Description: Extracts an appropriate mesh from a set, or mesh, or transform,
//              so as to simplify the process of selection for artists indicating
//              which mesh they refer to.
//              In UnrealEd, multiple meshes are selectable, so this is used on each
//              member of a set.
//
// 


#include "precompiled.h"
#include "exportmesh.h"
#include <maya/MSelectionList.h>
#include <maya/MFnSet.h>
#include <maya/MSelectionList.h>
#include <maya/MFnTransform.h>
#include <maya/MItDag.h>

CExportMesh::CExportMesh(MString const& name, MString& error)
: m_name(name), m_mesh(0)
{
	MSelectionList selectionList;
	selectionList.add(GetName());

	if(selectionList.length() != 1)
	{
		error = MString("no object matches " + GetName());
		return;
	}
	MObject meshObject;
	if(selectionList.getDependNode(0, meshObject) != MS::kSuccess)
	{
		error = MString("failed to make selectionList");
		return;
	}

	const char * type = meshObject.apiTypeStr();

	m_mesh = SelectMesh(meshObject, error);
	if(m_mesh == 0)
	{
		return;
	}
}

CExportMesh::CExportMesh(MObject const& meshObject, MString& error)
: m_mesh(0)
{
	Reset(meshObject,error);
}



CExportMesh::~CExportMesh(void)
{
	if (m_mesh) {
		delete m_mesh;
	}
	m_mesh = 0;
}


void	CExportMesh::Reset(MObject const& meshObject, MString& error)
{
	m_name = "";
	m_mesh = SelectMesh(meshObject, error);
	if(m_mesh == 0)
	{
		return;
	}

	m_name = m_mesh->name();
}

MFnMesh *	CExportMesh::SelectMesh(MObject const& dagObject, MString& error) const
{
	MStatus stat;
	MFnMesh result(dagObject,&stat);
	if (stat == MS::kSuccess) {
		return new MFnMesh(dagObject);
	}

	MFnSet asSet(dagObject,&stat);
	if (stat == MS::kSuccess) {
		MSelectionList setMembers;
		// attempt to get the members of the set
		if(asSet.getMembers(setMembers, false) != MS::kSuccess)
		{
			// TODO: report failure
			error = "failed to make MSelectionList from " + asSet.name();
			return 0;
		}

		if(setMembers.length() != 1)
		{
			error = asSet.name() + "::setMembers.length() != 1";
			return 0;
		}

		MObject memberObject;
		// extract the single joint node in the set
		setMembers.getDependNode(0, memberObject);

		// recursive call
		return SelectMesh(memberObject,error);
	}

	// tackle transform
	MFnTransform trans(dagObject,&stat);
	if (stat == MS::kSuccess) {
		MItDag diter;
		diter.reset( dagObject, MItDag::kBreadthFirst, MFn::kMesh );
		MObject res = diter.item(&stat);
		if (stat == MS::kSuccess) {
			return SelectMesh(res,error);
		} else {
			error = "failed to find mesh under transform " + trans.name();
			return 0;
		}
	}

	error = "failed to locate mesh";
	return 0;
}
