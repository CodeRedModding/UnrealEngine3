#!/usr/bin/perl.exe

#############################################################
# directories/programs
$root_dir					= 'E:\\Build_UE3\\UnrealEngine3\\';
$systemdir					= $root_dir.'Binaries';
$ucc						= $systemdir.'\\ucc.exe';
$checkoutfilelistfile		= $root_dir.'Development\\Build\\BuildFileList.txt';			 # List of files to delete and build - included in P4
$unobjvercpp				= $root_dir.'Development\\Src\\Core\\Src\\UnObjVer.cpp';

$buildlogdir				= 'c:\\UnrealEngine3Build\\BuildLogs\\';                            # Dir to dump log files into
$failedlogdir				= $buildlogdir."Failed Builds\\";

# perforce options
$clientspec			 = 'UnrealEngine3Build';
$cspecpath			 = 'E:\\Build_UE3\\UnrealEngine3';							
$owner				 = 'build_machine';
$depot				 = '//depot/UnrealEngine3/';

# mail options
$blat				 = 'c:\\blat\\blat.exe';
$fullannounceaddr    = 'warfare-external-list@epicgames.com';
#$announceaddr		 = 'unprog3@udn.epicgames.com';
$announceaddr = 'scott.bigwood@epicgames.com';
#$announceaddr = 'alan.willard@epicgames.com';
$failaddr			 = 'warfare-coders@epicgames.com';
$fromaddr		     = 'build@epicgames.com';
$failsubject		 = "UE3 BUILD FAILED";
$mailserver          = "mercury.epicgames.net";

# build options
$devenv				 = 'c:\\program files\\microsoft visual studio .net 2003\\common7\\ide\\devenv.exe';
$incredibuild        = 0;
$solution			 = $root_dir.'Development\\Src\\UnrealEngine3.sln';		                 # Solution to compile in devenv
$BuildCfg            = 'Release';

@build_games = ('Demo','War','UT');

# debug options
$dosync              = 1;
$docheckout          = 1;                                 # perform actual file checkout?
$dobuild             = 1;
$dobuildpc			 = 1;
$dobuildscript       = 1;
$domail              = 1;                                 # send emails?
$dosubmit            = 1;

# changes categories, match each file to a specific category
%file_cat = ("uc"      =>    "Code",
             "cpp"     =>    "Code",
             "h"       =>    "Code",
             "vcproj"  =>    "Code",
             "sln"     =>    "Code",
             "ini"     =>    "Code",
             "int"     =>    "Code",
             "psh"     =>    "Code",
             "vsh"     =>    "Code",
             "xpu"     =>    "Code",
             "xvu"     =>    "Code",
             "pl"      =>    "Code",
             "bat"     =>    "Code",
             "hlsl"    =>    "Code",
             "ukx"     =>    "Art",
             "usx"     =>    "Art",
             "utx"     =>    "Art",
             "unr"     =>    "Art",
             "dds"     =>    "Art",
             "psd"     =>    "Art",
             "tga"     =>    "Art",
             "bmp"     =>    "Art",
             "pcx"     =>    "Art",
             "ase"     =>    "Art",
             "max"     =>    "Art",
             "raw"     =>    "Art",
             "xmv"     =>    "Art",
             "upk"     =>    "Art",
             "uax"     =>    "Sound",
             "xsb"     =>    "Sound",
             "xwb"     =>    "Sound",
             "xap"     =>    "Sound",
             "wav"     =>    "Sound",
             "wma"     =>    "Sound",
             "ogg"     =>    "Sound",
             "doc"     =>    "Documentation",
             "xls"     =>    "Documentation",
             "html"    =>    "Documentation",
);
%changelist;
$default_change                = 0;       # did default.ini change?
$user_change                   = 0;       # did defaultuser.ini change?

@checkedoutfiles;

#############################################################
# clock/unclock variables
$timer      = 0;
$timer_op   = "";

#############################################################
# elapsed_time
# - returns a string describing the elapsed time between to time() calls
# - $_[0] should be the difference between the time() calls
sub elapsed_time {
   $seconds = $_[0];
   $minutes = $seconds / 60;
   $hours = int($minutes / 60);
   $minutes = int($minutes - ($hours * 60));
   $seconds = int($seconds - ($minutes * 60));
   return $hours." hrs ".$minutes." min ".$seconds." sec";
}

#############################################################
# clock
# - simple timer, call in conjunction with unclock to get a rough estimate of elapsed time
sub clock {
   if ($timer_op ne "") {
      print "Warn: new clock interrupting previous clock for $timer_op!\n";
   }
   $timer_op = $_[0];
   $timer = time;
   print "Starting [$timer_op] at ".localtime($timer)."\n";
}

#############################################################
# unclock
# - calculates the elapsed time since clock was called
sub unclock {
   if ($_[0] ne $timer_op) {
      print "Warn: unclock mismatch, $_[0] vs. $timer_op!\n";
   }
   else
   {
      print "Finished [$timer_op], elapsed: ".elapsed_time(time-$timer)."\n";
   }
   $timer_op = "";
}

#############################################################
sub incenginever()
{
   open( VERFILE, "$unobjvercpp" );
   @vertext = <VERFILE>;
   close(VERFILE);
	
   open(VERFILE, ">$unobjvercpp");
   foreach $verline ( @vertext )
   {
      $verline =~ s/\n//;
      if( $verline =~ m/^#define\sENGINE_VERSION\s([0-9]+)$/ )
      {
         $v = $1+1;
	 $verline = "#define ENGINE_VERSION	$v";
      }
      print VERFILE "$verline\n";
   }
   close(VERFILE);
}

##################################################
sub setversion
{
    ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();
    $version = sprintf("UnrealEngine3_Build_[%04d-%02d-%02d_%02d.%02d]", $year + 1900, $mon + 1, $mday, $hour, $min);
    $announcesubject = "New UnrealEngine3 version in P4: $version";
    $buildlog = $buildlogdir.$version.".log";
}

#############################################################
sub getcheckoutlist
{
	open( CHECKOUTFILELIST, $checkoutfilelistfile );
	chomp( @checkoutfiles = <CHECKOUTFILELIST> );
	close( CHECKOUTFILELIST );
}

#############################################################
sub revertall
{
   system("p4 -c \"$clientspec\" revert -c default \/\/\.\.\. 2>&1");
}

#############################################################
sub checkoutall
{
	foreach $file (@checkoutfiles)
	{
		open( EDIT, "p4 -c \"$clientspec\" edit \"$cspecpath\\$file\" 2>&1 |");
		@edit = <EDIT>;
		close(EDIT);
		
		if( ! ($edit[0] =~ m/\-\ opened\ for\ edit$/) ) {
                   announcefailure("Perforce checkout","<no log file>");
         	  print "UE3 build failure: Checkout failed.\n\n$edit[0]";
		  return 0;
		}
                else
		{
                  foreach $line (@edit) {
                    if ($line =~ /(.+)\#/) {
                      # add to checked out list
                      #print "checked out file:$1\n";
                      push @checkedoutfiles,$1;
                    }
                  }
                }
	}
        # lock all the files opened for edit
        system("p4 -c $clientspec lock 2>&1");
	
	return 1;
}

#############################################################
sub submitall
{
        # unlock the files for check in
        system("p4 -c $clientspec unlock 2>&1");
		# revert unchanged files
#		system("p4 -c $clientspec revert -a 2>&1");

	open( SUBMITCMD, "|p4 -c \"$clientspec\" submit -i" );

	print SUBMITCMD "Change:\tnew\n";
	print SUBMITCMD "Client:\t$clientspec\n";
	print SUBMITCMD "Status:\tnew\n";		
	print SUBMITCMD "Description:\t$version\n";		
        print SUBMITCMD "Files:\n";

        foreach $file (@checkedoutfiles)
        {
                print SUBMITCMD "\t$file#edit\n";
        }
	
	close(SUBMITCMD);
}


#############################################################
sub label
{
   # create the version label
   open( LABELCMD, "|p4 -c \"$clientspec\" label -i" );
   print LABELCMD "Label:\t$version\n";
   print LABELCMD "Owner:\t$owner\n";
   print LABELCMD "Description:\n\t$version\n";		
   print LABELCMD "Options:\tunlocked\n";
   print LABELCMD "View:\n\t\"$depot...\"\n";
   close(LABELCMD);
   system( "p4 -c \"$clientspec\" labelsync -l $version" );

   # create the latestUnrealEngine3 label
   system( -c \"$clientspec\" label -d latestUnrealEngine3" );
   open( LABELCMD, "|p4 -c \"$clientspec\" label -i" );
   print LABELCMD "Label:\tlatestUnrealEngine3\n";
   print LABELCMD "Owner:\t$owner\n";
   print LABELCMD "Description:\n\t$version\n";		
   print LABELCMD "Options:\tunlocked\n";
   print LABELCMD "View:\n\t\"$depot...\"\n";
   close(LABELCMD);
   system( "p4 -c \"$clientspec\" labelsync -l latestUnrealEngine3" );
}

#############################################################
# get changes since last build, put them in @changelist
sub getchanges
{
        # grab the last build change
        $foundlastbuild = 0;
		$lastbuildchange = 0;
		$lastbuildtime = 0;
	open( CHANGES, "p4 changes -c \"$clientspec\" \"$depot...\"|" );
	foreach $line (<CHANGES>) {
           # make sure this is a build version change
           if ($line =~ /^Change ([0-9]+) on ([0-9]{4})\/([0-9]{2})\/([0-9]{2}).+UnrealEngine3_Build_/) {
              $lastbuildchange = $1;
              print "Last known build on $2/$3/$4\n";
			  # read the actual build time
			  # format: UnrealEngine3_Build_[2004-07-30_15.56]
			  open (BUILDDESC,"p4 describe $lastbuildchange|");
			  foreach $line (<BUILDDESC>) {
				 if ($line =~ m/UnrealEngine3_Build_\[([0-9]{4})-([0-9]{2})-([0-9]{2})_([0-9]{2})\.([0-9]{2})\]/) {
					$lastbuildtime = $1.$2.$3.$4.$5;
					print "Build time: $lastbuildtime\n";
					last;
				 }
			  }
			  close (BUILDDESC);
              # stop searching
              $foundlastbuild++;
              last;
           }
        }
	close(CHANGES);
        # make sure $lastbuildchange is valid
        if ($foundlastbuild == 0) {
           print "ERROR: failed to find last build version!\n";
           return;
        }

        # open up the changes for this depot
        open (CHANGES,"p4 changes \"$depot...\"|");
        foreach $change (<CHANGES>) {
           # grab the change number
           if ($change =~ /Change ([0-9]+) on/) {
              $change_num = $1;
              if ($change_num == $lastbuildchange) {
                  skip;
              }
              elsif ($change_num < $lastbuildchange) {
				 if ($lastbuildtime == 0) {
					last;
				 }
				 # check the actual time to make sure it wasn't submitted in between build submissions
				 else
				 {
					$changetime = 0;
					open (DESCRIBE,"p4 describe $change_num|");
                                        foreach $line (<DESCRIBE>) {
					  # on 2004/07/30 16:04:51
					  if ($line =~ m/on ([0-9]{4})\/([0-9]{2})\/([0-9]{2}) ([0-9]{2})\:([0-9]{2})/) {
					     $changetime = $1.$2.$3.$4.$5;
					     # no more changes
					     last;
                                          }
					}
					close (DESCRIBE);
					if ($changetime == 0 || $changetime < $lastbuildtime) {
					   # no more changes
					   last;
					}
				 }
              }
              # list of categories this change affects
              %belongs_in = ();
              # parse the description of this change
              open (DESCRIBE,"p4 describe $change_num|");
              $reading_desc = 1;    # are we reading the description of this change?
			  $change_header = "";
              $desc = "";
			  $warfare_desc = "";
			  $reading_warfare = 0;
              foreach $line (<DESCRIBE>) {
				 # check to see if this is a warfare change
				 if ($line =~ /#\s*Warfare/i) {
					$reading_warfare = 1;
				 }
                 # if we're reading the desc, check to see if this line is the end
				 elsif ($reading_desc == 1) {
                    if ($line =~ /Affected files/) {
                       # we've reached the end of the changes
                       $reading_desc = 0;
                    }
                    else
                    {
                       # append to the list of changes, only if it contains something other than whitespace
                       if ($line =~ /\w/) {
						  if ($line =~ /Change\s*(\d+)/) {
							 $change_header .= $line;
						  }
						  # if we're reading warfare changes, then store in a separate list
						  elsif ($reading_warfare == 1) {
							 $warfare_desc .= $line;
						  }
						  else
						  {
							 $desc .= $line;
						  }
                       }
                    }
                 }
                 else
                 {
                    # check to see if this line contains a file ref
                    if ($line =~ /\.(\w+)\#/) {
                       $category = $file_cat{lc($1)};
                       if ($category eq "") {
                          $category = "Misc";
                       }
                       # mark this category as one we belong in
                       $belongs_in{$category} += 1;    # some arbitrary value
                       # check if it's a critical change
                       if ($default_change == 0 && $line =~ /Default\.ini/) {
                          $default_change = 1;
                       }
                       elsif ($user_change == 0 && $line =~ /DefUser\.ini/) {
                          $user_change = 1;
                       }
                    }
                 }
              }
              close (DESCRIBE);
              # for each category it belongs to, add the desc
			  if ($desc ne "") {
				 $desc = $change_header.$desc;
				 $best_cnt = 0;
				 $best_cat = "Misc";
				 foreach $cat (keys(%belongs_in)) {
					if ($belongs_in{$cat} > $best_cnt) {
					   $best_cnt = $belongs_in{$cat};
					   $best_cat = $cat;
					}
				 }
				 $changelist{$best_cat} .= $desc."\n";
			  }
			  if ($warfare_desc ne "") {
				 $warfare_desc = $change_header.$warfare_desc;
				 $changelist{"Warfare"} .= $warfare_desc."\n";
			  }
           }
        }
        # all done!
        close (CHANGES);
}

#############################################################
# return 1 if change was from $depot, 0 if not
sub belongstodepot {
   $changenum = $_[0];
   $result = 0;
   open(DESCRIBE, "p4 describe $changenum|");
   foreach $line (<DESCRIBE>) {
      if ($line =~ /$depot/) {
         $result = 1;
         last;
      }
   }
   close(DESCRIBE);
   return $result;
}

#############################################################
sub sendannounce {
   $attempts = 0;
   $success = 0;
   while ($success == 0 && $attempts < 10) {
      open( SMTP, "|\"$blat\" - -t $announceaddr -f $fromaddr -s \"$announcesubject\" -server $mailserver > output.txt" );
      print SMTP '@'."$version\n\nCode changes since last version:\n\n";

      # if there was a critical change, mark in the build notes
      if ($default_change == 1) {
         print SMTP "NOTE: Default.ini has changed!\n\n";
      }
      if ($user_change == 1) {
         print SMTP "NOTE: DefUser.ini has changed!\n\n";
      }

      # only dump code changes
      if ($changelist{"Code"} ne "") {
         print SMTP "CODE CHANGES:\n\n";
         print SMTP $changelist{"Code"}."\n";
      }
      else
      {
         print SMTP "No changes.\n";
      }
      close(SMTP);

      # make sure we were able to send the mail
      if (open(OUTPUT,"output.txt")) {
         $errors = 0;
         foreach $line (<OUTPUT>) {
            if ($line =~ m/Error:/i) {
               chomp($line);
               $errors++;
            }
         }
         $success = $errors == 0 ? 1 : 0;
         close(OUTPUT);
      }
      else
      {
         $success = 1;
      }

      # if we failed, delay a few seconds before attempting again
      if ($success == 0) {
         print "Unable to send mail, retrying in 15 seconds...\n";
         sleep(15);
      }
      $attempts++;
   }
   # notify that we weren't able to send the mail
   if ($success == 0) {
      print "Failed to send mail after $attempts attempts!\n";
   }
}

sub sendfullannounce {
   $attempts = 0;
   $success = 0;
   while ($success == 0 && $attempts < 10) {
      open( SMTP, "|\"$blat\" - -t $fullannounceaddr -f $fromaddr -s \"$announcesubject\" -server $mailserver > output.txt" );
      print SMTP '@'."$version\n\nChanges since last version:\n\n";
      # if there was a critical change, mark in the build notes
      if ($default_change == 1) {
         print SMTP "NOTE: Please delete Unreal.ini, otherwise the game may not run correctly!\n\n";
      }
      if ($user_change == 1) {
         print SMTP "NOTE: Please delete User.ini, otherwise the game may not run correctly!\n\n";
      }

      if ($changelist{"Code"} ne "") {
         print SMTP "ENGINE CODE CHANGES:\n\n";
         print SMTP $changelist{"Code"}."\n";
      }

	  if ($changelist{"Warfare"} ne "") {
		 print SMTP "WARFARE CODE CHANGES:\n\n";
		 print SMTP $changelist{"Warfare"}."\n";
	  }

      if ($changelist{"Art"} ne "") {
         print SMTP "ART CHANGES:\n\n";
         print SMTP $changelist{"Art"}."\n";
      }

      if ($changelist{"Documentation"} ne "") {
         print SMTP "DOCUMENTATION CHANGES:\n\n";
         print SMTP $changelist{"Documentation"}."\n";
      }

      if ($changelist{"Sound"} ne "") {
         print SMTP "SOUND CHANGES:\n\n";
         print SMTP $changelist{"Sound"}."\n";
      }

      if ($changelist{"Misc"} ne "") {
         print SMTP "MISC CHANGES:\n\n";
         print SMTP $changelist{"Misc"}."\n";
      }
      close(SMTP);

      # make sure we were able to send the mail
      if (open(OUTPUT,"output.txt")) {
         $errors = 0;
         foreach $line (<OUTPUT>) {
            if ($line =~ m/Error:/i) {
               chomp($line);
               $errors++;
            }
         }
         $success = $errors == 0 ? 1 : 0;
         close(OUTPUT);
      }
      else
      {
         $success = 1;
      }

      # if we failed, delay a few seconds before attempting again
      if ($success == 0) {
         print "Unable to send mail, retrying in 15 seconds...\n";
         sleep(15);
      }
      $attempts++;
   }
   # notify that we weren't able to send the mail
   if ($success == 0) {
      print "Failed to send mail after $attempts attempts!\n";
   }
}

sub saveannounce {
   if (open(CHANGELOG,"> ".$buildlogdir."changes_$version.txt"))
   {
      print CHANGELOG '@'."$version\n\nChanges since last version:\n\n";
      # if there was a critical change, mark in the build notes
      if ($default_change == 1) {
         print CHANGELOG "NOTE: Please delete Unreal.ini, otherwise the game may not run correctly!\n\n";
      }
      if ($user_change == 1) {
         print CHANGELOG "NOTE: Please delete User.ini, otherwise the game may not run correctly!\n\n";
      }

      if ($changelist{"Code"} ne "") {
         print CHANGELOG "CODE CHANGES:\n\n";
         print CHANGELOG $changelist{"Code"}."\n";
      }

	  if ($changelist{"Warfare"} ne "") {
		 print CHANGELOG "WARFARE CODE CHANGES:\n\n";
		 print CHANGELOG $changelist{"Warfare"}."\n";
	  }

      if ($changelist{"Art"} ne "") {
         print CHANGELOG "ART CHANGES:\n\n";
         print CHANGELOG $changelist{"Art"}."\n";
      }

      if ($changelist{"Documentation"} ne "") {
         print CHANGELOG "DOCUMENTATION CHANGES:\n\n";
         print CHANGELOG $changelist{"Documentation"}."\n";
      }

      if ($changelist{"Sound"} ne "") {
         print CHANGELOG "SOUND CHANGES:\n\n";
         print CHANGELOG $changelist{"Sound"}."\n";
      }

      if ($changelist{"Misc"} ne "") {
         print CHANGELOG "MISC CHANGES:\n\n";
         print CHANGELOG $changelist{"Misc"}."\n";
      }

      close(CHANGELOG);
   }
}

sub announce
{
   if ($domail == 1) {
      # announce all changes to internal list
      sendfullannounce;
      # announce code change to external list
      sendannounce;
   }
   # save all changes locally as well
   saveannounce;
}

#############################################################
# look through the file for failures and errors.  if find none, succeeded.
sub parse_native_log
{
   local $log_file = $_;
   open(COMPILELOG, $log_file);
   while (<COMPILELOG>)
   {
	  chomp;
	  if ( /.*([0-9]+)\ failed/ )
	  {
		 if( $1 != 0 )
		 {
			close(COMPILELOG);
			return 0;
		 }
	  }
	  
	  if ( /.*([0-9]+)\ error\(s\)/ )
	  {
		 if ( $1 != 0 ) {
			close (COMPILELOG);
			return 0;
		 }
	  }

	}
	close(COMPILELOG);
	return 1;
}

#########################################
# parse_script
# - determines error/warning count for script compiles
sub parse_script_log {
   local $log_file = $_;
   $error_cnt = 0;
   $warning_cnt = 0;
   $header_cnt = 0;
   $last_class = "Unknown class";
   @errors;
   @warnings;
   # try to open ucc.log
   if (open(UCCLOG,$log_file)) {
      local $last_header = "";
      local $last_class = "";
      # parse contents of the log file
      foreach $line (<UCCLOG>) {
         chomp($line);
         # generic error format
         if ($line =~ m/: Error/i) {
            push(@errors,$line);
            $error_cnt++;
         }
         # or a more specific error
         elsif ($line =~ m/Critical:/i) {
            push(@errors,$line);
            $error_cnt++;
         }
         # generic warning format
         elsif ($line =~ m/: Warning/i) {
            push(@warnings,$line);
         }
         # special check for import warnings
         elsif ($line =~ m/Unknown property/i) {
            push(@warnings,$last_class.": ".$line);
            $warning_cnt++;
         }
         # check for the class for the following warnings
         elsif ($line =~ m/Class: (\w+) extends/i) {
            $last_class = $1;
         }
         elsif ($line =~ m/Cannot export/i) {
            push(@errors,$line);
         }
      }
      # close the log file
      close(UCCLOG);
   }
   else
   {
      print "Failed to open $log_file!\n";
   }
   return ($error_cnt == 0);
}

#############################################################
sub announcefailure
{
   local ($localpart) = $_[0];
   local ($localfile) = $_[1];

   if ( $domail == 1 ) {
      $attempts = 0;
      $success = 0;
      while ($success == 0 && $attempts < 10) {
         open( SMTP, "|\"$blat\" - -t $failaddr -f $fromaddr -s \"$failsubject\" -server $mailserver > output.txt" );
         print SMTP "UE3 build failure: $localpart compile failed.\n\nSee $localfile for the build log.";
         close( SMTP );

         # make sure we were able to send the mail
         if (open(OUTPUT,"output.txt")) {
            $errors = 0;
            foreach $line (<OUTPUT>) {
               if ($line =~ m/Error:/i) {
                  chomp($line);
                  $errors++;
               }
            }
            $success = $errors == 0 ? 1 : 0;
            close(OUTPUT);
         }
         else
         {
            $success = 1;
         }

         # if we failed, delay a few seconds before attempting again
         if ($success == 0) {
            print "Unable to send mail, retrying in 15 seconds...\n";
            sleep(15);
         }
         $attempts++;
      }
   }
   print "UE3 build failure: $localpart compile failed.\n\nSee $localfile for the build log.";
}

sub compile_pc {
   local $build_game = $_[0];
   #system("\"$devenv\" \"$solution\" /rebuild \"Release\" /project \"PCLaunch-".$build_game."Game\" /projectconfig \"Release\" /out $buildlog");
   # build pc
   print "- Building PC\n";
   system("\"$devenv\" \"$solution\" /rebuild \"Release\" /out $buildlog");
   return $buildlog;
}

sub compile_xenon {
   local $build_game = $_[0];
   # build xenon
   print "- Building Xenon\n";
   system("\"$devenv\" \"$solution\" /rebuild \"XeRelease\" /out $buildlog");
   return $buildlog;
}

sub compile_script {
   local $build_game = $_[0];
   local $script_dir = $root_dir.$build_game."Game\\";
   # delete all writeable .ini's
   system("del ".$script_dir."Config\\*.ini");
   # remove all existing .u's
   system("del ".$script_dir."Script\\*.u /f");
   # remove log file
   system("del ".$script_dir."Logs\\Launch.log /f");
   # and compile
   system("..\\..\\Binaries\\".$build_game."Game make -silent");
   # wait for the log, indicating build finish
   while (!(-e $script_dir."Logs\\Launch.log")) {
	  sleep(10);
   }
   # check for build errors
   return $script_dir."Logs\\ucc.log";
}

#############################################################
sub dobuild
{
        $build_time = time;
		# sync
        if ($dosync == 1) {
           clock "SYNC";
           system("p4 -c \"$clientspec\" sync \"$depot...\"");
           unclock "SYNC";
        }
	
        if ($docheckout == 1) {
           clock "CHECKOUT";
           # retrieve checkout list from BuildFileList.txt
           getcheckoutlist();
           # mark checkout list files for edit in P4
           if( !checkoutall() ) {
             revertall();
             return;
           }
           unclock "CHECKOUT";
        }

        clock "GET CHANGES";
        getchanges(0);
        unclock "GET CHANGES";

        setversion;

        if ($dobuild == 1) {
            print "Compiling new build version: $version\n";

            # increment the engine version
            incenginever();

			# first build native
            clock "BUILD CPP";
			if ($dobuildpc == 1) {
			   local $compilelog = compile_pc($game);
			   if (parse_native_log($compilelog) == 0)
			   {
				  system("copy $compilelog $failedlogdir /y");
				  revertall();
				  announcefailure("Native Rebuild ($game)", $failedlogdir);
				  return;
			   }
			   # copy xenon executables to binaries folder
			  $compilelog = compile_xenon($game);
			   if (parse_native_log($compilelog) == 0) {
			    system("copy $compilelog $failedlogdir /y");
			     revertall();
			   announcefailure("Xenon Rebuild ($game)", $failedlogdir);
			     return;
			   }
			   #system("copy ".$root_dir."Development\\lib\\xerelease\\lib\\xerelease\\*.exe ".$root_dir."Binaries\\ /y");
			}
            unclock "BUILD CPP";

			# build script next
			clock "BUILD SCRIPT";
			if ($dobuildscript == 1) {
			   foreach $game (@build_games) {
				  local $compilelog = compile_script($game);
				  if (parse_script_log($compilelog) == 0) {
					 system("copy $compilelog $failedlogdir /y");
					 revertall();
					 announcefailure("Script Rebuild ($game)", $failedlogdir);
					 return;
				  }
			   }
			}
			unclock "BUILD SCRIPT";
        }
	
        if ($dosubmit == 1) {
           # submit
           print "START SUBMIT $version\n";
           clock "SUBMIT BUILD";
           submitall();
           # label
           label();
           unclock "SUBMIT BUILD";
        }

	# announce build success
    clock "ANNOUNCE";
	announce();
    unclock "ANNOUNCE";

	print "BUILD COMPLETE $version\n";
    print "Total time: ".elapsed_time(time-$build_time)."\n";
}

#############################################################
# main

# make sure the compile log directory exists
if( !(-e $buildlogdir) ) {
   system("mkdir $buildlogdir");
}

# fire off the build
dobuild;
