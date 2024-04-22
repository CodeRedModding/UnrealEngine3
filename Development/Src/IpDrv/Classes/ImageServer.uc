/*=============================================================================
ImageServer.uc - example image server
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
class ImageServer extends WebApplication;

/* Usage:
[IpDrv.WebServer]
Applications[0]="IpDrv.ImageServer"
ApplicationPaths[0]="/images"
bEnabled=True

http://server.ip.address/images/test.jpg
*/

event Query(WebRequest Request, WebResponse Response)
{
	local string Image;

	Image = Request.URI;
	if (!Response.FileExists(Path $ Image))
	{
		Response.HTTPError(404);
		return;
	}
	else if( Right(Caps(Image), 4) == ".JPG" || Right(Caps(Image), 5) == ".JPEG" )
	{
		Response.SendStandardHeaders("image/jpeg", true);
	}
	else if( Right(Caps(Image), 4) == ".GIF" )
	{
		Response.SendStandardHeaders("image/gif", true);
	}
	else if( Right(Caps(Image), 4) == ".BMP" )
	{
		Response.SendStandardHeaders("image/bmp", true);
	}
	else if( Right(Caps(Image), 4) == ".PNG" )
	{
		Response.SendStandardHeaders("image/png", true);
	}
	else
	{
		Response.SendStandardHeaders("application/octet-stream", true);
	}
	Response.IncludeBinaryFile( Path $ Image );
}

