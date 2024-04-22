/*=============================================================================
// WebRequest: Parent class for handling/decoding web server requests
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
class WebRequest extends Object
	native;

enum ERequestType
{
	Request_GET,
	Request_POST
};

var string RemoteAddr;
var string URI;
var string Username;
var string Password;
var int ContentLength;
var string ContentType;
var ERequestType RequestType;

var private native const Map_Mirror HeaderMap{TMultiMap<FString, FString>};  // C++ placeholder.
var private native const Map_Mirror VariableMap{TMultiMap<FString, FString>};  // C++ placeholder.

native final function string DecodeBase64(string Encoded);
native final function string EncodeBase64(string Decoded);

native final function AddHeader(string HeaderName, coerce string Value);
native final function string GetHeader(string HeaderName, optional string DefaultValue);
native final function GetHeaders(out array<string> headers);

native final function AddVariable(string VariableName, coerce string Value);
native final function string GetVariable(string VariableName, optional string DefaultValue);
native final function int GetVariableCount(string VariableName);
native final function string GetVariableNumber(string VariableName, int Number, optional string DefaultValue);
native final function GetVariables(out array<string> varNames);

native final function Dump(); // only works in dev mode

function ProcessHeaderString(string S)
{
	local int i;

	//`log("ProcessHeaderString"@S);
	if(Left(S, 21) ~= "Authorization: Basic ")
	{
		S = DecodeBase64(Mid(S, 21));
		i = InStr(S, ":");
		if(i != -1)
		{
			Username = Left(S, i);
			Password = Mid(S, i+1);
			//`log("User:"@Username@"Password:"@Password);
		}
	}
	else if(Left(S, 16) ~= "Content-Length: ")
	{
		ContentLength = Int(Mid(S, 16, 64));
	}
	else if(Left(S, 14) ~= "Content-Type: ")
	{
		ContentType = Mid(S, 14);
	}

	i = InStr(S, ":");
	if (i > -1)
	{
		AddHeader(Left(S, i),  Mid(S, i+2)); // 2 = ": "
	}
}

function DecodeFormData(string Data)
{
	local string Token[2], ch;
	local int i, H1, H2, limit;
	local int t;

	//`log("DecodeFormData"@Data);
	t = 0;
	for( i = 0; i < Len(Data); i++ )
	{
		if ( limit > class'WebConnection'.default.MaxValueLength || i > class'WebConnection'.default.MaxLineLength )
			break;

		ch = mid(Data, i, 1);
		switch(ch)
		{
		case "+":
			Token[t] $= " ";
			limit++;
			break;

		case "&":
		case "?":
			if(Token[0] != "")
				AddVariable(Token[0], Token[1]);

			Token[0] = "";
			Token[1] = "";
			t = 0;

			limit=0;
			break;

		case "=":
			if(t == 0)
			{
				limit = 0;
				t = 1;
			}
			else
			{
				Token[1] $= "=";
				limit++;
			}

			break;

		case "%":
			H1 = GetHexDigit(Mid(Data, ++i, 1));
			if ( H1 != -1 )
			{
				limit++;
				H1 *= 16;
				H2 = GetHexDigit(Mid(Data,++i,1));
				if ( H2 != -1 )
					Token[t] $= Chr(H1 + H2);
			}

			limit++;
			break;

		default:
			Token[t] $= ch;
			limit++;
		}
	}

	if(Token[0] != "")
		AddVariable(Token[0], Token[1]);
}

function int GetHexDigit(string D)
{
	switch(caps(D))
	{
	case "0": return 0;
	case "1": return 1;
	case "2": return 2;
	case "3": return 3;
	case "4": return 4;
	case "5": return 5;
	case "6": return 6;
	case "7": return 7;
	case "8": return 8;
	case "9": return 9;
	case "A": return 10;
	case "B": return 11;
	case "C": return 12;
	case "D": return 13;
	case "E": return 14;
	case "F": return 15;
	}

	return -1;
}

defaultproperties
{
}
