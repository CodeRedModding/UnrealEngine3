WXDIR = $(%WXWIN)

!include $(WXDIR)\src\makewat.env

WXLIB = $(WXDIR)\lib
LNK = $(PROGRAM).lnk

all: $(PROGRAM).exe $(EXTRATARGETS)

$(PROGRAM).exe : $(OBJECTS) $(PROGRAM).res $(LNK) $(WXLIB)\wx.lib
    wlink @$(LNK)
    $(BINDCOMMAND) $(PROGRAM).res

$(PROGRAM).res :      $(PROGRAM).rc $(WXDIR)\include\wx\msw\wx.rc
     $(RC) $(RESFLAGS1) $(PROGRAM).rc

$(LNK) : makefile.wat
    %create $(LNK)
    @%append $(LNK) debug all
    @%append $(LNK) system $(LINKOPTION)
    @%append $(LNK) $(STACK)
    @%append $(LNK) name $(PROGRAM).exe
    @for %i in ($(OBJECTS)) do @%append $(LNK) file %i
    @for %i in ($(LIBS)) do @%append $(LNK) lib %i
    @for %i in ($(EXTRALIBS)) do @%append $(LNK) lib %i
#    @%append $(LNK) $(MINDATA)
#    @%append $(LNK) $(MAXDATA)

clean:   .SYMBOLIC
    -erase *.obj
    -erase *.bak
    -erase *.err
    -erase *.pch
    -erase *.lib
    -erase $(LNK)
    -erase *.res
    -erase *.exe
    -erase *.lbc

