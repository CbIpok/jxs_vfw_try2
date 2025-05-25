//------------------------------------------------------------------------------
//  C:\Users\dmitrienko\CLionProjects\untitled\src\vfw_wrapper.cpp
//------------------------------------------------------------------------------

#include "vfw_wrapper.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <cstdio>     // _popen / _pclose
#include <cerrno>
#include <cstdlib>    // _wgetenv

using namespace std::string_literals;

//------------------------------------------------------------------------------
// Глобальные (настраиваемые) пути + авто-инициализация из переменных среды
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
}

// ------------------------------------------------------------------+
// UTF-16 → UTF-8
static std::string narrow(const std::wstring& ws)
{
    if (ws.empty()) return {};
    int n = ::WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(n - 1, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), n, nullptr, nullptr);
    return out;
}

// ------------------------------------------------------------------+
// Захват stdout+stderr через cmd.exe /C …  без временных файлов
static std::string exec(const std::string& cmd)
{
    // Оборачиваем команду в cmd.exe так, чтобы кавычки и редирект 2>&1 обрабатывались правильно
    std::string shellCmd = "cmd.exe /C \"" + cmd + " 2>&1\"";
    std::cout << "[exec] shell cmd: " << shellCmd << '\n';

    FILE* pipe = _popen(shellCmd.c_str(), "r");
    if (!pipe)
        return "[exec] _popen failed: "s + std::strerror(errno);

    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe))
        out += buf;

    int rc = _pclose(pipe);
    out += "\n[exec] rc=" + std::to_string(rc);
    return out;
}

// ------------------------------------------------------------------+
extern "C" {

//— ICInfo / ICLocate остаются без изменений —----------------------------------
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

// ------------------------------------------------------------------+
BOOL VFWAPI ICSeqCompressFrameStart(PCOMPVARS pc, LPBITMAPINFO lpbiIn)
{
    std::cout << "[ICSeqCompressFrameStart]\n";
    initPathsFromEnv();

    auto ctx   = new CodecContext{};
    ctx->width  = lpbiIn->bmiHeader.biWidth;
    ctx->height = lpbiIn->bmiHeader.biHeight;
    ctx->outBuf.resize(1024 * 1024);                 // стартовый объём 1 MiB

    ctx->baseDir = g_baseDir;                        // <- теперь настраиваемо
    std::error_code ec;
    std::filesystem::create_directories(ctx->baseDir, ec);
    if (ec)
        std::cerr << "[Start] cannot create base dir: " << ec.message() << '\n';
    else
        std::cout << "[Start] baseDir=" << ctx->baseDir.u8string() << '\n';

    pc->lpState = ctx;
    return TRUE;
}

// ------------------------------------------------------------------+
LPVOID VFWAPI ICSeqCompressFrame(PCOMPVARS pc, UINT, LPVOID lpBits,
                                 BOOL* pfKey, LONG* plSize)
{
    initPathsFromEnv();
    auto ctx = reinterpret_cast<CodecContext*>(pc->lpState);
    if (!ctx) return nullptr;

    const auto inFile  = ctx->baseDir / L"in.raw";
    const auto outFile = ctx->baseDir / L"out.mkv";

    // 1) сохраняем кадр -------------------------------------------------------
    {
        std::ofstream ofs(inFile, std::ios::binary);
        ofs.write(static_cast<char*>(lpBits),
                  static_cast<std::streamsize>(ctx->width) *
                  static_cast<std::streamsize>(ctx->height) * 6);
    }

    // 2) собираем и запускаем FFmpeg -----------------------------------------
    std::wcout << L"[Compress] FFmpeg "
               << (std::filesystem::exists(g_ffmpegPath) ? L"found" : L"NOT found")
               << L" at " << g_ffmpegPath << L'\n';

    std::wstringstream ss;
    ss << L"-y -f rawvideo"
       << L" -pix_fmt gbrp12le"
       << L" -s:v " << ctx->width << L"x" << ctx->height
       << L" -i \"" << inFile.wstring() << L"\""
       << L" -c:v libsvtjpegxs -bpp 1.25"
       << L" \"" << outFile.wstring() << L"\"";

    // Преобразуем в UTF-8 и выполняем через cmd.exe /C
    std::string cmd = narrow(ss.str());
    std::string log = exec("\"" + narrow(g_ffmpegPath.wstring()) + "\" " + cmd);

    std::cout << "[Compress] ---------- FFmpeg LOG BEGIN ----------\n"
              << log
              << "\n[Compress] ----------- FFmpeg LOG END -----------\n";

    // 3) читаем результат -----------------------------------------------------
    if (!std::filesystem::exists(outFile)) {
        std::cerr << "[Compress] out.mkv not found\n";
        return nullptr;
    }

    std::ifstream ifs(outFile, std::ios::binary | std::ios::ate);
    const size_t size = static_cast<size_t>(ifs.tellg());
    ifs.seekg(0);

    if (size > ctx->outBuf.size()) {
        ctx->outBuf.resize(size);
        std::cout << "[Compress] grow outBuf to " << size << " bytes\n";
    }
    ifs.read(reinterpret_cast<char*>(ctx->outBuf.data()), size);

    *pfKey  = TRUE;
    *plSize = static_cast<LONG>(size);
    std::cout << "[Compress] compressed size = " << size << '\n';
    return ctx->outBuf.data();
}

// ------------------------------------------------------------------+
void VFWAPI ICSeqCompressFrameEnd(PCOMPVARS pc)
{
    std::cout << "[ICSeqCompressFrameEnd]\n";
    delete reinterpret_cast<CodecContext*>(pc->lpState);
    pc->lpState = nullptr;
}

//— новые экспортируемые функции —---------------------------------------------
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

STDAPI DllRegisterServer()   { return S_OK; }
STDAPI DllUnregisterServer() { return S_OK; }

} // extern "C"
