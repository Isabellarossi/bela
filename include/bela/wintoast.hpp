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
// #pragma comment(lib,"shlwapi")
// #pragma comment(lib,"user32")

namespace bela::wintoast {
using namespace Microsoft::WRL;
using namespace ABI::Windows::Data::Xml::Dom;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::UI::Notifications;
using namespace Windows::Foundation;

} // namespace bela::wintoast

#endif