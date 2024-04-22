//! @file ImageInput.cpp
//! @brief Substance Air image class used into image inputs
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "framework/imageinput.h"

#include "framework/details/detailsinputimagetoken.h"

#include <assert.h>
#include <memory.h>

std::tr1::shared_ptr<SubstanceAir::ImageInput>
SubstanceAir::ImageInput::create(const SubstanceTexture& texture)
{
	std::tr1::shared_ptr<SubstanceAir::ImageInput> ptr(new ImageInput(texture));
	
	if (ptr->mImageInputToken->mTexture.buffer==NULL)
	{
		// Return NULL ptr if invalid
		ptr.reset();
	}
	
	return ptr;
}


//! @brief Constructor from ImageInput to access (thread safe)
//! @param ImageInput The input image to lock, cannot be NULL pointer 
//! @post The access to ImageInput is locked until destructor call.		
SubstanceAir::ImageInput::ScopedAccess::ScopedAccess(
		const std::tr1::shared_ptr<ImageInput>& ImageInput) :
	mImageInput(ImageInput)
{
	assert(mImageInput);
	mImageInput->mImageInputToken->lock();
	mImageInput->mDirty = true;
}


//! @brief Destructor, unlock ImageInput access
//! @warning Do not modify buffer outside ScopedAccess scope
SubstanceAir::ImageInput::ScopedAccess::~ScopedAccess()
{
	mImageInput->mImageInputToken->unlock();
}


//! @brief Accessor on texture description
//! @warning Do not delete buffer pointer. However its content can be 
//!		freely modified inside ScopedAccess scope.
const SubstanceTexture* 
SubstanceAir::ImageInput::ScopedAccess::operator->() const
{
	return &mImageInput->mImageInputToken->mTexture;
}


//! @brief Destructor
//! @warning Do not delete this class directly if it was already set
//!		into a ImageInput: it can be still used in rendering process.
//!		Use shared pointer mechanism instead.
SubstanceAir::ImageInput::~ImageInput()
{
	delete mImageInputToken;
}


SubstanceAir::Details::ImageInputToken* 
SubstanceAir::ImageInput::getToken() const
{
	return mImageInputToken;
}


SubstanceAir::ImageInput::ImageInput(const SubstanceTexture& texture) :
	mBufferSize(0),
	mDirty(true),
	mImageInputToken(new Details::ImageInputToken)
{
	const void*const prevBuffer = texture.buffer;
	SubstanceTextureInput &texinp = *mImageInputToken;
	
	texinp.mTexture = texture;
	texinp.level0Width = texinp.mTexture.level0Width;
	texinp.level0Height = texinp.mTexture.level0Height;
	texinp.pixelFormat = texinp.mTexture.pixelFormat;
	texinp.mipmapCount = texinp.mTexture.mipmapCount;
	texinp.mTexture.buffer = NULL;
	
	const size_t nbPixels = 
		texinp.level0Width*texinp.level0Height;
	const size_t nbDxtBlocks = 
		((texinp.level0Width+3)>>2)*((texinp.level0Height+3)>>2);
	
	switch (texinp.pixelFormat&0x1F)
	{
		case Substance_PF_RGBA:
		case Substance_PF_RGBx: mBufferSize = nbPixels*4; break;
		case Substance_PF_RGB : mBufferSize = nbPixels*3; break;
		case Substance_PF_L   : mBufferSize = nbPixels;   break;
		case Substance_PF_RGBA|Substance_PF_16b:
		case Substance_PF_RGBx|Substance_PF_16b: mBufferSize = nbPixels*8; break;
		case Substance_PF_RGB |Substance_PF_16b: mBufferSize = nbPixels*6; break;
		case Substance_PF_L   |Substance_PF_16b: mBufferSize = nbPixels*2; break;
		case Substance_PF_DXT1: mBufferSize = nbDxtBlocks*8;  break;
		case Substance_PF_DXT2:
		case Substance_PF_DXT3:
		case Substance_PF_DXT4:
		case Substance_PF_DXT5:
		case Substance_PF_DXTn: mBufferSize = nbDxtBlocks*16; break;
/*		case Substance_PF_JPEG: mBufferSize = bufferSize; break;*/
	}
	
	if (mBufferSize>0)
	{
		// Create buffer, ensure 16bytes alignment
		mBufferData.resize(mBufferSize+0xF);
		texinp.mTexture.buffer = 
			(void*)((((size_t)&mBufferData[0])+0xF)&~(size_t)0xF);
			
		if (prevBuffer)
		{
			memcpy(texinp.mTexture.buffer,prevBuffer,mBufferSize);
		}
	}
}


//! @brief Internal use only
bool SubstanceAir::ImageInput::resolveDirty()
{
	const bool res = mDirty;
	mDirty = false;
	return res;
}
