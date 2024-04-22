/*=============================================================================
	FacebookIntegration.cpp: Cross platform portion of Facebook integration.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePlatformInterfaceClasses.h"

IMPLEMENT_CLASS(UFacebookIntegration);


/**
 * Default implementations (of nothing)
 */
UBOOL UFacebookIntegration::Init()
{
	return FALSE;
}
UBOOL UFacebookIntegration::Authorize()
{
	return FALSE;
}
UBOOL UFacebookIntegration::IsAuthorized()
{
	return FALSE;
}
void UFacebookIntegration::Disconnect()
{
}
void UFacebookIntegration::FacebookRequest(const FString& GraphRequest)
{
}
void UFacebookIntegration::FacebookDialog(const FString& Action, const TArray<FString>& ParamKeysAndValues)
{
}





IMPLEMENT_CLASS(UJsonObject);

/**
 *  @return the JsonObject associated with the given key, or NULL if it doesn't exist
 */
UJsonObject* UJsonObject::GetObject(const FString& Key)
{
	return ObjectMap.FindRef(Key);
}

/**
 *  @return the value (string) associated with the given key, or "" if it doesn't exist
 */
FString UJsonObject::GetStringValue(const FString& Key)
{
	FString* Result = ValueMap.Find(Key);
	return Result ? *Result : FString(TEXT(""));
}


void UJsonObject::SetObject(const FString& Key, UJsonObject* Object)
{
	ObjectMap.Set(Key, Object);
}

void UJsonObject::SetStringValue(const FString& Key, const FString& Value)
{
	ValueMap.Set(Key, Value);
}

/**
 * @param Key the key to check on this object
 * 
 * @return true if the key exists, false otherwise
 */
UBOOL UJsonObject::HasKey(const FString& Key)
{
	return ValueMap.Find(Key) != NULL ? TRUE : FALSE;
}

/**
 * Encodes an object hierarchy to a string suitable for sending over the web
 *
 * This code written from scratch, with only the spec as a guide. No
 * source code was referenced.
 *
 * @param Root The toplevel object in the hierarchy
 *
 * @return A well-formatted Json string
 */
FString UJsonObject::EncodeJson(UJsonObject* Root)
{
	// an objewct can't be an dictionary and an array
	if (Root->ValueMap.Num() + Root->ObjectMap.Num() > 0 && 
		Root->ValueArray.Num() + Root->ObjectArray.Num() > 0)
	{
		debugf(TEXT("A JsonObject can't have map values and array values"));
		return TEXT("ERROR!");
	}

	UBOOL bIsArray = Root->ValueArray.Num() + Root->ObjectArray.Num() > 0;

	// object starts with a {, unless it's an array
	FString Out = bIsArray ? TEXT("[") : TEXT("{");

	UBOOL bIsFirst = TRUE;

	if (bIsArray)
	{
		for (INT Index = 0; Index < Root->ValueArray.Num(); Index++)
		{
			// comma between elements
			if (!bIsFirst)
			{
				Out += TEXT(",");
			}
			else
			{
				bIsFirst = FALSE;
			}

			// output the key/value pair
			if (Root->ValueArray(Index).StartsWith(TEXT("\\#")))
			{
				// if we have a \#, then export it without the quotes, as it's a number, not a string
				Out += FString::Printf(TEXT("%s"), (*Root->ValueArray(Index)) + 2);
			}
			else
			{
				Out += FString::Printf(TEXT("\"%s\""), *Root->ValueArray(Index));
			}
		}
		for (INT Index = 0; Index < Root->ObjectArray.Num(); Index++)
		{
			// comma between elements
			if (!bIsFirst)
			{
				Out += TEXT(",");
			}
			else
			{
				bIsFirst = FALSE;
			}

			// output the key/value pair
			Out += FString::Printf(TEXT("%s"), *EncodeJson(Root->ObjectArray(Index)));
		}
	}
	else
	{
		for (TMap<FString, FString>::TIterator It(Root->ValueMap); It; ++It)
		{
			// comma between elements
			if (!bIsFirst)
			{
				Out += TEXT(",");
			}
			else
			{
				bIsFirst = FALSE;
			}

			// output the key/value pair
			if (It.Value().StartsWith(TEXT("\\#")))
			{
				// if we have a \#, then export it without the quotes, as it's a number, not a string
				Out += FString::Printf(TEXT("\"%s\":%s"), *It.Key(), (*It.Value()) + 2);
			}
			else
			{
				const FString& K = It.Key();
				const FString& V = It.Value();
				Out += FString::Printf(TEXT("\"%s\":\"%s\""), *It.Key(), *It.Value());
			}
		}

		for (TMap<FString, UJsonObject*>::TIterator It(Root->ObjectMap); It; ++It)
		{
			// comma between elements
			if (!bIsFirst)
			{
				Out += TEXT(",");
			}
			else
			{
				bIsFirst = FALSE;
			}

			// output the key/value pair
			Out += FString::Printf(TEXT("\"%s\":%s"), *It.Key(), *EncodeJson(It.Value()));
		}
	}

	Out += bIsArray ? TEXT("]") : TEXT("}");
	return Out;
}

/**
 * Parse a string from a Json string, starting at the given location
 */
static FString ParseString(const FString& InStr, INT& Index)
{
	const TCHAR* Str = *InStr;
	// skip the opening quote
	check(Str[Index] == '\"');
	Index++;

	// loop over the string until a non-quoted close quote is found
	UBOOL bDone = FALSE;
	FString Result;
	while (!bDone)
	{
		// handle a quoted character
		if (Str[Index] == '\\')
		{
			// skip the quote, and deal with it depending on next character
			Index++;
			switch (Str[Index])
			{
				case '\"':
				case '\\':
				case '/':
					Result += Str[Index]; break;
				case 'f':
					Result += '\f'; break;
				case 'r':
					Result += '\r'; break;
				case 'n':
					Result += '\n'; break;
				case 'b':
					Result += '\b'; break;
				case 't':
					Result += '\t'; break;
				case 'u':
					// 4 hex digits, like \uAB23, which is a 16 bit number that we would usually see as 0xAB23
					{
						TCHAR Hex[5];
						Hex[0] = Str[Index + 1]; 
						Hex[1] = Str[Index + 2]; 
						Hex[2] = Str[Index + 3]; 
						Hex[3] = Str[Index + 4]; 
						Hex[4] = 0;
						Index += 4;
						INT Value;
						appSSCANF(Hex, TEXT("%x"), &Value);
						TCHAR UniChar = (TCHAR)Value;
						Result += UniChar;
					}
					break;
				default:
					appErrorf(TEXT("Bad Json escaped char at %s"), Str[Index]);
					break;
			}
		}
		// handle end of the string
		else if (Str[Index] == '\"')
		{
			bDone = TRUE;
		}
		// otherwise, just drop it in to the output
		else
		{
			Result += Str[Index];
		}

		// always skip to next character, even in the bDone case
		Index++;
	}

	// and we are done!
	return Result;
}

/**
 * Skip over any whitespace at current location
 */
static void SkipWhiteSpace(const FString& InStr, INT& Index)
{
	// skip over the white space
	while (appIsWhitespace(InStr[Index]) || InStr[Index] == '\n' || InStr[Index] == '\r')
	{
		Index++;
	}
}

/**
 * Trim trailing spaces,nl,cr
 */
static void TrimTrailingWhiteSpace(FString& InStr)
{
	INT Pos = InStr.Len() - 1;
	while (Pos >= 0)
	{
		if (!appIsWhitespace(InStr[Pos]) && 
			InStr[Pos] != '\n' &&
			InStr[Pos] != '\r')
		{
			break;
		}
		Pos--;
	}
	InStr = InStr.Left(Pos + 1);
}


/**
 * Decodes a Json string into an object hierarchy (all needed objects will be created)
 *
 * This code written from scratch, with only the spec as a guide. No
 * source code was referenced.
 *
 * @param Str A Json string (probably received from the web)
 *
 * @return The root object of the resulting hierarchy
 */
UJsonObject* UJsonObject::DecodeJson(const FString& InJsonString)
{
	// strip off any white space
	FString JsonString = InJsonString;
	JsonString.Trim();
	TrimTrailingWhiteSpace(JsonString);

	// get a pointer to the data
	const TCHAR* Str = *JsonString;

	// object or array start is required here
	if (Str[0] != '{' && Str[0] != '[' &&
		Str[JsonString.Len() - 1] != '}' && 
		Str[JsonString.Len() - 1] != ']')
	{
		debugf(TEXT("Poorly formed Json string at %s"), Str);
		return NULL;
	}

	// make a new object to hold the contents
	UJsonObject* NewObj = ConstructObject<UJsonObject>(UJsonObject::StaticClass());

	UBOOL bIsArray = Str[0] == '[';

	// parse the data, skipping over the opening bracket/brace
	INT Index = 1;
	while (Index < JsonString.Len() - 1)
	{
		SkipWhiteSpace(JsonString, Index);

		// if this is an dictionary, parse the key
		FString Key;
		if (!bIsArray)
		{
			Key = ParseString(JsonString, Index);
			SkipWhiteSpace(JsonString, Index);

			// this needs to be a :, skip over it
			check(Str[Index] == ':');
			Index++;

			SkipWhiteSpace(JsonString, Index);
		}

		// now read the value (for arrays and dictionaries, we are now pointing to a value)
		if (Str[Index] == '{' || Str[Index] == '[')
		{
			// look for the end
			INT EndOfSubString = Index;
			INT ObjectAndArrayStack = 0;
			// look for the end of the sub object/array 
			do 
			{
				// the start of an object or array increments the stack count
				if (Str[EndOfSubString] == '{' || Str[EndOfSubString] == '[')
				{
					ObjectAndArrayStack++;
				}
				// the end of an object or array decrements the stack count
				else if (Str[EndOfSubString] == '}' || Str[EndOfSubString] == ']')
				{
					ObjectAndArrayStack--;
				}
				EndOfSubString++;

			// go until the stack is finalized to 0
			} while (ObjectAndArrayStack > 0);

			// now we have the end of an object, recurse on that string
			FString SubString = JsonString.Mid(Index, EndOfSubString - Index);
			UJsonObject* SubObject = DecodeJson(SubString);

			// put the object into the arry or dictionary
			if (bIsArray)
			{
				NewObj->ObjectArray.AddItem(SubObject);
			}
			else
			{
				NewObj->ObjectMap.Set(Key, SubObject);
			}

			// skip past the object and carry on
			Index = EndOfSubString;
		}
		// if we aren't a subojbect, then we are a value (string or number)
		else
		{
			FString Value;
			if (Str[Index] == '\"')
			{
				Value = ParseString(JsonString, Index);
			}
			// handle literals, without quotes, and convert them to what unrealscript expects for bools
			else if (appStrncmp(Str + Index, TEXT("true"), 4) == 0)
			{
				Value = "\\#true";
				Index += 4;
			}
			else if (appStrncmp(Str + Index, TEXT("false"), 5) == 0)
			{
				Value = "\\#false";
				Index += 5;
			}
			// treat the null literal as an empty string (unclear how to handle it)
			else if (appStrncmp(Str + Index, TEXT("null"), 4) == 0)
			{
				Value = "";
				Index += 4;
			}
			else
			{
				// look for the end of the number (first non-valid number character)
				INT EndOfNumber = Index;
				UBOOL bDone = FALSE;
				while (!bDone)
				{
					TCHAR Digit = Str[EndOfNumber];
					// possible number values, according to standard
					if ((Digit >= '0' && Digit <= '9') || 
						Digit == 'E' || Digit == 'e' || 
						Digit == '-' ||	Digit == '+' ||
						Digit == '.')
					{
						EndOfNumber++;
					}
					else
					{
						bDone = TRUE;
					}
				}
				// get the string of all the number parts, prepending \#, since how we denote numbers with unrealscript
				Value = FString("\\#") + JsonString.Mid(Index, EndOfNumber - Index);
				// skip past it
				Index = EndOfNumber;
			}

			// put the object into the arry or dictionary
			if (bIsArray)
			{
				NewObj->ValueArray.AddItem(Value);
			}
			else
			{
				NewObj->ValueMap.Set(Key, Value);
			}
		}

		// skip any whitespace after the value
		SkipWhiteSpace(JsonString, Index);

		// continue if not at the end yet
		if (Index < JsonString.Len() - 1)
		{
			// this needs to be a comma
			checkf(Str[Index] == ',', TEXT("Should be a , but it's %s"), Str + Index);
			Index++;

			// skip whitespace after a comma
			SkipWhiteSpace(JsonString, Index);
		}
	}

	return NewObj;
}

