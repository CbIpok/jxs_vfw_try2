//----------C:\Users\dmitrienko\CLionProjects\untitled\src\test.cpp----------//
#include <windows.h>
#include <vfw.h>
#include <shellapi.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstring>
#include <cstdint>

// Test frame size
const int WIDTH  = 320;
const int HEIGHT = 240;

// ---------------------------------------------------------------------------
// gbrp12le (4:4:4, planar, 12 bits/component)
static void fillBlueFrame(std::vector<BYTE>& buf)
{
    std::cout << "[test] fillBlueFrame() – gbrp12le planar, blue\n";

    const size_t planePixels = static_cast<size_t>(WIDTH) * HEIGHT;
    const size_t planeBytes  = planePixels * 2;          // 16 bits / sample
    if (buf.size() != planeBytes * 3) {
        std::cerr << "[test] buffer size mismatch\n";
        return;
    }

    BYTE* gPlane = buf.data();
    BYTE* bPlane = gPlane + planeBytes;
    BYTE* rPlane = bPlane + planeBytes;

    std::memset(gPlane, 0, planeBytes);                  // G-plane → 0

    for (size_t i = 0; i < planePixels; ++i) {           // B-plane → full
        bPlane[2 * i]     = 0xFF;
        bPlane[2 * i + 1] = 0x0F;
    }

    std::memset(rPlane, 0, planeBytes);                  // R-plane → 0
}
//---------------------------------------------------------------------------

// Unicode entry point
int wmain(int argc, wchar_t* argv[])
{
    std::wcout << L"[test] argv[0]=" << argv[0] << L'\n';

    // 1) DLL path
    std::wstring exeDir  = std::filesystem::path(argv[0]).parent_path().wstring();
    std::wstring dllPath = exeDir + L"\\jxs_ffmpeg_vfw.dll";
    std::wcout << L"[test] DLL path = " << dllPath << L'\n';

    // 2) Load DLL
    HMODULE hMod = LoadLibraryW(dllPath.c_str());
    if (!hMod) {
        std::wcerr << L"[test] LoadLibraryW failed, err=" << GetLastError() << L'\n';
        return -1;
    }
    std::cout << "[test] DLL loaded\n";

    // 3) Get needed functions
    auto pICInfo = reinterpret_cast<BOOL (WINAPI*)(DWORD,DWORD,ICINFO*)>(
        GetProcAddress(hMod, "ICInfo"));
    auto pICLocate = reinterpret_cast<HIC (WINAPI*)(
        DWORD,DWORD,LPBITMAPINFOHEADER,LPBITMAPINFOHEADER,WORD)>(
        GetProcAddress(hMod, "ICLocate"));
    auto pStart = reinterpret_cast<BOOL (WINAPI*)(PCOMPVARS,LPBITMAPINFO)>(
        GetProcAddress(hMod, "ICSeqCompressFrameStart"));
    auto pFrame = reinterpret_cast<LPVOID (WINAPI*)(PCOMPVARS,UINT,LPVOID,BOOL*,LONG*)>(
        GetProcAddress(hMod, "ICSeqCompressFrame"));
    auto pEnd   = reinterpret_cast<void (WINAPI*)(PCOMPVARS)>(
        GetProcAddress(hMod, "ICSeqCompressFrameEnd"));

    if (!pICInfo || !pICLocate || !pStart || !pFrame || !pEnd) {
        std::cerr << "[test] Missing exported symbols\n";
        FreeLibrary(hMod);
        return -2;
    }

    // 4) ICInfo
    ICINFO info{ sizeof(info) };
    pICInfo(ICTYPE_VIDEO, MAKEFOURCC('J','X','S','F'), &info);

    // 5) Locate codec
    HIC hic = pICLocate(ICTYPE_VIDEO, MAKEFOURCC('J','X','S','F'),
                        nullptr, nullptr, ICMODE_COMPRESS);
    if (!hic) {
        std::cerr << "[test] ICLocate failed\n";
        FreeLibrary(hMod);
        return -3;
    }

    // 6) Input header
    BITMAPINFOHEADER inHdr{};
    inHdr.biSize        = sizeof(inHdr);
    inHdr.biWidth       = WIDTH;
    inHdr.biHeight      = HEIGHT;
    inHdr.biPlanes      = 1;
    inHdr.biBitCount    = 24;
    inHdr.biCompression = BI_RGB;
    inHdr.biSizeImage   = WIDTH * HEIGHT * 6;   // gbrp12le: 6 B/px

    // 7) COMPVARS
    COMPVARS cv{};
    cv.cbSize     = sizeof(cv);
    cv.hic        = hic;
    cv.fccType    = ICTYPE_VIDEO;
    cv.fccHandler = MAKEFOURCC('J','X','S','F');
    cv.lpbiIn     = reinterpret_cast<LPBITMAPINFO>(&inHdr);

    // 8) Start
    if (!pStart(&cv, cv.lpbiIn)) {
        std::cerr << "[test] Start failed\n";
        ICClose(hic); FreeLibrary(hMod); return -4;
    }

    // 9) One frame
    std::vector<BYTE> frameBuf(inHdr.biSizeImage);
    fillBlueFrame(frameBuf);

    BOOL  isKey{};
    LONG  outSize{};
    LPVOID outPtr = pFrame(&cv, 0, frameBuf.data(), &isKey, &outSize);
    if (!outPtr) {
        std::cerr << "[test] Compress failed\n";
        pEnd(&cv); ICClose(hic); FreeLibrary(hMod); return -5;
    }

    // 10) Save stream
    HANDLE hFile = CreateFileW(L"out.mkv", GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written{};
        WriteFile(hFile, outPtr, outSize, &written, nullptr);
        CloseHandle(hFile);
    }

    // 11) End
    pEnd(&cv); ICClose(hic); FreeLibrary(hMod);
    std::cout << "[test] Completed OK\n";
    return 0;
}

// WinMain stub for MinGW
extern "C" int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    int      argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int ret = wmain(argc, argv);
    LocalFree(argv);
    return ret;
}
