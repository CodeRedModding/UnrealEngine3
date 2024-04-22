/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;

namespace UnrealBuildTool
{
	/** A build action. */
	class Action
	{
		public List<FileItem> PrerequisiteItems = new List<FileItem>();
		public List<FileItem> ProducedItems = new List<FileItem>();
		public delegate void EventHandler(Action A);

		/** Total number of actions depending on this one. */
		public int NumTotalDependentActions = 0;
		/** Relative cost of producing items for this action. */
		public long RelativeCost = 0;
		public string WorkingDirectory = null;
		public string CommandPath = null;
		public string CommandArguments = null;
		public string StatusDescription = "...";
        public string StatusDetailedDescription = "";
		public bool bCanExecuteRemotely = false;
		public bool bIsVCCompiler = false;
		public bool bIsGCCCompiler = false;
		/** Whether this action is invoking the linker. */
		public bool bIsLinker = false;
		/** Whether the action is using a precompiled header to speed it up. */
		public bool bIsUsingPCH = false;
		/** Whether to delete the prerequisite files on completion */
		public bool bShouldDeletePrereqs = false;
		/** Whether the files in ProducedItems should be deleted before executing this action, when the action is outdated */
		public bool bShouldDeleteProducedItems = false;
		/** Whether we should track the command used to execute this action as a prerequisite */
		public bool bShouldTrackCommand = true;
		/**
		 * Whether we should log this action if executed by the local executor. This is useful for actions that take time
		 * but invoke tools without any console output.
		 */
		public bool bShouldLogIfExecutedLocally = true;
		/**
		 * Whether we should log this action if executed by XGE. Useful for actions that take time but don't have console output.
		 */
		public bool bShouldLogIfExecutedByXGE = false;
		/** True if we should redirect standard input such that input will only come from the builder (which is none) */
		public bool bShouldBlockStandardInput = false;
		/** True if we should redirect standard output such that text will not be logged */
		public bool bShouldBlockStandardOutput = false;
		/** Start time of action, optionally set by executor. */
		public DateTimeOffset StartTime = DateTimeOffset.MinValue;
		/** End time of action, optionally set by executor. */
        public DateTimeOffset EndTime = DateTimeOffset.MinValue;
		/** Optional custom event handler for standard output. */
		public DataReceivedEventHandler OutputEventHandler = null;
		/** Optional link error handler, called if the bIsLinker is true the output file wasn't generated properly. */
		public EventHandler LinkErrorHandler = null;

		public delegate void BlockingActionHandler(Action Action, out int ExitCode, out string Output);
		public BlockingActionHandler ActionHandler = null;

		public Action()
		{
			UnrealBuildTool.AllActions.Add(this);
		}

		/**
		 * Compares two actions based on total number of dependent items, descending.
		 * 
		 * @param	A	Action to compare
		 * @param	B	Action to compare
		 */
		public static int Compare( Action A, Action B )
		{
			// Primary sort criteria is total number of dependent files, up to max depth.
			if( B.NumTotalDependentActions != A.NumTotalDependentActions )
			{
				return Math.Sign( B.NumTotalDependentActions - A.NumTotalDependentActions );
			}
			// Secondary sort criteria is relative cost.
			if( B.RelativeCost != A.RelativeCost )
			{
				return Math.Sign( B.RelativeCost - A.RelativeCost );
			}
			// Tertiary sort criteria is number of pre-requisites.
			else
			{
				return Math.Sign( B.PrerequisiteItems.Count - A.PrerequisiteItems.Count );
			}
		}

		public override string ToString()
		{
			string ReturnString = "";
			if (CommandPath != null)
			{
				ReturnString += CommandPath + " - ";
			}
			if (CommandArguments != null)
			{
				ReturnString += CommandArguments;
			}
			return ReturnString;
		}
	};

	partial class UnrealBuildTool
	{
		public static List<Action> AllActions = new List<Action>();

		/** Number of outdated actions encountered. We know we are doing a full rebuild If outdated actions == AllActions.Count. */
		public static int NumOutdatedActions = 0;


		/** Builds a list of actions that need to be executed to produce the specified output items. */
		static List<Action> GetActionsToExecute(List<FileItem> OutputItems)
		{
			// Link producing actions to the items they produce.
			LinkActionsAndItems();

			// Detect cycles in the action graph.
			DetectActionGraphCycles();

			// Sort action list by "cost" in descending order to improve parallelism.
			SortActionList();

			// Build a set of all actions needed for this target.
			Dictionary<Action, bool> ActionsNeededForThisTarget = new Dictionary<Action, bool>();

			// For now simply treat all object files as the root target.
			foreach (FileItem OutputItem in OutputItems)
			{
				GatherPrerequisiteActions(OutputItem, ref ActionsNeededForThisTarget);
			}

			// Build a set of all actions that are outdated.
			Dictionary<Action, bool> OutdatedActionDictionary = GatherAllOutdatedActions();

			// Delete produced items that are outdated.
			DeleteOutdatedProducedItems(OutdatedActionDictionary, BuildConfiguration.bShouldDeleteAllOutdatedProducedItems);

			// Create directories for the outdated produced items.
			CreateDirectoriesForProducedItems(OutdatedActionDictionary);

			// Build a list of actions that are both needed for this target and outdated.
			List<Action> ActionsToExecute = new List<Action>();
			foreach (Action Action in AllActions)
			{
				if (ActionsNeededForThisTarget.ContainsKey(Action) && OutdatedActionDictionary[Action] && Action.CommandPath != null)
				{
					ActionsToExecute.Add(Action);
				}
			}

			return ActionsToExecute;
		}

		/** Executes a list of actions. */
		static bool ExecuteActions(List<Action> ActionsToExecute, out string ExecutorName, UnrealTargetPlatform Platform)
		{
			bool Result = true;
			bool bUsedXGE = false;
			ExecutorName = "";
			if (ActionsToExecute.Count > 0)
			{
				if (BuildConfiguration.bAllowXGE)
				{
					XGE.ExecutionResult XGEResult = XGE.ExecutionResult.TasksSucceeded;

					// Batch up XGE execution by actions with the same output event handler.
					List<Action> ActionBatch = new List<Action>();
					ActionBatch.Add(ActionsToExecute[0]);
					for (int ActionIndex = 1; ActionIndex < ActionsToExecute.Count && XGEResult == XGE.ExecutionResult.TasksSucceeded; ++ActionIndex)
					{
						Action CurrentAction = ActionsToExecute[ActionIndex];
						if (CurrentAction.OutputEventHandler == ActionBatch[0].OutputEventHandler)
						{
							ActionBatch.Add(CurrentAction);
						}
						else
						{
							XGEResult = XGE.ExecuteActions(ActionBatch);
							ActionBatch.Clear();
							ActionBatch.Add(CurrentAction);
						}
					}
					if (ActionBatch.Count > 0 && XGEResult == XGE.ExecutionResult.TasksSucceeded)
					{
						XGEResult = XGE.ExecuteActions(ActionBatch);
						ActionBatch.Clear();
					}

					if (XGEResult != XGE.ExecutionResult.Unavailable)
					{
						ExecutorName = "XGE";
						Result = (XGEResult == XGE.ExecutionResult.TasksSucceeded);
						// don't do local compilation
						bUsedXGE = true;
					}
				}

				// If XGE is disallowed or unavailable, execute the commands locally.
				if (!bUsedXGE)
				{
					ExecutorName = "Local";
					Result = LocalExecutor.ExecuteActions(ActionsToExecute);
				}

				// Verify the link outputs were created (seems to happen with Win64 compiles)
				foreach (Action BuildAction in ActionsToExecute)
				{
					if (BuildAction.bIsLinker)
					{
						foreach (FileItem Item in BuildAction.ProducedItems)
						{
							// find out if the 
							bool bExists;
							if (Platform == UnrealTargetPlatform.IPhone || Platform == UnrealTargetPlatform.Mac)
							{
								DateTime UnusedTime;
								long UnusedLength;
								bExists = RPCUtilHelper.GetRemoteFileInfo(Item.AbsolutePath, out UnusedTime, out UnusedLength);
							}
							else
							{
								bExists = new FileInfo(Item.AbsolutePath).Exists;
							}

							if (!bExists)
							{
								Console.WriteLine("UBT ERROR: Failed to produce item: " + Item.AbsolutePath);
								if ( BuildAction.LinkErrorHandler != null )
								{
									BuildAction.LinkErrorHandler(BuildAction);
								}
								Result = false;
							}
						}
					}
				}
			}
			// Nothing to execute.
			else
			{
				ExecutorName = "NoActionsToExecute";
				Console.WriteLine("Target is up to date.");
			}

			// Perform any cleanup
			foreach (Action Action in ActionsToExecute)
			{
				if (Action.bShouldDeletePrereqs)
				{
					foreach (FileItem FileItem in Action.PrerequisiteItems)
					{
						FileItem.Delete();
					}
				}
			}

			return Result;
		}

		/** Links actions with their prerequisite and produced items into an action graph. */
		static void LinkActionsAndItems()
		{
			foreach (Action Action in AllActions)
			{
				foreach (FileItem ProducedItem in Action.ProducedItems)
				{
					ProducedItem.ProducingAction = Action;
					Action.RelativeCost += ProducedItem.RelativeCost;
				}
			}
		}

		/**
		 * Sorts the action list for improved parallelism with local execution.
		 */
		public static void SortActionList()
		{
			// Mapping from action to a list of actions that directly or indirectly depend on it (up to a certain depth).
			Dictionary<Action,HashSet<Action>> ActionToDependentActionsMap = new Dictionary<Action,HashSet<Action>>();
			// Perform multiple passes over all actions to propagate dependencies.
			const int MaxDepth = 5;
			for (int Pass=0; Pass<MaxDepth; Pass++)
			{
				foreach (Action DependendAction in AllActions)
				{
					foreach (FileItem PrerequisiteItem in DependendAction.PrerequisiteItems)
					{
						Action PrerequisiteAction = PrerequisiteItem.ProducingAction;						
						if( PrerequisiteAction != null )
						{
							HashSet<Action> DependentActions = null;
							if( ActionToDependentActionsMap.ContainsKey(PrerequisiteAction) )
							{
								DependentActions = ActionToDependentActionsMap[PrerequisiteAction];
							}
							else
							{
								DependentActions = new HashSet<Action>();
								ActionToDependentActionsMap[PrerequisiteAction] = DependentActions;
							}
							// Add dependent action...
							DependentActions.Add( DependendAction );
							// ... and all actions depending on it.
							if( ActionToDependentActionsMap.ContainsKey(DependendAction) )
							{
								DependentActions.UnionWith( ActionToDependentActionsMap[DependendAction] );
							}
						}
					}
				}

			}
			// At this point we have a list of dependent actions for each action, up to MaxDepth layers deep.
			foreach (KeyValuePair<Action,HashSet<Action>> ActionMap in ActionToDependentActionsMap)
			{
				ActionMap.Key.NumTotalDependentActions = ActionMap.Value.Count;
			}
			// Sort actions by number of actions depending on them, descending. Secondary sort criteria is file size.
			AllActions.Sort( Action.Compare );		
		}

		/** Checks for cycles in the action graph. */
		static void DetectActionGraphCycles()
		{
			// Starting with actions that only depend on non-produced items, iteratively expand a set of actions that are only dependent on
			// non-cyclical actions.
			Dictionary<Action, bool> ActionIsNonCyclical = new Dictionary<Action, bool>();
			while (true)
			{
				bool bFoundNewNonCyclicalAction = false;

				foreach (Action Action in AllActions)
				{
					if (!ActionIsNonCyclical.ContainsKey(Action))
					{
						// Determine if the action depends on only actions that are already known to be non-cyclical.
						bool bActionOnlyDependsOnNonCyclicalActions = true;
						foreach (FileItem PrerequisiteItem in Action.PrerequisiteItems)
						{
							if (PrerequisiteItem.ProducingAction != null)
							{
								if (!ActionIsNonCyclical.ContainsKey(PrerequisiteItem.ProducingAction))
								{
									bActionOnlyDependsOnNonCyclicalActions = false;
								}
							}
						}

						// If the action only depends on known non-cyclical actions, then add it to the set of known non-cyclical actions.
						if (bActionOnlyDependsOnNonCyclicalActions)
						{
							ActionIsNonCyclical.Add(Action, true);
							bFoundNewNonCyclicalAction = true;
						}
					}
				}

				// If this iteration has visited all actions without finding a new non-cyclical action, then all non-cyclical actions have
				// been found.
				if (!bFoundNewNonCyclicalAction)
				{
					break;
				}
			}

			// If there are any cyclical actions, throw an exception.
			if (ActionIsNonCyclical.Count < AllActions.Count)
			{
				// Describe the cyclical actions.
				string CycleDescription = "";
				foreach (Action Action in AllActions)
				{
					if (!ActionIsNonCyclical.ContainsKey(Action))
					{
						CycleDescription += string.Format("Action: {0}\r\n", Action.CommandPath);
						CycleDescription += string.Format("\twith arguments: {0}\r\n", Action.CommandArguments);
						foreach (FileItem PrerequisiteItem in Action.PrerequisiteItems)
						{
							CycleDescription += string.Format("\tdepends on: {0}\r\n", PrerequisiteItem.AbsolutePath);
						}
						foreach (FileItem ProducedItem in Action.ProducedItems)
						{
							CycleDescription += string.Format("\tproduces:   {0}\r\n", ProducedItem.AbsolutePath);
						}
						CycleDescription += "\r\n\r\n";
					}
				}

				throw new BuildException("Action graph contains cycle!\r\n\r\n{0}", CycleDescription);
			}
		}

		/**
		 * Determines the full set of actions that must be built to produce an item.
		 * @param OutputItem - The item to be built.
		 * @param PrerequisiteActions - The actions that must be built and the root action are 
		 */
		static void GatherPrerequisiteActions(
			FileItem OutputItem,
			ref Dictionary<Action, bool> PrerequisiteActions
			)
		{
			if (OutputItem != null && OutputItem.ProducingAction != null)
			{
				if (!PrerequisiteActions.ContainsKey(OutputItem.ProducingAction))
				{
					PrerequisiteActions.Add(OutputItem.ProducingAction, true);
					foreach (FileItem PrerequisiteItem in OutputItem.ProducingAction.PrerequisiteItems)
					{
						GatherPrerequisiteActions(PrerequisiteItem, ref PrerequisiteActions);
					}
				}
			}
		}

		/**
		 * Determines whether an action is outdated based on the modification times for its prerequisite
		 * and produced items.
		 * @param RootAction - The action being considered.
		 * @param OutdatedActionDictionary - 
         * @return true if outdated
		 */
        static public bool IsActionOutdated(Action RootAction, ref Dictionary<Action, bool> OutdatedActionDictionary)
		{
			// Only compute the outdated-ness for actions that don't aren't cached in the outdated action dictionary.
			bool bIsOutdated = false;
			if (!OutdatedActionDictionary.TryGetValue(RootAction, out bIsOutdated))
			{
				// Determine the last time the action was run based on the write times of its produced files.
                DateTimeOffset LastExecutionTime = DateTimeOffset.MaxValue;
				bool bAllProducedItemsExist = true;
				FileItem LastExecutionProducedItem = null;
				foreach (FileItem ProducedItem in RootAction.ProducedItems)
				{
					// If the produced file doesn't exist or has zero size, consider it outdated.  The zero size check is to detect cases
					// where aborting an earlier compile produced invalid zero-sized obj files, but that may cause actions where that's
					// legitimate output to always be considered outdated.
					if (ProducedItem.bExists && ProducedItem.Length > 0)
					{
						// Use the oldest produced item's time as the last execution time.
                        if (ProducedItem.LastWriteTime < LastExecutionTime)
						{
                            LastExecutionTime = ProducedItem.LastWriteTime;
							LastExecutionProducedItem = ProducedItem;
						}
					}
					else
					{
						// If any of the produced items doesn't exist, the action is outdated.
						if (BuildConfiguration.bPrintDebugInfo)
						{
							Console.WriteLine(
								"{0}: Produced item \"{1}\" doesn't exist.",
								RootAction.StatusDescription,
								Path.GetFileName(ProducedItem.AbsolutePath)
								);
						}
						bAllProducedItemsExist = false;
						bIsOutdated = true;
						break;
					}
				}

				if(bAllProducedItemsExist)
				{
					// Check if any of the prerequisite items are produced by outdated actions, or have changed more recently than
					// the oldest produced item.
					foreach (FileItem PrerequisiteItem in RootAction.PrerequisiteItems)
					{
						if (PrerequisiteItem.ProducingAction != null)
						{
							if(IsActionOutdated(PrerequisiteItem.ProducingAction,ref OutdatedActionDictionary))
							{
								if (BuildConfiguration.bPrintDebugInfo)
								{
									Console.WriteLine(
										"{0}: Prerequisite {1} is produced by outdated action.",
										RootAction.StatusDescription,
										Path.GetFileName(PrerequisiteItem.AbsolutePath)
										);
								}
								bIsOutdated = true;
							}
						}

						if (PrerequisiteItem.bExists)
						{
							// allow a 1 second slop for network copies
                            TimeSpan TimeDifference = PrerequisiteItem.LastWriteTime - LastExecutionTime;
							bool bPrerequisiteItemIsNewerThanLastExecution = TimeDifference.TotalSeconds > 1;
							if (bPrerequisiteItemIsNewerThanLastExecution)
							{
								if (BuildConfiguration.bPrintDebugInfo)
								{
									Console.WriteLine(
										"{0}: Prerequisite {1} is newer than the last execution of the action: {2} vs {3} from {4}",
										RootAction.StatusDescription,
										Path.GetFileName(PrerequisiteItem.AbsolutePath),
                                        PrerequisiteItem.LastWriteTime.LocalDateTime,
                                        LastExecutionTime.LocalDateTime,
										LastExecutionProducedItem.AbsolutePath
										);
								}
								bIsOutdated = true;
							}
						}

						// GatherAllOutdatedActions will ensure all actions are checked for outdated-ness, so we don't need to recurse with
						// all this action's prerequisites once we've determined it's outdated.
						if (bIsOutdated)
						{
							break;
						}
					}
				}

				// Cache the outdated-ness of this action.
				OutdatedActionDictionary.Add(RootAction, bIsOutdated);

				// Keep track of how many outdated actions there are. Used to determine whether this was a full rebuild or not.
				if( bIsOutdated )
				{
					NumOutdatedActions++;
				}
			}

			return bIsOutdated;
		}

		/**
		 * Builds a dictionary containing the actions from AllActions that are outdated by calling
		 * IsActionOutdated.
		 */
		static Dictionary<Action,bool> GatherAllOutdatedActions()
		{
			Dictionary<Action, bool> OutdatedActionDictionary = new Dictionary<Action, bool>();

			foreach (Action Action in AllActions)
			{
				IsActionOutdated(Action, ref OutdatedActionDictionary);
			}

			return OutdatedActionDictionary;
		}

		/**
		 * Deletes all the items produced by actions in the provided outdated action dictionary. 
		 * 
		 * @param	OutdatedActionDictionary	Dictionary of outdated actions
		 * @param	bShouldDeleteAllFiles		Whether to delete all files associated with outdated items or just ones required
		 */
		static void DeleteOutdatedProducedItems(Dictionary<Action, bool> OutdatedActionDictionary, bool bShouldDeleteAllFiles)
		{
			foreach (KeyValuePair<Action,bool> OutdatedActionInfo in OutdatedActionDictionary)
			{
				if (OutdatedActionInfo.Value)
				{
					Action OutdatedAction = OutdatedActionInfo.Key;
					foreach (FileItem ProducedItem in OutdatedActionInfo.Key.ProducedItems)
					{
						if( ProducedItem.bExists
						&&	(	bShouldDeleteAllFiles
							// Delete PDB files as incremental updates are slower than full ones. The exception is UnrealEdCSharp.pdb as we might not have
							// to rebuild it. This is a bit of a hack.
							||	(ProducedItem.AbsolutePathUpperInvariant.EndsWith(".PDB") && !ProducedItem.AbsolutePathUpperInvariant.Contains("UNREALEDCSHARP.PDB")) 
							||	OutdatedAction.bShouldDeleteProducedItems) )
						{
							if (BuildConfiguration.bPrintDebugInfo)
							{
								Console.WriteLine("Deleting outdated item: {0}", ProducedItem.AbsolutePath);
							}
							ProducedItem.Delete();
						}
					}
				}
			}
		}

		/**
		 * Creates directories for all the items produced by actions in the provided outdated action
		 * dictionary.
		 */
		static void CreateDirectoriesForProducedItems(Dictionary<Action, bool> OutdatedActionDictionary)
		{
			foreach (KeyValuePair<Action, bool> OutdatedActionInfo in OutdatedActionDictionary)
			{
				if (OutdatedActionInfo.Value)
				{
					foreach (FileItem ProducedItem in OutdatedActionInfo.Key.ProducedItems)
					{
						string DirectoryPath = Path.GetDirectoryName(ProducedItem.AbsolutePath);
						if(!Directory.Exists(DirectoryPath))
						{
							if (BuildConfiguration.bPrintDebugInfo)
							{
								Console.WriteLine("Creating directory for produced item: {0}", DirectoryPath);
							}
							Directory.CreateDirectory(DirectoryPath);
						}
					}
				}
			}
		}
	};
}
