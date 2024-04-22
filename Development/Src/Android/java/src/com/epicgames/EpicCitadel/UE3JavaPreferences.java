/*=============================================================================
	UE3JavaPreferences.java: Displays a menu for the user to choose game settings
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

package com.epicgames.EpicCitadel;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Bundle;

import android.preference.Preference;
import android.preference.DialogPreference;
import android.preference.PreferenceActivity;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.ListPreference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceScreen;
import android.preference.Preference;

import android.widget.Toast;
import java.util.List;
import org.apache.http.Header;
import android.preference.PreferenceScreen;

import java.util.List;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;

import android.widget.LinearLayout;
import android.view.ViewGroup;
import android.app.AlertDialog.Builder;
import android.app.AlertDialog;
import android.content.DialogInterface;

import java.lang.Integer;
import java.lang.Float;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Toast;


public class UE3JavaPreferences extends PreferenceActivity 
{
	private SharedPreferences Preferences;
	private ListPreference performancePreference;
	private ListPreference resolutionPreference;
	private String oldPerformanceValue;
	
	// called when the user cancels changing quality preference, resets preference
	public void onDialogNegativeClick()
	{
		performancePreference.setValue(oldPerformanceValue);
	}

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		final Activity owningActivity = this;

		super.onCreate(savedInstanceState);
		addPreferencesFromResource(R.xml.preferences);
		setContentView(R.layout.preferencelayout);

		performancePreference	= (ListPreference) findPreference( getString(R.string.PerformancePref_Key) );
		resolutionPreference	= (ListPreference) findPreference( getString(R.string.ResolutionPref_Key) );
		Preference applyButton 	= (Preference) findPreference( getString(R.string.Apply_Button) );

		Preferences = getApplicationContext().getSharedPreferences(getPackageName(), MODE_PRIVATE);

		// Load the shared preferences values into our settings
		ReinitializeMenuSettingsValues();

		performancePreference.setOnPreferenceChangeListener(new OnPreferenceChangeListener()
		{
			public boolean onPreferenceChange(Preference preference, Object newValue)
			{
				int newValueAsInt = Integer.parseInt( newValue.toString() );
				int oldValueAsInt = Integer.parseInt( performancePreference.getValue() );

				if (newValueAsInt > oldValueAsInt)
				{
					oldPerformanceValue = performancePreference.getValue();

					new AlertDialog.Builder(owningActivity)
					.setMessage( "Increasing quality setting may impact application performance." )
					.setTitle("WARNING")
					.setPositiveButton("Ok", null)
					.setNegativeButton("Cancel", new DialogInterface.OnClickListener() 
					{
						public void onClick(DialogInterface dialog, int id) 
						{
							((UE3JavaPreferences)owningActivity).onDialogNegativeClick();
						}
					})
					.show();
				}

				return true;
			}
		});
		
		applyButton.setOnPreferenceClickListener(new OnPreferenceClickListener()
		{
			public boolean onPreferenceClick(Preference preference)
			{
				// Put the temporary settings into the current settings of our shared preferences
				SharedPreferences.Editor editor = Preferences.edit();
				int TempPerformanceValue = Integer.parseInt( performancePreference.getValue() );
				float TempResScaleValue = Float.parseFloat( resolutionPreference.getValue() );

				editor.putInt( UE3JavaApp.currentPerformanceKey, TempPerformanceValue );				
				editor.putFloat( UE3JavaApp.currentResScaleKey, TempResScaleValue );
				editor.commit();

				// make native callback to recompile shaders etc.
				UE3JavaApp.NativeCallback_UpdatePerformanceSettings(TempPerformanceValue, TempResScaleValue);

				Toast.makeText(getApplicationContext(), "Settings may take several minutes to take effect.", Toast.LENGTH_LONG).show();

				finish();
				return true;
			}
		});

	}

	private void ReinitializeMenuSettingsValues()
	{
		// Reset menu settings to what is stored in our shared preferences
		int CurrentPerformanceValue = Preferences.getInt( UE3JavaApp.currentPerformanceKey, 0 );
		float CurrentResScaleValue = Preferences.getFloat( UE3JavaApp.currentResScaleKey, 1 );

		performancePreference.setValue( Integer.toString(CurrentPerformanceValue) );
		resolutionPreference.setValue( Float.toString(CurrentResScaleValue) );
	}

}
