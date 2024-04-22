/*=============================================================================
	UnPrefab.h: Prefab Support
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNPREFAB_H__
#define __UNPREFAB_H__

/**
 *	The complex part about Prefabs is handling the things that always differ between instances and the Prefab defaults -
 *	that is, their position and references within the Prefab. These will always be saved because they always differ from the Prefab
 *	defaults, so it is not possible to propagate changes to the base Prefab automatically to instances of these objects.
 *	Changing properties like draw scale would be automatically propagated when the map is reloaded, because we are changing
 *	the default properties of the object. For positions and pointers though, we need a more complex approach. What we do is
 *	serialize the difference between a PrefanInstance and its prefab defaults at the time. To do this we make the instance look as much
 *	like the defaults as we can, by changing references from instance pointers to archetype pointers, and changing locations to
 *	'relative to prefab' locations, and then serializing it to a PrefabUpdateArc. This is then the information that the LD has changed from defaults. 
 *	Then when we detect that the Prefab has changed when we open a map, we set the objects in the PrefabInstance back to the new Prefab defaults, and then 
 *	load the archive in the PrefabInstance over the top to update it to the latest version, but keeping any changes the LD has made to the instance.
 */

/** 
 *	Archive class used for saving/loading differences between an a PrefabInstance and the Prefab defaults.
 *	This is very similar to FReloadObjectArc.
 *	Because there is no Linker when we use this archive, we store an array of the various objects that we serialize
 *	and store an index to them in the BYTE array. Also, we store names as an array of strings and an index in that array.
 */
class FPrefabUpdateArc : public FReloadObjectArc
{
	/** Allow PrefabInstance to access all our personals */
	friend class APrefabInstance;

public:

	/** Constructor */
	FPrefabUpdateArc();

	void SetPersistant(UBOOL InPersist)
	{
		ArIsPersistent = InPersist;
	}

	FArchive& operator<<( class FName& Name );

protected:

	/** Names that were serialized.*/
	TArray<FString>		SavedNames;
};



#endif	// __UNPREFAB_H__
