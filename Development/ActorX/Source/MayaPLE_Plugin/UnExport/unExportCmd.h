//
// Copyright (C) 2002 Secret Level 
// 
// File: unEditorCmd.cpp
//
// Author: Michael Arnold
//
// Description: Re-used code. Extracts information from an argument list, pertinent 
//              to psk/psa export, or (in unEditor.mll) direct export of skeletons
//				and animations into UnrealEdDLL.dll
//
// 


#pragma once
#include "maya/MString.h"

//--------------------------------------------------------------------------
struct CCmdArg
{
public:
	
	MString const&	GetIndicator(void) const	{return m_indicator;}
	MString const&	GetValue(void) const		{return m_value;}

	void	SetIndicator(MString const& sce)
	{
		m_indicator = sce;
#if _DEBUG
		m_indicatorAsChar = m_indicator.asChar();
#endif
	}

	void	SetValue(MString const& sce)
	{
		m_value = sce;
#if _DEBUG
		m_valueAsChar = m_value.asChar();
#endif
	}

	void	ClearValue(void)
	{
		m_value.clear();
#if _DEBUG
		m_valueAsChar = 0;
#endif
	}

private:
	MString		m_indicator;
	MString		m_value;
#if _DEBUG
	const char *	m_indicatorAsChar;
	const char *	m_valueAsChar;
#endif
};

class CUnExportCmd
{
public:
	enum
	{
		setname = 0,
		logpath = 1,
		to_path = 2,
		animfile = 3,
		skinfile = 4,
		num_args = 5
	};

public:
	explicit CUnExportCmd(MString const& name);
	virtual ~CUnExportCmd(void)						{DestroySetPointers();}

	MString const&	GetName(void) const				{return m_name;}

	MString	const&	GetSetName(void) const			{return m_arguments[setname].GetValue();}
	MString	const&	GetLogPath(void) const			{return m_arguments[logpath].GetValue();}
	MString	const&	GetToPath(void) const			{return m_arguments[to_path].GetValue();}
	MString	const&	GetAnimFile(void) const			{return m_arguments[animfile].GetValue();}
	MString	const&	GetSkinFile(void) const			{return m_arguments[skinfile].GetValue();}

	virtual MStatus doIt( const MArgList& args )
	{
		return MStatus::kSuccess;
	}

	void         setResult( MString const& val )
	{
		MPxCommand::setResult(val);
	}

	class MFnSet *		GetSet(void) const		{return m_set;}
	class MFnIkJoint *	GetSetRoot(void) const	{return m_setroot;}

	void				OrientRoot(void);

	void	ParseForNames( const MArgList& args );

	void	DestroySetPointers(void);
	MStatus	DetermineSetInfo(void);

protected:
	void	SetSetName(MString const& value) 		{m_arguments[setname].SetValue(value);}
	void	SetLogPath(MString const& value) 		{m_arguments[logpath].SetValue(value);}
	void	SetToPath(MString const& value) 		{m_arguments[to_path].SetValue(value);}
	void	SetAnimFile(MString const& value) 		{m_arguments[animfile].SetValue(value);}
	void	SetSkinFile(MString const& value) 		{m_arguments[skinfile].SetValue(value);}
	

private:
	MString			m_name;

	CCmdArg			m_arguments[num_args];

	class MFnSet *		m_set;
	class MFnIkJoint *	m_setroot;
};

//--------------------------------------------------------------------------

class CAboutCmd : public CUnExportCmd
{
public:
	CAboutCmd(void) : CUnExportCmd("about")		{}
	virtual ~CAboutCmd(void)					{}

	MStatus doIt( const MArgList& args );

private:

};

class CHelpCmd : public CUnExportCmd
{
public:
	CHelpCmd(void) : CUnExportCmd("help")		{}
	virtual ~CHelpCmd(void)						{}

	MStatus doIt( const MArgList& args );

private:

};

extern void ClearScenePointers();


