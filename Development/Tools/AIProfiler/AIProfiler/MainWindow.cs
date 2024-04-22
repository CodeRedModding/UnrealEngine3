using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.IO;

namespace AIProfiler
{
	public partial class MainWindow : Form
	{
		public MainWindow(String InFileName)
		{
			InitializeComponent();
			ApplyTokenFilter(FilterTokenByFrameTimeSpan);
			ApplyTokenFilter(FilterTokenBySearchString);
			ApplyTokenFilter(FilterTokenByEventCategory);
			ApplyAIControllerFilter(FilterControllerByClassName);
			if (!String.IsNullOrEmpty(InFileName))
			{
				ChangeFileInternal(InFileName);
			}
		}

		/** Current profiler stream */
		private ProfilerStream CurProfilerStream = null;

		#region File Handling
		/** Track when the file is being changed so events can skip their logic if so */
		private bool bIsChangingFile = false;

		/** Called when the user selects the File->Open menu options */
		private void OnFileOpen(object sender, EventArgs e)
		{
			OpenFileDialog.ShowDialog();
		}

		/** Called when the user ok's the file dialog */
		private void OnFileDialogOk(object sender, CancelEventArgs e)
		{
			ChangeFileInternal(OpenFileDialog.FileName);
		}

		/**
		 * Handle a file change
		 * 
		 * @param	FileName	Name of the new file to change to
		 */
		private void ChangeFileInternal(String FileName)
		{
			// Indicate that the file is changing so events can skip execution as needed
			bIsChangingFile = true;

			// Clear any expanded node data
			ExpandedNodes.Clear();

			// Reset the filter controls so the new file doesn't start pre-filtered
			ResetFilterControls();

			try
			{
				// Open the file and parse it into a profiler stream
				CurProfilerStream = ProfilerStream.Parse(File.OpenRead(FileName));

				// Extract all of the event categories from the stream and add them into
				// the event category checkered list box (defaulting to selected)
				List<String> EventCategories = CurProfilerStream.GetAllEventCategories();
				foreach (String CurCategory in EventCategories)
				{
					EventCategoryListBox.Items.Add(CurCategory, true);
				}

				// Extract all of the controller classes from the stream and add them into
				// the controller class checkered list box (defaulting to selected)
				List<String> ControllerClassNames = CurProfilerStream.GetAllControllerClassNames();
				foreach (String CurControllerClass in ControllerClassNames)
				{
					ControllerClassListBox.Items.Add(CurControllerClass, true);
				}

				// No longer changing file, restore events
				bIsChangingFile = false;

				// Update the filters/tree view
				UpdateFilters();

				// Update the title of the window to specify the current file name
				Text = "AI Profiler - " + FileName;
			}
			catch(Exception e)
			{
				System.Console.WriteLine("Error opening/reading file: {0}", e.ToString());
			}
		}
		#endregion

		#region Filtering
		/** Delegate for Token filter methods */
		private delegate bool TokenFilterDelegate(EmittedTokenBase InToken);
		private TokenFilterDelegate TokenFilterMethods = null;

		/** Delegate for Controller filter methods */
		private delegate bool AIControllerFilterDelegate(AIController InController);
		private AIControllerFilterDelegate ControllerFilterMethods = null;

		/** Track if the filters need to be updated or not; Used for deferring updates if necessary */
		private bool bNeedsFilterUpdate = false;

		/** Helper method to reset the filter controls to a default state */
		private void ResetFilterControls()
		{
			SearchTextBox.Text = String.Empty;
			StartTimeControl.Value = new Decimal(0.0);
			EndTimeControl.Value = new Decimal(99999999.99);
			EventCategoryListBox.Items.Clear();
			ControllerClassListBox.Items.Clear();
		}

		/**
		 * Helper method to apply a token filter
		 * 
		 * @param	InDelegate	Filter delegate to apply
		 */
		private void ApplyTokenFilter(TokenFilterDelegate InDelegate)
		{
			TokenFilterMethods += InDelegate;
		}

		/**
		 * Helper method to apply an AI controller filter
		 * 
		 * @param	InDelegate	Filter delegate to apply
		 */
		private void ApplyAIControllerFilter(AIControllerFilterDelegate InDelegate)
		{
			ControllerFilterMethods += InDelegate;
		}

		/** Update the filters/tree view immediately */
		private void UpdateFilters()
		{
			if (CurProfilerStream != null && !bIsChangingFile)
			{
				PopulateTreeView();
				bNeedsFilterUpdate = false;
			}
		}

		/** Called when the deferred filter timer ticks */
		private void OnDeferredFilterUpdateTick(object sender, EventArgs e)
		{
			if (bNeedsFilterUpdate)
			{
				UpdateFilters();
			}
		}

		/** Defer a filter update */
		private void DeferUpdateFilters()
		{
			if (!bIsChangingFile)
			{
				bNeedsFilterUpdate = true;
				DeferredFilterUpdateTimer.Stop();
				DeferredFilterUpdateTimer.Start();
			}
		}

		/**
		 * Method to filter a token by the frame time span specified by the time controls
		 * 
		 * @param	InToken	Token to test against the filter
		 * 
		 * @return	true if the token passes the filter, false if it does not
		 */
		private bool FilterTokenByFrameTimeSpan(EmittedTokenBase InToken)
		{
			return InToken.WorldTimeSeconds >= Decimal.ToSingle(StartTimeControl.Value)
				&& InToken.WorldTimeSeconds <= Decimal.ToSingle(EndTimeControl.Value);
		}

		/**
		 * Method to filter a token by the search string specified by the search box
		 * 
		 * @param	InToken	Token to test against the filter
		 *
		 * @return	true if the token passes the filter, false if it does not
		 */
		private bool FilterTokenBySearchString(EmittedTokenBase InToken)
		{
			String SearchText = SearchTextBox.Text.ToLower();
			return String.IsNullOrEmpty(SearchText) || InToken.ToString().ToLower().Contains(SearchText);
		}

		/**
		 * Method to filter a token by event category based on categories selected in the list box
		 * 
		 * @param	InToken	Token to test against the filter
		 *
		 * @return	true if the token passes the filter, false if it does not
		 */
		private bool FilterTokenByEventCategory(EmittedTokenBase InToken)
		{
			return EventCategoryListBox.CheckedItems.Contains(InToken.GetEventCategory());
		}

		/**
		 * Method to filter a controller by class name based on classes selected in the list box
		 * 
		 * @param	InController	Controller to test against the filter
		 * 
		 * @return true if the controller passes the filter, false if it does not
		 */
		private bool FilterControllerByClassName(AIController InController)
		{
			return ControllerClassListBox.CheckedItems.Contains(InController.GetClassName());
		}

		/**
		 * Main method to filter tokens with; Executes all currently applied filter delegates
		 * 
		 * @param	InToken	Token to test against the filters
		 * 
		 * @return	true if the token passed all filters, false if it did not
		 */
		private bool MainTokenFilterMethod(EmittedTokenBase InToken)
		{
			bool bPassesFilter = true;
			if (TokenFilterMethods != null)
			{
				// Iterate over each delegate assigned to filter tokens and call it on the current token
				foreach (TokenFilterDelegate CurDelegate in TokenFilterMethods.GetInvocationList())
				{
					if (!CurDelegate(InToken))
					{
						bPassesFilter = false;
						break;
					}
				}
			}
			return bPassesFilter;
		}

		/**
		 * Main method to filter controllers with; Executes all currently applied filter delegates
		 * 
		 * @param	InController	Controller to test against the filters
		 * 
		 * @return	true if the controller passed all filters, false if it did not
		 */
		private bool MainControllerFilterMethod(AIController InController)
		{
			bool bPassesFilter = true;
			if (ControllerFilterMethods != null)
			{
				// Iterate over each delegate assigned to filter controllers and call it on the current controller
				foreach (AIControllerFilterDelegate CurDelegate in ControllerFilterMethods.GetInvocationList())
				{
					if (!CurDelegate(InController))
					{
						bPassesFilter = false;
						break;
					}
				}
			}
			return bPassesFilter;
		}

		/** Called when the user edits the start time control */
		private void OnStartTimeValueChanged(object sender, EventArgs e)
		{
			UpdateFilters();
		}

		/** Called when the user edits the end time control */
		private void OnEndTimeValueChanged(object sender, EventArgs e)
		{
			UpdateFilters();
		}

		/** Called when the user changes the text in the search text box */
		private void OnSearchTextChanged(object sender, EventArgs e)
		{
			// Defer updates so that updates don't fire repeatedly as the user types
			DeferUpdateFilters();
		}

		/** Called when the user checks or unchecks an event category in the list box */
		private void OnEventCategoryCheck(object sender, ItemCheckEventArgs e)
		{
			// Update has to be deferred because at the time of the event, the value of the check
			// box has not been properly updated by Windows Forms yet
			DeferUpdateFilters();
		}

		/** Called when the user checks or unchecks a controller class in the list box */
		private void OnControllerClassCheck(object sender, ItemCheckEventArgs e)
		{
			// Update has to be deferred because at the time of the event, the value of the check
			// box has not been properly updated by Windows Forms yet
			DeferUpdateFilters();
		}
		#endregion

		#region TreeView
		private HashSet<String> ExpandedNodes = new HashSet<String>();

		/** Populate the tree view based on the current profiler stream */
		private void PopulateTreeView()
		{
			if (CurProfilerStream != null)
			{
				MainTreeView.BeginUpdate();
				MainTreeView.Nodes.Clear();

				// Find which AI controllers pass the currently set filters
				List<AIController> ControllersToDisplay = CurProfilerStream.ControllerTable.FindAll(MainControllerFilterMethod);
				
				// Sort the controllers to display based on their strings (can't do this in the controller table because
				// the indices must remain intact)
				ControllersToDisplay.Sort(
					delegate(AIController Controller1, AIController Controller2)
					{
						return Controller1.ToString().CompareTo(Controller2.ToString());
					});

				// Add each controller to the tree view
				foreach (AIController CurController in ControllersToDisplay)
				{
					AddControllerToTreeView(CurController);
				}
				MainTreeView.EndUpdate();
			}
		}

		/**
		 * Add an AI controller to the tree view
		 * 
		 * @param	InController	Controller to add to the tree view
		 */
		private void AddControllerToTreeView(AIController InController)
		{
			List<EmittedTokenBase> TokensToDisplay = InController.EmittedTokens.FindAll(MainTokenFilterMethod);
			if (TokensToDisplay.Count > 0)
			{
				TreeNode ControllerNode = MainTreeView.Nodes.Add(InController.ToString());
				foreach (EmittedTokenBase CurToken in TokensToDisplay)
				{
					ControllerNode.Nodes.Add(CurToken.ToString());
				}

				// See if the node was previously expanded before this update, and if so, expand it again
				if (ExpandedNodes.Contains(ControllerNode.Text))
				{
					ControllerNode.Expand();
				}
			}
		}

		/** Called when a tree node is collapsed */
		private void OnAfterNodeCollapse(object sender, TreeViewEventArgs e)
		{
			ExpandedNodes.Remove(e.Node.Text);
		}

		/** Called when a tree node is expanded */
		private void OnAfterNodeExpand(object sender, TreeViewEventArgs e)
		{
			ExpandedNodes.Add(e.Node.Text);
		}
		#endregion
	}
}
