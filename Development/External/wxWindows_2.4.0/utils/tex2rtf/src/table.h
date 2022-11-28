/////////////////////////////////////////////////////////////////////////////
// Name:        table.h
// Purpose:     Table utilities
// Author:      Julian Smart
// Modified by:
// Created:     7.9.93
// RCS-ID:      $Id: table.h,v 1.1 1999/01/02 00:45:09 JS Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

/*
 * Table dimensions
 *
 */

struct ColumnData
{
  char justification; // l, r, c
  int width;          // -1 or a width in twips
  int spacing;        // Space between columns in twips
  bool leftBorder;
  bool rightBorder;
  bool absWidth;      // If FALSE (the default), don't use an absolute width if you can help it.
};

extern ColumnData TableData[];
extern bool inTabular;
extern bool startRows;
extern bool tableVerticalLineLeft;
extern bool tableVerticalLineRight;
extern int noColumns;   // Current number of columns in table
extern int ruleTop;
extern int ruleBottom;
extern int currentRowNumber;
extern bool ParseTableArgument(char *value);
