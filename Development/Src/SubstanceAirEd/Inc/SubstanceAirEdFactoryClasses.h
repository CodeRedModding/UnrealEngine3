//! @file SubstanceAirEdFactoryClasses.h
//! @brief Factory for Substance Air Textures
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_ED_FACTORIES_H_
#define _SUBSTANCE_AIR_ED_FACTORIES_H_

class USubstanceAirInstanceFactory;
class USubstanceAirImageInput;


class USubstanceAirImportFactory : public UFactory
{
	DECLARE_CLASS_INTRINSIC(USubstanceAirImportFactory,UFactory,CLASS_CollapseCategories,USubstanceAirImportFactory);
public:

	void StaticConstructor();
    
	/**
	 * Initializes property values for intrinsic classes.  It is called 
	 * immediately after the class default object is initialized against 
	 * its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();

    UObject* FactoryCreateBinary(
		UClass* Class,
		UObject* InParent,
		FName Name,
		EObjectFlags Flags,
		UObject* Context,
		const TCHAR* FileName,
		const BYTE*& Buffer,
		const BYTE* BufferEnd,
		FFeedbackContext* Warn);

	UBOOL bCreateMaterial;
	UBOOL bCreateDefaultInstance;
	UBOOL bSpecifyInstancePackage;
};


/**
* Factory to handle texture reimporting from source media to package files
*/
class UReimportSubstanceAirImportFactory : public USubstanceAirImportFactory, FReimportHandler
{
	DECLARE_CLASS_INTRINSIC(UReimportSubstanceAirImportFactory,USubstanceAirImportFactory,CLASS_CollapseCategories,USubstanceAirImportFactory)    

	UReimportSubstanceAirImportFactory();

	USubstanceAirInstanceFactory* pOriginalSubstance;    

public:
	void StaticConstructor();

	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();

	/**
	* Reimports specified texture from its source material, if the meta-data exists
	* @param Package texture to reimport
	*/
	virtual UBOOL Reimport( UObject* Obj );
};


class USubstanceAirImageInputFactory : public UFactory
{
	DECLARE_CLASS_INTRINSIC(USubstanceAirImageInputFactory,UFactory,0,SubstanceAirEd);
public:

	void StaticConstructor();

	void InitializeIntrinsicPropertyValues();

	UObject* FactoryCreateBinary(
		UClass* Class,
		UObject* InParent,
		FName Name,
		EObjectFlags Flags,
		UObject* Context,
		const TCHAR* FileName,
		const BYTE*& Buffer,
		const BYTE* BufferEnd,
		FFeedbackContext* Warn);

protected:
	UObject* USubstanceAirImageInputFactory::FactoryCreateBinaryFromTexture(
		USubstanceAirImageInput* ImageInput,
		UTexture2D* ContextTexture,
		const BYTE*& Buffer,
		const BYTE*	BufferEnd,
		FFeedbackContext* Warn);

	UObject* USubstanceAirImageInputFactory::FactoryCreateBinaryFromTga(
		USubstanceAirImageInput* ImageInput,
		const BYTE*& Buffer,
		const BYTE*	BufferEnd,
		FFeedbackContext* Warn);

	UObject* USubstanceAirImageInputFactory::FactoryCreateBinaryFromJpeg(
		USubstanceAirImageInput* ImageInput,
		const BYTE*& Buffer,
		const BYTE*	BufferEnd,
		FFeedbackContext* Warn);
};


/**
* Factory to handle texture reimporting from source media to package files
*/
class UReimportSubstanceAirImageInputFactory : public USubstanceAirImageInputFactory, FReimportHandler
{
	DECLARE_CLASS_INTRINSIC(UReimportSubstanceAirImageInputFactory,USubstanceAirImageInputFactory,CLASS_CollapseCategories,USubstanceAirImageInputFactory)    

	UReimportSubstanceAirImageInputFactory();

	USubstanceAirImageInput* pOriginalImage;    

public:
	void StaticConstructor();

	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();

	/**
	* Reimports specified image input from its source material, if the meta-data exists
	* @param Package texture to reimport
	*/
	virtual UBOOL Reimport( UObject* Obj );
};


#define AUTO_INITIALIZE_REGISTRANTS_SUBSTANCEAIRED_FACTORIES \
	USubstanceAirImportFactory::StaticClass(); \
	UReimportSubstanceAirImportFactory::StaticClass(); \
	USubstanceAirImageInputFactory::StaticClass(); \
	UReimportSubstanceAirImageInputFactory::StaticClass();

#endif // _SUBSTANCE_AIR_ED_FACTORIES_H_
