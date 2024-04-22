/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SCOPEDPROPERTYCHANGE_H__
#define __SCOPEDPROPERTYCHANGE_H__

/**
 * This class is used for encapsulating tasks performed when the value of a property is changed through an editor property window.
 * It fires the Pre/PostEditChange notifications and manages an archetype propagation archive.
 */
class FScopedPropertyChange
{
public:
	/**
	 * Constructor
	 *
	 * @param	InModifiedItem		the property window item corresponding to the property that was modified.
	 * @param	InModifiedProperty	alternate property to use when calling Pre/PostEditChange; if not specified,
	 *								InModifiedItem->Property is used
	 * @param	bInPreventPropertyWindowRebuild
	 *								if TRUE, will prevent CB_ObjectPropertyChanged from calling Rebuild() on the WxPropertyWindow
	 *								which contains InModifiedItem.  Useful if InModifiedItem is handling the property value change
	 *								notification itself, as Rebuild() would cause InModifiedItem to be deleted before we go out of scope.
	 */
	FScopedPropertyChange( class WxPropertyControl* InModifiedItem, class UProperty* InModifiedProperty=NULL, UBOOL bInPreventPropertyWindowRebuild=FALSE );

	/** Destructor */
	~FScopedPropertyChange();

	/**
	 * Creates the archetype propagation archive and send the PreEditChange notifications.
	 */
	void BeginEdit();

	/**
	 * Sends the PostEditChange notifications and deletes the archetype propagation archive.
	 */
	void FinishEdit();

private:
	/**
	 * An archive used for propagating property value changes to instances of archetypes.
	 */
	class FArchetypePropagationArc* PropagationArchive;

	/**
	 * Stores a reference to the previous value of GMemoryArchive so that it can be restored when FinishEdit is called.
	 */
	class FReloadObjectArc* PreviousPropagationArchive;

	/**
	 * The property window item which contains the reference to the property which is being modified.
	 */
	class WxPropertyControl* ModifiedItem;

	/**
	 * The property to use when calling Pre/PostEditChange on ModifiedItem;  typically the value of ModifiedItem->Property.
	 */
	class UProperty* ModifiedProperty;

	/**
	 * Prevents CB_ObjectPropertyChanged from calling Rebuild() on the WxPropertyWindow which contains ModifiedItem.
	 */
	UBOOL bPreventPropertyWindowRebuild;
};


#endif	//	__SCOPEDPROPERTYCHANGE_H__

