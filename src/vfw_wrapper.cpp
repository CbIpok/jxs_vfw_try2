#include "vfw_wrapper.h"
#include <stdexcept>

FFmpegProcess::FFmpegProcess(const std::wstring& ffmpegPath)
    : ffmpegPath_(ffmpegPath), hProcess_(NULL), hStdinWrite_(NULL), hStdoutRead_(NULL) {}

FFmpegProcess::~FFmpegProcess() {
    stop();
}

bool FFmpegProcess::start(const std::wstring& args) {
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hStdinRead = NULL;
    HANDLE hStdoutWrite = NULL;
    if (!CreatePipe(&hStdinRead, &hStdinWrite_, &sa, 0) ||
        !CreatePipe(&hStdoutRead_, &hStdoutWrite, &sa, 0)) {
        return false;
    }

    PROCESS_INFORMATION pi = {};
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError  = hStdoutWrite;

    std::wstring cmd = L"\"" + ffmpegPath_ + L"\" " + args;
    if (!CreateProcessW(NULL, &cmd[0], NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        return false;
    }
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);
    hProcess_ = pi.hProcess;
    CloseHandle(pi.hThread);
    return true;
}

void FFmpegProcess::stop() {
    if (hProcess_) {
        TerminateProcess(hProcess_, 0);
        CloseHandle(hProcess_);
        hProcess_ = NULL;
    }
    if (hStdinWrite_)  { CloseHandle(hStdinWrite_); hStdinWrite_ = NULL; }
    if (hStdoutRead_) { CloseHandle(hStdoutRead_); hStdoutRead_ = NULL; }
}

HANDLE FFmpegProcess::inputPipe()  { return hStdinWrite_; }
HANDLE FFmpegProcess::outputPipe() { return hStdoutRead_; }

// Stub implementations for VFW exports
extern "C" {

WINBOOL WINAPI ICInfo(DWORD fccType, DWORD fccHandler, ICINFO* lpicinfo) {
    // TODO: fill lpicinfo with codec metadata
    return TRUE;
}

HIC WINAPI ICLocate(DWORD fccType, DWORD fccHandler,
                     LPBITMAPINFOHEADER lpbiIn,
                     LPBITMAPINFOHEADER lpbiOut,
                     WORD wFlags) {
    // TODO: match fccType/fccHandler and return handle/context
    return reinterpret_cast<HIC>(1);
}

WINBOOL WINAPI ICSeqCompressFrameStart(PCOMPVARS pc, LPBITMAPINFO lpbiIn) {
    // TODO: initialize compression sequence
    return TRUE;
}

LPVOID WINAPI ICSeqCompressFrame(PCOMPVARS pc,
                                 UINT uiFlags,
                                 LPVOID lpBits,
                                 WINBOOL* pfKey,
                                 LONG* plSize) {
    // TODO: send raw frame via FFmpegProcess, read compressed data
    *pfKey = TRUE;
    *plSize = 0;
    return nullptr;
}

void WINAPI ICSeqCompressFrameEnd(PCOMPVARS pc) {
    // TODO: finalize sequence
}

} // extern C