/*=============================================================================
	UnDownload.h: Unreal file-download interface
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

class UDownload : public UObject
{
	DECLARE_ABSTRACT_CLASS_INTRINSIC(UDownload,UObject,CLASS_Transient|CLASS_Config|0,Engine);
    NO_DEFAULT_CONSTRUCTOR(UDownload);

	// Variables.
	UNetConnection* Connection;			// Connection
	INT				PackageIndex;		// Index of package in Map.
	FPackageInfo*	Info;				// Package Info
	FString			DownloadParams;		// Download params sent to the client.
	UBOOL			UseCompression;		// Send compressed files to the client.
	FArchive*		RecvFileAr;			// File being received.
	TCHAR			TempFilename[MAX_SPRINTF];	// Filename being transfered.
	TCHAR			Error[256];			// A download error occurred.
	INT				Transfered;			// Bytes transfered.
	INT				FileSize;			// Size of the file being downloaded
	UBOOL			SkippedFile;		// File was skipped.
	UBOOL			IsCompressed;		// Use file compression.

	/** If this is true, the first 4 bytes received is the filesize, followed by the actual file */
	UBOOL			bDownloadSendsFileSizeInData;

	// Constructors.
	void StaticConstructor();
	virtual void FinishDestroy();

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UDownload Interface.
	virtual UBOOL TrySkipFile();
	virtual void ReceiveFile( UNetConnection* InConnection, INT PackageIndex, const TCHAR *Params=NULL, UBOOL InCompression=0 );
	virtual void ReceiveData( BYTE* Data, INT Count );
	virtual void Tick() {} 
	virtual void DownloadError( const TCHAR* Error );
	virtual void DownloadDone();
	/** cleans up structures and prepares for deletion */
	void CleanUp();
};

class UChannelDownload : public UDownload
{
	DECLARE_CLASS_INTRINSIC(UChannelDownload,UDownload,CLASS_Transient|CLASS_Config,Engine);
	
	// Variables.
	UFileChannel* Ch;

	// Constructors.
	void StaticConstructor();
	UChannelDownload();

	// UObject interface.
	void FinishDestroy();
	void Serialize( FArchive& Ar );

	// UDownload Interface.
	void ReceiveFile( UNetConnection* InConnection, INT PackageIndex, const TCHAR *Params=NULL, UBOOL InCompression=0 );
	UBOOL TrySkipFile();
};

