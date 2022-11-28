/*=============================================================================
	Editor.h: Unreal editor public header file.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_EDITOR_COMMANDLETS
#define _INC_EDITOR_COMMANDLETS

class UAnalyzeBuildCommandlet : public UCommandlet
{
	DECLARE_CLASS(UAnalyzeBuildCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR *args );
};

class UAnalyzeScriptCommandlet : public UCommandlet
{
	DECLARE_CLASS(UAnalyzeScriptCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main(const TCHAR *Parms);

	UFunction* FindSuperFunction(UFunction *evilFunc);
};

class UBatchExportCommandlet : public UCommandlet
{
	DECLARE_CLASS(UBatchExportCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class UConformCommandlet : public UCommandlet
{
	DECLARE_CLASS(UConformCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class UCheckUnicodeCommandlet : public UCommandlet
{
	DECLARE_CLASS(UCheckUnicodeCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class UPackageFlagCommandlet : public UCommandlet
{
	DECLARE_CLASS(UPackageFlagCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class ULoadPackage : public UCommandlet
{
	DECLARE_CLASS(ULoadPackage,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class UListExports : public UCommandlet
{
	DECLARE_CLASS(UListExports,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class UReimportTextures : public UCommandlet
{
	DECLARE_CLASS(UReimportTextures,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class UConvertTextures : public UCommandlet
{
	DECLARE_CLASS(UConvertTextures,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class UCutdownPackages : public UCommandlet
{
	DECLARE_CLASS(UCutdownPackages,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class UCreateStreamingLevel : public UCommandlet
{
	DECLARE_CLASS(UCreateStreamingLevel,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class UResavePackages : public UCommandlet
{
	DECLARE_CLASS(UResavePackages,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class UCookPackagesXenon : public UCommandlet
{
	DECLARE_CLASS(UCookPackagesXenon,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class URebuildCommandlet : public UCommandlet
{
	DECLARE_CLASS(URebuildCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class UStripSourceCommandlet : public UCommandlet
{
	DECLARE_CLASS(UStripSourceCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class UMakeCommandlet : public UCommandlet
{
	DECLARE_CLASS(UMakeCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class UFixAnimNodeCommandlet : public UCommandlet
{
	DECLARE_CLASS(UFixAnimNodeCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};



#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
