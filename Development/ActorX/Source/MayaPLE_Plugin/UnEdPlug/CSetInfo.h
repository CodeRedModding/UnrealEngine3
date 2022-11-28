//
// Copyright (C) 2002 Secret Level 
// 
// File: CSetInfo.h
//
// Author: Michael Arnold
//
// Description: Simple re-used code for MFnSet extraction
//
// 


#pragma once

#include <maya/MString.h>

class CSetInfo
{
public:
	CSetInfo(MString const& set_name);
	~CSetInfo(void);

	MString const&				GetName(void) const	{return m_name;}
	class MFnDependencyNode *	GetNode(void) const	{return	m_node;}
	class MFnSet *				GetSet(void) const	{return m_set;}
	class MFnIkJoint *			GetRoot(void) const	{return m_root;}

private:
	MString	m_name;

	class MFnDependencyNode *	m_node;
	class MFnSet *				m_set;
	class MFnIkJoint *			m_root;
};