/*=============================================================================
 UE3 extensions for NSString.
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#import <Foundation/NSString.h>

class FString;

@interface NSString (FString_Extensions)

/**
 * Converts an FString to an NSString
 */
+ (NSString*) stringWithFString:(const FString&)MyFString;

@end

