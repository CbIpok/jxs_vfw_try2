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

//— новое: конфигурация путей —-------------------------------------------------
/// Установить рабочую директорию (куда будут писаться *.raw / *.mkv).
/// Принимает null-terminated wide-строку. Если dir == nullptr или пусто — игнор.
void WINAPI JXSVFW_SetBaseDir(const wchar_t* dir);

/// Установить полный путь к ffmpeg.exe.
/// Принимает null-terminated wide-строку. Если path == nullptr или пусто — игнор.
void WINAPI JXSVFW_SetFFmpegPath(const wchar_t* path);
}
//---------------------------------------------------------------------------//
