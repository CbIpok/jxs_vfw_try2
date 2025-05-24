#pragma once

#include <windows.h>
#include <vfw.h>
#include <string>

// Simple wrapper for launching ffmpeg.exe
class FFmpegProcess {
public:
    FFmpegProcess(const std::wstring& ffmpegPath);
    ~FFmpegProcess();

    bool start(const std::wstring& args);
    void stop();

    HANDLE inputPipe();
    HANDLE outputPipe();

private:
    std::wstring ffmpegPath_;
    HANDLE hProcess_;
    HANDLE hStdinWrite_;
    HANDLE hStdoutRead_;
};

// VFW callback exports matching vfw.h signatures
extern "C" {

    WINBOOL WINAPI ICInfo(DWORD fccType, DWORD fccHandler, ICINFO* lpicinfo);
    HIC    WINAPI ICLocate(DWORD fccType, DWORD fccHandler,
                           LPBITMAPINFOHEADER lpbiIn,
                           LPBITMAPINFOHEADER lpbiOut,
                           WORD wFlags);

    WINBOOL WINAPI ICSeqCompressFrameStart(PCOMPVARS pc, LPBITMAPINFO lpbiIn);
    LPVOID  WINAPI ICSeqCompressFrame(      PCOMPVARS pc,
                                            UINT      uiFlags,
                                            LPVOID    lpBits,
                                            WINBOOL*  pfKey,
                                            LONG*     plSize);
    void    WINAPI ICSeqCompressFrameEnd(    PCOMPVARS pc);

} // extern C