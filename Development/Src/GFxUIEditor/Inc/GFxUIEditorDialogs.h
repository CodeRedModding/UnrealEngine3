/**********************************************************************

Filename    :   GFxUIEditorDialogs.h
Content     :   Browser entries and dialogs for Flash movies.

Copyright   :   (c) 2001-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :   

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING 
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/


#include "GFxUIEditor.h"

#if WITH_GFx

#include "GFxUIClasses.h"
#include "Dialogs.h"

class GFxUIWxDlgCreateInstance : public wxDialog
{
    using wxDialog::ShowModal;

public:
    GFxUIWxDlgCreateInstance();
    virtual ~GFxUIWxDlgCreateInstance();

    const FString& GetPackage() const
    {
        return Package;
    }
    const FString& GetGroup() const
    {
        return Group;
    }
    const FString& GetObjectName() const
    {
        return Name;
    }
    UClass* GetClass() const
    {
        return Class;
    }

    int ShowModal(const FString& InPackage, const FString& InGroup, const FString& InName, UClass* Base );
    virtual bool Validate();

protected:
    FString Package, Group, Name;
    UClass* Class;
    wxBoxSizer* PGNSizer;
    wxPanel* PGNPanel;
    WxPkgGrpNameCtrl* PGNCtrl;
    wxComboBox* ClassCtrl;

private:
    void OnOK( wxCommandEvent& In );

    DECLARE_EVENT_TABLE()
};


#endif // WITH_GFx