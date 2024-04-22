/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Streams gameplay events recorded during a session to disk
 */
class GameplayEventsReader extends GameplayEvents
	native;

/** Array of handlers for this file when it processes */
var transient array<GameplayEventsHandler> RegisteredHandlers;

/** 
 *   Loads a stat file from disk
 * @param Filename - name of the file that will be open for serialization
 * @return TRUE if successful, else FALSE
 */
native function bool OpenStatsFile(string Filename);

/** 
 * Closes and deletes the archive being read from
 * clearing all data stored within
 */
native function CloseStatsFile();

/** Serialize the contents of the file header */
native protected function bool SerializeHeader();

/** Register a handler with this reader */
event RegisterHandler(GameplayEventsHandler NewHandler)
{
	local int AddIndex;
	if (RegisteredHandlers.Find(NewHandler) == INDEX_NONE)
	{
		AddIndex = RegisteredHandlers.Length;
		RegisteredHandlers.Length = RegisteredHandlers.Length + 1;
		RegisteredHandlers[AddIndex] = NewHandler;
		NewHandler.SetReader(self);
	}
}

/** Unregister a handler with this reader */
event UnregisterHandler(GameplayEventsHandler ExistingHandler)
{
	local int RemoveIndex;
	RemoveIndex = RegisteredHandlers.Find(ExistingHandler);
	// Verify that it is in the array
	if (RemoveIndex != INDEX_NONE)
	{
		RegisteredHandlers.Remove(RemoveIndex,1);
		ExistingHandler.SetReader(None);
	}
}

/** Signal start of stream processing */
native private function ProcessStreamStart();

/** Read / process stream data from the file */
native function ProcessStream();

/** Signal end of stream processing */
native private function ProcessStreamEnd();

/** Return the unique session ID */
native function string GetSessionID();

/** Return the title ID of the recorded session */
native function int GetTitleID();

/** Return the platform the data was recorded on */
native function int GetPlatform();

/** Return the timestamp the session started recording */
native function string GetSessionTimestamp();

/** Get the time the session started */
native function float GetSessionStart();

/** Get the time the session ended */
native function float GetSessionEnd();

/** Return the total time the session lasted */
native function float GetSessionDuration();

defaultproperties
{
}