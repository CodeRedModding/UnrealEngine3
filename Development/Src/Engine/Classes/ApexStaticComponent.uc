/*=============================================================================
	ApexStaticComponent.uc: PhysX APEX integration. Static component
	Copyright 2008-2009 NVIDIA Corporation.
=============================================================================*/

/***
* This class defines the base component object for apex objects
**/
class ApexStaticComponent extends ApexComponentBase
	native(Mesh);

cpptext
{
	public:
		//UObject
		/** Serializes this object
		*
		* @param : Ar the archive object to serialize into or out of.
		*/
		virtual void Serialize(FArchive& Ar);

		/*** Creates a primitive scene proxy for this object.
		*/
		virtual FPrimitiveSceneProxy* CreateSceneProxy();

	protected:
		/**
		* @return	FALSE since fractured geometry will handle its own decal detachment
		*/
		virtual UBOOL AllowDecalRemovalOnDetach() const
		{
			return FALSE;
		}

		friend class FApexStaticSceneProxy;
}

defaultproperties
{
	// By default static components use precomputed shadows.
	bUsePrecomputedShadows=TRUE
}
