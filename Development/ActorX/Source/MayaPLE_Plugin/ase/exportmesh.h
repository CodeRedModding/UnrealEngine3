//
// Copyright (C) 2002 Secret Level 
// 
// File: exportmesh.h
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


#pragma once

#include <maya\MFnMesh.h>

class CExportMesh
{
public:
	CExportMesh(MString const& name, MString& error);
	CExportMesh(MObject const& meshObject, MString& error);
	~CExportMesh(void);

	void	Reset(MObject const& meshObject, MString& error);

	MString const&	GetName(void) const	{return m_name;}
	const MFnMesh *	GetMesh(void) const	{return m_mesh;}

	MFnMesh *	SelectMesh(MObject const& dagObject, MString& error) const;


private:
	MString		m_name;
	MFnMesh	*	m_mesh;
};