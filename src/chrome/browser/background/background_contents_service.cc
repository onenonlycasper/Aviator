// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_contents_service.h"

#include "apps/app_load_service.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ipc/ipc_message.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

#if defined(ENABLE_NOTIFICATIONS)
#include "ui/message_center/message_center.h"
#endif

using content::SiteInstance;
using content::WebContents;
using extensions::BackgroundInfo;
using extensions::Extension;
using extensions::UnloadedExtensionInfo;

namespace {

const char kNotificationPrefix[] = "app.background.crashed.";

void CloseBalloon(const std::string& balloon_id) {
  NotificationUIManager* notification_ui_manager =
      g_browser_process->notification_ui_manager();
  bool cancelled ALLOW_UNUSED = notification_ui_manager->CancelById(balloon_id);
#if defined(ENABLE_NOTIFICATIONS)
  if (cancelled) {
    // TODO(dewittj): Add this functionality to the notification UI manager's
    // API.
    g_browser_process->message_center()->SetVisibility(
        message_center::VISIBILITY_TRANSIENT);
  }
#endif
}

// Closes the crash notification balloon for the app/extension with this id.
void ScheduleCloseBalloon(const std::string& extension_id) {
  if (!base::MessageLoop::current())  // For unit_tests
    return;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&CloseBalloon, kNotificationPrefix + extension_id));
}

// Delegate for the app/extension crash notification balloon. Restarts the
// app/extension when the balloon is clicked.
class CrashNotificationDelegate : public NotificationDelegate {
 public:
  CrashNotificationDelegate(Profile* profile,
                            const Extension* extension)
      : profile_(profile),
        is_hosted_app_(extension->is_hosted_app()),
        is_platform_app_(extension->is_platform_app()),
        extension_id_(extension->id()) {
  }

  virtual void Display() OVERRIDE {}

  virtual void Error() OVERRIDE {}

  virtual void Close(bool by_user) OVERRIDE {}

  virtual void Click() OVERRIDE {
    // http://crbug.com/247790 involves a crash notification balloon being
    // clicked while the extension isn't in the TERMINATED state. In that case,
    // any of the "reload" methods called below can unload the extension, which
    // indirectly destroys *this, invalidating all the member variables, so we
    // copy the extension ID before using it.
    std::string copied_extension_id = extension_id_;
    if (is_hosted_app_) {
      // There can be a race here: user clicks the balloon, and simultaneously
      // reloads the sad tab for the app. So we check here to be safe before
      // loading the background page.
      BackgroundContentsService* service =
          BackgroundContentsServiceFactory::GetForProfile(profile_);
      if (!service->GetAppBackgroundContents(
              base::ASCIIToUTF16(copied_extension_id))) {
        service->LoadBackgroundContentsForExtension(profile_,
                                                    copied_extension_id);
      }
    } else if (is_platform_app_) {
      apps::AppLoadService::Get(profile_)->
          RestartApplication(copied_extension_id);
    } else {
      extensions::ExtensionSystem::Get(profile_)->extension_service()->
          ReloadExtension(copied_extension_id);
    }

    // Closing the crash notification balloon for the app/extension here should
    // be OK, but it causes a crash on Mac, see: http://crbug.com/78167
    ScheduleCloseBalloon(copied_extension_id);
  }

  virtual bool HasClickedListener() OVERRIDE { return true; }

  virtual std::string id() const OVERRIDE {
    return kNotificationPrefix + extension_id_;
  }

  virtual content::WebContents* GetWebContents() const OVERRIDE {
    return NULL;
  }

 private:
  virtual ~CrashNotificationDelegate() {}

  Profile* profile_;
  bool is_hosted_app_;
  bool is_platform_app_;
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(CrashNotificationDelegate);
};

#if defined(ENABLE_NOTIFICATIONS)
void NotificationImageReady(
    const std::string extension_name,
    const base::string16 message,
    scoped_refptr<CrashNotificationDelegate> delegate,
    Profile* profile,
    const gfx::Image& icon) {
  gfx::Image notification_icon(icon);
  if (notification_icon.IsEmpty()) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    notification_icon = rb.GetImageNamed(IDR_EXTENSION_DEFAULT_ICON);
  }

  // Origin URL must be different from the crashed extension to avoid the
  // conflict. NotificationSystemObserver will cancel all notifications from
  // the same origin when NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED.
  // TODO(mukai, dewittj): remove this and switch to message center
  // notifications.
  DesktopNotificationService::AddIconNotification(
      GURL("aviator://extension-crash"),  // Origin URL.
      base::string16(),                  // Title of notification.
      message,
      notification_icon,
      base::UTF8ToUTF16(delegate->id()),  // Replace ID.
      delegate.get(),
      profile);
}
#endif

// Show a popup notification balloon with a crash message for a given app/
// extension.
void ShowBalloon(const Extension* extension, Profile* profile) {
#if defined(ENABLE_NOTIFICATIONS)
  const base::string16 message = l10n_util::GetStringFUTF16(
      extension->is_app() ? IDS_BACKGROUND_CRASHED_APP_BALLOON_MESSAGE :
                            IDS_BACKGROUND_CRASHED_EXTENSION_BALLOON_MESSAGE,
      base::UTF8ToUTF16(extension->name()));
  extension_misc::ExtensionIcons size(extension_misc::EXTENSION_ICON_MEDIUM);
  extensions::ExtensionResource resource =
      extensions::IconsInfo::GetIconResource(
          extension, size, ExtensionIconSet::MATCH_SMALLER);
  // We can't just load the image in the Observe method below because, despite
  // what this method is called, it may call the callback synchronously.
  // However, it's possible that the extension went away during the interim,
  // so we'll bind all the pertinent data here.
  extensions::ImageLoader::Get(profile)->LoadImageAsync(
      extension,
      resource,
      gfx::Size(size, size),
      base::Bind(
          &NotificationImageReady,
          extension->name(),
          message,
          make_scoped_refptr(new CrashNotificationDelegate(profile, extension)),
          profile));
#endif
}

void ReloadExtension(const std::string& extension_id, Profile* profile) {
  if (g_browser_process->IsShuttingDown() ||
      !g_browser_process->profile_manager()->IsValidProfile(profile)) {
      return;
  }

  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  if (!extension_system || !extension_system->extension_service() ||
      !extension_registry) {
    return;
  }

  if (!extension_registry->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::TERMINATED)) {
    // Either the app/extension was uninstalled by policy or it has since
    // been restarted successfully by someone else (the user).
    return;
  }
  extension_system->extension_service()->ReloadExtension(extension_id);
}

}  // namespace

// Keys for the information we store about individual BackgroundContents in
// prefs. There is one top-level DictionaryValue (stored at
// prefs::kRegisteredBackgroundContents). Information about each
// BackgroundContents is stored under that top-level DictionaryValue, keyed
// by the parent application ID for easy lookup.
//
// kRegisteredBackgroundContents:
//    DictionaryValue {
//       <appid_1>: { "url": <url1>, "name": <frame_name> },
//       <appid_2>: { "url": <url2>, "name": <frame_name> },
//         ... etc ...
//    }
const char kUrlKey[] = "url";
const char kFrameNameKey[] = "name";

int BackgroundContentsService::restart_delay_in_ms_ = 3000;  // 3 seconds.

BackgroundContentsService::BackgroundContentsService(
    Profile* profile, const CommandLine* command_line)
    : prefs_(NULL) {
  // Don't load/store preferences if the parent profile is incognito.
  if (!profile->IsOffTheRecord())
    prefs_ = profile->GetPrefs();

  // Listen for events to tell us when to load/unload persisted background
  // contents.
  StartObserving(profile);
}

BackgroundContentsService::~BackgroundContentsService() {
  // BackgroundContents should be shutdown before we go away, as otherwise
  // our browser process refcount will be off.
  DCHECK(contents_map_.empty());
}

// static
void BackgroundContentsService::
    SetRestartDelayForForceInstalledAppsAndExtensionsForTesting(
        int restart_delay_in_ms) {
  restart_delay_in_ms_ = restart_delay_in_ms;
}

// static
std::string BackgroundContentsService::GetNotificationIdForExtensionForTesting(
    const std::string& extension_id) {
  return kNotificationPrefix + extension_id;
}

// static
void BackgroundContentsService::ShowBalloonForTesting(
    const extensions::Extension* extension,
    Profile* profile) {
  ShowBalloon(extension, profile);
}

std::vector<BackgroundContents*>
BackgroundContentsService::GetBackgroundContents() const
{
  std::vector<BackgroundContents*> contents;
  for (BackgroundContentsMap::const_iterator it = contents_map_.begin();
       it != contents_map_.end(); ++it)
    contents.push_back(it->second.contents);
  return contents;
}

void BackgroundContentsService::StartObserving(Profile* profile) {
  // On startup, load our background pages after extension-apps have loaded.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                 content::Source<Profile>(profile));

  // Track the lifecycle of all BackgroundContents in the system to allow us
  // to store an up-to-date list of the urls. Start tracking contents when they
  // have been opened via CreateBackgroundContents(), and stop tracking them
  // when they are closed by script.
  registrar_.Add(this, chrome::NOTIFICATION_BACKGROUND_CONTENTS_CLOSED,
                 content::Source<Profile>(profile));

  // Stop tracking BackgroundContents when they have been deleted (happens
  // during shutdown or if the render process dies).
  registrar_.Add(this, chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED,
                 content::Source<Profile>(profile));

  // Track when the BackgroundContents navigates to a new URL so we can update
  // our persisted information as appropriate.
  registrar_.Add(this, chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED,
                 content::Source<Profile>(profile));

  // Listen for new extension installs so that we can load any associated
  // background page.
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
                 content::Source<Profile>(profile));

  // Track when the extensions crash so that the user can be notified
  // about it, and the crashed contents can be restarted.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_BACKGROUND_CONTENTS_TERMINATED,
                 content::Source<Profile>(profile));

  // Listen for extensions to be unloaded so we can shutdown associated
  // BackgroundContents.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                 content::Source<Profile>(profile));

  // Make sure the extension-crash balloons are removed when the extension is
  // uninstalled/reloaded. We cannot do this from UNLOADED since a crashed
  // extension is unloaded immediately after the crash, not when user reloads or
  // uninstalls the extension.
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED,
                 content::Source<Profile>(profile));
}

void BackgroundContentsService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSIONS_READY: {
      Profile* profile = content::Source<Profile>(source).ptr();
      LoadBackgroundContentsFromManifests(profile);
      LoadBackgroundContentsFromPrefs(profile);
      SendChangeNotification(profile);
      break;
    }
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED:
      BackgroundContentsShutdown(
          content::Details<BackgroundContents>(details).ptr());
      SendChangeNotification(content::Source<Profile>(source).ptr());
      break;
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_CLOSED:
      DCHECK(IsTracked(content::Details<BackgroundContents>(details).ptr()));
      UnregisterBackgroundContents(
          content::Details<BackgroundContents>(details).ptr());
      // CLOSED is always followed by a DELETED notification so we'll send our
      // change notification there.
      break;
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED: {
      DCHECK(IsTracked(content::Details<BackgroundContents>(details).ptr()));

      // Do not register in the pref if the extension has a manifest-specified
      // background page.
      BackgroundContents* bgcontents =
          content::Details<BackgroundContents>(details).ptr();
      Profile* profile = content::Source<Profile>(source).ptr();
      const base::string16& appid = GetParentApplicationId(bgcontents);
      ExtensionService* extension_service =
          extensions::ExtensionSystem::Get(profile)->extension_service();
      // extension_service can be NULL when running tests.
      if (extension_service) {
        const Extension* extension = extension_service->GetExtensionById(
            base::UTF16ToUTF8(appid), false);
        if (extension && BackgroundInfo::HasBackgroundPage(extension))
          break;
      }
      RegisterBackgroundContents(bgcontents);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_LOADED_DEPRECATED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      Profile* profile = content::Source<Profile>(source).ptr();
      if (extension->is_hosted_app() &&
          BackgroundInfo::HasBackgroundPage(extension)) {
        // If there is a background page specified in the manifest for a hosted
        // app, then blow away registered urls in the pref.
        ShutdownAssociatedBackgroundContents(
            base::ASCIIToUTF16(extension->id()));

        ExtensionService* service =
            extensions::ExtensionSystem::Get(profile)->extension_service();
        if (service && service->is_ready()) {
          // Now load the manifest-specified background page. If service isn't
          // ready, then the background page will be loaded from the
          // EXTENSIONS_READY callback.
          LoadBackgroundContents(profile,
                                 BackgroundInfo::GetBackgroundURL(extension),
                                 base::ASCIIToUTF16("background"),
                                 base::UTF8ToUTF16(extension->id()));
        }
      }

      // Close the crash notification balloon for the app/extension, if any.
      ScheduleCloseBalloon(extension->id());
      SendChangeNotification(profile);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED:
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_TERMINATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      const Extension* extension = NULL;
      if (type == chrome::NOTIFICATION_BACKGROUND_CONTENTS_TERMINATED) {
        BackgroundContents* bg =
            content::Details<BackgroundContents>(details).ptr();
        std::string extension_id = base::UTF16ToASCII(
            BackgroundContentsServiceFactory::GetForProfile(profile)->
                GetParentApplicationId(bg));
        extension =
          extensions::ExtensionSystem::Get(profile)->extension_service()->
              GetExtensionById(extension_id, false);
      } else {
        extensions::ExtensionHost* extension_host =
            content::Details<extensions::ExtensionHost>(details).ptr();
        extension = extension_host->extension();
      }
      if (!extension)
        break;

      const bool force_installed =
          extensions::Manifest::IsComponentLocation(extension->location()) ||
          extensions::Manifest::IsPolicyLocation(extension->location());
      if (!force_installed) {
        ShowBalloon(extension, profile);
      } else {
        // Restart the extension.
        RestartForceInstalledExtensionOnCrash(extension, profile);
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED:
      switch (content::Details<UnloadedExtensionInfo>(details)->reason) {
        case UnloadedExtensionInfo::REASON_DISABLE:    // Fall through.
        case UnloadedExtensionInfo::REASON_TERMINATE:  // Fall through.
        case UnloadedExtensionInfo::REASON_UNINSTALL:  // Fall through.
        case UnloadedExtensionInfo::REASON_BLACKLIST:  // Fall through.
        case UnloadedExtensionInfo::REASON_PROFILE_SHUTDOWN:
          ShutdownAssociatedBackgroundContents(base::ASCIIToUTF16(
              content::Details<UnloadedExtensionInfo>(details)->
                  extension->id()));
          SendChangeNotification(content::Source<Profile>(source).ptr());
          break;
        case UnloadedExtensionInfo::REASON_UPDATE: {
          // If there is a manifest specified background page, then shut it down
          // here, since if the updated extension still has the background page,
          // then it will be loaded from LOADED callback. Otherwise, leave
          // BackgroundContents in place.
          // We don't call SendChangeNotification here - it will be generated
          // from the LOADED callback.
          const Extension* extension =
              content::Details<UnloadedExtensionInfo>(details)->extension;
          if (BackgroundInfo::HasBackgroundPage(extension)) {
            ShutdownAssociatedBackgroundContents(
                base::ASCIIToUTF16(extension->id()));
          }
          break;
        }
        default:
          NOTREACHED();
          ShutdownAssociatedBackgroundContents(base::ASCIIToUTF16(
              content::Details<UnloadedExtensionInfo>(details)->
                  extension->id()));
          break;
      }
      break;

    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED: {
      // Close the crash notification balloon for the app/extension, if any.
      ScheduleCloseBalloon(
          content::Details<const Extension>(details).ptr()->id());
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

void BackgroundContentsService::RestartForceInstalledExtensionOnCrash(
    const Extension* extension,
    Profile* profile) {
  base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&ReloadExtension, extension->id(), profile),
      base::TimeDelta::FromMilliseconds(restart_delay_in_ms_));
}

// Loads all background contents whose urls have been stored in prefs.
void BackgroundContentsService::LoadBackgroundContentsFromPrefs(
    Profile* profile) {
  if (!prefs_)
    return;
  const base::DictionaryValue* contents =
      prefs_->GetDictionary(prefs::kRegisteredBackgroundContents);
  if (!contents)
    return;
  ExtensionService* extensions_service =
          extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(extensions_service);
  for (base::DictionaryValue::Iterator it(*contents);
       !it.IsAtEnd(); it.Advance()) {
    // Check to make sure that the parent extension is still enabled.
    const Extension* extension = extensions_service->
        GetExtensionById(it.key(), false);
    if (!extension) {
      // We should never reach here - it should not be possible for an app
      // to become uninstalled without the associated BackgroundContents being
      // unregistered via the EXTENSIONS_UNLOADED notification, unless there's a
      // crash before we could save our prefs, or if the user deletes the
      // extension files manually rather than uninstalling it.
      NOTREACHED() << "No extension found for BackgroundContents - id = "
                   << it.key();
      // Don't cancel out of our loop, just ignore this BackgroundContents and
      // load the next one.
      continue;
    }
    LoadBackgroundContentsFromDictionary(profile, it.key(), contents);
  }
}

void BackgroundContentsService::SendChangeNotification(Profile* profile) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_CONTENTS_SERVICE_CHANGED,
      content::Source<Profile>(profile),
      content::Details<BackgroundContentsService>(this));
}

void BackgroundContentsService::LoadBackgroundContentsForExtension(
    Profile* profile,
    const std::string& extension_id) {
  // First look if the manifest specifies a background page.
  const Extension* extension =
      extensions::ExtensionSystem::Get(profile)->extension_service()->
          GetExtensionById(extension_id, false);
  DCHECK(!extension || extension->is_hosted_app());
  if (extension && BackgroundInfo::HasBackgroundPage(extension)) {
    LoadBackgroundContents(profile,
                           BackgroundInfo::GetBackgroundURL(extension),
                           base::ASCIIToUTF16("background"),
                           base::UTF8ToUTF16(extension->id()));
    return;
  }

  // Now look in the prefs.
  if (!prefs_)
    return;
  const base::DictionaryValue* contents =
      prefs_->GetDictionary(prefs::kRegisteredBackgroundContents);
  if (!contents)
    return;
  LoadBackgroundContentsFromDictionary(profile, extension_id, contents);
}

void BackgroundContentsService::LoadBackgroundContentsFromDictionary(
    Profile* profile,
    const std::string& extension_id,
    const base::DictionaryValue* contents) {
  ExtensionService* extensions_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(extensions_service);

  const base::DictionaryValue* dict;
  if (!contents->GetDictionaryWithoutPathExpansion(extension_id, &dict) ||
      dict == NULL)
    return;

  base::string16 frame_name;
  std::string url;
  dict->GetString(kUrlKey, &url);
  dict->GetString(kFrameNameKey, &frame_name);
  LoadBackgroundContents(profile,
                         GURL(url),
                         frame_name,
                         base::UTF8ToUTF16(extension_id));
}

void BackgroundContentsService::LoadBackgroundContentsFromManifests(
    Profile* profile) {
  const extensions::ExtensionSet* extensions =
      extensions::ExtensionSystem::Get(profile)->
          extension_service()->extensions();
  for (extensions::ExtensionSet::const_iterator iter = extensions->begin();
       iter != extensions->end(); ++iter) {
    const Extension* extension = iter->get();
    if (extension->is_hosted_app() &&
        BackgroundInfo::HasBackgroundPage(extension)) {
      LoadBackgroundContents(profile,
                             BackgroundInfo::GetBackgroundURL(extension),
                             base::ASCIIToUTF16("background"),
                             base::UTF8ToUTF16(extension->id()));
    }
  }
}

void BackgroundContentsService::LoadBackgroundContents(
    Profile* profile,
    const GURL& url,
    const base::string16& frame_name,
    const base::string16& application_id) {
  // We are depending on the fact that we will initialize before any user
  // actions or session restore can take place, so no BackgroundContents should
  // be running yet for the passed application_id.
  DCHECK(!GetAppBackgroundContents(application_id));
  DCHECK(!application_id.empty());
  DCHECK(url.is_valid());
  DVLOG(1) << "Loading background content url: " << url;

  BackgroundContents* contents = CreateBackgroundContents(
      SiteInstance::CreateForURL(profile, url),
      MSG_ROUTING_NONE,
      profile,
      frame_name,
      application_id,
      std::string(),
      NULL);

  // TODO(atwilson): Create RenderViews asynchronously to avoid increasing
  // startup latency (http://crbug.com/47236).
  contents->web_contents()->GetController().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_LINK, std::string());
}

BackgroundContents* BackgroundContentsService::CreateBackgroundContents(
    SiteInstance* site,
    int routing_id,
    Profile* profile,
    const base::string16& frame_name,
    const base::string16& application_id,
    const std::string& partition_id,
    content::SessionStorageNamespace* session_storage_namespace) {
  BackgroundContents* contents = new BackgroundContents(
      site, routing_id, this, partition_id, session_storage_namespace);

  // Register the BackgroundContents internally, then send out a notification
  // to external listeners.
  BackgroundContentsOpenedDetails details = {contents,
                                             frame_name,
                                             application_id};
  BackgroundContentsOpened(&details);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_CONTENTS_OPENED,
      content::Source<Profile>(profile),
      content::Details<BackgroundContentsOpenedDetails>(&details));

  // A new background contents has been created - notify our listeners.
  SendChangeNotification(profile);
  return contents;
}

void BackgroundContentsService::RegisterBackgroundContents(
    BackgroundContents* background_contents) {
  DCHECK(IsTracked(background_contents));
  if (!prefs_)
    return;

  // We store the first URL we receive for a given application. If there's
  // already an entry for this application, no need to do anything.
  // TODO(atwilson): Verify that this is the desired behavior based on developer
  // feedback (http://crbug.com/47118).
  DictionaryPrefUpdate update(prefs_, prefs::kRegisteredBackgroundContents);
  base::DictionaryValue* pref = update.Get();
  const base::string16& appid = GetParentApplicationId(background_contents);
  base::DictionaryValue* current;
  if (pref->GetDictionaryWithoutPathExpansion(base::UTF16ToUTF8(appid),
                                              &current)) {
    return;
  }

  // No entry for this application yet, so add one.
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString(kUrlKey, background_contents->GetURL().spec());
  dict->SetString(kFrameNameKey, contents_map_[appid].frame_name);
  pref->SetWithoutPathExpansion(base::UTF16ToUTF8(appid), dict);
}

bool BackgroundContentsService::HasRegisteredBackgroundContents(
    const base::string16& app_id) {
  if (!prefs_)
    return false;
  std::string app = base::UTF16ToUTF8(app_id);
  const base::DictionaryValue* contents =
      prefs_->GetDictionary(prefs::kRegisteredBackgroundContents);
  return contents->HasKey(app);
}

void BackgroundContentsService::UnregisterBackgroundContents(
    BackgroundContents* background_contents) {
  if (!prefs_)
    return;
  DCHECK(IsTracked(background_contents));
  const base::string16 appid = GetParentApplicationId(background_contents);
  DictionaryPrefUpdate update(prefs_, prefs::kRegisteredBackgroundContents);
  update.Get()->RemoveWithoutPathExpansion(base::UTF16ToUTF8(appid), NULL);
}

void BackgroundContentsService::ShutdownAssociatedBackgroundContents(
    const base::string16& appid) {
  BackgroundContents* contents = GetAppBackgroundContents(appid);
  if (contents) {
    UnregisterBackgroundContents(contents);
    // Background contents destructor shuts down the renderer.
    delete contents;
  }
}

void BackgroundContentsService::BackgroundContentsOpened(
    BackgroundContentsOpenedDetails* details) {
  // Add the passed object to our list. Should not already be tracked.
  DCHECK(!IsTracked(details->contents));
  DCHECK(!details->application_id.empty());
  contents_map_[details->application_id].contents = details->contents;
  contents_map_[details->application_id].frame_name = details->frame_name;

  ScheduleCloseBalloon(base::UTF16ToASCII(details->application_id));
}

// Used by test code and debug checks to verify whether a given
// BackgroundContents is being tracked by this instance.
bool BackgroundContentsService::IsTracked(
    BackgroundContents* background_contents) const {
  return !GetParentApplicationId(background_contents).empty();
}

void BackgroundContentsService::BackgroundContentsShutdown(
    BackgroundContents* background_contents) {
  // Remove the passed object from our list.
  DCHECK(IsTracked(background_contents));
  base::string16 appid = GetParentApplicationId(background_contents);
  contents_map_.erase(appid);
}

BackgroundContents* BackgroundContentsService::GetAppBackgroundContents(
    const base::string16& application_id) {
  BackgroundContentsMap::const_iterator it = contents_map_.find(application_id);
  return (it != contents_map_.end()) ? it->second.contents : NULL;
}

const base::string16& BackgroundContentsService::GetParentApplicationId(
    BackgroundContents* contents) const {
  for (BackgroundContentsMap::const_iterator it = contents_map_.begin();
       it != contents_map_.end(); ++it) {
    if (contents == it->second.contents)
      return it->first;
  }
  return base::EmptyString16();
}

void BackgroundContentsService::AddWebContents(
    WebContents* new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture,
    bool* was_blocked) {
  Browser* browser = chrome::FindLastActiveWithProfile(
      Profile::FromBrowserContext(new_contents->GetBrowserContext()),
      chrome::GetActiveDesktop());
  if (browser) {
    chrome::AddWebContents(browser, NULL, new_contents, disposition,
                           initial_pos, user_gesture, was_blocked);
  }
}
