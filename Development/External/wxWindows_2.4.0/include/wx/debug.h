/////////////////////////////////////////////////////////////////////////////
// Name:        wx/debug.h
// Purpose:     Misc debug functions and macros
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29/01/98
// RCS-ID:      $Id: debug.h,v 1.21.2.4 2002/12/09 14:35:09 JS Exp $
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifndef   _WX_DEBUG_H_
#define   _WX_DEBUG_H_

#include  <assert.h>
#include  <limits.h>            // for CHAR_BIT used below

#include  "wx/wxchar.h"         // for __TFILE__ and wxChar

// ----------------------------------------------------------------------------
// Defines controlling the debugging macros
// ----------------------------------------------------------------------------

// if _DEBUG is defined (MS VC++ and others use it in debug builds), define
// __WXDEBUG__ too
#ifdef _DEBUG
    #ifndef __WXDEBUG__
        #define __WXDEBUG__
    #endif // !__WXDEBUG__
#endif // _DEBUG

// if NDEBUG is defined (<assert.h> uses it), undef __WXDEBUG__ and WXDEBUG
#ifdef NDEBUG
    #undef __WXDEBUG__
    #undef WXDEBUG
#endif // NDEBUG

// if __WXDEBUG__ is defined, make sure that WXDEBUG is defined and >= 1
#ifdef __WXDEBUG__
    #if !defined(WXDEBUG) || !WXDEBUG
        #undef WXDEBUG
        #define WXDEBUG 1
    #endif // !WXDEBUG
#endif // __WXDEBUG__

// ----------------------------------------------------------------------------
// Debugging macros
//
// All debugging macros rely on ASSERT() which in turn calls user-defined
// OnAssert() function. To keep things simple, it's called even when the
// expression is TRUE (i.e. everything is ok) and by default does nothing: just
// returns the same value back. But if you redefine it to do something more sexy
// (popping up a message box in your favourite GUI, sending you e-mail or
// whatever) it will affect all ASSERTs, FAILs and CHECKs in your code.
//
// Warning: if you don't like advices on programming style, don't read
// further! ;-)
//
// Extensive use of these macros is recommended! Remember that ASSERTs are
// disabled in final (without __WXDEBUG__ defined) build, so they add strictly
// nothing to your program's code. On the other hand, CHECK macros do stay
// even in release builds, but in general are not much of a burden, while
// a judicious use of them might increase your program's stability.
// ----------------------------------------------------------------------------

// Macros which are completely disabled in 'release' mode
//
// NB: these functions are implemented in src/common/appcmn.cpp
#ifdef  __WXDEBUG__
  /*
    this function may be redefined to do something non trivial and is called
    whenever one of debugging macros fails (i.e. condition is false in an
    assertion)

    parameters:
       szFile and nLine - file name and line number of the ASSERT
       szMsg            - optional message explaining the reason
  */
  extern void WXDLLEXPORT wxOnAssert(const wxChar *szFile,
                                     int nLine,
                                     const wxChar *szCond,
                                     const wxChar *szMsg = NULL);

  // call this function to break into the debugger uncodnitionally (assuming
  // the program is running under debugger, of course)
  extern void WXDLLEXPORT wxTrap();

  // helper function used to implement wxASSERT and wxASSERT_MSG
  //
  // note using "int" and not "bool" for cond to avoid VC++ warnings about
  // implicit conversions when doing "wxAssert( pointer )" and also use of
  // "!!cond" below to ensure that everything is converted to int
  extern void WXDLLEXPORT wxAssert(int cond,
                                   const wxChar *szFile,
                                   int nLine,
                                   const wxChar *szCond,
                                   const wxChar *szMsg = NULL) ;

  // generic assert macro
  #define wxASSERT(cond) wxAssert(!!(cond), __TFILE__, __LINE__, _T(#cond))

  // assert with additional message explaining it's cause
  #define wxASSERT_MSG(cond, msg) \
    wxAssert(!!(cond), __TFILE__, __LINE__, _T(#cond), msg)

  // an assert helper used to avoid warning when testing constant expressions,
  // i.e. wxASSERT( sizeof(int) == 4 ) can generate a compiler warning about
  // expression being always true, but not using
  // wxASSERT( wxAssertIsEqual(sizeof(int), 4) )
  //
  // NB: this is made obsolete by wxCOMPILE_TIME_ASSERT() and shouldn't be
  //     used any longer
  extern bool WXDLLEXPORT wxAssertIsEqual(int x, int y);
#else
  #define wxTrap()

  // nothing to do in release modes (hopefully at this moment there are
  // no more bugs ;-)
  #define wxASSERT(cond)
  #define wxASSERT_MSG(x, m)
#endif  //__WXDEBUG__

// Use of wxFalse instead of FALSE suppresses compiler warnings about testing
// constant expression
WXDLLEXPORT_DATA(extern const bool) wxFalse;
#define wxAssertFailure wxFalse

// special form of assert: always triggers it (in debug mode)
#define wxFAIL                 wxASSERT(wxAssertFailure)

// FAIL with some message
#define wxFAIL_MSG(msg)        wxASSERT_MSG(wxAssertFailure, msg)

// NB: the following macros work also in release mode!

/*
  These macros must be used only in invalid situation: for example, an
  invalid parameter (NULL pointer) is passed to a function. Instead of
  dereferencing it and causing core dump the function might try using
  CHECK( p != NULL ) or CHECK( p != NULL, return LogError("p is NULL!!") )
*/

// check that expression is true, "return" if not (also FAILs in debug mode)
#define wxCHECK(x, rc)            if (!(x)) {wxFAIL; return rc; }

// as wxCHECK but with a message explaining why we fail
#define wxCHECK_MSG(x, rc, msg)   if (!(x)) {wxFAIL_MSG(msg); return rc; }

// check that expression is true, perform op if not
#define wxCHECK2(x, op)           if (!(x)) {wxFAIL; op; }

// as wxCHECK2 but with a message explaining why we fail
#define wxCHECK2_MSG(x, op, msg)  if (!(x)) {wxFAIL_MSG(msg); op; }

// special form of wxCHECK2: as wxCHECK, but for use in void functions
//
// NB: there is only one form (with msg parameter) and it's intentional:
//     there is no other way to tell the caller what exactly went wrong
//     from the void function (of course, the function shouldn't be void
//     to begin with...)
#define wxCHECK_RET(x, msg)       if (!(x)) {wxFAIL_MSG(msg); return; }

// ----------------------------------------------------------------------------
// Compile time asserts
//
// Unlike the normal assert and related macros above which are checked during
// the program tun-time the macros below will result in a compilation error if
// the condition they check is false. This is usually used to check the
// expressions containing sizeof()s which cannot be tested with the
// preprocessor. If you can use the #if's, do use them as you can give a more
// detailed error message then.
// ----------------------------------------------------------------------------

/*
  How this works (you don't have to understand it to be able to use the
  macros): we rely on the fact that it is invalid to define a named bit field
  in a struct of width 0. All the rest are just the hacks to minimize the
  possibility of the compiler warnings when compiling this macro: in
  particular, this is why we define a struct and not an object (which would
  result in a warning about unused variable) and a named struct (otherwise we'd
  get a warning about an unnamed struct not used to define an object!).
  The _n__ part is to stop VC++ 7 being confused since it encloses __LINE++ in
  parentheses. Unfortunately this does not work with other compilers, so
  we will only enable it when we know the _precise_ symbols to test for.
 */

#define wxMAKE_ASSERT_NAME_HELPER(line)     wxAssert_ ## line
#define wxMAKE_ASSERT_NAME(line)            wxMAKE_ASSERT_NAME_HELPER(line)
#if 0
#define wxMAKE_UNIQUE_ASSERT_NAME           wxMAKE_ASSERT_NAME(_n___ ## __LINE__)
#else
#define wxMAKE_UNIQUE_ASSERT_NAME           wxMAKE_ASSERT_NAME(__LINE__)
#endif
#define wxMAKE_UNIQUE_ASSERT_NAME2(text)    wxMAKE_ASSERT_NAME(text)

/*
  The second argument of this macro must be a valid C++ identifier and not a
  string. I.e. you should use it like this:

    wxCOMPILE_TIME_ASSERT( sizeof(int) >= 2, YourIntsAreTooSmall );

 It may be used both within a function and in the global scope.
*/
#define wxCOMPILE_TIME_ASSERT(expr, msg) \
    struct wxMAKE_UNIQUE_ASSERT_NAME { unsigned int msg: expr; }

#define wxCOMPILE_TIME_ASSERT2(expr, msg, text) \
    struct wxMAKE_UNIQUE_ASSERT_NAME2(text) { unsigned int msg: expr; }

// helpers for wxCOMPILE_TIME_ASSERT below, for private use only
#define wxMAKE_BITSIZE_MSG(type, size) type ## SmallerThan ## size ## Bits

// a special case of compile time assert: check that the size of the given type
// is at least the given number of bits
#define wxASSERT_MIN_BITSIZE(type, size) \
    wxCOMPILE_TIME_ASSERT(sizeof(type) * CHAR_BIT >= size, \
                          wxMAKE_BITSIZE_MSG(type, size))

#endif  // _WX_DEBUG_H_

