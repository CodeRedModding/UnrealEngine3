/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __EXPORT_LOCALIZATION_WINDOW_H__
#define __EXPORT_LOCALIZATION_WINDOW_H__

#include "LocalizationExport.h"

namespace ExportLocalizationWindow
{
	/** Helper struct to specify localization export options */
	struct FExportLocalizationOptions
	{
		FLocalizationExportFilter Filter;						// Export filter options
		FString ExportPath;										// Path to export to
		UBOOL bExportBinaries;									// Whether to export binaries or not
		UBOOL bCompareAgainstDefaults;							// Whether to only export properties differing from their defaults or not
	};

	/**
	 * Helper method to prompt the user for localization export options
	 *
	 * @param	OutOptions	Options specified by user via the dialog
	 *
	 * @return	TRUE if the user closed the dialog by specifying to export; FALSE if the user canceled out of the dialog
	 */
	UBOOL PromptForExportLocalizationOptions( FExportLocalizationOptions& OutOptions );
};

#endif // #define __EXPORT_LOCALIZATION_WINDOW_H__
