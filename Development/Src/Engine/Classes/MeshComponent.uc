class MeshComponent extends PrimitiveComponent
	native
	noexport
	abstract;

var(Rendering) const array<MaterialInstance>	Materials;

// GetMaterial - Returns the indexed material used by this mesh.

native function MaterialInstance GetMaterial(int SkinIndex);

// SetMaterial - Changes the indexed material used by this mesh.

native function SetMaterial(int SkinIndex,MaterialInstance Material);

defaultproperties
{
	CastShadow=True
}