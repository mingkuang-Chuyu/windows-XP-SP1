########################
# Cabinet Cache Manager
########################

$RedistInfoLoc=$ARGV[0];
$CabCacheTargetFile=$ARGV[1]; 
$CabCacheFlag=$ARGV[2];
$ProductTimeDate=$ARGV[3];
$MyNTTREE=$ARGV[4];
$Language=$ARGV[5];
$LocalizedRoot=$ARGV[6];

$RootDir=$ENV{DXROOT};
$BuildType=$ENV{NTDEBUG};
$AltSourceFiles=$ENV{_ALT_NTTREE};
$MultiLocalizedDirectory="$LocalizedRoot\\multi";
$LocalizedDirectory="$LocalizedRoot\\$Language";
$BuildWarnMsg="nmake : warning CACMGR : ";
$BuildErrMsg="nmake : error CACMGR : ";
$WriteLockFile="wlfile.lck";

open (WRITELOCK,">$WriteLockFile") || die "$BuildWrnMsg Time/Date process in use. Can't open $WriteLockFile";
 
$Path="$RootDir\\public\\sdk\\lib\\placefil.txt";

open(PLACEFIL, $Path) || die "$BuildErrorMsg Unable to open placefil.txt at ".$Path." for input\n";
@LocalizedFiles =grep(m/;localize/, <PLACEFIL>);
close (PLACEFIL);

$FileName=substr($CabCacheTargetFile,rindex($CabCacheTargetFile,"\\")+1);

open (REDISTFILES,$RedistInfoLoc) || die "$BuildErrorMsg Unable to open nt50redist.dat at $RedistInfoLoc for input\n";
($FileConfiguration)=grep(m/$FileName/ig, <REDISTFILES>);
close (REDISTFILES);

($FileName, $SubPath, $RedistTimeDate, $Filters) = split /,/,$FileConfiguration,4;

$RedistTimeDate=~s/^\s*(.*?)\s*$/$1/;
$FileName=~s/^\s*(.*?)\s*$/$1/;
$SubPath=~s/^\s*(.*?)\s*$/$1/;
 
	if (grep(m/$FileName/i, @LocalizedFiles))
	{
#		if ($Filters=~m/SINGLELANGUAGE/ig)
		if (!($Language eq "eng" && $FileName eq "dxdiag.exe"))
		{
			$SubPath=($Language ne "eng")?(($SubPath=~m/Win9X/i)?$SubPath."\\".$LocalizedDirectory:$LocalizedDirectory):$SubPath; 
		} else {
		   $SubPath=$MultiLocalizedDirectory;
		}
	}

	if ($FileName=~m/\.cat/i) {
		$SubPath=$SubPath."\\".$Language; 
	}

#	if (($Filters=~m/SWITCHFILE/i) && !(grep(m/$FileName/i, @LocalizedFiles)))
	if (($Filters=~m/SWITCHFILE/i) && (!(grep(m/$FileName/i, @LocalizedFiles)) || ($Language eq "eng")))
	{
		if ($BuildType eq "" || $BuildType eq "ntsdnodbg")
		{
			if ($Filters=~m/SWITCHFILER/i){
				$OriginalFile=$MyNTTREE."\\".$SubPath."\\".$FileName;
			}
			if ($Filters=~m/SWITCHFILED/i){
				$OriginalFile=$AltSourceFiles."\\".$SubPath."\\".$FileName;
			}
		} else {
			if ($Filters=~m/SWITCHFILER/i){
				$OriginalFile=$AltSourceFiles."\\".$SubPath."\\".$FileName;
			}
			if ($Filters=~m/SWITCHFILED/i){
				$OriginalFile=$MyNTTREE."\\".$SubPath."\\".$FileName;
			}
		}
	} else {
		$OriginalFile=$MyNTTREE."\\".$SubPath."\\".$FileName;
	}

	if (-e $OriginalFile) {
		open (MAKEFLAG,">$CabCacheFlag") || die "$BuildErrMsg Problem creating flag file $CabCacheFlag";
		close (MAKEFLAG);
	} else {
		die "nmake : error : can not find the file to pack: $OriginalFile";
	}

printf("\ncopying %s to \n %s\n\n",$OriginalFile, $CabCacheTargetFile);
system "copy $OriginalFile,$CabCacheTargetFile";

$TimeDateToUse=(($RedistTimeDate=~m/CURRENT DATE/i)||($RedistTimeDate eq ""))?$ProductTimeDate:$RedistTimeDate;
system("touch /f /t $TimeDateToUse $CabCacheTargetFile\n");

close (WRITELOCK);
exit;
