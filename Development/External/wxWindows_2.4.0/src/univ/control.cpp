/////////////////////////////////////////////////////////////////////////////
// Name:        src/univ/control.cpp
// Purpose:     universal wxControl: adds handling of mnemonics
// Author:      Vadim Zeitlin
// Modified by:
// Created:     14.08.00
// RCS-ID:      $Id: control.cpp,v 1.7 2002/03/17 20:07:17 RR Exp $
// Copyright:   (c) 2000 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

#ifdef __GNUG__
    #pragma implementation "control.h"
#endif

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_CONTROLS

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/control.h"
    #include "wx/dcclient.h"
#endif

#include "wx/univ/renderer.h"
#include "wx/univ/inphand.h"
#include "wx/univ/theme.h"

// ============================================================================
// implementation
// ============================================================================

IMPLEMENT_DYNAMIC_CLASS(wxControl, wxWindow)

BEGIN_EVENT_TABLE(wxControl, wxControlBase)
    WX_EVENT_TABLE_INPUT_CONSUMER(wxControl)
END_EVENT_TABLE()

WX_FORWARD_TO_INPUT_CONSUMER(wxControl)

// ----------------------------------------------------------------------------
// creation
// ----------------------------------------------------------------------------

void wxControl::Init()
{
    m_indexAccel = -1;

    m_inputHandler = (wxInputHandler *)NULL;
}

bool wxControl::Create(wxWindow *parent,
                       wxWindowID id,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxValidator& validator,
                       const wxString& name)
{
    // Ee use wxNO_FULL_REPAINT_ON_RESIZE by default as it results in much
    // less flicker and none of the standard controls needs to be entirely
    // repainted after resize anyhow.
    if ( !wxControlBase::Create(parent, id, pos, size,
                                style | wxNO_FULL_REPAINT_ON_RESIZE ,
                                validator, name) )
    {
        // underlying window creation failed?
        return FALSE;
    }

    return TRUE;
}

// ----------------------------------------------------------------------------
// mnemonics handling
// ----------------------------------------------------------------------------

/* static */
int wxControl::FindAccelIndex(const wxString& label, wxString *labelOnly)
{
    // the character following MNEMONIC_PREFIX is the accelerator for this
    // control unless it is MNEMONIC_PREFIX too - this allows to insert
    // literal MNEMONIC_PREFIX chars into the label
    static const wxChar MNEMONIC_PREFIX = _T('&');

    if ( labelOnly )
    {
        labelOnly->Empty();
        labelOnly->Alloc(label.length());
    }

    int indexAccel = -1;
    for ( const wxChar *pc = label; *pc != wxT('\0'); pc++ )
    {
        if ( *pc == MNEMONIC_PREFIX )
        {
            pc++; // skip it
            if ( *pc != MNEMONIC_PREFIX )
            {
                if ( indexAccel == -1 )
                {
                    // remember it (-1 is for MNEMONIC_PREFIX itself
                    indexAccel = pc - label.c_str() - 1;
                }
                else
                {
                    wxFAIL_MSG(_T("duplicate accel char in control label"));
                }
            }
        }

        if ( labelOnly )
        {
            *labelOnly += *pc;
        }
    }

    return indexAccel;
}

void wxControl::SetLabel(const wxString& label)
{
    wxString labelOld = m_label;
    m_indexAccel = FindAccelIndex(label, &m_label);

    if ( m_label != labelOld )
    {
        Refresh();
    }
}

wxString wxControl::GetLabel() const
{
    return m_label;
}

#endif // wxUSE_CONTROLS
