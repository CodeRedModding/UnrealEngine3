/////////////////////////////////////////////////////////////////////////////
// Name:        No names yet.
// Purpose:     Contrib. demo
// Author:      Aleksandras Gluchovas
// Modified by:
// Created:     22/09/98
// RCS-ID:      $Id: srcparser.cpp,v 1.6 2001/12/03 10:54:59 GT Exp $
// Copyright:   (c) Aleskandars Gluchovas
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
#  pragma implementation "srcparser.h"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#ifndef __DARWIN__
#  include <malloc.h>
#endif
#include <stdio.h>

#include "srcparser.h"

/***** Implementation for class spVisitor *****/

void spVisitor::VisitAll( spContext& atContext,
                          bool sortContent
                        )
{
    mSiblingSkipped = FALSE;
    mChildSkipped   = FALSE;
    mContextMask    = SP_CTX_ANY; // FIXME:: should be an arg.

    if ( sortContent && !atContext.IsSorted() )

        atContext.SortMembers();

    mpCurCxt = &atContext; // FIXME:: this is dirty, restoring it each time

    if ( atContext.GetContextType() & mContextMask )

        atContext.AcceptVisitor( *this );

    MMemberListT& members = atContext.GetMembers();

    for( size_t i = 0; i != members.size(); ++i )
    {
        if ( mSiblingSkipped )
        
            return;

        if ( !mChildSkipped )
        {
            size_t prevSz = members.size();

            // visit members of the context recursivelly
            VisitAll( *members[i], sortContent );

            if ( members.size() != prevSz )

                --i; // current member was removed!

            mChildSkipped = 0;
        }
    }
}

void spVisitor::RemoveCurrentContext()
{
    if ( mpCurCxt->GetParent() )

        mpCurCxt->GetParent()->RemoveChild( mpCurCxt );
}

void spVisitor::SkipSiblings()
{
    mSiblingSkipped = TRUE;
}

void spVisitor::SkipChildren()
{
    mChildSkipped = TRUE;
}

void spVisitor::SetFilter( int contextMask )
{
    mContextMask = contextMask;
}

/***** Implementation for class spComment *****/

bool spComment::IsMultiline()  const
{
    return mIsMultiline;
}

bool spComment::StartsParagraph() const
{
    return mStartsPar;
}

string& spComment::GetText()
{
    return mText;
}

string spComment::GetText() const
{
    return mText;
}

/***** Implementation for class spContext *****/

spContext::spContext()

    : mpParent        ( NULL ),
      mpFirstOccurence( NULL ),
      mAlreadySorted  ( FALSE ),

      mSrcLineNo    (-1),
      mSrcOffset    (-1),
      mContextLength(-1),
      mLastScrLineNo(-1),

      mHeaderLength (-1),
      mFooterLength (-1),

      mFirstCharPos (-1),
      mLastCharPos  (-1),

      mVisibility( SP_VIS_PRIVATE ),

      mIsVirtualContext         ( FALSE ),
      mVirtualContextHasChildren( FALSE ),

      mpUserData( NULL )
{}

void spContext::RemoveChildren()
{
    for( size_t i = 0; i != mMembers.size(); ++i )
    
        delete mMembers[i];

    mMembers.erase( mMembers.begin(), mMembers.end() );
}

spContext::~spContext()
{
    RemoveChildren();

    for( size_t i = 0; i != mComments.size(); ++i )
    
        delete mComments[i];
}

bool spContext::IsSorted()
{
    return mAlreadySorted;
}

void spContext::GetContextList( MMemberListT& lst, int contextMask )
{
    for( size_t i = 0; i != mMembers.size(); ++i )
    {
        spContext& member = *mMembers[i];

        if ( member.GetContextType() & contextMask )

            lst.push_back( &member );

        // collect required contexts recursively
        member.GetContextList( lst, contextMask );
    }
}

bool spContext::HasComments()
{
    return ( mComments.size() != 0 );
}

void spContext::RemoveChild( spContext* pChild )
{
    for( size_t i = 0; i != mMembers.size(); ++i )

        if ( mMembers[i] == pChild )
        {
            mMembers.erase( &mMembers[i] );

            delete pChild;
            return;
        }

    // the given child should exist on the parent's list 
    wxASSERT( 0 );
}

spContext* spContext::GetEnclosingContext( int mask )
{
    spContext* cur = this->GetParent();

    while ( cur && !(cur->GetContextType() & mask) ) 
        
        cur = cur->GetParent();

    return cur;
}

bool spContext::PositionIsKnown()
{
    return ( mSrcOffset != (-1) && mContextLength != (-1) );
}

bool spContext::IsVirtualContext()
{
    return mIsVirtualContext;
}

bool spContext::VitualContextHasChildren()
{
    return mVirtualContextHasChildren;
}

string spContext::GetVirtualContextBody()
{
    wxASSERT( mIsVirtualContext );

    return mVirtualContextBody;
}

string spContext::GetFooterOfVirtualContextBody()
{
    wxASSERT( mIsVirtualContext );

    return mVittualContextFooter;
}


void spContext::SetVirtualContextBody( const string& body, 
                                       bool          hasChildren,
                                       const string& footer )
{
    mVirtualContextHasChildren = hasChildren;

    mVirtualContextBody   = body;
    mVittualContextFooter = footer;

    // atuomaticllay becomes virtual context

    mIsVirtualContext   = TRUE;
}

string spContext::GetBody( spContext* pCtx )
{
    if ( ( pCtx == NULL || pCtx == this ) && mIsVirtualContext ) 
        
        return mVirtualContextBody;

    if ( GetParent() )

        return GetParent()->GetBody( ( pCtx != NULL ) ? pCtx : this );
    else
        return ""; // source-fragment cannot be found
}

string spContext::GetHeader( spContext* pCtx )
{
    if ( GetParent() )

        return GetParent()->GetHeader( ( pCtx != NULL ) ? pCtx : this );
    else
        return ""; // source-fragment cannot be found
}

bool spContext::IsFirstOccurence()
{
    return ( mpFirstOccurence != 0 );
}

spContext* spContext::GetFirstOccurence()
{
    // this object should not itself be 
    // the first occurence of the context
    wxASSERT( mpFirstOccurence != 0 );

    return mpFirstOccurence;
}

void spContext::AddMember( spContext* pMember )
{
    mMembers.push_back( pMember );

    pMember->mpParent = this;
}

void spContext::AddComment( spComment* pComment )
{
    mComments.push_back( pComment );
}

MMemberListT& spContext::GetMembers()
{
    return mMembers;
}

spContext* spContext::FindContext( const string& identifier,
                                   int   contextType,
                                   bool  searchSubMembers
                                 )
{
    for( size_t i = 0; i != mMembers.size(); ++i )
    {
        spContext& member = *mMembers[i];

        if ( member.GetName() == identifier && 
             ( contextType & member.GetContextType() )
           )

           return &member;

        if ( searchSubMembers )
        {
            spContext* result = 
                member.FindContext( identifier, contextType, 1 );

            if ( result ) return result;
        }
    }

    return 0;
}

void spContext::RemoveThisContext()
{
    if ( mpParent )
        mpParent->RemoveChild( this );
    else
        // context should have a parent
        wxFAIL_MSG("Context should have a parent");
}

spContext* spContext::GetOutterContext()
{
    return mpParent;
}

bool spContext::HasOutterContext()
{
    return ( mpParent != 0 );
}

bool spContext::IsInFile()
{
    return ( GetOutterContext()->GetContextType() == SP_CTX_FILE );
}

bool spContext::IsInNameSpace()
{
    return ( GetOutterContext()->GetContextType() == SP_CTX_NAMESPACE );
}

bool spContext::IsInClass()
{
    return ( GetOutterContext()->GetContextType() == SP_CTX_CLASS );
}

bool spContext::IsInOperation()
{
    return ( GetOutterContext()->GetContextType() == SP_CTX_OPERATION );
}

spClass& spContext::GetClass()
{
    wxASSERT( GetOutterContext()->GetType() == SP_CTX_CLASS );
    return *((spClass*)mpParent );
}

spFile& spContext::GetFile()
{
    wxASSERT( GetOutterContext()->GetType() == SP_CTX_FILE );
    return *((spFile*)mpParent );
}

spNameSpace& spContext::GetNameSpace()
{
    wxASSERT( GetOutterContext()->GetType() == SP_CTX_NAMESPACE );
    return *((spNameSpace*)mpParent );
}

spOperation& spContext::GetOperation()
{
    wxASSERT( GetOutterContext()->GetType() == SP_CTX_OPERATION );
    return *((spOperation*)mpParent );
}

/***** Implementation for class spClass *****/

void spClass::SortMembers()
{
    // TBD::
}

/***** Implementation for class spOperation *****/

spOperation::spOperation()

    : mHasDefinition( FALSE )
{
    mIsConstant =
    mIsVirtual =
    mHasDefinition = false;
}

string spOperation::GetFullName(MarkupTagsT tags)
{
    string txt = tags[TAG_BOLD].start + mRetType;
    txt += " ";
    txt += mName;
    txt += "( ";
    txt += tags[TAG_BOLD].end;
    
    for( size_t i = 0; i != mMembers.size(); ++i )
    {
        // DBG::
        wxASSERT( mMembers[i]->GetContextType() == SP_CTX_PARAMETER );

        spParameter& param = *((spParameter*)mMembers[i]);

        if ( i != 0 )
            txt += ", ";
        
        txt += tags[TAG_BOLD].start;
        
        txt += param.mType;

        txt += tags[TAG_BOLD].end;
        txt += tags[TAG_ITALIC].start;

        txt += " ";
        txt += param.mName;

        if ( param.mInitVal != "" )
        {
            txt += " = ";
            txt += tags[TAG_BOLD].start;

            txt += param.mInitVal;

            txt += tags[TAG_BOLD].end;
        }

        txt += tags[TAG_ITALIC].end;;
    }

    txt += tags[TAG_BOLD].start;
    txt += " )";
    txt += tags[TAG_BOLD].end;

    // TBD:: constantness of method

    return txt;
}

/***** Implemenentation for class spPreprocessorLine *****/

string spPreprocessorLine::CPP_GetIncludedFileNeme() const
{
    wxASSERT( GetStatementType() == SP_PREP_DEF_INCLUDE_FILE );

    size_t i = 0;

    while( i < mLine.length() && mLine[i] != '"' && mLine[i] != '<' ) 
        
        ++i;

    ++i;

    size_t start = i;

    while( i < mLine.length() && mLine[i] != '"' && mLine[i] != '>' ) 

        ++i;

    if ( start < mLine.length() )
    {
        string fname;
        fname.append( mLine, start, ( i - start ) );

        return fname;
    }
    else
        return ""; // syntax error probably
}



/***** Implemenentation for class SourceParserBase *****/

SourceParserBase::SourceParserBase()

    : mpFileBuf( NULL ),
      mFileBufSz( 0 ),

      mpPlugin( NULL )
{}

SourceParserBase::~SourceParserBase()
{
    if ( mpFileBuf ) free( mpFileBuf );

    if ( mpPlugin ) delete mpPlugin;
}

spFile* SourceParserBase::ParseFile( const char* fname )
{
    // FIXME:: the below should not be fixed!

    const size_t MAX_BUF_SIZE = 1024*256;

    if ( !mpFileBuf ) mpFileBuf = (char*)malloc( MAX_BUF_SIZE );

    mFileBufSz = MAX_BUF_SIZE;

    FILE* fp = fopen( fname, "rt" );

    if ( (int)fp == -1 || !fp ) return NULL;

    int sz = fread( mpFileBuf, 1, mFileBufSz, fp );

    return Parse( mpFileBuf, mpFileBuf + sz );
}

void SourceParserBase::SetPlugin( SourceParserPlugin* pPlugin )
{
    if ( mpPlugin ) delete mpPlugin;

    mpPlugin = pPlugin;
}

// ===========================================================================
// debug methods
// ===========================================================================

#ifdef __WXDEBUG__

void spContext::Dump(const wxString& indent) const
{
    DumpThis(indent);

    // increase it for the children
    wxString indentChild = indent + "    ";

    for ( MMemberListT::const_iterator i = mMembers.begin();
          i != mMembers.end();
          i++ ) {
        (*i)->Dump(indentChild);
    }
}

void spContext::DumpThis(const wxString& indent) const
{
    wxFAIL_MSG("abstract base class can't be found in parser tree!");
}

void spParameter::DumpThis(const wxString& indent) const
{
    wxLogDebug("%sparam named '%s' of type '%s'",
               indent.c_str(), mName.c_str(), mType.c_str());
}

void spAttribute::DumpThis(const wxString& indent) const
{
    wxLogDebug("%svariable named '%s' of type '%s'",
               indent.c_str(), mName.c_str(), mType.c_str());
}

void spOperation::DumpThis(const wxString& indent) const
{
    wxString protection;
    if ( !!mScope ) {
        switch ( mVisibility ) {
            case SP_VIS_PUBLIC:
                protection = "public";
                break;

            case SP_VIS_PROTECTED:
                protection = "protected";
                break;

            case SP_VIS_PRIVATE:
                protection = "private";
                break;

            default:
                wxFAIL_MSG("unknown protection type");
        }
    }
    else {
        protection = "global";
    }

    wxLogDebug("%s%s%s%s function named '%s::%s' of type '%s'",
               indent.c_str(),
               mIsConstant ? "const " : "",
               mIsVirtual ? "virtual " : "",
               protection.c_str(),
               mScope.c_str(), mName.c_str(), mRetType.c_str());
}

void spPreprocessorLine::DumpThis(const wxString& indent) const
{
    wxString kind;
    switch ( mDefType ) {
        case SP_PREP_DEF_DEFINE_SYMBOL:
            kind = "define";
            break;

        case SP_PREP_DEF_REDEFINE_SYMBOL:
            kind = "redefine";
            break;

        case SP_PREP_DEF_INCLUDE_FILE:
            kind.Printf("include (%s)", CPP_GetIncludedFileNeme().c_str());
            break;

        case SP_PREP_DEF_OTHER:
            kind = "other";
            break;

    }

    wxLogDebug("%spreprocessor statement: %s",
               indent.c_str(), kind.c_str());
}

void spClass::DumpThis(const wxString& indent) const
{
    wxString base;
    for ( StrListT::const_iterator i = mSuperClassNames.begin();
          i != mSuperClassNames.end();
          i++ ) {
        if ( !!base )
            base += ", ";
        base += *i;
    }

    if ( !base )
        base = "none";

    wxString kind;
    switch ( mClassSubType ) {
        case SP_CLTYPE_CLASS:
            kind = "class";
            break;

        case SP_CLTYPE_TEMPLATE_CLASS:
            kind = "template class";
            break;

        case SP_CLTYPE_STRUCTURE:
            kind = "struc";
            break;

        case SP_CLTYPE_UNION:
            kind = "union";
            break;

        case SP_CLTYPE_INTERFACE:
            kind = "interface";
            break;

        default:
            wxFAIL_MSG("unknown class subtype");
    }

    wxLogDebug("%s%s named '%s' (base classes: %s)",
               indent.c_str(), kind.c_str(),
               mName.c_str(), base.c_str());
}

void spEnumeration::DumpThis(const wxString& indent) const
{
    wxLogDebug("%senum named '%s'",
               indent.c_str(), mName.c_str());
}

void spTypeDef::DumpThis(const wxString& indent) const
{
    wxLogDebug("%stypedef %s = %s",
               indent.c_str(), mName.c_str(), mOriginalType.c_str());
}

void spFile::DumpThis(const wxString& indent) const
{
    wxLogDebug("%sfile '%s'",
               indent.c_str(), mFileName.c_str());
}

#endif // __WXDEBUG__
