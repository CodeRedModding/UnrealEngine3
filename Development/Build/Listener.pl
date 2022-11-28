#!/usr/bin/perl.exe


#############################################################
# directories/programs
$perlpath				= 'c:\\Perl\\bin\\perl.exe';
$buildscript			        = 'c:\\UnrealEngine3Build\\UnrealEngine3\\Development\\Build\\Build.pl';
$buildflag				= '\\\\server\\warfaredev\\UnrealEngine3BuildScript\\logs\\makebuild.txt';
$clientspec				= 'UnrealEngine3Build';

#############################################################
# main

while(1)
{
	print "Checking for buildflag...\n";
	if( -e $buildflag )
	{
		print "MAKING BUILD\n";
		system( "p4 -c \"$clientspec\" sync \"$buildscript\"");
		system( "$perlpath $buildscript" );
		system( "del $buildflag /f" );
	}
	sleep(15);
}

