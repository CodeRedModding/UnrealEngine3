/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Per object config provider that exposes dynamic playlists to the UI system
 */
class OnlinePlaylistGameTypeProvider extends UIResourceDataProvider
	PerObjectConfig
	Config(Playlist);

/** Unique name identifier for this PlayerlistGameType */
var	config Name PlaylistGameTypeName;

/** Localized name for the gamemode */
var config localized String DisplayName;

/** Localized description for the gamemode */
var config localized String Description;

/** Unique identifier for this game type */
var	config int GameTypeId;
