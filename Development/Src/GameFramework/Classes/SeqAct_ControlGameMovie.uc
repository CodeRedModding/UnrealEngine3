/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_ControlGameMovie extends SeqAct_Latent
	native;


/** Which movie to play */
var(Movie) string MovieName;

/** When the fading in from just audio to audio and video should occur **/
var(Movie) int StartOfRenderingMovieFrame;

/** When the fading from audio and video to just audio should occur **/
var(Movie) int EndOfRenderingMovieFrame;


cpptext
{
	/**
	 * Executes the action when it is triggered 
	 */
	void Activated();
	UBOOL UpdateOp(FLOAT deltaTime);
}



defaultproperties
{
	ObjName="Control Game Movie"
	ObjCategory="Cinematic"

	bAutoActivateOutputLinks=FALSE

	InputLinks(0)=(LinkDesc="Play")
	InputLinks(1)=(LinkDesc="Stop")

	OutputLinks(0)=(LinkDesc="Out")
	OutputLinks(1)=(LinkDesc="Movie Completed")

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_String',LinkDesc="MovieName",PropertyName=MovieName)

	StartOfRenderingMovieFrame=-1
	EndOfRenderingMovieFrame=-1
} 
