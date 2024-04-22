/*=============================================================================
 UE3 extensions for NSString.
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#import "NSStringExtensions.h"
#include "Engine.h"

@implementation NSString (FString_Extensions)

+ (NSString*) stringWithFString:(const FString&)MyFString
{
	return [NSString stringWithCString:TCHAR_TO_UTF8(*MyFString) encoding:NSUTF8StringEncoding];
}

@end
