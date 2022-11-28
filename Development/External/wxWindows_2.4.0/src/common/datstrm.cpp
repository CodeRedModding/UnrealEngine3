/////////////////////////////////////////////////////////////////////////////
// Name:        datstrm.cpp
// Purpose:     Data stream classes
// Author:      Guilhem Lavaux
// Modified by:
// Created:     28/06/98
// RCS-ID:      $Id: datstrm.cpp,v 1.36.2.2 2002/10/30 18:01:24 RR Exp $
// Copyright:   (c) Guilhem Lavaux
// Licence:   	wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "datstrm.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
  #pragma hdrstop
#endif

#if wxUSE_STREAMS

#include "wx/datstrm.h"

// ---------------------------------------------------------------------------
// wxDataInputStream
// ---------------------------------------------------------------------------

#if wxUSE_UNICODE
wxDataInputStream::wxDataInputStream(wxInputStream& s, wxMBConv& conv)
  : m_input(&s), m_be_order(FALSE), m_conv(conv)
#else
wxDataInputStream::wxDataInputStream(wxInputStream& s)
  : m_input(&s), m_be_order(FALSE)
#endif
{
}

wxDataInputStream::~wxDataInputStream()
{
}

wxUint64 wxDataInputStream::Read64()
{
  wxUint64 i64;

  m_input->Read(&i64, 8);

  if (m_be_order)
    return wxUINT64_SWAP_ON_LE(i64);
  else
    return wxUINT64_SWAP_ON_BE(i64);
}

wxUint32 wxDataInputStream::Read32()
{
  wxUint32 i32;

  m_input->Read(&i32, 4);

  if (m_be_order)
    return wxUINT32_SWAP_ON_LE(i32);
  else
    return wxUINT32_SWAP_ON_BE(i32);
}

wxUint16 wxDataInputStream::Read16()
{
  wxUint16 i16;

  m_input->Read(&i16, 2);

  if (m_be_order)
    return wxUINT16_SWAP_ON_LE(i16);
  else
    return wxUINT16_SWAP_ON_BE(i16);
}

wxUint8 wxDataInputStream::Read8()
{
  wxUint8 buf;

  m_input->Read(&buf, 1);
  return (wxUint8)buf;
}

// Must be at global scope for VC++ 5
extern "C" double ConvertFromIeeeExtended(const unsigned char *bytes);

double wxDataInputStream::ReadDouble()
{
#if wxUSE_APPLE_IEEE
  char buf[10];

  m_input->Read(buf, 10);
  return ConvertFromIeeeExtended((unsigned char *)buf);
#else
  return 0.0;
#endif
}

wxString wxDataInputStream::ReadString()
{
  size_t len;

  len = Read32();

  if (len > 0)
  {
#if wxUSE_UNICODE
    char *tmp = new char[len + 1];
    m_input->Read(tmp, len);
    tmp[len] = 0;
    wxString ret( (const wxChar*) m_conv.cMB2WX(tmp) );
    delete[] tmp;
#else
    wxString ret;
    m_input->Read( ret.GetWriteBuf(len), len);
    ret.UngetWriteBuf();
#endif
    return ret;
  }
  else
    return wxEmptyString;
}

wxDataInputStream& wxDataInputStream::operator>>(wxString& s)
{
  s = ReadString();
  return *this;
}

wxDataInputStream& wxDataInputStream::operator>>(wxInt8& c)
{
  c = (wxInt8)Read8();
  return *this;
}

wxDataInputStream& wxDataInputStream::operator>>(wxInt16& i)
{
  i = (wxInt16)Read16();
  return *this;
}

wxDataInputStream& wxDataInputStream::operator>>(wxInt32& i)
{
  i = (wxInt32)Read32();
  return *this;
}

wxDataInputStream& wxDataInputStream::operator>>(wxUint8& c)
{
  c = Read8();
  return *this;
}

wxDataInputStream& wxDataInputStream::operator>>(wxUint16& i)
{
  i = Read16();
  return *this;
}

wxDataInputStream& wxDataInputStream::operator>>(wxUint32& i)
{
  i = Read32();
  return *this;
}

wxDataInputStream& wxDataInputStream::operator>>(wxUint64& i)
{
  i = Read64();
  return *this;
}

wxDataInputStream& wxDataInputStream::operator>>(double& i)
{
  i = ReadDouble();
  return *this;
}

wxDataInputStream& wxDataInputStream::operator>>(float& f)
{
  f = (float)ReadDouble();
  return *this;
}

// ---------------------------------------------------------------------------
// wxDataOutputStream
// ---------------------------------------------------------------------------

#if wxUSE_UNICODE
wxDataOutputStream::wxDataOutputStream(wxOutputStream& s, wxMBConv& conv)
  : m_output(&s), m_be_order(FALSE), m_conv(conv)
#else
wxDataOutputStream::wxDataOutputStream(wxOutputStream& s)
  : m_output(&s), m_be_order(FALSE)
#endif
{
}

wxDataOutputStream::~wxDataOutputStream()
{
}

void wxDataOutputStream::Write64(wxUint64 i)
{
  wxUint64 i64;

  if (m_be_order)
    i64 = wxUINT64_SWAP_ON_LE(i);
  else
    i64 = wxUINT64_SWAP_ON_BE(i);
  m_output->Write(&i64, 8);
}

void wxDataOutputStream::Write32(wxUint32 i)
{
  wxUint32 i32;

  if (m_be_order)
    i32 = wxUINT32_SWAP_ON_LE(i);
  else
    i32 = wxUINT32_SWAP_ON_BE(i);
  m_output->Write(&i32, 4);
}

void wxDataOutputStream::Write16(wxUint16 i)
{
  wxUint16 i16;

  if (m_be_order)
    i16 = wxUINT16_SWAP_ON_LE(i);
  else
    i16 = wxUINT16_SWAP_ON_BE(i);

  m_output->Write(&i16, 2);
}

void wxDataOutputStream::Write8(wxUint8 i)
{
  m_output->Write(&i, 1);
}

void wxDataOutputStream::WriteString(const wxString& string)
{
#if wxUSE_UNICODE
  const wxWX2MBbuf buf = string.mb_str(m_conv);
#else
  const wxWX2MBbuf buf = string.mb_str();
#endif
  size_t len = strlen(buf);
  Write32(len);
  if (len > 0)
      m_output->Write(buf, len);
}

// Must be at global scope for VC++ 5
extern "C" void ConvertToIeeeExtended(double num, unsigned char *bytes);

void wxDataOutputStream::WriteDouble(double d)
{
  char buf[10];

#if wxUSE_APPLE_IEEE
  ConvertToIeeeExtended(d, (unsigned char *)buf);
#else
#ifndef __VMS__
# pragma warning "wxDataOutputStream::WriteDouble() not using IeeeExtended - will not work!"
#endif
   buf[0] = '\0';
#endif
  m_output->Write(buf, 10);
}

wxDataOutputStream& wxDataOutputStream::operator<<(const wxChar *string)
{
  Write32(wxStrlen(string));
  m_output->Write((const char *)string, wxStrlen(string)*sizeof(wxChar));
  return *this;
}

wxDataOutputStream& wxDataOutputStream::operator<<(const wxString& string)
{
  WriteString(string);
  return *this;
}

wxDataOutputStream& wxDataOutputStream::operator<<(wxInt8 c)
{
  Write8((wxUint8)c);
  return *this;
}

wxDataOutputStream& wxDataOutputStream::operator<<(wxInt16 i)
{
  Write16((wxUint16)i);
  return *this;
}

wxDataOutputStream& wxDataOutputStream::operator<<(wxInt32 i)
{
  Write32((wxUint32)i);
  return *this;
}

wxDataOutputStream& wxDataOutputStream::operator<<(wxUint8 c)
{
  Write8(c);
  return *this;
}

wxDataOutputStream& wxDataOutputStream::operator<<(wxUint16 i)
{
  Write16(i);
  return *this;
}

wxDataOutputStream& wxDataOutputStream::operator<<(wxUint32 i)
{
  Write32(i);
  return *this;
}

wxDataOutputStream& wxDataOutputStream::operator<<(wxUint64 i)
{
  Write64(i);
  return *this;
}

wxDataOutputStream& wxDataOutputStream::operator<<(double f)
{
  WriteDouble(f);
  return *this;
}

wxDataOutputStream& wxDataOutputStream::operator<<(float f)
{
  WriteDouble((double)f);
  return *this;
}

#endif
  // wxUSE_STREAMS

