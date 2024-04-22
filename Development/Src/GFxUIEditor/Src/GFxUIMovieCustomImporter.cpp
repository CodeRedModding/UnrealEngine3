/**********************************************************************

Filename    :   GFxUIMovieCustomImporter.cpp
Content     :   Importer used to add GFX/SWF files into Unreal package

Copyright   :   (c) 2001-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :   

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING 
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

#include "GFxUIEditor.h"

#if WITH_GFx

#include "ScaleformEngine.h"
#include "SourceControl.h"

#include "Factories.h"
#include "PackageHelperFunctions.h"

#include "zlib.h"

IMPLEMENT_CLASS(UGFxReimportCommandlet);
IMPLEMENT_CLASS(UGFxImportCommandlet);

#if WITH_GFx

UObject* UGFxMovieFactory::CreateMovieGFxExport(	UObject* InParent,
                                                    FName Name,
													const FString& OriginalSwfPath,
                                                    EObjectFlags Flags,
                                                    const BYTE* Buffer, const BYTE* BufferEnd,
                                                    const FString& GFxExportCmdline,
                                                    FFeedbackContext* Warn)
{
    FString strTempPath =  appConvertRelativePathToFull(appGameDir() + TEXT("GFxExportTemp"));
    FString strSwfFile = strTempPath + PATH_SEPARATOR + Name.ToString() + TEXT(".swf");

	if (!DeleteFolder(strTempPath))
	{
		return NULL;
	}
    if (!GFileManager->MakeDirectory(*strTempPath, TRUE))
	{
        return NULL;
	}

	// write the swf data so it can be processed with GFxExport.
    FArchive* Ar = GFileManager->CreateFileWriter( *strSwfFile, 0, GNull, BufferEnd-Buffer);
    if( !Ar )
	{
        return NULL;
	}
    Ar->Serialize( const_cast<BYTE*>(Buffer), BufferEnd - Buffer );
    delete Ar;

	FString GfxExportArgs = GFxExportCmdline;
	GfxExportArgs += TEXT(" \"");
	GfxExportArgs += strSwfFile;
	GfxExportArgs += TEXT("\" -d \"");
	GfxExportArgs += strTempPath;
	GfxExportArgs += TEXT("\" -replace_images -id \"");
	const FString OriginalResourceDir = GetOriginalResourceDir(OriginalSwfPath);
	GfxExportArgs += OriginalResourceDir;
	GfxExportArgs += TEXT("\"");

	FString GfxExportErrors;

	if (!RunGFXExport( GfxExportArgs, &GfxExportErrors ))
	{
		return NULL;
	}

	UObject* Result = BuildPackage(strTempPath, strSwfFile, OriginalSwfPath, Name, InParent, Flags, Warn);

	DeleteFolder(strTempPath);

	if ( GfxExportErrors.Len() > 0 )
	{
		FString WarningMessage = FString::Printf( TEXT("Errors while importing '%s': \n\n"), *(FFilename(strSwfFile).GetCleanFilename()) );

		WarningMessage += GfxExportErrors;

		WarningMessage += FString::Printf(TEXT("\nNOTE: Using output of gfxexport for missing files!\n"));

		Warn->Logf( *WarningMessage );

		if (!GIsUCC)
		{
			WxModelessPrompt* WarningPrompt = new WxModelessPrompt( *WarningMessage, LocalizeUnrealEd("Warning_SwfArtImportErrors_Title") );
			WarningPrompt->Show();
		}	
	}

    return Result;
}

UBOOL UGFxMovieFactory::RunGFXExport( const FString& strCmdLineParams, FString* OutGfxExportErrors )
{
	*OutGfxExportErrors = FString(TEXT(""));
	const FString strGFXExportToolFullPath( appConvertRelativePathToFull( TEXT( "..\\GFx\\gfxexport.exe" ) ) );
	
#ifdef WIN32
	// Require Win32 for this bit for extra control.

	// Win32 requires a modifiable buffer for cmd line prams.
	INT charCount = strCmdLineParams.Len() + 1;
	TCHAR* pTempCmdLine = new TCHAR[charCount];
	memcpy(pTempCmdLine, *strCmdLineParams, charCount * sizeof(TCHAR));

	class FGfxExportScopeGuard
	{
	public:
		FGfxExportScopeGuard( TCHAR* InCommandLine, HANDLE InTempFileHandle )
			: CommandLine(InCommandLine)
			, TempFileHandle( InTempFileHandle )
		{}

		~FGfxExportScopeGuard()
		{
			delete CommandLine;
			CloseHandle(TempFileHandle);
		}
	private:
		TCHAR* CommandLine;
		HANDLE TempFileHandle;
	};

	PROCESS_INFORMATION pi;
	::ZeroMemory(&pi, sizeof(pi));

	// Temporary file for getting stderror from gfxexport.
	FString GfxExportErrorsFilename = appConvertRelativePathToFull( TEXT(".\\GfxExportErrors.txt") );
	SECURITY_ATTRIBUTES FileSecurityAttributes;
	FileSecurityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
	FileSecurityAttributes.lpSecurityDescriptor = NULL;
	FileSecurityAttributes.bInheritHandle = TRUE; // bInheritHandle must be TRUE so that stderror can be redirected here.
	HANDLE FileHandle = CreateFile( *GfxExportErrorsFilename, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, &FileSecurityAttributes, CREATE_ALWAYS, 0, 0 );

	{
		FGfxExportScopeGuard GfxExportScopeGuard( pTempCmdLine, FileHandle );

		STARTUPINFO sui;
		::ZeroMemory(&sui, sizeof(sui));
		sui.cb = sizeof(sui);
		sui.dwFlags |= STARTF_USESTDHANDLES;
		sui.hStdError = FileHandle;

		debugf( *FString::Printf( TEXT("Running: \"%s\" %s."), *strGFXExportToolFullPath, pTempCmdLine )  );
		if (CreateProcess(*strGFXExportToolFullPath, pTempCmdLine, NULL, NULL, TRUE, 0,
			NULL, /* *strSourcePath*/ NULL, &sui, &pi))
		{
			// Wait for GFXExport to finish
			::WaitForSingleObject(pi.hProcess, INFINITE);

			::CloseHandle(pi.hProcess);
			::CloseHandle(pi.hThread);
		}
		else
		{
			appMsgf(AMT_OK, TEXT("Cannot run %s."), *strGFXExportToolFullPath);
			return FALSE;
		}

#else // !WIN32
	appCreateProc(*strGFXExportToolFullPath, *strCmdLineParams);
#endif //WIN32
	}
	// Read in the stderr dump from GfxExport so we can report any errors..
	appLoadFileToString( *OutGfxExportErrors, *GfxExportErrorsFilename );
	return TRUE;
}

UObject* UGFxMovieFactory::BuildPackage(const FString& strInputDataFolder, const FString& strSwfFile, const FString& OriginalSwfLocation,
                                        const FName& Name, UObject* InOuter, EObjectFlags Flags, FFeedbackContext* Warn)
{
    TArray<BYTE> Data;
    if( !appLoadFileToArray( Data, *strSwfFile.Replace(TEXT(".swf"), TEXT(".gfx"), TRUE) ) )
	{
        return NULL;
	}

    USwfMovie* SwfMovie = ConstructObject<USwfMovie>(USwfMovie::StaticClass(), InOuter, Name, Flags);
    SwfMovie->SetRawData(&Data(0), Data.Num());

	TArray<UFactory*> Factories;
	GetAllFactories(Factories);

	// Get a directory listing
	TArray<FString> fileList;
	FString fileMask = strInputDataFolder + FString("\\*.*");
	GFileManager->FindFiles(fileList, *fileMask, true, false);

    Warn->BeginSlowTask(*LocalizeUnrealEd("Importing"), TRUE);

	TArray<FString> ImportErrors;

    // Find a factory for each file in the list- if one exists, import the file
	for (UINT checkingFile = 0; checkingFile < (UINT)fileList.Num(); ++checkingFile)
	{
		FFilename ResourceFile;
		UFactory* FoundFactory;
		UBOOL bShouldImport = FALSE;
		{
			FFilename checkFilename = strInputDataFolder + PATH_SEPARATOR + fileList(checkingFile);
			
			// Check if this resource is an image? (we use factories to do this)
			FoundFactory = FindMatchingFactory(Factories, checkFilename.GetExtension());
			if ( FoundFactory != NULL && NULL == Cast<UGFxMovieFactory>(FoundFactory) )
			{
				// OK, we know how to import this resource file and it isn't a GFxMovie.
				ResourceFile = checkFilename;
				bShouldImport = TRUE;
			}
			else
			{
				ResourceFile = FFilename(TEXT(""));
				FoundFactory = NULL;
				bShouldImport = FALSE;
			}
		}


		// Modify object names with "."s in them to use "_"s instead.
		FString objectName = ResourceFile.GetBaseFilename().Replace(TEXT("."), TEXT("_"));

		if ( bShouldImport )
		{
            if (Cast<UTextureFactory>(FoundFactory))
			{
                UTextureFactory::SuppressImportOverwriteDialog();
				UTextureFactory::SuppressImportResolutionWarningDialog();
			}

            Warn->StatusUpdatef( checkingFile, fileList.Num(), *FString::Printf(LocalizeSecure(LocalizeUnrealEd("Importingf"), checkingFile, fileList.Num())) );

            GWarn->Logf(TEXT("Importing \"%s\""), *(ResourceFile.GetCleanFilename()));
            UObject* pObject = UFactory::StaticImportObject( FoundFactory->SupportedClass, InOuter, FName(*objectName), RF_Public, *ResourceFile, NULL, FoundFactory);
            if (pObject != NULL)
            {
                SwfMovie->References.Push(pObject);

                if (pObject->IsA(UTexture2D::StaticClass()))
				{
                    FixupTextureImport((UTexture2D*)pObject, ResourceFile);
				}
            }
			else
			{
				ImportErrors.AddItem( FString::Printf(TEXT("Failed importing %s. See log for details."), *ResourceFile) );
			}
		}
		else
		{
			GWarn->Logf(TEXT("Skipping  \"%s\""), *(ResourceFile.GetCleanFilename()));
		}
	}
    Warn->EndSlowTask();

	// The original art is possibly missing. Make sure that we find it.
	if ( ImportErrors.Num() > 0 )
	{
		FString WarningMessage = FString::Printf( TEXT("Warnings while importing '%s': \n\n"), *(FFilename(strSwfFile).GetCleanFilename()) );
		

		for( TArray<FString>::TConstIterator ErrorIt(ImportErrors); ErrorIt; ++ErrorIt )
		{
			const FString& Error = *ErrorIt;
			WarningMessage += FString::Printf( TEXT("   %s \n"), *Error );
		}

		WarningMessage += FString::Printf(TEXT("\nNOTE: Using output of gfxexport for missing files!\n"));
		
		Warn->Logf( *WarningMessage );

		if (!GIsUCC)
		{
			WxModelessPrompt* WarningPrompt = new WxModelessPrompt( *WarningMessage, LocalizeUnrealEd("Warning_SwfArtImportErrors_Title") );
			WarningPrompt->Show();
		}
	}

    // Content Browser
    GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_ObjectCreated, SwfMovie));

	return SwfMovie;
}

UBOOL UGFxMovieFactory::DeleteFolder(const FString& strFolderPath)
{
	FString strWildcard = strFolderPath + TEXT("\\*");

	// Delete all files in the folder.
	TArray<FString> aFolders;
	GFileManager->FindFiles(aFolders, *strWildcard, FALSE, TRUE);

//	for (TArray<FString>::TConstIterator it(aFolders); it; ++it)
	for (TArray<FString>::TIterator it(aFolders); it; ++it)
	{
		FString strTempFolderPath = strFolderPath + FString(TEXT("\\")) + (*it);
		if (!DeleteFolder(*strTempFolderPath))
		{
			appMsgf(AMT_OK, TEXT("Can't delete folder \"%s\". Aborting import."), *strTempFolderPath);
			return FALSE;
		}
	}

	// Delete all files in the folder.
	TArray<FString> aFiles;
	GFileManager->FindFiles(aFiles, *strWildcard, TRUE, FALSE);
//	for (TArray<FString>::TConstIterator it(aFiles); it; ++it)
	for (TArray<FString>::TIterator it(aFiles); it; ++it)
	{
		FString strTempFilePath = strFolderPath + FString(TEXT("\\")) + (*it);
		if (!GFileManager->Delete(*strTempFilePath, TRUE, TRUE))
		{
			appMsgf(AMT_OK, TEXT("Can't delete file \"%s\". Aborting import."), *strTempFilePath);
			return FALSE;
		}
	}

	// Delete folder, don't require that it exists, only delete that folder.
	return GFileManager->DeleteDirectory(*strFolderPath, FALSE, FALSE);
}

void UGFxMovieFactory::GetAllFactories(TArray<UFactory*> &factories)
{
	for( TObjectIterator<UClass> it ; it ; ++it )
	{
		if( it->IsChildOf(UFactory::StaticClass()) && !(it->ClassFlags & CLASS_Abstract) )
		{
			UFactory* factory = ConstructObject<UFactory>( *it );
			factories.AddItem( factory );
		}
	}
}

UFactory *UGFxMovieFactory::FindMatchingFactory(TArray<UFactory*> &factories, const FString &fileExtension)
{
	for (UINT factoryIndex = 0 ; factoryIndex < (UINT)factories.Num(); ++factoryIndex )
	{
		UFactory *checkFactory = factories(factoryIndex);
		for (UINT formatIndex = 0; formatIndex < (UINT)checkFactory->Formats.Num(); ++formatIndex)
		{
			if (checkFactory->Formats(formatIndex).Left(fileExtension.Len()) == fileExtension )
			{
				return(checkFactory);
			}
		}
	}
	return(NULL);
}

/**
 * Attempt to locate the directory with original resources that were imported into the .FLA document.
 * Given the SwfLocation, GfxExport will search for original files in the SWF's sibling directory
 * with the same name as the SWF.
 * E.g. If we have a c:\Art\SWFName.swf and it uses SomePicture.TGA, then we tell GfxExport to 
 * look for c:\Art\SWFName\SomePicture.PNG.
 *
 * @param InSwfLocation The location of the SWF; original resources are searched relative to the SWF.
 *
 * @return A directory where the original images used in this SWF are found.
 */
FString UGFxMovieFactory::GetOriginalResourceDir( const FString& InSwfPath )
{
	FFilename SwfPath( appConvertRelativePathToFull(InSwfPath) );
	
	// The original art directory is SWF_DIRECTORY / SWF_NAME
	FString SwfDirectory = SwfPath.GetPath();
	FString Swfname = SwfPath.GetBaseFilename();


	FString OriginalArtDir = SwfDirectory + PATH_SEPARATOR + Swfname;
	return OriginalArtDir;
}

void UGFxMovieFactory::FixupTextureImport(UTexture2D* pTexture, const FString& /*strTextureFileName*/)
{
	// By default, we never stream, and we mark the texture group as part of the UI.
	pTexture->LODGroup = TEXTUREGROUP_UI;
    pTexture->SRGB = bSetSRGBOnImportedTextures;

	// We'll defer compression on imported swf movie textures by default to speed up workflow
	// when reimporting many textures at once
	pTexture->DeferCompression = TRUE;

    // trigger resource recreation but not recompression
    UProperty* Property = FindField<UProperty>(UTexture2D::StaticClass(), TEXT("SRGB"));

	FPropertyChangedEvent PropertyEvent(Property);
    pTexture->PostEditChangeProperty(PropertyEvent);
}


/** Examine SWF data for import tags; add movies referenced by import tags to References list for seekfree loading. */

/* from GFxUILocalization.cpp */
UBOOL LocalizeArray(TArray<FString>& Result, const TCHAR* Section, const TCHAR* Key, const TCHAR* Package=GPackage, const TCHAR* LangExt = NULL);

struct FCheckImports
{
    USwfMovie* MovieInfo;
    UBOOL          AddFonts;
};
static UBOOL FontLibWarned = 0;

/**
 * Translate this swf tag into a UE3 reference string, and add it to the list of references needed by this Swf.
 * Also attempt to resolve the "weak reference" into a real UE3 reference.
 */
static void CheckImportTag(const char* Name, FCheckImports* Params, TArray<FString>* OutMissingRefs)
{

#ifndef GFX_NO_LOCALIZATION
	FString NameFs (Name);
	const TCHAR* NameW = *NameFs;
	const TCHAR* p = appStristr(NameW, TEXT("gfxfontlib.swf"));
	if (p != NULL && (p == NameW || p[-1] == TEXT('\\') || p[-1] == TEXT('/')))
	{
		Params->MovieInfo->bUsesFontlib = TRUE;
		if (Params->AddFonts)
		{
			TArray<FString> FontLibFiles;
			if (LocalizeArray(FontLibFiles, TEXT("FontLib"), TEXT("FontLib"), TEXT("GFxUI")))
			{
				for(INT i=0; i<FontLibFiles.Num(); i++)
				{
					USwfMovie* Ref = LoadObject<USwfMovie>(NULL, *FontLibFiles(i), NULL, LOAD_None, NULL);
					if (Ref)
					{
						Params->MovieInfo->References.AddUniqueItem(Ref);
					}
					else if (!FontLibWarned)
					{
						warnf(NAME_Warning, TEXT("GFx FontLib references missing font %s"), *FontLibFiles(i));
					}
				}
			}

			FontLibWarned = TRUE;
		}
	}
	else
#endif // GFX_NO_LOCALIZATION
	{
		FFilename   strFilename;
		if (!FGFxEngine::GetPackagePath(Name, strFilename))
		{
			// The reference is a relative path
			// we must convert it to a UE3 fullpath for loading
			strFilename = Params->MovieInfo->GetOuter()->GetPathName();
			FGFxEngine::ReplaceCharsInFString(strFilename, TEXT("."), PATH_SEPARATOR[0]);
			strFilename += PATH_SEPARATOR;
			strFilename += Name;
			if (!appStricmp(*strFilename.GetExtension(), TEXT("gfx")) ||
				!appStricmp(*strFilename.GetExtension(), TEXT("swf")))
			{
				strFilename = strFilename.GetBaseFilename(FALSE);
			}
			strFilename = FGFxEngine::CollapseRelativePath(strFilename);
			FGFxEngine::ReplaceCharsInFString(strFilename, PATH_SEPARATOR, L'.');
		}
		
		// Attempt to find the UE3 resource for this import tag
		Params->MovieInfo->ReferencedSwfs.AddUniqueItem(strFilename);
		USwfMovie* Ref = LoadObject<USwfMovie>(NULL, *strFilename, NULL, LOAD_NoWarn|LOAD_Quiet, NULL);
		if (Ref)
		{
			Params->MovieInfo->References.AddUniqueItem(Ref);
		}
		else
		{
			OutMissingRefs->AddUniqueItem( strFilename );
			warnf(NAME_Warning, TEXT("GFxMovieInfo %s references missing import %s"), *Params->MovieInfo->GetFullName(), *strFilename);
		}
	}
}


struct SimpleSwfStream
{
    UBOOL         Zlib;
    BYTE*         Buffer;
    INT           BufSize;
    INT           Pos, End;
    UBOOL         AtEnd;
    z_stream      Stream;

    SimpleSwfStream(BYTE *in, INT ins, BYTE* zbuf, INT zbufsize)
    {
        Zlib = TRUE;
        AtEnd = FALSE;
        Buffer = zbuf;
        BufSize = zbufsize;
        memset(&Stream, 0, sizeof(Stream));
        if (Z_OK != inflateInit(&Stream))
            abort();
        Stream.next_in = in;
        Stream.avail_in = ins;
        Fill();
    }
    SimpleSwfStream(BYTE *in, INT ins)
    {
        Zlib = FALSE;
        Buffer = in;
        BufSize = ins;
        Pos = 0;
        End = BufSize;
        AtEnd = FALSE;
    }
    ~SimpleSwfStream()
    {
        if (Zlib)
            inflateEnd(&Stream);
    }

    void Fill()
    {
        if (!Zlib)
        {
            AtEnd = TRUE;
            Pos = 0;
            return;
        }
        Stream.next_out = Buffer;
        Stream.avail_out = BufSize;
        int ret = inflate(&Stream, Z_SYNC_FLUSH);
        check(ret == Z_OK || ret == Z_STREAM_END);
        Pos = 0;
        End = BufSize-Stream.avail_out;
        if (End == 0)
            AtEnd = TRUE;
    }
    BYTE read()
    {
        if (Pos == End)
            Fill();
        return Buffer[Pos++];
    }
    INT readU16()
    {
        INT value = read();
        value |= (read() << 8);
        return value;
    }
    INT readS32()
    { 
        INT value = read();
        value |= (read() << 8);
        value |= (read() << 16);
        value |= (read() << 24);
        return value;
    }

    void skip(INT len)
    {
        while (len > End-Pos && !AtEnd)
        {
            len -= End-Pos;
            Fill();
        }
        Pos += len;
    }
};

static BYTE* SkipRect(BYTE* p)
{
    unsigned rbits = *p >> 3;
    return p + ((5 + 4*rbits + 7) >> 3);
}

static UBOOL CheckSwfTag(SimpleSwfStream& S, FCheckImports* Params, TArray<FString>* OutMissingRefs)
{
    unsigned tag = S.readU16();
    unsigned len = tag & 0x3f;
    tag = tag >> 6;
    if (len == 0x3f)
        len = S.readS32();

    if (tag == 57 || tag == 71)
    {
        TArray<char> import;
        len--;
        BYTE c = S.read();
        while (c)
        {
            import.AddItem(c);
            c = S.read();
            len--;
        }
        import.AddItem(0);
        CheckImportTag(import.GetTypedData(), Params, OutMissingRefs);
		
    }
    else if (S.AtEnd || tag == 0 || len < 0)
        return FALSE;

    S.skip(len);
    return S.AtEnd ? FALSE : TRUE;
}

void UGFxMovieFactory::CheckImportTags(USwfMovie* SwfMovie, TArray<FString>* OutMissingRefs, UBOOL bAddFonts)
{
    // skip class default or stub
    if (SwfMovie->RawData.Num() < 8)
        return;

    FCheckImports Params;
    Params.MovieInfo = SwfMovie;
    Params.AddFonts = bAddFonts;

    BYTE *Data = SwfMovie->RawData.GetTypedData();
    if (Data[0] == 0x43 && 
        ((Data[1] == 0x57 && Data[2] == 0x53) || (Data[1] == 0x46 && Data[2] == 0x58)))
    {
        // compressed swf/gfx
        BYTE Buffer[4096];
        SimpleSwfStream Stream(Data+8, SwfMovie->RawData.Num()-8, Buffer, sizeof(Buffer));
        Stream.skip(SkipRect(Stream.Buffer+Stream.Pos) - Stream.Buffer);
        Stream.readS32();
        while (CheckSwfTag(Stream, &Params, OutMissingRefs));
    }
    else if ((Data[0] == 0x46 || Data[0] == 0x47) &&
        ((Data[1] == 0x57 && Data[2] == 0x53) || (Data[1] == 0x46 && Data[2] == 0x58)))
    {
        BYTE *p = SkipRect(Data+8)+4;
        SimpleSwfStream Stream(p, Data+SwfMovie->RawData.Num()-p);
        while (CheckSwfTag(Stream, &Params, OutMissingRefs));
    }
}

/** Return the Flash/ directory for this game. E.g. d:/UE3/UDKGame/Flash/ */
FString UGFxMovieFactory::GetGameFlashDir()
{
	return appConvertRelativePathToFull( appGameDir() + TEXT("Flash") + PATH_SEPARATOR ) ;
}

/** 
 * Given a path, ensure that this path uses only the approved PATH_SEPARATOR.
 * If this path is relative, the optional ./ at the beginning is removed.
 *
 * @param InPath Path to canonize
 *
 * @return The canonical copy of InPath.
 */
FString UGFxMovieFactory::SwfImportInfo::EnforceCanonicalPath(const FString& InPath)
{
	// Make a copy of InPath and ensure it only uses PATH_SEPARATOR.
	FString CanonicalPath = InPath;
	FPackageFileCache::NormalizePathSeparators( CanonicalPath );
	
	// Strip the optional ./ from the beginning of the path
	const FString DotSlash = FString(TEXT(".")) + PATH_SEPARATOR;

	if ( CanonicalPath.StartsWith( DotSlash ) )
	{
		CanonicalPath = CanonicalPath.Right( CanonicalPath.Len() - DotSlash.Len() );
	}

	return CanonicalPath;
}

/**
 * Given a path to a SWF file (either an absolute path or a path relative to the game's Flash/ directory)
 * attempt to find the file. If successfully found, fill out all the struct's members. See member description for details.
 */
UGFxMovieFactory::SwfImportInfo::SwfImportInfo( const FString& InSwfFilePath )
{
	// Ensure that our swf path is of the form Path/To/Swf.swf
	FString SwfToImport = EnforceCanonicalPath( InSwfFilePath );

	// The absolute path to GAMEName/Flash/ directory. e.g. d:/UE3/UDKGame/Flash/
	const FString FlashRoot = EnforceCanonicalPath( UGFxMovieFactory::GetGameFlashDir() );

	// Absolute path to the swf file, e.g. d:/UE3/UDKGame/Flash/Common_ui_assets/buttons.swf
	this->AbsoluteSwfFileLocation = FFilename( EnforceCanonicalPath( appConvertRelativePathToFull(InSwfFilePath) ) );
	
	// We could not find the file. Try finding it relative to the Flash Root for the current game.
	if ( !this->AbsoluteSwfFileLocation.FileExists() )
	{
		this->AbsoluteSwfFileLocation = FFilename( EnforceCanonicalPath( appConvertRelativePathToFull(FlashRoot + InSwfFilePath) ) );
	}

	const FString AbsoluteSwfFilePath = AbsoluteSwfFileLocation.GetPath();

	// The file is only valid for import if we could find it.
	this->bIsValidForImport = AbsoluteSwfFileLocation.FileExists() && AbsoluteSwfFilePath.InStr(FlashRoot, FALSE, TRUE) != INDEX_NONE;
	
	FString AssetPathSting = EnforceCanonicalPath( AbsoluteSwfFilePath.Replace(*FlashRoot, TEXT(""), TRUE).Replace(PATH_SEPARATOR, TEXT(".")) );	

	// Figure out the Outermost package name and the segment of the path that is just groups.
	TArray<FString> AssetPathSegments;
	AssetPathSting.ParseIntoArray( &AssetPathSegments, TEXT("."), TRUE );

	this->bIsValidForImport = this->bIsValidForImport & (AssetPathSegments.Num() > 0);

	if (this->bIsValidForImport)
	{
		FString OutermostPackage = AssetPathSegments(0);
		FString JustGroupPath = TEXT("");

		// Build the JustGroups portion.
		AssetPathSegments.Remove(0);	
		TArray<FString>::TConstIterator GroupPathIt(AssetPathSegments);
		if (GroupPathIt)
		{
			JustGroupPath = *(*GroupPathIt);
			++GroupPathIt;
			for( ; GroupPathIt; ++GroupPathIt)
			{
				JustGroupPath += TEXT(".");
				JustGroupPath += *GroupPathIt;
			}
		}

		this->AssetName = AbsoluteSwfFileLocation.GetBaseFilename(TRUE);
		this->PathToAsset = AssetPathSting;
		this->OutermostPackageName = OutermostPackage;
		this->GroupOnlyPath = JustGroupPath;
	}

}

#endif // WITH_GFx

INT UGFxReimportCommandlet::Main(const FString& Params)
{
#if WITH_GFx
	warnf( NAME_Warning, TEXT("The GFxReimport commandlet has been deprecated.  Please use the GFxImport commandlet instead") );
#endif // WITH_GFx	
    return 0;
}

/**
 * A utility that imports and/or re-imports SWF assets 
 *
 * @param Params the string containing the parameters for the commandlet
 */
INT UGFxImportCommandlet::Main( const FString& Params )
{
#if WITH_GFx

	// Start profile how long the import takes to report at the end
	DOUBLE StartTime = appSeconds();

	// Parse switches into the switches array and everything else (i.e. not starting with - or /) into the tokens.
	TArray<FString> CommandLineTokens;
	TArray<FString> CommandLineSwitches;
	ParseCommandLine( *Params, CommandLineTokens, CommandLineSwitches );

	// Warn about any switches the user may have tries because we support none
	for( TArray<FString>::TConstIterator SwitchIterator(CommandLineSwitches); SwitchIterator; ++SwitchIterator )
	{
		warnf( NAME_Warning, TEXT("Unrecognized option: %s"), *(*SwitchIterator) );
	}

	// We want to import every asset mentioned as a command line token.
	TArray<FString> AssetsToImport = CommandLineTokens;

	UBOOL bReimportOnly = FALSE;
	
	if( AssetsToImport.Num() == 0 )
	{
		// Only reimport out of date assets if the user didn't specify any assets to import
		bReimportOnly = TRUE;

		// Find all .SWF files if the user didn't specify any;
		TArray<FString> AllFiles;
		appFindFilesInDirectory( AllFiles, *(appGameDir()*TEXT("Flash")), FALSE, TRUE );
		for( TArray<FString>::TConstIterator FileIt(AllFiles); FileIt; ++FileIt )
		{
			const FString& File = *FileIt;
			if( FFilename( File ).GetExtension().ToLower() == TEXT("swf") )
			{
				AssetsToImport.AddItem( File );
			}
		}
	}
	warnf( NAME_Log, TEXT("Found %d files to import"), AssetsToImport.Num() );

	TArray<UPackage*> PackagesNeedingResave;

	TArray<FString> UpToDateAssets;
	TArray<FString> ReimportedAssets;
	TArray<FString> NewlyImportedAssets;

	for( TArray<FString>::TConstIterator AssetIt(AssetsToImport); AssetIt; ++AssetIt )
	{	
		UGFxMovieFactory::SwfImportInfo SwfImportInfo( *AssetIt );

		if (SwfImportInfo.bIsValidForImport)
		{
			
			FString FullNameOfAssetToLoad = SwfImportInfo.PathToAsset + TEXT(".") + SwfImportInfo.AssetName;

			// Attempt to find the SwfMovie.
			USwfMovie* MovieToReimport = Cast<USwfMovie>( UObject::StaticLoadObject( USwfMovie::StaticClass(), NULL, *FullNameOfAssetToLoad, NULL, LOAD_NoWarn | LOAD_Quiet, NULL ) );
			if (MovieToReimport != NULL)
			{
				// We found the movie; Reimport it if the file on disk is newer than the timestamp of the found object

				FFileManager::FTimeStamp FileTimeStamp, StoredTimeStamp;
				FFileManager::FTimeStamp::FStringToTimestamp( MovieToReimport->SourceFileTimestamp, StoredTimeStamp );
				UBOOL bFound = GFileManager->GetTimestamp( *SwfImportInfo.AbsoluteSwfFileLocation, FileTimeStamp );
				check( bFound );

				if( StoredTimeStamp < FileTimeStamp )
				{
					warnf( NAME_Log, TEXT("Reimporting %s"), *FullNameOfAssetToLoad );
					if ( FReimportManager::Instance()->Reimport( MovieToReimport ) ) 
					{
						PackagesNeedingResave.AddUniqueItem( MovieToReimport->GetOutermost() );
						ReimportedAssets.AddItem( FullNameOfAssetToLoad );					
					}
				}
				else
				{
					warnf( NAME_Log, TEXT("%s is up to date, not reimporting"), *FullNameOfAssetToLoad );
					UpToDateAssets.AddItem( FullNameOfAssetToLoad );
				}

			}
			else if( !bReimportOnly )
			{
				// We could not find this movie, so we need to import it.

				warnf( NAME_Log, TEXT("Importing %s"), *FullNameOfAssetToLoad );
				UPackage* NewPackage = CreatePackage( NULL, *(SwfImportInfo.PathToAsset) );
				if ( NewPackage != NULL )
				{					
					UObject* ImportedObject = UFactory::StaticImportObject(
						USwfMovie::StaticClass(),
						NewPackage,
						FName( *(SwfImportInfo.AssetName) ),
						RF_Public|RF_Standalone,
						*(SwfImportInfo.AbsoluteSwfFileLocation));

					USwfMovie* ImportedSwfMovie = Cast<USwfMovie>( ImportedObject );

					if (ImportedSwfMovie != NULL)
					{
						PackagesNeedingResave.AddUniqueItem( ImportedSwfMovie->GetOutermost() );	
						NewlyImportedAssets.AddItem( *FullNameOfAssetToLoad );
					}
					else
					{
						warnf( NAME_Error, TEXT("Error loading %s. Does the file exist?"), *(FullNameOfAssetToLoad) );
					}
				}
				else
				{
					warnf( NAME_Error, TEXT("Failed to create package for %s"), *(FullNameOfAssetToLoad) );
				}
			}
			else
			{
				warnf( NAME_Log, TEXT("Skipping %s"), *FullNameOfAssetToLoad );
			}
		}
		else
		{
			warnf( NAME_Error, TEXT("Could not find %s for import."), *( *AssetIt ) );
		}
	}


	// Save all the packages that we modified.
	for (TArray<UPackage*>::TConstIterator PackageIterator(PackagesNeedingResave); PackageIterator; ++PackageIterator  )
	{
		FString PackageName = (*PackageIterator)->GetName();
		FString PackageFilename;

		// We have the package in memory, but it may not be fully loaded.
		if ( !(*PackageIterator)->IsFullyLoaded() )
		{
			(*PackageIterator)->FullyLoad();
		}

		
		UBOOL bPackageExistsOnDisk = GPackageFileCache->FindPackageFile( *PackageName, NULL, PackageFilename );

		// If the package does not yet exist, figure out what its filename should be.
		if (!bPackageExistsOnDisk)
		{
			FString GfxPackageLocation = appGameDir() + TEXT("Content") + PATH_SEPARATOR + TEXT("GFx") + PATH_SEPARATOR;
			PackageFilename = GfxPackageLocation + PackageName + TEXT(".upk");
		}

		PackageFilename = UGFxMovieFactory::SwfImportInfo::EnforceCanonicalPath(appConvertRelativePathToFull( *PackageFilename ));

		warnf( NAME_Log, TEXT("Saving %s"), *PackageFilename );
			
		
		// An array version of the package filename (used by Source Control methods);
		TArray <FString> PackageFilenames;
		PackageFilenames.AddItem( PackageFilename );

		// If the package is on disk and is read-only, attempt to check it out from source control.
		if( GFileManager->IsReadOnly( *PackageFilename ) )
		{
			#if HAVE_SCC
			if( GWarn->YesNof(LocalizeSecure(LocalizeQuery(TEXT("CheckoutPerforce"),TEXT("Core")), *PackageFilename)) )
			{
				FSourceControl::Init();
				FSourceControl::CheckOut(NULL, PackageFilenames);
				FSourceControl::Close();
			}
			#endif //HAVE_SCC
		}

		// Actually save the package
		UObject::SavePackage( *PackageIterator, NULL, RF_Standalone, *PackageFilename, GWarn );

		// We just created this package, let's add it to the default changelist.
		if (!bPackageExistsOnDisk)
		{
			#if HAVE_SCC
			FSourceControl::Init();
			if ( ! FSourceControl::AddToDefaultChangelist(NULL, PackageFilenames) )
			{
				warnf( NAME_Warning, TEXT("Could not add %s to source control. You must do this yourself."), *(PackageFilenames(0)) );
			}

			FSourceControl::Close();
			#endif //HAVE_SCC
		}
	}

	// End profile how long the import takes for the report
	DOUBLE ElapsedTimeSeconds = appSeconds() - StartTime;

	// Print out a summary of what we did.
	warnf( NAME_Log, TEXT("---Summary---") );
	warnf( NAME_Log, TEXT("%d assets reimported"), ReimportedAssets.Num() );
	for( INT AssetIndex = 0; AssetIndex < ReimportedAssets.Num(); ++AssetIndex )
	{
		warnf( NAME_Log, TEXT("\t%s"), *ReimportedAssets(AssetIndex) );
	}

	warnf( NAME_Log, TEXT("%d assets up to date"), UpToDateAssets.Num() );
	for( INT AssetIndex = 0; AssetIndex < UpToDateAssets.Num(); ++AssetIndex )
	{
		warnf( NAME_Log, TEXT("\t%s"), *UpToDateAssets(AssetIndex) );
	}

	if( !bReimportOnly )
	{
		warnf( NAME_Log, TEXT("%d new assets imported"), NewlyImportedAssets.Num() );
		for( INT AssetIndex = 0; AssetIndex < NewlyImportedAssets.Num(); ++AssetIndex )
		{
			warnf( NAME_Log, TEXT("\t%s"), *NewlyImportedAssets(AssetIndex) );
		}
	}

	const INT TimeHours		= ElapsedTimeSeconds / 3600.0;
	const INT TimeMinutes	= (ElapsedTimeSeconds - TimeHours*3600) / 60.0;
	const INT TimeSeconds	= appTrunc( ElapsedTimeSeconds - TimeHours*3600 - TimeMinutes*60 );
	warnf( NAME_Log, TEXT("[%i:%02i:%02i] time taken"), TimeHours, TimeMinutes, TimeSeconds );

#endif // WITH_GFx
	return 0;
}


#endif // WITH_GFx
