/////////////////////////////////////////////////////////////////////////////
// Name:        reswrite.cpp
// Purpose:     Resource writing functionality
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id: reswrite.cpp,v 1.20 2002/04/30 07:34:15 JS Exp $
// Copyright:   (c) Julian Smart
// Licence:   	wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#pragma implementation
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "wx/scrolbar.h"
#include "wx/string.h"
#include "wx/wfstream.h"
#include "wx/txtstrm.h"

#include "reseditr.h"

static char deBuffer[512];

char *SafeString(char *s);
char *SafeWord(const wxString& s);

// Save an association between the child resource and the panel item, to allow
// us not to require unique window names.
wxControl *wxResourceTableWithSaving::CreateItem(wxPanel *panel, const wxItemResource *childResource, const wxItemResource* parentResource)
{
    wxControl *item = wxResourceTable::CreateItem(panel, childResource, parentResource);
    if (item)
        wxResourceManager::GetCurrentResourceManager()->GetResourceAssociations().Put((long)childResource, item);
    return item;
}

void wxResourceTableWithSaving::OutputFont(wxTextOutputStream& stream, const wxFont& font)
{
    stream << "[" << font.GetPointSize() << ", '";
    stream << font.GetFamilyString() << "', '";
    stream << font.GetStyleString() << "', '";
    stream << font.GetWeightString() << "', ";
    stream << (int)font.GetUnderlined();
    if (font.GetFaceName() != "")
        stream << ", '" << font.GetFaceName() << "'";
    stream << "]";
}

/*
* Resource table with saving (basic one only has loading)
*/

bool wxResourceTableWithSaving::Save(const wxString& filename)
{
    wxFileOutputStream file_output( filename );
    if (file_output.LastError())
        return FALSE;
    
    wxTextOutputStream stream( file_output );
    
    BeginFind();
    wxNode *node = NULL;
    while ((node = Next()))
    {
        wxItemResource *item = (wxItemResource *)node->Data();
        wxString resType(item->GetType());
        
        if (resType == "wxDialogBox" || resType == "wxDialog" || resType == "wxPanel" || resType == "wxBitmap")
        {
            if (!SaveResource(stream, item, (wxItemResource*) NULL))
                return FALSE;
        }
    }
    return TRUE;
}

bool wxResourceTableWithSaving::SaveResource(wxTextOutputStream& stream, wxItemResource* item, wxItemResource* parentItem)
{
    char styleBuf[400];
    wxString itemType(item->GetType());
    
    if (itemType == "wxDialogBox" || itemType == "wxDialog" || itemType == "wxPanel")
    {
        if (itemType == "wxDialogBox" || itemType == "wxDialog")
        {
            stream << "static char *" << item->GetName() << " = \"dialog(name = '" << item->GetName() << "',\\\n";
            GenerateDialogStyleString(item->GetStyle(), styleBuf);
        }
        else
        {
            stream << "static char *" << item->GetName() << " = \"panel(name = '" << item->GetName() << "',\\\n";
            GenerateDialogStyleString(item->GetStyle(), styleBuf);
        }
        
        stream << "  style = '" << styleBuf << "',\\\n";
        stream << "  title = " << SafeWord(item->GetTitle()) << ",\\\n";
        stream << "  id = " << item->GetId() << ",\\\n";
        stream << "  x = " << item->GetX() << ", y = " << item->GetY();
        stream << ", width = " << item->GetWidth() << ", height = " << item->GetHeight();
        
        if (1) // item->GetStyle() & wxNO_3D)
        {
            if (item->GetBackgroundColour().Ok())
            {
                char buf[7];
                wxDecToHex(item->GetBackgroundColour().Red(), buf);
                wxDecToHex(item->GetBackgroundColour().Green(), buf+2);
                wxDecToHex(item->GetBackgroundColour().Blue(), buf+4);
                buf[6] = 0;
                
                stream << ",\\\n  " << "background_colour = '" << buf << "'";
            }
        }
        
        int dialogUnits = 0;
        int useDefaults = 0;
        if ((item->GetResourceStyle() & wxRESOURCE_DIALOG_UNITS) != 0)
            dialogUnits = 1;
        if ((item->GetResourceStyle() & wxRESOURCE_USE_DEFAULTS) != 0)
            useDefaults = 1;
        
        stream << ",\\\n  " << "use_dialog_units = " << dialogUnits;
        stream << ",\\\n  " << "use_system_defaults = " << useDefaults;
        
        if (item->GetFont().Ok())
        {
            stream << ",\\\n  font = ";
            OutputFont(stream, item->GetFont());
        }
        
        if (item->GetChildren().Number() > 0)
            stream << ",\\\n";
        else
            stream << "\\\n";
        wxNode *node = item->GetChildren().First();
        while (node)
        {
            wxItemResource *child = (wxItemResource *)node->Data();
            
            stream << "  control = [";
            
            SaveResource(stream, child, item);
            
            stream << "]";
            
            if (node->Next())
                stream << ",\\\n";
            node = node->Next();
        }
        stream << ").\";\n\n";
    }
    else if (itemType == "wxButton" || itemType == "wxBitmapButton")
    {
        GenerateControlStyleString(itemType, item->GetStyle(), styleBuf);
        stream << item->GetId() << ", " << itemType << ", " << SafeWord(item->GetTitle()) << ", '" << styleBuf << "', ";
        stream << SafeWord(item->GetName()) << ", " << item->GetX() << ", " << item->GetY() << ", ";
        stream << item->GetWidth() << ", " << item->GetHeight();
        if (item->GetValue4())
            stream << ", '" << item->GetValue4() << "'";
        if (item->GetFont().Ok())
        {
            stream << ",\\\n      ";
            OutputFont(stream, item->GetFont());
        }
    }
    else if (itemType == "wxStaticText" || itemType == "wxStaticBitmap")
    {
        GenerateControlStyleString(itemType, item->GetStyle(), styleBuf);
        stream << item->GetId() << ", " << itemType << ", " << SafeWord(item->GetTitle()) << ", '" << styleBuf << "', ";
        stream << SafeWord(item->GetName()) << ", " << item->GetX() << ", " << item->GetY() << ", ";
        stream << item->GetWidth() << ", " << item->GetHeight();
        if (item->GetValue4())
            stream << ", '" << item->GetValue4() << "'";
        if (item->GetFont().Ok())
        {
            stream << ",\\\n      ";
            OutputFont(stream, item->GetFont());
        }
    }
    else if (itemType == "wxCheckBox")
    {
        GenerateControlStyleString(itemType, item->GetStyle(), styleBuf);
        stream << item->GetId() << ", " << "wxCheckBox, " << SafeWord(item->GetTitle()) << ", '" << styleBuf << "', ";
        stream << SafeWord(item->GetName()) << ", " << item->GetX() << ", " << item->GetY() << ", ";
        stream << item->GetWidth() << ", " << item->GetHeight();
        stream << ", " << item->GetValue1();
        if (item->GetFont().Ok())
        {
            stream << ",\\\n      ";
            OutputFont(stream, item->GetFont());
        }
    }
    else if (itemType == "wxRadioButton")
    {
        GenerateControlStyleString(itemType, item->GetStyle(), styleBuf);
        stream << item->GetId() << ", " << "wxRadioButton, " << SafeWord(item->GetTitle()) << ", '" << styleBuf << "', ";
        stream << SafeWord(item->GetName()) << ", " << item->GetX() << ", " << item->GetY() << ", ";
        stream << item->GetWidth() << ", " << item->GetHeight();
        stream << ", " << item->GetValue1();
        if (item->GetFont().Ok())
        {
            stream << ",\\\n      ";
            OutputFont(stream, item->GetFont());
        }
    }
    else if (itemType == "wxStaticBox")
    {
        GenerateControlStyleString(itemType, item->GetStyle(), styleBuf);
        stream << item->GetId() << ", " << "wxStaticBox, " << SafeWord(item->GetTitle()) << ", '" << styleBuf << "', ";
        stream << SafeWord(item->GetName()) << ", " << item->GetX() << ", " << item->GetY() << ", ";
        stream << item->GetWidth() << ", " << item->GetHeight();
        if (item->GetFont().Ok())
        {
            stream << ",\\\n      ";
            OutputFont(stream, item->GetFont());
        }
    }
    else if (itemType == "wxText" || itemType == "wxMultiText" || itemType == "wxTextCtrl")
    {
        GenerateControlStyleString(itemType, item->GetStyle(), styleBuf);
        stream << item->GetId() << ", " << "wxTextCtrl, ";
        stream << SafeWord(item->GetTitle()) << ", '" << styleBuf << "', ";
        stream << SafeWord(item->GetName()) << ", " << item->GetX() << ", " << item->GetY() << ", ";
        stream << item->GetWidth() << ", " << item->GetHeight();
        stream << ", " << SafeWord(item->GetValue4());
        if (item->GetFont().Ok())
        {
            stream << ",\\\n      ";
            OutputFont(stream, item->GetFont());
        }
    }
    else if (itemType == "wxGauge")
    {
        GenerateControlStyleString(itemType, item->GetStyle(), styleBuf);
        stream << item->GetId() << ", " << "wxGauge, " << SafeWord(item->GetTitle()) << ", '" << styleBuf << "', ";
        stream << SafeWord(item->GetName()) << ", " << item->GetX() << ", " << item->GetY() << ", ";
        stream << item->GetWidth() << ", " << item->GetHeight();
        stream << ", " << item->GetValue1() << ", " << item->GetValue2();
        if (item->GetFont().Ok())
        {
            stream << ",\\\n      ";
            OutputFont(stream, item->GetFont());
        }
    }
    else if (itemType == "wxSlider")
    {
        GenerateControlStyleString(itemType, item->GetStyle(), styleBuf);
        stream << item->GetId() << ", " << "wxSlider, " << SafeWord(item->GetTitle()) << ", '" << styleBuf << "', ";
        stream << SafeWord(item->GetName()) << ", " << item->GetX() << ", " << item->GetY() << ", ";
        stream << item->GetWidth() << ", " << item->GetHeight();
        stream << ", " << item->GetValue1() << ", " << item->GetValue2() << ", " << item->GetValue3();
        if (item->GetFont().Ok())
        {
            stream << ",\\\n      ";
            OutputFont(stream, item->GetFont());
        }
    }
    else if (itemType == "wxScrollBar")
    {
        GenerateControlStyleString(itemType, item->GetStyle(), styleBuf);
        stream << item->GetId() << ", " << "wxScrollBar, " << SafeWord(item->GetTitle()) << ", '" << styleBuf << "', ";
        stream << SafeWord(item->GetName()) << ", " << item->GetX() << ", " << item->GetY() << ", ";
        stream << item->GetWidth() << ", " << item->GetHeight();
        stream << ", " << item->GetValue1() << ", " << item->GetValue2() << ", " << item->GetValue3() << ", ";
        stream << item->GetValue5();
    }
    else if (itemType == "wxListBox")
    {
        GenerateControlStyleString(itemType, item->GetStyle(), styleBuf);
        stream << item->GetId() << ", " << "wxListBox, " << SafeWord(item->GetTitle()) << ", '" << styleBuf << "', ";
        stream << SafeWord(item->GetName()) << ", " << item->GetX() << ", " << item->GetY() << ", ";
        stream << item->GetWidth() << ", " << item->GetHeight();
        
        // Default list of values
        
        stream << ", [";
        if (item->GetStringValues().Number() > 0)
        {
            wxNode *node = item->GetStringValues().First();
            while (node)
            {
                char *s = (char *)node->Data();
                stream << SafeWord(s);
                if (node->Next())
                    stream << ", ";
                node = node->Next();
            }
        }
        stream << "]";
        /* Styles are now in the window style, not in a separate arg
        stream << ", ";
        switch (item->GetValue1())
        {
        case wxLB_MULTIPLE:
        {
        stream << "'wxLB_MULTIPLE'";
        break;
        }
        case wxLB_EXTENDED:
        {
        stream << "'wxLB_EXTENDED'";
        break;
        }
        case wxLB_SINGLE:
        default:
        {
        stream << "'wxLB_SINGLE'";
        break;
        }
        }
        */
        
        if (item->GetFont().Ok())
        {
            stream << ",\\\n      ";
            OutputFont(stream, item->GetFont());
        }
    }
    else if (itemType == "wxChoice" || itemType == "wxComboBox")
    {
        GenerateControlStyleString(itemType, item->GetStyle(), styleBuf);
        
        stream << item->GetId() << ", " << itemType << ", " << SafeWord(item->GetTitle()) << ", '" << styleBuf << "', ";
        stream << SafeWord(item->GetName()) << ", " << item->GetX() << ", " << item->GetY() << ", ";
        stream << item->GetWidth() << ", " << item->GetHeight();
        
        if (itemType == "wxComboBox")
            stream << ", " << SafeWord(item->GetValue4());
        
        // Default list of values
        
        stream << ", [";
        if (item->GetStringValues().Number() > 0)
        {
            wxNode *node = item->GetStringValues().First();
            while (node)
            {
                char *s = (char *)node->Data();
                stream << SafeWord(s);
                if (node->Next())
                    stream << ", ";
                node = node->Next();
            }
        }
        stream << "]";
        if (item->GetFont().Ok())
        {
            stream << ",\\\n      ";
            OutputFont(stream, item->GetFont());
        }
    }
    else if (itemType == "wxRadioBox")
    {
        // Must write out the orientation and number of rows/cols!!
        GenerateControlStyleString(itemType, item->GetStyle(), styleBuf);
        stream << item->GetId() << ", " << "wxRadioBox, " << SafeWord(item->GetTitle()) << ", '" << styleBuf << "', ";
        stream << SafeWord(item->GetName()) << ", " << item->GetX() << ", " << item->GetY() << ", ";
        stream << item->GetWidth() << ", " << item->GetHeight();
        
        // Default list of values
        
        stream << ", [";
        if (item->GetStringValues().Number() > 0)
        {
            wxNode *node = item->GetStringValues().First();
            while (node)
            {
                char *s = (char *)node->Data();
                stream << SafeWord(s);
                if (node->Next())
                    stream << ", ";
                node = node->Next();
            }
        }
        stream << "], " << item->GetValue1();
        if (item->GetFont().Ok())
        {
            stream << ",\\\n      ";
            OutputFont(stream, item->GetFont());
        }
    }
    else if (itemType == "wxBitmap")
    {
        stream << "static char *" << item->GetName() << " = \"bitmap(name = '" << item->GetName() << "',\\\n";
        
        wxNode *node = item->GetChildren().First();
        while (node)
        {
            wxItemResource *child = (wxItemResource *)node->Data();
            stream << "  bitmap = [";
            
            char buf[400];
            strcpy(buf, child->GetName());
#ifdef __WXMSW__
            wxDos2UnixFilename(buf);
#endif
            
            stream << "'" << buf << "', ";
            
            int bitmapType = (int)child->GetValue1();
            switch (bitmapType)
            {
            case wxBITMAP_TYPE_XBM_DATA:
                {
                    stream << "wxBITMAP_TYPE_XBM_DATA";
                    break;
                }
            case wxBITMAP_TYPE_XPM_DATA:
                {
                    stream << "wxBITMAP_TYPE_XPM_DATA";
                    break;
                }
            case wxBITMAP_TYPE_XBM:
                {
                    stream << "wxBITMAP_TYPE_XBM";
                    break;
                }
            case wxBITMAP_TYPE_XPM:
                {
                    stream << "wxBITMAP_TYPE_XPM";
                    break;
                }
            case wxBITMAP_TYPE_BMP:
                {
                    stream << "wxBITMAP_TYPE_BMP";
                    break;
                }
            case wxBITMAP_TYPE_BMP_RESOURCE:
                {
                    stream << "wxBITMAP_TYPE_BMP_RESOURCE";
                    break;
                }
            case wxBITMAP_TYPE_GIF:
                {
                    stream << "wxBITMAP_TYPE_GIF";
                    break;
                }
            case wxBITMAP_TYPE_TIF:
                {
                    stream << "wxBITMAP_TYPE_TIF";
                    break;
                }
            case wxBITMAP_TYPE_ICO:
                {
                    stream << "wxBITMAP_TYPE_ICO";
                    break;
                }
            case wxBITMAP_TYPE_ICO_RESOURCE:
                {
                    stream << "wxBITMAP_TYPE_ICO_RESOURCE";
                    break;
                }
            case wxBITMAP_TYPE_CUR:
                {
                    stream << "wxBITMAP_TYPE_CUR";
                    break;
                }
            case wxBITMAP_TYPE_CUR_RESOURCE:
                {
                    stream << "wxBITMAP_TYPE_CUR_RESOURCE";
                    break;
                }
            default:
            case wxBITMAP_TYPE_ANY:
                {
                    stream << "wxBITMAP_TYPE_ANY";
                    break;
                }
            }
            stream << ", ";
            int platform = child->GetValue2();
            switch (platform)
            {
            case RESOURCE_PLATFORM_WINDOWS:
                {
                    stream << "'WINDOWS'";
                    break;
                }
            case RESOURCE_PLATFORM_X:
                {
                    stream << "'X'";
                    break;
                }
            case RESOURCE_PLATFORM_MAC:
                {
                    stream << "'MAC'";
                    break;
                }
            case RESOURCE_PLATFORM_ANY:
                {
                    stream << "'ANY'";
                    break;
                }
            }
            int noColours = (int)child->GetValue3();
            if (noColours > 0)
                stream << ", " << noColours;
            
            stream << "]";
            
            if (node->Next())
                stream << ",\\\n";
            
            node = node->Next();
      }
      stream << ").\";\n\n";
    }
    else
    {
        wxString str("Unimplemented resource type: ");
        str += itemType;
        wxMessageBox(str);
    }
    return TRUE;
}

void wxResourceTableWithSaving::GenerateDialogStyleString(long windowStyle, char *buf)
{
    buf[0] = 0;
    m_styleTable.GenerateStyleStrings("wxWindow", windowStyle, buf);
    m_styleTable.GenerateStyleStrings("wxPanel", windowStyle, buf);
    m_styleTable.GenerateStyleStrings("wxDialog", windowStyle, buf);
    
    if (strlen(buf) == 0)
        strcat(buf, "0");
}

void wxResourceTableWithSaving::GeneratePanelStyleString(long windowStyle, char *buf)
{
    buf[0] = 0;
    m_styleTable.GenerateStyleStrings("wxWindow", windowStyle, buf);
    m_styleTable.GenerateStyleStrings("wxPanel", windowStyle, buf);
    
    if (strlen(buf) == 0)
        strcat(buf, "0");
}


void wxResourceTableWithSaving::GenerateControlStyleString(const wxString& windowClass, long windowStyle, char *buf)
{
    buf[0] = 0;
    m_styleTable.GenerateStyleStrings("wxWindow", windowStyle, buf);
    m_styleTable.GenerateStyleStrings("wxControl", windowStyle, buf);
    m_styleTable.GenerateStyleStrings(windowClass, windowStyle, buf);
    
    if (strlen(buf) == 0)
        strcat(buf, "0");
}

// Returns quoted string or "NULL"
char *SafeString(const wxString& s)
{
    if (s == "")
        return "NULL";
    else
    {
        strcpy(deBuffer, "\"");
        strcat(deBuffer, s);
        strcat(deBuffer, "\"");
        return deBuffer;
    }
}

// Returns quoted string or '' : convert " to \"
char *SafeWord(const wxString& s)
{
    const char *cp;
    char *dp;
    
    if (s == "")
        return "''";
    else
    {
        dp = deBuffer;
        cp = s.c_str();
        *dp++ = '\'';
        while(*cp != 0) {
            if(*cp == '"') {
                *dp++ = '\\';
                *dp++ = '"';
            } else if(*cp == '\'') {
                *dp++ = '\\';
                *dp++ = '\'';
            } else
                *dp++ = *cp;
            
            cp++;
        }
        *dp++ = '\'';
        *dp++ = 0;
        
        return deBuffer;
    }
}

