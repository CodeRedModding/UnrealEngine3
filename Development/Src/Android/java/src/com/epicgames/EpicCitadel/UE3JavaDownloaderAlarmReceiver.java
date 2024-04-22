/*=============================================================================
	UnrealEngine3.java: The java expansion file downloader implementation for Android.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
package com.epicgames.EpicCitadel;

import android.content.Context;
import android.content.Intent;

import com.google.android.vending.expansion.downloader.DownloaderClientMarshaller;
import android.content.BroadcastReceiver;
import android.content.pm.PackageManager.NameNotFoundException;

public class UE3JavaDownloaderAlarmReceiver extends BroadcastReceiver 
{
    @Override
    public void onReceive(Context context, Intent intent) 
    {
        try 
        {
            DownloaderClientMarshaller.startDownloadServiceIfRequired(context, intent, UE3JavaFileDownloader.class);
        } 
        catch (NameNotFoundException e) 
        {
            e.printStackTrace();
        }      
    }
}