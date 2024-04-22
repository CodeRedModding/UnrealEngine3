//------------------------------------------------------------------------------
// The UE3 command.
//
// Owner: Jamie Redmond
//
// Copyright (c) 2002-2006 OC3 Entertainment, Inc.
//------------------------------------------------------------------------------

#ifndef FxUE3Command_H__
#define FxUE3Command_H__

#ifdef __UNREAL__

#include "FxCommand.h"

namespace OC3Ent
{

namespace Face
{

// The UE3 command.
class FxUE3Command : public FxCommand
{
	// Declare the class.
	FX_DECLARE_CLASS(FxUE3Command, FxCommand);
public:
	// Constructor.
	FxUE3Command();
	// Destructor.
	virtual ~FxUE3Command();

	// Sets up the argument syntax.
	static FxCommandSyntax CreateSyntax( void );

	// Execute the command.
	virtual FxCommandError Execute( const FxCommandArgumentList& argList );

	// Update all USoundCue objects.
	void UpdateSoundCues( void );
};

} // namespace Face

} // namespace OC3Ent

#endif

#endif // __UNREAL__
