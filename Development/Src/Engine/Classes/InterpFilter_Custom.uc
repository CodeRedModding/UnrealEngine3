/** 
 * InterpFilter_Custom.uc: Filter class for filtering matinee groups.  
 * Used by the matinee editor to let users organize tracks/groups.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class InterpFilter_Custom extends InterpFilter
	native(Interpolation);

cpptext
{
	/** 
	 * Given a interpdata object, updates visibility of groups and tracks based on the filter settings
	 *
	 * @param InData			Data to filter.
	 */
	virtual void FilterData(class USeqAct_Interp* InData);
}

/** Which groups are included in this filter. */
var editoronly	array<InterpGroup>	GroupsToInclude;