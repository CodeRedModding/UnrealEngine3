/*=============================================================================
	ApexDynamicComponent.uc: PhysX APEX integration. Dynamic Component
	Copyright 2008-2009 NVIDIA Corporation.
=============================================================================*/

/* This class defines a default APEX dynamic component */
class ApexDynamicComponent extends ApexComponentBase
	native(Mesh);

/* Render resources used by this component, and whose release progress is tracked by the FRenderCommandFence in FracturedBaseComponent. */
var protected{protected} const native transient pointer ComponentDynamicResources{class FApexDynamicResources};

cpptext
{
public:

protected:

	friend class FApexDynamicSceneProxy;
}

defaultproperties
{
}
