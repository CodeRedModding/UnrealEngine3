//
// Copyright (C) 2002 Secret Level 
// 
// File: maattrval.h
//
// Author: Michael Arnold
//
// Description: Simplification of the task of getting attributes from MPlug objects
//
// 


#pragma once

#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>

class MGetAttribute
{
public:
	MGetAttribute(MFnDependencyNode const& node, MString const& attrname)
		: m_node(node)
	{
		m_plug = m_node.findPlug(attrname);
	}
	
	MFnDependencyNode const&	GetNode(void) const		{return m_node;}
	MPlug const&				GetPlug(void) const		{return m_plug;}
	MStatus const&				IsValid(void) const		{return m_valid;}

	template <class T>
	MStatus		GetBasicValue(T& value) const
	{
		if (m_valid == MS::kSuccess) {
			return m_plug.getValue(value);
		} else {
			return m_valid;
		}
	}

	template <class T>
	MStatus		GetCompositeValue(T* value, unsigned int sz) const
	{
		if (m_valid == MS::kSuccess && m_plug.isCompound() && sz <= m_plug.numChildren()) {
			MStatus res = MS::kSuccess;
			for (unsigned int i = 0; i < sz; i++) {
				if (m_plug.child(i).getValue(value[i]) != MS::kSuccess) {
					res = MS::kFailure;
				}
			}
			return res;
		} else {
			return MS::kFailure;
		}
	}


private:
	MFnDependencyNode const&	m_node;
	MPlug 						m_plug;	
	MStatus						m_valid;
};
