/**
 * This class is used for rendering a box around a group of kismet objects in the kismet editor, for organization
 * and clarity.  Corresponds to a "comment box" in the kismet editor.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AnimNodeFrame extends AnimObject
	native(Anim);

cpptext
{
#if WITH_EDITOR
	/** Draws the box part of the comment (including handle) */
	void DrawFrameBox(FCanvas* Canvas, UBOOL bSelected);

	/**
	 * Draws this node in the AnimTreeEditor.
	 *
	 * @param	Canvas			The canvas to use.
	 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
	 * @param	bShowWeight		If TRUE, show the global percentage weight of this node, if applicable.
	 */
	virtual void DrawNode(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes, UBOOL bShowWeight);
	
#endif
}

/** Horizontal size of comment box in pixels. */
var()	int			SizeX;

/** Vertical size of comment box in pixels. */
var()	int			SizeY;

/** Width of border of comment box in pixels. */
var()	int			BorderWidth;

/** Should we draw a box for this comment object, or leave it just as text. */
var()	bool		bDrawBox;

/** If we are drawing a box, should it be filled, or just an outline. */
var()	bool		bFilled;

/** If bDrawBox and bFilled are true, and FillMaterial or FillTexture are true, should be tile it across the box or stretch to fit. */
var()	bool		bTileFill;

/** If we are drawing a box for this comment object, what colour should the border be. */
var()	color		BorderColor;

/** If bDrawBox and bFilled are true, what colour should the background be. */
var()	color		FillColor;

/**
 *	If bDrawBox and bFilled, you can optionally specify a texture to fill the box with.
 *	If both FillTexture and FillMaterial are specified, the FillMaterial will be used.
 */
var()	editoronly	Texture2D	FillTexture;

/**
 *	If bDrawBox and bFilled, you can optionally specify a material to fill the box with.
 *	If both FillTexture and FillMaterial are specified, the FillMaterial will be used.
 */
var()	editoronly	Material	FillMaterial;

var()   editoronly String       ObjComment;

defaultproperties
{
	//bDrawFirst=true
	//ObjName="Sequence Comment"
	//ObjComment="Comment"

	SizeX=128
	SizeY=64

	DrawWidth=128
	DrawHeight=64

	BorderWidth=1
	bFilled=true

	FillColor=(R=255,G=255,B=255,A=16)
	BorderColor=(R=0,G=0,B=0,A=255)
	bDrawBox = true
}
