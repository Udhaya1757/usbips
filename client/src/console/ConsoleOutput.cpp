#include "ConsoleOutput.h"

#include <Windows.h>
#include <iostream>
#include <mutex>
#include <streambuf>

namespace {

std::mutex g_outputMutex;

HANDLE GetOutputHandle(bool error) {
    return GetStdHandle(error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
}

bool IsConsoleHandle(HANDLE handle) {
    DWORD mode = 0;
    return handle != nullptr && handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode) != FALSE;
}

void WriteUtf8(HANDLE handle, const std::wstring& text) {
    if (text.empty()) {
        return;
    }

    int byteCount = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (byteCount <= 0) {
        return;
    }

    std::string utf8(static_cast<size_t>(byteCount), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            utf8.data(),
            byteCount,
            nullptr,
            nullptr) <= 0) {
        return;
    }

    const char* current = utf8.data();
    DWORD remaining = static_cast<DWORD>(utf8.size());
    while (remaining > 0) {
        DWORD written = 0;
        if (!WriteFile(handle, current, remaining, &written, nullptr) || written == 0) {
            return;
        }
        current += written;
        remaining -= written;
    }
}

void WriteText(HANDLE handle, const std::wstring& text) {
    std::lock_guard<std::mutex> lock(g_outputMutex);
    if (IsConsoleHandle(handle)) {
        const wchar_t* current = text.data();
        DWORD remaining = static_cast<DWORD>(text.size());
        while (remaining > 0) {
            DWORD written = 0;
            if (!WriteConsoleW(handle, current, remaining, &written, nullptr) || written == 0) {
                return;
            }
            current += written;
            remaining -= written;
        }
        return;
    }

    if (handle != nullptr && handle != INVALID_HANDLE_VALUE && GetFileType(handle) != FILE_TYPE_UNKNOWN) {
        WriteUtf8(handle, text);
    }
}

class ConsoleStreamBuffer final : public std::wstreambuf {
public:
    explicit ConsoleStreamBuffer(bool error) : m_error(error) {}

protected:
    std::streamsize xsputn(const wchar_t* text, std::streamsize count) override {
        if (count > 0) {
            ConsoleOutput::Write(std::wstring(text, static_cast<size_t>(count)), m_error);
        }
        return count;
    }

    int_type overflow(int_type character) override {
        if (!traits_type::eq_int_type(character, traits_type::eof())) {
            ConsoleOutput::Write(std::wstring(1, traits_type::to_char_type(character)), m_error);
        }
        return traits_type::not_eof(character);
    }

    int sync() override {
        return 0;
    }

private:
    bool m_error;
};

ConsoleStreamBuffer g_stdoutBuffer(false);
ConsoleStreamBuffer g_stderrBuffer(true);

}

namespace ConsoleOutput {

void Write(const std::wstring& text, bool error) {
    WriteText(GetOutputHandle(error), text);
}

void WriteLine(const std::wstring& text, bool error) {
    Write(text + L"\n", error);
}

void Install() {
    std::wcout.rdbuf(&g_stdoutBuffer);
    std::wcerr.rdbuf(&g_stderrBuffer);
}

}
