/*=============================================================================
	FileHelper.h: Classes for loading/saving files

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

class UUnrealEdEngine;

/*-----------------------------------------------------------------------------
	FileHelper.

	Base class for all file helpers.
-----------------------------------------------------------------------------*/

enum EFileHelperIdx
{
	FHI_Load,
	FHI_Save,
	FHI_Import,
	FHI_Export,
	FHI_MAX,
};

class FFileHelper
{
public:

	FFileHelper();
	~FFileHelper();

	void AskSaveChanges();
	void CheckSaveAs();

	// Entry functions that determine the state of the file and display dialogs/call the worker functions.

	void Load();
	void Import( UBOOL InFlag1 );
	void SaveAs();
	void Save();
	void Export( UBOOL InFlag1 );

	virtual FString GetFilter( EFileHelperIdx InIdx );

	// Worker functions that actually perform the loading/saving

	virtual void LoadFile( FFilename InFilename )=0;
	virtual void ImportFile( FFilename InFilename, UBOOL InFlag1 )=0;
	virtual void SaveFile( FFilename InFilename )=0;
	virtual void ExportFile( FFilename InFilename, UBOOL InFlag1 )=0;
	virtual void NewFile()=0;
	
	// Utility functions that gather information about the file

	virtual UBOOL IsDirty()=0;
	virtual FString GetDefaultDirectory()=0;
	virtual FFilename GetFilename()=0;

	FString Filters[FHI_MAX];
};

/*-----------------------------------------------------------------------------
	FileHelperMap.

	For saving map files through the main editor frame.
-----------------------------------------------------------------------------*/

class FFileHelperMap : public FFileHelper
{
public:

	FFileHelperMap();
	~FFileHelperMap();

	virtual FString GetFilter( EFileHelperIdx InIdx );
	virtual void LoadFile( FFilename InFilename );
	virtual void ImportFile( FFilename InFilename, UBOOL InFlag1 );
	virtual void SaveFile( FFilename InFilename );
	virtual void ExportFile( FFilename InFilename, UBOOL InFlag1 );
	virtual void NewFile();
	virtual UBOOL IsDirty();
	virtual FString GetDefaultDirectory();
	virtual FFilename GetFilename();
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
