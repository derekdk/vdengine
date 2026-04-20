/**
 * @file main.cpp
 * @brief Text Adventure Demo — interactive text adventure pushing all text features.
 *
 * Phase 4 capstone demo demonstrating:
 * - Large TTF-rendered room title (TrueTypeFont + TextEntity)
 * - Pixel-font ASCII mini-map (BitmapFont::small())
 * - Scrolling narrative area with word-wrap (ring-buffer of TextEntity lines)
 * - Live command-prompt with blinking cursor
 * - Pixel-font status bar (inventory + HP)
 * - Keyboard input: letter keys, Enter, Backspace via onCharInput / onKeyPress
 */

#include <vde/api/GameAPI.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <deque>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../ExampleBase.h"

using namespace vde;

// ============================================================================
// Constants
// ============================================================================

// Use a 16:9 viewport to match the window aspect ratio exactly (no stretching)
static constexpr float VIEW_W = 16.0f;
static constexpr float VIEW_H = 9.0f;
static constexpr int NARRATIVE_LINES = 8;
static constexpr float TTF_SIZE = 32.0f;
static constexpr size_t WRAP_WIDTH = 38;
static constexpr float MAX_TEXT_W = 10.5f;  // max sprite width in world units

// ============================================================================
// World data
// ============================================================================

struct Room {
    std::string name;
    std::string description;
    // Exits: direction -> room index
    std::unordered_map<std::string, int> exits;
    // Items on the ground in this room
    std::vector<std::string> items;
    // Triggered event key (empty if none)
    std::string eventKey;
};

struct GameWorld {
    std::vector<Room> rooms;
    int currentRoom = 0;
    int hp = 20;
    int maxHp = 20;
    std::vector<std::string> inventory;
    std::unordered_set<std::string> triggeredEvents;

    void init() {
        rooms.resize(6);

        // Room 0: Entrance Hall
        rooms[0].name = "ENTRANCE HALL";
        rooms[0].description = "A grand stone hall stretches before you. "
                               "Torches flicker along the walls casting long shadows. "
                               "A heavy oak door leads NORTH and a narrow passage goes EAST.";
        rooms[0].exits = {{"NORTH", 1}, {"EAST", 2}};
        rooms[0].items = {"RUSTY KEY"};

        // Room 1: Library
        rooms[1].name = "LIBRARY";
        rooms[1].description = "Shelves of ancient books line every wall. "
                               "Dust motes drift lazily through a beam of pale light from above. "
                               "A leather-bound SCROLL rests on the reading desk. "
                               "Exits lead SOUTH and WEST.";
        rooms[1].exits = {{"SOUTH", 0}, {"WEST", 3}};
        rooms[1].items = {"SCROLL"};
        rooms[1].eventKey = "library_ghost";

        // Room 2: Armory
        rooms[2].name = "ARMORY";
        rooms[2].description = "Weapon racks stand in orderly rows, most of them empty. "
                               "A gleaming SWORD remains in one rack. "
                               "The passage continues WEST and a staircase climbs UP.";
        rooms[2].exits = {{"WEST", 0}, {"UP", 4}};
        rooms[2].items = {"SWORD"};

        // Room 3: Garden
        rooms[3].name = "OVERGROWN GARDEN";
        rooms[3].description = "Moonlight bathes a courtyard overtaken by vines and moss. "
                               "A stone fountain trickles softly in the center. "
                               "A HEALING HERB grows by the fountain. "
                               "Paths lead EAST and NORTH.";
        rooms[3].exits = {{"EAST", 1}, {"NORTH", 5}};
        rooms[3].items = {"HEALING HERB"};
        rooms[3].eventKey = "garden_fairy";

        // Room 4: Tower
        rooms[4].name = "TOWER CHAMBER";
        rooms[4].description = "A circular room at the top of a winding staircase. "
                               "A CRYSTAL ORB sits on a pedestal emanating a faint hum. "
                               "A locked chest stands in the corner. "
                               "The staircase descends DOWN.";
        rooms[4].exits = {{"DOWN", 2}};
        rooms[4].items = {"CRYSTAL ORB"};
        rooms[4].eventKey = "tower_trap";

        // Room 5: Dungeon
        rooms[5].name = "DUNGEON";
        rooms[5].description = "Cold damp stone surrounds you. Chains hang from the walls. "
                               "A GOLDEN AMULET gleams in the far corner. "
                               "An iron gate blocks the way NORTH but you can go SOUTH. "
                               "A TORCH rests in a bracket on the wall.";
        rooms[5].exits = {{"SOUTH", 3}};
        rooms[5].items = {"GOLDEN AMULET", "TORCH"};
        rooms[5].eventKey = "dungeon_escape";
    }

    const Room& current() const { return rooms[currentRoom]; }

    bool hasItem(const std::string& item) const {
        return std::find(inventory.begin(), inventory.end(), item) != inventory.end();
    }
};

// ============================================================================
// Mini-map builder
// ============================================================================

static std::string buildMiniMap(const GameWorld& world) {
    // 3x3 grid layout:
    //   [3] [1] [ ]
    //   [0] [2] [4]
    //   [ ] [5] [ ]
    // Positions for each room id on a 5x3 grid
    struct Pos {
        int col, row;
    };
    Pos positions[6] = {
        {0, 1},  // Room 0 - Entrance Hall
        {1, 0},  // Room 1 - Library
        {1, 1},  // Room 2 - Armory
        {0, 0},  // Room 3 - Garden
        {2, 1},  // Room 4 - Tower
        {1, 2},  // Room 5 - Dungeon
    };

    // Build 3-row, 3-col grid
    char grid[3][3];
    for (auto& row : grid)
        for (auto& c : row)
            c = '.';

    for (int i = 0; i < 6; ++i) {
        auto& p = positions[i];
        if (i == world.currentRoom)
            grid[p.row][p.col] = '@';
        else
            grid[p.row][p.col] = '#';
    }

    std::string result;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            result += grid[r][c];
            if (c < 2)
                result += ' ';
        }
        if (r < 2)
            result += '/';
    }
    return result;
}

// ============================================================================
// Word-wrap helper
// ============================================================================

static std::vector<std::string> wordWrap(const std::string& text, size_t maxChars) {
    std::vector<std::string> lines;
    if (text.empty()) {
        lines.push_back("");
        return lines;
    }

    std::istringstream stream(text);
    std::string word;
    std::string currentLine;

    while (stream >> word) {
        if (currentLine.empty()) {
            currentLine = word;
        } else if (currentLine.size() + 1 + word.size() <= maxChars) {
            currentLine += " " + word;
        } else {
            lines.push_back(currentLine);
            currentLine = word;
        }
    }
    if (!currentLine.empty())
        lines.push_back(currentLine);

    return lines;
}

// ============================================================================
// Input handler with character input
// ============================================================================

class AdventureInputHandler : public vde::examples::BaseExampleInputHandler {
  public:
    void onCharInput(unsigned int codepoint) override {
        // Accept printable ASCII
        if (codepoint >= 32 && codepoint < 127) {
            m_charBuffer += static_cast<char>(codepoint);
        }
    }

    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);

        if (key == vde::KEY_ENTER) {
            m_enterPressed = true;
        }
        if (key == vde::KEY_BACKSPACE) {
            m_backspacePressed = true;
        }
    }

    // Drain accumulated character input
    std::string drainChars() {
        std::string result = m_charBuffer;
        m_charBuffer.clear();
        return result;
    }

    bool consumeEnter() {
        bool v = m_enterPressed;
        m_enterPressed = false;
        return v;
    }

    bool consumeBackspace() {
        bool v = m_backspacePressed;
        m_backspacePressed = false;
        return v;
    }

  private:
    std::string m_charBuffer;
    bool m_enterPressed = false;
    bool m_backspacePressed = false;
};

// ============================================================================
// Scene
// ============================================================================

class AdventureScene : public vde::examples::BaseExampleScene {
  public:
    AdventureScene() : BaseExampleScene() {}

    void onEnter() override {
        printExampleHeader();

        setup2D(VIEW_W, VIEW_H, Color(0.02f, 0.02f, 0.06f, 1.0f));

        m_world.init();

        // Margins
        const float LEFT = -VIEW_W * 0.5f + 0.3f;  // -7.7
        const float RIGHT = VIEW_W * 0.5f - 0.3f;  // 7.7
        const float TOP = VIEW_H * 0.5f;           // 4.5
        const float BOT = -VIEW_H * 0.5f;          // -4.5

        // Get VulkanContext for TrueTypeFont loading
        auto* ctx = getGame()->getVulkanContext();

        // Load TTF font for room title
        m_ttfFont = std::make_unique<TrueTypeFont>();
        if (!m_ttfFont->loadFromFile(ctx, "assets/fonts/VDE_default.ttf", TTF_SIZE)) {
            std::cerr << "WARNING: TTF font load failed, will use bitmap font\n";
            m_ttfFont.reset();
        }

        // ---- Room Title (top center) ----
        m_roomTitle = addEntity<TextEntity>();
        if (m_ttfFont) {
            m_roomTitle->setTrueTypeFont(m_ttfFont.get());
        } else {
            m_roomTitle->setFont(BitmapFont::large());
        }
        m_roomTitle->setStyle({.color = Color::yellow(), .pixelScale = 2, .letterSpacing = 1});
        m_roomTitle->setPosition(0.0f, TOP - 0.5f, 0.1f);

        // ---- Mini-map (top-right corner) ----
        m_miniMapLabel = addEntity<TextEntity>();
        m_miniMapLabel->setFont(BitmapFont::small());
        m_miniMapLabel->setStyle({.color = Color::cyan(), .pixelScale = 1, .letterSpacing = 1});
        m_miniMapLabel->setAnchor(1.0f, 0.5f);
        m_miniMapLabel->setPosition(RIGHT, TOP - 0.3f, 0.1f);
        m_miniMapLabel->setText("MAP");
        m_miniMapLabel->setWorldHeight(0.25f);

        m_miniMap = addEntity<TextEntity>();
        m_miniMap->setFont(BitmapFont::small());
        m_miniMap->setStyle({.color = Color::green(), .pixelScale = 2, .letterSpacing = 1});
        m_miniMap->setAnchor(1.0f, 0.5f);
        m_miniMap->setPosition(RIGHT, TOP - 0.7f, 0.1f);

        // ---- Narrative area (left column, 8 lines) ----
        const float NARR_TOP = TOP - 1.3f;  // 3.2
        const float NARR_SPACING = 0.50f;
        const float NARR_HEIGHT = 0.30f;

        for (int i = 0; i < NARRATIVE_LINES; ++i) {
            m_narrativeEntities[i] = addEntity<TextEntity>();
            m_narrativeEntities[i]->setFont(BitmapFont::small());
            m_narrativeEntities[i]->setStyle(
                {.color = Color::white(), .pixelScale = 1, .letterSpacing = 1});
            m_narrativeEntities[i]->setAnchor(0.0f, 0.5f);
            float y = NARR_TOP - i * NARR_SPACING;
            m_narrativeEntities[i]->setPosition(LEFT, y, 0.0f);
            m_narrativeEntities[i]->setText(" ");
            m_narrativeEntities[i]->setWorldHeight(NARR_HEIGHT);
            m_narrativeEntities[i]->setMaxWidth(MAX_TEXT_W);
        }

        // ---- Command prompt (below narrative) ----
        const float PROMPT_Y = BOT + 1.4f;  // -3.1

        m_promptLabel = addEntity<TextEntity>();
        m_promptLabel->setFont(BitmapFont::small());
        m_promptLabel->setStyle({.color = Color::cyan(), .pixelScale = 1, .letterSpacing = 1});
        m_promptLabel->setAnchor(0.0f, 0.5f);
        m_promptLabel->setPosition(LEFT, PROMPT_Y, 0.0f);
        m_promptLabel->setText("> ");
        m_promptLabel->setWorldHeight(0.30f);

        m_promptText = addEntity<TextEntity>();
        m_promptText->setFont(BitmapFont::small());
        m_promptText->setStyle({.color = Color::white(), .pixelScale = 1, .letterSpacing = 1});
        m_promptText->setAnchor(0.0f, 0.5f);
        m_promptText->setPosition(LEFT + 0.7f, PROMPT_Y, 0.0f);
        m_promptText->setText("_");
        m_promptText->setWorldHeight(0.30f);
        m_promptText->setMaxWidth(MAX_TEXT_W);

        // ---- Status bar (bottom row) ----
        const float STATUS_Y = BOT + 0.5f;  // -4.0

        m_statusInventory = addEntity<TextEntity>();
        m_statusInventory->setFont(BitmapFont::small());
        m_statusInventory->setStyle(
            {.color = Color::yellow(), .pixelScale = 1, .letterSpacing = 1});
        m_statusInventory->setAnchor(0.0f, 0.5f);
        m_statusInventory->setPosition(LEFT, STATUS_Y, 0.0f);
        m_statusInventory->setWorldHeight(0.25f);
        m_statusInventory->setMaxWidth(8.0f);

        m_statusHp = addEntity<TextEntity>();
        m_statusHp->setFont(BitmapFont::small());
        m_statusHp->setStyle({.color = Color::green(), .pixelScale = 1, .letterSpacing = 1});
        m_statusHp->setAnchor(1.0f, 0.5f);
        m_statusHp->setPosition(RIGHT, STATUS_Y, 0.0f);
        m_statusHp->setWorldHeight(0.25f);

        // Initial room display
        enterRoom();
    }

    void update(float deltaTime) override {
        BaseExampleScene::update(deltaTime);
        m_time += deltaTime;

        // Handle character input
        auto* input = dynamic_cast<AdventureInputHandler*>(getInputHandler());
        if (!input)
            return;

        // Append typed characters
        std::string chars = input->drainChars();
        if (!chars.empty()) {
            m_commandBuffer += chars;
            m_promptDirty = true;
        }

        // Backspace
        if (input->consumeBackspace() && !m_commandBuffer.empty()) {
            m_commandBuffer.pop_back();
            m_promptDirty = true;
        }

        // Enter -> dispatch command
        if (input->consumeEnter() && !m_commandBuffer.empty()) {
            // Convert to uppercase for matching
            std::string cmd = m_commandBuffer;
            for (auto& c : cmd)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            m_commandBuffer.clear();
            m_promptDirty = true;
            processCommand(cmd);
        }

        // Blinking cursor (toggle every 0.5s)
        m_cursorAccum += deltaTime;
        if (m_cursorAccum >= 0.5f) {
            m_cursorAccum -= 0.5f;
            m_cursorVisible = !m_cursorVisible;
            m_promptDirty = true;
        }

        // Update prompt display if dirty
        if (m_promptDirty) {
            m_promptDirty = false;
            std::string display = m_commandBuffer;
            if (m_cursorVisible)
                display += "_";
            if (display.empty())
                display = " ";
            m_promptText->setText(display);
        }
    }

  protected:
    std::string getExampleName() const override { return "Text Adventure"; }

    std::vector<std::string> getFeatures() const override {
        return {"TTF room title at top with TrueTypeFont + TextEntity",
                "Pixel-font ASCII mini-map in top-right corner",
                "Scrolling narrative with word-wrap (8 lines)",
                "Live command prompt with blinking cursor",
                "Status bar with inventory and HP",
                "6 rooms, 8 items, 4 triggered events",
                "All text rendering features composed together"};
    }

    std::vector<std::string> getExpectedVisuals() const override {
        return {"Yellow room title at top center",
                "Green ASCII mini-map in top-right (@ = current room)",
                "White narrative text lines filling center area",
                "Cyan > prompt with blinking cursor at bottom",
                "Yellow inventory and green HP in status bar"};
    }

    std::vector<std::string> getControls() const override {
        return {"Type letters to build a command", "ENTER   - Execute command",
                "BACKSPACE - Delete last character",
                "Commands: GO <dir>, TAKE <item>, LOOK, INVENTORY, USE <item>"};
    }

  private:
    GameWorld m_world;
    std::unique_ptr<TrueTypeFont> m_ttfFont;
    float m_time = 0.0f;

    // UI entities
    std::shared_ptr<TextEntity> m_roomTitle;
    std::shared_ptr<TextEntity> m_miniMapLabel;
    std::shared_ptr<TextEntity> m_miniMap;
    std::shared_ptr<TextEntity> m_narrativeEntities[NARRATIVE_LINES];
    std::shared_ptr<TextEntity> m_promptLabel;
    std::shared_ptr<TextEntity> m_promptText;
    std::shared_ptr<TextEntity> m_statusInventory;
    std::shared_ptr<TextEntity> m_statusHp;

    // Narrative ring buffer
    std::deque<std::string> m_narrativeLines;

    // Command input state
    std::string m_commandBuffer;
    bool m_promptDirty = true;
    float m_cursorAccum = 0.0f;
    bool m_cursorVisible = true;

    // ---- Room entry ----
    void enterRoom() {
        const auto& room = m_world.current();

        // Update title
        m_roomTitle->setText(room.name);
        m_roomTitle->setWorldHeight(0.50f);
        m_roomTitle->setMaxWidth(8.0f);

        // Update mini-map
        std::string mapStr = buildMiniMap(m_world);
        m_miniMap->setText(mapStr);
        m_miniMap->setWorldHeight(0.35f);
        m_miniMap->setMaxWidth(3.0f);

        // Show room description with word-wrap
        addNarrative("");
        auto wrapped = wordWrap(room.description, WRAP_WIDTH);
        for (const auto& line : wrapped)
            addNarrative(line);

        // List visible items
        if (!room.items.empty()) {
            std::string itemLine = "You see:";
            for (const auto& item : room.items)
                itemLine += " " + item + ",";
            itemLine.pop_back();  // remove trailing comma
            addNarrative(itemLine);
        }

        // List exits
        std::string exitLine = "Exits:";
        for (const auto& [dir, _] : room.exits)
            exitLine += " " + dir;
        addNarrative(exitLine);

        // Trigger room event
        if (!room.eventKey.empty() &&
            m_world.triggeredEvents.find(room.eventKey) == m_world.triggeredEvents.end()) {
            triggerEvent(room.eventKey);
        }

        // Update status bar
        refreshStatusBar();
    }

    // ---- Narrative management ----
    void addNarrative(const std::string& line) {
        m_narrativeLines.push_front(line.empty() ? " " : line);
        while (static_cast<int>(m_narrativeLines.size()) > NARRATIVE_LINES)
            m_narrativeLines.pop_back();

        for (int i = 0; i < NARRATIVE_LINES; ++i) {
            if (i < static_cast<int>(m_narrativeLines.size())) {
                m_narrativeEntities[i]->setText(m_narrativeLines[i]);
            }
        }
    }

    // ---- Status bar ----
    void refreshStatusBar() {
        // Inventory
        std::string inv = "INV:";
        if (m_world.inventory.empty()) {
            inv += " (empty)";
        } else {
            for (const auto& item : m_world.inventory)
                inv += " " + item + ",";
            inv.pop_back();
        }
        m_statusInventory->setText(inv);

        // HP
        std::string hpStr =
            "HP: " + std::to_string(m_world.hp) + "/" + std::to_string(m_world.maxHp);
        Color hpColor = Color::green();
        if (m_world.hp <= m_world.maxHp / 4)
            hpColor = Color::red();
        else if (m_world.hp <= m_world.maxHp / 2)
            hpColor = Color::yellow();
        m_statusHp->setText(hpStr);
        m_statusHp->setStyle({.color = hpColor, .pixelScale = 1, .letterSpacing = 1});
    }

    // ---- Command processing ----
    void processCommand(const std::string& cmd) {
        addNarrative("> " + cmd);

        if (cmd == "LOOK") {
            cmdLook();
        } else if (cmd == "INVENTORY" || cmd == "INV" || cmd == "I") {
            cmdInventory();
        } else if (cmd.substr(0, 3) == "GO ") {
            cmdGo(cmd.substr(3));
        } else if (cmd.substr(0, 5) == "TAKE ") {
            cmdTake(cmd.substr(5));
        } else if (cmd.substr(0, 4) == "USE ") {
            cmdUse(cmd.substr(4));
        } else if (cmd == "HELP") {
            addNarrative("Commands: GO <dir>, TAKE <item>,");
            addNarrative("  LOOK, INVENTORY, USE <item>, HELP");
        } else {
            addNarrative("I don't understand that command.");
            addNarrative("Type HELP for a list of commands.");
        }
    }

    void cmdLook() {
        const auto& room = m_world.current();
        auto wrapped = wordWrap(room.description, 52);
        for (const auto& line : wrapped)
            addNarrative(line);

        if (!room.items.empty()) {
            std::string itemLine = "You see:";
            for (const auto& item : room.items)
                itemLine += " " + item + ",";
            itemLine.pop_back();
            addNarrative(itemLine);
        }
    }

    void cmdInventory() {
        if (m_world.inventory.empty()) {
            addNarrative("You are carrying nothing.");
        } else {
            addNarrative("You are carrying:");
            for (const auto& item : m_world.inventory)
                addNarrative("  " + item);
        }
    }

    void cmdGo(const std::string& direction) {
        const auto& room = m_world.current();
        auto it = room.exits.find(direction);
        if (it == room.exits.end()) {
            addNarrative("You can't go " + direction + " from here.");
            return;
        }
        m_world.currentRoom = it->second;
        enterRoom();
    }

    void cmdTake(const std::string& itemName) {
        auto& room = m_world.rooms[m_world.currentRoom];
        auto it = std::find(room.items.begin(), room.items.end(), itemName);
        if (it == room.items.end()) {
            addNarrative("There is no " + itemName + " here.");
            return;
        }
        m_world.inventory.push_back(*it);
        room.items.erase(it);
        addNarrative("You pick up the " + itemName + ".");
        refreshStatusBar();
    }

    void cmdUse(const std::string& itemName) {
        if (!m_world.hasItem(itemName)) {
            addNarrative("You don't have a " + itemName + ".");
            return;
        }

        if (itemName == "HEALING HERB") {
            int healed = std::min(8, m_world.maxHp - m_world.hp);
            m_world.hp += healed;
            removeItem(itemName);
            addNarrative("You eat the HEALING HERB and recover " + std::to_string(healed) + " HP.");
            refreshStatusBar();
        } else if (itemName == "RUSTY KEY" && m_world.currentRoom == 4) {
            addNarrative("You unlock the chest with the RUSTY KEY!");
            addNarrative("Inside you find a SILVER RING.");
            removeItem(itemName);
            m_world.inventory.push_back("SILVER RING");
            refreshStatusBar();
        } else if (itemName == "TORCH" && m_world.currentRoom == 5) {
            addNarrative("The torch illuminates hidden runes on the wall.");
            addNarrative("They read: THE ORB REVEALS THE PATH.");
        } else if (itemName == "CRYSTAL ORB") {
            addNarrative("The orb glows softly and shows a");
            addNarrative("vision of treasure beyond the gate.");
        } else {
            addNarrative("You can't use the " + itemName + " here.");
        }
    }

    void removeItem(const std::string& itemName) {
        auto it = std::find(m_world.inventory.begin(), m_world.inventory.end(), itemName);
        if (it != m_world.inventory.end())
            m_world.inventory.erase(it);
    }

    // ---- Events ----
    void triggerEvent(const std::string& key) {
        m_world.triggeredEvents.insert(key);

        if (key == "library_ghost") {
            addNarrative("A spectral figure appears among the shelves!");
            addNarrative("It whispers: 'Seek the key before the tower...'");
            addNarrative("The ghost fades into the dust.");
        } else if (key == "garden_fairy") {
            addNarrative("A tiny light dances above the fountain.");
            addNarrative("A voice says: 'The herb heals all wounds.'");
            m_world.hp = std::max(m_world.hp - 3, 1);
            addNarrative("A chill runs through you. (-3 HP)");
            refreshStatusBar();
        } else if (key == "tower_trap") {
            addNarrative("CLICK! A dart shoots from the wall!");
            m_world.hp = std::max(m_world.hp - 5, 1);
            addNarrative("You are hit! (-5 HP)");
            if (m_world.hasItem("SWORD")) {
                addNarrative("Your SWORD deflects a second dart.");
            } else {
                m_world.hp = std::max(m_world.hp - 3, 1);
                addNarrative("Another dart hits you! (-3 HP)");
            }
            refreshStatusBar();
        } else if (key == "dungeon_escape") {
            addNarrative("The iron gate slams shut behind you!");
            addNarrative("You hear a deep growl in the darkness...");
            if (m_world.hasItem("TORCH")) {
                addNarrative("Your TORCH drives the creature back.");
            } else {
                m_world.hp = std::max(m_world.hp - 4, 1);
                addNarrative("Something scratches you! (-4 HP)");
                refreshStatusBar();
            }
        }
    }
};

// ============================================================================
// Game
// ============================================================================

class AdventureGame : public vde::examples::BaseExampleGame<AdventureInputHandler, AdventureScene> {
  public:
    AdventureGame() = default;
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    AdventureGame game;
    return vde::examples::runExample(game, "VDE Text Adventure Demo", 1280, 720, argc, argv);
}
