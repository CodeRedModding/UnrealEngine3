How to install the Add-in
-------------------------

Currently, only manual installation is supported.  

1. Close Visual Studio
2. Open your "My Documents" directory and browse to the "Visual Studio 2008" directory.
3. Open the "Addins" directory.
4. Copy the file "UnrealscriptDevSuite.AddIn" from your root source directory in to the "Addins" folder
5. Copy the file "UnrealscriptDevelopmentSuite.dll" from the \Bin directory in to the "Addins" folder

At this point, UDS is installed and ready to go. To bring up the hierarchy go to "View | Other Windows".


How to debug the add-in
-----------------------

Developing the add-in requires a different type of .AddIn file in your documents directory.  You will need to rename it from "UnrealscriptDevSuite.AddIn" to "UnrealscriptDevSuite - For Testing.AddIn".  You will also need to make several modifications that Visual Studio would have done for you if you created a new add-in.  So after renaming the file in your documents folder, edit it and make the following changes:

1. Find all entries "<Assembly>" and update the entries to point to the dll in your development\bin directory
2. Find the entry "<LoadBehavior> under "<Addin>" and update the entry to be "0" instead of "1"

Save this file and you should now be able to work/debug the add-in.  Note, you will need to load Visual Studio once, then close it before you can successfully compile the source.


Known Bugs
----------

[-] The docking information for the Hierarchy window will sometimes be lost causing it to being in the floating state
[-] If you attempt to dock the hierarchy window while the classes are reloading, it will crash


Release History

[1/18/2011]
  - Initial release


