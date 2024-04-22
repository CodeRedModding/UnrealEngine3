//! @file inputimage.h
//! @brief Substance Air image class used into image inputs
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_INPUTIMAGE_H
#define _SUBSTANCE_AIR_INPUTIMAGE_H

#include "SubstanceAirTypedefs.h"

#pragma pack ( push, 8 )
#include <substance/textureinput.h>
#include <substance/context.h>
#pragma pack ( pop )

#include <SubstanceAirUncopyable.h>

namespace SubstanceAir
{

namespace Details
{
	struct ImageInputToken;
}


//! @brief Substance Air image used into image inputs
//! ImageInput are used by InputInstanceImage (input.h).
//! It contains image content used as input of graph instances.
//! Use ImageInput::ScopedAccess to read/write texture buffer
class ImageInput : Uncopyable
{
public:
	//! @brief Build a new ImageInput from texture descrition
	//! @param texture SubstanceTexture (BLEND platform version), that describes
	//!		the texture. A new buffer is automatically allocated to the proper 
	//!		size. If texture.buffer is NULL the internal buffer is not
	//!		initialized (use ScopedAccess to modify its content). Otherwise,
	//!		texture.buffer is not NULL, its content is copied into the 
	//!		internal buffer (texture.buffer is no more usefull after this call).
	//! @warning If the buffer is provided (SubstanceTexture::buffer!=NULL), it
	//!		is copied and not used directly. 
	//! @see substance/texture.h (BLEND platform version)
	//! @return Return an ImageInput instance or NULL pointer if size or format
	//!		are not valid.
	static std::tr1::shared_ptr<ImageInput> create(const SubstanceTexture& texture);
	
	//! @brief Mutexed texture scoped access
	struct ScopedAccess
	{
		//! @brief Constructor from ImageInput to access (thread safe)
		//! @param inputImage The input image to lock, cannot be NULL pointer 
		//! @post The access to inputimage is locked until destructor call.		
		ScopedAccess(const std::tr1::shared_ptr<ImageInput>& inputImage);

		//! @brief Destructor, unlock ImageInput access
		//! @warning Do not modify buffer outside ScopedAccess scope
		~ScopedAccess();
	
		//! @brief Accessor on texture description
		//! @warning Do not delete buffer pointer. However its content can be 
		//!		freely modified inside ScopedAccess scope.
		const SubstanceTexture* operator->() const;
		
		//! @brief Helper: returns buffer content size in bytes
		size_t getSize() const { return mImageInput->mBufferSize; }
		
	protected:
		const std::tr1::shared_ptr<ImageInput> mImageInput;
	};
	
	//! @brief Destructor
	//! @warning Do not delete this class directly if it was already set
	//!		into a InputImageImage: it can be still used in rendering process.
	//!		Use shared pointer mechanism instead.
	~ImageInput();

	bool resolveDirty();                            //!< Internal use only
	Details::ImageInputToken* getToken() const;     //!< Internal use only

protected:

	aligned_binary_t mBufferData;

	size_t mBufferSize;

	bool mDirty;

	Details::ImageInputToken *mImageInputToken;

	ImageInput(const SubstanceTexture& texture);
};


} // namespace SubstanceAir

#endif //_SUBSTANCE_AIR_INPUTIMAGE_H
