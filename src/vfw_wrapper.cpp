// vfw_wrapper.cpp
#include <windows.h>
#include <vfw.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
using namespace std::string_literals;
// ------------------------------------------------------------------+
// Узкий UTF-8 вариант из std::wstring  ➜  std::string
static std::string narrow(const std::wstring &ws) {
    if (ws.empty()) return {};
    int n = ::WideCharToMultiByte(CP_UTF8, 0,
                                  ws.c_str(), -1,
                                  nullptr, 0, nullptr, nullptr);
    std::string out(n - 1, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0,
                          ws.c_str(), -1,
                          out.data(), n, nullptr, nullptr);
    return out;
}

// Полный захват stdout+stderr внешней команды ― «auto exec = …»
static std::string exec(const std::string &cmd)
{
    char tmpName[L_tmpnam];
    std::tmpnam(tmpName);                     // имя временного файла

    const std::string fullCmd = cmd + " > \"" + tmpName + "\" 2>&1";
    const int rc = std::system(fullCmd.c_str());

    std::ifstream ifs(tmpName, std::ios::binary);
    std::string out((std::istreambuf_iterator<char>(ifs)),
                    std::istreambuf_iterator<char>());
    std::remove(tmpName);                     // чистим лог-файл

    out += "\n[exec] rc=" + std::to_string(rc);
    return out;
}

// ------------------------------------------------------------------+


struct CodecContext {
    int width;
    int height;
    size_t outBufSize;
    BYTE *outBuf;
    std::filesystem::path tempDir;
};


extern "C" {
// Оставляем ICInfo/ICLocate без изменений:
BOOL VFWAPI ICInfo(DWORD fccType, DWORD fccHandler, ICINFO *lpicinfo) {
    if (!lpicinfo) return FALSE;
    ZeroMemory(lpicinfo, sizeof(*lpicinfo));
    lpicinfo->dwSize = sizeof(ICINFO);
    lpicinfo->fccType = ICTYPE_VIDEO;
    lpicinfo->fccHandler = mmioFOURCC('J', 'X', 'S', 'F');
    return TRUE;
}

HIC VFWAPI ICLocate(DWORD fccType, DWORD fccHandler,
                    LPBITMAPINFOHEADER lpbiIn,
                    LPBITMAPINFOHEADER lpbiOut,
                    WORD wFlags) {
    if ((wFlags & ICMODE_COMPRESS) && !lpbiIn && !lpbiOut)
        return reinterpret_cast<HIC>(1);
    if (fccType != ICTYPE_VIDEO || fccHandler != mmioFOURCC('J', 'X', 'S', 'F'))
        return NULL;
    if (lpbiIn->biCompression != BI_RGB || lpbiIn->biBitCount != 24)
        return NULL;
    lpbiOut->biSize = sizeof(BITMAPINFOHEADER);
    lpbiOut->biWidth = lpbiIn->biWidth;
    lpbiOut->biHeight = lpbiIn->biHeight;
    lpbiOut->biPlanes = 1;
    lpbiOut->biBitCount = 24;
    lpbiOut->biCompression = mmioFOURCC('M', 'J', 'P', 'G');
    lpbiOut->biSizeImage = 0;
    return reinterpret_cast<HIC>(1);
}

BOOL VFWAPI ICSeqCompressFrameStart(PCOMPVARS pc, LPBITMAPINFO lpbiIn) {
    std::cout << "[ICSeqCompressFrameStart]\n";
    auto ctx = new CodecContext;
    ctx->width = lpbiIn->bmiHeader.biWidth;
    ctx->height = lpbiIn->bmiHeader.biHeight;
    ctx->outBufSize = 1024 * 1024;
    ctx->outBuf = new BYTE[ctx->outBufSize];
    ctx->tempDir = std::filesystem::temp_directory_path() / L"vfwffmpeg";
    std::error_code ec;
    std::filesystem::create_directories(ctx->tempDir, ec);
    if (ec) {
        std::cerr << "[ICSeqCompressFrameStart] cannot create temp dir: "
                << ec.message() << "\n";
    } else {
        std::cout << "[ICSeqCompressFrameStart] tempDir="
                << ctx->tempDir.u8string() << "\n";
    }
    pc->lpState = ctx;
    return TRUE;
}

LPVOID VFWAPI ICSeqCompressFrame(PCOMPVARS pc,
                                 UINT /*uiFlags*/,
                                 LPVOID lpBits,
                                 BOOL *pfKey,
                                 LONG *plSize)
{
    auto ctx = reinterpret_cast<CodecContext *>(pc->lpState);
    if (!ctx) return nullptr;

    const auto inFile  = ctx->tempDir / L"in.raw";
    const auto outFile = ctx->tempDir / L"out.jxs";

    // 1) сохраняем входной кадр
    {
        std::ofstream ofs(inFile, std::ios::binary);
        ofs.write(reinterpret_cast<char *>(lpBits),
                  ctx->width * ctx->height * 3);
    }

    // 2) формируем команду FFmpeg
    const std::wstring ffmpegPath =
        L"C:\\dmitrienkomy\\cpp\\jxs_ffmpeg\\install-dir\\bin\\ffmpeg.exe";

    std::wstringstream ss;
    ss << L"-y"
       << L" -f rawvideo"
       << L" -pix_fmt bgr24"
       << L" -s:v " << ctx->width << L"x" << ctx->height
       << L" -i \"" << inFile.wstring() << L"\""
       << L" -c:v libsvtjpegxs"
       << L" -bpp 1.25"
       << L" \"" << outFile.wstring() << L"\"";

    // БЕЗ второго 2>&1 — exec сам всё перенаправит
    const std::string fullCmd = narrow(L"\"" + ffmpegPath + L"\" " + ss.str());

    std::cout << "[ICSeqCompressFrame] run: " << fullCmd << '\n';
    const std::string ffmpegLog = exec(fullCmd);
    std::cout << "[ICSeqCompressFrame] ---------- FFmpeg LOG BEGIN ----------\n"
              << ffmpegLog
              << "\n[ICSeqCompressFrame] ----------- FFmpeg LOG END -----------\n";

    // 3) проверяем результат
    if (!std::filesystem::exists(outFile)) {
        std::cerr << "[ICSeqCompressFrame] out.jxs not found\n";
        return nullptr;
    }

    std::ifstream ifs(outFile, std::ios::binary | std::ios::ate);
    const size_t size = static_cast<size_t>(ifs.tellg());
    ifs.seekg(0);
    if (size > ctx->outBufSize) {
        delete[] ctx->outBuf;
        ctx->outBufSize = size;
        ctx->outBuf = new BYTE[size];
    }
    ifs.read(reinterpret_cast<char *>(ctx->outBuf), size);

    *pfKey  = TRUE;
    *plSize = static_cast<LONG>(size);
    std::cout << "[ICSeqCompressFrame] compressed size=" << size << "\n";
    return ctx->outBuf;
}

void VFWAPI ICSeqCompressFrameEnd(PCOMPVARS pc)
{
    std::cout << "[ICSeqCompressFrameEnd]\n";
    auto ctx = reinterpret_cast<CodecContext *>(pc->lpState);
    if (!ctx) return;

    delete[] ctx->outBuf;
    std::filesystem::remove(ctx->tempDir / L"in.raw");
    std::filesystem::remove(ctx->tempDir / L"out.jxs");   // ← было .jpg
    delete ctx;
    pc->lpState = nullptr;
}

STDAPI DllRegisterServer() { return S_OK; }
STDAPI DllUnregisterServer() { return S_OK; }
} // extern "C"
