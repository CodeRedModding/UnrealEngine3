/*=============================================================================
	ScopedDebugInfo.h: Scoped debug info definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SCOPEDDEBUGINFO_H__
#define __SCOPEDDEBUGINFO_H__

/** Debug info that is stored on the stack and handled by stack unwinding. */
class FScopedDebugInfo
{
public:

	enum { MaxDescriptionLength = 128 };

	/** The number of calls on the stack the debug info replaces. */
	const INT NumReplacedOuterCalls;

	/** The next debug info on the stack, or NULL if this is the last debug info on the stack. */
	FScopedDebugInfo* const NextOuterInfo;

	/** Initialization constructor. */
	FScopedDebugInfo(INT InNumReplacedOuterCalls);

	/** Destructor. */
	~FScopedDebugInfo();

	/** @return The "function name" to display on the call stack for this debug info. */
	virtual FString GetFunctionName() const = 0;

	/** @return The filename to display on the call stack for this debug info. */
	virtual FString GetFilename() const = 0;

	/** @return The line number to display on the call stack for this debug info. */
	virtual INT GetLineNumber() const = 0;

	/** Accesses the list of debug infos on the stack in this thread. */
	static FScopedDebugInfo* GetDebugInfoStack();
};

#endif
