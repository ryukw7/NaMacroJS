#include "stdafx.h"

#include "Common.h"
#include "BasicModule.h"

#include "Windows.h"

void NaBasicModule::Init(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate>& global_template)
{
#define ADD_GLOBAL_API(_js_func, _c_func) \
	global_template->Set( \
		v8::String::NewFromUtf8(isolate, #_js_func, v8::NewStringType::kNormal).ToLocalChecked(), \
		v8::FunctionTemplate::New(isolate, _c_func) \
	);

	ADD_GLOBAL_API(sleep, Sleep);
	ADD_GLOBAL_API(alert, Alert);
	ADD_GLOBAL_API(print, Print);
	ADD_GLOBAL_API(exit, Exit);
}

void NaBasicModule::Release()
{

}

// description: Print message to console
// syxtax:		print(message) 
void Print(V8_FUNCTION_ARGS)
{
	bool first = true;
	for (int i = 0; i < args.Length(); i++)
	{
		v8::HandleScope handle_scope(args.GetIsolate());
		if (first) {
			first = false;
		}
		else {
			printf(" ");
		}
		v8::String::Utf8Value str(args[i]);
		const char* cstr = ToCString(str);
		printf("%s", cstr);
	}
	printf("\n");
	fflush(stdout);
}

// description: show MessageBox with message
// syntax:		alert(message, title, type)
void Alert(V8_FUNCTION_ARGS)
{
	v8::String::Utf8Value msg(args[0]);
	v8::String::Utf8Value title(args[1]);
	int nType = args[2]->Int32Value();

	//const char* cstr = (const char*)*str;
	::MessageBoxA(NULL, 
		*msg, 
		args.Length() >= 2 ? *title : "Alert", 
		args.Length() >= 3 ? nType : MB_OK
		);
}

void Sleep(V8_FUNCTION_ARGS)
{
	int nTime = args[0]->Int32Value();

	::Sleep(args.Length() > 0 ? nTime : 1);
}

// Reads a file into a v8 string.
v8::Local<v8::String> ReadFile(v8::Isolate *isolate, const char* name)
{
	FILE* file = fopen(name, "rb");
	if (file == NULL) 
		return v8::Local<v8::String>();

	fseek(file, 0, SEEK_END);
	int size = ftell(file);
	rewind(file);

	char* chars = new char[size + 1];
	chars[size] = '\0';
	for (int i = 0; i < size;)
	{
		int read = static_cast<int>(fread(&chars[i], 1, size - i, file));
		i += read;
	}

	fclose(file);
	v8::Local<v8::String> result = v8::String::NewFromUtf8(isolate, chars, v8::NewStringType::kNormal/*, size*/).ToLocalChecked();
	delete[] chars;
	return result;
}

void Exit(V8_FUNCTION_ARGS)
{
	g_bExit = true;
}