/////////////////////////////////////////////////////////////////////////////
// Name:        memory.h
// Purpose:     MDI classes
// Author:      Arthur Seaton, Julian Smart
// Modified by:
// Created:     29/01/98
// RCS-ID:      $Id: memory.h,v 1.35 2002/08/31 11:29:10 GD Exp $
// Copyright:   (c) 1998 Julian Smart
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MEMORYH__
#define _WX_MEMORYH__

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "memory.h"
#endif

#include "wx/defs.h"
#include "wx/string.h"

/*
  The macro which will be expanded to include the file and line number
  info, or to be a straight call to the new operator.
*/

#if (defined(__WXDEBUG__) && wxUSE_MEMORY_TRACING) || wxUSE_DEBUG_CONTEXT

#include <stddef.h>

// Obsolete
#if 0
#if wxUSE_IOSTREAMH
    // N.B. BC++ doesn't have istream.h, ostream.h
#   include <iostream.h>
#else
#   include <iostream>
#   if defined(__VISUALC__) || defined(__MWERKS__)
//        using namespace std;
#   endif
#endif
#endif

#ifdef __WXDEBUG__

// devik 2000-8-27: export these because new/delete operators are now inline
WXDLLEXPORT void * wxDebugAlloc(size_t size, wxChar * fileName, int lineNum, bool isObject, bool isVect = FALSE);
WXDLLEXPORT void wxDebugFree(void * buf, bool isVect = FALSE);

//**********************************************************************************
/*
  The global operator new used for everything apart from getting
  dynamic storage within this function itself.
*/

// We'll only do malloc and free for the moment: leave the interesting
// stuff for the wxObject versions.
// devik 2000-8-29: All new/delete ops are now inline because they can't
// be marked as dllexport/dllimport. It then leads to weird bugs when
// used on MSW as DLL

#if wxUSE_GLOBAL_MEMORY_OPERATORS

#error THIS WILL NOT WORK IN CONJUNCTION WITH UNREAL ENGINE

// Undefine temporarily (new is #defined in object.h) because we want to
// declare some new operators.
#ifdef new
    #undef new
#endif

#if defined(__SUNCC__)
    #define wxUSE_ARRAY_MEMORY_OPERATORS 0
#elif !( defined (__VISUALC__) && (__VISUALC__ <= 1020) ) || defined( __MWERKS__)
    #define wxUSE_ARRAY_MEMORY_OPERATORS 1
#elif defined (__SGI_CC_)
    // only supported by -n32 compilers
    #ifndef __EDG_ABI_COMPATIBILITY_VERSION
        #define wxUSE_ARRAY_MEMORY_OPERATORS 0
    #endif
#elif !( defined (__VISUALC__) && (__VISUALC__ <= 1020) ) || defined( __MWERKS__)
    #define wxUSE_ARRAY_MEMORY_OPERATORS 1
#else
    // ::operator new[] is a recent C++ feature, so assume it's not supported
    #define wxUSE_ARRAY_MEMORY_OPERATORS 0
#endif

inline void * operator new (size_t size, wxChar * fileName, int lineNum)
{
  return wxDebugAlloc(size, fileName, lineNum, FALSE, FALSE);
}

inline void * operator new (size_t size)
{
  return wxDebugAlloc(size, NULL, 0, FALSE);
}

inline void operator delete (void * buf)
{
  wxDebugFree(buf, FALSE);
}

#if wxUSE_ARRAY_MEMORY_OPERATORS
inline void * operator new[] (size_t size)
{
  return wxDebugAlloc(size, NULL, 0, FALSE, TRUE);
}

inline void * operator new[] (size_t size, wxChar * fileName, int lineNum)
{
  return wxDebugAlloc(size, fileName, lineNum, FALSE, TRUE);
}

inline void operator delete[] (void * buf)
{
  wxDebugFree(buf, TRUE);
}
#endif

// VC++ 6.0 and MWERKS
#if ( defined(__VISUALC__) && (__VISUALC__ >= 1200) ) || defined(__MWERKS__)
inline void operator delete(void* pData, wxChar* /* fileName */, int /* lineNum */)
{
  wxDebugFree(pData, FALSE);
}
inline void operator delete[](void* pData, wxChar* /* fileName */, int /* lineNum */)
{
  wxDebugFree(pData, TRUE);
}
#endif // __VISUALC__>=1200
#endif // wxUSE_GLOBAL_MEMORY_OPERATORS
#endif // __WXDEBUG__

//**********************************************************************************

typedef unsigned int wxMarkerType;

/*
  Define the struct which will be placed at the start of all dynamically
  allocated memory.
*/

class WXDLLEXPORT wxMemStruct {

friend class WXDLLEXPORT wxDebugContext; // access to the m_next pointer for list traversal.

public:
public:
    int AssertList ();

    size_t RequestSize () { return m_reqSize; }
    wxMarkerType Marker () { return m_firstMarker; }

    // When an object is deleted we set the id slot to a specific value.
    inline void SetDeleted ();
    inline int IsDeleted ();

    int Append ();
    int Unlink ();

    // Used to determine if the object is really a wxMemStruct.
    // Not a foolproof test by any means, but better than none I hope!
    int AssertIt ();

    // Do all validation on a node.
    int ValidateNode ();

    // Check the integrity of a node and of the list, node by node.
    int CheckBlock ();
    int CheckAllPrevious ();

    // Print a single node.
    void PrintNode ();

    // Called when the memory linking functions get an error.
    void ErrorMsg (const char *);
    void ErrorMsg ();

    inline void *GetActualData(void) const { return m_actualData; }

    void Dump(void);

public:
    // Check for underwriting. There are 2 of these checks. This one
    // inside the struct and another right after the struct.
    wxMarkerType        m_firstMarker;

    // File name and line number are from cpp.
    wxChar*             m_fileName;
    int                 m_lineNum;

    // The amount of memory requested by the caller.
    size_t              m_reqSize;

    // Used to try to verify that we really are dealing with an object
    // of the required class. Can be 1 of 2 values these indicating a valid
    // wxMemStruct object, or a deleted wxMemStruct object.
    wxMarkerType        m_id;

    wxMemStruct *       m_prev;
    wxMemStruct *       m_next;

    void *              m_actualData;
    bool                m_isObject;
};


typedef void (wxMemStruct::*PmSFV) ();


/*
  Debugging class. This will only have a single instance, but it\'s
  a reasonable way to keep everything together and to make this
  available for change if needed by someone else.
  A lot of this stuff would be better off within the wxMemStruct class, but
  it\'s stuff which we need to access at times when there is no wxMemStruct
  object so we use this class instead. Think of it as a collection of
  globals which have to do with the wxMemStruct class.
*/

class WXDLLEXPORT wxDebugContext {

protected:
    // Used to set alignment for markers.
    static size_t CalcAlignment ();

    // Returns the amount of padding needed after something of the given
    // size. This is so that when we cast pointers backwards and forwards
    // the pointer value will be valid for a wxMarkerType.
    static size_t GetPadding (const size_t size) ;

    // Traverse the list.
    static void TraverseList (PmSFV, wxMemStruct *from = NULL);

    // Obsolete
#if 0
    static wxSTD streambuf *m_streamBuf;
    static wxSTD ostream *m_debugStream;
#endif

    static int debugLevel;
    static bool debugOn;

    static int m_balign;            // byte alignment
    static int m_balignmask;        // mask for performing byte alignment
public:
    // Set a checkpoint to dump only the memory from
    // a given point
    static wxMemStruct *checkPoint;

    wxDebugContext(void);
    ~wxDebugContext(void);

    // Obsolete
#if 0
    static bool HasStream(void) { return (m_debugStream != NULL); };
    static wxSTD ostream& GetStream(void) { return *m_debugStream; }
    static wxSTD streambuf *GetStreamBuf(void) { return m_streamBuf; }
    static void SetStream(wxSTD ostream *stream, wxSTD streambuf *buf = NULL);
    static bool SetFile(const wxString& file);
    static bool SetStandardError(void);
#endif

    static int GetLevel(void) { return debugLevel; }
    static void SetLevel(int level) { debugLevel = level; }

    static bool GetDebugMode(void) { return debugOn; }
    static void SetDebugMode(bool flag) { debugOn = flag; }

    static void SetCheckpoint(bool all = FALSE);
    static wxMemStruct *GetCheckpoint(void) { return checkPoint; }

    // Calculated from the request size and any padding needed
    // before the final marker.
    static size_t PaddedSize (const size_t reqSize);

    // Calc the total amount of space we need from the system
    // to satisfy a caller request. This includes all padding.
    static size_t TotSize (const size_t reqSize);

    // Return valid pointers to offsets within the allocated memory.
    static char * StructPos (const char * buf);
    static char * MidMarkerPos (const char * buf);
    static char * CallerMemPos (const char * buf);
    static char * EndMarkerPos (const char * buf, const size_t size);

    // Given a pointer to the start of the caller requested area
    // return a pointer to the start of the entire alloc\'d buffer.
    static char * StartPos (const char * caller);

    // Access to the list.
    static wxMemStruct * GetHead () { return m_head; }
    static wxMemStruct * GetTail () { return m_tail; }

    // Set the list sentinals.
    static wxMemStruct * SetHead (wxMemStruct * st) { return (m_head = st); }
    static wxMemStruct * SetTail (wxMemStruct * st) { return (m_tail = st); }

    // If this is set then every new operation checks the validity
    // of the all previous nodes in the list.
    static bool GetCheckPrevious () { return m_checkPrevious; }
    static void SetCheckPrevious (bool value) { m_checkPrevious = value; }

    // Checks all nodes, or all nodes if checkAll is TRUE
    static int Check(bool checkAll = FALSE);

    // Print out the list of wxMemStruct nodes.
    static bool PrintList(void);

    // Dump objects
    static bool Dump(void);

    // Print statistics
    static bool PrintStatistics(bool detailed = TRUE);

    // Print out the classes in the application.
    static bool PrintClasses(void);

    // Count the number of non-wxDebugContext-related objects
    // that are outstanding
    static int CountObjectsLeft(bool sinceCheckpoint = FALSE);

private:
    // Store these here to allow access to the list without
    // needing to have a wxMemStruct object.
    static wxMemStruct*         m_head;
    static wxMemStruct*         m_tail;

    // Set to FALSE if we're not checking all previous nodes when
    // we do a new. Set to TRUE when we are.
    static bool                 m_checkPrevious;
};

// Output a debug message, in a system dependent fashion.
void WXDLLEXPORT wxTrace(const wxChar *fmt ...) ATTRIBUTE_PRINTF_1;
void WXDLLEXPORT wxTraceLevel(int level, const wxChar *fmt ...) ATTRIBUTE_PRINTF_2;

#define WXTRACE wxTrace
#define WXTRACELEVEL wxTraceLevel

#else // else part for the #if __WXDEBUG__

inline void wxTrace(const wxChar *WXUNUSED(fmt)) {}
inline void wxTraceLevel(int WXUNUSED(level), const wxChar *WXUNUSED(fmt)) {}

#define WXTRACE TRUE ? (void)0 : wxTrace
#define WXTRACELEVEL TRUE ? (void)0 : wxTraceLevel
// #define WXDEBUG_NEW new

#endif // __WXDEBUG__

#endif
    // _WX_MEMORYH__

