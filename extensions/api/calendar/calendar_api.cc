// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/calendar/calendar_api.h"

#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "browser/vivaldi_internal_handlers.h"
#include "calendar/calendar_model_observer.h"
#include "calendar/calendar_service.h"
#include "calendar/calendar_service_factory.h"
#include "calendar/calendar_util.h"
#include "calendar/event_exception_type.h"
#include "calendar/invite_type.h"
#include "calendar/notification_type.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/calendar.h"
#include "extensions/tools/vivaldi_tools.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

using calendar::CalendarService;
using calendar::CalendarServiceFactory;
using vivaldi::GetTime;
using vivaldi::MilliSecondsFromTime;

namespace extensions {

using calendar::AccountRow;
using calendar::CalendarRow;
using calendar::EventExceptions;
using calendar::EventExceptionType;
using calendar::EventTypeRow;
using calendar::GetIdAsInt64;
using calendar::GetStdStringAsInt64;
using calendar::InviteRow;
using calendar::InviteRows;
using calendar::InviteToCreate;
using calendar::NotificationRow;
using calendar::NotificationRows;
using calendar::NotificationToCreate;
using calendar::RecurrenceExceptionRow;
using calendar::RecurrenceExceptionRows;
using vivaldi::calendar::Account;
using vivaldi::calendar::AccountType;
using vivaldi::calendar::Calendar;
using vivaldi::calendar::CreateEventsResults;
using vivaldi::calendar::CreateInviteRow;
using vivaldi::calendar::CreateNotificationRow;
using vivaldi::calendar::EventException;
using vivaldi::calendar::EventType;
using vivaldi::calendar::Invite;
using vivaldi::calendar::Notification;
using vivaldi::calendar::RecurrenceException;
using vivaldi::calendar::SupportedCalendarComponents;

namespace OnIcsFileOpened = vivaldi::calendar::OnIcsFileOpened;
namespace OnWebcalUrlOpened = vivaldi::calendar::OnWebcalUrlOpened;
namespace OnMailtoOpened = vivaldi::calendar::OnMailtoOpened;

namespace OnEventCreated = vivaldi::calendar::OnEventCreated;
namespace OnNotificationChanged = vivaldi::calendar::OnNotificationChanged;
namespace OnCalendarDataChanged = vivaldi::calendar::OnCalendarDataChanged;

typedef std::vector<vivaldi::calendar::CalendarEvent> EventList;
typedef std::vector<vivaldi::calendar::Account> AccountList;
typedef std::vector<vivaldi::calendar::Notification> NotificaionList;
typedef std::vector<vivaldi::calendar::Calendar> CalendarList;
typedef std::vector<vivaldi::calendar::EventType> EventTypeList;
typedef std::vector<vivaldi::calendar::RecurrenceException>
    RecurrenceExceptions;

// static
RecurrenceException CreateException(const RecurrenceExceptionRow& row) {
  RecurrenceException exception;
  exception.exception_id.reset(new std::string(base::NumberToString(row.id)));
  exception.cancelled.reset(new bool(row.cancelled));
  exception.date.reset(new double(MilliSecondsFromTime(row.exception_day)));
  exception.exception_event_id.reset(
      new std::string(base::NumberToString(row.exception_event_id)));
  exception.parent_event_id.reset(
      new std::string(base::NumberToString(row.parent_event_id)));

  return exception;
}

std::unique_ptr<std::vector<RecurrenceException>> CreateRecurrenceException(
    const RecurrenceExceptionRows& exceptions) {
  auto new_exceptions = std::make_unique<std::vector<RecurrenceException>>();
  for (const auto& exception : exceptions) {
    new_exceptions->push_back(CreateException(exception));
  }

  return new_exceptions;
}

int MapAccountType(const AccountType& account_type) {
  int type = 0;
  switch (account_type) {
    case vivaldi::calendar::ACCOUNT_TYPE_LOCAL:
      type = 0;
      break;
    case vivaldi::calendar::ACCOUNT_TYPE_VIVALDI:
      type = 1;
      break;
    case vivaldi::calendar::ACCOUNT_TYPE_GOOGLE:
      type = 2;
      break;
    case vivaldi::calendar::ACCOUNT_TYPE_CALDAV:
      type = 3;
      break;
    case vivaldi::calendar::ACCOUNT_TYPE_ICAL:
      type = 4;
      break;
    case vivaldi::calendar::ACCOUNT_TYPE_NONE:
      type = 0;
      break;
  }
  return type;
}

AccountType MapAccountTypeFromDb(int type) {
  AccountType account_type = vivaldi::calendar::ACCOUNT_TYPE_LOCAL;
  switch (type) {
    case calendar::ACCOUNT_TYPE_LOCAL:
      account_type = vivaldi::calendar::ACCOUNT_TYPE_LOCAL;
      break;
    case calendar::ACCOUNT_TYPE_VIVALDINET:
      account_type = vivaldi::calendar::ACCOUNT_TYPE_VIVALDI;
      break;
    case calendar::ACCOUNT_TYPE_GOOGLE:
      account_type = vivaldi::calendar::ACCOUNT_TYPE_GOOGLE;
      break;
    case calendar::ACCOUNT_TYPE_CALDAV:
      account_type = vivaldi::calendar::ACCOUNT_TYPE_CALDAV;
      break;
    case calendar::ACCOUNT_TYPE_ICAL:
      account_type = vivaldi::calendar::ACCOUNT_TYPE_ICAL;
      break;
  }
  return account_type;
}

Notification CreateNotification(const NotificationRow& row) {
  Notification notification;
  notification.id = base::NumberToString(row.id);
  notification.event_id.reset(
      new std::string(base::NumberToString(row.event_id)));
  notification.name = base::UTF16ToUTF8(row.name);
  notification.description.reset(
      new std::string(base::UTF16ToUTF8(row.description)));
  notification.when = MilliSecondsFromTime(row.when);
  notification.delay.reset(new int(row.delay));
  notification.period.reset(new double(MilliSecondsFromTime(row.period)));

  return notification;
}

std::unique_ptr<std::vector<Notification>> CreateNotifications(
    const NotificationRows& notifications) {
  auto new_exceptions = std::make_unique<std::vector<Notification>>();
  for (const auto& notification : notifications) {
    new_exceptions->push_back(CreateNotification(notification));
  }

  return new_exceptions;
}

Invite CreateInviteItem(const InviteRow& row) {
  Invite invite;
  invite.id = base::NumberToString(row.id);
  invite.event_id = base::NumberToString(row.event_id);
  invite.name.reset(new std::string(base::UTF16ToUTF8(row.name)));
  invite.address = base::UTF16ToUTF8(row.address);
  invite.partstat.reset(new std::string(row.partstat));
  invite.sent.reset(new bool(row.sent));

  return invite;
}

std::unique_ptr<std::vector<Invite>> CreateInvites(const InviteRows& invites) {
  auto new_invites = std::make_unique<std::vector<Invite>>();
  for (const auto& invite : invites) {
    new_invites->push_back(CreateInviteItem(invite));
  }

  return new_invites;
}

std::unique_ptr<SupportedCalendarComponents> GetSupportedComponents(
    int supported_component_set) {
  bool vevent = (supported_component_set & calendar::CALENDAR_VEVENT);
  bool vtodo = (supported_component_set & calendar::CALENDAR_VTODO);
  bool vjournal = (supported_component_set & calendar::CALENDAR_VJOURNAL);
  auto supported_components_set =
      std::make_unique<SupportedCalendarComponents>();
  supported_components_set->vevent = vevent;
  supported_components_set->vtodo = vtodo;
  supported_components_set->vjournal = vjournal;

  return supported_components_set;
}

Calendar GetCalendarItem(const CalendarRow& row) {
  Calendar calendar;
  calendar.id = base::NumberToString(row.id());
  calendar.account_id = base::NumberToString(row.account_id());
  calendar.name = base::UTF16ToUTF8(row.name());
  calendar.description.reset(
      new std::string(base::UTF16ToUTF8(row.description())));
  calendar.ctag.reset(new std::string(row.ctag()));
  calendar.orderindex.reset(new int(row.orderindex()));
  calendar.active.reset(new bool(row.active()));
  calendar.iconindex.reset(new int(row.iconindex()));
  calendar.color.reset(new std::string(row.color()));
  calendar.last_checked.reset(
      new double(MilliSecondsFromTime(row.last_checked())));
  calendar.timezone.reset(new std::string(row.timezone()));
  calendar.supported_calendar_component =
      GetSupportedComponents(row.supported_component_set());

  return calendar;
}

EventType GetEventType(const EventTypeRow& row) {
  EventType event_type;
  event_type.id = base::NumberToString(row.id());
  event_type.name = base::UTF16ToUTF8(row.name());
  event_type.color.reset(new std::string(row.color()));
  event_type.iconindex.reset(new int(row.iconindex()));

  return event_type;
}

Account GetAccountType(const AccountRow& row) {
  Account account;
  account.id = base::NumberToString(row.id);
  account.name = base::UTF16ToUTF8(row.name);
  account.username = base::UTF16ToUTF8(row.username);
  account.account_type = MapAccountTypeFromDb(row.account_type);
  account.url = row.url.spec();
  account.interval.reset(new int(row.interval));
  return account;
}

CalendarEventRouter::CalendarEventRouter(Profile* profile,
                                         CalendarService* calendar_service)
    : profile_(profile) {
  DCHECK(profile);
  calendar_service_observation_.Observe(calendar_service);
}

CalendarEventRouter::~CalendarEventRouter() {}

void CalendarEventRouter::ExtensiveCalendarChangesBeginning(
    CalendarService* model) {}

void CalendarEventRouter::ExtensiveCalendarChangesEnded(
    CalendarService* model) {}
std::unique_ptr<CalendarEvent> CreateVivaldiEvent(
    const calendar::EventResult& event) {
  std::unique_ptr<CalendarEvent> cal_event(new CalendarEvent());

  cal_event->id = base::NumberToString(event.id);
  cal_event->calendar_id = base::NumberToString(event.calendar_id);
  cal_event->alarm_id.reset(
      new std::string(base::NumberToString(event.alarm_id)));

  cal_event->title = base::UTF16ToUTF8(event.title);
  cal_event->description.reset(
      new std::string(base::UTF16ToUTF8(event.description)));
  cal_event->start.reset(new double(MilliSecondsFromTime(event.start)));
  cal_event->end.reset(new double(MilliSecondsFromTime(event.end)));
  cal_event->all_day.reset(new bool(event.all_day));
  cal_event->is_recurring.reset(new bool(event.is_recurring));
  cal_event->location.reset(new std::string(base::UTF16ToUTF8(event.location)));
  cal_event->url.reset(new std::string(base::UTF16ToUTF8(event.url)));
  cal_event->etag.reset(new std::string(event.etag));
  cal_event->href.reset(new std::string(event.href));
  cal_event->uid.reset(new std::string(event.uid));
  cal_event->event_type_id.reset(
      new std::string(base::NumberToString(event.event_type_id)));
  cal_event->task.reset(new bool(event.task));
  cal_event->complete.reset(new bool(event.complete));
  cal_event->trash.reset(new bool(event.trash));
  cal_event->trash_time.reset(
      new double(MilliSecondsFromTime(event.trash_time)));
  cal_event->sequence.reset(new int(event.sequence));
  cal_event->ical.reset(new std::string(base::UTF16ToUTF8(event.ical)));
  cal_event->rrule.reset(new std::string(event.rrule));
  cal_event->recurrence_exceptions =
      CreateRecurrenceException(event.recurrence_exceptions);

  cal_event->notifications = CreateNotifications(event.notifications);
  cal_event->invites = CreateInvites(event.invites);
  cal_event->organizer.reset(new std::string(event.organizer));
  cal_event->timezone.reset(new std::string(event.timezone));
  cal_event->priority.reset(new int(event.priority));
  cal_event->status.reset(new std::string(event.status));
  cal_event->percentage_complete.reset(new int(event.percentage_complete));
  cal_event->categories.reset(
      new std::string(base::UTF16ToUTF8(event.categories)));
  cal_event->component_class.reset(
      new std::string(base::UTF16ToUTF8(event.component_class)));
  cal_event->attachment.reset(
      new std::string(base::UTF16ToUTF8(event.attachment)));
  cal_event->completed.reset(new double(MilliSecondsFromTime(event.completed)));
  cal_event->sync_pending.reset(new bool(event.sync_pending));
  cal_event->delete_pending.reset(new bool(event.delete_pending));
  return cal_event;
}
void CalendarEventRouter::OnEventCreated(CalendarService* service,
                                         const calendar::EventResult& event) {
  std::unique_ptr<CalendarEvent> createdEvent = CreateVivaldiEvent(event);

  base::Value::List args = OnEventCreated::Create(*createdEvent);
  DispatchEvent(profile_, OnEventCreated::kEventName, std::move(args));
}

void CalendarEventRouter::OnIcsFileOpened(std::string path) {
  DispatchEvent(profile_, OnIcsFileOpened::kEventName,
                OnIcsFileOpened::Create(path));
}

void CalendarEventRouter::OnWebcalUrlOpened(GURL url) {
  DispatchEvent(profile_, OnWebcalUrlOpened::kEventName,
                OnWebcalUrlOpened::Create(url.spec()));
}

void CalendarEventRouter::OnMailtoOpened(GURL mailto) {
  DispatchEvent(profile_, OnMailtoOpened::kEventName,
                OnMailtoOpened::Create(mailto.spec()));
}

void CalendarEventRouter::OnNotificationChanged(
    CalendarService* service,
    const calendar::NotificationRow& row) {
  Notification changedNotification = CreateNotification(row);
  base::Value::List args =
      OnNotificationChanged::Create(changedNotification);
  DispatchEvent(profile_, OnNotificationChanged::kEventName, std::move(args));
}

void CalendarEventRouter::OnCalendarModified(CalendarService* service) {
  base::Value::List args;
  DispatchEvent(profile_, OnCalendarDataChanged::kEventName, std::move(args));
}

// Helper to actually dispatch an event to extension listeners.
void CalendarEventRouter::DispatchEvent(Profile* profile,
                                        const std::string& event_name,
                                        base::Value::List event_args) {
  if (profile && EventRouter::Get(profile)) {
    EventRouter* event_router = EventRouter::Get(profile);
    if (event_router) {
      event_router->BroadcastEvent(base::WrapUnique(
          new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                                event_name, std::move(event_args))));
    }
  }
}

void BroadcastCalendarEvent(const std::string& eventname,
                            base::Value::List args,
                            content::BrowserContext* context) {
  std::unique_ptr<extensions::Event> event(
      new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                            eventname, std::move(args), context));
  EventRouter* event_router = EventRouter::Get(context);
  if (event_router) {
    event_router->BroadcastEvent(std::move(event));
  }
}

CalendarAPI::CalendarAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, OnEventCreated::kEventName);
  event_router->RegisterObserver(this, OnCalendarDataChanged::kEventName);
}

CalendarAPI::~CalendarAPI() {}

void CalendarAPI::Shutdown() {
  calendar_event_router_.reset();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<CalendarAPI>>::
    DestructorAtExit g_factory_calendar = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<CalendarAPI>* CalendarAPI::GetFactoryInstance() {
  RegisterInternalHandlers();
  return g_factory_calendar.Pointer();
}

/*static*/
void CalendarAPI::RegisterInternalHandlers() {
  constexpr base::FilePath::CharType kIcsExtension[] =
      FILE_PATH_LITERAL(".ics");
  constexpr char kWebcalProtocol[] = "webcal";
  constexpr char kWMailtoProtocol[] = "mailto";

  static bool internal_handlers_registered = false;
  if (internal_handlers_registered)
    return;
  internal_handlers_registered = true;
  ::vivaldi::RegisterDownloadHandler(
      kIcsExtension, base::BindRepeating([](Profile* profile,
                                            download::DownloadItem* download) {
        auto* calendar_api =
            BrowserContextKeyedAPIFactory<CalendarAPI>::GetIfExists(profile);
        if (!calendar_api || !calendar_api->calendar_event_router_)
          return false;
        if (!profile->GetPrefs()->GetBoolean(
                vivaldiprefs::kCalendarHandleIcsDownloads))
          return false;
        calendar_api->calendar_event_router_->OnIcsFileOpened(
            download->GetTargetFilePath().AsUTF8Unsafe());
        return true;
      }));
  ::vivaldi::RegisterProtocolHandler(
      kWebcalProtocol, base::BindRepeating([](Profile* profile, GURL url) {
        auto* calendar_api =
            BrowserContextKeyedAPIFactory<CalendarAPI>::GetIfExists(profile);
        if (!calendar_api || !calendar_api->calendar_event_router_)
          return false;
        if (!profile->GetPrefs()->GetBoolean(
                vivaldiprefs::kCalendarHandleWebcalLinks))
          return false;
        calendar_api->calendar_event_router_->OnWebcalUrlOpened(url);
        return true;
      }));

  ::vivaldi::RegisterProtocolHandler(
      kWMailtoProtocol, base::BindRepeating([](Profile* profile, GURL mailto) {
        auto* calendar_api =
            BrowserContextKeyedAPIFactory<CalendarAPI>::GetIfExists(profile);
        if (!calendar_api || !calendar_api->calendar_event_router_)
          return false;
        if (!profile->GetPrefs()->GetBoolean(
                vivaldiprefs::kMailMailtoInVivaldi))
          return false;
        calendar_api->calendar_event_router_->OnMailtoOpened(mailto);
        return true;
      }));
}

void CalendarAPI::OnListenerAdded(const EventListenerInfo& details) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);

  calendar_event_router_ = std::make_unique<CalendarEventRouter>(
      profile, CalendarServiceFactory::GetForProfile(profile));

  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

CalendarService* CalendarAsyncFunction::GetCalendarService() {
  return CalendarServiceFactory::GetForProfile(GetProfile());
}

ExtensionFunction::ResponseAction CalendarGetAllEventsFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllEvents(
      base::BindOnce(&CalendarGetAllEventsFunction::GetAllEventsComplete, this),
      &task_tracker_);

  return RespondLater();  // GetAllEventsComplete() will be called
                          // asynchronously.
}

void CalendarGetAllEventsFunction::GetAllEventsComplete(
    std::shared_ptr<calendar::EventQueryResults> results) {
  EventList eventList;

  if (results && !results->empty()) {
    for (calendar::EventQueryResults::EventResultVector::const_iterator
             iterator = results->begin();
         iterator != results->end(); ++iterator) {
      eventList.push_back(std::move(
          *base::WrapUnique((CreateVivaldiEvent(**iterator).release()))));
    }
  }

  Respond(ArgumentList(
      vivaldi::calendar::GetAllEvents::Results::Create(eventList)));
}

Profile* CalendarAsyncFunction::GetProfile() const {
  return Profile::FromBrowserContext(browser_context());
}

ExtensionFunction::ResponseAction CalendarEventCreateFunction::Run() {
  std::unique_ptr<vivaldi::calendar::EventCreate::Params> params(
      vivaldi::calendar::EventCreate::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  calendar::EventRow createEvent = calendar::GetEventRow(params->event);

  model->CreateCalendarEvent(
      createEvent,
      base::BindOnce(&CalendarEventCreateFunction::CreateEventComplete, this),
      &task_tracker_);
  return RespondLater();
}

void CalendarEventCreateFunction::CreateEventComplete(
    std::shared_ptr<calendar::EventResultCB> results) {
  if (!results->success) {
    Respond(Error("Error creating event. " + results->message));
  } else {
    std::unique_ptr<CalendarEvent> event = CreateVivaldiEvent(results->event);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::EventCreate::Results::Create(*event)));
  }
}

ExtensionFunction::ResponseAction CalendarEventsCreateFunction::Run() {
  std::unique_ptr<vivaldi::calendar::EventsCreate::Params> params(
      vivaldi::calendar::EventsCreate::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  std::vector<vivaldi::calendar::CreateDetails>& events = params->events_list;
  size_t count = events.size();
  EXTENSION_FUNCTION_VALIDATE(count > 0);

  std::vector<calendar::EventRow> event_rows;

  for (size_t i = 0; i < count; ++i) {
    vivaldi::calendar::CreateDetails& create_details = events[i];
    calendar::EventRow createEvent = calendar::GetEventRow(create_details);
    event_rows.push_back(createEvent);
  }

  model->CreateCalendarEvents(
      event_rows,
      base::BindOnce(&CalendarEventsCreateFunction::CreateEventsComplete, this),
      &task_tracker_);

  return RespondLater();
}

CreateEventsResults GetCreateEventsItem(
    const calendar::CreateEventsResult& res) {
  CreateEventsResults event_item;
  event_item.created_count = res.number_success;
  event_item.failed_count = res.number_failed;
  return event_item;
}

void CalendarEventsCreateFunction::CreateEventsComplete(
    std::shared_ptr<calendar::CreateEventsResult> results) {
  CreateEventsResults return_results = GetCreateEventsItem(*results);
  Respond(
      ArgumentList(extensions::vivaldi::calendar::EventsCreate::Results::Create(
          return_results)));
}

ExtensionFunction::ResponseAction CalendarUpdateEventFunction::Run() {
  std::unique_ptr<vivaldi::calendar::UpdateEvent::Params> params(
      vivaldi::calendar::UpdateEvent::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  calendar::EventRow updatedEvent;

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventID eventId;

  if (!GetIdAsInt64(id, &eventId)) {
    return RespondNow(Error("Error. Invalid event id"));
  }

  if (params->changes.calendar_id.get()) {
    calendar::CalendarID calendarId;
    if (!GetStdStringAsInt64(*params->changes.calendar_id, &calendarId)) {
      return RespondNow(Error("Error. Invalid calendar_id"));
    }
    updatedEvent.calendar_id = calendarId;
    updatedEvent.updateFields |= calendar::CALENDAR_ID;
  }

  if (params->changes.alarm_id.get()) {
    calendar::AlarmID alarmId;
    if (!GetStdStringAsInt64(*params->changes.calendar_id, &alarmId)) {
      return RespondNow(Error("Error. Invalid alarm"));
    }

    updatedEvent.alarm_id = alarmId;
    updatedEvent.updateFields |= calendar::ALARM_ID;
  }

  if (params->changes.description.get()) {
    updatedEvent.description = base::UTF8ToUTF16(*params->changes.description);
    updatedEvent.updateFields |= calendar::DESCRIPTION;
  }

  if (params->changes.title.get()) {
    updatedEvent.title = base::UTF8ToUTF16(*params->changes.title);
    updatedEvent.updateFields |= calendar::TITLE;
  }

  if (params->changes.start.get()) {
    double start = *params->changes.start;
    updatedEvent.start = GetTime(start);
    updatedEvent.updateFields |= calendar::START;
  }

  if (params->changes.end.get()) {
    double end = *params->changes.end;
    updatedEvent.end = GetTime(end);
    updatedEvent.updateFields |= calendar::END;
  }

  if (params->changes.all_day.get()) {
    updatedEvent.all_day = *params->changes.all_day;
    updatedEvent.updateFields |= calendar::ALLDAY;
  }

  if (params->changes.is_recurring.get()) {
    updatedEvent.is_recurring = *params->changes.is_recurring;
    updatedEvent.updateFields |= calendar::ISRECURRING;
  }

  if (params->changes.location.get()) {
    updatedEvent.location = base::UTF8ToUTF16(*params->changes.location);
    updatedEvent.updateFields |= calendar::LOCATION;
  }

  if (params->changes.url.get()) {
    updatedEvent.url = base::UTF8ToUTF16(*params->changes.url);
    updatedEvent.updateFields |= calendar::URL;
  }

  if (params->changes.etag.get()) {
    updatedEvent.etag = *params->changes.etag;
    updatedEvent.updateFields |= calendar::ETAG;
  }

  if (params->changes.href.get()) {
    updatedEvent.href = *params->changes.href;
    updatedEvent.updateFields |= calendar::HREF;
  }

  if (params->changes.uid.get()) {
    updatedEvent.uid = *params->changes.uid;
    updatedEvent.updateFields |= calendar::UID;
  }

  if (params->changes.task.get()) {
    updatedEvent.task = *params->changes.task;
    updatedEvent.updateFields |= calendar::TASK;
  }

  if (params->changes.complete.get()) {
    updatedEvent.complete = *params->changes.complete;
    updatedEvent.updateFields |= calendar::COMPLETE;
  }

  if (params->changes.trash.get()) {
    updatedEvent.trash = *params->changes.trash;
    updatedEvent.updateFields |= calendar::TRASH;
  }

  if (params->changes.sequence.get()) {
    updatedEvent.sequence = *params->changes.sequence;
    updatedEvent.updateFields |= calendar::SEQUENCE;
  }

  if (params->changes.ical.get()) {
    updatedEvent.ical = base::UTF8ToUTF16(*params->changes.ical);
    updatedEvent.updateFields |= calendar::ICAL;
  }

  if (params->changes.rrule.get()) {
    updatedEvent.rrule = *params->changes.rrule;
    updatedEvent.updateFields |= calendar::RRULE;
  }

  if (params->changes.organizer.get()) {
    updatedEvent.organizer = *params->changes.organizer;
    updatedEvent.updateFields |= calendar::ORGANIZER;
  }

  if (params->changes.timezone.get()) {
    updatedEvent.timezone = *params->changes.timezone;
    updatedEvent.updateFields |= calendar::TIMEZONE;
  }

  if (params->changes.event_type_id.get()) {
    calendar::EventTypeID event_type_id;
    if (!GetStdStringAsInt64(*params->changes.event_type_id, &event_type_id)) {
      return RespondNow(Error("Error. Invalid event_type_id"));
    }

    updatedEvent.event_type_id = event_type_id;
    updatedEvent.updateFields |= calendar::EVENT_TYPE_ID;
  }

  if (params->changes.priority.get()) {
    updatedEvent.priority = *params->changes.priority;
    updatedEvent.updateFields |= calendar::PRIORITY;
  }

  if (params->changes.status.get()) {
    updatedEvent.status = *params->changes.status;
    updatedEvent.updateFields |= calendar::STATUS;
  }

  if (params->changes.percentage_complete.get()) {
    updatedEvent.percentage_complete = *params->changes.percentage_complete;
    updatedEvent.updateFields |= calendar::PERCENTAGE_COMPLETE;
  }

  if (params->changes.categories.get()) {
    updatedEvent.categories = base::UTF8ToUTF16(*params->changes.categories);
    updatedEvent.updateFields |= calendar::CATEGORIES;
  }

  if (params->changes.component_class.get()) {
    updatedEvent.component_class =
        base::UTF8ToUTF16(*params->changes.component_class);
    updatedEvent.updateFields |= calendar::COMPONENT_CLASS;
  }

  if (params->changes.attachment.get()) {
    updatedEvent.attachment = base::UTF8ToUTF16(*params->changes.attachment);
    updatedEvent.updateFields |= calendar::ATTACHMENT;
  }

  if (params->changes.completed.get()) {
    updatedEvent.completed = GetTime(*params->changes.completed);
    updatedEvent.updateFields |= calendar::COMPLETED;
  }

  if (params->changes.sync_pending.get()) {
    updatedEvent.sync_pending = *params->changes.sync_pending;
    updatedEvent.updateFields |= calendar::SYNC_PENDING;
  }

  if (params->changes.delete_pending.get()) {
    updatedEvent.delete_pending = *params->changes.delete_pending;
    updatedEvent.updateFields |= calendar::DELETE_PENDING;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->UpdateCalendarEvent(
      eventId, updatedEvent,
      base::BindOnce(&CalendarUpdateEventFunction::UpdateEventComplete, this),
      &task_tracker_);
  return RespondLater();  // UpdateEventComplete() will be called
                          // asynchronously.
}

void CalendarUpdateEventFunction::UpdateEventComplete(
    std::shared_ptr<calendar::EventResultCB> results) {
  if (!results->success) {
    Respond(Error("Error updating event"));
  } else {
    std::unique_ptr<CalendarEvent> event = CreateVivaldiEvent(results->event);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::UpdateEvent::Results::Create(*event)));
  }
}

ExtensionFunction::ResponseAction CalendarDeleteEventFunction::Run() {
  std::unique_ptr<vivaldi::calendar::DeleteEvent::Params> params(
      vivaldi::calendar::DeleteEvent::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventID eventId;

  if (!GetIdAsInt64(id, &eventId)) {
    return RespondNow(Error("Error. Invalid event id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteCalendarEvent(
      eventId,
      base::BindOnce(&CalendarDeleteEventFunction::DeleteEventComplete, this),
      &task_tracker_);
  return RespondLater();  // DeleteEventComplete() will be called
                          // asynchronously.
}

void CalendarDeleteEventFunction::DeleteEventComplete(
    std::shared_ptr<calendar::DeleteEventResult> results) {
  if (!results->success) {
    Respond(Error("Error deleting event"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarDeleteEventExceptionFunction::Run() {
  std::unique_ptr<vivaldi::calendar::DeleteEventException::Params> params(
      vivaldi::calendar::DeleteEventException::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::u16string id;
  id = base::UTF8ToUTF16(params->exception_id);
  calendar::RecurrenceExceptionID exception_id;

  if (!GetIdAsInt64(id, &exception_id)) {
    return RespondNow(Error("Error. Invalid exception id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteEventRecurrenceException(
      exception_id,
      base::BindOnce(
          &CalendarDeleteEventExceptionFunction::DeleteEventExceptionComplete,
          this),
      &task_tracker_);
  return RespondLater();  // DeleteEventExceptionComplete() will be called
                          // asynchronously.
}

void CalendarDeleteEventExceptionFunction::DeleteEventExceptionComplete(
    std::shared_ptr<calendar::EventResultCB> results) {
  if (!results->success) {
    Respond(Error("Error deleting event exception"));
  } else {
    std::unique_ptr<CalendarEvent> event = CreateVivaldiEvent(results->event);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::DeleteEventException::Results::Create(
            *event)));
  }
}

std::unique_ptr<vivaldi::calendar::Calendar> CreateVivaldiCalendar(
    const calendar::CalendarResult& result) {
  std::unique_ptr<vivaldi::calendar::Calendar> calendar(
      new vivaldi::calendar::Calendar());

  calendar->id = base::NumberToString(result.id());
  calendar->account_id = base::NumberToString(result.account_id());
  calendar->name = base::UTF16ToUTF8(result.name());

  calendar->description.reset(
      new std::string(base::UTF16ToUTF8(result.description())));
  calendar->orderindex.reset(new int(result.orderindex()));
  calendar->color.reset(new std::string(result.color()));
  calendar->hidden.reset(new bool(result.hidden()));
  calendar->ctag.reset(new std::string(result.ctag()));
  calendar->active.reset(new bool(result.active()));
  calendar->iconindex.reset(new int(result.iconindex()));
  calendar->last_checked.reset(
      new double(MilliSecondsFromTime(result.last_checked())));
  calendar->timezone.reset(new std::string(result.timezone()));
  calendar->supported_calendar_component =
      GetSupportedComponents(result.supported_component_set());

  return calendar;
}

ExtensionFunction::ResponseAction CalendarCreateFunction::Run() {
  std::unique_ptr<vivaldi::calendar::Create::Params> params(
      vivaldi::calendar::Create::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  calendar::CalendarRow createCalendar;

  std::u16string name;
  name = base::UTF8ToUTF16(params->calendar.name);
  createCalendar.set_name(name);

  std::u16string account_id = base::UTF8ToUTF16(params->calendar.account_id);
  calendar::AccountID accountId;

  if (!GetIdAsInt64(account_id, &accountId)) {
    return RespondNow(Error("Error. Invalid account id"));
  }

  createCalendar.set_account_id(accountId);

  if (params->calendar.description.get()) {
    std::u16string description =
        base::UTF8ToUTF16(*params->calendar.description);
    createCalendar.set_description(description);
  }

  if (params->calendar.orderindex.get()) {
    int orderindex = *params->calendar.orderindex.get();
    createCalendar.set_orderindex(orderindex);
  }

  if (params->calendar.color.get()) {
    std::string color = *params->calendar.color.get();
    createCalendar.set_color(color);
  }

  if (params->calendar.hidden.get()) {
    bool hidden = *params->calendar.hidden.get();
    createCalendar.set_hidden(hidden);
  }

  if (params->calendar.active.get()) {
    bool active = *params->calendar.active.get();
    createCalendar.set_active(active);
  }

  if (params->calendar.last_checked.get()) {
    int last_checked = *params->calendar.last_checked.get();
    createCalendar.set_last_checked(GetTime(last_checked));
  }

  if (params->calendar.timezone.get()) {
    std::string timezone = *params->calendar.timezone.get();
    createCalendar.set_timezone(timezone);
  }

  if (params->calendar.ctag.get()) {
    std::string timezone = *params->calendar.ctag.get();
    createCalendar.set_ctag(timezone);
  }

  int supported_components = calendar::NONE;
  if (params->calendar.supported_calendar_component.get()) {
    if (params->calendar.supported_calendar_component->vevent)
      supported_components |= calendar::CALENDAR_VEVENT;

    if (params->calendar.supported_calendar_component->vtodo)
      supported_components |= calendar::CALENDAR_VTODO;

    if (params->calendar.supported_calendar_component->vjournal)
      supported_components |= calendar::CALENDAR_VJOURNAL;

    createCalendar.set_supported_component_set(supported_components);
  } else {
    supported_components |= calendar::CALENDAR_VEVENT;
    supported_components |= calendar::CALENDAR_VTODO;
    createCalendar.set_supported_component_set(supported_components);
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->CreateCalendar(
      createCalendar,
      base::BindOnce(&CalendarCreateFunction::CreateComplete, this),
      &task_tracker_);
  return RespondLater();
}

void CalendarCreateFunction::CreateComplete(
    std::shared_ptr<calendar::CreateCalendarResult> results) {
  if (!results->success) {
    Respond(Error("Error creating calendar"));
  } else {
    Calendar ev = GetCalendarItem(results->createdRow);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::Create::Results::Create(ev)));
  }
}

CalendarGetAllFunction::~CalendarGetAllFunction() {}

ExtensionFunction::ResponseAction CalendarGetAllFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllCalendars(
      base::BindOnce(&CalendarGetAllFunction::GetAllComplete, this),
      &task_tracker_);

  return RespondLater();  // GetAllComplete() will be called
                          // asynchronously.
}

void CalendarGetAllFunction::GetAllComplete(
    std::shared_ptr<calendar::CalendarQueryResults> results) {
  CalendarList calendarList;

  if (results && !results->empty()) {
    for (calendar::CalendarQueryResults::CalendarResultVector::const_iterator
             iterator = results->begin();
         iterator != results->end(); ++iterator) {
      calendarList.push_back(std::move(
          *base::WrapUnique((CreateVivaldiCalendar(*iterator).release()))));
    }
  }

  Respond(
      ArgumentList(vivaldi::calendar::GetAll::Results::Create(calendarList)));
}

ExtensionFunction::ResponseAction CalendarUpdateFunction::Run() {
  std::unique_ptr<vivaldi::calendar::Update::Params> params(
      vivaldi::calendar::Update::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  calendar::Calendar updatedCalendar;

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::CalendarID calendarId;
  if (!GetIdAsInt64(id, &calendarId)) {
    return RespondNow(Error("Error. Invalid calendar id"));
  }

  if (params->changes.name.get()) {
    updatedCalendar.name = base::UTF8ToUTF16(*params->changes.name);
    updatedCalendar.updateFields |= calendar::CALENDAR_NAME;
  }

  if (params->changes.description.get()) {
    updatedCalendar.description =
        base::UTF8ToUTF16(*params->changes.description);
    updatedCalendar.updateFields |= calendar::CALENDAR_DESCRIPTION;
  }

  if (params->changes.orderindex.get()) {
    updatedCalendar.orderindex = *params->changes.orderindex;
    updatedCalendar.updateFields |= calendar::CALENDAR_ORDERINDEX;
  }

  if (params->changes.color.get()) {
    updatedCalendar.color = *params->changes.color;
    updatedCalendar.updateFields |= calendar::CALENDAR_COLOR;
  }

  if (params->changes.hidden.get()) {
    updatedCalendar.hidden = *params->changes.hidden;
    updatedCalendar.updateFields |= calendar::CALENDAR_HIDDEN;
  }

  if (params->changes.active.get()) {
    updatedCalendar.active = *params->changes.active;
    updatedCalendar.updateFields |= calendar::CALENDAR_ACTIVE;
  }

  if (params->changes.iconindex.get()) {
    updatedCalendar.iconindex = *params->changes.iconindex;
    updatedCalendar.updateFields |= calendar::CALENDAR_ICONINDEX;
  }

  if (params->changes.ctag.get()) {
    updatedCalendar.ctag = *params->changes.ctag;
    updatedCalendar.updateFields |= calendar::CALENDAR_CTAG;
  }

  if (params->changes.last_checked.get()) {
    updatedCalendar.last_checked = GetTime(*params->changes.last_checked);
    updatedCalendar.updateFields |= calendar::CALENDAR_LAST_CHECKED;
  }

  if (params->changes.timezone.get()) {
    updatedCalendar.timezone = *params->changes.timezone;
    updatedCalendar.updateFields |= calendar::CALENDAR_TIMEZONE;
  }

  if (params->changes.supported_calendar_component.get()) {
    int supported_components = calendar::NONE;
    if (params->changes.supported_calendar_component->vevent)
      supported_components |= calendar::CALENDAR_VEVENT;

    if (params->changes.supported_calendar_component->vtodo)
      supported_components |= calendar::CALENDAR_VTODO;

    if (params->changes.supported_calendar_component->vjournal)
      supported_components |= calendar::CALENDAR_VJOURNAL;

    updatedCalendar.supported_component_set = supported_components;
    updatedCalendar.updateFields |= calendar::CALENDAR_SUPPORTED_COMPONENT_SET;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->UpdateCalendar(
      calendarId, updatedCalendar,
      base::BindOnce(&CalendarUpdateFunction::UpdateCalendarComplete, this),
      &task_tracker_);
  return RespondLater();  // UpdateCalendarComplete() will be called
                          // asynchronously.
}

void CalendarUpdateFunction::UpdateCalendarComplete(
    std::shared_ptr<calendar::UpdateCalendarResult> results) {
  if (!results->success) {
    Respond(
        ErrorWithArguments(vivaldi::calendar::Update::Results::Create(false),
                           "Error updating calendar"));
  } else {
    Respond(ArgumentList(vivaldi::calendar::Update::Results::Create(true)));
  }
}

ExtensionFunction::ResponseAction CalendarDeleteFunction::Run() {
  std::unique_ptr<vivaldi::calendar::Delete::Params> params(
      vivaldi::calendar::Delete::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::CalendarID calendarId;

  if (!GetIdAsInt64(id, &calendarId)) {
    return RespondNow(Error("Error. Invalid calendar id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteCalendar(
      calendarId,
      base::BindOnce(&CalendarDeleteFunction::DeleteCalendarComplete, this),
      &task_tracker_);
  return RespondLater();  // DeleteCalendarComplete() will be called
                          // asynchronously.
}

void CalendarDeleteFunction::DeleteCalendarComplete(
    std::shared_ptr<calendar::DeleteCalendarResult> results) {
  if (!results->success) {
    Respond(Error("Error deleting calendar"));
  } else {
    Respond(ArgumentList(vivaldi::calendar::Delete::Results::Create(true)));
  }
}

ExtensionFunction::ResponseAction CalendarGetAllEventTypesFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllEventTypes(
      base::BindOnce(
          &CalendarGetAllEventTypesFunction::GetAllEventTypesComplete, this),
      &task_tracker_);

  return RespondLater();  // CalendarGetAllEventTypesFunction() will be called
                          // asynchronously.
}

void CalendarGetAllEventTypesFunction::GetAllEventTypesComplete(
    std::shared_ptr<calendar::EventTypeRows> results) {
  EventTypeList event_type_list;
  for (EventTypeRow event_type : *results) {
    event_type_list.push_back(GetEventType(std::move(event_type)));
  }

  Respond(ArgumentList(
      vivaldi::calendar::GetAllEventTypes::Results::Create(event_type_list)));
}

ExtensionFunction::ResponseAction CalendarEventTypeCreateFunction::Run() {
  std::unique_ptr<vivaldi::calendar::EventTypeCreate::Params> params(
      vivaldi::calendar::EventTypeCreate::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  calendar::EventTypeRow create_event_type;

  std::u16string name;
  name = base::UTF8ToUTF16(params->event_type.name);
  create_event_type.set_name(name);

  if (params->event_type.color.get()) {
    std::string color = *params->event_type.color;
    create_event_type.set_color(color);
  }

  if (params->event_type.iconindex.get()) {
    int iconindex = *params->event_type.iconindex;
    create_event_type.set_iconindex(iconindex);
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->CreateEventType(
      create_event_type,
      base::BindOnce(&CalendarEventTypeCreateFunction::CreateEventTypeComplete,
                     this),
      &task_tracker_);
  return RespondLater();
}

void CalendarEventTypeCreateFunction::CreateEventTypeComplete(
    std::shared_ptr<calendar::CreateEventTypeResult> results) {
  if (!results->success) {
    Respond(Error("Error creating event type"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarEventTypeUpdateFunction::Run() {
  std::unique_ptr<vivaldi::calendar::EventTypeUpdate::Params> params(
      vivaldi::calendar::EventTypeUpdate::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventTypeID event_type_id;

  if (!GetIdAsInt64(id, &event_type_id)) {
    return RespondNow(Error("Error. Invalid event type id"));
  }

  calendar::EventType update_event_type;

  if (params->changes.name.get()) {
    std::u16string name;
    name = base::UTF8ToUTF16(*params->changes.name);
    update_event_type.name = name;
    update_event_type.updateFields |= calendar::NAME;
  }

  if (params->changes.color.get()) {
    std::string color = *params->changes.color;
    update_event_type.color = color;
    update_event_type.updateFields |= calendar::COLOR;
  }

  if (params->changes.iconindex.get()) {
    int iconindex = *params->changes.iconindex;
    update_event_type.iconindex = iconindex;
    update_event_type.updateFields |= calendar::ICONINDEX;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->UpdateEventType(
      event_type_id, update_event_type,
      base::BindOnce(&CalendarEventTypeUpdateFunction::UpdateEventTypeComplete,
                     this),
      &task_tracker_);
  return RespondLater();
}

void CalendarEventTypeUpdateFunction::UpdateEventTypeComplete(
    std::shared_ptr<calendar::UpdateEventTypeResult> results) {
  if (!results->success) {
    Respond(Error("Error updating event type"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarDeleteEventTypeFunction::Run() {
  std::unique_ptr<vivaldi::calendar::DeleteEventType::Params> params(
      vivaldi::calendar::DeleteEventType::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventTypeID event_type_id;

  if (!GetIdAsInt64(id, &event_type_id)) {
    return RespondNow(Error("Error. Invalid event type id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteEventType(
      event_type_id,
      base::BindOnce(&CalendarDeleteEventTypeFunction::DeleteEventTypeComplete,
                     this),
      &task_tracker_);
  return RespondLater();  // DeleteEventTypeComplete() will be called
                          // asynchronously.
}

void CalendarDeleteEventTypeFunction::DeleteEventTypeComplete(
    std::shared_ptr<calendar::DeleteEventTypeResult> result) {
  if (!result->success) {
    Respond(Error("Error deleting event type"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarCreateEventExceptionFunction::Run() {
  std::unique_ptr<vivaldi::calendar::CreateEventException::Params> params(
      vivaldi::calendar::CreateEventException::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::u16string id;
  id = base::UTF8ToUTF16(params->parent_event_id);
  calendar::EventID parent_event_id;

  if (!GetIdAsInt64(id, &parent_event_id)) {
    return RespondNow(Error("Error. Invalid parent event id"));
  }

  RecurrenceExceptionRow row;
  row.parent_event_id = parent_event_id;
  row.exception_day = GetTime(*params->date.get());
  row.cancelled = params->cancelled;

  if (params->exception_event_id.get()) {
    std::string ex_id = *params->exception_event_id;
    if (ex_id.length() > 0) {
      calendar::EventID exception_event_id;
      if (!GetStdStringAsInt64(ex_id, &exception_event_id)) {
        return RespondNow(Error("Error. Invalid exception event id"));
      }
      row.exception_event_id = exception_event_id;
    }
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->CreateRecurrenceException(
      row,
      base::BindOnce(
          &CalendarCreateEventExceptionFunction::CreateEventExceptionComplete,
          this),
      &task_tracker_);

  return RespondLater();
}

void CalendarCreateEventExceptionFunction::CreateEventExceptionComplete(
    std::shared_ptr<calendar::EventResultCB> results) {
  if (!results->success) {
    Respond(Error("Error creating event exception"));
  } else {
    std::unique_ptr<CalendarEvent> event = CreateVivaldiEvent(results->event);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::CreateEventException::Results::Create(
            *event)));
  }
}

ExtensionFunction::ResponseAction CalendarGetAllNotificationsFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllNotifications(
      base::BindOnce(
          &CalendarGetAllNotificationsFunction::GetAllNotificationsComplete,
          this),
      &task_tracker_);

  return RespondLater();  // GetAllNotificationsComplete() will be called
                          // asynchronously.
}

void CalendarGetAllNotificationsFunction::GetAllNotificationsComplete(
    std::shared_ptr<calendar::GetAllNotificationResult> result) {
  NotificaionList notification_list;

  for (const auto& notification : result->notifications) {
    notification_list.push_back(CreateNotification(notification));
  }

  Respond(ArgumentList(vivaldi::calendar::GetAllNotifications::Results::Create(
      notification_list)));
}

ExtensionFunction::ResponseAction CalendarCreateNotificationFunction::Run() {
  std::unique_ptr<vivaldi::calendar::CreateNotification::Params> params(
      vivaldi::calendar::CreateNotification::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  NotificationRow row;
  if (params->create_notification.event_id.get()) {
    std::u16string id;
    id = base::UTF8ToUTF16(*params->create_notification.event_id);
    calendar::EventID event_id;

    if (!GetIdAsInt64(id, &event_id)) {
      return RespondNow(Error("Error. Invalid event id"));
    }
    row.event_id = event_id;
  }

  row.name = base::UTF8ToUTF16(params->create_notification.name);
  row.when = GetTime(params->create_notification.when);
  if (params->create_notification.description.get()) {
    row.description =
        base::UTF8ToUTF16(*params->create_notification.description);
  }

  if (params->create_notification.delay.get()) {
    row.delay = *params->create_notification.delay;
  }

  if (params->create_notification.period.get()) {
    row.period = GetTime(*params->create_notification.period);
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->CreateNotification(
      row,
      base::BindOnce(
          &CalendarCreateNotificationFunction::CreateNotificationComplete,
          this),
      &task_tracker_);

  return RespondLater();
}

void CalendarCreateNotificationFunction::CreateNotificationComplete(
    std::shared_ptr<calendar::NotificationResult> results) {
  if (!results->success) {
    Respond(Error("Error creating notification"));
  } else {
    Notification notification = CreateNotification(results->notification_row);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::CreateNotification::Results::Create(
            notification)));
  }
}

ExtensionFunction::ResponseAction CalendarUpdateNotificationFunction::Run() {
  std::unique_ptr<vivaldi::calendar::UpdateNotification::Params> params(
      vivaldi::calendar::UpdateNotification::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  calendar::UpdateNotificationRow update_notification;

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::EventID eventId;

  if (!GetIdAsInt64(id, &eventId)) {
    return RespondNow(Error("Error. Invalid notification id"));
  }

  update_notification.notification_row.id = eventId;

  if (params->changes.name.get()) {
    update_notification.notification_row.name =
        base::UTF8ToUTF16(*params->changes.name);
    update_notification.updateFields |= calendar::NOTIFICATION_NAME;
  }

  if (params->changes.description.get()) {
    update_notification.notification_row.description =
        base::UTF8ToUTF16(*params->changes.description);
    update_notification.updateFields |= calendar::NOTIFICATION_DESCRIPTION;
  }

  if (params->changes.when.get()) {
    double when = *params->changes.when;

    update_notification.notification_row.when = GetTime(when);
    update_notification.updateFields |= calendar::NOTIFICATION_WHEN;
  }

  if (params->changes.period) {
    update_notification.notification_row.period =
        GetTime(*params->changes.period);
    update_notification.updateFields |= calendar::NOTIFICATION_PERIOD;
  }

  if (params->changes.delay) {
    update_notification.notification_row.delay = *params->changes.delay;
    update_notification.updateFields |= calendar::NOTIFICATION_DELAY;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->UpdateNotification(
      eventId, update_notification,
      base::BindOnce(
          &CalendarUpdateNotificationFunction::UpdateNotificationComplete,
          this),
      &task_tracker_);
  return RespondLater();  // UpdateNotificationComplete() will be called
                          // asynchronously.
}

void CalendarUpdateNotificationFunction::UpdateNotificationComplete(
    std::shared_ptr<calendar::NotificationResult> results) {
  if (!results->success) {
    Respond(Error(results->message));
  } else {
    Notification notification = CreateNotification(results->notification_row);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::UpdateNotification::Results::Create(
            notification)));
  }
}

ExtensionFunction::ResponseAction CalendarDeleteNotificationFunction::Run() {
  std::unique_ptr<vivaldi::calendar::DeleteNotification::Params> params(
      vivaldi::calendar::DeleteNotification::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::NotificationID notification_id;

  if (!GetIdAsInt64(id, &notification_id)) {
    return RespondNow(Error("Error. Invalid notification id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteNotification(
      notification_id,
      base::BindOnce(
          &CalendarDeleteNotificationFunction::DeleteNotificationComplete,
          this),
      &task_tracker_);
  return RespondLater();  // DeleteNotificationComplete() will be called
                          // asynchronously.
}

void CalendarDeleteNotificationFunction::DeleteNotificationComplete(
    std::shared_ptr<calendar::DeleteNotificationResult> results) {
  if (!results->success) {
    Respond(Error("Error deleting event"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarCreateInviteFunction::Run() {
  std::unique_ptr<vivaldi::calendar::CreateInvite::Params> params(
      vivaldi::calendar::CreateInvite::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  InviteRow row;

  std::u16string id;

  id = base::UTF8ToUTF16(params->create_invite.event_id);
  calendar::EventID event_id;

  if (!GetIdAsInt64(id, &event_id)) {
    return RespondNow(Error("Error. Invalid event id"));
  }
  row.event_id = event_id;
  row.name = base::UTF8ToUTF16(params->create_invite.name);
  row.address = base::UTF8ToUTF16(params->create_invite.address);

  if (params->create_invite.sent.get()) {
    row.sent = *params->create_invite.sent;
  }

  if (params->create_invite.partstat.get()) {
    row.partstat = *params->create_invite.partstat;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->CreateInvite(
      row,
      base::BindOnce(&CalendarCreateInviteFunction::CreateInviteComplete, this),
      &task_tracker_);

  return RespondLater();
}

void CalendarCreateInviteFunction::CreateInviteComplete(
    std::shared_ptr<calendar::InviteResult> results) {
  if (!results->success) {
    Respond(Error("Error creating invite"));
  } else {
    vivaldi::calendar::Invite invite = CreateInviteItem(results->inviteRow);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::CreateInvite::Results::Create(invite)));
  }
}

ExtensionFunction::ResponseAction CalendarDeleteInviteFunction::Run() {
  std::unique_ptr<vivaldi::calendar::DeleteNotification::Params> params(
      vivaldi::calendar::DeleteNotification::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::InviteID invite_id;

  if (!GetIdAsInt64(id, &invite_id)) {
    return RespondNow(Error("Error. Invalid invite id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->DeleteInvite(
      invite_id,
      base::BindOnce(&CalendarDeleteInviteFunction::DeleteInviteComplete, this),
      &task_tracker_);
  return RespondLater();  // DeleteInviteComplete() will be called
                          // asynchronously.
}

void CalendarDeleteInviteFunction::DeleteInviteComplete(
    std::shared_ptr<calendar::DeleteInviteResult> results) {
  if (!results->success) {
    Respond(Error("Error deleting invite"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction CalendarUpdateInviteFunction::Run() {
  std::unique_ptr<vivaldi::calendar::UpdateInvite::Params> params(
      vivaldi::calendar::UpdateInvite::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::u16string id;
  id = base::UTF8ToUTF16(params->update_invite.id);
  calendar::InviteID invite_id;

  if (!GetIdAsInt64(id, &invite_id)) {
    return RespondNow(Error("Error. Invalid invite id"));
  }

  if (!GetIdAsInt64(id, &invite_id)) {
    return RespondNow(Error("Error. Invalid invite id"));
  }

  calendar::UpdateInviteRow updateInvite;

  updateInvite.invite_row.id = invite_id;

  if (params->update_invite.address.get()) {
    updateInvite.invite_row.address =
        base::UTF8ToUTF16(*params->update_invite.address);
    updateInvite.updateFields |= calendar::INVITE_ADDRESS;
  }

  if (params->update_invite.name.get()) {
    updateInvite.invite_row.name =
        base::UTF8ToUTF16(*params->update_invite.name);
    updateInvite.updateFields |= calendar::INVITE_NAME;
  }

  if (params->update_invite.partstat.get()) {
    updateInvite.invite_row.partstat = *params->update_invite.partstat;
    updateInvite.updateFields |= calendar::INVITE_PARTSTAT;
  }

  if (params->update_invite.sent.get()) {
    updateInvite.invite_row.sent = *params->update_invite.sent;
    updateInvite.updateFields |= calendar::INVITE_SENT;
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->UpdateInvite(
      updateInvite,
      base::BindOnce(&CalendarUpdateInviteFunction::UpdateInviteComplete, this),
      &task_tracker_);
  return RespondLater();  // DeleteInviteComplete() will be called
                          // asynchronously.
}

void CalendarUpdateInviteFunction::UpdateInviteComplete(
    std::shared_ptr<calendar::InviteResult> results) {
  if (!results->success) {
    Respond(Error("Error updating invite"));
  } else {
    vivaldi::calendar::Invite invite = CreateInviteItem(results->inviteRow);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::UpdateInvite::Results::Create(invite)));
  }
}

ExtensionFunction::ResponseAction CalendarCreateAccountFunction::Run() {
  std::unique_ptr<vivaldi::calendar::CreateAccount::Params> params(
      vivaldi::calendar::CreateAccount::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AccountRow row;
  row.name = base::UTF8ToUTF16(params->to_create.name);
  row.url = GURL(params->to_create.url);
  row.account_type = MapAccountType(params->to_create.account_type);

  if (params->to_create.interval) {
    row.interval = *params->to_create.interval;
  }
  row.username = base::UTF8ToUTF16(params->to_create.username);

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->CreateAccount(
      row,
      base::BindOnce(&CalendarCreateAccountFunction::CreateAccountComplete,
                     this),
      &task_tracker_);

  return RespondLater();
}

void CalendarCreateAccountFunction::CreateAccountComplete(
    std::shared_ptr<calendar::CreateAccountResult> results) {
  if (!results->success) {
    Respond(Error("Error creating account"));
  } else {
    vivaldi::calendar::Account account = GetAccountType(results->createdRow);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::CreateAccount::Results::Create(account,
                                                                      true)));
  }
}

ExtensionFunction::ResponseAction CalendarDeleteAccountFunction::Run() {
  std::unique_ptr<vivaldi::calendar::DeleteAccount::Params> params(
      vivaldi::calendar::DeleteAccount::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::AccountID accountId;

  if (!GetIdAsInt64(id, &accountId)) {
    return RespondNow(Error("Error. Invalid account id"));
  }

  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  model->DeleteAccount(
      accountId,
      base::BindOnce(&CalendarDeleteAccountFunction::DeleteAccountComplete,
                     this),
      &task_tracker_);

  return RespondLater();
}

void CalendarDeleteAccountFunction::DeleteAccountComplete(
    std::shared_ptr<calendar::DeleteAccountResult> results) {
  if (!results->success) {
    Respond(Error("Error deleting account"));
  } else {
    Respond(ArgumentList(
        extensions::vivaldi::calendar::DeleteAccount::Results::Create(
            results->success)));
  }
}

ExtensionFunction::ResponseAction CalendarUpdateAccountFunction::Run() {
  std::unique_ptr<vivaldi::calendar::UpdateAccount::Params> params(
      vivaldi::calendar::UpdateAccount::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AccountRow row;
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());
  std::u16string id;
  id = base::UTF8ToUTF16(params->id);
  calendar::AccountID accountId;

  if (!GetIdAsInt64(id, &accountId)) {
    return RespondNow(Error("Error. Invalid account id"));
  }

  row.id = accountId;

  if (params->changes.name.get()) {
    std::u16string name;
    name = base::UTF8ToUTF16(*params->changes.name);
    row.name = name;
    row.updateFields |= calendar::ACCOUNT_NAME;
  }

  if (params->changes.username.get()) {
    std::u16string username;
    username = base::UTF8ToUTF16(*params->changes.username);
    row.username = username;
    row.updateFields |= calendar::ACCOUNT_USERNAME;
  }

  if (params->changes.url.get()) {
    row.url = GURL(*params->changes.url);
    row.updateFields |= calendar::ACCOUNT_URL;
  }

  if (params->changes.account_type) {
    row.account_type = MapAccountType(params->changes.account_type);
    row.updateFields |= calendar::ACCOUNT_TYPE;
  }

  if (params->changes.interval.get()) {
    row.interval = *params->changes.interval;
    row.updateFields |= calendar::ACCOUNT_INTERVAL;
  }

  model->UpdateAccount(
      row,
      base::BindOnce(&CalendarUpdateAccountFunction::UpdateAccountComplete,
                     this),
      &task_tracker_);

  return RespondLater();
}

void CalendarUpdateAccountFunction::UpdateAccountComplete(
    std::shared_ptr<calendar::UpdateAccountResult> results) {
  if (!results->success) {
    Respond(Error("Error updating account"));
  } else {
    vivaldi::calendar::Account account = GetAccountType(results->updatedRow);
    Respond(ArgumentList(
        extensions::vivaldi::calendar::CreateAccount::Results::Create(account,
                                                                      true)));
  }
}

ExtensionFunction::ResponseAction CalendarGetAllAccountsFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllAccounts(
      base::BindOnce(&CalendarGetAllAccountsFunction::GetAllAccountsComplete,
                     this),
      &task_tracker_);

  return RespondLater();
}

void CalendarGetAllAccountsFunction::GetAllAccountsComplete(
    std::shared_ptr<calendar::AccountRows> accounts) {
  AccountList accountList;

  for (calendar::AccountRow account : *accounts) {
    accountList.push_back(GetAccountType(std::move(account)));
  }

  Respond(ArgumentList(
      vivaldi::calendar::GetAllAccounts::Results::Create(accountList)));
}

ExtensionFunction::ResponseAction CalendarGetAllEventTemplatesFunction::Run() {
  CalendarService* model = CalendarServiceFactory::GetForProfile(GetProfile());

  model->GetAllEventTemplates(
      base::BindOnce(
          &CalendarGetAllEventTemplatesFunction::GetAllEventTemplatesComplete,
          this),
      &task_tracker_);

  return RespondLater();  // GetAllEventTemplatesComplete() will be called
                          // asynchronously.
}

void CalendarGetAllEventTemplatesFunction::GetAllEventTemplatesComplete(
    std::shared_ptr<calendar::EventQueryResults> results) {
  EventList eventList;

  if (results && !results->empty()) {
    for (calendar::EventQueryResults::EventResultVector::const_iterator
             iterator = results->begin();
         iterator != results->end(); ++iterator) {
      eventList.push_back(std::move(
          *base::WrapUnique((CreateVivaldiEvent(**iterator).release()))));
    }
  }

  Respond(ArgumentList(
      vivaldi::calendar::GetAllEventTemplates::Results::Create(eventList)));
}

}  //  namespace extensions
