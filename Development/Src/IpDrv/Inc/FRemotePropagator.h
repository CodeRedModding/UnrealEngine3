/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#if WITH_UE3_NETWORKING

struct FRemoteTargetInfo
{
	DWORD IPAddress;
	UBOOL bIntelByteOrder;
};

class FRemotePropagator : public FStandardObjectPropagator
{
public:
	UBOOL Connect();

	void OnPropertyChange(UObject* Object, UProperty* Property, INT PropertyOffset);
	void OnActorMove(AActor* Actor);
	void OnActorCreate(class AActor* Actor);
	void OnActorDelete(class AActor* Actor);
	void OnObjectRename(UObject* Object, const TCHAR* NewName);

	virtual void AddTarget(TARGETHANDLE Target, DWORD RemoteIPAddress, UBOOL bIntelByteOrder);
	virtual void RemoveTarget(TARGETHANDLE Target);
	virtual UINT GetTargetCount() const;
	virtual void ClearTargets();

	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );

protected:

	void SendChange(class FNetworkPropagatorBase* Change);

	TMap<TARGETHANDLE, FRemoteTargetInfo> TargetMap;
};

class FListenPropagator : public FStandardObjectPropagator
{
public:
	UBOOL Connect();

	void Tick(FLOAT DeltaTime);
	void OnReceivedData(FIpAddr SrcAddr, BYTE* Data, INT Count);

protected:
	void OnNetworkPropertyChange(FIpAddr SrcAddr, struct FNetworkPropertyChange* PropertyChange);
	void OnNetworkActorMove(FIpAddr SrcAddr, struct FNetworkActorMove* ActorMove);
	void OnNetworkActorCreate(FIpAddr SrcAddr, struct FNetworkActorCreate* ActorCreate);
	void OnNetworkActorDelete(FIpAddr SrcAddr, struct FNetworkActorDelete* ActorDelete);
	void OnNetworkObjectRename(FIpAddr SrcAddr, struct FNetworkObjectRename* ObjectRename);
	void OnNetworkRemoteConsoleCommand(FIpAddr SrcAddr, struct FNetworkRemoteConsoleCommand* RemoteConsoleCommand);
	// @todo addtypes: add more change types here

	/**
	 * Ignore sending actor as we're a listen propagator
	 * @param Actor Unused
	 */
	virtual void PropagateActor(class AActor* /*Actor*/) {}
};


////////////////////////////////////////////////////////////
//
// Remote propagation network data management
//
////////////////////////////////////////////////////////////

enum EPropagationType 
{
	PROP_Invalid,
	PROP_PropertyChange,
	PROP_ActorMove,
	PROP_ActorCreate,
	PROP_ActorDelete,
	PROP_ObjectRename,
	PROP_RemoteConsoleCommand,
	PROP_AssetStats,
};

// e.g. FNetworkPropertyChange
#define HANDLE_RECEIVED_DATA(ChangeType) \
	case PROP_##ChangeType: \
		{ \
			FNetwork##ChangeType ChangeType; \
			ChangeType.Serialize(PayloadReader); \
			OnNetwork##ChangeType(SrcAddr, &ChangeType); \
			break; \
		}

/**
 * This class handles sending a property change across the network
 */
class FNetworkPropagatorBase
{
public:
	FNetworkPropagatorBase(DWORD InType) : Type(InType)
	{ }

	/**
	* Sends a FNetworkPropagatorBase across the network. The subclass will fill
	* in the actual data, in FillPayload()
	*
	* @param	Link			The UDP link to send the data over
	* @param	IpAddr			The IP address to send the data to
	* @param	bIsIntelByteOrder	Is the destination Intel byte ordered?
	*/
	void SendTo(FUdpLink* Link, const FIpAddr& IpAddr, UBOOL bIsIntelByteOrder);

	/**
	* Puts the actual network data into the Data array
	*
	* @param	Ar				An archive to serialize into
	*/
	virtual void Serialize(FArchive& Ar) = 0;

protected:
	FNetworkPropagatorBase()
	{ }

	// EPropagationType e.g. PROP_RemoteConsoleCommand
	DWORD Type;
};

/**
 * This class handles sending and receiving a UObject property change over the network
 */
struct FNetworkPropertyChange : public FNetworkPropagatorBase
{
	FNetworkPropertyChange(const FString& InObjectName, const FString& InPropertyName, const FString& InPropertyValue, DWORD InPropertyOffset);
	FNetworkPropertyChange() {}
	/**
	* Puts the actual network data into the Data array
	*
	* @param	Ar				An archive to serialize into
	*/
	virtual void Serialize(FArchive& Ar);

public:
	FString ObjectName;
	FString PropertyName;
	FString PropertyValue;
	DWORD PropertyOffset;
};

/**
 * This class handles sending and receiving an AActor movement over the network
 */
struct FNetworkActorMove : public FNetworkPropagatorBase
{
	FNetworkActorMove(const FString& InActorName, const FVector& InLocation, const FRotator& InRotation);
	FNetworkActorMove() {}
	/**
	* Puts the actual network data into the Data array
	*
	* @param	Ar				An archive to serialize into
	*/
	virtual void Serialize(FArchive& Ar);

public:
	FString ActorName;
	FVector Location;
	FRotator Rotation;
};

/**
 * This class handles sending and receiving AActor creation over the network
 */
struct FNetworkActorCreate : public FNetworkPropagatorBase
{
	FNetworkActorCreate(const FString& InActorClass, const FString& InActorName, const FVector& InLocation, const FRotator& InRotation, const TArray<FString>& InComponentTemplatesAndNames);
	FNetworkActorCreate() {}
	/**
	* Puts the actual network data into the Data array
	*
	* @param	Ar				An archive to serialize into
	*/
	virtual void Serialize(FArchive& Ar);

public:
	FString ActorClass;
	FString ActorName;
	FVector Location;
	FRotator Rotation;
	TArray<FString> ComponentTemplatesAndNames;
};

/**
 * This class handles sending and receiving AActor deletion over the network
 */
struct FNetworkActorDelete : public FNetworkPropagatorBase
{
	FNetworkActorDelete(const FString& InActorName);
	FNetworkActorDelete() {}
	/**
	* Puts the actual network data into the Data array
	*
	* @param	Ar				An archive to serialize into
	*/
	virtual void Serialize(FArchive& Ar);

public:
	FString ActorName;
};

/**
 * This class handles sending and receiving object renaming over the network
 */
struct FNetworkObjectRename : public FNetworkPropagatorBase
{
	FNetworkObjectRename(const FString& InObjectName, const FString& InNewName);
	FNetworkObjectRename() {}
	/**
	* Puts the actual network data into the Data array
	*
	* @param	Ar				An archive to serialize into
	*/
	virtual void Serialize(FArchive& Ar);

public:
	FString ObjectName;
	FString NewName;
};

/**
 * This class handles sending and receiving console commands
 */
struct FNetworkRemoteConsoleCommand : public FNetworkPropagatorBase
{
	FNetworkRemoteConsoleCommand(const FString& InConsoleCommand);
	FNetworkRemoteConsoleCommand() {}

	/**
	* Puts the actual network data into the Data array
	*
	* @param	Ar				An archive to serialize into
	*/
	virtual void Serialize(FArchive& Ar);
public:
	FString ConsoleCommand;
};


#endif	//#if WITH_UE3_NETWORKING
