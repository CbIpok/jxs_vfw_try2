#pragma once

#include <windows.h>
#include <vfw.h>
#include <string>

class FFmpegProcess {
public:
    FFmpegProcess(const std::wstring &ffmpegPath);
    ~FFmpegProcess();

    bool start(const std::wstring &args);
    void stop();
    HANDLE inputPipe() const;
    HANDLE outputPipe() const;

private:
    std::wstring ffmpegPath_;
    HANDLE hProcess_;
    HANDLE hStdinWrite_;
    HANDLE hStdoutRead_;
};

struct CodecContext {
    FFmpegProcess* proc;
    int            width;
    int            height;
    DWORD          outBufSize;
    BYTE*          outBuf;
};

extern "C" {
    BOOL    VFWAPI ICInfo(DWORD fccType, DWORD fccHandler, ICINFO *lpicinfo);
    HIC     VFWAPI ICLocate(DWORD fccType, DWORD fccHandler,
                             LPBITMAPINFOHEADER lpbiIn,
                             LPBITMAPINFOHEADER lpbiOut,
                             WORD wFlags);
    BOOL    VFWAPI ICSeqCompressFrameStart(PCOMPVARS pc, LPBITMAPINFO lpbiIn);
    LPVOID  VFWAPI ICSeqCompressFrame(PCOMPVARS pc,
                                      UINT uiFlags,
                                      LPVOID lpBits,
                                      BOOL *pfKey,
                                      LONG *plSize);
    void    VFWAPI ICSeqCompressFrameEnd(PCOMPVARS pc);
    STDAPI  DllRegisterServer();
    STDAPI  DllUnregisterServer();
}
