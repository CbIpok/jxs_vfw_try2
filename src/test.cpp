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
// gbrp12le (4:4:4, planar, 12 bits/component):
//   • memory layout:  G-plane | B-plane | R-plane
//   • each sample:    unsigned little-endian 16-bit, lower 12 bits used
//   • bytes/pixel:    6
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

    // G-plane (all zero)
    std::memset(gPlane, 0, planeBytes);

    // B-plane (full – 0x0FFF -> FF 0F little-endian)
    for (size_t i = 0; i < planePixels; ++i) {
        bPlane[2 * i]     = 0xFF;   // low byte
        bPlane[2 * i + 1] = 0x0F;   // high byte (only lower 4 bits significant)
    }

    // R-plane (all zero)
    std::memset(rPlane, 0, planeBytes);
}
//---------------------------------------------------------------------------

// Unicode entry point
int wmain(int argc, wchar_t* argv[])
{
    std::wcout << L"[test] wmain, argv[0]=" << argv[0] << L"\n";

    // 1) Path to DLL
    std::wstring exeDir  = std::filesystem::path(argv[0]).parent_path().wstring();
    std::wstring dllPath = exeDir + L"\\jxs_ffmpeg_vfw.dll";
    std::wcout << L"[test] DLL path = " << dllPath << L"\n";

    // 2) Load DLL
    HMODULE hMod = LoadLibraryW(dllPath.c_str());
    if (!hMod) {
        std::wcerr << L"[test] LoadLibraryW failed, err=" << GetLastError() << L"\n";
        return -1;
    }
    std::cout << "[test] DLL loaded successfully\n";

    // 3) Get function pointers we need
    auto pICInfo = reinterpret_cast<BOOL (WINAPI*)(DWORD, DWORD, ICINFO*)>(
        GetProcAddress(hMod, "ICInfo"));
    auto pICLocate = reinterpret_cast<HIC (WINAPI*)(
        DWORD, DWORD, LPBITMAPINFOHEADER, LPBITMAPINFOHEADER, WORD)>(
        GetProcAddress(hMod, "ICLocate"));
    auto pICSeqCompressFrameStart = reinterpret_cast<BOOL (WINAPI*)(
        PCOMPVARS, LPBITMAPINFO)>(
        GetProcAddress(hMod, "ICSeqCompressFrameStart"));
    auto pICSeqCompressFrame = reinterpret_cast<LPVOID (WINAPI*)(
        PCOMPVARS, UINT, LPVOID, BOOL*, LONG*)>(
        GetProcAddress(hMod, "ICSeqCompressFrame"));
    auto pICSeqCompressFrameEnd = reinterpret_cast<void (WINAPI*)(PCOMPVARS)>(
        GetProcAddress(hMod, "ICSeqCompressFrameEnd"));

    std::cout << "[test] Got proc addresses: "
              << (pICInfo ? "ICInfo " : "")
              << (pICLocate ? "ICLocate " : "")
              << (pICSeqCompressFrameStart ? "Start " : "")
              << (pICSeqCompressFrame ? "Compress " : "")
              << (pICSeqCompressFrameEnd ? "End" : "")
              << "\n";

    if (!pICInfo || !pICLocate || !pICSeqCompressFrameStart ||
        !pICSeqCompressFrame || !pICSeqCompressFrameEnd)
    {
        std::cerr << "[test] Missing one or more functions\n";
        FreeLibrary(hMod);
        return -2;
    }

    // 4) Optional: ICInfo
    ICINFO info = {};
    info.dwSize = sizeof(info);
    BOOL infoOk = pICInfo(ICTYPE_VIDEO, MAKEFOURCC('J','X','S','F'), &info);
    std::cout << "[test] ICInfo returned " << infoOk << "\n";

    // 5) Locate codec (compress mode)
    HIC hic = pICLocate(
        ICTYPE_VIDEO,
        MAKEFOURCC('J','X','S','F'),
        nullptr, nullptr,
        ICMODE_COMPRESS);
    std::cout << "[test] ICLocate returned HIC=" << hic << "\n";
    if (!hic) {
        std::cerr << "[test] ICLocate failed\n";
        FreeLibrary(hMod);
        return -3;
    }

    // 6) Prepare input header
    BITMAPINFOHEADER inHdr = {};
    inHdr.biSize        = sizeof(inHdr);
    inHdr.biWidth       = WIDTH;
    inHdr.biHeight      = HEIGHT;
    inHdr.biPlanes      = 1;
    inHdr.biBitCount    = 24;   // still 24 – Windows header can’t represent 12 bpc
    inHdr.biCompression = BI_RGB;
    inHdr.biSizeImage   = WIDTH * HEIGHT * 6;   // gbrp12le: 6 bytes / pixel
    std::cout << "[test] Prepared BITMAPINFOHEADER, sizeImage="
              << inHdr.biSizeImage << "\n";

    // 7) Setup COMPVARS
    COMPVARS cv = {};
    cv.cbSize      = sizeof(cv);
    cv.dwFlags     = 0;
    cv.hic         = hic;
    cv.fccType     = ICTYPE_VIDEO;
    cv.fccHandler  = MAKEFOURCC('J','X','S','F');
    cv.lpbiIn      = reinterpret_cast<LPBITMAPINFO>(&inHdr);
    cv.lpbiOut     = nullptr;
    cv.lKey        = 0;
    cv.lDataRate   = 0;
    cv.lQ          = ICQUALITY_DEFAULT;
    cv.lKeyCount   = 0;

    // 8) Start compression
    std::cout << "[test] Calling ICSeqCompressFrameStart()\n";
    if (!pICSeqCompressFrameStart(&cv, cv.lpbiIn)) {
        std::cerr << "[test] ICSeqCompressFrameStart failed\n";
        ICClose(hic);
        FreeLibrary(hMod);
        return -4;
    }
    std::cout << "[test] ICSeqCompressFrameStart succeeded\n";

    // 9) Generate + compress one frame
    std::vector<BYTE> frameBuf(inHdr.biSizeImage);
    fillBlueFrame(frameBuf);

    BOOL isKey = FALSE;
    LONG outSize = 0;
    std::cout << "[test] Calling ICSeqCompressFrame()\n";
    LPVOID outPtr = pICSeqCompressFrame(&cv, 0,
                                        frameBuf.data(), &isKey, &outSize);
    std::cout << "[test] ICSeqCompressFrame returned ptr=" << outPtr
              << " key=" << isKey << " size=" << outSize << "\n";
    if (!outPtr) {
        std::cerr << "[test] ICSeqCompressFrame failed\n";
        pICSeqCompressFrameEnd(&cv);
        ICClose(hic);
        FreeLibrary(hMod);
        return -5;
    }

    // 10) Write compressed stream to file
    std::cout << "[test] Writing output to out.jxs\n";
    HANDLE hFile = CreateFileW(L"out.jxs", GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hFile, outPtr, outSize, &written, nullptr);
        CloseHandle(hFile);
        std::cout << "[test] Wrote " << written << " bytes to out.jxs\n";
    } else {
        std::cerr << "[test] CreateFileW failed, err=" << GetLastError() << "\n";
    }

    // 11) End compression + cleanup
    std::cout << "[test] Calling ICSeqCompressFrameEnd()\n";
    pICSeqCompressFrameEnd(&cv);
    ICClose(hic);
    FreeLibrary(hMod);

    std::cout << "[test] Completed successfully\n";
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
//---------------------------------------------------------------------------//
