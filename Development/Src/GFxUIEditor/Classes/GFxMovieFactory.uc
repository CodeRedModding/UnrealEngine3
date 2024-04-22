/**********************************************************************

Filename    :   GFxMovieFactory.uc
Content     :   GFx Movie Factory integration class

Copyright   :   (c) 2006-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :   Since 'ucc' will prefix all class names with 'U'
there is not conflict with GFx file / class naming.

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING 
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/


class GFxMovieFactory extends Factory
	dependson(Factory)
	dependson(SwfMovie)
	inherits(FReimportHandler)
	native
	transient;

var(Import) editconst bool bSetSRGBOnImportedTextures;
var(Import) bool bPackTextures;
var(Import) int PackTextureSize <editcondition=bPackTextures | ClampMin=256 | Multiple=32>;
var(Import) bool bForceSquarePacking <editcondition=bPackTextures>;
var(Import) FlashTextureRescale TextureRescale;
var(Import) editconst string TextureFormat;

cpptext
{
	UObject* FactoryCreateBinary(UClass* InClass,
		UObject* InOuter,
		FName InName,
		EObjectFlags InFlags,
		UObject* Context,
		const TCHAR* Type,
		const BYTE*& Buffer,
		const BYTE* BufferEnd,
		FFeedbackContext* Warn);

	UBOOL Reimport(UObject* InObject);

#if WITH_GFx
	/**
	 * Parses the Swf data in the movie and locates all the import tags.
	 * Each import tag is translated to a "weak referece" (i.e. a UE3 fullpath pointing at a SwfMovie.)
	 * All weak references are added to the ReferencedSwfs property of the USwfMovie being imported.
	 *
	 * @param MovieInfo The SwfMovie being imported; its contents will be parsed for references to other Swfs.
	 * @param OutMissingRefs A list of references to Swfs that we failed to convert to full UE3 references (used for error reporting only)
	 */
	static void CheckImportTags(USwfMovie* SwfMovie, TArray<FString>* OutMissingRefs, UBOOL bAddFonts = FALSE);

	/**
	 * Utility class with lots of useful info for importing a SWF.
	 * It can be constructed given a SWF file path. See constructor for more details.
	 */
	struct SwfImportInfo
	{
		/** 
		 * Given a path, ensure that this path uses only the approved PATH_SEPARATOR.
		 * If this path is relative, the optional ./ at the beginning is removed.
		 *
		 * @param InPath Path to canonize
		 *
		 * @return The canonical copy of InPath.
		 */
		static FString EnforceCanonicalPath(const FString& InPath);

		/**
		 * Given a path to a SWF file (either an absolute path or a path relative to the game's Flash/ directory)
		 * fill out all the struct's members. See member description for details.
		 */
		SwfImportInfo( const FString& InSwfFile );

		/** The absolute path to swf including the filename */
		FFilename AbsoluteSwfFileLocation;

		/** The asset name of the SwfMovie. This will be the same as the filename but without the .SWF extension */
		FString AssetName;

		/** UE3 Path to the asset not including the actual asset name. E.g. Package.Group0.Group1 */
		FString PathToAsset;
		
		/** The name of the outermost package into which this asset should be imported */
		FString OutermostPackageName;

		/** The group only portion of the path. E.g. If the fullpath is Package.Group0.Group1.Asset, then this field is just Group0.Group1 */
		FString GroupOnlyPath;

		/** Is it OK to import this swf? */
		UBOOL bIsValidForImport;
	};

	/** Return the Flash/ directory for this game. E.g. d:/UE3/UDKGame/Flash/ */
	static FString GetGameFlashDir();

private:
	UObject* CreateMovieGFxExport(UObject* InParent, FName Name, const FString& OriginalSwfPath, EObjectFlags Flags, const BYTE* Buffer, const BYTE* BufferEnd,
		const FString& GFxExportCmdline, FFeedbackContext* Warn);
#endif

private:
	UBOOL RunGFXExport( const FString& strCmdLineParams, FString* OutGfxExportErrors );
	UObject* BuildPackage(const FString& strInputDataFolder, const FString& strSwfFile, const FString& OriginalSwfLocation,
		const FName& Name, UObject* InOuter, EObjectFlags Flags, FFeedbackContext* Warn);

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
	static FString GetOriginalResourceDir( const FString& InSwfPath );

	/**
	 * Function that is used to modify textures on import. This should be extended
	 * where possible to set correct texture compression values, etc, for the different
	 * platforms based on the texture data.
	 *
	 * @param strTextureFileName - the file path of the texture being imported
	 * @param pTexture - the Texture object to be modified
	 */
	void FixupTextureImport(UTexture2D* pTexture, const FString& strTextureFileName);

	/**
	 * Deletes a folder and all of its contents.
	 */
	UBOOL DeleteFolder(const FString& strFolderPath);

	void GetAllFactories(TArray<UFactory*> &factories);
	UFactory *FindMatchingFactory(TArray<UFactory*> &factories, const FString &fileExtension);

}


defaultproperties
{
	//base factory members
	bEditorImport=true
	SupportedClass=class'SwfMovie'

	Description="SWF Movie";
	Formats.Add("swf;SWF Movie");
	Formats.Add("gfx;SWF Movie (stripped)");

	bSetSRGBOnImportedTextures=false 

	TextureRescale=FlashTextureScale_Mult4
	bPackTextures=false
	PackTextureSize=1024
	bForceSquarePacking=false
	TextureFormat="TGA"
}
