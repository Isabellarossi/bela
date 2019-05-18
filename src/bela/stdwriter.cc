///////////
#include <bela/base.hpp>
#include <bela/stdwriter.hpp>
#include <bela/codecvt.hpp>
#include <bela/strcat.hpp>
#include <io.h>

namespace bela {

enum class ConsoleMode {
  File, //
  TTY,
  Conhost,
  PTY
};

// Remove all color string
ssize_t WriteToLegacy(HANDLE hConsole, std::wstring_view sv) {
  // L"\x1b[0mzzz"
  constexpr const wchar_t sep = 0x1b;
  std::wstring buf;
  buf.reserve(sv.size());
  do {
    auto pos = sv.find(sep);
    if (pos == std::wstring_view::npos) {
      buf.append(sv);
      break;
    }
    buf.append(sv.substr(0, pos));
    sv.remove_prefix(pos + 1);
    pos = sv.find('m');
    if (pos == std::wstring::npos) {
      buf.push_back(sep);
      buf.append(sv);
      break;
    }
    sv.remove_prefix(pos + 1);
  } while (!sv.empty());
  DWORD dwWrite = 0;
  if (!WriteConsoleW(hConsole, buf.data(), (DWORD)buf.size(), &dwWrite,
                     nullptr)) {
    return -1;
  }
  return static_cast<ssize_t>(dwWrite);
}

static inline ssize_t WriteToFile(FILE *out, std::wstring_view sv) {
  auto s = bela::ToNarrow(sv);
  return static_cast<ssize_t>(fwrite(s.data(), 1, s.size(), out));
}

static inline ssize_t WriteToConsole(HANDLE hConsole, std::wstring_view sv) {
  DWORD dwWrite = 0;
  if (!WriteConsoleW(hConsole, sv.data(), (DWORD)sv.size(), &dwWrite,
                     nullptr)) {
    return -1;
  }
  return static_cast<ssize_t>(dwWrite);
}

class Adapter {
public:
  Adapter(const Adapter &) = delete;
  Adapter &operator=(const Adapter &) = delete;
  static Adapter &instance() {
    static Adapter a;
    return a;
  }
  std::wstring FileTypeName(FILE *file) const;
  ssize_t StdWriteConsole(HANDLE hFile, std::wstring_view sv,
                          ConsoleMode cm) const {
    if (cm == ConsoleMode::Conhost) {
      return WriteToLegacy(hFile, sv);
    }
    return WriteToConsole(hFile, sv);
  }
  ssize_t StdWrite(FILE *out, std::wstring_view sv) const {
    if (out == stderr &&
        (em == ConsoleMode::Conhost || em == ConsoleMode::PTY)) {
      return StdWriteConsole(hStderr, sv, em);
    }
    if (out == stdout &&
        (om == ConsoleMode::Conhost || em == ConsoleMode::PTY)) {
      return StdWriteConsole(hStdout, sv, om);
    }
    return WriteToFile(out, sv);
  }

private:
  ConsoleMode om;
  ConsoleMode em;
  HANDLE hStdout;
  HANDLE hStderr;
  void Initialize();
  Adapter() { Initialize(); }
};

inline bool EnableVirtualTerminal(HANDLE hFile) {
  DWORD dwMode = 0;
  if (!GetConsoleMode(hFile, &dwMode)) {
    return false;
  }
  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if (!SetConsoleMode(hFile, dwMode)) {
    return false;
  }
  return true;
}

ConsoleMode GetConsoleModeEx(HANDLE hFile) {
  //
  if (hFile == nullptr || hFile == INVALID_HANDLE_VALUE) {
    return ConsoleMode::File;
  }
  auto ft = GetFileType(hFile);
  if (ft == FILE_TYPE_DISK) {
    return ConsoleMode::File;
  }
  if (ft != FILE_TYPE_CHAR) {
    return ConsoleMode::TTY; // cygwin is pipe
  }
  if (EnableVirtualTerminal(hFile)) {
    return ConsoleMode::PTY;
  }
  return ConsoleMode::Conhost;
}

void Adapter::Initialize() {
  /*
  GetStdHandle: https://docs.microsoft.com/en-us/windows/console/getstdhandle
  If the function succeeds, the return value is a handle to the specified
device, or a redirected handle set by a previous call to SetStdHandle. The
handle has GENERIC_READ and GENERIC_WRITE access rights, unless the application
has used SetStdHandle to set a standard handle with lesser access.

If the function fails, the return value is INVALID_HANDLE_VALUE. To get extended
error information, call GetLastError.

If an application does not have associated standard handles, such as a service
running on an interactive desktop, and has not redirected them, the return value
is NULL
  */
  hStderr = GetStdHandle(STD_ERROR_HANDLE);
  em = GetConsoleModeEx(hStderr);
  hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
  om = GetConsoleModeEx(hStdout);
}

std::wstring FileTypeModeName(HANDLE hFile) {
  if (hFile == nullptr || hFile == INVALID_HANDLE_VALUE) {
    return L"UNKOWN";
  }
  switch (GetFileType(hFile)) {
  default:
    return L"UNKOWN";
  case FILE_TYPE_DISK:
    return L"Disk File";
  case FILE_TYPE_PIPE:
    return L"Pipe";
  case FILE_TYPE_CHAR:
    break;
  }
  DWORD dwMode = 0;
  if (GetConsoleMode(hFile, &dwMode) &&
      (dwMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
    return L"VT Mode Console";
  }
  return L"Legacy Console";
}

std::wstring FileTypeModeName(FILE *file) {
  auto hFile = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file)));
  return FileTypeModeName(hFile);
}

std::wstring Adapter::FileTypeName(FILE *file) const {
  if (file == stderr) {
    auto m = FileTypeModeName(hStderr);
    return m;
  }
  if (file == stdout) {
    auto m = FileTypeModeName(hStdout);
    return m;
  }
  return FileTypeModeName(file);
}

ssize_t StdWrite(FILE *out, std::wstring_view msg) {
  return Adapter::instance().StdWrite(out, msg);
}

std::wstring FileTypeName(FILE *file) {
  return Adapter::instance().FileTypeName(file);
}

} // namespace bela
