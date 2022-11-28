//
// Copyright (C) 2002 Secret Level 
// 
// File: CSetInfo.cpp
//
// Author: Michael Arnold
//
// Description: Re-used set-information extractor
//
// 
#include "precompiled.h"
#include "CSetInfo.h"
#include "unExportCmd.h"
#include <maya/MFnDependencyNode.h>
#include <maya/MFnSet.h>
#include <maya/MFnIkJoint.h>


CSetInfo::CSetInfo(MString const& set_name)
: m_name(set_name), m_node(0), m_set(0), m_root(0)
{
	MArgList alist;
	alist.addArg(MString("-setname"));
	alist.addArg(set_name);

	CUnExportCmd oldstyle(set_name);
	oldstyle.ParseForNames(alist);
	oldstyle.DetermineSetInfo();

	if (oldstyle.GetSet()) {
		m_set = new MFnSet( *oldstyle.GetSet() );
		m_node = new MFnDependencyNode( *oldstyle.GetSet() );
	}
	if (oldstyle.GetSetRoot()) {
		m_root = new MFnIkJoint( *oldstyle.GetSetRoot() );
	}
}

CSetInfo::~CSetInfo(void)
{
	if (m_set) {
		delete m_set;
		m_set = 0;
	}
	if (m_root) {
		delete m_root;
		m_root = 0;
	}
	if (m_node) {
		delete m_node;
		m_node = 0;
	}
}