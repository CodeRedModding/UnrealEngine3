/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This file contains the definitions of the various remote stats facilities
 * and base classes
 */

#ifndef _STATS_NOTIFY_PROVIDERS_BASE_H
#define _STATS_NOTIFY_PROVIDERS_BASE_H

/**
 * This base class defines the interface for sending a stat to an endpoint
 * (networked client, file, etc.)
 */
class FStatNotifyProvider
{
	/**
	 * Provider name for INIs, etc.
	 */
	const TCHAR* ProviderName;
	/**
	 * Whether this provider is currently enabled
	 */
	UBOOL bIsEnabled;

protected:
	/**
	 * The current frame we are sending stats out for
	 */
	DWORD CurrentFrame;

private:
friend struct FStatNotifyProviderFactory;
	/**
	 * Hidden so that only the factory class can create these
	 */
	FStatNotifyProvider(void) :
		ProviderName(TEXT("")),
		bIsEnabled(FALSE),
		CurrentFrame((DWORD)-1)
	{
	}

protected:
	/**
	 * Hidden so that only the factory or derived class can create these
	 *
	 * @param InProviderName the name of this provider
	 * @param InProviderCmdLine	Short name for this provider that may be specified on the app's command line
	 */
	FStatNotifyProvider( const TCHAR* InProviderName, const TCHAR* InProviderCmdLine );

public:
	virtual ~FStatNotifyProvider()
	{

	}

	/**
	 * Function that must be implemented by child classes to handle per
	 * provider initialization
	 */
	virtual UBOOL Init(void) = 0;

	/**
	 * Function that must be implemented by child classes to handle per
	 * provider clean up
	 */
	virtual void Destroy(void) = 0;

	/**
	 * Tells the provider that we are starting to supply it with descriptions
	 * for all of the stats/groups.
	 */
	virtual void StartDescriptions(void) = 0;

	/**
	 * Tells the provider that we are finished sending descriptions for all of
	 * the stats/groups.
	 */
	virtual void EndDescriptions(void) = 0;

	/**
	 * Tells the provider that we are starting to supply it with group descriptions
	 */
	virtual void StartGroupDescriptions(void) = 0;

	/**
	 * Tells the provider that we are finished sending stat descriptions
	 */
	virtual void EndGroupDescriptions(void) = 0;

	/**
	 * Tells the provider that we are starting to supply it with stat descriptions
	 */
	virtual void StartStatDescriptions(void) = 0;

	/**
	 * Tells the provider that we are finished sending group descriptions
	 */
	virtual void EndStatDescriptions(void) = 0;

	/**
	 * Adds a stat to the list of descriptions. Used to allow custom stats to
	 * report who they are, parentage, etc. Prevents applications that consume
	 * the stats data from having to change when stats information changes
	 *
	 * @param StatId the id of the stat
	 * @param StatName the name of the stat
	 * @param StatType the type of stat this is
	 * @param GroupId the id of the group this stat belongs to
	 */
	virtual void AddStatDescription(DWORD StatId,const TCHAR* StatName,DWORD StatType,DWORD GroupId) = 0;

	/**
	 * Adds a group to the list of descriptions
	 *
	 * @param GroupId the id of the group being added
	 * @param GroupName the name of the group
	 */
	virtual void AddGroupDescription(DWORD GroupId,const TCHAR* GroupName) = 0;

	/**
	 * Function to write the stat out to the provider's data store
	 *
	 * @param StatId the id of the stat that is being written out
	 * @param GroupId the id of the group the stat belongs to
	 * @param ParentId the id of parent stat
	 * @param InstanceId the instance id of the stat being written
	 * @param ParentInstanceId the instance id of parent stat
	 * @param ThreadId the thread this stat is for
	 * @param Value the value of the stat to write out
	 * @param CallsPerFrame the number of calls for this frame
	 */
	virtual void WriteStat(DWORD StatId,DWORD GroupId,DWORD ParentId,DWORD InstanceId,
		DWORD ParentInstanceId,DWORD ThreadId,DWORD Value,
		DWORD CallsPerFrame) = 0;

	/**
	 * Function to write the stat out to the provider's data store
	 *
	 * @param StatId the id of the stat that is being written out
	 * @param GroupId the id of the group the stat belongs to
	 * @param Value the value of the stat to write out
	 */
	virtual void WriteStat(DWORD StatId,DWORD GroupId,FLOAT Value) = 0;

	/**
	 * Function to write the stat out to the provider's data store
	 *
	 * @param StatId the id of the stat that is being written out
	 * @param GroupId the id of the group the stat belongs to
	 * @param Value the value of the stat to write out
	 */
	virtual void WriteStat(DWORD StatId,DWORD GroupId,DWORD Value) = 0;

	/**
	 * Returns whether the stats collection is always 'on' when enabled. E.g. the
	 * UDP provider is only enabled when a connection is made, whereof the PIX
	 * provider is always sending information when enabled.
	 */
	virtual UBOOL IsAlwaysOn()
	{
		return FALSE;
	}

	/**
	 * Sets the current frame number we are processing
	 *
	 * @param FrameNumber the new frame number being processed
	 */
	virtual void SetFrameNumber(DWORD FrameNumber)
	{
		CurrentFrame = FrameNumber;
	}

	/**
	 * Start capturing stat file
	 */
	virtual void StartWritingStatsFile()
	{
		// Base implementation does not support this
	}

	/**
	 * Stop capturing stat file
	 */
	virtual void StopWritingStatsFile()
	{
		// Base implementation does not support this
	}

	/**
	 * Returns whether this provider is currently enabled or not
	 */
	inline UBOOL IsEnabled(void)
	{
		return bIsEnabled;
	}

	/**
	 * Returns the name of the provider
	 */
	inline const TCHAR* GetProviderName(void)
	{
		return ProviderName;
	}
};

/**
 * This class handles sending a stat event to zero or more stat providers. The
 * stat providers are registered via factory classes.
 */
class FStatNotifyManager : public FStatNotifyProvider
{
	/**
	 * Holds the list of providers that are registered to 
	 */
	TArray<FStatNotifyProvider*> Providers;

public:
	/**
	 * Simple constructor
	 */
	FStatNotifyManager(void) :
		FStatNotifyProvider(TEXT("StatNotifyManager"),NULL)
	{
	}
	/**
	 * Deletes all providers upon destruction
	 */
	~FStatNotifyManager(void)
	{
		Destroy();
	}

	/**
	 * Uses the list of factory classes to create the list of providers
	 * that this manager will forward calls to
	 */
	virtual UBOOL Init(void);
	/**
	 * Works through the list of providers destroying each in turn
	 */
	virtual void Destroy(void);

	/**
	 * Tells the provider that we are starting to supply it with descriptions
	 * for all of the stats/groups.
	 */
	virtual void StartDescriptions(void);

	/**
	 * Tells the provider that we are finished sending descriptions for all of
	 * the stats/groups.
	 */
	virtual void EndDescriptions(void);

	/**
	 * Tells the provider that we are starting to supply it with group descriptions
	 */
	virtual void StartGroupDescriptions(void);

	/**
	 * Tells the provider that we are finished sending stat descriptions
	 */
	virtual void EndGroupDescriptions(void);

	/**
	 * Tells the provider that we are starting to supply it with stat descriptions
	 */
	virtual void StartStatDescriptions(void);

	/**
	 * Tells the provider that we are finished sending group descriptions
	 */
	virtual void EndStatDescriptions(void);

	/**
	 * Adds a stat to the list of descriptions. Used to allow custom stats to
	 * report who they are, parentage, etc. Prevents applications that consume
	 * the stats data from having to change when stats information changes
	 *
	 * @param StatId the id of the stat
	 * @param StatName the name of the stat
	 * @param ParentId the parent for hierarchical stats
	 * @param StatType the type of stat this is
	 * @param GroupId the id of the group this stat belongs to
	 */
	virtual void AddStatDescription(DWORD StatId,const TCHAR* StatName,DWORD StatType,DWORD GroupId);

	/**
	 * Adds a group to the list of descriptions
	 *
	 * @param GroupId the id of the group being added
	 * @param GroupName the name of the group
	 */
	virtual void AddGroupDescription(DWORD GroupId,const TCHAR* GroupName);

	/**
	 * Function to write the stat out to the provider's data store
	 *
	 * @param StatId the id of the stat that is being written out
	 * @param GroupId the id of the group the stat belongs to
	 * @param ParentId the id of parent stat
	 * @param InstanceId the instance id of the stat being written
	 * @param ParentInstanceId the instance id of parent stat
	 * @param ThreadId the thread this stat is for
	 * @param Value the value of the stat to write out
	 * @param CallsPerFrame the number of calls for this frame
	 */
	virtual void WriteStat(DWORD StatId,DWORD GroupId,DWORD ParentId,DWORD InstanceId,
		DWORD ParentInstanceId,DWORD ThreadId,DWORD Value,
		DWORD CallsPerFrame);

	/**
	 * Function to write the stat out to the provider's data store
	 *
	 * @param StatId the id of the stat that is being written out
	 * @param GroupId the id of the group the stat belongs to
	 * @param Value the value of the stat to write out
	 */
	virtual void WriteStat(DWORD StatId,DWORD GroupId,FLOAT Value);

	/**
	 * Function to write the stat out to the provider's data store
	 *
	 * @param StatId the id of the stat that is being written out
	 * @param GroupId the id of the group the stat belongs to
	 * @param Value the value of the stat to write out
	 */
	virtual void WriteStat(DWORD StatId,DWORD GroupId,DWORD Value);

	/**
	 * Passes the current frame number on to all providers
	 *
	 * @param FrameNumber the new frame number being processed
	 */
	virtual void SetFrameNumber(DWORD FrameNumber);

	/**
	 * Start capturing stat file
	 */
	virtual void StartWritingStatsFile();

	/**
	 * Stop capturing stat file
	 */
	virtual void StopWritingStatsFile();
};

/**
 * Factory class for various provider types
 */
struct FStatNotifyProviderFactory
{
	/**
	 * The next factory in the list
	 */
	const FStatNotifyProviderFactory* NextFactory;
	/**
	 * The global list of factories
	 */
	static FStatNotifyProviderFactory* FirstFactory;

	/**
	 * Constructor that inserts this instance into the list of factories
	 */
	FORCENOINLINE FStatNotifyProviderFactory(void) :
		NextFactory(FirstFactory)
	{
		FirstFactory = this;
	}

	/**
	 * Used to create an instance of the specific factory. Must be implemented
	 * by child classes
	 */
	virtual FStatNotifyProvider* CreateInstance(void) const = 0;
};

#if _MSC_VER
#define FACTORY_EXPORT __declspec(dllexport)
#else
#define FACTORY_EXPORT
#endif

/**
 * Macro that creates a new factory class and declares an instance of it
 */
#define DECLARE_STAT_NOTIFY_PROVIDER_FACTORY(FactoryName,ProviderName,InstanceName) \
	struct FactoryName : FStatNotifyProviderFactory \
	{ \
		virtual FStatNotifyProvider* CreateInstance(void) const \
		{ \
			return new ProviderName(); \
		} \
	}; \
	FACTORY_EXPORT FactoryName InstanceName

#endif
