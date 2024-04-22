using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using P4API;
using UnrealControls;


namespace UnrealQABuildSync
{
   public partial class UnrealQABuildSync : Form
   {
       /** delegates */
       delegate void DelegateAddLine(Color? TextColor, string Line);
       delegate void DelegateAddClientItem(string ClientItem);
       delegate void DelegateAddLabelItem(string LabelItem);
       delegate void DelegateValidateClientsAndLabels();
       delegate void DelegateSyncFinished();
       delegate void DelegateSyncFile(P4Connection InP4);
       delegate void DelegateMakeFileListToSync();
       delegate void DelegateStartToSync();
       delegate void DelegateRefreshProgressBar();

       public class QALabel
       {
           public double UpdateTime;
           public string LabelName;

           public QALabel(double UpdateTime, string LabelName)
           {
               this.UpdateTime = UpdateTime;
               this.LabelName = LabelName;
           }
       }

       public class SyncProgressBar : ProgressBar {

           private const int WM_PAINT = 0xF;
           private static StringFormat AlignCenter = new StringFormat() {Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center};

           protected override void WndProc(ref Message message)
           {
               switch (message.Msg)
               {
                   case WM_PAINT:
                       {
                           using (Graphics graphicInstance = CreateGraphics())
                           {
                               base.WndProc(ref message);
                               OnPaint(new PaintEventArgs(graphicInstance, ClientRectangle));
                           }
                       }
                       break;
                   default:
                       base.WndProc(ref message);
                       break;
               }
           }
           protected override void OnPaint(PaintEventArgs e)
           {
               Rectangle clientRect = ClientRectangle;
               RectangleF textClientRect = new RectangleF(clientRect.Left, clientRect.Top, clientRect.Width, clientRect.Height);

               string progressString = this.Value + "/" + this.Maximum;
               Brush textBrush = new SolidBrush(Color.Black);
               e.Graphics.DrawString(progressString, Font, textBrush, textClientRect, AlignCenter);

               base.OnPaint(e);
           }

       }

       /** Count of how many threads and p4connections will be used. */
       private const int SyncThreadsCount = 10;

       /** The document used to display output. */
       private UnrealControls.OutputWindowDocument mOutputDocument = new UnrealControls.OutputWindowDocument();

       /** Delegate instances used to call user interface functions from worker thread: */
       private DelegateAddLine varDelegateAddLine;
       private DelegateSyncFinished varDelegateSyncFinished;
       private DelegateAddClientItem varDelegateAddClientItem;
       private DelegateAddLabelItem varDelegateAddLabelItem;
       private DelegateValidateClientsAndLabels varDelegateValidateClientsAndLabels;
       private DelegateMakeFileListToSync varDelegateMakeFileListToSync;
       private DelegateStartToSync varDelegateStartToSync;
       private DelegateRefreshProgressBar varDelegateRefreshProgressBar;

       /** Main p4Connection to get information */
       private P4Connection p4 = null;

       /** Selected workspace(client) */
       private string SelectedWorkspace;
       /** Selected label */
       private string SelectedQABuildLabel;
       /** if this is true, then it syncs files with force option. */
       private bool bForcesync = false;
       /** if this is true, then it syncs files after validating local files with the diff command */
       private bool bValidate = false;

       /** An array of local files to sync to */
       private string[] LocalFilesToSync;
       /** Index of LocalFilesToSync that will be synced next */
       private int CurrentSyncIndex = 0;
       /** Indexes of threads are using to sync*/
       private List<int> CurrentSyncingIndexList;

       private Mutex SyncMutex = new Mutex();
       private Mutex AmountMutex = new Mutex();
       /** Monitor thread to watch SyncThreads  */
       private Thread ManagerThread = null;
       /** Threads to actually sync files */
       private Thread[] SyncThreads;
       /** Thread to use to get clients, labels and client files */
       private Thread PreparingThread = null;
       /** The time being started to sync */
       private DateTime SyncStartTime;
       /** The total count of files will be synced*/
       private int TotalCountOfFiles;
       /** The count of files that the tool has syned*/
       private int CountOfSyncedFiles;

       public UnrealQABuildSync()
       {
           InitializeComponent();

           // initialize delegates
           varDelegateAddLine = new DelegateAddLine(this.AddLine);
           varDelegateSyncFinished = new DelegateSyncFinished(this.SyncFinished);
           varDelegateAddClientItem = new DelegateAddClientItem(this.AddClientItem);
           varDelegateAddLabelItem = new DelegateAddLabelItem(this.AddLabelItem);
           varDelegateValidateClientsAndLabels = new DelegateValidateClientsAndLabels(this.ValidateClientsAndLabels);
           varDelegateMakeFileListToSync = new DelegateMakeFileListToSync(this.MakeFileListToSync);
           varDelegateStartToSync = new DelegateStartToSync(this.StartToSync);
           varDelegateRefreshProgressBar = new DelegateRefreshProgressBar(this.RefreshProgressBar);

           OutputWindowView_LogWindow.AutoScroll = true;
           OutputWindowView_LogWindow.Document = mOutputDocument;

           progressBar_Sync.Minimum = 0;
           progressBar_Sync.Maximum = 0;

           // initialize Threads
           SyncThreads = new Thread[SyncThreadsCount];

           CurrentSyncingIndexList = new List<int>();
           p4 = new P4Connection();
           // Don't throw an exception on a Perforce error.  We handle these manually.
           p4.ExceptionLevel = P4ExceptionLevels.NoExceptionOnErrors;
       }

       ~UnrealQABuildSync()
       {
           p4.Disconnect();
           p4 = null;
       }

       /// <summary>
       /// Adds a line of output text.
       /// </summary>
       /// <param name="TextColor">The color of the text.</param>
       /// <param name="Line">The text to be appended.</param>
       /// <param name="Parms">Parameters controlling the final output string.</param>
       void AddLine(Color? TextColor, string Line)
       {
           if (Line == null)
           {
               return;
           }
           mOutputDocument.AppendLine(TextColor, Line);
       }

       /// <summary>
       /// 
       /// </summary>
       /// <param name="ClientItem"></param>
       /// <returns></returns>
       void AddClientItem(string ClientItem)
       {
           listBox_Clients.Items.Add(ClientItem);
           listBox_Clients.SelectedIndex = 0;
       }

       /// <summary>
       /// 
       /// </summary>
       /// <param name="LabelItem"></param>
       /// <returns></returns>
       void AddLabelItem(string LabelItem)
       {
           listBox_Labels.Items.Add(LabelItem);
           listBox_Labels.SelectedIndex = 0;
       }

       /// <summary>
       /// 
       /// </summary>
       /// <param name="sender"></param>
       /// <param name="e"></param>
       /// <returns></returns>
       private void Btn_Connect_Click(object sender, EventArgs e)
       {
           try
           {
               // Reset
               p4.Disconnect();
               listBox_Clients.Items.Clear();
               listBox_Labels.Items.Clear();
               listBox_Clients.SelectedIndex = -1;
               listBox_Labels.SelectedIndex = -1;
               textBox_Port.Enabled = false;
               textBox_User.Enabled = false;
               textBox_PW.Enabled = false;

               // To disable the connect button.
               Btn_Connect.Enabled = false;

               p4.Port = textBox_Port.Text;
               p4.User = textBox_User.Text;
               p4.Connect();
               p4.Login(textBox_PW.Text);

               if (textBox_Port.Text == "")
               {
                   textBox_Port.Text = p4.Port;
               }
               if (textBox_User.Text == "")
               {
                   textBox_User.Text = p4.User;
               }

               AddLine(Color.Black, "Connecting to the perforce server and getting QAbuild labels and client workspaces...\nPlease wait a moment...");

               PreparingThread = new Thread(GetClientsAndLabels);
               PreparingThread.IsBackground = true;
               PreparingThread.Start();
           }
           catch (System.Exception ex)
           {
               p4.Disconnect();

               string Title = "Error";
               string ErrorMsg = "";
               string[] Msgs = ex.Message.Split('\n');
               if (Msgs.Length > 2)
               {
                   Title = Msgs[0];
                   for (int index = 1; index < Msgs.Length; ++index)
                   {
                       ErrorMsg = ErrorMsg + Msgs[index] + "\n";
                   }
               }
               else
               {
                   ErrorMsg = Msgs[0];
               }

               MessageBox.Show(ErrorMsg, Title,
                   MessageBoxButtons.OK,
                   MessageBoxIcon.Exclamation,
                   MessageBoxDefaultButton.Button1);

               // To enable the connect button.
               Btn_Connect.Enabled = true;
               // To disable the sync button.
               button_Sync.Enabled = false;
               checkBox_Forcesync.Enabled = false;
               checkBox_Varify.Enabled = false;
               textBox_Port.Enabled = true;
               textBox_User.Enabled = true;
               textBox_PW.Enabled = true;
           }
       }

       /// <summary>
       /// GetClientsAndLabels
       /// </summary>
       /// <returns></returns>
       private void GetClientsAndLabels()
       {
           // Get workspaces(clients) of only current owner and host.
           P4RecordSet Workspaces = p4.Run("workspaces");
           foreach (P4Record workspace in Workspaces)
           {
               if (String.Compare(workspace["Host"], p4.Host, true) == 0
                   && String.Compare(workspace["Owner"], p4.User, true) == 0)
               {
                   BeginInvoke(varDelegateAddClientItem,workspace["client"]);
               }
           }

           // Get QAbuildLabels.
           P4RecordSet Labels = p4.Run("labels", "-e", "QA_APPROVED_*");
           List<QALabel> LabelList = new List<QALabel>();
           foreach (P4Record Label in Labels)
           {
               LabelList.Add(new QALabel(System.Convert.ToDouble(Label["Update"]), Label["label"]));
           }

           // Sort QAbuildLabels by updatetime.
           LabelList.Sort(delegate(QALabel q1, QALabel q2) { return q2.UpdateTime.CompareTo(q1.UpdateTime); });

           foreach (QALabel Q in LabelList)
           {
               BeginInvoke(varDelegateAddLabelItem, Q.LabelName);
           }

           BeginInvoke(varDelegateValidateClientsAndLabels);
       }

       /// <summary>
       /// ValidateClientsAndLabels
       /// </summary>
       /// <returns></returns>
       private void ValidateClientsAndLabels()
       {
           if (listBox_Clients.Items.Count <= 0)
           {
               AddLine(Color.Red, "There are no client workspaces for Host:" + p4.Host + " Owner:" + p4.User);
               AddLine(Color.Red, "The tool doesn't support to create a new client workspace yet, so please create a new client workspace using the perforce client tools(P4Win or P4V).");
               return;
           }
           if (listBox_Labels.Items.Count <= 0)
           {
               AddLine(Color.Red, "There are no QAbuild labels...");
               return;
           }

           AddLine(Color.Green, "Connected to the perforce server.");

           listBox_Clients.SelectedIndex = 0;
           listBox_Labels.SelectedIndex = 0;

           // To enable the sync button.
           button_Sync.Enabled = true;
           checkBox_Forcesync.Enabled = true;
           checkBox_Varify.Enabled = true;
       }

       /// <summary>
       /// Button_Sync_Click
       /// </summary>
       /// <param name="sender"></param>
       /// <param name="e"></param>
       /// <returns></returns>
       private void Button_Sync_Click(object sender, EventArgs e)
       {
           try
           {
               // To disable the sync button.
               TotalCountOfFiles = 0; ;
               CountOfSyncedFiles = 0;
               progressBar_Sync.Minimum = 0;
               progressBar_Sync.Maximum = 0;
               progressBar_Sync.Value = 0;
               button_Sync.Enabled = false;
               checkBox_Forcesync.Enabled = false;
               checkBox_Varify.Enabled = false;
               listBox_Clients.Enabled = false;
               listBox_Labels.Enabled = false;

               if (mOutputDocument != null)
               {
                   mOutputDocument.Clear();
               }

               SyncStartTime = DateTime.Now;

               CurrentSyncIndex = 0;
               bForcesync = checkBox_Forcesync.Checked;
               bValidate = checkBox_Varify.Checked;
               SelectedQABuildLabel = "@" + listBox_Labels.Text;

               PreparingThread.Abort();
               PreparingThread = new Thread(MakeFileListToSync);
               PreparingThread.IsBackground = true;
               PreparingThread.Start();


           }
           catch (System.Exception ex)
           {
               throw ex;
           }
       }

       /// <summary>
       /// 
       /// </summary>
       /// <returns></returns>
       private void MakeFileListToSync()
       {
           try
           {
               // Get files that have to be synced.
               P4RecordSet Files = null;
               if (bForcesync || bValidate)
               {
                   // With -f option, so it will get all files even it doesn't need to be synced.
                   Files = p4.Run("sync", "-f", "-n", SelectedQABuildLabel);
               }
               else
               {
                   // Just get files that have to be synced.
                   Files = p4.Run("sync", "-n", SelectedQABuildLabel);
               }

               LocalFilesToSync = null;
               // If the length is 0, it means there aren't files that have to be synced.
               if (Files.Records.Length > 0)
               {
                   int count = 0;
                   CurrentSyncingIndexList.Clear();
                   LocalFilesToSync = new string[Files.Records.Length];
                   foreach (P4Record File in Files)
                   {
                       // Make a argument to give the sync command, it uses a client file path and selected QAbuildlabel name.
                       LocalFilesToSync[count++] = File.Fields["clientFile"] + SelectedQABuildLabel;
                       //TotalAmountOfFiles += System.Convert.ToDouble(File.Fields["fileSize"]);
                   }
                   TotalCountOfFiles = Files.Records.Length;
                   Invoke(varDelegateStartToSync);
               }
               else
               {
                   Invoke(varDelegateSyncFinished);
               }
           }
           catch (P4API.Exceptions.PerforceInitializationError ex)
           {
               Console.WriteLine(ex.Message);
               BeginInvoke(varDelegateAddLine, Color.Red, ex.Message);
               Invoke(varDelegateSyncFinished);
           }
           catch (System.Exception ex)
           {
               throw ex;
           }
       }

       /// <summary>
       /// StartToSync
       /// </summary>
       /// <returns></returns>
       private void StartToSync()
       {
           progressBar_Sync.Maximum = TotalCountOfFiles;
           // Each thread has a p4connection because of the p4 connection doesn't support to handle several command at the same time.
           P4Connection newP4 = null;

           ManagerThread = new Thread(new ParameterizedThreadStart(ManageSyncing));
           ManagerThread.IsBackground = true;
           for (int Index = 0; Index < SyncThreadsCount; ++Index)
           {
               SyncThreads[Index] = new Thread(new ParameterizedThreadStart(SyncFile));
               SyncThreads[Index].IsBackground = true;


               newP4 = new P4Connection();
               newP4.Port = textBox_Port.Text;
               newP4.User = textBox_User.Text;
               newP4.Connect();
               newP4.Login(textBox_PW.Text);
               newP4.Client = SelectedWorkspace;
               newP4.ExceptionLevel = P4ExceptionLevels.NoExceptionOnErrors;
               SyncThreads[Index].Start(newP4);
           }
           
           p4.Disconnect();
           p4.Port = textBox_Port.Text;
           p4.User = textBox_User.Text;
           p4.Connect();
           p4.Login(textBox_PW.Text);
           p4.Client = SelectedWorkspace;
           p4.ExceptionLevel = P4ExceptionLevels.NoExceptionOnErrors;
           ManagerThread.Start(p4);

           button_CancelSync.Enabled = true;
       }

       /// <summary>
       /// Watching sync threads to handle a connection problem or finishing sync.
       /// </summary>
       /// <param name="args"></param>
       /// <returns></returns>
       private void ManageSyncing(Object args)
       {
           P4Connection InP4 = (P4Connection)args;
           try
           {
               bool WaitingToTerminate = true;
               while (WaitingToTerminate)
               {
                   // Validate the perforce connection for automatically retrying to sync.
                   if (!InP4.IsValidConnection(true, true))
                   {
                       // Kill all sync threads.
                       for (int Index = 0; Index < SyncThreadsCount; ++Index)
                       {
                           if (SyncThreads[Index].IsAlive)
                           {
                               SyncThreads[Index].Abort();
                           }
                       }

                       SyncMutex.WaitOne();
                       {
                           // Set CurrentSyncIndex to the lowest value in CurrentSyncingIndexList, to sync sync-failed files.
                           if (CurrentSyncingIndexList.Count > 0)
                           {
                               CurrentSyncingIndexList.Sort();
                               CurrentSyncIndex = CurrentSyncingIndexList[0];
                           }
                       }
                       SyncMutex.ReleaseMutex();

                       Console.WriteLine("Trying to reconnect...");
                       BeginInvoke(varDelegateAddLine, Color.Red, "Trying to reconnect...");

                       while (true)
                       {
                           // Keep validating the perforce connection.
                           if (InP4.IsValidConnection(true, true))
                           {
                               Console.WriteLine("Connected to server!");
                               BeginInvoke(varDelegateAddLine, Color.Blue, "Connected to server!");
                               for (int Index = 0; Index < SyncThreadsCount; ++Index)
                               {
                                   SyncThreads[Index] = new Thread(new ParameterizedThreadStart(SyncFile));
                                   SyncThreads[Index].IsBackground = true;

                                   P4Connection newP4 = new P4Connection();
                                   newP4.Port = textBox_Port.Text;
                                   newP4.User = textBox_User.Text;
                                   newP4.Connect();
                                   newP4.Login(textBox_PW.Text);
                                   newP4.Client = SelectedWorkspace;
                                   newP4.ExceptionLevel = P4ExceptionLevels.NoExceptionOnErrors;
                                   SyncThreads[Index].Start(newP4);
                               }
                               break;
                           }
                           else
                           {
                               Thread.Sleep(5000);
                           }
                       }

                       continue;
                   }

                   // Watching 
                   WaitingToTerminate = false;
                   for (int Index = 0; Index < SyncThreadsCount; ++Index)
                   {
                       if (SyncThreads[Index] != null && SyncThreads[Index].IsAlive)
                       {
                           WaitingToTerminate = true;
                           break;
                       }
                       else
                       {
                           SyncThreads[Index] = null;
                       }
                   }

                   if (!WaitingToTerminate)
                       Thread.Sleep(5000);
               }

               Invoke(varDelegateSyncFinished);
           }
           catch (SynchronizationLockException e)
           {
               Console.WriteLine(e);
           }
           catch (ThreadInterruptedException e)
           {
               Console.WriteLine(e);
           }
           catch (ThreadAbortException e)
           {
               Console.WriteLine(e);
           }
           catch (System.Exception ex)
           {
               throw ex;
           }
           finally
           {
               InP4.Disconnect();
               InP4 = null;
           }
       }

       /// <summary>
       /// 
       /// </summary>
       /// <returns></returns>
       private void SyncFinished()
       {
           // To enable the sync button.
           button_Sync.Enabled = true;
           button_CancelSync.Enabled = false;
           checkBox_Forcesync.Enabled = true;
           checkBox_Varify.Enabled = true;
           listBox_Clients.Enabled = true;
           listBox_Labels.Enabled = true;

           // totaltime
           TimeSpan TotalTime = DateTime.Now - SyncStartTime;
           Console.WriteLine("TotalTime: " + TotalTime);
           AddLine(Color.Black, "TotalTime: " + TotalTime);
       }

       /// <summary>
       /// 
       /// </summary>
       /// <param name="args"></param>
       /// <returns></returns>
       private void SyncFile(Object args)
       {
           P4Connection InP4 = (P4Connection)args;
           string SyncArgument = "none";
           int CurrentContentIndex = -1;
           try
           {
               while (true)
               {

                   SyncMutex.WaitOne();
                   {

                       if (CurrentSyncIndex >= LocalFilesToSync.Length)
                       {
                           SyncMutex.ReleaseMutex();
                           break;
                       }

                       DateTime startTime = DateTime.Now;

                       if (CurrentContentIndex != -1)
                           CurrentSyncingIndexList.Remove(CurrentContentIndex);
                       // Backup the index to use when the perforce connection has a problem.
                       CurrentContentIndex = CurrentSyncIndex;
                       CurrentSyncingIndexList.Add(CurrentSyncIndex);

                       // Get a argument to sync the file.
                       SyncArgument = LocalFilesToSync[CurrentSyncIndex++];
                   }
                   SyncMutex.ReleaseMutex();

                   P4RecordSet SyncResult = null;
                   if (bForcesync)
                   {
                       SyncResult = InP4.Run("sync", "-f", SyncArgument);
                   }
                   else if (bValidate)
                   {
                       SyncResult = InP4.Run("diff", "-sl", "-f", SyncArgument);
                       if (SyncResult.Records.Length == 0)
                       {
                           // This case it seems like a file doesn't exist on client.
                           SyncResult = InP4.Run("sync", SyncArgument);
                       }
                       else
                       {
                           foreach (P4Record Result in SyncResult)
                           {
                               if (Result.Fields["status"] == "diff")
                               {
                                   SyncResult = InP4.Run("sync", "-f", SyncArgument);
                               }
                               else if (Result.Fields["status"] == "missing")
                               {
                                   SyncResult = InP4.Run("sync", "-f", SyncArgument);
                               }
                               else // same
                               {
                                   SyncResult = InP4.Run("sync", SyncArgument);
                               }
                           }
                       }
                   }
                   else
                   {
                       SyncResult = InP4.Run("sync", SyncArgument);
                       if (SyncResult.Records.Length == 0)
                       {
                           SyncResult = InP4.Run("sync", SyncArgument);
                       }
                   }

                   P4BaseRecordSet BaseSyncResult = SyncResult;
                   if (SyncResult.Errors.Length > 0)
                   {
                       foreach (String e in SyncResult.Errors)
                       {
                           Console.WriteLine(e);
                           BeginInvoke(varDelegateAddLine, Color.Red, e);
                       }
                       BeginInvoke(varDelegateAddLine, Color.Red, "Try to force sync...");
                       SyncResult = InP4.Run("sync", "-f", SyncArgument);
                       BaseSyncResult = SyncResult;
                   }
                   for (int index = 0; index < BaseSyncResult.Warnings.Length; ++index)
                   {
                       Console.WriteLine(BaseSyncResult.Warnings[index]);
                       BeginInvoke(varDelegateAddLine, Color.Black, BaseSyncResult.Warnings[index]);
                   }
                   foreach (P4Record Result in SyncResult)
                   {
                       Console.WriteLine(Result.Fields["depotFile"] + "#" + Result.Fields["rev"] + " - " + Result.Fields["action"] + " " + Result.Fields["clientFile"]);
                       BeginInvoke(varDelegateAddLine, Color.Black, Result.Fields["depotFile"] + "#" + Result.Fields["rev"] + " - " + Result.Fields["action"] + " " + Result.Fields["clientFile"]);
                   }

                   AmountMutex.WaitOne();
                   {
                       CountOfSyncedFiles++;
                       BeginInvoke(varDelegateRefreshProgressBar);
                   }
                   AmountMutex.ReleaseMutex();

                   // reset
                   SyncArgument = "none";
               }
           }
           catch (ThreadAbortException e)
           {
               Console.WriteLine(e);
           }
           catch (P4API.Exceptions.PerforceInitializationError ex)
           {
               Console.WriteLine(ex);
           }
           catch (System.Exception ex)
           {
               throw ex;
           }
           finally
           {
               InP4.Disconnect();
               InP4 = null;
           }
       }

       /// <summary>
       /// 
       /// </summary>
       /// <returns></returns>
       private void RefreshProgressBar()
       {
           if (CountOfSyncedFiles > 0)
           {
               progressBar_Sync.Value = CountOfSyncedFiles;
           }
       }

       /// <summary>
       /// 
       /// </summary>
       /// <param name="sender"></param>
       /// <param name="e"></param>
       /// <returns></returns>
       private void Button_CreateNC_Click(object sender, EventArgs e)
       {
           NewClientDlg Dlg = new NewClientDlg(p4);

           if (Dlg.ShowDialog() == DialogResult.OK)
           {

           }
       }

       /// <summary>
       /// 
       /// </summary>
       /// <param name="sender"></param>
       /// <param name="e"></param>
       /// <returns></returns>
       private void ListBox_Clients_SelectedIndexChanged(object sender, EventArgs e)
       {
           p4.Client = SelectedWorkspace = listBox_Clients.Text;
       }

       /// <summary>
       /// 
       /// </summary>
       /// <param name="sender"></param>
       /// <param name="e"></param>
       /// <returns></returns>
       private void Button_CancelSync_Click(object sender, EventArgs e)
       {
           // To disable the "cancel sync" button.
           button_CancelSync.Enabled = false;

           {
               ManagerThread.Abort();
               for (int Index = 0; Index < SyncThreadsCount; ++Index)
               {
                   if (SyncThreads[Index].IsAlive)
                       SyncThreads[Index].Abort();
               }
           }

           SyncFinished();
       }

       private void MainFormClosed(object sender, FormClosedEventArgs e)
       {
           if (PreparingThread != null && PreparingThread.IsAlive )
           {
               PreparingThread.Abort();
           }
           if (ManagerThread != null && ManagerThread.IsAlive )
           {
               ManagerThread.Abort();
           }
           for (int Index = 0; Index < SyncThreadsCount; ++Index)
           {
               if (SyncThreads[Index] != null && SyncThreads[Index].IsAlive)
               {
                   SyncThreads[Index].Abort();
               }
           }
       }

       private void CheckBox_CheckedChanged(object sender, EventArgs e)
       {
           if (sender == checkBox_Forcesync)
           {
               if (checkBox_Forcesync.Checked)
               {
                   checkBox_Varify.Checked = false;
               }
           }
           else if (sender == checkBox_Varify)
           {
               if (checkBox_Varify.Checked)
               {
                   checkBox_Forcesync.Checked = false;
               }
           }
       }
   }
}