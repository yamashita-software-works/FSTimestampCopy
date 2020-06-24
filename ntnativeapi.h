#pragma once

EXTERN_C
ULONG
NTAPI
RtlGetProcessHeaps(
    ULONG MaxNumberOfHeaps,
    PVOID *HeapArray
    );

EXTERN_C
NTSTATUS
NTAPI
RtlUnicodeStringCat(
    IN OUT PUNICODE_STRING  DestinationString,
    IN PCUNICODE_STRING  SourceString
    );

typedef struct _CURDIR
{
    UNICODE_STRING DosPath;
    HANDLE Handle;
} CURDIR;

EXTERN_C
VOID
NTAPI
RtlSetLastWin32Error(
    ULONG ErrorCode
    );

EXTERN_C
ULONG
NTAPI
RtlGetLastWin32Error(
    VOID
    );

EXTERN_C
NTSYSAPI
ULONG
NTAPI
RtlGetCurrentDirectory_U(
    IN ULONG nBufferLength,
    OUT PWSTR lpBuffer
    );

EXTERN_C
BOOLEAN
NTAPI
RtlDosPathNameToNtPathName_U(
    IN PCWSTR DosPathName,
    OUT PUNICODE_STRING NtPathName,
    OUT PCWSTR *NtFileNamePart,
    OUT CURDIR *DirectoryInfo
    );

EXTERN_C
ULONG
NTAPI
RtlGetFullPathName_U(
    PCWSTR lpFileName,
    ULONG nBufferLength,
    PWSTR lpBuffer,
    PWSTR *lpFilePart
    );

EXTERN_C
BOOLEAN
NTAPI
RtlDoesFileExists_U(
    PCWSTR FileName
    );

EXTERN_C
NTSTATUS
NTAPI
RtlGetLengthWithoutLastFullDosOrNtPathElement(
    IN  ULONG            Flags,
    IN  PCUNICODE_STRING Path,
    OUT ULONG*           LengthOut
    );

EXTERN_C
NTSTATUS
NTAPI
RtlGetLengthWithoutTrailingPathSeperators(
    IN  ULONG            Flags,
    IN  PCUNICODE_STRING Path,
    OUT ULONG*           LengthOut
    );

typedef enum _RTL_PATH_TYPE {
    RtlPathTypeUnknown,         // 0
    RtlPathTypeUncAbsolute,     // 1
    RtlPathTypeDriveAbsolute,   // 2
    RtlPathTypeDriveRelative,   // 3
    RtlPathTypeRooted,          // 4
    RtlPathTypeRelative,        // 5
    RtlPathTypeLocalDevice,     // 6
    RtlPathTypeRootLocalDevice  // 7
} RTL_PATH_TYPE;

EXTERN_C
NTSYSAPI
RTL_PATH_TYPE
NTAPI
RtlDetermineDosPathNameType_U(
    PCWSTR DosFileName
    );

EXTERN_C
NTSTATUS
NTAPI
NtQueryFullAttributesFile(
    IN POBJECT_ATTRIBUTES  ObjectAttributes,
    OUT PFILE_NETWORK_OPEN_INFORMATION  FileInformation
    );

typedef struct _RTLP_CURDIR_REF *PRTLP_CURDIR_REF;

typedef struct _RTL_RELATIVE_NAME_U {
    UNICODE_STRING RelativeName;
    HANDLE ContainingDirectory;
    PRTLP_CURDIR_REF CurDirRef;
} RTL_RELATIVE_NAME_U, *PRTL_RELATIVE_NAME_U;

EXTERN_C
NTSYSAPI
BOOLEAN
NTAPI
RtlDosPathNameToRelativeNtPathName_U(
    PCWSTR DosFileName,
    PUNICODE_STRING NtFileName,
    PWSTR *FilePart,
    PRTL_RELATIVE_NAME_U RelativeName
    );

EXTERN_C
VOID
NTAPI
RtlReleaseRelativeName(
    PRTL_RELATIVE_NAME_U RelativeName
    );

#if 1 // copy from winnt.h
#define ANSI_NULL ((CHAR)0)     
#define UNICODE_NULL ((WCHAR)0) 
#define UNICODE_STRING_MAX_BYTES ((USHORT) 65534) 
#define UNICODE_STRING_MAX_CHARS (32767) 
#endif

#define PATH_BUFFER_BYTES   (UNICODE_STRING_MAX_BYTES + sizeof(WCHAR))
#define PATH_BUFFER_LENGTH  (UNICODE_STRING_MAX_CHARS)

#define IS_RELATIVE_DIR_NAME_WITH_UNICODE_SIZE(path,size) \
            ((path[0] == L'.' && size == sizeof(WCHAR)) || \
            (path[0] == L'.' && path[1] == L'.' && (size == (sizeof(WCHAR)*2))))

