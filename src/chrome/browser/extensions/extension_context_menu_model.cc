// Copyright (c) [2013-2015] WhiteHat. All Rights Reserved. WhiteHat contributions included 
// in this file are licensed under the BSD-3-clause license (the "License") included in 
// the WhiteHat-LICENSE file included in the root directory of the distributed source code 
// archive. Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF
// ANY KIND, either express or implied. See the License for the specific language governing 
// permissions and limitations under the License. 

#include "chrome/browser/extensions/extension_context_menu_model.h"

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "chrome/browser/ui/browser_window.h" // champion : for nativewindow call (amresh)
#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"           // champion : for native windows to hwnd conversion (amresh)
#endif
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using extensions::Extension;

ExtensionContextMenuModel::ExtensionContextMenuModel(const Extension* extension,
                                                     Browser* browser,
                                                     PopupDelegate* delegate)
    : SimpleMenuModel(this),
      extension_id_(extension->id()),
      browser_(browser),
      profile_(browser->profile()),
      delegate_(delegate) {
  InitMenu(extension);

  if (profile_->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode) &&
      delegate_) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithStringId(INSPECT_POPUP, IDS_EXTENSION_ACTION_INSPECT_POPUP);
  }
}

ExtensionContextMenuModel::ExtensionContextMenuModel(const Extension* extension,
                                                     Browser* browser)
    : SimpleMenuModel(this),
      extension_id_(extension->id()),
      browser_(browser),
      profile_(browser->profile()),
      delegate_(NULL) {
  InitMenu(extension);
}

bool ExtensionContextMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ExtensionContextMenuModel::IsCommandIdEnabled(int command_id) const {
  const Extension* extension = this->GetExtension();
  if (!extension)
    return false;

  if (command_id == CONFIGURE) {
    return
        extensions::ManifestURL::GetOptionsPage(extension).spec().length() > 0;
  } else if (command_id == NAME) {
    // The NAME links to the Homepage URL. If the extension doesn't have a
    // homepage, we just disable this menu item.
    return extensions::ManifestURL::GetHomepageURL(extension).is_valid();
  } else if (command_id == INSPECT_POPUP) {
    WebContents* web_contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    if (!web_contents)
      return false;

    return extension_action_ &&
        extension_action_->HasPopup(SessionID::IdForTab(web_contents));
  } else if (command_id == UNINSTALL) {
    // Some extension types can not be uninstalled.
    return extensions::ExtensionSystem::Get(
        profile_)->management_policy()->UserMayModifySettings(extension, NULL);
  }
  return true;
}

bool ExtensionContextMenuModel::GetAcceleratorForCommandId(
    int command_id, ui::Accelerator* accelerator) {
  return false;
}

void ExtensionContextMenuModel::ExecuteCommand(int command_id,
                                               int event_flags) {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  // champion : lets get handle for our message (amresh)
  #if defined(OS_WIN)
  gfx::NativeWindow ptrNativeWnd = browser_->window()->GetNativeWindow();
  HWND h4Msg = views::HWNDForNativeWindow(ptrNativeWnd);
  #endif  
  switch (command_id) {
    case NAME: {
      OpenURLParams params(extensions::ManifestURL::GetHomepageURL(extension),
                           Referrer(), NEW_FOREGROUND_TAB,
                           content::PAGE_TRANSITION_LINK, false);
      browser_->OpenURL(params);
      break;
    }
    case CONFIGURE:
      DCHECK(!extensions::ManifestURL::GetOptionsPage(extension).is_empty());
      extensions::ExtensionTabUtil::OpenOptionsPage(extension, browser_);
      break;
    case HIDE: {
      // champion : Disconnect & HTTPS Everywhere(amresh)
      if (extension->name() == "Disconnect") {
         #if defined(OS_WIN)
        ::MessageBox (h4Msg, L"Oops! Disconnect is mandatory security plugin, can't hide.", L"Hide failed", MB_ICONERROR);
         #endif
        break;
      } /*else if (extension->name() == "HTTPS Everywhere"){
        ::MessageBox (h4Msg, L"Oops! HTTPS Everywhere is mandatory security plugin, can't hide.", L"Hide failed", MB_ICONERROR);
        break;
      }*/
      extensions::ExtensionActionAPI::SetBrowserActionVisibility(
          extensions::ExtensionPrefs::Get(profile_), extension->id(), false);
      break;
    }
    case UNINSTALL: {
      // champion : Disconnect, PDF Viewer & HTTPS Everywhere (amresh)
      if (extension->name() == "Disconnect") {
         #if defined(OS_WIN)
        ::MessageBox (h4Msg, L"Oops! Disconnect is mandatory security plugin, can't uninstall.", L"Uninstall failed", MB_ICONERROR);
         #endif
        break;
      } /*else if (extension->name() == "PDF Viewer") {
        ::MessageBox (h4Msg, L"PDF Viewer is mandatory bundled plugin to view embedded\nPDF files into browser, can't uninstall.", L"Uninstall failed", MB_ICONERROR);
        break;
      } else if (extension->name() == "HTTPS Everywhere") {
        ::MessageBox (h4Msg, L"Oops! HTTPS Everywhere is mandatory security plugin, can't uninstall.", L"Uninstall failed", MB_ICONERROR);
        break;
      }*/
      AddRef();  // Balanced in Accepted() and Canceled()
      extension_uninstall_dialog_.reset(
          extensions::ExtensionUninstallDialog::Create(
              profile_, browser_, this));
      extension_uninstall_dialog_->ConfirmUninstall(extension);
      break;
    }
    case MANAGE: {
      chrome::ShowExtensions(browser_, extension->id());
      break;
    }
    case INSPECT_POPUP: {
      delegate_->InspectPopup(extension_action_);
      break;
    }
    default:
     NOTREACHED() << "Unknown option";
     break;
  }
}

void ExtensionContextMenuModel::ExtensionUninstallAccepted() {
  if (GetExtension()) {
    extensions::ExtensionSystem::Get(profile_)->extension_service()->
        UninstallExtension(extension_id_, false, NULL);
  }
  Release();
}

void ExtensionContextMenuModel::ExtensionUninstallCanceled() {
  Release();
}

ExtensionContextMenuModel::~ExtensionContextMenuModel() {}

void ExtensionContextMenuModel::InitMenu(const Extension* extension) {
  DCHECK(extension);

  extensions::ExtensionActionManager* extension_action_manager =
      extensions::ExtensionActionManager::Get(profile_);
  extension_action_ = extension_action_manager->GetBrowserAction(*extension);
  if (!extension_action_)
    extension_action_ = extension_action_manager->GetPageAction(*extension);

  std::string extension_name = extension->name();
  // Ampersands need to be escaped to avoid being treated like
  // mnemonics in the menu.
  base::ReplaceChars(extension_name, "&", "&&", &extension_name);
  AddItem(NAME, base::UTF8ToUTF16(extension_name));
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(CONFIGURE, IDS_EXTENSIONS_OPTIONS_MENU_ITEM);
  AddItem(UNINSTALL, l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNINSTALL));
  if (extension_action_manager->GetBrowserAction(*extension))
    AddItemWithStringId(HIDE, IDS_EXTENSIONS_HIDE_BUTTON);
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(MANAGE, IDS_MANAGE_EXTENSION);
}

const Extension* ExtensionContextMenuModel::GetExtension() const {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  return extension_service->GetExtensionById(extension_id_, false);
}
