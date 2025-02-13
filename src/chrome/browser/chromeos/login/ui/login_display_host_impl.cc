// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"

#include <vector>

#include "ash/audio/sounds.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/charger_replace/charger_replacement_dialog.h"
#include "chrome/browser/chromeos/first_run/drive_first_run_controller.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_actor.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/input_events_blocker.h"
#include "chrome/browser/chromeos/login/ui/keyboard_driven_oobe_key_handler.h"
#include "chrome/browser/chromeos/login/ui/oobe_display.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/chromeos/policy/auto_enrollment_client.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/ui/focus_ring_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "chromeos/chromeos_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/login/login_state.h"
#include "chromeos/settings/timezone_settings.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "media/audio/sounds/sounds_manager.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/gfx/transform.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_animations.h"
#include "url/gurl.h"

namespace {

// Maximum delay for startup sound after 'loginPromptVisible' signal.
const int kStartupSoundMaxDelayMs = 2000;

// URL which corresponds to the login WebUI.
const char kLoginURL[] = "aviator://oobe/login";

// URL which corresponds to the OOBE WebUI.
const char kOobeURL[] = "aviator://oobe/oobe";

// URL which corresponds to the user adding WebUI.
const char kUserAddingURL[] = "aviator://oobe/user-adding";

// URL which corresponds to the app launch splash WebUI.
const char kAppLaunchSplashURL[] = "aviator://oobe/app-launch-splash";

// Duration of sign-in transition animation.
const int kLoginFadeoutTransitionDurationMs = 700;

// Number of times we try to reload OOBE/login WebUI if it crashes.
const int kCrashCountLimit = 5;

// Whether to enable tnitializing WebUI in hidden state (see
// |initialize_webui_hidden_|) by default.
const bool kHiddenWebUIInitializationDefault = true;

// Switch values that might be used to override WebUI init type.
const char kWebUIInitParallel[] = "parallel";
const char kWebUIInitPostpone[] = "postpone";

// The delay of triggering initialization of the device policy subsystem
// after the login screen is initialized. This makes sure that device policy
// network requests are made while the system is idle waiting for user input.
const int64 kPolicyServiceInitializationDelayMilliseconds = 100;

// A class to observe an implicit animation and invokes the callback after the
// animation is completed.
class AnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit AnimationObserver(const base::Closure& callback)
      : callback_(callback) {}
  virtual ~AnimationObserver() {}

 private:
  // ui::ImplicitAnimationObserver implementation:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    callback_.Run();
    delete this;
  }

  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(AnimationObserver);
};

// ShowLoginWizard is split into two parts. This function is sometimes called
// from ShowLoginWizard(), and sometimes from OnLanguageSwitchedCallback()
// (if locale was updated).
void ShowLoginWizardFinish(
    const std::string& first_screen_name,
    const chromeos::StartupCustomizationDocument* startup_manifest,
    chromeos::LoginDisplayHost* display_host) {
  TRACE_EVENT0("chromeos", "ShowLoginWizard::ShowLoginWizardFinish");

  scoped_ptr<base::DictionaryValue> params;
  display_host->StartWizard(first_screen_name, params.Pass());

  // Set initial timezone if specified by customization.
  const std::string timezone_name = startup_manifest->initial_timezone();
  VLOG(1) << "Initial time zone: " << timezone_name;
  // Apply locale customizations only once to preserve whatever locale
  // user has changed to during OOBE.
  if (!timezone_name.empty()) {
    chromeos::system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
        base::UTF8ToUTF16(timezone_name));
  }
}

struct ShowLoginWizardSwitchLanguageCallbackData {
  explicit ShowLoginWizardSwitchLanguageCallbackData(
      const std::string& first_screen_name,
      const chromeos::StartupCustomizationDocument* startup_manifest,
      chromeos::LoginDisplayHost* display_host)
      : first_screen_name(first_screen_name),
        startup_manifest(startup_manifest),
        display_host(display_host) {}

  const std::string first_screen_name;
  const chromeos::StartupCustomizationDocument* const startup_manifest;
  chromeos::LoginDisplayHost* const display_host;

  // lock UI while resource bundle is being reloaded.
  chromeos::InputEventsBlocker events_blocker;
};

void OnLanguageSwitchedCallback(
    scoped_ptr<ShowLoginWizardSwitchLanguageCallbackData> self,
    const std::string& locale,
    const std::string& loaded_locale,
    const bool success) {
  if (!success)
    LOG(WARNING) << "Locale could not be found for '" << locale << "'";

  ShowLoginWizardFinish(
      self->first_screen_name, self->startup_manifest, self->display_host);
}

void EnableSystemSoundsForAccessibility() {
  chromeos::AccessibilityManager::Get()->EnableSystemSounds(true);
}

void AddToSetIfIsGaiaAuthIframe(std::set<content::RenderFrameHost*>* frame_set,
                                content::RenderFrameHost* frame) {
  content::RenderFrameHost* parent = frame->GetParent();
  if (parent && parent->GetFrameName() == "signin-frame")
    frame_set->insert(frame);
}

// A login implementation of WidgetDelegate.
class LoginWidgetDelegate : public views::WidgetDelegate {
 public:
  explicit LoginWidgetDelegate(views::Widget* widget) : widget_(widget) {
  }
  virtual ~LoginWidgetDelegate() {}

  // Overridden from WidgetDelegate:
  virtual void DeleteDelegate() OVERRIDE {
    delete this;
  }
  virtual views::Widget* GetWidget() OVERRIDE {
    return widget_;
  }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return widget_;
  }
  virtual bool CanActivate() const OVERRIDE {
    return true;
  }
  virtual bool ShouldAdvanceFocusToTopLevelWidget() const OVERRIDE {
    return true;
  }

 private:
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(LoginWidgetDelegate);
};

// Disables virtual keyboard overscroll. Login UI will scroll user pods
// into view on JS side when virtual keyboard is shown.
void DisableKeyboardOverscroll() {
  keyboard::SetKeyboardOverscrollOverride(
      keyboard::KEYBOARD_OVERSCROLL_OVERRIDE_DISABLED);
}

void ResetKeyboardOverscrollOverride() {
  keyboard::SetKeyboardOverscrollOverride(
      keyboard::KEYBOARD_OVERSCROLL_OVERRIDE_NONE);
}

}  // namespace

namespace chromeos {

// static
LoginDisplayHost* LoginDisplayHostImpl::default_host_ = NULL;

// static
const int LoginDisplayHostImpl::kShowLoginWebUIid = 0x1111;

// static
content::RenderFrameHost* LoginDisplayHostImpl::GetGaiaAuthIframe(
    content::WebContents* web_contents) {
  std::set<content::RenderFrameHost*> frame_set;
  web_contents->ForEachFrame(
      base::Bind(&AddToSetIfIsGaiaAuthIframe, &frame_set));
  DCHECK_EQ(1U, frame_set.size());
  return *frame_set.begin();
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, public

LoginDisplayHostImpl::LoginDisplayHostImpl(const gfx::Rect& background_bounds)
    : background_bounds_(background_bounds),
      pointer_factory_(this),
      shutting_down_(false),
      oobe_progress_bar_visible_(false),
      session_starting_(false),
      login_window_(NULL),
      login_view_(NULL),
      webui_login_display_(NULL),
      is_showing_login_(false),
      is_wallpaper_loaded_(false),
      status_area_saved_visibility_(false),
      crash_count_(0),
      restore_path_(RESTORE_UNKNOWN),
      finalize_animation_type_(ANIMATION_WORKSPACE),
      animation_weak_ptr_factory_(this),
      startup_sound_played_(false),
      startup_sound_honors_spoken_feedback_(false),
      is_observing_keyboard_(false) {
  DBusThreadManager::Get()->GetSessionManagerClient()->AddObserver(this);
  CrasAudioHandler::Get()->AddAudioObserver(this);
  if (keyboard::KeyboardController::GetInstance()) {
    keyboard::KeyboardController::GetInstance()->AddObserver(this);
    is_observing_keyboard_ = true;
  }

  ash::Shell::GetInstance()->delegate()->AddVirtualKeyboardStateObserver(this);
  ash::Shell::GetScreen()->AddObserver(this);

  // We need to listen to CLOSE_ALL_BROWSERS_REQUEST but not APP_TERMINATING
  // because/ APP_TERMINATING will never be fired as long as this keeps
  // ref-count. CLOSE_ALL_BROWSERS_REQUEST is safe here because there will be no
  // browser instance that will block the shutdown.
  registrar_.Add(this,
                 chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
                 content::NotificationService::AllSources());

  // NOTIFICATION_BROWSER_OPENED is issued after browser is created, but
  // not shown yet. Lock window has to be closed at this point so that
  // a browser window exists and the window can acquire input focus.
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllSources());

  // Login screen is moved to lock screen container when user logs in.
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                 content::NotificationService::AllSources());

  DCHECK(default_host_ == NULL);
  default_host_ = this;

  // Make sure chrome won't exit while we are at login/oobe screen.
  chrome::IncrementKeepAliveCount();

  bool is_registered = StartupUtils::IsDeviceRegistered();
  bool zero_delay_enabled = WizardController::IsZeroDelayEnabled();
  bool disable_boot_animation = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableBootAnimation);

  waiting_for_wallpaper_load_ = !zero_delay_enabled &&
                                (!is_registered || !disable_boot_animation);

  // For slower hardware we have boot animation disabled so
  // we'll be initializing WebUI hidden, waiting for user pods to load and then
  // show WebUI at once.
  waiting_for_user_pods_ = !zero_delay_enabled && !waiting_for_wallpaper_load_;

  initialize_webui_hidden_ =
      kHiddenWebUIInitializationDefault && !zero_delay_enabled;

  // Check if WebUI init type is overriden.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshWebUIInit)) {
    const std::string override_type =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kAshWebUIInit);
    if (override_type == kWebUIInitParallel)
      initialize_webui_hidden_ = true;
    else if (override_type == kWebUIInitPostpone)
      initialize_webui_hidden_ = false;
  }

  // Always postpone WebUI initialization on first boot, otherwise we miss
  // initial animation.
  if (!StartupUtils::IsOobeCompleted())
    initialize_webui_hidden_ = false;

  // There is no wallpaper for KioskMode, don't initialize the webui hidden.
  if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled())
    initialize_webui_hidden_ = false;

  if (waiting_for_wallpaper_load_) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_WALLPAPER_ANIMATION_FINISHED,
                   content::NotificationService::AllSources());
  }

  // When we wait for WebUI to be initialized we wait for one of
  // these notifications.
  if ((waiting_for_user_pods_ || waiting_for_wallpaper_load_) &&
      initialize_webui_hidden_) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,
                   content::NotificationService::AllSources());
  }
  LOG(WARNING) << "Login WebUI >> "
               << "zero_delay: " << zero_delay_enabled
               << " wait_for_wp_load_: " << waiting_for_wallpaper_load_
               << " wait_for_pods_: " << waiting_for_user_pods_
               << " init_webui_hidden_: " << initialize_webui_hidden_;

  media::SoundsManager* manager = media::SoundsManager::Get();
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  manager->Initialize(chromeos::SOUND_STARTUP,
                      bundle.GetRawDataResource(IDR_SOUND_STARTUP_WAV));
}

LoginDisplayHostImpl::~LoginDisplayHostImpl() {
  DBusThreadManager::Get()->GetSessionManagerClient()->RemoveObserver(this);
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  if (keyboard::KeyboardController::GetInstance() && is_observing_keyboard_) {
    keyboard::KeyboardController::GetInstance()->RemoveObserver(this);
    is_observing_keyboard_ = false;
  }

  ash::Shell::GetInstance()->delegate()->
      RemoveVirtualKeyboardStateObserver(this);
  ash::Shell::GetScreen()->RemoveObserver(this);

  if (login::LoginScrollIntoViewEnabled())
    ResetKeyboardOverscrollOverride();

  views::FocusManager::set_arrow_key_traversal_enabled(false);
  ResetLoginWindowAndView();

  // Let chrome process exit after login/oobe screen if needed.
  chrome::DecrementKeepAliveCount();

  default_host_ = NULL;
  // TODO(tengs): This should be refactored. See crbug.com/314934.
  if (UserManager::Get()->IsCurrentUserNew()) {
    // DriveOptInController will delete itself when finished.
    (new DriveFirstRunController(
        ProfileManager::GetActiveUserProfile()))->EnableOfflineMode();
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, LoginDisplayHost implementation:

LoginDisplay* LoginDisplayHostImpl::CreateLoginDisplay(
    LoginDisplay::Delegate* delegate) {
  webui_login_display_ = new WebUILoginDisplay(delegate);
  webui_login_display_->set_background_bounds(background_bounds());
  return webui_login_display_;
}

gfx::NativeWindow LoginDisplayHostImpl::GetNativeWindow() const {
  return login_window_ ? login_window_->GetNativeWindow() : NULL;
}

WebUILoginView* LoginDisplayHostImpl::GetWebUILoginView() const {
  return login_view_;
}

void LoginDisplayHostImpl::BeforeSessionStart() {
  session_starting_ = true;
}

void LoginDisplayHostImpl::Finalize() {
  DVLOG(1) << "Session starting";
  if (ash::Shell::HasInstance()) {
    ash::Shell::GetInstance()->
        desktop_background_controller()->MoveDesktopToUnlockedContainer();
  }
  if (wizard_controller_.get())
    wizard_controller_->OnSessionStart();

  switch (finalize_animation_type_) {
    case ANIMATION_NONE:
      ShutdownDisplayHost(false);
      break;
    case ANIMATION_WORKSPACE:
      if (ash::Shell::HasInstance())
        ScheduleWorkspaceAnimation();

      ShutdownDisplayHost(false);
      break;
    case ANIMATION_FADE_OUT:
      // Display host is deleted once animation is completed
      // since sign in screen widget has to stay alive.
      ScheduleFadeOutAnimation();
      break;
  }
}

void LoginDisplayHostImpl::OnCompleteLogin() {
  if (auto_enrollment_controller_)
    auto_enrollment_controller_->Cancel();
}

void LoginDisplayHostImpl::OpenProxySettings() {
  if (login_view_)
    login_view_->OpenProxySettings();
}

void LoginDisplayHostImpl::SetStatusAreaVisible(bool visible) {
  if (initialize_webui_hidden_)
    status_area_saved_visibility_ = visible;
  else if (login_view_)
    login_view_->SetStatusAreaVisible(visible);
}

AutoEnrollmentController* LoginDisplayHostImpl::GetAutoEnrollmentController() {
  if (!auto_enrollment_controller_) {
    auto_enrollment_controller_.reset(new AutoEnrollmentController());
    auto_enrollment_progress_subscription_ =
        auto_enrollment_controller_->RegisterProgressCallback(
            base::Bind(&LoginDisplayHostImpl::OnAutoEnrollmentProgress,
                       base::Unretained(this)));
  }
  return auto_enrollment_controller_.get();
}

void LoginDisplayHostImpl::StartWizard(
    const std::string& first_screen_name,
    scoped_ptr<base::DictionaryValue> screen_parameters) {
  if (login::LoginScrollIntoViewEnabled())
    DisableKeyboardOverscroll();

  startup_sound_honors_spoken_feedback_ = true;
  TryToPlayStartupSound();

  // Keep parameters to restore if renderer crashes.
  restore_path_ = RESTORE_WIZARD;
  wizard_first_screen_name_ = first_screen_name;
  if (screen_parameters.get())
    wizard_screen_parameters_.reset(screen_parameters->DeepCopy());
  else
    wizard_screen_parameters_.reset();
  is_showing_login_ = false;

  if (waiting_for_wallpaper_load_ && !initialize_webui_hidden_) {
    LOG(WARNING) << "Login WebUI >> wizard postponed";
    return;
  }
  LOG(WARNING) << "Login WebUI >> wizard";

  if (!login_window_)
    LoadURL(GURL(kOobeURL));

  DVLOG(1) << "Starting wizard, first_screen_name: " << first_screen_name;
  // Create and show the wizard.
  // Note, dtor of the old WizardController should be called before ctor of the
  // new one, because "default_controller()" is updated there. So pure "reset()"
  // is done before new controller creation.
  wizard_controller_.reset();
  wizard_controller_.reset(CreateWizardController());

  oobe_progress_bar_visible_ = !StartupUtils::IsDeviceRegistered();
  SetOobeProgressBarVisible(oobe_progress_bar_visible_);
  wizard_controller_->Init(first_screen_name, screen_parameters.Pass());
}

WizardController* LoginDisplayHostImpl::GetWizardController() {
  return wizard_controller_.get();
}

AppLaunchController* LoginDisplayHostImpl::GetAppLaunchController() {
  return app_launch_controller_.get();
}

void LoginDisplayHostImpl::StartUserAdding(
    const base::Closure& completion_callback) {
  if (login::LoginScrollIntoViewEnabled())
    DisableKeyboardOverscroll();

  restore_path_ = RESTORE_ADD_USER_INTO_SESSION;
  completion_callback_ = completion_callback;
  finalize_animation_type_ = ANIMATION_NONE;
  LOG(WARNING) << "Login WebUI >> user adding";
  if (!login_window_)
    LoadURL(GURL(kUserAddingURL));
  // We should emit this signal only at login screen (after reboot or sign out).
  login_view_->set_should_emit_login_prompt_visible(false);

  // Lock container can be transparent after lock screen animation.
  aura::Window* lock_container = ash::Shell::GetContainer(
      ash::Shell::GetPrimaryRootWindow(),
      ash::kShellWindowId_LockScreenContainersContainer);
  lock_container->layer()->SetOpacity(1.0);

  ash::Shell::GetInstance()->
      desktop_background_controller()->MoveDesktopToLockedContainer();

  sign_in_controller_.reset();  // Only one controller in a time.
  sign_in_controller_.reset(new chromeos::ExistingUserController(this));
  SetOobeProgressBarVisible(oobe_progress_bar_visible_ = false);
  SetStatusAreaVisible(true);
  sign_in_controller_->Init(
      chromeos::UserManager::Get()->GetUsersAdmittedForMultiProfile());
  CHECK(webui_login_display_);
  GetOobeUI()->ShowSigninScreen(LoginScreenContext(),
                                webui_login_display_,
                                webui_login_display_);
}

void LoginDisplayHostImpl::StartSignInScreen(
    const LoginScreenContext& context) {
  if (login::LoginScrollIntoViewEnabled())
    DisableKeyboardOverscroll();

  startup_sound_honors_spoken_feedback_ = true;
  TryToPlayStartupSound();

  restore_path_ = RESTORE_SIGN_IN;
  is_showing_login_ = true;
  finalize_animation_type_ = ANIMATION_WORKSPACE;

  PrewarmAuthentication();

  if (waiting_for_wallpaper_load_ && !initialize_webui_hidden_) {
    LOG(WARNING) << "Login WebUI >> sign in postponed";
    return;
  }
  LOG(WARNING) << "Login WebUI >> sign in";

  if (!login_window_) {
    TRACE_EVENT_ASYNC_BEGIN0("ui", "ShowLoginWebUI", kShowLoginWebUIid);
    TRACE_EVENT_ASYNC_STEP_INTO0(
        "ui", "ShowLoginWebUI", kShowLoginWebUIid, "StartSignInScreen");
    BootTimesLoader::Get()->RecordCurrentStats("login-start-signin-screen");
    LoadURL(GURL(kLoginURL));
  }

  DVLOG(1) << "Starting sign in screen";
  const chromeos::UserList& users = chromeos::UserManager::Get()->GetUsers();

  // Fix for users who updated device and thus never passed register screen.
  // If we already have users, we assume that it is not a second part of
  // OOBE. See http://crosbug.com/6289
  if (!StartupUtils::IsDeviceRegistered() && !users.empty()) {
    VLOG(1) << "Mark device registered because there are remembered users: "
            << users.size();
    StartupUtils::MarkDeviceRegistered(base::Closure());
  }

  sign_in_controller_.reset();  // Only one controller in a time.
  sign_in_controller_.reset(new chromeos::ExistingUserController(this));
  oobe_progress_bar_visible_ = !StartupUtils::IsDeviceRegistered();
  SetOobeProgressBarVisible(oobe_progress_bar_visible_);
  SetStatusAreaVisible(true);
  sign_in_controller_->Init(users);

  // We might be here after a reboot that was triggered after OOBE was complete,
  // so check for auto-enrollment again. This might catch a cached decision from
  // a previous oobe flow, or might start a new check with the server.
  if (GetAutoEnrollmentController()->ShouldEnrollSilently())
    sign_in_controller_->DoAutoEnrollment();
  else
    GetAutoEnrollmentController()->Start();

  // Initiate mobile config load.
  MobileConfig::GetInstance();

  // Initiate device policy fetching.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  connector->ScheduleServiceInitialization(
      kPolicyServiceInitializationDelayMilliseconds);

  CHECK(webui_login_display_);
  GetOobeUI()->ShowSigninScreen(context,
                                webui_login_display_,
                                webui_login_display_);
  if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled())
    SetStatusAreaVisible(false);
  TRACE_EVENT_ASYNC_STEP_INTO0("ui",
                               "ShowLoginWebUI",
                               kShowLoginWebUIid,
                               "WaitForScreenStateInitialize");
  BootTimesLoader::Get()->RecordCurrentStats(
      "login-wait-for-signin-state-initialize");
}

void LoginDisplayHostImpl::ResumeSignInScreen() {
  // We only get here after a previous call the StartSignInScreen. That sign-in
  // was successful but was interrupted by an auto-enrollment execution; once
  // auto-enrollment is complete we resume the normal login flow from here.
  DVLOG(1) << "Resuming sign in screen";
  CHECK(sign_in_controller_.get());
  SetOobeProgressBarVisible(oobe_progress_bar_visible_);
  SetStatusAreaVisible(true);
  sign_in_controller_->ResumeLogin();
}


void LoginDisplayHostImpl::OnPreferencesChanged() {
  if (is_showing_login_)
    webui_login_display_->OnPreferencesChanged();
}

void LoginDisplayHostImpl::PrewarmAuthentication() {
  auth_prewarmer_.reset(new AuthPrewarmer());
  auth_prewarmer_->PrewarmAuthentication(
      base::Bind(&LoginDisplayHostImpl::OnAuthPrewarmDone,
                 pointer_factory_.GetWeakPtr()));
}

void LoginDisplayHostImpl::StartDemoAppLaunch() {
  LOG(WARNING) << "Login WebUI >> starting demo app.";
  SetStatusAreaVisible(false);

  demo_app_launcher_.reset(new DemoAppLauncher());
  demo_app_launcher_->StartDemoAppLaunch();
}

void LoginDisplayHostImpl::StartAppLaunch(const std::string& app_id,
                                          bool diagnostic_mode) {
  LOG(WARNING) << "Login WebUI >> start app launch.";
  SetStatusAreaVisible(false);
  finalize_animation_type_ = ANIMATION_FADE_OUT;
  if (!login_window_)
    LoadURL(GURL(kAppLaunchSplashURL));

  login_view_->set_should_emit_login_prompt_visible(false);

  app_launch_controller_.reset(new AppLaunchController(
      app_id, diagnostic_mode, this, GetOobeUI()));

  app_launch_controller_->StartAppLaunch();
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, public

WizardController* LoginDisplayHostImpl::CreateWizardController() {
  // TODO(altimofeev): ensure that WebUI is ready.
  OobeDisplay* oobe_display = GetOobeUI();
  return new WizardController(this, oobe_display);
}

void LoginDisplayHostImpl::OnBrowserCreated() {
  // Close lock window now so that the launched browser can receive focus.
  ResetLoginWindowAndView();
}

OobeUI* LoginDisplayHostImpl::GetOobeUI() const {
  if (!login_view_)
    return NULL;
  return static_cast<OobeUI*>(login_view_->GetWebUI()->GetController());
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, content:NotificationObserver implementation:

void LoginDisplayHostImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (chrome::NOTIFICATION_WALLPAPER_ANIMATION_FINISHED == type) {
    LOG(WARNING) << "Login WebUI >> wp animation done";
    is_wallpaper_loaded_ = true;
    ash::Shell::GetInstance()->user_wallpaper_delegate()
        ->OnWallpaperBootAnimationFinished();
    if (waiting_for_wallpaper_load_) {
      // StartWizard / StartSignInScreen could be called multiple times through
      // the lifetime of host.
      // Make sure that subsequent calls are not postponed.
      waiting_for_wallpaper_load_ = false;
      if (initialize_webui_hidden_)
        ShowWebUI();
      else
        StartPostponedWebUI();
    }
    registrar_.Remove(this,
                      chrome::NOTIFICATION_WALLPAPER_ANIMATION_FINISHED,
                      content::NotificationService::AllSources());
  } else if (chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE == type ||
             chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN == type) {
    LOG(WARNING) << "Login WebUI >> WEBUI_VISIBLE";
    if (waiting_for_user_pods_ && initialize_webui_hidden_) {
      waiting_for_user_pods_ = false;
      ShowWebUI();
    } else if (waiting_for_wallpaper_load_ && initialize_webui_hidden_) {
      // Reduce time till login UI is shown - show it as soon as possible.
      waiting_for_wallpaper_load_ = false;
      ShowWebUI();
    }
    registrar_.Remove(this,
                      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                      content::NotificationService::AllSources());
    registrar_.Remove(this,
                      chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,
                      content::NotificationService::AllSources());
  } else if (type == chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST) {
    ShutdownDisplayHost(true);
  } else if (type == chrome::NOTIFICATION_BROWSER_OPENED && session_starting_) {
    // Browsers created before session start (windows opened by extensions, for
    // example) are ignored.
    OnBrowserCreated();
    registrar_.Remove(this,
                      chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
                      content::NotificationService::AllSources());
    registrar_.Remove(this,
                      chrome::NOTIFICATION_BROWSER_OPENED,
                      content::NotificationService::AllSources());
  } else if (type == chrome::NOTIFICATION_LOGIN_USER_CHANGED &&
             chromeos::UserManager::Get()->IsCurrentUserNew()) {
    // For new user, move desktop to locker container so that windows created
    // during the user image picker step are below it.
    ash::Shell::GetInstance()->
        desktop_background_controller()->MoveDesktopToLockedContainer();
    registrar_.Remove(this,
                      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                      content::NotificationService::AllSources());
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, WebContentsObserver implementation:

void LoginDisplayHostImpl::RenderProcessGone(base::TerminationStatus status) {
  // Do not try to restore on shutdown
  if (browser_shutdown::GetShutdownType() != browser_shutdown::NOT_VALID)
    return;

  crash_count_++;
  if (crash_count_ > kCrashCountLimit)
    return;

  if (status != base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    // Render with login screen crashed. Let's crash browser process to let
    // session manager restart it properly. It is hard to reload the page
    // and get to controlled state that is fully functional.
    // If you see check, search for renderer crash for the same client.
    LOG(FATAL) << "Renderer crash on login window";
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, chromeos::SessionManagerClient::Observer
// implementation:

void LoginDisplayHostImpl::EmitLoginPromptVisibleCalled() {
  OnLoginPromptVisible();
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, chromeos::CrasAudioHandler::AudioObserver
// implementation:

void LoginDisplayHostImpl::OnActiveOutputNodeChanged() {
  TryToPlayStartupSound();
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, ash::KeyboardStateObserver:
// implementation:

void LoginDisplayHostImpl::OnVirtualKeyboardStateChanged(bool activated) {
  if (keyboard::KeyboardController::GetInstance()) {
    if (activated) {
      if (!is_observing_keyboard_) {
        keyboard::KeyboardController::GetInstance()->AddObserver(this);
        is_observing_keyboard_ = true;
      }
    } else {
      keyboard::KeyboardController::GetInstance()->RemoveObserver(this);
      is_observing_keyboard_ = false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, keyboard::KeyboardControllerObserver:
// implementation:

void LoginDisplayHostImpl::OnKeyboardBoundsChanging(
    const gfx::Rect& new_bounds) {
  if (new_bounds.IsEmpty() && !keyboard_bounds_.IsEmpty()) {
    // Keyboard has been hidden.
    if (GetOobeUI()) {
      GetOobeUI()->GetCoreOobeActor()->ShowControlBar(true);
      if (login::LoginScrollIntoViewEnabled())
        GetOobeUI()->GetCoreOobeActor()->SetKeyboardState(false, new_bounds);
    }
  } else if (!new_bounds.IsEmpty() && keyboard_bounds_.IsEmpty()) {
    // Keyboard has been shown.
    if (GetOobeUI()) {
      GetOobeUI()->GetCoreOobeActor()->ShowControlBar(false);
      if (login::LoginScrollIntoViewEnabled())
        GetOobeUI()->GetCoreOobeActor()->SetKeyboardState(true, new_bounds);
    }
  }

  keyboard_bounds_ = new_bounds;
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, gfx::DisplayObserver implementation:

void LoginDisplayHostImpl::OnDisplayAdded(const gfx::Display& new_display) {
}

void LoginDisplayHostImpl::OnDisplayRemoved(const gfx::Display& old_display) {
}

void LoginDisplayHostImpl::OnDisplayMetricsChanged(const gfx::Display& display,
                                                   uint32_t changed_metrics) {
  if (display.id() != ash::Shell::GetScreen()->GetPrimaryDisplay().id() ||
      !(changed_metrics & DISPLAY_METRIC_BOUNDS)) {
    return;
  }

  if (GetOobeUI()) {
    const gfx::Size& size = ash::Shell::GetScreen()->GetPrimaryDisplay().size();
    GetOobeUI()->GetCoreOobeActor()->SetClientAreaSize(size.width(),
                                                       size.height());
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, private

void LoginDisplayHostImpl::ShutdownDisplayHost(bool post_quit_task) {
  if (shutting_down_)
    return;

  shutting_down_ = true;
  registrar_.RemoveAll();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  if (post_quit_task)
    base::MessageLoop::current()->Quit();

  if (!completion_callback_.is_null())
    completion_callback_.Run();
}

void LoginDisplayHostImpl::ScheduleWorkspaceAnimation() {
  if (ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                               ash::kShellWindowId_DesktopBackgroundContainer)
          ->children()
          .empty()) {
    // If there is no background window, don't perform any animation on the
    // default and background layer because there is nothing behind it.
    return;
  }

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableLoginAnimations))
    ash::Shell::GetInstance()->DoInitialWorkspaceAnimation();
}

void LoginDisplayHostImpl::ScheduleFadeOutAnimation() {
  ui::Layer* layer = login_window_->GetLayer();
  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.AddObserver(new AnimationObserver(
      base::Bind(&LoginDisplayHostImpl::ShutdownDisplayHost,
                 animation_weak_ptr_factory_.GetWeakPtr(),
                 false)));
  layer->SetOpacity(0);
}

void LoginDisplayHostImpl::OnAutoEnrollmentProgress(
    policy::AutoEnrollmentState state) {
  VLOG(1) << "OnAutoEnrollmentProgress, state " << state;

  if (sign_in_controller_ &&
      auto_enrollment_controller_->ShouldEnrollSilently()) {
    sign_in_controller_->DoAutoEnrollment();
  }
}

void LoginDisplayHostImpl::LoadURL(const GURL& url) {
  InitLoginWindowAndView();
  // Subscribe to crash events.
  content::WebContentsObserver::Observe(login_view_->GetWebContents());
  login_view_->LoadURL(url);

  // LoadURL could be called after the spring charger dialog shows, and
  // take away the focus from it. Set the focus back to the charger dialog
  // if it is visible.
  // See crbug.com/328538.
  ChargerReplacementDialog::SetFocusOnChargerDialogIfVisible();
}

void LoginDisplayHostImpl::ShowWebUI() {
  if (!login_window_ || !login_view_) {
    NOTREACHED();
    return;
  }
  LOG(WARNING) << "Login WebUI >> Show already initialized UI";
  login_window_->Show();
  login_view_->GetWebContents()->Focus();
  if (::switches::IsTextInputFocusManagerEnabled())
    login_view_->RequestFocus();
  login_view_->SetStatusAreaVisible(status_area_saved_visibility_);
  login_view_->OnPostponedShow();

  // Login window could be shown after the spring charger dialog shows, and
  // take away the focus from it. Set the focus back to the charger dialog
  // if it is visible.
  // See crbug.com/328538.
  ChargerReplacementDialog::SetFocusOnChargerDialogIfVisible();

  // We should reset this flag to allow changing of status area visibility.
  initialize_webui_hidden_ = false;
}

void LoginDisplayHostImpl::StartPostponedWebUI() {
  if (!is_wallpaper_loaded_) {
    NOTREACHED();
    return;
  }
  LOG(WARNING) << "Login WebUI >> Init postponed WebUI";

  // Wallpaper has finished loading before StartWizard/StartSignInScreen has
  // been called. In general this should not happen.
  // Let go through normal code path when one of those will be called.
  if (restore_path_ == RESTORE_UNKNOWN) {
    NOTREACHED();
    return;
  }

  switch (restore_path_) {
    case RESTORE_WIZARD:
      StartWizard(wizard_first_screen_name_,
                  wizard_screen_parameters_.Pass());
      break;
    case RESTORE_SIGN_IN:
      StartSignInScreen(LoginScreenContext());
      break;
    case RESTORE_ADD_USER_INTO_SESSION:
      StartUserAdding(completion_callback_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void LoginDisplayHostImpl::InitLoginWindowAndView() {
  if (login_window_)
    return;

  if (system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation()) {
    views::FocusManager::set_arrow_key_traversal_enabled(true);

    focus_ring_controller_.reset(new FocusRingController);
    focus_ring_controller_->SetVisible(true);

    keyboard_driven_oobe_key_handler_.reset(new KeyboardDrivenOobeKeyHandler);
  }

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = background_bounds();
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent =
      ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                               ash::kShellWindowId_LockScreenContainer);

  login_window_ = new views::Widget;
  params.delegate = new LoginWidgetDelegate(login_window_);
  login_window_->Init(params);

  login_view_ = new WebUILoginView();
  login_view_->Init();
  if (login_view_->webui_visible())
    OnLoginPromptVisible();

  wm::SetWindowVisibilityAnimationDuration(
      login_window_->GetNativeView(),
      base::TimeDelta::FromMilliseconds(kLoginFadeoutTransitionDurationMs));
  wm::SetWindowVisibilityAnimationTransition(
      login_window_->GetNativeView(),
      wm::ANIMATE_HIDE);

  login_window_->SetContentsView(login_view_);

  // If WebUI is initialized in hidden state, show it only if we're no
  // longer waiting for wallpaper animation/user images loading. Otherwise,
  // always show it.
  if (!initialize_webui_hidden_ ||
      (!waiting_for_wallpaper_load_ && !waiting_for_user_pods_)) {
    LOG(WARNING) << "Login WebUI >> show login wnd on create";
    login_window_->Show();
  } else {
    LOG(WARNING) << "Login WebUI >> login wnd is hidden on create";
    login_view_->set_is_hidden(true);
  }
  login_window_->GetNativeView()->SetName("WebUILoginView");
}

void LoginDisplayHostImpl::ResetLoginWindowAndView() {
  if (!login_window_)
    return;
  login_window_->Close();
  login_window_ = NULL;
  login_view_ = NULL;
}

void LoginDisplayHostImpl::OnAuthPrewarmDone() {
  auth_prewarmer_.reset();
}

void LoginDisplayHostImpl::SetOobeProgressBarVisible(bool visible) {
  GetOobeUI()->ShowOobeUI(visible);
}

void LoginDisplayHostImpl::TryToPlayStartupSound() {
  if (startup_sound_played_ || login_prompt_visible_time_.is_null() ||
      !CrasAudioHandler::Get()->GetActiveOutputNode()) {
    return;
  }

  startup_sound_played_ = true;

  // Don't try play startup sound if login prompt is already visible
  // for a long time or can't be played.
  if (base::TimeTicks::Now() - login_prompt_visible_time_ >
      base::TimeDelta::FromMilliseconds(kStartupSoundMaxDelayMs)) {
    EnableSystemSoundsForAccessibility();
    return;
  }

  if (!startup_sound_honors_spoken_feedback_ &&
      !ash::PlaySystemSoundAlways(SOUND_STARTUP)) {
    EnableSystemSoundsForAccessibility();
    return;
  }

  if (startup_sound_honors_spoken_feedback_ &&
      !ash::PlaySystemSoundIfSpokenFeedback(SOUND_STARTUP)) {
    EnableSystemSoundsForAccessibility();
    return;
  }

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&EnableSystemSoundsForAccessibility),
      media::SoundsManager::Get()->GetDuration(SOUND_STARTUP));
}

void LoginDisplayHostImpl::OnLoginPromptVisible() {
  if (!login_prompt_visible_time_.is_null())
    return;
  login_prompt_visible_time_ = base::TimeTicks::Now();
  TryToPlayStartupSound();
}

////////////////////////////////////////////////////////////////////////////////
// external

// Declared in login_wizard.h so that others don't need to depend on our .h.
// TODO(nkostylev): Split this into a smaller functions.
void ShowLoginWizard(const std::string& first_screen_name) {
  if (browser_shutdown::IsTryingToQuit())
    return;

  VLOG(1) << "Showing OOBE screen: " << first_screen_name;

  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();

  // Set up keyboards. For example, when |locale| is "en-US", enable US qwerty
  // and US dvorak keyboard layouts.
  if (g_browser_process && g_browser_process->local_state()) {
    manager->SetInputMethodLoginDefault();

    PrefService* prefs = g_browser_process->local_state();
    // Apply owner preferences for tap-to-click and mouse buttons swap for
    // login screen.
    system::InputDeviceSettings::Get()->SetPrimaryButtonRight(
        prefs->GetBoolean(prefs::kOwnerPrimaryMouseButtonRight));
    system::InputDeviceSettings::Get()->SetTapToClick(
        prefs->GetBoolean(prefs::kOwnerTapToClickEnabled));
  }
  system::InputDeviceSettings::Get()->SetNaturalScroll(
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNaturalScrollDefault));

  gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));

  // Check whether we need to execute OOBE process.
  bool oobe_complete = chromeos::StartupUtils::IsOobeCompleted();
  if (!oobe_complete) {
    LoginState::Get()->SetLoggedInState(
        LoginState::LOGGED_IN_OOBE, LoginState::LOGGED_IN_USER_NONE);
  } else {
    LoginState::Get()->SetLoggedInState(
        LoginState::LOGGED_IN_NONE, LoginState::LOGGED_IN_USER_NONE);
  }

  LoginDisplayHost* display_host = new LoginDisplayHostImpl(screen_bounds);

  bool show_app_launch_splash_screen = (first_screen_name ==
      chromeos::WizardController::kAppLaunchSplashScreenName);
  if (show_app_launch_splash_screen) {
    const std::string& auto_launch_app_id =
        chromeos::KioskAppManager::Get()->GetAutoLaunchApp();
    display_host->StartAppLaunch(auto_launch_app_id,
                                 false /* diagnostic_mode */);
    return;
  }

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  bool enrollment_screen_wanted =
      chromeos::WizardController::ShouldRecoverEnrollment() ||
      (chromeos::WizardController::ShouldAutoStartEnrollment() &&
       oobe_complete &&
       !connector->IsEnterpriseManaged());
  if (enrollment_screen_wanted && first_screen_name.empty()) {
    // Shows networks screen instead of enrollment screen to resume the
    // interrupted auto start enrollment flow because enrollment screen does
    // not handle flaky network. See http://crbug.com/332572
    display_host->StartWizard(chromeos::WizardController::kNetworkScreenName,
                              scoped_ptr<base::DictionaryValue>());
    return;
  }

  if (StartupUtils::IsEulaAccepted()) {
    DelayNetworkCall(
        ServicesCustomizationDocument::GetInstance()
            ->EnsureCustomizationAppliedClosure(),
        base::TimeDelta::FromMilliseconds(kDefaultNetworkRetryDelayMS));
  }

  bool show_login_screen =
      (first_screen_name.empty() && oobe_complete) ||
      first_screen_name == chromeos::WizardController::kLoginScreenName;

  if (show_login_screen) {
    display_host->StartSignInScreen(LoginScreenContext());
    return;
  }

  // Load startup manifest.
  const chromeos::StartupCustomizationDocument* startup_manifest =
      chromeos::StartupCustomizationDocument::GetInstance();

  // Switch to initial locale if specified by customization
  // and has not been set yet. We cannot call
  // chromeos::LanguageSwitchMenu::SwitchLanguage here before
  // EmitLoginPromptReady.
  PrefService* prefs = g_browser_process->local_state();
  const std::string& current_locale =
      prefs->GetString(prefs::kApplicationLocale);
  VLOG(1) << "Current locale: " << current_locale;
  const std::string& locale = startup_manifest->initial_locale_default();

  const std::string& layout = startup_manifest->keyboard_layout();
  VLOG(1) << "Initial locale: " << locale << "keyboard layout " << layout;

  // Determine keyboard layout from OEM customization (if provided) or
  // initial locale and save it in preferences.
  manager->SetInputMethodLoginDefaultFromVPD(locale, layout);

  if (!current_locale.empty() || locale.empty()) {
    ShowLoginWizardFinish(first_screen_name, startup_manifest, display_host);
    return;
  }

  // Save initial locale from VPD/customization manifest as current
  // Chrome locale. Otherwise it will be lost if Chrome restarts.
  // Don't need to schedule pref save because setting initial local
  // will enforce preference saving.
  prefs->SetString(prefs::kApplicationLocale, locale);
  chromeos::StartupUtils::SetInitialLocale(locale);

  scoped_ptr<ShowLoginWizardSwitchLanguageCallbackData> data(
      new ShowLoginWizardSwitchLanguageCallbackData(
          first_screen_name, startup_manifest, display_host));

  scoped_ptr<locale_util::SwitchLanguageCallback> callback(
      new locale_util::SwitchLanguageCallback(
          base::Bind(&OnLanguageSwitchedCallback, base::Passed(data.Pass()))));

  // Load locale keyboards here. Hardware layout would be automatically enabled.
  locale_util::SwitchLanguage(
      locale, true, true /* login_layouts_only */, callback.Pass());
}

}  // namespace chromeos
