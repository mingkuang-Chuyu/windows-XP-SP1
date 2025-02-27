/*

Modifications:

    12.12.95	Joe Holman		Added GetLastError() to a FindFirstFile()
                                error path.

*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <time.h>
#include "general.h"

#define MAX_DISKS 40


//
// Macro for rounding up any number (x) to multiple of (n) which
// must be a power of 2.  For example, ROUNDUP( 2047, 512 ) would
// yield result of 2048.
//

#define ROUNDUP2( x, n ) (((x) + ((n) - 1 )) & ~((n) - 1 ))


FILE* logFile;
int cdProduct;


HANDLE hActivateCopyThread, hCopyThreadIsAvailable;
CHAR chCopyThreadSource[ MAX_PATH ], chCopyThreadDestin[ MAX_PATH ];
CHAR chPreviousSource[ MAX_PATH ], chPreviousDestin[ MAX_PATH ];

void MakeDbgName( LPCSTR pszSourceName, LPSTR pszTargetName );
void DoThreadedCopy( LPSTR pszSource, LPSTR pszDestin );
DWORD CopyThread( LPVOID lpvParam );

void    Msg ( const char * szFormat, ... ) {

    va_list vaArgs;

    va_start ( vaArgs, szFormat );
    vprintf  ( szFormat, vaArgs );
    vfprintf ( logFile, szFormat, vaArgs );
    va_end   ( vaArgs );
}

void Header(argv)
char* argv[];
{
    time_t t;

    Msg ("\n=========== MCPYFILE =============\n");
    Msg ("logfile     : %s\n", argv[1] );
    Msg ("Input Layout: %s\n",argv[2]);
    Msg ("category    : %s\n",argv[3]);
    Msg ("Compressed files: %s\n", argv[4]);
    Msg ("Uncompressed files  : %s\n", argv[5]);
    Msg ("Hard drive: %s\n",argv[6]);
    Msg ("Copy d-DBG, x-FLOPPY, c-CDROM files: %s\n", argv[7] );
    time(&t);
    Msg ("Time: %s",ctime(&t));
    Msg ("==================================\n\n");
}

void Usage()
{
    printf("PURPOSE: Copy files to hardrive or into disk1, disk2, ... dirs.\n");
    printf("\n");
    printf("PARAMETERS:\n");
    printf("\n");
    printf("[logFile] - Path to append a log of actions and errors.\n");
    printf("[InLayout] - Path of Layout which lists files to copy.\n");
    printf("[Category] - Specifies the category of files to copy.\n");
    printf("[files share] - location of Compressed files.\n" );
    printf("[files share] - location of Uncompressed files\n" );
    printf("[Harddrive] - Drive where files are stored.\n");
    printf("[copy dbg,floppy,or cd files] - use D for .dbg files or x for floppy files\n" );
}

int __cdecl DiskDirCompare(const void*,const void*);

int __cdecl main(argc,argv)
int argc;
char* argv[];
{
    Entry ee;
    char sourcePath[MAX_PATH];
    char destinPath[MAX_PATH];
    int disks[MAX_DISKS];
    Entry *e;
    char *buf;
    int records,i;
    BOOL shouldCopy;
    BOOL update;
    BOOL bCompressedFile;
    BOOL bCopyDbgFiles;
    HANDLE hSource,hDestin, hThread;
    DWORD actualSize,bomSize, dwThreadID;
    WIN32_FIND_DATA fdSource, fdDestin;

    if ( argc != 8 ) {
        Usage();
        return (1);
    }

    if ((logFile=fopen(argv[1],"a"))==NULL) {

        printf("ERROR Couldn't open log file %s.\n",argv[1]);
        return (1);
    }

    hActivateCopyThread    = CreateEvent( NULL, FALSE, FALSE, NULL );
    hCopyThreadIsAvailable = CreateEvent( NULL, FALSE, TRUE,  NULL );
    hThread = CreateThread( NULL, 0, CopyThread, NULL, 0, &dwThreadID );
    CloseHandle( hThread );

    Header(argv);

    //	Load all of the current entries in the layout file
    //	provided to the program.
    //
    LoadFile(argv[2],&buf,&e,&records,"ALL");

    cdProduct     = TRUE;
    bCopyDbgFiles = FALSE;
    update        = TRUE;
    if ( !_stricmp ( "ntflop", argv[3] ) || !_stricmp ( "lmflop", argv[3] ) ||
         _stricmp ( argv[7], "x" ) == 0 ) {

        cdProduct = FALSE;
        bCopyDbgFiles = FALSE;

        Msg ( "Making X86 Floppies...\n" );
    } else {
        Msg ( "Making CDs...\n" );
        bCopyDbgFiles = !_stricmp( argv[7], "d" );
    }

    Msg ( "bCopyDbgFiles = %d\n", bCopyDbgFiles );
    //qsort(e,records,sizeof(ee),DiskDirCompare);

    for (i=0;i<MAX_DISKS;i++) {
        disks[i]=0;
    }

    for (i=0;i<records;i++) {

        if (e[i].cdpath[strlen(e[i].cdpath)-1]=='\\') {

            e[i].cdpath[strlen(e[i].cdpath)-1]='\0';
        }
        if (e[i].path[strlen(e[i].path)-1]=='\\') {

            e[i].path[strlen(e[i].path)-1]='\0';
        }

        disks[e[i].disk]++;
    }

    for (i=0;i<records;i++) {

        if (!((records-i)%100)) {
            printf("# files remaining:%5d/%d\n",records-i,records);
        }

        ee=e[i];

        if (!_stricmp(ee.source,argv[3])) {    // if category matches

            if ( cdProduct ) {

                //	Making CD.
                //
                //
                //  It's a compressed file IFF
                //  the nocompress flag is NOT set (i.e. null) AND
                //  we're NOT copying dbg-files.
                //
                bCompressedFile = !ee.nocompress[0] && !bCopyDbgFiles;

            } else {

                //	Making x86 floppies.
                //
                //	NOTE:  in Layout.C, we go back to the convention of:
                //
                //			""  == yes, compress this file
                //			"x" == no don't compress this file
                //
                bCompressedFile = _stricmp(ee.nocompress, "x" );

                //Msg ( "%s, bCompressedFile = %d\n", ee.name, bCompressedFile );

            }


//Msg ( "bCompressedFile = %d, %s\n", bCompressedFile, ee.name );

            if ( bCompressedFile ) {
                strcpy( sourcePath, argv[ 4 ] );    // use compressed path
                bomSize = ee.csize;         // and compressed size
            } else {
                strcpy( sourcePath, argv[ 5 ] );    // uncompressed path
                bomSize = ee.size;          // uncompressed size
            }

            strcat(sourcePath,ee.path);
            strcat(sourcePath,"\\");

            if ( bCompressedFile ) {
                convertName( ee.name, strchr( sourcePath, 0 ));
            } else if ( bCopyDbgFiles ) {
                MakeDbgName( ee.name, strchr( sourcePath, 0 ));
            } else {
                strcat( sourcePath, ee.name );
            }


            //  May have to massage disk1...disk tagfiles...
            if ( cdProduct ) {
                strcpy(destinPath,argv[6]);
                if ( ! bCopyDbgFiles ) {
                    strcat(destinPath,ee.cdpath);
                }
            } else {
                strcpy(destinPath,argv[6]);
                sprintf(&destinPath[strlen(destinPath)],"\\disk%d",ee.disk);
            }

            strcat(destinPath,"\\");

            if ( bCopyDbgFiles ) {
                MakeDbgName( ee.name, strchr( destinPath, 0 ));
            } else {
                if (ee.medianame[0]) {
                    if ( bCompressedFile ) {
                        convertName( ee.medianame, strchr( destinPath, 0 ));

                        //	For simplification in the BOM, we no longer
                        //	rename compressed files. I.E, any file that has
                        //	to be renamed, CANNOT be compressed.
                        //
                        Msg ( "ERROR: renaming compressed file not supported:  %s\n",
                              destinPath );
                    } else {
                        strcat( destinPath, ee.medianame );
                    }
                } else {
                    if ( bCompressedFile ) {
                        convertName( ee.name, strchr( destinPath, 0 ));
                    } else {
                        strcat( destinPath, ee.name );
                    }
                }
            }

            if (disks[ee.disk]>1) {

                //
                //  Don't attempt to copy same file twice (target file might
                //  not yet completely exist since threaded copy might not be
                //  complete, so can't rely on timestamp equivalence yet).
                //
                if ( _stricmp( sourcePath, chPreviousSource ) ||
                     _stricmp( destinPath, chPreviousDestin )) {

                    hSource=FindFirstFile( sourcePath, &fdSource );

                    if (hSource==INVALID_HANDLE_VALUE) {
                        Msg ("ERROR Source: %s, gle()=%d\n",sourcePath, GetLastError());
                    } else {

                        FindClose( hSource );

                        if ( !cdProduct ) {
                            actualSize = ROUNDUP2( fdSource.nFileSizeLow,
                                                   DMF_ALLOCATION_UNIT );
                        } else {
                            actualSize = ROUNDUP2( fdSource.nFileSizeLow,
                                                   ALLOCATION_UNIT );
                        }

                        //  Check the size of the file vs. the size in the
                        //  bom just for a verification of file sizes.
                        //  Don't do this for Dbg files, since these sizes are
                        //  never put in the layout.
                        //
                        if ( !bCopyDbgFiles && (bomSize < actualSize) ) {
                            Msg ( "ERROR:  disk#%d, %s Size of file: %d > BOM: %d Diff: %d\n",
                                  ee.disk,ee.name,
                                  actualSize,bomSize,actualSize-bomSize);
                        }

                        shouldCopy=TRUE;

                        if (update) {

                            hDestin=FindFirstFile( destinPath, &fdDestin );

                            if (hDestin==INVALID_HANDLE_VALUE) {
                                //  Msg ("New file %s\n", destinPath);
                            } else {
                                FindClose( hDestin );

                                if ( CompareFileTime( &fdSource.ftLastWriteTime,
                                                      &fdDestin.ftLastWriteTime ) <= 0 ) {
                                    shouldCopy=FALSE;
                                } else {
                                    //Msg ("Updating %s\n",destinPath);
                                }

                            }
                        }

                        if (shouldCopy) {
                            Msg ( "Copy:  %s >>> %s\n", sourcePath, destinPath);
                            DoThreadedCopy( sourcePath, destinPath );
                            strcpy( chPreviousSource, sourcePath );
                            strcpy( chPreviousDestin, destinPath );
                        }
                    }
                }
            } else {
                Msg ("WARNING Skipped Disk %d, File: %s\n",ee.disk,ee.name);
            }
        }
    }
    fclose(logFile);
    WaitForSingleObject( hCopyThreadIsAvailable, INFINITE );

    return 0;
}


int __cdecl DiskDirCompare(const void *v1, const void *v2)
{
    Entry *e1 = (Entry *)v1;
    Entry *e2 = (Entry *)v2;

    //
    // If the files are not on the same disk,
    // the comparison is easy.
    //
    if (e1->disk != e2->disk) {
        return (e1->disk - e2->disk);
    }

    //
    // If this is a cd-rom, sort by location on the cd.
    //
    if (cdProduct) {
        return (_stricmp(e1->cdpath,e2->cdpath));
    }

    //
    // Floppy product: we know the files are on the same disk
    // and files on the floppy are all in the same directory.
    //
    return (0);
}


void DoThreadedCopy( LPSTR pszSource, LPSTR pszDestin ) {
    WaitForSingleObject( hCopyThreadIsAvailable, INFINITE );
    strcpy( chCopyThreadSource, pszSource );
    strcpy( chCopyThreadDestin, pszDestin );
    SetEvent( hActivateCopyThread );
}

#if _MSC_FULL_VER >= 13008827
#pragma warning(push)
#pragma warning(disable:4715)			// Not all control paths return (due to infinite loop)
#endif

DWORD CopyThread( LPVOID lpvParam ) {

    BOOL bSuccess;
    UINT i, len;

    for (;;) {

        WaitForSingleObject( hActivateCopyThread, INFINITE );

        bSuccess = CopyFile( chCopyThreadSource, chCopyThreadDestin, FALSE );

        if ( ! bSuccess ) {

            SetFileAttributes( chCopyThreadDestin, FILE_ATTRIBUTE_NORMAL );

            len = strlen( chCopyThreadDestin );
            for ( i = 2; i < len; i++ ) {
                if ( chCopyThreadDestin[ i ] == '\\' ) {
                    chCopyThreadDestin[ i ] = '\0';
                    CreateDirectory( chCopyThreadDestin, NULL );
                    chCopyThreadDestin[ i ] = '\\';
                }
            }

            bSuccess = CopyFile( chCopyThreadSource, chCopyThreadDestin, FALSE );

        }

        if ( ! bSuccess ) {
            Msg (   "ERROR Source: %s\n"
                    "      Destin: %s\n"
                    "      GLE=%d\n",
                    chCopyThreadSource,
                    chCopyThreadDestin,
                    GetLastError() );
        }

        SetEvent( hCopyThreadIsAvailable );
    }

    return 0;
}

#if _MSC_FULL_VER >= 13008827
#pragma warning(pop)
#endif


void MakeDbgName( LPCSTR pszSourceName, LPSTR pszTargetName ) {

    //
    //  Converts "filename.ext" into "ext\filename.dbg".
    //

    const char *p = strchr( pszSourceName, '.' );

    if ( p != NULL ) {
        strcpy( pszTargetName, p + 1 );                 // old extension
        strcat( pszTargetName, "\\" );                  // path separator
        strcat( pszTargetName, pszSourceName );         // base name
        strcpy( strchr( pszTargetName, '.' ), ".dbg" ); // new extension
    } else {
        strcpy( pszTargetName, pszSourceName );
    }
}
