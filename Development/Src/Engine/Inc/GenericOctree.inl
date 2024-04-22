/*=============================================================================
	GenericOctree.inl: Generic octree implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __GENERICOCTREE_INL__
#define __GENERICOCTREE_INL__

FORCEINLINE UBOOL FOctreeChildNodeSubset::Contains(FOctreeChildNodeRef ChildRef) const
{
	// This subset contains the child if it has all the bits set that are set for the subset containing only the child node.
	const FOctreeChildNodeSubset ChildSubset(ChildRef);
	return (ChildBits & ChildSubset.ChildBits) == ChildSubset.ChildBits;
}

FORCEINLINE FOctreeChildNodeSubset FOctreeNodeContext::GetIntersectingChildren(const FBoxCenterAndExtent& QueryBounds) const
{
	FOctreeChildNodeSubset Result;

	// Load the query bounding box values as VectorRegisters.
	const VectorRegister QueryBoundsCenter = VectorLoadAligned(&QueryBounds.Center);
	const VectorRegister QueryBoundsExtent = VectorLoadAligned(&QueryBounds.Extent);
	const VectorRegister QueryBoundsMax = VectorAdd(QueryBoundsCenter,QueryBoundsExtent);
	const VectorRegister QueryBoundsMin = VectorSubtract(QueryBoundsCenter,QueryBoundsExtent);

	// Compute the bounds of the node's children.
	const VectorRegister BoundsCenter = VectorLoadAligned(&Bounds.Center);
	const VectorRegister BoundsExtent = VectorLoadAligned(&Bounds.Extent);
	const VectorRegister PositiveChildBoundsMin = VectorSubtract(
		VectorAdd(BoundsCenter,VectorLoadFloat1(&ChildCenterOffset)),
		VectorLoadFloat1(&ChildExtent)
		);
	const VectorRegister NegativeChildBoundsMax = VectorAdd(
		VectorSubtract(BoundsCenter,VectorLoadFloat1(&ChildCenterOffset)),
		VectorLoadFloat1(&ChildExtent)
		);

	// Intersect the query bounds with the node's children's bounds.
	Result.bPositiveX = VectorAnyGreaterThan(VectorReplicate(QueryBoundsMax,0),VectorReplicate(PositiveChildBoundsMin,0)) != FALSE;
	Result.bPositiveY = VectorAnyGreaterThan(VectorReplicate(QueryBoundsMax,1),VectorReplicate(PositiveChildBoundsMin,1)) != FALSE;
	Result.bPositiveZ = VectorAnyGreaterThan(VectorReplicate(QueryBoundsMax,2),VectorReplicate(PositiveChildBoundsMin,2)) != FALSE;
	Result.bNegativeX = VectorAnyGreaterThan(VectorReplicate(QueryBoundsMin,0),VectorReplicate(NegativeChildBoundsMax,0)) == FALSE;
	Result.bNegativeY = VectorAnyGreaterThan(VectorReplicate(QueryBoundsMin,1),VectorReplicate(NegativeChildBoundsMax,1)) == FALSE;
	Result.bNegativeZ = VectorAnyGreaterThan(VectorReplicate(QueryBoundsMin,2),VectorReplicate(NegativeChildBoundsMax,2)) == FALSE;
	return Result;
}

FORCEINLINE FOctreeChildNodeRef FOctreeNodeContext::GetContainingChild(const FBoxCenterAndExtent& QueryBounds) const
{
	FOctreeChildNodeRef Result;

	// Load the query bounding box values as VectorRegisters.
	const VectorRegister QueryBoundsCenter = VectorLoadAligned(&QueryBounds.Center);
	const VectorRegister QueryBoundsExtent = VectorLoadAligned(&QueryBounds.Extent);

	// Compute the bounds of the node's children.
	const VectorRegister BoundsCenter = VectorLoadAligned(&Bounds.Center);
	const VectorRegister ChildCenterOffsetVector = VectorLoadFloat1(&ChildCenterOffset);
	const VectorRegister NegativeCenterDifference = VectorSubtract(QueryBoundsCenter,VectorSubtract(BoundsCenter,ChildCenterOffsetVector));
	const VectorRegister PositiveCenterDifference = VectorSubtract(VectorAdd(BoundsCenter,ChildCenterOffsetVector),QueryBoundsCenter);

	// If the query bounds isn't entirely inside the bounding box of the child it's closest to, it's not contained by any of the child nodes.
	const VectorRegister MinDifference = VectorMin(PositiveCenterDifference,NegativeCenterDifference);
	if(VectorAnyGreaterThan(VectorAdd(QueryBoundsExtent,MinDifference),VectorLoadFloat1(&ChildExtent)))
	{
		Result.bNULL = TRUE;
	}
	else
	{
		// Return the child node that the query is closest to as the containing child.
		Result.X = VectorAnyGreaterThan(VectorReplicate(QueryBoundsCenter,0),VectorReplicate(BoundsCenter,0)) != FALSE;
		Result.Y = VectorAnyGreaterThan(VectorReplicate(QueryBoundsCenter,1),VectorReplicate(BoundsCenter,1)) != FALSE;
		Result.Z = VectorAnyGreaterThan(VectorReplicate(QueryBoundsCenter,2),VectorReplicate(BoundsCenter,2)) != FALSE;
	}

	return Result;
}

template<typename ElementType,typename OctreeSemantics>
void TOctree<ElementType,OctreeSemantics>::AddElement(typename TContainerTraits<ElementType>::ConstInitType Element)
{
	AddElementToNode(Element,RootNode,RootNodeContext);
}
															
template<typename ElementType,typename OctreeSemantics>
void TOctree<ElementType,OctreeSemantics>::AddElementToNode(
	typename TContainerTraits<ElementType>::ConstInitType Element,
	const FNode& InNode,
	const FOctreeNodeContext& InContext
	)
{
	const FBoxCenterAndExtent ElementBounds(OctreeSemantics::GetBoundingBox(Element));

	for(TConstIterator<TInlineAllocator<1> > NodeIt(InNode,InContext);NodeIt.HasPendingNodes();NodeIt.Advance())
	{
		const FNode& Node = NodeIt.GetCurrentNode();
		const FOctreeNodeContext& Context = NodeIt.GetCurrentContext();
		const UBOOL bIsLeaf = Node.IsLeaf();

		UBOOL bAddElementToThisNode = FALSE;

		// Increment the number of elements included in this node and its children.
		Node.InclusiveNumElements++;

		if(bIsLeaf)
		{
			// If this is a leaf, check if adding this element would turn it into a node by overflowing its element list.
			if(Node.Elements.Num() + 1 > OctreeSemantics::MaxElementsPerLeaf && Context.Bounds.Extent.X > MinLeafExtent)
			{
				// Copy the leaf's elements, remove them from the leaf, and turn it into a node.
				ElementArrayType ChildElements;
				Exchange(ChildElements,Node.Elements);
				TotalSizeBytes -= ChildElements.Num() * sizeof(ElementType);
				Node.InclusiveNumElements = 0;

				// Allow elements to be added to children of this node.
				Node.bIsLeaf = FALSE;

				// Re-add all of the node's child elements, potentially creating children of this node for them.
				for(ElementConstIt ElementIt(ChildElements);ElementIt;++ElementIt)
				{
					AddElementToNode(*ElementIt,Node,Context);
				}

				// Add the element to this node.
				AddElementToNode(Element,Node,Context);
				return;
			}
			else
			{
				// If the leaf has room for the new element, simply add it to the list.
				bAddElementToThisNode = TRUE;
			}
		}
		else
		{
			// If this isn't a leaf, find a child that entirely contains the element.
			const FOctreeChildNodeRef ChildRef = Context.GetContainingChild(ElementBounds);	
			if(ChildRef.IsNULL())
			{
				// If none of the children completely contain the element, add it to this node directly.
				bAddElementToThisNode = TRUE;
			}
			else
			{
				// Create the child node if it hasn't been created yet.
				if(!Node.Children[ChildRef.Index])
				{
					Node.Children[ChildRef.Index] = new typename TOctree<ElementType,OctreeSemantics>::FNode(&Node);
					TotalSizeBytes += sizeof(*Node.Children[ChildRef.Index]);
				}

				// Push the node onto the stack to visit.
				NodeIt.PushChild(ChildRef);
			}
		}

		if(bAddElementToThisNode)
		{
			// Add the element to this node.
			new(Node.Elements) ElementType(Element);

			TotalSizeBytes += sizeof(ElementType);
			
			// Set the element's ID.
			OctreeSemantics::SetElementId(Element,FOctreeElementId(&Node,Node.Elements.Num() - 1));
			return;
		}
	}

	appErrorf(
		TEXT("Failed to find an octree node for an element with bounds (%f,%f,%f) +/- (%f,%f,%f)!"),
		ElementBounds.Center.X,
		ElementBounds.Center.Y,
		ElementBounds.Center.Z,
		ElementBounds.Extent.X,
		ElementBounds.Extent.Y,
		ElementBounds.Extent.Z
		);
}

template<typename ElementType,typename OctreeSemantics>
void TOctree<ElementType,OctreeSemantics>::RemoveElement(FOctreeElementId ElementId)
{
	check(ElementId.IsValidId()); 

	FNode* ElementIdNode = (FNode*)ElementId.Node;

	// Remove the element from the node's element list.
	ElementIdNode->Elements.RemoveSwap(ElementId.ElementIndex);

	TotalSizeBytes -= sizeof(ElementType);

	if(ElementId.ElementIndex < ElementIdNode->Elements.Num())
	{
		// Update the external element id for the element that was swapped into the vacated element index.
		OctreeSemantics::SetElementId(ElementIdNode->Elements(ElementId.ElementIndex),ElementId);
	}

	// Update the inclusive element counts for the nodes between the element and the root node,
	// and find the largest node that is small enough to collapse.
	const FNode* CollapseNode = NULL;
	for(const FNode* Node = ElementIdNode;Node;Node = Node->Parent)
	{
		--Node->InclusiveNumElements;
		if(Node->InclusiveNumElements < OctreeSemantics::MinInclusiveElementsPerNode)
		{
			CollapseNode = Node;
		}
	}

	// Collapse the largest node that was pushed below the threshold for collapse by the removal.
	if(CollapseNode)
	{
		// Gather the elements contained in this node and its children.
		TArray<ElementType,TInlineAllocator<OctreeSemantics::MaxElementsPerLeaf> > CollapsedChildElements;
		CollapsedChildElements.Empty(CollapseNode->InclusiveNumElements);
		for(TConstIterator<> ChildNodeIt(*CollapseNode,RootNodeContext);ChildNodeIt.HasPendingNodes();ChildNodeIt.Advance())
		{
			const FNode& ChildNode = ChildNodeIt.GetCurrentNode();

			// Add the child's elements to the collapsed element list.
			for(ElementConstIt ElementIt(ChildNode.Elements);ElementIt;++ElementIt)
			{
				const INT NewElementIndex = CollapsedChildElements.AddItem(*ElementIt);

				// Update the external element id for the element that's being collapsed.
				OctreeSemantics::SetElementId(*ElementIt,FOctreeElementId(CollapseNode,NewElementIndex));
			}

			// Recursively visit all child nodes.
			FOREACH_OCTREE_CHILD_NODE(ChildRef)
			{
				if(ChildNode.HasChild(ChildRef))
				{
					ChildNodeIt.PushChild(ChildRef);
				}
			}
		}

		// Replace the node's elements with the collapsed element list.
		Exchange(CollapseNode->Elements,CollapsedChildElements);

		// Mark the node as a leaf.
		CollapseNode->bIsLeaf = TRUE;

		// Free the child nodes.
		FOREACH_OCTREE_CHILD_NODE(ChildRef)
		{
			if (CollapseNode->Children[ChildRef.Index])
			{
				TotalSizeBytes -= sizeof(*CollapseNode->Children[ChildRef.Index]);
			}

			delete CollapseNode->Children[ChildRef.Index];
			CollapseNode->Children[ChildRef.Index] = NULL;
		}
	}
}

template<typename ElementType,typename OctreeSemantics>
ElementType& TOctree<ElementType,OctreeSemantics>::GetElementById(FOctreeElementId ElementId)
{
	check(ElementId.IsValidId());
	FNode* ElementIdNode = (FNode*)ElementId.Node;
	return ElementIdNode->Elements(ElementId.ElementIndex);
}

template<typename ElementType,typename OctreeSemantics>
void TOctree<ElementType,OctreeSemantics>::DumpStats()
{
	INT NumNodes = 0;
	INT NumLeaves = 0;
	INT NumElements = 0;
	INT MaxElementsPerNode = 0;
	TArray<INT> NodeElementDistribution;

	for(TConstIterator<> NodeIt(*this);NodeIt.HasPendingNodes();NodeIt.Advance())
	{
		const FNode& CurrentNode = NodeIt.GetCurrentNode();
		const INT CurrentNodeElementCount = CurrentNode.GetElementCount();

		NumNodes++;
		if(CurrentNode.IsLeaf())
		{
			NumLeaves++;
		}

		NumElements += CurrentNodeElementCount;
		MaxElementsPerNode = Max(MaxElementsPerNode,CurrentNodeElementCount);

		if( CurrentNodeElementCount >= NodeElementDistribution.Num() )
		{
			NodeElementDistribution.AddZeroed( CurrentNodeElementCount - NodeElementDistribution.Num() + 1 );
		}
		NodeElementDistribution(CurrentNodeElementCount)++;

		FOREACH_OCTREE_CHILD_NODE(ChildRef)
		{
			if(CurrentNode.HasChild(ChildRef))
			{
				NodeIt.PushChild(ChildRef);
			}
		}
	}

	debugf(TEXT("Octree overview:"));
	debugf(TEXT("\t%i nodes"),NumNodes);
	debugf(TEXT("\t%i leaves"),NumLeaves);
	debugf(TEXT("\t%i elements"),NumElements);
	debugf(TEXT("\t%i >= elements per node"),MaxElementsPerNode);
	debugf(TEXT("Octree node element distribution:"));
	for( INT i=0; i<NodeElementDistribution.Num(); i++ )
	{
		if( NodeElementDistribution(i) > 0 )
		{
			debugf(TEXT("\tElements: %3i, Nodes: %3i"),i,NodeElementDistribution(i));
		}
	}
}

template<typename ElementType,typename OctreeSemantics>
TOctree<ElementType,OctreeSemantics>::TOctree(const FVector& InOrigin,FLOAT InExtent)
:	RootNode(NULL)
,	RootNodeContext(FBoxCenterAndExtent(InOrigin,FVector(InExtent,InExtent,InExtent)),0,0)
,	MinLeafExtent(InExtent * appPow((1.0f + 1.0f / (FLOAT)FOctreeNodeContext::LoosenessDenominator) / 2.0f,OctreeSemantics::MaxNodeDepth))
,	TotalSizeBytes(0)
{
}

#endif
