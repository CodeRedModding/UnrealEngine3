/////////////////////////////////////////////////////////////////////////////
// Name:        wx_cw.h
// Purpose:     wxWindows definitions for CodeWarrior builds
// Author:      Stefan Csomor
// Modified by:
// Created:     12/10/98
// RCS-ID:      $Id: wx_cwc.h,v 1.2 2000/12/10 10:15:06 csomor Exp $
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
		#include <wx_Carbon++_prof.mch>
	#endif
#else
	#if __POWERPC__
		#include <wx_Carbon_prof.mch>
	#endif
#endif
#else
#ifdef __cplusplus
	#if __POWERPC__
		#include <wx_Carbon++.mch>
	#endif
#else
	#if __POWERPC__
		#include <wx_Carbon.mch>
	#endif
#endif
#endif
#endif
    // _WX_CW__
