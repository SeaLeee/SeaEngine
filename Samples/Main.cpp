#include "SampleApp.h"
#include <Windows.h>
#include <iostream>
#include <DbgHelp.h>
#include <sstream>

#pragma comment(lib, "DbgHelp.lib")

// 打印堆栈信息
void PrintStackTrace()
{
    void* stack[64];
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);
    
    WORD frames = CaptureStackBackTrace(0, 64, stack, NULL);
    
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    
    std::cout << "\n=== Stack Trace ===" << std::endl;
    for (WORD i = 0; i < frames; i++)
    {
        SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
        std::cout << i << ": " << symbol->Name << " - 0x" << std::hex << symbol->Address << std::dec << std::endl;
    }
    std::cout << "===================" << std::endl;
    
    free(symbol);
}

// 暂停等待输入
void PauseAndWait(const char* message)
{
    std::cout << "\n" << message << std::endl;
    std::cout << "Press Enter to continue..." << std::endl;
    std::cin.get();
}

// 未处理异常过滤器
LONG WINAPI MyUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
{
    std::cerr << "\n!!! UNHANDLED EXCEPTION !!!" << std::endl;
    std::cerr << "Exception Code: 0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionCode << std::dec << std::endl;
    std::cerr << "Exception Address: 0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionAddress << std::dec << std::endl;
    
    PrintStackTrace();
    
    PauseAndWait("Application crashed!");
    
    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 分配控制台窗口用于查看日志
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);
    
    // 设置未处理异常过滤器
    SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);
    
    std::cout << "=== SeaEngine Starting ===" << std::endl;
    std::cout << "Working directory: " << std::filesystem::current_path().string() << std::endl;
    std::cout << std::endl;
    
    try
    {
        Sea::SampleApp app;
        std::cout << ">>> SampleApp created, calling Run()..." << std::endl;
        app.Run();
        std::cout << ">>> Run() returned normally" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n!!! C++ EXCEPTION !!!" << std::endl;
        std::cerr << "What: " << e.what() << std::endl;
        PrintStackTrace();
        PauseAndWait("Exception caught!");
    }
    catch (...)
    {
        std::cerr << "\n!!! UNKNOWN EXCEPTION !!!" << std::endl;
        PrintStackTrace();
        PauseAndWait("Unknown exception caught!");
    }
    
    std::cout << "\n=== SeaEngine Exiting ===" << std::endl;
    PauseAndWait("Program finished");
    
    FreeConsole();
    return 0;
}
