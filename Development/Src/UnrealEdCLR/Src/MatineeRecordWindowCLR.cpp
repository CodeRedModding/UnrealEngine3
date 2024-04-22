/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEdCLR.h"
#include "MatineeRecordWindowShared.h"
#include "InterpEditor.h"

#ifdef __cplusplus_cli
#include "ManagedCodeSupportCLR.h"
#include "WPFFrameCLR.h"
#include "LevelViewportHostWindowCLR.h"
#endif

namespace MatineeWindows
{
#ifdef __cplusplus_cli

//-----------------------------------------------
// Director Controls Panel
//-----------------------------------------------
ref class MRecordControlPanel : public MWPFPanel
{
public:
	MRecordControlPanel(String^ InXamlName)
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

private:
	/** Internal widgets to save having to get in multiple places*/
};


//-----------------------------------------------
// Director Layout Panel
//-----------------------------------------------
ref class MRecordViewportLayoutPanel : public MWPFPanel
{
public:
	MRecordViewportLayoutPanel(String^ InXamlName)
		: MWPFPanel(InXamlName)
	{
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
		Border^ ViewportBorder = safe_cast< Border^ >( LogicalTreeHelper::FindLogicalNode( this, TEXT("ViewportBorder") ) );
		ViewportBorder->Child = InNewPreviewPanel;
	}

	/** Sets the control panel.  The interface to Matinee */
	void SetControlPanel(MRecordControlPanel^ InNewControlPanel)
	{
		//set content
		Border^ ControlBorder = safe_cast< Border^ >( LogicalTreeHelper::FindLogicalNode( this, TEXT("ControlBorder") ) );
		ControlBorder->Child = InNewControlPanel;
	}

private:
	/** Internal widgets to save having to get in multiple places*/
};


/**
 * Helper class to hold the panels so they can be disposed of.  Compiler didn't like the List being part of GC Root
 */
ref class MRecordPanelHelper
{
public:
	/**allocate the panel list*/
	MRecordPanelHelper()
	{
		RecordPanelList = gcnew List<MWPFPanel^>();
	}

	/**Close all panels*/
	~MRecordPanelHelper()
	{
		//delete panels explicitely
		for (INT i = 0; i < RecordPanelList->Count; ++i)
		{
			MWPFPanel^ TempPanel = RecordPanelList[i];
			delete TempPanel;
		}
		delete RecordPanelList;
		RecordPanelList = nullptr;
	}

	/** List of panels to delete on close*/
	List<MWPFPanel^>^ RecordPanelList;
};



/**Global pointer to re-usable Matinee Recording Frame*/
GCRoot(MWPFFrame^) GRecordFrame;
/**Saved pointers to child panels for manual destruction*/
GCRoot(MRecordPanelHelper^) GRecordPanelHelper;

#endif // __cplusplus_cli



/**Gateway function to launch the Recording interface window*/
void LaunchRecordWindow(WxInterpEd* InInterpEd)
{
	CloseRecordWindow(InInterpEd);

#ifdef __cplusplus_cli
	WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
	Settings->WindowTitle = CLRTools::ToString(*LocalizeUnrealEd("InterpEd_MatineeRecord_Title"));
	Settings->bResizable = TRUE;

	GRecordFrame = gcnew MWPFFrame(InInterpEd, Settings, TEXT("Matinee Director"));
	GRecordPanelHelper = gcnew MRecordPanelHelper();

	//list of layout window and panels
	List<MWPFPanel^>^ PanelList = GRecordPanelHelper->RecordPanelList;
	
	//create wrapper control
	MRecordViewportLayoutPanel^ LayoutPanel = gcnew MRecordViewportLayoutPanel(CLRTools::ToString(TEXT("MatineeRecordLayout.xaml")));
	PanelList->Add(LayoutPanel);

	//make preview viewport
	{
		MViewportPanel^ ViewportPanel = gcnew MViewportPanel(CLRTools::ToString(TEXT("MatineeRecordViewport.xaml")), SHOW_DefaultEditor | SHOW_ViewMode_Lit);
		check(ViewportPanel);
		ViewportPanel->EnableMatineeRecording();
		LayoutPanel->SetPreviewViewport(ViewportPanel);

		PanelList->Add(ViewportPanel);
	}

	//make the interface panel
	{
		MRecordControlPanel^ ControlPanel = gcnew MRecordControlPanel(CLRTools::ToString(TEXT("MatineeRecordControls.xaml")));
		check(ControlPanel);
		LayoutPanel->SetControlPanel(ControlPanel);

		PanelList->Add(ControlPanel);
	}

	check(GRecordFrame);
	GRecordFrame->Raise();
	GRecordFrame->SetContentAndShowComposite(LayoutPanel, PanelList);

	delete Settings;
#endif // __cplusplus_cli
}

/**Close Recording Window if one has been opened*/
void CloseRecordWindow(WxInterpEd* InInterpEd)
{
#ifdef __cplusplus_cli
	//if a color picker has been summoned already
	if (GRecordFrame)
	{
		delete GRecordFrame;
		GRecordFrame = NULL;
	}
	if (GRecordPanelHelper)
	{
		delete GRecordPanelHelper;
		GRecordPanelHelper = nullptr;
	}
#endif //#ifdef __cplusplus_cli
}


/**Gives focus to the Record Panel so it can receive input*/
void FocusRecordWindow()
{
#ifdef __cplusplus_cli
#endif //#ifdef __cplusplus_cli
}


};	//namespace MatineeWindows

