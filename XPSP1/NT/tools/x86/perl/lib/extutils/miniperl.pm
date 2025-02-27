# This File keeps the contents of miniperlmain.c.
#
# It was generated automatically by minimod.PL from the contents
# of miniperlmain.c. Don't edit this file!
#
#       ANY CHANGES MADE HERE WILL BE LOST! 
#


package ExtUtils::Miniperl;
require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(&writemain);

$head= <<'EOF!HEAD';
/*
 * "The Road goes ever on and on, down from the door where it began."
 */

#ifdef OEMVS
#pragma runopts(HEAP(1M,32K,ANYWHERE,KEEP,8K,4K))
#endif


#include "EXTERN.h"
#include "perl.h"

static void xs_init _((void));
static PerlInterpreter *my_perl;

#if defined (__MINT__) || defined (atarist)
/* The Atari operating system doesn't have a dynamic stack.  The
   stack size is determined from this value.  */
long _stksize = 64 * 1024;
#endif

int
main(int argc, char **argv, char **env)
{
    int exitstatus;

#ifdef PERL_GLOBAL_STRUCT
#define PERLVAR(var,type) /**/
#define PERLVARI(var,type,init) PL_Vars.var = init;
#define PERLVARIC(var,type,init) PL_Vars.var = init;
#include "perlvars.h"
#undef PERLVAR
#undef PERLVARI
#undef PERLVARIC
#endif

    PERL_SYS_INIT(&argc,&argv);

    perl_init_i18nl10n(1);

    if (!PL_do_undump) {
	my_perl = perl_alloc();
	if (!my_perl)
	    exit(1);
	perl_construct( my_perl );
	PL_perl_destruct_level = 0;
    }

    exitstatus = perl_parse( my_perl, xs_init, argc, argv, (char **) NULL );
    if (!exitstatus) {
	exitstatus = perl_run( my_perl );
    }

    perl_destruct( my_perl );
    perl_free( my_perl );

    PERL_SYS_TERM();

    exit( exitstatus );
    return exitstatus;
}

/* Register any extra external extensions */

EOF!HEAD
$tail=<<'EOF!TAIL';

static void
xs_init(void)
{
}
EOF!TAIL

sub writemain{
    my(@exts) = @_;

    my($pname);
    my($dl) = canon('/','DynaLoader');
    print $head;

    foreach $_ (@exts){
	my($pname) = canon('/', $_);
	my($mname, $cname);
	($mname = $pname) =~ s!/!::!g;
	($cname = $pname) =~ s!/!__!g;
	print "EXTERN_C void boot_${cname} _((CV* cv));\n";
    }

    my ($tail1,$tail2) = ( $tail =~ /\A(.*\n)(\s*\}.*)\Z/s );
    print $tail1;

    print "\tchar *file = __FILE__;\n";
    print "\tdXSUB_SYS;\n" if $] > 5.002;

    foreach $_ (@exts){
	my($pname) = canon('/', $_);
	my($mname, $cname, $ccode);
	($mname = $pname) =~ s!/!::!g;
	($cname = $pname) =~ s!/!__!g;
	print "\t{\n";
	if ($pname eq $dl){
	    # Must NOT install 'DynaLoader::boot_DynaLoader' as 'bootstrap'!
	    # boot_DynaLoader is called directly in DynaLoader.pm
	    $ccode = "\t/* DynaLoader is a special case */\n
\tnewXS(\"${mname}::boot_${cname}\", boot_${cname}, file);\n";
	    print $ccode unless $SEEN{$ccode}++;
	} else {
	    $ccode = "\tnewXS(\"${mname}::bootstrap\", boot_${cname}, file);\n";
	    print $ccode unless $SEEN{$ccode}++;
	}
	print "\t}\n";
    }
    print $tail2;
}

sub canon{
    my($as, @ext) = @_;
	foreach(@ext){
	    # might be X::Y or lib/auto/X/Y/Y.a
		next if s!::!/!g;
	    s:^(lib|ext)/(auto/)?::;
	    s:/\w+\.\w+$::;
	}
	grep(s:/:$as:, @ext) if ($as ne '/');
	@ext;
}

1;
__END__

=head1 NAME

ExtUtils::Miniperl, writemain - write the C code for perlmain.c

=head1 SYNOPSIS

C<use ExtUtils::Miniperl;>

C<writemain(@directories);>

=head1 DESCRIPTION

This whole module is written when perl itself is built from a script
called minimod.PL. In case you want to patch it, please patch
minimod.PL in the perl distribution instead.

writemain() takes an argument list of directories containing archive
libraries that relate to perl modules and should be linked into a new
perl binary. It writes to STDOUT a corresponding perlmain.c file that
is a plain C file containing all the bootstrap code to make the
modules associated with the libraries available from within perl.

The typical usage is from within a Makefile generated by
ExtUtils::MakeMaker. So under normal circumstances you won't have to
deal with this module directly.

=head1 SEE ALSO

L<ExtUtils::MakeMaker>

=cut

