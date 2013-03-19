#pragma once
#include <string>
using namespace std;

class Console
{
public:
	Console();
	~Console();

	void Startup();
	void InputReceived(string input);

	void GetInput();
	void AddLine(string text);
	static void InputThreadEntryPoint(void* pThis);

private:
	bool mThreadRunning;
};

extern Console* gConsole;