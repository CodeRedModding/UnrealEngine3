/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 */
class OnlineImageDownloaderWeb extends Object
	config(Engine);

`include(Engine\Classes\HttpStatusCodes.uci)

enum EOnlineImageDownloadState
{
	/** Download has not been kicked off yet */
	PIDS_NotStarted,
	/** Currently waiting for download to finish */
	PIDS_Downloading,
	/** Downloaded successfully and Texture is ready to be used */
	PIDS_Succeeded,
	/** Download failed. Can't assume Texture contents are valid/updated */
	PIDS_Failed
};

/** Entry for a cached FB profile image */
struct OnlineImageDownload
{
	/** URL for the image */
	var string URL;
	/** HTTP request object used to download the image */
	var HttpRequestInterface HTTPRequest;
	/** Current download state */
	var EOnlineImageDownloadState State;
	/** Marked for deletion/reuse */
	var bool bPendingRemoval;
	/** Texture that will be updated with the downloaded image */
	var Texture2DDynamic Texture;
};
/** Cache of textures images and the web requests used to download them */
var array<OnlineImageDownload> DownloadImages;
/** Maximum downloads that can be in flight at the same time */
var config int MaxSimultaneousDownloads;

/**
 * Called whenever a download for an image has completed
 *
 * @param OnlineImageDownload cached entry that was downloaded
 */
delegate OnOnlineImageDownloaded(OnlineImageDownload CachedEntry);

/**
 * Retrieve the texture for a given image URL if it has been successfully downloaded and is still cached
 *
 * @param URL original url of image request
 */
function Texture GetOnlineImageTexture(string URL)
{
	local int FoundIdx;

	FoundIdx = DownloadImages.Find('URL',URL);
	if (FoundIdx != INDEX_NONE &&
		DownloadImages[FoundIdx].State == PIDS_Succeeded)
	{
		return DownloadImages[FoundIdx].Texture;
	}
	return None;
}

/**
 * Start the downloading/caching of the images for the given list of URLs
 *
 * @param URLs list of addresses to download
 */
function RequestOnlineImages(array<string> URLs)
{
	local string URL;
	local int FoundIdx,Idx;

	// Start by marking any entries no longer needed for removal
	for (Idx=0; Idx < DownloadImages.Length; Idx++)
	{
		// If existing User id entry not in new list of user ids then mark for removal
		DownloadImages[Idx].bPendingRemoval = URLs.Find(DownloadImages[Idx].URL) == INDEX_NONE;
	}
	// Update/Add new user ids that are not being processed
	foreach URLs(URL)
	{	
		FoundIdx = DownloadImages.Find('URL',URL);
		// If found existing entry then just treat as if downloaded
		if (FoundIdx != INDEX_NONE)
		{
			OnOnlineImageDownloaded(DownloadImages[FoundIdx]);
		}
		// If no existing cached entry then need to update/add
		else
		{
			// Find an entry marked for removal
			FoundIdx = DownloadImages.Find('bPendingRemoval',true);
			if (FoundIdx == INDEX_NONE)
			{
				// Add a new entry since no empty spots 
				FoundIdx = DownloadImages.Length;
				DownloadImages.Length = DownloadImages.Length+1;
			}
			// Setup new cached image entry for user
			DownloadImages[FoundIdx].URL = URL;
			DownloadImages[FoundIdx].HTTPRequest = None;
			DownloadImages[FoundIdx].State = PIDS_NotStarted;
			DownloadImages[FoundIdx].bPendingRemoval = false;
			if (DownloadImages[FoundIdx].Texture == None)
			{
				DownloadImages[FoundIdx].Texture = class'Texture2DDynamic'.static.Create(50,50);
			}
		}
	}
	// Remove the unused entries
	for (Idx=0; Idx < DownloadImages.Length; Idx++)
	{
		if (DownloadImages[Idx].bPendingRemoval)
		{
			DownloadImages.Remove(Idx--,1);
		}
	}
	// Try to start next download
	DownloadNextImage();
}

/**
 * @return total # of entries that are still being downloaded
 */
function int GetNumPendingDownloads()
{
	local int Idx,Count;

	for (Idx=0; Idx < DownloadImages.Length; Idx++)
	{
		if (DownloadImages[Idx].State == PIDS_Downloading)
		{
			Count++;
		}
	}
	return Count;
}

/**
 * Clear out the cached entries for the given user ids
 *
 * @param FBUserIds list of FB ids to clear
 */
function ClearDownloads(array<string> URLs)
{
	local int Idx;

	// Remove the entries matching the FB user ids
	for (Idx=0; Idx < DownloadImages.Length; Idx++)
	{
		if (URLs.Find(DownloadImages[Idx].URL) != INDEX_NONE)
		{
			DownloadImages.Remove(Idx--,1);
		}
	}
}

/**
 * Clear out all of the cached entries. Even if currently in flight
 */
function ClearAllDownloads()
{
	DownloadImages.Length = 0;
}

/**
 * Kick off the download for the next image.  Up to MaxSimultaneousDownloads can be in flight at once
 */
private function DownloadNextImage()
{
	local int Idx,PendingDownloads;

	// Current pending downloads
	PendingDownloads = GetNumPendingDownloads();
	for (Idx=0; Idx < DownloadImages.Length; Idx++)
	{
		// Stop if we cant download any more
		if (PendingDownloads >= MaxSimultaneousDownloads)
		{
			break;
		}
		// Find next available entry that needs to be processed
		if (DownloadImages[Idx].State == PIDS_NotStarted)
		{
			//`log("FacebookImage:DownloadNextImage:2");

			DownloadImages[Idx].HTTPRequest = class'HttpFactory'.static.CreateRequest();
			if (DownloadImages[Idx].HTTPRequest != None)
			{
				DownloadImages[Idx].HTTPRequest.SetVerb("GET");
				DownloadImages[Idx].HTTPRequest.SetURL(DownloadImages[Idx].URL);
				DownloadImages[Idx].HTTPRequest.SetProcessRequestCompleteDelegate(OnDownloadComplete);
				if (DownloadImages[Idx].HTTPRequest.ProcessRequest())
				{
					DownloadImages[Idx].State = PIDS_Downloading;
					PendingDownloads++;
				}
			}
		}
	}
}

/**
 * Called when the download has completed for a single image
 *
 * @param OriginalRequest HTTP request object that was used to kick off the web request
 * @param Response contains the response code, headers, and data from the GET
 * @param bDidSucceed TRUE if the request completed
 */
private function OnDownloadComplete(HttpRequestInterface OriginalRequest, HttpResponseInterface Response, bool bDidSucceed)
{
	local int FoundIdx;
	local array<byte> JPEGData;

	// Match up the request object in the list of cached entries
	FoundIdx = DownloadImages.Find('HTTPRequest',OriginalRequest);
	if (FoundIdx != INDEX_NONE)
	{
		// Check for valid/successful response, and that the contents are a JPEG file
		if (bDidSucceed &&
			Response != None &&
			Response.GetResponseCode() == `HTTP_STATUS_OK &&
			InStr(Response.GetHeader("Content-Type"),"jpeg",false,true) != INDEX_NONE)
		{
			// Mark successful completion
			DownloadImages[FoundIdx].State = PIDS_Succeeded;
			// Copy JPEG image data 
			Response.GetContent(JPEGData);
			
			// Update the texture mip with the image data
			DownloadImages[FoundIdx].Texture.UpdateMipFromJPEG(0,JPEGData);
		}
		else
		{
			// Failed to download
			DownloadImages[FoundIdx].State = PIDS_Failed;
		}
		// Delegate called when download completed
		OnOnlineImageDownloaded(DownloadImages[FoundIdx]);
		// Done downloading so no longer need the request object
		DownloadImages[FoundIdx].HTTPRequest = None;
	}
	// Try to start next download
	DownloadNextImage();
}

/**
 * Debug draw the images that have downloaded
 *
 * @param Canvas used to draw the profile image textures on screen
 */
function DebugDraw(Canvas Canvas)
{
	local float PosX,PosY;
	local int Idx;

	PosX=0;
	PosY=0;
	for (Idx=0; Idx < DownloadImages.Length; Idx++)
	{
		if (DownloadImages[Idx].State == PIDS_Succeeded)
		{
			Canvas.SetDrawColor(255,255,255,255);
			Canvas.SetPos(PosX,PosY);
			Canvas.DrawTexture(DownloadImages[Idx].Texture,1);
			Canvas.SetDrawColor(0,255,0,255);
			Canvas.SetPos(PosX,PosY);
			Canvas.DrawBox(DownloadImages[Idx].Texture.SizeX,DownloadImages[Idx].Texture.SizeY);
			PosY += DownloadImages[Idx].Texture.SizeY;
		}
		else
		{
			Canvas.DrawBox(50,50);
			PosY += 50;
		}
		if (PosY > Canvas.ClipY)
		{
			PosY = 0;
			PosX += 50;
		}
	}
}

defaultproperties
{	
}

