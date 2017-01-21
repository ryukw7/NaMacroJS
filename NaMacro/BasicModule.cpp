#include "stdafx.h"

#include "Common.h"
#include "BasicModule.h"

#include "Windows.h"
#include "NaWindow.h"
#include "NaControl.h"
#include "NaImage.h"
#include "NaFile.h"

#include "../NaLib/NaMessageBox.h"
#include "../NaLib/NaNotifyWindow.h"

#include <iostream>

NaWindow* NaBasicModule::s_pTimerWindow = NULL;
std::map<int, Persistent<Function, CopyablePersistentTraits<Function>>> NaBasicModule::s_mapIntervalCallback;
std::map<int, Persistent<Function, CopyablePersistentTraits<Function>>> NaBasicModule::s_mapTimeoutCallback;
int NaBasicModule::s_nTimerID = 1; // begin from 0 causes hard to store variable

void NaBasicModule::Create(Isolate * isolate, Local<ObjectTemplate>& global_template)
{
#define ADD_GLOBAL_METHOD(_js_func, _c_func)	ADD_TEMPLATE_METHOD(global_template, _js_func, _c_func);

	// methods
	ADD_GLOBAL_METHOD(include,		Include);
	ADD_GLOBAL_METHOD(sleep,		Sleep);
	ADD_GLOBAL_METHOD(setInterval,	SetInterval);
	ADD_GLOBAL_METHOD(clearInterval, ClearInterval);
	ADD_GLOBAL_METHOD(setTimeout,	SetTimeout);
	ADD_GLOBAL_METHOD(alert,		Alert);
	ADD_GLOBAL_METHOD(confirm,		Confirm);
	ADD_GLOBAL_METHOD(prompt,		Prompt);
	ADD_GLOBAL_METHOD(print,		Print);
	ADD_GLOBAL_METHOD(trace,		Print);
	ADD_GLOBAL_METHOD(notify,		Notify);
	ADD_GLOBAL_METHOD(exit,			Exit);
	ADD_GLOBAL_METHOD(getWindow,	GetWindow);
	ADD_GLOBAL_METHOD(getActiveWindow, GetActiveWindow);
	ADD_GLOBAL_METHOD(findWindows,	FindWindows);
	ADD_GLOBAL_METHOD(findProcesses, FindProcesses);
	ADD_GLOBAL_METHOD(findTrays,	FindTrays);
}

void NaBasicModule::Init(Isolate * isolate, Local<ObjectTemplate>& global_template)
{
	// add global object
	Local<Object> global = isolate->GetCurrentContext()->Global();
	{
		// Init ConsoleWindow
		Local<String> consolewindow_name = String::NewFromUtf8(isolate, "consoleWindow", NewStringType::kNormal).ToLocalChecked();
		Local<Value> consolewindow_value = global->Get(consolewindow_name);
		if (!consolewindow_value.IsEmpty() && consolewindow_value->IsUndefined())
		{
			NaWindow *pWindow = new NaWindow(0, NA_WINDOW_CONSOLE);
			Local<Object> consolewindow_object = NaWindow::WrapObject(isolate, pWindow);
			consolewindow_value = consolewindow_object;

			global->Set(consolewindow_name, consolewindow_value);
		}
	}

	{
		// Init Window class
		Local<String> window_name = String::NewFromUtf8(isolate, "Window", NewStringType::kNormal).ToLocalChecked();
		Local<Value> window_value = global->Get(window_name);
		if (!window_value.IsEmpty() && window_value->IsUndefined())
		{
			Local<FunctionTemplate> templ = FunctionTemplate::New(isolate, NaWindow::Constructor);
			/*
			Local<ObjectTemplate> obj_templ = templ->PrototypeTemplate();
			obj_templ->Set(
				window_name,
				FunctionTemplate::New(isolate, NaWindow::Constructor)->GetFunction()
			);

			global->Set(window_name, window_value);
			*/

			global->Set(window_name, templ->GetFunction());
		}

		// Init Control class
		/*
		Local<String> control_name = String::NewFromUtf8(isolate, "Control", NewStringType::kNormal).ToLocalChecked();
		Local<Value> control_value = global->Get(control_name);
		if (!control_value.IsEmpty() && control_value->IsUndefined())
		{
			Local<FunctionTemplate> templ = FunctionTemplate::New(isolate, NaControl::Constructor);
			global->Set(control_name, templ->GetFunction());
		}
		*/

		// Init Image class
		Local<String> image_name = String::NewFromUtf8(isolate, "Image", NewStringType::kNormal).ToLocalChecked();
		Local<Value> image_value = global->Get(image_name);
		if (!image_value.IsEmpty() && image_value->IsUndefined())
		{
			Local<FunctionTemplate> templ = FunctionTemplate::New(isolate, NaImage::Constructor);
			global->Set(image_name, templ->GetFunction());
		}

		// Init File class
		Local<String> file_name = String::NewFromUtf8(isolate, "File", NewStringType::kNormal).ToLocalChecked();
		Local<Value> file_value = global->Get(file_name);
		if (!file_value.IsEmpty() && file_value->IsUndefined())
		{
			Local<FunctionTemplate> templ = FunctionTemplate::New(isolate, NaFile::Constructor);
			global->Set(file_name, templ->GetFunction());
		}
	}

	{
		// Create Timer Window
		s_pTimerWindow = new NaWindow(NULL, NA_WINDOW_INTERNAL);
		s_pTimerWindow->Create();
	}
}

void NaBasicModule::Release()
{
	// Destroy Console Window

	// Destroy Timer Window
	if (s_pTimerWindow != NULL)
	{
		s_pTimerWindow->Destroy();

		delete s_pTimerWindow;
		s_pTimerWindow = NULL;
	}
}

void NaBasicModule::OnTimer(Isolate *isolate, int nTimerID)
{
	std::map <int, Persistent<Function, CopyablePersistentTraits<Function>>>::iterator it;
	it = NaBasicModule::s_mapIntervalCallback.find(nTimerID);
	if (it != NaBasicModule::s_mapIntervalCallback.end())
	{
		Persistent<Function, CopyablePersistentTraits<Function>> percy = it->second;
		Local<Function> callback = Local<Function>::New(isolate, percy);
		if (!callback.IsEmpty())
		{
			Local<Value> recv = isolate->GetCurrentContext()->Global();
			callback->Call(
				isolate->GetCurrentContext(),
				recv,
				0,
				NULL
			);
		}
	}

	it = NaBasicModule::s_mapTimeoutCallback.find(nTimerID);
	if (it != NaBasicModule::s_mapTimeoutCallback.end())
	{
		Persistent<Function, CopyablePersistentTraits<Function>> percy = it->second;
		Local<Function> callback = Local<Function>::New(isolate, percy);
		if (!callback.IsEmpty())
		{
			Local<Value> recv = isolate->GetCurrentContext()->Global();
			callback->Call(
				isolate->GetCurrentContext(),
				recv,
				0,
				NULL
			);
		}

		::KillTimer(NaBasicModule::s_pTimerWindow->m_hWnd, nTimerID);

		NaBasicModule::s_mapIntervalCallback.erase(nTimerID);
	}
}

// description: Include external source file
// syntax:		include(filename);
void NaBasicModule::Include(V8_FUNCTION_ARGS)
{
	Isolate *isolate = args.GetIsolate();
	Local<Context> context = isolate->GetCurrentContext();

	for (int i = 0; i < args.Length(); i++)
	{
		Local<String> script_source;
		MaybeLocal<String> script_name;

		String::Value arg_value(args[i]);
		wchar_t *wstr = (wchar_t*)(*arg_value);

		// converting relative path
		Local<StackTrace> stack_trace = StackTrace::CurrentStackTrace(isolate, 1, StackTrace::kScriptName);
		Local<StackFrame> cur_frame = stack_trace->GetFrame(0);
		NaString strBase(cur_frame->GetScriptName());

		//
		// 2016.07.27
		// GetScriptName Unicode Issue:
		// Wrong Conversion if Special character included.
		//
		NaDebugOut(L"================================================\n");
		NaDebugOut(L"Include src: %s\n", wstr);
		NaDebugOut(L"Include base: %s\n", strBase.wstr());

		NaString strUrl(wstr);
		NaUrl url;
		url.SetBase(strBase);
		url.SetUrl(strUrl);

		NaString strFullUrl(url.GetFullUrl());
		NaDebugOut(L"Include full: %s\n", strFullUrl.wstr());

		script_source = ReadFile(isolate, strFullUrl.cstr());
		script_name = String::NewFromUtf8(isolate, strFullUrl.cstr(), NewStringType::kNormal);
		if (script_source.IsEmpty())
		{
			NaDebugOut(L"Error reading '%s'\n", strFullUrl.wstr());

			// TODO ThrowException
			return;
		}

		Local<Script> script;
		{
			TryCatch try_catch(isolate);
			ScriptOrigin origin(script_name.ToLocalChecked());
			Script::Compile(context, script_source, &origin).ToLocal(&script);
			if (script.IsEmpty())
			{
				if (g_bReportExceptions)
					ReportException(isolate, &try_catch);

				NaDebugOut(L"included script is empty!\n");
				return;
			}
		}

		{
			TryCatch try_catch(isolate);
			script->Run(context);
			if (try_catch.HasCaught())
			{
				if (g_bReportExceptions)
					ReportException(isolate, &try_catch);
				return;
			}
		}
	}
}

// description: Print message to console
// syxtax:		print(message)
void NaBasicModule::Print(V8_FUNCTION_ARGS)
{
	bool first = true;
	for (int i = 0; i < args.Length(); i++)
	{
		HandleScope handle_scope(args.GetIsolate());
		if (first) {
			first = false;
		}
		else {
			printf(" ");
		}

		String::Value str(args[i]);
		wprintf(_T("%s"), (const wchar_t*)*str);
	}
	printf("\n");
	fflush(stdout);
}

// description: Notify message via notify window
// syxtax:		notify(message)
void NaBasicModule::Notify(V8_FUNCTION_ARGS)
{
	String::Value message(args[0]);
	NaString strMessage((wchar_t*)*message);
	NaString strTitle = L"NaMacro";

	if (args.Length() > 1)
	{
		String::Value title(args[1]);
		strTitle = (wchar_t*)*message;
	}

	NaNotifyWindow::AddNotifyWindow(strMessage, strTitle);
}

// description: show MessageBox with message
// syntax:		alert(message, title, type)
void NaBasicModule::Alert(V8_FUNCTION_ARGS)
{
	String::Value msg(args[0]);
	String::Value title(args[1]);
	int nType = args[2]->Int32Value();

	int nRet = ::MessageBoxW(NULL,
		(const wchar_t*)*msg,
		args.Length() >= 2 ? (const wchar_t*)*title : L"Alert",
		args.Length() >= 3 ? nType : MB_OK
		);

	Isolate *isolate = args.GetIsolate();
	args.GetReturnValue().Set(
		Integer::New(isolate, nRet)
		);
}

// description: show MessageBox with message
// syntax:		confirm(message, title)
void NaBasicModule::Confirm(V8_FUNCTION_ARGS)
{
	String::Value msg(args[0]);
	String::Value title(args[1]);

	int nRet = ::MessageBoxW(NULL,
		(const wchar_t*)*msg,
		args.Length() >= 2 ? (const wchar_t*)*title : L"Confirm",
		MB_OKCANCEL
	);

	bool bRet = false;
	switch (nRet)
	{
	case IDOK:
		bRet = true;
		break;
	case IDCANCEL:
	default:
		break;
	}

	Isolate *isolate = args.GetIsolate();
	args.GetReturnValue().Set(
		//Integer::New(isolate, nRet)
		Boolean::New(isolate, bRet)
	);
}

// description: show MessageBox with message
// syntax:		prompt(message, title[, defaultmessage])
void NaBasicModule::Prompt(V8_FUNCTION_ARGS)
{
	String::Value msg(args[0]);
	String::Value title(args[1]);
	String::Value default_string(args[2]);

	NaMessageBox MsgBox;
	NaString strRet =
		MsgBox.DoModal(NULL,
			(wchar_t*)*msg,
			args.Length() >= 2 ? (const wchar_t*)*title : L"Prompt",
			args.Length() >= 3 ? (const wchar_t*)*default_string : L""
		);

	Isolate *isolate = args.GetIsolate();
	args.GetReturnValue().Set(
		String::NewFromUtf8(isolate, strRet.cstr(), NewStringType::kNormal, strRet.GetLength()).ToLocalChecked()
	);
}

// description: suspends the excution script
// syntax:		sleep(time)
// param(time):	time interval in milliseconds
void NaBasicModule::Sleep(V8_FUNCTION_ARGS)
{
	int nTime = args[0]->Int32Value();

	::Sleep(args.Length() > 0 ? nTime : 1);
}

// description:
// syntax:		setInterval(interval, callback_function) : timer_id
void NaBasicModule::SetInterval(V8_FUNCTION_ARGS)
{
	if (args.Length() < 2)
		return;

	int nTime = args[0]->Int32Value();
	Local<Object> callback = args[1]->ToObject();
	if (callback->IsFunction())
	{
		int nTimerID = NaBasicModule::s_nTimerID++;
		::SetTimer(s_pTimerWindow->m_hWnd, nTimerID, nTime, NULL);

		Isolate *isolate = args.GetIsolate();
		Local<Function> callback_func = Local<Function>::Cast(args[1]);
		Persistent<Function, CopyablePersistentTraits<Function>> percy(isolate, callback_func);

		NaBasicModule::s_mapIntervalCallback.insert(
			std::pair<int, Persistent<Function, CopyablePersistentTraits<Function>>>(nTimerID, percy)
		);

		args.GetReturnValue().Set(
			Integer::New(isolate, nTimerID)
		);
	}
}

// description:
// syntax:		clearInterval(timer_id)
void NaBasicModule::ClearInterval(V8_FUNCTION_ARGS)
{
	if (args.Length() < 1)
		return;

	int nTimerID = args[0]->Int32Value();
	std::map <int, Persistent<Function, CopyablePersistentTraits<Function>>>::iterator it;
	it = NaBasicModule::s_mapIntervalCallback.find(nTimerID);
	if (it != NaBasicModule::s_mapIntervalCallback.end())
	{
		::KillTimer(NaBasicModule::s_pTimerWindow->m_hWnd, nTimerID);

		NaBasicModule::s_mapIntervalCallback.erase(nTimerID);
	}
}

// description:
// syntax:
void NaBasicModule::SetTimeout(V8_FUNCTION_ARGS)
{
	if (args.Length() < 2)
		return;

	int nTime = args[0]->Int32Value();
	Local<Object> callback = args[1]->ToObject();
	if (callback->IsFunction())
	{
		int nTimerID = NaBasicModule::s_nTimerID++;
		::SetTimer(s_pTimerWindow->m_hWnd, nTimerID, nTime, NULL);

		Isolate *isolate = args.GetIsolate();
		Local<Function> callback_func = Local<Function>::Cast(args[1]);
		Persistent<Function, CopyablePersistentTraits<Function>> percy(isolate, callback_func);

		NaBasicModule::s_mapTimeoutCallback.insert(
			std::pair<int, Persistent<Function, CopyablePersistentTraits<Function>>>(nTimerID, percy)
		);

		args.GetReturnValue().Set(
			Integer::New(isolate, nTimerID)
		);
	}
}

// description: exit current event loop
// syntax:		exit()
void NaBasicModule::Exit(V8_FUNCTION_ARGS)
{
	g_bExit = true;
}

// description: pick a window from point
// syntax:		getWindow(x, y) : window object
void NaBasicModule::GetWindow(V8_FUNCTION_ARGS)
{
	Isolate *isolate = args.GetIsolate();
	int x = args[0]->Int32Value();
	int y = args[1]->Int32Value();

	NaWindow *pWindow = NaWindow::GetWindow(x, y);
	Local<Object> result = NaWindow::WrapObject(isolate, pWindow);

	args.GetReturnValue().Set(result);
}

// description: get active window
// syntax:		getActiveWindow() : window object
void NaBasicModule::GetActiveWindow(V8_FUNCTION_ARGS)
{
	Isolate *isolate = args.GetIsolate();

	NaWindow *pWindow = NaWindow::GetActiveWindow();;
	Local<Object> result = NaWindow::WrapObject(isolate, pWindow);

	args.GetReturnValue().Set(result);
}

// description: find windows which contains specific text
// syntax:		findWindows(text)
void NaBasicModule::FindWindows(V8_FUNCTION_ARGS)
{
	Isolate *isolate = args.GetIsolate();
	String::Value str(args[0]);
	Local<Array> results = Array::New(isolate);

	NaWindow::FindWindows(isolate, (const wchar_t*)*str, results);
	args.GetReturnValue().Set(results);
}

// description:
// syntax:
void NaBasicModule::FindProcesses(V8_FUNCTION_ARGS)
{
	// Not Impl
	Isolate *isolate = args.GetIsolate();
	Local<String> result = String::NewFromUtf8(isolate, "NotImpl", NewStringType::kNormal, 7).ToLocalChecked();
	args.GetReturnValue().Set(result);
}

// description:
// syntax:
void NaBasicModule::FindTrays(V8_FUNCTION_ARGS)
{
	// Not Impl
	Isolate *isolate = args.GetIsolate();
	Local<String> result = String::NewFromUtf8(isolate, "NotImpl", NewStringType::kNormal, 7).ToLocalChecked();
	args.GetReturnValue().Set(result);
}
