//! @file SubstanceAirEdCommandlet.h
//!	@brief Substance Air commandlets
//! @contact antoine.gonzalez@allegorithmic.com
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_AIR_ED_COMMANDLETS_H
#define _SUBSTANCE_AIR_ED_COMMANDLETS_H

//! @brief This function search for specified package
//! @param PackageName The name of the package to search for
//! @param OutPath If true is returned, this contains the complete path of the \
//! package found.
//! @return True if the package exists.
UBOOL CommandletFindPackage(const FString& PackageName, FString& OutPath);

//! @brief Commandlet used to do batch textures import
BEGIN_COMMANDLET(ImportSbs,SubstanceAir)
END_COMMANDLET

//! @brief Commandlet used to do batch test scene creation
BEGIN_COMMANDLET(CreateTestMap,SubstanceAir)
void CreateCustomEngine();
END_COMMANDLET

#define AUTO_INITIALIZE_REGISTRANTS_SUBSTANCEAIRED_COMMANDLET \
	UImportSbsCommandlet::StaticClass(); \
	UCreateTestMapCommandlet::StaticClass(); \


#endif //_SUBSTANCE_AIR_ED_COMMANDLETS_H
