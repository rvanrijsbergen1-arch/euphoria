#pragma once
// Licensing for Euphoria: a 3-day trial, then the EUP serial code from the order e-mail
// (activate / validate / deactivate through the Ephoric license server at www.ephoric.nl, which needs no secret in
// the plugin; it answers in the same JSON shape as the Lemon Squeezy License API). Same system as the melody makers and The Final Master.
//
// - The trial starts the first time the plugin runs on a computer; its start is kept in two places, so deleting
//   one file does not restart it, and a clock set back in time does not extend it.
// - An activated serial is stored per computer (signed with a checksum tied to this computer, so a copied or edited
//   file is not accepted) and works offline. It is re-checked online every 7 days; after 30 days without a
//   successful check the plugin asks to go online once. A serial that was disabled (refund) stops working.
// - A serial works on 2 computers at a time; Deactivate frees a computer slot. The same computer activating again
//   gets its own slot back (the server recognises it by an anonymous device hash).
// - A key is only accepted for the configured store and product.
// - The trial is also registered on the server (per anonymous computer id), and a third copy of the trial start is
//   kept in the registry (Windows) or the preferences folder (macOS).
// - Update notice: www.ephoric.nl/api/version/euphoria is asked for the latest version.
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>
#include <mutex>

namespace lic
{
struct LicenseConfig
{
    // Must match STORE_ID / PRODUCT_ID in the website's functions/_lib/licenses.js. 0 = not configured:
    // any key is refused, so a build can never be unlocked by accident.
    static constexpr juce::int64 storeId = 1;
    // 0 until Euphoria is registered on the license server (functions/_lib/licenses.js); then set its product id here.
    // 1 = HDMM, 2 = CEMM, 3 = The Final Master. Until then the 3-day trial works and activation says "not connected yet".
    static constexpr juce::int64 productId = 0;
    static constexpr const char* serverUrl = "https://www.ephoric.nl/api/license/";
    static constexpr const char* buyUrl = "https://www.ephoric.nl/shop/";
    static constexpr const char* accountUrl = "https://www.ephoric.nl/account/";
    static constexpr const char* versionUrl = "https://www.ephoric.nl/api/version/euphoria";
    static constexpr const char* productName = "Euphoria";
    static constexpr double trialHours = 72.0;
    static constexpr double recheckDays = 7.0;
    static constexpr double offlineGraceDays = 30.0;
};

struct HttpResult { bool connected = false; int status = 0; juce::String body; };
using LicenseTransport = std::function<HttpResult (const juce::String& endpoint, const juce::StringPairArray& form)>;
LicenseTransport licenseServerTransport();

const char* currentVersion();
bool isNewerVersion (const juce::String& a, const juce::String& b);

class LicenseManager
{
public:
    enum class State { Trial, TrialExpired, Licensed, NeedsOnlineCheck };

    struct Options
    {
        juce::File folderA, folderB;
        juce::String deviceId;
        LicenseTransport transport;
        std::function<juce::Time()> clock;
        juce::int64 storeId = LicenseConfig::storeId, productId = LicenseConfig::productId;
        std::function<juce::String()> fetchVersionJson;
        std::function<juce::String()> readExtra;
        std::function<void (const juce::String&)> writeExtra;
        juce::String version = currentVersion();
    };
    static Options defaultOptions();

    explicit LicenseManager (Options o = defaultOptions());

    State getState() const;
    bool isUnlocked() const;
    double trialHoursLeft() const;
    juce::String statusText() const;
    juce::String licensedTo() const;
    juce::String maskedKey() const;

    /** Blocking network calls: run them off the message thread. */
    juce::String activate (const juce::String& key);
    juce::String deactivate();
    void revalidateIfDue();
    void checkTrialOnline();
    void checkForUpdate();
    juce::String availableUpdate() const;

    std::function<void()> onChange;

private:
    struct Data
    {
        juce::Time trialStart, lastSeen;
        juce::String key, instanceId, customer, email;
        juce::Time lastValidated;
    };
    void load();
    void save();
    juce::String checksum (const juce::String& payload) const;
    juce::String deviceHash() const;
    juce::Time now() const { return opt.clock ? opt.clock() : juce::Time::getCurrentTime(); }
    juce::String checkMeta (const juce::var& json) const;
    void changed();

    Options opt;
    mutable std::recursive_mutex lock;
    Data d;
    bool clockRolledBack = false;
    juce::String latestVersion;
};

LicenseManager& sharedLicense();
}
