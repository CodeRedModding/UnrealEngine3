/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEdCLR.h"
#include "MatineeDirectorWindowShared.h"
#include "InterpEditor.h"

#ifdef __cplusplus_cli
#include "ManagedCodeSupportCLR.h"
#include "WPFFrameCLR.h"
#include "LevelViewportHostWindowCLR.h"
#endif

namespace MatineeWindows
{
#ifdef __cplusplus_cli

using namespace System::Windows::Input;


//-----------------------------------------------
// Director Controls Panel
//-----------------------------------------------
ref class MDirectorControlPanel : public MWPFPanel
{
public:
	MDirectorControlPanel(String^ InXamlName)
		: MWPFPanel(InXamlName)
	{
		//hook up button events
		//Button^ OKButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "OKButton" ) );
		//OKButton->Click += gcnew RoutedEventHandler( this, &MSourceControlSubmitPanel::OKClicked );

		//Button^ CancelButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "CancelButton" ) );
		//CancelButton->Click += gcnew RoutedEventHandler( this, &MSourceControlSubmitPanel::CancelClicked );
	}

	/**
	 * Callback when the parent frame is set to hook up custom events to its widgets
	 */
	virtual void SetParentFrame (MWPFFrame^ InParentFrame) override
	{
		MWPFPanel::SetParentFrame(InParentFrame);
	}

	/** Sets the Matinee pointer for response to UI */
	void SetMatinee(WxInterpEd* InInterpEd)
	{
		check(InInterpEd);
		InterpEd = InInterpEd;
	}

private:
	/** Internal widgets to save having to get in multiple places*/


	WxInterpEd* InterpEd;
};


//-----------------------------------------------
// Director Layout Panel
//-----------------------------------------------
ref class MDirectorViewportLayoutPanel : public MWPFPanel
{
public:
	MDirectorViewportLayoutPanel(String^ InXamlName)
		: MWPFPanel(InXamlName)
	{
		ViewportDockPanel = safe_cast< DockPanel^ >( LogicalTreeHelper::FindLogicalNode( this, "ViewportDockPanel" ) );
	}

	/**
	 * Callback when the parent frame is set to hook up custom events to its widgets
	 */
	virtual void SetParentFrame (MWPFFrame^ InParentFrame) override
	{
		MWPFPanel::SetParentFrame(InParentFrame);
	}

	/** Sets viewport panel that is the preview of what matinee should see */
	void SetPreviewViewport(MViewportPanel^ InNewPreviewPanel)
	{
		//set content
		Border^ ViewportBorder = safe_cast< Border^ >( LogicalTreeHelper::FindLogicalNode( this, TEXT("PreviewBorder") ) );
		ViewportBorder->Child = InNewPreviewPanel;
	}

	/** Sets the control panel.  The interface to Matinee */
	void SetControlPanel(MDirectorControlPanel^ InNewControlPanel)
	{
		//set content
		Border^ ControlBorder = safe_cast< Border^ >( LogicalTreeHelper::FindLogicalNode( this, TEXT("ControlBorder") ) );
		ControlBorder->Child = InNewControlPanel;
	}

	/** Adds viewport panel to the array of camera viewports */
	void AddViewport(MViewportPanel^ InNewViewportPanel)
	{
		//set rendering options for the viewport
		//NEEDED


		ViewportDockPanel->Children->Add(InNewViewportPanel);
	}

	/** 
	* Captures keyboard input
	*
	* @param Sender			Object sending the event
	* @param EventArgs		Key event arguments
	*/
	void OnKeyPressed( Object^ Sender, KeyEventArgs^ EventArgs )
	{
		//if (IsWithin<INT>(EventArgs->Key, Input::Key::D0, Input::Key::D9))
		{
			//INT Index = EventArgs->Key - Input::Key::D0;
			//one should be a zero and so on, wrap around for 0
			//Index = (Index == 0) ? (Index - 1) : 9;

			//if recording, set the present key frame
		}
	}

private:
	/** Internal widgets to save having to get in multiple places*/
	DockPanel^ ViewportDockPanel;
};


/**
 * Helper class to hold the panels so they can be disposed of.  Compiler didn't like the List being part of GC Root
 */
ref class MDirectorPanelHelper
{
public:
	/**allocate the panel list*/
	MDirectorPanelHelper()
	{
		DirectorPanelList = gcnew List<MWPFPanel^>();
	}

	/**Close all panels*/
	~MDirectorPanelHelper()
	{
		//delete panels explicitely
		for (INT i = 0; i < DirectorPanelList->Count; ++i)
		{
			MWPFPanel^ TempPanel = DirectorPanelList[i];
			delete TempPanel;
		}
		delete DirectorPanelList;
		DirectorPanelList = nullptr;
	}

	/** List of panels to delete on close*/
	List<MWPFPanel^>^ DirectorPanelList;
};



/**Global pointer to re-usable Color Picker Frame*/
GCRoot(MWPFFrame^) GDirectorFrame;
/**Saved pointers to child panels for manual destruction*/
GCRoot(MDirectorPanelHelper^) GDirectorPanelHelper;

#endif // __cplusplus_cli

/**Gateway function to launch the director interface window*/
void LaunchDirectorWindow(WxInterpEd* InInterpEd)
{
	CloseDirectorWindow(InInterpEd);

#ifdef __cplusplus_cli
	WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
	Settings->WindowTitle = CLRTools::ToString(*LocalizeUnrealEd("InterpEd_MatineeDirector_Title"));

	GDirectorFrame = gcnew MWPFFrame(InInterpEd, Settings, TEXT("Matinee Director"));
	GDirectorPanelHelper = gcnew MDirectorPanelHelper();

	//list of layout window and panels
	List<MWPFPanel^>^ PanelList = GDirectorPanelHelper->DirectorPanelList;
	
	//create wrapper control
	MDirectorViewportLayoutPanel^ LayoutPanel = gcnew MDirectorViewportLayoutPanel(CLRTools::ToString(TEXT("MatineeDirectorLayout.xaml")));
	PanelList->Add(LayoutPanel);

	//make preview viewport
	{
		MViewportPanel^ PreviewPanel = gcnew MViewportPanel(CLRTools::ToString(TEXT("MatineeDirectorPreview.xaml")), SHOW_DefaultGame);
		PreviewPanel->ConnectToMatineeCamera(InInterpEd, -1);
		check(PreviewPanel);
		LayoutPanel->SetPreviewViewport(PreviewPanel);

		PanelList->Add(PreviewPanel);
	}

	//make the interface panel
	{
		MDirectorControlPanel^ ControlPanel = gcnew MDirectorControlPanel(CLRTools::ToString(TEXT("MatineeDirectorControls.xaml")));
		check(ControlPanel);
		LayoutPanel->SetControlPanel(ControlPanel);
		ControlPanel->SetMatinee(InInterpEd);

		PanelList->Add(ControlPanel);
	}

	//make each camera preview

	//Make viewport
	EShowFlags ThumbShowFlags = SHOW_DefaultEditor | SHOW_ViewMode_LightingOnly;

	INT ViewportCount = InInterpEd->GetNumCameraActors();
	for (int i = 0; i < ViewportCount; ++i)
	{
		MViewportPanel^ NewViewportPanel = gcnew MViewportPanel(CLRTools::ToString(TEXT("MatineeDirectorViewport.xaml")), ThumbShowFlags);
		NewViewportPanel->ConnectToMatineeCamera(InInterpEd, i);
		check(NewViewportPanel);
		LayoutPanel->AddViewport(NewViewportPanel);

		PanelList->Add(NewViewportPanel);
	}

	check(GDirectorFrame);
	GDirectorFrame->Raise();
	GDirectorFrame->SetContentAndShowComposite(LayoutPanel, PanelList);

	// Capture keyboard input
	GDirectorFrame->GetEventBorder()->KeyDown += gcnew System::Windows::Input::KeyEventHandler( LayoutPanel, &MDirectorViewportLayoutPanel::OnKeyPressed );

	//need to listen for windows movement events
	//GDirectorFrame->InteropWindow->AddHook(gcnew Interop::HwndSourceHook( this, &MWPFFrame::FrameMessageHookFunction ) );

	delete Settings;
#endif // __cplusplus_cli
}

/**Close Director Window if one has been opened*/
void CloseDirectorWindow(WxInterpEd* InInterpEd)
{
#ifdef __cplusplus_cli
	//if a color picker has been summoned already
	if (GDirectorFrame)
	{
		delete GDirectorFrame;
		GDirectorFrame = NULL;
	}
	if (GDirectorPanelHelper)
	{
		delete GDirectorPanelHelper;
		GDirectorPanelHelper = nullptr;
	}
#endif //#ifdef __cplusplus_cli
}

};	//namespace MatineeWindows

