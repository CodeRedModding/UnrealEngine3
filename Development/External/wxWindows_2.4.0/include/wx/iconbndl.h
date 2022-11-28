///////////////////////////////////////////////////////////////////////////////
// Name:        wx/iconbndl.h
// Purpose:     wxIconBundle
// Author:      Mattia barbon
// Modified by:
// Created:     23.03.02
// RCS-ID:      $Id: iconbndl.h,v 1.4 2002/08/31 11:29:10 GD Exp $
// Copyright:   (c) Mattia Barbon
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ICONBNDL_H_
#define _WX_ICONBNDL_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "iconbndl.h"
#endif

#include "wx/dynarray.h"
// for wxSize
#include "wx/gdicmn.h"

class WXDLLEXPORT wxIcon;
class WXDLLEXPORT wxString;

WX_DECLARE_EXPORTED_OBJARRAY( wxIcon, wxIconArray );

// this class can't load bitmaps of type wxBITMAP_TYPE_ICO_RESOURCE,
// if you need them, you have to load them manually and call
// wxIconCollection::AddIcon
class WXDLLEXPORT wxIconBundle
{
public:
    // default constructor
    wxIconBundle() : m_icons() {}
    // initializes the bundle with the icon(s) found in the file
    wxIconBundle( const wxString& file, long type ) : m_icons()
        { AddIcon( file, type ); }
    // initializes the bundle with a single icon
    wxIconBundle( const wxIcon& icon ) : m_icons()
        { AddIcon( icon ); }

    const wxIconBundle& operator =( const wxIconBundle& ic );
    wxIconBundle( const wxIconBundle& ic ) : m_icons()
        { *this = ic; }

    ~wxIconBundle() { DeleteIcons(); }

    // adds all the icons contained in the file to the collection,
    // if the collection already contains icons with the same
    // width and height, they are replaced
    void AddIcon( const wxString& file, long type );
    // adds the icon to the collection, if the collection already
    // contains an icon with the same width and height, it is
    // replaced
    void AddIcon( const wxIcon& icon );

    // returns the icon with the given size; if no such icon exists,
    // returns the icon with size wxSYS_ICON_[XY]; if no such icon exists,
    // returns the first icon in the bundle
    const wxIcon& GetIcon( const wxSize& size ) const;
    // equivalent to GetIcon( wxSize( size, size ) )
    const wxIcon& GetIcon( wxCoord size = -1 ) const
        { return GetIcon( wxSize( size, size ) ); }
private:
    // delete all icons
    void DeleteIcons();
public:
    wxIconArray m_icons;
};

#endif
    // _WX_ICONBNDL_H_
