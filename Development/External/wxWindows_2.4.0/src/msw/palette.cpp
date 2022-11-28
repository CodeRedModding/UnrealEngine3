/////////////////////////////////////////////////////////////////////////////
// Name:        palette.cpp
// Purpose:     wxPalette
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: palette.cpp,v 1.7 2001/09/30 22:06:39 VZ Exp $
// Copyright:   (c) Julian Smart and Markus Holzem
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "palette.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_PALETTE

#ifndef WX_PRECOMP
    #include "wx/palette.h"
#endif

#include "wx/msw/private.h"

IMPLEMENT_DYNAMIC_CLASS(wxPalette, wxGDIObject)

/*
 * Palette
 *
 */

wxPaletteRefData::wxPaletteRefData(void)
{
  m_hPalette = 0;
}

wxPaletteRefData::~wxPaletteRefData(void)
{
    if ( m_hPalette )
        ::DeleteObject((HPALETTE) m_hPalette);
}

wxPalette::wxPalette(void)
{
}

wxPalette::wxPalette(int n, const unsigned char *red, const unsigned char *green, const unsigned char *blue)
{
  Create(n, red, green, blue);
}

wxPalette::~wxPalette(void)
{
//    FreeResource(TRUE);
}

bool wxPalette::FreeResource(bool WXUNUSED(force))
{
    if ( M_PALETTEDATA && M_PALETTEDATA->m_hPalette)
    {
      DeleteObject((HPALETTE)M_PALETTEDATA->m_hPalette);
    }
    return TRUE;
}

bool wxPalette::Create(int n, const unsigned char *red, const unsigned char *green, const unsigned char *blue)
{
  UnRef();

#if defined(__WXWINE__) || defined(__WXMICROWIN__)

  return (FALSE);

#else

  m_refData = new wxPaletteRefData;

  NPLOGPALETTE npPal = (NPLOGPALETTE)LocalAlloc(LMEM_FIXED, sizeof(LOGPALETTE) +
                        (WORD)n * sizeof(PALETTEENTRY));
  if (!npPal)
    return(FALSE);

  npPal->palVersion = 0x300;
  npPal->palNumEntries = n;

  int i;
  for (i = 0; i < n; i ++)
  {
    npPal->palPalEntry[i].peRed = red[i];
    npPal->palPalEntry[i].peGreen = green[i];
    npPal->palPalEntry[i].peBlue = blue[i];
    npPal->palPalEntry[i].peFlags = 0;
  }
  M_PALETTEDATA->m_hPalette = (WXHPALETTE) CreatePalette((LPLOGPALETTE)npPal);
  LocalFree((HANDLE)npPal);
  return TRUE;

#endif
}

int wxPalette::GetPixel(const unsigned char red, const unsigned char green, const unsigned char blue) const
{
#ifdef __WXMICROWIN__
  return FALSE;
#else
  if ( !m_refData )
    return FALSE;

  return ::GetNearestPaletteIndex((HPALETTE) M_PALETTEDATA->m_hPalette, PALETTERGB(red, green, blue));
#endif
}

bool wxPalette::GetRGB(int index, unsigned char *red, unsigned char *green, unsigned char *blue) const
{
#ifdef __WXMICROWIN__
  return FALSE;
#else
  if ( !m_refData )
    return FALSE;

  if (index < 0 || index > 255)
         return FALSE;

  PALETTEENTRY entry;
  if (::GetPaletteEntries((HPALETTE) M_PALETTEDATA->m_hPalette, index, 1, &entry))
  {
         *red = entry.peRed;
         *green = entry.peGreen;
         *blue = entry.peBlue;
         return TRUE;
  } else
         return FALSE;
#endif
}

void wxPalette::SetHPALETTE(WXHPALETTE pal)
{
    if ( !m_refData )
        m_refData = new wxPaletteRefData;

    M_PALETTEDATA->m_hPalette = pal;
}

#endif // wxUSE_PALETTE

