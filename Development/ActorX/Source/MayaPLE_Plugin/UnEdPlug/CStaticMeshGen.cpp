//
// Copyright (C) 2002 Secret Level 
// 
// File: CStaticMeshGen.cpp
//
// Author: Michael Arnold
//
// Description: Implementation of class CStaticMeshGenerator.
//				Extracts information from a mesh in Maya in the
//				format understool by UnrealEdDLL.dll



#include "precompiled.h"
#include "CStaticMeshGen.h"
#include "maattrval.h"
#include <maya\MGlobal.h>
#include <maya\MItMeshPolygon.h>

using namespace UnEditor;

CStaticMeshGenerator::CStaticMeshGenerator(void)
{}

void	CStaticMeshGenerator::AddMesh(const MFnMesh& mesh)
{
	MeshAppend append(mesh);
	append.m_num_faces = m_result.Faces.Num();
	append.m_num_materials = m_result.Materials.Num();
	append.m_num_points = m_result.Points.Num();
	append.m_num_tpoints = m_result.TPoints.Num();

	DetermineMaterials(append);
	GeneratePoints(append);
	GenerateTPoints(append);
	GenerateFaces(append);
}


void	CStaticMeshGenerator::DetermineMaterials(MeshAppend const& addmesh)
{
	MObjectArray shaders;
	MIntArray indices;

	bool have_material_nums = (addmesh.m_mesh.getConnectedShaders(0,shaders,indices) == MS::kSuccess);

	if (have_material_nums) {
		m_result.Materials.Add( shaders.length() );

		extern void CalcMaterialFlags( MObject& MShaderObject, VMaterial& Stuff );
		
		for (unsigned int i = 0; i < shaders.length(); ++i) {
			unsigned int mat_ix = addmesh.m_num_materials + i;
			MFnDependencyNode shader_i( shaders[i] );
			strcpy( m_result.Materials[mat_ix].MaterialName , shader_i.name().asChar() );

			// check for attribute
			MString matname;
			if (MGetAttribute(shader_i,"URMaterialName").GetBasicValue(matname) == MS::kSuccess) {
				strcpy( m_result.Materials[mat_ix].MaterialName , matname.asChar() );
			}

			m_result.Materials[mat_ix].TextureIndex = mat_ix;
		}

	} else {
		
		m_result.Materials.Add(1);
		strcpy( m_result.Materials[addmesh.m_num_materials].MaterialName, "unknown" );
		m_result.Materials[addmesh.m_num_materials].TextureIndex = addmesh.m_num_materials;
	}
}


void	CStaticMeshGenerator::GeneratePoints(MeshAppend const& addmesh)
{
	int num_verts = addmesh.m_mesh.numVertices();

	bool flip_axis = MGlobal::isYAxisUp();

	m_result.Points.Add( num_verts );

	MPoint v;
	for (int i = 0; i < num_verts; ++i) {
		addmesh.m_mesh.getPoint(i,v);
		if (flip_axis) {
			m_result.Points[addmesh.m_num_points + i] = FVector(v.x,-v.z,v.y);
		} else {
			m_result.Points[addmesh.m_num_points + i] = FVector(v.x,v.y,v.z);
		}
	}
}


void	CStaticMeshGenerator::GenerateTPoints(MeshAppend const& addmesh)
{
	int num_uv = addmesh.m_mesh.numUVs();
	
	m_result.TPoints.Add( num_uv );

	for (int i = 0; i < num_uv; ++i) {
		float u,v;
		addmesh.m_mesh.getUV(i,u,v);
		m_result.TPoints[addmesh.m_num_tpoints + i].U = u;
		m_result.TPoints[addmesh.m_num_tpoints + i].V = 1.0f - v; // #Erik - Static mesh V flip necessary ...
	}
}

void	CStaticMeshGenerator::GenerateFaces(MeshAppend const& addmesh)
{
	MItMeshPolygon  polyiter(addmesh.m_mesh.object());

	MObjectArray shaders;
	MIntArray indices;

	bool have_material_nums = (addmesh.m_mesh.getConnectedShaders(0,shaders,indices) == MS::kSuccess);

	// count the number of triangles
	int count = 0;
	while (!polyiter.isDone()) {
		int len = polyiter.polygonVertexCount();
		count += (len > 2) ? len-2 : 0;
		polyiter.next();
	}

	polyiter.reset();
	m_result.Faces.Add(count);

	int index = addmesh.m_num_faces;
	while (!polyiter.isDone()) {
	
		int len = polyiter.polygonVertexCount();
		int uv[3], p[3];
		int mat_index = (have_material_nums) ? indices[polyiter.index()] : 0;

		polyiter.getUVIndex(0,uv[0]);
		p[0] = polyiter.vertexIndex(0);
		for (int j = 1; j+1 < len; ++j) {
			polyiter.getUVIndex(j,uv[1]);
			polyiter.getUVIndex(j+1,uv[2]);
			p[1] = polyiter.vertexIndex(j);
			p[2] = polyiter.vertexIndex(j+1);

			m_result.Faces[index].points[0] = p[0] + addmesh.m_num_points;
			m_result.Faces[index].points[1] = p[1] + addmesh.m_num_points;
			m_result.Faces[index].points[2] = p[2] + addmesh.m_num_points;
			m_result.Faces[index].t_points[0] = uv[0] + addmesh.m_num_tpoints;
			m_result.Faces[index].t_points[1] = uv[1] + addmesh.m_num_tpoints;
			m_result.Faces[index].t_points[2] = uv[2] + addmesh.m_num_tpoints;
			m_result.Faces[index].MatIndex = mat_index  + addmesh.m_num_materials;

			++index;
		}
	
		polyiter.next();

	}
}


// SL MODIFY
// scale everything before export
void	CStaticMeshGenerator::Scale(float scale)
{
	for (int i = 0; i < m_result.Points.Num(); ++i)
	{
		m_result.Points[i].X *= scale;
		m_result.Points[i].Y *= scale;
		m_result.Points[i].Z *= scale;
	}
}
// END SL MODIFY