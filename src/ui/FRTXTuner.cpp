#include "FRTXTuner.hpp"

#include "../FRTXConfig.hpp"
#include "../FRTXParams.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

using namespace geode::prelude;

namespace {
    constexpr float kPanelWidth = 268.0f;
    constexpr float kRowHeight = 12.0f;
    constexpr float kTextScale = 0.42f;

    double readValue(FRTXParam const& p) {
        switch (p.type) {
            case FRTXParamType::Bool:
                return Mod::get()->getSettingValue<bool>(p.key) ? 1.0 : 0.0;
            case FRTXParamType::Int:
                return static_cast<double>(Mod::get()->getSettingValue<int64_t>(p.key));
            default:
                return Mod::get()->getSettingValue<double>(p.key);
        }
    }

    void writeValue(FRTXParam const& p, double value) {
        switch (p.type) {
            case FRTXParamType::Bool:
                Mod::get()->setSettingValue<bool>(p.key, value > 0.5);
                break;
            case FRTXParamType::Int:
                Mod::get()->setSettingValue<int64_t>(
                    p.key, static_cast<int64_t>(std::llround(value)));
                break;
            default:
                Mod::get()->setSettingValue<double>(p.key, value);
                break;
        }
    }

    // Returns the roster index a digit key selects, or -1. 1-9 pick the first
    // nine presets and 0 picks the tenth, which is as far as one row of digits
    // stretches.
    int presetForKey(cocos2d::enumKeyCodes key) {
        switch (key) {
            case cocos2d::KEY_One:   return 0;
            case cocos2d::KEY_Two:   return 1;
            case cocos2d::KEY_Three: return 2;
            case cocos2d::KEY_Four:  return 3;
            case cocos2d::KEY_Five:  return 4;
            case cocos2d::KEY_Six:   return 5;
            case cocos2d::KEY_Seven: return 6;
            case cocos2d::KEY_Eight: return 7;
            case cocos2d::KEY_Nine:  return 8;
            case cocos2d::KEY_Zero:  return 9;
            default: return -1;
        }
    }

    std::string formatValue(FRTXParam const& p) {
        char buffer[32];
        double const value = readValue(p);
        switch (p.type) {
            case FRTXParamType::Bool:
                return value > 0.5 ? "on" : "off";
            case FRTXParamType::Int:
                std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::llround(value)));
                return buffer;
            default:
                // Enough decimals to see a single step move, no more.
                std::snprintf(buffer, sizeof(buffer), p.step < 0.01 ? "%.3f" : "%.2f", value);
                return buffer;
        }
    }
}

namespace {
    geode::Ref<FRTXTuner> g_instance = nullptr;
}

FRTXTuner* FRTXTuner::get() {
    if (!g_instance) {
        auto node = new FRTXTuner();
        if (node->init()) {
            node->autorelease();
            g_instance = node;
        } else {
            CC_SAFE_DELETE(node);
            return nullptr;
        }
    }
    return g_instance;
}

FRTXTuner* FRTXTuner::getIfExists() {
    return g_instance;
}

bool FRTXTuner::init() {
    if (!CCNode::init()) return false;
    this->setPosition(0.0f, 0.0f);
    this->setAnchorPoint(ccp(0.0f, 0.0f));
    this->setVisible(false);
    m_usable = this->build();
    if (!m_usable) {
        log::error("could not build the tuner overlay; is chatFont.fnt loaded?");
    }
    m_selected = this->firstSelectable();
    return true;
}

bool FRTXTuner::build() {
    auto const winSize = CCDirector::sharedDirector()->getWinSize();
    float const panelHeight = std::min(winSize.height - 16.0f, kRowHeight * (kVisibleRows + 4));

    m_panel = CCLayerColor::create(ccc4(0, 0, 0, 190), kPanelWidth, panelHeight);
    if (!m_panel) return false;
    m_panel->setPosition(8.0f, winSize.height - panelHeight - 8.0f);
    this->addChild(m_panel);

    m_highlight = CCLayerColor::create(ccc4(90, 160, 255, 70), kPanelWidth - 8.0f, kRowHeight);
    if (!m_highlight) return false;
    m_panel->addChild(m_highlight);

    m_title = CCLabelBMFont::create("FRTX Live Tuner", "chatFont.fnt");
    if (!m_title) return false;
    m_title->setAnchorPoint(ccp(0.0f, 0.5f));
    m_title->setScale(kTextScale * 1.15f);
    m_title->setPosition(6.0f, panelHeight - 10.0f);
    m_panel->addChild(m_title);

    for (int i = 0; i < kVisibleRows; ++i) {
        float const y = panelHeight - 26.0f - i * kRowHeight;

        m_names[i] = CCLabelBMFont::create("", "chatFont.fnt");
        if (!m_names[i]) return false;
        m_names[i]->setAnchorPoint(ccp(0.0f, 0.5f));
        m_names[i]->setScale(kTextScale);
        m_names[i]->setPosition(8.0f, y);
        m_panel->addChild(m_names[i]);

        m_values[i] = CCLabelBMFont::create("", "chatFont.fnt");
        if (!m_values[i]) return false;
        m_values[i]->setAnchorPoint(ccp(1.0f, 0.5f));
        m_values[i]->setScale(kTextScale);
        m_values[i]->setPosition(kPanelWidth - 8.0f, y);
        m_panel->addChild(m_values[i]);
    }

    m_hint = CCLabelBMFont::create(
        "arrows move/adjust  shift coarse  alt fine  R reset  0-9 / shift preset", "chatFont.fnt");
    if (!m_hint) return false;
    m_hint->setAnchorPoint(ccp(0.0f, 0.5f));
    m_hint->setScale(kTextScale * 0.85f);
    m_hint->setPosition(6.0f, 8.0f);
    m_hint->setColor(ccc3(170, 170, 170));
    m_panel->addChild(m_hint);

    return true;
}

int FRTXTuner::firstSelectable() const {
    for (int i = 0; i < kFRTXParamCount; ++i) {
        if (this->selectable(i)) return i;
    }
    return 0;
}

bool FRTXTuner::selectable(int index) const {
    return index >= 0 && index < kFRTXParamCount
        && kFRTXParams[index].type != FRTXParamType::Section;
}

void FRTXTuner::toggle() {
    if (!m_usable) return;
    m_open = !m_open;
    this->setVisible(m_open);
    if (m_open) {
        this->tick();
        this->refresh();
    }
}

void FRTXTuner::tick() {
    if (!m_open) return;
    auto scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;
    if (this->getParent() != scene) {
        this->removeFromParentAndCleanup(false);
        // Above everything, including our own composite, so the panel is never
        // graded or bloomed.
        scene->addChild(this, std::numeric_limits<int>::max());
    }
}

void FRTXTuner::refresh() {
    // Keep the selection inside the visible window.
    if (m_selected < m_scroll) m_scroll = m_selected;
    if (m_selected >= m_scroll + kVisibleRows) m_scroll = m_selected - kVisibleRows + 1;
    m_scroll = std::clamp(m_scroll, 0, std::max(0, kFRTXParamCount - kVisibleRows));

    m_title->setString(m_status.empty()
        ? "FRTX Live Tuner"
        : ("FRTX Live Tuner  -  " + m_status).c_str());

    float const panelHeight = m_panel->getContentSize().height;

    for (int row = 0; row < kVisibleRows; ++row) {
        int const index = m_scroll + row;
        if (index >= kFRTXParamCount) {
            m_names[row]->setString("");
            m_values[row]->setString("");
            continue;
        }

        auto const& p = kFRTXParams[index];
        if (p.type == FRTXParamType::Section) {
            m_names[row]->setString(p.label);
            m_names[row]->setColor(ccc3(120, 200, 255));
            m_values[row]->setString("");
        } else {
            m_names[row]->setString(p.label);
            m_names[row]->setColor(index == m_selected ? ccc3(255, 255, 255) : ccc3(190, 190, 190));
            m_values[row]->setString(formatValue(p).c_str());
            m_values[row]->setColor(index == m_selected ? ccc3(255, 255, 255) : ccc3(190, 190, 190));
        }

        if (index == m_selected) {
            m_highlight->setPosition(4.0f, panelHeight - 26.0f - row * kRowHeight - kRowHeight * 0.5f);
        }
    }
}

void FRTXTuner::moveSelection(int delta) {
    int index = m_selected;
    for (int guard = 0; guard < kFRTXParamCount; ++guard) {
        index += delta;
        if (index < 0) index = kFRTXParamCount - 1;
        if (index >= kFRTXParamCount) index = 0;
        if (this->selectable(index)) break;
    }
    m_selected = index;
    this->refresh();
}

void FRTXTuner::movePage(int direction) {
    for (int i = 0; i < kVisibleRows - 2; ++i) this->moveSelection(direction);
}

void FRTXTuner::moveToEnd(int direction) {
    m_selected = direction < 0 ? 0 : kFRTXParamCount - 1;
    if (!this->selectable(m_selected)) this->moveSelection(direction < 0 ? 1 : -1);
    else this->refresh();
}

void FRTXTuner::adjust(int direction, KeyboardModifier mods) {
    if (!this->selectable(m_selected)) return;
    auto const& p = kFRTXParams[m_selected];

    if (p.type == FRTXParamType::Bool) {
        writeValue(p, readValue(p) > 0.5 ? 0.0 : 1.0);
    } else {
        double step = p.step;
        if (mods & KeyboardModifier::Shift) step *= 10.0;
        if (mods & KeyboardModifier::Alt) step *= 0.1;
        writeValue(p, std::clamp(readValue(p) + step * direction, p.min, p.max));
    }

    this->refresh();
}

void FRTXTuner::resetSelected() {
    if (!this->selectable(m_selected)) return;
    auto const& p = kFRTXParams[m_selected];
    writeValue(p, p.def);
    this->refresh();
}

bool FRTXTuner::handleKey(KeyboardInputData& data) {
    if (!m_open) return false;
    if (data.action == KeyboardInputData::Action::Release) return false;

    // enumKeyCodes carries two arrow families: KEY_Up/Down/Left/Right, which are
    // the Windows virtual key codes and what the input layer actually sends, and
    // KEY_ArrowUp and friends at 0x11B, which it does not. Matching only the
    // latter is why navigation did nothing at all; accept both.
    switch (data.key) {
        case KEY_Up:
        case KEY_ArrowUp:    this->moveSelection(-1); return true;
        case KEY_Down:
        case KEY_ArrowDown:  this->moveSelection(1); return true;
        case KEY_Left:
        case KEY_ArrowLeft:  this->adjust(-1, data.modifiers); return true;
        case KEY_Right:
        case KEY_ArrowRight: this->adjust(1, data.modifiers); return true;
        case KEY_PageUp:     this->movePage(-1); return true;
        case KEY_PageDown:   this->movePage(1); return true;
        case KEY_Home:       this->moveToEnd(-1); return true;
        case KEY_End:        this->moveToEnd(1); return true;
        case KEY_R:          this->resetSelected(); return true;
        case KEY_Escape:     this->toggle(); return true;
        default:             break;
    }

    // Number keys apply presets in roster order; there are more presets than
    // digits, so Shift reaches the second bank of ten.
    int index = presetForKey(data.key);
    if (index >= 0) {
        if (data.modifiers & KeyboardModifier::Shift) index += 10;
        if (index < kFRTXPresetCount) {
            FRTXConfig::applyPreset(kFRTXPresets[index].preset);
            m_status = kFRTXPresets[index].label;
            this->refresh();
        }
        return true;
    }

    return false;
}

void FRTXTuner::registerListeners() {
    listenForKeybindSettingPresses("tuner-key",
        [](Keybind const&, bool down, bool repeat, double) {
            if (!down || repeat) return;
            if (auto tuner = FRTXTuner::get()) tuner->toggle();
        });

    // Swallowing the arrow keys while the panel is open is the whole point:
    // otherwise adjusting a slider also makes the player jump.
    KeyboardInputEvent().listen([](KeyboardInputData& data) -> bool {
        auto tuner = FRTXTuner::getIfExists();
        return tuner && tuner->handleKey(data);
    }).leak();

    // Applying a preset writes its values into the settings, so every slider
    // stays live afterwards.
    auto const onPreset = [](std::string_view button) -> bool {
        FRTXPreset preset;
        if (frtxPresetFromId(std::string(button).c_str(), preset)) {
            FRTXConfig::applyPreset(preset);
            if (auto tuner = FRTXTuner::getIfExists()) tuner->refresh();
        }
        return false;
    };
    ButtonSettingPressedEventV3(Mod::get(), "presets").listen(onPreset).leak();
    ButtonSettingPressedEventV3(Mod::get(), "style-presets").listen(onPreset).leak();
    ButtonSettingPressedEventV3(Mod::get(), "ray-presets").listen(onPreset).leak();
}
