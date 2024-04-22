/*=============================================================================
	UnrealEngine3.java: The java expansion file downloader implementation for Android.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
package com.epicgames.EpicCitadel;

import com.google.android.vending.expansion.downloader.impl.DownloaderService;

public class UE3JavaFileDownloader extends DownloaderService 
{
    // You must use the public key belonging to your publisher account
    public static final String BASE64_PUBLIC_KEY = "YOUR_KEY_HERE";

    // You should also modify this salt
    public static final byte[] SALT = new byte[] { 2, 41, -15, -2, 56, 99,
            -101, -10, 42, 3, -7, -5, 10, 4, -107, -106, -32, 47, -1, 84
    };

    public UE3JavaFileDownloader()
    {
        super();
        UE3JavaApp.IncrementDownloadsCounter();
    }

    @Override
    public String getPublicKey() 
    {
        return BASE64_PUBLIC_KEY;
    }

    @Override
    public byte[] getSALT() 
    {
        return SALT;
    }

    @Override
    public String getAlarmReceiverClassName() 
    {
        return UE3JavaDownloaderAlarmReceiver.class.getName();
    }
}