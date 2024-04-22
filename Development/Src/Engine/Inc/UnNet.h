/*=============================================================================
	UnNet.h: Unreal networking.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Forward declarations.
-----------------------------------------------------------------------------*/

#ifndef _UNNET_H_
#define _UNNET_H_

class	UChannel;
class	UControlChannel;
class	UActorChannel;
class	UFileChannel;
class	FInBunch;
class	FOutBunch;
class	UChannelIterator;

class	UNetDriver;
class	UNetConnection;
class	UChildConnection;
class	UPendingLevel;
class	UNetPendingLevel;

/*-----------------------------------------------------------------------------
	Types.
-----------------------------------------------------------------------------*/

// Up to this many reliable channel bunches may be buffered.
enum {RELIABLE_BUFFER         = 128   }; // Power of 2 >= 1.
enum {MAX_PACKETID            = 16384 }; // Power of 2 >= 1, covering guaranteed loss/misorder time.
enum {MAX_CHSEQUENCE          = 1024  }; // Power of 2 >RELIABLE_BUFFER, covering loss/misorder time.
enum {MAX_BUNCH_HEADER_BITS   = 64    };

enum {MAX_PACKET_HEADER_BITS  = 16    };

#if WITH_STEAMWORKS_SOCKETS
enum {SESSION_UID_BITS        = 24    }; // If bUseSessionUID is set in UNetConnection, this is added to MAX_PACKET_HEADER_BITS
#endif

enum {MAX_PACKET_TRAILER_BITS = 1     };

// Return the value of Max/2 <= Value-Reference+some_integer*Max < Max/2.
inline INT BestSignedDifference( INT Value, INT Reference, INT Max )
{
	return ((Value-Reference+Max/2) & (Max-1)) - Max/2;
}
inline INT MakeRelative( INT Value, INT Reference, INT Max )
{
	return Reference + BestSignedDifference(Value,Reference,Max);
}

// Types of channels.
enum EChannelType
{
	CHTYPE_None			= 0,  // Invalid type.
	CHTYPE_Control		= 1,  // Connection control.
	CHTYPE_Actor  		= 2,  // Actor-update channel.
	CHTYPE_File         = 3,  // Binary file transfer.
	CHTYPE_Voice		= 4,  // VoIP data channel
	CHTYPE_MAX          = 8,  // Maximum.
};

// The channel index to use for voice
#define VOICE_CHANNEL_INDEX 1

/*-----------------------------------------------------------------------------
	Flags
-----------------------------------------------------------------------------*/
//
// Whether to support net lag and packet loss testing.
//
#define DO_ENABLE_NET_TEST (!FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE)

#if DO_ENABLE_NET_TEST
/** Holds the packet simulation settings in one place */
struct FPacketSimulationSettings
{
	INT	PktLoss;
	INT	PktOrder;
	INT	PktDup;
	INT	PktLag;
	INT	PktLagVariance;

	/** Ctor. Zeroes the settings */
	FPacketSimulationSettings() : 
		PktLoss(0),
		PktOrder(0),
		PktDup(0),
		PktLag(0),
		PktLagVariance(0) 
	{
	}

	/** reads in settings from the .ini file 
	 * @note: overwrites all previous settings
	 */
	void LoadConfig();

	/**
	 * Reads the settings from a string: command line or an exec
	 *
	 * @param Stream the string to read the settings from
	 */
	UBOOL ParseSettings(const TCHAR* Stream);
};
#endif

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#include "UnNetDrv.h"		// Network driver class.
#include "UnBunch.h"		// Bunch class.
#include "UnDownload.h"		// Autodownloading classes.
#include "UnConn.h"			// Connection class.
#include "UnChan.h"			// Channel class.
#include "UnPenLev.h"		// Pending levels.

/*-----------------------------------------------------------------------------
	Replication.
-----------------------------------------------------------------------------*/
static inline UBOOL NEQ(BYTE A,BYTE B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}
static inline UBOOL NEQ(INT A,INT B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}
static inline UBOOL NEQ(BITFIELD A,BITFIELD B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}
static inline UBOOL NEQ(FLOAT& A,FLOAT& B,UPackageMap* Map,UActorChannel* Channel) {return *(INT*)&A!=*(INT*)&B;}
static inline UBOOL NEQ(FVector& A,FVector& B,UPackageMap* Map,UActorChannel* Channel) {return ((INT*)&A)[0]!=((INT*)&B)[0] || ((INT*)&A)[1]!=((INT*)&B)[1] || ((INT*)&A)[2]!=((INT*)&B)[2];}

// only compare high order bytes of FRotator components, since that's all that's replicated
static inline UBOOL NEQ(FRotator& A,FRotator& B,UPackageMap* Map,UActorChannel* Channel) {return A.Pitch>>8 != B.Pitch>>8 || A.Yaw>>8 != B.Yaw>>8 || A.Roll>>8 != B.Roll>>8;}

static inline UBOOL NEQ(UObject* A,UObject* B,UPackageMap* Map,UActorChannel* Channel) {if( Map->CanSerializeObject(A) )return A!=B; Channel->bActorMustStayDirty = true; 
//debugf(TEXT("%s Must stay dirty because of %s"),*Channel->Actor->GetName(),*A->GetName());
return (B!=NULL);}
static inline UBOOL NEQ(FName& A,FName B,UPackageMap* Map,UActorChannel* Channel) {return *(INT*)&A!=*(INT*)&B;}
static inline UBOOL NEQ(FColor& A,FColor& B,UPackageMap* Map,UActorChannel* Channel) {return *(INT*)&A!=*(INT*)&B;}
static inline UBOOL NEQ(FLinearColor& A,FLinearColor& B,UPackageMap* Map,UActorChannel* Channel) {return A.R!=B.R || A.G!=B.G || A.B!=B.B || A.A!=B.A;}
static inline UBOOL NEQ(FPlane& A,FPlane& B,UPackageMap* Map,UActorChannel* Channel) {return
((INT*)&A)[0]!=((INT*)&B)[0] || ((INT*)&A)[1]!=((INT*)&B)[1] ||
((INT*)&A)[2]!=((INT*)&B)[2] || ((INT*)&A)[3]!=((INT*)&B)[3];}
static inline UBOOL NEQ(const FString& A,const FString& B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}
static inline UBOOL NEQ(const struct FVehicleState& A,const struct FVehicleState& B,UPackageMap* Map,UActorChannel* Channel) {return 1;}
static inline UBOOL NEQ(const struct FRigidBodyState& A,const struct FRigidBodyState& B,UPackageMap* Map,UActorChannel* Channel)
{
	return ((A.Position - B.Position).SizeSquared() > 0.4f || (A.Quaternion - B.Quaternion).SizeSquared() > 0.001f || A.bNewData != B.bNewData);
}

static inline UBOOL NEQ(FUniqueNetId& A,FUniqueNetId& B,UPackageMap* Map,UActorChannel* Channel)
{
	return A.Uid - B.Uid;
}
static inline UBOOL NEQ(FVector2D& A,FVector2D& B,UPackageMap* Map,UActorChannel* Channel) {return A.X!=B.X || A.Y!=B.Y;}

static inline UBOOL NEQ(FGuid& A,FGuid& B,UPackageMap* Map,UActorChannel* Channel) {return A != B;}

/** wrapper to find replicated properties that also makes sure they're valid */
static UProperty* GetReplicatedProperty(UClass* CallingClass, UClass* PropClass, const TCHAR* PropName)
{
	if (!CallingClass->IsChildOf(PropClass))
	{
		appErrorf(TEXT("Attempt to replicate property '%s.%s' in C++ but class '%s' is not a child of '%s'"), *PropClass->GetName(), PropName, *CallingClass->GetName(), *PropClass->GetName());
	}
	UProperty* TheProperty = FindObjectChecked<UProperty>(PropClass, PropName);
	if (!(TheProperty->PropertyFlags & CPF_Net))
	{
		appErrorf(TEXT("Attempt to replicate property '%s' in C++ that is not in the script replication block for the class"), *TheProperty->GetFullName());
	}
	return TheProperty;
}

#define DOREP(c,v) \
{ \
	static UProperty* sp##v = GetReplicatedProperty(StaticClass(), A##c::StaticClass(),TEXT(#v)); \
	/** always send config properties in the first bunch, as the default value may be different between client and server,
	 * thus invalidating the initial value in the Recent data
	 */ \
	if ((Channel->OpenPacketId == INDEX_NONE && (sp##v->PropertyFlags & CPF_Config)) || NEQ(v,((A##c*)Recent)->v,Map,Channel)) \
	{ \
		*Ptr++ = sp##v->RepIndex; \
	} \
}

#define DOREPARRAY(c,v) \
{ \
	static UProperty* sp##v = GetReplicatedProperty(StaticClass(), A##c::StaticClass(),TEXT(#v)); \
	/** always send config properties in the first bunch, as the default value may be different between client and server,
	 * thus invalidating the initial value in the Recent data
	 */ \
	if (Channel->OpenPacketId != INDEX_NONE || !(sp##v->PropertyFlags & CPF_Config)) \
	{ \
		for( INT i=0; i<ARRAY_COUNT(v); i++ ) \
			if( NEQ(v[i],((A##c*)Recent)->v[i],Map,Channel) ) \
				*Ptr++ = sp##v->RepIndex+i; \
	} \
	else \
	{ \
		for( INT i=0; i<ARRAY_COUNT(v); i++ ) \
			*Ptr++ = sp##v->RepIndex+i; \
	} \
}

#endif

