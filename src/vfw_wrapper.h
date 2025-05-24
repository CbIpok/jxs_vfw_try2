//-------C:\Users\dmitrienko\CLionProjects\untitled\src\vfw_wrapper.h--------//
#pragma once

#include <windows.h>
#include <vfw.h>
#include <string>
#include <vector>
#include <filesystem>

//------------------------------------------------------------------------------
//  RAII-контекст кодека
//------------------------------------------------------------------------------
struct CodecContext
{
    int                      width{};
    int                      height{};
    std::vector<BYTE>        outBuf;      // буфер кодированного потока
    std::filesystem::path    baseDir;     // рабочая папка для *.raw / *.mkv
};

//------- VFW экспорт ----------------------------------------------------------
extern "C" {
    BOOL    VFWAPI ICInfo               (DWORD fccType, DWORD fccHandler, ICINFO *lpicinfo);
    HIC     VFWAPI ICLocate             (DWORD fccType, DWORD fccHandler,
                                         LPBITMAPINFOHEADER lpbiIn,
                                         LPBITMAPINFOHEADER lpbiOut,
                                         WORD wFlags);
    BOOL    VFWAPI ICSeqCompressFrameStart(PCOMPVARS pc, LPBITMAPINFO lpbiIn);
    LPVOID  VFWAPI ICSeqCompressFrame   (PCOMPVARS pc, UINT uiFlags,
                                         LPVOID lpBits, BOOL *pfKey, LONG *plSize);
    void    VFWAPI ICSeqCompressFrameEnd(PCOMPVARS pc);

    STDAPI  DllRegisterServer();
    STDAPI  DllUnregisterServer();
}
