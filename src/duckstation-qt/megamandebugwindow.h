// SPDX-FileCopyrightText: 2019-2022 Connor McLaughlin <stenzek@gmail.com>
// SPDX-License-Identifier: (GPL-3.0 OR CC-BY-NC-ND-4.0)

#pragma once
#include "core/types.h"
#include "ui_megamandebugwindow.h"
#include <QtWidgets/QMainWindow>
#include <memory>
#include <optional>

namespace Bus {
enum class MemoryRegion;
}

class DebuggerCodeModel;
class DebuggerRegistersModel;
class DebuggerStackModel;

class MegaManDebugWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MegaManDebugWindow(QWidget* parent = nullptr);
  ~MegaManDebugWindow();

Q_SIGNALS:
  void closed();

public Q_SLOTS:
  void onEmulationPaused();
  void onEmulationResumed();
  void onSetHealthButtonPressed();
  void onNormalItemAddButtonPressed();
  void onNormalItemRemoveButtonPressed();
  void onEnergyCanteenChecked(bool checked);
  void onBlueRefractorChecked(bool checked);
  void onWalkieTalkieChecked(bool checked);
  void onHelmetChecked(bool checked);
  void onYellowRefractorChecked(bool checked);
  void onBonneKeyChecked(bool checked);
  void onJetSkatesChecked(bool checked);
  void onRedRefractorChecked(bool checked);
  void onCardonKeysChecked(int index);
  void onJumpSpringsChecked(bool checked);
  void onCitizensCardChecked(bool checked);
  void onLakeJyunKeysChecked(int index);
  void onArmorJacketChecked(int index);
  void onClassALicenseChecked(bool checked);
  void onClozerWoodsKeysChecked(int index);
  void onAdapterPlugChecked(bool checked);
  void onClassBLicenseChecked(bool checked);
  void onSubCityKeysChecked(int index);
  void onUpdateCanteenPressed();

  void onEquipHelmetChecked(bool checked);
  void onEquipJetSkatesChecked(bool checked);
  void onEquipJumpSpringsChecked(bool checked);
  void onEquipArmorChecked(bool checked);

  void onBusterPartEquippedSlot1Changed(int index);
  void onBusterPartEquippedSlot2Changed(int index);
  void onBusterPartEquippedSlot3Changed(int index);

  void onBusterPartInventorySlot0Changed(int index);
  void onBusterPartInventorySlot1Changed(int index);
  void onBusterPartInventorySlot2Changed(int index);
  void onBusterPartInventorySlot3Changed(int index);
  void onBusterPartInventorySlot4Changed(int index);
  void onBusterPartInventorySlot5Changed(int index);
  void onBusterPartInventorySlot6Changed(int index);
  void onBusterPartInventorySlot7Changed(int index);
  void onBusterPartInventorySlot8Changed(int index);
  void onBusterPartInventorySlot9Changed(int index);
  void onBusterPartInventorySlot10Changed(int index);
  void onBusterPartInventorySlot11Changed(int index);
  void onBusterPartInventorySlot12Changed(int index);
  void onBusterPartInventorySlot13Changed(int index);
  void onBusterPartInventorySlot14Changed(int index);
  void onBusterPartInventorySlot15Changed(int index);
  void onBusterPartInventorySlot16Changed(int index);
  void onBusterPartInventorySlot17Changed(int index);
  void onBusterPartInventorySlot18Changed(int index);
  void onBusterPartInventorySlot19Changed(int index);
  void onBusterPartInventorySlot20Changed(int index);
  void onBusterPartInventorySlot21Changed(int index);
  void onBusterPartInventorySlot22Changed(int index);
  void onBusterPartInventorySlot23Changed(int index);
  void onBusterPartInventorySlot24Changed(int index);
  void onBusterPartInventorySlot25Changed(int index);
  void onBusterPartInventorySlot26Changed(int index);
  void onBusterPartInventorySlot27Changed(int index);
  void onBusterPartInventorySlot28Changed(int index);
  void onBusterPartInventorySlot29Changed(int index);
  void onBusterPartInventorySlot30Changed(int index);
  void onBusterPartInventorySlot31Changed(int index);
  void onBusterPartManuallyChangeValuesChanged(bool value);

  void onBusterPartAttackValueChanged(const QString& value);
  void onBusterPartEnergyValueChanged(const QString& value);
  void onBusterPartRangeValueChanged(const QString& value);
  void onBusterPartRapidValueChanged(const QString& value);

  void onSpecialWeaponsSlot0Changed(bool value);
  void onSpecialWeaponsSlot1Changed(bool value);
  void onSpecialWeaponsSlot2Changed(bool value);
  void onSpecialWeaponsSlot3Changed(bool value);
  void onSpecialWeaponsSlot4Changed(bool value);
  void onSpecialWeaponsSlot5Changed(bool value);
  void onSpecialWeaponsSlot6Changed(bool value);
  void onSpecialWeaponsSlot7Changed(bool value);
  void onSpecialWeaponsSlot8Changed(bool value);
  void onSpecialWeaponsSlot9Changed(bool value);
  void onSpecialWeaponsSlot10Changed(bool value);
  void onSpecialWeaponsSlot11Changed(bool value);
  void onSpecialWeaponsSlot12Changed(bool value);
  void onSpecialWeaponsSlot13Changed(bool value);
  void onSpecialWeaponsSlot14Changed(bool value);
  void onSpecialWeaponsSlot15Changed(bool value);

protected:
  void closeEvent(QCloseEvent* event);

private Q_SLOTS:
  void onDebuggerMessageReported(const QString& message);
  void onPauseActionToggled(bool paused);

private:
  void setupAdditionalUi();
  void connectSignals();
  void disconnectSignals();
  void createModels();
  void setUIEnabled(bool enabled);
  void updateNormalItems();
  void updateSpecialItems();
  void flipBitSpecialItem(int offset, bool value);
  void initBusterPartsTab();
  void updateBusterParts();

  void initSpecialWeaponsTab();
  void updateSpecialWeapons();
  void flipBitSpecialWeapon(int offset, bool value);


  Ui::MegaManDebugWindow m_ui;

  // Menu related memory addresses
  int normalItemsAddress;
  int specialItemsAddress;
  int canteenAddress;
  int helmetByteAddess;
  int skatesAndSpringsAddress;
  int armorAddress;
  int equippedBusterSlotAddress;
  int inventoryBusterSlotAddress;
  int actualBusterValuesAddress;
  int busterTable;
  int specialWeaponHaveAddress;

  // Game related memory addresses
  int currentGameAreaAddress;
  int inGameTimerAddress;
  int clearCountAddress;
  int storyFlag1Address; // 2 bytes

  QComboBox* equippedSlots[3];
  QComboBox* inventorySlots[32];

  QCheckBox* specialWeapons[16];

  const char* specialWeaponNames[16] = { // big endian order
    "Active Buster", // 0
    "Blade Arm",
    "Grand Grenade",
    "Splash Mine",
    "Shield Arm",
    "Shining Laser",
    "(unused)",
    "(unused)",
    "Normal Arm",
    "(unused)",
    "Machine Buster",
    "Powered Buster",
    "Drill Arm",
    "Grenade Arm",
    "Spread Buster",
    "Vacuum Arm",
  };

  const char* busterPartNames[256] = {
      "", // 0
      "Blaster Unit Omega",
      "Sniper Unit Omega",
      "Laser",
      "Sniper Range",
      "Turbo Battery",
      "Power Raiser Omega",
      "Range Booster Omega",
      "Turbo Charger Omega", // 8
      "Blast Unit",
      "Sniper Unit",
      "Power Raiser Alpha",
      "Range Booster Alpha",
      "Turbo Charger Alpha",
      "Power Raiser",
      "Range Booster",
      "Turbo Charger",  // 16
      "Buster Max",
      "Power Stream",
      "Blaster Unit R",
      "Buster Unit Omega",
      "Omni-Unit Omega",
      "Auto Battery",
      "Sniper Scope",
      "Rapid Striker", // 24
      "Gatling Gun",
      "Omni-Unit",
      "Power Blaster R",
      "Power Blaster L",
      "Machine Gun",
      "Triple Access",
      "Buster Unit",

      "Rapid Fire", // 32
      "Helmet",
      "Jump Springs",
      "Jet Skates",
      "Life Guage",
      "Energy Canteen",
      "Extra Pack",
      "Adapter Plug",
      "Refractor", // 40
      "Refractor",
      "Refractor",
      "Citizen's Card",
      "Class A License",
      "Class B License",
      "Bonne Key",
      "Starter Key",

      "Starter Key", // 48
      "Starter Key",
      "Starter Key",
      "Starter Key",
      "Starter Key",
      "ID Card",
      "ID Card",
      "ID Card",
      "'Watcher' Key",
      "'Sleeper' Key",
      "'Dreamer' Key",
      "Flak Jacket",
      "Kevlar Jacket",
      "Kevlar Jacket Omega",
      "Walkie-Talkie",
      "KEY-ITEM 1 E",

      // 64
      "",
      "Shield Repair",
      "Hyper Cartridge",
      "Chameleon Net",
      "Defense Shield",
      "Flower",
      "Bag",
      "Trunk",
      "Pick",
      "Saw",
      "Lipstick",
      "Music Box",
      "Old Bone",
      "Old Heater",
      "Old Doll",
      "Antique Bell",
      "Giant Horn",
      "Shiny Object",
      "Old Shield",
      "Shiny Red Stone",
      "Stag Beetle",
      "Beetle",
      "Comic Book",
      "Ring",
      "EMPTY",
      "Mine Parts Kit",
      "Cannon Kit",
      "Grenade Kit",
      "Blumebear Parts",
      "Mystic Orb",
      "Marlwolf Shell",
      "Broken Motor",

      // 96
      "Broken Propeller",
      "Broken Cleaner",
      "Bomb Schematic",
      "Blunted Drill",
      "Guidance Unit",
      "Zetsabre",
      "Pen Light",
      "Old Launcher",
      "Ancient Book",
      "Arm Supporter",
      "X Buster",
      "Weapon Plans",
      "Prism Crystal",
      "Spring Set",
      "Safety Helmet",
      "Rollerboard",
      "Old Hoverjets",
      "Joint Plug",
      "Broken Circuits",
      "Main Core Shard",
      "Sun-light",
      "Rapidfire Barrel",
      "Plastique",
      "Bomb",
      "Gatling Part",
      "Flower Pearl",
      "Autofire Barrel",
      "Generator Part",
      "Target Sensor",
      "Tele-lens",
      "JUNK-ITEM 25",
      "JUNK-ITEM 26",

      // 128 (past this point there's only invalid data)
      "", // CAUSES CRASH IN SLOT 0, Exception DBE
      "", // Endless glitch text
      "", // CAUSES CRASH IN SLOT 0, Exception ADEL
      "", // CAUSES CRASH IN SLOT 0, Exception DBE
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",

      // 160
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",

      // 192
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",

      // 224
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",

      // 256
  };

  const char* normalItemNames[64] = {
    "Mine Parts Kit",
    "Cannon Kit",
    "Grenade Kit",
    "Blumebear Parts",

    "Mystic Orb",
    "Marlwolf Shell",
    "Broken Motor",
    "Broken Propeller",

    "Shiny Object",
    "Old Shield",
    "Shiny Red Stone",
    "Stag Beetle",

    "Beetle",
    "Comic Book",
    "Ring",
    "EMPTY",

    "Saw",
    "Lipstick",
    "Music Box",
    "Old Bone",

    "Old Heater",
    "Old Doll",
    "Antique Bell",
    "Giant Horn",

    "Shield Repair",
    "Hyper Cartridge",
    "Chameleon Net",
    "Defense Shield",

    "Flower",
    "Bag",
    "Trunk",
    "Pick",

    "Flower Pearl",
    "Autofire Barrel",
    "Generator Part",
    "Target Sensor",

    "Tele-lens",
    "JUNK-ITEM 25",
    "JUNK-ITEM 26",
    "JUNK-ITEM 27",

    "Joint Plug",
    "Broken Circuits",
    "Main Core Shard",
    "Sun-light",

    "Rapidfire Barrel",
    "Plastique",
    "Bomb",
    "Gatling Part",

    "Arm Supporter",
    "X Buster",
    "Weapon Plans",
    "Prism Crystal",

    "Spring Set",
    "Safety Helmet",
    "Rollerboard",
    "Old Hoverjets",

    "Broken Cleaner",
    "Bomb Schematic",
    "Blunted Drill",
    "Guidance Unit",

    "Zetsabre",
    "Pen Light",
    "Old Launcher",
    "Ancient Book",
  };

  const char* roomNames[32] = {
    "Starter Ruin",
    "Opening Text Cutscene",
    "Crash Cutscene",
    "Cardon Forest",
    "Apple Market",
    "Downtown",
    "City Hall 1",
    "Gesselschaft Interior",
    "Uptown",
    "Ruin Network",
    "Clozer Woods (Tiesel)",
    "Lake Jyun",
    "Outside Cardon Forest Subgate",
    "Boat Shop",
    "Cardon Subgate",
    "City Hall 2",
    "Yass Plains",
    "Clozer Woods With Bridge",
    "Outside Main Gate",
    "Clozer Woods Subgate",
    "Lake Jyun Subgate",
    "Bonne Ending Boat",
    "Flutter To Subgate Cutscene",
    "Gesselschaft Battle",
    "Flutter Takeoff",
    "Old City",
    "Main Gate",
    "Fixed Flutter",
    "Museum",
    "Sub-Cities",
    "Ending",
  };

  const char* roomSubNames[31][17] = {
    {"Intro cutscene", "First Area", "Second Area", "Boss Room", "Outdoor Cutscene", "Back To Title Screen", "Empty?",
     "Freezes"},
    {"Scrolling Text"},
                                      {"Cutscene"},
    {"Cardon Forest (near ruin entrance)", "Crash Site", "City Entrance", "Unknown (floating model 1)",
     "Unknown (floating model 2)", "Barrell's Room", "Roll's Room (no Roll)", "Flutter", "Mega Man's Room"},
    {"Apple Market", "Junk Shop", "Electronics Shop", "Clothing Shop", "Clothing Shop 2", "Music Shop"},
    {"Downtown", "Downtown (no music, no sub-city, missing door texture)", "Library", "Tron's Cockpit", "Stripe Burger"},
    {"Empty?", "Engine Room", "Hallway", "Kitchen", "Main Deck", "First Meeting Room Cutscene", "Meeting Room", "Empty?"},
    {"Uptown", "Hospital", "TV Station", "Beast Hunter Game", "Balloon Game", "Ira's Room"},
    {"??", "??", "??"},
    {"Tiesel Area"},
    {"Inside Bonne Robot", "???", "On the Lake", "Side River", "Inside Bonne Robot", "Empty?", "Door Opening Mechanism",
     "Subgate with Open Door"},
    {"Outside", "Door Opening Mechanism", "Subgate with Open Door", "Small Piece of Ground"},
    {"Walkway", "Inside", "Boat Area", "Inside the Boat", "Walkway (no box or woman)", "Walkway (no box, woman, or boat)"},
    {"??"},
    {"Police Station", "Inspector's Office", "City Hall 1st Floor", "Bank"},
    {"Yass Plains Outdoors", "House Stage 1", "House Stage 2", "House Stage 3", "Empty House", "Junk Shop"},
    {"??"},
    {"??"},
    {
      "??",
      "??",
      "Key Room",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
    },
    {
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
      "??",
    },
    {"Boat Interior", "Ocean",

},
    {"Cutscene", "Flutter Bridge", "Return Cutscene",

},
    {" Small Bridge", "Flutter Bridge", "Gesselschaft HQ", "Scrolling Clouds"},
    {"Cardon Forest with no Flutter (Yasmar Woods)", "Flutter Engine Room", "Glowing Refractor"},
    {"Old City (no dogs, weapons usable)", "Bonne's Warehouse", "Power Plant", "Old City (dogs, no weapons)"},
      {"Second Area", "Third Area", "Final Area", "First Area 1", "First Area 2", "First Area 3", "First Area 4", "First Area 5", "First Area 6", "First Area 7", "Tiny Kattelox", "Juno Wall", "Amelia's Office", "Downtown",
      "Empty?", "Juno's Room Without Door", "Unknown (textures?)"},
    {"Glitched Cardon Forest", "Cardon Foreset with Flutter in Air", "Glitched Cardon Forest",
     "Unknown (floating model)", "Unknown", "Barell's Room", "Roll's Room", "Flutter", "Mega Man's Room"},
    {"First Floor", "Second Floor"},
    {"??", "??", "??", "??", "??", "??"},
    {"Flutter Launch Area (different)", "Flutter Bridge", "Cliff and Ocean", "Empty?", "Empty?", "Tiny Kattelox"}
  };
};
