/**********************************************************************

Filename    :   GFxEngine.uc
Content     :   GFx Engine class

Copyright   :   (c) 2006-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :   Since 'ucc' will prefix all class names with 'U'
                there is not conflict with GFx file / class naming.

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING 
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/


class GFxEngine extends Object
	native;

struct native GCReference
{
	var const Object m_object;
	var int m_count;
	var int m_statid;
};

/** adding buffer for storing texture references created by ui renderer */
var private transient array< GCReference > GCReferences;
var private transient int RefCount;

cpptext
{
    UGFxEngine();
    void FinishDestroy();
    void Release();

#if WITH_GFx
	/** Texture GC Management */
	// Returns true if object reference was added, false otherwise
	UBOOL AddGCReferenceFor( const UObject* const pObjectToBeAdded, INT statid);

	// Returns true if object reference is found (and removed), false otherwise
	UBOOL RemoveGCReferenceFor( const UObject* const pObjectToBeRemoved );

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize();

	/**
	 * Dumps memory information about the GFX system
	 */ 
	static void DumpGFXMemoryStats(FOutputDevice& Ar);
#endif	// WITH_GFx
}

defaultproperties
{
    RefCount=1
}
