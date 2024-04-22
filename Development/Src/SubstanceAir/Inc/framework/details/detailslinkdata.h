//! @file detailslinkdata.h
//! @brief Substance Air Framework Link data classes definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111031
//! @copyright Allegorithmic. All rights reserved.
//!
 

#ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSLINKDATA_H
#define _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSLINKDATA_H

#include "SubstanceAirPackage.h"
#include "SubstanceAirUncopyable.h"

#include "framework/stacking.h"

#include <vector>
#include <utility>

namespace SubstanceAir
{
namespace Details
{

struct LinkContext;


//! @brief Link data abstract base class
class LinkData : Uncopyable
{
public:
	//! @brief Virtual destructor
	virtual ~LinkData();
		
	//! @brief Push data to link
	//! @param cxt Used to push link data
	virtual UBOOL push(LinkContext& cxt) const = 0;

	//! @brief Size of the resource, in bytes
	virtual INT getSize() {return 0;}

	//! @brief Remove the content of the LinkData
	virtual void clear() {}

};  // class LinkData


//! @brief Link data simple package from assembly class
class LinkDataAssembly : public LinkData
{
public:
	//! @brief Constructor from assembly data
	//! @param ptr Pointer on assembly data
	//! @param size Size of assembly data in bytes
	LinkDataAssembly(const BYTE* ptr, UINT size);
		
	//! @brief Push data to link
	//! @param cxt Used to push link data
	UBOOL push(LinkContext& cxt) const;
	
	//! @brief Force output format/mipmap
	//! @param uid Output uid
	//! @param format New output format
	//! @param mipmap New mipmap count
	void setOutputFormat(UINT uid, int format, UINT mipmap=0);

	//! @brief Size of the resource, in bytes
	INT getSize() {return mAssembly.size();}

	//! @brief Remove the content of the LinkData
	void clear()
	{
		mAssembly.clear();
	}

	//! @brief Accessor to the assembly
	const std::string& getAssembly() const
	{
		return mAssembly;
	}
	
protected:
	//! @brief Output format force entry
	struct OutputFormat
	{
		UINT uid;             //!< Output uid
		INT format;           //!< New output format
		INT mipmap;           //!< New mipmap count
	};  // struct OutputFormat
	
	//! @brief Output format force entries container
	typedef std::vector< OutputFormat > OutputFormats;

	//! @brief Assembly data
	std::string mAssembly;
	
	//! @brief Output formats override
	OutputFormats mOutputFormats;
	
};  // class LinkDataAssembly


//! @brief Link data simple package from stacking class
class LinkDataStacking : public LinkData
{
public:
	//! @brief UID translation container (initial,translated pair)
	typedef std::vector< std::pair<UINT,UINT> > UidTranslates;

	//! @brief Construct from pre and post Graphs, and connection options.
	//! @param preGraph Source pre graph.
	//! @param postGraph Source post graph.
	//! @param connOptions Connections options.
	LinkDataStacking(
		const FGraphDesc* preGraph,
		const FGraphDesc* postGraph,
		const ConnectionsOptions& connOptions);
		
	//! @brief Push data to link
	//! @param cxt Used to push link data
	UBOOL push(LinkContext& cxt) const;
	
	//! @brief Connections options.
	ConnectionsOptions mOptions;
	
	//! @brief Pair of pre,post UIDs to fuse (initial UIDs)
	//! Post UID sorted (second)
	ConnectionsOptions::Connections mFuseInputs;  
	
	//! @brief Disabled UIDs (initial UIDs)
	//! UID sorted
	Uids mDisabledOutputs;
	
	//! @brief Post graph Inputs UID translation (initial->translated)
	//! initial UID sorted
	UidTranslates mTrPostInputs;
	
	//! @brief Pre graph Outputs UID translation (initial->translated)
	//! initial UID sorted
	UidTranslates mTrPreOutputs;
	
protected:
	//! @brief Link data of Source pre graph.
	std::tr1::shared_ptr<LinkData> mPreLinkData;
	
	//! @brief Link data of Source post graph.
	std::tr1::shared_ptr<LinkData> mPostLinkData;
	
};  // class LinkDataStacking


} // namespace Details
} // namespace SubstanceAir

#endif // ifndef _SUBSTANCE_AIR_FRAMEWORK_DETAILS_DETAILSLINKDATA_H
