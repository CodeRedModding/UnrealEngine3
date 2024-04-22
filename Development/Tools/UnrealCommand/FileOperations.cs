/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;

namespace UnrealCommand
{
	partial class Program
	{
		/**
		 * Safely delete a directory
		 */
		static private void DeleteDirectory(DirectoryInfo DirInfo)
		{
			if (DirInfo.Exists)
			{
				foreach (FileInfo Info in DirInfo.GetFiles())
				{
					try
					{
						Info.IsReadOnly = false;
						Info.Delete();
					}
					catch
					{
						Error("Could not delete file: " + Info.FullName);
						ReturnCode = 300;
					}
				}

				foreach (DirectoryInfo SubDirInfo in DirInfo.GetDirectories())
				{
					DeleteDirectory(SubDirInfo);
				}

				try
				{
					DirInfo.Delete();
				}
				catch
				{
					Error("Could not delete folder: " + DirInfo.FullName);
					ReturnCode = 310;
				}
			}
		}

		/**
		 * Safely delete a file
		 */
		static private void DeleteFile(string FileName)
		{
			FileInfo Info = new FileInfo(FileName);
			if (Info.Exists)
			{
				Info.IsReadOnly = false;
				Info.Delete();
			}
		}

		/**
		 * Safely copy a folder structure
		 */
		static private void RecursiveFolderCopy(DirectoryInfo SourceFolderInfo, DirectoryInfo DestFolderInfo)
		{
			foreach (FileInfo SourceFileInfo in SourceFolderInfo.GetFiles())
			{
				string DestFileName = Path.Combine(DestFolderInfo.FullName, SourceFileInfo.Name);
				SourceFileInfo.CopyTo(DestFileName);

				FileInfo DestFileInfo = new FileInfo(DestFileName);
				DestFileInfo.IsReadOnly = false;
			}

			foreach (DirectoryInfo SourceSubFolderInfo in SourceFolderInfo.GetDirectories())
			{
				string DestFolderName = Path.Combine(DestFolderInfo.FullName, SourceSubFolderInfo.Name);
				Directory.CreateDirectory(DestFolderName);
				RecursiveFolderCopy(SourceSubFolderInfo, new DirectoryInfo(DestFolderName));
			}
		}

		static private bool CopyFolder(string SourceFolder, string DestFolder)
		{
			DirectoryInfo SourceFolderInfo = new DirectoryInfo(SourceFolder);
			if (!SourceFolderInfo.Exists)
			{
				Error("Source folder does not exist");
				return (false);
			}

			DirectoryInfo DestFolderInfo = new DirectoryInfo(DestFolder);
			if (DestFolderInfo.Exists)
			{
				DeleteDirectory(DestFolderInfo);
			}

			Directory.CreateDirectory(DestFolderInfo.FullName);

			RecursiveFolderCopy(SourceFolderInfo, DestFolderInfo);

			return (true);
		}

		/** 
		 * Copy a set of files
		 */
		static private bool CopyFiles(string SourceFolder, string DestFolder, string DisplayDestFolder, string FileSpec, string ExcludeExtension)
		{
			if (DisplayDestFolder == null)
			{
				DisplayDestFolder = DestFolder;
			}

			Log(" ... '" + SourceFolder + "\\" + FileSpec + "' -> '" + DisplayDestFolder + "'");

			// Ensure the source folder exists - fail otherwise
			DirectoryInfo SourceFolderInfo = new DirectoryInfo(SourceFolder);
			if (!SourceFolderInfo.Exists)
			{
				Error("Source folder does not exist: " + SourceFolderInfo);
				return (false);
			}

			// Ensure the destination folder exists - create otherwise
			DirectoryInfo DestFolderInfo = new DirectoryInfo(DestFolder);
			if (!DestFolderInfo.Exists)
			{
				Directory.CreateDirectory(DestFolder);
			}

			foreach (FileInfo SourceInfo in SourceFolderInfo.GetFiles(FileSpec))
			{
				// Don't copy any file with the excluded extension
				if (ExcludeExtension != null && SourceInfo.Extension == ExcludeExtension)
				{
					continue;
				}

				string DestPathName = Path.Combine(DestFolderInfo.FullName, SourceInfo.Name);
				try
				{
					// Delete the destination file if it exists
					FileInfo DestInfo = new FileInfo(DestPathName);
					if (DestInfo.Exists)
					{
						DestInfo.IsReadOnly = false;
						DestInfo.Delete();
					}

					// Copy over the new file
					SourceInfo.CopyTo(DestPathName);

					// Make sure the destination file is writable
					DestInfo = new FileInfo(DestPathName);
					DestInfo.IsReadOnly = false;
				}
				catch
				{
					Error("Failed to copy file: " + DestPathName);
					ReturnCode = 320;
				}
			}

			return (true);
		}

		/**
		 * Rename a file
		 */
		static private bool RenameFile(string Folder, string SourceName, string DestName)
		{
			Log(" ... '" + Folder + "\\" + SourceName + "' -> '" + DestName + "'");

			// Ensure the source file exists - fail otherwise
			string FullSourceName = Path.Combine(Folder, SourceName);
			FileInfo SourceInfo = new FileInfo(FullSourceName);
			if (!SourceInfo.Exists)
			{
				Error("File does not exist for renaming: " + FullSourceName);
				return (false);
			}

			// Delete the destination file if it exists
			string FullDestName = Path.Combine(Folder, DestName);
			FileInfo DestInfo = new FileInfo(FullDestName);
			if (DestInfo.Exists)
			{
				DestInfo.IsReadOnly = false;
				DestInfo.Delete();
			}

			// Rename
			SourceInfo.MoveTo(DestInfo.FullName);

			return (true);
		}

		/* 
		 * Convert a text .xcent file to binary
		 * 
		 * @param SourcePath Ascii .xcent file (can have Windows line feeds)
		 * @param DestPath Binary .xcent file location (will have a binary header, and Mac line feeds)
		 * 
		 * @return true if successful
		 */
		static private bool WrapXcentFile(string SourcePath, string DestPath)
		{
			Log(" ... '" + SourcePath + "' => '" + DestPath + "'");

			// amke sure file exists
			if (!File.Exists(SourcePath))
			{
				Error("Source file does not exist: " + SourcePath);
				return false;
			}

			// read in the text
			string SourceContents = File.ReadAllText(SourcePath);
			SourceContents = SourceContents.Replace("\r\n", "\n");

			// convert the string to bytes
			System.Text.ASCIIEncoding Encoding = new System.Text.ASCIIEncoding();
			byte[] StringBytes = Encoding.GetBytes(SourceContents);

			// make header bytes to write to a binary file
			byte[] HeaderBytes = BitConverter.GetBytes(0x7171DEFA);

			List<byte> TotalBytes = new List<byte>(HeaderBytes);

			// swap the bytes for the size
			UInt32 BinarySize = (UInt32)StringBytes.Length + sizeof(UInt32) * 2;
			TotalBytes.Add((byte)((BinarySize >> 24) & 0xFF));
			TotalBytes.Add((byte)((BinarySize >> 16) & 0xFF));
			TotalBytes.Add((byte)((BinarySize >> 8) & 0xFF));
			TotalBytes.Add((byte)((BinarySize >> 0) & 0xFF));

			// append the meat of the file
			TotalBytes.AddRange(StringBytes);

			try
			{
				// write out all the bytes
				File.WriteAllBytes(DestPath, TotalBytes.ToArray());
			}
			catch
			{
				return false;
			}

			return true;
		}

		/* 
		 * Creates a PkgInfo file at the specified path
		 * 
		 * @param DestPath Location to write the PkgInfo to
		 * 
		 * @return true if successful
		 */
		static private bool WritePkgInfo(string DestPath)
		{
			Log(" ... creating file '<PAYLOADDIR>\\PkgInfo'");

			try
			{
				// the file is actually just the string APPL????
				File.WriteAllText(DestPath, "APPL????");
			}
			catch
			{
				return false;
			}

			return true;
		}
	}
}
