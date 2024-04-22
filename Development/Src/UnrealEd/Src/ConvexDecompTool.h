/*=============================================================================
	ConvexDecompTool.h: Utility for turning graphics mesh into convex hulls.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __CONVEXDECOMPTOOL_H__
#define __CONVEXDECOMPTOOL_H__


/** Owning tool inherits from this, to get call when 'apply' button is pressed. */
class FConvexDecompOptionHook
{
public :
	virtual void DoDecomp(INT Depth, INT MaxVerts, FLOAT CollapseThresh) {}
	virtual void DecompOptionsClosed() {}
	virtual void DecompNewVolume() {}
};

/** Window that offers convex decomposition options, and an Apply button to use them. */
class WxConvexDecompOptions : public wxDialog
{
public:
	WxConvexDecompOptions( FConvexDecompOptionHook* InHook, wxWindow* Parent, UBOOL InHideOnClose = FALSE, UBOOL InShowNewVolumeButton = FALSE );

private:
	wxSlider *DepthSlider, *MaxVertSlider, *SplitSlider;
	wxButton *ApplyButton, *CloseButton, *NewVolumeButton, *DefaultsButton;

	/** If TRUE, this dialog will hide itself instead of destroying. */
	UBOOL bHideOnClose;

	/** If TRUE, a push button called "New Volume" will appear on the dialog, which will call DecompNewVolume on the hook. */
	UBOOL bShowNewVolumeButton;

	/** DoDecomp is called on this when Apply is pressed. */
	FConvexDecompOptionHook* Hook;

	void OnApply( wxCommandEvent& In );
	void OnPressClose( wxCommandEvent& In );
	void OnDefaults( wxCommandEvent& In );
	void OnNewVolume( wxCommandEvent& In );
	void OnClose( wxCloseEvent& In );

	DECLARE_EVENT_TABLE()
};

/** 
 *	Utlity for turning arbitary mesh into convex hulls.
 *	@output		OutGeom				Collection of convex that will be filled in by function. OutGeom.ConvexElems will be emptied and re-filled by this utility.
 *	@param		InVertices			Array of vertex positions of input mesh
 *	@param		InIndices			Array of triangle indices for input mesh
 *	@param		InDepth				Depth to split, a maximum of 10, generally not over 7
 *	@param		InConcavityThresh	Threshold amount of convexity for splitting, between 0.0 and 1.0 (0.0-0.2 is reasonable)
 *	@param		InCollapseThresh	Threshold volume preservation to collapse, between 0.0 and 1.0 (0.0-0.3 is reasonable)
 *	@param		InMaxHullVerts		Number of verts allowed in a hull
 */
void DecomposeMeshToHulls(FKAggregateGeom* OutGeom, const TArray<FVector>& InVertices, const TArray<INT>& InIndices, INT InDepth, FLOAT InConcavityThresh, FLOAT InCollapseThresh, INT InMaxHullVerts);

#endif // __CONVEXDECOMPTOOL_H__
