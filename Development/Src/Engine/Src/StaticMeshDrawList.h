/*=============================================================================
	StaticMeshDrawList.h: Static mesh draw list definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __STATICMESHDRAWLIST_H__
#define __STATICMESHDRAWLIST_H__

/** Base class of the static draw list, used when comparing draw lists and the drawing policy type is not necessary. */
class FStaticMeshDrawListBase
{
public:

	static SIZE_T TotalBytesUsed;
};

/**
 * A set of static meshes, each associated with a mesh drawing policy of a particular type.
 * @param DrawingPolicyType - The drawing policy type used to draw mesh in this draw list.
 */
template<typename DrawingPolicyType>
class TStaticMeshDrawList : public FStaticMeshDrawListBase
{
public:
	typedef typename DrawingPolicyType::ElementDataType ElementPolicyDataType;

private:

	/** A handle to an element in the draw list.  Used by FStaticMesh to keep track of draw lists containing the mesh. */
	class FElementHandle : public FStaticMesh::FDrawListElementLink
	{
	public:

		/** Initialization constructor. */
		FElementHandle(TStaticMeshDrawList* InStaticMeshDrawList,FSetElementId InSetId,INT InElementIndex):
		  StaticMeshDrawList(InStaticMeshDrawList)
		  ,SetId(InSetId)
		  ,ElementIndex(InElementIndex)
		{
		}

		virtual UBOOL IsInDrawList(const FStaticMeshDrawListBase* DrawList) const
		{
			return DrawList == StaticMeshDrawList;
		}
		// FAbstractDrawListElementLink interface.
		virtual void Remove();

	private:
		TStaticMeshDrawList* StaticMeshDrawList;
		FSetElementId SetId;
		INT ElementIndex;
	};

	/**
	 * This structure stores the info needed for visibility culling a static mesh element.
	 * Stored separately to avoid bringing the other info about non-visible meshes into the cache.
	 */
	struct FElementCompact
	{
		FRelativeBitReference VisibilityBitReference;
		FElementCompact() {}
		FElementCompact(INT MeshId)
		: VisibilityBitReference(MeshId)
		{}
	};

	struct FElement
	{
		ElementPolicyDataType PolicyData;
		FStaticMesh* Mesh;
#if MOBILE
		mutable void* CachedProgramInstance;
#endif
		TRefCountPtr<FElementHandle> Handle;

		/** Default constructor. */
		FElement():
			Mesh(NULL)
#if MOBILE
			, CachedProgramInstance(NULL)
#endif
		{}

		/** Minimal initialization constructor. */
		FElement(
			FStaticMesh* InMesh,
			const ElementPolicyDataType& InPolicyData,
			TStaticMeshDrawList* StaticMeshDrawList,
			FSetElementId SetId,
			INT ElementIndex
			):
			PolicyData(InPolicyData),
			Mesh(InMesh),
#if MOBILE
			CachedProgramInstance(NULL),
#endif
			Handle(new FElementHandle(StaticMeshDrawList,SetId,ElementIndex))
		{}

		/** Destructor. */
		~FElement()
		{
			if(Mesh)
			{
				Mesh->UnlinkDrawList(Handle);
			}
		}
	};

	IMPLEMENT_COMPARE_CONSTREF(FLOAT, SimpleFloatCompare, { return( B - A ) < 0 ? 1 : -1; } )

	/** A set of draw list elements with the same drawing policy. */
	struct FDrawingPolicyLink
	{
		TArray<FElementCompact>		CompactElements;    // The elements array and the compact elements array are always synchronized
		TArray<FElement>			Elements;
		DrawingPolicyType			DrawingPolicy;
		FBoundShaderStateRHIRef		BoundShaderState;
#if WITH_MOBILE_RHI
		TMap<INT,FLOAT>				PendingDrawElements;
#endif

		/** The id of this link in the draw list's set of drawing policy links. */
		FSetElementId SetId;

		TStaticMeshDrawList* DrawList;

		/** Initialization constructor. */
		FDrawingPolicyLink(TStaticMeshDrawList* InDrawList,const DrawingPolicyType& InDrawingPolicy):
			DrawingPolicy(InDrawingPolicy),
			DrawList(InDrawList)
		{
			BoundShaderState = DrawingPolicy.CreateBoundShaderState();
		}

		SIZE_T GetSizeBytes() const
		{
			return sizeof(*this) + CompactElements.GetAllocatedSize() + Elements.GetAllocatedSize();
		}

		void SortPendingDrawElements()
		{
#if WITH_MOBILE_RHI
			PendingDrawElements.ValueSort<COMPARE_CONSTREF_CLASS(FLOAT,SimpleFloatCompare)>();
#endif
		}
	};

	/** Functions to extract the drawing policy from FDrawingPolicyLink as a key for TSet. */
	struct FDrawingPolicyKeyFuncs : BaseKeyFuncs<FDrawingPolicyLink,DrawingPolicyType>
	{
		static const DrawingPolicyType& GetSetKey(const FDrawingPolicyLink& Link)
		{
			return Link.DrawingPolicy;
		}

		static UBOOL Matches(const DrawingPolicyType& A,const DrawingPolicyType& B)
		{
			return A.Matches(B);
		}

		static DWORD GetKeyHash(const DrawingPolicyType& DrawingPolicy)
		{
			return DrawingPolicy.GetTypeHash();
		}
	};

	/**
	* Draws a single FElement
	* @param View - The view of the meshes to render.
	* @param Element - The mesh element
	* @param DrawingPolicyLink - the drawing policy link
	* @param bDrawnShared - determines whether to draw shared 
	*/
	void DrawElement(const FViewInfo& View, const FElement& Element, const FDrawingPolicyLink* DrawingPolicyLink, UBOOL &bDrawnShared) const;

public:

	/**
	 * Adds a mesh to the draw list.
	 * @param Mesh - The mesh to add.
	 * @param PolicyData - The drawing policy data for the mesh.
	 * @param InDrawingPolicy - The drawing policy to use to draw the mesh.
	 */
	void AddMesh(
		FStaticMesh* Mesh,
		const ElementPolicyDataType& PolicyData,
		const DrawingPolicyType& InDrawingPolicy
		);

	/**
	 * Removes all meshes from the draw list.
	 */
	void RemoveAllMeshes();

	/**
	 * Draws only the static meshes which are in the visibility map.
	 * @param CI - The command interface to the execute the draw commands on.
	 * @param View - The view of the meshes to render.
	 * @param StaticMeshVisibilityMap - An map from FStaticMesh::Id to visibility state.
	 * @return True if any static meshes were drawn.
	 */
	UBOOL DrawVisible(const FViewInfo& View, const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap) const;

	/**
	 * @return total number of meshes in all draw policies
	 */
	INT NumMeshes() const;

	~TStaticMeshDrawList();

private:
	/** All drawing policies in the draw list, in rendering order. */
    TArray<FSetElementId> OrderedDrawingPolicies;

	/** All drawing policy element sets in the draw list, hashed by drawing policy. */
	TSet<FDrawingPolicyLink,FDrawingPolicyKeyFuncs> DrawingPolicySet;
};

#include "StaticMeshDrawList.inl"

#endif
