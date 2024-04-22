namespace MobileShaderAnalyzer
{
    partial class Form1
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
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle7 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle8 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle9 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle10 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle11 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle12 = new System.Windows.Forms.DataGridViewCellStyle();
            this.ButtonClose = new System.Windows.Forms.Button();
            this.StatusBar = new System.Windows.Forms.StatusStrip();
            this.StatusLabel = new System.Windows.Forms.ToolStripStatusLabel();
            this.StatusLabel2 = new System.Windows.Forms.ToolStripStatusLabel();
            this.OpenFileDialog = new System.Windows.Forms.OpenFileDialog();
            this.ButtonLoadDB = new System.Windows.Forms.Button();
            this.ButtonLoadWarmData = new System.Windows.Forms.Button();
            this.LabelSourceXML = new System.Windows.Forms.Label();
            this.LabelWarmData = new System.Windows.Forms.Label();
            this.GroupBoxReport = new System.Windows.Forms.GroupBox();
            this.label1 = new System.Windows.Forms.Label();
            this.NumericBoxLimit = new System.Windows.Forms.NumericUpDown();
            this.ComboBoxReport = new System.Windows.Forms.ComboBox();
            this.ListBoxDetails = new System.Windows.Forms.ListBox();
            this.TextBoxFilter = new System.Windows.Forms.TextBox();
            this.DataGridMain = new System.Windows.Forms.DataGridView();
            this.DataGridInfo = new System.Windows.Forms.DataGridView();
            this.ButtonApplyFilter = new System.Windows.Forms.Button();
            this.label2 = new System.Windows.Forms.Label();
            this.ButtonWriteReport = new System.Windows.Forms.Button();
            this.SaveFileDialog = new System.Windows.Forms.SaveFileDialog();
            this.groupBoxFilter = new System.Windows.Forms.GroupBox();
            this.label3 = new System.Windows.Forms.Label();
            this.RadioButtonNoFilter = new System.Windows.Forms.RadioButton();
            this.RadioButtonUnWarmedOnly = new System.Windows.Forms.RadioButton();
            this.RadioButtonWarmOnly = new System.Windows.Forms.RadioButton();
            this.CheckBoxSimpleReport = new System.Windows.Forms.CheckBox();
            this.ButtonShowSimilar = new System.Windows.Forms.Button();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.ButtonSetCompareMaterial = new System.Windows.Forms.Button();
            this.ButtonCompareMaterials = new System.Windows.Forms.Button();
            this.LabelCurCompareMaterial = new System.Windows.Forms.Label();
            this.StatusBar.SuspendLayout();
            this.GroupBoxReport.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.NumericBoxLimit)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.DataGridMain)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.DataGridInfo)).BeginInit();
            this.groupBoxFilter.SuspendLayout();
            this.groupBox1.SuspendLayout();
            this.SuspendLayout();
            // 
            // ButtonClose
            // 
            this.ButtonClose.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.ButtonClose.Location = new System.Drawing.Point(1166, 753);
            this.ButtonClose.Name = "ButtonClose";
            this.ButtonClose.Size = new System.Drawing.Size(75, 23);
            this.ButtonClose.TabIndex = 0;
            this.ButtonClose.Text = "Close";
            this.ButtonClose.UseVisualStyleBackColor = true;
            this.ButtonClose.Click += new System.EventHandler(this.ButtonClose_Click);
            // 
            // StatusBar
            // 
            this.StatusBar.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.StatusLabel,
            this.StatusLabel2});
            this.StatusBar.Location = new System.Drawing.Point(0, 779);
            this.StatusBar.Name = "StatusBar";
            this.StatusBar.Size = new System.Drawing.Size(1253, 22);
            this.StatusBar.TabIndex = 1;
            this.StatusBar.Text = "statusStrip1";
            // 
            // StatusLabel
            // 
            this.StatusLabel.Name = "StatusLabel";
            this.StatusLabel.Size = new System.Drawing.Size(0, 17);
            // 
            // StatusLabel2
            // 
            this.StatusLabel2.Name = "StatusLabel2";
            this.StatusLabel2.Size = new System.Drawing.Size(118, 17);
            this.StatusLabel2.Text = "toolStripStatusLabel1";
            // 
            // OpenFileDialog
            // 
            this.OpenFileDialog.FileName = "openFileDialog1";
            // 
            // ButtonLoadDB
            // 
            this.ButtonLoadDB.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.ButtonLoadDB.Location = new System.Drawing.Point(382, 13);
            this.ButtonLoadDB.Name = "ButtonLoadDB";
            this.ButtonLoadDB.Size = new System.Drawing.Size(149, 23);
            this.ButtonLoadDB.TabIndex = 4;
            this.ButtonLoadDB.Text = "Load XML";
            this.ButtonLoadDB.UseVisualStyleBackColor = true;
            this.ButtonLoadDB.Click += new System.EventHandler(this.ButtonLoadDB_Click);
            // 
            // ButtonLoadWarmData
            // 
            this.ButtonLoadWarmData.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.ButtonLoadWarmData.Location = new System.Drawing.Point(382, 42);
            this.ButtonLoadWarmData.Name = "ButtonLoadWarmData";
            this.ButtonLoadWarmData.Size = new System.Drawing.Size(149, 23);
            this.ButtonLoadWarmData.TabIndex = 5;
            this.ButtonLoadWarmData.Text = "Load Warm Data";
            this.ButtonLoadWarmData.UseVisualStyleBackColor = true;
            this.ButtonLoadWarmData.Click += new System.EventHandler(this.ButtonLoadWarmData_Click);
            // 
            // LabelSourceXML
            // 
            this.LabelSourceXML.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.LabelSourceXML.AutoSize = true;
            this.LabelSourceXML.Location = new System.Drawing.Point(537, 15);
            this.LabelSourceXML.Name = "LabelSourceXML";
            this.LabelSourceXML.Size = new System.Drawing.Size(78, 13);
            this.LabelSourceXML.TabIndex = 6;
            this.LabelSourceXML.Text = "None Selected";
            // 
            // LabelWarmData
            // 
            this.LabelWarmData.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.LabelWarmData.AutoSize = true;
            this.LabelWarmData.Location = new System.Drawing.Point(537, 42);
            this.LabelWarmData.Name = "LabelWarmData";
            this.LabelWarmData.Size = new System.Drawing.Size(78, 13);
            this.LabelWarmData.TabIndex = 7;
            this.LabelWarmData.Text = "None Selected";
            // 
            // GroupBoxReport
            // 
            this.GroupBoxReport.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.GroupBoxReport.Controls.Add(this.label1);
            this.GroupBoxReport.Controls.Add(this.NumericBoxLimit);
            this.GroupBoxReport.Controls.Add(this.ComboBoxReport);
            this.GroupBoxReport.Location = new System.Drawing.Point(690, 13);
            this.GroupBoxReport.Name = "GroupBoxReport";
            this.GroupBoxReport.Size = new System.Drawing.Size(259, 239);
            this.GroupBoxReport.TabIndex = 8;
            this.GroupBoxReport.TabStop = false;
            this.GroupBoxReport.Text = "Report";
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(7, 53);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(28, 13);
            this.label1.TabIndex = 3;
            this.label1.Text = "Limit";
            // 
            // NumericBoxLimit
            // 
            this.NumericBoxLimit.Enabled = false;
            this.NumericBoxLimit.Location = new System.Drawing.Point(7, 72);
            this.NumericBoxLimit.Minimum = new decimal(new int[] {
            1,
            0,
            0,
            0});
            this.NumericBoxLimit.Name = "NumericBoxLimit";
            this.NumericBoxLimit.Size = new System.Drawing.Size(120, 20);
            this.NumericBoxLimit.TabIndex = 2;
            this.NumericBoxLimit.Value = new decimal(new int[] {
            1,
            0,
            0,
            0});
            this.NumericBoxLimit.ValueChanged += new System.EventHandler(this.NumericBoxLimit_ValueChanged);
            // 
            // ComboBoxReport
            // 
            this.ComboBoxReport.FormattingEnabled = true;
            this.ComboBoxReport.Items.AddRange(new object[] {
            "All Keys",
            "All Materials",
            "Keys from few (Use Limit) Materials",
            "Materials creating unique (Use Limit) Keys"});
            this.ComboBoxReport.Location = new System.Drawing.Point(7, 20);
            this.ComboBoxReport.Name = "ComboBoxReport";
            this.ComboBoxReport.Size = new System.Drawing.Size(246, 21);
            this.ComboBoxReport.TabIndex = 0;
            this.ComboBoxReport.SelectedIndexChanged += new System.EventHandler(this.ComboBoxReport_SelectedIndexChanged);
            // 
            // ListBoxDetails
            // 
            this.ListBoxDetails.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.ListBoxDetails.FormattingEnabled = true;
            this.ListBoxDetails.Location = new System.Drawing.Point(382, 258);
            this.ListBoxDetails.Name = "ListBoxDetails";
            this.ListBoxDetails.Size = new System.Drawing.Size(859, 251);
            this.ListBoxDetails.TabIndex = 10;
            this.ListBoxDetails.SelectedIndexChanged += new System.EventHandler(this.listBox2_SelectedIndexChanged);
            // 
            // TextBoxFilter
            // 
            this.TextBoxFilter.BackColor = System.Drawing.Color.LightSalmon;
            this.TextBoxFilter.Location = new System.Drawing.Point(6, 46);
            this.TextBoxFilter.Name = "TextBoxFilter";
            this.TextBoxFilter.Size = new System.Drawing.Size(290, 20);
            this.TextBoxFilter.TabIndex = 11;
            this.TextBoxFilter.KeyDown += new System.Windows.Forms.KeyEventHandler(this.TextBoxFilter_KeyDown);
            this.TextBoxFilter.Leave += new System.EventHandler(this.TextBoxFilter_Leave);
            // 
            // DataGridMain
            // 
            this.DataGridMain.AllowUserToAddRows = false;
            this.DataGridMain.AllowUserToDeleteRows = false;
            this.DataGridMain.AllowUserToResizeRows = false;
            this.DataGridMain.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.DataGridMain.AutoSizeColumnsMode = System.Windows.Forms.DataGridViewAutoSizeColumnsMode.DisplayedCells;
            dataGridViewCellStyle7.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle7.BackColor = System.Drawing.SystemColors.Control;
            dataGridViewCellStyle7.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle7.ForeColor = System.Drawing.SystemColors.WindowText;
            dataGridViewCellStyle7.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle7.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle7.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.DataGridMain.ColumnHeadersDefaultCellStyle = dataGridViewCellStyle7;
            this.DataGridMain.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            dataGridViewCellStyle8.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle8.BackColor = System.Drawing.SystemColors.Window;
            dataGridViewCellStyle8.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle8.ForeColor = System.Drawing.SystemColors.ControlText;
            dataGridViewCellStyle8.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle8.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle8.WrapMode = System.Windows.Forms.DataGridViewTriState.False;
            this.DataGridMain.DefaultCellStyle = dataGridViewCellStyle8;
            this.DataGridMain.Location = new System.Drawing.Point(13, 39);
            this.DataGridMain.MinimumSize = new System.Drawing.Size(362, 689);
            this.DataGridMain.MultiSelect = false;
            this.DataGridMain.Name = "DataGridMain";
            this.DataGridMain.ReadOnly = true;
            dataGridViewCellStyle9.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle9.BackColor = System.Drawing.SystemColors.Control;
            dataGridViewCellStyle9.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle9.ForeColor = System.Drawing.SystemColors.WindowText;
            dataGridViewCellStyle9.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle9.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle9.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.DataGridMain.RowHeadersDefaultCellStyle = dataGridViewCellStyle9;
            this.DataGridMain.RowHeadersVisible = false;
            this.DataGridMain.RowHeadersWidthSizeMode = System.Windows.Forms.DataGridViewRowHeadersWidthSizeMode.AutoSizeToFirstHeader;
            this.DataGridMain.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect;
            this.DataGridMain.Size = new System.Drawing.Size(362, 689);
            this.DataGridMain.TabIndex = 14;
            this.DataGridMain.SelectionChanged += new System.EventHandler(this.DataGridMain_SelectionChanged);
            // 
            // DataGridInfo
            // 
            this.DataGridInfo.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            dataGridViewCellStyle10.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle10.BackColor = System.Drawing.SystemColors.Control;
            dataGridViewCellStyle10.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle10.ForeColor = System.Drawing.SystemColors.WindowText;
            dataGridViewCellStyle10.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle10.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle10.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.DataGridInfo.ColumnHeadersDefaultCellStyle = dataGridViewCellStyle10;
            this.DataGridInfo.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            dataGridViewCellStyle11.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle11.BackColor = System.Drawing.SystemColors.Window;
            dataGridViewCellStyle11.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle11.ForeColor = System.Drawing.SystemColors.ControlText;
            dataGridViewCellStyle11.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle11.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle11.WrapMode = System.Windows.Forms.DataGridViewTriState.False;
            this.DataGridInfo.DefaultCellStyle = dataGridViewCellStyle11;
            this.DataGridInfo.Location = new System.Drawing.Point(382, 515);
            this.DataGridInfo.Name = "DataGridInfo";
            dataGridViewCellStyle12.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle12.BackColor = System.Drawing.SystemColors.Control;
            dataGridViewCellStyle12.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle12.ForeColor = System.Drawing.SystemColors.WindowText;
            dataGridViewCellStyle12.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle12.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle12.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.DataGridInfo.RowHeadersDefaultCellStyle = dataGridViewCellStyle12;
            this.DataGridInfo.Size = new System.Drawing.Size(859, 213);
            this.DataGridInfo.TabIndex = 15;
            // 
            // ButtonApplyFilter
            // 
            this.ButtonApplyFilter.BackColor = System.Drawing.SystemColors.Control;
            this.ButtonApplyFilter.Location = new System.Drawing.Point(6, 19);
            this.ButtonApplyFilter.Name = "ButtonApplyFilter";
            this.ButtonApplyFilter.Size = new System.Drawing.Size(143, 23);
            this.ButtonApplyFilter.TabIndex = 16;
            this.ButtonApplyFilter.Text = "Apply Text Filter(Off):";
            this.ButtonApplyFilter.UseVisualStyleBackColor = false;
            this.ButtonApplyFilter.Click += new System.EventHandler(this.ButtonApplyFilter_Click);
            // 
            // label2
            // 
            this.label2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.label2.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label2.Location = new System.Drawing.Point(310, 737);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(850, 39);
            this.label2.TabIndex = 17;
            this.label2.Text = "Note: Cook with -DebugFullMaterialReport to generate source XML for this tool, RU" +
    "N with -DebugShowWarmedKeys to create warm data (outputs to debug window, paste " +
    "into text file for import)";
            // 
            // ButtonWriteReport
            // 
            this.ButtonWriteReport.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.ButtonWriteReport.Location = new System.Drawing.Point(6, 109);
            this.ButtonWriteReport.Name = "ButtonWriteReport";
            this.ButtonWriteReport.Size = new System.Drawing.Size(114, 23);
            this.ButtonWriteReport.TabIndex = 18;
            this.ButtonWriteReport.Text = "<-- Create Report";
            this.ButtonWriteReport.UseVisualStyleBackColor = true;
            this.ButtonWriteReport.Click += new System.EventHandler(this.ButtonWriteReport_Click);
            // 
            // groupBoxFilter
            // 
            this.groupBoxFilter.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.groupBoxFilter.Controls.Add(this.label3);
            this.groupBoxFilter.Controls.Add(this.RadioButtonNoFilter);
            this.groupBoxFilter.Controls.Add(this.RadioButtonUnWarmedOnly);
            this.groupBoxFilter.Controls.Add(this.RadioButtonWarmOnly);
            this.groupBoxFilter.Controls.Add(this.ButtonApplyFilter);
            this.groupBoxFilter.Controls.Add(this.TextBoxFilter);
            this.groupBoxFilter.Location = new System.Drawing.Point(382, 102);
            this.groupBoxFilter.Name = "groupBoxFilter";
            this.groupBoxFilter.Size = new System.Drawing.Size(302, 150);
            this.groupBoxFilter.TabIndex = 19;
            this.groupBoxFilter.TabStop = false;
            this.groupBoxFilter.Text = "Filters";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(15, 75);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(63, 13);
            this.label3.TabIndex = 20;
            this.label3.Text = "Warm Filter:";
            // 
            // RadioButtonNoFilter
            // 
            this.RadioButtonNoFilter.AutoSize = true;
            this.RadioButtonNoFilter.Checked = true;
            this.RadioButtonNoFilter.Enabled = false;
            this.RadioButtonNoFilter.Location = new System.Drawing.Point(99, 73);
            this.RadioButtonNoFilter.Name = "RadioButtonNoFilter";
            this.RadioButtonNoFilter.Size = new System.Drawing.Size(95, 17);
            this.RadioButtonNoFilter.TabIndex = 19;
            this.RadioButtonNoFilter.TabStop = true;
            this.RadioButtonNoFilter.Text = "No Warm Filter";
            this.RadioButtonNoFilter.UseVisualStyleBackColor = true;
            this.RadioButtonNoFilter.CheckedChanged += new System.EventHandler(this.RadioButtonFilter_CheckedChanged);
            // 
            // RadioButtonUnWarmedOnly
            // 
            this.RadioButtonUnWarmedOnly.AutoSize = true;
            this.RadioButtonUnWarmedOnly.Enabled = false;
            this.RadioButtonUnWarmedOnly.Location = new System.Drawing.Point(99, 120);
            this.RadioButtonUnWarmedOnly.Name = "RadioButtonUnWarmedOnly";
            this.RadioButtonUnWarmedOnly.Size = new System.Drawing.Size(100, 17);
            this.RadioButtonUnWarmedOnly.TabIndex = 18;
            this.RadioButtonUnWarmedOnly.Text = "Unwarmed Only";
            this.RadioButtonUnWarmedOnly.UseVisualStyleBackColor = true;
            this.RadioButtonUnWarmedOnly.CheckedChanged += new System.EventHandler(this.RadioButtonFilter_CheckedChanged);
            // 
            // RadioButtonWarmOnly
            // 
            this.RadioButtonWarmOnly.AutoSize = true;
            this.RadioButtonWarmOnly.Enabled = false;
            this.RadioButtonWarmOnly.Location = new System.Drawing.Point(99, 96);
            this.RadioButtonWarmOnly.Name = "RadioButtonWarmOnly";
            this.RadioButtonWarmOnly.Size = new System.Drawing.Size(89, 17);
            this.RadioButtonWarmOnly.TabIndex = 17;
            this.RadioButtonWarmOnly.Text = "Warmed Only";
            this.RadioButtonWarmOnly.UseVisualStyleBackColor = true;
            this.RadioButtonWarmOnly.CheckedChanged += new System.EventHandler(this.RadioButtonFilter_CheckedChanged);
            // 
            // CheckBoxSimpleReport
            // 
            this.CheckBoxSimpleReport.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.CheckBoxSimpleReport.AutoSize = true;
            this.CheckBoxSimpleReport.Location = new System.Drawing.Point(126, 113);
            this.CheckBoxSimpleReport.Name = "CheckBoxSimpleReport";
            this.CheckBoxSimpleReport.Size = new System.Drawing.Size(92, 17);
            this.CheckBoxSimpleReport.TabIndex = 20;
            this.CheckBoxSimpleReport.Text = "Simple Report";
            this.CheckBoxSimpleReport.UseVisualStyleBackColor = true;
            // 
            // ButtonShowSimilar
            // 
            this.ButtonShowSimilar.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.ButtonShowSimilar.Location = new System.Drawing.Point(5, 155);
            this.ButtonShowSimilar.Name = "ButtonShowSimilar";
            this.ButtonShowSimilar.Size = new System.Drawing.Size(114, 23);
            this.ButtonShowSimilar.TabIndex = 21;
            this.ButtonShowSimilar.Text = "Find Similar Keys";
            this.ButtonShowSimilar.UseVisualStyleBackColor = true;
            this.ButtonShowSimilar.Click += new System.EventHandler(this.ButtonShowSimilar_Click);
            // 
            // groupBox1
            // 
            this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.groupBox1.Controls.Add(this.LabelCurCompareMaterial);
            this.groupBox1.Controls.Add(this.ButtonCompareMaterials);
            this.groupBox1.Controls.Add(this.ButtonSetCompareMaterial);
            this.groupBox1.Controls.Add(this.ButtonShowSimilar);
            this.groupBox1.Controls.Add(this.CheckBoxSimpleReport);
            this.groupBox1.Controls.Add(this.ButtonWriteReport);
            this.groupBox1.Location = new System.Drawing.Point(955, 12);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(286, 240);
            this.groupBox1.TabIndex = 22;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "Tools";
            // 
            // ButtonSetCompareMaterial
            // 
            this.ButtonSetCompareMaterial.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.ButtonSetCompareMaterial.Location = new System.Drawing.Point(7, 13);
            this.ButtonSetCompareMaterial.Name = "ButtonSetCompareMaterial";
            this.ButtonSetCompareMaterial.Size = new System.Drawing.Size(135, 23);
            this.ButtonSetCompareMaterial.TabIndex = 22;
            this.ButtonSetCompareMaterial.Text = "Set Cur as Compare Mtl";
            this.ButtonSetCompareMaterial.UseVisualStyleBackColor = true;
            this.ButtonSetCompareMaterial.Click += new System.EventHandler(this.ButtonSetCompareMaterial_Click);
            // 
            // ButtonCompareMaterials
            // 
            this.ButtonCompareMaterials.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.ButtonCompareMaterials.Location = new System.Drawing.Point(6, 55);
            this.ButtonCompareMaterials.Name = "ButtonCompareMaterials";
            this.ButtonCompareMaterials.Size = new System.Drawing.Size(135, 23);
            this.ButtonCompareMaterials.TabIndex = 23;
            this.ButtonCompareMaterials.Text = "Compare Materials";
            this.ButtonCompareMaterials.UseVisualStyleBackColor = true;
            this.ButtonCompareMaterials.Click += new System.EventHandler(this.ButtonCompareMaterials_Click);
            // 
            // LabelCurCompareMaterial
            // 
            this.LabelCurCompareMaterial.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.LabelCurCompareMaterial.AutoSize = true;
            this.LabelCurCompareMaterial.Location = new System.Drawing.Point(7, 39);
            this.LabelCurCompareMaterial.Name = "LabelCurCompareMaterial";
            this.LabelCurCompareMaterial.Size = new System.Drawing.Size(103, 13);
            this.LabelCurCompareMaterial.TabIndex = 24;
            this.LabelCurCompareMaterial.Text = "No material selected";
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1253, 801);
            this.Controls.Add(this.groupBox1);
            this.Controls.Add(this.groupBoxFilter);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.DataGridInfo);
            this.Controls.Add(this.DataGridMain);
            this.Controls.Add(this.ListBoxDetails);
            this.Controls.Add(this.GroupBoxReport);
            this.Controls.Add(this.LabelWarmData);
            this.Controls.Add(this.LabelSourceXML);
            this.Controls.Add(this.ButtonLoadWarmData);
            this.Controls.Add(this.ButtonLoadDB);
            this.Controls.Add(this.StatusBar);
            this.Controls.Add(this.ButtonClose);
            this.MinimumSize = new System.Drawing.Size(1269, 839);
            this.Name = "Form1";
            this.Text = "Key/Material Analyzer";
            this.Load += new System.EventHandler(this.Form1_Load);
            this.StatusBar.ResumeLayout(false);
            this.StatusBar.PerformLayout();
            this.GroupBoxReport.ResumeLayout(false);
            this.GroupBoxReport.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.NumericBoxLimit)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.DataGridMain)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.DataGridInfo)).EndInit();
            this.groupBoxFilter.ResumeLayout(false);
            this.groupBoxFilter.PerformLayout();
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button ButtonClose;
        private System.Windows.Forms.StatusStrip StatusBar;
        private System.Windows.Forms.OpenFileDialog OpenFileDialog;
        private System.Windows.Forms.Button ButtonLoadDB;
        private System.Windows.Forms.Button ButtonLoadWarmData;
        private System.Windows.Forms.Label LabelSourceXML;
        private System.Windows.Forms.Label LabelWarmData;
        private System.Windows.Forms.GroupBox GroupBoxReport;
        private System.Windows.Forms.NumericUpDown NumericBoxLimit;
        private System.Windows.Forms.ComboBox ComboBoxReport;
        private System.Windows.Forms.ToolStripStatusLabel StatusLabel;
        private System.Windows.Forms.ListBox ListBoxDetails;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.TextBox TextBoxFilter;
        private System.Windows.Forms.DataGridView DataGridMain;
        private System.Windows.Forms.DataGridView DataGridInfo;
        private System.Windows.Forms.Button ButtonApplyFilter;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Button ButtonWriteReport;
        private System.Windows.Forms.SaveFileDialog SaveFileDialog;
        private System.Windows.Forms.ToolStripStatusLabel StatusLabel2;
        private System.Windows.Forms.GroupBox groupBoxFilter;
        private System.Windows.Forms.RadioButton RadioButtonWarmOnly;
        private System.Windows.Forms.RadioButton RadioButtonNoFilter;
        private System.Windows.Forms.RadioButton RadioButtonUnWarmedOnly;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.CheckBox CheckBoxSimpleReport;
        private System.Windows.Forms.Button ButtonShowSimilar;
        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.Label LabelCurCompareMaterial;
        private System.Windows.Forms.Button ButtonCompareMaterials;
        private System.Windows.Forms.Button ButtonSetCompareMaterial;
    }
}

