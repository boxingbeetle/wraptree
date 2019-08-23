// SPDX-License-Identifier: BSD-3-Clause

#include "StdAfx.h"

#define INITFAILED_EXIT_CODE 66601
#define TERMINATED_EXIT_CODE 66602
#define UNEXPECTED_EXIT_CODE 66603
#define NOTSTARTED_EXIT_CODE 66604
#define WATCHER_WAIT_TIMEOUT 5000
#define THREAD_START_TIMEOUT 5000
#define PRIMARY_SHUTDOWN_PRIORITY 0x300
#define WATCHER_SHUTDOWN_PRIORITY 0x301
#define KILL_ALL_ON_EXIT_VARIABLE TEXT("WRAPTREE_KILL_ON_EXIT")

HANDLE hProcessHeap = GetProcessHeap();

static void PrintErrorMessage(LPCTSTR lpMessage)
{
  DWORD dwError = GetLastError();
  TCHAR szDescription[] = _TEXT("Unknown");
  LPTSTR lpDescription = NULL;
  DWORD dwLength = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
    NULL, dwError, 0, (LPTSTR)&lpDescription, 0, NULL);
  if (!lpDescription)
  {
    lpDescription = szDescription;
  }
  _tprintf(_TEXT("%s: Error %u (0x%08X): %s\n"), lpMessage, dwError, dwError,
    lpDescription);
  if (lpDescription && (lpDescription != szDescription))
  {
    LocalFree(lpDescription);
  }
}

static HANDLE PrepareStandardHandle(DWORD dwHandleType)
{
  LPCTSTR lpTypeText;
  switch (dwHandleType)
  {
  case STD_INPUT_HANDLE:
    lpTypeText = TEXT("input ");
    break;
  case STD_OUTPUT_HANDLE:
    lpTypeText = TEXT("output ");
    break;
  case STD_ERROR_HANDLE:
    lpTypeText = TEXT("error ");
    break;
  default:
    lpTypeText = TEXT("stream ");
  }
  HANDLE hHandle = GetStdHandle(dwHandleType);
  if (hHandle == INVALID_HANDLE_VALUE)
  {
    TCHAR szMessage[64];
    _sntprintf_s(szMessage, 64, _TEXT("Failed to get standard %shandle"),
      lpTypeText);
    PrintErrorMessage(szMessage);
    hHandle = NULL;
  }
  else if (!hHandle)
  {
    _tprintf(_TEXT("Warning: the standard %shandle is NULL\n"), lpTypeText);
  }
  else
  {
    DWORD dwFlags = 0;
    if (!((GetHandleInformation(hHandle, &dwFlags) && (dwFlags &
      HANDLE_FLAG_INHERIT)) || SetHandleInformation(hHandle,
      HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)))
    {
      _tprintf(_TEXT("The standard %shandle may not be inheritable\n"),
        lpTypeText);
    }
  }
  return hHandle;
}

static int WatchPrimaryProcess(HANDLE hProcess, HANDLE hJob, HANDLE hEvent)
{
  //SetConsoleCtrlHandler(NULL, FALSE);
  SetProcessShutdownParameters(WATCHER_SHUTDOWN_PRIORITY, SHUTDOWN_NORETRY);
  DWORD dwExitCode = UNEXPECTED_EXIT_CODE;
  _tprintf(_TEXT("Watcher process is ready\n"));
  fflush(NULL);
  if (!SetEvent(hEvent))
  {
    PrintErrorMessage(TEXT("Failed to set watcher initialization event"));
  }
  CloseHandle(hEvent);
  SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
  switch (WaitForSingleObject(hProcess, INFINITE))
  {
  case WAIT_OBJECT_0:
    _tprintf(_TEXT("Primary process terminated\n"));
    if (!GetExitCodeProcess(hProcess, &dwExitCode))
    {
      dwExitCode = UNEXPECTED_EXIT_CODE;
    }
    break;
  case WAIT_FAILED:
    PrintErrorMessage(TEXT("Failed to wait for the primary process"));
    break;
  default:
    _tprintf(_TEXT("Unexpected result of waiting for the primary process\n"));
  }
  if (!TerminateJobObject(hJob, TERMINATED_EXIT_CODE))
  {
    PrintErrorMessage(TEXT("Failed to terminate the job"));
  }
  return dwExitCode;
}

static DWORD WINAPI InputPumpThreadProc(LPVOID lpHandles)
{
  HANDLE hToKill = ((LPHANDLE)lpHandles)[4];
  HANDLE hSource = ((LPHANDLE)lpHandles)[3];
  HANDLE hTarget = ((LPHANDLE)lpHandles)[2];
  if (SetEvent(((LPHANDLE)lpHandles)[1]))
  {
    BYTE Buffer[256];
    DWORD dwCount = 0;
    while (true)
    {
      if (ReadFile(hSource, Buffer, sizeof(Buffer), &dwCount, NULL))
      {
        if (!WriteFile(hTarget, Buffer, dwCount, &dwCount, NULL))
        {
          PrintErrorMessage(TEXT("Failed to write data to the output"));
        }
      }
      else if (GetLastError() == ERROR_BROKEN_PIPE)
      {
        if (!TerminateJobObject(hToKill, TERMINATED_EXIT_CODE))
        {
          PrintErrorMessage(TEXT("Failed to terminate the job"));
        }
        break;
      }
      else
      {
        PrintErrorMessage(TEXT("Failed to read data from the input"));
      }
    }
  }
  else
  {
    PrintErrorMessage(TEXT("Failed to set thread initialization event"));
  }
  return 0;
}

static void HookStandardInputStream(LPHANDLE lpHandle, HANDLE hJob)
{
  if ((*lpHandle) && (GetFileType(*lpHandle) == FILE_TYPE_PIPE))
  {
    HANDLE hPipeRead = NULL;
    HANDLE lpHandles[5] = {NULL, NULL, NULL, *lpHandle, hJob};
    if (CreatePipe(&hPipeRead, &lpHandles[2], NULL, 0))
    {
      bool fClosePipe = true;
      if (!(SetHandleInformation(hPipeRead, HANDLE_FLAG_INHERIT,
        HANDLE_FLAG_INHERIT) || (DuplicateHandle(GetCurrentProcess(),
        hPipeRead, GetCurrentProcess(), &hPipeRead, 0, TRUE,
        DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE) && hPipeRead)))
      {
        PrintErrorMessage(TEXT("Failed to set pipe inheritance flag"));
        CloseHandle(lpHandles[2]); //pipe write
        fClosePipe = false;
      }
      else if (lpHandles[1] = CreateEvent(NULL, TRUE, FALSE, NULL))
      {
        DWORD dwThreadId;
        if (lpHandles[0] = CreateThread(NULL, 0, InputPumpThreadProc,
          lpHandles, 0, &dwThreadId))
        {
          DWORD dwWaitResult = WaitForMultipleObjects(2, lpHandles, FALSE,
            THREAD_START_TIMEOUT);
          CloseHandle(lpHandles[1]); //event
          switch (dwWaitResult)
          {
          case WAIT_OBJECT_0:
            _tprintf(_TEXT("The pump thread has unexpectedly terminated\n"));
            break;
          case WAIT_OBJECT_0 + 1:
            *lpHandle = hPipeRead;
            fClosePipe = false;
            break;
          case WAIT_TIMEOUT:
            _tprintf(_TEXT("Pump thread initialization must have failed\n"));
            break;
          case WAIT_FAILED:
            PrintErrorMessage(TEXT("Failed to wait for the pump thread"));
            break;
          default:
            _tprintf(_TEXT("Unexpected result of waiting for the pump")
              _TEXT(" thread: 0x%08X\n"), dwWaitResult);
          }
          CloseHandle(lpHandles[0]); //thread
        }
        else
        {
          PrintErrorMessage(TEXT("Failed to create input pump thread"));
          CloseHandle(lpHandles[1]); //event
        }
      }
      else
      {
        PrintErrorMessage(TEXT("Failed to create thread start event"));
      }
      if (fClosePipe)
      {
        CloseHandle(hPipeRead);
        CloseHandle(lpHandles[2]); //pipe write
      }
    }
    else
    {
      PrintErrorMessage(TEXT("Failed to create input stream pipe"));
    }
  }
}

static HANDLE ExecuteWatchProcess(HANDLE hProcess, HANDLE hJob, HANDLE hEvent)
{
  HANDLE hResult = NULL;
  TCHAR szExecute[MAX_PATH];
  DWORD dwLength = GetModuleFileName(NULL, szExecute, MAX_PATH);
  if (dwLength && (dwLength < MAX_PATH - 1))
  {
    TCHAR szCommand[36]; //": 0x00000000 0x00000000 0x00000000"
    if (_stprintf_s(szCommand, TEXT(": 0x%08p 0x%08p 0x%08p"),
      hProcess, hJob, hEvent) > 0)
    {
      STARTUPINFO StartupInfo;
      ZeroMemory(&StartupInfo, sizeof(StartupInfo));
      StartupInfo.cb = sizeof(StartupInfo);
      //StartupInfo.hStdInput = PrepareStandardHandle(STD_INPUT_HANDLE);
      StartupInfo.hStdOutput = PrepareStandardHandle(STD_OUTPUT_HANDLE);
      StartupInfo.hStdError = PrepareStandardHandle(STD_ERROR_HANDLE);
      if (StartupInfo.hStdOutput || StartupInfo.hStdError)
      {
        StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
      }
      PROCESS_INFORMATION ProcessInfo;
      if (CreateProcess(szExecute, szCommand, NULL, NULL, TRUE,
        CREATE_NEW_PROCESS_GROUP | NORMAL_PRIORITY_CLASS, NULL, NULL,
        &StartupInfo, &ProcessInfo))
      {
        CloseHandle(ProcessInfo.hThread);
        hResult = ProcessInfo.hProcess;
      }
      else
      {
        PrintErrorMessage(TEXT("Failed to start watcher process"));
      }
    }
    else
    {
      _tprintf(_TEXT("Failed to construct watcher command line\n"));
    }
  }
  else
  {
    _tprintf(_TEXT("Failed to obtain watcher executable path\n"));
  }
  return hResult;
}

static int ExecuteChildProcess(LPCTSTR lpCommand)
{
  //SetConsoleCtrlHandler(NULL, FALSE);
  DWORD dwExitCode = NOTSTARTED_EXIT_CODE;
  SetErrorMode(
    SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
  SetProcessShutdownParameters(PRIMARY_SHUTDOWN_PRIORITY, SHUTDOWN_NORETRY);
  SECURITY_ATTRIBUTES Inherited = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
  _tprintf(_TEXT("Executing external command:\n%s\n"), lpCommand);
  if (HANDLE hJob = CreateJobObject(&Inherited, NULL))
  {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION JobExtendedInfo;
    ZeroMemory(&JobExtendedInfo, sizeof(JobExtendedInfo));
    /*
    DWORD dwReturned = 0;
    if (QueryInformationJobObject(hJob, JobObjectExtendedLimitInformation,
      &JobExtendedInfo, sizeof(JobExtendedInfo), &dwReturned)) { }
    //*/
    JobExtendedInfo.BasicLimitInformation.LimitFlags |=
      JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION |
      JOB_OBJECT_LIMIT_BREAKAWAY_OK;
    if (SetInformationJobObject(hJob, JobObjectExtendedLimitInformation,
      &JobExtendedInfo, sizeof(JobExtendedInfo)))
    {
      HANDLE Handles[] = {NULL, NULL};
      HANDLE hProcess = GetCurrentProcess();
      if (DuplicateHandle(hProcess, hProcess, hProcess, &hProcess,
        SYNCHRONIZE | PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE,
        TRUE, 0))
      {
        if (Handles[1] = CreateEvent(&Inherited, TRUE, FALSE, NULL))
        {
          if (Handles[0] = ExecuteWatchProcess(hProcess, hJob, Handles[1]))
          {
            SetHandleInformation(Handles[1], HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(hProcess, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(hJob, HANDLE_FLAG_INHERIT, 0);
            enum {aAbandon, aContinue, aTerminate} eAction = aTerminate;
            switch (WaitForMultipleObjects(2, Handles, FALSE,
              WATCHER_WAIT_TIMEOUT))
            {
            case WAIT_OBJECT_0:
              _tprintf(
                _TEXT("The watcher process has unexpectedly terminated\n"));
              eAction = aAbandon;
              break;
            case WAIT_OBJECT_0 + 1:
              eAction = aContinue;
              break;
            case WAIT_TIMEOUT:
              _tprintf(_TEXT("Watcher process initialization timeout\n"));
              break;
            case WAIT_FAILED:
              PrintErrorMessage(TEXT("Failed to wait for the child process"));
              break;
            default:
              _tprintf(_TEXT("Unexpected result of waiting for the child\n"));
            }
            if (eAction != aContinue)
            {
              if (eAction == aTerminate)
              {
                if (!TerminateProcess(Handles[0], TERMINATED_EXIT_CODE))
                {
                  PrintErrorMessage(TEXT("Failed to terminate the watcher"));
                }
              }
              if ((eAction != aAbandon) ||
                (!GetExitCodeProcess(Handles[0], &dwExitCode)))
              {
                dwExitCode = UNEXPECTED_EXIT_CODE;
              }
              CloseHandle(Handles[0]);
              Handles[0] = NULL;
            }
          }
          CloseHandle(Handles[1]);
          Handles[1] = NULL;
        }
        else
        {
          PrintErrorMessage(TEXT("Failed to create event object"));
        }
        CloseHandle(hProcess);
      }
      else
      {
        PrintErrorMessage(TEXT("Failed to duplicate process handle"));
      }
      if (Handles[0])
      {
        int iCmdLength = lstrlen(lpCommand);
        if (LPTSTR lpCmdLine =
          (LPTSTR)HeapAlloc(hProcessHeap, 0, (++iCmdLength) * sizeof(TCHAR)))
        {
          lstrcpyn(lpCmdLine, lpCommand, iCmdLength);
          STARTUPINFO StartupInfo;
          ZeroMemory(&StartupInfo, sizeof(StartupInfo));
          StartupInfo.cb = sizeof(StartupInfo);
          StartupInfo.hStdInput = PrepareStandardHandle(STD_INPUT_HANDLE);
          StartupInfo.hStdOutput = PrepareStandardHandle(STD_OUTPUT_HANDLE);
          StartupInfo.hStdError = PrepareStandardHandle(STD_ERROR_HANDLE);
          HookStandardInputStream(&StartupInfo.hStdInput, hJob);
          if (StartupInfo.hStdInput ||
            StartupInfo.hStdOutput || StartupInfo.hStdError)
          {
            StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
          }
          PROCESS_INFORMATION ProcessInfo;
          if (CreateProcess(NULL, lpCmdLine, NULL, NULL,
            (StartupInfo.dwFlags&STARTF_USESTDHANDLES)? TRUE: FALSE,
            CREATE_SUSPENDED | NORMAL_PRIORITY_CLASS, NULL, NULL,
            &StartupInfo, &ProcessInfo))
          {
            if (AssignProcessToJobObject(hJob, ProcessInfo.hProcess))
            {
              bool fTerminate = true;
              DWORD dwResumed = ResumeThread(ProcessInfo.hThread);
              CloseHandle(ProcessInfo.hThread);
              if (dwResumed == ((DWORD)-1))
              {
                PrintErrorMessage(TEXT("Failed to resume child thread"));
              }
              else if ((INT)dwResumed < 0)
              {
                _tprintf(_TEXT("Unexpected result of resuming child process")
                  _TEXT(" main thread: %d\n"), dwResumed);
              }
              else
              {
                if (dwResumed != 1)
                {
                  _tprintf(_TEXT("Warning! The child process suspend counter")
                    _TEXT(" has unexpected value: %d\n"), dwResumed);
                }
                fflush(NULL);
                Handles[1] = ProcessInfo.hProcess;
                SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
                DWORD dwWaitResult =
                  WaitForMultipleObjects(2, Handles, FALSE, INFINITE);
                switch (dwWaitResult)
                {
                case WAIT_OBJECT_0:
                  _tprintf(_TEXT("Watcher process terminated\n"));
                  break;
                case WAIT_OBJECT_0 + 1:
                  fTerminate = false;
                  break;
                case WAIT_FAILED:
                  PrintErrorMessage(TEXT("Failed to wait for the processes"));
                  break;
                default:
                  _tprintf(_TEXT("Unexpected result of waiting for processes")
                    _TEXT(": 0x%08X\n"), dwWaitResult);
                }
              }
              if (fTerminate)
              {
                if (!TerminateJobObject(hJob, TERMINATED_EXIT_CODE))
                {
                  PrintErrorMessage(TEXT("Failed to terminate the job"));
                }
              }
              else
              {
                TCHAR szFlag[2];
                if ((GetEnvironmentVariable(KILL_ALL_ON_EXIT_VARIABLE, szFlag,
                  sizeof(szFlag)/sizeof(TCHAR)) == 1) && (szFlag[0] == '0') &&
                  (!TerminateProcess(Handles[0], TERMINATED_EXIT_CODE)))
                {
                  PrintErrorMessage(TEXT("Failed to terminate the watcher"));
                }
              }
              if (!GetExitCodeProcess(Handles[1], &dwExitCode))
              {
                dwExitCode = UNEXPECTED_EXIT_CODE;
              }
            }
            else
            {
              PrintErrorMessage(TEXT("Failed to add the child to the job"));
              CloseHandle(ProcessInfo.hThread);
              TerminateProcess(ProcessInfo.hProcess, TERMINATED_EXIT_CODE);
            }
            CloseHandle(ProcessInfo.hProcess);
          }
          else
          {
            PrintErrorMessage(TEXT("Failed to start child process"));
          }
          HeapFree(hProcessHeap, 0, lpCmdLine);
        }
        else
        {
          _tprintf(_TEXT("Failed to allocate memory for command line\n"));
        }
      }
    }
    else
    {
      PrintErrorMessage(TEXT("Failed to configure job object"));
    }
    CloseHandle(hJob);
  }
  else
  {
    PrintErrorMessage(TEXT("Failed to create job object"));
  }
  return dwExitCode;
}

static bool ParseHandleValue(LPCTSTR lpString, HANDLE *lpHandle)
{
  bool fResult = false;
  if ((lstrlen(lpString) > 2) && (lpString[0] == '0') && (lpString[1] == 'x'))
  {
    LPTSTR lpEnd = NULL;
    unsigned long ulValue = _tcstoul(lpString + 2, &lpEnd, 16);
    if (ulValue && lpEnd && (lpEnd > lpString + 2) && (*lpEnd =='\0'))
    {
      *lpHandle = (HANDLE)ulValue;
      fResult = true;
    }
  }
  return fResult;
}

int __cdecl _tmain(int argc, _TCHAR* argv[])
{
  if (argc <= 0)
  {
    _tprintf(_TEXT("Something went wrong during startup: argc=%d\n"), argc);
    return INITFAILED_EXIT_CODE;
  }
  LPCTSTR lpCommand = NULL;
  int iCmdLength = lstrlen(argv[0]);
  if ((iCmdLength == 1) && (*argv[0] == ':'))
  {
    //This is the watcher process
    if (argc != 4)
    {
      _tprintf(_TEXT("Watcher process requires exactly three parameters\n"));
      return INITFAILED_EXIT_CODE;
    }
    HANDLE hProcess = NULL, hJob = NULL, hEvent = NULL;
    if (!(ParseHandleValue(argv[1], &hProcess) && 
      ParseHandleValue(argv[2], &hJob) && ParseHandleValue(argv[3], &hEvent)))
    {
      _tprintf(_TEXT("Failed to parse watch handle values: %s %s %s\n"),
        argv[1], argv[2], argv[3]);
      return INITFAILED_EXIT_CODE;
    }
    return WatchPrimaryProcess(hProcess, hJob, hEvent);
  }
  else
  {
    //This is the primary process
    LPCTSTR lpCmdLine = GetCommandLine();
    if (_tcsncmp(argv[0], lpCmdLine, iCmdLength) == 0)
    {
      lpCommand = lpCmdLine + iCmdLength;
    }
    else if ((lpCmdLine[0] == '"') && (lpCmdLine[iCmdLength + 1] == '"') &&
      (_tcsncmp(argv[0], lpCmdLine + 1, iCmdLength) == 0))
    {
      lpCommand = lpCmdLine + iCmdLength + 2;
    }
    if ((!lpCommand) || (*lpCommand > ' '))
    {
      _tprintf(_TEXT("Failed to parse command line: %s\n"), lpCmdLine);
      return INITFAILED_EXIT_CODE;
    }
    for (/*nothing*/; (*lpCommand) && (*lpCommand <= ' '); lpCommand++);
    if (!*lpCommand)
    {
      _tprintf(_TEXT("No command line specified to run child process\n"));
      return INITFAILED_EXIT_CODE;
    }
    return ExecuteChildProcess(lpCommand);
  }
}
