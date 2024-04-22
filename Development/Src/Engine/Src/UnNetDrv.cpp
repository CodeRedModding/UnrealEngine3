/*=============================================================================
	UnNetDrv.cpp: Unreal network driver base class.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

// Default net driver stats
DECLARE_STATS_GROUP(TEXT("Net"),STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Ping"),STAT_Ping,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Channels"),STAT_Channels,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("In Rate (bytes)"),STAT_InRate,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Out Rate (bytes)"),STAT_OutRate,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("In Packets"),STAT_InPackets,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Out Packets"),STAT_OutPackets,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("In Bunches"),STAT_InBunches,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Out Bunches"),STAT_OutBunches,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Out Loss"),STAT_OutLoss,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("In Loss"),STAT_InLoss,STATGROUP_Net);
// Voice specific stats
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Voice bytes sent"),STAT_VoiceBytesSent,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Voice bytes recv"),STAT_VoiceBytesRecv,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Voice packets sent"),STAT_VoicePacketsSent,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Voice packets recv"),STAT_VoicePacketsRecv,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("In % Voice"),STAT_PercentInVoice,STATGROUP_Net);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Out % Voice"),STAT_PercentOutVoice,STATGROUP_Net);
// Peer net driver stats
DECLARE_STATS_GROUP(TEXT("Peer"),STATGROUP_PeerNet);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Ping"),STAT_PeerPing,STATGROUP_PeerNet);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Channels"),STAT_PeerChannels,STATGROUP_PeerNet);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("In Rate (bytes)"),STAT_PeerInRate,STATGROUP_PeerNet);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Out Rate (bytes)"),STAT_PeerOutRate,STATGROUP_PeerNet);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("In Packets"),STAT_PeerInPackets,STATGROUP_PeerNet);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Out Packets"),STAT_PeerOutPackets,STATGROUP_PeerNet);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("In Bunches"),STAT_PeerInBunches,STATGROUP_PeerNet);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Out Bunches"),STAT_PeerOutBunches,STATGROUP_PeerNet);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Out Loss"),STAT_PeerOutLoss,STATGROUP_PeerNet);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("In Loss"),STAT_PeerInLoss,STATGROUP_PeerNet);
// Peer net driver voice specific stats
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Voice bytes sent"),STAT_PeerVoiceBytesSent,STATGROUP_PeerNet);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Voice bytes recv"),STAT_PeerVoiceBytesRecv,STATGROUP_PeerNet);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Voice packets sent"),STAT_PeerVoicePacketsSent,STATGROUP_PeerNet);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Voice packets recv"),STAT_PeerVoicePacketsRecv,STATGROUP_PeerNet);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("In % Voice"),STAT_PeerPercentInVoice,STATGROUP_PeerNet);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Out % Voice"),STAT_PeerPercentOutVoice,STATGROUP_PeerNet);

#if DEDICATED_SERVER
//Instantiation of dedicated server perf counters
FDedicatedServerPerfCounters GDedicatedServerPerfCounters;
#endif //DEDICATED_SERVER

/*-----------------------------------------------------------------------------
	UPackageMapLevel implementation.
-----------------------------------------------------------------------------*/

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UPackageMapLevel::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UPackageMapLevel, Connection ) );
}

UBOOL UPackageMapLevel::CanSerializeObject( UObject* Obj )
{
	AActor* Actor = Cast<AActor>(Obj);
	if (Actor != NULL && !Actor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		if (!Actor->IsStatic() && !Actor->bNoDelete)
		{
			// dynamic actors can be serialized if they have a channel
			UActorChannel* Ch = Connection->ActorChannels.FindRef(Actor);
			//old: induces a bit of lag. return Ch && Ch->OpenAcked;
			return Ch!=NULL; //new: reduces lag, increases bandwidth slightly.
		}
		else
		{
			// static actors can always be serialized on the client and can be on the server if the client has initialized the level it's in
			return (!GWorld->IsServer() || Connection->ClientHasInitializedLevelFor(Actor));
		}
	}
	// for others, we can if we're the client, it's not in a level, or the client has initialized the level it's in
	else if (Obj == NULL || !GWorld->IsServer() || Connection->ClientHasInitializedLevelFor(Obj))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

UBOOL UPackageMapLevel::SerializeObject( FArchive& Ar, UClass* Class, UObject*& Object )
{
	DWORD Index=0;
	if( Ar.IsLoading() )
	{
		Object = NULL;
		BYTE B=0; Ar.SerializeBits( &B, 1 );
		if( B )
		{
			// Dynamic actor or None.
			Ar.SerializeInt( Index, UNetConnection::MAX_CHANNELS );

			// JohnB: Adjusted this to check Index <= 0 rather than == 0, in case SerializeInt is ever modified to handle minus numbers
			if( Index <= 0 )
			{
				Object = NULL;
			}
			else if
			(	!Ar.IsError()
			&&	Index<UNetConnection::MAX_CHANNELS
			&&	Connection->Channels[Index]
			&&	Connection->Channels[Index]->ChType==CHTYPE_Actor 
			&&	!Connection->Channels[Index]->Closing )
				Object = ((UActorChannel*)Connection->Channels[Index])->GetActor();
		}
		else
		{
			// Static object.
			Ar.SerializeInt( Index, MAX_OBJECT_INDEX );
			if( !Ar.IsError() )
				Object = IndexToObject( Index, 1 );

			// ignore it if it's an object inside a level we haven't finished initializing
			if (Object != NULL && GWorld != NULL)
			{
				// get the level for the object
				ULevel* Level = NULL;
				for (UObject* Obj = Object; Obj != NULL; Obj = Obj->GetOuter())
				{
					Level = Cast<ULevel>(Obj);
					if (Level != NULL)
					{
						break;
					}
				}
				if (Level != NULL && Level != GWorld->PersistentLevel)
				{
					AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
					UBOOL bLevelVisible = FALSE;
					for (INT i = 0; i < WorldInfo->StreamingLevels.Num(); i++)
					{
						if (WorldInfo->StreamingLevels(i)->LoadedLevel == Level)
						{
							bLevelVisible = WorldInfo->StreamingLevels(i)->bIsVisible;
							break;
						}
					}
					if (!bLevelVisible)
					{
						debugfSuppressed(NAME_DevNetTraffic, TEXT("Using None instead of replicated reference to %s because the level it's in has not been made visible"), *Object->GetFullName());
						Object = NULL;
					}
				}
			}
		}
		if( Object && !Object->IsA(Class) )
		{
			debugf(NAME_DevNet, TEXT("Forged object: got %s, expecting %s"),*Object->GetFullName(),*Class->GetFullName());
			Object = NULL;
		}
		return 1;
	}
	else
	{
		AActor* Actor = Cast<AActor>(Object);
		if (Actor != NULL && !Actor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			if (!Actor->IsStatic() && !Actor->bNoDelete)
			{
				// Map dynamic actor through channel index.
				BYTE B=1; Ar.SerializeBits( &B, 1 );
				UActorChannel* Ch = Connection->ActorChannels.FindRef(Actor);
				UBOOL Mapped = 0;
				if( Ch )
				{
					Index  = Ch->ChIndex;
					Mapped = Ch->OpenAcked;
				}
				Ar.SerializeInt( Index, UNetConnection::MAX_CHANNELS );
				return Mapped;
			}
			else if (GWorld->IsServer() && !Connection->ClientHasInitializedLevelFor(Object))
			{
				// send NULL for static actor because the client has not initialized its level yet
				BYTE B=1; Ar.SerializeBits( &B, 1 );
				Ar.SerializeInt( Index, UNetConnection::MAX_CHANNELS );
				return 0;
			}
		}
		else if (Object != NULL && GWorld->IsServer() && !Connection->ClientHasInitializedLevelFor(Object))
		{
			// send NULL for object in level because the client has not initialized that level yet
			BYTE B=1; Ar.SerializeBits( &B, 1 );
			Ar.SerializeInt( Index, UNetConnection::MAX_CHANNELS );
			return 0;
		}
		if (Object == NULL)
		{
			// NULL object
			BYTE B=1; Ar.SerializeBits( &B, 1 );
			Ar.SerializeInt( Index, UNetConnection::MAX_CHANNELS );
			return 1;
		}
		else
		{
			INT IntIndex = ObjectToIndex(Object);
			if (IntIndex == INDEX_NONE)
			{
				 // we cannot serialize this object
				BYTE B=1; Ar.SerializeBits( &B, 1 );
				Ar.SerializeInt( Index, UNetConnection::MAX_CHANNELS );
				return 1;
			}
			else
			{
				// Map regular object.
				// Since mappability doesn't change dynamically, there is no advantage to setting Result!=0.
				Index = DWORD(IntIndex);
				BYTE B=0; Ar.SerializeBits( &B, 1 );
				Ar.SerializeInt( Index, MAX_OBJECT_INDEX );
				return 1;
			}
		}
	}
}
IMPLEMENT_CLASS(UPackageMapLevel);

/*-----------------------------------------------------------------------------
	UPackageMapSeekFree implementation.
-----------------------------------------------------------------------------*/

UBOOL UPackageMapSeekFree::SerializeObject( FArchive& Ar, UClass* Class, UObject*& Object )
{
	if( Ar.IsLoading() )
	{
		INT ChannelIndex;
		Ar << ChannelIndex;
		if( Ar.IsError() )
		{
			ChannelIndex = 0;
		}
		
		// Static object.
		if( ChannelIndex == INDEX_NONE )
		{
			FString ObjectPathName;
			Ar << ObjectPathName;
			if( !Ar.IsError() )
			{
				Object = UObject::StaticFindObject( Class, NULL, *ObjectPathName, FALSE );
			}
		}
		// Null object (NOTE: No index below INDEX_NONE i.e. -1 is valid)
		else if( ChannelIndex == 0 || ChannelIndex < INDEX_NONE )
		{
			Object = NULL;
		}
		// Dynamic object.
		else
		{
			if( ChannelIndex<UNetConnection::MAX_CHANNELS
			&&	Connection->Channels[ChannelIndex]
			&&	Connection->Channels[ChannelIndex]->ChType==CHTYPE_Actor 
			&&	!Connection->Channels[ChannelIndex]->Closing )
			{
				Object = ((UActorChannel*)Connection->Channels[ChannelIndex])->GetActor();
			}
		}
		if( Object && !Object->IsA(Class) )
		{
			debugf(NAME_DevNet, TEXT("Forged object: got %s, expecting %s"),*Object->GetFullName(),*Class->GetFullName());
			Object = NULL;
		}
	}
	else if( Ar.IsSaving() )
	{
		AActor* Actor = Cast<AActor>(Object);
		if (Actor != NULL && !Actor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && !Actor->IsStatic() && !Actor->bNoDelete)
		{
			// Map dynamic actor through channel index.
			UActorChannel* ActorChannel = Connection->ActorChannels.FindRef(Actor);
			UBOOL bIsMapped		= 0;
			INT ChannelIndex	= 0;
			if( ActorChannel )
			{
				ChannelIndex	= ActorChannel->ChIndex;
				bIsMapped		= ActorChannel->OpenAcked;
			}

			Ar << ChannelIndex;
			return bIsMapped;
		}
		else if( !Object )
		{
			INT ChannelIndex = 0;
			Ar << ChannelIndex;
		}
		else
		{
			INT ChannelIndex = INDEX_NONE;
			Ar << ChannelIndex;
			FString ObjectPathName = Object->GetPathName();
			Ar << ObjectPathName;
		}
	}
	return TRUE;
}
UBOOL UPackageMapSeekFree::SerializeName( FArchive& Ar, FName& Name )
{
	if( Ar.IsLoading() )
	{
		FString InString;
		INT Number;
		Ar << InString << Number;
		Name = FName(*InString, Number);
	}
	else if( Ar.IsSaving() )
	{
		FString OutString = Name.GetNameString();
		INT Number = Name.GetNumber();
		Ar << OutString << Number;
	}
	return TRUE;
}

UBOOL UPackageMapSeekFree::SupportsPackage( UObject* InOuter )
{
	return TRUE;
}
UBOOL UPackageMapSeekFree::SupportsObject( UObject* Obj )
{
	return TRUE;
}
IMPLEMENT_CLASS(UPackageMapSeekFree);

/*-----------------------------------------------------------------------------
	UNetDriver implementation.
-----------------------------------------------------------------------------*/

UNetDriver::UNetDriver()
:	ClientConnections()
,	Time( 0.f )
,	bIsPeer(FALSE)
,	DownloadManagers( E_NoInit )
,	InBytes(0)
,	OutBytes(0)
,	InPackets(0)
,	OutPackets(0)
,	InBunches(0)
,	OutBunches(0)
,	InPacketsLost(0)
,	OutPacketsLost(0)
,	InOutOfOrderPackets(0)
,	OutOutOfOrderPackets(0)
,	StatUpdateTime(0.0)
,	StatPeriod(1.f)
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
#if DO_ENABLE_NET_TEST
		// read the settings from .ini and command line, with the command line taking precedence
		PacketSimulationSettings.LoadConfig();
		PacketSimulationSettings.ParseSettings(appCmdLine());
#endif
		RoleProperty       = FindObjectChecked<UProperty>( AActor::StaticClass(), TEXT("Role"      ) );
		RemoteRoleProperty = FindObjectChecked<UProperty>( AActor::StaticClass(), TEXT("RemoteRole") );
		MasterMap          = new UPackageMap;
		ProfileStats	   = ParseParam(appCmdLine(),TEXT("profilestats"));
	}
}
void UNetDriver::StaticConstructor()
{
	// Expose CPF_Config properties to be loaded from .ini.
	new(GetClass(),TEXT("ConnectionTimeout"),    RF_Public)UFloatProperty(CPP_PROPERTY(ConnectionTimeout    ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("InitialConnectTimeout"),RF_Public)UFloatProperty(CPP_PROPERTY(InitialConnectTimeout), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("KeepAliveTime"),        RF_Public)UFloatProperty(CPP_PROPERTY(KeepAliveTime        ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("RelevantTimeout"),      RF_Public)UFloatProperty(CPP_PROPERTY(RelevantTimeout      ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("SpawnPrioritySeconds"), RF_Public)UFloatProperty(CPP_PROPERTY(SpawnPrioritySeconds ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("ServerTravelPause"),    RF_Public)UFloatProperty(CPP_PROPERTY(ServerTravelPause    ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("MaxClientRate"),		 RF_Public)UIntProperty  (CPP_PROPERTY(MaxClientRate        ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("MaxInternetClientRate"),RF_Public)UIntProperty  (CPP_PROPERTY(MaxInternetClientRate), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("NetServerMaxTickRate"), RF_Public)UIntProperty  (CPP_PROPERTY(NetServerMaxTickRate ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("bClampListenServerTickRate"),RF_Public)UBoolProperty(CPP_PROPERTY(bClampListenServerTickRate), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("AllowDownloads"),       RF_Public)UBoolProperty (CPP_PROPERTY(AllowDownloads       ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("AllowPeerConnections"), RF_Public)UBoolProperty(CPP_PROPERTY(AllowPeerConnections	), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("AllowPeerVoice"),		 RF_Public)UBoolProperty(CPP_PROPERTY(AllowPeerVoice		), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("MaxDownloadSize"),	     RF_Public)UIntProperty  (CPP_PROPERTY(MaxDownloadSize      ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("NetConnectionClassName"),RF_Public)UStrProperty(CPP_PROPERTY(NetConnectionClassName), TEXT("Client"), CPF_Config );	

	UArrayProperty* B = new(GetClass(),TEXT("DownloadManagers"),RF_Public)UArrayProperty( CPP_PROPERTY(DownloadManagers), TEXT("Client"), CPF_Config );
	B->Inner = new(B,TEXT("StrProperty0"),RF_Public)UStrProperty;

	UClass* TheClass = GetClass();
	TheClass->EmitObjectArrayReference( STRUCT_OFFSET( UNetDriver, ClientConnections ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UNetDriver, ServerConnection ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UNetDriver, MasterMap ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UNetDriver, RoleProperty ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UNetDriver, RemoteRoleProperty ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UNetDriver, NetConnectionClass ) );
}

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UNetDriver::InitializeIntrinsicPropertyValues()
{
	// Default values.
	MaxClientRate = 15000;
	MaxInternetClientRate = 10000;
}

void UNetDriver::AssertValid()
{
}
void UNetDriver::TickFlush()
{
	// Update network stats
	if (Time - StatUpdateTime > StatPeriod)
	{
		FLOAT RealTime = Time - StatUpdateTime;
		// Use the elapsed time to keep things scaled to one measured unit
		InBytes = appTrunc(InBytes / RealTime);
		OutBytes = appTrunc(OutBytes / RealTime);

		// Save off for stats later

		InBytesPerSecond = InBytes;
		OutBytesPerSecond = OutBytes;

		InPackets = appTrunc(InPackets / RealTime);
		OutPackets = appTrunc(OutPackets / RealTime);
		InBunches = appTrunc(InBunches / RealTime);
		OutBunches = appTrunc(OutBunches / RealTime);
		OutPacketsLost = appTrunc(100.f * OutPacketsLost / ::Max((FLOAT)OutPackets,1.f));
		InPacketsLost = appTrunc(100.f * InPacketsLost / ::Max((FLOAT)InPackets + InPacketsLost,1.f));


#if DEDICATED_SERVER
		//Update total connections
		GDedicatedServerPerfCounters.NumClientConnections = ClientConnections.Num();

		//Update per connection statistics
		for (INT i = 0; i < ClientConnections.Num(); i++)
		{
			UNetConnection* Connection = ClientConnections(i);

			if (Connection != NULL)
			{
				if (Connection->Actor != NULL && Connection->Actor->PlayerReplicationInfo != NULL)
				{
					//Ping value replicated by clients to server
					GDedicatedServerPerfCounters.ClientPerfCounters[i].Ping = Connection->Actor->PlayerReplicationInfo->Ping * 4;
				}

				//Store the data per frame only 
				GDedicatedServerPerfCounters.ClientPerfCounters[i].InBytes = Connection->InBytes;
				GDedicatedServerPerfCounters.ClientPerfCounters[i].OutBytes = Connection->OutBytes;
				GDedicatedServerPerfCounters.ClientPerfCounters[i].PacketLoss = Connection->InPacketsLost + Connection->OutPacketsLost;
			}
		}
#endif //DEDICATED_SERVER

		
#if STATS
		if (bIsPeer)
		{
			// Copy the net status values over
			SET_DWORD_STAT(STAT_PeerPing, 0);
			SET_DWORD_STAT(STAT_PeerChannels, 0);
			if (ServerConnection != NULL)
			{
				INC_DWORD_STAT_BY(STAT_PeerChannels, ServerConnection->OpenChannels.Num());
			}
			for (INT i = 0; i < ClientConnections.Num(); i++)
			{
				INC_DWORD_STAT_BY(STAT_PeerChannels, ClientConnections(i)->OpenChannels.Num());
			}
			SET_DWORD_STAT(STAT_PeerOutLoss,OutPacketsLost);
			SET_DWORD_STAT(STAT_PeerInLoss,InPacketsLost);
			SET_DWORD_STAT(STAT_PeerInRate,InBytes);
			SET_DWORD_STAT(STAT_PeerOutRate,OutBytes);
			SET_DWORD_STAT(STAT_PeerInPackets,InPackets);
			SET_DWORD_STAT(STAT_PeerOutPackets,OutPackets);
			SET_DWORD_STAT(STAT_PeerInBunches,InBunches);
			SET_DWORD_STAT(STAT_PeerOutBunches,OutBunches);
			// Use the elapsed time to keep things scaled to one measured unit
			VoicePacketsSent = appTrunc(VoicePacketsSent / RealTime);
			SET_DWORD_STAT(STAT_PeerVoicePacketsSent,VoicePacketsSent);
			VoicePacketsRecv = appTrunc(VoicePacketsRecv / RealTime);
			SET_DWORD_STAT(STAT_PeerVoicePacketsRecv,VoicePacketsRecv);
			VoiceBytesSent = appTrunc(VoiceBytesSent / RealTime);
			SET_DWORD_STAT(STAT_PeerVoiceBytesSent,VoiceBytesSent);
			VoiceBytesRecv = appTrunc(VoiceBytesRecv / RealTime);
			SET_DWORD_STAT(STAT_PeerVoiceBytesRecv,VoiceBytesRecv);
			// Determine voice percentages
			if (InBytes > 0)
			{
				VoiceInPercent = appTrunc(100.f * (FLOAT)VoiceBytesRecv / (FLOAT)InBytes);
			}
			else
			{
				VoiceInPercent = 0;
			}
			if (OutBytes > 0)
			{
				VoiceOutPercent = appTrunc(100.f * (FLOAT)VoiceBytesSent / (FLOAT)OutBytes);
			}
			else
			{
				VoiceOutPercent = 0;
			}
			SET_DWORD_STAT(STAT_PeerPercentInVoice,VoiceInPercent);
			SET_DWORD_STAT(STAT_PeerPercentOutVoice,VoiceOutPercent);
		}
		else
		{
			// Copy the net status values over
			if (ServerConnection != NULL && ServerConnection->Actor != NULL && ServerConnection->Actor->PlayerReplicationInfo != NULL)
			{
				SET_DWORD_STAT(STAT_Ping, appTrunc(1000.0f * ServerConnection->Actor->PlayerReplicationInfo->ExactPing));
			}
			else
			{
				SET_DWORD_STAT(STAT_Ping, 0);
			}
			SET_DWORD_STAT(STAT_Channels, 0);
			if (ServerConnection != NULL)
			{
				INC_DWORD_STAT_BY(STAT_Channels, ServerConnection->OpenChannels.Num());
			}
			for (INT i = 0; i < ClientConnections.Num(); i++)
			{
				INC_DWORD_STAT_BY(STAT_Channels, ClientConnections(i)->OpenChannels.Num());
			}
			SET_DWORD_STAT(STAT_OutLoss,OutPacketsLost);
			SET_DWORD_STAT(STAT_InLoss,InPacketsLost);
			SET_DWORD_STAT(STAT_InRate,InBytes);
			SET_DWORD_STAT(STAT_OutRate,OutBytes);
			SET_DWORD_STAT(STAT_InPackets,InPackets);
			SET_DWORD_STAT(STAT_OutPackets,OutPackets);
			SET_DWORD_STAT(STAT_InBunches,InBunches);
			SET_DWORD_STAT(STAT_OutBunches,OutBunches);
			// Use the elapsed time to keep things scaled to one measured unit
			VoicePacketsSent = appTrunc(VoicePacketsSent / RealTime);
			SET_DWORD_STAT(STAT_VoicePacketsSent,VoicePacketsSent);
			VoicePacketsRecv = appTrunc(VoicePacketsRecv / RealTime);
			SET_DWORD_STAT(STAT_VoicePacketsRecv,VoicePacketsRecv);
			VoiceBytesSent = appTrunc(VoiceBytesSent / RealTime);
			SET_DWORD_STAT(STAT_VoiceBytesSent,VoiceBytesSent);
			VoiceBytesRecv = appTrunc(VoiceBytesRecv / RealTime);
			SET_DWORD_STAT(STAT_VoiceBytesRecv,VoiceBytesRecv);
			// Determine voice percentages
			if (InBytes > 0)
			{
				VoiceInPercent = appTrunc(100.f * (FLOAT)VoiceBytesRecv / (FLOAT)InBytes);
			}
			else
			{
				VoiceInPercent = 0;
			}
			if (OutBytes > 0)
			{
				VoiceOutPercent = appTrunc(100.f * (FLOAT)VoiceBytesSent / (FLOAT)OutBytes);
			}
			else
			{
				VoiceOutPercent = 0;
			}
			SET_DWORD_STAT(STAT_PercentInVoice,VoiceInPercent);
			SET_DWORD_STAT(STAT_PercentOutVoice,VoiceOutPercent);
		}
#endif
		// Reset everything
		InBytes = 0;
		OutBytes = 0;
		InPackets = 0;
		OutPackets = 0;
		InBunches = 0;
		OutBunches = 0;
		OutPacketsLost = 0;
		InPacketsLost = 0;
		VoicePacketsSent = 0;
		VoiceBytesSent = 0;
		VoicePacketsRecv = 0;
		VoiceBytesRecv = 0;
		VoiceInPercent = 0;
		VoiceOutPercent = 0;
		StatUpdateTime = Time;
	}

	// Poll all sockets.
	if( ServerConnection )
	{
		// Queue client voice packets in the server's voice channel
		ProcessLocalClientPackets();
		ServerConnection->Tick();
	}
	else
	{
		// Queue up any voice packets the server has locally
		ProcessLocalServerPackets();
	}
	for( INT i=0; i<ClientConnections.Num(); i++ )
	{
		ClientConnections(i)->Tick();
	}
}

/**
 * Initializes the net connection class to use for new connections
 */
UBOOL UNetDriver::InitConnectionClass(void)
{
	if (NetConnectionClass == NULL && NetConnectionClassName != TEXT(""))
	{
		NetConnectionClass = LoadClass<UNetConnection>(NULL,*NetConnectionClassName,NULL,LOAD_None,NULL);
		if (NetConnectionClass == NULL)
		{
			debugf(NAME_Error,TEXT("Failed to load class '%s'"),*NetConnectionClassName);
		}
	}
	return NetConnectionClass != NULL;
}


UBOOL UNetDriver::InitConnect( FNetworkNotify* InNotify, const FURL& URL, FString& Error )
{
	InitConnectionClass();
	Notify = InNotify;

	return 1;
}
UBOOL UNetDriver::InitListen( FNetworkNotify* InNotify, FURL& URL, FString& Error )
{
	InitConnectionClass();
	Notify = InNotify;
	return 1;
}

/**
 * Initialize a new peer connection on the net driver
 *
 * @param InNotify notification object to associate with the net driver
 * @param ConnectURL remote ip:port of client peer to connect
 * @param RemotePlayerId remote net id of client peer player
 * @param LocalPlayerId net id of primary local player
 * @param Error resulting error string from connection attempt
 */
UBOOL UNetDriver::InitPeer( FNetworkNotify* InNotify, const FURL& ConnectURL, FUniqueNetId RemotePlayerId, FUniqueNetId LocalPlayerId, FString& Error )
{
	InitConnectionClass();
	Notify = InNotify;
	return TRUE;
}

/**
 * Update peer connections. Associate each connection with the net owner.
 *
 * @param NetOwner player controller (can be NULL) for the current client
 */
void UNetDriver::UpdatePeerConnections(APlayerController* NetOwner)
{
	if (bIsPeer)
	{
		// Ignore net owner if it is being destroyed
		if (NetOwner != NULL && 
			(NetOwner->IsPendingKill() || NetOwner->bDeleteMe))
		{
			NetOwner = NULL;
		}
		// Each peer connection uses the net owner as their actor
		for (INT i=0; i < ClientConnections.Num(); i++)
		{
			UNetConnection* PeerConn = ClientConnections(i);
			if (PeerConn != NULL)
			{
				PeerConn->Actor = NetOwner;
			}
		}

		for (INT i=0; i < ClientConnections.Num(); i++)
		{
			UNetConnection* PeerConn = ClientConnections(i);
			if (PeerConn != NULL && 
				PeerConn->State == USOCK_Pending)
			{
				if (Time - PeerConn->ConnectTime > InitialConnectTimeout)
				{
					//@todo peer - this actually fails since the other client peer will continue to send data and re-establish the connection

					FString ErrCode(TEXT("Peer timeout"));
					FNetControlMessage<NMT_Failure>::Send(PeerConn, ErrCode);
					PeerConn->FlushNet();
					PeerConn->CleanUp();
				}
			}
		}

		if (NetOwner != NULL)
		{
			// sync removed peers net ids on owning player
			for (INT PeerIdx=0; PeerIdx < NetOwner->ConnectedPeers.Num(); PeerIdx++)
			{
				const FConnectedPeerInfo& PeerInfo = NetOwner->ConnectedPeers(PeerIdx);
				if (PeerInfo.PlayerID.HasValue())
				{
					UBOOL bFoundConnection = FALSE;
					for (INT i=0; i < ClientConnections.Num(); i++)
					{
						UNetConnection* PeerConn = ClientConnections(i);
						if (PeerConn != NULL && 
							PeerConn->PlayerId == PeerInfo.PlayerID)
						{
							bFoundConnection = TRUE;
							break;
						}
					}
					if (!bFoundConnection)
					{
						NetOwner->eventRemovePeer(PeerInfo.PlayerID);
						PeerIdx--;
					}
				}
			}
			// sync newly added peer net ids on owning player
			for (INT i=0; i < ClientConnections.Num(); i++)
			{
				UNetConnection* PeerConn = ClientConnections(i);
				if (PeerConn != NULL && 
					PeerConn->PlayerId.HasValue() &&
					!NetOwner->HasPeerConnection(PeerConn->PlayerId))
				{
					//@todo peer - send nattype when peer connects
					NetOwner->eventAddPeer(PeerConn->PlayerId,NAT_Open);
				}
			}
		}
	}
}

void UNetDriver::TickDispatch( FLOAT DeltaTime )
{
	SendCycles=RecvCycles=0;

	// Get new time.
	Time += DeltaTime;

	// Checks for standby cheats if enabled
	UpdateStandbyCheatStatus();

	// Delete any straggler connections.
	if( !ServerConnection )
	{
		for( INT i=ClientConnections.Num()-1; i>=0; i-- )
		{
			if( ClientConnections(i)->State==USOCK_Closed )
			{
				ClientConnections(i)->CleanUp();
			}
		}
	}
}

/**
 * Updates the standby cheat information triggering/removing the cheat dialog as needed
 */
void UNetDriver::UpdateStandbyCheatStatus(void)
{
	// Only the server needs to check
	if (ServerConnection == NULL && ClientConnections.Num())
	{
		// Only check for cheats if enabled and one wasn't previously detected
		if (bIsStandbyCheckingEnabled &&
			bHasStandbyCheatTriggered == FALSE &&
			ClientConnections.Num() > 2)
		{
			INT CountBadTx = 0;
			INT CountBadRx = 0;
			INT CountBadPing = 0;
			FLOAT CurrentTime = GWorld->GetTimeSeconds();
			// Look at each connection checking for a receive time and an ack time
			for (INT Index = 0; Index < ClientConnections.Num(); Index++)
			{
				UNetConnection* NetConn = ClientConnections(Index);
				// Don't check connections that aren't fully formed (still loading & no controller)
				// Controller won't be present until the join message is sent, which is after loading has completed
				if (NetConn &&
					NetConn->Actor &&
					CurrentTime - NetConn->Actor->CreationTime > JoinInProgressStandbyWaitTime &&
					// Ignore players with pending delete (kicked/timed out, but connection not closed)
					NetConn->Actor->bPendingDelete == FALSE)
				{
					if (Time - NetConn->LastReceiveTime > StandbyRxCheatTime)
					{
						CountBadRx++;
					}
					if (Time - NetConn->LastRecvAckTime > StandbyTxCheatTime)
					{
						CountBadTx++;
					}
					// Check for host tampering or crappy upstream bandwidth
					if (NetConn->Actor->PlayerReplicationInfo &&
						NetConn->Actor->PlayerReplicationInfo->Ping * 4 > BadPingThreshold)
					{
						CountBadPing++;
					}
				}
			}
			AGameInfo* GameInfo = GWorld->GetWorldInfo() ? GWorld->GetWorldInfo()->Game : NULL;
			if (GameInfo)
			{
				// See if we hit the percentage required for either TX or RX standby detection
				if (FLOAT(CountBadRx) / FLOAT(ClientConnections.Num()) > PercentMissingForRxStandby)
				{
					bHasStandbyCheatTriggered = TRUE;
					// Send to the GameInfo for processing
					GameInfo->eventStandbyCheatDetected(STDBY_Rx);
				}
				else if (FLOAT(CountBadPing) / FLOAT(ClientConnections.Num()) > PercentForBadPing)
				{
					bHasStandbyCheatTriggered = TRUE;
					// Send to the GameInfo for processing
					GameInfo->eventStandbyCheatDetected(STDBY_BadPing);
				}
				// Check for the host not sending to the clients, but only during a match
				else if (GameInfo->GetStateFrame()->StateNode->GetFName() == FName(TEXT("MatchInProgress"),FNAME_Find) &&
					FLOAT(CountBadTx) / FLOAT(ClientConnections.Num()) > PercentMissingForTxStandby)
				{
					bHasStandbyCheatTriggered = TRUE;
					// Send to the GameInfo for processing
					GameInfo->eventStandbyCheatDetected(STDBY_Tx);
				}
			}
		}
	}
}

void UNetDriver::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	// Prevent referenced objects from being garbage collected.
	Ar << ClientConnections << ServerConnection << MasterMap << RoleProperty << RemoteRoleProperty;

	if (Ar.IsCountingMemory())
	{
		ClientConnections.CountBytes(Ar);
		ForcedInitialReplicationMap.CountBytes(Ar);
		Ar << DownloadManagers;
	}
}
void UNetDriver::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Destroy server connection.
		if( ServerConnection )
		{
			ServerConnection->CleanUp();
		}
		// Destroy client connections.
		while( ClientConnections.Num() )
		{
			UNetConnection* ClientConnection = ClientConnections(0);
			ClientConnection->CleanUp();
		}
		// Low level destroy.
		LowLevelDestroy();

		// remove from net object notify
		UPackage::NetObjectNotifies.RemoveItem(this);

		// Delete the master package map.
		MasterMap = NULL;
	}
	else
	{
		check(ServerConnection==NULL);
		check(ClientConnections.Num()==0);
		check(MasterMap==NULL);
	}
	Super::FinishDestroy();
}

/**
 * Called by the game engine, when the game is exiting, to allow for special game-exit cleanup
 */
void UNetDriver::PreExit()
{
}

UBOOL UNetDriver::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( ParseCommand(&Cmd,TEXT("SOCKETS")) )
	{
		// Print list of open connections.
		Ar.Logf( TEXT("%s Connections:"), *GetDescription() );
		if( ServerConnection )
		{
			Ar.Logf( TEXT("   Server %s"), *ServerConnection->LowLevelDescribe() );
			for( INT i=0; i<ServerConnection->OpenChannels.Num(); i++ )
				Ar.Logf( TEXT("      Channel %i: %s"), ServerConnection->OpenChannels(i)->ChIndex, *ServerConnection->OpenChannels(i)->Describe() );
		}
		for( INT i=0; i<ClientConnections.Num(); i++ )
		{
			UNetConnection* Connection = ClientConnections(i);
			Ar.Logf( TEXT("   Client (0x%016I64X) %s"), Connection->PlayerId.Uid, *Connection->LowLevelDescribe() );
			for( INT j=0; j<Connection->OpenChannels.Num(); j++ )
				Ar.Logf( TEXT("      Channel %i: %s"), Connection->OpenChannels(j)->ChIndex, *Connection->OpenChannels(j)->Describe() );
		}
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PACKAGEMAP")))
	{
		// Print packagemap for open connections
		Ar.Logf(TEXT("Package Map:"));
		if (ServerConnection != NULL)
		{
			Ar.Logf(TEXT("   Server %s"), *ServerConnection->LowLevelDescribe());
			ServerConnection->PackageMap->LogDebugInfo(Ar);
		}
		for (INT i = 0; i < ClientConnections.Num(); i++)
		{
			UNetConnection* Connection = ClientConnections(i);
			Ar.Logf( TEXT("   Client %s"), *Connection->LowLevelDescribe() );
			Connection->PackageMap->LogDebugInfo(Ar);
		}
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("NETFLOOD")))
	{
		UNetConnection* TestConn = NULL;
		if (ServerConnection != NULL)
		{
			TestConn = ServerConnection;
		}
		else if (ClientConnections.Num() > 0)
		{
			TestConn = ClientConnections(0);
		}
		if (TestConn != NULL)
		{
			Ar.Logf(TEXT("Flooding connection 0 with control messages"));

			for (INT i = 0; i < 256 && TestConn->State == USOCK_Open; i++)
			{
				FNetControlMessage<NMT_Netspeed>::Send(TestConn, TestConn->CurrentNetSpeed);
				TestConn->FlushNet();
			}
		}
		return TRUE;
	}
#if DO_ENABLE_NET_TEST
	// This will allow changing the Pkt* options at runtime
	else if (PacketSimulationSettings.ParseSettings(Cmd))
	{
		if (ServerConnection)
		{
			// Notify the server connection of the change
			ServerConnection->UpdatePacketSimulationSettings();
		}
		else
		{
			// Notify all client connections that the settings have changed
			for (INT Index = 0; Index < ClientConnections.Num(); Index++)
			{
				ClientConnections(Index)->UpdatePacketSimulationSettings();
			}
		}		
		return TRUE;
	}
#endif
	else if (ParseCommand(&Cmd, TEXT("NETDEBUGTEXT")))
	{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		// Send a text string for testing connection
		FString TestStr = ParseToken(Cmd,FALSE);
		if (ServerConnection != NULL)
		{
			debugf(NAME_DevNet,TEXT("%s sending NMT_DebugText [%s] to [%s]"), 
				*GetDescription(),*TestStr, *ServerConnection->LowLevelDescribe());

			FNetControlMessage<NMT_DebugText>::Send(ServerConnection,TestStr);
				ServerConnection->FlushNet(TRUE);
		}
		else
		{
			for (INT ClientIdx=0; ClientIdx < ClientConnections.Num(); ClientIdx++)
			{
				UNetConnection* Connection = ClientConnections(ClientIdx);
				if (Connection)
				{
					debugf(NAME_DevNet,TEXT("%s sending NMT_DebugText [%s] to [%s]"), 
						*GetDescription(),*TestStr, *Connection->LowLevelDescribe());

					FNetControlMessage<NMT_DebugText>::Send(Connection,TestStr);
					Connection->FlushNet(TRUE);
				}
			}
		}
#endif
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("NETDISCONNECT")))
	{
		FString Msg(TEXT("NETDISCONNECT MSG"));
		if (ServerConnection != NULL)
		{
			debugf(NAME_DevNet,TEXT("%s disconnecting connection from host [%s]"), 
				*GetDescription(),*ServerConnection->LowLevelDescribe());

			FNetControlMessage<NMT_Failure>::Send(ServerConnection,Msg);
		}
		else
		{
			for (INT ClientIdx=0; ClientIdx < ClientConnections.Num(); ClientIdx++)
			{
				UNetConnection* Connection = ClientConnections(ClientIdx);
				if (Connection)
				{
					debugf(NAME_DevNet,TEXT("%s disconnecting from client [%s]"), 
						*GetDescription(),*Connection->LowLevelDescribe());

					FNetControlMessage<NMT_Failure>::Send(Connection,Msg);
					Connection->FlushNet(TRUE);
				}
			}
		}
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}
void UNetDriver::NotifyActorDestroyed( AActor* ThisActor )
{
	ForcedInitialReplicationMap.Remove(ThisActor);
	for( INT i=ClientConnections.Num()-1; i>=0; i-- )
	{
		UNetConnection* Connection = ClientConnections(i);
		if( ThisActor->bNetTemporary )
			Connection->SentTemporaries.RemoveItem( ThisActor );
		UActorChannel* Channel = Connection->ActorChannels.FindRef(ThisActor);
		if( Channel )
		{
			check(Channel->OpenedLocally);
			Channel->bClearRecentActorRefs = FALSE;
			Channel->Close();
		}
	}
}

/** creates a child connection and adds it to the given parent connection */
UChildConnection* UNetDriver::CreateChild(UNetConnection* Parent)
{
	debugf(NAME_DevNet, TEXT("Creating child connection with %s parent"), *Parent->GetName());
	UChildConnection* Child = new UChildConnection();
	Child->Driver = this;
	Child->URL = FURL();
	Child->State = Parent->State;
	Child->URL.Host = Parent->URL.Host;
	Child->Parent = Parent;
	Child->PackageMap = Parent->PackageMap;
	Child->CurrentNetSpeed = Parent->CurrentNetSpeed;
	Parent->Children.AddItem(Child);
	return Child;
}

/** notification when a package is added to the NetPackages list */
void UNetDriver::NotifyNetPackageAdded(UPackage* Package)
{
	if (!GIsRequestingExit && ServerConnection == NULL)
	{
		MasterMap->AddPackage(Package);
		for (INT i = 0; i < ClientConnections.Num(); i++)
		{
			if (ClientConnections(i) != NULL)
			{
				ClientConnections(i)->AddNetPackage(Package);
			}
		}
	}
}

/** notification when a package is removed from the NetPackages list */
void UNetDriver::NotifyNetPackageRemoved(UPackage* Package)
{
	if (!GIsRequestingExit && ServerConnection == NULL)
	{
		// we actually delete the entry in the MasterMap as otherwise when it gets sent to new clients those clients will attempt to load packages that it shouldn't have around
		MasterMap->RemovePackage(Package, TRUE);
		// we DO NOT delete the entry in client PackageMaps so that indices are not reshuffled during play as that can potentially result in mismatches
		for (INT i = 0; i < ClientConnections.Num(); i++)
		{
			if (ClientConnections(i) != NULL)
			{
				ClientConnections(i)->RemoveNetPackage(Package);
			}
		}
	}
}

/** notification when an object is removed from a net package's NetObjects list */
void UNetDriver::NotifyNetObjectRemoved(UObject* Object)
{
	UClass* RemovedClass = Cast<UClass>(Object);
	if (RemovedClass != NULL)
	{
		MasterMap->RemoveClassNetCache(RemovedClass);
		for (INT i = 0; i < ClientConnections.Num(); i++)
		{
			ClientConnections(i)->PackageMap->RemoveClassNetCache(RemovedClass);
		}
		if (ServerConnection != NULL)
		{
			ServerConnection->PackageMap->RemoveClassNetCache(RemovedClass);
		}
	}
}

/** The global structure for managing voice data */
FVoiceData GVoiceData;

/**
 * Mark all local voice packets as being processed.
 * This occurs after all net drivers have had a chance to read the local voice data.
 */
void UNetDriver::ClearLocalVoicePackets(void)
{
	for (DWORD Index = 0; Index < MAX_SPLITSCREEN_TALKERS; Index++)
	{
		FVoicePacket& LocalPacket = GVoiceData.LocalPackets[Index];
		if (LocalPacket.Length > 0)
		{
			// Mark the local packet as processed
			LocalPacket.Length = 0;
		}
	}
}

/**
 * Process any local talker packets that need to be sent to clients
 */
void UNetDriver::ProcessLocalServerPackets(void)
{
	// Process all of the local packets
	for (DWORD Index = 0; Index < MAX_SPLITSCREEN_TALKERS; Index++)
	{
		FVoicePacket& LocalPacket = GVoiceData.LocalPackets[Index];
		// Check for something to send for this local talker
		if (LocalPacket.Length > 0)
		{
			// Create a new ref counted packet that is a copy of this local packet
			FVoicePacket* VoicePacket = new FVoicePacket(LocalPacket,1);
			// See if anyone wants this packet
			ReplicateVoicePacket(VoicePacket,NULL);
			// Relase our copy of the packet
			VoicePacket->DecRef();
			// once all local voice packets are processed then call ClearLocalVoicePackets()
		}
	}
}

/**
 * Determine if local voice pakcets should be replicated to server.
 * If there are peer connections that can handle the data then no need to replicate to host.
 *
 * @return TRUE if voice data should be replicated on the server connection
 */
UBOOL UNetDriver::ShouldSendVoicePacketsToServer()
{
	// Determine if the peer net driver will be forwarding the packet
	if (AllowPeerVoice)
	{
		UBOOL bResult = FALSE;		
		APlayerController* NetOwner = ServerConnection->Actor;

		// Get list of PRI actor channels that may care about voice data
		for (INT ChIdx=0; ChIdx < ServerConnection->OpenChannels.Num(); ChIdx++)
		{
			UActorChannel* ActorChan = Cast<UActorChannel>(ServerConnection->OpenChannels(ChIdx));
			if (ActorChan != NULL)
			{
				APlayerReplicationInfo* PRI = Cast<APlayerReplicationInfo>(ActorChan->Actor);
				if (PRI != NULL && 
					PRI->UniqueId.HasValue() &&
					// Ignore ourself
					PRI->Owner != NetOwner &&
					// No need to send any data to server if a player is muted
					!NetOwner->IsPlayerMuted(PRI->UniqueId) &&
					// If peer connection exists then no need to send to server
					!NetOwner->HasPeerConnection(PRI->UniqueId))
				{
					bResult = TRUE;
					break;
				}
			}
		}
		return bResult;
	}
	else
	{
		return TRUE;
	}
}

/**
 * Process any local talker packets that need to be sent to the server
 */
void UNetDriver::ProcessLocalClientPackets(void)
{
	//@todo peer - verify that voice packets are only replicated to server when needed
	// should occur if server is a player host and is not muted for this local sender
	// should occur if server is a player or dedicated host and there is a missing peer connection that is not muted for this player
	// note that ded server has no player so no need to send to it unless above condition

	UVoiceChannel* VoiceChannel = ServerConnection->GetVoiceChannel();
	// Process all of the local packets
	for (DWORD Index = 0; Index < MAX_SPLITSCREEN_TALKERS; Index++)
	{
		FVoicePacket& LocalPacket = GVoiceData.LocalPackets[Index];
		// Check for something to send for this local talker
		if (LocalPacket.Length > 0)
		{
			// If there is a voice channel to the server, submit the packets
			if (VoiceChannel != NULL &&
				ShouldSendVoicePacketsToServer())
			{
				// Create a new ref counted packet that is a copy of this local packet
				FVoicePacket* VoicePacket = new FVoicePacket(LocalPacket,1);
				// Add the voice packet for network sending
				VoiceChannel->AddVoicePacket(VoicePacket);
				// Relase our copy of the packet
				VoicePacket->DecRef();
			}
			// once all local voice packets are processed then call ClearLocalVoicePackets()
		}
	}
}

/**
 * Determines which other connections should receive the voice packet and
 * queues the packet for those connections. Used for sending both local/remote voice packets.
 *
 * @param VoicePacket the packet to be queued
 * @param CameFromConn the connection this packet came from (NULL if local)
 */
void UNetDriver::ReplicateVoicePacket(FVoicePacket* VoicePacket,UNetConnection* CameFromConn)
{
	// Iterate the connections and see if they want the packet
	for (INT Index = 0; Index < ClientConnections.Num(); Index++)
	{
		UNetConnection* Conn = ClientConnections(Index);
		// Skip the originating connection
		if (CameFromConn != Conn)
		{
			// If server then determine if it should replicate the voice packet from another sender to this connection
			const UBOOL bReplicateAsServer = !bIsPeer && Conn->ShouldReplicateVoicePacketFrom(VoicePacket->Sender);
			// If client peer then determine if it should send the voice packet to another client peer
			const UBOOL bReplicateAsPeer = (bIsPeer && AllowPeerVoice) && Conn->ShouldReplicateVoicePacketToPeer(Conn->PlayerId);
			
			if (bReplicateAsServer || bReplicateAsPeer)
			{
				UVoiceChannel* VoiceChannel = Conn->GetVoiceChannel();
				if (VoiceChannel != NULL)
				{
					// Add the voice packet for network sending
					VoiceChannel->AddVoicePacket(VoicePacket);
				}
			}
		}
	}
}

IMPLEMENT_CLASS(UNetDriver);

#if DO_ENABLE_NET_TEST

/** reads in settings from the .ini file 
 * @note: overwrites all previous settings
 */
void FPacketSimulationSettings::LoadConfig()
{
	if (GConfig->GetInt(TEXT("PacketSimulationSettings"), TEXT("PktLoss"), PktLoss, GEngineIni))
	{
		PktLoss = Clamp<INT>(PktLoss, 0, 100);
	}
	
	UBOOL InPktOrder = UBOOL(PktOrder);
	GConfig->GetBool(TEXT("PacketSimulationSettings"), TEXT("PktOrder"), InPktOrder, GEngineIni);
	PktOrder = INT(InPktOrder);
	
	GConfig->GetInt(TEXT("PacketSimulationSettings"), TEXT("PktLag"), PktLag, GEngineIni);
	
	if (GConfig->GetInt(TEXT("PacketSimulationSettings"), TEXT("PktDup"), PktDup, GEngineIni))
	{
		PktDup = Clamp<INT>(PktDup, 0, 100);
	}

	if (GConfig->GetInt(TEXT("PacketSimulationSettings"), TEXT("PktLagVariance"), PktLagVariance, GEngineIni))
	{
		PktLagVariance = Clamp<INT>(PktLagVariance, 0, 100);
	}
}

/**
 * Reads the settings from a string: command line or an exec
 *
 * @param Stream the string to read the settings from
 */
UBOOL FPacketSimulationSettings::ParseSettings(const TCHAR* Cmd)
{
	// note that each setting is tested.
	// this is because the same function will be used to parse the command line as well
	UBOOL bParsed = FALSE;

	if( Parse(Cmd,TEXT("PktLoss="), PktLoss) )
	{
		bParsed = TRUE;
		Clamp<INT>( PktLoss, 0, 100 );
		debugf(TEXT("PktLoss set to %d"), PktLoss);
	}
	if( Parse(Cmd,TEXT("PktOrder="), PktOrder) )
	{
		bParsed = TRUE;
		Clamp<INT>( PktOrder, 0, 1 );
		debugf(TEXT("PktOrder set to %d"), PktOrder);
	}
	if( Parse(Cmd,TEXT("PktLag="), PktLag) )
	{
		bParsed = TRUE;
		debugf(TEXT("PktLag set to %d"), PktLag);
	}
	if( Parse(Cmd,TEXT("PktDup="), PktDup) )
	{
		bParsed = TRUE;
		Clamp<INT>( PktDup, 0, 100 );
		debugf(TEXT("PktDup set to %d"), PktDup);
	}
	if( Parse(Cmd,TEXT("PktLagVariance="), PktLagVariance) )
	{
		bParsed = TRUE;
		Clamp<INT>( PktLagVariance, 0, 100 );
		debugf(TEXT("PktLagVariance set to %d"), PktLagVariance);
	}
	return bParsed;
}

#endif
