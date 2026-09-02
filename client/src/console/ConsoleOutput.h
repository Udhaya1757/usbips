#pragma once

#include <string>

namespace ConsoleOutput {

void Write(const std::wstring& text, bool error = false);
void WriteLine(const std::wstring& text, bool error = false);
void Install();

}
