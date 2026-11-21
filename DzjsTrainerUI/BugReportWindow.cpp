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
			CHAR szTest[1000];
			memset(szTest, 0, sizeof(szTest));
			std::string bugReportContent;
			int len = 0;
			FILE *fp = NULL;
			_wfopen_s(&fp, bugFilePath, L"r");
			if (NULL != fp)
			{
				while (fgets(szTest, 1000, fp) != NULL)
				{
					bugReportContent += szTest;
					memset(szTest, 0, sizeof(szTest));
				}
				fclose(fp);

				SetDlgItemTextA(hDlg, IDC_BUGREPORT_CONTENT, bugReportContent.c_str());
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
			std::wstring content(32768, L'\0');
			const int length = GetDlgItemTextW(hDlg, IDC_BUGREPORT_CONTENT, content.data(),
				static_cast<int>(content.size()));
			content.resize(length > 0 ? static_cast<size_t>(length) : 0);

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
			EndDialog(hDlg, LOWORD(wParam));
			if (IsDlgButtonChecked(hDlg, IDC_REBOOT))
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
