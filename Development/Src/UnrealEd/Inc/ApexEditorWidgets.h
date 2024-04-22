#ifndef APEX_EDITOR_WIDGETS_H
#define APEX_EDITOR_WIDGETS_H

#include "UnrealEd.h"

#if defined(PX_WINDOWS) && WITH_APEX_PARTICLES
	#define WITH_APEX_EDITOR 1
#endif

#if WITH_APEX_EDITOR

#include "stdwx.h"
#pragma pack( push, 8 )
#include "PxGenericPropertiesEditorCallback.h"
#pragma pack(pop )
class UApexGenericAsset;
class ApexCurveEditorPanel;

namespace physx
{
	class PxGenericPropertiesEditorPanel;
	class PxCurveEditorPanel;
}

namespace NxParameterized
{
	class Interface;
}

struct ApexEditObject
{
	ApexEditObject(void)
	{
		AssetObject = NULL;
		AssetProperties = NULL;
		EditNotify = NULL;
	}
	NxParameterized::Interface            *AssetProperties;
	UApexGenericAsset					  *AssetObject;
	ApexEditNotify						  *EditNotify;
};

// generic apex asset editor that can be embedded in other windows.
class ApexGenericEditorPanel : public wxPanel, public physx::PxGenericPropertiesEditorCallback, public ApexEditInterface
{
	public:
		ApexGenericEditorPanel(wxWindow &parent);
		virtual ~ApexGenericEditorPanel(void);
		
		void ClearProperties(void);
		void AddObject(UApexGenericAsset &ApexAsset);
		
		void SetCurveEditor(ApexCurveEditorPanel *CurveEditor);

		virtual void SetApexEditNotify(UApexAsset *obj,ApexEditNotify *notify);
		virtual void NotifyObjectDestroy(UApexAsset *obj);

		virtual const char *onFindNamedReference(const char *variant);

	
	private:
		void OnSize(wxSizeEvent &In);
	
	private:
		//! called just after a property has been editted by the user.
		virtual void onPostPropertyEdit(NxParameterized::Handle &handle);
	
	private:
		physx::PxGenericPropertiesEditorPanel *EditorPanel;
		TArray< ApexEditObject >               EditObjects;
	
	private:
		DECLARE_EVENT_TABLE()
};

// self contained window that includes a  ApexGenericEditorPanel.
class ApexGenericEditorFrame : public wxFrame
{
	public:
		ApexGenericEditorFrame(void);
		virtual ~ApexGenericEditorFrame(void);
		
		void ClearProperties(void);
		void AddObject(UApexGenericAsset &ApexAsset);
		
	private:
		ApexGenericEditorPanel *EditorPanel;
	
	private:
		DECLARE_EVENT_TABLE()
};

// curve editor that can be embedded in other windows.
class ApexCurveEditorPanel : public wxPanel
{
	friend class ApexGenericEditorPanel;
	public:
		ApexCurveEditorPanel(wxWindow &parent);
		virtual ~ApexCurveEditorPanel(void);
	
	private:
		void OnSize(wxSizeEvent &In);
	
	private:
		physx::PxCurveEditorPanel   *CurvePanel;
	
	private:
		DECLARE_EVENT_TABLE()
};

#endif // #if WITH_APEX_EDITOR

class UApexGenericAsset;

void CreateApexEditorWidget(UApexGenericAsset *object);


#endif
