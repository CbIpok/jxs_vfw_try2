#include <windows.h>
#include <vfw.h>
#include <iostream>
#include <vector>

// Параметры тестового кадра
const int WIDTH  = 320;
const int HEIGHT = 240;

// Простейшая функция: синий кадр BGR24
void fillBlueFrame(std::vector<BYTE>& buf) {
    for (size_t i = 0; i < buf.size(); i += 3) {
        buf[i + 0] = 255; // B
        buf[i + 1] = 0;   // G
        buf[i + 2] = 0;   // R
    }
}

int main() {
    // 1. ICInfo: метаданные кодека (автоматически выбираем MJPG)
    ICINFO info = {};
    info.dwSize = sizeof(info);
    ICInfo(ICTYPE_VIDEO, MAKEFOURCC('M','J','P','G'), &info);

    // 2. ICLocate: получаем HIC
    HIC hic = ICLocate(ICTYPE_VIDEO, MAKEFOURCC('M','J','P','G'),
                       NULL, NULL, ICMODE_COMPRESS);
    if (!hic) {
        std::cerr << "ICLocate failed\n";
        return -1;
    }

    // 3. Настройка BITMAPINFOHEADER для BGR24
    BITMAPINFOHEADER inHdr = {};
    inHdr.biSize        = sizeof(inHdr);
    inHdr.biWidth       = WIDTH;
    inHdr.biHeight      = HEIGHT;
    inHdr.biPlanes      = 1;
    inHdr.biBitCount    = 24;                          // BGR24
    inHdr.biCompression = BI_RGB;
    inHdr.biSizeImage   = ((WIDTH * 24 + 31) & ~31) / 8 * HEIGHT;

    // 4. COMPVARS: инициализация
    COMPVARS cv = {};
    cv.cbSize      = sizeof(cv);                       // СМ. COMPVARS.cbSize :contentReference[oaicite:8]{index=8}
    cv.dwFlags     = 0;
    cv.hic         = hic;                              // HIC из ICLocate :contentReference[oaicite:9]{index=9}
    cv.fccType     = ICTYPE_VIDEO;
    cv.fccHandler  = MAKEFOURCC('M','J','P','G');
    cv.lpbiIn      = reinterpret_cast<LPBITMAPINFO>(&inHdr);
    cv.lpbiOut     = nullptr;
    cv.lKey        = 0;                                // без ключевых кадров по принуждению
    cv.lDataRate   = 0;                                // авто
    cv.lQ          = ICQUALITY_DEFAULT;                // стандартное качество
    cv.lKeyCount   = 0;

    // 5. Запуск последовательного сжатия
    if (!ICSeqCompressFrameStart(&cv, cv.lpbiIn)) {     // СМ. сигнатуру :contentReference[oaicite:10]{index=10}
        std::cerr << "ICSeqCompressFrameStart failed\n";
        ICClose(hic);
        return -2;
    }

    // 6. Подготовка и сжатие одного кадра
    std::vector<BYTE> frameBuf(inHdr.biSizeImage);
    fillBlueFrame(frameBuf);

    BOOL  isKey = FALSE;
    LONG  outSize = 0;
    LPVOID outPtr = ICSeqCompressFrame(
        &cv,
        0,                                            // uiFlags must be zero :contentReference[oaicite:11]{index=11}
        frameBuf.data(),
        &isKey,
        &outSize
    );
    if (!outPtr) {
        std::cerr << "ICSeqCompressFrame failed\n";
        ICSeqCompressFrameEnd(&cv);
        ICClose(hic);
        return -3;
    }

    // 7. Запись результата в файл
    HANDLE hFile = CreateFileW(L"out.avi",
        GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hFile, outPtr, outSize, &written, NULL);
        CloseHandle(hFile);
    }

    // 8. Завершение сессии
    ICSeqCompressFrameEnd(&cv);                         // СМ.  :contentReference[oaicite:12]{index=12}
    ICClose(hic);

    std::cout << "Compression OK, out.avi (" << outSize << " bytes)\n";
    return 0;
}
