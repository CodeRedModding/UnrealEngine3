<%@ Page Language="C#" AutoEventWireup="true" CodeFile="Offline.aspx.cs" Inherits="Offline" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" >
<head runat="server">
    <title>UnrealProp Offline</title>
     <link href="default.css" rel="stylesheet" type="text/css" /> 
</head>
<body>
<center>
    <form id="form1" runat="server">
    <div>
    <asp:Label ID="Label_Title" runat="server" Height="48px" Width="320px" Font-Bold="True" Font-Names="Arial" Font-Size="XX-Large" ForeColor="Blue" Text="UnrealProp" />
    <br />
    <br />
    <br />
    <asp:Label ID="Label_Offline" runat="server" Height="48px" Width="384px" Font-Bold="True" Font-Names="Arial" Font-Size="X-Large" ForeColor="Red" Text="System is currently down for maintenance" />
    <br />
    <br />
    <br />
    <br />
    <asp:Label ID="Label_Explain" runat="server" Height="24px" Width="384px" Font-Bold="False" Font-Names="Arial" Font-Size="Medium" ForeColor="Black" Text="If you think this is in error, please email John Scott and Max Pruessner" />
    </div>
    </form>
</center>
</body>
</html>
