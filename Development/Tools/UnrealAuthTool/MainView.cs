using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Net;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Web;
using System.Windows.Forms;
using System.IO;

namespace UnrealAuthTool
{
	public partial class MainView : Form
	{
	    private String AccessToken;

		private void DidNav(object sender, WebBrowserNavigatedEventArgs Event)
		{
            try
            {
                // is this the special final authorized redirection?
                if (Event.Url.Host == "www.facebook.com" && Event.Url.AbsolutePath == "/connect/login_success.html")
                {
                    AccessToken = HttpUtility.ParseQueryString(Event.Url.Fragment.Replace('#', '?'))["access_token"];
                    if (AccessToken == null)
                    {
                        throw new ApplicationException("No access token found: " + Event.Url.AbsolutePath);
                    }
                    Clipboard.SetText(AccessToken);
                }
            }
            catch (Exception e)
            {
                Clipboard.SetText(string.Format("error\r\n{0}", e));
            }
		}

	    public MainView(string AppId, string Permissions)
		{
			InitializeComponent();

			// build the authentication URL
			var URL = string.Format("https://www.facebook.com/dialog/oauth?client_id={0}&response_type=token&redirect_uri=http://www.facebook.com/connect/login_success.html", AppId);
			if (Permissions != null)
			{
				URL += "&scope=" + Permissions;
			}

			Browser.Navigate(URL);
        }
	}
}
