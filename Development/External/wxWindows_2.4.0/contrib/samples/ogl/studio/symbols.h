/////////////////////////////////////////////////////////////////////////////
// Name:        symbols.h
// Purpose:     Symbol classes (symbol database)
// Author:      Julian Smart
// Modified by:
// Created:     12/07/98
// RCS-ID:      $Id: symbols.h,v 1.3 2002/09/07 12:12:22 GD Exp $
// Copyright:   (c) Julian Smart
// Licence:
/////////////////////////////////////////////////////////////////////////////

#ifndef _STUDIO_SYMBOLS_H_
#define _STUDIO_SYMBOLS_H_

#if defined(__GNUG__) && !defined(__APPLE__)
// #pragma interface
#endif

#include <wx/docview.h>
#include <wx/string.h>
#include <wx/wxexpr.h>

#include <wx/ogl/ogl.h>

/*
 * csSymbol
 * Represents information about a symbol.
 */

class csSymbol: public wxObject
{
public:
    csSymbol(const wxString& name, wxShape* shape);
    ~csSymbol();

    inline void SetName(const wxString& name) { m_name = name; }
    inline wxString GetName() const { return m_name; }

    inline void SetShape(wxShape* shape) { m_shape = shape; }
    inline wxShape* GetShape() const { return m_shape; }

    inline void SetToolId(int id) { m_toolId = id; }
    inline int GetToolId() const { return m_toolId; }
protected:
    wxString    m_name;
    wxShape*    m_shape;
    int         m_toolId;
};

/*
 * A table of all possible shapes.
 * We can use this to construct a palette, etc.
 */
class csSymbolDatabase: public wxObject
{
public:
    csSymbolDatabase();
    ~csSymbolDatabase();

// Accessors
    inline wxList& GetSymbols() const { return (wxList&) m_symbols; }

// Operations
    void AddSymbol(csSymbol* symbol);
    void ClearSymbols();
    csSymbol* FindSymbol(const wxString& name) const;
    csSymbol* FindSymbol(int toolId) const;
    wxBitmap* CreateToolBitmap(csSymbol* symbol, const wxSize& sz);

protected:
    wxList          m_symbols;
    int             m_currentId;
};

#endif
  // _STUDIO_SYMBOLS_H_
