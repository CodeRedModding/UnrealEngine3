/////////////////////////////////////////////////////////////////////////////
// Name:        m_tables.cpp
// Purpose:     wxHtml module for tables
// Author:      Vaclav Slavik
// RCS-ID:      $Id: m_tables.cpp,v 1.27.2.3 2002/11/09 00:07:36 VS Exp $
// Copyright:   (c) 1999 Vaclav Slavik
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation
#endif

#include "wx/wxprec.h"

#include "wx/defs.h"
#if wxUSE_HTML && wxUSE_STREAMS
#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WXPRECOMP
#endif


/*
REMARKS:
    1. This version of m_tables doesn't support auto-layout algorithm.
       This means that all columns are of same width unless explicitly specified.
*/


#include "wx/html/forcelnk.h"
#include "wx/html/m_templ.h"

#include "wx/html/htmlcell.h"

FORCE_LINK_ME(m_tables)


#define TABLE_BORDER_CLR_1  wxColour(0xC5, 0xC2, 0xC5)
#define TABLE_BORDER_CLR_2  wxColour(0x62, 0x61, 0x62)


//-----------------------------------------------------------------------------
// wxHtmlTableCell
//-----------------------------------------------------------------------------


struct colStruct
{
    int width, units;
            // width of the column either in pixels or percents
            // ('width' is the number, 'units' determines its meaning)
    int minWidth, maxWidth;
            // minimal/maximal column width. This is needed by HTML 4.0
            // layouting algorithm and can be determined by trying to
            // layout table cells with width=1 and width=infinity
    int leftpos, pixwidth, maxrealwidth;
            // temporary (depends on actual width of table)
};

enum cellState
{
    cellSpan,
    cellUsed,
    cellFree
};

struct cellStruct
{
    wxHtmlContainerCell *cont;
    int colspan, rowspan;
    int minheight, valign;
    cellState flag;
};


class wxHtmlTableCell : public wxHtmlContainerCell
{
protected:
    /* These are real attributes: */

    // should we draw borders or not?
    bool m_HasBorders;
    // number of columns; rows
    int m_NumCols, m_NumRows;
    // array of column information
    colStruct *m_ColsInfo;
    // 2D array of all cells in the table : m_CellInfo[row][column]
    cellStruct **m_CellInfo;
    // spaces between cells
    int m_Spacing;
    // cells internal indentation
    int m_Padding;

private:
    /* ...and these are valid only when parsing the table: */

    // number of actual column (ranging from 0..m_NumCols)
    int m_ActualCol, m_ActualRow;

    // default values (for table and row):
    wxColour m_tBkg, m_rBkg;
    wxString m_tValign, m_rValign;

    double m_PixelScale;


public:
    wxHtmlTableCell(wxHtmlContainerCell *parent, const wxHtmlTag& tag, double pixel_scale = 1.0);
    ~wxHtmlTableCell();
    virtual void Layout(int w);

    void AddRow(const wxHtmlTag& tag);
    void AddCell(wxHtmlContainerCell *cell, const wxHtmlTag& tag);

private:
    // Reallocates memory to given number of cols/rows
    // and changes m_NumCols/m_NumRows value to reflect this change
    // NOTE! You CAN'T change m_NumCols/m_NumRows before calling this!!
    void ReallocCols(int cols);
    void ReallocRows(int rows);

    // Computes minimal and maximal widths of columns. Needs to be called
    // only once, before first Layout(). 
    void ComputeMinMaxWidths();
};



wxHtmlTableCell::wxHtmlTableCell(wxHtmlContainerCell *parent, const wxHtmlTag& tag, double pixel_scale)
 : wxHtmlContainerCell(parent)
{
    m_PixelScale = pixel_scale;
    m_HasBorders = 
            (tag.HasParam(wxT("BORDER")) && tag.GetParam(wxT("BORDER")) != wxT("0"));
    m_ColsInfo = NULL;
    m_NumCols = m_NumRows = 0;
    m_CellInfo = NULL;
    m_ActualCol = m_ActualRow = -1;

    /* scan params: */
    if (tag.HasParam(wxT("BGCOLOR")))
        tag.GetParamAsColour(wxT("BGCOLOR"), &m_tBkg);
    if (tag.HasParam(wxT("VALIGN")))
        m_tValign = tag.GetParam(wxT("VALIGN"));
    else
        m_tValign = wxEmptyString;
    if (!tag.GetParamAsInt(wxT("CELLSPACING"), &m_Spacing))
        m_Spacing = 2;
    if (!tag.GetParamAsInt(wxT("CELLPADDING"), &m_Padding))
        m_Padding = 3;
    m_Spacing = (int)(m_PixelScale * (double)m_Spacing);
    m_Padding = (int)(m_PixelScale * (double)m_Padding);

    if (m_HasBorders)
        SetBorder(TABLE_BORDER_CLR_1, TABLE_BORDER_CLR_2);
}



wxHtmlTableCell::~wxHtmlTableCell()
{
    if (m_ColsInfo) free(m_ColsInfo);
    if (m_CellInfo)
    {
        for (int i = 0; i < m_NumRows; i++)
            free(m_CellInfo[i]);
        free(m_CellInfo);
    }
}



void wxHtmlTableCell::ReallocCols(int cols)
{
    int i,j;

    for (i = 0; i < m_NumRows; i++)
    {
        m_CellInfo[i] = (cellStruct*) realloc(m_CellInfo[i], sizeof(cellStruct) * cols);
        for (j = m_NumCols; j < cols; j++)
            m_CellInfo[i][j].flag = cellFree;
    }

    m_ColsInfo = (colStruct*) realloc(m_ColsInfo, sizeof(colStruct) * cols);
    for (j = m_NumCols; j < cols; j++)
    {
           m_ColsInfo[j].width = 0;
           m_ColsInfo[j].units = wxHTML_UNITS_PERCENT;
           m_ColsInfo[j].minWidth = m_ColsInfo[j].maxWidth = -1;
    }

    m_NumCols = cols;
}



void wxHtmlTableCell::ReallocRows(int rows)
{
    m_CellInfo = (cellStruct**) realloc(m_CellInfo, sizeof(cellStruct*) * rows);
    for (int row = m_NumRows; row < rows ; row++)
    {
        if (m_NumCols == 0)
            m_CellInfo[row] = NULL;
        else
        {
            m_CellInfo[row] = (cellStruct*) malloc(sizeof(cellStruct) * m_NumCols);
            for (int col = 0; col < m_NumCols; col++)
                m_CellInfo[row][col].flag = cellFree;
        }
    }
    m_NumRows = rows;
}


void wxHtmlTableCell::AddRow(const wxHtmlTag& tag)
{
    m_ActualCol = -1;
    // VS: real allocation of row entry is done in AddCell in order
    //     to correctly handle empty rows (i.e. "<tr></tr>")
    //     m_ActualCol == -1 indicates that AddCell has to allocate new row.

    // scan params:
    m_rBkg = m_tBkg;
    if (tag.HasParam(wxT("BGCOLOR")))
        tag.GetParamAsColour(wxT("BGCOLOR"), &m_rBkg);
    if (tag.HasParam(wxT("VALIGN")))
        m_rValign = tag.GetParam(wxT("VALIGN"));
    else
        m_rValign = m_tValign;
}



void wxHtmlTableCell::AddCell(wxHtmlContainerCell *cell, const wxHtmlTag& tag)
{
    // Is this cell in new row?
    // VS: we can't do it in AddRow, see my comment there
    if (m_ActualCol == -1)
    {
        if (m_ActualRow + 1 > m_NumRows - 1)
            ReallocRows(m_ActualRow + 2);
        m_ActualRow++;
    }

    // cells & columns:
    do
    {
        m_ActualCol++;
    } while ((m_ActualCol < m_NumCols) &&
             (m_CellInfo[m_ActualRow][m_ActualCol].flag != cellFree));

    if (m_ActualCol > m_NumCols - 1)
        ReallocCols(m_ActualCol + 1);

    int r = m_ActualRow, c = m_ActualCol;

    m_CellInfo[r][c].cont = cell;
    m_CellInfo[r][c].colspan = 1;
    m_CellInfo[r][c].rowspan = 1;
    m_CellInfo[r][c].flag = cellUsed;
    m_CellInfo[r][c].minheight = 0;
    m_CellInfo[r][c].valign = wxHTML_ALIGN_TOP;

    /* scan for parameters: */

    // width:
    {
        if (tag.HasParam(wxT("WIDTH")))
        {
            wxString wd = tag.GetParam(wxT("WIDTH"));

            if (wd[wd.Length()-1] == wxT('%'))
            {
                wxSscanf(wd.c_str(), wxT("%i%%"), &m_ColsInfo[c].width);
                m_ColsInfo[c].units = wxHTML_UNITS_PERCENT;
            }
            else
            {
                wxSscanf(wd.c_str(), wxT("%i"), &m_ColsInfo[c].width);
                m_ColsInfo[c].width = (int)(m_PixelScale * (double)m_ColsInfo[c].width);
                m_ColsInfo[c].units = wxHTML_UNITS_PIXELS;
            }
        }
    }


    // spanning:
    {
        tag.GetParamAsInt(wxT("COLSPAN"), &m_CellInfo[r][c].colspan);
        tag.GetParamAsInt(wxT("ROWSPAN"), &m_CellInfo[r][c].rowspan);

        // VS: the standard says this about col/rowspan:
        //     "This attribute specifies the number of rows spanned by the 
        //     current cell. The default value of this attribute is one ("1"). 
        //     The value zero ("0") means that the cell spans all rows from the 
        //     current row to the last row of the table." All mainstream 
        //     browsers act as if 0==1, though, and so does wxHTML.
        if (m_CellInfo[r][c].colspan < 1) 
            m_CellInfo[r][c].colspan = 1;
        if (m_CellInfo[r][c].rowspan < 1) 
            m_CellInfo[r][c].rowspan = 1;

        if ((m_CellInfo[r][c].colspan > 1) || (m_CellInfo[r][c].rowspan > 1))
        {
            int i, j;

            if (r + m_CellInfo[r][c].rowspan > m_NumRows)
                ReallocRows(r + m_CellInfo[r][c].rowspan);
            if (c + m_CellInfo[r][c].colspan > m_NumCols)
                ReallocCols(c + m_CellInfo[r][c].colspan);
            for (i = r; i < r + m_CellInfo[r][c].rowspan; i++)
                for (j = c; j < c + m_CellInfo[r][c].colspan; j++)
                    m_CellInfo[i][j].flag = cellSpan;
            m_CellInfo[r][c].flag = cellUsed;
        }
    }

    //background color:
    {
        wxColour bk = m_rBkg;
        if (tag.HasParam(wxT("BGCOLOR")))
            tag.GetParamAsColour(wxT("BGCOLOR"), &bk);
        if (bk.Ok())
            cell->SetBackgroundColour(bk);
    }
    if (m_HasBorders)
        cell->SetBorder(TABLE_BORDER_CLR_2, TABLE_BORDER_CLR_1);

    // vertical alignment:
    {
        wxString valign;
        if (tag.HasParam(wxT("VALIGN")))
            valign = tag.GetParam(wxT("VALIGN"));
        else
            valign = m_tValign;
        valign.MakeUpper();
        if (valign == wxT("TOP"))
            m_CellInfo[r][c].valign = wxHTML_ALIGN_TOP;
        else if (valign == wxT("BOTTOM"))
            m_CellInfo[r][c].valign = wxHTML_ALIGN_BOTTOM;
        else m_CellInfo[r][c].valign = wxHTML_ALIGN_CENTER;
    }

    cell->SetIndent(m_Padding, wxHTML_INDENT_ALL, wxHTML_UNITS_PIXELS);
}

void wxHtmlTableCell::ComputeMinMaxWidths()
{
    if (m_NumCols == 0 || m_ColsInfo[0].minWidth != -1) return;
    
    int left, right, width;

    for (int c = 0; c < m_NumCols; c++)
    {
        for (int r = 0; r < m_NumRows; r++)
        {
            cellStruct& cell = m_CellInfo[r][c];
            if (cell.flag == cellUsed)
            {
                cell.cont->Layout(2*m_Padding + 1);
                cell.cont->GetHorizontalConstraints(&left, &right);
                width = right - left;
                width -= (cell.colspan-1) * m_Spacing;
                // HTML 4.0 says it is acceptable to distribute min/max
                // width of spanning cells evently
                width /= cell.colspan;
                for (int j = 0; j < cell.colspan; j++)
                    if (width > m_ColsInfo[c+j].minWidth)
                        m_ColsInfo[c+j].minWidth = width;
            }
        }
    }
    
    // FIXME -- compute maxWidth as well. Not needed yet, so there's no
    //          point in computing it. 
}


void wxHtmlTableCell::Layout(int w)
{
    ComputeMinMaxWidths();
    
    wxHtmlCell::Layout(w);

    /*

    WIDTH ADJUSTING :

    */

    if (m_WidthFloatUnits == wxHTML_UNITS_PERCENT)
    {
        if (m_WidthFloat < 0) m_Width = (100 + m_WidthFloat) * w / 100;
        else m_Width = m_WidthFloat * w / 100;
    }
    else
    {
        if (m_WidthFloat < 0) m_Width = w + m_WidthFloat;
        else m_Width = m_WidthFloat;
    }


    /*

    LAYOUTING :

    */

    /* 1.  setup columns widths: */
    {
        int wpix = m_Width - (m_NumCols + 1) * m_Spacing;
        int i, j;

        // 1a. setup fixed-width columns:
        for (i = 0; i < m_NumCols; i++)
            if (m_ColsInfo[i].units == wxHTML_UNITS_PIXELS)
            {
                m_ColsInfo[i].pixwidth = wxMax(m_ColsInfo[i].width, 
                                               m_ColsInfo[i].minWidth);
                wpix -= m_ColsInfo[i].pixwidth;
            }

        // 1b. setup floating-width columns:
        int wtemp = 0;
        for (i = 0; i < m_NumCols; i++)
            if ((m_ColsInfo[i].units == wxHTML_UNITS_PERCENT) && (m_ColsInfo[i].width != 0))
            {
                m_ColsInfo[i].pixwidth = wxMax(m_ColsInfo[i].width * wpix / 100,
                                               m_ColsInfo[i].minWidth);
                wtemp += m_ColsInfo[i].pixwidth;
            }
        wpix -= wtemp;

        // 1c. setup defalut columns (no width specification supplied):
        // NOTE! This algorithm doesn't conform to HTML standard : it assigns equal widths
        // instead of optimal
        for (i = j = 0; i < m_NumCols; i++)
            if (m_ColsInfo[i].width == 0) j++;
        for (i = 0; i < m_NumCols; i++)
            if (m_ColsInfo[i].width == 0)
                m_ColsInfo[i].pixwidth = wpix / j;
    }

    /* 2.  compute positions of columns: */
    {
        int wpos = m_Spacing;
        for (int i = 0; i < m_NumCols; i++)
        {
            m_ColsInfo[i].leftpos = wpos;
            wpos += m_ColsInfo[i].pixwidth + m_Spacing;
        }
    }

    /* 3.  sub-layout all cells: */
    {
        int *ypos = new int[m_NumRows + 1];

        int actcol, actrow;
        int fullwid;
        wxHtmlContainerCell *actcell;

        ypos[0] = m_Spacing;
        for (actrow = 1; actrow <= m_NumRows; actrow++) ypos[actrow] = -1;
        for (actrow = 0; actrow < m_NumRows; actrow++)
        {
            if (ypos[actrow] == -1) ypos[actrow] = ypos[actrow-1];
            // 3a. sub-layout and detect max height:

            for (actcol = 0; actcol < m_NumCols; actcol++) {
                if (m_CellInfo[actrow][actcol].flag != cellUsed) continue;
                actcell = m_CellInfo[actrow][actcol].cont;
                fullwid = 0;
                for (int i = actcol; i < m_CellInfo[actrow][actcol].colspan + actcol; i++)
                    fullwid += m_ColsInfo[i].pixwidth;
                fullwid += (m_CellInfo[actrow][actcol].colspan - 1) * m_Spacing;
                actcell->SetMinHeight(m_CellInfo[actrow][actcol].minheight, m_CellInfo[actrow][actcol].valign);
                actcell->Layout(fullwid);

                if (ypos[actrow] + actcell->GetHeight() + m_CellInfo[actrow][actcol].rowspan * m_Spacing > ypos[actrow + m_CellInfo[actrow][actcol].rowspan])
                    ypos[actrow + m_CellInfo[actrow][actcol].rowspan] =
                            ypos[actrow] + actcell->GetHeight() + m_CellInfo[actrow][actcol].rowspan * m_Spacing;
            }
        }

        for (actrow = 0; actrow < m_NumRows; actrow++)
        {
            // 3b. place cells in row & let'em all have same height:

            for (actcol = 0; actcol < m_NumCols; actcol++)
            {
                if (m_CellInfo[actrow][actcol].flag != cellUsed) continue;
                actcell = m_CellInfo[actrow][actcol].cont;
                actcell->SetMinHeight(
                                 ypos[actrow + m_CellInfo[actrow][actcol].rowspan] - ypos[actrow] -  m_Spacing,
                                 m_CellInfo[actrow][actcol].valign);
                fullwid = 0;
                for (int i = actcol; i < m_CellInfo[actrow][actcol].colspan + actcol; i++)
                    fullwid += m_ColsInfo[i].pixwidth;
                fullwid += (m_CellInfo[actrow][actcol].colspan - 1) * m_Spacing;
                actcell->Layout(fullwid);
                actcell->SetPos(m_ColsInfo[actcol].leftpos, ypos[actrow]);
            }
        }
        m_Height = ypos[m_NumRows];
        delete[] ypos;
    }
}






//-----------------------------------------------------------------------------
// The tables handler:
//-----------------------------------------------------------------------------


TAG_HANDLER_BEGIN(TABLE, "TABLE,TR,TD,TH")

    TAG_HANDLER_VARS
        wxHtmlTableCell* m_Table;
        wxString m_tAlign, m_rAlign;

    TAG_HANDLER_CONSTR(TABLE)
    {
        m_Table = NULL;
        m_tAlign = m_rAlign = wxEmptyString;
    }


    TAG_HANDLER_PROC(tag)
    {
        wxHtmlContainerCell *c;

        // new table started, backup upper-level table (if any) and create new:
        if (tag.GetName() == wxT("TABLE"))
        {
            wxHtmlTableCell *oldt = m_Table;
            wxHtmlContainerCell *oldcont;

            oldcont = c = m_WParser->OpenContainer();

            c->SetWidthFloat(tag, m_WParser->GetPixelScale());
            m_Table = new wxHtmlTableCell(c, tag, m_WParser->GetPixelScale());
            int oldAlign = m_WParser->GetAlign();
            m_tAlign = wxEmptyString;
            if (tag.HasParam(wxT("ALIGN")))
                m_tAlign = tag.GetParam(wxT("ALIGN"));

            ParseInner(tag);

            m_WParser->SetAlign(oldAlign);
            m_WParser->SetContainer(oldcont);
            m_WParser->CloseContainer();
            
            m_Table = oldt;
            return TRUE;
        }


        else if (m_Table)
        {
            // new row in table
            if (tag.GetName() == wxT("TR"))
            {
                m_Table->AddRow(tag);
                m_rAlign = m_tAlign;
                if (tag.HasParam(wxT("ALIGN")))
                    m_rAlign = tag.GetParam(wxT("ALIGN"));
            }

            // new cell
            else
            {
                c = m_WParser->SetContainer(new wxHtmlContainerCell(m_Table));
                m_Table->AddCell(c, tag);

                m_WParser->OpenContainer();

                if (tag.GetName() == wxT("TH")) /*header style*/
                    m_WParser->SetAlign(wxHTML_ALIGN_CENTER);
                else
                    m_WParser->SetAlign(wxHTML_ALIGN_LEFT);

                wxString als;

                als = m_rAlign;
                if (tag.HasParam(wxT("ALIGN")))
                    als = tag.GetParam(wxT("ALIGN"));
                als.MakeUpper();
                if (als == wxT("RIGHT"))
                    m_WParser->SetAlign(wxHTML_ALIGN_RIGHT);
                else if (als == wxT("LEFT"))
                    m_WParser->SetAlign(wxHTML_ALIGN_LEFT);
                else if (als == wxT("CENTER"))
                    m_WParser->SetAlign(wxHTML_ALIGN_CENTER);

                m_WParser->OpenContainer();
            }
        }
        return FALSE;
    }

TAG_HANDLER_END(TABLE)





TAGS_MODULE_BEGIN(Tables)

    TAGS_MODULE_ADD(TABLE)

TAGS_MODULE_END(Tables)


#endif
