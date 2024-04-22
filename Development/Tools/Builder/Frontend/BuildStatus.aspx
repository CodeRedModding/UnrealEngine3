<%@ Page Language="C#" AutoEventWireup="true" CodeFile="BuildStatus.aspx.cs" Inherits="BuildStatus" Debug="true" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" >
<head runat="server">
    <title>The Current Status of the Build</title>
</head>
<body>

    <form id="form1" runat="server">
    <div>
    <center>
        <asp:Label ID="Label1" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="XX-Large"
            Height="56px" Text="Epic Build System - Current Build Status" Width="640px" ForeColor="Blue">
        </asp:Label>
        <br />
        <asp:Label ID="Label_BranchName" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="Large"
            Height="36px" Text="" Width="640px" ForeColor="Blue">
        </asp:Label>
        <br />
            
        <asp:Label ID="Label_Welcome" runat="server" Height="32px" Font-Bold="True" Font-Names="Arial" Font-Size="Small" ForeColor="Blue">
        </asp:Label>
        <br />
        <br />
    </center>    
    </div>
        
    <div>
    <center>
        <asp:Label ID="Label_State" runat="server" Height="48px" Font-Bold="True" Font-Names="Arial" Font-Size="X-Large" ForeColor="Green">
        </asp:Label>
        <br />
        
        <asp:Label ID="Label_Changelist" runat="server" Height="32px" Font-Bold="True" Font-Names="Arial" Font-Size="Large" ForeColor="Green">
        </asp:Label>
        <br />
        <br />
    </center>    
    </div>
        
    <div style="font-family: Consolas; font-size: small;">
    <center>
        <asp:Label ID="Label_CISExamplePC" runat="server" Height="28px" Font-Bold="True" Font-Names="Arial" Font-Size="Medium" ForeColor="Green">
        <br />
        </asp:Label>
    </center>
        <% Response.Write( Errors["Example"] ); %>
    <center>
        <asp:Label ID="Label_CISGearPC" runat="server" Height="28px" Font-Bold="True" Font-Names="Arial" Font-Size="Medium" ForeColor="Green">
        <br />
        </asp:Label>
    </center>
        <% Response.Write( Errors["Gear"] ); %>
    <center>
        <asp:Label ID="Label_CISUDKPC" runat="server" Height="28px" Font-Bold="True" Font-Names="Arial" Font-Size="Medium" ForeColor="Green">
        <br />
        </asp:Label>
    </center>
        <% Response.Write( Errors["UDK"] ); %>
    <center>
        <asp:Label ID="Label_CISMobile" runat="server" Height="28px" Font-Bold="True" Font-Names="Arial" Font-Size="Medium" ForeColor="Green">
        <br />
        </asp:Label>
    </center>
        <% Response.Write( Errors["Mobile"] ); %>
    <center>
        <asp:Label ID="Label_CISXbox360" runat="server" Height="28px" Font-Bold="True" Font-Names="Arial" Font-Size="Medium" ForeColor="Green">
        <br />
        </asp:Label>
    </center>
        <% Response.Write( Errors["Xbox360"] ); %>
    <center>
        <asp:Label ID="Label_CISPS3" runat="server" Height="28px" Font-Bold="True" Font-Names="Arial" Font-Size="Medium" ForeColor="Green">
        <br />
        </asp:Label>
    </center>
        <% Response.Write( Errors["PS3"] ); %>
    <center>
        <asp:Label ID="Label_CISTools" runat="server" Height="28px" Font-Bold="True" Font-Names="Arial" Font-Size="Medium" ForeColor="Green">
        <br />
        </asp:Label>
    </center>
        <% Response.Write( Errors["Tools"] ); %>
    <center>
    </center>    
    </div>
        
    </form>

</body>
</html>

