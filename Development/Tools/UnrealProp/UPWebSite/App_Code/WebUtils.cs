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

static public class WebUtils
{
    static public string GetParamFromCCD( string ListItem, bool NumericValue )
    {
        string Parameter = "-1";

        if( ListItem != null )
        {
			string[] Parameters = ListItem.Split( new string[] { ":::" }, 3, StringSplitOptions.RemoveEmptyEntries );
            if( Parameters.Length == 2 )
            {
				if( NumericValue )
                {
					Parameter = Parameters[0];
                }
                else
                {
					Parameter = Parameters[1].Trim();
                }

                if( Parameter == "" )
                {
                    Parameter = "-1";
                }
            }
        }

        return( Parameter.Trim() );
    }
}

