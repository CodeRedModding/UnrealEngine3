/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#pragma once

using namespace System;
using namespace System::Xml::Serialization;

namespace ConsoleInterface
{
	ref class FileGroup;

	// Holder struct for a generic tag
	[Serializable]
	public ref class Tag
	{
	public:
		/// The name of the tag
		[XmlAttribute]
		String ^Name;

	public:
		Tag();
	};

	// Groups of tags so that a sync can be specified with one name, and do multiple FileGroup tags
	[Serializable]
	public ref class TagSet
	{
	public:
		// Name of the TagSet
		[XmlAttribute]
		String ^Name;

		// List of Tags
		[XmlArray]
		array<Tag^> ^Tags;

	public:
		TagSet();
	};

	[Serializable]
	public ref class FileFilter
	{
	public:
		[XmlAttribute]
		String ^Name;

		// If this is TRUE (default), the filter matches filenames
		[XmlAttribute]
		bool bIsForFilename;

		// If this is TRUE (not default), the filter matches any single directory name in the path to the file
		[XmlAttribute]
		bool bIsForDirectory;

	public:
		FileFilter();
	};

	// Information about a file or set of files in a file group
	[Serializable]
	public ref class FileSet
	{
	public:
		// Wildcard that specifies some files to sync
		[XmlAttribute]
		String ^Path;

		// Should this set of files to a recursive sync
		[XmlAttribute]
		bool bIsRecursive;

		// Optional filter to apply to files (array of filenames) to not copy them
		[XmlArray]
		array<FileFilter^> ^FileFilters;

	public:
		FileSet();
	};

	// Group of files and tags associated with them for filtering syncing
	[Serializable]
	public ref class FileGroup
	{
	public:
		// If true, this file will be synced to the target console (for files that need syncing, but don't want to be in the TOC)
		[XmlAttribute]
		bool bIsForSync;

		// If true, this file will be put in the TOC
		[XmlAttribute]
		bool bIsForTOC;

		// Should this set of files be copied to the root folder when publishing
		[XmlAttribute]
		bool bDeploy;

		// Tag for this group.
		[XmlAttribute]
		String ^Tag;

		// Platform for this group. Can also be * for any (same as null), or Console for non-PC platforms
		[XmlAttribute]
		String ^Platform;

		// List of file infos for the group
		[XmlArray]
		array<FileSet^> ^Files;

	public:
		FileGroup();
	};

	[Serializable]
	public ref class GameDescription
	{
	public:
		// The name of the game
		[XmlAttribute]
		String^ Name;

	public:
		GameDescription();
	};

	[Serializable]
	public ref class LanguageDescription
	{
	public:
		// The three letter code for the language
		[XmlAttribute]
		String^ ThreeLetterCode;

	public:
		LanguageDescription();
	};

	[Serializable]
	public ref class PlatformDescription
	{
	public:
		// The name of the platform
		[XmlAttribute]
		String^ Name;
		[XmlAttribute]
		bool bCaseSensitiveFileSystem;

	public:
		PlatformDescription();
	};

	[Serializable]
	public ref class TextureExtensionDescription
	{
	public:
		// The name of the texture extension
		[XmlAttribute]
		String^ Name;

	public:
		TextureExtensionDescription();
	};

	// Summary description for per-platform CookerSettings.
	[Serializable]
	public ref class SharedSettings
	{	
	public:
		// List of known games
		[XmlArray]
		array<GameDescription^>^ KnownGames;

		// List of known platforms
		[XmlArray]
		array<PlatformDescription^>^ KnownPlatforms;

		// List of known languages (should match the list in C++ land)
		[XmlArray]
		array<LanguageDescription^>^ KnownLanguages;

		// List of known texture extensions
		[XmlArray]
		array<TextureExtensionDescription^>^ KnownTextureExtensions;

		// List of TagSet objects
		[XmlArray]
		array<TagSet^>^ TagSets;

		// Set of file groups for syncing
		[XmlArray]
		array<FileGroup^>^ FileGroups;

	public:
		SharedSettings();
	};
}
