/*
 *
 *  FSTimestampCopy.cpp
 *
 *  Create 2020.06.09
 *
 */
#include <ntifs.h>
#include <stdio.h>
#include <malloc.h>
#include <strsafe.h>
#include <locale.h>
#include <conio.h>
#include <winerror.h>
#include "ntnativeapi.h"

#define OPT_PROMPT_SUPPRESS        0x80000000
#define OPT_COPY_CREATIONTIME      0x00000001
#define OPT_COPY_LASTWRITETIME     0x00000002
#define OPT_COPY_LASTACCESSTIME    0x00000004
#define OPT_COPY_CHANGETIME        0x00000008
#define OPT_COPY_ALL               (OPT_COPY_CREATIONTIME|OPT_COPY_LASTWRITETIME|\
                                    OPT_COPY_LASTACCESSTIME|OPT_COPY_CHANGETIME)

//////////////////////////////////////////////////////////////////////////////
// Helper Functions

BOOLEAN IsNtDevicePath(PCWSTR pszPath)
{
    // e.g.
    // "\Device\HarddiskVolume1\Windows"
    // "\??\C:\Windows" 
    //
#if 0
    return (wcsnicmp(pszPath,L"\\Device\\",8) == 0) ||  
           (wcsnicmp(pszPath,L"\\??\\",4) == 0);
#else
    UNICODE_STRING String;
    UNICODE_STRING Prefix;
    RtlInitUnicodeString(&String,pszPath);
    RtlInitUnicodeString(&Prefix,L"\\Device\\");
    if( RtlPrefixUnicodeString(&Prefix,&String,TRUE) )
        return TRUE;
    RtlInitUnicodeString(&Prefix,L"\\??\\");
    return RtlPrefixUnicodeString(&Prefix,&String,TRUE);
#endif
}

BOOLEAN DoesFileExists_NtPath(UNICODE_STRING *pusNtPath)
{
    OBJECT_ATTRIBUTES oa = {0};
    FILE_NETWORK_OPEN_INFORMATION fi = {0};

    InitializeObjectAttributes(&oa,pusNtPath,0,0,0);

    if( NtQueryFullAttributesFile(&oa,&fi) == STATUS_SUCCESS )
    {
        return TRUE;
    }

    return FALSE;
}

BOOLEAN IsRelativePath(PCWSTR pszDosPath)
{
    RTL_PATH_TYPE Type = RtlDetermineDosPathNameType_U( pszDosPath );
    return (Type == RtlPathTypeRelative)||(Type == RtlPathTypeDriveRelative);
}

NTSTATUS AllocateUnicodeString(UNICODE_STRING *pus,PCWSTR psz)
{
    UNICODE_STRING us;
    RtlInitUnicodeString(&us,psz);
    return RtlDuplicateUnicodeString(
        RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE|RTL_DUPLICATE_UNICODE_STRING_ALLOCATE_NULL_STRING,
        &us,pus);
}

//////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------
//
//  displayError()
//
//----------------------------------------------------------------------------
void displayError(CHAR *msg,ULONG code)
{
    printf("ERROR: %s 0x%08X\n",msg,code);
}

//----------------------------------------------------------------------------
//
//  openFile()
//
//----------------------------------------------------------------------------
HANDLE openFile(UNICODE_STRING& FilePath,BOOLEAN bRead)
{
    HANDLE hFile = NULL;

    NTSTATUS Status = 0;
    OBJECT_ATTRIBUTES ObjectAttributes;
    IO_STATUS_BLOCK IoStatus = {0};

    InitializeObjectAttributes(&ObjectAttributes,&FilePath,0,NULL,NULL);

    Status = NtOpenFile(&hFile,
                    (bRead ? FILE_READ_ATTRIBUTES : FILE_WRITE_ATTRIBUTES)|SYNCHRONIZE,
                    &ObjectAttributes,&IoStatus,
                    FILE_SHARE_READ,
                    FILE_OPEN_FOR_BACKUP_INTENT|FILE_SYNCHRONOUS_IO_NONALERT
//					|FILE_OPEN_REPARSE_POINT // bypass reparse point processing for the file. 
                    );

    if( Status != STATUS_SUCCESS )
    {
        RtlSetLastWin32Error(Status);
        return NULL;
    }

    return hFile;
}

//----------------------------------------------------------------------------
//
//  copyTimeStamp()
//
//----------------------------------------------------------------------------
BOOLEAN copyTimeStamp(UNICODE_STRING& PathDst,UNICODE_STRING& PathSrc,ULONG OptionFlag)
{
    HANDLE hSrcFile = NULL;
    HANDLE hDstFile = NULL;
    BOOLEAN Result = FALSE;

    __try
    {
        // open source path
        hSrcFile = openFile(PathSrc,TRUE);
        if( hSrcFile == NULL )
        {
            displayError("Source File Open",RtlGetLastWin32Error());
            __leave;
        }

        hDstFile = openFile(PathDst,FALSE);
        if( hDstFile == NULL )
        {
            displayError("Destination File Open",RtlGetLastWin32Error());
            __leave;
        }

        FILE_BASIC_INFORMATION fbi;
        IO_STATUS_BLOCK IoStatus;
        NTSTATUS Status;

        Status = NtQueryInformationFile(hSrcFile,&IoStatus,&fbi,sizeof(fbi),FileBasicInformation);

        if( Status != STATUS_SUCCESS )
        {
            displayError("Read time stamp",Status);
            __leave;
        }

        if( (OptionFlag & OPT_COPY_CREATIONTIME) == 0 )
            fbi.CreationTime.QuadPart = -1;

        if( (OptionFlag & OPT_COPY_LASTWRITETIME) == 0 )
            fbi.LastWriteTime.QuadPart = -1;

        if( (OptionFlag & OPT_COPY_LASTACCESSTIME) == 0 )
            fbi.LastAccessTime.QuadPart = -1;

        if( (OptionFlag & OPT_COPY_CHANGETIME) == 0 )
            fbi.ChangeTime.QuadPart = -1;

        fbi.FileAttributes = (ULONG)0; // no change attribues

        Status = NtSetInformationFile(hDstFile,&IoStatus,&fbi,sizeof(fbi),FileBasicInformation);

        if( Status != STATUS_SUCCESS )
        {
            displayError("Set time stamp",Status);
            __leave;
        }

        Result = TRUE;
    }
    __finally
    {
        if( hSrcFile != NULL )
            NtClose(hSrcFile);
        if( hDstFile != NULL )
            NtClose(hDstFile);
    };

    return Result;
}

//----------------------------------------------------------------------------
//
//  GetExecutionOption()
//
//----------------------------------------------------------------------------
BOOLEAN GetExecutionOption(int argc, WCHAR *argv[],PWSTR *ppSrcPath,PWSTR *ppDstPath,PULONG OptionFlags)
{
    PWSTR SrcPath = NULL;
    PWSTR DstPath = NULL;
    int i;

    ULONG DefaultCopyTimeFlags = *OptionFlags;

    *OptionFlags &= ~OPT_COPY_ALL;

    for(i = 1; i < argc; i++)
    {
        if( argv[i][0] == L'/' || argv[i][0] == L'-' )
        {
            // option switch
            if( wcslen( argv[i] ) >= 2 )
            {
                switch( argv[i][1] )
                {
                    case L'y':
                    case L'Y':
                        *OptionFlags |= OPT_PROMPT_SUPPRESS;
                        break;
                    case L'c':
                    case L'C':
                    case L'w':
                    case L'W':
                    case L'a':
                    case L'A':
                    case L'h':
                    case L'H':
                    {
                        if( ((*OptionFlags) & OPT_COPY_ALL) != 0 )
                            return FALSE;

                        (*OptionFlags) &= ~OPT_COPY_ALL;

                        WCHAR *p = &argv[i][1];

                        while( *p )
                        {
                            if( *p == L'c'|| *p == L'C' )
                                (*OptionFlags) |= OPT_COPY_CREATIONTIME;
                            else if( *p == L'w'|| *p == L'W' )
                                (*OptionFlags) |= OPT_COPY_LASTWRITETIME;
                            else if( *p == L'a'|| *p == L'A' )
                                (*OptionFlags) |= OPT_COPY_LASTACCESSTIME;
                            else if( *p == L'h'|| *p == L'H' )
                                (*OptionFlags) |= OPT_COPY_CHANGETIME;
                            else
                                return FALSE; // error char
                            p++;
                        }
                        break;
                    }
                }
            }
        }
        else
        {
            if( SrcPath == NULL )
                SrcPath = argv[i];
            else if( DstPath == NULL )
                DstPath = argv[i];
        }
    }

    if( SrcPath == NULL || DstPath == NULL )
        return FALSE;

    if( ((*OptionFlags) & OPT_COPY_ALL) == 0 )
        *OptionFlags |= (DefaultCopyTimeFlags & OPT_COPY_ALL);

    *ppSrcPath = SrcPath;
    *ppDstPath = DstPath;

    return TRUE;
}

//----------------------------------------------------------------------------
//
//  makePath()
//
//----------------------------------------------------------------------------
BOOLEAN makePath(PCWSTR pszFilename,UNICODE_STRING& FullPath)
{
    FullPath.Buffer = NULL;
    FullPath.Length = FullPath.MaximumLength = 0;

    if( IsNtDevicePath(pszFilename) )
    {
        //
        // NT device name space path
        // In this case specifies absolute path only.
        //
        if( AllocateUnicodeString(&FullPath,pszFilename) != STATUS_SUCCESS )
        {
            return FALSE;
        }
    }
    else
    {
        //
        // MS-DOS drive path
        //
        if( IsRelativePath(pszFilename) )
        {
            // If path string is a relative format, to determine as a DOS path.
            //
            PWSTR FilePart = NULL;
            if( !RtlDosPathNameToRelativeNtPathName_U(pszFilename,&FullPath,NULL,NULL) )
            {
                return FALSE;
            }
        }
        else
        {
            if( !RtlDosPathNameToNtPathName_U(pszFilename,&FullPath,NULL,NULL) )
            {
                return FALSE;
            }
        }
    }

    return (FullPath.Buffer != NULL);
}

//----------------------------------------------------------------------------
//
//  wmain()
//
//----------------------------------------------------------------------------
int __cdecl wmain(int argc, WCHAR* argv[])
{
    setlocale(LC_ALL,"");

    if( argc < 3 )
    {
        printf("Usage: fstscp [-[attibutes]][-y] source_path destination_path\n\n"
               "  attributes  Time stamp attribute to copy.\n"
               "                c : creation datetime\n"
               "                w : last write datetime\n"
               "                a : last access datetime\n"
               "                h : change datetime\n"
               "              Default switch is '-cwah'.\n"
               "  -y          Suppresses prompting to confirm does copy.\n"
                );
        return 0;
    }

    UNICODE_STRING PathSrc;
    UNICODE_STRING PathDst;
    PWSTR pszSrcFile;
    PWSTR pszDstFile;

    ULONG OptionFlag = OPT_COPY_ALL;
    if( GetExecutionOption(argc,argv,&pszSrcFile,&pszDstFile,&OptionFlag) == FALSE )
    {
        printf("Invalid parameter.\n\n");
        return 0;
    }

    makePath(pszSrcFile,PathSrc);
    makePath(pszDstFile,PathDst);

    if( PathSrc.Buffer == NULL || !DoesFileExists_NtPath(&PathSrc) )
    {
        displayError("Source File Not Exist",0);
    }
    else if( PathDst.Buffer == NULL || !DoesFileExists_NtPath(&PathDst) )
    {
        displayError("Destination File Not Exist",0);
    }
    else
    {
        int ch;
        if( OptionFlag & OPT_PROMPT_SUPPRESS )
        {
            ch = 'y';
        }
        else
        {
            printf("copy timestamp ok? (Y|y)");
            ch = _getch();
            printf("\n");
        }

        if( ch == 'y' || ch == 'Y' )
        {
            if( copyTimeStamp(PathDst,PathSrc,OptionFlag) )
            {
                printf("copy succeeded.\n");
            }
        }
    }

    RtlFreeUnicodeString(&PathSrc);
    RtlFreeUnicodeString(&PathDst);

    return 0;
}
