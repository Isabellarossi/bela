///
#include <bela/wintoast.hpp>

namespace bela::notice {
namespace details {
//
// HRESULT dlopen()
template <typename Function> HRESULT dlfunc(HINSTANCE library, LPCSTR name, Function &func) {
  if (!library) {
    return E_INVALIDARG;
  }
  func = reinterpret_cast<Function>(GetProcAddress(library, name));
  return (func != nullptr) ? S_OK : E_FAIL;
}

typedef HRESULT(WINAPI *f_SetCurrentProcessExplicitAppUserModelID)(__in PCWSTR AppID);
typedef HRESULT(WINAPI *f_PropVariantToString)(_In_ REFPROPVARIANT propvar, _Out_writes_(cch) PWSTR psz, _In_ UINT cch);
typedef HRESULT(WINAPI *f_RoGetActivationFactory)(_In_ HSTRING activatableClassId, _In_ REFIID iid,
                                                  _COM_Outptr_ void **factory);
typedef HRESULT(WINAPI *f_WindowsCreateStringReference)(
    _In_reads_opt_(length + 1) PCWSTR sourceString, UINT32 length, _Out_ HSTRING_HEADER *hstringHeader,
    _Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING *string);
typedef PCWSTR(WINAPI *f_WindowsGetStringRawBuffer)(_In_ HSTRING string, _Out_ UINT32 *length);
typedef HRESULT(WINAPI *f_WindowsDeleteString)(_In_opt_ HSTRING string);
static f_SetCurrentProcessExplicitAppUserModelID SetCurrentProcessExplicitAppUserModelID;
static f_PropVariantToString PropVariantToString;
static f_RoGetActivationFactory RoGetActivationFactory;
static f_WindowsCreateStringReference WindowsCreateStringReference;
static f_WindowsGetStringRawBuffer WindowsGetStringRawBuffer;
static f_WindowsDeleteString WindowsDeleteString;
template <class T>
_Check_return_ __inline HRESULT _1_GetActivationFactory(_In_ HSTRING activatableClassId, _COM_Outptr_ T **factory) {
  return RoGetActivationFactory(activatableClassId, IID_INS_ARGS(factory));
}

template <typename T>
inline HRESULT Wrap_GetActivationFactory(_In_ HSTRING activatableClassId,
                                         _Inout_ Microsoft::WRL::Details::ComPtrRef<T> factory) noexcept {
  return _1_GetActivationFactory(activatableClassId, factory.ReleaseAndGetAddressOf());
}

inline HRESULT initialize() {
  HINSTANCE LibShell32 = LoadLibraryW(L"SHELL32.DLL");
  HRESULT hr = dlfunc(LibShell32, "SetCurrentProcessExplicitAppUserModelID", SetCurrentProcessExplicitAppUserModelID);
  if (SUCCEEDED(hr)) {
    HINSTANCE LibPropSys = LoadLibraryW(L"PROPSYS.DLL");
    hr = dlfunc(LibPropSys, "PropVariantToString", PropVariantToString);
    if (SUCCEEDED(hr)) {
      HINSTANCE LibComBase = LoadLibraryW(L"COMBASE.DLL");
      const bool succeded =
          SUCCEEDED(dlfunc(LibComBase, "RoGetActivationFactory", RoGetActivationFactory)) &&
          SUCCEEDED(dlfunc(LibComBase, "WindowsCreateStringReference", WindowsCreateStringReference)) &&
          SUCCEEDED(dlfunc(LibComBase, "WindowsGetStringRawBuffer", WindowsGetStringRawBuffer)) &&
          SUCCEEDED(dlfunc(LibComBase, "WindowsDeleteString", WindowsDeleteString));
      return succeded ? S_OK : E_FAIL;
    }
  }
  return hr;
}
#define STATUS_SUCCESS (0x00000000)
typedef LONG NTSTATUS, *PNTSTATUS;
typedef NTSTATUS(WINAPI *RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
inline RTL_OSVERSIONINFOW getRealOSVersion() {
  HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
  if (hMod) {
    RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
    if (fxPtr != nullptr) {
      RTL_OSVERSIONINFOW rovi = {0};
      rovi.dwOSVersionInfoSize = sizeof(rovi);
      if (STATUS_SUCCESS == fxPtr(&rovi)) {
        return rovi;
      }
    }
  }
  RTL_OSVERSIONINFOW rovi = {0};
  return rovi;
}

inline PCWSTR AsString(ComPtr<IXmlDocument> &xmlDocument) {
  HSTRING xml;
  ComPtr<IXmlNodeSerializer> ser;
  HRESULT hr = xmlDocument.As<IXmlNodeSerializer>(&ser);
  hr = ser->GetXml(&xml);
  if (SUCCEEDED(hr)) {
    return WindowsGetStringRawBuffer(xml, nullptr);
  }
  return nullptr;
}

inline PCWSTR AsString(HSTRING hstr) { return WindowsGetStringRawBuffer(hstr, nullptr); }

} // namespace details

class winstring {
public:
  winstring(_In_reads_(length) PCWSTR stringRef, _In_ UINT32 length) noexcept {
    HRESULT hr = details::WindowsCreateStringReference(stringRef, length, &_header, &_hstring);
    if (FAILED(hr)) {
      RaiseException(static_cast<DWORD>(STATUS_INVALID_PARAMETER), EXCEPTION_NONCONTINUABLE, 0, nullptr);
    }
  }

  winstring(_In_ std::wstring_view stringView) noexcept {
    HRESULT hr = details::WindowsCreateStringReference(stringView.data(), static_cast<UINT32>(stringView.size()),
                                                       &_header, &_hstring);
    if (FAILED(hr)) {
      RaiseException(static_cast<DWORD>(STATUS_INVALID_PARAMETER), EXCEPTION_NONCONTINUABLE, 0, nullptr);
    }
  }

  ~winstring() { details::WindowsDeleteString(_hstring); }

  inline HSTRING Get() const noexcept { return _hstring; }

private:
  HSTRING _hstring{nullptr};
  HSTRING_HEADER _header;
};

class InternalDateTime : public IReference<DateTime> {
public:
  static INT64 Now() {
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    return ((((INT64)now.dwHighDateTime) << 32) | now.dwLowDateTime);
  }

  InternalDateTime(DateTime dateTime) : _dateTime(dateTime) {}

  InternalDateTime(INT64 millisecondsFromNow) { _dateTime.UniversalTime = Now() + millisecondsFromNow * 10000; }

  virtual ~InternalDateTime() = default;

  operator INT64() { return _dateTime.UniversalTime; }

  HRESULT STDMETHODCALLTYPE get_Value(DateTime *dateTime) {
    *dateTime = _dateTime;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(const IID &riid, void **ppvObject) {
    if (!ppvObject) {
      return E_POINTER;
    }
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IReference<DateTime>)) {
      *ppvObject = static_cast<IUnknown *>(static_cast<IReference<DateTime> *>(this));
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE Release() { return 1; }

  ULONG STDMETHODCALLTYPE AddRef() { return 2; }

  HRESULT STDMETHODCALLTYPE GetIids(ULONG *, IID **) { return E_NOTIMPL; }

  HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING *) { return E_NOTIMPL; }

  HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel *) { return E_NOTIMPL; }

protected:
  DateTime _dateTime;
};

inline HRESULT setNodeStringValue(std::wstring_view str, IXmlNode *node, IXmlDocument *xml) {
  ComPtr<IXmlText> textNode;
  HRESULT hr = xml->CreateTextNode(winstring(str).Get(), &textNode);
  if (SUCCEEDED(hr)) {
    ComPtr<IXmlNode> stringNode;
    hr = textNode.As(&stringNode);
    if (SUCCEEDED(hr)) {
      ComPtr<IXmlNode> appendedChild;
      hr = node->AppendChild(stringNode.Get(), &appendedChild);
    }
  }
  return hr;
}

inline HRESULT setEventHandlers(_In_ IToastNotification *notification,
                                _In_ std::shared_ptr<IWinToastHandler> eventHandler, _In_ INT64 expirationTime) {
  EventRegistrationToken activatedToken, dismissedToken, failedToken;
  HRESULT hr = notification->add_Activated(
      Callback<Implements<RuntimeClassFlags<ClassicCom>, ITypedEventHandler<ToastNotification *, IInspectable *>>>(
          [eventHandler](IToastNotification *, IInspectable *inspectable) {
            IToastActivatedEventArgs *activatedEventArgs;
            HRESULT hr = inspectable->QueryInterface(&activatedEventArgs);
            if (SUCCEEDED(hr)) {
              HSTRING argumentsHandle;
              hr = activatedEventArgs->get_Arguments(&argumentsHandle);
              if (SUCCEEDED(hr)) {
                PCWSTR arguments = details::AsString(argumentsHandle);
                if (arguments && *arguments) {
                  eventHandler->toastActivated(static_cast<int>(wcstol(arguments, nullptr, 10)));
                  return S_OK;
                }
              }
            }
            eventHandler->toastActivated();
            return S_OK;
          })
          .Get(),
      &activatedToken);

  if (SUCCEEDED(hr)) {
    hr = notification->add_Dismissed(
        Callback<Implements<RuntimeClassFlags<ClassicCom>,
                            ITypedEventHandler<ToastNotification *, ToastDismissedEventArgs *>>>(
            [eventHandler, expirationTime](IToastNotification *, IToastDismissedEventArgs *e) {
              ToastDismissalReason reason;
              if (SUCCEEDED(e->get_Reason(&reason))) {
                if (reason == ToastDismissalReason_UserCanceled && expirationTime &&
                    InternalDateTime::Now() >= expirationTime)
                  reason = ToastDismissalReason_TimedOut;
                eventHandler->toastDismissed(static_cast<IWinToastHandler::WinToastDismissalReason>(reason));
              }
              return S_OK;
            })
            .Get(),
        &dismissedToken);
    if (SUCCEEDED(hr)) {
      hr = notification->add_Failed(
          Callback<Implements<RuntimeClassFlags<ClassicCom>,
                              ITypedEventHandler<ToastNotification *, ToastFailedEventArgs *>>>(
              [eventHandler](IToastNotification *, IToastFailedEventArgs *) {
                eventHandler->toastFailed();
                return S_OK;
              })
              .Get(),
          &failedToken);
    }
  }
  return hr;
}

inline HRESULT addAttribute(_In_ IXmlDocument *xml, std::wstring_view name, IXmlNamedNodeMap *attributeMap) {
  ComPtr<ABI::Windows::Data::Xml::Dom::IXmlAttribute> srcAttribute;
  HRESULT hr = xml->CreateAttribute(winstring(name).Get(), &srcAttribute);
  if (SUCCEEDED(hr)) {
    ComPtr<IXmlNode> node;
    hr = srcAttribute.As(&node);
    if (SUCCEEDED(hr)) {
      ComPtr<IXmlNode> pNode;
      hr = attributeMap->SetNamedItem(node.Get(), &pNode);
    }
  }
  return hr;
}

inline HRESULT createElement(_In_ IXmlDocument *xml, _In_ std::wstring_view root_node,
                             _In_ std::wstring_view element_name,
                             _In_ const std::vector<std::wstring> &attribute_names) {
  ComPtr<IXmlNodeList> rootList;
  HRESULT hr = xml->GetElementsByTagName(winstring(root_node).Get(), &rootList);
  if (SUCCEEDED(hr)) {
    ComPtr<IXmlNode> root;
    hr = rootList->Item(0, &root);
    if (SUCCEEDED(hr)) {
      ComPtr<ABI::Windows::Data::Xml::Dom::IXmlElement> audioElement;
      hr = xml->CreateElement(winstring(element_name).Get(), &audioElement);
      if (SUCCEEDED(hr)) {
        ComPtr<IXmlNode> audioNodeTmp;
        hr = audioElement.As(&audioNodeTmp);
        if (SUCCEEDED(hr)) {
          ComPtr<IXmlNode> audioNode;
          hr = root->AppendChild(audioNodeTmp.Get(), &audioNode);
          if (SUCCEEDED(hr)) {
            ComPtr<IXmlNamedNodeMap> attributes;
            hr = audioNode->get_Attributes(&attributes);
            if (SUCCEEDED(hr)) {
              for (const auto &it : attribute_names) {
                hr = addAttribute(xml, it, attributes.Get());
              }
            }
          }
        }
      }
    }
  }
  return hr;
}

void Template::SetAudioPath(AudioSystemFile file) {
  static constexpr struct audio_system_file {
    AudioSystemFile id;
    std::wstring_view path;
  } Files[] = {
      {AudioSystemFile::DefaultSound, L"ms-winsoundevent:Notification.Default"},
      {AudioSystemFile::IM, L"ms-winsoundevent:Notification.IM"},
      {AudioSystemFile::Mail, L"ms-winsoundevent:Notification.Mail"},
      {AudioSystemFile::Reminder, L"ms-winsoundevent:Notification.Reminder"},
      {AudioSystemFile::SMS, L"ms-winsoundevent:Notification.SMS"},
      {AudioSystemFile::Alarm, L"ms-winsoundevent:Notification.Looping.Alarm"},
      {AudioSystemFile::Alarm2, L"ms-winsoundevent:Notification.Looping.Alarm2"},
      {AudioSystemFile::Alarm3, L"ms-winsoundevent:Notification.Looping.Alarm3"},
      {AudioSystemFile::Alarm4, L"ms-winsoundevent:Notification.Looping.Alarm4"},
      {AudioSystemFile::Alarm5, L"ms-winsoundevent:Notification.Looping.Alarm5"},
      {AudioSystemFile::Alarm6, L"ms-winsoundevent:Notification.Looping.Alarm6"},
      {AudioSystemFile::Alarm7, L"ms-winsoundevent:Notification.Looping.Alarm7"},
      {AudioSystemFile::Alarm8, L"ms-winsoundevent:Notification.Looping.Alarm8"},
      {AudioSystemFile::Alarm9, L"ms-winsoundevent:Notification.Looping.Alarm9"},
      {AudioSystemFile::Alarm10, L"ms-winsoundevent:Notification.Looping.Alarm10"},
      {AudioSystemFile::Call, L"ms-winsoundevent:Notification.Looping.Call"},
      {AudioSystemFile::Call1, L"ms-winsoundevent:Notification.Looping.Call1"},
      {AudioSystemFile::Call2, L"ms-winsoundevent:Notification.Looping.Call2"},
      {AudioSystemFile::Call3, L"ms-winsoundevent:Notification.Looping.Call3"},
      {AudioSystemFile::Call4, L"ms-winsoundevent:Notification.Looping.Call4"},
      {AudioSystemFile::Call5, L"ms-winsoundevent:Notification.Looping.Call5"},
      {AudioSystemFile::Call6, L"ms-winsoundevent:Notification.Looping.Call6"},
      {AudioSystemFile::Call7, L"ms-winsoundevent:Notification.Looping.Call7"},
      {AudioSystemFile::Call8, L"ms-winsoundevent:Notification.Looping.Call8"},
      {AudioSystemFile::Call9, L"ms-winsoundevent:Notification.Looping.Call9"},
      {AudioSystemFile::Call10, L"ms-winsoundevent:Notification.Looping.Call10"},
  };
  for (auto &e : Files) {
    if (e.id == file) {
      audioPath = e.path;
    }
  }
}

bool WinToast::IsCompatible() {
  details::initialize();
  return !((details::SetCurrentProcessExplicitAppUserModelID == nullptr) || (details::PropVariantToString == nullptr) ||
           (details::RoGetActivationFactory == nullptr) || (details::WindowsCreateStringReference == nullptr) ||
           (details::WindowsDeleteString == nullptr));
}
bool WinToast::IsSupportingModernFeatures() {
  constexpr auto MinimumSupportedVersion = 6;
  return details::getRealOSVersion().dwMajorVersion > MinimumSupportedVersion;
}

bool WinToast::Initialize(bela::error_code &ec) {
  if (!IsCompatible()) {
    ec = bela::make_error_code(1, L"system not supported");
    return false;
  }
  if (appName.empty() || appUserModelID.empty()) {
    ec = bela::make_error_code(1, L"while initializing, did you set up a valid AppUserModelID and App name?");
    return false;
  }
  initialized = true;
  return initialized;
}

HRESULT SetTextFieldHelper(_In_ IXmlDocument *xml, _In_ std::wstring_view text, _In_ UINT32 pos) {
  ComPtr<IXmlNodeList> nodeList;
  HRESULT hr = S_OK;
  if (hr = xml->GetElementsByTagName(winstring(L"text").Get(), &nodeList); SUCCEEDED(hr)) {
    ComPtr<IXmlNode> node;
    if (hr = nodeList->Item(pos, &node); SUCCEEDED(hr)) {
      hr = setNodeStringValue(text, node.Get(), xml);
    }
  }
  return hr;
}

//
// Available as of Windows 10 Anniversary Update
// Ref: https://docs.microsoft.com/en-us/windows/uwp/design/shell/tiles-and-notifications/adaptive-interactive-toasts
//
// NOTE: This will add a new text field, so be aware when iterating over
//       the toast's text fields or getting a count of them.
//
HRESULT SetAttributionTextFieldHelper(IXmlDocument *xml, _In_ const std::wstring_view text) {
  createElement(xml, L"binding", L"text", {L"placement"});
  ComPtr<IXmlNodeList> nodeList;
  HRESULT hr = xml->GetElementsByTagName(winstring(L"text").Get(), &nodeList);
  if (FAILED(hr)) {
    return hr;
  }
  UINT32 nodeListLength;
  if (hr = nodeList->get_Length(&nodeListLength); FAILED(hr)) {
    return hr;
  }
  for (UINT32 i = 0; i < nodeListLength; i++) {
    ComPtr<IXmlNode> textNode;
    hr = nodeList->Item(i, &textNode);
    if (SUCCEEDED(hr)) {
      ComPtr<IXmlNamedNodeMap> attributes;
      hr = textNode->get_Attributes(&attributes);
      if (SUCCEEDED(hr)) {
        ComPtr<IXmlNode> editedNode;
        if (SUCCEEDED(hr)) {
          hr = attributes->GetNamedItem(winstring(L"placement").Get(), &editedNode);
          if (FAILED(hr) || !editedNode) {
            continue;
          }
          hr = setNodeStringValue(L"attribution", editedNode.Get(), xml);
          if (SUCCEEDED(hr)) {
            return SetTextFieldHelper(xml, text, i);
          }
        }
      }
    }
  }
  return S_OK;
}

HRESULT AddDurationHelper(IXmlDocument *xml, std::wstring_view duration) {
  ComPtr<IXmlNodeList> nodeList;
  HRESULT hr = xml->GetElementsByTagName(winstring(L"toast").Get(), &nodeList);
  if (SUCCEEDED(hr)) {
    UINT32 length;
    hr = nodeList->get_Length(&length);
    if (SUCCEEDED(hr)) {
      ComPtr<IXmlNode> toastNode;
      hr = nodeList->Item(0, &toastNode);
      if (SUCCEEDED(hr)) {
        ComPtr<IXmlElement> toastElement;
        hr = toastNode.As(&toastElement);
        if (SUCCEEDED(hr)) {
          hr = toastElement->SetAttribute(winstring(L"duration").Get(), winstring(duration).Get());
        }
      }
    }
  }
  return hr;
}

HRESULT AddImagePath(IXmlDocument *xml, std::wstring_view path) {
  auto imagePath = bela::StringCat(L"file:///", path);
  ComPtr<IXmlNodeList> nodeList;
  HRESULT hr = xml->GetElementsByTagName(winstring(L"image").Get(), &nodeList);
  if (FAILED(hr)) {
    return hr;
  }
  ComPtr<IXmlNode> node;
  if (hr = nodeList->Item(0, &node); FAILED(hr)) {
    return hr;
  }
  ComPtr<IXmlNamedNodeMap> attributes;
  if (hr = node->get_Attributes(&attributes); FAILED(hr)) {
    return hr;
  }
  ComPtr<IXmlNode> editedNode;
  if (hr = attributes->GetNamedItem(winstring(L"src").Get(), &editedNode); FAILED(hr)) {
    return hr;
  }
  setNodeStringValue(imagePath, editedNode.Get(), xml);
  return S_OK;
}

HRESULT SetAudioFieldHelper(_In_ IXmlDocument *xml, _In_ std::wstring_view path, Template::AudioOptionEnum option) {
  std::vector<std::wstring> attrs;
  if (!path.empty()) {
    attrs.emplace_back(L"src");
  }
  if (option == Template::AudioOptionEnum::Loop) {
    attrs.emplace_back(L"loop");
  }
  if (option == Template::AudioOptionEnum::Silent) {
    attrs.emplace_back(L"silent");
  }
  createElement(xml, L"toast", L"audio", attrs);

  ComPtr<IXmlNodeList> nodeList;
  HRESULT hr = xml->GetElementsByTagName(winstring(L"audio").Get(), &nodeList);
  if (FAILED(hr)) {
    return hr;
  }
  ComPtr<IXmlNode> node;
  if (hr = nodeList->Item(0, &node); SUCCEEDED(hr)) {
    return hr;
  }
  ComPtr<IXmlNamedNodeMap> attributes;
  if (hr = node->get_Attributes(&attributes); FAILED(hr)) {
    return hr;
  }
  ComPtr<IXmlNode> editedNode;
  if (!path.empty()) {
    if (SUCCEEDED(hr)) {
      hr = attributes->GetNamedItem(winstring(L"src").Get(), &editedNode);
      if (SUCCEEDED(hr)) {
        hr = setNodeStringValue(path, editedNode.Get(), xml);
      }
    }
  }
  if (SUCCEEDED(hr)) {
    switch (option) {
    case Template::AudioOptionEnum::Loop:
      hr = attributes->GetNamedItem(winstring(L"loop").Get(), &editedNode);
      if (SUCCEEDED(hr)) {
        hr = setNodeStringValue(L"true", editedNode.Get(), xml);
      }
      break;
    case Template::AudioOptionEnum::Silent:
      hr = attributes->GetNamedItem(winstring(L"silent").Get(), &editedNode);
      if (SUCCEEDED(hr)) {
        hr = setNodeStringValue(L"true", editedNode.Get(), xml);
      }
    default:
      break;
    }
  }
  return hr;
}

HRESULT AddActionHelper(IXmlDocument *xml, std::wstring_view content, std::wstring_view arguments) {
  ComPtr<IXmlNodeList> nodeList;
  HRESULT hr = xml->GetElementsByTagName(winstring(L"actions").Get(), &nodeList);
  if (FAILED(hr)) {
    return hr;
  }
  UINT32 length;
  if (hr = nodeList->get_Length(&length); FAILED(hr)) {
    return hr;
  }
  ComPtr<IXmlNode> actionsNode;
  if (length > 0) {
    hr = nodeList->Item(0, &actionsNode);
  } else {
    hr = xml->GetElementsByTagName(winstring(L"toast").Get(), &nodeList);
    if (SUCCEEDED(hr)) {
      hr = nodeList->get_Length(&length);
      if (SUCCEEDED(hr)) {
        ComPtr<IXmlNode> toastNode;
        hr = nodeList->Item(0, &toastNode);
        if (SUCCEEDED(hr)) {
          ComPtr<IXmlElement> toastElement;
          hr = toastNode.As(&toastElement);
          if (SUCCEEDED(hr)) {
            hr = toastElement->SetAttribute(winstring(L"template").Get(), winstring(L"ToastGeneric").Get());
          }
          if (SUCCEEDED(hr)) {
            hr = toastElement->SetAttribute(winstring(L"duration").Get(), winstring(L"long").Get());
          }
          if (SUCCEEDED(hr)) {
            ComPtr<IXmlElement> actionsElement;
            hr = xml->CreateElement(winstring(L"actions").Get(), &actionsElement);
            if (SUCCEEDED(hr)) {
              hr = actionsElement.As(&actionsNode);
              if (SUCCEEDED(hr)) {
                ComPtr<IXmlNode> appendedChild;
                hr = toastNode->AppendChild(actionsNode.Get(), &appendedChild);
              }
            }
          }
        }
      }
    }
  }
  if (SUCCEEDED(hr)) {
    ComPtr<IXmlElement> actionElement;
    hr = xml->CreateElement(winstring(L"action").Get(), &actionElement);
    if (SUCCEEDED(hr)) {
      hr = actionElement->SetAttribute(winstring(L"content").Get(), winstring(content).Get());
    }
    if (SUCCEEDED(hr)) {
      hr = actionElement->SetAttribute(winstring(L"arguments").Get(), winstring(arguments).Get());
    }
    if (SUCCEEDED(hr)) {
      ComPtr<IXmlNode> actionNode;
      hr = actionElement.As(&actionNode);
      if (SUCCEEDED(hr)) {
        ComPtr<IXmlNode> appendedChild;
        hr = actionsNode->AppendChild(actionNode.Get(), &appendedChild);
      }
    }
  }
  return hr;
}

int64_t WinToast::Display(const Template &toast, IWinToastHandler *handler, bela::error_code &ec) {
  if (!initialized) {
    ec = bela::make_error_code(1, L"when launching the toast. WinToast is not initialized.");
    return -1;
  }
  if (handler == nullptr) {
    ec = bela::make_error_code(1, L"when launching the toast. Handler cannot be nullptr.");
    return -1;
  }
  ComPtr<IToastNotificationManagerStatics> notificationManager;
  HRESULT hr = details::Wrap_GetActivationFactory(
      winstring(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(), &notificationManager);
  if (FAILED(hr)) {
    bela::make_system_error_code(L"IToastNotificationManagerStatics ");
    return -1;
  }
  ComPtr<IToastNotifier> notifier;
  hr = notificationManager->CreateToastNotifierWithId(winstring(appUserModelID).Get(), &notifier);
  if (FAILED(hr)) {
    bela::make_system_error_code(L"CreateToastNotifierWithId ");
    return -1;
  }
  ComPtr<IToastNotificationFactory> notificationFactory;
  hr = details::Wrap_GetActivationFactory(winstring(RuntimeClass_Windows_UI_Notifications_ToastNotification).Get(),
                                          &notificationFactory);
  if (FAILED(hr)) {
    bela::make_system_error_code(L"IToastNotificationFactory ");
    return -1;
  }
  ComPtr<IXmlDocument> xmlDocument;
  HRESULT hr = notificationManager->GetTemplateContent(ToastTemplateType(toast.Type()), &xmlDocument);
  if (FAILED(hr)) {
    bela::make_system_error_code(L"GetTemplateContent ");
    return -1;
  }
  auto fieldcount = static_cast<uint32_t>(toast.Fields().size());
  for (uint32_t i = 0; i < fieldcount; i++) {
    hr = SetTextFieldHelper(xmlDocument.Get(), toast.Field(Template::TextField(i)), i);
    if (FAILED(hr)) {
      break;
    }
  }
  if (FAILED(hr)) {
    bela::make_system_error_code(L"SetTextFieldHelper ");
    return -1;
  }
  if (IsSupportingModernFeatures()) {
    if (!toast.AttributionText().empty()) {
      hr = SetAttributionTextFieldHelper(xmlDocument.Get(), toast.AttributionText());
    }
    auto actionCount = static_cast<uint32_t>(toast.Actions().size());
    for (uint32_t i = 0; i < actionCount; i++) {
      hr = AddActionHelper(xmlDocument.Get(), toast.ActionLabel(i), bela::AlphaNum(i).Piece());
      if (FAILED(hr)) {
        break;
      }
    }
    if (SUCCEEDED(hr)) {
      hr = (toast.AudioPath().empty() && toast.AudioOption() == Template::AudioOptionEnum::Default)
               ? hr
               : SetAudioFieldHelper(xmlDocument.Get(), toast.AudioPath(), toast.AudioOption());
    }

    if (SUCCEEDED(hr) && toast.Duration() != Template::DurationEnum::System) {
      hr = AddDurationHelper(xmlDocument.Get(),
                             (toast.Duration() == Template::DurationEnum::Short) ? L"short" : L"long");
    }
  }
  hr = toast.HasImage() ? AddImagePath(xmlDocument.Get(), toast.ImagePath()) : hr;
  if (FAILED(hr)) {
    ec = bela::make_system_error_code(L"AddImagePath ");
    return -1;
  }
  ComPtr<IToastNotification> notification;
  hr = notificationFactory->CreateToastNotification(xmlDocument.Get(), &notification);
  if (FAILED(hr)) {
    ec = bela::make_system_error_code(L"CreateToastNotification ");
    return -1;
  }
  INT64 expiration = 0, relativeExpiration = toast.Expiration();
  if (relativeExpiration > 0) {
    InternalDateTime expirationDateTime(relativeExpiration);
    expiration = expirationDateTime;
    if (hr = notification->put_ExpirationTime(&expirationDateTime); FAILED(hr)) {
      bela::make_system_error_code(L"put_ExpirationTime ");
      return -1;
    }
  }
  if (hr = setEventHandlers(notification.Get(), std::shared_ptr<IWinToastHandler>(handler), expiration); FAILED(hr)) {
    ec = bela::make_system_error_code(L"setEventHandlers ");
    return -1;
  }
  GUID guid;
  if (hr = CoCreateGuid(&guid); FAILED(hr)) {
    ec = bela::make_system_error_code(L"CoCreateGuid ");
    return -1;
  }
  auto id = guid.Data1;
  buffer[id] = notification;
  if (hr = notifier->Show(notification.Get()); FAILED(hr)) {
    ec = bela::make_system_error_code(L"notifier->Show ");
    return -1;
  }
  return id;
}

ComPtr<IToastNotifier> notifier(std::wstring_view appUserModelID, bool *succeded) {
  ComPtr<IToastNotificationManagerStatics> notificationManager;
  ComPtr<IToastNotifier> notifier;
  HRESULT hr = details::Wrap_GetActivationFactory(
      winstring(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(), &notificationManager);
  if (SUCCEEDED(hr)) {
    hr = notificationManager->CreateToastNotifierWithId(winstring(appUserModelID).Get(), &notifier);
  }
  *succeded = SUCCEEDED(hr);
  return notifier;
}
bool WinToast::Hide(int64_t id, bela::error_code &ec) {
  if (!initialized) {
    ec = bela::make_error_code(1, L"when hiding the toast. WinToast is not initialized.");
    return false;
  }
  if (buffer.find(id) == buffer.end()) {
    ec = bela::make_error_code(1, L"when hiding the toast. WinToast ID not found.");
    return false;
  }
  auto succeded = false;
  auto notify = notifier(appUserModelID, &succeded);
  if (!succeded) {
    ec = bela::make_error_code(1, L"when hiding the toast. WinToast is not succeded.");
    return false;
  }
  auto result = notify->Hide(buffer[id].Get());
  buffer.erase(id);
  if (FAILED(result)) {
    ec = bela::make_system_error_code(L"WinToast hide ");
    return false;
  }
  return true;
}

void WinToast::Clear() {
  auto succeded = false;
  auto notify = notifier(appUserModelID, &succeded);
  if (succeded) {
    auto end = buffer.end();
    for (auto it = buffer.begin(); it != end; ++it) {
      notify->Hide(it->second.Get());
    }
    buffer.clear();
  }
}

} // namespace bela::notice