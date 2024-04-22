/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MATERIALEDITORCONTEXTMENUS_H__
#define __MATERIALEDITORCONTEXTMENUS_H__

// Forward declarations.
class WxMaterialEditor;

/**
 * Presented when the user right-clicks on an empty region of the material editor viewport.
 */
class WxMaterialEditorContextMenu_NewNode : public wxMenu
{
public:
	WxMaterialEditorContextMenu_NewNode(WxMaterialEditor* MaterialEditor);
};

/**
 * Presented when the user right-clicks on a material expression node in the material editor.
 */
class WxMaterialEditorContextMenu_NodeOptions : public wxMenu
{
public:
	WxMaterialEditorContextMenu_NodeOptions(WxMaterialEditor* MaterialEditor);
};

/**
 * Presented when the user right-clicks on an object connector in the material editor viewport.
 */
class WxMaterialEditorContextMenu_ConnectorOptions : public wxMenu
{
public:
	WxMaterialEditorContextMenu_ConnectorOptions(WxMaterialEditor* MaterialEditor);
};

#endif // __MATERIALEDITORCONTEXTMENUS_H__
