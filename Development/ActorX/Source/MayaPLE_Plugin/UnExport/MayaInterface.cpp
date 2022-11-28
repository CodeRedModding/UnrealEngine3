/**************************************************************************
 
   Copyright (c) 2000, 2001, 2002 Epic MegaGames, Inc. All Rights Reserved.

   MayaInterface.cpp - MAYA-specific exporter scene interface code.

   Created by Erik de Neve

   Lots of fragments snipped from all over the Maya SDK sources.

   Todo:  - Move any remaining non-Maya dependent code to SceneIFC
          - Assertions for exact poly/vertex/wedge bounds (16 bit indices !) 
 	         (max 21840)
  
   Change log:  

   Feb 2001:
   See #ROOT: Added explicit root bone scaler into bone sizes - as this is where in Maya
   the skeleton is scaled by default (independently of the skin).

   Feb 2002
     - 2/08/02   Misc cleanups for stability; added dummy UVSet string to the MeshFunction.getPolygonUV (which should be optional?) solved strange crash...
	 - 

 ////////////
	  
 WORK NOTES:

 ////////////

  -> accessing materials & material names: MFnMesh ;    and MDtApi's MDtMaterial.cpp
			  -> Index
			  -> Material name...
			  -> Actual bitmap name...

  Mostly from:
	>>MStatus MFnMesh:: getConnectedShaders ( unsigned instanceNumber, MObjectArray & shaders, MIntArray & indices ) const 
	and: see MDtApi's "shader_multitextures".

  
    * "Throw away all "end_*" joints...(like dummies culling)
	* >> in-string stuff for that..


 **********
    Main bugs
	- Skeleton with haphazard angles.
	- Root orientation quite different from the root stance of the mesh ( skeleton's off, rather than the mesh )
	- Material parsing incomplete.


  :::::::::::::::::::: Short Maya class SDK summary ::::::::::::::::::::::::::

     MDagPath:
		MDagPath ::node()  -> MObject node.
		MObject MDagPath:: transform ( MStatus *ReturnStatus) const  -> Retrieves the lowest Transform in the DAG Path.
		MMatrix MDagPath:: inclusiveMatrix ( MStatus *ReturnStatus) const  =>The matrix for all Transforms in the Path including the the end Node in the Path if it is a Transform 
		MMatrix MDagPath:: exclusiveMatrix ( MStatus * ReturnStatus) const => The matrix for all transforms in the Path excluding the end Node in the Path if it is Transform 
		M4 MDagPath:: inclusiveMatrixInverse ( MStatus * ReturnStatus) const 
		MMatrix MDagPath:: exclusiveMatrixInverse ( MStatus * ReturnStatus) const 
		MDagPath:: MDagPath ( const MDagPath & src )  => Copy constructor
		bool MDagPath:: hasFn ( MFn::Type type, MStatus *ReturnStatus ) const 
		MObject MDagPath:: node ( MStatus * ReturnStatus) const => Retrieves the DAG Node for this DAG Path.
		MString MDagPath:: fullPathName ( MStatus * ReturnStatus) const  => Full path name.
		
     MObject:
		const char * MObject:: apiTypeStr () const => Returns a string that gives the object's type is a readable form. This is useful for debugging when type of an object needs to be printed
		MFn::Type MObject:: apiType () const  =>  Determines the exact type of the Maya internal Object.

     MFnDagNode:
		MStatus  getPath ( MDagPath & path ) 

		MFnDagNode ( MObject & object, MStatus * ret = NULL )  => constructor
		MMatrix MFnDagNode:: transformationMatrix ( MStatus * ReturnStatus ) const 
		=>Returns the transformation matrix for this DAG node. In general, only 
		transform nodes have matrices associated with them. Nodes such as shapes 
		(geometry nodes) do not have transform matrices.
		The identity matrix will be returned if this node does not have a transformation matrix.
		If the entire world space transformation is required, then an MDagPath should be used.

		MStatus MFnDagNode:: getPath ( MDagPath & path )  
		=>Returns a DAG Path to the DAG Node attached 
		to the Function Set. The difference between this method 
		and the method dagPath below is that this one will not 
		fail if the function set is not attached to a dag path, 
		it will always return a path to the node. dagPath will 
		fail if the function set is not attached to a dag path.

		MDagPath MFnDagNode:: dagPath ( MStatus * ReturnStatus ) const 
        =>Returns the DagPath to which the Function Set is attached. 
		The difference between this method and the method getPath above 
		is that this one will fail if the function set is not attached 
		to a dag path. getPath will find a dag path if the function set is not attached to one.


	 MFnDependencyNode:
		MFnDependencyNode ( MObject & object, MStatus * ReturnStatus = NULL )  => constructor
		MString MFnDependencyNode:: classification ( const MString & nodeTypeName )
		=>  Retrieves the classification string for a node type. This is a string that is used 
		    in dependency nodes that are also shaders to provide more detailed type information 
			to the rendering system. See the documentation for the MEL commands getClassification 
			and listNodeTypes for information on the strings that can be provided.
			User-defined nodes set this value through a parameter to MFnPlugin::registerNode.

		MString  name ( MStatus * ReturnStatus = NULL ) const 
		MString  typeName ( MStatus * ReturnStatus = NULL ) const	
	    MTypeId  typeId ( MStatus * ReturnStatus = NULL ) const
    
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
    
  
***************************************************************************/

#include "precompiled.h"
#include "MayaInterface.h"
#include "SceneIfc.h"
#include "unExportCmd.h"

#include <maya/MItMeshPolygon.h>
//#include "ActorX.h"

#include "maattrval.h"

//
// Fast global scene object storage array(s).
//

//static	TArray<MObject> AxSceneObjects;  // All relevant scene nodes
//static	TArray<MObject> AxSkinClusters;  // Skin clusters
//static    TArray<MObject> AxShaders;       // All encountered shader objects

MObjectArray AxSceneObjects;
MObjectArray AxSkinClusters;
MObjectArray AxShaders;

TArray<INT>     AxShaderFlags;   // Shader usage flags

// BRUSH export support
extern char materialnames[8][MAX_PATH];
extern char to_brushfile[MAX_PATH];
extern char to_brushpath[MAX_PATH];

//extern WinRegistry PluginReg;

void ClearScenePointers()
{
	AxSceneObjects.clear();
	AxSkinClusters.clear();
	AxShaders.clear();
}


INT WhereInMobjectArray( MObjectArray& MArray, MObject& LocateObject )
{
	INT KnownIndex = -1;
	for( DWORD t=0; t < MArray.length(); t++)
	{
		if( MArray[t] == LocateObject ) // comparison legal ?
			KnownIndex = t;
	}
	return KnownIndex;
}

// Print a string to the Maya log window.
void MayaLog(char* LogString, ... )
{
	//if (!DEBUGMSGS) return;
	char TempStr[4096];
	appGetVarArgs(TempStr,4096,LogString);
	MStatus	stat;
	stat.perror(TempStr);		
}


void SceneIFC::StoreNodeTree(AXNode* node) 
{
	MStatus	stat;

	INT NodeNum = 0;
	INT ObjectCount = 0;

	// Global scene object array

	// #NOTE: Clusters Empty() doesn't work right in reproducable cases -> though runs fine in 'release' build.
	// Temp fix by Talin: 
	// AxSceneObjects.Data = NULL;
	// AxSkinClusters.Data = NULL;

	//AxSceneObjects.Empty(); 
	//AxSkinClusters.Empty(); 

	AxSceneObjects.clear();
	AxSkinClusters.clear();
	AxShaders.clear();

	for( MItDependencyNodes DepIt(MFn::kDependencyNode);!DepIt.isDone();DepIt.next())
	{
		MFnDependencyNode	Node(DepIt.item());

		// Get object for this Node.
		MObject	Object = Node.object();
		
		
		// Maya SkinCluster modifier is not a DAG node - store independently.

		if ( Object.apiType() == MFn::kSkinClusterFilter ) 
		{
			AxSkinClusters.append( Object );//AxSkinClusters.AddItem( Object );
		}

		if(! DepIt.item().hasFn(MFn::kDagNode))
				continue;

		MFnDagNode DagNode = DepIt.item();
		
		if(DagNode.isIntermediateObject())
				continue;
				
		//
		// Simply record all relevant objects.
		// Includes skinclusterfilters(?) (and materials???)
		//
				
		if( Object != MObject::kNullObj  )
		{
			if( //( Object.hasFn(MFn::kMesh)) ||
				( Object.apiType() == MFn::kMesh ) ||
				( Object.apiType() == MFn::kJoint ) ||
				( Object.apiType() == MFn::kTransform ) )
			{
				AxSceneObjects.append( Object ); //AxSceneObjects.AddItem(Object); 
								
				//if ( ! Object.hasFn(MFn::kDagNode) )
				//	//PopupBox("Object %i [%s] has no DAG node",NodeNum, Object.apiTypeStr() );

				if ( Object.apiType() == MFn::kSkinClusterFilter ) 
				{
					////PopupBox("Skincluster found, scenetree item %i name %s",AxSceneObjects.Num()-1, Node.name(&stat).asChar() );
				}

				NodeInfo NewNode;
				NewNode.node = (void*) ObjectCount;
				NewNode.NumChildren = 0;

				SerialTree.AddItem( NewNode ); // Store index rather than Object.

				ObjectCount++;
			}
			else // Not a mesh or skincluster....
			{
				//DLog.Logf("NonMesh item:%i  Type: [%s]  NodeName: [%s] \r\n",NodeNum, Object.apiTypeStr(), Node.name(&stat).asChar());
			}
			
		}
		else // Null objects...
		{		
			// DLog.Logf("Null object - Item number: %i   NodeName: [%s] \r\n",NodeNum, Node.name(&stat).asChar() );
		}

		NodeNum++;
	}
}


FLOAT GetMayaTimeStart()
{
	MTime start( MAnimControl::minTime().value(), MTime::uiUnit() );
	return start.value();
}

FLOAT GetMayaTimeEnd()
{
	MTime end( MAnimControl::maxTime().value(), MTime::uiUnit() );
	return end.value();
}

void SetMayaSceneTime( FLOAT CurrentTime)
{
	MTime NowTime;
	NowTime.setValue(CurrentTime);
	MGlobal::viewFrame( NowTime );
}

int DigestMayaMaterial( MObject& NewShader  )
{
	INT KnownIndex = -1;
	for( DWORD t=0; t < AxShaders.length(); t++)
	{
		if( AxShaders[t] == NewShader ) // comparison legal ?
			KnownIndex = t;
	}
	if( KnownIndex == -1)
	{
		KnownIndex = AxShaders.length();
		AxShaders.append( NewShader );		
	}
	//AxShaders.AddUniqueItem( NewShader );
	return( KnownIndex );
}


// Maya-specific Y-Z matrix reversal.
void MatrixSwapXYZ( FMatrix& ThisMatrix)
{
	for(INT i=0;i<4;i++)
	{
		FLOAT TempX = ThisMatrix.M[i][0];
		FLOAT TempY = ThisMatrix.M[i][1];
		FLOAT TempZ = ThisMatrix.M[i][2];
		
		ThisMatrix.M[i][0] = TempZ;
		ThisMatrix.M[i][1] = -TempX;
		ThisMatrix.M[i][2] = TempY;		
	}
	
	/*
	FPlane Temp = ThisMatrix.YPlane;
	ThisMatrix.YPlane = ThisMatrix.ZPlane;
	ThisMatrix.ZPlane = Temp;
	*/
}

void MatrixSwapYZTrafo( FMatrix& ThisMatrix)
{
		
	for(INT i=3;i<4;i++)
	{
		FLOAT TempZ = ThisMatrix.M[i][1];
		ThisMatrix.M[i][1] =    ThisMatrix.M[i][0];
		ThisMatrix.M[i][0] =    TempZ;
	}
	
	/*
	FPlane Temp = ThisMatrix.YPlane;
	ThisMatrix.YPlane = ThisMatrix.ZPlane;
	ThisMatrix.ZPlane = Temp;
	*/
}




//
//	Description:
//		return the type of the object
//
const char* objectType( MObject object )
{
	if( object.isNull() )
	{
		return NULL;
	}
	MStatus stat = MS::kSuccess;
	MFnDependencyNode dgNode;
	stat = dgNode.setObject( object );
	MString typeName = dgNode.typeName( &stat );
	if( MS::kSuccess != stat)
	{
		//cerr << "Error: can not get the type name of this object.\n";
		return NULL;
	}
	return typeName.asChar();
}


int GetMayaShaderTextureSize( MObject alShader, INT& xres, INT& yres )
{
    //MtlStruct	  *mtl;
	MString   shaderName;
	MString   textureName;
	MString   textureFile = "";
	MString	  projectionName = "";
	MString	  convertName;
	MString   command;
	MStringArray result;
	MStringArray arrayResult;

	MStatus   status;
	MStatus   statusT;

	xres=256;
	yres=256; //#debug
	
	bool		projectionFound = false;
	bool		layeredFound = false;
    bool		fileTexture = false;

    MFnDependencyNode dgNode;
    status = dgNode.setObject( alShader );
	shaderName = dgNode.name( &status );

	textureName = shaderName; //#debug default texturename...

    result.clear();

	////PopupBox("Material name: <%s> type: [%s] shadername: ",textureName,objectType(alShader),shaderName);	

	MObject             currentNode;
	MObject 			thisNode;
	MObjectArray        nodePath;
	MFnDependencyNode   nodeFn,dgNodeFnSet;
	MObject             node;

	// Get the dependent texture for this shader by going over a dependencygraph ???
	MItDependencyGraph* dgIt; 
	dgIt = new MItDependencyGraph( alShader,
                               MFn::kFileTexture,
                               MItDependencyGraph::kUpstream, 
                               MItDependencyGraph::kBreadthFirst,
                               MItDependencyGraph::kNodeLevel, 
                               &status );
	//
	for ( ; ! dgIt->isDone(); dgIt->next() ) 
	{      
		thisNode = dgIt->thisNode();
        dgNodeFnSet.setObject( thisNode ); 
        status = dgIt->getNodePath( nodePath );
                                                                 
        if ( !status ) 
		{
			status.perror("getNodePath");
			continue;
        }

        //
        // append the starting node.
        //
        nodePath.append(node);

        //dumpInfo( thisNode, dgNodeFnSet, nodePath );
		////PopupBox("texture node found: [%s] type [%s] ",dgNodeFnSet.name(), objectType(thisNode) );

		textureName = dgNodeFnSet.name();
		fileTexture = true;
	}
    delete dgIt;

    // If we are doing file texture lets see how large the file is
    // else lets use our default sizes.  If we can't open the file
	// we will use the default size.

	if (fileTexture) // fileTexture)
	{
			//getTextureFileSize( textureName, xres, yres );

			// Lets try getting the size of the textures this way.
			MStatus		status;
			MString		command;
			MIntArray	sizes;

			command = MString( "getAttr " ) + textureName + MString( ".outSize;" );
			status = MGlobal::executeCommand( command, sizes );
			if ( MS::kSuccess == status )
			{
				xres = sizes[0];
				yres = sizes[1];
				return 1;
			}
			else
			{
				//PopupBox("Unable to retrieve textures with .outSize command from [%s]",textureName);
			}
	}

	return 0;
}


//
// Retrieve joint trafo
//
void RetrieveTrafoForJoint( MObject& ThisJoint, FMatrix& ThisMatrix, FQuat& ThisQuat, INT RootCheck, FVector& BoneScale )
{
	// try to directly get local transformations only.

	static float	mtxI[4][4];
	static float	mtxIrot[4][4];
	static double   newQuat[4];

	MStatus stat;	
	// Retrieve dagnode and dagpath from object..
	MFnDagNode fnTransNode( ThisJoint, &stat );
	MFnDagNode fnTransNodeParent( ThisJoint, &stat );

	MDagPath dagPath;
	MDagPath dagPathParent;

	stat = fnTransNode.getPath( dagPath );
	stat = fnTransNodeParent.getPath( dagPathParent );

	MFnDagNode fnDagPath( dagPath, &stat );
	MFnDagNode fnDagPathParent( dagPathParent, &stat );

	MMatrix RotationMatrix;
	MMatrix localMatrix;
	MMatrix globalMatrix;
	MMatrix testMatrix;
	MMatrix globalParentMatrix;

	FMatrix MayaRotationMatrix;

	// RotationMatrix = dagPath.inclusiveMatrix();
	// testMatrix = dagPath.exclusiveMatrix();
	// Get global matrix if RootCheck indicates skeletal root

	if ( RootCheck )
	{		
		localMatrix = fnTransNode.transformationMatrix ( &stat );
		globalMatrix = dagPath.inclusiveMatrix();
		//globalMatrix.homogenize();

		globalMatrix.get( mtxI ); 
	}
	else
	{
		localMatrix = fnTransNode.transformationMatrix ( &stat );
		globalMatrix = dagPath.inclusiveMatrix();
		//localMatrix.homogenize();

		localMatrix.get( mtxI );
	}

	// UT bones are supposed to not have scaling; but we need the maya-specific per-bone scale
	// because this is sometimes used to scale up the entire skeleton & skin.
	
	// To get the proper scaler for the translation ( not fully animated scaling )
	double DoubleScale[3];
	if ( !RootCheck ) // -> (!Rootcheck) works - but still messes up root ANGLE
	{
		globalParentMatrix = dagPathParent.inclusiveMatrix();
		
		MTransformationMatrix   matrix( globalParentMatrix );
		matrix.getScale( DoubleScale, MSpace::kWorld);	
	}
	else
	{
		DoubleScale[0] = DoubleScale[1] = DoubleScale[2] = 1.0f;
	}
	BoneScale.X = DoubleScale[0];
	BoneScale.Y = DoubleScale[1];
	BoneScale.Z = DoubleScale[2];

	//
	//  localMatrix.homogenize(); //=> destroys rotation !
	//  #TODO - needs coordinate flipping ?  ( XYZ order in Maya different from Unreal )
	//
	//MQuaternion MayaQuat;
	//MayaQuat = localMatrix; // implied conversion to a normalized quaternion
	//MayaQuat.normalizeIt();
	//MayaQuat.get(newQuat);
	
	// Copy result
	
	ThisMatrix.XPlane.X = mtxI[0][0];
	ThisMatrix.XPlane.Y = mtxI[0][1];
	ThisMatrix.XPlane.Z = mtxI[0][2];
	ThisMatrix.XPlane.W = 0.0f;

	ThisMatrix.YPlane.X = mtxI[1][0];
	ThisMatrix.YPlane.Y = mtxI[1][1];
	ThisMatrix.YPlane.Z = mtxI[1][2];
	ThisMatrix.YPlane.W = 0.0f;

	ThisMatrix.ZPlane.X = mtxI[2][0];
	ThisMatrix.ZPlane.Y = mtxI[2][1];
	ThisMatrix.ZPlane.Z = mtxI[2][2];
	ThisMatrix.ZPlane.W = 0.0f;
	// Trafo..
	ThisMatrix.WPlane.X = mtxI[3][0];
	ThisMatrix.WPlane.Y = mtxI[3][1];
	ThisMatrix.WPlane.Z = mtxI[3][2];
	ThisMatrix.WPlane.W = 1.0f;
	
	// if( RootCheck )MatrixSwapXYZ(ThisMatrix); //#debug RootCheck

	// Extract quaternion rotations
	ThisQuat = FMatrixToFQuat(ThisMatrix);
	
	if (! RootCheck ) // Necessary
	{
		ThisQuat.W = - ThisQuat.W;	
	}
	ThisQuat.Normalize(); // 
	
	
	// #inversion..
	if( 0 ) // RootCheck ) //  Hardcoded twistaround ?
	{
		// Construct a rotation matrix to orient the root correctly. Use the same one
		// to rotate the root offset as if:
			// NewPoint.Point.X = -Vertex.z;
			// NewPoint.Point.Y = -Vertex.x;
			// NewPoint.Point.Z =  Vertex.y;

		FMatrix RotMatrix;
		RotMatrix.XPlane.X =0.0f;
		RotMatrix.XPlane.Y =1.0f;
		RotMatrix.XPlane.Z =0.0f; //-1.0
		RotMatrix.XPlane.W =0.0f;

		RotMatrix.YPlane.X =1.0f; //-1.0
		RotMatrix.YPlane.Y =0.0f;
		RotMatrix.YPlane.Z =0.0f; 
		RotMatrix.YPlane.W =0.0f;

		RotMatrix.ZPlane.X =0.0f;
		RotMatrix.ZPlane.Y =0.0f; //1.0
		RotMatrix.ZPlane.Z =1.0f;
		RotMatrix.ZPlane.W =0.0f;
		// Trafo...
		RotMatrix.WPlane.X =0.0f;
		RotMatrix.WPlane.Y =0.0f;
		RotMatrix.WPlane.Z =0.0f;
		RotMatrix.WPlane.W =0.0f;
				
		FCoords RotCoords = FCoordsFromFMatrix( RotMatrix );

		DLog.LogfLn("RotCoords X: %f %f %f %f", RotCoords.XAxis.X, RotCoords.XAxis.Y,RotCoords.XAxis.Z );
		DLog.LogfLn("RotCoords Y: %f %f %f %f", RotCoords.YAxis.X, RotCoords.YAxis.Y,RotCoords.YAxis.Z );
		DLog.LogfLn("RotCoords Z: %f %f %f %f", RotCoords.ZAxis.X, RotCoords.ZAxis.Y,RotCoords.ZAxis.Z );
		DLog.LogfLn("RotCoords O: %f %f %f %f", RotCoords.Origin.X,RotCoords.Origin.Y,RotCoords.Origin.Z );

		DLog.LogfLn("ThisMatrixWP: %f %f %f %f", ThisMatrix.WPlane.X,ThisMatrix.WPlane.Y,ThisMatrix.WPlane.Z,ThisMatrix.WPlane.W );

		FVector RootOffset( ThisMatrix.WPlane.X, ThisMatrix.WPlane.Y, ThisMatrix.WPlane.Z );
		RootOffset = RootOffset.TransformVectorBy( RotCoords );

        FCoords QuatCoords = FCoordsFromFMatrix( FQuatToFMatrix( ThisQuat ) );
		QuatCoords = RotCoords.ApplyPivot( QuatCoords ); //QuatCoords.ApplyPivot( RotCoords );
		ThisQuat = FMatrixToFQuat( FMatrixFromFCoords( QuatCoords ) );

		// Transform quat
		/*
		DLog.LogfLn("ThisQuat: %f %f %f %f", ThisQuat.X, ThisQuat.Y, ThisQuat.Z, ThisQuat.W );
		FVector QuatVector = FVector(ThisQuat.X,ThisQuat.Y,ThisQuat.Z);
		QuatVector = QuatVector.TransformVectorBy(RotCoords);
		ThisQuat.X = QuatVector.X;
		ThisQuat.Y = QuatVector.Y;
		ThisQuat.Z = QuatVector.Z;
		*/
		ThisQuat.Normalize();
	
		// ThisQuat.X = ThisQuat * TwistQuat; // Order??
		DLog.LogfLn("ThisQuat: %f %f %f %f", ThisQuat.X, ThisQuat.Y, ThisQuat.Z, ThisQuat.W );

		ThisMatrix.WPlane.X = RootOffset.X;
		ThisMatrix.WPlane.Y = RootOffset.Y;
		ThisMatrix.WPlane.Z = RootOffset.Z;
		ThisMatrix.WPlane.W = 0.0f;
	}
}


UBOOL isObjectSelected( const MDagPath & path )
//   
//  Description:
//      Check if the given object is selected
//  
{
	MDagPath sDagPath;

    MSelectionList activeList;
    MGlobal::getActiveSelectionList (activeList);

	MItSelectionList iter ( activeList, MFn::kDagNode );

	while ( !iter.isDone() )
	{
		if ( iter.getDagPath( sDagPath ) )
		{
			if ( sDagPath == path )
				return true;
		}
		iter.next();
	}

	return false;
}



#if (0) //SMOOTHING

//
// Detect smoothing groups... - edge processing code copied from the Maya SDK's ObjExport.cpp 
// 

//////////////////////////////////////////////////////////////

#define NO_SMOOTHING_GROUP      -1
#define INITIALIZE_SMOOTHING    -2
#define INVALID_ID              -1


//
// Edge info structure
//
typedef struct EdgeInfo 
{
    int                 polyIds[2]; // Id's of polygons that reference edge
    int                 vertId;     // The second vertex of this edge
    int                 nextIdx;    // Pointer to next edge
    bool                smooth;     // Is this edge smooth
};

// * EdgeInfoPtr;

//
// Edge lookup table (by vertex id) and smoothing group info
//
    TArray<EdgeInfo> Edges;
	TArray<INT> VertexFirstEdge; // pointers for each vertex, leadning
	// into a 'nextIdx'-linked list of edges in Edges.
	// initialized with -1.
	//
	TArray<PolySmoothingGroups> polySmoothingGroups;

    //EdgeInfoPtr *   Edges;
    //int *           polySmoothingGroups;
    int             EdgesSize;
    int             nextSmoothingGroup;
    int             currSmoothingGroup;
    bool            newSmoothingGroup;
//////////////////////////////////////////////////////////////


	
//
// Adds a new edge info element to the vertex table.
//
void addEdgeInfo( int v1, int v2, bool smooth )
{
    EdgeInfo element;

    // Setup data for new edge
    //
    element->vertId     = v2;
    element->smooth     = smooth;
    element->next       = -1;
   
    // Initialize array of id's of polygons that reference this edge.
    // There are at most 2 polygons per edge.
    //
    element->polyIds[0] = INVALID_ID;
    element->polyIds[1] = INVALID_ID;

	Edges.AddItem(element);

	if( Edges[VertexFirstEdge[v1] ] == -1)
	{
		// Point to new one.
		VertexFirstEdge[v1] = Edges.Num()-1;
	}
	else
	{
		INT Vert = VertexFirstEdge[v1];
		while( Edges[Vert].nextIdx != -1) 
		{	
			Vert = Edges[Vert].nextIdx;
		}
		// Point to new one.
		Edges[Vert].nextIdx = Edges.Num()-1;
	}
}



EdgeInfo* findEdgeInfo( int v1, int v2 )
//
// Finds the info for the specified edge.
//
{
    EdgeInfo* element;

	if( VertexFirstEdge[v1] == -1 )
		element = NULL
	else
		element = Edges[VertexFirstEdge[v1]];
	// not in table yet ?


	// Look for v2 at the other end of edges at v1
    while ( NULL != element ) 
	{
        if ( v2 == element->vertId ) 
		{
            return element;
        }
        element = element->next;
    }
    
	// Look for v1 at the other end of edges at v2
    if ( element == NULL ) 
	{
        element = Edges[VertexFirstEdge[v2]];
        while ( NULL != element ) 
		{
            if ( v1 == element->vertId ) 
			{
                return element;
            }
            element = element->next;
        }
    }
    return NULL;
}

bool smoothingAlgorithm( int polyId, MFnMesh& fnMesh )
{
    MIntArray vertexList;
    fnMesh.getPolygonVertices( polyId, vertexList );
    int vcount = vertexList.length();
    bool smoothEdgeFound = false;
    
    for ( int vid=0; vid<vcount;vid++ ) {
        int a = vertexList[vid];
        int b = vertexList[ vid==(vcount-1) ? 0 : vid+1 ];
        
        EdgeInfoPtr elem = findEdgeInfo( a, b );

        if ( NULL != elem ) {
            // NOTE: We assume there are at most 2 polygons per edge
            //       halfEdge polygons get a smoothing group of
            //       NO_SMOOTHING_GROUP which is equivalent to "s off"
            //
            if ( NO_SMOOTHING_GROUP != elem->polyIds[1] ) { // Edge not a border
                
                // We are starting a new smoothing group
                //                
                if ( newSmoothingGroup ) {
                    currSmoothingGroup = nextSmoothingGroup++;
                    newSmoothingGroup = false;
                    
                    // This is a SEED (starting) polygon and so we always
                    // give it the new smoothing group id.
                    // Even if all edges are hard this must be done so
                    // that we know we have visited the polygon.
                    //
                    polySmoothingGroups[polyId] = currSmoothingGroup;
                }
                
                // If we have a smooth edge then this poly must be a member
                // of the current smoothing group.
                //
                if ( elem->smooth ) {
                    polySmoothingGroups[polyId] = currSmoothingGroup;
                    smoothEdgeFound = true;
                }
                else { // Hard edge so ignore this polygon
                    continue;
                }
                
                // Find the adjacent poly id
                //
                int adjPoly = elem->polyIds[0];
                if ( adjPoly == polyId ) {
                    adjPoly = elem->polyIds[1];
                }                             
                
                // If we are this far then adjacent poly belongs in this
                // smoothing group.
                // If the adjacent polygon's smoothing group is not
                // NO_SMOOTHING_GROUP then it has already been visited
                // so we ignore it.
                //
                if ( NO_SMOOTHING_GROUP == polySmoothingGroups[adjPoly] ) 
				{
                    smoothingAlgorithm( adjPoly, fnMesh );
                }
                else if ( polySmoothingGroups[adjPoly] != currSmoothingGroup ) 
				{
                    //DebugBox("Warning: smoothing group problem at polyon %i",adjPoly);
                }
            }
        }
    }
    return smoothEdgeFound;
}


//
//
//
void buildEdges( MDagPath& mesh )
{
    // Create our edge lookup table and initialize all entries to NULL

    MFnMesh fnMesh( mesh );
    EdgesSize = fnMesh.numVertices();

	Edges.Empty();
	VertexFirstEdge.Empty();

	VertexFirstEdge.AddZeroed(EdgesSize);
	for(INT t=0; t<EdgesSize; t++)
		VertexFirstEdge(t) = -1;
	
	//
    // Add entries, for each edge, to the lookup table
    //
    MItMeshEdge eIt( mesh );
    for ( ; !eIt.isDone(); eIt.next() )
    {
        bool smooth = eIt.isSmooth();
        addEdgeInfo( eIt.index(0), eIt.index(1), smooth );
    }

    // Fill in referenced polygons
    //
    MItMeshPolygon pIt( mesh );
    for ( ; !pIt.isDone(); pIt.next() )
    {
        int pvc = pIt.polygonVertexCount();
        for ( int v=0; v<pvc; v++ )
        {
            int a = pIt.vertexIndex( v );
            int b = pIt.vertexIndex( v==(pvc-1) ? 0 : v+1 );

            EdgeInfoPtr elem = findEdgeInfo( a, b );
            if ( NULL != elem ) {
                int edgeId = pIt.index();
                
                if ( INVALID_ID == elem->polyIds[0] ) {
                    elem->polyIds[0] = edgeId;
                }
                else {
                    elem->polyIds[1] = edgeId;
                }                                    
            }
        }
    }

	//
    // Now create a polyId->smoothingGroup table
    //   
    int numPolygons = fnMesh.numPolygons();
    polySmoothingGroups.Empty();
	polySmoothingGroups.AddZeroed(numPolygons);

    for ( int i=0; i< numPolygons; i++ ) {
        polySmoothingGroups[i] = NO_SMOOTHING_GROUP;
    }    
    
    // Now call the smoothingAlgorithm to fill in the polySmoothingGroups
    // table.
    // Note: we have to traverse ALL polygons to handle the case
    // of disjoint polygons.
    //
    nextSmoothingGroup = 1;
    currSmoothingGroup = 1;
    for ( int pid=0; pid<numPolygons; pid++ ) {
        newSmoothingGroup = true;
        // Check polygon has not already been visited
        if ( NO_SMOOTHING_GROUP == polySmoothingGroups[pid] ) {
           if ( !smoothingAlgorithm(pid,fnMesh) ) {
               // No smooth edges for this polygon so we set
               // the smoothing group to NO_SMOOTHING_GROUP (off)
               polySmoothingGroups[pid] = NO_SMOOTHING_GROUP;
           }
        }
    }
}


//
// Build smoothing group info for a certain mesh;
// One word for each face. Works with a single Maya mesh object; if we're processing
// multiple meshes, this needs to be called once for each mesh, then copied
// into the final skin info.
//
void BuildSmoothingGroups( MDagPath& mesh )
{
	BuildEdges( mesh );
	//
	// Now smoohthing groups are in 
	//
	//
}

#endif


//
// Build the hierarchy in SerialTree
//
int SceneIFC::SerializeSceneTree() 
{
	OurSkin=NULL;
	OurRootBone=NULL;
	
	/*
	// Get/set MAX animation Timing info.
	Interval AnimInfo = TheMaxInterface->GetAnimRange();
	TimeStatic = AnimInfo.Start();
	TimeStatic = 0;

	Frame 0 for reference: 
	if( DoPoseZero ) TimeStatic = 0.0f;

	FrameStart = AnimInfo.Start(); // in ticks..
	FrameEnd   = AnimInfo.End();   // in ticks..
	FrameTicks = GetTicksPerFrame();
	FrameRate  = GetFrameRate();
	*/
	
	FrameStart = GetMayaTimeStart(); // in ticks..
	TimeStatic = max( FrameStart, 0.0f );
	FrameEnd   = GetMayaTimeEnd();   // in ticks..
	FrameTicks = 1.0f;
	FrameRate  = 30.0f;// TODO: Maya framerate retrieval. 

	if( DoForceRate ) FrameRate = PersistentRate; //Forced rate.
	
	SerialTree.Empty();

	int ChildCount = 0;

	////PopupBox("Start store node tree");

	// Store all (relevant) nodes into SerialTree (actually, indices into AXSceneObjects. )
    StoreNodeTree( NULL );
	
	// Update basic nodeinfo contents : parent/child relationships.
	// Beware (later) that in Maya there is alwas a transform object between geometry and joints.
	for(int  i=0; i<SerialTree.Num(); i++)
	{	
		// SerialTree[i].IsSelected = ( AxSceneObjects[(INT)SerialTree[i].node]

		int ParentIndex = -1;
		MFnDagNode DagNode = AxSceneObjects[(int)SerialTree[i].node]; // object to DagNode... 

		MObject ParentNode = DagNode.parent(0);
		// Match to AxSceneObjects
		//int InSceneIdx = AxSceneObjects.Where(ParentNode);
		int InSceneIdx = WhereInMobjectArray( AxSceneObjects, ParentNode );

		ParentIndex = MatchNodeToIndex( (void*) InSceneIdx ); // find parent
		SerialTree[i].ParentIndex = ParentIndex;

		int NumChildren = 0;
		NumChildren = DagNode.childCount();
		SerialTree[i].NumChildren = NumChildren;
		//#todo Note: this may be wrong when we ignoderd children in the selection.

		if( NumChildren > 0 ) ChildCount+=NumChildren;

		// Update selected status
		// SerialTree[i].IsSelected = ((INode*)SerialTree[i].node)->Selected();
		// MAX:  Establish our hierarchy by way of Serialtree indices... upward only.
		// SerialTree[i].ParentIndex = MatchNodeToIndex(((INode*)SerialTree[i].node)->GetParentNode());
		// SerialTree[i].NumChildren = ( ((INode*)SerialTree[i].node)->NumberOfChildren() );
		
		// Print tree node name... note: is a pointer to an inner name, don't change content !
		// char* NodeNamePtr;
		// DtShapeGetName( (int)SerialTree[i].node,  &NodeNamePtr );
		// MayaLog("Node#: %i  Name: [%s] NodeID: %i ",i,NodeNamePtr, (int)SerialTree[i].node );

		MDagPath ObjectPath;
		DagNode.getPath( ObjectPath );
		// stat = skinCluster.getPathAtIndex(index,skinPath);
		// Check whether selected:
		if( isObjectSelected ( ObjectPath ))
			SerialTree[i].IsSelected = 1;
	}

	// //PopupBox("Serialtree num: %i children: %i",SerialTree.Num(), ChildCount );
	return SerialTree.Num();
}


//
//   See if there's a skincluster in this scene that influences MObject...
//
INT FindSkinCluster( MObject &TestObject, SceneIFC* ThisScene )
{
	INT SoftSkin = -1;

	for( DWORD t=0; t<AxSkinClusters.length(); t++ )
	{
		// 'physique / skincluster modifier' detection
		//MFnDagNode DagNode = AxSkinClusters[t];

		MObject	SkinObject = AxSkinClusters[t];

		MFnDependencyNode SkinNode = (MFnDependencyNode) SkinObject;

		////PopupBox("Skin search: node name: [%s] ", SkinNode.typeName().asChar() );
		
		if( SkinObject.apiType() == MFn::kSkinClusterFilter )
		{
			// For each skinCluster node, get the list of influenced objects			
			MFnSkinCluster skinCluster(SkinObject);
			MDagPathArray infs;
			MStatus stat;
		    int nInfs = skinCluster.influenceObjects(infs, &stat);

			////PopupBox("Skincluster found, looking for affected geometry");

			if( nInfs )
			{
				int nGeoms = skinCluster.numOutputConnections();
				for (int ii = 0; ii < nGeoms; ++ii) 
				{
					int index = skinCluster.indexForOutputConnection(ii,&stat);
					//CheckError(stat,"Error getting geometry index.");					

					// Get the dag path of the ii'th geometry.
					MDagPath skinPath;
					stat = skinCluster.getPathAtIndex(index,skinPath);						
					//CheckError(stat,"Error getting geometry path.");
					
					MObject SkinGeom = skinPath.node();

					//?? DagNode in skinPath ????					
					if ( SkinGeom == TestObject ) //TestObjectode == DagNode ) // compare node & node from path
					{
						////PopupBox("Skincluster influence found for -  %s \n", skinPath.fullPathName().asChar() );
						return t;
					}
					// DLog.Logf("Geometry pathname: # [%3i] -  %s \n",ii,skinPath.fullPathName().asChar() );
				}
			}
		}	
	}
	return -1;
}
			/*
			if (1)	{
						// For each skinCluster node, get the list of influencdoe objects
						//
						MFnSkinCluster skinCluster(Object);
						MDagPathArray infs;
						MStatus stat;
						unsigned int nInfs = skinCluster.influenceObjects(infs, &stat);
						//CheckError(stat,"Error getting influence objects.");

						if( 0 == nInfs ) 
						{
							stat = MS::kFailure;
							//CheckError(stat,"Error: No influence objects found.");
						}
						
						// loop through the geometries affected by this cluster
						//
						unsigned int nGeoms = skinCluster.numOutputConnections();
						for (size_t ii = 0; ii < nGeoms; ++ii) 
						{
							unsigned int index = skinCluster.indexForOutputConnection(ii,&stat);
							//CheckError(stat,"Error getting geometry index.");

							// Get the dag path of the ii'th geometry.
							//
							MDagPath skinPath;
							stat = skinCluster.getPathAtIndex(index,skinPath);
							//CheckError(stat,"Error getting geometry path.");

							DLog.Logf("Geometry pathname: # [%3i] -  %s \n",ii,skinPath.fullPathName().asChar() );
						}
					}
			*/



int SceneIFC::GetSceneInfo(VActor * pTempActor)
{
	// DLog.Logf(" Total nodecount %i \n",SerialTree.Num());

	MStatus stat;

	PhysiqueNodes = 0;
	TotalBones    = 0;
	TotalDummies  = 0;
	TotalBipBones = 0;
	TotalMaxBones = 0;

	GeomMeshes = 0;

	LinkedBones = 0;
	TotalSkinLinks = 0;

	if (SerialTree.Num() == 0) return 0;

	SetMayaSceneTime( TimeStatic );

	//DebugBox("Go over serial nodes & set flags");

	OurSkins.Empty();	

	for( int i=0; i<SerialTree.Num(); i++)
	{					
		MFnDagNode DagNode = AxSceneObjects[ (int)SerialTree[i].node ]; 
		MObject	Object = DagNode.object();

		// A mesh or geometry ? #TODO check against untextured geometry...
		SerialTree[i].HasMesh = ( Object.hasFn(MFn::kMesh) ? 1:0 );
		SerialTree[i].HasTexture = SerialTree[i].HasMesh;

		// Check for a bone candidate.
		SerialTree[i].IsBone = ( Object.apiType() == MFn::kJoint ) ? 1:0;

		// Check Mesh against all SerialTree items for a skincluster ('physique') modifier:		

		INT IsSkin = ( Object.apiType() == MFn::kMesh) ? 1: 0; //( Object.apiType() == MFn::kMesh ) ? 1 : 0;

		SerialTree[i].ModifierIdx = -1; // No cluster.			
		INT HasCluster = 0;
		if( IsSkin) 
		{
			INT Cluster = FindSkinCluster( Object, this );
			HasCluster = ( Cluster >= 0 ) ? 1 : 0;
			if ( Cluster >= 0 ) 
			{
				SerialTree[i].ModifierIdx = Cluster; //Skincluster index..
			}
		}
				
		UBOOL SkipThisMesh = false;
		
		GeomMeshes += SerialTree[i].HasMesh;

		//UBOOL SkipThisMesh = ( this->DoSkipSelectedGeom && ((INode*)SerialTree[i].node)->Selected() );




		if ( IsSkin && !SkipThisMesh  )  // && DoPhysGeom  (HasCluster > -1)
		{
			// SkinCluster/Physique skins dont act as bones.
			SerialTree[i].IsBone = 0; 
			SerialTree[i].IsSkin = 1;
			SerialTree[i].IsSmooth = HasCluster;

			// Multiple physique nodes...
			SkinInf NewSkin;			

			NewSkin.Node = SerialTree[i].node;
			NewSkin.IsSmoothSkinned = HasCluster;
			NewSkin.IsTextured = 1;
			NewSkin.SceneIndex = i;
			NewSkin.SeparateMesh = 0;

			OurSkins.AddItem(NewSkin);

			// Prefer the first or the selected skin if more than one encountered.
			/*
			if ((((INode*)SerialTree[i].node)->Selected()) || (PhysiqueNodes == 0))
			{
					OurSkin = SerialTree[i].node;
			}
			*/

			PhysiqueNodes++;
		}
	}

	////PopupBox("Number of skinned nodes: %i",OurSkins.Num());

	//DebugBox("Add skinlink stats");

	// Accumulate some skin linking stats.
	for( i=0; i<SerialTree.Num(); i++)
	{
		if (SerialTree[i].LinksToSkin) LinkedBones++;
		TotalSkinLinks += SerialTree[i].LinksToSkin;
	}

	//DebugBox("Tested main skin, bone links: %i total links %i",LinkedBones, TotalSkinLinks );

	/*
	//
	// Cull from skeleton all dummies that don't have any physique links to it and that don't have any children.
	//
	INT CulledDummies = 0;
	if (this->DoCullDummies)
	{
		for( i=0; i<SerialTree.Num(); i++)
		{	
			if ( 
			   ( SerialTree[i].IsDummy ) &&  
			   ( SerialTree[i].LinksToSkin == 0)  && 
			   ( ((INode*)SerialTree[i].node)->NumberOfChildren()==0 ) && 
			   ( SerialTree[i].HasTexture == 0 ) && 
			   ( SerialTree[i].IsBone == 1 )
			   )
			{
				SerialTree[i].IsBone = 0;
				SerialTree[i].IsCulled = 1;
				CulledDummies++;
			}
		}
		//PopupBox("Culled dummies: %i",CulledDummies);
	}
	*/
	
	if( DEBUGFILE && HaveLogPath() )
	{
		char LogFileName[] = ("GetSceneInfo.LOG");
		DLog.Open(m_logpath,LogFileName,DEBUGFILE);
	}

	for( i=0; i<SerialTree.Num(); i++)
	{	
		// 'physique / skincluster modifier' detection
		MFnDagNode DagNode = AxSceneObjects[ (int)SerialTree[i].node ]; 
		INT IsRoot = ( pTempActor->MatchNodeToSkeletonIndex( (void*) i ) == 0 );

		// If mesh, determine 
		INT MeshFaces = 0;
		INT MeshVerts = 0;
		if( SerialTree[i].IsSkin )
		{
			MFnMesh MeshFunction( AxSceneObjects[ (int)SerialTree[i].node ], &stat );
			MeshVerts = MeshFunction.numVertices();
			MeshFaces = MeshFunction.numPolygons();
		}

		//char NodeName[300]; NodeName = (((INode*)SerialTree[i].node)->GetName());
		DLog.Logf("Node: %4i - %22s %22s Dummy: %2i Bone:%2i Skin:%2i  Smooth:%2i PhyLk:%4i NodeID %10i ModifierIdx: %3i Parent %4i Children %4i HasTexture %i Culled %i Sel %i IsRoot %i \n",i,
			DagNode.name().asChar(), //NodeName
			DagNode.typeName().asChar(), //NodeName
			SerialTree[i].IsDummy,
			SerialTree[i].IsBone,
			SerialTree[i].IsSkin,
			SerialTree[i].IsSmooth,
			SerialTree[i].LinksToSkin,
			(INT)SerialTree[i].node,
			SerialTree[i].ModifierIdx,
			SerialTree[i].ParentIndex,
			SerialTree[i].NumChildren,
			//(INT)((INode*)SerialTree[i].node)->IsRootNode(),
			SerialTree[i].HasTexture,
			//((INode*)SerialTree[i].node)->NumberOfChildren(),
			SerialTree[i].IsCulled,
			SerialTree[i].IsSelected,
			IsRoot
			);

		if( MeshFaces )
		{
			DLog.Logf("Mesh stats:  Faces %4i  Verts %4i \n", MeshFaces, MeshVerts );
		}
		
	}

	DLog.LogfLn("");

	{for( DWORD i=0; i<AxSkinClusters.length(); i++)
	{
		MFnDependencyNode Node( AxSkinClusters[i]);
	
		MObject SkinObject = AxSkinClusters[i];

		MFnSkinCluster skinCluster(SkinObject);

		DLog.Logf("Node: %4i - name: [%22s] type: [%22s] \n",
			i,
			Node.name().asChar(), 
			Node.typeName().asChar()			
			);


		DWORD nGeoms = skinCluster.numOutputConnections();

		if ( nGeoms == 0 ) 
			DLog.LogfLn("  > No influenced objects for this skincluster.");

		for (DWORD ii = 0; ii < nGeoms; ++ii) 
		{
			unsigned int index = skinCluster.indexForOutputConnection(ii,&stat);
			// get the dag path of the ii'th geometry
			MDagPath bonePath;
			stat = skinCluster.getPathAtIndex(index,bonePath);

			// MFnDependencyNode Node( skinPath );
			// MObject BoneObject = Node.object();
			// MObject BoneGeom = bonePath.node();

			DLog.LogfLn("  > Influenced object#: %4i - name: [%s] type: [%s] ",
			ii,
			bonePath.fullPathName().asChar(), 
			//BoneGeom.typeName().asChar()			
			" - "
			);

		}
	}}

	DLog.Close();
	
	TreeInitialized = 1;	
	
	return 0;
}










// Skin exporting: from example plugin "ExportSkinClusterDataCmd"

#define CheckError(stat,msg)		\
	if ( MS::kSuccess != stat ) {	\
		MayaLog(msg);			\
		continue;					\
	}



/*
int DigestSoftSkin(VActor *Thing )
{
	
	size_t count = 0;
	
	// Iterate through graph and search for skinCluster nodes
	//
	MItDependencyNodes iter( MFn::kInvalid);
	MayaLog("Start Iterating graph....");

	for ( ; !iter.isDone(); iter.next() ) 
	{	

		MObject object = iter.item();
		if (object.apiType() == MFn::kSkinClusterFilter) 
		{
			count++;
			
			// For each skinCluster node, get the list of influencdoe objects
			//
			MFnSkinCluster skinCluster(object);
			MDagPathArray infs;
			MStatus stat;
			unsigned int nInfs = skinCluster.influenceObjects(infs, &stat);
			CheckError(stat,"Error getting influence objects.");

			if( 0 == nInfs ) 
			{
				stat = MS::kFailure;
				CheckError(stat,"Error: No influence objects found.");
			}
			
			// loop through the geometries affected by this cluster
			//
			unsigned int nGeoms = skinCluster.numOutputConnections();
			for (size_t ii = 0; ii < nGeoms; ++ii) 
			{
				unsigned int index = skinCluster.indexForOutputConnection(ii,&stat);
				CheckError(stat,"Error getting geometry index.");

				// Get the dag path of the ii'th geometry.
				//
				MDagPath skinPath;
				stat = skinCluster.getPathAtIndex(index,skinPath);
				CheckError(stat,"Error getting geometry path.");

				// iterate through the components of this geometry (vertices)
				//
				MItGeometry gIter(skinPath);

				//
				// print out the path name of the skin, vertexCount & influenceCount
				//
				// #TODO
				// fprintf(file,"%s [vertexcount %d] [Influences %d] \r\n",skinPath.partialPathName().asChar(), gIter.count(),	nInfs);
				
				// print out the influence objects
				//
				for (size_t kk = 0; kk < nInfs; ++kk) 
				{
					// #TODO
					MayaLog("[%s] \r\n",infs[kk].partialPathName().asChar());
				}
				// fprintf(file,"\r\n");
			
				for (  ; !gIter.isDone(); gIter.next() ) 
				{
					MObject comp = gIter.component(&stat);
					CheckError(stat,"Error getting component.");

					// Get the weights for this vertex (one per influence object)
					//
					MFloatArray wts;
					unsigned int infCount;
					stat = skinCluster.getWeights(skinPath,comp,wts,infCount);
					CheckError(stat,"Error getting weights.");
					if (0 == infCount) 
					{
						stat = MS::kFailure;
						CheckError(stat,"Error: 0 influence objects.");
					}

					// Output the weight data for this vertex
					//
					float TotalWeight = 0.0f;

					// fprintf(file,"[%4d] ",gIter.index());

					for (unsigned int jj = 0; jj < infCount ; ++jj ) 
					{
						float BoneWeight = wts[jj];
						if( BoneWeight != 0.0f )
						{
							TotalWeight+=BoneWeight;
							// fprintf(file,"Bone: %4i Weight: %9f ",jj,wts[jj]);
						}
					}
					// fprintf(file," Total: %9.5f",TotalWeight);
					// fprintf(file,"\r\n");
				}
			}
		}
	}

	//if (0 == count) {
		//displayError("No skinClusters found in this scene.");
	//}
	//fclose(file);
	//return MS::kSuccess;

	return 1;
}
*/

	
//
// Digest a single animation. called after heirarchy/nodelist digested.
//
int SceneIFC::DigestAnim(class VActor *Thing,const char *AnimName, int from, int to)
{
	if( DEBUGFILE && HaveLogPath())
	{
		DLog.Close();//precaution
		char LogFileName[] = ("DigestAnim.LOG");
		DLog.Open(m_logpath,LogFileName,DEBUGFILE);
	}

	INT TimeSlider = 0;
	INT AnimEnabled = 1;

	FLOAT TimeStart, TimeEnd;
	TimeStart = 0.0f;
	TimeEnd   = 0.0f;

	TimeStart = GetMayaTimeStart();
	TimeEnd = GetMayaTimeEnd();

	//#TODO
	INT FirstFrame = (INT)TimeStart;
	INT LastFrame = (INT)TimeEnd;
	if (0 <= from && from < to) {
		FirstFrame = from;
		LastFrame = to;
	}

	// Parse a framerange that's identical to or a subset (possibly including same frame multiple times) of the slider range.
	// ParseFrameRange( RangeString, FirstFrame, LastFrame );
	FrameList.Empty();
	FrameList.StartFrame = from;
	FrameList.EndFrame   = to;
	for (int k = from; k <= to; k++) {
		FrameList.AddFrame(k);
	}

	OurTotalFrames = FrameList.GetTotal();	

	////PopupBox(" Animation range: start %f  end %f  frames %i ", TimeStart, TimeEnd, OurTotalFrames );

	Thing->BetaKeys.Empty();
	Thing->BetaNumFrames = OurTotalFrames; 
	Thing->BetaNumBones  = OurBoneTotal;
	Thing->BetaKeys.SetSize( OurTotalFrames * OurBoneTotal );
	Thing->BetaNumBones  = OurBoneTotal;
	Thing->FrameTotalTicks = OurTotalFrames; // * FrameTicks; 
	Thing->FrameRate = 30.0f; // GetFrameRate(); //FrameRate;  #TODO proper Maya framerate retrieval.

	// //PopupBox(" Frame rate %f  Ticks per frame %f  Total frames %i FrameTimeInfo %f ",(FLOAT)GetFrameRate(), (FLOAT)FrameTicks, (INT)OurTotalFrames, (FLOAT)Thing->FrameTotalTicks );
	// //PopupBox(" TotalFrames/BetaNumFrames: %i ",Thing->BetaNumFrames);

	//_tcscpy(Thing->BetaAnimName,CleanString( AnimName ));
	_tcscpy(Thing->BetaAnimName,AnimName);
	CleanString(Thing->BetaAnimName);

	//
	// Rootbone is in tree starting from RootBoneIndex.
	//

	int KeyIndex  = 0;
	int FrameCount = 0;
	int TotalAnimNodes = SerialTree.Num()-RootBoneIndex; 
	
	for( INT t=0; t<FrameList.GetTotal(); t++)		
	{
		FLOAT TimeNow = FrameList.GetFrame(t); // * FrameTicks;
		SetMayaSceneTime( TimeNow );

		for (int i = 0; i<SerialTree.Num(); i++) // i = RootBoneIndex...
		{
			/*
			bool our_set = false;

			if (m_client->GetSetRoot()) {
				MObject JointObject = AxSceneObjects[ (int)SerialTree[i].node ]; 
				if (JointObject.hasFn(MFn::kJoint)) {
					our_set = Depends( JointObject );
				}
			}
			*/

			if ( SerialTree[i].InSkeleton ) 
			{
				//MFnDagNode DagNode  = AxSceneObjects[ (int)SerialTree[i].node ]; 
				//MObject JointObject = AxSceneObjects[ (int)SerialTree[i].node ]; 
				MFnDagNode DagNode  = AxSceneObjects[ (int)SerialTree[i].node ]; 
				MObject JointObject = AxSceneObjects[ (int)SerialTree[i].node ]; 

				INT ParentIndex = (int)SerialTree[i].ParentIndex;
				if( ParentIndex < 0)
					ParentIndex = 0;
				MObject ParentObject = AxSceneObjects[ (int)SerialTree[ParentIndex].node ]; 

				// Check whether root bone
				INT RootCheck = ( Thing->MatchNodeToSkeletonIndex( (void*) i ) == 0) ? 1:0 ;			

				DLog.LogfLn(" Skeletal bone name [] num %i parent %i  NumChildren %i  Isroot %i RootCheck %i",i,(int)SerialTree[i].ParentIndex,SerialTree[i].NumChildren, (i != RootBoneIndex)?0:1,RootCheck );

				FMatrix LocalMatrix;
				FQuat   LocalQuat;
				FVector BoneScale(1.0f,1.0f,1.0f);

				// RootCheck
				// if( RootCheck && t==0) //PopupBox(" Root in Anim digest: serialtree [%i] ",i);
				RetrieveTrafoForJoint( JointObject, LocalMatrix, LocalQuat, RootCheck, BoneScale );

				// Raw translation
				FVector Trans( LocalMatrix.WPlane.X, LocalMatrix.WPlane.Y, LocalMatrix.WPlane.Z );				
				// Incorporate matrix scaling - for translation only.
				Trans.X *= BoneScale.X;
				Trans.Y *= BoneScale.Y;
				Trans.Z *= BoneScale.Z;

				FQuat Orient;
				//if ( RootCheck ) 				
				Orient = FQuat(LocalQuat.X, LocalQuat.Y, LocalQuat.Z,LocalQuat.W);  //#SKEL - attempt full turn.. Z and Y switched 
			
				Thing->BetaKeys[KeyIndex].Orientation = Orient;
				Thing->BetaKeys[KeyIndex].Position = Trans;

				KeyIndex++;
			}			
		}
		FrameCount++;
	}

	//#debug
	//PopupBox("Animation digested: [%s] total frames: %i Ourbonetotal:%i total keys: %i  ",AnimName, FrameCount, OurBoneTotal, Thing->BetaKeys.Num());

	DLog.Close();

	return 1;
}




/*
void defineMaterials(void)
{
	int	  count;
	int   matNum;
	char *name;
	float ared, agreen, ablue;
	float dred, dgreen, dblue;
	float sred, sgreen, sblue;
	float ered, egreen, eblue;
	float shininess;
	float transparency;
	char *texName;

	if ( rtg_output_materials ) {

		DtMtlGetSceneCount( &count );

		doTabs();
		fprintf(outputFile, "MATERIAL_LIST %d", count);
		doCR();

		for ( matNum = 0; matNum < count; matNum++ ) {

			tabs++;

			doTabs();
			fprintf(outputFile, "MATERIAL %d", matNum);
			doCR();

			DtMtlGetNameByID( matNum, &name );

			DtMtlGetAllClrbyID( matNum,0, 
							    &ared, &agreen,&ablue,
							    &dred, &dgreen, &dblue,
							    &sred, &sgreen, &sblue,
							    &ered, &egreen, &eblue,
							    &shininess, &transparency );


			DtTextureGetName( name, &texName );


			doTabs();
			fprintf(outputFile, "NAME %s", name );
			doCR();

			doTabs();
			fprintf(outputFile, "AMBIENT %f %f %f", ared, agreen, ablue );
			doCR();

			doTabs();
			fprintf(outputFile, "DIFFUSE %f %f %f", dred, dgreen, dblue );
			doCR();

			doTabs();
			fprintf(outputFile, "SPECULAR %f %f %f", sred, sgreen, sblue );
			doCR();

			doTabs();
			fprintf(outputFile, "EMMISION %f %f %f", ered, egreen, eblue );
			doCR();

			doTabs();
			fprintf(outputFile, "SHININESS %f", shininess );
			doCR();

			doTabs();
			fprintf(outputFile, "TRANSPARENCY %f", transparency );
			doCR();

			if ( texName ) {
				doTabs();
				fprintf(outputFile, "TEXTURE_NAME %s", texName );
				doCR();

				//getTextureFiles( name, texName );
				char *fName = NULL;
				DtTextureGetFileName( name, &fName );
				if ( fName && fName[0] )
				{
					doTabs();
					fprintf(outputFile, "TEXTURE_FILENAME %s", fName );
					doCR();
				}
					
			}

			if ( DtExt_MultiTexture() )
			{
				defineMultiTextures( matNum );
			}

			doCR();

			tabs--;
		}
		doTabs();
		fprintf(outputFile, "END_MATERIAL_LIST");
		doCR();
		doCR();

	}
}
*/
			

// Digest points, weights, wedges, faces..
int	SceneIFC::ProcessMesh(AXNode* SkinNode, int TreeIndex, VActor *Thing, VSkin& LocalSkin, INT SmoothSkin )
{

	MStatus	stat;
	MObject SkinObject = AxSceneObjects[ (INT)SkinNode ];
	MFnDagNode currentDagNode( SkinObject, &stat );

	MFnMesh MeshFunction( SkinObject, &stat );
	// #todo act on SmoothSkin flag
	if ( stat != MS::kSuccess ) 
	{
			//PopupBox(" Mesh methods could not be created for MESH %s",currentDagNode.name(&stat).asChar() );
			return 0;
	}

	MFnDependencyNode	Node(SkinObject);

	// Retrieve the first dag path.
    MDagPath ShapedagPath;
    stat = currentDagNode.getPath( ShapedagPath );

	// If we are doing world space, then we need to initialize the 
	// Mesh with a DagPath and not just the node

	MFloatPointArray	Points;	
	// Get points array & iterate vertices.
	// MeshFunction.getPoints( Points, MSpace::kWorld); // can also get other spaces!!				
	MeshFunction.setObject( ShapedagPath );
	MeshFunction.getPoints( Points, MSpace::kWorld ); // MSpace::kTransform);  //KWorld is wrong ?...

	int ParentJoint = TreeIndex;

	// Traverse hierarchy to find valid parent joint for this mesh. #DEBUG - make NoExport work !
	if( ParentJoint > -1) SerialTree[ParentJoint].NoExport = 0; //#SKEL
	
	while( (ParentJoint > 0)  &&  ((!SerialTree[ParentJoint].InSkeleton ) || ( SerialTree[ParentJoint].NoExport ) ) )
	{
		ParentJoint = SerialTree[ParentJoint].ParentIndex;
	}

	if( ParentJoint < 0) ParentJoint = 0;
	
	INT MatchedNode = Thing->MatchNodeToSkeletonIndex( (void*)ParentJoint ); // SkinNode ) ;

	// Not linked to a joint?
	if( (MatchedNode < 0) )
	{
		MatchedNode = 0;
	}


	INT SkinClusterVertexCount = 0;

	// //PopupBox("Found Parent [%i]   bone [%i]   for mesh %i [%s] ",ParentJoint,MatchedNode, TreeIndex,  currentDagNode.name(&stat).asChar() );

	if( !SmoothSkin )
	{
		for( int PointIndex = 0; PointIndex < MeshFunction.numVertices(); PointIndex++)
		{
			MFloatPoint	Vertex;
			// Get the vertex info.
			Vertex = Points[PointIndex];
			
			// Max: SkinLocal brings the skin 3d vertices into world space.
			// Point3 VxPos = OurTriMesh->getVert(i)*SkinLocalTM;

			VPoint NewPoint;
			// #inversion..
			NewPoint.Point.X =  Vertex.x; // -z
			NewPoint.Point.Y =  Vertex.y; // -x
			NewPoint.Point.Z =  Vertex.z; //  y

			LocalSkin.Points.AddItem(NewPoint); 		

			DLog.Logf(" # Skin vertex %i   Position: %f %f %f \n",PointIndex,NewPoint.Point.X,NewPoint.Point.Y,NewPoint.Point.Z);

			VRawBoneInfluence TempInf;
			TempInf.PointIndex = PointIndex; // Raw skinvertices index
			TempInf.BoneIndex  = MatchedNode; // Bone is it's own node.
			TempInf.Weight     = 1.0f;

			LocalSkin.RawWeights.AddItem(TempInf);  					
		}
	}
	else // Smooth skin (SkinCluster)
	{

		// First the points, then the weights ??????????? &
		for( int PointIndex = 0; PointIndex < MeshFunction.numVertices(); PointIndex++)
		{
				MFloatPoint	Vertex;
				// Get the vertex info.
				Vertex = Points[PointIndex];
				
				// Max: SkinLocal brings the skin 3d vertices into world space.
				// Point3 VxPos = OurTriMesh->getVert(i)*SkinLocalTM;

				VPoint NewPoint;
				// #inversion
				NewPoint.Point.X =  Vertex.x; // -z
				NewPoint.Point.Y =  Vertex.y; // -x
				NewPoint.Point.Z =  Vertex.z; //  y

				LocalSkin.Points.AddItem(NewPoint); 		
		}

		// Current object:"SkinObject/ShapeDagPath".
		MFnSkinCluster skinCluster( AxSkinClusters[SerialTree[TreeIndex].ModifierIdx] );
		// SerialTree[TreeIndex].ModifierIdx holds the skincluster 'modifier', from which we can
		// obtain the proper weighted vertices.

		MDagPathArray infs;
		MStatus stat;
		unsigned int nInfs = skinCluster.influenceObjects(infs, &stat);

		// The array of nodes that influence the vertices should be traceable back through their bone names (or dagpaths?) into the skeletal array.

		//  Create translation array - influence index -> refskeleton index.
		TArray <INT> BoneIndices;
		// Match bone to influence
		for( DWORD b=0; b < nInfs; b++)
		{			
			INT BoneIndex = -1;
			for (int i=0; i<Thing->RefSkeletonNodes.Num(); i++)
			{
				// Object to Dagpath
				MObject TestBoneObj( AxSceneObjects[(INT)Thing->RefSkeletonNodes[i] ]  );
				MFnDagNode DNode( TestBoneObj, &stat );
				MDagPath BonePath;
				DNode.getPath( BonePath );
				
				if ( infs[b] == BonePath )
					BoneIndex = i;  // Skeletal index..
			}


			#if 0
			// See if we can find a valid parent if our node happens to not be a bone in the skeleton.
			if( BoneIndex = -1 )
			{
				INT LeadIndex = -1;
				// First find our node:
				for(int i=0; i<SerialTree.Num(); i++)
				{
					MObject TestBoneObj( AxSceneObjects[(INT)SerialTree[i].node ] );
					MFnDagNode DNode( TestBoneObj, &stat );
					MDagPath BonePath;
					DNode.getPath( BonePath );					
					if ( infs[b] == BonePath )
					{
						LeadIndex = i;
					}
				}
				//
				// See if SerialTree[i].node has known-bone parent...
				//				
				if( (LeadIndex > -1 ) && SerialTree[LeadIndex].NoExport ) // ??? //#SKEL: do this for ALL leadIndex>-1...
				{
					//PopupBox("Finding alternative BoneIndex = %i ",BoneIndex); //#SKEL
					// Traverse until we find a bone parent... assumes all hierarchy up-chains end at -1 / 0
					while( LeadIndex > 0 && !SerialTree[LeadIndex].IsBone )
					{
						LeadIndex = SerialTree[LeadIndex].ParentIndex;
					}

					if( LeadIndex >= 0) // Found valid parent - now get the skeletal index of it..
					{
						INT AltParentNode = (INT)SerialTree[LeadIndex].node;
						for(int i=0; i<Thing->RefSkeletonNodes.Num(); i++)
						{
							if( Thing->RefSkeletonNodes[i] == (void*)AltParentNode )
							{
								BoneIndex = i; 
								//PopupBox(" Alternative bone parent found for noexport-bone: %i ",BoneIndex ); //#SKEL
							}
						}						
					}
				}
		
			}
			#endif

			// Note:  noexport-bone's influences automatically revert to the closest valid parent in the heirarchy.
			BoneIndices.AddItem(BoneIndex > -1 ? BoneIndex: 0 );
		}

		

		// Also allow for things linked to bones 
		// SerialTree[i].NoExport = 1;
		
		unsigned int nGeoms = skinCluster.numOutputConnections();

		for (DWORD ii = 0; ii < nGeoms; ++ii) 
		{
			unsigned int index = skinCluster.indexForOutputConnection(ii,&stat);

			// get the dag path of the ii'th geometry
			//
			MDagPath skinPath;
			stat = skinCluster.getPathAtIndex(index,skinPath);
			//CheckError(stat,"Error getting geometry path.");

			// Is this DagPath pointing to the current Mesh's path ??
			if ( skinPath == ShapedagPath )
			{
				// go ahead and save all weights and points....
				// iterate through the components of this geometry (vertices)
				//
				MItGeometry gIter(skinPath);
				//
				// //PopupBox(" Found CLusterPath  %s [vertexcount %d] [Influences %d] \r\n",skinPath.partialPathName().asChar(),gIter.count(),nInfs) ;			
							
				INT VertexCount = 0;
				for ( ; !gIter.isDone(); gIter.next() ) 
				{
					MObject comp = gIter.component(&stat);
					CheckError(stat,"Error getting component.");

					// Get the weights for this vertex (one per influence object)
					//
					MFloatArray wts;
					unsigned int infCount;
					stat = skinCluster.getWeights(skinPath,comp,wts,infCount);
					//CheckError(stat,"Error getting weights.");
					//if (0 == infCount) {
					//	stat = MS::kFailure;
					//	CheckError(stat,"Error: 0 influence objects.");
					//}

					// Output the weight data for this vertex
					float TotalWeight = 0.0f;

					//fprintf(file,"[%4d] ",gIter.index());
					for (unsigned int jj = 0; jj < infCount ; ++jj ) 
					{
						float BoneWeight = wts[jj];
						if( BoneWeight != 0.0f )
						{
							VRawBoneInfluence TempInf;
							TempInf.PointIndex = VertexCount; // PointIndex.../ Raw skinvertices index
							TempInf.BoneIndex  = BoneIndices[jj];          //Translate bone index into skeleton bone index
							TempInf.Weight     = wts[jj];

							LocalSkin.RawWeights.AddItem(TempInf);  												
							//fprintf(file,"Bone: %4i Weight: %9f ",jj,wts[jj]);
						}
					}
					VertexCount++;
					SkinClusterVertexCount++;
				}				
			}
		}		
	}


	// //PopupBox("{}{}{} Vertices: %i  Weighted: %i Total Weights: %i ",LocalSkin.Points.Num(),SkinClusterVertexCount, LocalSkin.RawWeights.Num());


	//
	// Digest any mesh triangle data -> into unique wedges (ie common texture UV'sand (later) smoothing groups.)
	//

	UBOOL NakedTri = false;
	int Handedness = 0;
	int NumFaces = MeshFunction.numPolygons(&stat);
	
	if( LocalSkin.Points.Num() ==  0 ) // Actual points allocated yet ? (Can be 0 if no matching bones were found)
	{
		MString   NodeName;
		MStatus status;
		MFnDependencyNode dgNode;
		status = dgNode.setObject( SkinObject );
		NodeName = dgNode.name( &status );

		NumFaces = 0;
		////ErrorBox("Node without matching vertices encountered: [%s]", NodeName);
	}

	//DebugBox("Digesting %i Triangles.",NumFaces);
	DLog.LogfLn("Digesting %i Triangles.",NumFaces);

	//Arguments
	//instanceNumber The instance number of the mesh to query 
	//shaders Storage for set objects (shader objects) 
	//indices Storage for indices matching faces to shaders. For each face, this array contains the index into the shaders array for the shader assigned to the face.

	MObjectArray Shaders;
	MIntArray ShaderIndices;
	MeshFunction.getConnectedShaders( 0, Shaders, ShaderIndices);

	if( Shaders.length() == 0) 
	{
		//ErrorBox("Warning: possible shader-less geometry encountered for node [%s] ",Node.name().asChar());
	}
	

	// Get Maya normals.. ?
	// MFloatVectorArray Normals;
	// stat = MeshFunction.getNormals( Normals, MSpace:kWorld );

	//#SKEL! handedness
	for (int TriIndex = 0; TriIndex < NumFaces; TriIndex++)
	{
		// Face*	TriFace			= &OurTriMesh->faces[TriIndex];
		// NewFace.SmoothingGroups = TriFace->getSmGroup();

		MIntArray	FaceVertices;

		// Get the vertex indices for this polygon.
		MeshFunction.getPolygonVertices(TriIndex,FaceVertices);

		INT VertCount = FaceVertices.length();

		// Treat as triangle fan... #TODO account for >3 vertex polygons.
		/*
		NumVerts = Vertices.length();
		for( int VertexIndex = 0; VertexIndex < NumVerts;VertexIndex++ )
		{
			MeshFunction.getPolygonUV( TriIndex,VertexIndex,U,V );
		}
		*/

		VVertex Wedge0;
		VVertex Wedge1;
		VVertex Wedge2;
		VTriangle NewFace;

		/*
		int	PolyIndex;

		// Set the mesh's triangle base.

		Mesh->TriangleBase = Header.NumTriangles;

		// Iterate through the mesh's polygons.

		for(PolyIndex = 0;PolyIndex < Mesh->MeshAPI.numPolygons();PolyIndex++)
		{
			MIntArray	Vertices;
			int			VertexIndex,s
						NumVertices;

			// Get the vertex indices for this polygon.

			Mesh->MeshAPI.getPolygonVertices(PolyIndex,Vertices);

			// Write a triangle fan.

			NumVertices = Vertices.length();
			Writer << NumVertices;

			for(VertexIndex = 0;VertexIndex < NumVertices;VertexIndex++)
			{
				float	U,
						V;

				Mesh->MeshAPI.getPolygonUV(PolyIndex,VertexIndex,U,V);

				Writer << Vertices[VertexIndex];
				Writer << U << V;
			}

			Mesh->NumTriangles += Vertices.length() - 2;
			NumPolygons++;
		}

		Header.NumTriangles += Mesh->NumTriangles;
		Mesh = Mesh->NextMesh;
		*/


		// Normals (determine handedness...
		//Point3 MaxNormal = OurTriMesh->getFaceNormal(TriIndex);		
		// Transform normal vector into world space along with 3 vertices. 
		//MaxNormal = VectorTransform( Mesh2WorldTM, MaxNormal );

		// FVector FaceNormal = MeshNormals(TriIndex );
		// Can't use MeshNormals -> vertex normals, we want face normals. 

		//#SKEL
		NewFace.WedgeIndex[0] = LocalSkin.Wedges.Num();
		NewFace.WedgeIndex[1] = LocalSkin.Wedges.Num()+1;
		NewFace.WedgeIndex[2] = LocalSkin.Wedges.Num()+2;

		//if (&LocalSkin.Points[0] == NULL )//PopupBox("Triangle digesting 1: [%i]  %i %i",TriIndex,(DWORD)TriFace, (DWORD)&LocalSkin.Points[0]); //#! LocalSkin.Points[9] failed;==0..

		// Face's vertex indices ('point indices')
			
		//#SKEL
		Wedge0.PointIndex = FaceVertices[0]; //MeshFunction.getPolygonVertices( 0,VertexIndex );
		Wedge1.PointIndex = FaceVertices[1]; //MeshFunction.getPolygonVertices( 1,VertexIndex );
		Wedge2.PointIndex = FaceVertices[2]; //MeshFunction.getPolygonVertices( 2,VertexIndex );

		DLog.LogfLn(" New point indices %i %i %i  pointarray size: %i",Wedge0.PointIndex,Wedge1.PointIndex,Wedge2.PointIndex, LocalSkin.Points.Num());

		// Vertex coordinates are in localskin.point, already in worldspace.
		FVector WV0 = LocalSkin.Points[ Wedge0.PointIndex ].Point;
		FVector WV1 = LocalSkin.Points[ Wedge1.PointIndex ].Point;
		FVector WV2 = LocalSkin.Points[ Wedge2.PointIndex ].Point;

		// Figure out the handedness of the face by constructing a normal and comparing it to Maya's (??)s normal.		
		// FVector OurNormal = (WV0 - WV1) ^ (WV2 - WV0);
		// OurNormal /= sqrt(OurNormal.SizeSquared()+0.001f);// Normalize size (unnecessary ?);			
		// Handedness = ( ( OurNormal | FaceNormal ) < 0.0f); // normals anti-parallel ?

		BYTE MaterialIndex = ShaderIndices[TriIndex] & 255;
		// TriFace->getMatID() & 255; // 255 & (TriFace->flags >> 32);  ???
		
		//NewFace.MatIndex = MaterialIndex;
		Wedge0.Reserved = 0;
		Wedge1.Reserved = 0;
		Wedge2.Reserved = 0;

		////WarningBox("Inside Get wedges4d - Tri index: %i",TriIndex );//#SKEL!!
		////WarningBox("Inside Get wedges4d - MeshFunction tris total: %i", MeshFunction.numTriangles() );//#SKEL!!
		////WarningBox("Inside Get wedges4d - MeshFunction tris total: %i", MeshFunction.numPolygons() );//#SKEL!!

		//
		// Get UV texture coordinates: allocate a Wedge with for each and every 
		// UV+ MaterialIndex ID; then merge these, and adjust links in the Triangles 
		// that point to them.
		//

		if( 1 ) // TriFace->flags & HAS_TVERTS )
		{
			/*
			MString  UVSet; // ? - Despite being optional, this helps prevent a weird crash...
			FLOAT U,V;

			stat = MeshFunction.getPolygonUV( TriIndex,0,U,V,&UVSet  );
			if ( stat != MS::kSuccess )
				DLog.Logf(" Invalid UV retrieval ");
			Wedge0.U =  U;
			Wedge0.V =  1.0f - V; // The Y-flip: an Unreal requirement..

			stat = MeshFunction.getPolygonUV( TriIndex,1,U,V,&UVSet  );
			if ( stat != MS::kSuccess )
				DLog.Logf(" Invalid UV retrieval ");
			Wedge1.U =  U;
			Wedge1.V =  1.0f - V; 
			
			stat = MeshFunction.getPolygonUV( TriIndex,2,U,V,&UVSet  );
			if ( stat != MS::kSuccess )
				DLog.Logf(" Invalid UV retrieval ");
			Wedge2.U =  U;
			Wedge2.V =  1.0f - V;
			*/
			// mikearno - changed as the getPolygonUV function does not always
			// function correctly
			MItMeshPolygon poly_iter(SkinObject);
			int preIndex;
			poly_iter.setIndex( TriIndex, preIndex );
			float2 UV;

			stat = poly_iter.getUV(0,UV);
			if ( stat != MS::kSuccess )
				DLog.Logf(" Invalid UV retrieval ");
			Wedge0.U =  UV[0];
			Wedge0.V =  1.0f - UV[1]; // The Y-flip: an Unreal requirement..

			stat = poly_iter.getUV(1,UV);
			if ( stat != MS::kSuccess )
				DLog.Logf(" Invalid UV retrieval ");
			Wedge1.U =  UV[0];
			Wedge1.V =  1.0f - UV[1]; // The Y-flip: an Unreal requirement..
			
			stat = poly_iter.getUV(2,UV);
			if ( stat != MS::kSuccess )
				DLog.Logf(" Invalid UV retrieval ");
			Wedge2.U =  UV[0];
			Wedge2.V =  1.0f - UV[1]; // The Y-flip: an Unreal requirement..

		}
		else NakedTri = true; 					
		
		// To verify: DigestMaterial should give a correct final material index 
		// even for multiple multi/sub materials on complex hierarchies of meshes.

		INT ThisMaterial;
		if( Shaders.length()== 0) 
		{
			
			ThisMaterial = 0;
		}
		else		
		{
			ThisMaterial = DigestMayaMaterial(Shaders[MaterialIndex]);
		}


		NewFace.MatIndex = ThisMaterial;

		Wedge0.MatIndex = ThisMaterial;
		Wedge1.MatIndex = ThisMaterial;
		Wedge2.MatIndex = ThisMaterial;
		
		LocalSkin.Wedges.AddItem(Wedge0);
		LocalSkin.Wedges.AddItem(Wedge1);
		LocalSkin.Wedges.AddItem(Wedge2);

		LocalSkin.Faces.AddItem(NewFace);

		// #DEBUG
		int VertIdx = LocalSkin.Wedges[ LocalSkin.Wedges.Num()-1 ].PointIndex; // Last added 3d point.
		FVector VertPoint = LocalSkin.Points[ VertIdx ].Point;
		DLog.LogfLn(" [tri %4i ] Wedge UV  %f  %f  MaterialIndex %i  Vertex %i Vertcount %i ",TriIndex,LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].U ,LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].V ,LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].MatIndex, LocalSkin.Wedges[LocalSkin.Wedges.Num()-1].PointIndex,VertCount );
		DLog.LogfLn("            Vertex [%i] XYZ %6.6f  %6.6f  %6.6f", VertIdx, VertPoint.X,VertPoint.Y,VertPoint.Z );
	}

	TArray  <VVertex> NewWedges;
	TArray  <int>     WedgeRemap;

	WedgeRemap.SetSize( LocalSkin.Wedges.Num() ); 

	//DebugBox( "Digesting %i Wedges.", LocalSkin.Wedges.Num() );

	// Get wedge, see which others match and flag those,
	// then store wedge, get next nonflagged one. O(n^2/2)

	for( int t=0; t< LocalSkin.Wedges.Num(); t++ )
	{
		if( LocalSkin.Wedges[t].Reserved != 0xFF ) // not flagged ?
		{
			// Remember this wedge's unique new index.
			WedgeRemap[t] = NewWedges.Num();		
			NewWedges.AddItem( LocalSkin.Wedges[t] ); // then it's unique.

			// Any copies ?
			for (int d=t+1; d< LocalSkin.Wedges.Num(); d++)
			{
				if ( Thing->IsSameVertex( LocalSkin.Wedges[t], LocalSkin.Wedges[d] ) )
				{
					LocalSkin.Wedges[d].Reserved = 0xFF;
					WedgeRemap[d] = NewWedges.Num()-1;
				}
			}
		}
	}

	//
	// Re-point all indices from all Triangles to the proper unique-ified Wedges.
	// 

	for( TriIndex = 0; TriIndex < LocalSkin.Faces.Num(); TriIndex++)
	{		
		LocalSkin.Faces[TriIndex].WedgeIndex[0] = WedgeRemap[TriIndex*3+0];
		LocalSkin.Faces[TriIndex].WedgeIndex[1] = WedgeRemap[TriIndex*3+1]; //#SKEL
		LocalSkin.Faces[TriIndex].WedgeIndex[2] = WedgeRemap[TriIndex*3+2];

		// Verify we all mapped within new bounds.
		if (LocalSkin.Faces[TriIndex].WedgeIndex[0] >= NewWedges.Num()) //ErrorBox("Wedge Overflow 1");
		if (LocalSkin.Faces[TriIndex].WedgeIndex[1] >= NewWedges.Num()) //ErrorBox("Wedge Overflow 1");
		if (LocalSkin.Faces[TriIndex].WedgeIndex[2] >= NewWedges.Num()) //ErrorBox("Wedge Overflow 1");	

		// Flip faces?
		
		if (1) // (Handedness ) 
		{			
			//FlippedFaces++;
			INT Index1 = LocalSkin.Faces[TriIndex].WedgeIndex[2];
			LocalSkin.Faces[TriIndex].WedgeIndex[2] = LocalSkin.Faces[TriIndex].WedgeIndex[1];
			LocalSkin.Faces[TriIndex].WedgeIndex[1] = Index1;
		}
	}


	//  Replace old array with new.
	LocalSkin.Wedges.SetSize( 0 );
	LocalSkin.Wedges.SetSize( NewWedges.Num() );

	for( t=0; t<NewWedges.Num(); t++ )
	{
		LocalSkin.Wedges[t] = NewWedges[t];
		if (DEBUGFILE && HaveLogPath()) DLog.Logf(" [ %4i ] Wedge UV  %f  %f  Material %i  Vertex %i  XYZ %f %f %f \n",
			t,
			NewWedges[t].U,
			NewWedges[t].V,
			NewWedges[t].MatIndex,
			NewWedges[t].PointIndex, 
			LocalSkin.Points[NewWedges[t].PointIndex].Point.X,
			LocalSkin.Points[NewWedges[t].PointIndex].Point.Y,
			LocalSkin.Points[NewWedges[t].PointIndex].Point.Z 
			);
	}

	//DebugBox( " NewWedges %i SkinDataVertices %i ", NewWedges.Num(), LocalSkin.Wedges.Num() );

	if (DEBUGFILE)
	{
		DLog.Logf(" Digested totals: Wedges %i Points %i Faces %i \n", LocalSkin.Wedges.Num(),LocalSkin.Points.Num(),LocalSkin.Faces.Num());
	}

	return 1;
}

DWORD FlagsFromShader(MFnDependencyNode& )
{
	return MTT_Normal | MTT_NormalTwoSided;
}

/*
// THIS STUFF DOES NOT EFFECT THE RESULT, THIS INFO IS IN THE MATERIAL IN UNREAL
DWORD FlagsFromShader(MFnDependencyNode& shader )
{
	bool two = true;
	bool translucent = false;
	bool weapon = false;
	bool unlit = false;
	bool enviro = false;
	bool nofiltering = false;
	bool modulate = false;
	bool masked = false;
	bool flat = false;
	bool alpha = false;

	bool temp;
	if (MGetAttribute(shader,"twosided").GetBasicValue(temp) == MS::kSuccess) {
		two = temp;
	}
	if (MGetAttribute(shader,"translucent").GetBasicValue(temp) == MS::kSuccess) {
		translucent = temp;
	}
	if (MGetAttribute(shader,"weapon").GetBasicValue(temp) == MS::kSuccess) {
		weapon = temp;
	}
	if (MGetAttribute(shader,"environment").GetBasicValue(temp) == MS::kSuccess) {
		enviro = temp;
	}
	if (MGetAttribute(shader,"nofiltering").GetBasicValue(temp) == MS::kSuccess) {
		nofiltering = temp;
	}
	if (MGetAttribute(shader,"modulate").GetBasicValue(temp) == MS::kSuccess) {
		modulate = temp;
	}
	if (MGetAttribute(shader,"masked").GetBasicValue(temp) == MS::kSuccess) {
		masked = temp;
	}
	if (MGetAttribute(shader,"flat").GetBasicValue(temp) == MS::kSuccess) {
		flat = temp;
	}
	if (MGetAttribute(shader,"alpha").GetBasicValue(temp) == MS::kSuccess) {
		alpha = temp;
	}

	BYTE MatFlags= MTT_Normal;
	
	if (two)
		MatFlags|= MTT_NormalTwoSided;
	
	if (translucent)
		MatFlags|= MTT_Translucent;
	
	if (masked)
		MatFlags|= MTT_Masked;
	
	if (modulate)
		MatFlags|= MTT_Modulate;
	
	if (unlit)
		MatFlags|= MTT_Unlit;
	
	if (flat)
		MatFlags|= MTT_Flat;
	
	if (enviro)
		MatFlags|= MTT_Environment;
	
	if (nofiltering)
		MatFlags|= MTT_NoSmooth;

	if (alpha)
		MatFlags|= MTT_Alpha;
	
	if (weapon)
		MatFlags= MTT_Placeholder;

	return (DWORD) MatFlags;
}
*/



DWORD FlagsFromName(char* pName )
{

//	BOOL	two=FALSE;
	BOOL	two=TRUE;
	BOOL	translucent=FALSE;
	BOOL	weapon=FALSE;
	BOOL	unlit=FALSE;
	BOOL	enviro=FALSE;
	BOOL	nofiltering=FALSE;
	BOOL	modulate=FALSE;
	BOOL	masked=FALSE;
	BOOL	flat=FALSE;
	BOOL	alpha=FALSE;

	// Substrings in arbitrary positions enforce flags.
	if (CheckSubString(pName,_T("twosid")))    two=true;
	if (CheckSubString(pName,_T("doublesid"))) two=true;
	if (CheckSubString(pName,_T("weapon"))) weapon=true;
	if (CheckSubString(pName,_T("modul"))) modulate=true;
	if (CheckSubString(pName,_T("mask")))  masked=true;
	if (CheckSubString(pName,_T("flat")))  flat=true;
	if (CheckSubString(pName,_T("envir"))) enviro=true;
	if (CheckSubString(pName,_T("mirro")))  enviro=true;
	if (CheckSubString(pName,_T("nosmo")))  nofiltering=true;
	if (CheckSubString(pName,_T("unlit")))  unlit=true;
	if (CheckSubString(pName,_T("bright"))) unlit=true;
	if (CheckSubString(pName,_T("trans")))  translucent=true;
	if (CheckSubString(pName,_T("opaque"))) translucent=false;
	if (CheckSubString(pName,_T("alph"))) alpha=true;
 
	BYTE MatFlags= MTT_Normal;
	
	if (two)
		MatFlags|= MTT_NormalTwoSided;
	
	if (translucent)
		MatFlags|= MTT_Translucent;
	
	if (masked)
		MatFlags|= MTT_Masked;
	
	if (modulate)
		MatFlags|= MTT_Modulate;
	
	if (unlit)
		MatFlags|= MTT_Unlit;
	
	if (flat)
		MatFlags|= MTT_Flat;
	
	if (enviro)
		MatFlags|= MTT_Environment;
	
	if (nofiltering)
		MatFlags|= MTT_NoSmooth;

	if (alpha)
		MatFlags|= MTT_Alpha;
	
	if (weapon)
		MatFlags= MTT_Placeholder;

	return (DWORD) MatFlags;
}


//
// Unreal flags from materials (original material tricks by Steve Sinclair, Digital Extremes ) 
// - uses classic Unreal/UT flag conventions for now.
// Determine the polyflags, in case a multimaterial is present
// reference the matID value (pass in the face's matID).
//

void CalcMaterialFlags( MObject& MShaderObject, VMaterial& Stuff )
{
	
	char pName[MAX_PATH];
	MFnDependencyNode shader( MShaderObject );		
	strcpy( pName, shader.name().asChar() );

	// check for attribute
	MString matname;
	if (MGetAttribute(shader,"URMaterialName").GetBasicValue(matname) == MS::kSuccess) {
		strcpy( pName, matname.asChar() );
	}

	Stuff.SetName(pName);

	// Check skin index 
	INT skinIndex = FindValueTag( pName, _T("skin") ,2 ); // Double (or single) digit skin index.......
	if ( skinIndex < 0 ) 
	{
		skinIndex = 0;
		Stuff.AuxFlags = 0;  
	}
	else
		Stuff.AuxFlags = 1; // Indicates an explicitly defined skin index.

	Stuff.TextureIndex = skinIndex;
	
	// Lodbias
	INT LodBias = FindValueTag( pName, _T("lodbias"),2); 
	if ( LodBias < 0 ) LodBias = 5; // Default value when tag not found.
	Stuff.LodBias = LodBias;

	// LodStyle
	INT LodStyle = FindValueTag( pName, _T("lodstyle"),2);
	if ( LodStyle < 0 ) LodStyle = 0; // Default LOD style.
	Stuff.LodStyle = LodStyle;

//	Stuff.PolyFlags = FlagsFromName(pName); //MatFlags;

	Stuff.PolyFlags = FlagsFromShader(shader);

}


int SceneIFC::DigestBrush( VActor *Thing )
{
	// Similar to digestskin but - digest all _selected_ meshes regardless of their linkup-state...
	SetMayaSceneTime( TimeStatic );

	// Maya shaders
	AxShaders.clear(); //.Empty();
	AxShaderFlags.Empty();

	Thing->SkinData.Points.Empty();
	Thing->SkinData.Wedges.Empty();
	Thing->SkinData.Faces.Empty();
	Thing->SkinData.RawWeights.Empty();
	Thing->SkinData.RawMaterials.Empty();
	Thing->SkinData.MaterialVSize.Empty();
	Thing->SkinData.MaterialUSize.Empty();
	Thing->SkinData.Materials.Empty();

	/*
	if (DEBUGFILE)
	{
		char LogFileName[MAX_PATH]; // = _T("\\X_AnimInfo_")+ _T(AnimName) +_T(".LOG");	
		sprintf(LogFileName,"\\ActXSkin.LOG");
		DLog.Open( m_client->GetLogPath(),LogFileName,DEBUGFILE);
	}
	//DLog.Logf(" Starting BRUSH digestion logfile \n");
	*/


	// Digest multiple meshes.

	for( INT i=0; i< SerialTree.Num(); i++ )
	{
		INT HasRootAsParent = 0;


		// If not a skincluster subject, Figure out whether the skeletal root is this mesh's parent somehow,
		// so that it is definitely part of the skin.
		if( SerialTree[i].IsSkin )
		{			
			MObject Object = AxSceneObjects[ (INT)SerialTree[i].node ];

			// Allow for zero-bone mesh digesting
			if( Thing->RefSkeletonNodes.Num() )
			{
				MObject RootBoneObj( AxSceneObjects[(INT)Thing->RefSkeletonNodes[0]] );
				MFnDagNode RootNode( RootBoneObj );
				if( RootNode.hasChild(Object) )
					HasRootAsParent = 1;
			}


			// Selection in Maya: the TRANSFORM-parent of a mesh got selected, not the actual mesh.
			if( SerialTree[i].ParentIndex >= 0 )
			{
				if( SerialTree[SerialTree[i].ParentIndex].IsSelected)
				{
					SerialTree[i].IsSelected = 1;
				}
			}
		}


		// Skin or mesh; if mesh, parent must be a joint unless it's a skincluster <- Restriction removed - exporting brushes here.
		if( SerialTree[i].IsSelected && SerialTree[i].IsSkin  ) // && ( SerialTree[i].IsSmooth || HasRootAsParent ) )
		{
			VSkin LocalSkin;

			// Smooth-skinned or regular?
			if( SerialTree[i].IsSmooth )
			{								
				// Skincluster deformable mesh.
				ProcessMesh( (void*)((INT)SerialTree[i].node), i, Thing, LocalSkin, 1 );
			}
			else
			{				
				// Regular mesh - but throw away if its parent is ROOT !
				ProcessMesh( (void*)((INT)SerialTree[i].node), i, Thing, LocalSkin, 0 );
			}

			// //PopupBox(" New points %i faces %i Wedges %i rawweights %i", LocalSkin.Points.Num(), LocalSkin.Faces.Num(), LocalSkin.Wedges.Num(), LocalSkin.RawWeights.Num() );

			// DLog.LogfLn(" New points %i faces %i Wedges %i rawweights %i", LocalSkin.Points.Num(), LocalSkin.Faces.Num(), LocalSkin.Wedges.Num(), LocalSkin.RawWeights.Num() );
			
			// Add points, wedges, faces, RAW WEIGHTS from localskin to Thing->Skindata.
			// Indices need to be shifted to create a single mesh soup!


			INT PointBase = Thing->SkinData.Points.Num();
			INT WedgeBase = Thing->SkinData.Wedges.Num();

			{for(INT j=0; j<LocalSkin.Points.Num(); j++)
			{
				Thing->SkinData.Points.AddItem(LocalSkin.Points[j]);
			}}

			// MAYA Z/Y reversal...
			{for(INT v=0;v<Thing->SkinData.Points.Num(); v++)
			{
				FLOAT Z = Thing->SkinData.Points[v].Point.Z;
				FLOAT Y = Thing->SkinData.Points[v].Point.Y;
				Thing->SkinData.Points[v].Point.Z =  Y;
				Thing->SkinData.Points[v].Point.Y = -Z;
			}}

			{for(INT j=0; j<LocalSkin.Wedges.Num(); j++)
			{
				VVertex Wedge = LocalSkin.Wedges[j];
				//adapt wedge's POINTINDEX...
				Wedge.PointIndex += PointBase;
				Thing->SkinData.Wedges.AddItem(Wedge);
			}}

			{for(INT j=0; j<LocalSkin.Faces.Num(); j++)
			{
				//WedgeIndex[3] needs replacing
				VTriangle Face = LocalSkin.Faces[j];
				// adapt faces indices into wedges...
				Face.WedgeIndex[0] += WedgeBase;
				Face.WedgeIndex[1] += WedgeBase;
				Face.WedgeIndex[2] += WedgeBase;
				Thing->SkinData.Faces.AddItem(Face);
			}}

			{for(INT j=0; j<LocalSkin.RawWeights.Num(); j++)
			{
				VRawBoneInfluence RawWeight = LocalSkin.RawWeights[j];
				// adapt RawWeights POINTINDEX......
				RawWeight.PointIndex += PointBase;
				Thing->SkinData.RawWeights.AddItem(RawWeight);
			}}

		}
	}

	FixMaterials(Thing);

	//DLog.Close();	
	return 1;
}


bool	SceneIFC::Depends(MObject const& tgt) const
{
	if (m_ikjoint == 0) {
		return true;
	}

	MObject base = m_ikjoint->object();

	MItDag diter;
	diter.reset( base, MItDag::kDepthFirst, MFn::kJoint );
	while (!diter.isDone()) {
		MObject obj = diter.item();
		MFnDagNode dObject(obj);
		MItDependencyGraph iter( obj, tgt.apiType() );
		while (!iter.isDone()) {
			MFnDagNode dObject(iter.thisNode());
			if (iter.thisNode() == tgt) {
				return true;
			}
			iter.next();
		}
		diter.next();
	}

	return false;
}


int	SceneIFC::DigestSkin(VActor *Thing )
{
	// DigestSoftSkin( Thing ); //#TODO

	//
	// if (OurSkins.Num() == 0) return 0; // no physique skin detected !!
	//
	//
	// We need a digest of all objects in the scene that are meshes belonging
	// to the skin; then go over these and use the parent node for non-physique ones,
	// and physique bones digestion where possible.
	//

	SetMayaSceneTime( TimeStatic );

	// Maya shaders
	AxShaders.clear(); //Empty();
	AxShaderFlags.Empty();

	Thing->SkinData.Points.Empty();
	Thing->SkinData.Wedges.Empty();
	Thing->SkinData.Faces.Empty();
	Thing->SkinData.RawWeights.Empty();
	Thing->SkinData.RawMaterials.Empty();
	Thing->SkinData.Materials.Empty();

	if (DEBUGFILE && HaveLogPath())
	{
		char LogFileName[MAX_PATH]; // = _T("\\X_AnimInfo_")+ _T(AnimName) +_T(".LOG");	
		sprintf(LogFileName,"ActXSkin.LOG");
		DLog.Open( m_logpath,LogFileName,DEBUGFILE);
	}

	DLog.Logf(" Starting skin digestion logfile \n");


	// Digest multiple meshes.
	MObject RootBoneObj( AxSceneObjects[(INT)Thing->RefSkeletonNodes[0]] );
	MFnDagNode RootNode( RootBoneObj );

	
	// mikearno
	for( INT i=0; i< SerialTree.Num(); i++ )
	{
		bool HasRootAsParent = false;
		bool our_set = (m_ikjoint == 0);

		// If not a skincluster subject, figure out whether the skeletal root is this mesh's parent somehow,
		// so that it is definitely part of the skin.
		if( SerialTree[i].IsSkin )
		{			
			MObject Object_i = AxSceneObjects[ (INT)SerialTree[i].node ];
			MFnDagNode DObject(Object_i);
			if( RootNode.hasChild(Object_i) ) {
				HasRootAsParent = true;
			}
			if (m_ikjoint) {
				our_set = Depends( Object_i );
			}
		}

		// Skin or mesh; if mesh, parent must be a joint unless it's a skincluster
		if(our_set && SerialTree[i].IsSkin && ( SerialTree[i].IsSmooth || HasRootAsParent ) )
		{

			VSkin LocalSkin;

			// Smooth-skinned or regular?
			if( SerialTree[i].IsSmooth)
			{								
				// Skincluster deformable mesh.
				ProcessMesh( (void*)((INT)SerialTree[i].node), i, Thing, LocalSkin, 1 );
			}
			else
			{				
				// Regular mesh - but throw away if its parent is ROOT !
				ProcessMesh( (void*)((INT)SerialTree[i].node), i, Thing, LocalSkin, 0 );
			}

			////PopupBox(" New points %i faces %i Wedges %i rawweights %i", LocalSkin.Points.Num(), LocalSkin.Faces.Num(), LocalSkin.Wedges.Num(), LocalSkin.RawWeights.Num() );

			DLog.LogfLn(" New points %i faces %i Wedges %i rawweights %i", LocalSkin.Points.Num(), LocalSkin.Faces.Num(), LocalSkin.Wedges.Num(), LocalSkin.RawWeights.Num() );
			
			// Add points, wedges, faces, RAW WEIGHTS from localskin to Thing->Skindata.
			// Indices need to be shifted to create a single mesh soup!

			INT PointBase = Thing->SkinData.Points.Num();
			INT WedgeBase = Thing->SkinData.Wedges.Num();

			{for(INT j=0; j<LocalSkin.Points.Num(); j++)
			{
				Thing->SkinData.Points.AddItem(LocalSkin.Points[j]);
			}}

			{for(INT j=0; j<LocalSkin.Wedges.Num(); j++)
			{
				VVertex Wedge = LocalSkin.Wedges[j];
				//adapt wedge's POINTINDEX...
				Wedge.PointIndex += PointBase;
				Thing->SkinData.Wedges.AddItem(Wedge);
			}}

			{for(INT j=0; j<LocalSkin.Faces.Num(); j++)
			{
				//WedgeIndex[3] needs replacing
				VTriangle Face = LocalSkin.Faces[j];
				// adapt faces indices into wedges...
				Face.WedgeIndex[0] += WedgeBase;
				Face.WedgeIndex[1] += WedgeBase;
				Face.WedgeIndex[2] += WedgeBase;
				Thing->SkinData.Faces.AddItem(Face);
			}}

			{for(INT j=0; j<LocalSkin.RawWeights.Num(); j++)
			{
				VRawBoneInfluence RawWeight = LocalSkin.RawWeights[j];
				// adapt RawWeights POINTINDEX......
				RawWeight.PointIndex += PointBase;
				Thing->SkinData.RawWeights.AddItem(RawWeight);
			}}
		}
	}


	FixMaterials(Thing);

	DLog.Close();	
	return 1;
}



int	SceneIFC::FixMaterials(VActor *Thing )
{
	//
	// Digest the accumulated Maya materials, and if necessary re-sort materials and update material indices.
	//

	UBOOL IndexOverride = false;

	for(DWORD i=0; i< AxShaders.length(); i++)
	{
		// Get materials from the collected objects.
		VMaterial Stuff;

		//
		char pName[MAX_PATH];
		MFnDependencyNode MFnShader ( AxShaders[i] );
		strcpy( pName, MFnShader.name().asChar() );
		Stuff.SetName(pName);

		// Calc material flags
		CalcMaterialFlags( AxShaders[i], Stuff );

		// SL MODIFY *** WATCH THIS ONE
		// override this 
		Stuff.TextureIndex = i;
		
		// Add to materials
		Thing->SkinData.Materials.AddItem( Stuff );

		// Reserved - for material sorting.
		Stuff.AuxMaterial = 0;

		// Skin index override detection.
		if (Stuff.AuxFlags ) IndexOverride = true;

		// Add texture source path
		VBitmapOrigin TextureSource;
		// Get path and bitmap name from shader, somehow...
		//GetBitmapName( TextureSource.RawBitmapName, TextureSource.RawBitmapPath, (Mtl*)Thing->SkinData.RawMaterials[i] );
		//
		TextureSource.RawBitmapName[0] = 0;
		TextureSource.RawBitmapPath[0] = 0;

		//
		Thing->SkinData.RawBMPaths.AddItem( TextureSource );


		// Retrieve texture size from MFnShader somehow..
		INT USize = 256;
		INT VSize = 256;
		MObject Object = MFnShader.object();
		GetMayaShaderTextureSize( Object, USize, VSize );

		////PopupBox("Shader [%i]  USize [%i] VSize [%i] ",i,USize,VSize); //#DEBUG

		Thing->SkinData.MaterialUSize.AddItem( USize );
		Thing->SkinData.MaterialVSize.AddItem( VSize );
	}


	//
	// Sort Thing->materials in order of 'skin' assignment.
	// Auxflags = 1: explicitly defined material -> these have be sorted in their assigned positions;
	// if they are incomplete (e,g, skin00, skin02 ) then the 'skin02' index becomes 'skin01'...
	// Wedge indices  then have to be remapped also.
	//
	if ( IndexOverride)
	{
		INT NumMaterials = Thing->SkinData.Materials.Num();
		// AuxMaterial will hold original index.
		for (int t=0; t<NumMaterials;t++)
		{
			Thing->SkinData.Materials[t].AuxMaterial = t;
		}

		// Bubble sort.		
		int Sorted = 0;
		while (Sorted==0)
		{
			Sorted=1;
			for (int t=0; t<(NumMaterials-1);t++)
			{	
				UBOOL SwapNeeded = false;
				if( Thing->SkinData.Materials[t].AuxFlags && (!Thing->SkinData.Materials[t+1].AuxFlags) )
				{
					SwapNeeded = true;
				}
				else
				{
					if( ( Thing->SkinData.Materials[t].AuxFlags && Thing->SkinData.Materials[t+1].AuxFlags ) &&
						( Thing->SkinData.Materials[t].TextureIndex > Thing->SkinData.Materials[t+1].TextureIndex ) )
						SwapNeeded = true;
				}				
				if( SwapNeeded )
				{
					Sorted=0;

					VMaterial VMStore = Thing->SkinData.Materials[t];
					Thing->SkinData.Materials[t]=Thing->SkinData.Materials[t+1];
					Thing->SkinData.Materials[t+1]=VMStore;					
										
					VBitmapOrigin BOStore = Thing->SkinData.RawBMPaths[t];
					Thing->SkinData.RawBMPaths[t] =Thing->SkinData.RawBMPaths[t+1];
					Thing->SkinData.RawBMPaths[t+1] = BOStore;					

					INT USize = Thing->SkinData.MaterialUSize[t];
					INT VSize = Thing->SkinData.MaterialVSize[t];
					Thing->SkinData.MaterialUSize[t] = Thing->SkinData.MaterialUSize[t+1];
					Thing->SkinData.MaterialVSize[t] = Thing->SkinData.MaterialVSize[t+1];
					Thing->SkinData.MaterialUSize[t+1] = USize;
					Thing->SkinData.MaterialVSize[t+1] = VSize;
				}			
			}
		}

		//DebugBox("Re-sorted material order for skin-index override.");	

		// Remap wedge material indices.
		if (1)
		{
			INT RemapArray[256];
			memset(RemapArray,0,256*sizeof(INT));
			{for( int t=0; t<NumMaterials;t++)
			{
				RemapArray[Thing->SkinData.Materials[t].AuxMaterial] = t;
			}}
			{for( int i=0; i<Thing->SkinData.Wedges.Num(); i++)
			{
				INT MatIdx = Thing->SkinData.Wedges[i].MatIndex;
				if ( MatIdx >=0 && MatIdx < Thing->SkinData.Materials.Num() )
					Thing->SkinData.Wedges[i].MatIndex =  RemapArray[MatIdx];
			}}		
		}
	}

	return 1;
}


int SceneIFC::DigestMaterial(AXNode *node,  INT matIndex,  TArray<void*>& RawMaterials)
{
	void* ThisMtl = (void*) matIndex;
	
	// The different material properties can be investigated at the end, for now we care about uniqueness only.
	RawMaterials.AddUniqueItem( (void*) ThisMtl );

	return( RawMaterials.Where( (void*) ThisMtl ) );
}


int SceneIFC::RecurseValidBones(const int RIndex, int &BoneCount)
{
	if ( SerialTree[RIndex].IsBone == 0) return 0; // Exit hierarchy for non-bones...
	if ( SerialTree[RIndex].InSkeleton == 1) return 0; // already counted

	SerialTree[RIndex].InSkeleton = 1; //part of our skeleton.
	BoneCount++;

	for (int c = 0; c < SerialTree[RIndex].NumChildren; c++) 
	{
		if( GetChildNodeIndex(RIndex,c) <= 0 ) //ErrorBox("GetChildNodeIndex assertion failed");

		RecurseValidBones( GetChildNodeIndex(RIndex,c), BoneCount); 
	}
	return 1;
}


int	SceneIFC::MarkBonesOfSystem(int RIndex)
{
	int BoneCount = 0;

	for( int i=0; i<SerialTree.Num(); i++)
	{
		SerialTree[i].InSkeleton = 0; // Erase skeletal flags.
		if (m_ikjoint) {
			MObject JointObject = AxSceneObjects[ (int)SerialTree[i].node ]; 
			if (m_ikjoint->object() == AxSceneObjects[ (int)SerialTree[i].node ]) {
				SerialTree[i].InSkeleton = 1;
			} else if (JointObject.hasFn(MFn::kJoint)) {
				SerialTree[i].InSkeleton = (Depends( JointObject )) ? 1 : 0;
			}
			BoneCount += SerialTree[i].InSkeleton;
		}
	}

	if (!m_ikjoint) {
		RecurseValidBones( RIndex, BoneCount );
	}

	// //DebugBox("Recursed valid bones: %i",BoneCount);

	// How to get the hierarchy with support of our 
	// flags, too:
	// - ( a numchildren, and look-for-nth-child routine for our array !)

	return BoneCount;
};

/*
int	SceneIFC::MarkBonesOfSystem(int RIndex)
{
	int BoneCount = 0;

	for( int i=0; i<SerialTree.Num(); i++)
	{
		SerialTree[i].InSkeleton = 0; // Erase skeletal flags.
	}

	RecurseValidBones( RIndex, BoneCount );

	// //DebugBox("Recursed valid bones: %i",BoneCount);

	// How to get the hierarchy with support of our 
	// flags, too:
	// - ( a numchildren, and look-for-nth-child routine for our array !)

	return BoneCount;
};
*/


//
// Evaluate the skeleton & set node flags...
//


int SceneIFC::EvaluateSkeleton(INT RequireBones)
{
	// Our skeleton: always the one hierarchy
	// connected to the scene root which has either the most
	// members, or the most SELECTED members, or a physique
	// mesh attached...
	// For now, a hack: find all root bones; then choose the one
	// which is selected.

	SetMayaSceneTime( TimeStatic );

	RootBoneIndex = 0;
	if (SerialTree.Num() == 0) return 0;

	int BonesMax = 0;

	RootBoneIndex = -1;
	if (m_ikjoint != 0) {
		// have a specific object
		MObject asObj = m_ikjoint->object();
		for( int i=0; RootBoneIndex<0 && i<SerialTree.Num(); i++) {
			MObject DagNode = AxSceneObjects[(int)SerialTree[i].node]; // object to DagNode... 
			if (DagNode == asObj) {
				RootBoneIndex = i; 
			}
		}
		

	} else {
		for( int i=0; RootBoneIndex<0 && i<SerialTree.Num(); i++)
		{
			// First bone assumed root bone //#TODO unsafe
			if( SerialTree[i].IsBone )
			{
				if (RootBoneIndex == -1)
					RootBoneIndex = i; 

				// All bones assumed to be in skeleton.
			//	SerialTree[i].InSkeleton = 1 ;
			//	BonesMax++;
			}
		}
	}

	BonesMax = MarkBonesOfSystem(RootBoneIndex);

	if (RootBoneIndex == -1)
	{
		if ( RequireBones ) //PopupBox("ERROR: Failed to find a root bone.");

		TotalBipBones = TotalBones = TotalMaxBones = TotalDummies = 0;
		return 0;// failed to find any.
	}

	OurRootBone = SerialTree[RootBoneIndex].node;
	OurBoneTotal = BonesMax;

	/*
	for( int i=0; i<SerialTree.Num(); i++)
	{	
		if( (SerialTree[i].IsBone != 0 ) && (SerialTree[i].ParentIndex == 0) )
		{
			// choose one with the biggest number of bones, that must be the
			// intended skeleton/hierarchy.
			int TotalBones = MarkBonesOfSystem(i);

			if (TotalBones>BonesMax)
			{
				RootBoneIndex = i;
				BonesMax = TotalBones;
			}
		}
	}

	if (RootBoneIndex == 0) return 0;// failed to find any.

	// Real node
	OurRootBone = SerialTree[RootBoneIndex].node;

	// Total bones
	OurBoneTotal = MarkBonesOfSystem(RootBoneIndex);

   */

	//
	// Digest bone-name functional tag substrings - 'noexport' means don't export a bone,
	// 'ignore' means don't do this bone nor any that are its children.
	//
	for (int i=0; i<SerialTree.Num(); i++) // i = RootBoneIndex...
	{
		if ( SerialTree[i].InSkeleton ) 
		{
			MFnDagNode DagNode = AxSceneObjects[ (int)SerialTree[i].node ]; 
			MObject JointObject = AxSceneObjects[ (int)SerialTree[i].node ]; 
			//Copy name. Truncate/pad for safety. &&&
			char BoneName[MAX_PATH];
			sprintf(BoneName,DagNode.name().asChar());

			SerialTree[i].NoExport = 0;
			// noexport: simply force this node not to be a bone
			if (CheckSubString(BoneName,_T("noexport")))  
			{
				SerialTree[i].IsBone = 0;
				SerialTree[i].NoExport = 1;
			}

			// Ignore: make sure anything down the hierarchy is not exported.
			if (CheckSubString(BoneName,_T("ignore")))  
			{
				SerialTree[i].IsBone = 0;
				SerialTree[i].IgnoreSubTree = 1;
			}

			// Todo? any exported geometry/skincluster references  that refer to such a non-exported
			// bone should default to bones higher in the hierarchy...			
		} else {
			SerialTree[i].NoExport = 1;
		}
	}

	// IgnoreSubTree 
	for ( i=0; i<SerialTree.Num(); i++) // i = RootBoneIndex...
	{
		INT pIdx = SerialTree[i].ParentIndex;
		if( pIdx>=0 && SerialTree[pIdx].IgnoreSubTree )
		{
			SerialTree[i].IsBone = 0;
			SerialTree[i].IgnoreSubTree = 1;
		}		
	}


	//
	// Tally the elements of the current skeleton - obsolete except for dummy culling.
	//
	for( i=0; i<SerialTree.Num(); i++)
	{
		if (SerialTree[i].InSkeleton )
		{
			TotalDummies  += SerialTree[i].IsDummy;
			TotalMaxBones += SerialTree[i].IsMaxBone;
			TotalBones    += SerialTree[i].IsBone;
		}
	}
	TotalBipBones = TotalBones - TotalMaxBones - TotalDummies;

	return 1;
};



int SceneIFC::DigestSkeleton(VActor *Thing)
{
	SetMayaSceneTime( TimeStatic );

	if( DEBUGFILE && HaveLogPath() )
	{
		char LogFileName[] = ("DigestSkeleton.LOG");
		DLog.Open(m_logpath,LogFileName,DEBUGFILE);
	}

	// Digest bones and their transformations
	DLog.Logf(" Starting DigestSkeleton log \n\n");                                 
	
	Thing->RefSkeletonBones.SetSize(0);  // SetSize( SerialNodes ); //#debug  'ourbonetotal ?'
	Thing->RefSkeletonNodes.SetSize(0);  // SetSize( SerialNodes );

	////PopupBox("DigestSkeleton start - our bone total: %i Serialnodes: %i  RootBoneIndex %i \n",OurBoneTotal,SerialTree.Num()); //#TODO

	// for reassigning parents...
	TArray <int> NewParents;

	// Assign all bones & store names etc.
	// Rootbone is in tree starting from RootBoneIndex.

	int BoneIndex = 0;

	//for (int i = RootBoneIndex; i<SerialNodes; i++)
	for (int i = 0; i<SerialTree.Num(); i++) // i = RootBoneIndex...
	{
		if ( SerialTree[i].InSkeleton ) 
		{ 
			////PopupBox("Bone in skeleton: %i",i);

			// construct:
			//Thing->RefSkeletonBones.
			//Thing->RefSkeletonNodes.

			NewParents.AddItem(i);
			VBone ThisBone; //= &Thing->RefSkeletonBones[BoneIndex];
			//INode* ThisNode = (INode*) SerialTree[i].node;	

			MFnDagNode DagNode = AxSceneObjects[ (int)SerialTree[i].node ]; 
			MObject JointObject = AxSceneObjects[ (int)SerialTree[i].node ]; 

			INT RootCheck = 0; // Thing->MatchNodeToSkeletonIndex( (void*) i );- skeletal hierarchy not yet digested.

			//Copy name. Truncate/pad for safety. 
			// char BoneName[32]; 
			// _snprintf(BoneName,32,"%s%s",DagNode.name().asChar(),"                                ");
			
			//#ERIK -reverted this to old code for safety.
			char BoneName[MAX_PATH];
			sprintf(BoneName,DagNode.name().asChar());
			ResizeString( BoneName,31); // will be padded if needed.
			_tcscpy((ThisBone.Name),BoneName);
			
			////PopupBox("Bone name: [%s] ",BoneName);

			ThisBone.Flags = 0;//ThisBone.Info.Flags = 0;
			ThisBone.NumChildren = SerialTree[i].NumChildren;//ThisBone.Info.NumChildren = SerialTree[i].NumChildren;

			if (i != RootBoneIndex)
			{
				//#debug needs to be Local parent if we're not the root.
				int OldParent = SerialTree[i].ParentIndex;
				int LocalParent = -1;

				// Find what new index this corresponds to. Parent must already be in set.
				for (int f=0; f<NewParents.Num(); f++)
				{
					if( NewParents[f] == OldParent ) LocalParent=f;
				}

				if (LocalParent == -1 )
				{
					// //ErrorBox(" Local parent lookup failed."); >> may be a transform rather than the root scene.
					LocalParent = 0;
				}

				ThisBone.ParentIndex = LocalParent;//ThisBone.Info.ParentIndex = LocalParent;
			}
			else
			{
				ThisBone.ParentIndex = 0;  // root links to itself...? .Info.ParentIndex
			}

			RootCheck = ( Thing->RefSkeletonBones.Num() == 0 ) ? 1:0; //#debug

			DLog.LogfLn(" Skeletal bone name %s num %i parent %i skelparent %i  NumChildren %i  Isroot %i RootCheck %i",BoneName,i,SerialTree[i].ParentIndex,ThisBone.ParentIndex,SerialTree[i].NumChildren, (i != RootBoneIndex)?0:1,RootCheck );

			FMatrix LocalMatrix;
			FQuat   LocalQuat;
			FVector BoneScale(1.0f,1.0f,1.0f);

			// Get parent object
			INT ParentIndex = (int)SerialTree[i].ParentIndex;
			if( ParentIndex < 0)
				ParentIndex = 0;

			//if( RootCheck )//PopupBox("Root bone detected; Bone index: %i SerialTree idx: %i ",Thing->RefSkeletonBones.Num(),i);
			RetrieveTrafoForJoint( JointObject, LocalMatrix, LocalQuat, RootCheck, BoneScale );
			{
				DLog.LogfLn("Bone scale for  joint [%i]  %f %f %f ",i, BoneScale.X,BoneScale.Y,BoneScale.Z ); //********
				MStatus stat;	
				MFnDagNode fnTransNode( JointObject, &stat );
				MDagPath dagPath;
				stat = fnTransNode.getPath( dagPath );
				if(stat == MS::kSuccess) DLog.LogfLn("Path for joint [%i] [%s]",i,dagPath.fullPathName().asChar()); //********
			}
			
			// Clean up matrix scaling.

			// Raw translation.
			FVector Trans( LocalMatrix.WPlane.X, LocalMatrix.WPlane.Y, LocalMatrix.WPlane.Z );
			// Incorporate matrix scaling - for translation only.
			Trans.X *= BoneScale.X;
			Trans.Y *= BoneScale.Y;
			Trans.Z *= BoneScale.Z;

			// //PopupBox("Local Bone offset for %s is %f %f %f ",DagNode.name().asChar(),Trans.X,Trans.Y,Trans.Z );			

			// Affine decomposition ?
			ThisBone.BonePos.Length = 0.0f ; //#TODO yet to be implemented

			// Is this node the root (first bone) of the skeleton ?

			// if ( RootCheck ) 
			//LocalQuat =  FQuat( LocalQuat.X,LocalQuat.Y,LocalQuat.Z, LocalQuat.W );

			ThisBone.BonePos.Orientation = LocalQuat; // FQuat( LocalQuat.X,LocalQuat.Y,LocalQuat.Z,LocalQuat.W );
			ThisBone.BonePos.Position = Trans;

			//ThisBone.BonePos.Orientation.Normalize();

			Thing->RefSkeletonNodes.AddItem( (void*) (int)SerialTree[i].node ); //index
			Thing->RefSkeletonBones.AddItem(ThisBone);			
		}
	}


	// Bone/node duplicate name check
	INT Duplicates = 0;
	{for(INT i=0; i< Thing->RefSkeletonBones.Num(); i++)
	{
		for(INT j=0; j< Thing->RefSkeletonBones.Num(); j++)
		{
			if( (i!=j) && (Thing->RefSkeletonBones[i].Name == Thing->RefSkeletonBones[j].Name) ) Duplicates++;
		}
	}}

	if (Duplicates)
	{ 
    	//WarningBox( "%i duplicate bone name(s) detected.", Duplicates );
	}

	// Logging...
	// PopupBox("Total reference skeleton bones: %i %i",Thing->RefSkeletonNodes.Num(),BoneIndex);

	DLog.Close();

	return 1;
}





void SceneIFC::SurveyScene()
{

	// Serialize scene tree 
	SerializeSceneTree();

}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
#if 0
// Dialogs. #TODO move to header file
void ShowAbout(HWND hWnd);
void ShowPanelOne(HWND hWnd);
void ShowPanelTwo(HWND hWnd);
void ShowActorManager(HWND hWnd);
void ShowBrushPanel(HWND hWnd);


BOOL CALLBACK SceneInfoDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ActorManagerDlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
BOOL CALLBACK BrushPanelDlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );


//HINSTANCE ParentApp_hInstance; // Parent instance 

extern struct HINSTANCE__ * MhInstPlugin;  // Link to parent app


#define DeclareCommandClass( _className )						\
class _className : public MPxCommand							\
{																\
public:															\
	_className() {};											\
	virtual MStatus	doIt ( const MArgList& );					\
	static void*	creator();									\
};																\
void* _className::creator()										\
{																\
	return new _className;										\
}																\

	
//
// Take care of command definitions. 
// Each one gets an explicitly definedActorXTool#::DoIt() command.
//


DeclareCommandClass( ActorXTool0 );
DeclareCommandClass( ActorXTool1 );
DeclareCommandClass( ActorXTool2 );
DeclareCommandClass( ActorXTool3 );
DeclareCommandClass( ActorXTool4 );
DeclareCommandClass( ActorXTool5 );
DeclareCommandClass( ActorXTool6 );


//////////////////

class AXTranslator : public MPxFileTranslator 
{
public:
	AXTranslator() { };
	virtual ~AXTranslator() { };
	static void    *creator ();

	MStatus         reader ( const MFileObject& file,
			                 const MString& optionsString,
			                 MPxFileTranslator::FileAccessMode mode);
	MStatus         writer ( const MFileObject& file,
		                     const MString& optionsString,
		                     MPxFileTranslator::FileAccessMode mode);
	bool            haveReadMethod () const;
	bool            haveWriteMethod () const;
	MString         defaultExtension () const;
	bool            canBeOpened () const;
	MFileKind       identifyFile ( const MFileObject& fileName,
				                   const char *buffer,
				                   short size) const;
private:
	static MString  magic;
};


MStatus initializePlugin( MObject _obj )						 
{
  

	MFnPlugin	plugin( _obj, "Epic Games, Inc.", "3.0", "Any"  ); 
	MStatus		stat;
	
	//MayaLog("Register filetranslator....");	

	//Register the translator with the system
	stat = plugin.registerFileTranslator ("psk",
							"AXTranslator.rgb", //image?
							AXTranslator::creator, 
							//"AXTranslatorOpts", 
							"ActorXToolOpts",   // Name must match .mel file == mel command for properties interface.
							"",
							true);

	if(stat != MS::kSuccess)
	{
		stat.perror (" registerFileTranslator error!");
		//return status;
	}

	//stat.perror("Register commands..");	

	// Register all commands
	stat = plugin.registerCommand( "axabout", ActorXTool0::creator ); 
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: ActorXAbout");

	stat = plugin.registerCommand( "axmain", ActorXTool1::creator ); 
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: ActorXMain");

	stat = plugin.registerCommand( "axoptions", ActorXTool2::creator );
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: ActorXOptions");

	stat = plugin.registerCommand( "axdigestanim", ActorXTool3::creator );
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: ActorXDigestAnim");

	stat = plugin.registerCommand( "axanim", ActorXTool4::creator );
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: ActorXAnimations");

	stat = plugin.registerCommand( "axskin", ActorXTool5::creator );
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: ActorXSkinExport");	

	stat = plugin.registerCommand( "axbrush", ActorXTool6::creator );
	if (stat != MS::kSuccess)												 
		stat.perror("registerCommand error: ActorXBrushExport");	

	// Reset plugin: mainly to initialize registry paths & settings..
	OurScene.DoLog = 1;
	ResetPlugin();

	// Todo - combine all the errors.
	stat = MS::kSuccess;
	return stat;												 
}												
	
	 
MStatus uninitializePlugin( MObject _obj )				 
{							
	
	MFnPlugin	plugin( _obj );									 
	MStatus		stat;					
	//stat.perror("Deregistering started:");	

	// deregister exporter...
	stat = plugin.deregisterFileTranslator("psk");
	if (stat != MS::kSuccess)
	{
		stat.perror ("deregisterFileTranslator");
		//return status;
	}	

	stat = plugin.deregisterCommand( "axabout" ); 	 			 
	if (stat != MS::kSuccess)												 
		stat.perror("deregisterCommand");		

	stat = plugin.deregisterCommand( "axmain" ); 	 			 
	if (stat != MS::kSuccess)												 
		stat.perror("deregisterCommand");		

	stat = plugin.deregisterCommand( "axoptions" );	
	if (stat != MS::kSuccess)												 
		stat.perror("deregisterCommand");		

	stat = plugin.deregisterCommand( "axdigestanim" );				 
	if (stat != MS::kSuccess)												 
		stat.perror("deregisterCommand");		

	stat = plugin.deregisterCommand( "axanim" );	
	if (stat != MS::kSuccess)												 
		stat.perror("deregisterCommand");		

	stat = plugin.deregisterCommand( "axskin" );	
	if (stat != MS::kSuccess)												 
		stat.perror("deregisterCommand");		

	stat = plugin.deregisterCommand( "axbrush" );
	if (stat != MS::kSuccess)									 
		stat.perror("deregisterCommand");
	
	// Todo - combine all the errors.
	stat = MS::kSuccess;
	return stat;												 

}



//
// Main MEL-type command implementations
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

#if 1 

// "About"
MStatus ActorXTool0::doIt( const MArgList& args )
{
	MStatus stat = MS::kSuccess;
	HWND hWnd = GetActiveWindow();

	ShowAbout(hWnd);
	
	setResult( "command 0 executed!\n" );
	return stat;
}

//"AXmain"
MStatus ActorXTool1::doIt( const MArgList& args )
{
	assert(0);

	ClearScenePointers();//#SKEL2

	MStatus stat = MS::kSuccess;
	HWND hWnd = GetActiveWindow();

	ShowPanelOne(hWnd);

	ClearScenePointers();
	
	setResult( " command 1 executed!\n" );
	return stat;
}


//"AXOptions"
MStatus ActorXTool2::doIt( const MArgList& args )
{
	MStatus stat = MS::kSuccess;
	HWND hWnd = GetActiveWindow();

	ShowPanelTwo(hWnd);

	setResult( "command 2 executed!\n" );
	return stat;
}

//"AXDigestAnim"
MStatus ActorXTool3::doIt( const MArgList& args )
{
	MStatus stat = MS::kSuccess;
	HWND hWnd = GetActiveWindow();


	setResult( "command 3 executed!\n" );
	return stat;
}

//"AXAnim"
MStatus ActorXTool4::doIt( const MArgList& args )
{
	MStatus stat = MS::kSuccess;
	HWND hWnd = GetActiveWindow();

	ShowActorManager(hWnd);

	setResult( "command 4 executed!\n" );
	return stat;
}

//"AXSkin"
MStatus ActorXTool5::doIt( const MArgList& args )
{
	MStatus stat = MS::kSuccess;
	HWND hWnd = GetActiveWindow();

	//#debug
	char LogFileName[] = ("ActXDagScanTree.LOG");
	DLog.Open(m_client->GetLogPath(),LogFileName,DEBUGFILE);

	scanDag ThisDag;
	stat = ThisDag.doScan(); // Print out DAG...

	DLog.Close();

	setResult( "command 5 executed!\n" );
	return stat;
}

//"AXbrush"
MStatus ActorXTool6::doIt( const MArgList& args )
{
	ClearScenePointers(); //#SKEL2

	MStatus stat = MS::kSuccess;
	HWND hWnd = GetActiveWindow();

	ShowBrushPanel(hWnd);

	ClearScenePointers(); 

	setResult( "command 6 executed!\n" );
	return stat;
}
#endif



///////////////////////////////////////////////////////////////

void* AXTranslator::creator()
{
	return new AXTranslator(); 
}

// Initialize our magic "number"
MString AXTranslator::magic("HDR");

// PSK file = skeletal skin file
// PSA file = skeletal animation file
//
// The reader is not implemented yet.
//
MStatus AXTranslator::reader (const MFileObject & file,
								const MString & options,
								MPxFileTranslator::FileAccessMode mode)
{
	MStatus rval (MS::kSuccess);

	return rval;
}


// Write method of the PSK translator / file exporter

MStatus AXTranslator::writer ( const MFileObject & fileObject,
								  const MString & options,
								  MPxFileTranslator::FileAccessMode mode)
{

	//char				LTmpStr[512];
	//unsigned int		i;
	//int				LN;

	const MString   fname = fileObject.fullName ();
	MString         extension;
	MString         baseFileName;

    int             TimeSlider = 0;
    int             AnimEnabled = 0;

	// See RTGExport


	// 
	return MS::kSuccess;
}


bool AXTranslator::haveReadMethod () const
{
	return false;
}

bool AXTranslator::haveWriteMethod () const
{
	return true;
}

// Whenever Maya needs to know the preferred extension of this file format,
// it calls this method.  For example, if the user tries to save a file called
// "test" using the Save As dialog, Maya will call this method and actually
// save it as "test.wrl2".  Note that the period should * not * be included in
// the extension.
//

MString AXTranslator::defaultExtension () const
{
	return "psk";
}

// This method tells Maya whether the translator can open and import files
// (returns true) or only import  files (returns false)

bool AXTranslator::canBeOpened () const
{
	return true;
}


MPxFileTranslator::MFileKind AXTranslator::identifyFile (
	      const MFileObject & fileName,
	      const char *buffer,
	      short size) const
{
	// Check the buffer for the "PSK" magic number, the string "HEADER_TITLE"

	MFileKind rval = kNotMyFileType;

	if (((int)size >= (int)magic.length ()) &&
	    (0 == strncmp (buffer, magic.asChar (), magic.length ())))
	{
		rval = kIsMyFileType;
	}
	return rval;
}

//
// Maya-specific SceneIFC implementations.
//


static BOOL CALLBACK AboutBoxDlgProc(HWND hWnd, UINT msg, 
	WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		//CenterWindow(hWnd, GetParent(hWnd)); 
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {

		case IDOK:
			EndDialog(hWnd, 1);
			break;

		case IDCANCEL:
			EndDialog(hWnd, 0);
		break;

		}
		break;
		default:
			return FALSE;
	}
	return TRUE;	
}       


void ShowAbout(HWND hWnd)
{
	DialogBoxParam( MhInstPlugin, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, AboutBoxDlgProc, 0 );
}

void DoDigestAnimation()
{
	// Scene stats
	//DebugBox("Start serializing scene tree");
	OurScene.SurveyScene();

	if( OurScene.SerialTree.Num() > 1 ) // Anything detected ?
	{
		//u->ip->ProgressStart((" Digesting Animation... "), true, fn, NULL );

		//DebugBox("Start getting scene info; Scene tree size: %i",OurScene.SerialTree.Num() );
		OurScene.GetSceneInfo();

		//DebugBox("Start evaluating Skeleton");
		OurScene.EvaluateSkeleton(1);

			//DebugBox("Start digest Skeleton");
		OurScene.DigestSkeleton(&TempActor);

			//DebugBox("Start digest Anim");
		OurScene.DigestAnim(&TempActor, animname, framerange );  

		OurScene.FixRootMotion( &TempActor );

		// //PopupBox("Digested animation:%s Betakeys: %i Current animation # %i", animname, TempActor.BetaKeys.Num(), TempActor.Animations.Num() );
		if(OurScene.DoForceRate )
			TempActor.FrameRate = OurScene.PersistentRate;

		INT DigestedKeys = TempActor.RecordAnimation();			

		////PopupBox( " Animation sequence %s digested. Frames: %i  total anims: %i ",animname, TempActor.WorkAnim.AnimInfo.NumRawFrames, TempActor.Animations.Num() );
	}
}


// Maya panel 1 (like Max Utility Panel)

static BOOL CALLBACK PanelOneDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) 
	{
		case WM_INITDIALOG:
			// Store the object pointer into window's user data area.
			// u = (MaxPluginClass *)lParam;
			// SetWindowLong(hWnd, GWL_USERDATA, (LONG)u);
			// SetInterface(u->ip);
			// Initialize this panel's custom controls if any.
			// ThisMaxPlugin.InitSpinner(hWnd,IDC_EDIT_ATIME,IDC_SPIN_ATIME,0,0,1000);

			// Initialize all non-empty edit windows.

			if ( to_path[0]     ) SetWindowText(GetDlgItem(hWnd,IDC_PATH),to_path);
			_tcscpy(m_client->GetLogPath(),to_path); 

			if ( to_skinfile[0] ) SetWindowText(GetDlgItem(hWnd,IDC_SKINFILENAME),to_skinfile);
			if ( to_animfile[0] ) PrintWindowString(hWnd,IDC_ANIMFILENAME,to_animfile);
			if ( animname[0]    ) PrintWindowString(hWnd,IDC_ANIMNAME,animname);			
			if ( framerange[0]  ) PrintWindowString(hWnd,IDC_ANIMRANGE,framerange);
			if ( basename[0]    ) PrintWindowString(hWnd,IDC_EDITBASE,basename);
			if ( classname[0]   ) PrintWindowString(hWnd,IDC_EDITCLASS,classname);		
			break;
	
		case WM_COMMAND:
			switch (LOWORD(wParam)) 
			{
				case IDOK:
					EndDialog(hWnd, 1);
					break;

				case IDCANCEL:
					EndDialog(hWnd, 0);
				break;
			}

			/*
			// HIWORD(wParam) : focus switches
			switch(HIWORD(wParam))
			{
				case EN_SETFOCUS :
					DisableAccelerators();
					break;
				case EN_KILLFOCUS :
					EnableAccelerators();
					break;
			}
			*/

			// LOWORD(wParam) has commands for the controls in our rollup window.
			switch (LOWORD(wParam)) 
			{
				case IDC_ANIMNAME:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_ANIMNAME),animname,100);
						}
						break;
					};
				}
				break;		

			case IDC_ANIMRANGE:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_ANIMRANGE),framerange,500);
						}
						break;
					};
				}
				break;				

			case IDC_PATH:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_PATH),to_path,300);										
							_tcscpy(m_client->GetLogPath(),to_path);
							PluginReg.SetKeyString("TOPATH", to_path );
						}
						break;
					};
				}
				break;

			case IDC_SKINFILENAME:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_SKINFILENAME),to_skinfile,300);
							PluginReg.SetKeyString("TOSKINFILE", to_skinfile );
						}
						break;
					};
				}
				break;

			case IDC_ANIMFILENAME:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_ANIMFILENAME),to_animfile,300);
							PluginReg.SetKeyString("TOANIMFILE", to_animfile );
						}
						break;
					};
				}
				break;
				
			case IDC_ANIMMANAGER: // Animation manager activated.
				{
					HWND hWnd = GetActiveWindow();
					ShowActorManager(hWnd);
				}
				break;
	
			// Browse for a destination directory.
			case IDC_BROWSE:
				{					
					char  dir[MAX_PATH];
					if( GetFolderName(hWnd, dir) )
					{
						_tcscpy(to_path,dir); 
						_tcscpy(m_client->GetLogPath(),dir);					
						PluginReg.SetKeyString("TOPATH", to_path );
					}
					SetWindowText(GetDlgItem(hWnd,IDC_PATH),to_path);															
				}	
				break;
			

			// Quickly save all digested anims to the animation file, overwriting whatever's there...
			case IDC_QUICKSAVE:
				{						
					if( TempActor.Animations.Num() <= 0 )
					{
						//PopupBox("Error: no digested animations present to quicksave.");
						break;
					}

					INT ExistingSequences = 0;
					INT FreshSequences = TempActor.Animations.Num();

					// If 'QUICKDISK' is on, load to output package, move/overwrite, and save all at once.
					char to_ext[32];
					_tcscpy(to_ext, ("PSA"));
					sprintf(DestPath,"%s\\%s.%s",(char*)to_path,(char*)to_animfile,to_ext);					
 
					// Optionally load them from the existing on disk..
					if( OurScene.QuickSaveDisk )
					{
						// Clear outanims...
						{for( INT c=0; c<TempActor.OutAnims.Num(); c++)
						{
							TempActor.OutAnims[c].KeyTrack.Empty();
						}}
						TempActor.OutAnims.Empty();

						// Quick load; silent if file doesn't exist yet.
						{							
							FastFileClass InFile;
							if ( InFile.OpenExistingFileForReading(DestPath) != 0) // Error!
							{
								////ErrorBox("File [%s] does not exist yet.", DestPath);
							}
							else // Load all relevant chunks into TempActor and its arrays.
							{
								INT DebugBytes = TempActor.LoadAnimation(InFile);								
								// Log input 
								// //PopupBox("Total animation sequences loaded:  %i", TempActor.OutAnims.Num());
							}
							InFile.Close();
						}				
						
						ExistingSequences = TempActor.OutAnims.Num();
					}

					int NameUnique = 1;
					int ReplaceIndex = -1;
					int ConsistentBones = 1;

					// Go over animations in 'inbox' and copy or add to outbox.
					for( INT c=0; c<TempActor.Animations.Num(); c++)
					{
					    INT AnimIdx = c;

						// Name uniqueness check as well as bonenum-check for every sequence; _overwrite_ if a name already exists...
						for( INT t=0;t<TempActor.OutAnims.Num(); t++)
						{
							if( strcmp( TempActor.OutAnims[t].AnimInfo.Name, TempActor.Animations[AnimIdx].AnimInfo.Name ) == 0)
							{
								NameUnique = 0;
								ReplaceIndex = t;
							}

							if( TempActor.OutAnims[t].AnimInfo.TotalBones != TempActor.Animations[AnimIdx].AnimInfo.TotalBones)
								ConsistentBones = 0;
						}

						// Add or replace.
						if( ConsistentBones )
						{
							INT NewIdx = 0;
							if( NameUnique ) // Add -
							{
								NewIdx = TempActor.OutAnims.Num();
								TempActor.OutAnims.AddZeroed(1); // Add a single element.
							}
							else // Replace.. delete existing.
							{
								NewIdx = ReplaceIndex;
								TempActor.OutAnims[NewIdx].KeyTrack.Empty();
							}

							TempActor.OutAnims[NewIdx].AnimInfo = TempActor.Animations[AnimIdx].AnimInfo;
							////PopupBox("Copying Keytracks now, total %i...",TempActor.Animations[InSelect].KeyTrack.Num()); //#debug
							TempActor.OutAnims[NewIdx].KeyTrack.Append( TempActor.Animations[AnimIdx].KeyTrack );
							//for( INT j=0; j<TempActor.Animations[AnimIdx].KeyTrack.Num(); j++ )
							//{								
							//	TempActor.OutAnims[NewIdx].KeyTrack.AddItem( TempActor.Animations[AnimIdx].KeyTrack[j] );
							//}																													
						}
						else 
						{							
							//PopupBox("ERROR !! Aborting the quicksave/move, inconsistent bone counts detected.");
							break;
						}						
					}

					// Delete 'left-box' items.
					if( ConsistentBones )
					{
						// Clear 'left pane' anims..
						{for( INT c=0; c<TempActor.Animations.Num(); c++)
						{
							TempActor.Animations[c].KeyTrack.Empty();
						}}
						TempActor.Animations.Empty();
					}
										
										
					// Optionally SAVE them _over_ the existing on disk (we added/overwrote stuff)
					if( OurScene.QuickSaveDisk )
					{
						if ( TempActor.OutAnims.Num() == 0)
						{
							//PopupBox(" No digested animations available. ");
						}
						else
						{											
							FastFileClass OutFile;
							if ( OutFile.CreateNewFile(DestPath) != 0) // Error!
							{
								//ErrorBox("File creation error.");
							}
							else
							{
								// //PopupBox(" Bones in first track: %i",TempActor.RefSkeletonBones.Num());
								TempActor.SerializeAnimation(OutFile);
								OutFile.CloseFlush();
								if( OutFile.GetError()) //ErrorBox("Animation Save Error.");
							}

							INT WrittenBones = TempActor.RefSkeletonBones.Num();

							if( OurScene.DoLog )
							{
								OurScene.LogAnimInfo(&TempActor, to_animfile ); 												
							}

							//PopupBox( "OK: Animation sequence appended to [%s.%s] total: %i sequences.", to_animfile, to_ext, TempActor.OutAnims.Num() );
						}			
					}
				}
			break;
				
			// A click inside the logo.
			case IDC_LOGOUNREAL: 
				{
					// Show our About box.
				}
			break;
			

			// DO it.
			case IDC_EVAL:
				{
					// Survey the scene, to conclude which items to export or evaluate, if any.
					// Logic: go for the deepest hierarchy; if it has a physique skin, we have
					// a full skeleton+skin, otherwise we assume a hierarchical object - which
					// can have max bones, though.					
#if 0
					////DebugBox("Start surveying the scene.");
					OurScene.SurveyScene();

					// Initializes the scene tree.						
						//DebugBox("Start getting scene info");
					OurScene.GetSceneInfo();

						//DebugBox("Start evaluating Skeleton");
					OurScene.EvaluateSkeleton(1);					
					/////////////////////////////////////
					//u->ShowSceneInfo(hWnd);
#endif
				}
				break;

			
			case IDC_SAVESKIN:
				{
					// Scene stats. #debug: surveying should be done only once ?

					OurScene.SurveyScene();
					
					OurScene.GetSceneInfo();

					OurScene.EvaluateSkeleton(1);


					////PopupBox("Start digest Skeleton");
					OurScene.DigestSkeleton( &TempActor );  // Digest skeleton into tempactor

					// Skin
					OurScene.DigestSkin( &TempActor );

					//DLog.Logf(" Skin vertices: %6i\n",Thing->SkinData.Points.Num());
					if( TempActor.SkinData.Points.Num() == 0)
					{
						//WarningBox(" Warning: No valid skin triangles digested (mesh may lack proper mapping or valid linkups)");
					}
					else
					{

						////WarningBox(" Warning: Skin triangles found: %i ",TempActor.SkinData.Points.Num() ); 

						// TCHAR MessagePopup[512];
						TCHAR to_ext[32];
						_tcscpy(to_ext, ("PSK"));
						sprintf(DestPath,"%s\\%s.%s",(char*)to_path,(char*)to_skinfile,to_ext);
						FastFileClass OutFile;

						if ( OutFile.CreateNewFile(DestPath) != 0) // Error!
							//ErrorBox( "File creation error. ");
				
						TempActor.SerializeActor(OutFile); //save skin
					
						 // Close.
						OutFile.CloseFlush();
						if( OutFile.GetError()) //ErrorBox("Skin Save Error");

						//DebugBox("Logging skin info:");
						// Log materials and skin stats.
						if( OurScene.DoLog )
						{
							OurScene.LogSkinInfo( &TempActor, to_skinfile );
							////DebugBox("Logged skin info:");
						}
																								
						//PopupBox(" Skin file %s.%s written.",to_skinfile,to_ext );
					}

				}
				break;

			case IDC_DIGESTANIM:
				{									
					DoDigestAnimation();					
				}
				break;			
			}
			break;
			// END command

		default:
			return FALSE;
	}
	return TRUE;
}       


static BOOL CALLBACK PanelTwoDlgProc(HWND hWnd, UINT msg, 
	WPARAM wParam, LPARAM lParam)
{
	
	switch (msg) 
	{
		case WM_INITDIALOG:
			// Store the object pointer into window's user data area.
			//u = (MaxPluginClass *)lParam;
			//SetWindowLong(hWnd, GWL_USERDATA, (LONG)u);

			// Optional persistence: retrieve setting from registry.						
			INT SwitchPersistent;
			PluginReg.GetKeyValue("PERSISTSETTINGS", SwitchPersistent);
			_SetCheckBox(hWnd,IDC_CHECKPERSISTSETTINGS,SwitchPersistent?1:0);

			INT SwitchPersistPaths;
			PluginReg.GetKeyValue("PERSISTPATHS", SwitchPersistPaths);
			_SetCheckBox(hWnd,IDC_CHECKPERSISTPATHS,SwitchPersistPaths?1:0);

			//if ( OurScene.DoForceRate ) 
			{
				char RateString[MAX_PATH];				
				sprintf(RateString,"%5.3f",OurScene.PersistentRate);
				PrintWindowString(hWnd,IDC_PERSISTRATE,RateString);		
			}
					
			// Make sure defaults are updated when boxes first appear.
			_SetCheckBox( hWnd, IDC_QUICKSAVEDISK, OurScene.QuickSaveDisk );
			_SetCheckBox( hWnd, IDC_LOCKROOTX, OurScene.DoFixRoot);
			_SetCheckBox( hWnd, IDC_POSEZERO, OurScene.DoPoseZero);

			_SetCheckBox( hWnd, IDC_CHECKNOLOG, !OurScene.DoLog);

			_SetCheckBox( hWnd, IDC_CHECKSKINALLT, OurScene.DoTexGeom );
			_SetCheckBox( hWnd, IDC_CHECKSKINALLP, OurScene.DoPhysGeom );
			_SetCheckBox( hWnd, IDC_CHECKSKINALLS, OurScene.DoSelectedGeom );
			_SetCheckBox( hWnd, IDC_CHECKSKINNOS,  OurScene.DoSkipSelectedGeom );
			_SetCheckBox( hWnd, IDC_CHECKTEXPCX,   OurScene.DoTexPCX );
			_SetCheckBox( hWnd, IDC_CHECKQUADTEX,  OurScene.DoQuadTex );
			_SetCheckBox( hWnd, IDC_CHECKCULLDUMMIES, OurScene.DoCullDummies );
			_SetCheckBox( hWnd, IDC_CHECKFORCERATE, OurScene.DoForceRate );

			//if ( batchfoldername[0] ) SetWindowText(GetDlgItem(hWnd,IDC_BATCHPATH),batchfoldername);
		
			//ToggleControlEnable(hWnd,IDC_CHECKSKIN1,true);
			//ToggleControlEnable(hWnd,IDC_CHECKSKIN2,true);

			// Start Lockroot disabled
			_EnableCheckBox( hWnd, IDC_LOCKROOTX, 0 );			

			// Initialize this panel's custom controls, if any.
			// ThisMaxPlugin.InitSpinner(hWnd,IDC_EDIT_ACOMP,IDC_SPIN_ACOMP,100,0,100);
			break;

		case WM_DESTROY:
			// Release this panel's custom controls.
			// ThisMaxPlugin.DestroySpinner(hWnd,IDC_SPIN_ACOMP);
			break;

		//case WM_LBUTTONDOWN:
		//case WM_LBUTTONUP:
		//case WM_MOUSEMOVE:
		//	ThisMaxPlugin.ip->RollupMouseMessage(hWnd,msg,wParam,lParam); 
		//	break;

		case WM_COMMAND:
			// Needed to have keyboard focus on this window!
			// HIWORD(wParam) : focus switches
			switch(HIWORD(wParam))
			{
				case EN_SETFOCUS :
					//DisableAccelerators();
					break;
				case EN_KILLFOCUS :
					//EnableAccelerators();
					break;
			};

			// LOWORD(wParam) has commands for the controls in our rollup window.
			switch (LOWORD(wParam)) 
			{
				// Window-closing

				case IDOK:
					EndDialog(hWnd, 1);
				break;

				case IDCANCEL:
					EndDialog(hWnd, 0);
				break;

				// get the values of the toggles
				case IDC_CHECKPERSISTSETTINGS:
				{
					//Read out the value...
					OurScene.CheckPersistSettings = _GetCheckBox(hWnd,IDC_CHECKPERSISTSETTINGS);
					PluginReg.SetKeyValue("PERSISTSETTINGS", OurScene.CheckPersistSettings);
				}
				break;

				case IDC_CHECKPERSISTPATHS:
				{
					//Read out the value...
					OurScene.CheckPersistPaths = _GetCheckBox(hWnd,IDC_CHECKPERSISTPATHS);
					PluginReg.SetKeyValue("PERSISTPATHS", OurScene.CheckPersistPaths);
				}
				break;

				// Switch to indicate QuickSave is to save/append/overwrite sequences immediately to disk (off by default)...			
				case IDC_QUICKSAVEDISK:
				{
					OurScene.QuickSaveDisk = _GetCheckBox( hWnd, IDC_QUICKSAVEDISK );
					PluginReg.SetKeyValue("QUICKSAVEDISK",OurScene.QuickSaveDisk );
				}
				break;

				case IDC_FIXROOT:
				{
					OurScene.DoFixRoot = _GetCheckBox(hWnd,IDC_FIXROOT);
					PluginReg.SetKeyValue("DOFIXROOT",OurScene.DoFixRoot );

					//ToggleControlEnable(hWnd,IDC_LOCKROOTX,OurScene.DoFixRoot);
					if (!OurScene.DoFixRoot)
					{
						_SetCheckBox( hWnd,IDC_LOCKROOTX,false );
						_EnableCheckBox( hWnd,IDC_LOCKROOTX,0 );			
					}
					else 
					{
						_EnableCheckBox( hWnd,IDC_LOCKROOTX,1 );
					}

					// HWND hDlg = GetDlgItem(hWnd,control);
					// EnableWindow(hDlg,FALSE);					
					// see also MSDN  SDK info, Buttons/ using buttons etc
				}
				break;


				case IDC_POSEZERO:
				{
					OurScene.DoPoseZero = _GetCheckBox(hWnd,IDC_POSEZERO);
					//ToggleControlEnable(hWnd,IDC_LOCKROOTX,OurScene.DoPOSEZERO);
					if (!OurScene.DoPoseZero)
					{
						_SetCheckBox(hWnd,IDC_LOCKROOTX,false);
						_EnableCheckBox( hWnd,IDC_LOCKROOTX, 0);			
					}
					else 
						_EnableCheckBox( hWnd,IDC_LOCKROOTX, 1 );			
					// HWND hDlg = GetDlgItem(hWnd,control);
					// EnableWindow(hDlg,FALSE);					
					// see also MSDN  SDK info, Buttons/ using buttons etc
				}
				break;

				case IDC_LOCKROOTX:
				{
					//Read out the value...
					OurScene.DoLockRoot =_GetCheckBox(hWnd,IDC_LOCKROOTX);
				}
				break;

				case IDC_CHECKNOLOG:
				{
					OurScene.DoLog = !_GetCheckBox(hWnd,IDC_CHECKNOLOG);	
					PluginReg.SetKeyValue("DOLOG",OurScene.DoLog);
				}
				break;

				case IDC_CHECKFORCERATE:
				{
					OurScene.DoForceRate =_GetCheckBox(hWnd,IDC_CHECKFORCERATE);
					PluginReg.SetKeyValue("DOFORCERATE",OurScene.DoForceRate );
					if (!OurScene.DoForceRate) _SetCheckBox(hWnd,IDC_CHECKFORCERATE,false);
				}
				break;

				case IDC_PERSISTRATE:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							char RateString[MAX_PATH];
							GetWindowText( GetDlgItem(hWnd,IDC_PERSISTRATE),RateString,100 );
							OurScene.PersistentRate = atof(RateString);
							PluginReg.SetKeyValue("PERSISTENTRATE", OurScene.PersistentRate );
						}
						break;
					};
				}
				break;
				
				case IDC_CHECKSKINALLT: // all textured geometry skin checkbox
				{
					OurScene.DoTexGeom =_GetCheckBox(hWnd,IDC_CHECKSKINALLT);
					PluginReg.SetKeyValue("DOTEXGEOM",OurScene.DoTexGeom );
					if (!OurScene.DoTexGeom) _SetCheckBox(hWnd,IDC_CHECKSKINALLT,false);
				}
				break;

				case IDC_CHECKTEXPCX: // all textured geometry skin checkbox
				{
					OurScene.DoTexPCX =_GetCheckBox(hWnd,IDC_CHECKTEXPCX);
					PluginReg.SetKeyValue("DOPCX",OurScene.DoTexPCX); // update registry
					//if (!OurScene.DoTexPCX) _SetCheckBox(hWnd,IDC_CHECKTEXPCX,false);
				}
				break;

				case IDC_CHECKQUADTEX: // all textured geometry skin checkbox
				{
					OurScene.DoQuadTex =_GetCheckBox(hWnd,IDC_CHECKQUADTEX);
				}
				break;

				case IDC_VERTEXOUT: // Classic vertex animation export
				{
					OurScene.DoVertexOut =_GetCheckBox(hWnd,IDC_VERTEXOUT);
					//if (!OurScene.DoVertexOut) _SetCheckBox(hWnd,IDC_CHECKTEXPCX,false);
				}

				case IDC_CHECKSKINALLS: // all selected meshes skin checkbox
				{
					OurScene.DoSelectedGeom =_GetCheckBox(hWnd,IDC_CHECKSKINALLS);
					PluginReg.SetKeyValue("DOSELGEOM",OurScene.DoSelectedGeom);					
					if (!OurScene.DoSelectedGeom) _SetCheckBox(hWnd,IDC_CHECKSKINALLS,false);
				}
				break;

				case IDC_CHECKSKINALLP: // all selected physique meshes checkbox
				{
					OurScene.DoPhysGeom =_GetCheckBox(hWnd,IDC_CHECKSKINALLP);
					PluginReg.SetKeyValue("DOPHYSGEOM", OurScene.DoPhysGeom );
					if (!OurScene.DoPhysGeom) _SetCheckBox(hWnd,IDC_CHECKSKINALLP,false);
				}
				break;

				/*
				case IDC_CHECKSKINNOS: // all selected meshes skin checkbox
				{
					OurScene.DoSkipSelectedGeom =_GetCheckBox(hWnd,IDC_CHECKSKINNOS);
					if (!OurScene.DoSkipSelectedGeom) _SetCheckBox(hWnd,IDC_CHECKSKINNOS,false);
				}
				break;
				*/

				case IDC_CHECKCULLDUMMIES: // all selected physique meshes checkbox
				{
					OurScene.DoCullDummies =_GetCheckBox(hWnd,IDC_CHECKCULLDUMMIES);
					PluginReg.SetKeyValue("DOCULLDUMMIES",OurScene.DoCullDummies );
					//ToggleControlEnable(hWnd,IDC_CHECKCULLDUMMIES,OurScene.DoCullDummies);
					if (!OurScene.DoCullDummies) _SetCheckBox(hWnd,IDC_CHECKCULLDUMMIES,false);					
				}
				break;

				case IDC_EXPLICIT:
				{
					OurScene.DoExplicitSequences =_GetCheckBox(hWnd,IDC_EXPLICIT);
					PluginReg.SetKeyValue("DOEXPSEQ",OurScene.DoExplicitSequences);
				}
				break;

				case IDC_SAVESCRIPT: // save the script
				{
					if( 0 ) // TempActor.OutAnims.Num() == 0 ) 
					{
						//PopupBox("Warning: no animations present in output package.\n Aborting template generation.");
					}
					else
					{
						OurScene.WriteScriptFile( &TempActor, classname, basename, to_skinfile, to_animfile );
						//PopupBox(" Script template file written: %s",classname);
					}
				}
				break;

				case IDC_BATCHPATH:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText( GetDlgItem(hWnd,IDC_EDITCLASS),batchfoldername,300 );
						}
						break;
					};
				}
				break;		

				case IDC_BATCHSRC: // Browse for batch source folder.
				{
					// TCHAR dir[MAX_PATH];
					// TCHAR desc[MAX_PATH];
					//_tcscpy(desc, ("Batch Source Folder"));
					// u->ip->ChooseDirectory(u->ip->GetMAXHWnd(),("Choose Folder"), dir, desc);					
					//_tcscpy(batchfoldername,dir); 
					// SetWindowText(GetDlgItem(hWnd,IDC_BATCHPATH),batchfoldername);					
				}
				break;

				case IDC_BATCHMAX: // process the batch files.
				{	
					// Get all max files from folder '' and digest them.
					//PopupBox(" Digested %i animation files, total of %i frames",0,0);
				}
				break;

				case IDC_EDITCLASS:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText( GetDlgItem(hWnd,IDC_EDITCLASS),classname,100 );
							PluginReg.SetKeyString("CLASSNAME", classname );
						}
						break;
					};
				}
				break;		

				case IDC_EDITBASE:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							GetWindowText(GetDlgItem(hWnd,IDC_EDITBASE),basename,100);
							PluginReg.SetKeyString("BASENAME", basename );
						}
						break;
					};
				}
				break;		
			

			};
			break;

		default:
		return FALSE;
	}
	return TRUE;
}       


// #TODO: move to Dialogs file if they can be made parent-independent...

void ShowPanelOne( HWND hWnd)
{
	DialogBoxParam( MhInstPlugin, MAKEINTRESOURCE(IDD_PANEL), hWnd, PanelOneDlgProc, 0 );
}

void ShowPanelTwo( HWND hWnd)
{
	DialogBoxParam( MhInstPlugin, MAKEINTRESOURCE(IDD_AUXPANEL), hWnd, PanelTwoDlgProc, 0 );
}

void ShowActorManager( HWND hWnd )
{
	DialogBoxParam(MhInstPlugin, MAKEINTRESOURCE(IDD_ACTORMANAGER), hWnd, ActorManagerDlgProc, 0);	
}

void ShowBrushPanel( HWND hWnd )
{
	DialogBoxParam(MhInstPlugin, MAKEINTRESOURCE(IDD_BRUSHPANEL), hWnd, BrushPanelDlgProc, 0);	
}

#endif // 0
//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------

//
//################################################
//
// Maya DAG scanner from the Maya SDK documentation example.
//
// DAG walking example 
// The following example is the scanDagSyntaxCmd example. It demonstrates iterating through the DAG in either a depth first, or a breadth first manner. This
// code makes a good basis for many DAG walking plug-ins, in particular those written as file translators. 
//
// See scanDagCmd.cpp in devkit/plug-ins for the complete example. 



MStatus	scanDag::doScan(  )
{
	//MItDag::TraversalType	traversalType = MItDag::kDepthFirst;
	MItDag::TraversalType	traversalType = MItDag::kBreadthFirst;
	
	MFn::Type				filter        = MFn::kInvalid;
	MStatus					status;
	bool					quiet = false;

	return doScan( traversalType, filter, quiet);
};


MStatus scanDag::doScan( const MItDag::TraversalType traversalType, MFn::Type filter, bool quiet)
{   
	char LogFileName[] = ("ActXDAGDebug.LOG");
	DLog.Open(m_client->GetLogPath(),LogFileName,DEBUGFILE);

	MStatus status;
	// Class MItDag provides the capability to traverse the DAG and to retrieve specific nodes for subsequent querying and editing using compatible Function Sets.
	MItDag dagIterator( traversalType, filter, &status);

	if ( !status) 
	{
		status.perror("MItDag constructor");
		return status;
	}
	
	////PopupBox("ScanDag start");

	//	Scan the entire DAG and output the name and depth of each node

	if (traversalType == MItDag::kBreadthFirst)
	{
	//	cout << endl << "Starting Breadth First scan of the Dag";
	}
	else
	if (!quiet)
	{
	//	cout << endl << "Starting Depth First scan of the Dag";
	}

	/*
	switch (filter) 
	{
		case MFn::kCamera:
			if (!quiet)
			{
				//cout << ": Filtering for Cameras\n";
			}
			break;
		case MFn::kLight:
			if (!quiet)
			{
				//cout << ": Filtering for Lights\n";
			}
			break;
		case MFn::kNurbsSurface:
			if (!quiet)
			{
				//cout << ": Filtering for Nurbs Surfaces\n";
			}
			break;
		default:
			{}
	}
	*/
	
	int objectCount = 0;
	for ( ; !dagIterator.isDone(); dagIterator.next() ) {

		MDagPath dagPath;

		status = dagIterator.getPath(dagPath);
		if ( !status ) {
			status.perror("MItDag::getPath");
			continue;
		}

		// The dagnode.
		MFnDagNode dagNode(dagPath, &status);
		if ( !status ) {
			status.perror("MFnDagNode constructor");
			continue;
		}

		DLog.Logf("\n");


		MObject	Object = dagNode.object();
		if( Object != MObject::kNullObj  )
		{
			DLog.Logf("Object name:[%s] \n ",Object.apiTypeStr() ); //, Node.name(&stat).asChar()
		}

		DLog.Logf(" node # %3i \n",objectCount);
		DLog.Logf(" dagName: [%s] type: [%s] \n", dagNode.name().asChar(), dagNode.typeName().asChar());

		DLog.Logf(" dagPath: [%s] \n",dagPath.fullPathName().asChar());


		
		int ParentCount = dagNode.parentCount();
		if ( ParentCount )
		{
			DLog.Logf("Parents: %i \n",ParentCount);
			for( int i=0;i<ParentCount;i++)
			{
				DLog.Logf("Parent #%i typename: %s \n",i, dagNode.parent(i).apiTypeStr());
			}
		}

		int ChildCount = dagNode.childCount();
		if ( ChildCount )
		{
			DLog.Logf("Children: %i \n",ChildCount);
			for( int i=0;i<ChildCount;i++)
			{
				
				MFnDagNode childNode(dagNode.child(i), &status);

				//DLog.Logf("Child #%i typename: %s \n",i, dagNode.child(i).apiTypeStr());
				DLog.Logf("Child #%i  name: [%s]  type: %s \n",i,childNode.name().asChar(),dagNode.child(i).apiTypeStr());
			}
		}



		objectCount += 1;

		if (dagPath.hasFn(MFn::kCamera)) 
		{
			MFnCamera camera (dagPath, &status);
			if ( !status ) 
			{
				status.perror("MFnCamera constructor");
				continue;
			}

			// Get the translation/rotation/scale data
			getTransformData(dagPath , false );

			// Extract some interesting Camera data
			/*
			if (!quiet)
			{	
				cout << "  eyePoint: "
					 << camera.eyePoint(MSpace::kWorld) << endl;
				cout << "  upDirection: "
					 << camera.upDirection(MSpace::kWorld) << endl;
				cout << "  viewDirection: "
					 << camera.viewDirection(MSpace::kWorld) << endl;
				cout << "  aspectRatio: " << camera.aspectRatio() << endl;
				cout << "  horizontalFilmAperture: "
					 << camera.horizontalFilmAperture() << endl;
				cout << "  verticalFilmAperture: "
					 << camera.verticalFilmAperture() << endl;
			}
			*/
		} 
		else if (dagPath.hasFn(MFn::kLight)) 
		{
			MFnLight light (dagPath, &status);
			if ( !status ) {
				status.perror("MFnLight constructor");
				continue;
			}

			// Get the translation/rotation/scale data
			getTransformData(dagPath, quiet);

			/*
			// Extract some interesting Light data
			MColor color;

			color = light.color();
			if (!quiet)
			{
				cout << "  color: ["
					 << color.r << ", "
					 << color.g << ", "
					 << color.b << "]\n";
			}
			color = light.shadowColor();
			if (!quiet)
			{
				cout << "  shadowColor: ["
					 << color.r << ", "
					 << color.g << ", "
					 << color.b << "]\n";

				cout << "  intensity: " << light.intensity() << endl;
			}
			*/
		} 
		else if (dagPath.hasFn(MFn::kNurbsSurface)) 
		{
			MFnNurbsSurface surface (dagPath, &status);
			if ( !status ) {
				status.perror("MFnNurbsSurface constructor");
				continue;
			}

			// Get the translation/rotation/scale data
			getTransformData(dagPath, quiet);

			// Extract some interesting Surface data
			/*
			if (!quiet)
			{
				cout << "  numCVs: "
					 << surface.numCVsInU()
					 << " * "
					 << surface.numCVsInV()
					 << endl;
				cout << "  numKnots: "
					 << surface.numKnotsInU()
					 << " * "
					 << surface.numKnotsInV()
					 << endl;
				cout << "  numSpans: "
					 << surface.numSpansInU()
					 << " * "
					 << surface.numSpansInV()
					 << endl;
			}
			*/
		} 
		else 
		{
			// Get the translation/rotation/scale data
			getTransformData( dagPath, false );
		}
	}
	
	DLog.Close();

	return MS::kSuccess;
}


void scanDag::getTransformData(const MDagPath& dagPath, bool quiet )
{

	// This method simply determines the transformation information on the DAG node and prints it out. 

    MStatus     status;
    MObject     transformNode = dagPath.transform(&status);

    // This node has no transform - i.e., it's the world node
    if (!status && status.statusCode () == MStatus::kInvalidParameter)
        return;

    MFnDagNode  transform (transformNode, &status);

    if (!status) 
	 {
        status.perror("MFnDagNode constructor");
        return;
    }

    MTransformationMatrix   matrix (transform.transformationMatrix());
	 //     "  translation: " << matrix.translation(MSpace::kWorld)
             
    double                                  threeDoubles[3];
    MTransformationMatrix::RotationOrder    rOrder;

    matrix.getRotation (threeDoubles, rOrder, MSpace::kWorld);
	 DLog.Logf("  rotation: [ %f %f %f ] \n", threeDoubles[0],threeDoubles[1],threeDoubles[2]);
 
    matrix.getScale (threeDoubles, MSpace::kWorld);
	 DLog.Logf("     scale: [ %f %f %f ] \n", threeDoubles[0],threeDoubles[1],threeDoubles[2]);

     
}
 
 

 


//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
#if 0 
//
// Brush export (universal) dialog procedure - can write brushes to separarte destination folder.
//
static INT MatIDC[16];


void UpdateSlotNames(HWND hWnd)
{
	for(INT m=0; m<8; m++)
	{
		if ( materialnames[m][0] ) PrintWindowString( hWnd, MatIDC[m], materialnames[m] );
	}   
}

BOOL CALLBACK BrushPanelDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) 
	{
		case WM_INITDIALOG:

			MatIDC[0] = IDC_EDITTEX0;
			MatIDC[1] = IDC_EDITTEX1;
			MatIDC[2] = IDC_EDITTEX2;
			MatIDC[3] = IDC_EDITTEX3;
			MatIDC[4] = IDC_EDITTEX4;
			MatIDC[5] = IDC_EDITTEX5;
			MatIDC[6] = IDC_EDITTEX6;
			MatIDC[7] = IDC_EDITTEX7;
			//
			// Store the object pointer into window's user data area.
			// u = (MaxPluginClass *)lParam;
			// SetWindowLong(hWnd, GWL_USERDATA, (LONG)u);
			// SetInterface(u->ip);
			// Initialize this panel's custom controls if any.
			// ThisMaxPlugin.InitSpinner(hWnd,IDC_EDIT_ATIME,IDC_SPIN_ATIME,0,0,1000);
			//

			m_client->GetLogPath()[0] = 0; // disable logging.

			_SetCheckBox( hWnd, IDC_CHECKSMOOTH, OurScene.DoSmooth );
			_SetCheckBox( hWnd, IDC_CHECKSINGLE, OurScene.DoSingleTex );

			// Initialize all non-empty edit windows.
			if ( to_brushpath[0] ) SetWindowText(GetDlgItem(hWnd,IDC_BRUSHPATH),to_brushpath);
			if ( to_brushfile[0] ) SetWindowText(GetDlgItem(hWnd,IDC_BRUSHFILENAME),to_brushfile);			

			// See if there are materials from the mesh to write back to the window..
			{for(INT m=0; m < TempActor.SkinData.Materials.Num(); m++)
			{
				// Override:
				if(! materialnames[m][0] )
				{
					strcpy( materialnames[m], TempActor.SkinData.Materials[m].MaterialName );
				}
			}}

			{for(INT m=0; m<8; m++)
			{
				if ( materialnames[m][0] ) PrintWindowString( hWnd, MatIDC[m], materialnames[m] );
			}}
			break;

	
		case WM_COMMAND:

			switch (LOWORD(wParam)) 
			{
				case IDOK:
					EndDialog(hWnd, 1);
					break;

				case IDCANCEL:
					EndDialog(hWnd, 0);
				break;

				case IDC_READMAT:  // Read materials from the skin...
				{
					// Digest the skin & read materials...

					// Clear scene
					OurScene.Cleanup();
					TempActor.Cleanup();


					// Digest everything...
					OurScene.SurveyScene();
					OurScene.GetSceneInfo();


					// Skeleton & hierarchy unneeded for brushes.
					OurScene.EvaluateSkeleton(0);
					OurScene.DigestSkeleton( &TempActor );  // Digest skeleton into tempactor


					OurScene.DigestBrush( &TempActor );


					// See if there are materials from the mesh to write back to the window..
					{for(INT m=0; m < TempActor.SkinData.Materials.Num(); m++)
					{
						// Override:
						if( 1) // ! materialnames[m][0] )
						{
							strcpy( materialnames[m], TempActor.SkinData.Materials[m].MaterialName );
						}
					}}

					UpdateSlotNames(hWnd);
				}

				case IDC_CHECKSMOOTH: // all textured geometry skin checkbox
				{
					OurScene.DoSmooth =_GetCheckBox(hWnd,IDC_CHECKSMOOTH);
				}
				break;

				case IDC_CHECKSINGLE: // all textured geometry skin checkbox
				{
					OurScene.DoSingleTex =_GetCheckBox(hWnd,IDC_CHECKSINGLE);
				}
				break;

				// Retrieve material name updates.
				case IDC_EDITTEX0:
				case IDC_EDITTEX1:
				case IDC_EDITTEX2:
				case IDC_EDITTEX3:
				case IDC_EDITTEX4:
				case IDC_EDITTEX5:
				case IDC_EDITTEX6:
				case IDC_EDITTEX7:
				{
					switch( HIWORD(wParam) )
					{
						case EN_KILLFOCUS:
						{
							INT MatIdx = -1;
							for(INT m=0;m<8; m++)
							{
								if(MatIDC[m] == LOWORD(wParam))
									MatIdx = m;
							}
							if( MatIdx > -1)
							{
								GetWindowText(GetDlgItem(hWnd,MatIDC[MatIdx]),materialnames[MatIdx],300);
							}
						}
					}
					//UpdateSlotNames(hWnd);
				}
			

				case IDC_BRUSHPATH:
					{
						switch( HIWORD(wParam) )
						{
							case EN_KILLFOCUS:
							{
								GetWindowText(GetDlgItem(hWnd,IDC_BRUSHPATH),to_brushpath,300);										
								//_tcscpy(m_client->GetLogPath(),to_brushpath);
							}
							break;
						};
					}
					break;

				case IDC_BRUSHFILENAME:
					{
						switch( HIWORD(wParam) )
						{
							case EN_KILLFOCUS:
							{
								GetWindowText(GetDlgItem(hWnd,IDC_BRUSHFILENAME),to_brushfile,300);
							}
							break;
						};
					}
					break;


				// Browse for a destination directory.
				case IDC_BRUSHBROWSE:
					{					
						char  dir[MAX_PATH];
						if( GetFolderName(hWnd, dir) )
						{
							_tcscpy(to_brushpath,dir); 
							//_tcscpy(m_client->GetLogPath(),dir);					
						}
						SetWindowText(GetDlgItem(hWnd,IDC_BRUSHPATH),to_brushpath);															
					}	
					break;

				// 
				case IDC_EXPORTBRUSH:
					{
						//
						// Special digest-brush command - 
						// Switches
						// - selectedonly: only selected (textured) objects
						// - otherwise, all geometry nodes will be exported.
						// 

						// Clear scene
						OurScene.Cleanup();
						TempActor.Cleanup();

						// Scene stats. #debug: surveying should be done only once ?
						OurScene.SurveyScene();
						OurScene.GetSceneInfo();

						// Skeleton & hierarchy unneeded for brushes....				
						OurScene.EvaluateSkeleton(0);
						//DebugBox("End EvalSK");
					#if 1
						OurScene.DigestSkeleton( &TempActor );  // Digest skeleton into tempactor
						//DebugBox("End DigestSk");
					#endif

						OurScene.DigestBrush( &TempActor );
						//DebugBox("End DigestBrush");

						//PopupBox("Processed brush; points:[%i]",TempActor.SkinData.Points.Num());

						//
						// See if there are materials to override
						//
						for(INT m=0; m < TempActor.SkinData.Materials.Num(); m++)
						{
							// Override:
							if( materialnames[m][0] )
							{								
								TempActor.SkinData.Materials[m].SetName(materialnames[m]);
							}
							else // copy it back...
							{
								strcpy( materialnames[m], TempActor.SkinData.Materials[m].MaterialName );
							}
						}

						//#SKEL - doesn't correctly re-flag yet..
						//TempActor.ReflagMaterials(); // Get new flags from TempActor.SkinData.Materials' new names.

						if( TempActor.SkinData.Points.Num() == 0 )
							//WarningBox(" Warning: no valid geometry digested (missing mapping or selection?)");
						else
						{
							TCHAR to_ext[32];
							_tcscpy(to_ext,("T3D"));
							sprintf(DestPath,"%s\\%s.%s", (char*)to_brushpath, (char*)to_brushfile, to_ext);
							FastFileClass OutFile;

							if ( OutFile.CreateNewFile(DestPath) != 0) // Error !
								//ErrorBox( "File creation error. ");
					
							TempActor.WriteBrush( OutFile, OurScene.DoSmooth, OurScene.DoSingleTex ); // Save brush.
						
							 // Close.
							OutFile.CloseFlush();

							INT MatCount =  OurScene.DoSingleTex ? 1 : TempActor.SkinData.Materials.Num();

							if( OutFile.GetError()) //ErrorBox("Brush Save Error");
								else
							//PopupBox(" Brush  %s\\%s.%s written; %i material(s).",to_brushpath,to_brushfile,to_ext, MatCount);		

							// Log materials and stats.
							// if( OurScene.DoLog ) OurScene.LogSkinInfo( &TempActor, to_brushfile );

							UpdateSlotNames(hWnd);
						}
					}
					break;
				}
				break;

		default:
		return FALSE;
	}
	return TRUE;
}       

#endif // 0 
//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------




















