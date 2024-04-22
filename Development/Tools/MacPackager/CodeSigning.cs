/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;
using System.Security.Cryptography.X509Certificates;
using System.Security.Cryptography;
using System.IO;
using MachObjectHandling;
using System.Diagnostics;
using System.Xml;

namespace MacPackager
{
    public class CodeSignatureBuilder
    {
		/// <summary>
		/// The file system used to read/write different parts of the application bundle
		/// </summary>
		public FileOperations.FileSystemAdapter FileSystem;

		/// <summary>
        /// The certificate used for code signing
        /// </summary>
        public X509Certificate2 SigningCert;

        /// <summary>
        /// Returns null if successful, or an error string if it failed
        /// </summary>
        public static string InstallCertificate(string CertificateFilename, string PrivateKeyFilename)
        {
            try
            {
                // Load the certificate
                string CertificatePassword = "";
                X509Certificate2 Cert = new X509Certificate2(CertificateFilename, CertificatePassword, X509KeyStorageFlags.MachineKeySet | X509KeyStorageFlags.PersistKeySet);
                if (!Cert.HasPrivateKey)
                {
                    return "Error: Certificate does not include a private key and cannot be used to code sign";
                }

                // Add the certificate to the store
                X509Store Store = new X509Store();
                Store.Open(OpenFlags.ReadWrite);
                Store.Add(Cert);
                Store.Close();
            }
            catch (Exception ex)
            {
                string ErrorMsg = String.Format("Failed to load or install certificate with error '{0}'", ex.Message);
                Program.Error(ErrorMsg);
                return ErrorMsg;
            }

            return null;
        }

        /// <summary>
        /// Makes sure the required files for code signing exist and can be found
        /// </summary>
        public static void FindRequiredFiles(out X509Certificate2 Cert, out bool bHasOverridesFile)
        {
            // Check for a suitable signature to match the mobile provision
            Cert = CodeSignatureBuilder.FindCertificate();
            bHasOverridesFile = File.Exists(Config.GetPlistOverrideFilename());
        }

        public static bool DoRequiredFilesExist()
        {
            X509Certificate2 Cert;
            bool bOverridesExists;
            CodeSignatureBuilder.FindRequiredFiles(out Cert, out bOverridesExists);

            return bOverridesExists && (Cert != null);
        }

        /// <summary>
        /// Tries to find a Mac Application certificate on this machine
        /// </summary>
        public static X509Certificate2 FindCertificate()
        {
            // Open the personal certificate store on this machine
            X509Store Store = new X509Store();
            Store.Open(OpenFlags.ReadOnly);

			// Try finding a certificate
            X509Certificate2 Result = null;

			X509Certificate2Collection FoundCerts = Store.Certificates.Find(X509FindType.FindByTimeValid, DateTime.Now, false);
            foreach (X509Certificate2 Certificate in FoundCerts)
            {
				if (Certificate.FriendlyName.Contains("Mac Developer Application"))
				{
					Result = Certificate;
					break;
				}
            }

			Store.Close();

            return Result;
        }

        /// <summary>
        /// Installs a list of certificates to the local store
        /// </summary>
        /// <param name="CertFilenames"></param>
        public static void InstallCertificates(List<string> CertFilenames)
        {
            // Open the personal certificate store on this machine
            X509Store Store = new X509Store();
            Store.Open(OpenFlags.ReadWrite);

            // Install the trust chain certs
            string CertificatePassword = "";
            foreach (string AdditionalCertFilename in CertFilenames)
            {
                X509Certificate2 AdditionalCert = new X509Certificate2(AdditionalCertFilename, CertificatePassword, X509KeyStorageFlags.MachineKeySet);
                Store.Add(AdditionalCert);
            }

            // Close (save) the certificate store
            Store.Close();
        }

        /// <summary>
        /// Loads the signing certificate
        /// </summary>
        protected virtual X509Certificate2 LoadSigningCertificate()
        {
            return FindCertificate();
        }

        // Creates an omitted resource entry for the resource rules dictionary
        protected Dictionary<string, object> CreateOmittedResource(int Weight)
        {
            Dictionary<string, object> Result = new Dictionary<string, object>();
            Result.Add("omit", true);
            Result.Add("weight", (double)Weight);
            return Result;
        }

        protected virtual byte[] CreateCodeResourcesDirectory()
        {
			// Verify that there is at least a CodeResources symlink, as we don't create one in the Zip yet
			if (FileSystem.ReadAllBytes("CodeResources") == null)
			{
				throw new InvalidDataException("Stub IPA must contain a symlink CodeResources -> _CodeSignature/CodeResources");
			}

			// Create a rules dict that includes (by wildcard) everything but Info.plist and the rules file
			Dictionary<string, object> Rules = new Dictionary<string, object>();
			Rules.Add(@"^Resources/", true);
			Rules.Add(@"^Resources/.*\.lproj/", CreateOmittedResource(1000));
			Rules.Add(@"^Resources/.*\.lproj/locversion.plist$", CreateOmittedResource(1100));
			Rules.Add(@"^version.plist$", true);

			// Hash each file in Resources folder
			IEnumerable<string> FileList = FileSystem.GetAllPayloadFiles();
			SHA1CryptoServiceProvider HashProvider = new SHA1CryptoServiceProvider();

			Utilities.PListHelper HashedFileEntries = new Utilities.PListHelper();
			foreach (string Filename in FileList)
			{
				if (Filename.StartsWith("Resources/"))
				{
					byte[] FileData = FileSystem.ReadAllBytes(Filename);
					byte[] HashData = HashProvider.ComputeHash(FileData);

					HashedFileEntries.AddKeyValuePair(Filename, HashData);
				}
			}

			// Create the CodeResources file that will contain the hashes
			Utilities.PListHelper CodeResources = new Utilities.PListHelper();
			CodeResources.AddKeyValuePair("files", HashedFileEntries);
			CodeResources.AddKeyValuePair("rules", Rules);

			// Write the CodeResources file out
			string CodeResourcesAsString = CodeResources.SaveToString();
			byte[] CodeResourcesAsBytes = Encoding.UTF8.GetBytes(CodeResourcesAsString);
			FileSystem.WriteAllBytes("_CodeSignature/CodeResources", CodeResourcesAsBytes);

			return CodeResourcesAsBytes;
		}

		/// <summary>
        /// Prepares this signer to sign an application
        ///   Modifies the following files:
        ///     embedded.mobileprovision
        /// </summary>
        public void PrepareForSigning()
        {
            // Install the Apple trust chain certs (required to do a CMS signature with full chain embedded)
            List<string> TrustChainCertFilenames = new List<string>();
            TrustChainCertFilenames.Add("AppleWorldwideDeveloperRelationsCA.pem");
            TrustChainCertFilenames.Add("AppleRootCA.pem");
            InstallCertificates(TrustChainCertFilenames);

            // Find and load the signing cert
            SigningCert = LoadSigningCertificate();
            if (SigningCert == null)
            {
                // Failed to find a cert already installed or to install, cannot proceed any futher
                Program.Error("... Failed to find a Mac Application certificate to be used for code signing");
            }
            else
            {
                Program.Log("... Found matching certificate '{0}'", SigningCert.FriendlyName);
            }
        }

        /// <summary>
        /// Does the actual work of signing the application
        ///   Modifies the following files:
        ///     Info.plist
        ///     [Executable] (file name derived from CFBundleExecutable in the Info.plist, e.g., UDKGame)
        ///     _CodeSignature/CodeResources
        ///     [ResourceRules] (file name derived from CFBundleResourceSpecification, e.g., CustomResourceRules.plist)
        /// </summary>
        public void PerformSigning()
		{
            DateTime SigningTime = DateTime.Now;

			// Create the code resources file and load it
            byte[] ResourceDirBytes = CreateCodeResourcesDirectory();

			string CFBundleExecutable = @"MacOS\" + Config.BundleName;
			string CFBundleIdentifier = Config.BundleID;

			// Open the executable
            Program.Log("Opening source executable...");
            byte[] SourceExeData = FileSystem.ReadAllBytes(CFBundleExecutable);

            MachObjectFile Exe = new MachObjectFile();
            Exe.LoadFromBytes(SourceExeData);

            //@TODO: Verify it's an executable (not an object file, etc...)


            // Find out if there was an existing code sign blob and find the linkedit segment command
            MachLoadCommandCodeSignature CodeSigningBlobLC = null;
            MachLoadCommandSegment64 LinkEditSegmentLC = null;
            UInt32 SizeOfCommands = 0;
            foreach (MachLoadCommand Command in Exe.Commands)
            {
                SizeOfCommands += Command.CommandSize;
                if (CodeSigningBlobLC == null)
                {
                    CodeSigningBlobLC = Command as MachLoadCommandCodeSignature;
                }

                if (LinkEditSegmentLC == null)
                {
                    LinkEditSegmentLC = Command as MachLoadCommandSegment64;
					if (LinkEditSegmentLC.SegmentName != "__LINKEDIT")
                    {
                        LinkEditSegmentLC = null;
                    }
                }
            }

            if (LinkEditSegmentLC == null)
            {
                throw new InvalidDataException("Did not find a Mach segment load command for the __LINKEDIT segment");
            }

            MemoryStream OutputExeStream = null;

            bool bCreatedNewSigningBlobLC = false;

            // If the existing code signing blob command is missing, make sure there is enough space to add it
            // Insert the code signing blob if it isn't present
            //@TODO: Make sure there is enough space to add it
            if (CodeSigningBlobLC == null)
            {
                MachLoadCommand LastCommand = Exe.Commands[Exe.Commands.Count - 1];
                CodeSigningBlobLC = new MachLoadCommandCodeSignature();
                CodeSigningBlobLC.Command = MachLoadCommand.LC_CODE_SIGNATURE;
                CodeSigningBlobLC.CommandSize = 16;
                CodeSigningBlobLC.BlobFileOffset = (uint)(LinkEditSegmentLC.FileOffset + LinkEditSegmentLC.FileSize);
                CodeSigningBlobLC.BlobFileSize = 0;
                CodeSigningBlobLC.StartingLoadOffset = LastCommand.StartingLoadOffset + LastCommand.CommandSize;
                CodeSigningBlobLC.RequiredForDynamicLoad = false;
                Exe.Commands.Add(CodeSigningBlobLC);
                SizeOfCommands += CodeSigningBlobLC.CommandSize;
                bCreatedNewSigningBlobLC = true;

                // Make enough room in the exe to store the code signature
                byte[] ExtendedExeData = new byte[SourceExeData.GetLength(0) + 0x40000];
                SourceExeData.CopyTo(ExtendedExeData, 0);
                OutputExeStream = new MemoryStream(ExtendedExeData);
            }
            else
            {
                OutputExeStream = new MemoryStream(SourceExeData);
            }

            // Verify that the code signing blob is at the end of the linkedit segment (and thus can be expanded if needed)
            if ((CodeSigningBlobLC.BlobFileOffset + CodeSigningBlobLC.BlobFileSize) != (LinkEditSegmentLC.FileOffset + LinkEditSegmentLC.FileSize))
            {
                throw new InvalidDataException("Code Signing LC was present but not at the end of the __LINKEDIT segment, unable to replace it");
            }

            int SignedFileLength = (int)CodeSigningBlobLC.BlobFileOffset;

            // Create the code directory blob
            CodeDirectoryBlob FinalCodeDirectoryBlob = CodeDirectoryBlob.Create(CFBundleIdentifier, SignedFileLength);

			// Create the entitlements blob
            string EntitlementsText = "";
            EntitlementsBlob FinalEntitlementsBlob = EntitlementsBlob.Create(EntitlementsText);

            // Create or preserve the requirements blob
            RequirementsBlob FinalRequirementsBlob = null;
            if ((CodeSigningBlobLC != null) && Config.bMaintainExistingRequirementsWhenCodeSigning)
            {
                RequirementsBlob OldRequirements = CodeSigningBlobLC.Payload.GetBlobByMagic(AbstractBlob.CSMAGIC_REQUIREMENTS_TABLE) as RequirementsBlob;
                FinalRequirementsBlob = OldRequirements;
            }

            if (FinalRequirementsBlob == null)
            {
                FinalRequirementsBlob = RequirementsBlob.CreateEmpty();
            }

            // Create the code signature blob (which actually signs the code directory)
            CodeDirectorySignatureBlob CodeSignatureBlob = CodeDirectorySignatureBlob.Create();

            // Create the code signature superblob (which contains all of the other signature-related blobs)
            CodeSigningTableBlob CodeSignPayload = CodeSigningTableBlob.Create();
            CodeSignPayload.Add(0x00000, FinalCodeDirectoryBlob);
            CodeSignPayload.Add(0x00002, FinalRequirementsBlob);
            CodeSignPayload.Add(0x00005, FinalEntitlementsBlob);
            CodeSignPayload.Add(0x10000, CodeSignatureBlob);


            // The ordering of the following steps (and doing the signature twice below) must be preserved.
            // The reason is there are some chicken-and-egg issues here:
            //   The code directory stores a hash of the header, but
            //   The header stores the size of the __LINKEDIT section, which is where the signature blobs go, but
            //   The CMS signature blob signs the code directory
            //
            // So, we need to know the size of a signature blob in order to write a header that is itself hashed
            // and signed by the signature blob

            // Do an initial signature just to get the size
            Program.Log("... Initial signature step");
            CodeSignatureBlob.SignCodeDirectory(SigningCert, SigningTime, FinalCodeDirectoryBlob);

            // Compute the size of everything, and push it into the EXE header
            byte[] DummyPayload = CodeSignPayload.GetBlobBytes();

            // Adjust the header and load command to have the correct size for the code sign blob
            WritingContext OutputExeContext = new WritingContext(new BinaryWriter(OutputExeStream));

			long StartPosition = (long)(CodeSigningBlobLC.BlobFileOffset - LinkEditSegmentLC.FileOffset);
			long BlobLength = DummyPayload.Length + 0x295;

			long NonCodeSigSize = (long)(LinkEditSegmentLC.FileSize - CodeSigningBlobLC.BlobFileSize);
			long BlobStartPosition = NonCodeSigSize + (long)LinkEditSegmentLC.FileOffset;

			LinkEditSegmentLC.PatchFileLength(OutputExeContext, (uint)(NonCodeSigSize + BlobLength));
			CodeSigningBlobLC.PatchPositionAndSize(OutputExeContext, (uint)BlobStartPosition, (uint)BlobLength);

            // Now that the executable loader command has been inserted and the appropriate section modified, compute all the hashes
            Program.Log("... Computing hashes");
            OutputExeContext.Flush();

            // Fill out the special hashes
            byte[] RawInfoPList = FileSystem.ReadAllBytes("Info.plist");
            FinalCodeDirectoryBlob.GenerateSpecialSlotHash(CodeDirectoryBlob.cdInfoSlot, RawInfoPList);
            FinalCodeDirectoryBlob.GenerateSpecialSlotHash(CodeDirectoryBlob.cdRequirementsSlot, FinalRequirementsBlob.GetBlobBytes());
            FinalCodeDirectoryBlob.GenerateSpecialSlotHash(CodeDirectoryBlob.cdResourceDirSlot, ResourceDirBytes);
            FinalCodeDirectoryBlob.GenerateSpecialSlotHash(CodeDirectoryBlob.cdApplicationSlot);
            FinalCodeDirectoryBlob.GenerateSpecialSlotHash(CodeDirectoryBlob.cdEntitlementSlot, FinalEntitlementsBlob.GetBlobBytes());

            // Fill out the regular hashes
            FinalCodeDirectoryBlob.ComputeImageHashes(OutputExeStream.ToArray());

            // And compute the final signature
            Program.Log("... Final signature step");
            CodeSignatureBlob.SignCodeDirectory(SigningCert, SigningTime, FinalCodeDirectoryBlob);

            // Generate the signing blob and place it in the output (verifying it didn't change in size)
            byte[] FinalPayload = CodeSignPayload.GetBlobBytes();

            if (DummyPayload.Length != FinalPayload.Length)
            {
                throw new InvalidDataException("CMS signature blob changed size between practice run and final run, unable to create useful code signing data");
            }

            if (bCreatedNewSigningBlobLC)
            {
                int CommandsCountPosition = 4 * sizeof(UInt32);
                OutputExeContext.PushPositionAndJump(CommandsCountPosition);
                OutputExeContext.Write(Exe.Commands.Count);
                OutputExeContext.Write(SizeOfCommands);
                OutputExeContext.PopPosition();
            }

			OutputExeContext.PushPositionAndJump(BlobStartPosition);
			OutputExeContext.Write(FinalPayload);
			OutputExeContext.PopPosition();

			// Truncate the data so the __LINKEDIT section extends right to the end
			OutputExeContext.CompleteWritingAndClose();

			Int64 DesiredExecutableLength = (Int64)(LinkEditSegmentLC.FileSize + LinkEditSegmentLC.FileOffset);
			byte[] FinalExeData = OutputExeStream.ToArray();
            if (FinalExeData.LongLength < DesiredExecutableLength)
            {
                throw new InvalidDataException("Data written is smaller than expected, unable to finish signing process");
            }
            Array.Resize(ref FinalExeData, (int)DesiredExecutableLength); //@todo: Extend the file system interface so we don't have to copy 20 MB just to truncate a few hundred bytes

			// Save the patched and signed executable
            Program.Log("Saving signed executable...");
            FileSystem.WriteAllBytes(CFBundleExecutable, FinalExeData);
		}
    }
}
