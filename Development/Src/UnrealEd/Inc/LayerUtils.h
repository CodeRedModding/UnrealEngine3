/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LAYERUTILS_H__
#define __LAYERUTILS_H__

// Forward declarations.
class AActor;
class FString;
class UEditorEngine;

/**
 * A set of static functions for common operations on Actor layers.  All functions return
 * a UBOOL indicating whether something changed (an actor was added to or removed form a layer,
 * an actor was selected/deselected, etc).  Returning FALSE does not necessarily mean that
 * an operation failed, but rather nothing changed.
 */
class FLayerUtils
{
public:
	/**
	 * Adds the actor to the named layer.
	 *
	 * @return		TRUE if the actor was added.  FALSE is returned if the
	 *              actor already belongs to the layer.
	 */
	static UBOOL AddActorToLayer(AActor* Actor, const FString& LayerName, UBOOL bModifyActor);

	/**
	 * Removes an actor from the specified layer.
	 *
	 * @return		TRUE if the actor was removed from the layer.  FALSE is returned if the
	 *              actor already belonged to the layer.
	 */
	static UBOOL RemoveActorFromLayer(AActor* Actor, const FString& LayerToRemove, UBOOL bModifyActor);

	/////////////////////////////////////////////////
	// Operations on selected actors.

	/**
	 * Adds selected actors to the named layer.
	 *
	 * @param	LayerName	A layer name.
	 * @return				TRUE if at least one actor was added.  FALSE is returned if all selected
	 *                      actors already belong to the named layer.
	 */
	static UBOOL AddSelectedActorsToLayer(const FString& LayerName);

	/**
	 * Adds selected actors to the named layers.
	 *
	 * @param	LayerNames	A valid list of layer names.
	 * @return				TRUE if at least one actor was added.  FALSE is returned if all selected
	 *                      actors already belong to the named layers.
	 */
	static UBOOL AddSelectedActorsToLayers(const TArray<FString>& LayerNames);

	/**
	 * Removes selected actors from the named layers.
	 *
	 * @param	LayerNames	A valid list of layer names.
	 * @return				TRUE if at least one actor was removed.
	 */
	static UBOOL RemoveSelectedActorsFromLayers(const TArray<FString>& LayerNames);

	/**
	 * Selects/de-selects actors belonging to the named layers.
	 *
	 * @param	LayerNames	A valid list of layer names.
	 * @param	bSelect		If TRUE actors are selected; if FALSE, actors are deselected.
	 * @param	Editor		The editor in which to select/de-select the actors.
	 * @return				TRUE if at least one actor was selected/deselected.
	 */
	static UBOOL SelectActorsInLayers(const TArray<FString>& LayerNames, UBOOL bSelect, UEditorEngine* Editor);




	/**
	 * Updates the per-view visibility for all actors for the given view
	 *
	 * @param ViewIndex Index of the view (into GEditor->ViewportClients)
	 * @param LayerThatChanged [optional] If one layer was changed (toggled in view pop-up, etc), then we only need to modify actors that use that layer
	 */
	static void UpdatePerViewVisibility(INT ViewIndex, FName LayerThatChanged=NAME_Skip);

	/**
	 * Updates per-view visibility for the given actor in the given view
	 *
	 * @param ViewIndex Index of the view (into GEditor->ViewportClients)
	 * @param Actor Actor to update
	 * @param bReattachIfDirty If TRUE, the actor will reattach itself to give the rendering thread updated information
	 */
	static void UpdateActorViewVisibility(INT ViewIndex, AActor* Actor, UBOOL bReattachIfDirty=TRUE);

	/**
	* Updates per-view visibility for the given actor for all views
	*
	* @param Actor Actor to update
	*/
	static void UpdateActorAllViewsVisibility(AActor* Actor);

	/**
	 * Removes the corresponding visibility bit from all actors (slides the later bits down 1)
	 *
	 * @param ViewIndex Index of the view (into GEditor->ViewportClients)
	 */
	static void RemoveViewFromActorViewVisibility(INT ViewIndex);

	/**
	* Gets all known layers from the world
	*
	* @param AllLayers Output array to store all known layers
	*/
	static void GetAllLayers(TArray<FName>& AllLayers);

	/**
	* Gets all known layers from the world
	*
	* @param AllLayers Output array to store all known layers
	*/
	static void GetAllLayers(TArray<FString>& AllLayers);

};

#endif // __LAYERUTILS_H__
