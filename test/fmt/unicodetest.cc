///
#include <bela/stdwriter.hpp>
#include <bela/unicode.hpp>
#include <bela/strcat.hpp>

int wmain(int argc, wchar_t **argv) {
  const auto ux = "\xf0\x9f\x98\x81 UTF-8 text \xE3\x8D\xA4"; // force encode UTF-8
  const wchar_t wx[] = L"Engine \xD83D\xDEE0 中国";
  constexpr auto iscpp17 = __cplusplus >= 201703L;
  bela::FPrintF(stderr,
                L"Argc: %d Arg0: \x1b[32m%s\x1b[0m W: %s UTF-8: %s "
                L"__cplusplus: %d C++17: %b\n",
                argc, argv[0], wx, ux, __cplusplus, iscpp17);
  auto ws = bela::unicode::mbrtowc(ux);
  bela::FPrintF(stderr, L"Fast: %s\n", ws);
  auto u8s = bela::unicode::ToNarrow(wx);
  bela::FPrintF(stderr, L"Then: %s\n", u8s);
  char32_t em = 0x1F603; // 😃 U+1F603
  char32_t sh = 0x1F496; //  💖
  char32_t em2 = U'中';
  auto s = bela::StringCat(L"Look emoji -->", em, L" U+",
                           bela::AlphaNum(bela::Hex(em)));
  bela::FPrintF(stderr, L"emoji %c %c %U %U %s P: %p\n", em, sh, em, em2, s,
                &em);
  bela::FPrintF(stderr, L"hStderr Mode:    %s.\nhStdin Mode:     %s.\n",
                bela::FileTypeName(stderr), bela::FileTypeName(stdin));
  return 0;
}