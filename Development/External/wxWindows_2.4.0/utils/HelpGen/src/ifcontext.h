/////////////////////////////////////////////////////////////////////////////
// Name:        No names yet.
// Purpose:     Contrib. demo
// Author:      Aleksandras Gluchovas
// Modified by:
// Created:     27/12/98
// RCS-ID:      $Id: ifcontext.h,v 1.1 1999/09/13 14:29:40 JS Exp $
// Copyright:   (c) Aleskandars Gluchovas
// Licence:   	wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __IFCONTEXT_G__
#define __IFCONTEXT_G__

#include "srcparser.h"
#include "cjparser.h"

class spBookmark
{
public:
	size_t mFrom;
	size_t mLen;
	size_t mFileNo;

	inline spBookmark() {}

	inline spBookmark( int from, int len, int fileNo )
		: mFrom( from ), mLen( len ), mFileNo( fileNo )
	{}
};

#if defined( wxUSE_TEMPLATE_STL )

	typedef vector<spBookmark) BookmarkListT

#else

	typedef WXSTL_VECTOR_SHALLOW_COPY(spBookmark) BookmarkListT;

#endif


class spInterFileContext : public spContext
{
protected:

	BookmarkListT  mDeletionMarks;

	BookmarkListT  mFiltered;

	size_t         mCurFileNo;

	CJSourceParser mParser;

protected:

	size_t GetFileNoOfContext( spContext& ctx );
	size_t GetFileNo( const string& fname );

	void InsertBookmarkSorted( BookmarkListT& lst, spBookmark& mark );

	void DoAppendSourceFragment( string& source, 
								 string& result, 
						         size_t  pos, size_t len );

	void GenerateContextBody( spContext& ctx, 
							  string&    source,
							  string&    result, 
							  size_t& lastSavedPos,
							  size_t& lastKnownPos );

public:

	StrListT       mFiles;
	StrListT       mContents;

public:

	spInterFileContext();
	~spInterFileContext();

	void AddFile( const string& fname, const string& content );

	void RemoveContext( spContext& ctx );

	void GenrateContents();

	void ParseContents( SourceParserPlugin* pPlugin = NULL );

	void WriteToFiles();

	// overriden method of the base class (finds out the source fragment)

	virtual string GetBody( spContext* pCtx = NULL );

	virtual string GetHeader( spContext* pCtx = NULL );
};


#endif