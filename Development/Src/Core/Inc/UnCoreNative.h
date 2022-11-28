/*=============================================================================
	UnCoreNative.h: Native function lookup table for static libraries.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Brandon Reinhart
=============================================================================*/

#ifndef UNCORENATIVE_H
#define UNCORENATIVE_H

#define NATIVE_INFO(t) \
	struct {TCHAR* Name; void(t::*Pointer)(FFrame&,RESULT_DECL);}

#define MAP_NATIVE(cls,name) \
	{TEXT("int")TEXT(#cls)TEXT(#name), &cls::name},

#define DECLARE_NATIVE_TYPE(pkg,type) \
	typedef void (type::*type##Native)( FFrame& TheStack, RESULT_DECL ); \
	struct type##NativeInfo \
	{ TCHAR* Name; type##Native Pointer; }; \
	extern void* Find##pkg##type##Native( TCHAR* NativeName );

#define IMPLEMENT_NATIVE_HANDLER(pkg,type) \
	void* Find##pkg##type##Native( TCHAR* NativeName ) \
	{ INT i=0; while (G##pkg##type##Natives[i].Name) \
		{ if (appStrcmp(NativeName, G##pkg##type##Natives[i].Name) == 0) \
			return &G##pkg##type##Natives[i].Pointer; \
		i++; } return NULL; }

typedef void* (*NativeLookup)(TCHAR*);
extern NativeLookup GNativeLookupFuncs[64];

inline void* FindNative( TCHAR* NativeName )
{
	for (INT i=0; i<ARRAY_COUNT(GNativeLookupFuncs); i++)
		if (GNativeLookupFuncs[i])
		{
			void* Result = GNativeLookupFuncs[i]( NativeName );
			if (Result)
				return Result;
		}
	return NULL;
}

DECLARE_NATIVE_TYPE(Core,UObject);
DECLARE_NATIVE_TYPE(Core,UCommandlet);

#endif

