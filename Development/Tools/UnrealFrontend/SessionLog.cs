/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;

using Color = System.Drawing.Color;

namespace UnrealFrontend
{
	public class SessionLog
	{
		delegate void AddLineDelegate(Color? TxtColor, string Msg);
		delegate void WriteCommandletEventDelegate(Color? TxtColor, string Msg);
		delegate void CommandletFinishedDelegate(CommandletProcess FinishedCommandlet);

		internal SessionLog()
		{
			UIDispatcher = System.Windows.Application.Current.Dispatcher;
			OutputDocument = new UnrealControls.OutputWindowDocument();
			HandleProcessOutput = CommandletProcess_Output;
			ConsoleOutputHandler = OnConsoleOutput;
		}

		internal void AddLine( Color? TextColor, string Line )
		{
			UIDispatcher.BeginInvoke( new AddLineDelegate( AddLineInternal ), TextColor, Line );
		}

		/// <summary>
		/// Adds a line of output text.
		/// </summary>
		/// <param name="TextColor">The color of the text.</param>
		/// <param name="Line">The text to be appended.</param>
		/// <param name="Parms">Parameters controlling the final output string.</param>
		void AddLineInternal(Color? TextColor, string Line)
		{
			if (Line == null)
			{
				return;
			}

			OutputDocument.AppendLine(TextColor, Line);

			//lock (OutputBuffer)
			//{
			//    OutputBuffer.AppendLine(Line);
			//}
		}

		internal EventHandler<CommandletOutputEventArgs> HandleProcessOutput { get; private set; }
		internal ConsoleInterface.OutputHandlerDelegate ConsoleOutputHandler { get; private set; }

		/// <summary>
		/// Handles commandlet output.
		/// </summary>
		/// <param name="sender">The commandlet that generated the output.</param>
		/// <param name="e">Information about the output.</param>
		internal void CommandletProcess_Output(object sender, CommandletOutputEventArgs e)
		{
			System.Diagnostics.Debug.WriteLine(e.Message);

            // Remove tool specific prefixes and check for error/warning messages to colorize
            // (for example, IPP does "IPP WARNING:" instead of "Warning:")
            string EffectiveMessage = e.Message.ToLower();
            if (EffectiveMessage.StartsWith("ipp "))
            {
                EffectiveMessage = EffectiveMessage.Substring("ipp ".Length);
            }

			CommandletProcess Cmdlet = (CommandletProcess)sender;
			string Message;
			if (Cmdlet.ConcurrencyIndex > 0)
			{
				Message = Cmdlet.ConcurrencyIndex.ToString() + "> " + e.Message;
			}
			else
			{
				Message = e.Message;
			}

            if (EffectiveMessage.StartsWith("warning") || EffectiveMessage.Contains(": warning,"))
			{
				AddLine(Color.Orange, Message);
			}
            else if (EffectiveMessage.StartsWith("error") || EffectiveMessage.Contains(": error,"))
			{
				AddLine(Color.Red, Message);
			}
			else
			{
				AddLine(Color.Black, Message);
			}
		}

		/// <summary>
		/// Event handler for when a commandlet has exited.
		/// </summary>
		/// <param name="sender">The commandlet that exited.</param>
		/// <param name="e">Information about the event.</param>
		public void CommandletProcess_Exited(object sender, EventArgs e)
		{
			CommandletProcess Cmdlet = (CommandletProcess)sender;

			if (Cmdlet.ExitCode == 0)
			{
				AddLine(Color.Green, string.Format("[{2}] COMMANDLET \'{0} {1}\' SUCCEEDED", System.IO.Path.GetFileName(Cmdlet.ExecutablePath), Cmdlet.CommandLine, DateTime.Now.ToString("MMM d, h:mm tt")));
			}
			else
			{
				AddLine(Color.Red, string.Format("[{2}] COMMANDLET \'{0} {1}\' FAILED", System.IO.Path.GetFileName(Cmdlet.ExecutablePath), Cmdlet.CommandLine, DateTime.Now.ToString("MMM d, h:mm tt")));
			}

			// marshal the handler into the UI thread so we can change UI state
			// UIDispatcher.BeginInvoke(new CommandletFinishedDelegate(this.OnCommandletFinished), Cmdlet);
		}

		/// <summary>
		/// Writes a commandlet event string to the output window.
		/// </summary>
		/// <param name="Clr">The color of the line.</param>
		/// <param name="Message">The message to output.</param>
		internal void WriteCommandletEvent(Color? Clr, string Message)
		{
			UIDispatcher.BeginInvoke(new AddLineDelegate(AddLineInternal), Clr, Environment.NewLine + "[" + Message + "] " + DateTime.Now.ToString("MMM d, h:mm tt") + Environment.NewLine);
		}

		/// <summary>
		/// Output handler for commandlets.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		void OnConsoleOutput(object sender, ConsoleInterface.OutputEventArgs e)
		{
			if (e.Message.EndsWith(Environment.NewLine))
			{
				AddLine( e.TextColor, e.Message.Substring(0, e.Message.Length - Environment.NewLine.Length) );
			}
		}

		internal UnrealControls.OutputWindowDocument OutputDocument { get; private set; }
		internal System.Windows.Threading.Dispatcher UIDispatcher { get; private set; }
	}
}
