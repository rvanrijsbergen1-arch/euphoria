#include "Licensing.h"
#include <juce_cryptography/juce_cryptography.h>

namespace lic
{
namespace
{
constexpr const char* kSalt = "eup-4b9d27f1-licence-v1";
constexpr const char* kStateFile = "euphoria.state";

juce::String formEncode (const juce::StringPairArray& form)
{
    juce::StringArray parts;
    for (auto& k : form.getAllKeys())
        parts.add (juce::URL::addEscapeChars (k, true) + "=" + juce::URL::addEscapeChars (form[k], true));
    return parts.joinIntoString ("&");
}

juce::Time timeFrom (const juce::var& v) { return juce::Time ((juce::int64) v); }
}

LicenseTransport licenseServerTransport()
{
    return [] (const juce::String& endpoint, const juce::StringPairArray& form)
    {
        HttpResult r;
        const auto url = juce::URL (juce::String (LicenseConfig::serverUrl) + endpoint).withPOSTData (formEncode (form));
        int status = 0;
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                           .withExtraHeaders ("Accept: application/json\r\nContent-Type: application/x-www-form-urlencoded")
                           .withConnectionTimeoutMs (12000)
                           .withStatusCode (&status);
        if (auto in = url.createInputStream (options))
        {
            r.connected = true;
            r.status = status;
            r.body = in->readEntireStreamAsString();
        }
        return r;
    };
}

const char* currentVersion()
{
#ifdef EUP_VERSION
    return EUP_VERSION;
#else
    return "0.0.0";
#endif
}

bool isNewerVersion (const juce::String& a, const juce::String& b)
{
    juce::StringArray x, y;
    x.addTokens (a.trim(), ".", {});
    y.addTokens (b.trim(), ".", {});
    if (a.trim().isEmpty() || ! a.trim().containsOnly ("0123456789.")) return false;
    for (int i = 0; i < juce::jmax (x.size(), y.size()); ++i)
    {
        const int p = x[i].getIntValue(), q = y[i].getIntValue();
        if (p != q) return p > q;
    }
    return false;
}

LicenseManager::Options LicenseManager::defaultOptions()
{
    Options o;
    const auto appData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
    o.folderA = appData.getChildFile ("Euphoria");
    o.folderB = appData.getChildFile ("Ephoric").getChildFile ("Shared");
    o.deviceId = juce::SystemStats::getUniqueDeviceID();
    if (o.deviceId.isEmpty()) o.deviceId = juce::SystemStats::getComputerName() + "/" + juce::SystemStats::getLogonName();
    o.transport = licenseServerTransport();
   #if JUCE_WINDOWS
    static const juce::String regKey = "HKEY_CURRENT_USER\\Software\\Ephoric\\Euphoria\\State";
    o.readExtra = [] { return juce::WindowsRegistry::getValue (regKey); };
    o.writeExtra = [] (const juce::String& s) { juce::WindowsRegistry::setValue (regKey, s); };
   #else
   #if JUCE_MAC
    const auto extra = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Preferences/com.ephoric.euphoria.state");
   #else
    const auto extra = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config/ephoric/euphoria.state");
   #endif
    o.readExtra = [extra] { return extra.existsAsFile() ? extra.loadFileAsString() : juce::String(); };
    o.writeExtra = [extra] (const juce::String& s) { extra.getParentDirectory().createDirectory(); extra.replaceWithText (s); };
   #endif
    o.fetchVersionJson = []
    {
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                           .withExtraHeaders ("Accept: application/json")
                           .withConnectionTimeoutMs (8000);
        if (auto in = juce::URL (LicenseConfig::versionUrl).createInputStream (options))
            return in->readEntireStreamAsString();
        return juce::String();
    };
    return o;
}

LicenseManager::LicenseManager (Options o) : opt (std::move (o))
{
    load();
}

juce::String LicenseManager::checksum (const juce::String& payload) const
{
    return juce::SHA256 ((payload + "|" + kSalt + "|" + opt.deviceId).toUTF8()).toHexString();
}

juce::String LicenseManager::deviceHash() const
{
    return juce::SHA256 ((juce::String ("device|") + kSalt + "|" + opt.deviceId).toUTF8()).toHexString().substring (0, 32);
}

void LicenseManager::load()
{
    std::lock_guard<std::recursive_mutex> g (lock);
    const auto t = now();
    bool anyFile = false, anyValid = false;
    Data best;
    juce::StringArray stored;
    for (auto& f : { opt.folderA.getChildFile ("license.dat"), opt.folderB.getChildFile (kStateFile) })
        if (f.existsAsFile()) stored.add (f.loadFileAsString());
    if (opt.readExtra) if (const auto s = opt.readExtra(); s.trim().isNotEmpty()) stored.add (s);
    for (auto& blob : stored)
    {
        anyFile = true;
        juce::MemoryOutputStream decoded;
        if (! juce::Base64::convertFromBase64 (decoded, blob.trim())) continue;
        const auto text = decoded.toString();
        const auto payload = text.upToLastOccurrenceOf ("\n", false, false);
        if (text.fromLastOccurrenceOf ("\n", false, false) != checksum (payload)) continue;
        const auto json = juce::JSON::parse (payload);
        if (! json.isObject()) continue;
        Data x;
        x.trialStart = timeFrom (json["trialStart"]);
        x.lastSeen = timeFrom (json["lastSeen"]);
        x.key = json["key"].toString();
        x.instanceId = json["instance"].toString();
        x.customer = json["customer"].toString();
        x.email = json["email"].toString();
        x.lastValidated = timeFrom (json["validated"]);
        if (! anyValid) best = x;
        else
        {
            best.trialStart = juce::jmin (best.trialStart, x.trialStart);
            best.lastSeen = juce::jmax (best.lastSeen, x.lastSeen);
            if (x.key.isNotEmpty() && (best.key.isEmpty() || x.lastValidated > best.lastValidated))
            { best.key = x.key; best.instanceId = x.instanceId; best.customer = x.customer; best.email = x.email; best.lastValidated = x.lastValidated; }
        }
        anyValid = true;
    }
    if (anyValid) d = best;
    else if (anyFile) d.trialStart = t - juce::RelativeTime::hours (LicenseConfig::trialHours);
    else d.trialStart = t;
    if (d.lastSeen.toMilliseconds() == 0) d.lastSeen = t;
    clockRolledBack = t < d.lastSeen - juce::RelativeTime::hours (2);
    if (! clockRolledBack) d.lastSeen = juce::jmax (d.lastSeen, t);
    save();
}

void LicenseManager::save()
{
    std::lock_guard<std::recursive_mutex> g (lock);
    auto* o = new juce::DynamicObject();
    o->setProperty ("trialStart", d.trialStart.toMilliseconds());
    o->setProperty ("lastSeen", d.lastSeen.toMilliseconds());
    o->setProperty ("key", d.key);
    o->setProperty ("instance", d.instanceId);
    o->setProperty ("customer", d.customer);
    o->setProperty ("email", d.email);
    o->setProperty ("validated", d.lastValidated.toMilliseconds());
    const auto payload = juce::JSON::toString (juce::var (o), true);
    const auto text = payload + "\n" + checksum (payload);
    const auto encoded = juce::Base64::toBase64 (text.toRawUTF8(), text.getNumBytesAsUTF8());
    for (auto& f : { opt.folderA.getChildFile ("license.dat"), opt.folderB.getChildFile (kStateFile) })
    {
        f.getParentDirectory().createDirectory();
        f.replaceWithText (encoded);
    }
    if (opt.writeExtra) opt.writeExtra (encoded);
}

LicenseManager::State LicenseManager::getState() const
{
    std::lock_guard<std::recursive_mutex> g (lock);
    if (d.key.isNotEmpty())
        return (now() - d.lastValidated).inDays() > LicenseConfig::offlineGraceDays ? State::NeedsOnlineCheck : State::Licensed;
    return trialHoursLeft() > 0.0 ? State::Trial : State::TrialExpired;
}

bool LicenseManager::isUnlocked() const
{
    const auto s = getState();
    return s == State::Trial || s == State::Licensed;
}

double LicenseManager::trialHoursLeft() const
{
    std::lock_guard<std::recursive_mutex> g (lock);
    if (clockRolledBack) return 0.0;
    return juce::jmax (0.0, LicenseConfig::trialHours - (now() - d.trialStart).inHours());
}

juce::String LicenseManager::licensedTo() const
{
    std::lock_guard<std::recursive_mutex> g (lock);
    return d.customer.isNotEmpty() ? d.customer : d.email;
}

juce::String LicenseManager::maskedKey() const
{
    std::lock_guard<std::recursive_mutex> g (lock);
    return d.key.length() > 8 ? "****-" + d.key.getLastCharacters (9) : d.key;
}

juce::String LicenseManager::statusText() const
{
    switch (getState())
    {
        case State::Trial:
        {
            const int h = (int) std::ceil (trialHoursLeft());
            return "Trial: " + (h >= 24 ? juce::String (h / 24) + (h / 24 == 1 ? " day " : " days ") + juce::String (h % 24) + " h" : juce::String (h) + " h") + " left";
        }
        case State::TrialExpired:     return "Trial ended - enter your serial code";
        case State::NeedsOnlineCheck: return "Connect to the internet once to confirm your license";
        case State::Licensed:         return "Licensed" + (licensedTo().isNotEmpty() ? " to " + licensedTo() : juce::String());
    }
    return {};
}

juce::String LicenseManager::checkMeta (const juce::var& json) const
{
    if (opt.storeId == 0 || opt.productId == 0) return "This build is not connected to the store yet.";
    const auto meta = json["meta"];
    if ((juce::int64) meta["store_id"] != opt.storeId || (juce::int64) meta["product_id"] != opt.productId)
        return juce::String ("This license key is not for ") + LicenseConfig::productName + ".";
    return {};
}

void LicenseManager::changed()
{
    if (onChange) onChange();
}

juce::String LicenseManager::activate (const juce::String& keyIn)
{
    const auto key = keyIn.trim().toUpperCase();
    if (key.length() < 8) return "Please paste the serial code (EUP-....) from your order e-mail.";
    if (opt.storeId == 0 || opt.productId == 0) return "This build is not connected to the store yet.";
    juce::StringPairArray form;
    form.set ("license_key", key);
    form.set ("instance_name", juce::SystemStats::getComputerName().substring (0, 60) + " (" + juce::SystemStats::getOperatingSystemName() + ")");
    form.set ("device", deviceHash());
    const auto r = opt.transport ("activate", form);
    if (! r.connected) return "No connection to the license server. Check your internet connection and try again.";
    const auto json = juce::JSON::parse (r.body);
    if (! json.isObject()) return "The license server gave an unexpected answer (" + juce::String (r.status) + "). Please try again later.";
    if (! (bool) json["activated"])
    {
        const auto err = json["error"].toString();
        return err.isNotEmpty() ? err : juce::String ("This serial code could not be activated.");
    }
    const auto instanceId = json["instance"]["id"].toString();
    if (const auto wrong = checkMeta (json); wrong.isNotEmpty())
    {
        juce::StringPairArray undo;
        undo.set ("license_key", key);
        undo.set ("instance_id", instanceId);
        opt.transport ("deactivate", undo);
        return wrong;
    }
    {
        std::lock_guard<std::recursive_mutex> g (lock);
        d.key = key;
        d.instanceId = instanceId;
        d.customer = json["meta"]["customer_name"].toString();
        d.email = json["meta"]["customer_email"].toString();
        d.lastValidated = now();
        d.lastSeen = juce::jmax (d.lastSeen, now());
        save();
    }
    changed();
    return {};
}

juce::String LicenseManager::deactivate()
{
    juce::String key, instance;
    {
        std::lock_guard<std::recursive_mutex> g (lock);
        key = d.key;
        instance = d.instanceId;
    }
    if (key.isEmpty()) return {};
    juce::StringPairArray form;
    form.set ("license_key", key);
    form.set ("instance_id", instance);
    const auto r = opt.transport ("deactivate", form);
    if (! r.connected) return "Deactivating needs an internet connection, so the computer slot is freed on the server.";
    const auto json = juce::JSON::parse (r.body);
    if (! (bool) json["deactivated"] && r.status != 404)
    {
        const auto err = json["error"].toString();
        return err.isNotEmpty() ? err : juce::String ("The license could not be deactivated. Please try again.");
    }
    {
        std::lock_guard<std::recursive_mutex> g (lock);
        d.key.clear(); d.instanceId.clear(); d.customer.clear(); d.email.clear();
        d.lastValidated = {};
        save();
    }
    changed();
    return {};
}

void LicenseManager::revalidateIfDue()
{
    juce::String key, instance;
    {
        std::lock_guard<std::recursive_mutex> g (lock);
        if (d.key.isEmpty() || (now() - d.lastValidated).inDays() < LicenseConfig::recheckDays) return;
        key = d.key;
        instance = d.instanceId;
    }
    juce::StringPairArray form;
    form.set ("license_key", key);
    form.set ("instance_id", instance);
    const auto r = opt.transport ("validate", form);
    if (! r.connected || r.status >= 500) return;
    const auto json = juce::JSON::parse (r.body);
    if (! json.isObject()) return;
    const bool ok = (bool) json["valid"] && checkMeta (json).isEmpty() && json["license_key"]["status"].toString() == "active";
    {
        std::lock_guard<std::recursive_mutex> g (lock);
        if (ok) d.lastValidated = now();
        else { d.key.clear(); d.instanceId.clear(); d.customer.clear(); d.email.clear(); d.lastValidated = {}; }
        d.lastSeen = juce::jmax (d.lastSeen, now());
        save();
    }
    changed();
}

void LicenseManager::checkTrialOnline()
{
    juce::StringPairArray form;
    {
        std::lock_guard<std::recursive_mutex> g (lock);
        if (d.key.isNotEmpty()) return;
        form.set ("device", deviceHash());
        form.set ("start", juce::String (d.trialStart.toMilliseconds()));
    }
    const auto r = opt.transport ("trial", form);
    if (! r.connected || r.status != 200) return;
    const auto json = juce::JSON::parse (r.body);
    const auto start = (juce::int64) json["start"], serverNow = (juce::int64) json["now"];
    if (start <= 0) return;
    {
        std::lock_guard<std::recursive_mutex> g (lock);
        d.trialStart = juce::jmin (d.trialStart, juce::Time (start));
        if (serverNow > 0) d.lastSeen = juce::jmax (d.lastSeen, juce::Time (serverNow));
        clockRolledBack = now() < d.lastSeen - juce::RelativeTime::hours (2);
        save();
    }
    changed();
}

void LicenseManager::checkForUpdate()
{
    if (! opt.fetchVersionJson) return;
    const auto json = juce::JSON::parse (opt.fetchVersionJson());
    const auto latest = json["version"].toString().trim();
    {
        std::lock_guard<std::recursive_mutex> g (lock);
        latestVersion = isNewerVersion (latest, opt.version) ? latest : juce::String();
    }
    changed();
}

juce::String LicenseManager::availableUpdate() const
{
    std::lock_guard<std::recursive_mutex> g (lock);
    return latestVersion;
}

LicenseManager& sharedLicense()
{
    static LicenseManager instance;
    static std::once_flag started;
    std::call_once (started, [] { juce::Thread::launch ([] { instance.revalidateIfDue(); instance.checkTrialOnline(); instance.checkForUpdate(); }); });
    return instance;
}
}
