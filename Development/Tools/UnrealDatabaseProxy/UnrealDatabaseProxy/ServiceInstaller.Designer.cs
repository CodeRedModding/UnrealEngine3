namespace UnrealDatabaseProxy
{
	partial class ServiceInstaller
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary> 
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose( bool disposing )
		{
			if( disposing && ( components != null ) )
			{
				components.Dispose();
			}
			base.Dispose( disposing );
		}

		#region Component Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.UnrealDatabaseProxyProcInstaller = new System.ServiceProcess.ServiceProcessInstaller();
			this.UnrealDatabaseProxyServiceInstaller = new System.ServiceProcess.ServiceInstaller();
			// 
			// UnrealDatabaseProxyProcInstaller
			// 
			this.UnrealDatabaseProxyProcInstaller.Account = System.ServiceProcess.ServiceAccount.LocalSystem;
			this.UnrealDatabaseProxyProcInstaller.Password = null;
			this.UnrealDatabaseProxyProcInstaller.Username = null;
			// 
			// UnrealDatabaseProxyServiceInstaller
			// 
			this.UnrealDatabaseProxyServiceInstaller.Description = "A service to forward SQL requests from a console to a database.";
			this.UnrealDatabaseProxyServiceInstaller.DisplayName = "Unreal Database Proxy";
			this.UnrealDatabaseProxyServiceInstaller.ServiceName = "UnrealDatabaseProxy";
			this.UnrealDatabaseProxyServiceInstaller.StartType = System.ServiceProcess.ServiceStartMode.Automatic;
			// 
			// ServiceInstaller
			// 
			this.Installers.AddRange( new System.Configuration.Install.Installer[] {
            this.UnrealDatabaseProxyProcInstaller,
            this.UnrealDatabaseProxyServiceInstaller} );

		}

		#endregion

		private System.ServiceProcess.ServiceProcessInstaller UnrealDatabaseProxyProcInstaller;
		private System.ServiceProcess.ServiceInstaller UnrealDatabaseProxyServiceInstaller;
	}
}