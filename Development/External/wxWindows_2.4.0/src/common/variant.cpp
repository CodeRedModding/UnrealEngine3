/////////////////////////////////////////////////////////////////////////////
// Name:        variant.cpp
// Purpose:     wxVariant class, container for any type
// Author:      Julian Smart
// Modified by:
// Created:     10/09/98
// RCS-ID:      $Id: variant.cpp,v 1.38 2002/08/01 20:00:55 JS Exp $
// Copyright:   (c)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation "variant.h"
#pragma implementation "time.h"
#pragma implementation "date.h"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_STD_IOSTREAM
    #if wxUSE_IOSTREAMH
        #include <fstream.h>
    #else
        #include <fstream>
    #endif
#endif

#if wxUSE_STREAMS
#include "wx/stream.h"
#include "wx/txtstrm.h"
#endif

#include "wx/string.h"
#include "wx/tokenzr.h"

#include "wx/variant.h"

#if wxUSE_TIMEDATE
IMPLEMENT_DYNAMIC_CLASS(wxDate, wxObject)
IMPLEMENT_DYNAMIC_CLASS(wxTime, wxObject)

wxTime::tFormat    wxTime::ms_Format    = wxTime::wx12h;
wxTime::tPrecision wxTime::ms_Precision  = wxTime::wxStdMinSec;
wxChar             wxTime::ms_bufTime[128];
#endif

IMPLEMENT_ABSTRACT_CLASS(wxVariantData, wxObject)

wxVariant WXDLLEXPORT wxNullVariant;

/*
 * wxVariantDataList
 */

class WXDLLEXPORT wxVariantDataList: public wxVariantData
{
DECLARE_DYNAMIC_CLASS(wxVariantDataList)
public:
    wxVariantDataList() {}
    wxVariantDataList(const wxList& list);
    ~wxVariantDataList();

    wxList& GetValue() const { return (wxList&) m_value; }
    void SetValue(const wxList& value) ;

    virtual void Copy(wxVariantData& data);
    virtual bool Eq(wxVariantData& data) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Write(wxSTD ostream& str) const;
#endif
    virtual bool Write(wxString& str) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Read(wxSTD istream& str);
#endif
    virtual bool Read(wxString& str);
    virtual wxString GetType() const { return wxT("list"); };

    void Clear();

protected:
    wxList  m_value;
};

IMPLEMENT_DYNAMIC_CLASS(wxVariantDataList, wxVariantData)

wxVariantDataList::wxVariantDataList(const wxList& list)
{
    SetValue(list);
}

wxVariantDataList::~wxVariantDataList()
{
    Clear();
}

void wxVariantDataList::SetValue(const wxList& value)
{
    Clear();
    wxNode* node = value.First();
    while (node)
    {
        wxVariant* var = (wxVariant*) node->Data();
        m_value.Append(new wxVariant(*var));
        node = node->Next();
    }
}

void wxVariantDataList::Clear()
{
    wxNode* node = m_value.First();
    while (node)
    {
        wxVariant* var = (wxVariant*) node->Data();
        delete var;
        node = node->Next();
    }
    m_value.Clear();
}

void wxVariantDataList::Copy(wxVariantData& data)
{
    wxASSERT_MSG( (data.GetType() == wxT("list")), wxT("wxVariantDataList::Copy: Can't copy to this type of data") );

    wxVariantDataList& listData = (wxVariantDataList&) data;

    listData.Clear();
    wxNode* node = m_value.First();
    while (node)
    {
        wxVariant* var = (wxVariant*) node->Data();
        listData.m_value.Append(new wxVariant(*var));
        node = node->Next();
    }
}

bool wxVariantDataList::Eq(wxVariantData& data) const
{
    wxASSERT_MSG( (data.GetType() == wxT("list")), wxT("wxVariantDataList::Eq: argument mismatch") );

    wxVariantDataList& listData = (wxVariantDataList&) data;
    wxNode* node1 = m_value.First();
    wxNode* node2 = listData.GetValue().First();
    while (node1 && node2)
    {
        wxVariant* var1 = (wxVariant*) node1->Data();
        wxVariant* var2 = (wxVariant*) node2->Data();
        if ((*var1) != (*var2))
            return FALSE;
        node1 = node1->Next();
        node2 = node2->Next();
    }
    if (node1 || node2) return FALSE;
    return TRUE;
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataList::Write(wxSTD ostream& str) const
{
    wxString s;
    Write(s);
    str << (const char*) s.mb_str();
    return TRUE;
}
#endif

bool wxVariantDataList::Write(wxString& str) const
{
    str = wxT("");
    wxNode* node = m_value.First();
    while (node)
    {
        wxVariant* var = (wxVariant*) node->Data();
        if (node != m_value.First())
          str += wxT(" ");
        wxString str1;
        str += var->MakeString();
        node = node->Next();
    }

    return TRUE;
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataList::Read(wxSTD istream& WXUNUSED(str))
{
    wxFAIL_MSG(wxT("Unimplemented"));
    // TODO
    return FALSE;
}
#endif

bool wxVariantDataList::Read(wxString& WXUNUSED(str))
{
    wxFAIL_MSG(wxT("Unimplemented"));
    // TODO
    return FALSE;
}

/*
 * wxVariantDataStringList
 */

class WXDLLEXPORT wxVariantDataStringList: public wxVariantData
{
DECLARE_DYNAMIC_CLASS(wxVariantDataStringList)
public:
    wxVariantDataStringList() {}
    wxVariantDataStringList(const wxStringList& list) { m_value = list; }

    wxStringList& GetValue() const { return (wxStringList&) m_value; }
    void SetValue(const wxStringList& value);

    virtual void Copy(wxVariantData& data);
    virtual bool Eq(wxVariantData& data) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Write(wxSTD ostream& str) const;
#endif
    virtual bool Write(wxString& str) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Read(wxSTD istream& str);
#endif
    virtual bool Read(wxString& str);
    virtual wxString GetType() const { return wxT("stringlist"); };

protected:
    wxStringList  m_value;
};

IMPLEMENT_DYNAMIC_CLASS(wxVariantDataStringList, wxVariantData)

void wxVariantDataStringList::SetValue(const wxStringList& value)
{
    m_value = value;
}

void wxVariantDataStringList::Copy(wxVariantData& data)
{
    wxASSERT_MSG( (data.GetType() == wxT("stringlist")), wxT("wxVariantDataStringList::Copy: Can't copy to this type of data") );

    wxVariantDataStringList& listData = (wxVariantDataStringList&) data;

    listData.m_value = m_value ;
}

bool wxVariantDataStringList::Eq(wxVariantData& data) const
{
    wxASSERT_MSG( (data.GetType() == wxT("stringlist")), wxT("wxVariantDataStringList::Eq: argument mismatch") );

    wxVariantDataStringList& listData = (wxVariantDataStringList&) data;
    wxNode* node1 = m_value.First();
    wxNode* node2 = listData.GetValue().First();
    while (node1 && node2)
    {
        wxString str1 ((wxChar*) node1->Data());
        wxString str2 ((wxChar*) node2->Data());
        if (str1 != str2)
            return FALSE;
        node1 = node1->Next();
        node2 = node2->Next();
    }
    if (node1 || node2) return FALSE;
    return TRUE;
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataStringList::Write(wxSTD ostream& str) const
{
    wxString s;
    Write(s);
    str << (const char*) s.mb_str();
    return TRUE;
}
#endif

bool wxVariantDataStringList::Write(wxString& str) const
{
    str = wxT("");
    wxNode* node = m_value.First();
    while (node)
    {
        wxChar* s = (wxChar*) node->Data();
        if (node != m_value.First())
          str += wxT(" ");
        str += s;
        node = node->Next();
    }

    return TRUE;
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataStringList::Read(wxSTD istream& WXUNUSED(str))
{
    wxFAIL_MSG(wxT("Unimplemented"));
    // TODO
    return FALSE;
}
#endif

bool wxVariantDataStringList::Read(wxString& WXUNUSED(str))
{
    wxFAIL_MSG(wxT("Unimplemented"));
    // TODO
    return FALSE;
}

/*
 * wxVariantDataLong
 */

class WXDLLEXPORT wxVariantDataLong: public wxVariantData
{
DECLARE_DYNAMIC_CLASS(wxVariantDataLong)
public:
    wxVariantDataLong() { m_value = 0; }
    wxVariantDataLong(long value) { m_value = value; }

    inline long GetValue() const { return m_value; }
    inline void SetValue(long value) { m_value = value; }

    virtual void Copy(wxVariantData& data);
    virtual bool Eq(wxVariantData& data) const;

    virtual bool Read(wxString& str);
    virtual bool Write(wxString& str) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Read(wxSTD istream& str);
    virtual bool Write(wxSTD ostream& str) const;
#endif
#if wxUSE_STREAMS
    virtual bool Read(wxInputStream& str);
    virtual bool Write(wxOutputStream &str) const;
#endif // wxUSE_STREAMS

    virtual wxString GetType() const { return wxT("long"); };

protected:
    long m_value;
};

IMPLEMENT_DYNAMIC_CLASS(wxVariantDataLong, wxVariantData)

void wxVariantDataLong::Copy(wxVariantData& data)
{
    wxASSERT_MSG( (data.GetType() == wxT("long")), wxT("wxVariantDataLong::Copy: Can't copy to this type of data") );

    wxVariantDataLong& otherData = (wxVariantDataLong&) data;

    otherData.m_value = m_value;
}

bool wxVariantDataLong::Eq(wxVariantData& data) const
{
    wxASSERT_MSG( (data.GetType() == wxT("long")), wxT("wxVariantDataLong::Eq: argument mismatch") );

    wxVariantDataLong& otherData = (wxVariantDataLong&) data;

    return (otherData.m_value == m_value);
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataLong::Write(wxSTD ostream& str) const
{
    wxString s;
    Write(s);
    str << (const char*) s.mb_str();
    return TRUE;
}
#endif

bool wxVariantDataLong::Write(wxString& str) const
{
    str.Printf(wxT("%ld"), m_value);
    return TRUE;
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataLong::Read(wxSTD istream& str)
{
    str >> m_value;
    return TRUE;
}
#endif

#if wxUSE_STREAMS
bool wxVariantDataLong::Write(wxOutputStream& str) const
{
    wxTextOutputStream s(str);

    s.Write32((size_t)m_value);
    return TRUE;
}

bool wxVariantDataLong::Read(wxInputStream& str)
{
   wxTextInputStream s(str);
   m_value = s.Read32();
   return TRUE;
}
#endif // wxUSE_STREAMS

bool wxVariantDataLong::Read(wxString& str)
{
    m_value = wxAtol((const wxChar*) str);
    return TRUE;
}

/*
 * wxVariantDataReal
 */

class WXDLLEXPORT wxVariantDataReal: public wxVariantData
{
DECLARE_DYNAMIC_CLASS(wxVariantDataReal)
public:
    wxVariantDataReal() { m_value = 0.0; }
    wxVariantDataReal(double value) { m_value = value; }

    inline double GetValue() const { return m_value; }
    inline void SetValue(double value) { m_value = value; }

    virtual void Copy(wxVariantData& data);
    virtual bool Eq(wxVariantData& data) const;
    virtual bool Read(wxString& str);
#if wxUSE_STD_IOSTREAM
    virtual bool Write(wxSTD ostream& str) const;
#endif
    virtual bool Write(wxString& str) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Read(wxSTD istream& str);
#endif
#if wxUSE_STREAMS
    virtual bool Read(wxInputStream& str);
    virtual bool Write(wxOutputStream &str) const;
#endif // wxUSE_STREAMS
    virtual wxString GetType() const { return wxT("double"); };

protected:
    double m_value;
};

IMPLEMENT_DYNAMIC_CLASS(wxVariantDataReal, wxVariantData)

void wxVariantDataReal::Copy(wxVariantData& data)
{
    wxASSERT_MSG( (data.GetType() == wxT("double")), wxT("wxVariantDataReal::Copy: Can't copy to this type of data") );

    wxVariantDataReal& otherData = (wxVariantDataReal&) data;

    otherData.m_value = m_value;
}

bool wxVariantDataReal::Eq(wxVariantData& data) const
{
    wxASSERT_MSG( (data.GetType() == wxT("double")), wxT("wxVariantDataReal::Eq: argument mismatch") );

    wxVariantDataReal& otherData = (wxVariantDataReal&) data;

    return (otherData.m_value == m_value);
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataReal::Write(wxSTD ostream& str) const
{
    wxString s;
    Write(s);
    str << (const char*) s.mb_str();
    return TRUE;
}
#endif

bool wxVariantDataReal::Write(wxString& str) const
{
    str.Printf(wxT("%.4f"), m_value);
    return TRUE;
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataReal::Read(wxSTD istream& str)
{
    str >> m_value;
    return TRUE;
}
#endif

#if wxUSE_STREAMS
bool wxVariantDataReal::Write(wxOutputStream& str) const
{
    wxTextOutputStream s(str);
    s.WriteDouble((double)m_value);
    return TRUE;
}

bool wxVariantDataReal::Read(wxInputStream& str)
{
    wxTextInputStream s(str);
    m_value = (float)s.ReadDouble();
    return TRUE;
}
#endif // wxUSE_STREAMS

bool wxVariantDataReal::Read(wxString& str)
{
    m_value = wxAtof((const wxChar*) str);
    return TRUE;
}

#ifdef HAVE_BOOL
/*
 * wxVariantDataBool
 */

class WXDLLEXPORT wxVariantDataBool: public wxVariantData
{
DECLARE_DYNAMIC_CLASS(wxVariantDataBool)
public:
    wxVariantDataBool() { m_value = 0; }
    wxVariantDataBool(bool value) { m_value = value; }

    inline bool GetValue() const { return m_value; }
    inline void SetValue(bool value) { m_value = value; }

    virtual void Copy(wxVariantData& data);
    virtual bool Eq(wxVariantData& data) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Write(wxSTD ostream& str) const;
#endif
    virtual bool Write(wxString& str) const;
    virtual bool Read(wxString& str);
#if wxUSE_STD_IOSTREAM
    virtual bool Read(wxSTD istream& str);
#endif
#if wxUSE_STREAMS
    virtual bool Read(wxInputStream& str);
    virtual bool Write(wxOutputStream& str) const;
#endif // wxUSE_STREAMS
    virtual wxString GetType() const { return wxT("bool"); };

protected:
    bool m_value;
};

IMPLEMENT_DYNAMIC_CLASS(wxVariantDataBool, wxVariantData)

void wxVariantDataBool::Copy(wxVariantData& data)
{
    wxASSERT_MSG( (data.GetType() == wxT("bool")), wxT("wxVariantDataBool::Copy: Can't copy to this type of data") );

    wxVariantDataBool& otherData = (wxVariantDataBool&) data;

    otherData.m_value = m_value;
}

bool wxVariantDataBool::Eq(wxVariantData& data) const
{
    wxASSERT_MSG( (data.GetType() == wxT("bool")), wxT("wxVariantDataBool::Eq: argument mismatch") );

    wxVariantDataBool& otherData = (wxVariantDataBool&) data;

    return (otherData.m_value == m_value);
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataBool::Write(wxSTD ostream& str) const
{
    wxString s;
    Write(s);
    str << (const char*) s.mb_str();
    return TRUE;
}
#endif

bool wxVariantDataBool::Write(wxString& str) const
{
    str.Printf(wxT("%d"), (int) m_value);
    return TRUE;
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataBool::Read(wxSTD istream& WXUNUSED(str))
{
    wxFAIL_MSG(wxT("Unimplemented"));
//    str >> (long) m_value;
    return FALSE;
}
#endif

#if wxUSE_STREAMS
bool wxVariantDataBool::Write(wxOutputStream& str) const
{
    wxTextOutputStream s(str);

    s.Write8(m_value);
    return TRUE;
}

bool wxVariantDataBool::Read(wxInputStream& str)
{
    wxTextInputStream s(str);

    m_value = s.Read8() != 0;
    return TRUE;
}
#endif // wxUSE_STREAMS

bool wxVariantDataBool::Read(wxString& str)
{
    m_value = (wxAtol((const wxChar*) str) != 0);
    return TRUE;
}
#endif // HAVE_BOOL

/*
 * wxVariantDataChar
 */

class WXDLLEXPORT wxVariantDataChar: public wxVariantData
{
DECLARE_DYNAMIC_CLASS(wxVariantDataChar)
public:
    wxVariantDataChar() { m_value = 0; }
    wxVariantDataChar(char value) { m_value = value; }

    inline char GetValue() const { return m_value; }
    inline void SetValue(char value) { m_value = value; }

    virtual void Copy(wxVariantData& data);
    virtual bool Eq(wxVariantData& data) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Read(wxSTD istream& str);
    virtual bool Write(wxSTD ostream& str) const;
#endif
    virtual bool Read(wxString& str);
    virtual bool Write(wxString& str) const;
#if wxUSE_STREAMS
    virtual bool Read(wxInputStream& str);
    virtual bool Write(wxOutputStream& str) const;
#endif // wxUSE_STREAMS
    virtual wxString GetType() const { return wxT("char"); };

protected:
    char m_value;
};

IMPLEMENT_DYNAMIC_CLASS(wxVariantDataChar, wxVariantData)

void wxVariantDataChar::Copy(wxVariantData& data)
{
    wxASSERT_MSG( (data.GetType() == wxT("char")), wxT("wxVariantDataChar::Copy: Can't copy to this type of data") );

    wxVariantDataChar& otherData = (wxVariantDataChar&) data;

    otherData.m_value = m_value;
}

bool wxVariantDataChar::Eq(wxVariantData& data) const
{
    wxASSERT_MSG( (data.GetType() == wxT("char")), wxT("wxVariantDataChar::Eq: argument mismatch") );

    wxVariantDataChar& otherData = (wxVariantDataChar&) data;

    return (otherData.m_value == m_value);
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataChar::Write(wxSTD ostream& str) const
{
    wxString s;
    Write(s);
    str << (const char*) s.mb_str();
    return TRUE;
}
#endif

bool wxVariantDataChar::Write(wxString& str) const
{
    str.Printf(wxT("%c"), m_value);
    return TRUE;
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataChar::Read(wxSTD istream& WXUNUSED(str))
{
    wxFAIL_MSG(wxT("Unimplemented"));
//    str >> m_value;
    return FALSE;
}
#endif

#if wxUSE_STREAMS
bool wxVariantDataChar::Write(wxOutputStream& str) const
{
    wxTextOutputStream s(str);

    s.Write8(m_value);
    return TRUE;
}

bool wxVariantDataChar::Read(wxInputStream& str)
{
    wxTextInputStream s(str);

    m_value = s.Read8();
    return TRUE;
}
#endif // wxUSE_STREAMS

bool wxVariantDataChar::Read(wxString& str)
{
    m_value = str[(size_t)0];
    return TRUE;
}

/*
 * wxVariantDataString
 */

#if defined(__BORLANDC__) && defined(__WIN16__)
// Change name because of truncation
#define wxVariantDataString wxVariantStringData
#endif

class WXDLLEXPORT wxVariantDataString: public wxVariantData
{
#if defined(__BORLANDC__) && defined(__WIN16__)
DECLARE_DYNAMIC_CLASS(wxVariantStringData)
#else
DECLARE_DYNAMIC_CLASS(wxVariantDataString)
#endif
public:
    wxVariantDataString() { }
    wxVariantDataString(const wxString& value) { m_value = value; }

    inline wxString GetValue() const { return m_value; }
    inline void SetValue(const wxString& value) { m_value = value; }

    virtual void Copy(wxVariantData& data);
    virtual bool Eq(wxVariantData& data) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Write(wxSTD ostream& str) const;
#endif
    virtual bool Read(wxString& str);
    virtual bool Write(wxString& str) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Read(wxSTD istream& str);
#endif
#if wxUSE_STREAMS
    virtual bool Read(wxInputStream& str);
    virtual bool Write(wxOutputStream& str) const;
#endif // wxUSE_STREAMS
    virtual wxString GetType() const { return wxT("string"); };

protected:
    wxString m_value;
};

void wxVariantDataString::Copy(wxVariantData& data)
{
    wxASSERT_MSG( (data.GetType() == wxT("string")), wxT("wxVariantDataString::Copy: Can't copy to this type of data") );

    wxVariantDataString& otherData = (wxVariantDataString&) data;

    otherData.m_value = m_value;
}

bool wxVariantDataString::Eq(wxVariantData& data) const
{
    wxASSERT_MSG( (data.GetType() == wxT("string")), wxT("wxVariantDataString::Eq: argument mismatch") );

    wxVariantDataString& otherData = (wxVariantDataString&) data;

    return (otherData.m_value == m_value);
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataString::Write(wxSTD ostream& str) const
{
    str << (const char*) m_value.mb_str();
    return TRUE;
}
#endif

bool wxVariantDataString::Write(wxString& str) const
{
    str = m_value;
    return TRUE;
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataString::Read(wxSTD istream& str)
{
    str >> m_value;
    return TRUE;
}
#endif

#if wxUSE_STREAMS
bool wxVariantDataString::Write(wxOutputStream& str) const
{
  // why doesn't wxOutputStream::operator<< take "const wxString&"
    wxTextOutputStream s(str);
    s.WriteString(m_value);
    return TRUE;
}

bool wxVariantDataString::Read(wxInputStream& str)
{
    wxTextInputStream s(str);

    m_value = s.ReadString();
    return TRUE;
}
#endif // wxUSE_STREAMS

bool wxVariantDataString::Read(wxString& str)
{
    m_value = str;
    return TRUE;
}

#if defined(__BORLANDC__) && defined(__WIN16__)
IMPLEMENT_DYNAMIC_CLASS(wxVariantStringData, wxVariantData)
#else
IMPLEMENT_DYNAMIC_CLASS(wxVariantDataString, wxVariantData)
#endif

/*
 * wxVariantDataTime
 */

// For some reason, Watcom C++ can't link variant.cpp with time/date classes compiled
#if wxUSE_TIMEDATE && !defined(__WATCOMC__)

class wxVariantDataTime: public wxVariantData
{
 DECLARE_DYNAMIC_CLASS(wxVariantDataTime)
public:
    wxVariantDataTime() { }
    wxVariantDataTime(const wxTime& value) { m_value = value; }

    inline wxTime GetValue() const { return m_value; }
    inline void SetValue(const wxTime& value) { m_value = value; }

    virtual void Copy(wxVariantData& data);
    virtual bool Eq(wxVariantData& data) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Write(wxSTD ostream& str) const;
#endif
    virtual bool Write(wxString& str) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Read(wxSTD istream& str);
#endif
    virtual bool Read(wxString& str);
    virtual wxString GetType() const { return wxT("time"); };
	virtual wxVariantData* Clone() { return new wxVariantDataTime; }

protected:
    wxTime m_value;
};

IMPLEMENT_DYNAMIC_CLASS(wxVariantDataTime, wxVariantData)

void wxVariantDataTime::Copy(wxVariantData& data)
{
    wxASSERT_MSG( (data.GetType() == wxT("time")), wxT("wxVariantDataTime::Copy: Can't copy to this type of data") );

    wxVariantDataTime& otherData = (wxVariantDataTime&) data;

    otherData.m_value = m_value;
}

bool wxVariantDataTime::Eq(wxVariantData& data) const
{
    wxASSERT_MSG( (data.GetType() == wxT("time")), wxT("wxVariantDataTime::Eq: argument mismatch") );

    wxVariantDataTime& otherData = (wxVariantDataTime&) data;

    return (otherData.m_value == m_value);
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataTime::Write(wxSTD ostream& str) const
{
    wxString s;
    Write(s);
    str << (const char*) s.mb_str();
    return TRUE;
}
#endif

bool wxVariantDataTime::Write(wxString& str) const
{
    wxChar*s = m_value.FormatTime();
    str = s;
    return TRUE;
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataTime::Read(wxSTD istream& WXUNUSED(str))
{
    // Not implemented
    return FALSE;
}
#endif

bool wxVariantDataTime::Read(wxString& WXUNUSED(str))
{
    // Not implemented
    return FALSE;
}

/*
 * wxVariantDataDate
 */

class wxVariantDataDate: public wxVariantData
{
 DECLARE_DYNAMIC_CLASS(wxVariantDataDate)
public:
    wxVariantDataDate() { }
    wxVariantDataDate(const wxDate& value) { m_value = value; }

    inline wxDate GetValue() const { return m_value; }
    inline void SetValue(const wxDate& value) { m_value = value; }

    virtual void Copy(wxVariantData& data);
    virtual bool Eq(wxVariantData& data) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Write(wxSTD ostream& str) const;
#endif
    virtual bool Write(wxString& str) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Read(wxSTD istream& str);
#endif
    virtual bool Read(wxString& str);
    virtual wxString GetType() const { return wxT("date"); };
	virtual wxVariantData* Clone() { return new wxVariantDataDate; }

protected:
    wxDate m_value;
};

IMPLEMENT_DYNAMIC_CLASS(wxVariantDataDate, wxVariantData)

void wxVariantDataDate::Copy(wxVariantData& data)
{
    wxASSERT_MSG( (data.GetType() == wxT("date")), wxT("wxVariantDataDate::Copy: Can't copy to this type of data") );

    wxVariantDataDate& otherData = (wxVariantDataDate&) data;

    otherData.m_value = m_value;
}

bool wxVariantDataDate::Eq(wxVariantData& data) const
{
    wxASSERT_MSG( (data.GetType() == wxT("date")), wxT("wxVariantDataDate::Eq: argument mismatch") );

    wxVariantDataDate& otherData = (wxVariantDataDate&) data;

    return (otherData.m_value == m_value);
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataDate::Write(wxSTD ostream& str) const
{
    wxString s;
    Write(s);
    str << (const char*) s.mb_str();
    return TRUE;
}
#endif

bool wxVariantDataDate::Write(wxString& str) const
{
    str = m_value.FormatDate();
    return TRUE;
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataDate::Read(wxSTD istream& WXUNUSED(str))
{
    // Not implemented
    return FALSE;
}
#endif

bool wxVariantDataDate::Read(wxString& WXUNUSED(str))
{
    // Not implemented
    return FALSE;
}
#endif
  // wxUSE_TIMEDATE

/*
 * wxVariantDataVoidPtr
 */

class wxVariantDataVoidPtr: public wxVariantData
{
DECLARE_DYNAMIC_CLASS(wxVariantDataVoidPtr)
public:
    wxVariantDataVoidPtr() { }
    wxVariantDataVoidPtr(void* value) { m_value = value; }

    inline void* GetValue() const { return m_value; }
    inline void SetValue(void* value) { m_value = value; }

    virtual void Copy(wxVariantData& data);
    virtual bool Eq(wxVariantData& data) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Write(wxSTD ostream& str) const;
#endif
    virtual bool Write(wxString& str) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Read(wxSTD istream& str);
#endif
    virtual bool Read(wxString& str);
    virtual wxString GetType() const { return wxT("void*"); };
	virtual wxVariantData* Clone() { return new wxVariantDataVoidPtr; }

protected:
    void* m_value;
};

IMPLEMENT_DYNAMIC_CLASS(wxVariantDataVoidPtr, wxVariantData)

void wxVariantDataVoidPtr::Copy(wxVariantData& data)
{
    wxASSERT_MSG( (data.GetType() == wxT("void*")), wxT("wxVariantDataVoidPtr::Copy: Can't copy to this type of data") );

    wxVariantDataVoidPtr& otherData = (wxVariantDataVoidPtr&) data;

    otherData.m_value = m_value;
}

bool wxVariantDataVoidPtr::Eq(wxVariantData& data) const
{
    wxASSERT_MSG( (data.GetType() == wxT("void*")), wxT("wxVariantDataVoidPtr::Eq: argument mismatch") );

    wxVariantDataVoidPtr& otherData = (wxVariantDataVoidPtr&) data;

    return (otherData.m_value == m_value);
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataVoidPtr::Write(wxSTD ostream& str) const
{
    wxString s;
    Write(s);
    str << (const char*) s.mb_str();
    return TRUE;
}
#endif

bool wxVariantDataVoidPtr::Write(wxString& str) const
{
    str.Printf(wxT("%ld"), (long) m_value);
    return TRUE;
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataVoidPtr::Read(wxSTD istream& WXUNUSED(str))
{
    // Not implemented
    return FALSE;
}
#endif

bool wxVariantDataVoidPtr::Read(wxString& WXUNUSED(str))
{
    // Not implemented
    return FALSE;
}

/*
 * wxVariantDataDateTime
 */

class wxVariantDataDateTime: public wxVariantData
{
    DECLARE_DYNAMIC_CLASS(wxVariantDataDateTime)

public:
    wxVariantDataDateTime() { }
    wxVariantDataDateTime(const wxDateTime& value) { m_value = value; }
#if wxUSE_ODBC
    wxVariantDataDateTime(const TIME_STRUCT* valptr)
        { m_value = wxDateTime(valptr->hour, valptr->minute, valptr->second); }
    wxVariantDataDateTime(const DATE_STRUCT* valptr)
        { m_value = wxDateTime(valptr->day, (wxDateTime::Month) (valptr->month - 1),valptr->year); }
    wxVariantDataDateTime(const TIMESTAMP_STRUCT* valptr)
        { m_value = wxDateTime(valptr->day, (wxDateTime::Month) (valptr->month - 1), valptr->year,
                        valptr->hour, valptr->minute, valptr->second, valptr->fraction ); }
#endif //ODBC

    inline wxDateTime GetValue() const { return m_value; }
    inline void SetValue(const wxDateTime& value) { m_value = value; }

    virtual void Copy(wxVariantData& data);
    virtual bool Eq(wxVariantData& data) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Write(wxSTD ostream& str) const;
#endif
    virtual bool Write(wxString& str) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Read(wxSTD istream& str);
#endif
    virtual bool Read(wxString& str);
    virtual wxString GetType() const { return wxT("datetime"); };
    virtual wxVariantData* Clone() { return new wxVariantDataDateTime; }

protected:
    wxDateTime m_value;
};


IMPLEMENT_DYNAMIC_CLASS(wxVariantDataDateTime, wxVariantData)

void wxVariantDataDateTime::Copy(wxVariantData& data)
{
    wxASSERT_MSG( (data.GetType() == wxT("datetime")), wxT("wxVariantDataDateTime::Copy: Can't copy to this type of data") );

    wxVariantDataDateTime& otherData = (wxVariantDataDateTime&) data;

    otherData.m_value = m_value;
}


bool wxVariantDataDateTime::Eq(wxVariantData& data) const
{
    wxASSERT_MSG( (data.GetType() == wxT("datetime")), wxT("wxVariantDataDateTime::Eq: argument mismatch") );

    wxVariantDataDateTime& otherData = (wxVariantDataDateTime&) data;

    return (otherData.m_value == m_value);
}


#if wxUSE_STD_IOSTREAM
bool wxVariantDataDateTime::Write(wxSTD ostream& str) const
{
    // Not implemented
    return FALSE;
}
#endif


bool wxVariantDataDateTime::Write(wxString& str) const
{
    str = m_value.Format();
    return TRUE;
}


#if wxUSE_STD_IOSTREAM
bool wxVariantDataDateTime::Read(wxSTD istream& WXUNUSED(str))
{
    // Not implemented
    return FALSE;
}
#endif


bool wxVariantDataDateTime::Read(wxString& str)
{
    if(! m_value.ParseDateTime(str))
        return FALSE;
    return TRUE;
}

// ----------------------------------------------------------------------------
// wxVariantDataArrayString
// ----------------------------------------------------------------------------

class wxVariantDataArrayString: public wxVariantData
{
public:
    wxVariantDataArrayString() { }
    wxVariantDataArrayString(const wxArrayString& value) { m_value = value; }

    wxArrayString GetValue() const { return m_value; }
    void SetValue(const wxArrayString& value) { m_value = value; }

    virtual void Copy(wxVariantData& data);
    virtual bool Eq(wxVariantData& data) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Write(wxSTD ostream& str) const;
#endif
    virtual bool Write(wxString& str) const;
#if wxUSE_STD_IOSTREAM
    virtual bool Read(wxSTD istream& str);
#endif
    virtual bool Read(wxString& str);
    virtual wxString GetType() const { return wxT("arrstring"); };
    virtual wxVariantData* Clone() { return new wxVariantDataArrayString; }

protected:
    wxArrayString m_value;

    DECLARE_DYNAMIC_CLASS(wxVariantDataArrayString)
};

IMPLEMENT_DYNAMIC_CLASS(wxVariantDataArrayString, wxVariantData)

void wxVariantDataArrayString::Copy(wxVariantData& data)
{
    wxASSERT_MSG( data.GetType() == GetType(), wxT("wxVariantDataArrayString::Copy: Can't copy to this type of data") );

    wxVariantDataArrayString& otherData = (wxVariantDataArrayString&) data;

    otherData.m_value = m_value;
}


bool wxVariantDataArrayString::Eq(wxVariantData& data) const
{
    wxASSERT_MSG( data.GetType() == GetType(), wxT("wxVariantDataArrayString::Eq: argument mismatch") );

    wxVariantDataArrayString& otherData = (wxVariantDataArrayString&) data;

    return otherData.m_value == m_value;
}


#if wxUSE_STD_IOSTREAM
bool wxVariantDataArrayString::Write(wxSTD ostream& str) const
{
    // Not implemented
    return FALSE;
}
#endif


bool wxVariantDataArrayString::Write(wxString& str) const
{
    size_t count = m_value.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        if ( n )
            str += _T(';');

        str += m_value[n];
    }

    return TRUE;
}


#if wxUSE_STD_IOSTREAM
bool wxVariantDataArrayString::Read(wxSTD istream& WXUNUSED(str))
{
    // Not implemented
    return FALSE;
}
#endif


bool wxVariantDataArrayString::Read(wxString& str)
{
    wxStringTokenizer tk(str, _T(";"));
    while ( tk.HasMoreTokens() )
    {
        m_value.Add(tk.GetNextToken());
    }

    return TRUE;
}



/*
 * wxVariant
 */

IMPLEMENT_DYNAMIC_CLASS(wxVariant, wxObject)

// Construction & destruction
wxVariant::wxVariant()
{
    m_data = (wxVariantData*) NULL;
}

wxVariant::wxVariant(double val, const wxString& name)
{
    m_data = new wxVariantDataReal(val);
    m_name = name;
}

wxVariant::wxVariant(long val, const wxString& name)
{
    m_data = new wxVariantDataLong(val);
    m_name = name;
}

#ifdef HAVE_BOOL
wxVariant::wxVariant(bool val, const wxString& name)
{
    m_data = new wxVariantDataBool(val);
    m_name = name;
}
#endif

wxVariant::wxVariant(char val, const wxString& name)
{
    m_data = new wxVariantDataChar(val);
    m_name = name;
}

wxVariant::wxVariant(const wxString& val, const wxString& name)
{
    m_data = new wxVariantDataString(val);
    m_name = name;
}

wxVariant::wxVariant(const wxChar* val, const wxString& name)
{
    m_data = new wxVariantDataString(wxString(val));
    m_name = name;
}

wxVariant::wxVariant(const wxStringList& val, const wxString& name)
{
    m_data = new wxVariantDataStringList(val);
    m_name = name;
}

wxVariant::wxVariant(const wxList& val, const wxString& name) // List of variants
{
    m_data = new wxVariantDataList(val);
    m_name = name;
}

// For some reason, Watcom C++ can't link variant.cpp with time/date classes compiled
#if wxUSE_TIMEDATE && !defined(__WATCOMC__)
wxVariant::wxVariant(const wxTime& val, const wxString& name) // Time
{
    m_data = new wxVariantDataTime(val);
    m_name = name;
}

wxVariant::wxVariant(const wxDate& val, const wxString& name) // Date
{
    m_data = new wxVariantDataDate(val);
    m_name = name;
}
#endif

wxVariant::wxVariant(void* val, const wxString& name) // Void ptr
{
    m_data = new wxVariantDataVoidPtr(val);
    m_name = name;
}

wxVariant::wxVariant(const wxDateTime& val, const wxString& name) // Date
{
    m_data = new wxVariantDataDateTime(val);
    m_name = name;
}

#if wxUSE_ODBC
wxVariant::wxVariant(const TIME_STRUCT* valptr, const wxString& name) // Date
{
    m_data = new wxVariantDataDateTime(valptr);
    m_name = name;
}

wxVariant::wxVariant(const TIMESTAMP_STRUCT* valptr, const wxString& name) // Date
{
    m_data = new wxVariantDataDateTime(valptr);
    m_name = name;
}

wxVariant::wxVariant(const DATE_STRUCT* valptr, const wxString& name) // Date
{
    m_data = new wxVariantDataDateTime(valptr);
    m_name = name;
}
#endif // wxUSE_ODBC

wxVariant::wxVariant(const wxArrayString& val, const wxString& name) // Strings
{
    m_data = new wxVariantDataArrayString(val);
    m_name = name;
}

wxVariant::wxVariant(const wxVariant& variant)
    : wxObject()
{
    if (!variant.IsNull())
    {
        m_data = (wxVariantData*) variant.GetData()->GetClassInfo()->CreateObject();
        variant.m_data->Copy(*m_data);
    }
    else
        m_data = (wxVariantData*) NULL;
    m_name = variant.m_name;
}

wxVariant::wxVariant(wxVariantData* data, const wxString& name) // User-defined data
{
    m_data = data;
    m_name = name;
}

wxVariant::~wxVariant()
{
    delete m_data;
}


// Make NULL (i.e. delete the data)
void wxVariant::MakeNull()
{
    delete m_data;
    m_data = NULL;
}

// Generic operators
// Assignment
void wxVariant::operator= (const wxVariant& variant)
{
    if (variant.IsNull())
    {
        MakeNull();
        return;
    }

    if (IsNull() || (GetType() != variant.GetType()))
    {
        if (m_data)
            delete m_data;
        m_data = (wxVariantData*) variant.GetData()->GetClassInfo()->CreateObject();
    }

    variant.GetData()->Copy(* GetData());

    m_name = variant.m_name;
}

// Assignment using data, e.g.
// myVariant = new wxStringVariantData("hello")
void wxVariant::operator= (wxVariantData* variantData)
{
    MakeNull();
    m_data = variantData;
}

bool wxVariant::operator== (const wxVariant& variant) const
{
    if (IsNull() || variant.IsNull())
        return (IsNull() == variant.IsNull());

    return (GetData()->Eq(* variant.GetData()));
}

bool wxVariant::operator!= (const wxVariant& variant) const
{
    return (!(*this == variant));
}


// Specific operators
bool wxVariant::operator== (double value) const
{
    double thisValue;
    if (!Convert(&thisValue))
        return FALSE;
    else
        return (value == thisValue);
}

bool wxVariant::operator!= (double value) const
{
    return (!((*this) == value));
}

void wxVariant::operator= (double value)
{
    if (GetType() == wxT("double"))
    {
        ((wxVariantDataReal*)GetData())->SetValue(value);
    }
    else
    {
        if (m_data)
            delete m_data;
        m_data = new wxVariantDataReal(value);
    }
}

bool wxVariant::operator== (long value) const
{
    long thisValue;
    if (!Convert(&thisValue))
        return FALSE;
    else
        return (value == thisValue);
}

bool wxVariant::operator!= (long value) const
{
    return (!((*this) == value));
}

void wxVariant::operator= (long value)
{
    if (GetType() == wxT("long"))
    {
        ((wxVariantDataLong*)GetData())->SetValue(value);
    }
    else
    {
        if (m_data)
            delete m_data;
        m_data = new wxVariantDataLong(value);
    }
}

bool wxVariant::operator== (char value) const
{
    char thisValue;
    if (!Convert(&thisValue))
        return FALSE;
    else
        return (value == thisValue);
}

bool wxVariant::operator!= (char value) const
{
    return (!((*this) == value));
}

void wxVariant::operator= (char value)
{
    if (GetType() == wxT("char"))
    {
        ((wxVariantDataChar*)GetData())->SetValue(value);
    }
    else
    {
        if (m_data)
            delete m_data;
        m_data = new wxVariantDataChar(value);
    }
}

#ifdef HAVE_BOOL
bool wxVariant::operator== (bool value) const
{
    bool thisValue;
    if (!Convert(&thisValue))
        return FALSE;
    else
        return (value == thisValue);
}

bool wxVariant::operator!= (bool value) const
{
    return (!((*this) == value));
}

void wxVariant::operator= (bool value)
{
    if (GetType() == wxT("bool"))
    {
        ((wxVariantDataBool*)GetData())->SetValue(value);
    }
    else
    {
        if (m_data)
            delete m_data;
        m_data = new wxVariantDataBool(value);
    }
}
#endif // HAVE_BOOL

bool wxVariant::operator== (const wxString& value) const
{
    wxString thisValue;
    if (!Convert(&thisValue))
        return FALSE;

    return value == thisValue;
}

bool wxVariant::operator!= (const wxString& value) const
{
    return (!((*this) == value));
}

void wxVariant::operator= (const wxString& value)
{
    if (GetType() == wxT("string"))
    {
        ((wxVariantDataString*)GetData())->SetValue(value);
    }
    else
    {
        if (m_data)
            delete m_data;
        m_data = new wxVariantDataString(value);
    }
}

void wxVariant::operator= (const wxChar* value)
{
    if (GetType() == wxT("string"))
    {
        ((wxVariantDataString*)GetData())->SetValue(wxString(value));
    }
    else
    {
        if (m_data)
            delete m_data;
        m_data = new wxVariantDataString(wxString(value));
    }
}

bool wxVariant::operator== (const wxStringList& value) const
{
    wxASSERT_MSG( (GetType() == wxT("stringlist")), wxT("Invalid type for == operator") );

    wxVariantDataStringList other(value);
    return (m_data->Eq(other));
}

bool wxVariant::operator!= (const wxStringList& value) const
{
    return (!((*this) == value));
}

void wxVariant::operator= (const wxStringList& value)
{
    if (GetType() == wxT("stringlist"))
    {
        ((wxVariantDataStringList*)GetData())->SetValue(value);
    }
    else
    {
        if (m_data)
            delete m_data;
        m_data = new wxVariantDataStringList(value);
    }
}

bool wxVariant::operator== (const wxList& value) const
{
    wxASSERT_MSG( (GetType() == wxT("list")), wxT("Invalid type for == operator") );

    wxVariantDataList other(value);
    return (m_data->Eq(other));
}

bool wxVariant::operator!= (const wxList& value) const
{
    return (!((*this) == value));
}

void wxVariant::operator= (const wxList& value)
{
    if (GetType() == wxT("list"))
    {
        ((wxVariantDataList*)GetData())->SetValue(value);
    }
    else
    {
        if (m_data)
            delete m_data;
        m_data = new wxVariantDataList(value);
    }
}

// For some reason, Watcom C++ can't link variant.cpp with time/date classes compiled
#if wxUSE_TIMEDATE && !defined(__WATCOMC__)
bool wxVariant::operator== (const wxTime& value) const
{
    wxTime thisValue;
    if (!Convert(&thisValue))
        return FALSE;

    return value == thisValue;
}

bool wxVariant::operator!= (const wxTime& value) const
{
    return (!((*this) == value));
}

void wxVariant::operator= (const wxTime& value)
{
    if (GetType() == wxT("time"))
    {
        ((wxVariantDataTime*)GetData())->SetValue(value);
    }
    else
    {
        if (m_data)
            delete m_data;
        m_data = new wxVariantDataTime(value);
    }
}

bool wxVariant::operator== (const wxDate& value) const
{
    wxDate thisValue;
    if (!Convert(&thisValue))
        return FALSE;

    return (value == thisValue);
}

bool wxVariant::operator!= (const wxDate& value) const
{
    return (!((*this) == value));
}

void wxVariant::operator= (const wxDate& value)
{
    if (GetType() == wxT("date"))
    {
        ((wxVariantDataTime*)GetData())->SetValue(value);
    }
    else
    {
        if (m_data)
            delete m_data;
        m_data = new wxVariantDataDate(value);
    }
}
#endif

bool wxVariant::operator== (void* value) const
{
    return (value == ((wxVariantDataVoidPtr*)GetData())->GetValue());
}

bool wxVariant::operator!= (void* value) const
{
    return (!((*this) == (void*) value));
}

void wxVariant::operator= (void* value)
{
    if (GetType() == wxT("void*"))
    {
        ((wxVariantDataVoidPtr*)GetData())->SetValue(value);
    }
    else
    {
        if (m_data)
            delete m_data;
        m_data = new wxVariantDataVoidPtr(value);
    }
}

bool wxVariant::operator== (const wxDateTime& value) const
{
    wxDateTime thisValue;
    if (!Convert(&thisValue))
        return FALSE;

    return value.IsEqualTo(thisValue);
}

bool wxVariant::operator!= (const wxDateTime& value) const
{
    return (!((*this) == value));
}

void wxVariant::operator= (const wxDateTime& value)
{
    if (GetType() == wxT("datetime"))
    {
        ((wxVariantDataDateTime*)GetData())->SetValue(value);
    }
    else
    {
        if (m_data)
            delete m_data;
        m_data = new wxVariantDataDateTime(value);
    }
}

#if wxUSE_ODBC
void wxVariant::operator= (const DATE_STRUCT* value)
{
    if (m_data)
        delete m_data;
    m_data = new wxVariantDataDateTime(value);
}


void wxVariant::operator= (const TIME_STRUCT* value)
{
    if (m_data)
        delete m_data;
    m_data = new wxVariantDataDateTime(value);
}


void wxVariant::operator= (const TIMESTAMP_STRUCT* value)
{
    if (m_data)
        delete m_data;
    m_data = new wxVariantDataDateTime(value);
}

#endif // wxUSE_ODBC

bool wxVariant::operator==(const wxArrayString& WXUNUSED(value)) const
{
    wxFAIL_MSG( _T("TODO") );

    return FALSE;
}

bool wxVariant::operator!=(const wxArrayString& value) const
{
    return !(*this == value);
}

void wxVariant::operator=(const wxArrayString& value)
{
    if (GetType() == wxT("arrstring"))
    {
        ((wxVariantDataArrayString *)GetData())->SetValue(value);
    }
    else
    {
        delete m_data;
        m_data = new wxVariantDataArrayString(value);
    }
}

wxArrayString wxVariant::GetArrayString() const
{
    if ( GetType() == wxT("arrstring") )
        return ((wxVariantDataArrayString *)GetData())->GetValue();

    return wxArrayString();
}


// Treat a list variant as an array
wxVariant wxVariant::operator[] (size_t idx) const
{
    wxASSERT_MSG( (GetType() == wxT("list") || GetType() == wxT("stringlist")), wxT("Invalid type for array operator") );

    if (GetType() == wxT("list"))
    {
        wxVariantDataList* data = (wxVariantDataList*) m_data;
        wxASSERT_MSG( (idx < (size_t) data->GetValue().Number()), wxT("Invalid index for array") );
        return * (wxVariant*) (data->GetValue().Nth(idx)->Data());
    }
    else if (GetType() == wxT("stringlist"))
    {
        wxVariantDataStringList* data = (wxVariantDataStringList*) m_data;
        wxASSERT_MSG( (idx < (size_t) data->GetValue().Number()), wxT("Invalid index for array") );

        wxVariant variant( wxString( (wxChar*) (data->GetValue().Nth(idx)->Data()) ));
        return variant;
    }
    return wxNullVariant;
}

wxVariant& wxVariant::operator[] (size_t idx)
{
    // We can't return a reference to a variant for a string list, since the string
    // is actually stored as a char*, not a variant.

    wxASSERT_MSG( (GetType() == wxT("list")), wxT("Invalid type for array operator") );

    wxVariantDataList* data = (wxVariantDataList*) m_data;
    wxASSERT_MSG( (idx < (size_t) data->GetValue().Number()), wxT("Invalid index for array") );

    return * (wxVariant*) (data->GetValue().Nth(idx)->Data());
}

// Return the number of elements in a list
int wxVariant::GetCount() const
{
    wxASSERT_MSG( (GetType() == wxT("list") || GetType() == wxT("stringlist")), wxT("Invalid type for GetCount()") );

    if (GetType() == wxT("list"))
    {
        wxVariantDataList* data = (wxVariantDataList*) m_data;
        return data->GetValue().Number();
    }
    else if (GetType() == wxT("stringlist"))
    {
        wxVariantDataStringList* data = (wxVariantDataStringList*) m_data;
        return data->GetValue().Number();
    }
    return 0;
}

wxString wxVariant::MakeString() const
{
    if (!IsNull())
    {
        wxString str;
        if (GetData()->Write(str))
            return str;
    }
    return wxString(wxT(""));
}

// Accessors

void wxVariant::SetData(wxVariantData* data)
{
    if (m_data) delete m_data;
    m_data = data;
}


// Returns a string representing the type of the variant,
// e.g. "string", "bool", "stringlist", "list", "double", "long"
wxString wxVariant::GetType() const
{
    if (IsNull())
        return wxString(wxT("null"));
    else
        return m_data->GetType();
}


bool wxVariant::IsType(const wxString& type) const
{
    return (GetType() == type);
}


// Value accessors
double wxVariant::GetReal() const
{
    double value;
    if (Convert(& value))
        return value;
    else
    {
        wxFAIL_MSG(wxT("Could not convert to a real number"));
        return 0.0;
    }
}

long wxVariant::GetInteger() const
{
    long value;
    if (Convert(& value))
        return value;
    else
    {
        wxFAIL_MSG(wxT("Could not convert to an integer"));
        return 0;
    }
}

char wxVariant::GetChar() const
{
    char value;
    if (Convert(& value))
        return value;
    else
    {
        wxFAIL_MSG(wxT("Could not convert to a char"));
        return 0;
    }
}

bool wxVariant::GetBool() const
{
    bool value;
    if (Convert(& value))
        return value;
    else
    {
        wxFAIL_MSG(wxT("Could not convert to a bool"));
        return 0;
    }
}

wxString wxVariant::GetString() const
{
    wxString value;
    if (!Convert(& value))
    {
        wxFAIL_MSG(wxT("Could not convert to a string"));
    }

    return value;
}

// For some reason, Watcom C++ can't link variant.cpp with time/date classes compiled
#if wxUSE_TIMEDATE && !defined(__WATCOMC__)
wxTime wxVariant::GetTime() const
{
    wxTime value;
    if (!Convert(& value))
    {
        wxFAIL_MSG(wxT("Could not convert to a time"));
    }

    return value;
}

wxDate wxVariant::GetDate() const
{
    wxDate value;
    if (!Convert(& value))
    {
        wxFAIL_MSG(wxT("Could not convert to a date"));
    }

    return value;
}
#endif // wxUSE_TIMEDATE

void* wxVariant::GetVoidPtr() const
{
    wxASSERT( (GetType() == wxT("void*")) );

    return (void*) ((wxVariantDataVoidPtr*) m_data)->GetValue();
}

wxDateTime wxVariant::GetDateTime() const
{
    wxDateTime value;
    if (!Convert(& value))
    {
        wxFAIL_MSG(wxT("Could not convert to a datetime"));
    }

    return value;
}

wxList& wxVariant::GetList() const
{
    wxASSERT( (GetType() == wxT("list")) );

    return (wxList&) ((wxVariantDataList*) m_data)->GetValue();
}

wxStringList& wxVariant::GetStringList() const
{
    wxASSERT( (GetType() == wxT("stringlist")) );

    return (wxStringList&) ((wxVariantDataStringList*) m_data)->GetValue();
}

// Make empty list
void wxVariant::NullList()
{
    SetData(new wxVariantDataList());
};

// Append to list
void wxVariant::Append(const wxVariant& value)
{
    wxList& list = GetList();

    list.Append(new wxVariant(value));
}

// Insert at front of list
void wxVariant::Insert(const wxVariant& value)
{
    wxList& list = GetList();

    list.Insert(new wxVariant(value));
}

// Returns TRUE if the variant is a member of the list
bool wxVariant::Member(const wxVariant& value) const
{
    wxList& list = GetList();

    wxNode* node = list.First();
    while (node)
    {
        wxVariant* other = (wxVariant*) node->Data();
        if (value == *other)
            return TRUE;
        node = node->Next();
    }
    return FALSE;
}

// Deletes the nth element of the list
bool wxVariant::Delete(int item)
{
    wxList& list = GetList();

    wxASSERT_MSG( (item < list.Number()), wxT("Invalid index to Delete") );
    wxNode* node = list.Nth(item);
    wxVariant* variant = (wxVariant*) node->Data();
    delete variant;
    delete node;
    return TRUE;
}

// Clear list
void wxVariant::ClearList()
{
    if (!IsNull() && (GetType() == wxT("list")))
    {
        ((wxVariantDataList*) m_data)->Clear();
    }
    else
    {
        if (GetType() != wxT("list"))
        {
            delete m_data;
            m_data = NULL;
        }
        m_data = new wxVariantDataList;
    }
}

// Type conversion
bool wxVariant::Convert(long* value) const
{
    wxString type(GetType());
    if (type == wxT("double"))
        *value = (long) (((wxVariantDataReal*)GetData())->GetValue());
    else if (type == wxT("long"))
        *value = ((wxVariantDataLong*)GetData())->GetValue();
#ifdef HAVE_BOOL
    else if (type == wxT("bool"))
        *value = (long) (((wxVariantDataBool*)GetData())->GetValue());
#endif
    else if (type == wxT("string"))
        *value = wxAtol((const wxChar*) ((wxVariantDataString*)GetData())->GetValue());
    else
        return FALSE;

    return TRUE;
}

bool wxVariant::Convert(bool* value) const
{
    wxString type(GetType());
    if (type == wxT("double"))
        *value = ((int) (((wxVariantDataReal*)GetData())->GetValue()) != 0);
    else if (type == wxT("long"))
        *value = (((wxVariantDataLong*)GetData())->GetValue() != 0);
#ifdef HAVE_BOOL
    else if (type == wxT("bool"))
        *value = ((wxVariantDataBool*)GetData())->GetValue();
#endif
    else if (type == wxT("string"))
    {
        wxString val(((wxVariantDataString*)GetData())->GetValue());
        val.MakeLower();
        if (val == wxT("true") || val == wxT("yes"))
            *value = TRUE;
        else if (val == wxT("false") || val == wxT("no"))
            *value = FALSE;
        else
            return FALSE;
    }
    else
        return FALSE;

    return TRUE;
}

bool wxVariant::Convert(double* value) const
{
    wxString type(GetType());
    if (type == wxT("double"))
        *value = ((wxVariantDataReal*)GetData())->GetValue();
    else if (type == wxT("long"))
        *value = (double) (((wxVariantDataLong*)GetData())->GetValue());
#ifdef HAVE_BOOL
    else if (type == wxT("bool"))
        *value = (double) (((wxVariantDataBool*)GetData())->GetValue());
#endif
    else if (type == wxT("string"))
        *value = (double) wxAtof((const wxChar*) ((wxVariantDataString*)GetData())->GetValue());
    else
        return FALSE;

    return TRUE;
}

bool wxVariant::Convert(char* value) const
{
    wxString type(GetType());
    if (type == wxT("char"))
        *value = ((wxVariantDataChar*)GetData())->GetValue();
    else if (type == wxT("long"))
        *value = (char) (((wxVariantDataLong*)GetData())->GetValue());
#ifdef HAVE_BOOL
    else if (type == wxT("bool"))
        *value = (char) (((wxVariantDataBool*)GetData())->GetValue());
#endif
    else
        return FALSE;

    return TRUE;
}

bool wxVariant::Convert(wxString* value) const
{
    *value = MakeString();
    return TRUE;
}

// For some reason, Watcom C++ can't link variant.cpp with time/date classes compiled
#if wxUSE_TIMEDATE && !defined(__WATCOMC__)
bool wxVariant::Convert(wxTime* value) const
{
    wxString type(GetType());
    if (type == wxT("time"))
        *value = ((wxVariantDataTime*)GetData())->GetValue();
    else if (type == wxT("date"))
        *value = wxTime(((wxVariantDataDate*)GetData())->GetValue());
    else
        return FALSE;

    return TRUE;
}

bool wxVariant::Convert(wxDate* value) const
{
    wxString type(GetType());
    if (type == wxT("date"))
        *value = ((wxVariantDataDate*)GetData())->GetValue();
    else
        return FALSE;

    return TRUE;
}
#endif // wxUSE_TIMEDATE

bool wxVariant::Convert(wxDateTime* value) const
{
    wxString type(GetType());
    if (type == wxT("datetime"))
    {
        *value = ((wxVariantDataDateTime*)GetData())->GetValue();
    	return TRUE;
    } 
    // Fallback to string conversion
    wxString val;
    return Convert(&val) && (value->ParseDate(val));
}
