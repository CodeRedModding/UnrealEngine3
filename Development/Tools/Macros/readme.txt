Unfortunately, .vsmacros files are binary, which means no easy merging, etc...

In an effort to make updating your local copies of the macros easier if you've made any local modifications, I've split out the current contents of the vsmacros file into individual source files in the SourceFiles directory.  

Please take the time to also update those files when you check in a new version of the EpicMacros.vsmacros file.  They are split along module lines and can be easily exported from the Macro IDE using File..Export File.

If you've made local modifications (and other developers have been good), just look at the changes made to the .vb files and apply those directly to your copy.
