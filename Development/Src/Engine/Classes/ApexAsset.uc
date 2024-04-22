/*=============================================================================
	ApexAsset.uc: Wrapper for an NxApexAsset, an APEX base class. Apex Asset
	Copyright 2008-2009 NVIDIA corporation.
=============================================================================*/

/****
* This is the base class for ApexAssets
*
**/
class ApexAsset extends Object
	hidecategories(Object)
	native(Mesh);

var const editinline string OriginalApexName;
var native transient const array<ApexComponentBase> ApexComponents;
var() const editinlineuse editoronly array<ApexAsset>	NamedReferences;
var() const editconst editoronly string SourceFilePath;
var() const editconst editoronly string	SourceFileTimestamp;

cpptext
{
	public:
		/** Display strings for the generic browser */
		virtual	TArray<FString>	GetGenericBrowserInfo();

	   	/** virtual method to return the number of materials used by this asset */
		virtual UINT                GetNumMaterials(void) const   { return 0; }
		/** virtual method to return a particular material by index */
		virtual UMaterialInterface *GetMaterial(UINT Index) const { return 0; }
		/** Returns the default ::NxParameterized::Interface describing the Actor Desc for this asset. */
		virtual void * GetNxParameterized(void) { return 0; };
		/** Returns a *copy* of the :NxParameterized::Interface for this asset. Caller must manually 'destroy' it.*/
		virtual void * GetAssetNxParameterized(void) { return 0; };
		/*** Export asset to a file, in xml/bin format.
		**
		** @param Name: The name of file name for exported asset
		** @param isKeepUE3Coords: Export type, in original coords (true) or keep UE3 coords (false)
		** 
		**/
		virtual UBOOL Export(const FName& Name, UBOOL isKeepUE3Coords){ return true; };
		/** Re-assigns the APEX material resources by name with the current array of UE3 materials */
		virtual void UpdateMaterials(void) { };


		virtual void ResetNamedReferences(void);
		virtual void AddNamedReference(class UApexAsset *obj);
		virtual void RemoveNamedReference(class UApexAsset *obj);
		virtual void NotifyApexEditMode(class ApexEditInterface *iface) { };

		/** Whether the APEX asset's materials can be overridden in the actor's ApexComponent */
		virtual UBOOL SupportsMaterialOverride() const { return FALSE; }

	protected:
		// Called when the Asset gets rebuilt (in editor only).
		void OnApexAssetLost(void);
		void OnApexAssetReset(void);
}

defaultproperties
{
}
