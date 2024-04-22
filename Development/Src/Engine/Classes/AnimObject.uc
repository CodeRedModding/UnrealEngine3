/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimObject extends Object
	native(Anim)
	hidecategories(Object)
	abstract;

/** For editor use. */
var	editoronly int									DrawWidth;

/** for editor use  */
var editoronly int									DrawHeight;

/** for editor use. */
var	editoronly int									NodePosX;

/** For editor use. */
var editoronly int									NodePosY;

/** for editor use. */
var editoronly int									OutDrawY;

/**
 * Editor category for this object.  Determines which animtree submenu this object
 * should be placed in
 */
var editoronly string 					CategoryDesc;

/** SkeletalMeshComponent owner */
var const transient	duplicatetransient SkeletalMeshComponent SkelComponent;

cpptext
{
	virtual UAnimNode * GetAnimNode() { return NULL;}
	virtual UMorphNodeBase * GetMorphNodeBase() { return NULL;}
	virtual USkelControlBase * GetSkelControlBase() { return NULL; }

    /**
	 * Draws this node in the AnimTreeEditor.
	 *
	 * @param	Canvas			The canvas to use.
	 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
	 * @param	bShowWeight		If TRUE, show the global percentage weight of this node, if applicable.
	 */
	virtual void DrawNode(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes, UBOOL bShowWeight) {}
	/** Called after (copy/)pasted - reset values or re-link if needed**/
	virtual void OnPaste() {};

#if WITH_EDITORONLY_DATA
	/** Calculate the bounding box of this sequence object. For use by Kismet. */
	FIntRect GetObjBoundingBox()
	{
		return FIntRect(NodePosX, NodePosY, NodePosX + DrawWidth, NodePosY + DrawHeight);
	}
#endif
};

DefaultProperties
{

}
