/*=============================================================================
	ConvexDecompTool.cpp: Utility for turning graphics mesh into convex hulls.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.esc.
=============================================================================*/

#include "UnrealEd.h"
#pragma pack( push, 8 )
#include "..\..\..\External\ConvexDecomposition\ConvexDecomposition\ConvexDecomposition.h"
#pragma pack( pop )
#include "ConvexDecompTool.h"

// Link in the util library
#ifdef _DEBUG
#if _WIN64
	#pragma comment(lib, "ConvexDecompositionDEBUG_64.lib")
#else
	#pragma comment(lib, "ConvexDecompositionDEBUG.lib")
#endif
#else
#if _WIN64
	#pragma comment(lib, "ConvexDecomposition_64.lib")
#else
	#pragma comment(lib, "ConvexDecomposition.lib")
#endif
#endif

using namespace ConvexDecomposition;

/** Class for implementing result interface. Will take results and use them to fill in the passed in FKAggregateGeom. */
class FDecompResultInterface : public ConvexDecompInterface
{
public:
	/** Aggregate geometry that will be filled in. */
	FKAggregateGeom* AggregateGeom;

	FDecompResultInterface(FKAggregateGeom* OutGeom)
	{
		AggregateGeom = OutGeom;
	}

	/** Called by decompose function with each hull result, and will then fill in AggregateGeom. */
	virtual void ConvexDecompResult(ConvexResult &result)
	{
		// Create a new hull in the aggregate geometry
		const INT NewConvexIndex = AggregateGeom->ConvexElems.AddZeroed();
		FKConvexElem& Convex = AggregateGeom->ConvexElems(NewConvexIndex);

		// Read out each hull vertex
		for(UINT i=0; i<result.mHullVcount; i++)
		{
			const double* V = &result.mHullVertices[i*3];
			FVector HullVert( (FLOAT)V[0], (FLOAT)V[1], (FLOAT)V[2] );
			Convex.VertexData.AddItem(HullVert);
		}

		// Munge data into information we need. This may fail. If so, remove.
		UBOOL bSuccess = Convex.GenerateHullData();
		if(!bSuccess)
		{
			AggregateGeom->ConvexElems.Remove(NewConvexIndex);
		}
	}
};


void DecomposeMeshToHulls(FKAggregateGeom* OutGeom, const TArray<FVector>& InVertices, const TArray<INT>& InIndices, INT InDepth, FLOAT InConcavityThresh, FLOAT InCollapseThresh, INT InMaxHullVerts)
{
	// Create output interface object.
	OutGeom->ConvexElems.Empty();
	FDecompResultInterface ResultInterface(OutGeom);

	// Convert floats to doubles
	TArray<double>	InVerticesD;
	InVerticesD.Add(3*InVertices.Num());
	for(INT i=0; i<InVertices.Num(); i++)
	{
		InVerticesD((3*i)+0) = InVertices(i).X;
		InVerticesD((3*i)+1) = InVertices(i).Y;
		InVerticesD((3*i)+2) = InVertices(i).Z;
	}

	// Fill in data for decomposition tool.
	DecompDesc Desc;
	Desc.mVcount		=	InVertices.Num();
	Desc.mVertices		=	(double*)InVerticesD.GetData();
	Desc.mTcount		=	InIndices.Num()/3;
	Desc.mIndices		=	(UINT*)InIndices.GetData();
	Desc.mDepth			=	InDepth;
	Desc.mCpercent		=	InConcavityThresh * 100.f;
	Desc.mPpercent		=	InCollapseThresh * 100.f;
	Desc.mMaxVertices	=	InMaxHullVerts;
	Desc.mSkinWidth		=	0.f;
	Desc.mCallback		=	&ResultInterface;

	// Do actual decomposition. Will call ConvexDecompResult on the ResultInterface for each hull.
	performConvexDecomposition(Desc);
}

BEGIN_EVENT_TABLE( WxConvexDecompOptions, wxDialog )
	EVT_BUTTON( wxID_APPLY, WxConvexDecompOptions::OnApply )
	EVT_BUTTON( wxID_CLOSE, WxConvexDecompOptions::OnPressClose )
	EVT_BUTTON( IDM_DEFAULT, WxConvexDecompOptions::OnDefaults )
	EVT_BUTTON( IDM_NewVolume, WxConvexDecompOptions::OnNewVolume )
	EVT_CLOSE( WxConvexDecompOptions::OnClose )
END_EVENT_TABLE()

WxConvexDecompOptions::WxConvexDecompOptions( FConvexDecompOptionHook* InHook, wxWindow* Parent, UBOOL InHideOnClose /*= FALSE*/, UBOOL InShowNewVolumeButton /*= FALSE*/ )
	: wxDialog( Parent, -1, wxString(*LocalizeUnrealEd("ConvexDecomposition")), wxDefaultPosition, wxSize(400, 200) )
	, bHideOnClose( InHideOnClose )
	, bShowNewVolumeButton( InShowNewVolumeButton )
{
	// Save pointer to callback object
	Hook = InHook;

	// Create all the widgets
	wxBoxSizer* TopHSizer = new wxBoxSizer(wxHORIZONTAL);
	{
		wxFlexGridSizer* SliderGrid = new wxFlexGridSizer(3, 2, 0, 0);
		{
			SliderGrid->AddGrowableCol(1);

			wxStaticText* DepthText = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("Depth"), wxDefaultPosition, wxDefaultSize, 0 );
			SliderGrid->Add(DepthText, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

			DepthSlider = new wxSlider( this, ID_DECOMP_DEPTHSLIDER, 4, 1, 7, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL|wxSL_LABELS );
			SliderGrid->Add(DepthSlider, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

			wxStaticText* VertsText = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("MaxHullVerts"), wxDefaultPosition, wxDefaultSize, 0 );
			SliderGrid->Add(VertsText, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

			MaxVertSlider = new wxSlider( this, ID_DECOMP_SPLITSLIDER, 12, 6, 32, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL|wxSL_LABELS );
			SliderGrid->Add(MaxVertSlider, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);

			wxStaticText* SplitText = new wxStaticText( this, wxID_STATIC, *LocalizeUnrealEd("AllowSplits"), wxDefaultPosition, wxDefaultSize, 0 );
			SliderGrid->Add(SplitText, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

			SplitSlider = new wxSlider( this, ID_DECOMP_COMBINESLIDER, 16, 0, 40, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL|wxSL_LABELS );
			SliderGrid->Add(SplitSlider, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);
		}
		TopHSizer->Add(SliderGrid, 1, wxGROW|wxALL, 5);

		wxBoxSizer* ButtonBoxSizer = new wxBoxSizer(wxVERTICAL);
		{
			ApplyButton = new wxButton( this, wxID_APPLY, *LocalizeUnrealEd("Apply"), wxDefaultPosition, wxDefaultSize, 0 );
			ApplyButton->SetDefault();
			ButtonBoxSizer->Add(ApplyButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			CloseButton = new wxButton( this, wxID_CLOSE, *LocalizeUnrealEd("Close"), wxDefaultPosition, wxDefaultSize, 0 );
			ButtonBoxSizer->Add(CloseButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			DefaultsButton = new wxButton( this, IDM_DEFAULT, *LocalizeUnrealEd("Defaults"), wxDefaultPosition, wxDefaultSize, 0 );
			ButtonBoxSizer->Add(DefaultsButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			if( bShowNewVolumeButton )
			{
				NewVolumeButton = new wxButton( this, IDM_NewVolume, *LocalizeUnrealEd("NewVolume"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonBoxSizer->Add(NewVolumeButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
			}
		}
		TopHSizer->Add(ButtonBoxSizer, 0, wxALIGN_TOP|wxALL, 5);
	}
	SetSizer(TopHSizer);
}

/** When you press apply, call the DoDecomp function on the hook (if there is one) */
void WxConvexDecompOptions::OnApply(wxCommandEvent& In)
{
	if(Hook)
	{
		INT Depth = DepthSlider->GetValue();
		INT MaxVerts = MaxVertSlider->GetValue();
		FLOAT Combine = ((FLOAT)(40 - SplitSlider->GetValue()))/100.f;

		Hook->DoDecomp(Depth, MaxVerts, Combine);
	}
}

/** When you press close, close the window. */
void WxConvexDecompOptions::OnPressClose( wxCommandEvent& In )
{
	if( bHideOnClose )
	{
		Hide();

		// Even though we're only hiding, we still have to give the hook a chance to clean up
		if(Hook)
		{
			Hook->DecompOptionsClosed();
		}
	}
	else
	{
		Close();
	}
}

/** Set controls to their default values. */
void WxConvexDecompOptions::OnDefaults(wxCommandEvent& In)
{
	DepthSlider->SetValue( 4 );
	MaxVertSlider->SetValue( 12 );
	SplitSlider->SetValue( 16 );
}

/** Tell the hook that we want to start a new volume */
void WxConvexDecompOptions::OnNewVolume(wxCommandEvent& In)
{
	if(Hook)
	{
		Hook->DecompNewVolume();
	}
}

/** Send notification when window closes. */
void WxConvexDecompOptions::OnClose( wxCloseEvent& In )
{
	if(Hook)
	{
		Hook->DecompOptionsClosed();
	}

	Destroy();
}

