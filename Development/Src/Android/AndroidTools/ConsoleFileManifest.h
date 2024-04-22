/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class FConsoleFileManifest
{
public:
	//default constructor
	FConsoleFileManifest();

	/**
	 * Reads the manifest from the device and parses the data within
	 * If there is no manifest, or the file was made from a different machine, the entire manifest is emptied
	 * Emptying will result in a full copy
	 */
	void PullFromDevice(const char* AndroidRoot);

	/**
	 * Packages up the manifest in memory and pushes it to the target device
	 */
	void Push(const char* AndroidRoot);

	/**
	 * Returns the number of seconds for the timeout
	 * @param InFileName - The name of the file to be considered for copying
	 */
	int ShouldCopyFile(const wchar_t* InFilename);

	/**
	 * Updates the file's representation in the manifest, adding or amending as needed
	 */
	void UpdateFileStatusInLog(const wchar_t* InFilename);


private:
	struct ManifestEntry
	{
		wstring Name;
		QWORD TimeStamp;
		QWORD Size;
	};

	/**
	 * Reads the local manifest file into our internal format.
	 */
	void ReadManifestFromLocalFile();

	/**
	 * Writes the local manifest file from our internal format.
	 */
	void WriteManifestToLocalFile();

	//All file entries
	vector<ManifestEntry> ManifestEntries;
};