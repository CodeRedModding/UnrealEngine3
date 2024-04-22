/*=============================================================================
	GPUSkinVertexFactory.h: GPU skinning vertex factory definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
	
	This code contains embedded portions of source code from dqconv.c Conversion routines between (regular quaternion, translation) and dual quaternion, Version 1.0.0, Copyright (c) 2006-2007 University of Dublin, Trinity College, All Rights Reserved, which have been altered from their original version.

	The following terms apply to dqconv.c Conversion routines between (regular quaternion, translation) and dual quaternion, Version 1.0.0:

	This software is provided 'as-is', without any express or implied warranty.  In no event will the author(s) be held liable for any damages arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.


=============================================================================*/

#ifndef __GPUSKINVERTEXFACTORY_H__
#define __GPUSKINVERTEXFACTORY_H__

#include "GPUSkinPublicDefs.h"

#define QUAT_SKINNING DQ_SKINNING

/** for final bone matrices - this needs to move out of ifdef due to APEX using it*/
MS_ALIGN(16) struct FSkinMatrix3x4
{
	FLOAT M[3][4];
	FORCEINLINE void SetMatrix(const FMatrix& Mat)
	{
		const FLOAT * RESTRICT Src = &(Mat.M[0][0]);
		FLOAT * RESTRICT Dest = &(M[0][0]);

		Dest[0] = Src[0];   // [0][0]
		Dest[1] = Src[1];   // [0][1]
		Dest[2] = Src[2];   // [0][2]
		Dest[3] = Src[3];   // [0][3]

		Dest[4] = Src[4];   // [1][0]
		Dest[5] = Src[5];   // [1][1]
		Dest[6] = Src[6];   // [1][2]
		Dest[7] = Src[7];   // [1][3]

		Dest[8] = Src[8];   // [2][0]
		Dest[9] = Src[9];   // [2][1]
		Dest[10] = Src[10]; // [2][2]
		Dest[11] = Src[11]; // [2][3]
	}

	FORCEINLINE void SetMatrixTranspose(const FMatrix& Mat)
	{

		const FLOAT * RESTRICT Src = &(Mat.M[0][0]);
		FLOAT * RESTRICT Dest = &(M[0][0]);

		Dest[0] = Src[0];   // [0][0]
		Dest[1] = Src[4];   // [1][0]
		Dest[2] = Src[8];   // [2][0]
		Dest[3] = Src[12];  // [3][0]

		Dest[4] = Src[1];   // [0][1]
		Dest[5] = Src[5];   // [1][1]
		Dest[6] = Src[9];   // [2][1]
		Dest[7] = Src[13];  // [3][1]

		Dest[8] = Src[2];   // [0][2]
		Dest[9] = Src[6];   // [1][2]
		Dest[10] = Src[10]; // [2][2]
		Dest[11] = Src[14]; // [3][2]
	}
} GCC_ALIGN(16);

#if QUAT_SKINNING

/*
 * Dual Quaternion - http://isg.cs.tcd.ie/projects/DualQuaternions/
 * input: unit quaternion 'q0', translation vector 't' 
 * output: unit dual quaternion 'dq'
 */
FORCEINLINE void QuatTrans2UDQ(const FLOAT Q0[4], const FLOAT T[3], FLOAT DQ[][4])
{
	appMemcpy(&DQ[0][0], &Q0[0], sizeof(FLOAT)*4);

	// dual part:
	DQ[1][0] = -0.5*(T[0]*Q0[1] + T[1]*Q0[2] + T[2]*Q0[3]);
	DQ[1][1] = 0.5*( T[0]*Q0[0] + T[1]*Q0[3] - T[2]*Q0[2]);
	DQ[1][2] = 0.5*(-T[0]*Q0[3] + T[1]*Q0[0] + T[2]*Q0[1]);
	DQ[1][3] = 0.5*( T[0]*Q0[2] - T[1]*Q0[1] + T[2]*Q0[0]);
}

typedef FLOAT	FBoneScale;

MS_ALIGN(16) struct FBoneQuat
{
	FLOAT DQ1[4];
	FLOAT DQ2[4];

	FORCEINLINE void SetBoneAtom(const FBoneAtom& Bone)
	{
		FQuat Rotation = Bone.GetRotation();
		FVector Translation = Bone.GetTranslation();

		FLOAT Q0[] = {Rotation.W, Rotation.X, Rotation.Y, Rotation.Z};
		FLOAT T[] = {Translation.X, Translation.Y, Translation.Z};
		FLOAT DQ[2][4];
		QuatTrans2UDQ(Q0, T, DQ);

		appMemcpy(&DQ1[0], &DQ[0][0], sizeof(FLOAT)*4);
		appMemcpy(&DQ2[0], &DQ[1][0], sizeof(FLOAT)*4);
	}
} GCC_ALIGN(16);

typedef FBoneQuat FBoneSkinning;

#define SET_BONE_DATA(B, X) B.SetBoneAtom(X)

#else

typedef FSkinMatrix3x4 FBoneSkinning;

#define SET_BONE_DATA(B, X) B.SetMatrixTranspose(X)

#endif

class FBoneDataTexture :public FTextureResource
{
public:
	/** constructor */
	FBoneDataTexture();
	/** 
	* @param TotalTexelCount sum of all chunks bone count
	*/
	void SetTexelSize(UINT TotalTexelCount);
	/** 
	* call UnlockData() after this one
	* @return never 0
	*/
	FLOAT* LockData();
	/**
	* Needs to be called after LockData()
	*/
	void UnlockData();

	// interface FRenderResource ------------------------------------------

	/** Called when the resource is initialized. This is only called by the rendering thread. */
	virtual void InitDynamicRHI();
	/** Called when the resource is released. This is only called by the rendering thread. */
	virtual void ReleaseDynamicRHI();
	/** Returns the width of the texture in pixels. */
	virtual UINT GetSizeX() const;
	/** Returns the height of the texture in pixels. */
	virtual UINT GetSizeY() const;
	/** Accessor */
	FTexture2DRHIRef GetTexture2DRHI();

private: // -------------------------------------------------------

	/** Texture width in texels, multiple of 3, 0 if not created yet */
	UINT SizeX;
	/** Texture2D reference, used for locking/unlocking the mips. */
	FTexture2DRHIRef Texture2DRHI;
	/** @return in bytes */
	UINT ComputeMemorySize();
};

/** Vertex factory with vertex stream components for GPU skinned vertices */
class FGPUSkinVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FGPUSkinVertexFactory);

public:
	struct DataType
	{
		/** The stream to read the vertex position from. */
		FVertexStreamComponent PositionComponent;

		/** The streams to read the tangent basis from. */
		FVertexStreamComponent TangentBasisComponents[2];

		/** The streams to read the texture coordinates from. */
		TPreallocatedArray<FVertexStreamComponent,MAX_TEXCOORDS> TextureCoordinates;

		/** The stream to read the vertex color from. */
		FVertexStreamComponent ColorComponent;

		/** The stream to read the bone indices from */
		FVertexStreamComponent BoneIndices;

		/** The stream to read the bone weights from */
		FVertexStreamComponent BoneWeights;
	};

	struct ShaderDataType
	{
		/**
		 * Constructor, presizing bones array.
		 *
		 * @param	InBoneMatrices	Reference to shared bone matrices array.
		 */
		ShaderDataType(UBOOL bInUsePerBoneMotionBlur,
			TArray<FBoneSkinning>& InBoneMatrices
#if QUAT_SKINNING
			, TArray<FBoneScale>& InBoneScales 
#endif
			)
		:	BoneMatrices( InBoneMatrices )
#if QUAT_SKINNING
		, BoneScales(InBoneScales)
#endif
		{
			if(bInUsePerBoneMotionBlur)
			{
				OldBoneDataStartIndex[0] = 0xffffffff;
				OldBoneDataStartIndex[1] = 0xffffffff;
			}
			else
			{
				// special id to disable MotiopnBlurskinning on this one
				OldBoneDataStartIndex[0] = 0xdeaddead;
			}

			// BoneDataOffset and BoneTextureSize are not set as they are only valid if IsValidRef(BoneTexture)
		}

		/** Reference to shared ref pose to local space matrices */
		TArray<FBoneSkinning>& BoneMatrices;	
#if QUAT_SKINNING		
		TArray<FBoneScale>& BoneScales;
#endif

		/** Mesh origin and Mesh Extension for Mesh compressions **/
		/** This value will be (0, 0, 0), (1, 1, 1) relatively for non compressed meshes **/
		FVector MeshOrigin;
		FVector MeshExtension;

		/** @return 0xffffffff means not valid */
		UINT GetOldBoneData(UINT InFrameNumber) const
		{
			// will also work in the wrap around case
			UINT LastFrameNumber = InFrameNumber - 1;

			// pick the data with the right frame number
			if(OldBoneFrameNumber[0] == LastFrameNumber)
			{
				// [0] has the right data
				return OldBoneDataStartIndex[0];
			}
			else if(OldBoneFrameNumber[1] == LastFrameNumber)
			{
				// [1] has the right data
				return OldBoneDataStartIndex[1];
			}
			else
			{
				// all data stored is too old
				return 0xffffffff;
			}
		}

		/** Set the data with the given frame number, keeps data for the last frame. */
		void SetOldBoneData(UINT FrameNumber, UINT Index) const
		{
			// keep the one from last from, someone might read want to read that
			if(OldBoneFrameNumber[0] + 1 == FrameNumber)
			{
				// [1] has the right data
				OldBoneFrameNumber[1] = FrameNumber;
				OldBoneDataStartIndex[1] = Index;
			}
			else
			{
				// [0] has the right data
				OldBoneFrameNumber[0] = FrameNumber;
				OldBoneDataStartIndex[0] = Index;
			}
		}

		/** Checks if we need to update the data for this frame */
		UBOOL IsOldBoneDataUpdateNeeded(UINT InFrameNumber) const
		{
			if(OldBoneDataStartIndex[0] == 0xdeaddead)
			{
				// special id to disable MotionBlurSkinning
				return FALSE;
			}

			if(OldBoneFrameNumber[0] == InFrameNumber
			|| OldBoneFrameNumber[1] == InFrameNumber)
			{
				return FALSE;
			}

			return TRUE;
		}

	private:

		// the following members can be stored in less bytes (after some adjustments)

		/** Start bone index in BoneTexture valid means != 0xffffffff */
		mutable UINT OldBoneDataStartIndex[2];
		/** FrameNumber from the view when the data was set, only valid if OldBoneDataStartIndex != 0xffffffff */
		mutable UINT OldBoneFrameNumber[2];
	};

	/**
	 * Constructor presizing bone matrices array to used amount.
	 *
	 * @param	InBoneMatrices	Reference to shared bone matrices array.
	 */
	FGPUSkinVertexFactory(
		UBOOL bInUsePerBoneMotionBlur,
		TArray<FBoneSkinning>& InBoneMatrices 
#if QUAT_SKINNING
		, TArray<FBoneScale>& InBoneScales 
#endif
		)
	:	ShaderData( bInUsePerBoneMotionBlur, InBoneMatrices
#if QUAT_SKINNING
	, InBoneScales 
#endif
	)
	{}

	/**
	* Modify compile environment to enable the decal codepath
	* @param OutEnvironment - shader compile environment to modify
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);

	/** ShouldCache function that is shared with the decal vertex factories. */
	static UBOOL SharedShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const FShaderType* ShaderType);

	/**
	* An implementation of the interface used by TSynchronizedResource to 
	* update the resource with new data from the game thread.
	* @param	InData - new stream component data
	*/
	void SetData(const DataType& InData)
	{
		Data = InData;
		UpdateRHI();
	}

	/** accessor */
	FORCEINLINE ShaderDataType& GetShaderData()
	{
		return ShaderData;
	}

	FORCEINLINE const ShaderDataType& GetShaderData() const
	{
		return ShaderData;
	}

	virtual UBOOL IsGPUSkinned() const { return TRUE; }

	// FRenderResource interface.

	/**
	* Creates declarations for each of the vertex stream components and
	* initializes the device resource
	*/
	virtual void InitRHI();

	/**
	 * Set the data with the given frame number, keeps data for the last frame.
	 * @param Index 0xffffffff to disable motion blur skinning
	 */
	void SetOldBoneDataStartIndex(UINT FrameNumber, UINT Index) const
	{
		ShaderData.SetOldBoneData(FrameNumber, Index);
	}

	static UBOOL SupportsTessellationShaders() { return TRUE; }

	/** Checks if we need to update the data for this frame */
	UBOOL IsOldBoneDataUpdateNeeded(UINT InFrameNumber) const
	{
		return ShaderData.IsOldBoneDataUpdateNeeded(InFrameNumber);
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

protected:
	/**
	* Add the decl elements for the streams
	* @param InData - type with stream components
	* @param OutElements - vertex decl list to modify
	*/
	void AddVertexElements(DataType& InData, FVertexDeclarationElementList& OutElements);

private:
	/** stream component data bound to this vertex factory */
	DataType Data;  
	/** dynamic data need for setting the shader */ 
	ShaderDataType ShaderData;
};

/** Vertex factory with vertex stream components for GPU-skinned and morph target streams */
class FGPUSkinMorphVertexFactory : public FGPUSkinVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FGPUSkinMorphVertexFactory);

	typedef FGPUSkinVertexFactory Super;
public:

	struct DataType : FGPUSkinVertexFactory::DataType
	{
		/** stream which has the position deltas to add to the vertex position */
		FVertexStreamComponent DeltaPositionComponent;
		/** stream which has the TangentZ deltas to add to the vertex normals */
		FVertexStreamComponent DeltaTangentZComponent;
	};

	/**
	 * Constructor presizing bone matrices array to used amount.
	 *
	 * @param	InBoneMatrices	Reference to shared bone matrices array.
	 */
	FGPUSkinMorphVertexFactory(UBOOL bInUsePerBoneMotionBlur,
		TArray<FBoneSkinning>& InBoneMatrices
#if QUAT_SKINNING
		, TArray<FBoneScale>& InBoneScales 
#endif
		)
	: FGPUSkinVertexFactory(bInUsePerBoneMotionBlur,
		InBoneMatrices
#if QUAT_SKINNING
	, InBoneScales 
#endif	
	)
	{}

	/**
	* Modify compile environment to enable the morph blend codepath
	* @param OutEnvironment - shader compile environment to modify
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);

	/** ShouldCache function that is shared with the decal vertex factories. */
	static UBOOL SharedShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	* Should we cache the material's shader type on this platform with this vertex factory? 
	*/
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const FShaderType* ShaderType);

	/**
	* An implementation of the interface used by TSynchronizedResource to 
	* update the resource with new data from the game thread.
	* @param	InData - new stream component data
	*/
	void SetData(const DataType& InData)
	{
		MorphData = InData;
		UpdateRHI();
	}

	// FRenderResource interface.

	/**
	* Creates declarations for each of the vertex stream components and
	* initializes the device resource
	*/
	virtual void InitRHI();

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

protected:
	/**
	* Add the decl elements for the streams
	* @param InData - type with stream components
	* @param OutElements - vertex decl list to modify
	*/
	void AddVertexElements(DataType& InData, FVertexDeclarationElementList& OutElements);

private:
	/** stream component data bound to this vertex factory */
	DataType MorphData; 

};

#include "SkelMeshDecalVertexFactory.h"
 
/** Decal vertex factory with vertex stream components for GPU-skinned vertices */
class FGPUSkinDecalVertexFactory : public FGPUSkinVertexFactory, public FSkelMeshDecalVertexFactoryBase
{
	DECLARE_VERTEX_FACTORY_TYPE(FGPUSkinDecalVertexFactory);

public:
	typedef FGPUSkinVertexFactory Super;

	/**
	 * Constructor presizing bone matrices array to used amount.
	 *
	 * @param	InBoneMatrices	Reference to shared bone matrices array.
	 */
	FGPUSkinDecalVertexFactory(UBOOL bInUsePerBoneMotionBlur,
		TArray<FBoneSkinning>& InBoneMatrices
#if QUAT_SKINNING
		, TArray<FBoneScale>& InBoneScales 
#endif
		)
	: FGPUSkinVertexFactory(bInUsePerBoneMotionBlur,
		InBoneMatrices
#if QUAT_SKINNING
	, InBoneScales 
#endif
	)
	{}

	/** Must match the value of the DECAL_FACTORY define */
	virtual UBOOL IsDecalFactory() const { return TRUE; }

	virtual FVertexFactory* CastToFVertexFactory()
	{
		return static_cast<FVertexFactory*>( this );
	}

	virtual FSkelMeshDecalVertexFactoryBase* CastToFSkelMeshDecalVertexFactoryBase()
	{
		return static_cast<FSkelMeshDecalVertexFactoryBase*>( this );
	}

	/**
	 * Should we cache the material's shader type on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	* Modify compile environment to enable the decal codepath
	* @param OutEnvironment - shader compile environment to modify
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);
};

/** Decal vertex factory with vertex stream components for GPU-skinned vertices */
class FGPUSkinMorphDecalVertexFactory : public FGPUSkinMorphVertexFactory, public FSkelMeshDecalVertexFactoryBase
{
	DECLARE_VERTEX_FACTORY_TYPE(FGPUSkinMorphDecalVertexFactory);

public:
	typedef FGPUSkinMorphVertexFactory Super;

	/**
	 * Constructor presizing bone matrices array to used amount.
	 *
	 * @param	InBoneMatrices	Reference to shared bone matrices array.
	 */
	FGPUSkinMorphDecalVertexFactory(UBOOL bInUsePerBoneMotionBlur,
		TArray<FBoneSkinning>& InBoneMatrices
#if QUAT_SKINNING
		, TArray<FBoneScale>& InBoneScales 
#endif
		)
	: FGPUSkinMorphVertexFactory(bInUsePerBoneMotionBlur,
	InBoneMatrices
#if QUAT_SKINNING
	, InBoneScales 
#endif
	)
	{}

	/** Must match the value of the DECAL_FACTORY define */
	virtual UBOOL IsDecalFactory() const { return TRUE; }

	virtual FVertexFactory* CastToFVertexFactory()
	{
		return static_cast<FVertexFactory*>( this );
	}

	virtual FSkelMeshDecalVertexFactoryBase* CastToFSkelMeshDecalVertexFactoryBase()
	{
		return static_cast<FSkelMeshDecalVertexFactoryBase*>( this );
	}

	/**
	* Should we cache the material's shader type on this platform with this vertex factory? 
	*/
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	* Modify compile environment to enable the decal codepath
	* @param OutEnvironment - shader compile environment to modify
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);
};

#endif // __GPUSKINVERTEXFACTORY_H__
