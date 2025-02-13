// Copyright (c) [2013-2015] WhiteHat. All Rights Reserved. WhiteHat contributions included 
// in this file are licensed under the BSD-3-clause license (the "License") included in 
// the WhiteHat-LICENSE file included in the root directory of the distributed source code 
// archive. Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF
// ANY KIND, either express or implied. See the License for the specific language governing 
// permissions and limitations under the License. 

#include "chrome/browser/importer/importer_list.h"

#include "base/bind.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/importer/firefox_importer_utils.h"
#include "chrome/browser/championconfig/chromeimporter/chrome_importer_utils.h" //added for champion for including Google Chrome Browser
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/foundation_util.h"
#include "chrome/common/importer/safari_importer_utils.h"
#endif

using content::BrowserThread;

namespace {

#if defined(OS_WIN)
void DetectIEProfiles(std::vector<importer::SourceProfile*>* profiles) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  // IE always exists and doesn't have multiple profiles.
  importer::SourceProfile* ie = new importer::SourceProfile;
  ie->importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_IE);
  ie->importer_type = importer::TYPE_IE;
  ie->source_path.clear();
  ie->app_path.clear();
  ie->services_supported = importer::HISTORY | importer::FAVORITES |
      importer::COOKIES | importer::PASSWORDS | importer::SEARCH_ENGINES;
  profiles->push_back(ie);
}
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
void DetectSafariProfiles(std::vector<importer::SourceProfile*>* profiles) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  uint16 items = importer::NONE;
  if (!SafariImporterCanImport(base::mac::GetUserLibraryPath(), &items))
    return;

  importer::SourceProfile* safari = new importer::SourceProfile;
  safari->importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_SAFARI);
  safari->importer_type = importer::TYPE_SAFARI;
  safari->source_path.clear();
  safari->app_path.clear();
  safari->services_supported = items;
  profiles->push_back(safari);
}
#endif  // defined(OS_MACOSX)

// |locale|: The application locale used for lookups in Firefox's
// locale-specific search engines feature (see firefox_importer.cc for
// details).
void DetectFirefoxProfiles(const std::string locale,
                           std::vector<importer::SourceProfile*>* profiles) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::FilePath profile_path = GetFirefoxProfilePath();
  if (profile_path.empty())
    return;

  // Detects which version of Firefox is installed.
  importer::ImporterType firefox_type;
  base::FilePath app_path;
  int version = 0;
#if defined(OS_WIN)
  version = GetCurrentFirefoxMajorVersionFromRegistry();
#endif
  if (version < 2)
    GetFirefoxVersionAndPathFromProfile(profile_path, &version, &app_path);

  if (version >= 3) {
    firefox_type = importer::TYPE_FIREFOX;
  } else {
    // Ignores old versions of firefox.
    return;
  }

  importer::SourceProfile* firefox = new importer::SourceProfile;
  firefox->importer_name = GetFirefoxImporterName(app_path);
  firefox->importer_type = firefox_type;
  firefox->source_path = profile_path;
#if defined(OS_WIN)
  firefox->app_path = GetFirefoxInstallPathFromRegistry();
#endif
  if (firefox->app_path.empty())
    firefox->app_path = app_path;
  firefox->services_supported = importer::HISTORY | importer::FAVORITES |
      importer::PASSWORDS | importer::SEARCH_ENGINES;
  firefox->locale = locale;
  profiles->push_back(firefox);
}

/*Added for chrome for champion*/
/*finding all install_browser list for import bookmarks utpalendu */
void DetectChromeProfiles(std::vector<importer::SourceProfile*>* profiles) {
	//sNo need to check installation path for importing bookmarks.we need only preference file location
   //base::FilePath profile_path = GetChromeProfilePath();
   
  // base::FilePath profile_path =GetProfilesPreferencePath(); //champion - Dinesh
   base::FilePath profile_path;
  #if defined(OS_MACOSX) //champion - import bookmark list - Dinesh 
  // champion: Added another check to see if chrome is installed and is not just placed in device manager
  FILE *proc = popen("mdfind 'kMDItemCFBundleIdentifier == com.google.Chrome'","r");
  char buf[1024];
  if (!( !feof(proc) && fgets(buf,sizeof(buf),proc)) ) 
  {
   pclose(proc);
    return;
  }
  pclose(proc);
  profile_path = GetAppInstallPath();//GetProfilesPreferencePath();
  #endif
  #if defined(OS_WIN)
  profile_path = GetChromeInstallPathFromRegistry();
  #endif
  
  
  if (profile_path.empty())
   return;

   profile_path =GetProfilesPreferencePath();

   if (profile_path.empty())          
    return;

  // Detects which version of chrome is installed.
  importer::ImporterType chrome_type = importer:: TYPE_CHROME;
  base::FilePath app_path;
 
 //No need to find out chrome version number like firefox 
 //due to preference file format is same for the version of google chrome
	
	//No need to find importer name like firefor // GetChromeImporterName(app_path);
   importer::SourceProfile* chrome = new importer::SourceProfile;
   chrome->importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_CHROME);
   chrome->importer_type = chrome_type;
   chrome->source_path = profile_path;
  

#if defined(OS_WIN)
  chrome->app_path = GetChromeInstallPathFromRegistry();//GetProfilesPreferencePath();//GetChromeInstallPathFromRegistry();//windows function need to implement to find out chrome exe path .
#endif
#if defined(OS_MACOSX)
	 chrome->app_path=GetAppInstallPath();
 /* if (chrome->app_path.empty())
    chrome->app_path = app_path;*/
#endif	
  chrome->services_supported = importer::HISTORY | importer::FAVORITES |
      importer::PASSWORDS | importer::SEARCH_ENGINES;
  if (!chrome->app_path.empty())
    profiles->push_back(chrome);
}

/*End of champion*/

std::vector<importer::SourceProfile*> DetectSourceProfilesWorker(
    const std::string& locale,
    bool include_interactive_profiles) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  std::vector<importer::SourceProfile*> profiles;

  // The first run import will automatically take settings from the first
  // profile detected, which should be the user's current default.
#if defined(OS_WIN)
  if (ShellIntegration::IsFirefoxDefaultBrowser()) {
    DetectFirefoxProfiles(locale, &profiles);
    DetectIEProfiles(&profiles);
  } else {
    DetectIEProfiles(&profiles);
    DetectFirefoxProfiles(locale, &profiles);
  }
#elif defined(OS_MACOSX)
  if (ShellIntegration::IsFirefoxDefaultBrowser()) {
    DetectFirefoxProfiles(locale, &profiles);
    DetectSafariProfiles(&profiles);
  } else {
    DetectSafariProfiles(&profiles);
    DetectFirefoxProfiles(locale, &profiles);
  }
#else
  DetectFirefoxProfiles(locale, &profiles);
#endif
   //need to add google chrome brfore that import bookmark from html for remove javascript problem
   DetectChromeProfiles(&profiles); //added for champion for detecting chrome browser install or not .need to check for default browser
  if (include_interactive_profiles) {
    importer::SourceProfile* bookmarks_profile = new importer::SourceProfile;
    bookmarks_profile->importer_name =
        l10n_util::GetStringUTF16(IDS_IMPORT_FROM_BOOKMARKS_HTML_FILE);
    bookmarks_profile->importer_type = importer::TYPE_BOOKMARKS_FILE;
    bookmarks_profile->services_supported = importer::FAVORITES;
    profiles.push_back(bookmarks_profile);
  }

  return profiles;
}

}  // namespace

ImporterList::ImporterList()
    : weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ImporterList::~ImporterList() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ImporterList::DetectSourceProfiles(
    const std::string& locale,
    bool include_interactive_profiles,
    const base::Closure& profiles_loaded_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&DetectSourceProfilesWorker,
                 locale,
                 include_interactive_profiles),
      base::Bind(&ImporterList::SourceProfilesLoaded,
                 weak_ptr_factory_.GetWeakPtr(),
                 profiles_loaded_callback));
}

const importer::SourceProfile& ImporterList::GetSourceProfileAt(
    size_t index) const {
  DCHECK_LT(index, count());
  return *source_profiles_[index];
}

void ImporterList::SourceProfilesLoaded(
    const base::Closure& profiles_loaded_callback,
    const std::vector<importer::SourceProfile*>& profiles) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  source_profiles_.assign(profiles.begin(), profiles.end());
  profiles_loaded_callback.Run();
}
