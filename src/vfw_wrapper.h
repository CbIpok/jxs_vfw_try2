#pragma once
#include <windows.h>
#include <vfw.h>
#include <string>

// Context for each compression session
struct CodecContext {
    class FFmpegProcess* proc;
    LONG width;
    LONG height;
    BYTE* outBuf;
    DWORD outBufSize;
};

class FFmpegProcess {
public:
    explicit FFmpegProcess(const std::wstring& ffmpegPath);
    ~FFmpegProcess();
    bool start(const std::wstring& args);
    void stop();
    HANDLE inputPipe() const;
    HANDLE outputPipe() const;
private:
    std::wstring ffmpegPath_;
    HANDLE hProcess_;
    HANDLE hStdinWrite_;
    HANDLE hStdoutRead_;
};

extern "C" {
    // Retrieve codec information
    BOOL VFWAPI ICInfo(DWORD fccType, DWORD fccHandler, ICINFO* lpicinfo);
    // Find codec for given formats
    HIC  VFWAPI ICLocate(DWORD fccType, DWORD fccHandler,
                          LPBITMAPINFOHEADER lpbiIn,
                          LPBITMAPINFOHEADER lpbiOut,
                          WORD wFlags);
    // Sequence compression
    BOOL    VFWAPI ICSeqCompressFrameStart(PCOMPVARS pc, LPBITMAPINFO lpbiIn);
    LPVOID  VFWAPI ICSeqCompressFrame(      PCOMPVARS pc,
                                             UINT      uiFlags,
                                             LPVOID    lpBits,
                                             BOOL*     pfKey,
                                             LONG*     plSize);
    void    VFWAPI ICSeqCompressFrameEnd(   PCOMPVARS pc);
}