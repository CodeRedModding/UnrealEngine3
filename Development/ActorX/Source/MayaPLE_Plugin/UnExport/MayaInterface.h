/**************************************************************************
 
   Copyright (c) 2000 Epic MegaGames, Inc. All Rights Reserved.

   MaxInterface.h - MAX-specific exporter scene interface code.

   Created by Erik de Neve

   Lots of fragments snipped from all over the Max SDK sources.

***************************************************************************/

#ifndef __MayaInterface__H
#define __MayaInterface__H

#define WORLD 0
#define FLAT  1
#define FULL  2
#define NONE  0
#define TRI   1
#define QUAD  2

// Misc includes
#include <assert.h>

// Maya specific includes

#include <maya/MStatus.h>
#include <maya/MArgList.h>
#include <maya/MFnCamera.h>
#include <maya/MObject.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MDagPathArray.h>

#include <maya/MFnPhongShader.h>
#include <maya/MFnBlinnShader.h>
#include <maya/MFnLambertShader.h>
#include <maya/MFnReflectShader.h>
#include <maya/MColor.h>

#include <maya/MItSelectionList.h>
#include <maya/MSelectionList.h>
#include <maya/MIntArray.h>
#include <maya/MFloatArray.h>
#include <maya/MObjectArray.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MItGeometry.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItMeshEdge.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnSingleIndexedComponent.h>

#include <maya/MFnMesh.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnIkHandle.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFloatPoint.h>
#include <maya/MFloatPointArray.h>
#include <maya/MItMeshVertex.h>

#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnTransform.h>

#include <maya/MPoint.h>
#include <maya/MVector.h>
#include <maya/MMatrix.h>
#include <maya/MQuaternion.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MFnLight.h>
#include <maya/MColor.h>
#include <maya/MFnNurbsSurface.h>


#include <maya/MFnDependencyNode.h>
#include <maya/MSelectionList.h>
#include <maya/MObjectArray.h>
#include <maya/MItSelectionList.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MFnStringData.h>


#include <maya/MArgList.h>
#include <maya/MAnimControl.h>


// MDt Game exporter support..
/*
extern "C" 
{
	#include <sys/types.h>
	#include "MDt.h"
	#include "MDtExt.h"
}
*/

//#include <maya/MSimple.h>
//#include <maya/MString.h>
//#include <maya/MArgList.h>
//#include <maya/MObject.h>

//#include <maya/MFnPlugin.h>
#include <maya/MPxCommand.h>
#include <maya/MPxFileTranslator.h>


// Local includes
//#include "Phyexp.h"  // Max' PHYSIQUE interface
#include "Vertebrate.h"
//#include "Win32IO.h"

//#include  <COMMCTRL.H>

// Resources
//#include ".\res\resource.h"


// Minimum bone influence below which the physique weight link is ignored.
#define SMALLESTWEIGHT (0.0001f)
#define MAXSKININDEX 64



class scanDag
{
public:
	const class CUnExportCmd *	m_client;

	explicit scanDag(CUnExportCmd const& client) : m_client(&client) {};
	~scanDag() {};
	
	virtual MStatus	doScan( );
private:
	MStatus			parseArgs( const MArgList& args,
							   MItDag::TraversalType& traversalType,
							   MFn::Type& filter, bool & quiet);
	MStatus			doScan( const MItDag::TraversalType traversalType,
							MFn::Type filter, bool quiet);
	void			getTransformData(const MDagPath& dagPath, bool quiet);
};



#endif // _MayaInterface__H
