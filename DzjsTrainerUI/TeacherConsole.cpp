// 教师端内置控制台：接管子进程的 stdin/stdout，用编辑框做交互。
// 单独成文件，避免继续把 MainWindow.cpp 撑大。
#include "stdafx.h"
#include "MainWindow.h"
#include "../DzjsTrainer/Logger.h"
#include "../DzjsTrainer/AppPublic.h"

extern LoggerInternal* currentLogger;

namespace {
constexpr UINT kTeacherConsoleOutputMessage = WM_APP + 71;
constexpr int kTeacherConsoleSendId = 0x7C01;
HWND g_consoleOwner = nullptr;
WNDPROC g_hostOldProc = nullptr;
WNDPROC g_inputOldProc = nullptr;

LRESULT CALLBACK TeacherHostProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// 宿主是 STATIC，BN_CLICKED 只会发给它自己；转给主窗口处理。
	if (message == WM_COMMAND && LOWORD(wParam) == kTeacherConsoleSendId) {
		if (g_consoleOwner) PostMessageW(g_consoleOwner, WM_COMMAND, wParam, lParam);
		return 0;
	}
	return CallWindowProcW(g_hostOldProc, hwnd, message, wParam, lParam);
}

LRESULT CALLBACK TeacherInputProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// 输入框里按回车等同于点"发送"。
	if (message == WM_KEYDOWN && wParam == VK_RETURN) {
		if (g_consoleOwner) PostMessageW(g_consoleOwner, WM_COMMAND,
			static_cast<WPARAM>(kTeacherConsoleSendId), 0);
		return 0;
	}
	return CallWindowProcW(g_inputOldProc, hwnd, message, wParam, lParam);
}

DWORD WINAPI TeacherConsoleReaderThread(LPVOID parameter)
{
	HANDLE pipe = static_cast<HANDLE>(parameter);
	char buffer[4096];
	DWORD read = 0;
	std::string pending;
	while (ReadFile(pipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
		pending.append(buffer, read);
		size_t lineEnd = 0;
		while ((lineEnd = pending.find('\n')) != std::string::npos) {
			std::string line = pending.substr(0, lineEnd + 1);
			pending.erase(0, lineEnd + 1);
			UINT codePage = CP_UTF8;
			int wideLength = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), nullptr, 0);
			if (wideLength <= 0) {
				codePage = CP_ACP;
				wideLength = MultiByteToWideChar(CP_ACP, 0, line.c_str(), static_cast<int>(line.size()), nullptr, 0);
			}
			std::wstring wide(static_cast<size_t>(wideLength > 0 ? wideLength : 0), L'\0');
			if (wideLength > 0) {
				MultiByteToWideChar(codePage, 0, line.c_str(), static_cast<int>(line.size()), wide.data(), wideLength);
			}
			if (!wide.empty() && g_consoleOwner) {
				auto* payload = new std::wstring(wide);
				if (!PostMessageW(g_consoleOwner, kTeacherConsoleOutputMessage, 0,
					reinterpret_cast<LPARAM>(payload))) {
					delete payload;   // 窗口没了或队列满：不留无主堆对象
				}
			}
		}
	}
	return 0;
}
} // namespace

UINT MainWindowTeacherConsoleMessage() { return kTeacherConsoleOutputMessage; }
int MainWindowTeacherConsoleSendId() { return kTeacherConsoleSendId; }

bool MainWindow::StartTeacherConsoleProcess(const std::wstring& executable, const std::wstring& workingDirectory)
{
	SECURITY_ATTRIBUTES attributes{ sizeof(attributes), nullptr, TRUE };
	HANDLE inRead = nullptr, inWrite = nullptr, outRead = nullptr, outWrite = nullptr;
	if (!CreatePipe(&inRead, &inWrite, &attributes, 0)) return false;
	if (!CreatePipe(&outRead, &outWrite, &attributes, 0)) {
		CloseHandle(inRead);
		CloseHandle(inWrite);
		return false;
	}
	// 只有被继承的三端需要 inherit 标记。
	SetHandleInformation(inWrite, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOW startup{ sizeof(startup) };
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdInput = inRead;
	startup.hStdOutput = outWrite;
	startup.hStdError = outWrite;

	std::wstring commandLine = L"\"" + executable + L"\"";
	std::vector<wchar_t> command(commandLine.begin(), commandLine.end());
	command.push_back(L'\0');

	PROCESS_INFORMATION process{};
	const BOOL started = CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
		CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, nullptr, workingDirectory.c_str(), &startup, &process);
	CloseHandle(inRead);
	CloseHandle(outWrite);

	if (!started) {
		CloseHandle(inWrite);
		CloseHandle(outRead);
		return false;
	}

	CloseHandle(process.hThread);
	teacherProcess = process.hProcess;
	teacherProcessId = process.dwProcessId;
	teacherStdin = inWrite;
	teacherStdout = outRead;
	teacherConsoleMode = true;
	teacherLaunchPending = false;
	teacherLaunchDeadline = 0;
	g_consoleOwner = _hWnd;
	teacherReader = CreateThread(nullptr, 0, TeacherConsoleReaderThread, teacherStdout, 0, nullptr);

	EnsureTeacherConsole();
	if (teacherLaunchButton) SetWindowTextW(teacherLaunchButton, L"关闭教师端");
	AppendTeacherConsole(L"教师端已启动（内置控制台）。输入命令后回车，例如 login / help。\r\n");
	if (currentLogger) currentLogger->LogInfo(L"teacher-gui: console host started (pid=%lu)", teacherProcessId);
	return true;
}

void MainWindow::EnsureTeacherConsole()
{
	if (!teacherHostWindow) return;
	if (!teacherConsoleLog) {
		teacherConsoleLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
			0, 0, 10, 10, teacherHostWindow, nullptr, JTAppGetInstanceDirect(), nullptr);
		SendMessageW(teacherConsoleLog, WM_SETFONT,
			reinterpret_cast<WPARAM>(GetStockObject(ANSI_FIXED_FONT)), TRUE);
	}
	if (!teacherConsoleInput) {
		teacherConsoleInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
			0, 0, 10, 10, teacherHostWindow, nullptr, JTAppGetInstanceDirect(), nullptr);
		SendMessageW(teacherConsoleInput, WM_SETFONT,
			reinterpret_cast<WPARAM>(GetStockObject(ANSI_FIXED_FONT)), TRUE);
	}
	if (!teacherConsoleSend) {
		teacherConsoleSend = CreateWindowExW(0, L"BUTTON", L"发送",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 10, 10, teacherHostWindow,
			reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTeacherConsoleSendId)),
			JTAppGetInstanceDirect(), nullptr);
	}
	if (teacherHostWindow && !g_hostOldProc) {
		g_hostOldProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(teacherHostWindow,
			GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(TeacherHostProc)));
	}
	if (teacherConsoleInput && !g_inputOldProc) {
		g_inputOldProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(teacherConsoleInput,
			GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(TeacherInputProc)));
	}
	LayoutTeacherConsole();
}

void MainWindow::LayoutTeacherConsole()
{
	if (!teacherHostWindow || !teacherConsoleLog) return;
	RECT client{};
	GetClientRect(teacherHostWindow, &client);
	const int width = client.right > 40 ? client.right : 600;
	const int height = client.bottom > 80 ? client.bottom : 400;
	const int buttonWidth = 76;
	const int inputHeight = 28;
	MoveWindow(teacherConsoleLog, 0, 0, width, height - inputHeight - 6, TRUE);
	MoveWindow(teacherConsoleInput, 0, height - inputHeight, width - buttonWidth - 6, inputHeight, TRUE);
	MoveWindow(teacherConsoleSend, width - buttonWidth, height - inputHeight, buttonWidth, inputHeight, TRUE);
}

void MainWindow::AppendTeacherConsole(const std::wstring& text)
{
	if (!teacherConsoleLog) return;
	const int length = GetWindowTextLengthW(teacherConsoleLog);
	SendMessageW(teacherConsoleLog, EM_SETSEL, length, length);
	SendMessageW(teacherConsoleLog, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
	SendMessageW(teacherConsoleLog, EM_SCROLLCARET, 0, 0);
}

void MainWindow::SendTeacherConsoleCommand()
{
	if (!teacherConsoleInput || !teacherStdin) return;
	wchar_t buffer[2048]{};
	GetWindowTextW(teacherConsoleInput, buffer, _countof(buffer));
	if (!buffer[0]) return;
	std::wstring line = std::wstring(buffer) + L"\r\n";
	const int bytes = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), nullptr, 0, nullptr, nullptr);
	if (bytes > 0) {
		std::string utf8(static_cast<size_t>(bytes), '\0');
		WideCharToMultiByte(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), utf8.data(), bytes, nullptr, nullptr);
		DWORD written = 0;
		WriteFile(teacherStdin, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
	}
	AppendTeacherConsole(L"> " + std::wstring(buffer) + L"\r\n");
	SetWindowTextW(teacherConsoleInput, L"");
}

void MainWindow::CloseTeacherConsole()
{
	// stdin 先关：子进程读到 EOF 会退出，读线程随之中断 ReadFile，
	// 这时再关读端和线程句柄才是安全的（反过来会让线程读已关闭/复用的句柄）。
	if (teacherStdin) { CloseHandle(teacherStdin); teacherStdin = nullptr; }
	if (teacherReader) {
		WaitForSingleObject(teacherReader, 3000);
		CloseHandle(teacherReader);
		teacherReader = nullptr;
	}
	if (teacherStdout) { CloseHandle(teacherStdout); teacherStdout = nullptr; }
	if (teacherConsoleLog) { DestroyWindow(teacherConsoleLog); teacherConsoleLog = nullptr; }
	if (teacherConsoleInput) { DestroyWindow(teacherConsoleInput); teacherConsoleInput = nullptr; }
	if (teacherConsoleSend) { DestroyWindow(teacherConsoleSend); teacherConsoleSend = nullptr; }
	teacherConsoleMode = false;
	g_consoleOwner = nullptr;
}
