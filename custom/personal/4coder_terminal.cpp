#pragma once
#include "4coder_default_include.cpp"

#pragma warning(disable:4042)
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>

HANDLE g_hChildStd_IN_Rd = NULL;
HANDLE g_hChildStd_IN_Wr = NULL;
HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;

global Buffer_ID terminal_buffer_id;
global Buffer_ID terminal_view_id;


void ReadFromPipe(Application_Links* app);
void terminal_setup(Application_Links *app)
{
    terminal_buffer_id = create_buffer(app, str8_lit("*terminal*"), BufferCreate_NeverAttachToFile);
    terminal_view_id = open_view(app, get_active_view(app, Access_Always), ViewSplit_Right);

    buffer_set_setting(app, terminal_buffer_id, BufferSetting_Terminal, 1);

    view_set_buffer(app, terminal_view_id, terminal_buffer_id, 0);

    // Scratch_Block scratch(app);
    // String_Const_u8 hot = push_hot_directory(app, scratch);
    // auto pid = create_child_process(app, hot, str8_lit("nu"));

    // child_process_set_target_buffer(app, pid, terminal_buffer_id, 0);

    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};

    SECURITY_ATTRIBUTES saAttr = {0};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0);
    SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);
    CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0);
    SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0);

    si.cb = sizeof(STARTUPINFO);
    si.hStdError = g_hChildStd_OUT_Wr;
    si.hStdOutput = g_hChildStd_OUT_Wr;
    si.hStdInput = g_hChildStd_IN_Rd;
    // si.dwFlags |= STARTF_USESTDHANDLES;

    CreateProcess(nullptr, TEXT("nu"), nullptr, nullptr, false, 0, nullptr, nullptr, &si, &pi);
    // ReadFromPipe(app);
}


void foobar()
{
}

void ReadFromPipe(Application_Links* app)
{
    #define BUFSIZE 2048
    DWORD dwRead;
    CHAR chBuf[BUFSIZE];
    BOOL bSuccess = FALSE;
    // HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

    for (;;)
    {
        bSuccess = ReadFile(g_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL);
        if (!bSuccess || dwRead == 0)
            break;

        Range_i64 range = buffer_range(app, terminal_buffer_id);
        String_Const_u8 str;
        str.str = (u8*)chBuf;
        str.size = (u64)dwRead;
        buffer_replace_range(app, terminal_buffer_id, { range.end, range.end }, str);

        // bSuccess = WriteFile(hParentStdOut, chBuf,
        //                      dwRead, &dwWritten, NULL);
        if (!bSuccess)
            break;
    }
}
