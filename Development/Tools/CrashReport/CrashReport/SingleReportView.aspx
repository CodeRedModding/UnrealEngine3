<%-- // Copyright 1998-2013 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Language="C#" AutoEventWireup="true" CodeBehind="SingleReportView.aspx.cs" Inherits="CrashReport.SingleReportView" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml">
<head runat="server">
    <title></title>
    <script runat="server">
        protected void Page_Load(object sender, EventArgs e)
        {
            Response.Redirect(string.Format("/crashes/ShowOld/{1}", Request.Url.Host, Request.QueryString.Get("rowid")));
        }
</script>

</head>
<body>
    <form id="form1" runat="server">
    <div>
    
    </div>
    </form>
</body>
</html>
