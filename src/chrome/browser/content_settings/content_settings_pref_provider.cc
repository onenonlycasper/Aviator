// Copyright (c) [2013-2015] WhiteHat. All Rights Reserved. WhiteHat contributions included 
// in this file are licensed under the BSD-3-clause license (the "License") included in 
// the WhiteHat-LICENSE file included in the root directory of the distributed source code 
// archive. Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF
// ANY KIND, either express or implied. See the License for the specific language governing 
// permissions and limitations under the License. 

#include "chrome/browser/content_settings/content_settings_pref_provider.h"

#include <map>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/content_settings_rule.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "url/gurl.h"

#include <algorithm> // added for purge referrer champion
#include "../../../net/http/http_request_headers.h" // added for purge referrer champion
#include "../../../net/http/http_util.h" // added for purge referrer champion
//End of champion

using base::UserMetricsAction;
using content::BrowserThread;

namespace {

typedef std::pair<std::string, std::string> StringPair;
typedef std::map<std::string, std::string> StringMap;

const char kPerPluginPrefName[] = "per_plugin";
const char kAudioKey[] = "audio";
const char kVideoKey[] = "video";

ContentSetting FixObsoleteCookiePromptMode(ContentSettingsType content_type,
                                           ContentSetting setting) {
  if (content_type == CONTENT_SETTINGS_TYPE_COOKIES &&
      setting == CONTENT_SETTING_ASK) {
    return CONTENT_SETTING_BLOCK;
  }
  return setting;
}

// If the given content type supports resource identifiers in user preferences,
// returns true and sets |pref_key| to the key in the content settings
// dictionary under which per-resource content settings are stored.
// Otherwise, returns false.
bool GetResourceTypeName(ContentSettingsType content_type,
                         std::string* pref_key) {
  if (content_type == CONTENT_SETTINGS_TYPE_PLUGINS) {
    *pref_key = kPerPluginPrefName;
    return true;
  }
  return false;
}

}  // namespace

namespace content_settings {

// ////////////////////////////////////////////////////////////////////////////
// PrefProvider:
//

// static
void PrefProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kContentSettingsVersion,
      ContentSettingsPattern::kContentSettingsPatternVersion,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kContentSettingsPatternPairs,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

PrefProvider::PrefProvider(PrefService* prefs,
                           bool incognito)
  : prefs_(prefs),
    is_incognito_(incognito),
    updating_preferences_(false) {
  DCHECK(prefs_);
  // Verify preferences version.
  if (!prefs_->HasPrefPath(prefs::kContentSettingsVersion)) {
    prefs_->SetInteger(prefs::kContentSettingsVersion,
                      ContentSettingsPattern::kContentSettingsPatternVersion);
  }
  if (prefs_->GetInteger(prefs::kContentSettingsVersion) >
      ContentSettingsPattern::kContentSettingsPatternVersion) {
    return;
  }
  
  /*champion added for sentinel start*/
  /* const GURL& url1=GURL("https://sentinel.whitehatsec.com"); // blocked for new requirement on 04 sep 2013 
    const ContentSettingsPattern& primary_pattern_sentinalAvi=ContentSettingsPattern::FromURL(url1);
    const ContentSettingsPattern& secondary_pattern_sentinalAvi=ContentSettingsPattern::Wildcard();
        
    SetWebsiteSetting(primary_pattern_sentinalAvi,
                          secondary_pattern_sentinalAvi,
                          CONTENT_SETTINGS_TYPE_REFERRER,
                          "",
                          Value::CreateIntegerValue(CONTENT_SETTING_ALLOW)); */
        
    /*champion added for sentinel end*/  

  // Read content settings exceptions.
  ReadContentSettingsFromPref(false);

  if (!is_incognito_) {
    UMA_HISTOGRAM_COUNTS("ContentSettings.NumberOfExceptions",
                         value_map_.size());
  }

  // Migrate the obsolete media content setting exceptions to the new settings.
  // This needs to be done after ReadContentSettingsFromPref().
  if (!is_incognito_)
    MigrateObsoleteMediaContentSetting();

  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(
      prefs::kContentSettingsPatternPairs,
      base::Bind(&PrefProvider::OnContentSettingsPatternPairsChanged,
                 base::Unretained(this)));
}

bool PrefProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    base::Value* in_value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs_);
  // Default settings are set using a wildcard pattern for both
  // |primary_pattern| and |secondary_pattern|. Don't store default settings in
  // the |PrefProvider|. The |PrefProvider| handles settings for specific
  // sites/origins defined by the |primary_pattern| and the |secondary_pattern|.
  // Default settings are handled by the |DefaultProvider|.
  if (primary_pattern == ContentSettingsPattern::Wildcard() &&
      secondary_pattern == ContentSettingsPattern::Wildcard() &&
      resource_identifier.empty()) {
    return false;
  }

  // At this point take the ownership of the |in_value|.
  scoped_ptr<base::Value> value(in_value);
  // Update in memory value map.
  OriginIdentifierValueMap* map_to_modify = &incognito_value_map_;
  if (!is_incognito_)
    map_to_modify = &value_map_;

  {
    base::AutoLock auto_lock(lock_);
    if (value.get()) {
      map_to_modify->SetValue(
          primary_pattern,
          secondary_pattern,
          content_type,
          resource_identifier,
          value->DeepCopy());
    } else {
      map_to_modify->DeleteValue(
          primary_pattern,
          secondary_pattern,
          content_type,
          resource_identifier);
    }
  }
  // Update the content settings preference.
  if (!is_incognito_) {
    UpdatePref(primary_pattern,
               secondary_pattern,
               content_type,
               resource_identifier,
               value.get());
  }

  NotifyObservers(
      primary_pattern, secondary_pattern, content_type, resource_identifier);

  return true;
}

void PrefProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs_);

  OriginIdentifierValueMap* map_to_modify = &incognito_value_map_;
  if (!is_incognito_)
    map_to_modify = &value_map_;

  std::vector<Rule> rules_to_delete;
  {
    base::AutoLock auto_lock(lock_);
    scoped_ptr<RuleIterator> rule_iterator(
        map_to_modify->GetRuleIterator(content_type, std::string(), NULL));
    // Copy the rules; we cannot call |UpdatePref| while holding |lock_|.
    while (rule_iterator->HasNext())
      rules_to_delete.push_back(rule_iterator->Next());

    map_to_modify->DeleteValues(content_type, std::string());
  }

  for (std::vector<Rule>::const_iterator it = rules_to_delete.begin();
       it != rules_to_delete.end(); ++it) {
    UpdatePref(it->primary_pattern,
               it->secondary_pattern,
               content_type,
               std::string(),
               NULL);
  }
  NotifyObservers(ContentSettingsPattern(),
                  ContentSettingsPattern(),
                  content_type,
                  std::string());
}

PrefProvider::~PrefProvider() {
  DCHECK(!prefs_);
}

RuleIterator* PrefProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  if (incognito)
    return incognito_value_map_.GetRuleIterator(content_type,
                                                resource_identifier,
                                                &lock_);
  return value_map_.GetRuleIterator(content_type, resource_identifier, &lock_);
}

// ////////////////////////////////////////////////////////////////////////////
// Private

void PrefProvider::UpdatePref(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    const base::Value* value) {
  // Ensure that |lock_| is not held by this thread, since this function will
  // send out notifications (by |~DictionaryPrefUpdate|).
  AssertLockNotHeld();

  base::AutoReset<bool> auto_reset(&updating_preferences_, true);
  {
    DictionaryPrefUpdate update(prefs_,
                                prefs::kContentSettingsPatternPairs);
    base::DictionaryValue* pattern_pairs_settings = update.Get();

    // Get settings dictionary for the given patterns.
    std::string pattern_str(CreatePatternString(primary_pattern,
                                                secondary_pattern));
    base::DictionaryValue* settings_dictionary = NULL;
    bool found = pattern_pairs_settings->GetDictionaryWithoutPathExpansion(
        pattern_str, &settings_dictionary);

    if (!found && value) {
      settings_dictionary = new base::DictionaryValue;
      pattern_pairs_settings->SetWithoutPathExpansion(
          pattern_str, settings_dictionary);
    }

    if (settings_dictionary) {
      std::string res_dictionary_path;
      if (GetResourceTypeName(content_type, &res_dictionary_path) &&
          !resource_identifier.empty()) {
        base::DictionaryValue* resource_dictionary = NULL;
        found = settings_dictionary->GetDictionary(
            res_dictionary_path, &resource_dictionary);
        if (!found) {
          if (value == NULL)
            return;  // Nothing to remove. Exit early.
          resource_dictionary = new base::DictionaryValue;
          settings_dictionary->Set(res_dictionary_path, resource_dictionary);
        }
        // Update resource dictionary.
        if (value == NULL) {
          resource_dictionary->RemoveWithoutPathExpansion(resource_identifier,
                                                          NULL);
          if (resource_dictionary->empty()) {
            settings_dictionary->RemoveWithoutPathExpansion(
                res_dictionary_path, NULL);
          }
        } else {
          resource_dictionary->SetWithoutPathExpansion(
              resource_identifier, value->DeepCopy());
        }
      } else {
        // Update settings dictionary.
        std::string setting_path = GetTypeName(content_type);
        if (value == NULL) {
          settings_dictionary->RemoveWithoutPathExpansion(setting_path,
                                                          NULL);
        } else {
          settings_dictionary->SetWithoutPathExpansion(
              setting_path, value->DeepCopy());
        }
      }
      // Remove the settings dictionary if it is empty.
      if (settings_dictionary->empty()) {
        pattern_pairs_settings->RemoveWithoutPathExpansion(
            pattern_str, NULL);
      }
    }
  }
}


void PrefProvider::MigrateObsoleteMediaContentSetting() {
  std::vector<Rule> rules_to_delete;
  {
    scoped_ptr<RuleIterator> rule_iterator(GetRuleIterator(
        CONTENT_SETTINGS_TYPE_MEDIASTREAM, std::string(), false));
    while (rule_iterator->HasNext()) {
      // Skip default setting and rules without a value.
      const content_settings::Rule& rule = rule_iterator->Next();
      DCHECK(rule.primary_pattern != ContentSettingsPattern::Wildcard());
      if (!rule.value.get())
        continue;
      rules_to_delete.push_back(rule);
    }
  }

  for (std::vector<Rule>::const_iterator it = rules_to_delete.begin();
       it != rules_to_delete.end(); ++it) {
    const base::DictionaryValue* value_dict = NULL;
    if (!it->value->GetAsDictionary(&value_dict) || value_dict->empty())
      return;

    std::string audio_device, video_device;
    value_dict->GetString(kAudioKey, &audio_device);
    value_dict->GetString(kVideoKey, &video_device);
    // Add the exception to the new microphone content setting.
    if (!audio_device.empty()) {
      SetWebsiteSetting(it->primary_pattern,
                        it->secondary_pattern,
                        CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                        std::string(),
                        base::Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));
    }
    // Add the exception to the new camera content setting.
    if (!video_device.empty()) {
      SetWebsiteSetting(it->primary_pattern,
                        it->secondary_pattern,
                        CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                        std::string(),
                        base::Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));
    }

    // Remove the old exception in CONTENT_SETTINGS_TYPE_MEDIASTREAM.
    SetWebsiteSetting(it->primary_pattern,
                      it->secondary_pattern,
                      CONTENT_SETTINGS_TYPE_MEDIASTREAM,
                      std::string(),
                      NULL);
  }
}

// Added for purge referrer for removing dublicate element champion 
template<typename T>
void removeDuplicates(std::vector<T>* vec) {
  std::sort(vec->begin(), vec->end());
  vec->erase(std::unique(vec->begin(), vec->end()), vec->end());
}
// End

void PrefProvider::ReadContentSettingsFromPref(bool overwrite) {
  // |DictionaryPrefUpdate| sends out notifications when destructed. This
  // construction order ensures |AutoLock| gets destroyed first and |lock_| is
  // not held when the notifications are sent. Also, |auto_reset| must be still
  // valid when the notifications are sent, so that |Observe| skips the
  // notification.
  
  // Added for purge referrer champion  for reading existing settings from preference file utpal
  std :: string key,valueforuser;
  int setting = CONTENT_SETTING_DEFAULT;
  std :: string host_val;	
  net::HttpUtil::AllowVector->clear();  // champion_clear the Allow vector
  net ::HttpUtil::BlockVector->clear(); // champion_clear the Block vector
  // End 	 
	 
  base::AutoReset<bool> auto_reset(&updating_preferences_, true);
  DictionaryPrefUpdate update(prefs_, prefs::kContentSettingsPatternPairs);
  base::AutoLock auto_lock(lock_);

  const base::DictionaryValue* all_settings_dictionary =
      prefs_->GetDictionary(prefs::kContentSettingsPatternPairs);

  if (overwrite)
    value_map_.clear();

  // Careful: The returned value could be NULL if the pref has never been set.
  if (!all_settings_dictionary)
    return;

  base::DictionaryValue* mutable_settings;
  scoped_ptr<base::DictionaryValue> mutable_settings_scope;

  if (!is_incognito_) {
    mutable_settings = update.Get();
  } else {
    // Create copy as we do not want to persist anything in OTR prefs.
    mutable_settings = all_settings_dictionary->DeepCopy();
    mutable_settings_scope.reset(mutable_settings);
  }
  // Convert all Unicode patterns into punycode form, then read.
  CanonicalizeContentSettingsExceptions(mutable_settings);

  size_t cookies_block_exception_count = 0;
  size_t cookies_allow_exception_count = 0;
  size_t cookies_session_only_exception_count = 0;
  for (base::DictionaryValue::Iterator i(*mutable_settings); !i.IsAtEnd();
       i.Advance()) {
    const std::string& pattern_str(i.key());
    std::pair<ContentSettingsPattern, ContentSettingsPattern> pattern_pair =
        ParsePatternString(pattern_str);
    if (!pattern_pair.first.IsValid() ||
        !pattern_pair.second.IsValid()) {
      // TODO: Change this to DFATAL when crbug.com/132659 is fixed.
      LOG(ERROR) << "Invalid pattern strings: " << pattern_str;
      continue;
    }

    // Get settings dictionary for the current pattern string, and read
    // settings from the dictionary.
    const base::DictionaryValue* settings_dictionary = NULL;
    bool is_dictionary = i.value().GetAsDictionary(&settings_dictionary);
    DCHECK(is_dictionary);
	// champion : add referrer (balaji)
    key=pattern_str;
    const base::DictionaryValue* referrer_value_settings_dictionary = NULL;
    bool is_make_dictionary;
    for (base::DictionaryValue::Iterator j(* mutable_settings);!j.IsAtEnd();j.Advance()) {
      valueforuser=j.key();
      is_make_dictionary = j.value().GetAsDictionary(&referrer_value_settings_dictionary);
      DCHECK(is_make_dictionary);		
    }
    for (base::DictionaryValue::Iterator k(* referrer_value_settings_dictionary);!k.IsAtEnd();k.Advance()) {
      valueforuser=k.key();
    }
    // champion : end
    for (size_t i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
      ContentSettingsType content_type = static_cast<ContentSettingsType>(i);

      std::string res_dictionary_path;
      if (GetResourceTypeName(content_type, &res_dictionary_path)) {
        const base::DictionaryValue* resource_dictionary = NULL;
        if (settings_dictionary->GetDictionary(
                res_dictionary_path, &resource_dictionary)) {
          for (base::DictionaryValue::Iterator j(*resource_dictionary);
               !j.IsAtEnd();
               j.Advance()) {
            const std::string& resource_identifier(j.key());
            int setting = CONTENT_SETTING_DEFAULT;
            bool is_integer = j.value().GetAsInteger(&setting);
            DCHECK(is_integer);
            DCHECK_NE(CONTENT_SETTING_DEFAULT, setting);
            value_map_.SetValue(pattern_pair.first,
                                pattern_pair.second,
                                content_type,
                                resource_identifier,
                                base::Value::CreateIntegerValue(setting));
          }
        }
      }
      base::Value* value = NULL;
      if (HostContentSettingsMap::ContentTypeHasCompoundValue(content_type)) {
        const base::DictionaryValue* setting = NULL;
        // TODO(xians): Handle the non-dictionary types.
        if (settings_dictionary->GetDictionaryWithoutPathExpansion(
            GetTypeName(ContentSettingsType(i)), &setting)) {
          DCHECK(!setting->empty());
          value = setting->DeepCopy();
        }
      } else {
        int setting = CONTENT_SETTING_DEFAULT;
        if (settings_dictionary->GetIntegerWithoutPathExpansion(
                GetTypeName(ContentSettingsType(i)), &setting)) {
          DCHECK_NE(CONTENT_SETTING_DEFAULT, setting);
          setting = FixObsoleteCookiePromptMode(content_type,
                                                ContentSetting(setting));
          value = base::Value::CreateIntegerValue(setting);
        }
        // Added for purge referrer champion
        host_val=pattern_pair.first.parts_.host;	
        // end
        // Added for purge referrer champion
        if( content_type == CONTENT_SETTINGS_TYPE_REFERRER && valueforuser =="referrer") {
          net::HttpRequestHeaders::Insert(setting, host_val);				
        } 
        //End
      }

      // |value_map_| will take the ownership of |value|.
      if (value != NULL) {
        value_map_.SetValue(pattern_pair.first,
                            pattern_pair.second,
                            content_type,
                            ResourceIdentifier(),
                            value);
        if (content_type == CONTENT_SETTINGS_TYPE_COOKIES) {
          ContentSetting s = ValueToContentSetting(value);
          switch (s) {
            case CONTENT_SETTING_ALLOW :
              ++cookies_allow_exception_count;
              break;
            case CONTENT_SETTING_BLOCK :
              ++cookies_block_exception_count;
              break;
            case CONTENT_SETTING_SESSION_ONLY :
              ++cookies_session_only_exception_count;
              break;
            default:
              NOTREACHED();
              break;
          }
        }
		
		// Added for purge referrer champion  champion_from_23
        if( content_type == CONTENT_SETTINGS_TYPE_REFERRER) {
          if(valueforuser =="referrer") {
            if(setting == 1) {
              net::HttpUtil::AllowVector->push_back(host_val);  
              removeDuplicates(net::HttpUtil::AllowVector);
            }
            if(setting == 2) {
              net::HttpUtil::BlockVector->push_back(host_val);
              removeDuplicates(net::HttpUtil::BlockVector);
            }
          }
        } 
		//End
	  }
    }
  }
  UMA_HISTOGRAM_COUNTS("ContentSettings.NumberOfBlockCookiesExceptions",
                       cookies_block_exception_count);
  UMA_HISTOGRAM_COUNTS("ContentSettings.NumberOfAllowCookiesExceptions",
                       cookies_allow_exception_count);
  UMA_HISTOGRAM_COUNTS("ContentSettings.NumberOfSessionOnlyCookiesExceptions",
                       cookies_session_only_exception_count);
}

void PrefProvider::OnContentSettingsPatternPairsChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (updating_preferences_)
    return;

  ReadContentSettingsFromPref(true);

  // champion : start
  bool enabled = prefs_->GetBoolean(prefs::kAllwaysOrNeverReferrersHeader);
 
  scoped_ptr<base::Value> value(base::Value::CreateBooleanValue(!enabled));
  prefs_->Set(prefs::kAllwaysOrNeverReferrersHeader, *value);
  
  enabled = prefs_->GetBoolean(prefs::kAllwaysOrNeverReferrersHeader);
 
  scoped_ptr<base::Value> value1(base::Value::CreateBooleanValue(!enabled));
  prefs_->Set(prefs::kAllwaysOrNeverReferrersHeader, *value1);
  // champion : end
  NotifyObservers(ContentSettingsPattern(),
                  ContentSettingsPattern(),
                  CONTENT_SETTINGS_TYPE_DEFAULT,
                  std::string());
}

// static
void PrefProvider::CanonicalizeContentSettingsExceptions(
    base::DictionaryValue* all_settings_dictionary) {
  DCHECK(all_settings_dictionary);

  std::vector<std::string> remove_items;
  std::vector<std::pair<std::string, std::string> > move_items;
  for (base::DictionaryValue::Iterator i(*all_settings_dictionary);
       !i.IsAtEnd();
       i.Advance()) {
    const std::string& pattern_str(i.key());
    std::pair<ContentSettingsPattern, ContentSettingsPattern> pattern_pair =
         ParsePatternString(pattern_str);
    if (!pattern_pair.first.IsValid() ||
        !pattern_pair.second.IsValid()) {
      LOG(ERROR) << "Invalid pattern strings: " << pattern_str;
      continue;
    }

    const std::string canonicalized_pattern_str = CreatePatternString(
        pattern_pair.first, pattern_pair.second);

    if (canonicalized_pattern_str.empty() ||
        canonicalized_pattern_str == pattern_str) {
      continue;
    }

    // Clear old pattern if prefs already have canonicalized pattern.
    const base::DictionaryValue* new_pattern_settings_dictionary = NULL;
    if (all_settings_dictionary->GetDictionaryWithoutPathExpansion(
            canonicalized_pattern_str, &new_pattern_settings_dictionary)) {
      remove_items.push_back(pattern_str);
      continue;
    }

    // Move old pattern to canonicalized pattern.
    const base::DictionaryValue* old_pattern_settings_dictionary = NULL;
    if (i.value().GetAsDictionary(&old_pattern_settings_dictionary)) {
      move_items.push_back(
          std::make_pair(pattern_str, canonicalized_pattern_str));
    }
  }

  for (size_t i = 0; i < remove_items.size(); ++i) {
    all_settings_dictionary->RemoveWithoutPathExpansion(remove_items[i], NULL);
  }

  for (size_t i = 0; i < move_items.size(); ++i) {
    scoped_ptr<base::Value> pattern_settings_dictionary;
    all_settings_dictionary->RemoveWithoutPathExpansion(
        move_items[i].first, &pattern_settings_dictionary);
    all_settings_dictionary->SetWithoutPathExpansion(
        move_items[i].second, pattern_settings_dictionary.release());
  }
}

void PrefProvider::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs_);
  RemoveAllObservers();
  pref_change_registrar_.RemoveAll();
  prefs_ = NULL;
}

void PrefProvider::AssertLockNotHeld() const {
#if !defined(NDEBUG)
  // |Lock::Acquire()| will assert if the lock is held by this thread.
  lock_.Acquire();
  lock_.Release();
#endif
}

}  // namespace content_settings
