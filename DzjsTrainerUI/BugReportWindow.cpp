#include "stdafx.h"
#include <shellapi.h>
#include "BugReportWindow.h"
#include "../include/WindowCaptureProtection.h"
#include "resource.h"
#include "../DzjsTrainer/AppPublic.h"
#include "../DzjsTrainer/StringHlp.h"
#include "../DzjsTrainer/SysHlp.h"
#include "../DzjsTrainerUpdater/DzjsTrainerUpdater.h"

extern JTApp* currentApp;

void ShowBugReportWindow() 
{
	DialogBox(currentApp->GetInstance(), MAKEINTRESOURCE(IDD_BUGREPORT), NULL, BugReportDlgFunc);
}

INT_PTR CALLBACK BugReportDlgFunc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG: {
		JiYuWindowCapture::ApplyCapturePolicy(hDlg);

		SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(currentApp->GetInstance(), MAKEINTRESOURCE(IDI_BUG)));
		SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(currentApp->GetInstance(), MAKEINTRESOURCE(IDI_BUG)));
		SetDlgItemText(hDlg, IDC_BUGREPORT_CONTENT, L"无法获取程序错误日志");
		CheckDlgButton(hDlg, IDC_REBOOT, BST_CHECKED);

		int  appArgCount = currentApp->GetCommandLineArraySize();
		LPWSTR *appArgList = currentApp->GetCommandLineArray();
		int argFIndex = currentApp->FindArgInCommandLine(appArgList, appArgCount, L"bugfile");
		if (argFIndex >= 0 && (argFIndex + 1) < appArgCount) {
			LPCWSTR bugFilePath = appArgList[argFIndex + 1];
			// 日志是 UTF-8 写的，原来用 _wfopen_s(..., L"r") + fgets + SetDlgItemTextA
			// 按 ANSI 解释，中文全部变成乱码并被原样存进报告。
			HANDLE logFile = CreateFileW(bugFilePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
				nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (logFile != INVALID_HANDLE_VALUE) {
				LARGE_INTEGER size{};
				std::string bytes;
				if (GetFileSizeEx(logFile, &size) && size.QuadPart > 0 && size.QuadPart < (16 << 20)) {
					bytes.resize(static_cast<size_t>(size.QuadPart));
					DWORD read = 0;
					if (!ReadFile(logFile, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr))
						bytes.clear();
					else
						bytes.resize(read);
				}
				CloseHandle(logFile);
				if (!bytes.empty()) {
					// 去掉 UTF-8 BOM，再按 UTF-8 解码；解不出来就退回 ACP。
					const bool hasBom = bytes.size() >= 3 &&
						static_cast<unsigned char>(bytes[0]) == 0xEF &&
						static_cast<unsigned char>(bytes[1]) == 0xBB &&
						static_cast<unsigned char>(bytes[2]) == 0xBF;
					const char* data = hasBom ? bytes.data() + 3 : bytes.data();
					const int dataSize = static_cast<int>(bytes.size() - (hasBom ? 3 : 0));
					UINT codePage = CP_UTF8;
					int wideLength = MultiByteToWideChar(CP_UTF8, 0, data, dataSize, nullptr, 0);
					if (wideLength <= 0) {
						codePage = CP_ACP;
						wideLength = MultiByteToWideChar(CP_ACP, 0, data, dataSize, nullptr, 0);
					}
					if (wideLength > 0) {
						std::wstring text(static_cast<size_t>(wideLength), L'\0');
						MultiByteToWideChar(codePage, 0, data, dataSize, text.data(), wideLength);
						SetDlgItemTextW(hDlg, IDC_BUGREPORT_CONTENT, text.c_str());
					}
				}
			}
		}

		return TRUE;
	}
	case WM_COMMAND: {
		switch (wParam)
		{
		case IDOK: {
			// 这个程序从来没有上报接口，原来的"已成功提交"只是 Sleep(2000) 后
			// 弹一句话。改成把内容真正存成文件，用户自己转给维护者。
			// 按实际长度取，不再固定 32K：超长日志原先会被静默截断，
			// 界面却仍然提示"已保存"。
			const int length = GetWindowTextLengthW(GetDlgItem(hDlg, IDC_BUGREPORT_CONTENT));
			std::wstring content(static_cast<size_t>(length) + 1, L'\0');
			GetDlgItemTextW(hDlg, IDC_BUGREPORT_CONTENT, content.data(), length + 1);
			content.resize(static_cast<size_t>(length));

			SYSTEMTIME now{};
			GetLocalTime(&now);
			wchar_t desktop[MAX_PATH]{};
			DWORD desktopLength = GetEnvironmentVariableW(L"USERPROFILE", desktop, _countof(desktop));
			std::wstring reportPath;
			if (desktopLength > 0 && desktopLength < _countof(desktop)) {
				reportPath = desktop;
				reportPath += L"\\Desktop";
			}
			else {
				reportPath = L".";
			}
			wchar_t fileName[128]{};
			swprintf_s(fileName, L"\\DzjsTrainer-bugreport-%04d%02d%02d-%02d%02d%02d.txt",
				now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
			reportPath += fileName;

			const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, nullptr, 0, nullptr, nullptr);
			bool saved = false;
			if (utf8Size > 0) {
				std::string utf8(static_cast<size_t>(utf8Size), '\0');
				WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, utf8.data(), utf8Size, nullptr, nullptr);
				HANDLE file = CreateFileW(reportPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
					FILE_ATTRIBUTE_NORMAL, nullptr);
				if (file != INVALID_HANDLE_VALUE) {
					DWORD written = 0;
					saved = WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size() - 1), &written, nullptr) != FALSE;
					CloseHandle(file);
				}
			}

			if (saved) {
				std::wstring message = L"错误报告已保存到：\n" + reportPath +
					L"\n\n请把该文件发送给维护者。";
				MessageBox(hDlg, message.c_str(), L"Dzjs Trainer", MB_ICONINFORMATION);
				ShellExecuteW(hDlg, L"open", reportPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
			}
			else {
				MessageBox(hDlg, L"保存错误报告失败，请检查桌面目录是否可写。", L"Dzjs Trainer", MB_ICONEXCLAMATION);
			}
			// 必须先读勾选状态再 EndDialog：对话框销毁后 IsDlgButtonChecked
			// 恒为 0，重启分支以前从来没执行过。
			const bool reboot = IsDlgButtonChecked(hDlg, IDC_REBOOT) == BST_CHECKED;
			EndDialog(hDlg, LOWORD(wParam));
			if (reboot)
				SysHlp::RunApplication(currentApp->GetFullPath(), L"-r3");
			return TRUE;
		}
		case IDCANCEL: {
			if (IsDlgButtonChecked(hDlg, IDC_REBOOT))
				SysHlp::RunApplication(currentApp->GetFullPath(), L"-r3");
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}		   
		default:
			break;
		}
		break;
	}
	default:
		break;
	}
	return FALSE;
}
