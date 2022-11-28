/////////////////////////////////////////////////////////////////////////////
// Name:        layout.h
// Purpose:     Layout classes
// Author:      Julian Smart
// Modified by:
// Created:     29/01/98
// RCS-ID:      $Id: layout.h,v 1.15 2002/09/02 18:10:39 VZ Exp $
// Copyright:   (c) 1998 Julian Smart
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_LAYOUTH__
#define _WX_LAYOUTH__

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#if defined(__GNUG__) && !defined(__APPLE__)
    #pragma interface "layout.h"
#endif

#include "wx/object.h"

// X stupidly defines these in X.h
#ifdef Above
    #undef Above
#endif
#ifdef Below
    #undef Below
#endif

// ----------------------------------------------------------------------------
// forward declrations
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxWindowBase;
class WXDLLEXPORT wxLayoutConstraints;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#define wxLAYOUT_DEFAULT_MARGIN 0

enum wxEdge
{
    wxLeft, wxTop, wxRight, wxBottom, wxWidth, wxHeight,
    wxCentre, wxCenter = wxCentre, wxCentreX, wxCentreY
};

enum wxRelationship
{
    wxUnconstrained = 0,
    wxAsIs,
    wxPercentOf,
    wxAbove,
    wxBelow,
    wxLeftOf,
    wxRightOf,
    wxSameAs,
    wxAbsolute
};

// ----------------------------------------------------------------------------
// wxIndividualLayoutConstraint: a constraint on window position
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxIndividualLayoutConstraint : public wxObject
{
    DECLARE_DYNAMIC_CLASS(wxIndividualLayoutConstraint)

protected:
    // To be allowed to modify the internal variables
    friend class wxIndividualLayoutConstraint_Serialize;

    // 'This' window is the parent or sibling of otherWin
    wxWindowBase *otherWin;

    wxEdge myEdge;
    wxRelationship relationship;
    int margin;
    int value;
    int percent;
    wxEdge otherEdge;
    bool done;

public:
    wxIndividualLayoutConstraint();
    ~wxIndividualLayoutConstraint();

    void Set(wxRelationship rel, wxWindowBase *otherW, wxEdge otherE, int val = 0, int marg = wxLAYOUT_DEFAULT_MARGIN);

    //
    // Sibling relationships
    //
    void LeftOf(wxWindowBase *sibling, int marg = wxLAYOUT_DEFAULT_MARGIN);
    void RightOf(wxWindowBase *sibling, int marg = wxLAYOUT_DEFAULT_MARGIN);
    void Above(wxWindowBase *sibling, int marg = wxLAYOUT_DEFAULT_MARGIN);
    void Below(wxWindowBase *sibling, int marg = wxLAYOUT_DEFAULT_MARGIN);

    //
    // 'Same edge' alignment
    //
    void SameAs(wxWindowBase *otherW, wxEdge edge, int marg = wxLAYOUT_DEFAULT_MARGIN);

    // The edge is a percentage of the other window's edge
    void PercentOf(wxWindowBase *otherW, wxEdge wh, int per);

    //
    // Edge has absolute value
    //
    void Absolute(int val);

    //
    // Dimension is unconstrained
    //
    void Unconstrained() { relationship = wxUnconstrained; }

    //
    // Dimension is 'as is' (use current size settings)
    //
    void AsIs() { relationship = wxAsIs; }

    //
    // Accessors
    //
    wxWindowBase *GetOtherWindow() { return otherWin; }
    wxEdge GetMyEdge() const { return myEdge; }
    void SetEdge(wxEdge which) { myEdge = which; }
    void SetValue(int v) { value = v; }
    int GetMargin() { return margin; }
    void SetMargin(int m) { margin = m; }
    int GetValue() const { return value; }
    int GetPercent() const { return percent; }
    int GetOtherEdge() const { return otherEdge; }
    bool GetDone() const { return done; }
    void SetDone(bool d) { done = d; }
    wxRelationship GetRelationship() { return relationship; }
    void SetRelationship(wxRelationship r) { relationship = r; }

    // Reset constraint if it mentions otherWin
    bool ResetIfWin(wxWindowBase *otherW);

    // Try to satisfy constraint
    bool SatisfyConstraint(wxLayoutConstraints *constraints, wxWindowBase *win);

    // Get the value of this edge or dimension, or if this
    // is not determinable, -1.
    int GetEdge(wxEdge which, wxWindowBase *thisWin, wxWindowBase *other) const;
};

// ----------------------------------------------------------------------------
// wxLayoutConstraints: the complete set of constraints for a window
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxLayoutConstraints : public wxObject
{
    DECLARE_DYNAMIC_CLASS(wxLayoutConstraints)

public:
    // Edge constraints
    wxIndividualLayoutConstraint left;
    wxIndividualLayoutConstraint top;
    wxIndividualLayoutConstraint right;
    wxIndividualLayoutConstraint bottom;
    // Size constraints
    wxIndividualLayoutConstraint width;
    wxIndividualLayoutConstraint height;
    // Centre constraints
    wxIndividualLayoutConstraint centreX;
    wxIndividualLayoutConstraint centreY;

    wxLayoutConstraints();
    ~wxLayoutConstraints();

    bool SatisfyConstraints(wxWindowBase *win, int *noChanges);
    bool AreSatisfied() const
    {
        return left.GetDone() && top.GetDone() &&
               width.GetDone() && height.GetDone();
    }
};

#endif
    // _WX_LAYOUTH__
