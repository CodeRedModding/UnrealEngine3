/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#if WITH_UE3_NETWORKING

protected:
	/**
	 * Handles updating of any async tasks that need to be performed
	 *
	 * @param DeltaTime the amount of time that has passed since the last tick
	 */
	virtual void Tick(FLOAT DeltaTime);

	/**
	 * Checks any queued async tasks for status, allows the task a change
	 * to process the results, and fires off their delegates if the task is
	 * complete
	 */
	void TickAsyncTasks(void);

	/**
	 * Determines whether the user's profile file exists or not
	 */
	UBOOL DoesProfileExist(void);

	/**
	 * Builds a file name for the user's profile data
	 */
	inline FString CreateProfileName(void)
	{
		// Use the player nick name to generate a profile name
		return ProfileDataDirectory * LoggedInPlayerName + ProfileDataExtension;
	}

public:

#endif	//#if WITH_UE3_NETWORKING
