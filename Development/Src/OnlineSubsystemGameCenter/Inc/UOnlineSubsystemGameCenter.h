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
	/** 
	 * Convert a property id (aka category) into the category name that should
	 * match the name specified in iTunes Connect (see UniqueCategoryPrefix)
	 *
	 * @param PropertyId Identifier for the property to write
	 *
	 * @return String name of a category
	 */
	FString GetCategoryNameFromPropertyId(INT PropertyId);

	/** 
	 * Convert an iTunes Connect category name into the UE3 identifier (see
	 * UniqueCategoryPrefix)
	 *
	 * @param CategoryName iTunes Connect catgegory name
	 *
	 * @return UE3 property id, or -1 if the name is bad
	 */
	INT GetPropertyIdFromCategoryName(const FString& CategoryName);

	/** 
	 * Convert an achievement identifier into the achievement name that should
	 * match the name specified in iTunes Connect (see UniqueAchievementPrefix)
	 *
	 * @param AchievementId UE3 achievement id
	 *
	 * @return String name of a achievement 
	 */
	FString GetAchievementNameFromId(INT AchievementId);

	/** 
	 * Convert an iTunes Connect achievement name into the UE3 identifier (see
	 * UniqueAchievementPrefix)
	 *
	 * @param AchievementName iTunes Connect achievement name
	 *
	 * @return UE3 achievement id, or -1 if the name is bad
	 */
	INT GetAchievementIdFromName(const FString& AchievementName);

#endif	//#if WITH_UE3_NETWORKING
