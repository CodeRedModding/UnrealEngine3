/*=============================================================================
	FConfigCacheIni.h: Unreal config file reading/writing.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Config cache.
-----------------------------------------------------------------------------*/

#ifndef INC_CONFIGCACHEINI
#define INC_CONFIGCACHEINI

typedef TMultiMap<FName,FString> FConfigSectionMap;

// One section in a config file.
class FConfigSection : public FConfigSectionMap
{
public:
	UBOOL HasQuotes( const FString& Test ) const;
	UBOOL operator==( const FConfigSection& Other ) const;
	UBOOL operator!=( const FConfigSection& Other ) const;
};

// One config file.
class FConfigFile : public TMap<FString,FConfigSection>
{
public:
	UBOOL Dirty, NoSave, Quotes;
	
	FConfigFile();
	
	UBOOL operator==( const FConfigFile& Other ) const;
	UBOOL operator!=( const FConfigFile& Other ) const;

	UBOOL Combine( const TCHAR* Filename);
	void CombineFromBuffer(const TCHAR* Filename,const FString& Buffer);
	void Read( const TCHAR* Filename );
	UBOOL Write( const TCHAR* Filename );
	void Dump(FOutputDevice& Ar);

	UBOOL GetString( const TCHAR* Section, const TCHAR* Key, FString& Value );
	UBOOL GetDouble( const TCHAR* Section, const TCHAR* Key, DOUBLE& Value );

	void SetString( const TCHAR* Section, const TCHAR* Key, const TCHAR* Value );
	void SetDouble( const TCHAR* Section, const TCHAR* Key, const DOUBLE Value );
	
	/**
	 * Process the contents of an .ini file that has been read into an FString
	 * 
	 * @param Filename Name of the .ini file the contents came from
	 * @param Contents Contents of the .ini file
	 */
	void ProcessInputFileContents(const TCHAR* Filename, FString& Contents);


	/** Adds any properties that exist in InSourceFile that this config file is missing */
	void AddMissingProperties( const FConfigFile& InSourceFile );

	/**
	 * Looks for any overrides on the commandline for this file
	 * 
	 * @param Filename Name of the .ini file to look for overrides
	 */
	void OverrideFromCommandline(const FFilename& Filename);
};


// Set of all cached config files.
class FConfigCacheIni : public TMap<FFilename,FConfigFile>
{
public:
	// Basic functions.
	FConfigCacheIni();
	~FConfigCacheIni();

	/**
	* Disables any file IO by the config cache system
	*/
	virtual void DisableFileOperations();

	/**
	* Re-enables file IO by the config cache system
	*/
	virtual void EnableFileOperations();

	/**
	 * Returns whether or not file operations are disabled
	 */
	virtual UBOOL AreFileOperationsDisabled();

	/**
	 * Coalesces .ini and localization files into single files.
	 * DOES NOT use the config cache in memory, rather it reads all files from disk,
	 * so it would be a static function if it wasn't virtual
	 *
	 * @param ConfigDir				The base directory to search for .ini files
	 * @param OutputDirectory		The directory to save the output file in
	 * @param bNeedsByteSwapping	TRUE if the output file is destined for a platform that expects byte swapped data
	 * @param IniFileWithFilters	Name of ini file to look in for the list of files to filter out
	 * @param GlobalLanguageCaches	If supplied, the function should fill in the language-specific caches here...
	 * @param PlatformString		The string of the console being cooked for.
	 * @param bCurrentLanguageOnly	If TRUE, only generate the coalesced files for the current INI
	 * @param LanguageMask			if non-zero only languages with corresponding bits set will be generated
	 */
	virtual void CoalesceFilesFromDisk(const TCHAR* ConfigDir, const TCHAR* OutputDirectory, UBOOL bNeedsByteSwapping, 
		const TCHAR* IniFileWithFilters, const TCHAR* PlatformString, UBOOL bCurrentLanguageOnly,DWORD LanguageMask);

	/**
	 * Reads a coalesced file, breaks it up, and adds the contents to the config cache. Can
	 * load .ini or locailzation file (see ConfigDir description)
	 *
	 * @param ConfigDir If loading ini a file, then this is the path to load from, otherwise if loading a localizaton file, 
	 *                  this MUST be NULL, and the current language is loaded
	 */
	virtual void LoadCoalescedFile(const TCHAR* CoalescedFilename);

	/**
	* Prases apart an ini section that contains a list of 1-to-N mappings of strings in the following format
	*	 [PerMapPackages]
	*	 MapName=Map1
	*	 Package=PackageA
	*	 Package=PackageB
	*	 MapName=Map2
	*	 Package=PackageC
	*	 Package=PackageD
	* 
	* @param Section Name of section to look in
	* @param KeyOne Key to use for the 1 in the 1-to-N (MapName in the above example)
	* @param KeyN Key to use for the N in the 1-to-N (Package in the above example)
	* @param OutMap Map containing parsed results
	* @param Filename Filename to use to find the section
	*
	* NOTE: The function naming is weird because you can't apparently have an overridden function differnt only by template type params
	*/
	virtual void Parse1ToNSectionOfStrings(const TCHAR* Section, const TCHAR* KeyOne, const TCHAR* KeyN, TMap<FString, TArray<FString> >& OutMap, const TCHAR* Filename);

	/**
	* Prases apart an ini section that contains a list of 1-to-N mappings of names in the following format
	*	 [PerMapPackages]
	*	 MapName=Map1
	*	 Package=PackageA
	*	 Package=PackageB
	*	 MapName=Map2
	*	 Package=PackageC
	*	 Package=PackageD
	* 
	* @param Section Name of section to look in
	* @param KeyOne Key to use for the 1 in the 1-to-N (MapName in the above example)
	* @param KeyN Key to use for the N in the 1-to-N (Package in the above example)
	* @param OutMap Map containing parsed results
	* @param Filename Filename to use to find the section
	*
	* NOTE: The function naming is weird because you can't apparently have an overridden function differnt only by template type params
	*/
	virtual void Parse1ToNSectionOfNames(const TCHAR* Section, const TCHAR* KeyOne, const TCHAR* KeyN, TMap<FName, TArray<FName> >& OutMap, const TCHAR* Filename);

	FConfigFile* FindConfigFile( const TCHAR* Filename );
	FConfigFile* Find( const TCHAR* InFilename, UBOOL CreateIfNotFound );
	void Flush( UBOOL Read, const TCHAR* Filename=NULL );

	void LoadFile( const TCHAR* InFilename, const FConfigFile* Fallback = NULL, const TCHAR* PlatformString = NULL );
	void SetFile( const TCHAR* InFilename, const FConfigFile* NewConfigFile );
	void UnloadFile( const TCHAR* Filename );
	void Detach( const TCHAR* Filename );

	UBOOL GetString( const TCHAR* Section, const TCHAR* Key, FString& Value, const TCHAR* Filename );
	UBOOL GetSection( const TCHAR* Section, TArray<FString>& Result, const TCHAR* Filename );
	FConfigSection* GetSectionPrivate( const TCHAR* Section, UBOOL Force, UBOOL Const, const TCHAR* Filename );
	void SetString( const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const TCHAR* Filename );
	void EmptySection( const TCHAR* Section, const TCHAR* Filename );
	void EmptySectionsMatchingString( const TCHAR* SectionString, const TCHAR* Filename );

	/**
	 * Retrieve a list of all of the config files stored in the cache
	 *
	 * @param ConfigFilenames Out array to receive the list of filenames
	 */
	void GetConfigFilenames(TArray<FFilename>& ConfigFilenames);

	/**
	 * Retrieve the names for all sections contained in the file specified by Filename
	 *
	 * @param	Filename			the file to retrieve section names from
	 * @param	out_SectionNames	will receive the list of section names
	 *
	 * @return	TRUE if the file specified was successfully found;
	 */
	UBOOL GetSectionNames( const TCHAR* Filename, TArray<FString>& out_SectionNames );

	/**
	 * Retrieve the names of sections which contain data for the specified PerObjectConfig class.
	 *
	 * @param	Filename			the file to retrieve section names from
	 * @param	SearchClass			the name of the PerObjectConfig class to retrieve sections for.
	 * @param	out_SectionNames	will receive the list of section names that correspond to PerObjectConfig sections of the specified class
	 * @param	MaxResults			the maximum number of section names to retrieve
	 *
	 * @return	TRUE if the file specified was found and it contained at least 1 section for the specified class
	 */
	UBOOL GetPerObjectConfigSections( const TCHAR* Filename, const FString& SearchClass, TArray<FString>& out_SectionNames, INT MaxResults=1024 );

	void Exit();
	void Dump( FOutputDevice& Ar );

	/**
	 * Dumps memory stats for each file in the config cache to the specified archive.
	 *
	 * @param	Ar	the output device to dump the results to
	 */
	virtual void ShowMemoryUsage( FOutputDevice& Ar );

	/**
	 * USed to get the max memory usage for the FConfigCacheIni
	 *
	 * @return the amount of memory in byes
	 */
	virtual SIZE_T GetMaxMemoryUsage();


	// Derived functions.
	FString GetStr
	(
		const TCHAR* Section, 
		const TCHAR* Key, 
		const TCHAR* Filename 
	);
	UBOOL GetInt
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		INT&			Value,
		const TCHAR*	Filename
	);
	UBOOL GetFloat
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		FLOAT&			Value,
		const TCHAR*	Filename
	);
	UBOOL GetDouble
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		DOUBLE&			Value,
		const TCHAR*	Filename
	);
	UBOOL GetBool
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		UBOOL&			Value,
		const TCHAR*	Filename
	);
	INT GetArray
	(
		const TCHAR* Section,
		const TCHAR* Key,
		TArray<FString>& out_Arr,
		const TCHAR* Filename/* =NULL  */
	);
	/** Loads a "delimited" list of strings
	 * @param Section - Section of the ini file to load from
	 * @param Key - The key in the section of the ini file to load
	 * @param out_Arr - Array to load into
	 * @param Filename - Ini file to load from
	 */
	INT GetSingleLineArray
	(
		const TCHAR* Section,
		const TCHAR* Key,
		TArray<FString>& out_Arr,
		const TCHAR* Filename
	);
	UBOOL GetColor
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		FColor&			Value,
		const TCHAR*	Filename
	);
	UBOOL GetVector
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		FVector&			Value,
		const TCHAR*	Filename
	);
	UBOOL GetRotator
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		FRotator&			Value,
		const TCHAR*	Filename
	);

	void SetInt
	(
		const TCHAR* Section,
		const TCHAR* Key,
		INT			 Value,
		const TCHAR* Filename
	);
	void SetFloat
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		FLOAT			Value,
		const TCHAR*	Filename
	);
	void SetDouble
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		DOUBLE			Value,
		const TCHAR*	Filename
	);
	void SetBool
	(
		const TCHAR* Section,
		const TCHAR* Key,
		UBOOL		 Value,
		const TCHAR* Filename
	);
	void SetArray
	(
		const TCHAR* Section,
		const TCHAR* Key,
		const TArray<FString>& Value,
		const TCHAR* Filename /* = NULL  */
	);
	/** Saves a "delimited" list of strings
	 * @param Section - Section of the ini file to save to
	 * @param Key - The key in the section of the ini file to save
	 * @param out_Arr - Array to save from
	 * @param Filename - Ini file to save to
	 */
	void SetSingleLineArray
	(
		const TCHAR* Section,
		const TCHAR* Key,
		const TArray<FString>& In_Arr,
		const TCHAR* Filename/* =NULL  */
	);
	void SetColor
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		FColor			Value,
		const TCHAR*	Filename
	);
	void SetVector
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		FVector			Value,
		const TCHAR*	Filename
	);
	void SetRotator
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		FRotator			Value,
		const TCHAR*	Filename
	);

	// Static allocator.
	static FConfigCacheIni* Factory();

private:
	/** TRUE if file operations should not be performed */
	UBOOL bAreFileOperationsDisabled;
};

#endif



