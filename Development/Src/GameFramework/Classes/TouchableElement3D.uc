/**
* TouchableElement3D
* Interface for any object in the 3D world that can trigger a kismet action
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
interface TouchableElement3D;

/** Handle being clicked by the user */
function HandleClick();

/** Handle being double clicked by the user */
function HandleDoubleClick();

/** Handle a touch moving over this object, and not necessarily tapping or releasing on it */
function HandleDragOver();