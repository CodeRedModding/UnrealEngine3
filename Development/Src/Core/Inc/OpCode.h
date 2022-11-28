/*=============================================================================
	OpCode.h: UDebugger bytecode debug information.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Ron Prestenback
=============================================================================*/

/*-----------------------------------------------------------------------------
	UDebugger Opcodes.
-----------------------------------------------------------------------------*/

enum EDebugInfo
{
	DI_Let					= 0x00,
	DI_SimpleIf				= 0x01,
	DI_Switch				= 0x02,
	DI_While				= 0x03,
	DI_Assert				= 0x04,
	DI_Return				= 0x10,
	DI_ReturnNothing		= 0x11,
	DI_NewStack				= 0x20,
	DI_NewStackLatent		= 0x21,
	DI_NewStackLabel		= 0x22,
	DI_PrevStack			= 0x30,
	DI_PrevStackLatent		= 0x31,
	DI_PrevStackLabel		= 0x32,
	DI_PrevStackState		= 0x33,
	DI_EFP					= 0x40,
	DI_EFPOper				= 0x41,
	DI_EFPIter				= 0x42,
	DI_ForInit				= 0x50,
	DI_ForEval				= 0x51,
	DI_ForInc				= 0x52,
	DI_BreakLoop			= 0x60,
	DI_BreakFor				= 0x61,
	DI_BreakForEach			= 0x62,
	DI_BreakSwitch			= 0x63,
	DI_ContinueLoop			= 0x70,
	DI_ContinueForeach		= 0x71,
	DI_ContinueFor			= 0x72,
	DI_MAX					= 0xFF,
};

TCHAR* GetOpCodeName( BYTE OpCode );

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
