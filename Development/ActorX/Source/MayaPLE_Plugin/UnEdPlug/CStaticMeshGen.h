//
// Copyright (C) 2002 Secret Level 
// 
// File: CStaticMeshGen.cpp
//
// Author: Michael Arnold
//
// Description: Generates the stuff required by un-real from Maya, mesh info
//				Used by progressively adding meshes with "AddMesh", culminating
//				in the UnEditor::FMesh structure used by "UNREALED_API void LoadMesh"
//
// 

#pragma once

#include <maya/MFnMesh.h>

#include "Vertebrate.h"
#include "UnrealEdImport.h"

class CStaticMeshGenerator
{
public:
	CStaticMeshGenerator(void);

	void	AddMesh(const MFnMesh& mesh);
	UnEditor::FMesh const&	GetResult(void) const	{return m_result;}

	// SL MODIFY
	void	Scale(float scale);

private:
	struct MeshAppend
	{
		MeshAppend(const MFnMesh& mesh)
			: m_mesh(mesh)	{}

		MFnMesh			m_mesh;
		unsigned int	m_num_materials;
		unsigned int	m_num_points;
		unsigned int	m_num_tpoints;
		unsigned int	m_num_faces;
	};

	void	DetermineMaterials(MeshAppend const& addmesh);
	void	GeneratePoints(MeshAppend const& addmesh);
	void	GenerateTPoints(MeshAppend const& addmesh);
	void	GenerateFaces(MeshAppend const& addmesh);

private:
	UnEditor::FMesh		m_result;
};