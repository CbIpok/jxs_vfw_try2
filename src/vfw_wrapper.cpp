// src/vfw_wrapper.cpp
#include "vfw_wrapper.h"
#include <sstream>
#include <iostream>

// — FFmpegProcess —

FFmpegProcess::FFmpegProcess(const std::wstring &ffmpegPath)
  : ffmpegPath_(ffmpegPath),
    hProcess_(NULL),
    hStdinWrite_(NULL),
    hStdoutRead_(NULL)
{}

FFmpegProcess::~FFmpegProcess() {
    stop();
}

bool FFmpegProcess::start(const std::wstring &args) {
    // Create pipes for stdin/stdout
    SECURITY_ATTRIBUTES sa{ sizeof(sa), NULL, TRUE };
    HANDLE hStdinRead  = NULL;
    HANDLE hStdoutWrite = NULL;

    if (!CreatePipe(&hStdinRead, &hStdinWrite_, &sa, 0) ||
        !CreatePipe(&hStdoutRead_, &hStdoutWrite, &sa, 0)) {
        std::cerr << "Failed to create pipes\n";
        return false;
    }

    PROCESS_INFORMATION pi{};
    STARTUPINFOW si{};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES;
    si.hStdInput   = hStdinRead;
    si.hStdOutput  = hStdoutWrite;
    si.hStdError   = hStdoutWrite;

    // Build a mutable command-line buffer
    std::wstring cmdLine = L"\"";
    cmdLine += ffmpegPath_;
    cmdLine += L"\" ";
    cmdLine += args;

    // Launch ffmpeg:
    if (!CreateProcessW(
            ffmpegPath_.c_str(),  // explicit application name
            &cmdLine[0],          // command-line (must be writable)
            NULL, NULL,
            TRUE,                 // inherit handles
            0,                    // no special flags
            NULL, NULL,           // env + cwd
            &si,
            &pi
        ))
    {
        DWORD err = GetLastError();
        std::cerr << "CreateProcessW failed, error=" << err << "\n";
        CloseHandle(hStdinRead);
        CloseHandle(hStdoutWrite);
        return false;
    }

    // We don't need these in the parent
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);

    hProcess_ = pi.hProcess;
    CloseHandle(pi.hThread);
    return true;
}

void FFmpegProcess::stop() {
    if (hStdinWrite_) {
        CloseHandle(hStdinWrite_);
        hStdinWrite_ = NULL;
    }
    if (hProcess_) {
        WaitForSingleObject(hProcess_, INFINITE);
        CloseHandle(hProcess_);
        hProcess_ = NULL;
    }
    if (hStdoutRead_) {
        CloseHandle(hStdoutRead_);
        hStdoutRead_ = NULL;
    }
}

HANDLE FFmpegProcess::inputPipe()  const { return hStdinWrite_; }
HANDLE FFmpegProcess::outputPipe() const { return hStdoutRead_; }


// — VFW callbacks —

extern "C" {

BOOL VFWAPI ICInfo(DWORD fccType, DWORD fccHandler, ICINFO *lpicinfo) {
    if (!lpicinfo) return FALSE;
    ZeroMemory(lpicinfo, sizeof(ICINFO));
    lpicinfo->dwSize     = sizeof(ICINFO);
    lpicinfo->fccType    = ICTYPE_VIDEO;
    lpicinfo->fccHandler = mmioFOURCC('J','X','S','F');
    lpicinfo->dwFlags    = 0;
    lpicinfo->dwVersion  = 1;
    lpicinfo->szName[0]       =
    lpicinfo->szDescription[0] = '\0';
    return TRUE;
}

HIC VFWAPI ICLocate(DWORD fccType, DWORD fccHandler,
                    LPBITMAPINFOHEADER lpbiIn,
                    LPBITMAPINFOHEADER lpbiOut,
                    WORD wFlags)
{
    if ((wFlags & ICMODE_COMPRESS) &&
        lpbiIn  == nullptr &&
        lpbiOut == nullptr)
    {
        // Enumerator ping
        return reinterpret_cast<HIC>(1);
    }

    if (fccType    != ICTYPE_VIDEO ||
        fccHandler != mmioFOURCC('J','X','S','F'))
        return NULL;

    if (lpbiIn->biCompression != BI_RGB ||
        lpbiIn->biBitCount   != 24)
        return NULL;

    // Advertise MJPEG
    lpbiOut->biSize        = sizeof(BITMAPINFOHEADER);
    lpbiOut->biWidth       = lpbiIn->biWidth;
    lpbiOut->biHeight      = lpbiIn->biHeight;
    lpbiOut->biPlanes      = 1;
    lpbiOut->biBitCount    = 24;
    lpbiOut->biCompression = mmioFOURCC('M','J','P','G');
    lpbiOut->biSizeImage   = 0;

    return reinterpret_cast<HIC>(1);
}

BOOL VFWAPI ICSeqCompressFrameStart(PCOMPVARS pc, LPBITMAPINFO lpbiIn) {
    CodecContext* ctx = new CodecContext;
    ctx->width      = lpbiIn->bmiHeader.biWidth;
    ctx->height     = lpbiIn->bmiHeader.biHeight;
    ctx->outBufSize = 1024 * 1024;
    ctx->outBuf     = new BYTE[ctx->outBufSize];
    pc->lpState     = ctx;

    // Build ffmpeg arguments
    std::wstringstream ss;
    ss << L"-f rawvideo -pix_fmt bgr24 -s "
       << ctx->width << L"x" << ctx->height
       << L" -i pipe:0 -vcodec mjpeg -f avi pipe:1 "
          L"-nostdin -hide_banner";

    // NOTE: Update this path to where *your* ffmpeg.exe lives:
    ctx->proc = new FFmpegProcess(
        L"C:/dmitrienkomy/cpp/jxs_ffmpeg/install-dir/bin/ffmpeg.exe"
    );
    return ctx->proc->start(ss.str());
}

LPVOID VFWAPI ICSeqCompressFrame(PCOMPVARS pc,
                                 UINT uiFlags,
                                 LPVOID lpBits,
                                 BOOL *pfKey,
                                 LONG *plSize)
{
    auto ctx = reinterpret_cast<CodecContext*>(pc->lpState);
    if (!ctx || !ctx->proc)
        return NULL;

    DWORD inSize = ctx->width * ctx->height * 3;
    DWORD written;
    if (!WriteFile(ctx->proc->inputPipe(), lpBits, inSize, &written, NULL))
        return NULL;

    DWORD readBytes;
    if (!ReadFile(ctx->proc->outputPipe(),
                  ctx->outBuf, ctx->outBufSize, &readBytes, NULL))
        return NULL;

    *pfKey  = TRUE;
    *plSize = readBytes;
    return ctx->outBuf;
}

void VFWAPI ICSeqCompressFrameEnd(PCOMPVARS pc) {
    auto ctx = reinterpret_cast<CodecContext*>(pc->lpState);
    if (!ctx) return;

    if (ctx->proc) {
        ctx->proc->stop();
        delete ctx->proc;
    }
    delete[] ctx->outBuf;
    delete ctx;
    pc->lpState = nullptr;
}

STDAPI DllRegisterServer()   { return S_OK; }
STDAPI DllUnregisterServer() { return S_OK; }

} // extern "C"
