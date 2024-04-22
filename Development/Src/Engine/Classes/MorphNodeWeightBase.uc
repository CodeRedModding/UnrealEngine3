/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MorphNodeWeightBase extends MorphNodeBase
	native(Anim)
	hidecategories(Object)
	abstract;

cpptext
{
	virtual void GetNodes(TArray<UMorphNodeBase*>& OutNodes);

	/**
	 * Draws this morph node in the AnimTreeEditor.
	 *
	 * @param	Canvas			The canvas to use.
	 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
	 */
	virtual void DrawMorphNode(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes);

	virtual FIntPoint GetConnectionLocation(INT ConnType, INT ConnIndex);
}

struct native MorphNodeConn
{
	/** Array of nodes attached to this connector. */
	var		array<MorphNodeBase>	ChildNodes;

	/** Name of this connector. */
	var		name					ConnName;

	/** Used in editor to draw line to this connector. */
	var		int						DrawY;
};

/** Array of connectors to which you can connect other MorphNodes. */
var		array<MorphNodeConn>	NodeConns;

defaultproperties
{
	CategoryDesc = "Weight"
}
