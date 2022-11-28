#ifndef _SQLTYPES_H
#define _SQLTYPES_H
#define SQL_API __stdcall
#pragma pack(push,1)

/* Portable SQL types */
typedef void* PVOID;
typedef signed char SCHAR;
typedef unsigned char UCHAR;
typedef long int SDWORD;
typedef short int SWORD;
typedef unsigned long int UDWORD;
typedef unsigned short int UWORD;
typedef unsigned short USHORT;
typedef unsigned long ULONG;
typedef signed long SLONG;
typedef short SSHORT;
typedef double SDOUBLE;
typedef double LDOUBLE;
typedef float SFLOAT;

typedef PVOID PTR;
typedef PVOID HENV;
typedef PVOID HDBC;
typedef PVOID HSTMT;



typedef signed short RETCODE;
typedef UCHAR SQLCHAR;
typedef SCHAR SQLSCHAR;
typedef SDWORD SQLINTEGER;
typedef SWORD SQLSMALLINT;
typedef UDWORD SQLUINTEGER;
typedef UWORD SQLUSMALLINT;
typedef PVOID SQLPOINTER;
typedef HENV SQLHENV;
typedef HDBC SQLHDBC;
typedef HSTMT SQLHSTMT;
typedef SQLSMALLINT SQLRETURN;
typedef HWND SQLHWND;
typedef unsigned long int BOOKMARK;


#ifdef _WCHAR_T_DEFINED
	typedef wchar_t SQLWCHAR;
#else
	typedef unsigned short SQLWCHAR;
#endif


#ifdef UNICODE
	typedef SQLWCHAR SQLTCHAR;
#else
	typedef SQLCHAR SQLTCHAR;
#endif


typedef struct tagDATE_STRUCT {
	SQLSMALLINT year;
	SQLUSMALLINT month;
	SQLUSMALLINT day;
} DATE_STRUCT;

typedef struct tagTIME_STRUCT {
	SQLUSMALLINT hour;
	SQLUSMALLINT minute;
	SQLUSMALLINT second;
} TIME_STRUCT;

typedef struct tagTIMESTAMP_STRUCT {
	SQLSMALLINT year;
	SQLUSMALLINT month;
	SQLUSMALLINT day;
	SQLUSMALLINT hour;
	SQLUSMALLINT minute;
	SQLUSMALLINT second;
	SQLUINTEGER fraction;
} TIMESTAMP_STRUCT;

typedef void* SQLHANDLE; 
typedef SQLHANDLE SQLHDESC; 
#pragma pack(pop)
#endif
