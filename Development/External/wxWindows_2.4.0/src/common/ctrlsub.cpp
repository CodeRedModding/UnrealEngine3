///////////////////////////////////////////////////////////////////////////////
// Name:        common/ctrlsub.cpp
// Purpose:     wxItemContainer implementation
// Author:      Vadim Zeitlin
// Modified by:
// Created:     22.10.99
// RCS-ID:      $Id: ctrlsub.cpp,v 1.6 2002/05/11 17:14:46 GD Exp $
// Copyright:   (c) wxWindows team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "controlwithitems.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_CONTROLS

#ifndef WX_PRECOMP
    #include "wx/ctrlsub.h"
#endif

// ============================================================================
// implementation
// ============================================================================

wxItemContainer::~wxItemContainer()
{
    // this destructor is required for Darwin
}

// ----------------------------------------------------------------------------
// selection
// ----------------------------------------------------------------------------

wxString wxItemContainer::GetStringSelection() const
{
    wxString s;
    int sel = GetSelection();
    if ( sel != -1 )
        s = GetString(sel);

    return s;
}

// ----------------------------------------------------------------------------
// appending items
// ----------------------------------------------------------------------------

void wxItemContainer::Append(const wxArrayString& strings)
{
    size_t count = strings.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        Append(strings[n]);
    }
}

// ----------------------------------------------------------------------------
// client data
// ----------------------------------------------------------------------------

void wxItemContainer::SetClientObject(int n, wxClientData *data)
{
    wxASSERT_MSG( m_clientDataItemsType != wxClientData_Void,
                  wxT("can't have both object and void client data") );

    // when we call SetClientObject() for the first time, m_clientDataItemsType
    // is still wxClientData_None and so calling DoGetItemClientObject() would
    // fail (in addition to being useless) - don't do it
    if ( m_clientDataItemsType == wxClientData_Object )
    {
        wxClientData *clientDataOld = DoGetItemClientObject(n);
        if ( clientDataOld )
            delete clientDataOld;
    }
    else // m_clientDataItemsType == wxClientData_None
    {
        // now we have object client data
        m_clientDataItemsType = wxClientData_Object;
    }

    DoSetItemClientObject(n, data);
}

wxClientData *wxItemContainer::GetClientObject(int n) const
{
    wxASSERT_MSG( m_clientDataItemsType == wxClientData_Object,
                  wxT("this window doesn't have object client data") );

    return DoGetItemClientObject(n);
}

void wxItemContainer::SetClientData(int n, void *data)
{
    wxASSERT_MSG( m_clientDataItemsType != wxClientData_Object,
                  wxT("can't have both object and void client data") );

    DoSetItemClientData(n, data);
    m_clientDataItemsType = wxClientData_Void;
}

void *wxItemContainer::GetClientData(int n) const
{
    wxASSERT_MSG( m_clientDataItemsType == wxClientData_Void,
                  wxT("this window doesn't have void client data") );

    return DoGetItemClientData(n);
}

wxControlWithItems::~wxControlWithItems()
{
    // this destructor is required for Darwin
}

#endif // wxUSE_CONTROLS
