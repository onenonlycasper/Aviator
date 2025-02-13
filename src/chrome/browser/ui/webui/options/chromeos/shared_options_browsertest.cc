// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

namespace {

const char* kTestOwner = "test-owner@example.com";
const char* kTestNonOwner = "test-user1@example.com";

const char* kKnownSettings[] = {
  kDeviceOwner,
  kAccountsPrefAllowGuest,
  kAccountsPrefAllowNewUser,
  kAccountsPrefDeviceLocalAccounts,
  kAccountsPrefShowUserNamesOnSignIn,
  kAccountsPrefSupervisedUsersEnabled,
};

// Stub settings provider that only handles the settings we need to control.
// StubCrosSettingsProvider handles more settings but leaves many of them unset
// which the Settings page doesn't expect.
class StubAccountSettingsProvider : public StubCrosSettingsProvider {
 public:
  StubAccountSettingsProvider() {
  }

  virtual ~StubAccountSettingsProvider() {
  }

  // StubCrosSettingsProvider implementation.
  virtual bool HandlesSetting(const std::string& path) const OVERRIDE {
    const char** end = kKnownSettings + arraysize(kKnownSettings);
    return std::find(kKnownSettings, end, path) != end;
  }
};

struct PrefTest {
  const char* pref_name;
  bool owner_only;
  bool indicator;
};

const PrefTest kPrefTests[] = {
  { kSystemTimezone, false, false },
  { prefs::kUse24HourClock, false, false },
  { kAttestationForContentProtectionEnabled, true, true },
  { kAccountsPrefAllowGuest, true, false },
  { kAccountsPrefAllowNewUser, true, false },
  { kAccountsPrefShowUserNamesOnSignIn, true, false },
  { kAccountsPrefSupervisedUsersEnabled, true, false },
#if defined(GOOGLE_CHROME_BUILD)
  { kStatsReportingPref, true, true },
  { prefs::kSpellCheckUseSpellingService, false, false },
#endif
};

}  // namespace

class SharedOptionsTest : public LoginManagerTest {
 public:
  SharedOptionsTest()
    : LoginManagerTest(false),
      device_settings_provider_(NULL) {
    stub_settings_provider_.Set(kDeviceOwner, base::StringValue(kTestOwner));
  }

  virtual ~SharedOptionsTest() {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    LoginManagerTest::SetUpOnMainThread();

    CrosSettings* settings = CrosSettings::Get();

    // Add the stub settings provider, moving the device settings provider
    // behind it so our stub takes precedence.
    device_settings_provider_ = settings->GetProvider(kDeviceOwner);
    settings->RemoveSettingsProvider(device_settings_provider_);
    settings->AddSettingsProvider(&stub_settings_provider_);
    settings->AddSettingsProvider(device_settings_provider_);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    CrosSettings* settings = CrosSettings::Get();
    settings->RemoveSettingsProvider(&stub_settings_provider_);
    LoginManagerTest::CleanUpOnMainThread();
  }

 protected:
  void CheckOptionsUI(const User* user, bool is_owner, bool is_primary) {
    Browser* browser = CreateBrowserForUser(user);
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();

    for (size_t i = 0; i < sizeof(kPrefTests) / sizeof(kPrefTests[0]); i++) {
      CheckPreference(contents,
                      kPrefTests[i].pref_name,
                      !is_owner && kPrefTests[i].owner_only,
                      !is_owner && kPrefTests[i].indicator ? "owner" :
                                                             std::string());
    }
    CheckBanner(contents, is_primary);
    CheckSharedSections(contents, is_primary);
    CheckAccountsOverlay(contents, is_owner);
  }

  // Creates a browser and navigates to the Settings page.
  Browser* CreateBrowserForUser(const User* user) {
    Profile* profile = UserManager::Get()->GetProfileByUser(user);
    profile->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                   user->email());

    ui_test_utils::BrowserAddedObserver observer;
    Browser* browser = CreateBrowser(profile);
    observer.WaitForSingleNewBrowser();

    ui_test_utils::NavigateToURL(browser,
                                 GURL("aviator://settings-frame"));
    return browser;
  }

  // Verifies a preference's disabled state and controlled-by indicator.
  void CheckPreference(content::WebContents* contents,
                       std::string pref_name,
                       bool disabled,
                       std::string controlled_by) {
    bool success;
    std::string js_expression = base::StringPrintf(
        "var prefSelector = '[pref=\"%s\"]';"
        "var controlledBy = '%s';"
        "var input = document.querySelector("
        "    'input' + prefSelector + ', select' + prefSelector);"
        "var success = false;"
        "if (input) {"
        "  success = input.disabled == %d;"
        "  var indicator = document.querySelector(input.tagName +"
        "      prefSelector + ' + span span.controlled-setting-indicator');"
        "  if (controlledBy) {"
        "    success = success && indicator &&"
        "              indicator.getAttribute('controlled-by') == controlledBy;"
        "  } else {"
        "    success = success && (!indicator ||"
        "              !indicator.hasAttribute('controlled-by') ||"
        "              indicator.getAttribute('controlled-by') == '')"
        "  }"
        "}"
        "window.domAutomationController.send(!!success);",
        pref_name.c_str(), controlled_by.c_str(), disabled);
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents, js_expression, &success));
    EXPECT_TRUE(success);
  }

  // Verifies a checkbox's disabled state, controlled-by indicator and value.
  void CheckBooleanPreference(content::WebContents* contents,
                              std::string pref_name,
                              bool disabled,
                              std::string controlled_by,
                              bool expected_value) {
    CheckPreference(contents, pref_name, disabled, controlled_by);
    bool actual_value;
    std::string js_expression = base::StringPrintf(
        "window.domAutomationController.send(document.querySelector('"
        "    input[type=\"checkbox\"][pref=\"%s\"]').checked);",
        pref_name.c_str());
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents, js_expression, &actual_value));
    EXPECT_EQ(expected_value, actual_value);
  }

  // Verifies that the shared settings banner is visible only for
  // secondary users.
  void CheckBanner(content::WebContents* contents,
                   bool is_primary) {
    bool banner_visible;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents,
        "var e = $('secondary-user-banner');"
        "window.domAutomationController.send(e && !e.hidden);",
        &banner_visible));
    EXPECT_EQ(!is_primary, banner_visible);
  }

  // Verifies that sections of shared settings have the appropriate indicator.
  void CheckSharedSections(content::WebContents* contents,
                           bool is_primary) {
    // This only applies to the Internet options section.
    std::string controlled_by;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        contents,
        "var e = document.querySelector("
        "    '#network-section-header span.controlled-setting-indicator');"
        "if (!e || !e.getAttribute('controlled-by')) {"
        "  window.domAutomationController.send('');"
        "} else {"
        "  window.domAutomationController.send("
        "      e.getAttribute('controlled-by'));"
        "}",
        &controlled_by));
    EXPECT_EQ(!is_primary ? "shared" : std::string(), controlled_by);
  }

  // Checks the Accounts header and non-checkbox inputs.
  void CheckAccountsOverlay(content::WebContents* contents, bool is_owner) {
    // Set cros.accounts.allowGuest to false so we can test the accounts list.
    // This has to be done after the PRE_* test or we can't add the owner.
    stub_settings_provider_.Set(
        kAccountsPrefAllowNewUser, base::FundamentalValue(false));

    bool success;
    std::string js_expression = base::StringPrintf(
        "var controlled = %d;"
        "var warning = $('ownerOnlyWarning');"
        "var userList = $('userList');"
        "var input = $('userNameEdit');"
        "var success;"
        "if (controlled)"
        "  success = warning && !warning.hidden && userList.disabled &&"
        "            input.disabled;"
        "else"
        "  success = (!warning || warning.hidden) && !userList.disabled &&"
        "            !input.disabled;"
        "window.domAutomationController.send(!!success);",
        !is_owner);
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents, js_expression, &success));
    EXPECT_TRUE(success) << "Accounts overlay incorrect for " <<
        (is_owner ? "owner." : "non-owner.");
  }

  StubAccountSettingsProvider stub_settings_provider_;
  CrosSettingsProvider* device_settings_provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedOptionsTest);
};

IN_PROC_BROWSER_TEST_F(SharedOptionsTest, PRE_SharedOptions) {
  RegisterUser(kTestOwner);
  RegisterUser(kTestNonOwner);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(SharedOptionsTest, SharedOptions) {
  // Log in the owner first, then add a secondary user.
  LoginUser(kTestOwner);
  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  AddUser(kTestNonOwner);

  UserManager* manager = UserManager::Get();
  ASSERT_EQ(2u, manager->GetLoggedInUsers().size());
  {
    SCOPED_TRACE("Checking settings for owner, primary user.");
    CheckOptionsUI(manager->FindUser(manager->GetOwnerEmail()), true, true);
  }
  {
    SCOPED_TRACE("Checking settings for non-owner, secondary user.");
    CheckOptionsUI(manager->FindUser(kTestNonOwner), false, false);
  }
  // TODO(michaelpg): Add tests for non-primary owner and primary non-owner
  // when the owner-only multiprofile restriction is removed, probably M38.
}

IN_PROC_BROWSER_TEST_F(SharedOptionsTest, PRE_ScreenLockPreference) {
  RegisterUser(kTestOwner);
  RegisterUser(kTestNonOwner);
  StartupUtils::MarkOobeCompleted();
}

// Tests that a shared setting indicator appears for the auto-lock setting
// when the user has the checkbox unselected but another user has enabled
// auto-lock. (The checkbox is unset if the user's preference is false,
// but if any other signed-in user has enabled this preference, the shared
// setting indicator explains this.)
IN_PROC_BROWSER_TEST_F(SharedOptionsTest, ScreenLockPreference) {
  LoginUser(kTestOwner);
  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  AddUser(kTestNonOwner);

  UserManager* manager = UserManager::Get();
  const User* user1 = manager->FindUser(kTestOwner);
  const User* user2 = manager->FindUser(kTestNonOwner);

  PrefService* prefs1 = manager->GetProfileByUser(user1)->GetPrefs();
  PrefService* prefs2 = manager->GetProfileByUser(user2)->GetPrefs();
  prefs1->SetBoolean(prefs::kEnableAutoScreenLock, false);
  prefs2->SetBoolean(prefs::kEnableAutoScreenLock, false);

  Browser* browser1 = CreateBrowserForUser(user1);
  Browser* browser2 = CreateBrowserForUser(user2);
  content::WebContents* contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();
  content::WebContents* contents2 =
      browser2->tab_strip_model()->GetActiveWebContents();

  bool disabled = false;
  bool expected_value;
  std::string empty_controlled;
  std::string shared_controlled("shared");

  // First test case: secondary user dependent on primary user.
  {
    SCOPED_TRACE("Screen lock false for both users");
    expected_value = false;
    CheckBooleanPreference(contents1, prefs::kEnableAutoScreenLock, disabled,
                           empty_controlled, expected_value);
    CheckBooleanPreference(contents2, prefs::kEnableAutoScreenLock, disabled,
                           empty_controlled, expected_value);
  }
  // Set the preference to true for the primary user and check that the value
  // changes appropriately.
  prefs1->SetBoolean(prefs::kEnableAutoScreenLock, true);
  {
    SCOPED_TRACE("Screen lock true for primary user");
    expected_value = true;
    CheckBooleanPreference(contents1, prefs::kEnableAutoScreenLock, disabled,
                           empty_controlled, expected_value);
  }
  // Reload the secondary user's browser to see the updated controlled-by
  // indicator. Also reload the primary user's to make sure the setting still
  // starts out correctly.
  chrome::Reload(browser1, CURRENT_TAB);
  chrome::Reload(browser2, CURRENT_TAB);
  content::WaitForLoadStop(contents1);
  content::WaitForLoadStop(contents2);
  {
    SCOPED_TRACE("Screen lock true for primary user, false for secondary user");
    expected_value = true;
    CheckBooleanPreference(contents1, prefs::kEnableAutoScreenLock, disabled,
                           empty_controlled, expected_value);
    expected_value = false;
    CheckBooleanPreference(contents2, prefs::kEnableAutoScreenLock, disabled,
                           shared_controlled, expected_value);
  }
  // Set the preference to true for the secondary user and check that the
  // indicator disappears.
  prefs2->SetBoolean(prefs::kEnableAutoScreenLock, true);
  {
    SCOPED_TRACE("Screen lock true for secondary user");
    expected_value = true;
    CheckBooleanPreference(contents2, prefs::kEnableAutoScreenLock, disabled,
                           empty_controlled, expected_value);
  }

  // Second test case: primary user dependent on secondary user.
  chrome::Reload(browser1, CURRENT_TAB);
  chrome::Reload(browser2, CURRENT_TAB);
  content::WaitForLoadStop(contents1);
  content::WaitForLoadStop(contents2);
  {
    SCOPED_TRACE("Screen lock true for both users");
    expected_value = true;
    CheckBooleanPreference(contents1, prefs::kEnableAutoScreenLock, disabled,
                           empty_controlled, expected_value);
    CheckBooleanPreference(contents2, prefs::kEnableAutoScreenLock, disabled,
                           empty_controlled, expected_value);
  }
  // Set the preference to false for the primary user and check that the
  // value changes correctly.
  prefs1->SetBoolean(prefs::kEnableAutoScreenLock, false);
  {
    SCOPED_TRACE("Screen lock false for primary user");
    expected_value = false;
    CheckBooleanPreference(contents1, prefs::kEnableAutoScreenLock, disabled,
                           shared_controlled, expected_value);
  }
  // The primary user should now see a shared setting indicator.
  chrome::Reload(browser1, CURRENT_TAB);
  chrome::Reload(browser2, CURRENT_TAB);
  content::WaitForLoadStop(contents1);
  content::WaitForLoadStop(contents2);
  {
    SCOPED_TRACE("Screen lock false for primary user, true for secondary user");
    expected_value = false;
    CheckBooleanPreference(contents1, prefs::kEnableAutoScreenLock, disabled,
                           shared_controlled, expected_value);
    expected_value = true;
    CheckBooleanPreference(contents2, prefs::kEnableAutoScreenLock, disabled,
                           empty_controlled, expected_value);
  }
}

}  // namespace chromeos
