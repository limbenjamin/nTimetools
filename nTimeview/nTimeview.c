// #######################################################################
// ############ HEADER FILES
// #######################################################################
#include <windows.h>
#include <stdio.h>
#include <winternl.h>
#include <inttypes.h>
#include <math.h>

typedef LONG NTSTATUS;
char* VERSION_NO = "1.0";

typedef struct _FILE_BASIC_INFORMATION {
	LARGE_INTEGER CreationTime;							// Created             
	LARGE_INTEGER LastAccessTime;                       // Accessed    
	LARGE_INTEGER LastWriteTime;                        // Modifed
	LARGE_INTEGER ChangeTime;                           // Entry Modified
	ULONG FileAttributes;
} FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;



typedef NTSTATUS(WINAPI *pNtQueryInformationFile)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);

HANDLE RetrieveFileBasicInformation(char *filename, FILE_BASIC_INFORMATION *fbi);
DWORD ConvertLargeIntegerToLocalTime(SYSTEMTIME *localsystemtime, LARGE_INTEGER largeinteger);
VOID About();
VOID Usage();

// #######################################################################
// ############ FUNCTIONS
// #######################################################################

VOID About() {
	printf("nTimeview, Version %s\r\n", VERSION_NO);
	printf("Copyright (C) 2019 Benjamin Lim\r\n");
	printf("Available for free from https://limbenjamin.com/pages/ntimetools\r\n");
	printf("\r\n");
}

VOID Usage() {
	printf("\r\n");
	printf("Usage: .\\nTimeview.exe [Filename]\r\n");
	printf("\r\n");
}

/* returns the handle on success or NULL on failure. this function opens a file and returns
the FILE_BASIC_INFORMATION on it. */
HANDLE RetrieveFileBasicInformation(char *filename, FILE_BASIC_INFORMATION *fbi) {
	
	HANDLE file = NULL;
	HMODULE ntdll = NULL;
	pNtQueryInformationFile NtQueryInformationFile = NULL;
	IO_STATUS_BLOCK iostatus;

	file = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		printf("Cannot open file: %S\r\n", filename);
		Usage();
		exit(1);
	}

	/* load ntdll and retrieve function pointer */
	ntdll = GetModuleHandle(TEXT("ntdll.dll"));
	if (ntdll == NULL) {
		printf("Cannot load ntdll\r\n");
		CloseHandle(file);
		exit(1);
	}

	/* retrieve current timestamps including file attributes which we want to preserve */
	NtQueryInformationFile = (pNtQueryInformationFile)GetProcAddress(ntdll, "NtQueryInformationFile");
	if (NtQueryInformationFile == NULL) {
		CloseHandle(file);
		exit(1);
	}
	
	/* obtain the current file information including attributes */
	if (NtQueryInformationFile(file, &iostatus, fbi, sizeof(FILE_BASIC_INFORMATION), 4) < 0) {
		CloseHandle(file);
		exit(1);
	}

	/* clean up */
	FreeLibrary(ntdll);

	return file;
}

/* returns 0 on error, 1 on success. this function converts a LARGE_INTEGER to a SYSTEMTIME structure */
DWORD ConvertLargeIntegerToLocalTime(SYSTEMTIME *localsystemtime, LARGE_INTEGER largeinteger) {

	FILETIME filetime;
	FILETIME localfiletime;
	DWORD result = 0;

	filetime.dwLowDateTime = largeinteger.LowPart;
	filetime.dwHighDateTime = largeinteger.HighPart;

	if (FileTimeToSystemTime(&filetime, localsystemtime) == 0) {
		printf("Invalid filetime\r\n");
		exit(1);
	}

	return 1;
}

int main(int argc, char* argv[]) {
	HANDLE file = NULL;
	struct _FILE_BASIC_INFORMATION fbi = { 0 };
	struct _SYSTEMTIME time = { 0 };
	wchar_t filename[4096] = { 0 };
	char str[256];
	CHAR lpVolumeNameBuffer[MAX_PATH + 1] = { 0 };
	CHAR lpFileSystemNameBuffer[MAX_PATH + 1] = { 0 };
	LARGE_INTEGER lpFileSizeHigh = { 0 };

	About();
	MultiByteToWideChar(CP_ACP, 0, argv[1], -1, filename, 4096);
	file = RetrieveFileBasicInformation(filename, &fbi);
	GetVolumeInformationByHandleW(file, &lpVolumeNameBuffer, ARRAYSIZE(lpVolumeNameBuffer), 0, 0, 0, &lpFileSystemNameBuffer, ARRAYSIZE(lpVolumeNameBuffer));
	GetFileSizeEx(file, &lpFileSizeHigh);

	printf("Filesystem type:		%S\r\n", lpFileSystemNameBuffer);
	printf("Filename:			%S\r\n", filename);
	printf("File size:			%d\r\n", lpFileSizeHigh.QuadPart);
	printf("\r\n");

	ConvertLargeIntegerToLocalTime(&time, fbi.LastWriteTime);
	if (fbi.LastWriteTime.QuadPart != 0) {
		sprintf_s(str, 256, "%lld", fbi.LastWriteTime.QuadPart);
		printf("[M] Last Write Time:		%04d-%02d-%02d %02d:%02d:%02d.%7s UTC\r\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, &str[11]);
	}else {
		printf("[M] Last Write Time:		%04d-%02d-%02d %02d:%02d:%02d.0000000 UTC\r\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	}
	memset(&time, 0, sizeof(time));

	ConvertLargeIntegerToLocalTime(&time, fbi.LastAccessTime);
	if (fbi.LastAccessTime.QuadPart != 0) {
		sprintf_s(str, 256, "%lld", fbi.LastAccessTime.QuadPart);
		printf("[A] Last Access Time:		%04d-%02d-%02d %02d:%02d:%02d.%7s UTC\r\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, &str[11]);
	} else {
		printf("[A] Last Access Time:		%04d-%02d-%02d %02d:%02d:%02d.0000000 UTC\r\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	}
	memset(&time, 0, sizeof(time));

	ConvertLargeIntegerToLocalTime(&time, fbi.ChangeTime);
	if (fbi.ChangeTime.QuadPart != 0) {
		sprintf_s(str, 256, "%lld", fbi.ChangeTime.QuadPart);
		printf("[C] Metadata Change Time:	%04d-%02d-%02d %02d:%02d:%02d.%7s UTC\r\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, &str[11]);
	} else {
		printf("[C] Metadata Change Time:	%04d-%02d-%02d %02d:%02d:%02d.0000000 UTC\r\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	}
	memset(&time, 0, sizeof(time));

	ConvertLargeIntegerToLocalTime(&time, fbi.CreationTime);
	if (fbi.CreationTime.QuadPart != 0) {
		sprintf_s(str, 256, "%lld", fbi.CreationTime.QuadPart);
		printf("[B] Creation Time:		%04d-%02d-%02d %02d:%02d:%02d.%7s UTC\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, &str[11]);
	} else {
		printf("[B] Creation Time:		%04d-%02d-%02d %02d:%02d:%02d.0000000 UTC\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	}
	printf("\r\n");
	CloseHandle(file);
}