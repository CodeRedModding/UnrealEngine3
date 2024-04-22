//! @file SubstanceAirInstanceFactory.uc
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.
//!
//! @brief the interface to a Substance Air Package

class SubstanceAirInstanceFactory extends Object
	native(InstanceFactory)
	hidecategories(Object);

// native code structure describing a package
// it contains the Substance Air data, the graphs and their instances
var native pointer SubstancePackage{struct SubstanceAir::FPackage};

cpptext
{
public:

	// UObject interface.
	virtual void InitializeIntrinsicPropertyValues();
	virtual void Serialize(FArchive& Ar);
	virtual void BeginDestroy();
	virtual void PostDuplicate();

	virtual INT GetResourceSize();
}
