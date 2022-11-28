#!/bin/sh
#
# File:      makeall.sh
# Purpose:   create wxWindows makefiles for all compilers
# Author:    Michael Bedward
# Created:   29 Aug 1999
# Copyright: (c) 1999 Michael Bedward
# Version:   $Id: makeall.sh,v 1.19 2002/09/05 20:54:30 RD Exp $
#
# This creates the makefiles for all compilers from the templates using
# tmake. The tmake executable should be in the path.

# Assume we are in distrib/msw/tmake
#
topdir="../../.."
srcdir="$topdir/src"
mswdir="$srcdir/msw"

for tname in `ls *.t`
do
    case $tname in
	b32.t)
	    echo "Generating $mswdir/makefile.b32 for Borland C++ (32 bits)..."
	    ./tmake -t b32 wxwin.pro -o $mswdir/makefile.b32
	    ./tmake -t b32univ wxwin.pro -o $mswdir/makeuniv.b32
	    ./tmake -t b32base wxwin.pro -o $mswdir/makebase.b32 ;;

	bcc.t)
	    echo "Generating $mswdir/makefile.bcc for Borland C++ (16 bits)..."
	    ./tmake -t bcc wxwin.pro -o $mswdir/makefile.bcc;;

	dos.t)
	    echo "Generating $mswdir/makefile.dos for Visual C++ 1.52..."
	    ./tmake -t dos wxwin.pro -o $mswdir/makefile.dos;;

	g95.t)
	    echo "Generating $mswdir/makefile.g95 for Cygwin/Mingw32..."
	    ./tmake -t g95 wxwin.pro -o $mswdir/makefile.g95;;

	sc.t)
	    echo "Generating $mswdir/makefile.sc for Symantec C++..."
	    ./tmake -t sc wxwin.pro -o $mswdir/makefile.sc;;

	vc.t)
	    echo "Generating $mswdir/makefile.vc for Visual C++ 4.0..."
	    ./tmake -t vc wxwin.pro -o $mswdir/makefile.vc;;

	vc6msw.t)
	    echo "Generating $srcdir/wxWindows.dsp for Visual C++ 6.0..."
	    ./tmake -t vc6msw wxwin.pro -o $srcdir/wxWindows.dsp;;

	vc6base.t)
	    echo "Generating $srcdir/wxBase.dsp for Visual C++ 6.0..."
	    ./tmake -t vc6base wxwin.pro -o $srcdir/wxBase.dsp;;

	vc6univ.t)
	    echo "Generating $srcdir/wxUniv.dsp for Visual C++ 6.0..."
	    ./tmake -t vc6univ wxwin.pro -o $srcdir/wxUniv.dsp;;

	wat.t)
	    echo "Generating $mswdir/makefile.wat for Watcom C++..."
	    ./tmake -t wat wxwin.pro -o $mswdir/makefile.wat;;

	base.t)
	    echo "Generating $topdir/src/files.lst for Configure..."
	    ./tmake -t base wxwin.pro -o $topdir/src/files.lst ;;

	gtk.t)
	    echo "Generating $topdir/src/gtk/files.lst for GTK and Configure..."
	    ./tmake -t gtk wxwin.pro -o $topdir/src/gtk/files.lst;;

	mgl.t)
	    echo "Generating $topdir/src/mgl/files.lst for MGL and Configure..."
	    ./tmake -t mgl wxwin.pro -o $topdir/src/mgl/files.lst;;

	micro.t)
	    echo "Generating $topdir/src/micro/files.lst for MicroWindows and Configure..."
	    ./tmake -t micro wxwin.pro -o $topdir/src/microwin/files.lst;;

	msw.t)
	    echo "Generating $topdir/src/msw/files.lst for MSW and Configure..."
	    ./tmake -t msw wxwin.pro -o $topdir/src/msw/files.lst;;

	mac.t)
	    echo "Generating $topdir/src/mac/files.lst for Mac and Configure..."
	    ./tmake -t mac wxwin.pro -o $topdir/src/mac/files.lst;;

	motif.t)
	    echo "Generating $topdir/src/motif/files.lst for Motif and Configure..."
	    ./tmake -t motif wxwin.pro -o $topdir/src/motif/files.lst;;

	univ.t)
	    echo "Generating $topdir/src/univ/files.lst for wxUniversal..."
	    ./tmake -t univ wxwin.pro -o $topdir/src/univ/files.lst;;

	unx.t)
	    echo "Generating $topdir/src/os2/files.lst for OS/2 PM and Configure..."
	    ./tmake -t os2 wxwin.pro -o $topdir/src/os2/files.lst;;

	mgl.t)
	    echo "Generating $topdir/src/mgl/files.lst for MGL and Configure..."
	    ./tmake -t mgl wxwin.pro -o $topdir/src/mgl/files.lst;;

	x11.t)
	    echo "Generating $topdir/src/x11/files.lst for X11 and Configure..."
	    ./tmake -t x11 wxwin.pro -o $topdir/src/x11/files.lst;;

	watmgl.t)
	    echo "Generating $topdir/src/mgl/makefile.wat for Watcom C++ and MGL+DOS..."
	    ./tmake -t watmgl wxwin.pro -o $topdir/src/mgl/makefile.wat;;
    esac
done

