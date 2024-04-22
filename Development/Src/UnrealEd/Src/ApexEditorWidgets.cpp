#include "ApexEditorWidgets.h"
#include "EnginePrivate.h"
#include "EngineMeshClasses.h"

#if WITH_APEX_EDITOR

#include "NvApexManager.h"
#pragma pack( push, 8 )
#include "PxEditorPanel.h"
#include "PxEditorWidgetManager.h"
#include "PxGenericPropertiesEditorPanel.h"
#include "PxGenericPropertiesEditorPanelDesc.h"
#include "PxCurveEditorPanel.h"
#include "NxParameterized.h"
#pragma pack( pop )
#include "GenericBrowser.h"
#include "NvApexCommands.h"

/*************************
** ApexEditorIdleHandle **
*************************/

// this is just a dummy/invisible window setup to trap the idle event from wx widgets...
class ApexEditorIdleHandle : public wxFrame
{
	public:
		static void ConditionalInitialize(void)
		{
			if(!Instance) new ApexEditorIdleHandle;
		}
		
	private:
		ApexEditorIdleHandle(void) :
			wxFrame(0, wxID_ANY, wxT(""))
		{
			Instance = this;
		}
		
		virtual ~ApexEditorIdleHandle(void)
		{
			Instance = 0;
		}
		
		void OnIdle(wxIdleEvent &In)
		{
			physx::PxEditorWidgetManager *WidgetManager = GApexManager->GetPxEditorWidgetManager();
			check(WidgetManager);
			if(WidgetManager)
			{
				WidgetManager->update();
			}
		}
	
	private:
		static ApexEditorIdleHandle *Instance;
	
	private:
		DECLARE_EVENT_TABLE()
};
ApexEditorIdleHandle *ApexEditorIdleHandle::Instance = 0;

BEGIN_EVENT_TABLE(ApexEditorIdleHandle, wxFrame)
    EVT_IDLE(ApexEditorIdleHandle::OnIdle)
END_EVENT_TABLE()

/***************************
** ApexGenericEditorPanel **
***************************/

BEGIN_EVENT_TABLE(ApexGenericEditorPanel, wxPanel)
	EVT_SIZE(ApexGenericEditorPanel::OnSize)
END_EVENT_TABLE()



ApexGenericEditorPanel::ApexGenericEditorPanel(wxWindow &parent) :
	wxPanel(&parent)
{
	EditorPanel     = NULL;
	
	ApexEditorIdleHandle::ConditionalInitialize(); // make sure we process idle events...
	
	physx::PxEditorWidgetManager *WidgetManager = GApexManager->GetPxEditorWidgetManager();
	check(WidgetManager);
	if(WidgetManager)
	{
		physx::PxGenericPropertiesEditorPanelDesc PanelDesc;
		PanelDesc.parentWindow = (HWND)GetHWND();
		PanelDesc.callback     = this;
		EditorPanel = WidgetManager->createGenericPropertiesEditor(PanelDesc);
		check(EditorPanel);
	}
}

ApexGenericEditorPanel::~ApexGenericEditorPanel(void)
{
	if(EditorPanel)     EditorPanel->release();
	for (INT i=0; i<EditObjects.Num(); i++)
	{
		ApexEditObject &a = EditObjects(i);
		if ( a.AssetProperties )
		{
			UBOOL kill = TRUE;
			if ( a.EditNotify )
			{
				kill = a.EditNotify->NotifyEditorClosed(a.AssetProperties);
			}
			if ( kill )
			{
				a.AssetProperties->destroy();
			}
		}
	}
}

void ApexGenericEditorPanel::ClearProperties(void)
{
	if(EditorPanel) EditorPanel->clearObjects();
	for (INT i=0; i<EditObjects.Num(); i++)
	{
		ApexEditObject &a = EditObjects(i);
		if ( a.AssetProperties )
		{
			UBOOL kill = true;
			if ( a.EditNotify )
			{
				kill = a.EditNotify->NotifyEditorClosed(a.AssetProperties);
			}
			if ( kill )
				a.AssetProperties->destroy();
			a.AssetProperties = NULL;
		}
	}
	EditObjects.Empty();
}

void ApexGenericEditorPanel::AddObject(UApexGenericAsset &ApexAsset)
{
	if(EditorPanel)
	{
		ApexEditObject ae;
		ae.AssetProperties = (NxParameterized::Interface*)ApexAsset.GetAssetNxParameterized();
		if(ae.AssetProperties)
		{
			ae.AssetObject = &ApexAsset;
			EditorPanel->addObject(*ae.AssetProperties);
			EditObjects.AddItem(ae);
			ApexAsset.NotifyApexEditMode(this);
		}
	}
}

void ApexGenericEditorPanel::SetCurveEditor(ApexCurveEditorPanel *CurveEditor)
{
	EditorPanel->setCurveEditor(CurveEditor ? CurveEditor->CurvePanel : 0);
}

void ApexGenericEditorPanel::SetApexEditNotify(UApexAsset *obj,ApexEditNotify *notify)
{
	for (INT i=0; i<EditObjects.Num(); i++)
	{
		ApexEditObject &ae = EditObjects(i);
		if ( ae.AssetObject == obj )
		{
			ae.EditNotify = notify;
			break;
		}
	}
}

void ApexGenericEditorPanel::NotifyObjectDestroy(UApexAsset *obj)
{
	for (INT i=0; i<EditObjects.Num(); i++)
	{
		ApexEditObject &ae = EditObjects(i);
		if ( ae.AssetObject == obj )
		{
			if ( EditorPanel )
			{
				EditorPanel->removeObject(*ae.AssetProperties);
				ae.AssetObject = NULL;
				UBOOL kill = TRUE;
				if ( ae.EditNotify )
				{
					kill = ae.EditNotify->NotifyEditorClosed(ae.AssetProperties);
				}
				if ( kill )
				{
					ae.AssetProperties->destroy();
					ae.AssetProperties = NULL;
				}
				ae.EditNotify = NULL;
			}
		}
	}
	UBOOL alive = FALSE;
	for (INT i=0; i<EditObjects.Num(); i++)
	{
		ApexEditObject &ae = EditObjects(i);
		if ( ae.AssetObject )
		{
			alive = TRUE;
			break;
		}
	}
	if ( !alive )
	{
		// James make the panel go away here!
	}

}


void ApexGenericEditorPanel::OnSize(wxSizeEvent &In)
{
	if(EditorPanel) EditorPanel->onResize();
}

//! called just after a property has been editted by the user.
void ApexGenericEditorPanel::onPostPropertyEdit(NxParameterized::Handle &handle)
{
	for (INT i=0; i<EditObjects.Num(); i++)
	{
		ApexEditObject &ae = EditObjects(i);
		if ( ae.AssetProperties == handle.getInterface() )
		{
			if ( ae.EditNotify )
			{
//				ae.EditNotify->OnPostPropertyEdit(ae.AssetProperties,handle);
			}
			break;
		}
	}
}

/***************************
** ApexGenericEditorFrame **
***************************/

BEGIN_EVENT_TABLE(ApexGenericEditorFrame, wxFrame)
END_EVENT_TABLE()

ApexGenericEditorFrame::ApexGenericEditorFrame(void) :
	wxFrame(0, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, 600), wxDEFAULT_FRAME_STYLE | wxSTAY_ON_TOP | wxFRAME_TOOL_WINDOW | wxFRAME_NO_TASKBAR)
{
	SetLabel(wxT("APEX Properties"));
	EditorPanel = new ApexGenericEditorPanel(*this);
	Show(true);
}

ApexGenericEditorFrame::~ApexGenericEditorFrame(void)
{
	if(EditorPanel) delete EditorPanel;
}

void ApexGenericEditorFrame::ClearProperties(void)
{
	if(EditorPanel) EditorPanel->ClearProperties();
}

void ApexGenericEditorFrame::AddObject(UApexGenericAsset &ApexAsset)
{
	if(EditorPanel) EditorPanel->AddObject(ApexAsset);
}

const char *ApexGenericEditorPanel::onFindNamedReference(const char *variant)
{
	const char *ResourceName = NULL;

	PX_FORCE_PARAMETER_REFERENCE(variant);

	USelection *Selection = WxGenericBrowser::GetSelection();
	if ( Selection && Selection->Num() == 1 )
	{
		UObject *found = NULL;
		if ( strcmp(variant,"ApexOpaqueMesh") == 0 )
		{
			UStaticMesh *obj = Selection->GetTop<UStaticMesh>();
			if ( obj )
			{
				found = obj;
			}
		}
		else if ( strcmp(variant,"NxFluidIosAsset") == 0 )
		{
			UApexGenericAsset *obj = Selection->GetTop<UApexGenericAsset>();
			if ( obj )
			{
				FIApexAsset *asset = obj->GetApexGenericAsset();
				if ( asset && asset->GetType() == AAT_FLUID_IOS )
				{
					found = obj;
				}
			}
		}
		else if ( strcmp(variant,"NxBasicIosAsset") == 0 )
		{
			UApexGenericAsset *obj = Selection->GetTop<UApexGenericAsset>();
			if ( obj )
			{
				FIApexAsset *asset = obj->GetApexGenericAsset();
				if ( asset && asset->GetType() == AAT_BASIC_IOS )
				{
					found = obj;
				}
			}
		}
		if ( found )
		{
			static char Name[512] = { 0 };
			FTCHARToANSI_Convert StringConverter;
			StringConverter.Convert( *found->GetPathName(), Name, sizeof(Name) );
			ResourceName = Name;
		}
	}
	return ResourceName;
}

/*************************
** ApexCurveEditorPanel **
*************************/

BEGIN_EVENT_TABLE(ApexCurveEditorPanel, wxPanel)
	EVT_SIZE(ApexCurveEditorPanel::OnSize)
END_EVENT_TABLE()

ApexCurveEditorPanel::ApexCurveEditorPanel(wxWindow &parent) :
	wxPanel(&parent)
{
	CurvePanel = 0;
	
	ApexEditorIdleHandle::ConditionalInitialize(); // make sure we process idle events...
	
	physx::PxEditorWidgetManager *WidgetManager = GApexManager->GetPxEditorWidgetManager();
	check(WidgetManager);
	if(WidgetManager)
	{
		physx::PxEditorPanelDesc PanelDesc;
		PanelDesc.parentWindow = (HWND)GetHWND();
		CurvePanel = WidgetManager->createCurveEditor(PanelDesc);
		check(CurvePanel);
	}
}

ApexCurveEditorPanel::~ApexCurveEditorPanel(void)
{
	if(CurvePanel) CurvePanel->release();
}

void ApexCurveEditorPanel::OnSize(wxSizeEvent &In)
{
	if(CurvePanel) CurvePanel->onResize();
}


#endif // WITH_APEX_EDITOR


/***************************
** CreateApexEditorWidget **
***************************/

void CreateApexEditorWidget(UApexGenericAsset *object)
{
#if WITH_APEX_EDITOR
	if(object)
	{
		ApexGenericEditorFrame *Editor = new ApexGenericEditorFrame();
		Editor->AddObject(*object);
	}
#endif
}
