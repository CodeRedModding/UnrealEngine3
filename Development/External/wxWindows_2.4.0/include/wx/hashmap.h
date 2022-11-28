/////////////////////////////////////////////////////////////////////////////
// Name:        hashmap.h
// Purpose:     wxHashMap class
// Author:      Mattia Barbon
// Modified by:
// Created:     29/01/2002
// RCS-ID:      $Id: hashmap.h,v 1.13.2.3 2002/12/07 01:33:37 VZ Exp $
// Copyright:   (c) Mattia Barbon
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_HASHMAP_H_
#define _WX_HASHMAP_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "hashmap.h"
#endif

#include "wx/string.h"

// private
struct WXDLLEXPORT _wxHashTable_NodeBase
{
    _wxHashTable_NodeBase() : m_nxt(0) {}

    _wxHashTable_NodeBase* m_nxt;
};

// private
class WXDLLEXPORT _wxHashTableBase2
{
public:
    typedef void (*NodeDtor)(_wxHashTable_NodeBase*);
    typedef unsigned long (*BucketFromNode)(_wxHashTableBase2*,_wxHashTable_NodeBase*);
    typedef _wxHashTable_NodeBase* (*ProcessNode)(_wxHashTable_NodeBase*);
protected:
    static _wxHashTable_NodeBase* DummyProcessNode(_wxHashTable_NodeBase* node);
    static void DeleteNodes( size_t buckets, _wxHashTable_NodeBase** table,
                             NodeDtor dtor );
    static _wxHashTable_NodeBase* GetFirstNode( size_t buckets,
                                                _wxHashTable_NodeBase** table )
    {
        for( size_t i = 0; i < buckets; ++i )
            if( table[i] )
                return table[i];
        return 0;
    }

    // as static const unsigned prime_count = 31 but works with all compilers
    enum { prime_count = 31 };
    static const unsigned long ms_primes[prime_count];

    // returns the first prime in ms_primes greater than n
    static unsigned long GetNextPrime( unsigned long n );

    // returns the first prime in ms_primes smaller than n
    // ( or ms_primes[0] if n is very small )
    static unsigned long GetPreviousPrime( unsigned long n );

    static void CopyHashTable( _wxHashTable_NodeBase** srcTable,
                               size_t srcBuckets, _wxHashTableBase2* dst,
                               _wxHashTable_NodeBase** dstTable,
                               BucketFromNode func, ProcessNode proc );

    static void** AllocTable( size_t sz )
    {
        return (void **)calloc(sz, sizeof(void*));
    }
};

#define _WX_DECLARE_HASHTABLE( VALUE_T, KEY_T, HASH_T, KEY_EX_T, KEY_EQ_T, CLASSNAME, CLASSEXP, SHOULD_GROW, SHOULD_SHRINK ) \
CLASSEXP CLASSNAME : protected _wxHashTableBase2 \
{ \
public: \
    typedef KEY_T key_type; \
    typedef VALUE_T value_type; \
    typedef HASH_T hasher; \
    typedef KEY_EQ_T key_equal; \
 \
    typedef size_t size_type; \
    typedef ptrdiff_t difference_type; \
    typedef value_type* pointer; \
    typedef const value_type* const_pointer; \
    typedef value_type& reference; \
    typedef const value_type& const_reference; \
    /* should these be protected? */ \
    typedef const KEY_T const_key_type; \
    typedef const VALUE_T const_mapped_type; \
public: \
    struct Node; \
    typedef KEY_EX_T key_extractor; \
    typedef CLASSNAME Self; \
protected: \
    Node** m_table; \
    size_t m_tableBuckets; \
    size_t m_items; \
    hasher m_hasher; \
    key_equal m_equals; \
    key_extractor m_getKey; \
public: \
    struct Node:public _wxHashTable_NodeBase \
    { \
    public: \
        Node( const value_type& value ) \
            : m_value( value ) {} \
        Node* m_next() { return (Node*)this->m_nxt; } \
 \
        value_type m_value; \
    }; \
 \
    struct Iterator; \
    friend struct Iterator; \
protected: \
    static void DeleteNode( _wxHashTable_NodeBase* node ) \
    { \
        delete (Node*)node; \
    } \
public: \
    /*                  */ \
    /* forward iterator */ \
    /*                  */ \
    struct Iterator \
    { \
        Node* m_node; \
        Self* m_ht; \
 \
        Iterator() : m_node(0), m_ht(0) {} \
        Iterator( Node* node, const Self* ht ) \
            : m_node(node), m_ht((Self*)ht) {} \
        bool operator ==( const Iterator& it ) const \
            { return m_node == it.m_node; } \
        bool operator !=( const Iterator& it ) const \
            { return m_node != it.m_node; } \
    protected: \
        Node* GetNextNode() \
        { \
            size_type bucket = GetBucketForNode(m_ht,m_node); \
            for( size_type i = bucket + 1; i < m_ht->m_tableBuckets; ++i ) \
            { \
                if( m_ht->m_table[i] ) \
                    return m_ht->m_table[i]; \
            } \
            return 0; \
        } \
 \
        void PlusPlus() \
        { \
            Node* next = m_node->m_next(); \
            m_node = next ? next : GetNextNode(); \
        } \
    }; \
 \
public: \
    struct iterator:public Iterator \
    { \
        iterator() : Iterator() {} \
        iterator( Node* node, Self* ht ) : Iterator( node, ht ) {} \
        iterator& operator++() { PlusPlus(); return *this; } \
        iterator operator++(int) { iterator it=*this;PlusPlus();return it; } \
        reference operator *() const { return m_node->m_value; } \
        pointer operator ->() const { return &(m_node->m_value); } \
    }; \
 \
    struct const_iterator:public Iterator \
    { \
        const_iterator() : Iterator() {} \
        const_iterator( Node* node, const Self* ht ) \
            : Iterator( node, (Self*)ht ) {} \
        const_iterator& operator++() { PlusPlus();return *this; } \
        const_iterator operator++(int) { const_iterator it=*this;PlusPlus();return it; } \
        const_reference operator *() const { return m_node->m_value; } \
        const_pointer operator ->() const { return &(m_node->m_value); } \
    }; \
 \
    CLASSNAME( size_type sz = 10, const hasher& hfun = hasher(), \
               const key_equal& k_eq = key_equal(), \
               const key_extractor& k_ex = key_extractor() ) \
        : m_tableBuckets( GetNextPrime( sz ) ), \
          m_items( 0 ), \
          m_hasher( hfun ), \
          m_equals( k_eq ), \
          m_getKey( k_ex ) \
    { \
        m_table = (Node**)AllocTable( m_tableBuckets ); \
    } \
 \
    CLASSNAME( const Self& ht ) \
        : m_table( 0 ), \
          m_tableBuckets( 0 ), \
          m_items( ht.m_items ), \
          m_hasher( ht.m_hasher ), \
          m_equals( ht.m_equals ), \
          m_getKey( ht.m_getKey ) \
    { \
        HashCopy( ht ); \
    } \
 \
    const Self& operator=( const Self& ht ) \
    { \
         clear(); \
         m_hasher = ht.m_hasher; \
         m_equals = ht.m_equals; \
         m_getKey = ht.m_getKey; \
         m_items = ht.m_items; \
         HashCopy( ht ); \
         return *this; \
    } \
 \
    ~CLASSNAME() \
    { \
        clear(); \
 \
        free(m_table); \
    } \
 \
    hasher hash_funct() { return m_hasher; } \
    key_equal key_eq() { return m_equals; } \
 \
    /* removes all elements from the hash table, but does not */ \
    /* shrink it ( perhaps it should ) */ \
    void clear() \
    { \
        DeleteNodes( m_tableBuckets, (_wxHashTable_NodeBase**)m_table, \
                     DeleteNode ); \
        m_items = 0; \
    } \
 \
    size_type size() const { return m_items; } \
    size_type max_size() const { return size_type(-1); } \
    bool empty() const { return size() == 0; } \
 \
    const_iterator end() const { return const_iterator( 0, this ); } \
    iterator end() { return iterator( 0, this ); } \
    const_iterator begin() const \
        { return const_iterator( (Node*)GetFirstNode( m_tableBuckets, (_wxHashTable_NodeBase**)m_table ), this ); }; \
    iterator begin() \
        { return iterator( (Node*)GetFirstNode( m_tableBuckets, (_wxHashTable_NodeBase**)m_table ), this ); }; \
 \
    size_type erase( const const_key_type& key ) \
    { \
        Node** node = GetNodePtr( key ); \
 \
        if( !node ) \
            return 0; \
 \
        --m_items; \
        Node* temp = (*node)->m_next(); \
        delete *node; \
        (*node) = temp; \
        if( SHOULD_SHRINK( m_tableBuckets, m_items ) ) \
            ResizeTable( GetPreviousPrime( m_tableBuckets ) - 1 ); \
        return 1; \
    } \
 \
protected: \
    static size_type GetBucketForNode( Self* ht, Node* node ) \
    { \
        return ht->m_hasher( ht->m_getKey( node->m_value ) ) \
            % ht->m_tableBuckets; \
    } \
    static Node* CopyNode( Node* node ) { return new Node( *node ); } \
 \
    Node* GetOrCreateNode( const value_type& value ) \
    { \
        const const_key_type& key = m_getKey( value ); \
        size_t bucket = m_hasher( key ) % m_tableBuckets; \
        Node* node = m_table[bucket]; \
 \
        while( node ) \
        { \
            if( m_equals( m_getKey( node->m_value ), key ) ) \
                return node; \
            node = node->m_next(); \
        } \
 \
        node = new Node( value ); \
        node->m_nxt = m_table[bucket]; \
        m_table[bucket] = node; \
 \
        /* must be after the node is inserted */ \
        ++m_items; \
        if( SHOULD_GROW( m_tableBuckets, m_items ) ) \
            ResizeTable( m_tableBuckets ); \
 \
        return node; \
    } \
 \
    /* returns NULL if not found */ \
    Node** GetNodePtr( const const_key_type& key ) const \
    { \
        unsigned long hash = m_hasher( key ); \
        Node** node = &m_table[hash % m_tableBuckets]; \
 \
        while( *node ) \
        { \
            if( m_equals( m_getKey( (*node)->m_value ), key ) ) \
                return node; \
            node = (Node**)&(*node)->m_nxt; \
        } \
 \
        return 0; \
    } \
 \
    /* returns NULL if not found */ \
    /* expressing it in terms of GetNodePtr is 5-8% slower :-( */ \
    Node* GetNode( const const_key_type& key ) const \
    { \
        unsigned long hash = m_hasher( key ); \
        Node* node = m_table[hash % m_tableBuckets]; \
 \
        while( node ) \
        { \
            if( m_equals( m_getKey( node->m_value ), key ) ) \
                return node; \
            node = node->m_next(); \
        } \
 \
        return 0; \
    } \
 \
    void ResizeTable( size_t newSize ) \
    { \
        newSize = GetNextPrime( newSize ); \
        Node** srcTable = m_table; \
        size_t srcBuckets = m_tableBuckets; \
        m_table = (Node**)AllocTable( newSize ); \
        m_tableBuckets = newSize; \
 \
        CopyHashTable( (_wxHashTable_NodeBase**)srcTable, srcBuckets, \
                       this, (_wxHashTable_NodeBase**)m_table, \
                       (BucketFromNode)&GetBucketForNode,\
                       (ProcessNode)&DummyProcessNode ); \
        free(srcTable); \
    } \
 \
    /* this must be called _after_ m_table has been cleaned */ \
    void HashCopy( const Self& ht ) \
    { \
        ResizeTable( ht.size() ); \
        CopyHashTable( (_wxHashTable_NodeBase**)ht.m_table, ht.m_tableBuckets,\
                       (_wxHashTableBase2*)this, \
                       (_wxHashTable_NodeBase**)m_table, \
                       (BucketFromNode)&GetBucketForNode, \
                       (ProcessNode)&CopyNode ); \
    } \
};

// defines an STL-like pair class CLASSNAME storing two fields: first of type
// KEY_T and second of type VALUE_T
#define _WX_DECLARE_PAIR( KEY_T, VALUE_T, CLASSNAME, CLASSEXP ) \
CLASSEXP CLASSNAME \
{ \
public: \
    typedef KEY_T t1; \
    typedef VALUE_T t2; \
    typedef const KEY_T const_t1; \
    typedef const VALUE_T const_t2; \
 \
    CLASSNAME( const const_t1& f, const const_t2& s ):first(t1(f)),second(t2(s)) {} \
    CLASSNAME( const const_t1& f ):first(t1(f)),second(t2()) {} \
 \
    t1 first; \
    t2 second; \
};

// defines the class CLASSNAME returning the key part (of type KEY_T) from a
// pair of type PAIR_T
#define _WX_DECLARE_HASH_MAP_KEY_EX( KEY_T, PAIR_T, CLASSNAME, CLASSEXP ) \
CLASSEXP CLASSNAME \
{ \
    typedef KEY_T key_type; \
    typedef PAIR_T pair_type; \
    typedef const key_type const_key_type; \
    typedef const pair_type const_pair_type; \
    typedef const_key_type& const_key_reference; \
    typedef const_pair_type& const_pair_reference; \
public: \
    CLASSNAME() { } \
    const_key_reference operator()( const_pair_reference pair ) const { return pair.first; }\
    \
    /* the dummy assignment operator is needed to suppress compiler */ \
    /* warnings from hash table class' operator=(): gcc complains about */ \
    /* "statement with no effect" without it */ \
    CLASSNAME& operator=(const CLASSNAME&) { return *this; } \
};

// grow/shrink predicates
inline bool never_grow( size_t, size_t ) { return FALSE; }
inline bool never_shrink( size_t, size_t ) { return FALSE; }
inline bool grow_lf70( size_t buckets, size_t items )
{
    return float(items)/float(buckets) >= 0.85;
}

// ----------------------------------------------------------------------------
// hashing and comparison functors
// ----------------------------------------------------------------------------

// NB: implementation detail: all of these classes must have dummy assignment
//     operators to suppress warnings about "statement with no effect" from gcc
//     in the hash table class assignment operator (where they're assigned)

// integer types
class WXDLLEXPORT wxIntegerHash
{
public:
    wxIntegerHash() { }
    unsigned long operator()( long x ) const { return (unsigned long)x; }
    unsigned long operator()( unsigned long x ) const { return x; }
    unsigned long operator()( int x ) const { return (unsigned long)x; }
    unsigned long operator()( unsigned int x ) const { return x; }
    unsigned long operator()( short x ) const { return (unsigned long)x; }
    unsigned long operator()( unsigned short x ) const { return x; }

    wxIntegerHash& operator=(const wxIntegerHash&) { return *this; }
};

class WXDLLEXPORT wxIntegerEqual
{
public:
    wxIntegerEqual() { }
    bool operator()( long a, long b ) const { return a == b; }
    bool operator()( unsigned long a, unsigned long b ) const { return a == b; }
    bool operator()( int a, int b ) const { return a == b; }
    bool operator()( unsigned int a, unsigned int b ) const { return a == b; }
    bool operator()( short a, short b ) const { return a == b; }
    bool operator()( unsigned short a, unsigned short b ) const { return a == b; }

    wxIntegerEqual& operator=(const wxIntegerEqual&) { return *this; }
};

// pointers
class WXDLLEXPORT wxPointerHash
{
public:
    wxPointerHash() { }

    // TODO: this might not work well on architectures with 64 bit pointers but
    //       32 bit longs, we should use % ULONG_MAX there
    unsigned long operator()( const void* k ) const { return (unsigned long)k; }

    wxPointerHash& operator=(const wxPointerHash&) { return *this; }
};

class WXDLLEXPORT wxPointerEqual
{
public:
    wxPointerEqual() { }
    bool operator()( const void* a, const void* b ) const { return a == b; }

    wxPointerEqual& operator=(const wxPointerEqual&) { return *this; }
};

// wxString, char*, wxChar*
class WXDLLEXPORT wxStringHash
{
public:
    wxStringHash() {}
    unsigned long operator()( const wxString& x ) const
        { return wxCharStringHash( x.c_str() ); }
    unsigned long operator()( const wxChar* x ) const
        { return wxCharStringHash( x ); }
    static unsigned long wxCharStringHash( const wxChar* );
#if wxUSE_UNICODE
    unsigned long operator()( const char* x ) const
        { return charStringHash( x ); }
    static unsigned long charStringHash( const char* );
#endif // wxUSE_UNICODE

    wxStringHash& operator=(const wxStringHash&) { return *this; }
};

class WXDLLEXPORT wxStringEqual
{
public:
    wxStringEqual() {}
    bool operator()( const wxString& a, const wxString& b ) const
        { return a == b; }
    bool operator()( const wxChar* a, const wxChar* b ) const
        { return wxStrcmp( a, b ) == 0; }
#if wxUSE_UNICODE
    bool operator()( const char* a, const char* b ) const
        { return strcmp( a, b ) == 0; }
#endif // wxUSE_UNICODE

    wxStringEqual& operator=(const wxStringEqual&) { return *this; }
};

#define _WX_DECLARE_HASH_MAP( KEY_T, VALUE_T, HASH_T, KEY_EQ_T, CLASSNAME, CLASSEXP ) \
_WX_DECLARE_PAIR( KEY_T, VALUE_T, CLASSNAME##_wxImplementation_Pair, CLASSEXP ) \
_WX_DECLARE_HASH_MAP_KEY_EX( KEY_T, CLASSNAME##_wxImplementation_Pair, CLASSNAME##_wxImplementation_KeyEx, CLASSEXP ) \
_WX_DECLARE_HASHTABLE( CLASSNAME##_wxImplementation_Pair, KEY_T, HASH_T, CLASSNAME##_wxImplementation_KeyEx, KEY_EQ_T, CLASSNAME##_wxImplementation_HashTable, CLASSEXP, grow_lf70, never_shrink ) \
CLASSEXP CLASSNAME:public CLASSNAME##_wxImplementation_HashTable \
{ \
public: \
    typedef VALUE_T mapped_type; \
 \
    CLASSNAME( size_type hint = 100, hasher hf = hasher(), key_equal eq = key_equal() ) \
        : CLASSNAME##_wxImplementation_HashTable( hint, hf, eq, CLASSNAME##_wxImplementation_KeyEx() ) {} \
 \
    mapped_type& operator[]( const const_key_type& key ) \
    { \
        return GetOrCreateNode( CLASSNAME##_wxImplementation_Pair( key ) )->m_value.second; \
    } \
 \
    const_iterator find( const const_key_type& key ) const \
    { \
        return const_iterator( GetNode( key ), this ); \
    } \
 \
    iterator find( const const_key_type& key ) \
    { \
        return iterator( GetNode( key ), this ); \
    } \
 \
    void insert( const value_type& v ) { (*this)[v.first] = v.second; } \
 \
    size_type erase( const key_type& k ) \
        { return CLASSNAME##_wxImplementation_HashTable::erase( k ); } \
    void erase( const iterator& it ) { erase( it->first ); } \
    void erase( const const_iterator& it ) { erase( it->first ); } \
 \
    /* count() == 0 | 1 */ \
    size_type count( const const_key_type& key ) \
        { return GetNode( key ) ? 1 : 0; } \
}

// these macros are to be used in the user code
#define WX_DECLARE_HASH_MAP( KEY_T, VALUE_T, HASH_T, KEY_EQ_T, CLASSNAME) \
    _WX_DECLARE_HASH_MAP( KEY_T, VALUE_T, HASH_T, KEY_EQ_T, CLASSNAME, class )

#define WX_DECLARE_STRING_HASH_MAP( VALUE_T, CLASSNAME ) \
    _WX_DECLARE_HASH_MAP( wxString, VALUE_T, wxStringHash, wxStringEqual, \
                          CLASSNAME, class )

#define WX_DECLARE_VOIDPTR_HASH_MAP( VALUE_T, CLASSNAME ) \
    _WX_DECLARE_HASH_MAP( void*, VALUE_T, wxPointerHash, wxPointerEqual, \
                          CLASSNAME, class )

// and these do exactly the same thing but should be used inside the
// library
#define WX_DECLARE_EXPORTED_HASH_MAP( KEY_T, VALUE_T, HASH_T, KEY_EQ_T, CLASSNAME) \
    _WX_DECLARE_HASH_MAP( KEY_T, VALUE_T, HASH_T, KEY_EQ_T, CLASSNAME, class WXDLLEXPORT )

#define WX_DECLARE_EXPORTED_STRING_HASH_MAP( VALUE_T, CLASSNAME ) \
    _WX_DECLARE_HASH_MAP( wxString, VALUE_T, wxStringHash, wxStringEqual, \
                          CLASSNAME, class WXDLLEXPORT )

#define WX_DECLARE_EXPORTED_VOIDPTR_HASH_MAP( VALUE_T, CLASSNAME ) \
    _WX_DECLARE_HASH_MAP( void*, VALUE_T, wxPointerHash, wxPointerEqual, \
                          CLASSNAME, class WXDLLEXPORT )

#endif // _WX_HASHMAP_H_

