/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class LineBatchComponent extends PrimitiveComponent
	native
	noexport;

/** These mirror the C++ side properties. I'm making a class here so
    LineBatchComponent will get the defaultprops from the PrimitiveComponent base class */

// Virtual function table.
var	native const noexport pointer FPrimitiveDrawInterfaceVfTable;
// FPrimitiveDrawInterface FSceneView*
var native const noexport pointer FPrimitiveDrawInterfaceView;

var native transient const array<pointer> BatchedLines;	
var native transient const array<pointer> BatchedPoints;
var native transient const float DefaultLifeTime;
