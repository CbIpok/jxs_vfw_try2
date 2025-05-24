#include "vfw_wrapper.h"
#include <sstream>
#include <iostream>

//
// — FFmpegProcess —
//

FFmpegProcess::FFmpegProcess(const std::wstring &ffmpegPath)
  : ffmpegPath_(ffmpegPath),
    hProcess_(NULL),
    hStdinWrite_(NULL),
    hStdoutRead_(NULL)
{
    std::wcout << L"[FFmpegProcess] ctor, path=" << ffmpegPath_ << L"\n";
}

FFmpegProcess::~FFmpegProcess() {
    std::cout << "[FFmpegProcess] destructor\n";
    stop();
}

bool FFmpegProcess::start(const std::wstring &args) {
    std::wcout << L"[FFmpegProcess] start(), args=" << args << L"\n";

    // Create pipes for stdin/stdout
    SECURITY_ATTRIBUTES sa{ sizeof(sa), NULL, TRUE };
    HANDLE hStdinRead  = NULL;
    HANDLE hStdoutWrite = NULL;

    if (!CreatePipe(&hStdinRead, &hStdinWrite_, &sa, 0) ||
        !CreatePipe(&hStdoutRead_, &hStdoutWrite, &sa, 0)) {
        DWORD err = GetLastError();
        std::cerr << "[FFmpegProcess] CreatePipe failed, err=" << err << "\n";
        return false;
    }
    std::cout << "[FFmpegProcess] pipes created\n";

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
    std::wcout << L"[FFmpegProcess] CmdLine=\"" << cmdLine << L"\"\n";

    // Launch ffmpeg:
    if (!CreateProcessW(
            ffmpegPath_.c_str(),
            &cmdLine[0],
            NULL, NULL,
            TRUE,
            0,
            NULL, NULL,
            &si,
            &pi
        ))
    {
        DWORD err = GetLastError();
        std::cerr << "[FFmpegProcess] CreateProcessW failed, err=" << err << "\n";
        CloseHandle(hStdinRead);
        CloseHandle(hStdoutWrite);
        return false;
    }

    std::cout << "[FFmpegProcess] process started, PID=" << pi.dwProcessId << "\n";
    // We don't need these in the parent
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);

    hProcess_ = pi.hProcess;
    CloseHandle(pi.hThread);
    return true;
}

void FFmpegProcess::stop() {
    std::cout << "[FFmpegProcess] stop()\n";
    if (hStdinWrite_) {
        std::cout << "[FFmpegProcess] closing stdin pipe\n";
        CloseHandle(hStdinWrite_);
        hStdinWrite_ = NULL;
    }
    if (hProcess_) {
        std::cout << "[FFmpegProcess] waiting for process to exit\n";
        WaitForSingleObject(hProcess_, INFINITE);
        CloseHandle(hProcess_);
        hProcess_ = NULL;
    }
    if (hStdoutRead_) {
        std::cout << "[FFmpegProcess] closing stdout pipe\n";
        CloseHandle(hStdoutRead_);
        hStdoutRead_ = NULL;
    }
}

HANDLE FFmpegProcess::inputPipe()  const { return hStdinWrite_; }
HANDLE FFmpegProcess::outputPipe() const { return hStdoutRead_; }


//
// — VFW Callbacks —
//

extern "C" {

BOOL VFWAPI ICInfo(DWORD fccType, DWORD fccHandler, ICINFO *lpicinfo) {
    std::cout << "[ICInfo] called, fccType=" << fccType
              << " fccHandler=" << fccHandler << "\n";
    if (!lpicinfo) {
        std::cerr << "[ICInfo] bad pointer\n";
        return FALSE;
    }
    ZeroMemory(lpicinfo, sizeof(ICINFO));
    lpicinfo->dwSize     = sizeof(ICINFO);
    lpicinfo->fccType    = ICTYPE_VIDEO;
    lpicinfo->fccHandler = mmioFOURCC('J','X','S','F');
    lpicinfo->dwFlags    = 0;
    lpicinfo->dwVersion  = 1;
    lpicinfo->szName[0]       =
    lpicinfo->szDescription[0]= '\0';
    std::cout << "[ICInfo] filling out info OK\n";
    return TRUE;
}

HIC VFWAPI ICLocate(DWORD fccType, DWORD fccHandler,
                    LPBITMAPINFOHEADER lpbiIn,
                    LPBITMAPINFOHEADER lpbiOut,
                    WORD wFlags)
{
    std::cout << "[ICLocate] called, fccType=" << fccType
              << " fccHandler=" << fccHandler
              << " wFlags=" << wFlags << "\n";

    if ((wFlags & ICMODE_COMPRESS) &&
        lpbiIn  == nullptr &&
        lpbiOut == nullptr)
    {
        std::cout << "[ICLocate] enumeration ping\n";
        return reinterpret_cast<HIC>(1);
    }

    if (fccType    != ICTYPE_VIDEO ||
        fccHandler != mmioFOURCC('J','X','S','F')) {
        std::cout << "[ICLocate] not our handler\n";
        return NULL;
    }

    if (lpbiIn->biCompression != BI_RGB ||
        lpbiIn->biBitCount   != 24) {
        std::cout << "[ICLocate] unsupported input format\n";
        return NULL;
    }

    // Advertise MJPEG
    lpbiOut->biSize        = sizeof(BITMAPINFOHEADER);
    lpbiOut->biWidth       = lpbiIn->biWidth;
    lpbiOut->biHeight      = lpbiIn->biHeight;
    lpbiOut->biPlanes      = 1;
    lpbiOut->biBitCount    = 24;
    lpbiOut->biCompression = mmioFOURCC('M','J','P','G');
    lpbiOut->biSizeImage   = 0;

    std::cout << "[ICLocate] returning dummy HIC\n";
    return reinterpret_cast<HIC>(1);
}

BOOL VFWAPI ICSeqCompressFrameStart(PCOMPVARS pc, LPBITMAPINFO lpbiIn) {
    std::cout << "[ICSeqCompressFrameStart] called\n";
    CodecContext* ctx = new CodecContext;
    ctx->width      = lpbiIn->bmiHeader.biWidth;
    ctx->height     = lpbiIn->bmiHeader.biHeight;
    ctx->outBufSize = 1024 * 1024;
    ctx->outBuf     = new BYTE[ctx->outBufSize];
    pc->lpState     = ctx;
    std::cout << "[ICSeqCompressFrameStart] ctx width=" << ctx->width
              << " height=" << ctx->height << "\n";

    // Build ffmpeg args to encode a single MJPEG frame
    std::wstringstream ss;
    ss << L"-f rawvideo -pix_fmt bgr24 -s "
       << ctx->width << L"x" << ctx->height
       << L" -i pipe:0"
       << L" -vcodec mjpeg"    // MJPEG encoder
       << L" -f mjpeg"         // raw MJPEG output
       << L" -frames:v 1"      // exactly one frame
       << L" pipe:1 -nostdin -hide_banner";
    std::wcout << L"[ICSeqCompressFrameStart] ffmpeg args=" << ss.str() << L"\n";

    // NOTE: adjust this path!
    ctx->proc = new FFmpegProcess(
        L"C:/dmitrienkomy/cpp/jxs_ffmpeg/install-dir/bin/ffmpeg.exe"
    );
    bool ok = ctx->proc->start(ss.str());
    std::cout << "[ICSeqCompressFrameStart] FFmpegProcess::start() -> "
              << (ok ? "SUCCESS" : "FAIL") << "\n";
    return ok;
}

LPVOID VFWAPI ICSeqCompressFrame(PCOMPVARS pc,
                                 UINT uiFlags,
                                 LPVOID lpBits,
                                 BOOL *pfKey,
                                 LONG *plSize)
{
    std::cout << "[ICSeqCompressFrame] called, uiFlags=" << uiFlags << "\n";
    auto ctx = reinterpret_cast<CodecContext*>(pc->lpState);
    if (!ctx || !ctx->proc) {
        std::cerr << "[ICSeqCompressFrame] no context or proc\n";
        return NULL;
    }

    DWORD inSize = ctx->width * ctx->height * 3;
    std::cout << "[ICSeqCompressFrame] writing " << inSize << " bytes to ffmpeg stdin\n";
    DWORD written = 0;
    if (!WriteFile(ctx->proc->inputPipe(), lpBits, inSize, &written, NULL)) {
        DWORD err = GetLastError();
        std::cerr << "[ICSeqCompressFrame] WriteFile failed, err=" << err << "\n";
        return NULL;
    }
    std::cout << "[ICSeqCompressFrame] wrote " << written << " bytes\n";

    // Signal EOF so ffmpeg quits and flushes
    std::cout << "[ICSeqCompressFrame] closing stdin to signal EOF\n";
    CloseHandle(ctx->proc->inputPipe());

    // Now read the compressed JPEG
    DWORD readBytes = 0;
    std::cout << "[ICSeqCompressFrame] attempting ReadFile from ffmpeg stdout\n";
    if (!ReadFile(ctx->proc->outputPipe(),
                  ctx->outBuf, ctx->outBufSize, &readBytes, NULL)) {
        DWORD err = GetLastError();
        std::cerr << "[ICSeqCompressFrame] ReadFile failed, err=" << err << "\n";
        return NULL;
    }
    std::cout << "[ICSeqCompressFrame] read " << readBytes << " bytes\n";

    *pfKey  = TRUE;
    *plSize = readBytes;
    return ctx->outBuf;
}

void VFWAPI ICSeqCompressFrameEnd(PCOMPVARS pc) {
    std::cout << "[ICSeqCompressFrameEnd] called\n";
    auto ctx = reinterpret_cast<CodecContext*>(pc->lpState);
    if (!ctx) return;

    if (ctx->proc) {
        std::cout << "[ICSeqCompressFrameEnd] stopping process\n";
        ctx->proc->stop();
        delete ctx->proc;
    }
    delete[] ctx->outBuf;
    delete ctx;
    pc->lpState = nullptr;
    std::cout << "[ICSeqCompressFrameEnd] cleanup done\n";
}

STDAPI DllRegisterServer()   { std::cout << "[DllRegisterServer]\n"; return S_OK; }
STDAPI DllUnregisterServer() { std::cout << "[DllUnregisterServer]\n"; return S_OK; }

} // extern "C"
