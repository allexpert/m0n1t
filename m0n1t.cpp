// m0n1t.cpp : main project file.

// MS service: monitor given registry entry and do some stuff, e.g. return back its initial value
// Is not generic yet, registry key is hardcoded; action is hardcoded (see in the code). 

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>

#include <iostream>
#include <fstream>
#include <cstdio>

#include <strsafe.h>

//using namespace std;

/* Parameters to pass to service:
e.g. HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet/Services/m0n1t/ImagePath
hmmm... not working so far...

The registry key to monitor in this code: 
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Internet Settings\ProxyOverride
*/

SERVICE_STATUS        g_ServiceStatus = {0};
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

LPTSTR wdir= L"C:\\tmp\\";
LPTSTR wfil= L"m0n1t.demo";
LPTSTR rdir= L"C:\\tmp\\";
LPTSTR rfil= L"m0n1t.demo.template";

VOID WINAPI ServiceMain (DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler (DWORD);
DWORD WINAPI ServiceWorkerThread (LPVOID lpParam);

FILE *flog= NULL;

#define SERVICE_NAME  _T("m0n1t")

void vaOutputDebugString0( LPCTSTR sFormat, ... )
{
	va_list argptr;      
	va_start( argptr, sFormat ); 
	TCHAR buffer[ 2000 ];
	HRESULT hr = StringCbVPrintf( buffer, sizeof( buffer ), sFormat, argptr );
	if ( STRSAFE_E_INSUFFICIENT_BUFFER == hr || S_OK == hr )
		//		WriteEventLogEntry(buffer,  EVENTLOG_INFORMATION_TYPE);
		OutputDebugString( buffer );
	else
		OutputDebugString( _T("StringCbVPrintf error.") );
}

void vaOutputDebugString( LPCTSTR sFormat, ... )
{
	va_list argptr;      
	va_start( argptr, sFormat ); 
	TCHAR buffer[2000];
	HRESULT hr = StringCbVPrintf( buffer, sizeof( buffer ), sFormat, argptr );
	if ( STRSAFE_E_INSUFFICIENT_BUFFER == hr || S_OK == hr )
		fprintf(flog, "%ls\n", buffer);
	else
		fprintf(flog, "StringCbVPrintf error.");
	fflush(flog);
}

#include <windows.h>
#include <tchar.h>
#include <stdio.h>

//void main(int argc, char *argv[])
int _tmain (int argc, TCHAR *argv[])
{
	flog= fopen("C:\\tmp\\m0n1t.log", "a");

    vaOutputDebugString(_T("m0n1t: Main: Entry argc=%d"), argc);

	if (argc > 0) {
		vaOutputDebugString(_T("tmain arg[0] %s"), argv[0]);
	}
	if (argc > 1) {
		wdir= argv[1];
		vaOutputDebugString(_T("tmain arg[1] %s"), argv[1]);
	}
	if (argc > 2) {
		wfil= argv[2];
		vaOutputDebugString(_T("tmain arg[2] %s"), argv[2]);
	}
	if (argc > 3) {
		rdir= argv[3];
		vaOutputDebugString(_T("tmain arg[3] %s"), argv[3]);
	}
	if (argc > 4) {
		rfil= argv[4];
		vaOutputDebugString(_T("tmain arg[4] %s"), argv[4]);
	}

    SERVICE_TABLE_ENTRY ServiceTable[] = 
    {
        {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION) ServiceMain},
        {NULL, NULL}
    };

	if (StartServiceCtrlDispatcher (ServiceTable) == FALSE)
    {
       OutputDebugString(_T("m0n1t: Main: StartServiceCtrlDispatcher returned error"));
       return GetLastError ();
    }

    OutputDebugString(_T("m0n1t: Main: Exit"));
    return 0;
}

VOID WINAPI ServiceMain (DWORD argc, LPTSTR *argv)
{
    DWORD Status = E_FAIL;

	vaOutputDebugString(_T("m0n1t: ServiceMain: Entry argc=%d"), argc);

	if (argc > 0) {
		vaOutputDebugString(_T("m0n1t arg[0] %s"), argv[0]);
	}

	g_StatusHandle = RegisterServiceCtrlHandler (SERVICE_NAME, ServiceCtrlHandler);

    if (g_StatusHandle == NULL) 
    {
        OutputDebugString(_T("m0n1t: ServiceMain: RegisterServiceCtrlHandler returned error"));
        goto EXIT;
    }

    // Tell the service controller we are starting
    ZeroMemory (&g_ServiceStatus, sizeof (g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus (g_StatusHandle, &g_ServiceStatus) == FALSE) 
    {
        vaOutputDebugString(_T("m0n1t: ServiceMain: SetServiceStatus returned error"));
    }

    /* 
     * Perform tasks neccesary to start the service here
     */
    vaOutputDebugString(_T("m0n1t: ServiceMain: Performing Service Start Operations"));

    // Create stop event to wait on later.
    g_ServiceStopEvent = CreateEvent (NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL) 
    {
        OutputDebugString(_T("m0n1t: ServiceMain: CreateEvent(g_ServiceStopEvent) returned error"));

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;

        if (SetServiceStatus (g_StatusHandle, &g_ServiceStatus) == FALSE)
	    {
		    vaOutputDebugString(_T("m0n1t: ServiceMain: SetServiceStatus returned error"));
	    }
        goto EXIT; 
    }    

    // Tell the service controller we are started
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus (g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
	    vaOutputDebugString(_T("m0n1t: ServiceMain: SetServiceStatus returned error"));
    }

    // Start the thread that will perform the main task of the service
    HANDLE hThread = CreateThread (NULL, 0, ServiceWorkerThread, wdir, 0, NULL);

    vaOutputDebugString(_T("m0n1t: ServiceMain: Waiting for Worker Thread to complete"));

    // Wait until our worker thread exits effectively signaling that the service needs to stop
    WaitForSingleObject (hThread, INFINITE);

    vaOutputDebugString(_T("m0n1t: ServiceMain: Worker Thread Stop Event signaled"));

    /* 
     * Perform any cleanup tasks
     */
    vaOutputDebugString(_T("m0n1t: ServiceMain: Performing Cleanup Operations"));

    CloseHandle (g_ServiceStopEvent);

    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;

    if (SetServiceStatus (g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
	    vaOutputDebugString(_T("m0n1t: ServiceMain: SetServiceStatus returned error"));
    }
    
    EXIT:
    vaOutputDebugString(_T("m0n1t: ServiceMain: Exit"));

    return;
}

VOID WINAPI ServiceCtrlHandler (DWORD CtrlCode)
{
    OutputDebugString(_T("m0n1t: ServiceCtrlHandler: Entry"));

    switch (CtrlCode) 
	{
     case SERVICE_CONTROL_STOP :

        OutputDebugString(_T("m0n1t: ServiceCtrlHandler: SERVICE_CONTROL_STOP Request"));

        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
           break;

        /* 
         * Perform tasks neccesary to stop the service here 
         */
        
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwWin32ExitCode = 0;
        g_ServiceStatus.dwCheckPoint = 4;

        if (SetServiceStatus (g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			OutputDebugString(_T("m0n1t: ServiceCtrlHandler: SetServiceStatus returned error"));
		}

        // This will signal the worker thread to start shutting down
        SetEvent (g_ServiceStopEvent);

        break;

     default:
         break;
    }

    OutputDebugString(_T("m0n1t: ServiceCtrlHandler: Exit"));
}

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383
void QueryKey(HKEY hKey)
{
    CHAR         achKey[MAX_KEY_LENGTH];   // buffer for subkey name
    DWORD    cbName;                                        // size of name string
    CHAR        achClass[MAX_PATH] = "";        // buffer for class name
    DWORD    cchClassName = MAX_PATH;  // size of class string
    DWORD    cSubKeys=0;                               // number of subkeys
    DWORD    cbMaxSubKey;                            // longest subkey size
    DWORD    cchMaxClass;                              // longest class string
    DWORD    cValues;                                       // number of values for key
    DWORD    cchMaxValue;                             // longest value name
    DWORD    cbMaxValueData;                      // longest value data
    DWORD    cbSecurityDescriptor;               // size of security descriptor
    FILETIME ftLastWriteTime;                         // last write time
    DWORD i, retCode;
    CHAR  achValue[MAX_VALUE_NAME];
    DWORD cchValue = MAX_VALUE_NAME;

    // Get the class name and the value count.

    retCode = RegQueryInfoKey(
        hKey,                                 // key handle
        (LPWSTR) achClass,                         // buffer for class name
        &cchClassName,           // size of class string
        NULL,                              // reserved
        &cSubKeys,                   // number of subkeys
        &cbMaxSubKey,            // longest subkey size
        &cchMaxClass,            // longest class string
        &cValues,                     // number of values for this key
        &cchMaxValue,                 // longest value name
        &cbMaxValueData,         // longest value data
        &cbSecurityDescriptor,   // security descriptor
        &ftLastWriteTime);          // last write time
   // Enumerate the subkeys, until RegEnumKeyEx() fails.
   if(cSubKeys)
    {
       vaOutputDebugString(_T("Subkey Names:\n"));
       for(i=0; i<cSubKeys; i++)
        {
            cbName = MAX_KEY_LENGTH;
            retCode = RegEnumKeyEx(
                    hKey,                          // Handle to an open/predefined key
                    i,                                 // Index of the subkey to retrieve.
                     (LPWSTR) achKey,                    // buffer that receives the name of the subkey
                     &cbName,               // size of the buffer specified by the achKey
                     NULL,                      // Reserved; must be NULL
                     NULL,                      // buffer that receives the class string of the enumerated subkey
                     NULL,                      // size of the buffer specified by the previous parameter
                     &ftLastWriteTime  // variable that receives the time at which  the enumerated subkey was last written
            );
            if(retCode == ERROR_SUCCESS){ 
				vaOutputDebugString(_T("(%d) %s\n"), i+1, achKey); }
       }
       vaOutputDebugString(_T("Number of subkeys: %d\n\n"), cSubKeys);
    } else
       vaOutputDebugString(_T("RegEnumKeyEx(), there is no subkey.\n"));
   // Enumerate the key values if any.
   if(cValues) {
        for(i=0, retCode=ERROR_SUCCESS; i<cValues; i++) {
            cchValue = MAX_VALUE_NAME;
            achValue[0] = '\0';
           retCode = RegEnumValue(
                hKey,              // Handle to an open key
                i,                     // Index of value
                (LPWSTR) achValue,      // Value name
                &cchValue,   // Buffer for value name
                NULL,           // Reserved
                NULL,           // Value type
                NULL,          // Value data
                NULL);         // Buffer for value data
            if(retCode == ERROR_SUCCESS)
            { vaOutputDebugString(_T("(%d) Value Name: %s.\n"), i+1, achValue); }
       }
       vaOutputDebugString(_T("Number of values: %d\n"), cValues);
    } else
       vaOutputDebugString(_T("No value under this key/subkey...\n"));
}

void PrintLValue(HKEY hKey, LPCWSTR lvalName)
{
	DWORD len = 10240;
    PPERF_DATA_BLOCK buffer = (PPERF_DATA_BLOCK) malloc( len );
	DWORD type= REG_SZ;
	LONG lErrorCode = RegQueryValueEx(hKey, lvalName, NULL, NULL, (LPBYTE) buffer, (LPDWORD) &len);
	if (lErrorCode != ERROR_SUCCESS)
	{
		vaOutputDebugString(_T("Error in RegQueryValueEx (%d).\n"), lErrorCode);
		ExitProcess(GetLastError()); 
	}
	vaOutputDebugString(_T("%ls= %s\n"), lvalName, buffer);
}

DWORD WINAPI ServiceWorkerThread (LPVOID lpParam)
{
	int iret= 0;
	LPTSTR lpDir= (LPTSTR) lpParam;

	OutputDebugString(_T("m0n1t: ServiceWorkerThread: Entry"));

	DWORD  dwFilter = REG_NOTIFY_CHANGE_LAST_SET |
		REG_NOTIFY_CHANGE_NAME |
		REG_NOTIFY_CHANGE_ATTRIBUTES |
		REG_NOTIFY_CHANGE_SECURITY;

	HANDLE hEvent;
	HKEY   hMainKey= HKEY_USERS;
	HKEY   hKey;
	LONG   lErrorCode;
	LPCWSTR lvalName = L"ProxyOverride";

	vaOutputDebugString(_T("m0n1t: ServiceWorkerThread"));

	// Convert parameters to appropriate handles.
	hMainKey= HKEY_CURRENT_USER;

	// Open a key (hardcoed!):
	lErrorCode = RegOpenKeyEx(hMainKey, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"), 0, /*KEY_NOTIFY*/ KEY_QUERY_VALUE | KEY_ALL_ACCESS /*| KEY_WOW64_64KEY*/, &hKey);
	if (lErrorCode != ERROR_SUCCESS)
	{
		vaOutputDebugString(_T("Error in RegOpenKeyEx (%d).\n"), lErrorCode);
		ExitProcess(GetLastError()); 
	}
	//ERROR_FILE_NOT_FOUND

	//QueryKey(hKey) (hardcoded!);
	PrintLValue(hKey, lvalName);

	// Create an event.
	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (hEvent == NULL)
	{
		vaOutputDebugString(_T("Error in CreateEvent (%d).\n"), GetLastError());
		ExitProcess(GetLastError()); 
	}

	// Watch the registry key for a change of value.
	lErrorCode = RegNotifyChangeKeyValue(hKey, 
		TRUE, 
		dwFilter, 
		hEvent, 
		TRUE);
	if (lErrorCode != ERROR_SUCCESS)
	{
		vaOutputDebugString(_T("Error in RegNotifyChangeKeyValue (%d).\n"), lErrorCode);
		ExitProcess(GetLastError()); 
	}

	// Wait for an event to occur.
	vaOutputDebugString(_T("Waiting for a change in the specified key...\n"));
	while (WaitForSingleObject(hEvent, INFINITE) != WAIT_FAILED)
	{
		// Action: change ProxyOverride to the string below
		const std::wstring kvalue=_T("127.*.*.*;*.hotmail.com;*.outlook.com;*.microsoft.com;*.msftncsi.com;*.microsoftstore.com;*google.com;*xmarks.com;*ebay.com;*paypal.com;<local>");

		vaOutputDebugString(_T("\nChange has occurred!\n"));
		PrintLValue(hKey, lvalName);
		vaOutputDebugString(_T("\n...rolling back...\n"));

		// Close the handle.
		if (!CloseHandle(hEvent))
		{
			vaOutputDebugString(_T("Error in CloseHandle.\n"));
			ExitProcess(GetLastError()); 
		}

		RegSetValueEx(hKey, lvalName, 0, REG_SZ, (BYTE*) kvalue.c_str(), (kvalue.size()+1)*sizeof(wchar_t));

		// Create an event.
		hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (hEvent == NULL)
		{
			vaOutputDebugString(_T("Error in CreateEvent (%d).\n"), GetLastError());
			ExitProcess(GetLastError()); 
		}

		// Watch the registry key for a change of value.
		lErrorCode = RegNotifyChangeKeyValue(hKey, 
			TRUE, 
			dwFilter, 
			hEvent, 
			TRUE);
		if (lErrorCode != ERROR_SUCCESS)
		{
			vaOutputDebugString(_T("Error in RegNotifyChangeKeyValue (%d).\n"), lErrorCode);
			ExitProcess(GetLastError()); 
		}
	}
	vaOutputDebugString(_T("Error in WaitForSingleObject (%d).\n"), GetLastError());

	// Close the key.
	lErrorCode = RegCloseKey(hKey);
	if (lErrorCode != ERROR_SUCCESS)
	{
		vaOutputDebugString(_T("Error in RegCloseKey (%d).\n"), GetLastError());
		ExitProcess(GetLastError()); 
	}

    return ERROR_SUCCESS;
}
