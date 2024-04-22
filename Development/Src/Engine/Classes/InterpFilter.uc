/** 
 * InterpFilter.uc: Filter class for filtering matinee groups.  
 * By default no groups are filtered.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class InterpFilter extends Object
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

/** Caption for this filter. */
var string Caption;
