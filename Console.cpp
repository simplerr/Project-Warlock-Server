#include "Console.h"
#include <windows.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <fstream>
#include <process.h>
#include "Runnable.h"

// Maximum mumber of lines the output console should have.
static const WORD MAX_CONSOLE_LINES = 500;

BOOL WINAPI ConsoleHandlerRoutine(DWORD dwCtrlType)
{
	if(dwCtrlType == CTRL_CLOSE_EVENT)
	{
		// Send WM_DESTROY message.
		SendMessage(GLib::GlobalApp->GetHwnd(), WM_DESTROY, 0, 0);

		// Give the main thread some time to cleanup.
		Sleep(3000);
	}

	return FALSE;
}

Console::Console()
{
	mThreadRunning = false; 
}

Console::~Console()
{

}

void Console::Startup()
{
	int hConHandle;
	long lStdHandle;
	CONSOLE_SCREEN_BUFFER_INFO coninfo;
	FILE *fp;

	// allocate a console for this app
	AllocConsole();

	// set the screen buffer to be big enough to let us scroll text
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
	coninfo.dwSize.Y = MAX_CONSOLE_LINES;

	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

	// redirect unbuffered STDOUT to the console
	lStdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);
	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	fp = _fdopen( hConHandle, "w" );
	*stdout = *fp;
	setvbuf( stdout, NULL, _IONBF, 0 );

	// redirect unbuffered STDIN to the console
	lStdHandle = (long)GetStdHandle(STD_INPUT_HANDLE);
	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	fp = _fdopen( hConHandle, "r" );
	*stdin = *fp;
	setvbuf( stdin, NULL, _IONBF, 0 );

	// redirect unbuffered STDERR to the console
	lStdHandle = (long)GetStdHandle(STD_ERROR_HANDLE);
	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	fp = _fdopen( hConHandle, "w" );
	*stderr = *fp;
	setvbuf( stderr, NULL, _IONBF, 0 );

	// make cout, wcout, cin, wcin, wcerr, cerr, wclog and clog
	// point to console as well
	std::ios::sync_with_stdio();

	// Set console handler, to catch the shutdown event.
	SetConsoleCtrlHandler(ConsoleHandlerRoutine, TRUE);
}

void Console::GetInput()
{
	mThreadRunning = true;

	char buffer[256];
	scanf(buffer, "%s", buffer);

	if(string(buffer) != "")
		int a = 1;

	//_beginthread(Console::InputThreadEntryPoint, 0, this);
}

void Console::InputThreadEntryPoint(void* pThis)
{
	char buffer[256];
	scanf(buffer, "%s", buffer);

	((Console*)pThis)->InputReceived(buffer);
}

void Console::InputReceived(string input)
{
	printf(string("Recieved input! " + input).c_str());
}

void Console::AddLine(string text)
{
	printf(string(text + "\n").c_str());
}