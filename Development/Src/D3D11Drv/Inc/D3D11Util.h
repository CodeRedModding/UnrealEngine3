/*=============================================================================
	D3D11Util.h: D3D RHI utility definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * Checks that the given result isn't a failure.  If it is, the application exits with an appropriate error message.
 * @param	Result - The result code to check
 * @param	Code - The code which yielded the result.
 * @param	Filename - The filename of the source file containing Code.
 * @param	Line - The line number of Code within Filename.
 */
extern void VerifyD3D11Result(HRESULT Result,const ANSICHAR* Code,const ANSICHAR* Filename,UINT Line);

/**
* Checks that the given result isn't a failure.  If it is, the application exits with an appropriate error message.
* @param	Result - The result code to check
* @param	Code - The code which yielded the result.
* @param	Filename - The filename of the source file containing Code.
* @param	Line - The line number of Code within Filename.	
*/
extern void VerifyD3D11CreateTextureResult(HRESULT D3DResult,const ANSICHAR* Code,const ANSICHAR* Filename,UINT Line,
										 UINT SizeX,UINT SizeY,BYTE D3DFormat,UINT NumMips,DWORD Flags);

/**
 * A macro for using VERIFYD3D11RESULT that automatically passes in the code and filename/line.
 */
#if DO_CHECK
#define VERIFYD3D11RESULT(x) VerifyD3D11Result(x,#x,__FILE__,__LINE__);
#define VERIFYD3D11CREATETEXTURERESULT(x,SizeX,SizeY,Format,NumMips,Flags) VerifyD3D11CreateTextureResult(x,#x,__FILE__,__LINE__,SizeX,SizeY,Format,NumMips,Flags);
#else
#define VERIFYD3D11RESULT(x) (x);
#define VERIFYD3D11CREATETEXTURERESULT(x,SizeX,SizeY,Format,NumMips,Flags) (x);
#endif

/**
* Convert from ECubeFace to D3DCUBEMAP_FACES type
* @param Face - ECubeFace type to convert
* @return D3D cube face enum value
*/
FORCEINLINE UINT GetD3D11CubeFace(ECubeFace Face)
{
	switch(Face)
	{
	case CubeFace_PosX:
	default:
		return 0;//D3DCUBEMAP_FACE_POSITIVE_X;
	case CubeFace_NegX:
		return 1;//D3DCUBEMAP_FACE_NEGATIVE_X;
	case CubeFace_PosY:
		return 2;//D3DCUBEMAP_FACE_POSITIVE_Y;
	case CubeFace_NegY:
		return 3;//D3DCUBEMAP_FACE_NEGATIVE_Y;
	case CubeFace_PosZ:
		return 4;//D3DCUBEMAP_FACE_POSITIVE_Z;
	case CubeFace_NegZ:
		return 5;//D3DCUBEMAP_FACE_NEGATIVE_Z;
	};
}

/** 
* Get an appropriate Quality level for the requested MSAA Count.  Should take into account IHV support as well.
*/
FORCEINLINE UINT GetMultisampleQuality(UINT MultiSampleCount, UINT NumMultiSampleQualities)
{
	// TODO: Add an ini setting to control CSAA or other levels
	return 0;
}

/**
 * Keeps track of Locks for D3D11 objects
 */
class FD3D11LockedKey
{
public:
	void* SourceObject;
	UINT Subresource;

public:
	FD3D11LockedKey() : SourceObject(NULL)
		, Subresource(0)
	{}
	FD3D11LockedKey(ID3D11Texture2D* source, UINT subres=0) : SourceObject((void*)source)
		, Subresource(subres)
	{}
	FD3D11LockedKey(ID3D11Texture3D* source, UINT subres=0) : SourceObject((void*)source)
		, Subresource(subres)
	{}
	FD3D11LockedKey(ID3D11Buffer* source, UINT subres=0) : SourceObject((void*)source)
		, Subresource(subres)
	{}
	UBOOL operator==( const FD3D11LockedKey& Other ) const
	{
		return SourceObject == Other.SourceObject && Subresource == Other.Subresource;
	}
	UBOOL operator!=( const FD3D11LockedKey& Other ) const
	{
		return SourceObject != Other.SourceObject || Subresource != Other.Subresource;
	}
	FD3D11LockedKey& operator=( const FD3D11LockedKey& Other )
	{
		SourceObject = Other.SourceObject;
		Subresource = Other.Subresource;
		return *this;
	}
	DWORD GetHash() const
	{
		return PointerHash( SourceObject );
	}

	/** Hashing function. */
	friend DWORD GetTypeHash( const FD3D11LockedKey& K )
	{
		return K.GetHash();
	}
};

/** Information about a D3D resource that is currently locked. */
struct FD3D11LockedData
{
	TRefCountPtr<ID3D11Resource> StagingResource;
	BYTE* Data;
	UINT Pitch;
	UINT DepthPitch;
};
