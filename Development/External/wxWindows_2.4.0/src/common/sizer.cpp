/////////////////////////////////////////////////////////////////////////////
// Name:        sizer.cpp
// Purpose:     provide new wxSizer class for layout
// Author:      Robert Roebling and Robin Dunn
// Modified by: Ron Lee
// Created:
// RCS-ID:      $Id: sizer.cpp,v 1.46.2.4 2002/11/09 13:29:22 RL Exp $
// Copyright:   (c) Robin Dunn, Dirk Holtwick and Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "sizer.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/sizer.h"
#include "wx/utils.h"
#include "wx/statbox.h"
#include "wx/notebook.h"

//---------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxSizerItem, wxObject)
IMPLEMENT_ABSTRACT_CLASS(wxSizer, wxObject)
IMPLEMENT_ABSTRACT_CLASS(wxGridSizer, wxSizer)
IMPLEMENT_ABSTRACT_CLASS(wxFlexGridSizer, wxGridSizer)
IMPLEMENT_ABSTRACT_CLASS(wxBoxSizer, wxSizer)
#if wxUSE_STATBOX
IMPLEMENT_ABSTRACT_CLASS(wxStaticBoxSizer, wxBoxSizer)
#endif
#if wxUSE_NOTEBOOK
IMPLEMENT_ABSTRACT_CLASS(wxNotebookSizer, wxSizer)
#endif

//---------------------------------------------------------------------------
// wxSizerItem
//---------------------------------------------------------------------------

wxSizerItem::wxSizerItem( int width, int height, int option, int flag, int border, wxObject* userData )
{
    m_window = (wxWindow *) NULL;
    m_sizer = (wxSizer *) NULL;
    m_option = option;
    m_border = border;
    m_flag = flag;
    m_show = TRUE;                // Cannot be changed
    m_userData = userData;

    // minimal size is the initial size
    m_minSize.x = width;
    m_minSize.y = height;

    SetRatio(width, height);

    // size is set directly
    m_size = m_minSize;
}

wxSizerItem::wxSizerItem( wxWindow *window, int option, int flag, int border, wxObject* userData )
{
    m_window = window;
    m_sizer = (wxSizer *) NULL;
    m_option = option;
    m_border = border;
    m_flag = flag;
    m_show = TRUE;
    m_userData = userData;

    // minimal size is the initial size
    m_minSize = window->GetSize();

    // aspect ratio calculated from initial size
    SetRatio(m_minSize);

    // size is calculated later
    // m_size = ...
}

wxSizerItem::wxSizerItem( wxSizer *sizer, int option, int flag, int border, wxObject* userData )
{
    m_window = (wxWindow *) NULL;
    m_sizer = sizer;
    m_option = option;
    m_border = border;
    m_flag = flag;
    m_show = TRUE;
    m_userData = userData;

    // minimal size is calculated later
    // m_minSize = ...
    m_ratio = 0;

    // size is calculated later
    // m_size = ...
}

wxSizerItem::~wxSizerItem()
{
    if (m_userData)
        delete m_userData;
    if (m_sizer)
        delete m_sizer;
}


wxSize wxSizerItem::GetSize()
{
    wxSize ret;
    if (IsSizer())
        ret = m_sizer->GetSize();
    else
    if (IsWindow())
        ret = m_window->GetSize();
    else ret = m_size;

    if (m_flag & wxWEST)
        ret.x += m_border;
    if (m_flag & wxEAST)
        ret.x += m_border;
    if (m_flag & wxNORTH)
        ret.y += m_border;
    if (m_flag & wxSOUTH)
        ret.y += m_border;

    return ret;
}

wxSize wxSizerItem::CalcMin()
{
    wxSize ret;
    if (IsSizer())
    {
        ret = m_sizer->GetMinSize();

        // if we have to preserve aspect ratio _AND_ this is
        // the first-time calculation, consider ret to be initial size
        if ((m_flag & wxSHAPED) && !m_ratio)
            SetRatio(ret);
    }
    else
    {
        if ( IsWindow() && (m_flag & wxADJUST_MINSIZE) )
        {
            // By user request, keep the minimal size for this item
            // in sync with the largest of BestSize and any user supplied
            // minimum size hint.  Useful in cases where the item is
            // changeable -- static text labels, etc.
            m_minSize = m_window->GetAdjustedBestSize();
        }

        ret = m_minSize;
    }

    if (m_flag & wxWEST)
        ret.x += m_border;
    if (m_flag & wxEAST)
        ret.x += m_border;
    if (m_flag & wxNORTH)
        ret.y += m_border;
    if (m_flag & wxSOUTH)
        ret.y += m_border;

    return ret;
}

void wxSizerItem::SetDimension( wxPoint pos, wxSize size )
{
    if (m_flag & wxSHAPED)
    {
        // adjust aspect ratio
        int rwidth = (int) (size.y * m_ratio);
        if (rwidth > size.x)
        {
            // fit horizontally
            int rheight = (int) (size.x / m_ratio);
            // add vertical space
            if (m_flag & wxALIGN_CENTER_VERTICAL)
                pos.y += (size.y - rheight) / 2;
            else if (m_flag & wxALIGN_BOTTOM)
                pos.y += (size.y - rheight);
            // use reduced dimensions
            size.y =rheight;
        }
        else if (rwidth < size.x)
        {
            // add horizontal space
            if (m_flag & wxALIGN_CENTER_HORIZONTAL)
                pos.x += (size.x - rwidth) / 2;
            else if (m_flag & wxALIGN_RIGHT)
                pos.x += (size.x - rwidth);
            size.x = rwidth;
        }
    }

    // This is what GetPosition() returns. Since we calculate
    // borders afterwards, GetPosition() will be the left/top
    // corner of the surrounding border.
    m_pos = pos;

    if (m_flag & wxWEST)
    {
        pos.x += m_border;
        size.x -= m_border;
    }
    if (m_flag & wxEAST)
    {
        size.x -= m_border;
    }
    if (m_flag & wxNORTH)
    {
        pos.y += m_border;
        size.y -= m_border;
    }
    if (m_flag & wxSOUTH)
    {
        size.y -= m_border;
    }

    if (IsSizer())
        m_sizer->SetDimension( pos.x, pos.y, size.x, size.y );

    if (IsWindow())
        m_window->SetSize( pos.x, pos.y, size.x, size.y, wxSIZE_ALLOW_MINUS_ONE );

    m_size = size;
}

void wxSizerItem::DeleteWindows()
{
    if (m_window)
         m_window->Destroy();

    if (m_sizer)
        m_sizer->DeleteWindows();
}

bool wxSizerItem::IsWindow()
{
    return (m_window != NULL);
}

bool wxSizerItem::IsSizer()
{
    return (m_sizer != NULL);
}

bool wxSizerItem::IsSpacer()
{
    return (m_window == NULL) && (m_sizer == NULL);
}

//---------------------------------------------------------------------------
// wxSizer
//---------------------------------------------------------------------------

wxSizer::wxSizer()
{
    m_children.DeleteContents( TRUE );
    m_minSize.x = 0;
    m_minSize.y = 0;
}

wxSizer::~wxSizer()
{
    Clear();
}

void wxSizer::Add( wxWindow *window, int option, int flag, int border, wxObject* userData )
{
    m_children.Append( new wxSizerItem( window, option, flag, border, userData ) );
    window->SetContainingSizer(this);
}

void wxSizer::Add( wxSizer *sizer, int option, int flag, int border, wxObject* userData )
{
    m_children.Append( new wxSizerItem( sizer, option, flag, border, userData ) );
}

void wxSizer::Add( int width, int height, int option, int flag, int border, wxObject* userData )
{
    m_children.Append( new wxSizerItem( width, height, option, flag, border, userData ) );
}

void wxSizer::Prepend( wxWindow *window, int option, int flag, int border, wxObject* userData )
{
    m_children.Insert( new wxSizerItem( window, option, flag, border, userData ) );
    window->SetContainingSizer(this);
}

void wxSizer::Prepend( wxSizer *sizer, int option, int flag, int border, wxObject* userData )
{
    m_children.Insert( new wxSizerItem( sizer, option, flag, border, userData ) );
}

void wxSizer::Prepend( int width, int height, int option, int flag, int border, wxObject* userData )
{
    m_children.Insert( new wxSizerItem( width, height, option, flag, border, userData ) );
}

void wxSizer::Insert( int before, wxWindow *window, int option, int flag, int border, wxObject* userData )
{
    m_children.Insert( before, new wxSizerItem( window, option, flag, border, userData ) );
    window->SetContainingSizer(this);
}

void wxSizer::Insert( int before, wxSizer *sizer, int option, int flag, int border, wxObject* userData )
{
    m_children.Insert( before, new wxSizerItem( sizer, option, flag, border, userData ) );
}

void wxSizer::Insert( int before, int width, int height, int option, int flag, int border, wxObject* userData )
{
    m_children.Insert( before, new wxSizerItem( width, height, option, flag, border, userData ) );
}

bool wxSizer::Remove( wxWindow *window )
{
    wxASSERT( window );

    wxNode *node = m_children.First();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*)node->Data();
        if (item->GetWindow() == window)
        {
            item->GetWindow()->SetContainingSizer(NULL);
            m_children.DeleteNode( node );
            return TRUE;
        }
        node = node->Next();
    }

    return FALSE;
}

bool wxSizer::Remove( wxSizer *sizer )
{
    wxASSERT( sizer );

    wxNode *node = m_children.First();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*)node->Data();
        if (item->GetSizer() == sizer)
        {
            m_children.DeleteNode( node );
            return TRUE;
        }
        node = node->Next();
    }

    return FALSE;
}

bool wxSizer::Remove( int pos )
{
    if ((size_t)pos >= m_children.GetCount())
        return FALSE;
    wxNode *node = m_children.Nth( pos );
    if (!node) return FALSE;

    m_children.DeleteNode( node );

    return TRUE;
}

void wxSizer::Clear( bool delete_windows )
{
    // First clear the ContainingSizer pointers
    wxNode *node = m_children.First();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*)node->Data();
        if (item->IsWindow())
            item->GetWindow()->SetContainingSizer(NULL);
        node = node->Next();
    }

    // Destroy the windows if needed
    if (delete_windows)
        DeleteWindows();

    // Now empty the list
    m_children.Clear();
}

void wxSizer::DeleteWindows()
{
    wxNode *node = m_children.First();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*)node->Data();
        item->DeleteWindows();
        node = node->Next();
    }
}

wxSize wxSizer::Fit( wxWindow *window )
{
    wxSize size;
    if (window->IsTopLevel())
        size = FitSize( window );
    else
        size = GetMinWindowSize( window );

    window->SetSize( size );

    return size;
}

void wxSizer::FitInside( wxWindow *window )
{
    wxSize size;
    if (window->IsTopLevel())
        size = VirtualFitSize( window );
    else
        size = GetMinClientSize( window );

    window->SetVirtualSize( size );
}

void wxSizer::Layout()
{
    CalcMin();
    RecalcSizes();
}

void wxSizer::SetSizeHints( wxWindow *window )
{
    // Preserve the window's max size hints, but set the
    // lower bound according to the sizer calculations.

    wxSize size = Fit( window );

    window->SetSizeHints( size.x,
                          size.y,
                          window->GetMaxWidth(),
                          window->GetMaxHeight() );
}

void wxSizer::SetVirtualSizeHints( wxWindow *window )
{
    // Preserve the window's max size hints, but set the
    // lower bound according to the sizer calculations.

    FitInside( window );
    wxSize size( window->GetVirtualSize() );
    window->SetVirtualSizeHints( size.x,
                                 size.y,
                                 window->GetMaxWidth(),
                                 window->GetMaxHeight() );
}

wxSize wxSizer::GetMaxWindowSize( wxWindow *window )
{
    return window->GetMaxSize();
}

wxSize wxSizer::GetMinWindowSize( wxWindow *window )
{
    wxSize minSize( GetMinSize() );
    wxSize size( window->GetSize() );
    wxSize client_size( window->GetClientSize() );
    return wxSize( minSize.x+size.x-client_size.x,
                   minSize.y+size.y-client_size.y );
}

// Return a window size that will fit within the screens dimensions
wxSize wxSizer::FitSize( wxWindow *window )
{
    wxSize size     = GetMinWindowSize( window );
    wxSize sizeMax  = GetMaxWindowSize( window );

    // Limit the size if sizeMax != wxDefaultSize

    if ( size.x > sizeMax.x && sizeMax.x != -1 )
        size.x = sizeMax.x;
    if ( size.y > sizeMax.y && sizeMax.y != -1 )
        size.y = sizeMax.y;

    return size;
}

wxSize wxSizer::GetMaxClientSize( wxWindow *window )
{
    wxSize maxSize( window->GetMaxSize() );

    if( maxSize != wxDefaultSize )
    {
        wxSize size( window->GetSize() );
        wxSize client_size( window->GetClientSize() );

        return wxSize( maxSize.x + client_size.x - size.x,
                       maxSize.y + client_size.y - size.y );
    }
    else
        return wxDefaultSize;
}

wxSize wxSizer::GetMinClientSize( wxWindow *WXUNUSED(window) )
{
    return GetMinSize();  // Already returns client size.
}

wxSize wxSizer::VirtualFitSize( wxWindow *window )
{
    wxSize size     = GetMinClientSize( window );
    wxSize sizeMax  = GetMaxClientSize( window );

    // Limit the size if sizeMax != wxDefaultSize

    if ( size.x > sizeMax.x && sizeMax.x != -1 )
        size.x = sizeMax.x;
    if ( size.y > sizeMax.y && sizeMax.y != -1 )
        size.y = sizeMax.y;

    return size;
}

void wxSizer::SetDimension( int x, int y, int width, int height )
{
    m_position.x = x;
    m_position.y = y;
    m_size.x = width;
    m_size.y = height;
    Layout();
}

wxSize wxSizer::GetMinSize()
{
    wxSize ret( CalcMin() );
    if (ret.x < m_minSize.x) ret.x = m_minSize.x;
    if (ret.y < m_minSize.y) ret.y = m_minSize.y;
    return ret;
}

void wxSizer::DoSetMinSize( int width, int height )
{
    m_minSize.x = width;
    m_minSize.y = height;
}

bool wxSizer::DoSetItemMinSize( wxWindow *window, int width, int height )
{
    wxASSERT( window );

    wxNode *node = m_children.First();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*)node->Data();
        if (item->GetWindow() == window)
        {
            item->SetInitSize( width, height );
            return TRUE;
        }
        node = node->Next();
    }

    node = m_children.First();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*)node->Data();
        if (item->GetSizer())
        {
            // It's a sizer, so lets search recursively.
            if (item->GetSizer()->DoSetItemMinSize( window, width, height ))
            {
                // A child sizer found the requested windw, exit.
                return TRUE;
            }
        }
        node = node->Next();
    }

    return FALSE;
}

bool wxSizer::DoSetItemMinSize( wxSizer *sizer, int width, int height )
{
    wxASSERT( sizer );

    wxNode *node = m_children.First();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*)node->Data();
        if (item->GetSizer() == sizer)
        {
            item->GetSizer()->DoSetMinSize( width, height );
            return TRUE;
        }
        node = node->Next();
    }

    node = m_children.First();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*)node->Data();
        if (item->GetSizer())
        {
            // It's a sizer, so lets search recursively.
            if (item->GetSizer()->DoSetItemMinSize( sizer, width, height ))
            {
                // A child sizer found the requested windw, exit.
                return TRUE;
            }
        }
        node = node->Next();
    }

    return FALSE;
}

bool wxSizer::DoSetItemMinSize( int pos, int width, int height )
{
    wxNode *node = m_children.Nth( pos );
    if (!node) return FALSE;

    wxSizerItem *item = (wxSizerItem*) node->Data();
    if (item->GetSizer())
    {
        // Sizers contains the minimal size in them, if not calculated ...
        item->GetSizer()->DoSetMinSize( width, height );
    }
    else
    {
        // ... but the minimal size of spacers and windows in stored in them
        item->SetInitSize( width, height );
    }

    return TRUE;
}

void wxSizer::Show(wxWindow *window, bool show)
{
    wxNode *node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*) node->Data();

        if (item->IsWindow() && item->GetWindow() == window)
        {
            item->Show(show);
            window->Show(show);
            return;
        }
        node = node->Next();
    }
}

void wxSizer::Show(wxSizer *sizer, bool show)
{
    wxNode *node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*) node->Data();

        if (item->IsSizer() && item->GetSizer() == sizer)
        {
            item->Show(show);
            sizer->ShowItems(show);
            return;
        }
        node = node->Next();
    }
}

void wxSizer::ShowItems (bool show)
{
    wxNode *node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*) node->Data();

        if (item->IsWindow())
            item->GetWindow()->Show (show);
        else if (item->IsSizer())
            item->GetSizer()->ShowItems (show);

        node = node->Next();
    }
}

bool wxSizer::IsShown (wxWindow *window)
{
    wxNode *node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*) node->Data();
        
        if (item->IsWindow() && item->GetWindow() == window)
        {
            return item->IsShown();
        }
        node = node->Next();
    }

    return FALSE;
}

bool wxSizer::IsShown (wxSizer *sizer)
{
    wxNode *node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*) node->Data();

        if (item->IsSizer() && item->GetSizer() == sizer)
        {
            return item->IsShown();
        }
        node = node->Next();
    }

    return FALSE;
}

//---------------------------------------------------------------------------
// wxGridSizer
//---------------------------------------------------------------------------

wxGridSizer::wxGridSizer( int rows, int cols, int vgap, int hgap )
{
    m_rows = rows;
    m_cols = cols;
    m_vgap = vgap;
    m_hgap = hgap;
}

wxGridSizer::wxGridSizer( int cols, int vgap, int hgap )
{
    m_rows = 0;
    m_cols = cols;
    m_vgap = vgap;
    m_hgap = hgap;
}

int wxGridSizer::CalcRowsCols(int& nrows, int& ncols) const
{
    int nitems = m_children.GetCount();
    if ( nitems)
    {
        if ( m_cols )
        {
            ncols = m_cols;
            nrows = (nitems + m_cols - 1) / m_cols;
        }
        else if ( m_rows )
        {
            ncols = (nitems + m_rows - 1) / m_rows;
            nrows = m_rows;
        }
        else // 0 columns, 0 rows?
        {
            wxFAIL_MSG( _T("grid sizer must have either rows or columns fixed") );

            nrows = ncols = 0;
        }
    }

    return nitems;
}

void wxGridSizer::RecalcSizes()
{
    int nitems, nrows, ncols;
    if ( (nitems = CalcRowsCols(nrows, ncols)) == 0 )
        return;

    wxSize sz( GetSize() );
    wxPoint pt( GetPosition() );

    int w = (sz.x - (ncols - 1) * m_hgap) / ncols;
    int h = (sz.y - (nrows - 1) * m_vgap) / nrows;

    int x = pt.x;
    for (int c = 0; c < ncols; c++)
    {
        int y = pt.y;
        for (int r = 0; r < nrows; r++)
        {
            int i = r * ncols + c;
            if (i < nitems)
            {
                wxNode *node = m_children.Nth( i );
                wxASSERT( node );

                SetItemBounds( (wxSizerItem*) node->Data(), x, y, w, h);
            }
            y = y + h + m_vgap;
        }
        x = x + w + m_hgap;
    }
}

wxSize wxGridSizer::CalcMin()
{
    int nitems, nrows, ncols;
    if ( (nitems = CalcRowsCols(nrows, ncols)) == 0 )
        return wxSize(10, 10);

    // Find the max width and height for any component
    int w = 0;
    int h = 0;

    wxNode *node = m_children.First();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*)node->Data();
        wxSize sz( item->CalcMin() );
        w = wxMax( w, sz.x );
        h = wxMax( h, sz.y );

        node = node->Next();
    }

    return wxSize(ncols * w + (ncols-1) * m_hgap,
                  nrows * h + (nrows-1) * m_vgap);
}

void wxGridSizer::SetItemBounds( wxSizerItem *item, int x, int y, int w, int h )
{
    wxPoint pt( x,y );
    wxSize sz( item->CalcMin() );
    int flag = item->GetFlag();

    if ((flag & wxEXPAND) || (flag & wxSHAPED))
    {
       sz = wxSize(w, h);
    }
    else
    {
        if (flag & wxALIGN_CENTER_HORIZONTAL)
        {
            pt.x = x + (w - sz.x) / 2;
        }
        else if (flag & wxALIGN_RIGHT)
        {
            pt.x = x + (w - sz.x);
        }

        if (flag & wxALIGN_CENTER_VERTICAL)
        {
            pt.y = y + (h - sz.y) / 2;
        }
        else if (flag & wxALIGN_BOTTOM)
        {
            pt.y = y + (h - sz.y);
        }
    }

    item->SetDimension(pt, sz);
}

//---------------------------------------------------------------------------
// wxFlexGridSizer
//---------------------------------------------------------------------------

wxFlexGridSizer::wxFlexGridSizer( int rows, int cols, int vgap, int hgap )
   : wxGridSizer( rows, cols, vgap, hgap )
{
    m_rowHeights = (int*) NULL;
    m_colWidths = (int*) NULL;
}

wxFlexGridSizer::wxFlexGridSizer( int cols, int vgap, int hgap )
   : wxGridSizer( cols, vgap, hgap )
{
    m_rowHeights = (int*) NULL;
    m_colWidths = (int*) NULL;
}

wxFlexGridSizer::~wxFlexGridSizer()
{
    if (m_rowHeights)
        delete[] m_rowHeights;
    if (m_colWidths)
        delete[] m_colWidths;
}

void wxFlexGridSizer::CreateArrays()
{
    if (m_rowHeights)
        delete[] m_rowHeights;
    if (m_colWidths)
        delete[] m_colWidths;

    int nitems, nrows, ncols;
    if ( (nitems = CalcRowsCols(nrows, ncols)) == 0 )
    {
        m_rowHeights =
        m_colWidths = NULL;
    }

    m_rowHeights = new int[nrows];
    m_colWidths = new int[ncols];

    for (int col = 0; col < ncols; col++)
        m_colWidths[ col ] = 0;
    for (int row = 0; row < nrows; row++)
        m_rowHeights[ row ] = 0;
}

void wxFlexGridSizer::RecalcSizes()
{
    int nitems, nrows, ncols;
    if ( (nitems = CalcRowsCols(nrows, ncols)) == 0 )
        return;

    wxSize sz( GetSize() );
    wxSize minsz( CalcMin() );
    wxPoint pt( GetPosition() );
    int    delta;
    size_t idx,num;
    wxArrayInt temp;

    // Transfer only those rows into temp which exist in the sizer
    // ignoring the superflouus ones. This prevents a segfault when
    // calling AddGrowableRow( 3 ) if the sizer only has 2 rows.
    for (idx = 0; idx < m_growableRows.GetCount(); idx++)
        if (m_growableRows[idx] < nrows)
            temp.Add( m_growableRows[idx] );
    num = temp.GetCount();

    if ((num > 0) && (sz.y > minsz.y))
    {
        delta = (sz.y - minsz.y) / num;
        for (idx = 0; idx < num; idx++)
            m_rowHeights[ temp[idx] ] += delta;
    }

    temp.Empty();
    // See above
    for (idx = 0; idx < m_growableCols.GetCount(); idx++)
        if (m_growableCols[idx] < ncols)
            temp.Add( m_growableCols[idx] );
    num = temp.GetCount();

    if ((num > 0) && (sz.x > minsz.x))
    {
        delta = (sz.x - minsz.x) / num;
        for (idx = 0; idx < num; idx++)
            m_colWidths[ temp[idx] ] += delta;
    }

    sz = wxSize( pt.x + sz.x, pt.y + sz.y );

    int x = pt.x;
    for (int c = 0; c < ncols; c++)
    {
        int y = pt.y;
        for (int r = 0; r < nrows; r++)
        {
            int i = r * ncols + c;
            if (i < nitems)
            {
                wxNode *node = m_children.Nth( i );
                wxASSERT( node );

                int w = wxMax( 0, wxMin( m_colWidths[c], sz.x - x ) );
                int h = wxMax( 0, wxMin( m_rowHeights[r], sz.y - y ) );

                SetItemBounds( (wxSizerItem*) node->Data(), x, y, w, h);
            }
            y = y + m_rowHeights[r] + m_vgap;
        }
        x = x + m_colWidths[c] + m_hgap;
    }
}

wxSize wxFlexGridSizer::CalcMin()
{
    int nitems, nrows, ncols;
    if ( (nitems = CalcRowsCols(nrows, ncols)) == 0 )
        return wxSize(10,10);

    CreateArrays();

    int i = 0;
    wxNode *node = m_children.First();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*)node->Data();
        wxSize sz( item->CalcMin() );
        int row = i / ncols;
        int col = i % ncols;
        m_rowHeights[ row ] = wxMax( sz.y, m_rowHeights[ row ] );
        m_colWidths[ col ] = wxMax( sz.x, m_colWidths[ col ] );

        node = node->Next();
        i++;
    }

    int width = 0;
    for (int col = 0; col < ncols; col++)
        width += m_colWidths[ col ];

    int height = 0;
    for (int row = 0; row < nrows; row++)
        height += m_rowHeights[ row ];

    return wxSize( width +  (ncols-1) * m_hgap,
                   height + (nrows-1) * m_vgap);
}

void wxFlexGridSizer::AddGrowableRow( size_t idx )
{
    m_growableRows.Add( idx );
}

void wxFlexGridSizer::RemoveGrowableRow( size_t WXUNUSED(idx) )
{
}

void wxFlexGridSizer::AddGrowableCol( size_t idx )
{
    m_growableCols.Add( idx );
}

void wxFlexGridSizer::RemoveGrowableCol( size_t WXUNUSED(idx) )
{
}

//---------------------------------------------------------------------------
// wxBoxSizer
//---------------------------------------------------------------------------

wxBoxSizer::wxBoxSizer( int orient )
{
    m_orient = orient;
}

void wxBoxSizer::RecalcSizes()
{
    if (m_children.GetCount() == 0)
        return;

    int delta = 0;
    int extra = 0;
    if (m_stretchable)
    {
        if (m_orient == wxHORIZONTAL)
        {
            delta = (m_size.x - m_fixedWidth) / m_stretchable;
            extra = (m_size.x - m_fixedWidth) % m_stretchable;
        }
        else
        {
            delta = (m_size.y - m_fixedHeight) / m_stretchable;
            extra = (m_size.y - m_fixedHeight) % m_stretchable;
        }
    }

    wxPoint pt( m_position );

    wxNode *node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*) node->Data();
        if (item->IsShown())
        {
            int weight = 1;
            if (item->GetOption())
                weight = item->GetOption();

            wxSize size( item->CalcMin() );

            if (m_orient == wxVERTICAL)
            {
                wxCoord height = size.y;
                if (item->GetOption())
                {
                    height = (delta * weight) + extra;
                    extra = 0; // only the first item will get the remainder as extra size
                }

                wxPoint child_pos( pt );
                wxSize  child_size( wxSize( size.x, height) );

                if (item->GetFlag() & (wxEXPAND | wxSHAPED))
                    child_size.x = m_size.x;
                else if (item->GetFlag() & wxALIGN_RIGHT)
                    child_pos.x += m_size.x - size.x;
                else if (item->GetFlag() & (wxCENTER | wxALIGN_CENTER_HORIZONTAL))
                // XXX wxCENTER is added for backward compatibility;
                //     wxALIGN_CENTER should be used in new code
                    child_pos.x += (m_size.x - size.x) / 2;

                item->SetDimension( child_pos, child_size );

                pt.y += height;
            }
            else
            {
                wxCoord width = size.x;
                if (item->GetOption())
                {
                    width = (delta * weight) + extra;
                    extra = 0; // only the first item will get the remainder as extra size
                }

                wxPoint child_pos( pt );
                wxSize  child_size( wxSize(width, size.y) );

                if (item->GetFlag() & (wxEXPAND | wxSHAPED))
                    child_size.y = m_size.y;
                else if (item->GetFlag() & wxALIGN_BOTTOM)
                    child_pos.y += m_size.y - size.y;
                else if (item->GetFlag() & (wxCENTER | wxALIGN_CENTER_VERTICAL))
                // XXX wxCENTER is added for backward compatibility;
                //     wxALIGN_CENTER should be used in new code
                    child_pos.y += (m_size.y - size.y) / 2;

                item->SetDimension( child_pos, child_size );

                pt.x += width;
            }
        }

        node = node->Next();
    }
}

wxSize wxBoxSizer::CalcMin()
{
    if (m_children.GetCount() == 0)
        return wxSize(10,10);

    m_stretchable = 0;
    m_minWidth = 0;
    m_minHeight = 0;
    m_fixedWidth = 0;
    m_fixedHeight = 0;

    // Find how long each stretch unit needs to be
    int stretchSize = 1;
    wxNode *node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*) node->Data();
        if (item->IsShown() && item->GetOption() != 0)
        {
            int stretch = item->GetOption();
            wxSize size( item->CalcMin() );
            int sizePerStretch;
            // Integer division rounded up is (a + b - 1) / b
            if (m_orient == wxHORIZONTAL)
                sizePerStretch = ( size.x + stretch - 1 ) / stretch;
            else
                sizePerStretch = ( size.y + stretch - 1 ) / stretch;
            if (sizePerStretch > stretchSize)
                stretchSize = sizePerStretch;
        }
        node = node->Next();
    }
    // Calculate overall minimum size
    node = m_children.GetFirst();
    while (node)
    {
        wxSizerItem *item = (wxSizerItem*) node->Data();
        if (item->IsShown())
        {
            m_stretchable += item->GetOption();

            wxSize size( item->CalcMin() );
            if (item->GetOption() != 0)
            {
                if (m_orient == wxHORIZONTAL)
                    size.x = stretchSize * item->GetOption();
                else
                    size.y = stretchSize * item->GetOption();
            }

            if (m_orient == wxHORIZONTAL)
            {
                m_minWidth += size.x;
                m_minHeight = wxMax( m_minHeight, size.y );
            }
            else
            {
                m_minHeight += size.y;
                m_minWidth = wxMax( m_minWidth, size.x );
            }

            if (item->GetOption() == 0)
            {
                if (m_orient == wxVERTICAL)
                {
                    m_fixedHeight += size.y;
                    m_fixedWidth = wxMax( m_fixedWidth, size.x );
                }
                else
                {
                    m_fixedWidth += size.x;
                    m_fixedHeight = wxMax( m_fixedHeight, size.y );
                }
            }
        }
        node = node->Next();
    }

    return wxSize( m_minWidth, m_minHeight );
}

//---------------------------------------------------------------------------
// wxStaticBoxSizer
//---------------------------------------------------------------------------

#if wxUSE_STATBOX

wxStaticBoxSizer::wxStaticBoxSizer( wxStaticBox *box, int orient )
                : wxBoxSizer( orient )
{
    wxASSERT_MSG( box, wxT("wxStaticBoxSizer needs a static box") );

    m_staticBox = box;
}

static void GetStaticBoxBorders(wxStaticBox *box,
                                int *borderTop, int *borderOther)
{
    // this has to be done platform by platform as there is no way to
    // guess the thickness of a wxStaticBox border
#ifdef __WXGTK__
    if ( box->GetLabel().IsEmpty() )
        *borderTop = 5;
    else
#endif // __WXGTK__
        *borderTop = 15;
    (void)box;
    *borderOther = 5;
}

void wxStaticBoxSizer::RecalcSizes()
{
    int top_border, other_border;
    GetStaticBoxBorders(m_staticBox, &top_border, &other_border);

    m_staticBox->SetSize( m_position.x, m_position.y, m_size.x, m_size.y );

    wxPoint old_pos( m_position );
    m_position.x += other_border;
    m_position.y += top_border;
    wxSize old_size( m_size );
    m_size.x -= 2*other_border;
    m_size.y -= top_border + other_border;

    wxBoxSizer::RecalcSizes();

    m_position = old_pos;
    m_size = old_size;
}

wxSize wxStaticBoxSizer::CalcMin()
{
    int top_border, other_border;
    GetStaticBoxBorders(m_staticBox, &top_border, &other_border);

    wxSize ret( wxBoxSizer::CalcMin() );
    ret.x += 2*other_border;
    ret.y += other_border + top_border;

    return ret;
}

#endif // wxUSE_STATBOX

//---------------------------------------------------------------------------
// wxNotebookSizer
//---------------------------------------------------------------------------

#if wxUSE_NOTEBOOK

wxNotebookSizer::wxNotebookSizer( wxNotebook *nb )
{
    wxASSERT_MSG( nb, wxT("wxNotebookSizer needs a notebook") );

    m_notebook = nb;
}

void wxNotebookSizer::RecalcSizes()
{
    m_notebook->SetSize( m_position.x, m_position.y, m_size.x, m_size.y );
}

wxSize wxNotebookSizer::CalcMin()
{
    wxSize sizeBorder = m_notebook->CalcSizeFromPage(wxSize(0, 0));

    sizeBorder.x += 5;
    sizeBorder.y += 5;

    if (m_notebook->GetChildren().GetCount() == 0)
    {
        return wxSize(sizeBorder.x + 10, sizeBorder.y + 10);
    }

    int maxX = 0;
    int maxY = 0;

    wxWindowList::Node *node = m_notebook->GetChildren().GetFirst();
    while (node)
    {
        wxWindow *item = node->GetData();
        wxSizer *itemsizer = item->GetSizer();

        if (itemsizer)
        {
            wxSize subsize( itemsizer->CalcMin() );

            if (subsize.x > maxX)
                maxX = subsize.x;
            if (subsize.y > maxY)
                maxY = subsize.y;
        }

        node = node->GetNext();
    }

    return wxSize( maxX, maxY ) + sizeBorder;
}

#endif // wxUSE_NOTEBOOK

// vi:sts=4:sw=4:et
