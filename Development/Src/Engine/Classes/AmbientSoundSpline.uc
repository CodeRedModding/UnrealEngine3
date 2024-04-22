/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Sound is emmited by virtual speaker. Virtual speaker is placed in evaluated in the mean loudest position in listener's scope.
 * The points used to virtual speaker evaluation are placed on spline.
 */ 

class AmbientSoundSpline extends AmbientSound
	AutoExpandCategories( AmbientSoundSpline )
	native( Sound );
/**
 * Maximal distance on spline between points, that are used to eval virtual speaker position (Minimal number of points is 3) 
 * Points are placed on spline automatically.
 */
var(AmbientSoundSpline) editoronly float            DistanceBetweenPoints<ToolTip=Maximal distance on spline between points, that are used to eval virtual speaker position (Minimal number of points is 3).>;

/** SplineComponent with spline curve defining the source of sound */
var(AmbientSoundSpline) editoronly SplineComponent  SplineComponent;

/** Only to test algorithm finding nearest point. Editor shows virtual speaker position for listener placed in TestPoint.*/
var editoronly vector                                   TestPoint;

cpptext
{
	virtual void PostLoad();
	virtual void PostEditMove(UBOOL bFinished);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

#if WITH_EDITOR
	virtual void EditorApplyTranslation(const FVector& DeltaTranslation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown);

	/** Force all spline data to be consistent. */
	virtual void UpdateSpline();

	/** Recalculate spline after any control point was moved. */
    virtual void UpdateSplineGeometry();
#endif
}

defaultproperties
{
	DistanceBetweenPoints=200.0

	Components.Remove( AudioComponent0 )

	Begin Object Class=SplineComponentSimplified Name=SplineComponent0
	End Object
	SplineComponent=SplineComponent0
	Components.Add( SplineComponent0 )

	Begin Object Class=SplineAudioComponent Name=AudioComponent1
		PreviewSoundRadius=DrawSoundRadius0
		bAutoPlay=false
		bStopWhenOwnerDestroyed=true
		bShouldRemainActiveIfDropped=true
	End Object
	AudioComponent=AudioComponent1
	Components.Add(AudioComponent1)
}