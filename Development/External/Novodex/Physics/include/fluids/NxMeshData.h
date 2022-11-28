#ifndef NX_FLUIDS_NXMESHDATA
#define NX_FLUIDS_NXMESHDATA
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

/**
Not available in the current release.
Very similar to NxMeshFlags used for the NxSimpleTriangleMesh type.
*/
enum NxMeshDataFlags
	{
	/**
	Not available in the current release.
	Denotes the use of 16-bit vertex indices.
	*/
	NX_MDF_16_BIT_INDICES				=	1 << 0, 

	/**
	Not available in the current release.
	Specifies that all floats are written compressed to 16 bit.
	*/
	NX_MDF_16_BIT_COMPRESSED_FLOATS 	=	1 << 1, 

	/**
	Not available in the current release.
	Specifies that triangle indices are generated and adjacent triangles share common vertices.
	If this flag is not set, all triangles are described as vertex triplets in the vertex array.
	*/
	NX_MDF_INDEXED_MESH					=	1 << 2,

	};

/**
Not available in the current release.
Descriptor-like user side class for describing fluid mesh data. Very similar to NxSimpleTriangleMesh, 
with the difference, that this user buffer wrapper is used to let the SDK write to user buffers instead
of reading from them.
*/
class NxMeshData
	{
	public:

	/**
	Not available in the current release.
	The pointer to the user specified buffer for vertex positions. A vertex position consists of three 
	consequent 32 or 16 bit floats, depending on whether NX_MDF_16_BIT_COMPRESSED_FLOATS has been set.
	*/
	void*					verticesPosBegin;

	/**
	Not available in the current release.
	The pointer to the user specified buffer for vertex normals. 
 	A vertex normal consists of three consequent 32/16 bit floats.
	*/
	void*					verticesNormalBegin;

	/**
	Not available in the current release.
	Specifies the distance of two vertex position start addresses in bytes.
	*/
	NxI32					verticesPosByteStride;
	
	/**
	Not available in the current release.
	Same as veticesPosByteStride for vertex normals.
	*/
	NxI32					verticesNormalByteStride;

	/**
	Not available in the current release.
	The maximal number of vertices which can be stored in the user vertex buffers.
	*/
	NxU32					maxVertices;

	/**
	Not available in the current release.
	Has to point to the user allocated memory holding the number of vertices stored in the user vertex 
	buffers. If the SDK writes to a given vertex buffer, it also sets the numbers of elements written.
	*/
	NxU32*					numVerticesPtr;

	/**
	Not available in the current release.
	The pointer to the user specified buffer for vertex indices. An index consist of one 32 or 16 bit 
	integer, depending on whether NX_MDF_16_BIT_INDICES has been set. This is only used when the flag
	NX_MDF_INDEXED_MESH has been set.
	*/
	void*					indicesBegin;

	/**
	Not available in the current release.
	Specifies the distance of two vertex indices start addresses in bytes. This is only used when the 
	flag NX_MDF_INDEXED_MESH has been set.
	*/
	NxI32					indicesByteStride;

	/**
	Not available in the current release.
	The maximal number of indices which can be stored in the user index buffer. This is only used 
	when the flag NX_MDF_INDEXED_MESH has been set.
	*/
	NxU32					maxIndices;

	/**
	Not available in the current release.
	Has to point to the user allocated memory holding the number of vertex triplets used to define 
	triangles. If the SDK writes to a given triangle index buffer, it also sets the numbers of
	triangles written. If the flag NX_MDF_INDEXED_MESH has not been set, the triangle number is
	equal to the number of vertices divided by 3.
	*/
	NxU32*					numTrianglesPtr;

	/**
	Not available in the current release.
	Flags of type NxMeshDataFlags
	*/
	NxU32					flags;




	const char*				name;			//!< Possible debug name. The string is not copied by the SDK, only the pointer is stored.

	NX_INLINE ~NxMeshData();
	/**
	Not available in the current release.
	(Re)sets the structure to the default.	
	*/
	NX_INLINE void setToDefault();
	/**
	Not available in the current release.
	Returns true if the current settings are valid
	*/
	NX_INLINE bool isValid() const;

	/**
	Not available in the current release.
	Constructor sets to default.
	*/
	NX_INLINE	NxMeshData();
	};

NX_INLINE NxMeshData::NxMeshData()
	{
	setToDefault();
	}

NX_INLINE NxMeshData::~NxMeshData()
	{
	}

NX_INLINE void NxMeshData::setToDefault()
	{
	verticesPosBegin				= NULL;
	verticesNormalBegin				= NULL;
	verticesPosByteStride			= 0;
	verticesNormalByteStride		= 0;
	maxVertices						= 0;
	numVerticesPtr					= NULL;
	indicesBegin					= NULL;
	indicesByteStride				= 0;
	maxIndices						= 0;
	numTrianglesPtr					= NULL;
	flags							= NX_MDF_INDEXED_MESH;

	name							= NULL;
	}

NX_INLINE bool NxMeshData::isValid() const
	{
	if (numVerticesPtr && !(verticesPosBegin && verticesNormalBegin)) return false;
	if (!numVerticesPtr && (verticesPosBegin || verticesNormalBegin)) return false;

	if (verticesPosBegin && !verticesPosByteStride) return false;
	if (verticesNormalBegin && !verticesNormalByteStride) return false;

	if (!numVerticesPtr && numTrianglesPtr) return false;
	if (numVerticesPtr && flags & NX_MDF_INDEXED_MESH && !numTrianglesPtr) return false;
	if (numVerticesPtr && !(flags & NX_MDF_INDEXED_MESH) && numTrianglesPtr) return false;

	if (!numTrianglesPtr && indicesBegin) return false;
	if (numTrianglesPtr && !indicesBegin) return false;

	if (indicesBegin && !indicesByteStride) return false;
				
	return true;
	}


#endif
