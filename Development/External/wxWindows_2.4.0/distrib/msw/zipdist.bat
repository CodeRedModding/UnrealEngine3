@echo off
rem Zip up an external, generic + Windows distribution of wxWindows 2
set src=%wxwin%
set dest=%src%\deliver
set wise=0

Rem Set this to the required version
set version=2.3.3

if "%src%" == "" goto usage
if "%dest%" == "" goto usage
if "%1%" == "-help" goto usage
if "%1%" == "--help" goto usage
if "%1%" == "/?" goto usage
if "%1%" == "wiseonly" goto dounzip
if "%1%" == "wise" set wise=1
echo About to archive an external wxWindows distribution:
echo   From   %src%
echo   To     %dest%
if "%wise" == "1" echo with WISE setup creation.
echo CTRL-C if this is not correct.
pause

erase %dest%\wx*.zip
erase %dest%\*.htb
erase %dest%\ogl3.zip
erase %dest%\tex2rtf2.zip
erase %dest%\wxxml.zip
erase %dest%\jpeg.zip
erase %dest%\tiff.zip
erase %dest%\dialoged.zip
erase %dest%\utils.zip
erase %dest%\extradoc.zip
erase %dest%\*-win32.zip

if exist %dest%\wx\BuildCVS.txt rmdir /S %dest%\wx

cd %src%
echo Zipping...

Rem Zip up the complete wxMSW-xxx.zip file
Rem NO, do it by unarchiving component files, deleting/copying some files, then
Rem rearchiving.
Rem zip -@ %dest%\wxMSW-%version%.zip < %src%\distrib\msw\generic.rsp
Rem zip -@ -u %dest%\wxMSW-%version%.zip < %src%\distrib\msw\msw.rsp
Rem zip -@ -u %dest%\wxMSW-%version%.zip < %src%\distrib\msw\wx_chm.rsp
Rem zip -@ -u %dest%\wxMSW-%version%.zip  < %src%\distrib\msw\jpeg.rsp
Rem zip -@ -u %dest%\wxMSW-%version%.zip < %src%\distrib\msw\tiff.rsp

Rem Below is the old-style separated-out format. This is retained only
Rem for local use, and for creating wxMSW-xxx.zip.
zip -@ %dest%\wxWindows-%version%-gen.zip < %src%\distrib\msw\generic.rsp
zip -@ -u %dest%\wxWindows-%version%-gen.zip < %src%\distrib\msw\makefile.rsp
zip -@ %dest%\wxWindows-%version%-msw.zip < %src%\distrib\msw\msw.rsp
zip -@ -u %dest%\wxWindows-%version%-msw.zip < %src%\distrib\msw\makefile.rsp
zip -@ %dest%\wxWindows-%version%-gtk.zip < %src%\distrib\msw\gtk.rsp
zip -@ -u %dest%\wxWindows-%version%-gtk.zip < %src%\distrib\msw\makefile.rsp
zip -@ %dest%\wxWindows-%version%-stubs.zip < %src%\distrib\msw\stubs.rsp
zip -@ %dest%\wxWindows-%version%-mot.zip < %src%\distrib\msw\motif.rsp
zip -@ -u %dest%\wxWindows-%version%-mot.zip < %src%\distrib\msw\makefile.rsp
Rem (Obsolete) zip -@ %dest%\wxWindows-%version%-user.zip < %src%\distrib\msw\user.rsp

zip -@ %dest%\wxWindows-%version%-DocSource.zip < %src%\distrib\msw\docsrc.rsp
zip -@ %dest%\wxWindows-%version%-WinHelp.zip < %src%\distrib\msw\wx_hlp.rsp
zip -@ %dest%\wxWindows-%version%-HTML.zip < %src%\distrib\msw\wx_html.rsp
zip -@ %dest%\wxWindows-%version%-PDF.zip < %src%\distrib\msw\wx_pdf.rsp
zip -@ %dest%\wxWindows-%version%-Word.zip < %src%\distrib\msw\wx_word.rsp
zip -@ %dest%\wxWindows-%version%-HTB.zip < %src%\distrib\msw\wx_htb.rsp
zip -@ %dest%\wxWindows-%version%-HTMLHelp.zip < %src%\distrib\msw\wx_chm.rsp

Rem PDF/HTML docs that should go into the Windows setup because
Rem there are no WinHelp equivalents
zip -@ %dest%\extradoc.zip < %src%\distrib\msw\extradoc.rsp

rem VC++ project files
zip -@ %dest%\wxWindows-%version%-vc.zip < %src%\distrib\msw\vc.rsp

rem BC++ project files
zip -@ %dest%\wxWindows-%version%-bc.zip < %src%\distrib\msw\bc.rsp

rem CodeWarrior project files
zip -@ %dest%\wxWindows-%version%-cw.zip < %src%\distrib\msw\cw.rsp

rem OGL 3
zip -@ %dest%\ogl3.zip < %src%\distrib\msw\ogl.rsp

rem wxXML
zip -@ %dest%\wxxml.zip < %src%\distrib\msw\xml.rsp

rem MMedia
zip -@ %dest%\mmedia.zip < %src%\distrib\msw\mmedia.rsp

rem STC (Scintilla widget)
zip -@ %dest%\stc.zip < %src%\distrib\msw\stc.rsp

rem GLCanvas: obsolete, now in main library
rem zip -@ %dest%\glcanvas.zip < %src%\distrib\msw\glcanvas.rsp

rem Tex2RTF
zip -@ %dest%\tex2rtf2.zip < %src%\distrib\msw\tex2rtf.rsp

rem JPEG source
zip -@ %dest%\jpeg.zip < %src%\distrib\msw\jpeg.rsp

rem TIFF source
zip -@ %dest%\tiff.zip < %src%\distrib\msw\tiff.rsp

rem Dialog Editor source and binary
erase %dest%\dialoged_source.zip
zip -@ %dest%\dialoged_source.zip < %src%\distrib\msw\dialoged.rsp
zip -j %dest%\dialoged.zip %dest%\dialoged_source.zip %src%\bin\dialoged.exe %src%\docs\winhelp\dialoged.hlp %src%\docs\winhelp\dialoged.cnt
erase %dest%\dialoged_source.zip

rem Misc. utils not in the main distribution
zip -@ %dest%\utils.zip < %src%\distrib\msw\utils.rsp
zip -@ -u %dest%\utils.zip < %src%\distrib\msw\utilmake.rsp

copy %src%\docs\changes.txt %dest%
copy %src%\docs\msw\install.txt %dest%\install_msw.txt
copy %src%\docs\motif\install.txt %dest%\install_motif.txt
copy %src%\docs\gtk\install.txt %dest%\install_gtk.txt
copy %src%\docs\readme.txt %dest%
copy %src%\docs\motif\readme.txt %dest%\readme_motif.txt
copy %src%\docs\gtk\readme.txt %dest%\readme_gtk.txt
copy %src%\docs\msw\readme.txt %dest%\readme_msw.txt
copy %src%\docs\readme_vms.txt %dest%
copy %src%\docs\motif\makewxmotif %dest%
copy %src%\docs\gtk\makewxgtk %dest%

Rem Skip WISE setup if wise is 0.
if "%wise%" == "0" goto end

:dounzip

Rem Make dialoged-win32.zip and tex2rtf-win32.zip

cd %src%\bin

zip %dest%\dialoged-win32.zip dialoged.*
zip %dest%\tex2rtf-win32.zip tex2rtf.*

cd %dest%

rem Unzip the Windows files into 'wx'
mkdir %dest%\wx

Rem After this change of directory, we're in the
Rem temporary 'wx' directory and not acting on
Rem the source wxWindows directory.
cd %dest%\wx
unzip -o ..\wxWindows-%version%-msw.zip
unzip -o ..\wxWindows-%version%-gen.zip
unzip -o ..\wxWindows-%version%-vc.zip
unzip -o ..\wxWindows-%version%-bc.zip
unzip -o ..\wxWindows-%version%-HTMLHelp.zip
unzip -o ..\extradoc.zip
Rem Need Word file, for Remstar DB classes
unzip -o ..\wxWindows-%version%-Word.zip
unzip -o ..\ogl3.zip
unzip -o ..\wxxml.zip
unzip -o ..\jpeg.zip
unzip -o ..\tiff.zip
unzip -o ..\tex2rtf2.zip

rem Now delete a few files that are unnecessary
attrib -R *
erase /Y BuildCVS.txt *.in *.spec *.guess *.sub mkinstalldirs modules install-sh *.sh descrip.mms
erase /Y configure samples\configure samples\*.in demos\configure demos\*.in contrib\configure contrib\*.in
erase /Y setup.h.in setup.h_vms
erase /SY Makefile.in
erase /Y docs\html\wxbook.htm docs\html\roadmap.htm
erase /Y contrib\docs\winhelp\mmedia.*
erase /Y contrib\docs\winhelp\stc.*
erase /Y contrib\docs\htmlhelp\mmedia.*
erase /Y contrib\docs\htmlhelp\stc.*
erase /Y contrib\docs\pdf\*.*
rmdir /S contrib\docs\latex\ogl
erase /Y src\mingegcs.bat
erase /Y distrib

rem Now copy some binary files to 'bin'
if not isdir bin mkdir bin
copy %src%\bin\dialoged.exe bin
copy %src%\bin\tex2rtf.exe bin
copy %src%\bin\dbgview.* bin
Rem copy %src%\bin\life.exe bin
copy %src%\docs\winhelp\dialoged.hlp %src%\docs\winhelp\dialoged.cnt bin
copy %src%\docs\winhelp\tex2rtf.hlp %src%\docs\winhelp\tex2rtf.cnt bin

Rem Make wxMSW-xxx.zip
zip -r ..\wxMSW-%version%.zip *

rem Time to regenerate the WISE install script, wxwin2.wse.
rem NB: if you've changed wxwin2.wse using WISE, call splitwise.exe
rem from within distrib\msw, to split off wisetop.txt and wisebott.txt.
echo Calling 'makewise' to generate wxwin2.wse...
call %WXWIN%\distrib\msw\makewise.bat

erase /Y %dest%\setup.*

rem Now invoke WISE install on the new wxwin2.wse
set wisecmd="c:\Program Files\wise\wise32.exe" /C %WXWIN%\distrib\msw\wxwin2.wse
echo Invoking %wisecmd...
start %wisecmd
pause Press any key to continue with the wxWindows distribution...
Rem ren %WXWIN%\deliver\setup.EXE %WXWIN%\deliver\wxMSW-%version%-setup.exe

cd %dest%

Rem tidy up capitalisation of filenames
ren setup.EXE s
ren s setup.exe

ren setup.w02 s
ren s setup.w02

ren setup.w03 s
ren s setup.w03

ren setup.w04 s
ren s setup.w04

ren setup.w05 s
ren s setup.w05

ren setup.w06 s
ren s setup.w06

ren setup.w07 s
ren s setup.w07

ren setup.w08 s
ren s setup.w08

rem Put all the setup files into a single zip archive.
zip wxMSW-%version%-setup.zip readme.txt setup.*

echo wxWindows archived.

goto end

:usage
echo DOS wxWindows distribution. Zips up all GTK/Motif/MSW/doc files,
echo and optionally makes a deliver\wx directory and a setup.exe
echo if you specify 'wise' (skipping the zipping if you use 'wiseonly').
echo.
echo Usage: zipdist [wise | wiseonly]

:end
