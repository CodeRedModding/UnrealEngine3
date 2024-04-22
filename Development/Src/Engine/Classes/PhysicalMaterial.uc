/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PhysicalMaterial extends Object
	native(Physics)
	collapsecategories
	hidecategories(Object);


// Used internally by physics engine.
var	transient int	MaterialIndex;


// Surface properties
var()	float	Friction;
var()	float	Restitution;
var()	bool	bForceConeFriction;

// Anisotropic friction support

/** Enable support for different friction in different directions. */
var(Advanced)	bool		bEnableAnisotropicFriction;
/** Direction (in physics object local space) for FrictionV to be applied. */
var(Advanced)	vector		AnisoFrictionDir;
/** Friction to be applied in AnisoFrictionDir - Friction will be used for the other direction. */
var(Advanced)	float		FrictionV;

// Object properties
var()	float	Density;
var()	float	AngularDamping;
var()	float	LinearDamping;
var()	float	MagneticResponse;
var()	float	WindResponse;

// This impact/slide system is temporary. We need a system that looks at both PhysicalMaterials, but that is something for the future.

// Impact effects 

/** How hard an impact must be to trigger effect/sound */
var(Impact)		float						ImpactThreshold;
/** Min time between effect/sound being triggered */
var(Impact)		float						ImpactReFireDelay;
/** Particle effect to play at impact location */
var(Impact)		ParticleSystem				ImpactEffect;
/** Sound to play  */
var(Impact)		SoundCue					ImpactSound;

// Slide effects
/** How fast an object must slide to trigger effect/sound */
var(Slide)		float						SlideThreshold;
/** How long since last slide before sound/effect can be re-triggered */
var(Slide)		float						SlideReFireDelay;
/** Effect to place at contact position and enable while sliding */
var(Slide)		ParticleSystem				SlideEffect;
/** Looping sound to play while objects are sliding */
var(Slide)		SoundCue					SlideSound;

// Fracture effects

/** Sound cue to play when multiple fracture parts are dislodged from a mesh */
var(Fracture)	SoundCue					FractureSoundExplosion;
/** Sound cue to play when a single piece is removed from mesh. */
var(Fracture)	SoundCue					FractureSoundSingle;


/**
* The PhysicalMaterial objects now have a parent reference / pointer.  This allows
* you to make single inheritance hierarchies of PhysicalMaterials.  Specifically
* this allows one to set default data and then have subclasses over ride that data.
* (e.g.  For all materials in the game we are going to say the default Impact Sound
* is SoundA.  Now for a Tin Shed we can make a Metal Physical Material and set its
* parent pointer to the Default Material.  And then for our Metal PhysicalMaterial
* we say:  Play SoundB for Pistols and Rifles.  Leaving everything else blank, our
* code can now traverse up the tree to the Default PhysicalMaterial and read the
* values out of that.
*
* This allows for very specific and interesting behavior that is for the most part
* completely in the hands of your content creators.
*
* A programmer is needed only to create the orig set of parameters and then it is
* all data driven parameterization!
*
**/
var(Parent) PhysicalMaterial Parent;

var(PhysicalProperties) export editinline PhysicalMaterialPropertyBase PhysicalMaterialProperty;



cpptext
{
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void FinishDestroy();

	/**
     * This will fix any old PhysicalMaterials that were created in the PhysMaterial's outer instead
	 * of correctly inside the PhysMaterial.  This will allow "broken" PhysMaterials to be renamed.
	 **/
	virtual UBOOL Rename( const TCHAR* NewName=NULL, UObject* NewOuter=NULL, ERenameFlags Flags=REN_None );
}



/** Enum to differentiate between impact and slide effects. */
enum EPhysEffectType
{
	EPMET_Impact,
	EPMET_Slide
};

/** Walk up the PhysMat heirarchy to fill in the supplied PhysEffectInfo struct. */
function native PhysEffectInfo FindPhysEffectInfo(EPhysEffectType Type);

/** Look up PhysicalMaterial heriarchy to find fracture sounds */
simulated function FindFractureSounds(out SoundCue OutSoundExplosion, out SoundCue OutSoundSingle)
{
	local PhysicalMaterial TestMat;

	OutSoundExplosion = None;
	OutSoundSingle = None;
	
	// keep looking until we find all sounds or run out of materials
	TestMat = self;
	while( (OutSoundExplosion == None || OutSoundSingle == None) && TestMat != None )
	{
		// For each empty slot, atempt to fill from this phys material

		if(OutSoundSingle == None)
		{
			OutSoundSingle = TestMat.FractureSoundSingle;
		}

		if(OutSoundExplosion == None)
		{
			OutSoundExplosion = TestMat.FractureSoundExplosion;
		}

		TestMat = TestMat.Parent;
	}

	return;
}

/** finds a physical material property of the desired class, querying the parent if this material doesn't have it
 * @param DesiredClass the class of physical material property to search for
 * @return a PhysicalMaterialPropertyBase matching the desired class, or none if there isn't one
 */
simulated function PhysicalMaterialPropertyBase GetPhysicalMaterialProperty(class<PhysicalMaterialPropertyBase> DesiredClass)
{
	if (PhysicalMaterialProperty != None && ClassIsChildOf(PhysicalMaterialProperty.Class, DesiredClass))
	{
		return PhysicalMaterialProperty;
	}
	else if (Parent != None)
	{
		return Parent.GetPhysicalMaterialProperty(DesiredClass);
	}
	else
	{
		return None;
	}
}

defaultproperties
{
	Friction=0.7
	Restitution=0.3

	Density=1.0
	AngularDamping=0.0
	LinearDamping=0.01
	MagneticResponse=0.0
	WindResponse=0.0
}
