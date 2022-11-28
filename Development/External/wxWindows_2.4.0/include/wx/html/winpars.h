/////////////////////////////////////////////////////////////////////////////
// Name:        winpars.h
// Purpose:     wxHtmlWinParser class (parser to be used with wxHtmlWindow)
// Author:      Vaclav Slavik
// RCS-ID:      $Id: winpars.h,v 1.15.2.1 2002/11/09 01:04:07 VS Exp $
// Copyright:   (c) 1999 Vaclav Slavik
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////


#ifndef _WX_WINPARS_H_
#define _WX_WINPARS_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "winpars.h"
#endif

#include "wx/defs.h"
#if wxUSE_HTML

#include "wx/module.h"
#include "wx/font.h"
#include "wx/html/htmlpars.h"
#include "wx/html/htmlcell.h"
#include "wx/encconv.h"

class WXDLLEXPORT wxHtmlWindow;
class WXDLLEXPORT wxHtmlWinParser;
class WXDLLEXPORT wxHtmlWinTagHandler;
class WXDLLEXPORT wxHtmlTagsModule;

//--------------------------------------------------------------------------------
// wxHtmlWinParser
//                  This class is derived from wxHtmlParser and its mail goal
//                  is to parse HTML input so that it can be displayed in
//                  wxHtmlWindow. It uses special wxHtmlWinTagHandler.
//--------------------------------------------------------------------------------

class WXDLLEXPORT wxHtmlWinParser : public wxHtmlParser
{
    friend class wxHtmlWindow;

public:
    wxHtmlWinParser(wxHtmlWindow *wnd = NULL);
    ~wxHtmlWinParser();

    virtual void InitParser(const wxString& source);
    virtual void DoneParser();
    virtual wxObject* GetProduct();

    virtual wxFSFile *OpenURL(wxHtmlURLType type, const wxString& url) const;

    // Set's the DC used for parsing. If SetDC() is not called,
    // parsing won't proceed
    virtual void SetDC(wxDC *dc, double pixel_scale = 1.0)
        { m_DC = dc; m_PixelScale = pixel_scale; }

    wxDC *GetDC() {return m_DC;}
    double GetPixelScale() {return m_PixelScale;}
    int GetCharHeight() const {return m_CharHeight;}
    int GetCharWidth() const {return m_CharWidth;}

    // NOTE : these functions do _not_ return _actual_
    // height/width. They return h/w of default font
    // for this DC. If you want actual values, call
    // GetDC()->GetChar...()

    // returns associated wxWindow
    wxHtmlWindow *GetWindow() {return m_Window;}

    // sets fonts to be used when displaying HTML page.
    void SetFonts(wxString normal_face, wxString fixed_face, const int *sizes);

    // Adds tags module. see wxHtmlTagsModule for details.
    static void AddModule(wxHtmlTagsModule *module);

    static void RemoveModule(wxHtmlTagsModule *module);

    // parsing-related methods. These methods are called by tag handlers:

    // Returns pointer to actual container. Common use in tag handler is :
    // m_WParser->GetContainer()->InsertCell(new ...);
    wxHtmlContainerCell *GetContainer() const {return m_Container;}

    // opens new container. This container is sub-container of opened
    // container. Sets GetContainer to newly created container
    // and returns it.
    wxHtmlContainerCell *OpenContainer();

    // works like OpenContainer except that new container is not created
    // but c is used. You can use this to directly set actual container
    wxHtmlContainerCell *SetContainer(wxHtmlContainerCell *c);

    // closes the container and sets actual Container to upper-level
    // container
    wxHtmlContainerCell *CloseContainer();

    int GetFontSize() const {return m_FontSize;}
    void SetFontSize(int s);
    int GetFontBold() const {return m_FontBold;}
    void SetFontBold(int x) {m_FontBold = x;}
    int GetFontItalic() const {return m_FontItalic;}
    void SetFontItalic(int x) {m_FontItalic = x;}
    int GetFontUnderlined() const {return m_FontUnderlined;}
    void SetFontUnderlined(int x) {m_FontUnderlined = x;}
    int GetFontFixed() const {return m_FontFixed;}
    void SetFontFixed(int x) {m_FontFixed = x;}
    wxString GetFontFace() const {return GetFontFixed() ? m_FontFaceFixed : m_FontFaceNormal;}
    void SetFontFace(const wxString& face);

    int GetAlign() const {return m_Align;}
    void SetAlign(int a) {m_Align = a;}
    const wxColour& GetLinkColor() const { return m_LinkColor; }
    void SetLinkColor(const wxColour& clr) { m_LinkColor = clr; }
    const wxColour& GetActualColor() const { return m_ActualColor; }
    void SetActualColor(const wxColour& clr) { m_ActualColor = clr ;}
    const wxHtmlLinkInfo& GetLink() const { return m_Link; }
    void SetLink(const wxHtmlLinkInfo& link);

#if !wxUSE_UNICODE
    void SetInputEncoding(wxFontEncoding enc);
    wxFontEncoding GetInputEncoding() const { return m_InputEnc; }
    wxFontEncoding GetOutputEncoding() const { return m_OutputEnc; }
    wxEncodingConverter *GetEncodingConverter() const { return m_EncConv; }
#endif

    // creates font depending on m_Font* members.
    virtual wxFont* CreateCurrentFont();

protected:
    virtual void AddText(const wxChar* txt);

private:
    bool m_tmpLastWasSpace;
    wxChar *m_tmpStrBuf;
    size_t  m_tmpStrBufSize;
        // temporary variables used by AddText
    wxHtmlWindow *m_Window;
            // window we're parsing for
    double m_PixelScale;
    wxDC *m_DC;
            // Device Context we're parsing for
    static wxList m_Modules;
            // list of tags modules (see wxHtmlTagsModule for details)
            // This list is used to initialize m_Handlers member.

    wxHtmlContainerCell *m_Container;
            // actual container. See Open/CloseContainer for details.

    int m_FontBold, m_FontItalic, m_FontUnderlined, m_FontFixed; // this is not TRUE,FALSE but 1,0, we need it for indexing
    int m_FontSize; /* -2 to +4,  0 is default */
    wxColour m_LinkColor;
    wxColour m_ActualColor;
            // basic font parameters.
    wxHtmlLinkInfo m_Link;
            // actual hypertext link or empty string
    bool m_UseLink;
            // TRUE if m_Link is not empty
    long m_CharHeight, m_CharWidth;
            // average height of normal-sized text
    int m_Align;
            // actual alignment

    wxFont* m_FontsTable[2][2][2][2][7];
    wxString m_FontsFacesTable[2][2][2][2][7];
#if !wxUSE_UNICODE
    wxFontEncoding m_FontsEncTable[2][2][2][2][7];
#endif
            // table of loaded fonts. 1st four indexes are 0 or 1, depending on on/off
            // state of these flags (from left to right):
            // [bold][italic][underlined][fixed_size]
            // last index is font size : from 0 to 6 (remapped from html sizes 1 to 7)
            // Note : this table covers all possible combinations of fonts, but not
            // all of them are used, so many items in table are usually NULL.
    int m_FontsSizes[7];
    wxString m_FontFaceFixed, m_FontFaceNormal;
            // html font sizes and faces of fixed and proportional fonts

#if !wxUSE_UNICODE
    wxFontEncoding m_InputEnc, m_OutputEnc;
            // I/O font encodings
    wxEncodingConverter *m_EncConv;
#endif
};






//--------------------------------------------------------------------------------
// wxHtmlWinTagHandler
//                  This is basicly wxHtmlTagHandler except
//                  it is extended with protected member m_Parser pointing to
//                  the wxHtmlWinParser object
//--------------------------------------------------------------------------------

class WXDLLEXPORT wxHtmlWinTagHandler : public wxHtmlTagHandler
{
    DECLARE_ABSTRACT_CLASS(wxHtmlWinTagHandler)

public:
    wxHtmlWinTagHandler() : wxHtmlTagHandler() {};

    virtual void SetParser(wxHtmlParser *parser) {wxHtmlTagHandler::SetParser(parser); m_WParser = (wxHtmlWinParser*) parser;};

protected:
    wxHtmlWinParser *m_WParser; // same as m_Parser, but overcasted
};






//--------------------------------------------------------------------------------
// wxHtmlTagsModule
//                  This is basic of dynamic tag handlers binding.
//                  The class provides methods for filling parser's handlers
//                  hash table.
//                  (See documentation for details)
//--------------------------------------------------------------------------------

class WXDLLEXPORT wxHtmlTagsModule : public wxModule
{
    DECLARE_DYNAMIC_CLASS(wxHtmlTagsModule)

public:
    wxHtmlTagsModule() : wxModule() {};

    virtual bool OnInit();
    virtual void OnExit();

    // This is called by wxHtmlWinParser.
    // The method must simply call parser->AddTagHandler(new <handler_class_name>);
    // for each handler
    virtual void FillHandlersTable(wxHtmlWinParser * WXUNUSED(parser)) { }
};


#endif

#endif // _WX_WINPARS_H_





