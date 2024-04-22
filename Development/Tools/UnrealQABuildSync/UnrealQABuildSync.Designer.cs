namespace UnrealQABuildSync
{
    partial class UnrealQABuildSync
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            UnrealControls.OutputWindowDocument outputWindowDocument1 = new UnrealControls.OutputWindowDocument();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(UnrealQABuildSync));
            this.textBox_User = new System.Windows.Forms.TextBox();
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.textBox_PW = new System.Windows.Forms.TextBox();
            this.textBox_Port = new System.Windows.Forms.TextBox();
            this.label3 = new System.Windows.Forms.Label();
            this.listBox_Clients = new System.Windows.Forms.ListBox();
            this.listBox_Labels = new System.Windows.Forms.ListBox();
            this.button_Sync = new System.Windows.Forms.Button();
            this.checkBox_Forcesync = new System.Windows.Forms.CheckBox();
            this.Btn_Connect = new System.Windows.Forms.Button();
            this.button_CreateNC = new System.Windows.Forms.Button();
            this.button_CancelSync = new System.Windows.Forms.Button();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.groupBox2 = new System.Windows.Forms.GroupBox();
            this.groupBox3 = new System.Windows.Forms.GroupBox();
            this.checkBox_Varify = new System.Windows.Forms.CheckBox();
            this.panel1 = new System.Windows.Forms.Panel();
            this.panel2 = new System.Windows.Forms.Panel();
            this.OutputWindowView_LogWindow = new UnrealControls.OutputWindowView();
            this.progressBar_Sync = new UnrealQABuildSync.SyncProgressBar();
            this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            this.groupBox1.SuspendLayout();
            this.groupBox2.SuspendLayout();
            this.groupBox3.SuspendLayout();
            this.panel1.SuspendLayout();
            this.panel2.SuspendLayout();
            this.tableLayoutPanel1.SuspendLayout();
            this.SuspendLayout();
            // 
            // textBox_User
            // 
            this.textBox_User.Location = new System.Drawing.Point(90, 45);
            this.textBox_User.Name = "textBox_User";
            this.textBox_User.Size = new System.Drawing.Size(283, 21);
            this.textBox_User.TabIndex = 1;
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(8, 48);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(35, 12);
            this.label1.TabIndex = 3;
            this.label1.Text = "User:";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(8, 74);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(66, 12);
            this.label2.TabIndex = 4;
            this.label2.Text = "Password:";
            // 
            // textBox_PW
            // 
            this.textBox_PW.Location = new System.Drawing.Point(90, 72);
            this.textBox_PW.Name = "textBox_PW";
            this.textBox_PW.PasswordChar = '*';
            this.textBox_PW.Size = new System.Drawing.Size(283, 21);
            this.textBox_PW.TabIndex = 2;
            this.textBox_PW.UseSystemPasswordChar = true;
            // 
            // textBox_Port
            // 
            this.textBox_Port.Location = new System.Drawing.Point(90, 18);
            this.textBox_Port.Name = "textBox_Port";
            this.textBox_Port.Size = new System.Drawing.Size(283, 21);
            this.textBox_Port.TabIndex = 0;
            this.textBox_Port.Text = "p4-licensee-proxy.epicgames.net:1667";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(8, 20);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(31, 12);
            this.label3.TabIndex = 7;
            this.label3.Text = "Port:";
            // 
            // listBox_Clients
            // 
            this.listBox_Clients.FormattingEnabled = true;
            this.listBox_Clients.HorizontalScrollbar = true;
            this.listBox_Clients.ItemHeight = 12;
            this.listBox_Clients.Location = new System.Drawing.Point(10, 20);
            this.listBox_Clients.Name = "listBox_Clients";
            this.listBox_Clients.Size = new System.Drawing.Size(371, 256);
            this.listBox_Clients.TabIndex = 4;
            this.listBox_Clients.SelectedIndexChanged += new System.EventHandler(this.ListBox_Clients_SelectedIndexChanged);
            // 
            // listBox_Labels
            // 
            this.listBox_Labels.FormattingEnabled = true;
            this.listBox_Labels.HorizontalScrollbar = true;
            this.listBox_Labels.ItemHeight = 12;
            this.listBox_Labels.Location = new System.Drawing.Point(10, 20);
            this.listBox_Labels.Name = "listBox_Labels";
            this.listBox_Labels.Size = new System.Drawing.Size(371, 280);
            this.listBox_Labels.TabIndex = 6;
            // 
            // button_Sync
            // 
            this.button_Sync.Enabled = false;
            this.button_Sync.Location = new System.Drawing.Point(10, 358);
            this.button_Sync.Name = "button_Sync";
            this.button_Sync.Size = new System.Drawing.Size(219, 45);
            this.button_Sync.TabIndex = 8;
            this.button_Sync.Text = "Sync!!!";
            this.button_Sync.UseVisualStyleBackColor = true;
            this.button_Sync.Click += new System.EventHandler(this.Button_Sync_Click);
            // 
            // checkBox_Forcesync
            // 
            this.checkBox_Forcesync.AutoSize = true;
            this.checkBox_Forcesync.Enabled = false;
            this.checkBox_Forcesync.Location = new System.Drawing.Point(10, 336);
            this.checkBox_Forcesync.Name = "checkBox_Forcesync";
            this.checkBox_Forcesync.Size = new System.Drawing.Size(88, 16);
            this.checkBox_Forcesync.TabIndex = 7;
            this.checkBox_Forcesync.Text = "Force sync";
            this.checkBox_Forcesync.UseVisualStyleBackColor = true;
            this.checkBox_Forcesync.CheckedChanged += new System.EventHandler(this.CheckBox_CheckedChanged);
            // 
            // Btn_Connect
            // 
            this.Btn_Connect.Location = new System.Drawing.Point(90, 99);
            this.Btn_Connect.Name = "Btn_Connect";
            this.Btn_Connect.Size = new System.Drawing.Size(283, 40);
            this.Btn_Connect.TabIndex = 3;
            this.Btn_Connect.Text = "Connect";
            this.Btn_Connect.UseVisualStyleBackColor = true;
            this.Btn_Connect.Click += new System.EventHandler(this.Btn_Connect_Click);
            // 
            // button_CreateNC
            // 
            this.button_CreateNC.Enabled = false;
            this.button_CreateNC.Location = new System.Drawing.Point(10, 282);
            this.button_CreateNC.Name = "button_CreateNC";
            this.button_CreateNC.Size = new System.Drawing.Size(371, 40);
            this.button_CreateNC.TabIndex = 13;
            this.button_CreateNC.Text = "Create new client...";
            this.button_CreateNC.UseVisualStyleBackColor = true;
            this.button_CreateNC.Click += new System.EventHandler(this.Button_CreateNC_Click);
            // 
            // button_CancelSync
            // 
            this.button_CancelSync.Enabled = false;
            this.button_CancelSync.Location = new System.Drawing.Point(229, 358);
            this.button_CancelSync.Name = "button_CancelSync";
            this.button_CancelSync.Size = new System.Drawing.Size(152, 45);
            this.button_CancelSync.TabIndex = 14;
            this.button_CancelSync.Text = "Cancel sync";
            this.button_CancelSync.UseVisualStyleBackColor = true;
            this.button_CancelSync.Click += new System.EventHandler(this.Button_CancelSync_Click);
            // 
            // groupBox1
            // 
            this.groupBox1.AutoSize = true;
            this.groupBox1.Controls.Add(this.label3);
            this.groupBox1.Controls.Add(this.label2);
            this.groupBox1.Controls.Add(this.Btn_Connect);
            this.groupBox1.Controls.Add(this.label1);
            this.groupBox1.Controls.Add(this.textBox_Port);
            this.groupBox1.Controls.Add(this.textBox_User);
            this.groupBox1.Controls.Add(this.textBox_PW);
            this.groupBox1.Location = new System.Drawing.Point(4, 15);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(388, 159);
            this.groupBox1.TabIndex = 16;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "Connection";
            // 
            // groupBox2
            // 
            this.groupBox2.AutoSize = true;
            this.groupBox2.Controls.Add(this.listBox_Labels);
            this.groupBox2.Location = new System.Drawing.Point(4, 177);
            this.groupBox2.Name = "groupBox2";
            this.groupBox2.Size = new System.Drawing.Size(388, 320);
            this.groupBox2.TabIndex = 17;
            this.groupBox2.TabStop = false;
            this.groupBox2.Text = "QABuild Labels";
            // 
            // groupBox3
            // 
            this.groupBox3.AutoSize = true;
            this.groupBox3.Controls.Add(this.checkBox_Varify);
            this.groupBox3.Controls.Add(this.button_CreateNC);
            this.groupBox3.Controls.Add(this.listBox_Clients);
            this.groupBox3.Controls.Add(this.button_CancelSync);
            this.groupBox3.Controls.Add(this.checkBox_Forcesync);
            this.groupBox3.Controls.Add(this.button_Sync);
            this.groupBox3.Location = new System.Drawing.Point(4, 499);
            this.groupBox3.Name = "groupBox3";
            this.groupBox3.Size = new System.Drawing.Size(388, 423);
            this.groupBox3.TabIndex = 18;
            this.groupBox3.TabStop = false;
            this.groupBox3.Text = "Client Workspaces";
            // 
            // checkBox_Varify
            // 
            this.checkBox_Varify.AutoSize = true;
            this.checkBox_Varify.Enabled = false;
            this.checkBox_Varify.Location = new System.Drawing.Point(104, 336);
            this.checkBox_Varify.Name = "checkBox_Varify";
            this.checkBox_Varify.Size = new System.Drawing.Size(163, 16);
            this.checkBox_Varify.TabIndex = 15;
            this.checkBox_Varify.Text = "Verify the client contents";
            this.checkBox_Varify.UseVisualStyleBackColor = true;
            this.checkBox_Varify.CheckedChanged += new System.EventHandler(this.CheckBox_CheckedChanged);
            // 
            // panel1
            // 
            this.panel1.AutoScroll = true;
            this.panel1.AutoSize = true;
            this.panel1.Controls.Add(this.groupBox1);
            this.panel1.Controls.Add(this.groupBox2);
            this.panel1.Controls.Add(this.groupBox3);
            this.panel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panel1.Location = new System.Drawing.Point(3, 3);
            this.panel1.MaximumSize = new System.Drawing.Size(416, 930);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(410, 924);
            this.panel1.TabIndex = 19;
            // 
            // panel2
            // 
            this.panel2.AutoSize = true;
            this.panel2.Controls.Add(this.OutputWindowView_LogWindow);
            this.panel2.Controls.Add(this.progressBar_Sync);
            this.panel2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panel2.Location = new System.Drawing.Point(419, 3);
            this.panel2.Name = "panel2";
            this.panel2.Size = new System.Drawing.Size(923, 924);
            this.panel2.TabIndex = 20;
            // 
            // OutputWindowView_LogWindow
            // 
            this.OutputWindowView_LogWindow.AutoSize = true;
            this.OutputWindowView_LogWindow.BackColor = System.Drawing.SystemColors.Window;
            this.OutputWindowView_LogWindow.Cursor = System.Windows.Forms.Cursors.Arrow;
            this.OutputWindowView_LogWindow.Dock = System.Windows.Forms.DockStyle.Fill;
            outputWindowDocument1.Text = "";
            this.OutputWindowView_LogWindow.Document = outputWindowDocument1;
            this.OutputWindowView_LogWindow.FindTextBackColor = System.Drawing.Color.Yellow;
            this.OutputWindowView_LogWindow.FindTextForeColor = System.Drawing.Color.Black;
            this.OutputWindowView_LogWindow.FindTextLineHighlight = System.Drawing.Color.FromArgb(((int)(((byte)(239)))), ((int)(((byte)(248)))), ((int)(((byte)(255)))));
            this.OutputWindowView_LogWindow.Font = new System.Drawing.Font("Courier New", 9F);
            this.OutputWindowView_LogWindow.ForeColor = System.Drawing.SystemColors.WindowText;
            this.OutputWindowView_LogWindow.Location = new System.Drawing.Point(0, 0);
            this.OutputWindowView_LogWindow.Name = "OutputWindowView_LogWindow";
            this.OutputWindowView_LogWindow.Size = new System.Drawing.Size(923, 900);
            this.OutputWindowView_LogWindow.TabIndex = 10;
            // 
            // progressBar_Sync
            // 
            this.progressBar_Sync.Dock = System.Windows.Forms.DockStyle.Bottom;
            this.progressBar_Sync.Location = new System.Drawing.Point(0, 900);
            this.progressBar_Sync.Name = "progressBar_Sync";
            this.progressBar_Sync.Size = new System.Drawing.Size(923, 24);
            this.progressBar_Sync.Style = System.Windows.Forms.ProgressBarStyle.Continuous;
            this.progressBar_Sync.TabIndex = 11;
            // 
            // tableLayoutPanel1
            // 
            this.tableLayoutPanel1.AutoSize = true;
            this.tableLayoutPanel1.ColumnCount = 2;
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 416F));
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Controls.Add(this.panel1);
            this.tableLayoutPanel1.Controls.Add(this.panel2);
            this.tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            this.tableLayoutPanel1.Name = "tableLayoutPanel1";
            this.tableLayoutPanel1.RowCount = 1;
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Size = new System.Drawing.Size(1345, 930);
            this.tableLayoutPanel1.TabIndex = 21;
            // 
            // UnrealQABuildSync
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 12F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1345, 930);
            this.Controls.Add(this.tableLayoutPanel1);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MinimumSize = new System.Drawing.Size(500, 500);
            this.Name = "UnrealQABuildSync";
            this.Text = "UnrealQABuildSync";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.MainFormClosed);
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            this.groupBox2.ResumeLayout(false);
            this.groupBox3.ResumeLayout(false);
            this.groupBox3.PerformLayout();
            this.panel1.ResumeLayout(false);
            this.panel1.PerformLayout();
            this.panel2.ResumeLayout(false);
            this.panel2.PerformLayout();
            this.tableLayoutPanel1.ResumeLayout(false);
            this.tableLayoutPanel1.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.TextBox textBox_User;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.TextBox textBox_PW;
        private System.Windows.Forms.TextBox textBox_Port;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.ListBox listBox_Clients;
        private System.Windows.Forms.Button Btn_Connect;
        private System.Windows.Forms.Button button_CreateNC;
        private System.Windows.Forms.Button button_Sync;
        private System.Windows.Forms.Button button_CancelSync;
        private System.Windows.Forms.CheckBox checkBox_Forcesync;
        private System.Windows.Forms.ListBox listBox_Labels;
        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.GroupBox groupBox2;
        private System.Windows.Forms.GroupBox groupBox3;
        private System.Windows.Forms.Panel panel1;
        private System.Windows.Forms.Panel panel2;
        private UnrealControls.OutputWindowView OutputWindowView_LogWindow;
        private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
        private System.Windows.Forms.CheckBox checkBox_Varify;
        private UnrealQABuildSync.SyncProgressBar progressBar_Sync;
    }
}

