/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _EDITOR_CLASS_HIERARCHY_H_
#define _EDITOR_CLASS_HIERARCHY_H_

#ifdef _MSC_VER
	#pragma once
#endif


class FEditorClassHierarchy
{
public:
	/**
	 * Builds the tree from the class manifest
	 * @return - whether the class hierarchy was successfully built
	 */
	UBOOL Init (void);

	/**
	 * TRUE, if the manifest file loaded classes successfully
	 */
	UBOOL WasLoadedSuccessfully(void) const;

	/**
	 * Gets the direct children of the class
	 * @param InClassIndex - the index of the class in question
	 * @param OutIndexArray - the array to fill in with child indices
	 */
	void GetChildrenOfClass(const INT InClassIndex, TArray<INT> &OutIndexArray);

	/** 
	 * Adds the node and all children recursively to the list of OutAllClasses 
	 * @param InClassIndex - The node in the hierarchy to start with
	 * @param OutAllClasses - The list of classes generated recursively
	 */
	void GetAllClasses(const INT InClassIndex, OUT TArray<UClass*> OutAllClasses);

	/**
	 * Gets the list of classes that are supported factory classes
	 * @param OutIndexArray - the array to fill in with factory class indices
	 */
	void GetFactoryClasses(TArray<INT> &OutIndexArray);

	/**
	 * Find the class index that matches the requested name
	 * @param InClassName - Name of the class to find
	 * @return - The index of the desired class
	 */
	INT Find(const FString& InClassName);

	/**
	 * returns the name of the class in the tree
	 * @param InClassIndex - the index of the class in question
	 */
	FString GetClassName(const INT InClassIndex);
	/**
	 * returns the UClass of the class in the tree
	 * @param InClassIndex - the index of the class in question
	 */
	UClass* GetClass(const INT InClassIndex);

	/**
	 * Returns the class index of the provided index's parent class
	 *
	 * @param	InClassIndex	Class index to find the parent of
	 *
	 * @return	Class index of the parent class of the provided index, if any; INDEX_NONE if not
	 */
	INT GetParentIndex(INT InClassIndex) const;
	
	/** 
	 * Returns a list of class group names for the provided class index 
	 *
	 * @param InClassIndex	The class index to find group names for
	 * @param OutGroups		The list of class groups found.
	 */
	void GetClassGroupNames( INT InClassIndex, TArray<FString>& OutGroups ) const;

	/**
	 * returns if the class is hidden or not
	 * @param InClassIndex - the index of the class in question
	 */
	UBOOL IsHidden(const INT InClassIndex) const;
	/**
	 * returns if the class is placeable or not
	 * @param InClassIndex - the index of the class in question
	 */
	UBOOL IsPlaceable(const INT InClassIndex) const;
	/**
	 * returns if the class is abstract or not
	 * @param InClassIndex - the index of the class in question
	 */
	UBOOL IsAbstract(const INT InClassIndex) const;
	/**
	 * returns if the class is a brush or not
	 * @param InClassIndex - the index of the class in question
	 */
	UBOOL IsBrush(const INT InClassIndex);
	/**
	 * Returns if the class is visible 
	 * @param InClassIndex - the index of the class in question
	 * @param bInPlaceable - if TRUE, return number of placeable children, otherwise returns all children
	 * @return - Number of children
	 */
	UBOOL IsClassVisible(const INT InClassIndex, const UBOOL bInPlaceableOnly);

	/**
	 * Returns if the class has any children (placeable or all)
	 * @param InClassIndex - the index of the class in question
	 * @param bInPlaceable - if TRUE, return if has placeable children, otherwise returns if has any children
	 * @return - Whether this node has children (recursively) that are visible
	 */
	UBOOL HasChildren(const INT InClassIndex, const UBOOL bInPlaceableOnly);

};

#endif