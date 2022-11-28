/////////////////////////////////////////////////////////////////////////////
// Name:        variant.h
// Purpose:     wxVariant class, container for any type
// Author:      Julian Smart
// Modified by:
// Created:     10/09/98
// RCS-ID:      $Id: variant.h,v 1.17 2002/08/31 11:29:11 GD Exp $
// Copyright:   (c)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_VARIANT_H_
#define _WX_VARIANT_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "variant.h"
#endif

#include "wx/defs.h"
#include "wx/object.h"
#include "wx/string.h"
#include "wx/list.h"

#if wxUSE_TIMEDATE
    #include "wx/time.h"
    #include "wx/date.h"
#endif // time/date

#include "wx/datetime.h"

#if wxUSE_ODBC
    #include "wx/db.h"  // will #include sqltypes.h
#endif //ODBC

#include "wx/ioswrap.h"

/*
 * wxVariantData stores the actual data in a wxVariant object,
 * to allow it to store any type of data.
 * Derive from this to provide custom data handling.
 *
 * TODO: in order to replace wxPropertyValue, we would need
 * to consider adding constructors that take pointers to C++ variables,
 * or removing that functionality from the wxProperty library.
 * Essentially wxPropertyValue takes on some of the wxValidator functionality
 * by storing pointers and not just actual values, allowing update of C++ data
 * to be handled automatically. Perhaps there's another way of doing this without
 * overloading wxVariant with unnecessary functionality.
 */

class WXDLLEXPORT wxVariantData: public wxObject
{
DECLARE_ABSTRACT_CLASS(wxVariantData)
public:

// Construction & destruction
    wxVariantData() {};

// Override these to provide common functionality
    // Copy to data
    virtual void Copy(wxVariantData& data) = 0;
    virtual bool Eq(wxVariantData& data) const = 0;
#if wxUSE_STD_IOSTREAM
    virtual bool Write(wxSTD ostream& str) const = 0;
#endif
    virtual bool Write(wxString& str) const = 0;
#if wxUSE_STD_IOSTREAM
    virtual bool Read(wxSTD istream& str) = 0;
#endif
    virtual bool Read(wxString& str) = 0;
    // What type is it? Return a string name.
    virtual wxString GetType() const = 0;
};

/*
 * wxVariant can store any kind of data, but has some basic types
 * built in.
 * NOTE: this eventually should have a reference-counting implementation.
 * PLEASE, if you change it to ref-counting, make sure it doesn't involve bloating
 * this class too much.
 */

class WXDLLEXPORT wxVariant: public wxObject
{
DECLARE_DYNAMIC_CLASS(wxVariant)
public:

// Construction & destruction
    wxVariant();
    wxVariant(double val, const wxString& name = wxEmptyString);
    wxVariant(long val, const wxString& name = wxEmptyString);
#ifdef HAVE_BOOL
    wxVariant(bool val, const wxString& name = wxEmptyString);
#endif
    wxVariant(char val, const wxString& name = wxEmptyString);
    wxVariant(const wxString& val, const wxString& name = wxEmptyString);
    wxVariant(const wxChar* val, const wxString& name = wxEmptyString); // Necessary or VC++ assumes bool!
    wxVariant(const wxStringList& val, const wxString& name = wxEmptyString);
    wxVariant(const wxList& val, const wxString& name = wxEmptyString); // List of variants
// For some reason, Watcom C++ can't link variant.cpp with time/date classes compiled
#if wxUSE_TIMEDATE && !defined(__WATCOMC__)
    wxVariant(const wxTime& val, const wxString& name = wxEmptyString); // Time
    wxVariant(const wxDate& val, const wxString& name = wxEmptyString); // Date
#endif
    wxVariant(void* ptr, const wxString& name = wxEmptyString); // void* (general purpose)
    wxVariant(wxVariantData* data, const wxString& name = wxEmptyString); // User-defined data
//TODO: Need to document
    wxVariant(const wxDateTime& val, const wxString& name = wxEmptyString); // Date
    wxVariant(const wxArrayString& val, const wxString& name = wxEmptyString); // String array
#if wxUSE_ODBC
    wxVariant(const DATE_STRUCT* valptr, const wxString& name = wxEmptyString); // DateTime
    wxVariant(const TIME_STRUCT* valptr, const wxString& name = wxEmptyString); // DateTime
    wxVariant(const TIMESTAMP_STRUCT* valptr, const wxString& name = wxEmptyString); // DateTime
#endif
//TODO: End of Need to document
    
    wxVariant(const wxVariant& variant);
    ~wxVariant();

// Generic operators
    // Assignment
    void operator= (const wxVariant& variant);

//TODO: Need to document
    bool operator== (const wxDateTime& value) const;
    bool operator!= (const wxDateTime& value) const;
    void operator= (const wxDateTime& value) ;

    bool operator== (const wxArrayString& value) const;
    bool operator!= (const wxArrayString& value) const;
    void operator= (const wxArrayString& value) ;
#if wxUSE_ODBC
    void operator= (const DATE_STRUCT* value) ;
    void operator= (const TIME_STRUCT* value) ;
    void operator= (const TIMESTAMP_STRUCT* value) ;
#endif
//TODO: End of Need to document

    // Assignment using data, e.g.
    // myVariant = new wxStringVariantData("hello");
    void operator= (wxVariantData* variantData);
    bool operator== (const wxVariant& variant) const;
    bool operator!= (const wxVariant& variant) const;

// Specific operators
    bool operator== (double value) const;
    bool operator!= (double value) const;
    void operator= (double value) ;
    bool operator== (long value) const;
    bool operator!= (long value) const;
    void operator= (long value) ;
    bool operator== (char value) const;
    bool operator!= (char value) const;
    void operator= (char value) ;
#ifdef HAVE_BOOL
    bool operator== (bool value) const;
    bool operator!= (bool value) const;
    void operator= (bool value) ;
#endif
    bool operator== (const wxString& value) const;
    bool operator!= (const wxString& value) const;
    void operator= (const wxString& value) ;
    void operator= (const wxChar* value) ; // Necessary or VC++ assumes bool!
    bool operator== (const wxStringList& value) const;
    bool operator!= (const wxStringList& value) const;
    void operator= (const wxStringList& value) ;
    bool operator== (const wxList& value) const;
    bool operator!= (const wxList& value) const;
    void operator= (const wxList& value) ;
// For some reason, Watcom C++ can't link variant.cpp with time/date classes compiled
#if wxUSE_TIMEDATE && !defined(__WATCOMC__)
    bool operator== (const wxTime& value) const;
    bool operator!= (const wxTime& value) const;
    void operator= (const wxTime& value) ;
    bool operator== (const wxDate& value) const;
    bool operator!= (const wxDate& value) const;
    void operator= (const wxDate& value) ;
#endif
    bool operator== (void* value) const;
    bool operator!= (void* value) const;
    void operator= (void* value) ;

    // Treat a list variant as an array
    wxVariant operator[] (size_t idx) const;
    wxVariant& operator[] (size_t idx) ;

    // Implicit conversion to a wxString
    inline operator wxString () const {  return MakeString(); }
    wxString MakeString() const;

    // Other implicit conversions
    inline operator double () const {  return GetDouble(); }
    inline operator char () const {  return GetChar(); }
    inline operator long () const {  return GetLong(); }
    inline operator bool () const {  return GetBool(); }
// For some reason, Watcom C++ can't link variant.cpp with time/date classes compiled
#if wxUSE_TIMEDATE && !defined(__WATCOMC__)
    inline operator wxTime () const {  return GetTime(); }
    inline operator wxDate () const {  return GetDate(); }
#endif
    inline operator void* () const {  return GetVoidPtr(); }
//TODO: Need to document
    inline operator wxDateTime () const { return GetDateTime(); }
//TODO: End of Need to document

// Accessors
    // Sets/gets name
    inline void SetName(const wxString& name) { m_name = name; }
    inline const wxString& GetName() const { return m_name; }

    // Tests whether there is data
    inline bool IsNull() const { return (m_data == (wxVariantData*) NULL); }

    wxVariantData* GetData() const { return m_data; }
    void SetData(wxVariantData* data) ;

    // Returns a string representing the type of the variant,
    // e.g. "string", "bool", "stringlist", "list", "double", "long"
    wxString GetType() const;

    bool IsType(const wxString& type) const;

    // Return the number of elements in a list
    int GetCount() const;

// Value accessors
    double GetReal() const ;
    inline double GetDouble() const { return GetReal(); };
    long GetInteger() const ;
    inline long GetLong() const { return GetInteger(); };
    char GetChar() const ;
    bool GetBool() const ;
    wxString GetString() const ;
    wxList& GetList() const ;
    wxStringList& GetStringList() const ;

// For some reason, Watcom C++ can't link variant.cpp with time/date classes compiled
#if wxUSE_TIMEDATE && !defined(__WATCOMC__)
    wxTime GetTime() const ;
    wxDate GetDate() const ;
#endif
    void* GetVoidPtr() const ;
//TODO: Need to document
    wxDateTime GetDateTime() const ;
    wxArrayString GetArrayString() const;
//TODO: End of Need to document

// Operations
    // Make NULL (i.e. delete the data)
    void MakeNull();

    // Make empty list
    void NullList();

    // Append to list
    void Append(const wxVariant& value);

    // Insert at front of list
    void Insert(const wxVariant& value);

    // Returns TRUE if the variant is a member of the list
    bool Member(const wxVariant& value) const;

    // Deletes the nth element of the list
    bool Delete(int item);

    // Clear list
    void ClearList();

// Implementation
public:
// Type conversion
    bool Convert(long* value) const;
    bool Convert(bool* value) const;
    bool Convert(double* value) const;
    bool Convert(wxString* value) const;
    bool Convert(char* value) const;
// For some reason, Watcom C++ can't link variant.cpp with time/date classes compiled
#if wxUSE_TIMEDATE && !defined(__WATCOMC__)
    bool Convert(wxTime* value) const;
    bool Convert(wxDate* value) const;
#endif
//TODO: Need to document
    bool Convert(wxDateTime* value) const;
//TODO: End of Need to document

// Attributes
protected:
    wxVariantData*  m_data;
    wxString        m_name;
};

extern wxVariant WXDLLEXPORT wxNullVariant;

#endif
    // _WX_VARIANT_H_
