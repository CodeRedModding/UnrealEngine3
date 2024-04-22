/*=============================================================================
	StaticMeshDrawList.inl: Static mesh draw list implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __STATICMESHDRAWLIST_INL__
#define __STATICMESHDRAWLIST_INL__

template<typename DrawingPolicyType>
void TStaticMeshDrawList<DrawingPolicyType>::FElementHandle::Remove()
{
	// Make a copy of this handle's variables on the stack, since the call to Elements.RemoveSwap deletes the handle.
	TStaticMeshDrawList* const LocalDrawList = StaticMeshDrawList;
	FDrawingPolicyLink* const LocalDrawingPolicyLink = &LocalDrawList->DrawingPolicySet(SetId);
	const INT LocalElementIndex = ElementIndex;

	checkSlow(LocalDrawingPolicyLink->SetId == SetId);

	// Unlink the mesh from this draw list.
	LocalDrawingPolicyLink->Elements(ElementIndex).Mesh->UnlinkDrawList(this);
	LocalDrawingPolicyLink->Elements(ElementIndex).Mesh = NULL;

	checkSlow(LocalDrawingPolicyLink->Elements.Num() == LocalDrawingPolicyLink->CompactElements.Num());

	// Remove this element from the drawing policy's element list.
	const DWORD LastDrawingPolicySize = LocalDrawingPolicyLink->GetSizeBytes();

	LocalDrawingPolicyLink->Elements.RemoveSwap(LocalElementIndex);
	LocalDrawingPolicyLink->CompactElements.RemoveSwap(LocalElementIndex);
	
	const DWORD CurrentDrawingPolicySize = LocalDrawingPolicyLink->GetSizeBytes();
	const DWORD DrawingPolicySizeDiff = LastDrawingPolicySize - CurrentDrawingPolicySize;

	LocalDrawList->TotalBytesUsed -= DrawingPolicySizeDiff;


	if (LocalElementIndex < LocalDrawingPolicyLink->Elements.Num())
	{
		// Fixup the element that was moved into the hole created by the removed element.
		LocalDrawingPolicyLink->Elements(LocalElementIndex).Handle->ElementIndex = LocalElementIndex;
	}

	// If this was the last element for the drawing policy, remove the drawing policy from the draw list.
	if(!LocalDrawingPolicyLink->Elements.Num())
	{
		LocalDrawList->TotalBytesUsed -= LocalDrawingPolicyLink->GetSizeBytes();

		LocalDrawList->OrderedDrawingPolicies.RemoveSingleItem(LocalDrawingPolicyLink->SetId);
		LocalDrawList->DrawingPolicySet.Remove(LocalDrawingPolicyLink->SetId);
	}
}

template<typename DrawingPolicyType>
void TStaticMeshDrawList<DrawingPolicyType>::DrawElement(
	const FViewInfo& View,
	const FElement& Element,
	const FDrawingPolicyLink* DrawingPolicyLink,
	UBOOL &bDrawnShared
	) const
{
	if(!bDrawnShared)
	{
		DrawingPolicyLink->DrawingPolicy.DrawShared(&View,DrawingPolicyLink->BoundShaderState);
		bDrawnShared = TRUE;
	}

	if( Element.Mesh->Elements.Num() == 1 )
	{
		for(INT bBackFace = 0;bBackFace < (DrawingPolicyLink->DrawingPolicy.NeedsBackfacePass() ? 2 : 1);bBackFace++)
		{
			INC_DWORD_STAT(STAT_StaticDrawListMeshDrawCalls);
			DrawingPolicyLink->DrawingPolicy.SetMeshRenderState(
				View,
				Element.Mesh->PrimitiveSceneInfo,
				*Element.Mesh,
				0,
				bBackFace,
				Element.PolicyData
				);

#if MOBILE
			RHISetMobileProgramInstance( Element.CachedProgramInstance );
#endif

			DrawingPolicyLink->DrawingPolicy.DrawMesh(*Element.Mesh, 0);

#if MOBILE
			Element.CachedProgramInstance = RHIGetMobileProgramInstance();
#endif
		}
	}
	else // Only for Landscape for now...
	{
		TArray<INT> BatchesToRender;
		BatchesToRender.Empty(Element.Mesh->Elements.Num());
		Element.Mesh->VertexFactory->GetStaticBatchElementVisibility(View,Element.Mesh,BatchesToRender);

		for (INT Index=0;Index<BatchesToRender.Num();Index++)
		{
			INT BatchElementIndex = BatchesToRender(Index);

			for(INT bBackFace = 0;bBackFace < (DrawingPolicyLink->DrawingPolicy.NeedsBackfacePass() ? 2 : 1);bBackFace++)
			{
				INC_DWORD_STAT(STAT_StaticDrawListMeshDrawCalls);

				DrawingPolicyLink->DrawingPolicy.SetMeshRenderState(
					View,
					Element.Mesh->PrimitiveSceneInfo,
					*Element.Mesh,
					BatchElementIndex,
					bBackFace,
					Element.PolicyData
					);
				DrawingPolicyLink->DrawingPolicy.DrawMesh(*Element.Mesh,BatchElementIndex);
			}
		}
	}
}

template<typename DrawingPolicyType>
void TStaticMeshDrawList<DrawingPolicyType>::AddMesh(
	FStaticMesh* Mesh,
	const ElementPolicyDataType& PolicyData,
	const DrawingPolicyType& InDrawingPolicy
	)
{
	// Check for an existing drawing policy matching the mesh's drawing policy.
	FDrawingPolicyLink* DrawingPolicyLink = DrawingPolicySet.Find(InDrawingPolicy);
	if(!DrawingPolicyLink)
	{
		// If no existing drawing policy matches the mesh, create a new one.
		const FSetElementId DrawingPolicyLinkId = DrawingPolicySet.Add(FDrawingPolicyLink(this,InDrawingPolicy));

		DrawingPolicyLink = &DrawingPolicySet(DrawingPolicyLinkId);
		DrawingPolicyLink->SetId = DrawingPolicyLinkId;

		TotalBytesUsed += DrawingPolicyLink->GetSizeBytes();

		// Insert the drawing policy into the ordered drawing policy list.
		INT MinIndex = 0;
		INT MaxIndex = OrderedDrawingPolicies.Num() - 1;
		while(MinIndex < MaxIndex)
		{
			INT PivotIndex = (MaxIndex + MinIndex) / 2;
			INT CompareResult = Compare(DrawingPolicySet(OrderedDrawingPolicies(PivotIndex)).DrawingPolicy,DrawingPolicyLink->DrawingPolicy);
			if(CompareResult < 0)
			{
				MinIndex = PivotIndex + 1;
			}
			else if(CompareResult > 0)
			{
				MaxIndex = PivotIndex;
			}
			else
			{
				MinIndex = MaxIndex = PivotIndex;
			}
		};
		check(MinIndex >= MaxIndex);
		OrderedDrawingPolicies.InsertItem(DrawingPolicyLinkId,MinIndex);
	}

	const INT ElementIndex = DrawingPolicyLink->Elements.Num();
	const SIZE_T PreviousElementsSize = DrawingPolicyLink->Elements.GetAllocatedSize();
	const SIZE_T PreviousCompactElementsSize = DrawingPolicyLink->CompactElements.GetAllocatedSize();
	FElement* Element = new(DrawingPolicyLink->Elements) FElement(Mesh, PolicyData, this, DrawingPolicyLink->SetId, ElementIndex);
	new(DrawingPolicyLink->CompactElements) FElementCompact(Mesh->Id);
	TotalBytesUsed += DrawingPolicyLink->Elements.GetAllocatedSize() - PreviousElementsSize + DrawingPolicyLink->CompactElements.GetAllocatedSize() - PreviousCompactElementsSize;
	Mesh->LinkDrawList(Element->Handle);
}

template<typename DrawingPolicyType>
void TStaticMeshDrawList<DrawingPolicyType>::RemoveAllMeshes()
{
#if STATS
	for (typename TArray<FSetElementId>::TConstIterator PolicyIt(OrderedDrawingPolicies); PolicyIt; ++PolicyIt)
	{
		const FDrawingPolicyLink* DrawingPolicyLink = &DrawingPolicySet(*PolicyIt);
		TotalBytesUsed -= DrawingPolicyLink->GetSizeBytes();
	}
#endif
	OrderedDrawingPolicies.Empty();
	DrawingPolicySet.Empty();
}

template<typename DrawingPolicyType>
TStaticMeshDrawList<DrawingPolicyType>::~TStaticMeshDrawList()
{
#if STATS
	for (typename TArray<FSetElementId>::TConstIterator PolicyIt(OrderedDrawingPolicies); PolicyIt; ++PolicyIt)
	{
		const FDrawingPolicyLink* DrawingPolicyLink = &DrawingPolicySet(*PolicyIt);
		TotalBytesUsed -= DrawingPolicyLink->GetSizeBytes();
	}
#endif
}

template<typename DrawingPolicyType>
UBOOL TStaticMeshDrawList<DrawingPolicyType>::DrawVisible(
	const FViewInfo& View,
	const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap
	) const
{
#if WITH_MOBILE_RHI
	// A map used to track the drawing policies which have anything to draw
	TMap<void*,FLOAT> DrawingPoliciesWithPendingDrawElements;
#endif
	UBOOL bDirty = FALSE;
	for(typename TArray<FSetElementId>::TConstIterator PolicyIt(OrderedDrawingPolicies); PolicyIt; ++PolicyIt)
	{
		FDrawingPolicyLink* DrawingPolicyLink = (FDrawingPolicyLink*)&DrawingPolicySet(*PolicyIt);
#if WITH_MOBILE_RHI
		// Also track the overall minimum distance to any single element in
		// the current drawing policy set, so we can coarse sort on that to
		// and fine sort within the set, to avoid switching between
		// materials when drawing later
		FLOAT MinElementDistance = MAX_FLT;
		DrawingPolicyLink->PendingDrawElements.Reset();
#endif
		UBOOL bDrawnShared = FALSE;
		PREFETCH(&DrawingPolicyLink->CompactElements(0));
		const INT NumElements = DrawingPolicyLink->Elements.Num();
		PREFETCH(&((&DrawingPolicyLink->CompactElements(0))->VisibilityBitReference));
		const FElementCompact* CompactElementPtr = &DrawingPolicyLink->CompactElements(0);
		for(INT ElementIndex = 0; ElementIndex < NumElements; ElementIndex++, CompactElementPtr++)
		{
			if(StaticMeshVisibilityMap.AccessCorrespondingBit(CompactElementPtr->VisibilityBitReference))
			{
				const FElement& Element = DrawingPolicyLink->Elements(ElementIndex);
#if STATS
				if( Element.Mesh->IsDecal() )
				{
					INC_DWORD_STAT_BY(STAT_DecalTriangles,Element.Mesh->GetNumPrimitives());
					INC_DWORD_STAT(STAT_DecalDrawCalls);
				}
				else
				{
					INC_DWORD_STAT_BY(STAT_StaticMeshTriangles,Element.Mesh->GetNumPrimitives());
				}
#endif
#if WITH_MOBILE_RHI
				// If we're on an architecture that sorted geometry is helpful, collect
				// the set of elements to draw, along with their distances from the eye,
				// rather than drawing here (we'll sort and draw them later)
				if (GUsingMobileRHI && !GMobileTiledRenderer)
				{
					const FVector ElementOrigin = Element.Mesh->PrimitiveSceneInfo->Bounds.Origin;
					const FVector ViewOrigin = FVector(View.ViewOrigin.X, View.ViewOrigin.Y, View.ViewOrigin.Z);
					const FVector ElementDistanceVector = ElementOrigin - ViewOrigin;
					FLOAT ElementDistance = ElementDistanceVector.Size();
					MinElementDistance = Min(MinElementDistance, ElementDistance);
					DrawingPolicyLink->PendingDrawElements.Set(ElementIndex, ElementDistance);
				}
				else
#endif
				{
					DrawElement(View, Element, DrawingPolicyLink, bDrawnShared);
				}
				bDirty = TRUE;
			}
		}

#if WITH_MOBILE_RHI
		// If the drawing policy had any elements to draw, add the policy to the pending set
		if (GUsingMobileRHI && !GMobileTiledRenderer)
		{
			if (DrawingPolicyLink->PendingDrawElements.Num() > 0)
			{
				// Perform the fine sort on the elements within the drawing policy,
				// then add it to the set of policies with pending draw calls
				DrawingPolicyLink->SortPendingDrawElements();
				DrawingPoliciesWithPendingDrawElements.Set(DrawingPolicyLink, MinElementDistance);
			}
		}
#endif
	}

#if WITH_MOBILE_RHI
	// Finally, sort and draw the visible elements
	if (GUsingMobileRHI && !GMobileTiledRenderer)
	{
		// Coarse sort the overall drawing policy groups
		DrawingPoliciesWithPendingDrawElements.ValueSort<COMPARE_CONSTREF_CLASS(FLOAT,SimpleFloatCompare)>();
		for(typename TMap<void*, FLOAT>::TIterator PendingPolicyIt(DrawingPoliciesWithPendingDrawElements); PendingPolicyIt; ++PendingPolicyIt)
		{
			// Iterate through the sorted elements and issue the actual draw calls
			UBOOL bDrawnShared = FALSE;
			FDrawingPolicyLink* DrawingPolicyLink = (FDrawingPolicyLink*)PendingPolicyIt.Key();
			for(typename TMap<INT, FLOAT>::TIterator PendingDrawElementsIt(DrawingPolicyLink->PendingDrawElements); PendingDrawElementsIt; ++PendingDrawElementsIt)
			{
				const FElement& Element = DrawingPolicyLink->Elements(PendingDrawElementsIt.Key());
				DrawElement(View, Element, DrawingPolicyLink, bDrawnShared);
			}
		}
	}
#endif

#if MOBILE
	RHISetMobileProgramInstance( NULL );
#endif
	return bDirty;
}

template<typename DrawingPolicyType>
INT TStaticMeshDrawList<DrawingPolicyType>::NumMeshes() const
{
	INT TotalMeshes=0;
	for(typename TArray<FSetElementId>::TConstIterator PolicyIt(OrderedDrawingPolicies); PolicyIt; ++PolicyIt)
	{
		const FDrawingPolicyLink* DrawingPolicyLink = &DrawingPolicySet(*PolicyIt);
		TotalMeshes += DrawingPolicyLink->Elements.Num();
	}
	return TotalMeshes;
}

#endif
