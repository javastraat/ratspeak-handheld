#include "LvSettingsScreen.h"
#include "ui/VoiceSettingsText.h"
void LvSettingsScreen::buildVoiceItems(int& idx) {
    auto& settings=_cfg->settings().voice;const int start=idx;
#ifdef RSM9
    _items.push_back({"Hardware",SettingType::READONLY,nullptr,nullptr,[](int) {return String("No audio hardware");}});_items.back().id = SettingId::VoiceHardware;++idx;
#else
    _items.push_back({"Playback volume",SettingType::INTEGER,[&settings] {return int(settings.volume);},[&settings](int value) {settings.volume=value;},[](int value) {return String(value)+"%";},0,100,10});_items.back().id = SettingId::PlaybackVolume;++idx;
    _items.push_back({"Voice messages",SettingType::READONLY,nullptr,nullptr,[](int) {return String("Up to 15 seconds");}});_items.back().id = SettingId::VoiceDuration;++idx;
#endif
    _items.push_back({"Max voice messages",SettingType::INTEGER,[&settings] {return int(settings.maxMessages);},[&settings](int value) {settings.maxMessages=value;},nullptr,1,50,1});_items.back().id = SettingId::MaxVoiceMessages;++idx;
    _items.push_back({"Storage limit",SettingType::READONLY,nullptr,nullptr,[](int) {return String(handheld::ui::voice_settings::InternalLimit);}});_items.back().id = SettingId::VoiceStorageLimit;++idx;
    _items.push_back({"At limit",SettingType::READONLY,nullptr,nullptr,[](int) {return String(handheld::ui::voice_settings::Expiry);}});_items.back().id = SettingId::VoiceAtLimit;++idx;
    _items.push_back({"SD card",SettingType::READONLY,nullptr,nullptr,[](int) {return String(handheld::ui::voice_settings::SdLimit);}});_items.back().id = SettingId::VoiceSdLimit;++idx;
    _items.push_back({"Live calls",SettingType::READONLY,nullptr,nullptr,[](int) {return String("Unavailable");}});_items.back().id = SettingId::LiveCalls;++idx;
    _categories.push_back({"Voice Messages",start,idx-start,[&settings] {
#ifdef RSM9
        (void)settings;return String("No audio hardware");
#else
        return String(settings.volume)+"% volume";
#endif
    }});
    _categories.back().id = SettingsCategoryId::Voice;
}
