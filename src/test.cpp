// test.cpp
#include <windows.h>
#include <vfw.h>
#include <shellapi.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>

// Параметры тестового кадра
const int WIDTH  = 320;
const int HEIGHT = 240;

// Функция: заполняем буфер BGR24 синим цветом
void fillBlueFrame(std::vector<BYTE>& buf) {
    for (size_t i = 0; i < buf.size(); i += 3) {
        buf[i + 0] = 255; // B
        buf[i + 1] =   0; // G
        buf[i + 2] =   0; // R
    }
}



// Unicode-точка входа
int wmain(int argc, wchar_t* argv[]) {
    // 1) Составляем путь к DLL рядом с EXE
    std::wstring exeDir = std::filesystem::path(argv[0]).parent_path().wstring();
    std::wstring dllPath = exeDir + L"\\jxs_ffmpeg_vfw.dll";

    // 2) Загружаем DLL
    HMODULE hMod = LoadLibraryW(dllPath.c_str());
    if (!hMod) {
        std::wcerr << L"Не удалось загрузить DLL: " << dllPath << L"\n";
        return -1;
    }

    // 3) Получаем адреса функций
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

    if (!pICInfo || !pICLocate || !pICSeqCompressFrameStart ||
        !pICSeqCompressFrame || !pICSeqCompressFrameEnd) {
        std::cerr << "Не удалось получить адреса функций из DLL\n";
        FreeLibrary(hMod);
        return -2;
    }

    // 4) Заполняем ICINFO (необязательно, но для примера)
    ICINFO info = {};
    info.dwSize = sizeof(info);
    pICInfo(ICTYPE_VIDEO, MAKEFOURCC('J','X','S','F'), &info);

    // 5) Ищем наш кодек по FOURCC "JXSF"
    HIC hic = pICLocate(
        ICTYPE_VIDEO,
        MAKEFOURCC('J','X','S','F'),
        nullptr, nullptr,
        ICMODE_COMPRESS);
    if (!hic) {
        std::cerr << "ICLocate failed for FOURCC JXSF\n";
        FreeLibrary(hMod);
        return -3;
    }

    // 6) Готовим заголовок BGR24
    BITMAPINFOHEADER inHdr = {};
    inHdr.biSize        = sizeof(inHdr);
    inHdr.biWidth       = WIDTH;
    inHdr.biHeight      = HEIGHT;
    inHdr.biPlanes      = 1;
    inHdr.biBitCount    = 24;
    inHdr.biCompression = BI_RGB;
    inHdr.biSizeImage   = ((WIDTH * 24 + 31) & ~31) / 8 * HEIGHT;

    // 7) Настраиваем COMPVARS, указывая тот же FOURCC
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

    // 8) Стартуем сжатие
    if (!pICSeqCompressFrameStart(&cv, cv.lpbiIn)) {
        std::cerr << "ICSeqCompressFrameStart failed\n";
        ICClose(hic);
        FreeLibrary(hMod);
        return -4;
    }

    // 9) Генерируем и сжимаем кадр
    std::vector<BYTE> frameBuf(inHdr.biSizeImage);
    fillBlueFrame(frameBuf);

    BOOL isKey = FALSE;
    LONG outSize = 0;
    LPVOID outPtr = pICSeqCompressFrame(&cv, 0, frameBuf.data(), &isKey, &outSize);
    if (!outPtr) {
        std::cerr << "ICSeqCompressFrame failed\n";
        pICSeqCompressFrameEnd(&cv);
        ICClose(hic);
        FreeLibrary(hMod);
        return -5;
    }

    // 10) Сохраняем в файл
    HANDLE hFile = CreateFileW(L"out.avi", GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hFile, outPtr, outSize, &written, nullptr);
        CloseHandle(hFile);
    } else {
        std::cerr << "Не удалось создать out.avi\n";
    }

    // 11) Завершаем и чистим
    pICSeqCompressFrameEnd(&cv);
    ICClose(hic);
    FreeLibrary(hMod);

    std::cout << "Compression OK, out.avi (" << outSize << " bytes)\n";
    return 0;
}


// Заглушка WinMain, чтобы MinGW не жаловался на её отсутствие
extern "C" int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int ret = wmain(argc, argv);
    LocalFree(argv);
    return ret;
}