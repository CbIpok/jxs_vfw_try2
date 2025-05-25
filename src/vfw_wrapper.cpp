//------C:\Users\dmitrienko\CLionProjects\untitled\src\vfw_wrapper.cpp-------
#include "vfw_wrapper.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <cerrno>
#include <cstdlib>    // _wgetenv

using namespace std::string_literals;

//------------------------------------------------------------------------------
// Глобальные пути + авто-инициализация из переменных среды
//------------------------------------------------------------------------------
namespace {
    std::filesystem::path g_baseDir    = L"C:\\dmitrienkomy\\python";
    std::filesystem::path g_ffmpegPath = L"C:\\dmitrienkomy\\cpp\\jxs_ffmpeg\\install-dir\\bin\\ffmpeg.exe";

    void initPathsFromEnv()
    {
        static bool done = false;
        if (done) return;
        done = true;

        if (const wchar_t* env = _wgetenv(L"JXS_VFW_BASEDIR"); env && *env)
            g_baseDir = env;

        if (const wchar_t* env = _wgetenv(L"JXS_VFW_FFMPEG"); env && *env)
            g_ffmpegPath = env;
    }

    // Запуск FFmpeg с перенаправлением stdin/stdout
    void startFFmpeg(CodecContext* ctx)
    {
        SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
        HANDLE inRd, inWr, outRd, outWr;

        if (!CreatePipe(&inRd,  &inWr,  &sa, 0) ||
            !CreatePipe(&outRd, &outWr, &sa, 0))
        {
            std::cerr << "[Start] CreatePipe failed\n";
            return;
        }

        // Чтобы child-процесс не наследовал лишние концы:
        SetHandleInformation(inWr,  HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(outRd, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si{ sizeof(si) };
        si.dwFlags    = STARTF_USESTDHANDLES;
        si.hStdInput  = inRd;   // child читает здесь
        si.hStdOutput = outWr;  // child пишет сюда
        si.hStdError  = outWr;  // stderr → stdout

        std::wstringstream cmd;
        cmd << L"\"" << g_ffmpegPath.wstring() << L"\" -y "
            L"-f rawvideo -pix_fmt gbrp12le "
            L"-s:v " << ctx->width << L"x" << ctx->height << L" "
            L"-i pipe:0 "
            L"-c:v libsvtjpegxs -bpp 1.25 "
            L"-f matroska pipe:1";

        PROCESS_INFORMATION pi;
        if (!CreateProcessW(
                nullptr,
                cmd.str().data(),
                nullptr, nullptr,
                TRUE,               // унаследовать дескрипторы
                CREATE_NO_WINDOW,
                nullptr, nullptr,
                &si, &pi))
        {
            std::cerr << "[Start] CreateProcessW failed, err=" << GetLastError() << "\n";
            CloseHandle(inRd);
            CloseHandle(inWr);
            CloseHandle(outRd);
            CloseHandle(outWr);
            return;
        }

        // сохраняем родительские концы
        ctx->ffmpegStdin  = inWr;
        ctx->ffmpegStdout = outRd;
        ctx->ffmpegProc   = pi.hProcess;

        // ненужное в родителе закрываем
        CloseHandle(inRd);
        CloseHandle(outWr);
        CloseHandle(pi.hThread);
    }
}

//------------------------------------------------------------------------------
// VFW API реализации
//------------------------------------------------------------------------------

BOOL VFWAPI ICInfo(DWORD, DWORD, ICINFO* lpicinfo)
{
    if (!lpicinfo) return FALSE;
    ZeroMemory(lpicinfo, sizeof(*lpicinfo));
    lpicinfo->dwSize     = sizeof(ICINFO);
    lpicinfo->fccType    = ICTYPE_VIDEO;
    lpicinfo->fccHandler = mmioFOURCC('J','X','S','F');
    return TRUE;
}

HIC VFWAPI ICLocate(DWORD fccType, DWORD fccHandler,
                    LPBITMAPINFOHEADER lpbiIn,
                    LPBITMAPINFOHEADER lpbiOut,
                    WORD wFlags)
{
    if ((wFlags & ICMODE_COMPRESS) && !lpbiIn && !lpbiOut)
        return reinterpret_cast<HIC>(1);
    if (fccType != ICTYPE_VIDEO || fccHandler != mmioFOURCC('J','X','S','F'))
        return NULL;
    if (lpbiIn->biCompression != BI_RGB || lpbiIn->biBitCount != 24)
        return NULL;

    lpbiOut->biSize        = sizeof(BITMAPINFOHEADER);
    lpbiOut->biWidth       = lpbiIn->biWidth;
    lpbiOut->biHeight      = lpbiIn->biHeight;
    lpbiOut->biPlanes      = 1;
    lpbiOut->biBitCount    = 24;
    lpbiOut->biCompression = mmioFOURCC('M','J','P','G');
    lpbiOut->biSizeImage   = 0;
    return reinterpret_cast<HIC>(1);
}

BOOL VFWAPI ICSeqCompressFrameStart(PCOMPVARS pc, LPBITMAPINFO lpbiIn)
{
    initPathsFromEnv();

    auto ctx = new CodecContext{};
    ctx->width   = lpbiIn->bmiHeader.biWidth;
    ctx->height  = lpbiIn->bmiHeader.biHeight;
    ctx->baseDir = g_baseDir;
    ctx->outBuf.clear();

    std::error_code ec;
    std::filesystem::create_directories(ctx->baseDir, ec);
    if (ec)
        std::cerr << "[Start] cannot create base dir: " << ec.message() << '\n';
    else
        std::cout << "[Start] baseDir=" << ctx->baseDir.u8string() << '\n';

    startFFmpeg(ctx);
    pc->lpState = ctx;
    return TRUE;
}

LPVOID VFWAPI ICSeqCompressFrame(PCOMPVARS pc, UINT, LPVOID lpBits,
                                 BOOL* pfKey, LONG* plSize)
{
    auto ctx = reinterpret_cast<CodecContext*>(pc->lpState);
    if (!ctx || ctx->ffmpegStdin == INVALID_HANDLE_VALUE)
        return nullptr;

    // 1) пишем кадр
    DWORD written = 0;
    WriteFile(ctx->ffmpegStdin, lpBits, ctx->width * ctx->height * 6, &written, nullptr);

    // 2) читаем доступные данные
    DWORD available = 0;
    std::vector<BYTE> buf;
    if (PeekNamedPipe(ctx->ffmpegStdout, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
        buf.resize(available);
        DWORD readBytes = 0;
        ReadFile(ctx->ffmpegStdout, buf.data(), available, &readBytes, nullptr);
        buf.resize(readBytes);
    }

    ctx->outBuf = std::move(buf);
    *pfKey      = TRUE;
    *plSize     = static_cast<LONG>(ctx->outBuf.size());
    return ctx->outBuf.data();
}

void VFWAPI ICSeqCompressFrameEnd(PCOMPVARS pc)
{
    auto ctx = reinterpret_cast<CodecContext*>(pc->lpState);
    if (!ctx) return;

    // закрываем stdin → ffmpeg допишет трейлер и выйдет
    CloseHandle(ctx->ffmpegStdin);

    // считываем остаток stdout
    std::vector<BYTE> trailer;
    for (;;) {
        BYTE temp[4096];
        DWORD r = 0;
        if (!ReadFile(ctx->ffmpegStdout, temp, sizeof(temp), &r, nullptr) || r == 0)
            break;
        trailer.insert(trailer.end(), temp, temp + r);
    }

    // ждём завершения ffmpeg
    WaitForSingleObject(ctx->ffmpegProc, INFINITE);
    CloseHandle(ctx->ffmpegStdout);
    CloseHandle(ctx->ffmpegProc);

    delete ctx;
    pc->lpState = nullptr;
}

// COM-регистрация (заглушки)
STDAPI DllRegisterServer()   { return S_OK; }
STDAPI DllUnregisterServer() { return S_OK; }

// Конфигурация путей
void WINAPI JXSVFW_SetBaseDir(const wchar_t* dir)
{
    if (dir && *dir)
        g_baseDir = dir;
}

void WINAPI JXSVFW_SetFFmpegPath(const wchar_t* path)
{
    if (path && *path)
        g_ffmpegPath = path;
}
