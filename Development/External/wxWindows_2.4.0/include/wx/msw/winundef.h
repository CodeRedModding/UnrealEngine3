/////////////////////////////////////////////////////////////////////////////
// Name:        winundef.h
// Purpose:     undefine the common symbols #define'd by <windows.h>
// Author:      Vadim Zeitlin
// Modified by:
// Created:     16.05.99
// RCS-ID:      $Id: winundef.h,v 1.21.2.1 2002/09/27 13:58:23 CE Exp $
// Copyright:   (c) wxWindows team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

/* THIS SHOULD NOT BE USED since you might include it once e.g. in window.h,
 * then again _AFTER_ you've included windows.h, in which case it won't work
 * a 2nd time -- JACS
#ifndef _WX_WINUNDEF_H_
#define _WX_WINUNDEF_H_
 */

// ----------------------------------------------------------------------------
// windows.h #defines the following identifiers which are also used in wxWin so
// we replace these symbols with the corresponding inline functions and
// undefine the macro.
//
// This looks quite ugly here but allows us to write clear (and correct!) code
// elsewhere because the functions, unlike the macros, respect the scope.
// ----------------------------------------------------------------------------

// CreateDialog

#ifdef CreateDialog
    #undef CreateDialog

    inline HWND CreateDialog(HINSTANCE hInstance,
                             LPCTSTR pTemplate,
                             HWND hwndParent,
                             DLGPROC pDlgProc)
    {
        #ifdef _UNICODE
            return CreateDialogW(hInstance, pTemplate, hwndParent, pDlgProc);
        #else
            return CreateDialogA(hInstance, pTemplate, hwndParent, pDlgProc);
        #endif
    }
#endif

// LoadMenu

#ifdef LoadMenu
    #undef LoadMenu

    inline HMENU LoadMenu(HINSTANCE instance, LPCTSTR name)
    {
        #ifdef _UNICODE
            return LoadMenuW(instance, name);
        #else
            return LoadMenuA(instance, name);
        #endif
    }
#endif

// FindText

#ifdef FindText
    #undef FindText

    inline HWND APIENTRY FindText(LPFINDREPLACE lpfindreplace)
    {
        #ifdef UNICODE
            return FindTextW(lpfindreplace);
        #else
            return FindTextA(lpfindreplace);
        #endif // !UNICODE
    }
#endif

// GetCharWidth

#ifdef GetCharWidth
   #undef GetCharWidth
   inline BOOL  GetCharWidth(HDC dc, UINT first, UINT last, LPINT buffer)
   {
   #ifdef _UNICODE
      return GetCharWidthW(dc, first, last, buffer);
   #else
      return GetCharWidthA(dc, first, last, buffer);
   #endif
   }
#endif

// FindWindow

#ifdef FindWindow
   #undef FindWindow
   #ifdef _UNICODE
   inline HWND FindWindow(LPCWSTR classname, LPCWSTR windowname)
   {
      return FindWindowW(classname, windowname);
   }
   #else
   inline HWND FindWindow(LPCSTR classname, LPCSTR windowname)
   {
      return FindWindowA(classname, windowname);
   }
   #endif
#endif

// PlaySound

#ifdef PlaySound
   #undef PlaySound
   #ifdef _UNICODE
   inline BOOL PlaySound(LPCWSTR pszSound, HMODULE hMod, DWORD fdwSound)
   {
      return PlaySoundW(pszSound, hMod, fdwSound);
   }
   #else
   inline BOOL PlaySound(LPCSTR pszSound, HMODULE hMod, DWORD fdwSound)
   {
      return PlaySoundA(pszSound, hMod, fdwSound);
   }
   #endif
#endif

// GetClassName

#ifdef GetClassName
   #undef GetClassName
   #ifdef _UNICODE
   inline int GetClassName(HWND h, LPWSTR classname, int maxcount)
   {
      return GetClassNameW(h, classname, maxcount);
   }
   #else
   inline int GetClassName(HWND h, LPSTR classname, int maxcount)
   {
      return GetClassNameA(h, classname, maxcount);
   }
   #endif
#endif

// GetClassInfo

#ifdef GetClassInfo
   #undef GetClassInfo
   #ifdef _UNICODE
   inline BOOL GetClassInfo(HINSTANCE h, LPCWSTR name, LPWNDCLASSW winclass)
   {
      return GetClassInfoW(h, name, winclass);
   }
   #else
   inline BOOL GetClassInfo(HINSTANCE h, LPCSTR name, LPWNDCLASSA winclass)
   {
      return GetClassInfoA(h, name, winclass);
   }
   #endif
#endif

// LoadAccelerators

#ifdef LoadAccelerators
   #undef LoadAccelerators
   #ifdef _UNICODE
   inline HACCEL LoadAccelerators(HINSTANCE h, LPCWSTR name)
   {
      return LoadAcceleratorsW(h, name);
   }
   #else
   inline HACCEL LoadAccelerators(HINSTANCE h, LPCSTR name)
   {
      return LoadAcceleratorsA(h, name);
   }
   #endif
#endif

// DrawText

#ifdef DrawText
   #undef DrawText
   #ifdef _UNICODE
   inline int DrawText(HDC h, LPCWSTR str, int count, LPRECT rect, UINT format)
   {
      return DrawTextW(h, str, count, rect, format);
   }
   #else
   inline int DrawText(HDC h, LPCSTR str, int count, LPRECT rect, UINT format)
   {
      return DrawTextA(h, str, count, rect, format);
   }
   #endif
#endif


/*
  When this file is included, sometimes the wxCHECK_W32API_VERSION macro
  is undefined. With for example CodeWarrior this gives problems with
  the following code:
  #if 0 && wxCHECK_W32API_VERSION( 0, 5 )
  Because CodeWarrior does macro expansion before test evaluation.
  We define wxCHECK_W32API_VERSION here if it's undefined.
*/
#if !defined(__GNUG__) && !defined(wxCHECK_W32API_VERSION)
    #define wxCHECK_W32API_VERSION(maj, min) (0)
#endif

// StartDoc

#ifdef StartDoc
   #undef StartDoc
   #if defined( __GNUG__ ) && !wxCHECK_W32API_VERSION( 0, 5 )
      #define DOCINFOW DOCINFO
      #define DOCINFOA DOCINFO
   #endif
   #ifdef _UNICODE
   inline int StartDoc(HDC h, CONST DOCINFOW* info)
   {
      return StartDocW(h, (DOCINFOW*) info);
   }
   #else
   inline int StartDoc(HDC h, CONST DOCINFOA* info)
   {
      return StartDocA(h, (DOCINFOA*) info);
   }
   #endif
#endif

// GetObject

#ifdef GetObject
   #undef GetObject
   inline int GetObject(HGDIOBJ h, int i, LPVOID buffer)
   {
   #ifdef _UNICODE
      return GetObjectW(h, i, buffer);
   #else
      return GetObjectA(h, i, buffer);
   #endif
   }
#endif

// GetMessage

#ifdef GetMessage
   #undef GetMessage
   inline int GetMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
   {
   #ifdef _UNICODE
      return GetMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
   #else
      return GetMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
   #endif
   }
#endif

// LoadLibrary

#ifdef LoadLibrary
    #undef LoadLibrary
    #ifdef _UNICODE
    inline HINSTANCE LoadLibrary(LPCWSTR lpLibFileName)
    {
        return LoadLibraryW(lpLibFileName);
    }
    #else
    inline HINSTANCE LoadLibrary(LPCSTR lpLibFileName)
    {
        return LoadLibraryA(lpLibFileName);
    }
    #endif
#endif

// FindResource
#ifdef FindResource
    #undef FindResource
    #ifdef _UNICODE
    inline HRSRC FindResource(HMODULE hModule, LPCWSTR lpName, LPCWSTR lpType)
    {
        return FindResourceW(hModule, lpName, lpType);
    }
    #else
    inline HRSRC FindResource(HMODULE hModule, LPCSTR lpName, LPCSTR lpType)
    {
        return FindResourceA(hModule, lpName, lpType);
    }
    #endif
#endif

// IsMaximized

#ifdef IsMaximized
    #undef IsMaximized
    inline BOOL IsMaximized(HWND hwnd)
    {
        return IsZoomed(hwnd);
    }
#endif

// GetFirstChild

#ifdef GetFirstChild
    #undef GetFirstChild
    inline HWND GetFirstChild(HWND hwnd)
    {
        return GetTopWindow(hwnd);
    }
#endif

// GetPrevSibling

#ifdef GetPrevSibling
    #undef GetPrevSibling
    inline HWND GetPrevSibling(HWND hwnd)
    {
        return GetWindow(hwnd,GW_HWNDPREV);
    }
#endif

// GetNextSibling

#ifdef GetNextSibling
    #undef GetNextSibling
    inline HWND GetNextSibling(HWND hwnd)
    {
        return GetWindow(hwnd,GW_HWNDNEXT);
    }
#endif

// For WINE

#if defined(GetWindowStyle) || defined(__WXWINE__)
  #undef GetWindowStyle
#endif

// For ming and cygwin

// GetFirstChild
#ifdef GetFirstChild
   #undef GetFirstChild
   inline HWND GetFirstChild(HWND h)
   {
      return GetTopWindow(h);
   }
#endif


// GetNextSibling
#ifdef GetNextSibling
   #undef GetNextSibling
   inline HWND GetNextSibling(HWND h)
   {
     return GetWindow(h, GW_HWNDNEXT);
   }
#endif


#ifdef Yield
    #undef Yield
#endif

// GetWindowProc
//ifdef GetWindowProc
//   #undef GetWindowProc
//endif
//ifdef GetNextChild
//    #undef GetNextChild
//endif

// #endif // _WX_WINUNDEF_H_

