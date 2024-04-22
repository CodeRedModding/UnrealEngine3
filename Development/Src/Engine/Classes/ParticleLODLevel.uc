/**
 *	ParticleLODLevel
 *
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleLODLevel extends Object
	native(Particle)
	collapsecategories
	hidecategories(Object)
	editinlinenew;

/** The index value of the LOD level												*/
var const				int						Level;

/** True if the LOD level is enabled, meaning it should be updated and rendered.	*/
var						bool					bEnabled;

/** The required module for this LOD level											*/
var editinline export	ParticleModuleRequired	RequiredModule;

/** An array of particle modules that contain the adjusted data for the LOD level	*/
var editinline export	array<ParticleModule>	Modules;

// Module<SINGULAR> used for emitter type "extension".
var				export	ParticleModule			TypeDataModule;

/** The SpawnRate/Burst module - required by all emitters. */
var				export	ParticleModuleSpawn		SpawnModule;

/** The optional EventGenerator module. */
var				export	ParticleModuleEventGenerator	EventGenerator;

/** SpawningModules - These are called to determine how many particles to spawn.	*/
var native				array<ParticleModuleSpawnBase>	SpawningModules;
/** SpawnModules - These are called when particles are spawned.						*/
var native				array<ParticleModule>			SpawnModules;
/** UpdateModules - These are called when particles are updated.					*/
var native				array<ParticleModule>			UpdateModules;

/** OrbitModules 
 *	These are used to do offsets of the sprite from the particle location.
 */
var native				array<ParticleModuleOrbit>		OrbitModules;

/** Event receiver modules only! */
var native				array<ParticleModuleEventReceiverBase>	EventReceiverModules;

var						bool					ConvertedModules;
var						int						PeakActiveParticles;

cpptext
{
	virtual void	PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	virtual void	PostLoad();
	virtual void	UpdateModuleLists();

	virtual UBOOL	GenerateFromLODLevel(UParticleLODLevel* SourceLODLevel, FLOAT Percentage, UBOOL bGenerateModuleData = TRUE);

	/**
	 *	CalculateMaxActiveParticleCount
	 *	Determine the maximum active particles that could occur with this emitter.
	 *	This is to avoid reallocation during the life of the emitter.
	 *
	 *	@return		The maximum active particle count for the LOD level.
	 */
	virtual INT	CalculateMaxActiveParticleCount();

	/**
	 *	Update to the new SpawnModule method
	 */
	void	ConvertToSpawnModule();
		
	/**
	 *	Return the index of the given module if it is contained in the LOD level
	 */
	INT		GetModuleIndex(UParticleModule* InModule);

	/**
	 *	Return the module at the given index if it is contained in the LOD level
	 */
	UParticleModule* GetModuleAtIndex(INT InIndex);

	/**
	 *	Sets the LOD 'Level' to the given value, properly updating the modules LOD validity settings.
	 */
	virtual void	SetLevelIndex(INT InLevelIndex);

	// For Cascade
	void	AddCurvesToEditor(UInterpCurveEdSetup* EdSetup);
	void	RemoveCurvesFromEditor(UInterpCurveEdSetup* EdSetup);
	void	ChangeEditorColor(FColor& Color, UInterpCurveEdSetup* EdSetup);
	/**
	 *	Return TRUE if the given module is editable for this LOD level.
	 *	
	 *	@param	InModule	The module of interest.
	 *	@return	TRUE		If it is editable for this LOD level.
	 *			FALSE		If it is not.
	 */
	UBOOL	IsModuleEditable(UParticleModule* InModule);

}

defaultproperties
{
	bEnabled=true
	ConvertedModules=true
	PeakActiveParticles=0
}
