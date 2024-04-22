/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class ScriptedTexture extends TextureRenderTarget2D
	native(Texture);

/** whether the texture needs to be redrawn. Render() will be called at the end of the tick, just before all other rendering. */
var transient bool bNeedsUpdate;

/** whether or not to clear the texture before the next call of the Render delegate  */
var transient bool bSkipNextClear;

cpptext
{
	/** global list of scripted textures that should be updated */
	static TArray<UScriptedTexture*> GScriptedTextures;

	UScriptedTexture();
	virtual void BeginDestroy();

	virtual void UpdateResource();
	/** calls Render() (after setting up Canvas, etc) if the scripted texture needs an update */
	void CheckUpdate();

protected:
	/** native rendering hook. Default implementation just calls script delegate. */
	virtual void Render(UCanvas* C);
}

/**
 * Called whenever bNeedsUpdate is true to update the texture. The texture is cleared to ClearColor prior to calling this function 
 * (unless bSkipNextClear is set to true).
 * bNeedsUpdate is reset before calling this function, so you can set it to true here to get another update next tick.
 * bSkipNextClear is reset to false before calling this function, so set it to true here whenever you want the next clear to be skipped
 */
delegate Render(Canvas C);

defaultproperties
{
	bNeedsUpdate=true
	bNeedsTwoCopies=false
	bSkipNextClear=false

	AddressX=TA_Clamp
	AddressY=TA_Clamp
}
