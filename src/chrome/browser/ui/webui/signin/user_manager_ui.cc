// Copyright (c) [2013-2015] WhiteHat. All Rights Reserved. WhiteHat contributions included 
// in this file are licensed under the BSD-3-clause license (the "License") included in 
// the WhiteHat-LICENSE file included in the root directory of the distributed source code 
// archive. Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF
// ANY KIND, either express or implied. See the License for the specific language governing 
// permissions and limitations under the License. 

#include "chrome/browser/ui/webui/signin/user_manager_ui.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/user_manager_screen_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

// JS file names.
const char kStringsJSPath[] = "strings.js";
const char kUserManagerJSPath[] = "user_manager.js";

UserManagerUI::UserManagerUI(content::WebUI* web_ui)
  : WebUIController(web_ui) {
  // The web_ui object takes ownership of the handler, and will
  // destroy it when it (the WebUI) is destroyed.
  user_manager_screen_handler_ = new UserManagerScreenHandler();
  web_ui->AddMessageHandler(user_manager_screen_handler_);

  base::DictionaryValue localized_strings;
  GetLocalizedStrings(&localized_strings);

  Profile* profile = Profile::FromWebUI(web_ui);
  // Set up the aviator://user-chooser/ source.
  content::WebUIDataSource::Add(profile, CreateUIDataSource(localized_strings));

#if defined(ENABLE_THEMES)
  // Set up the aviator://theme/ source
  ThemeSource* theme = new ThemeSource(profile);
  content::URLDataSource::Add(profile, theme);
#endif
}

UserManagerUI::~UserManagerUI() {
}

content::WebUIDataSource* UserManagerUI::CreateUIDataSource(
    const base::DictionaryValue& localized_strings) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIUserManagerHost);
  source->SetUseJsonJSFormatV2();
  source->AddLocalizedStrings(localized_strings);
  source->SetJsonPath(kStringsJSPath);

  source->SetDefaultResource(IDR_USER_MANAGER_HTML);
  source->AddResourcePath(kUserManagerJSPath, IDR_USER_MANAGER_JS);

  return source;
}

void UserManagerUI::GetLocalizedStrings(
    base::DictionaryValue* localized_strings) {
  user_manager_screen_handler_->GetLocalizedValues(localized_strings);
  webui::SetFontAndTextDirection(localized_strings);

#if defined(GOOGLE_CHROME_BUILD)
  localized_strings->SetString("buildType", "Aviator");
#else
  localized_strings->SetString("buildType", "Aviator"); // champion : branding (amresh)
#endif
}

