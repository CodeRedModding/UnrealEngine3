/*=============================================================================
	UExporter.cpp: Exporter class definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * Encapsulates a map form objects to their direct inners, used by UExporter::ExportObjectInner when exporting objects.
 * Should be recreated before new objects are created within objects that are to be exported!
 *
 * Typical usage pattern is:
 *
 *     const FExportObjectInnerContext Context;
 *     foreach( Object in a set of objects)
 *         Exporter->ExportObjectInner( Context, Object );
 */
class FExportObjectInnerContext
{
public:
	/**
	 * Creates the map from objects to their direct inners.
	 */
	FExportObjectInnerContext();

	/**
	 * Creates the map from objects to their direct inners.
	 *	@param	ObjsToIgnore	An array of objects that should NOT be put in the list
	 */
	FExportObjectInnerContext(TArray<UObject*>& ObjsToIgnore);

	/**Empty Constructor for derived export contexts */
	FExportObjectInnerContext(const UBOOL bIgnoredValue) {};

protected:
	friend class UExporter;

	/** Data structure used to map an object to its inners. */
	typedef TArray<UObject*>			InnerList;
	typedef TMap<UObject*,InnerList>	ObjectToInnerMapType;

	ObjectToInnerMapType				ObjectToInnerMap;

public:
	const TArray<UObject*>* GetObjectInners(UObject* InObj) const
	{
		return ObjectToInnerMap.Find(InObj);
	}

};

/**
 * An object responsible for exporting other objects to archives (files).
 */
class UExporter : public UObject
{
	DECLARE_ABSTRACT_CLASS_INTRINSIC(UExporter,UObject,CLASS_Transient,Core)

	// Variables.
	UClass*						SupportedClass;
	TArray<FString> 			FormatExtension;
	TArray<FString> 			FormatDescription;
	/** Index into FormatExtension/FormatDescription of the preferred export format. */
	INT							PreferredFormatIndex;
	INT							TextIndent;
	BITFIELD					bText					: 1;
	BITFIELD					bSelectedOnly			: 1;
	/** If TRUE, this will force the exporter code to create a file-based Ar (this can keep large output files from taking too much memory) */
	BITFIELD					bForceFileOperations	: 1;


	static				FString	CurrentFilename;
	static	const		UBOOL	bEnableDebugBrackets;

	// Constructor.
	UExporter();

	// UObject interface.
	void Serialize( FArchive& Ar );
	void StaticConstructor();

	// UExporter interface.
	virtual UBOOL ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags=0 ) { return( FALSE );}

	struct FExportPackageParams
	{
		FString RootMapPackageName;
		const FExportObjectInnerContext* Context;
		UPackage* InPackage;
		UObject* InObject;
		const TCHAR* Type;
		FOutputDevice* Ar;
		FFeedbackContext* Warn;
		DWORD PortFlags;
	};
	virtual void ExportPackageObject(FExportPackageParams& ExpPackageParams) {};
	virtual void ExportPackageInners(FExportPackageParams& ExpPackageParams) {};

	virtual UBOOL ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex = 0, DWORD PortFlags=0 ) { return( FALSE );}

	/** 
	 * Number of binary files to export for this object. Should be 1 in the vast majority of cases. Noted exception would be multichannel sounds
	 * which have upto 8 raw waves stored within them.
	 */
	virtual INT GetFileCount( void ) const { return( 1 ); }

	/** 
	 * Differentiates the filename for objects with multiple files to export. Only needs to be overridden if GetFileCount() returns > 1.
	 */
	virtual FString GetUniqueFilename( const TCHAR* Filename, INT FileIndex ) { check( FileIndex == 0 ); return( FString( Filename ) ); }

	static UExporter* FindExporter( UObject* Object, const TCHAR* Filetype );
	static INT ExportToFile( UObject* Object, UExporter* Exporter, const TCHAR* Filename, UBOOL InSelectedOnly, UBOOL NoReplaceIdentical=FALSE, UBOOL Prompt=FALSE );
	static UBOOL ExportToArchive( UObject* Object, UExporter* Exporter, FArchive& Ar, const TCHAR* FileType, INT FileIndex );
	static void ExportToOutputDevice( const FExportObjectInnerContext* Context, UObject* Object, UExporter* Exporter, FOutputDevice& Out, const TCHAR* FileType, INT Indent, DWORD PortFlags=0, UBOOL bInSelectedOnly=FALSE );

	struct FExportToFileParams
	{
		UObject* Object;
		UExporter* Exporter;
		const TCHAR* Filename;
		UBOOL InSelectedOnly;
		UBOOL NoReplaceIdentical/**=FALSE*/;
		UBOOL Prompt/**=FALSE*/;
		UBOOL bUseFileArchive;
		TArray<UObject*> IgnoreObjectList;
		UBOOL WriteEmptyFiles;
	};
	static INT ExportToFileEx( FExportToFileParams& ExportParams );

	/**
	 * Single entry point to export an object's subobjects, its components, and its properties
	 *
	 * @param Context			Context from which the set of 'inner' objects is extracted.  If NULL, an object iterator will be used.
	 * @param Object			The object to export 
	 * @param Ar				OutputDevice to print to
	 * @param PortFlags			Flags controlling export behavior
	 * @param bSkipComponents	TRUE if components should not be exported
	 */
	void ExportObjectInner(const FExportObjectInnerContext* Context, UObject* Object, FOutputDevice& Ar, DWORD PortFlags, UBOOL bSkipComponents=FALSE);

protected:
	/**
	 * Exports subobject definitions for all components specified.  Makes sure that components which are referenced
	 * by other components in the map are exported first.
	 *
	 * @param	Components		the components to export.  This map is typically generated by calling CollectComponents on the object being exported
	 * @param	Ar				the archive to output the subobject definitions to...usually the same archive that you're exporting the rest of the properties
	 * @param	PortFlags		the flags that were passed into the call to ExportText
	 */
	void ExportComponentDefinitions(const FExportObjectInnerContext* Context, const TArray<UComponent*>& Components, FOutputDevice& Ar, DWORD PortFlags);


	/**
	 * Allows the Exporter to export any extra information it would like each component.
	 * This occurs immediately after the component is exported.
	 *
	 * @param	Component		the component being exported.
	 * @param	Ar				the archive to output the subobject definitions to.
	 * @param	PortFlags		the flags that were passed into the call to ExportText
	 */
	virtual void ExportComponentExtra( const FExportObjectInnerContext* Context, const TArray<UComponent*>& Components, FOutputDevice& Ar, DWORD PortFlags) {}

	/**
	 * Emits the starting line for a subobject definition.
	 *
	 * @param	Ar					the archive to output the text to
	 * @param	Obj					the object to emit the subobject block for
	 * @param	PortFlags			the flags that were passed into the call to ExportText
	 */
	void EmitBeginObject( FOutputDevice& Ar, UObject* Obj, DWORD PortFlags );

	/**
	 * Emits the ending line for a subobject definition.
	 *
	 * @param	Ar					the archive to output the text to
	 * @param	bIncludeBrackets	(debugging purposes only)
	 */
	void EmitEndObject( FOutputDevice& Ar );
};
