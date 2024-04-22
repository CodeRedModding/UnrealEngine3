#!/usr/bin/perl.exe

###############################################################################
# Documentation options.

$doUE3Docs           = 1;    # Generate UE3 documentation?
$doXenonDocs         = 1;    # Generate Xenon-specific documentation?
$doPS3Docs           = 1;    # Generate PS3-specific documentation?

$deleteTempFiles     = 1;    # Delete temporary HTML files?
$doxygen             = 'c:\\bin\doxygen\\doxygen.exe';

$doxyfileUE3         = 'UnrealEngine3.dox';
$doxyfileXenon       = 'Xenon.dox';
$doxyfilePS3         = 'PS3.dox';

$doxyUE3Logfile      = 'latestDoxygenOutput_UE3.log';
$doxyXenonLogfile    = 'latestDoxygenOutput_Xenon.log';
$doxyPS3Logfile      = 'latestDoxygenOutput_PS3.log';

###############################################################################
# clock/unclock variables

$timer      = 0;
$timer_op   = "";

###############################################################################
# elapsedTime
# - Returns a string describing the elapsed time between to time() calls.
# - Input arg $_[0] is the difference between the time() calls.
###############################################################################

sub elapsedTime
{
	$seconds = $_[0];
	$minutes = $seconds / 60;
	$hours = int($minutes / 60);
	$minutes = int($minutes - ($hours * 60));
	$seconds = int($seconds - ($minutes * 60));
	return $hours." hrs ".$minutes." min ".$seconds." sec";
}

###############################################################################
# clock/unclock
# - Provides simple timing; a set of clock/unclock calls delimit a timing interval.
# - Both clock and unclock print start/end timing status to the output file handle.
# - clock/unclock blocks cannot be nested.  Unmatched or nested calls cause
#   warnings to be printed out.
###############################################################################

sub clock
{
	if ($timer_op ne "")
	{
		print "Warn: new clock interrupting previous clock for $timer_op!\n";
	}

	$timer_op = $_[0];
	$timer = time;
	print "Starting [$timer_op] at ".localtime($timer)."\n";
}


sub unclock
{
	if ($_[0] ne $timer_op)
	{
		print "Warn: unclock mismatch, $_[0] vs. $timer_op!\n";
	}
	else
	{
		print "Finished [$timer_op], elapsed: ".elapsedTime(time-$timer)."\n";
	}

	$timer_op = "";
}

###############################################################################

sub BuildDocs
{
	if ( $doUE3Docs == 1 )
	{
		print "Building UnrealEngine3 documentation . . .\n";
		system("$doxygen $doxyfileUE3 > $doxyUE3Logfile 2>&1");
	}

	if ( $doXenonDocs == 1 )
	{
		print "Building Xenon-specific documentation . . .\n";
		system("$doxygen $doxyfileXenon > $doxyXenonLogfile 2>&1");	
	}

	if ( $doPS3Docs == 1 )
	{
		print "Building PS3-specific documentation . . .\n";
		system("$doxygen $doxyfilePS3 > $doxyPS3Logfile 2>&1");	
	}


	# Copy off the compiled html files and kill temps.
	system("copy ..\\Documentation\\PC\\html\\UnrealEngine3.chm ..\\Documentation\\");
	system("copy ..\\Documentation\\Xenon\\html\\Xenon.chm ..\\Documentation\\Xenon\\");
	system("copy ..\\Documentation\\PS3\\html\\PS3.chm ..\\Documentation\\PS3\\");	

	if ( $deleteTempFiles == 1 )
	{
		system("rmdir /s /q ..\\Documentation\\html\\");
	}
}

###############################################################################
# main

clock "DOCUMENTATION";
BuildDocs;
unclock "DOCUMENTATION";
