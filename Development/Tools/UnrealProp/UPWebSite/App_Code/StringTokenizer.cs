/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Data;
using System.Configuration;
using System.Web;
using System.Web.Security;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Web.UI.WebControls.WebParts;
using System.Web.UI.HtmlControls;
using RemotableType;
using UnrealProp;

public class StringTokenizer
{
    private string StringToTokenize;
    private string Separator;
    private int BeginIndex = -1;
    private int CurrentIndex = -1;

    public StringTokenizer( string str, string separator )
    {
        StringToTokenize = str;
        Separator = separator;
    }

    public bool NextToken()
    {
        if( CurrentIndex == StringToTokenize.Length )
        {
            return false;
        }

        BeginIndex = CurrentIndex + 1;
        CurrentIndex = StringToTokenize.IndexOf( Separator, CurrentIndex + 1 );

        if( CurrentIndex == -1 )
        {
            CurrentIndex = StringToTokenize.Length;
        }

        return true;
    }

    public string GetToken()
    {
        if( BeginIndex != -1 )
        {
            return StringToTokenize.Substring( BeginIndex, CurrentIndex - BeginIndex );
        }
        else
        {
            return "";
        }
    }
}

