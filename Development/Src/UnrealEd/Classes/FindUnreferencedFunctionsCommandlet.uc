/**
 * This commandlet generates a list of functions which aren't referenced by any code in the game.  This commandlet does not call the
 * event "Main".
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FindUnreferencedFunctionsCommandlet extends Commandlet
	native;

/** Worker class which processes compiled bytecode. */
var		transient		const		ByteCodeSerializer		Serializer;

cpptext
{
	/**
	 * Find the original function declaration from an interface class implemented by FunctionOwnerClass.
	 *
	 * @param	FunctionOwnerClass	the class containing the function being looked up.
	 * @param	Function			the function being looked up
	 *
	 * @return	if Function is an implementation of a function declared in an interface class implemented by FunctionOwnerClass,
	 *			returns a pointer to the function from the interface class; NULL if Function isn't an implementation of an interface
	 *			function
	 */
	UFunction* GetInterfaceFunctionDeclaration( UClass* FunctionOwnerClass, UFunction* Function );

	/**
	 * Commandlet entry point
	 *
	 * @param	Params	the command line parameters that were passed in.
	 *
	 * @return	0 if the commandlet succeeded; otherwise, an error code defined by the commandlet.
	 */
	virtual INT Main(const FString& Params);
}

DefaultProperties
{

}
