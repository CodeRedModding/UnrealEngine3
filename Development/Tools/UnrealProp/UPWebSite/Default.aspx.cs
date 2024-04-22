using System;
using System.Configuration;
using System.Collections;
using System.Data;
using System.Diagnostics;
using System.Web;
using System.Web.Security;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Web.UI.WebControls.WebParts;
using System.Web.UI.HtmlControls;
using RemotableType;
using UnrealProp;

public partial class Web_Default : System.Web.UI.Page
{
    public void ReadNews()
    {
		try
		{
			using( HtmlTextWriter Writer = new HtmlTextWriter( Response.Output ) )
			{
				Writer.WriteLine( Global.News_Get() );
				Writer.Close();
			}
		}
		catch
		{
			Response.Redirect( "Offline.aspx" );
		}
    }
}
