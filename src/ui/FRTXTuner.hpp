#pragma once

#include <Geode/Geode.hpp>

// An in-game overlay for adjusting every setting while the game is running.
//
// A shader chain has too many interacting knobs to tune through a settings
// menu that you have to close before you can see the result. This draws a list
// over the running level and edits the settings live, so the picture changes
// under your hands.
//
// It deliberately lives outside the captured game layer, so the text stays
// crisp and ungraded no matter what the effect is doing.
class FRTXTuner : public cocos2d::CCNode {
public:
    static FRTXTuner* get();

    // Returns the tuner only if it has already been built. The keyboard
    // listener runs for every key in the game, and must not be what causes the
    // panel to be constructed -- building it needs the game's fonts, which are
    // not loaded during early startup.
    static FRTXTuner* getIfExists();

    // Registers the keybind and keyboard listeners. Call once at mod load.
    static void registerListeners();

    void toggle();
    bool isOpen() const { return m_open; }

    // Called once per frame from the game layer hook, so the panel follows
    // along when the scene changes underneath it.
    void tick();

private:
    static constexpr int kVisibleRows = 18;

    bool init() override;
    bool build();
    void refresh();
    void moveSelection(int delta);
    void adjust(int direction, geode::KeyboardModifier mods);
    void resetSelected();
    bool handleKey(geode::KeyboardInputData& data);

    // Index of the first row that is not a section header.
    int firstSelectable() const;
    bool selectable(int index) const;

    bool m_open = false;
    bool m_usable = false;
    int  m_selected = 0;
    int  m_scroll = 0;

    cocos2d::CCLayerColor* m_panel = nullptr;
    cocos2d::CCLayerColor* m_highlight = nullptr;
    cocos2d::CCLabelBMFont* m_title = nullptr;
    cocos2d::CCLabelBMFont* m_hint = nullptr;
    cocos2d::CCLabelBMFont* m_names[kVisibleRows] = {};
    cocos2d::CCLabelBMFont* m_values[kVisibleRows] = {};
};
