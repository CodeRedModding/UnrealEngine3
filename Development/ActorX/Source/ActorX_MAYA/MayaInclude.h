/**************************************************************************
 
   Copyright (c) 2000 Epic MegaGames, Inc. All Rights Reserved.

   MaxInterface.h - MAX-specific exporter scene interface code.

   Created by Erik de Neve

   Lots of fragments snipped from all over the Max SDK sources.

***************************************************************************/
#ifndef __MayaInclude__H
#define __MayaInclude__H


#define REQUIRE_IOSTREAM (1) // Force to use IOSTREAM instead of IOSTREAM.H - new visual c++ 7+ requirement...

// Maya API  includes  - 
#include <maya/MFileIO.h>
#include <maya/MStatus.h>
#include <maya/MArgList.h>
#include <maya/MObject.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnDagNode.h>
#include <maya/MMatrix.h>
#include <maya/MGlobal.h>

#include <maya/MFnMesh.h>
#include <maya/MSelectionList.h>
#include <maya/MItSelectionList.h>
#include <maya/MIntArray.h>
#include <maya/MFnIntArrayData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPoint.h>
#include <maya/MFloatPointArray.h>
#include <maya/MDoubleARray.h>
#include <maya/MObjectArray.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MItGeometry.h>

#include <maya/MPxNode.h>
#include <maya/MPxCommand.h>
#include <maya/MPxData.h>

#include <maya/MFnSkinCluster.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItMeshEdge.h>


#include <maya/MAnimControl.h>





// Resources
#include ".\res\resource.h"


#endif //__MayaInclude__H