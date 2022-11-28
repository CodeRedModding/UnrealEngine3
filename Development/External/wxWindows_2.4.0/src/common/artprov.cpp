/////////////////////////////////////////////////////////////////////////////
// Name:        artprov.cpp
// Purpose:     wxArtProvider class
// Author:      Vaclav Slavik
// Modified by:
// Created:     18/03/2002
// RCS-ID:      $Id: artprov.cpp,v 1.7.2.1 2002/11/03 17:19:09 VS Exp $
// Copyright:   (c) Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "artprov.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/list.h"
#endif

#include "wx/artprov.h"
#include "wx/hashmap.h"
#include "wx/module.h"
#include "wx/image.h"

// For the purposes of forcing this module to link
extern char g_ArtProviderModule;

// ===========================================================================
// implementation
// ===========================================================================

#include "wx/listimpl.cpp"
WX_DECLARE_LIST(wxArtProvider, wxArtProvidersList);
WX_DEFINE_LIST(wxArtProvidersList);

// ----------------------------------------------------------------------------
// Cache class - stores already requested bitmaps
// ----------------------------------------------------------------------------

WX_DECLARE_EXPORTED_STRING_HASH_MAP(wxBitmap, wxArtProviderBitmapsHash);

class WXDLLEXPORT wxArtProviderCache
{
public:
    bool GetBitmap(const wxString& full_id, wxBitmap* bmp);
    void PutBitmap(const wxString& full_id, const wxBitmap& bmp)
        { m_bitmapsHash[full_id] = bmp; }

    void Clear();

    static wxString ConstructHashID(const wxArtID& id,
                                    const wxArtClient& client,
                                    const wxSize& size);

private:
    wxArtProviderBitmapsHash m_bitmapsHash;
};

bool wxArtProviderCache::GetBitmap(const wxString& full_id, wxBitmap* bmp)
{
    wxArtProviderBitmapsHash::iterator entry = m_bitmapsHash.find(full_id);
    if ( entry == m_bitmapsHash.end() )
    {
        return FALSE;
    }
    else
    {
        *bmp = entry->second;
        return TRUE;
    }
}

void wxArtProviderCache::Clear()
{
    // Hack to make the default provider link
    // with the application
    g_ArtProviderModule = 0;
    m_bitmapsHash.clear();
}

/*static*/ wxString wxArtProviderCache::ConstructHashID(
                                const wxArtID& id, const wxArtClient& client,
                                const wxSize& size)
{
    wxString str;
    str.Printf(wxT("%s-%s-%i-%i"), id.c_str(), client.c_str(), size.x, size.y);
    return str;
}


// ----------------------------------------------------------------------------
// wxArtProvider class
// ----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxArtProvider, wxObject)

wxArtProvidersList *wxArtProvider::sm_providers = NULL;
wxArtProviderCache *wxArtProvider::sm_cache = NULL;

/*static*/ void wxArtProvider::PushProvider(wxArtProvider *provider)
{
    if ( !sm_providers )
    {
        sm_providers = new wxArtProvidersList;
        sm_providers->DeleteContents(TRUE);
        sm_cache = new wxArtProviderCache;
    }

    sm_providers->Insert(provider);
    sm_cache->Clear();
}

/*static*/ bool wxArtProvider::PopProvider()
{
    wxCHECK_MSG( sm_providers, FALSE, _T("no wxArtProvider exists") );
    wxCHECK_MSG( sm_providers->GetCount() > 0, FALSE, _T("wxArtProviders stack is empty") );

    sm_providers->DeleteNode(sm_providers->GetFirst());
    sm_cache->Clear();
    return TRUE;
}

/*static*/ bool wxArtProvider::RemoveProvider(wxArtProvider *provider)
{
    wxCHECK_MSG( sm_providers, FALSE, _T("no wxArtProvider exists") );

    if ( sm_providers->DeleteObject(provider) )
    {
        sm_cache->Clear();
        return TRUE;
    }

    return FALSE;
}

/*static*/ void wxArtProvider::CleanUpProviders()
{
    wxDELETE(sm_providers);
    wxDELETE(sm_cache);
}

/*static*/ wxBitmap wxArtProvider::GetBitmap(const wxArtID& id,
                                             const wxArtClient& client,
                                             const wxSize& size)
{
    // safety-check against writing client,id,size instead of id,client,size:
    wxASSERT_MSG( client.Last() == _T('C'), _T("invalid 'client' parameter") );

    wxCHECK_MSG( sm_providers, wxNullBitmap, _T("no wxArtProvider exists") );

    wxString hashId = wxArtProviderCache::ConstructHashID(id, client, size);

    wxBitmap bmp;
    if ( !sm_cache->GetBitmap(hashId, &bmp) )
    {
        for (wxArtProvidersList::Node *node = sm_providers->GetFirst();
             node; node = node->GetNext())
        {
            bmp = node->GetData()->CreateBitmap(id, client, size);
            if ( bmp.Ok() )
            {
                if ( size != wxDefaultSize &&
                     (bmp.GetWidth() != size.x || bmp.GetHeight() != size.y) )
                {
                    wxImage img = bmp.ConvertToImage();
                    img.Rescale(size.x, size.y);
                    bmp = wxBitmap(img);
                }
                break;
            }
        }

        sm_cache->PutBitmap(hashId, bmp);
    }

    return bmp;
}

/*static*/ wxIcon wxArtProvider::GetIcon(const wxArtID& id,
                                         const wxArtClient& client,
                                         const wxSize& size)
{
    wxCHECK_MSG( sm_providers, wxNullIcon, _T("no wxArtProvider exists") );

    wxBitmap bmp = GetBitmap(id, client, size);
    if ( !bmp.Ok() )
        return wxNullIcon;

    wxIcon icon;
    icon.CopyFromBitmap(bmp);
    return icon;
}


class wxArtProviderModule: public wxModule
{
public:
    bool OnInit()
    {
        wxArtProvider::InitStdProvider();
        return TRUE;
    }
    void OnExit()
    {
        wxArtProvider::CleanUpProviders();
    }

    DECLARE_DYNAMIC_CLASS(wxArtProviderModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxArtProviderModule, wxModule)
