class MaterialInstanceConstant extends MaterialInstance
	native(Material)
	hidecategories(Object)
	collapsecategories;

struct native VectorParameterValue
{
	var() name			ParameterName;
	var() LinearColor	ParameterValue;
};

struct native ScalarParameterValue
{
	var() name	ParameterName;
	var() float	ParameterValue;
};

var() const MaterialInstance				Parent;
var() const array<VectorParameterValue>		VectorParameterValues;
var() const array<ScalarParameterValue>		ScalarParameterValues;

var const native MaterialInstancePointer	MaterialInstance;
var private const native bool				ReentrantFlag;

cpptext
{
	// Constructor.

	UMaterialInstanceConstant();

	// UMaterialInstance interface.

	virtual UMaterial* GetMaterial();
	virtual DWORD GetLayerMask();
	virtual FMaterial* GetMaterialInterface(UBOOL Selected);
	virtual FMaterialInstance* GetInstanceInterface() { return MaterialInstance; }
	virtual bool SetVectorParameterValue(const FString& ParameterName,FColor Value);
	virtual bool SetScalarParameterValue(const FString& ParameterName,float Value);
	void SetParent(UMaterialInstance* NewParent);

	// UObject interface.

	virtual void PostLoad();
	virtual void PostEditChange(UProperty* PropertyThatChanged);
	virtual void Destroy();
}

// SetParent - Updates the parent.

native function SetParent(MaterialInstance NewParent);

// Set*ParameterValue - Updates the entry in ParameterValues for the named parameter, or adds a new entry.

native function SetVectorParameterValue(name ParameterName,LinearColor Value);
native function SetScalarParameterValue(name ParameterName,float Value);
