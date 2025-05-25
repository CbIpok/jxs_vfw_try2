//-------C:\Users\dmitrienko\CLionProjects\untitled\src\vfw_wrapper.h--------
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
    std::vector<BYTE>        outBuf;       // буфер фрагмента контейнера
    std::filesystem::path    baseDir;      // рабочая папка

    // FFmpeg-процесс и каналы
    HANDLE                   ffmpegStdin  = INVALID_HANDLE_VALUE;
    HANDLE                   ffmpegStdout = INVALID_HANDLE_VALUE;
    HANDLE                   ffmpegProc   = nullptr;
};

//------------------------- VFW экспорт ----------------------------------------
extern "C" {

    //— стандартные VFW-функции —---------------------------------------------------
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

    //— новые: конфигурация путей —-------------------------------------------------
    /// Установить рабочую директорию (куда будут писаться *.raw / *.mkv).
    void WINAPI JXSVFW_SetBaseDir(const wchar_t* dir);

    /// Установить полный путь к ffmpeg.exe.
    void WINAPI JXSVFW_SetFFmpegPath(const wchar_t* path);
}
