/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/dib.h
// Purpose:     Routines for loading and saving DIBs
// Author:      Various
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: dib.h,v 1.12 2001/04/26 16:38:11 VZ Exp $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DIB_H_
#define _WX_DIB_H_

class WXDLLEXPORT wxBitmap;
class WXDLLEXPORT wxPalette;

// WARNING: these functions are private to wxWindows and shouldn't be used
//          by the user code, they risk to disappear in the next versions!

// ----------------------------------------------------------------------------
// Functions for working with DIBs
// ----------------------------------------------------------------------------

// VZ: we have 3 different sets of functions: from bitmap.cpp (wxCreateDIB and
//     wxFreeDIB), from dib.cpp and from dataobj.cpp - surely there is some
//     redundancy between them? (FIXME)

// defined in bitmap.cpp
extern bool wxCreateDIB(long xSize, long ySize, long bitsPerPixel,
                       HPALETTE hPal, LPBITMAPINFO* lpDIBHeader);
extern void wxFreeDIB(LPBITMAPINFO lpDIBHeader);

// defined in ole/dataobj.cpp
extern WXDLLEXPORT size_t wxConvertBitmapToDIB(LPBITMAPINFO pbi, const wxBitmap& bitmap);
extern WXDLLEXPORT wxBitmap wxConvertDIBToBitmap(const LPBITMAPINFO pbi);

// the rest is defined in dib.cpp

// Save (device dependent) wxBitmap as a DIB
bool wxSaveBitmap(wxChar *filename, wxBitmap *bitmap, wxPalette *colourmap = NULL);

// Load device independent bitmap into device dependent bitmap
wxBitmap *wxLoadBitmap(wxChar *filename, wxPalette **colourmap = NULL);

// Load into existing bitmap;
bool wxLoadIntoBitmap(wxChar *filename, wxBitmap *bitmap, wxPalette **pal = NULL);

HANDLE wxBitmapToDIB (HBITMAP hBitmap, HPALETTE hPal);
BOOL   wxReadDIB(LPTSTR lpFileName, HBITMAP *bitmap, HPALETTE *palette);
HANDLE wxReadDIB2(LPTSTR lpFileName);
LPSTR wxFindDIBBits (LPSTR lpbi);
HPALETTE wxMakeDIBPalette(LPBITMAPINFOHEADER lpInfo);

#endif // _WX_DIB_H_

