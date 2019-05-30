///
/// unicode escape L"CH\u2082O\u2083" => L"CH₂O"
#include <bela/escaping.hpp>
#include <bela/stdwriter.hpp>

int wmain() {
  const wchar_t *msg = L"H\\u2082O \\xE3\\x8D\\xA4 \\U0001F496";
  std::wstring dest;
  if (bela::CUnescape(msg, &dest)) {
    bela::FPrintF(stderr, L"%s\n", dest);
  }
  return 0;
}
