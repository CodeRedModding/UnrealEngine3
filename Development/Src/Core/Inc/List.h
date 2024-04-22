/*=============================================================================
	List.h: Dynamic list definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LIST_H__
#define __LIST_H__


// We disable this warning as we're intentionally expecting packing to affect the alignment
#pragma warning(push)
#pragma warning(disable : 4121) // alignment of a member was sensitive to packing


/**
 * Encapsulates a link in a single linked list with constant access time.
 */
template<class ElementType> class TLinkedList
{
public:

	/**
	 * Used to iterate over the elements of a linked list.
	 */
	class TIterator
	{
	private:
		// private class for safe bool conversion
		struct PrivateBooleanHelper { INT Value; };

	public:
		TIterator(TLinkedList* FirstLink)
		:	CurrentLink(FirstLink)
		{}

		/**
		 * Advances the iterator to the next element.
		 */
		void Next()
		{
			checkSlow(CurrentLink);
			CurrentLink = CurrentLink->NextLink;
		}

		TIterator& operator++()
		{
			Next();
			return *this;
		}

		TIterator operator++(int)
		{
			TIterator Tmp(*this);
			Next();
			return Tmp;
		}

		/** conversion to "bool" returning TRUE if the iterator is valid. */
		typedef bool PrivateBooleanType;
		operator PrivateBooleanType() const { return CurrentLink != NULL ? &PrivateBooleanHelper::Value : NULL; }
		bool operator !() const { return !operator PrivateBooleanType(); }

		// Accessors.
		ElementType& operator->() const
		{
			checkSlow(CurrentLink);
			return CurrentLink->Element;
		}
		ElementType& operator*() const
		{
			checkSlow(CurrentLink);
			return CurrentLink->Element;
		}

	private:
		TLinkedList* CurrentLink;
	};

	// Constructors.
	TLinkedList():
		NextLink(NULL),
		PrevLink(NULL)
	{}
	TLinkedList(const ElementType& InElement):
		Element(InElement),
		NextLink(NULL),
		PrevLink(NULL)
	{}

	/**
	 * Removes this element from the list in constant time.
	 *
	 * This function is safe to call even if the element is not linked.
	 */
	void Unlink()
	{
		if( NextLink )
		{
			NextLink->PrevLink = PrevLink;
		}
		if( PrevLink )
		{
			*PrevLink = NextLink;
		}
		// Make it safe to call Unlink again.
		NextLink = NULL;
		PrevLink = NULL;
	}

	/**
	 * Adds this element to a list, before the given element.
	 *
	 * @param Before	The link to insert this element before.
	 */
	void Link(TLinkedList*& Before)
	{
		if(Before)
		{
			Before->PrevLink = &NextLink;
		}
		NextLink = Before;
		PrevLink = &Before;
		Before = this;
	}

	/**
	 * Returns whether element is currently linked.
	 *
	 * @return TRUE if currently linked, FALSE othwerise
	 */
	UBOOL IsLinked()
	{
		return PrevLink != NULL;
	}

	// Accessors.
	ElementType& operator->()
	{
		return Element;
	}
	const ElementType& operator->() const
	{
		return Element;
	}
	ElementType& operator*()
	{
		return Element;
	}
	const ElementType& operator*() const
	{
		return Element;
	}
	TLinkedList* Next()
	{
		return NextLink;
	}
private:
	ElementType Element;
	TLinkedList* NextLink;
	TLinkedList** PrevLink;
};

/**
 * Double linked list.
 */
template<class ElementType> class TDoubleLinkedList
{
public:

	class TIterator;
	class TDoubleLinkedListNode
	{
	public:
		friend class TDoubleLinkedList;
		friend class TIterator;

		/** Constructor */
		TDoubleLinkedListNode( const ElementType& InValue )
		: Value(InValue), NextNode(NULL), PrevNode(NULL)
		{
		}

		const ElementType& GetValue() const
		{
			return Value;
		}

		ElementType& GetValue()
		{
			return Value;
		}

		TDoubleLinkedListNode* GetNextNode()
		{
			return NextNode;
		}

		TDoubleLinkedListNode* GetPrevNode()
		{
			return PrevNode;
		}

	protected:
		ElementType				Value;
		TDoubleLinkedListNode*	NextNode;
		TDoubleLinkedListNode*	PrevNode;
	};

	/**
	 * Used to iterate over the elements of a linked list.
	 */
	class TIterator
	{
	private:
		// private class for safe bool conversion
		struct PrivateBooleanHelper { INT Value; };

	public:

		TIterator(TDoubleLinkedListNode* StartingNode)
		: CurrentNode(StartingNode)
		{}

		/** conversion to "bool" returning TRUE if the iterator is valid. */
		typedef bool PrivateBooleanType;
		operator PrivateBooleanType() const { return CurrentNode != NULL ? &PrivateBooleanHelper::Value : NULL; }
		bool operator !() const { return !operator PrivateBooleanType(); }

		TIterator& operator++()
		{
			checkSlow(CurrentNode);
			CurrentNode = CurrentNode->NextNode;
			return *this;
		}

		TIterator operator++(int)
		{
			TIterator Tmp(*this);
			++(*this);
			return Tmp;
		}

		TIterator& operator--()
		{
			checkSlow(CurrentNode);
			CurrentNode = CurrentNode->PrevNode;
			return *this;
		}

		TIterator operator--(int)
		{
			TIterator Tmp(*this);
			--(*this);
			return Tmp;
		}

		// Accessors.
		ElementType& operator->() const
		{
			checkSlow(CurrentNode);
			return CurrentNode->GetValue();
		}
		ElementType& operator*() const
		{
			checkSlow(CurrentNode);
			return CurrentNode->GetValue();
		}
		TDoubleLinkedListNode* GetNode() const
		{
			checkSlow(CurrentNode);
			return CurrentNode;
		}

	private:

		TDoubleLinkedListNode*	CurrentNode;
	};

	/** Constructors. */
	TDoubleLinkedList() : HeadNode(NULL), TailNode(NULL), ListSize(0)
	{}

	/** Destructor */
	virtual ~TDoubleLinkedList()
	{
		Clear();
	}

	// Adding/Removing methods
	/**
	 * Add the specified value to the beginning of the list, making that value the new head of the list.
	 *
	 * @param	InElement	the value to add to the list.
	 *
	 * @return	whether the node was successfully added into the list
	 */
	UBOOL AddHead( const ElementType& InElement )
	{
		TDoubleLinkedListNode* NewNode = new TDoubleLinkedListNode(InElement);
		if ( NewNode == NULL )
		{
			return FALSE;
		}

		// have an existing head node - change the head node to point to this one
		if ( HeadNode != NULL )
		{
			NewNode->NextNode = HeadNode;
			HeadNode->PrevNode = NewNode;
			HeadNode = NewNode;
		}
		else
		{
			HeadNode = TailNode = NewNode;
		}

		SetListSize(ListSize + 1);
		return TRUE;
	}

	/**
	 * Append the specified value to the end of the list
	 *
	 * @param	InElement	the value to add to the list.
	 *
	 * @return	whether the node was successfully added into the list
	 */
	UBOOL AddTail( const ElementType& InElement )
	{
		TDoubleLinkedListNode* NewNode = new TDoubleLinkedListNode(InElement);
		if ( NewNode == NULL )
		{
			return FALSE;
		}

		if ( TailNode != NULL )
		{
			TailNode->NextNode = NewNode;
			NewNode->PrevNode = TailNode;
			TailNode = NewNode;
		}
		else
		{
			HeadNode = TailNode = NewNode;
		}

		SetListSize(ListSize + 1);
		return TRUE;
	}

	/**
	 * Insert the specified value into the list at an arbitrary point.
	 *
	 * @param	InElement			the value to insert into the list
	 * @param	NodeToInsertBefore	the new node will be inserted into the list at the current location of this node
	 *								if NULL, the new node will become the new head of the list
	 *
	 * @return	whether the node was successfully added into the list
	 */
	UBOOL InsertNode( const ElementType& InElement, TDoubleLinkedListNode* NodeToInsertBefore=NULL )
	{
		if ( NodeToInsertBefore == NULL || NodeToInsertBefore == HeadNode )
		{
			return AddHead(InElement);
		}

		TDoubleLinkedListNode* NewNode = new TDoubleLinkedListNode(InElement);
		if ( NewNode == NULL )
		{
			return FALSE;
		}

		NewNode->PrevNode = NodeToInsertBefore->PrevNode;
		NewNode->NextNode = NodeToInsertBefore;

		NodeToInsertBefore->PrevNode->NextNode = NewNode;
		NodeToInsertBefore->PrevNode = NewNode;

		SetListSize(ListSize + 1);
		return TRUE;
	}

	/**
	 * Remove the node corresponding to InElement
	 *
	 * @param	InElement	the value to remove from the list
	 */
	void RemoveNode( const ElementType& InElement )
	{
		TDoubleLinkedListNode* ExistingNode = FindNode(InElement);
		RemoveNode(ExistingNode);
	}

	/**
	 * Removes the node specified.
	 */
	void RemoveNode( TDoubleLinkedListNode* NodeToRemove )
	{
		if ( NodeToRemove != NULL )
		{
			// if we only have one node, just call Clear() so that we don't have to do lots of extra checks in the code below
			if ( Num() == 1 )
			{
				checkSlow(NodeToRemove == HeadNode);
				Clear();
				return;
			}

			if ( NodeToRemove == HeadNode )
			{
				HeadNode = HeadNode->NextNode;
				HeadNode->PrevNode = NULL;
			}

			else if ( NodeToRemove == TailNode )
			{
				TailNode = TailNode->PrevNode;
				TailNode->NextNode = NULL;
			}
			else
			{
				NodeToRemove->NextNode->PrevNode = NodeToRemove->PrevNode;
				NodeToRemove->PrevNode->NextNode = NodeToRemove->NextNode;
			}

			delete NodeToRemove;
			NodeToRemove = NULL;
			SetListSize(ListSize - 1);
		}
	}

	/**
	 * Removes all nodes from the list.
	 */
	void Clear()
	{
		TDoubleLinkedListNode* pNode;
		while ( HeadNode != NULL )
		{
			pNode = HeadNode->NextNode;
			delete HeadNode;
			HeadNode = pNode;
		}

		HeadNode = TailNode = NULL;
		SetListSize(0);
	}

	// Accessors.

	/**
	 * Returns the node at the head of the list
	 */
	TDoubleLinkedListNode* GetHead() const
	{
		return HeadNode;
	}

	/**
	 * Returns the node at the end of the list
	 */
	TDoubleLinkedListNode* GetTail() const
	{
		return TailNode;
	}

	/**
	 * Finds the node corresponding to the value specified
	 *
	 * @param	InElement	the value to find
	 *
	 * @return	a pointer to the node that contains the value specified, or NULL of the value couldn't be found
	 */
	TDoubleLinkedListNode* FindNode( const ElementType& InElement )
	{
		TDoubleLinkedListNode* pNode = HeadNode;
		while ( pNode != NULL )
		{
			if ( pNode->GetValue() == InElement )
			{
				break;
			}

			pNode = pNode->NextNode;
		}

		return pNode;
	}

	/**
	 * Returns the size of the list.
	 */
	INT Num()
	{
		return ListSize;
	}

protected:

	/**
	 * Updates the size reported by Num().  Child classes can use this function to conveniently
	 * hook into list additions/removals.
	 *
	 * @param	NewListSize		the new size for this list
	 */
	virtual void SetListSize( INT NewListSize )
	{
		ListSize = NewListSize;
	}

private:
	TDoubleLinkedListNode* HeadNode;
	TDoubleLinkedListNode* TailNode;
	INT ListSize;
};

/*----------------------------------------------------------------------------
	TList.
----------------------------------------------------------------------------*/

//
// Simple single-linked list template.
//
template <class ElementType> class TList
{
public:

	ElementType			Element;
	TList<ElementType>*	Next;

	// Constructor.

	TList(const ElementType &InElement, TList<ElementType>* InNext = NULL)
	{
		Element = InElement;
		Next = InNext;
	}
};


#pragma warning(pop)


#endif
