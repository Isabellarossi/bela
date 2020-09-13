// Base https://github.com/mohabouje/WinToast
#ifndef BELA_WINTOAST_HPP
#define BELA_WINTOAST_HPP
#include "base.hpp"
#include <Windows.h>
#include <sdkddkver.h>
#include <WinUser.h>
#include <ShObjIdl.h>
#include <wrl/implements.h>
#include <wrl/event.h>
#include <windows.ui.notifications.h>
#include <strsafe.h>
#include <Psapi.h>
#include <ShlObj.h>
#include <roapi.h>
#include <propvarutil.h>
#include <functiondiscoverykeys.h>
#include <winstring.h>
#include <cstring>
#include <map>
// #pragma comment(lib,"shlwapi")
// #pragma comment(lib,"user32")

namespace bela::notice {
using namespace Microsoft::WRL;
using namespace ABI::Windows::Data::Xml::Dom;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::UI::Notifications;
using namespace Windows::Foundation;

class IWinToastHandler {
public:
  enum WinToastDismissalReason {
    UserCanceled = ToastDismissalReason::ToastDismissalReason_UserCanceled,
    ApplicationHidden = ToastDismissalReason::ToastDismissalReason_ApplicationHidden,
    TimedOut = ToastDismissalReason::ToastDismissalReason_TimedOut
  };
  virtual ~IWinToastHandler() = default;
  virtual void toastActivated() const = 0;
  virtual void toastActivated(int actionIndex) const = 0;
  virtual void toastDismissed(WinToastDismissalReason state) const = 0;
  virtual void toastFailed() const = 0;
};

class Template {
public:
  enum DurationEnum { System, Short, Long };
  enum AudioOptionEnum { Default = 0, Silent, Loop };
  enum TextField { FirstLine = 0, SecondLine, ThirdLine };
  enum TemplateType {
    ImageAndText01 = ToastTemplateType::ToastTemplateType_ToastImageAndText01,
    ImageAndText02 = ToastTemplateType::ToastTemplateType_ToastImageAndText02,
    ImageAndText03 = ToastTemplateType::ToastTemplateType_ToastImageAndText03,
    ImageAndText04 = ToastTemplateType::ToastTemplateType_ToastImageAndText04,
    Text01 = ToastTemplateType::ToastTemplateType_ToastText01,
    Text02 = ToastTemplateType::ToastTemplateType_ToastText02,
    Text03 = ToastTemplateType::ToastTemplateType_ToastText03,
    Text04 = ToastTemplateType::ToastTemplateType_ToastText04,
  };

  enum AudioSystemFile {
    DefaultSound,
    IM,
    Mail,
    Reminder,
    SMS,
    Alarm,
    Alarm2,
    Alarm3,
    Alarm4,
    Alarm5,
    Alarm6,
    Alarm7,
    Alarm8,
    Alarm9,
    Alarm10,
    Call,
    Call1,
    Call2,
    Call3,
    Call4,
    Call5,
    Call6,
    Call7,
    Call8,
    Call9,
    Call10,
  };
  Template(TemplateType type_ = TemplateType::ImageAndText02) : type(type_) {
    static constexpr std::size_t TextFieldsCount[] = {1, 2, 2, 3, 1, 2, 2, 3};
    fields = std::vector<std::wstring>(TextFieldsCount[type], L"");
  }
  void SetTextField(std::wstring_view txt, Template::TextField pos) {
    const auto position = static_cast<std::size_t>(pos);
    if (position < fields.size()) {
      fields[position] = txt;
    }
  }
  void SetAudioPath(AudioSystemFile file);
  std::wstring_view AudioPath() const { return audioPath; }
  std::wstring &AudioPath() { return audioPath; }
  const AudioOptionEnum AudioOption() const { return audioOption; }
  AudioOptionEnum &AudioOption() { return audioOption; }
  void First(std::wstring_view text) { SetTextField(text, FirstLine); }
  void Second(std::wstring_view text) { SetTextField(text, SecondLine); }
  void Third(std::wstring_view text) { SetTextField(text, ThirdLine); }
  const auto &Fields() const { return fields; }
  std::wstring_view Field(TextField pos) const {
    const auto position = static_cast<std::size_t>(pos);
    if (position < fields.size()) {
      return fields[position];
    }
    return L"";
  }
  const auto &Actions() const { return actions; }
  void AddAction(std::wstring_view label) { actions.emplace_back(label); }
  std::wstring_view ActionLabel(std::size_t pos) const {
    if (pos < actions.size()) {
      return actions[pos];
    }
    return L"";
  }
  bool HasImage() const { return type < TemplateType::Text01; }
  std::wstring_view ImagePath() const { return imagePath; }
  std::wstring &ImagePath() { return imagePath; }

  std::wstring_view AttributionText() const { return attributionText; }
  std::wstring &AttributionText() { return attributionText; }

  const INT64 Expiration() const { return expiration; }
  INT64 &Expiration() { return expiration; }
  TemplateType Type() const { type; }
  const DurationEnum Duration() const { return duration; }
  DurationEnum &Duration() { return duration; }

private:
  std::vector<std::wstring> fields;
  std::vector<std::wstring> actions;

  std::wstring imagePath;
  std::wstring audioPath;
  std::wstring attributionText;
  INT64 expiration{0};
  TemplateType type{TemplateType::Text01};
  AudioOptionEnum audioOption{Template::AudioOptionEnum::Default};
  DurationEnum duration{DurationEnum::System};
};

inline std::wstring MakeAppUserModelID(std::wstring_view companyName, std::wstring_view productName,
                                       std::wstring_view subProduct = L"", std::wstring_view versionInformation = L"") {
  auto amui = bela::StringCat(companyName, L".", productName);
  if (!subProduct.empty()) {
    bela::StrAppend(&amui, L".", subProduct);
  }
  if (!versionInformation.empty()) {
    bela::StrAppend(&amui, L".", versionInformation);
  }
  if (amui.size() > SCHAR_MAX) {
    amui.resize(SCHAR_MAX);
  }
  return amui;
}

class WinToast {
public:
  WinToast(const WinToast &) = delete;
  WinToast &operator=(const WinToast &) = delete;
  WinToast() = default;
  ~WinToast() {
    if (coinitialized) {
      CoUninitialize();
    }
  }
  static bool IsCompatible();
  static bool IsSupportingModernFeatures();
  bool Initialize(bela::error_code &ec);
  const bela::error_code &ErrorCode() const { return ec; }
  std::wstring_view AppName() const { return appName; }
  std::wstring &AppName() { return appName; }
  std::wstring_view AppUserModelID() const { return appUserModelID; }
  std::wstring &AppUserModelID() { return appUserModelID; }
  bool Hide(int64_t id, bela::error_code &ec);
  int64_t Display(const Template &toast, IWinToastHandler *handler, bela::error_code &ec);
  void Clear();

private:
  bool initialized{false};
  bool coinitialized{false};
  std::wstring appName;
  std::wstring appUserModelID;
  std::map<INT64, ComPtr<IToastNotification>> buffer{};
  bela::error_code ec;
};
} // namespace bela::notice

#endif