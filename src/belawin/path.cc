////
#include <vector>
#include <bela/path.hpp>
#include <bela/strip.hpp>
#include <bela/strcat.hpp>
#include <bela/str_split.hpp>
#include <bela/ascii.hpp>
#include <bela/base.hpp>

namespace bela {
// https://googleprojectzero.blogspot.com/2016/02/the-definitive-guide-on-win32-to-nt.html
inline bool PathStripUNC(std::wstring_view &sv) {
  constexpr const std::wstring_view uncprefix = L"\\\\?\\";
  return bela::ConsumePrefix(&sv, uncprefix);
}
inline bool PathStripDriveLatter(std::wstring_view &sv, wchar_t &dl) {
  if (sv.size() < 2) {
    return false;
  }
  if (sv[1] == L':') {
    auto ch = sv[0];
    if (ch > 'a' && ch <= 'z') {
      dl = tolower(ch);
      sv.remove_prefix(2);
      return true;
    }
    if (ch >= 'A' && ch <= 'Z') {
      dl = ch;
      sv.remove_prefix(2);
      return true;
    }
  }

  return false;
}
using container_t = std::vector<std::wstring_view>;
bool PathSplit(std::wstring_view sv, container_t &output) {
  constexpr std::wstring_view dotdot = L"..";
  constexpr std::wstring_view dot = L".";
  size_t first = 0;
  while (first < sv.size()) {
    const auto second = sv.find_first_of(L"/\\", first);
    if (first != second) {
      auto s = sv.substr(first, second - first);
      if (s == dotdot) {
        if (output.empty()) {
          return false;
        }
        output.pop_back();
      } else if (s != dot) {
        output.emplace_back(s);
      }
    }
    if (second == std::wstring_view::npos) {
      break;
    }
    first = second + 1;
  }
  return true;
}

namespace path_internal {

std::wstring getcurrentdir() {
  auto l = GetCurrentDirectoryW(0, nullptr);
  if (l == 0) {
    return L"";
  }
  std::wstring s;
  s.resize(l);
  auto N = GetCurrentDirectoryW(l, s.data());
  s.resize(N);
  return s;
}

std::wstring PathCatPieces(bela::Span<std::wstring_view> pieces) {
  if (pieces.empty()) {
    return L"";
  }
  auto p0 = pieces[0];
  std::wstring p0raw;
  if (p0 == L".") {
    p0raw = getcurrentdir();
    p0 = p0raw;
  }
  auto IsUNC = PathStripUNC(p0);
  wchar_t latter = 0;
  auto haslatter = PathStripDriveLatter(p0, latter);
  container_t pv;
  if (!PathSplit(p0, pv)) {
    return L"";
  }
  for (size_t i = 1; i < pieces.size(); i++) {
    if (!PathSplit(pieces[i], pv)) {
      return L"";
    }
  }
  std::wstring s;
  size_t alsize = IsUNC ? 4 : 0;
  if (haslatter) {
    alsize += 2;
  }
  for (const auto p : pv) {
    alsize += p.size() + 1;
  }
  s.reserve(alsize);
  if (IsUNC) {
    s.append(L"\\\\?\\");
  }
  if (haslatter) {
    s.push_back(latter);
    s.push_back(L':');
  }
  for (const auto p : pv) {
    if (!s.empty()) {
      s.push_back(L'\\');
    }
    s.append(p);
  }
  return s;
}

} // namespace path_internal

bool PathExists(std::wstring_view src, FileAttribute fa) {
  // GetFileAttributesExW()
  auto a = GetFileAttributesW(src.data());
  if (a == INVALID_FILE_ATTRIBUTES) {
    return false;
  }
  return fa == FileAttribute::None ? true : ((static_cast<DWORD>(fa) & a) != 0);
}

inline bool PathFileIsExists(std::wstring_view file) {
  auto at = GetFileAttributesW(file.data());
  return (INVALID_FILE_ATTRIBUTES != at &&
          (at & FILE_ATTRIBUTE_DIRECTORY) == 0);
}
//
inline std::wstring GetEnv(std::wstring_view val) {
  auto len = GetEnvironmentVariableW(val.data(), nullptr, 0);
  if (len == 0) {
    return L"";
  }
  std::wstring s;
  s.resize(len + 1);
  len = GetEnvironmentVariableW(val.data(), s.data(), len + 1);
  if (len == 0) {
    return L"";
  }
  s.resize(len);
  return s;
}

bool HasExt(std::wstring_view file) {
  auto pos = file.find(L'.');
  if (pos == std::wstring_view::npos) {
    return false;
  }
  return (file.find_last_of(L":\\/") < pos);
}

bool FindExecutable(std::wstring_view file,
                    const std::vector<std::wstring> &exts, std::wstring &p) {
  if (HasExt(file) && PathFileIsExists(file)) {
    p = file;
    return true;
  }
  std::wstring newfile;
  newfile.reserve(file.size() + 8);
  newfile.assign(file);
  auto rawsize = newfile.size();
  for (const auto &e : exts) {
    // rawsize always < newfile.size();
    // std::char_traits::assign
    newfile.resize(rawsize);
    newfile.append(e);
    if (PathFileIsExists(newfile)) {
      p.assign(std::move(newfile));
      return true;
    }
  }
  return false;
}

bool ExecutableExistsInPath(std::wstring_view cmd, std::wstring &exe) {
  constexpr std::wstring_view defaultexts[] = {L".com", L".exe", L".bat",
                                               L".cmd"};
  std::wstring suffixwapper;
  std::vector<std::wstring> exts;
  auto pathext = GetEnv(L"PATHEXT");
  if (!pathext.empty()) {
    bela::AsciiStrToLower(&pathext); // tolower
    exts = bela::StrSplit(pathext, bela::ByChar(L';'), bela::SkipEmpty());
  } else {
    exts.assign(std::begin(defaultexts), std::end(defaultexts));
  }
  if (cmd.find_first_of(L":\\/") != std::wstring_view::npos) {
    return FindExecutable(cmd, exts, exe);
  }
  auto cwdfile = bela::PathCat(L".", cmd);
  if (FindExecutable(cwdfile, exts, exe)) {
    return true;
  }
  auto path = GetEnv(L"PATH");
  std::vector<std::wstring_view> pathv =
      bela::StrSplit(path, bela::ByChar(L';'), bela::SkipEmpty());
  for (auto p : pathv) {
    auto pfile = bela::PathCat(p, cmd);
    if (FindExecutable(pfile, exts, exe)) {
      return true;
    }
  }
  return false;
}

} // namespace bela
