/////////////////////////////////////////////////////////////////////////////
// Name:        wx_cw.h
// Purpose:     wxWindows definitions for CodeWarrior builds
// Author:      Stefan Csomor
// Modified by:
// Created:     12/10/98
// RCS-ID:      $Id: wxshlba_cwc.h,v 1.1 2002/01/20 16:09:58 SC Exp $
// Copyright:   (c) Stefan Csomor
// Licence:   	wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CW__
#define _WX_CW__

#if __MWERKS__ >= 0x2400
#pragma old_argmatch on
#endif

#if __option(profile)
#ifdef __cplusplus
	#if __POWERPC__
		#include <wxshlba_Carbon++_prof.mch>
	#endif
#else
	#if __POWERPC__
		#include <wxshlba_Carbon_prof.mch>
	#endif
#endif
#else
#ifdef __cplusplus
	#if __POWERPC__
		#include <wxshlba_Carbon++.mch>
	#endif
#else
	#if __POWERPC__
		#include <wxshlba_Carbon.mch>
	#endif
#endif
#endif
#endif
    // _WX_CW__