// #######################################################################
// ############ HEADER FILES
// #######################################################################
#include <windows.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>

typedef LONG NTSTATUS;
char* VERSION_NO = "1.1";
HANDLE file = NULL;


typedef struct _IO_STATUS_BLOCK {
	union {
		NTSTATUS Status;
		PVOID Pointer;
	};
	ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef enum _FILE_INFORMATION_CLASS {
	FileBasicInformation = 4,
	FileStandardInformation = 5,
	FilePositionInformation = 14,
	FileEndOfFileInformation = 20,
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

typedef struct _FILE_BASIC_INFORMATION {
	LARGE_INTEGER CreationTime;							// Created             
	LARGE_INTEGER LastAccessTime;                       // Accessed    
	LARGE_INTEGER LastWriteTime;                        // Modifed
	LARGE_INTEGER ChangeTime;                           // Entry Modified
	ULONG FileAttributes;
} FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;

typedef NTSTATUS(WINAPI *pNtSetInformationFile)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);
typedef NTSTATUS(WINAPI *pNtQueryInformationFile)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);

HANDLE LoadFile(char *filename, FILE_BASIC_INFORMATION *fbi);
VOID RetrieveFileBasicInformation(char *filename, FILE_BASIC_INFORMATION *fbi);
DWORD SetFileMACE(HANDLE file, DWORD fileAttributes, char *mtimestamp, char *atimestamp, char *ctimestamp, char *btimestamp);
LARGE_INTEGER ParseDateTimeInput(char *inputstring);
VOID About();
VOID Usage();

// #######################################################################
// ############ FUNCTIONS
// #######################################################################

VOID About() {
	printf("nTimestomp, Version %s\r\n", VERSION_NO);
	printf("Copyright (C) 2019 Benjamin Lim\r\n");
	printf("Available for free from https://limbenjamin.com/pages/ntimetools\r\n");
	printf("\r\n");
}

VOID Usage() {
	printf("\r\n");
	printf("Usage: .\\nTimestomp.exe [Modified Date] [Last Access Date] [Last Write Date] [Creation Date]\r\n");
	printf("Date Format: yyyy-mm-dd hh:mm:ss.ddddddd\r\n");
	printf("\r\n");
}

HANDLE LoadFile(char *filename, FILE_BASIC_INFORMATION *fbi) {

	HANDLE file = NULL;
	HMODULE ntdll = NULL;

	file = CreateFile(filename, GENERIC_READ | GENERIC_WRITE | FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, 0, NULL);
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
	FreeLibrary(ntdll);

	return file;
}

/* returns the handle on success or NULL on failure. this function opens a file and returns
the FILE_BASIC_INFORMATION on it. */
VOID RetrieveFileBasicInformation(HANDLE file, FILE_BASIC_INFORMATION *fbi) {

	HMODULE ntdll = NULL;
	pNtQueryInformationFile NtQueryInformationFile = NULL;
	IO_STATUS_BLOCK iostatus;

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

}

DWORD SetFileMACE(HANDLE file, DWORD fileAttributes, char *mtimestamp, char *atimestamp, char *ctimestamp, char *btimestamp) {

	HMODULE ntdll = NULL;
	pNtSetInformationFile NtSetInformationFile = NULL;
	IO_STATUS_BLOCK iostatus;

	FILE_BASIC_INFORMATION fbi;
	fbi.LastWriteTime = ParseDateTimeInput(mtimestamp);
	fbi.LastAccessTime = ParseDateTimeInput(atimestamp);
	fbi.ChangeTime = ParseDateTimeInput(ctimestamp);
	fbi.CreationTime = ParseDateTimeInput(btimestamp);
	
	fbi.FileAttributes = fileAttributes;

	/* load ntdll and retrieve function pointer */
	ntdll = GetModuleHandle(TEXT("ntdll.dll"));
	if (ntdll == NULL) {
		printf("Cannot load ntdll\r\n");
		CloseHandle(file);
		exit(1);
	}

	NtSetInformationFile = (pNtSetInformationFile)GetProcAddress(ntdll, "NtSetInformationFile");
	if (NtSetInformationFile == NULL) {
		CloseHandle(file);
		exit(1);
	}

	if (NtSetInformationFile(file, &iostatus, &fbi, sizeof(FILE_BASIC_INFORMATION), FileBasicInformation) < 0) {
		CloseHandle(file);
		exit(1);
	}

	/* clean up */
	printf("File timestamp successfully set\r\n");
	FreeLibrary(ntdll);

	return 0;
}

LARGE_INTEGER ParseDateTimeInput(char *inputstring) {

	SYSTEMTIME systemtime = { 0 };
	LARGE_INTEGER nanoTime = { 0 };
	FILETIME filetime;
	LARGE_INTEGER dec = { 0 };
	LARGE_INTEGER res = { 0 };

	if (sscanf_s(inputstring, "%hu-%hu-%hu %hu:%hu:%hu.%7d", &systemtime.wYear, &systemtime.wMonth, &systemtime.wDay, &systemtime.wHour, &systemtime.wMinute, &systemtime.wSecond, &dec.QuadPart) == 0) {
		printf("Wrong Date Format");
		CloseHandle(file);
		exit(1);
	}

	/* sanitize input */

	if (systemtime.wMonth < 1 || systemtime.wMonth > 12) {
		printf("Wrong Date Format");
		CloseHandle(file);
		exit(1);
	}
	if (systemtime.wDay < 1 || systemtime.wDay > 31) {
		printf("Wrong Date Format");
		CloseHandle(file);
		exit(1);
	}
	if (systemtime.wYear < 1601 || systemtime.wYear > 30827) {
		printf("Wrong Date Format");
		CloseHandle(file);
		exit(1);
	}

	if (systemtime.wMinute < 0 || systemtime.wMinute > 59) {
		printf("Wrong Date Format");
		CloseHandle(file);
		exit(1);
	}
	if (systemtime.wSecond < 0 || systemtime.wSecond > 59) {
		printf("Wrong Date Format");
		CloseHandle(file);
		exit(1);
	}

	systemtime.wMilliseconds = 0;
	if (SystemTimeToFileTime(&systemtime, &filetime) == 0) {
		printf("Invalid filetime\r\n");
		CloseHandle(file);
		exit(1);
	}

	nanoTime.LowPart = filetime.dwLowDateTime;
	nanoTime.HighPart = filetime.dwHighDateTime;

	res.QuadPart = nanoTime.QuadPart + dec.QuadPart;

	return res;
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

	if (argc < 5) {
		Usage();
		exit(1);
	}

	FILE_BASIC_INFORMATION fbi; 
	struct _SYSTEMTIME time = { 0 };
	wchar_t filename[4096] = { 0 };
	char str[256];
	CHAR lpVolumeNameBuffer[MAX_PATH + 1] = { 0 };
	CHAR lpFileSystemNameBuffer[MAX_PATH + 1] = { 0 };
	LARGE_INTEGER lpFileSizeHigh = { 0 };

	About();
	MultiByteToWideChar(CP_ACP, 0, argv[1], -1, filename, 4096);
	file = LoadFile(filename, &fbi);
	GetVolumeInformationByHandleW(file, &lpVolumeNameBuffer, ARRAYSIZE(lpVolumeNameBuffer), 0, 0, 0, &lpFileSystemNameBuffer, ARRAYSIZE(lpVolumeNameBuffer));
	GetFileSizeEx(file, &lpFileSizeHigh);

	printf("Filesystem type:		%S\r\n", lpFileSystemNameBuffer);
	printf("Filename:			%S\r\n", filename);
	printf("File size:			%d\r\n", lpFileSizeHigh.QuadPart);
	printf("\r\n");

	SetFileMACE(file, fbi.FileAttributes, argv[2], argv[3], argv[4], argv[5]);
	RetrieveFileBasicInformation(file, &fbi);

	printf("\r\n");
	ConvertLargeIntegerToLocalTime(&time, fbi.LastWriteTime);
	if (fbi.LastWriteTime.QuadPart != 0) {
		sprintf_s(str, 256, "%lld", fbi.LastWriteTime.QuadPart);
		printf("[M] Last Write Time:		%04d-%02d-%02d %02d:%02d:%02d.%7s UTC\r\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, &str[11]);
	}
	else {
		printf("[M] Last Write Time:		%04d-%02d-%02d %02d:%02d:%02d.0000000 UTC\r\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	}
	memset(&time, 0, sizeof(time));

	ConvertLargeIntegerToLocalTime(&time, fbi.LastAccessTime);
	if (fbi.LastAccessTime.QuadPart != 0) {
		sprintf_s(str, 256, "%lld", fbi.LastAccessTime.QuadPart);
		printf("[A] Last Access Time:		%04d-%02d-%02d %02d:%02d:%02d.%7s UTC\r\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, &str[11]);
	}
	else {
		printf("[A] Last Access Time:		%04d-%02d-%02d %02d:%02d:%02d.0000000 UTC\r\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	}
	memset(&time, 0, sizeof(time));

	ConvertLargeIntegerToLocalTime(&time, fbi.ChangeTime);
	if (fbi.ChangeTime.QuadPart != 0) {
		sprintf_s(str, 256, "%lld", fbi.ChangeTime.QuadPart);
		printf("[C] Metadata Change Time:	%04d-%02d-%02d %02d:%02d:%02d.%7s UTC\r\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, &str[11]);
	}
	else {
		printf("[C] Metadata Change Time:	%04d-%02d-%02d %02d:%02d:%02d.0000000 UTC\r\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	}
	memset(&time, 0, sizeof(time));

	ConvertLargeIntegerToLocalTime(&time, fbi.CreationTime);
	if (fbi.CreationTime.QuadPart != 0) {
		sprintf_s(str, 256, "%lld", fbi.CreationTime.QuadPart);
		printf("[B] Creation Time:		%04d-%02d-%02d %02d:%02d:%02d.%7s UTC\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, &str[11]);
	}
	else {
		printf("[B] Creation Time:		%04d-%02d-%02d %02d:%02d:%02d.0000000 UTC\n", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	}
	printf("\r\n");
	CloseHandle(file);
}