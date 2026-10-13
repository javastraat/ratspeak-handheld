#include "SettingsScreen.h"
#include "ui/VoiceSettingsText.h"
void SettingsScreen::buildVoiceMenu() {
    clearRows();const auto& s=_candidate.settings().voice;
    char volume[32];snprintf(volume,sizeof volume,"Playback volume: %u%%",s.volume);
    addRow(volume, RowAction::Voice, 0);
    char limit[32];snprintf(limit,sizeof limit,"Max voice messages: %u",s.maxMessages);addRow(limit, RowAction::Voice, 1);
    addRow(handheld::ui::voice_settings::InternalLimit);
    addRow(handheld::ui::voice_settings::Expiry);
    addRow(std::string("SD card: ")+handheld::ui::voice_settings::SdLimit);
    addRow("Clips: up to 15 seconds");
    addRow("Live calls: unavailable");addRow("< Back", RowAction::Back);
}
void SettingsScreen::activateVoiceRow(int row) {
    if(!_config || _config->settingsPending() || _radioApplyPending) return;
    if(row==1) {startEditing(row,std::to_string(signed(_candidate.settings().voice.maxMessages)));_editLabel="Max voice messages: 1-50";_editInput.setMaxLength(2);return;}
    if(row!=0) return;
    auto& s=_candidate.settings().voice;s.volume=s.volume>=100?0:s.volume+10;
    applyAndSave();buildVoiceMenu();_list.setSelected(row);
}
