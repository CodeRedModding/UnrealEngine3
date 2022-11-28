///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/containr.cpp
// Purpose:     implementation of wxControlContainer
// Author:      Vadim Zeitlin
// Modified by:
// Created:     06.08.01
// RCS-ID:      $Id: containr.cpp,v 1.14.2.2 2002/10/29 00:34:15 VZ Exp $
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// License:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "containr.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/event.h"
    #include "wx/window.h"
#endif //WX_PRECOMP

#include "wx/containr.h"

// ============================================================================
// implementation
// ============================================================================

wxControlContainer::wxControlContainer(wxWindow *winParent)
{
    m_winParent = winParent;

    m_winLastFocused =
    m_winTmpDefault =
    m_winDefault = NULL;
}

void wxControlContainer::SetLastFocus(wxWindow *win)
{
    // the panel itself should never get the focus at all but if it does happen
    // temporarily (as it seems to do under wxGTK), at the very least don't
    // forget our previous m_winLastFocused
    if ( win != m_winParent )
    {
        // if we're setting the focus
        if ( win )
        {
            // find the last _immediate_ child which got focus
            wxWindow *winParent = win;
            while ( winParent != m_winParent )
            {
                win = winParent;
                winParent = win->GetParent();

                // Yes, this can happen, though in a totally pathological case.
                // like when detaching a menubar from a frame with a child
                // which has pushed itself as an event handler for the menubar.
                // (under wxGTK)

                wxASSERT_MSG( winParent,
                              _T("Setting last focus for a window that is not our child?") );
            }
        }

        m_winLastFocused = win;

        if ( win )
        {
            wxLogTrace(_T("focus"), _T("Set last focus to %s(%s)"),
                       win->GetClassInfo()->GetClassName(),
                       win->GetLabel().c_str());
        }
        else
        {
            wxLogTrace(_T("focus"), _T("No more last focus"));
        }
    }

    // propagate the last focus upwards so that our parent can set focus back
    // to us if it loses it now and regains later
    wxWindow *parent = m_winParent->GetParent();
    if ( parent )
    {
        wxChildFocusEvent eventFocus(m_winParent);
        parent->GetEventHandler()->ProcessEvent(eventFocus);
    }
}

// ----------------------------------------------------------------------------
// Keyboard handling - this is the place where the TAB traversal logic is
// implemented. As this code is common to all ports, this ensures consistent
// behaviour even if we don't specify how exactly the wxNavigationKeyEvent are
// generated and this is done in platform specific code which also ensures that
// we can follow the given platform standards.
// ----------------------------------------------------------------------------

void wxControlContainer::HandleOnNavigationKey( wxNavigationKeyEvent& event )
{
    wxWindow *parent = m_winParent->GetParent();

    // the event is propagated downwards if the event emitter was our parent
    bool goingDown = event.GetEventObject() == parent;

    const wxWindowList& children = m_winParent->GetChildren();

    // there is not much to do if we don't have children and we're not
    // interested in "notebook page change" events here
    if ( !children.GetCount() || event.IsWindowChange() )
    {
        // let the parent process it unless it already comes from our parent
        // of we don't have any
        if ( goingDown ||
             !parent || !parent->GetEventHandler()->ProcessEvent(event) )
        {
            event.Skip();
        }

        return;
    }

    // where are we going?
    bool forward = event.GetDirection();

    // the node of the children list from which we should start looking for the
    // next acceptable child
    wxWindowList::Node *node, *start_node;

    // we should start from the first/last control and not from the one which
    // had focus the last time if we're propagating the event downwards because
    // for our parent we look like a single control
    if ( goingDown )
    {
        // just to be sure it's not used (normally this is not necessary, but
        // doesn't hurt neither)
        m_winLastFocused = (wxWindow *)NULL;

        // start from first or last depending on where we're going
        node = forward ? children.GetFirst() : children.GetLast();

        // we want to cycle over all nodes
        start_node = (wxWindowList::Node *)NULL;
    }
    else
    {
        // try to find the child which has the focus currently

        // the event emitter might have done this for us
        wxWindow *winFocus = event.GetCurrentFocus();

        // but if not, we might know where the focus was ourselves
        if (!winFocus)
            winFocus = m_winLastFocused;

        // if still no luck, do it the hard way
        if (!winFocus)
            winFocus = wxWindow::FindFocus();

        if ( winFocus )
        {
            // ok, we found the focus - now is it our child?
            start_node = children.Find( winFocus );
        }
        else
        {
            start_node = (wxWindowList::Node *)NULL;
        }

        if ( !start_node && m_winLastFocused )
        {
            // window which has focus isn't our child, fall back to the one
            // which had the focus the last time
            start_node = children.Find( m_winLastFocused );
        }

        // if we still didn't find anything, we should start with the first one
        if ( !start_node )
        {
            start_node = children.GetFirst();
        }

        // and the first child which we can try setting focus to is the next or
        // the previous one
        node = forward ? start_node->GetNext() : start_node->GetPrevious();
    }

    // we want to cycle over all elements passing by NULL
    while ( node != start_node )
    {
        // Have we come to the last or first item on the panel?
        if ( !node )
        {
            if ( !goingDown )
            {
                // Check if our (may be grand) parent is another panel: if this
                // is the case, they will know what to do with this navigation
                // key and so give them the chance to process it instead of
                // looping inside this panel (normally, the focus will go to
                // the next/previous item after this panel in the parent
                // panel).
                wxWindow *focussed_child_of_parent = m_winParent;
                while ( parent )
                {
                    // we don't want to tab into a different dialog or frame
                    if ( focussed_child_of_parent->IsTopLevel() )
                        break;

                    event.SetCurrentFocus( focussed_child_of_parent );
                    if ( parent->GetEventHandler()->ProcessEvent( event ) )
                        return;

                    focussed_child_of_parent = parent;

                    parent = parent->GetParent();
                }
            }
            //else: as the focus came from our parent, we definitely don't want
            //      to send it back to it!

            // no, we are not inside another panel so process this ourself
            node = forward ? children.GetFirst() : children.GetLast();

            continue;
        }

        wxWindow *child = node->GetData();

        if ( child->AcceptsFocusFromKeyboard() )
        {
            // if we're setting the focus to a child panel we should prevent it
            // from giving it to the child which had the focus the last time
            // and instead give it to the first/last child depending from which
            // direction we're coming
            event.SetEventObject(m_winParent);
            if ( !child->GetEventHandler()->ProcessEvent(event) )
            {
                // set it first in case SetFocusFromKbd() results in focus
                // change too
                m_winLastFocused = child;

                // everything is simple: just give focus to it
                child->SetFocusFromKbd();
            }
            //else: the child manages its focus itself

            event.Skip( FALSE );

            return;
        }

        node = forward ? node->GetNext() : node->GetPrevious();
    }

    // we cycled through all of our children and none of them wanted to accept
    // focus
    event.Skip();
}

void wxControlContainer::HandleOnWindowDestroy(wxWindowBase *child)
{
    if ( child == m_winLastFocused )
        m_winLastFocused = NULL;

    if ( child == m_winDefault )
        m_winDefault = NULL;

    if ( child == m_winTmpDefault )
        m_winTmpDefault = NULL;
}

// ----------------------------------------------------------------------------
// focus handling
// ----------------------------------------------------------------------------

bool wxControlContainer::DoSetFocus()
{
    wxLogTrace(_T("focus"), _T("SetFocus on wxPanel 0x%08lx."),
               (unsigned long)m_winParent->GetHandle());

    // when the panel gets the focus we move the focus to either the last
    // window that had the focus or the first one that can get it unless the
    // focus had been already set to some other child

    wxWindow *win = wxWindow::FindFocus();
    while ( win )
    {
        if ( win == m_winParent )
        {
            // our child already has focus, don't take it away from it
            return TRUE;
        }

        if ( win->IsTopLevel() )
        {
            // don't look beyond the first top level parent - useless and
            // unnecessary
            break;
        }

        win = win->GetParent();
    }

    return SetFocusToChild();
}

void wxControlContainer::HandleOnFocus(wxFocusEvent& event)
{
    wxLogTrace(_T("focus"), _T("OnFocus on wxPanel 0x%08lx, name: %s"),
               (unsigned long)m_winParent->GetHandle(),
               m_winParent->GetName().c_str() );

    DoSetFocus();

    event.Skip();
}

bool wxControlContainer::SetFocusToChild()
{
    return wxSetFocusToChild(m_winParent, &m_winLastFocused);
}

// ----------------------------------------------------------------------------
// SetFocusToChild(): this function is used by wxPanel but also by wxFrame in
// wxMSW, this is why it is outside of wxControlContainer class
// ----------------------------------------------------------------------------

bool wxSetFocusToChild(wxWindow *win, wxWindow **childLastFocused)
{
    wxCHECK_MSG( win, FALSE, _T("wxSetFocusToChild(): invalid window") );
    wxCHECK_MSG( childLastFocused, FALSE,
                 _T("wxSetFocusToChild(): NULL child poonter") );

    if ( *childLastFocused )
    {
        // It might happen that the window got reparented
        if ( (*childLastFocused)->GetParent() == win )
        {
            wxLogTrace(_T("focus"),
                       _T("SetFocusToChild() => last child (0x%08lx)."),
                       (unsigned long)(*childLastFocused)->GetHandle());

            // not SetFocusFromKbd(): we're restoring focus back to the old
            // window and not setting it as the result of a kbd action
            (*childLastFocused)->SetFocus();
            return TRUE;
        }
        else
        {
            // it doesn't count as such any more
            *childLastFocused = (wxWindow *)NULL;
        }
    }

    // set the focus to the first child who wants it
    wxWindowList::Node *node = win->GetChildren().GetFirst();
    while ( node )
    {
        wxWindow *child = node->GetData();

        if ( child->AcceptsFocusFromKeyboard() && !child->IsTopLevel() )
        {
            wxLogTrace(_T("focus"),
                       _T("SetFocusToChild() => first child (0x%08lx)."),
                       (unsigned long)child->GetHandle());

            *childLastFocused = child;  // should be redundant, but it is not
            child->SetFocusFromKbd();
            return TRUE;
        }

        node = node->GetNext();
    }

    return FALSE;
}
