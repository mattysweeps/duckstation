// SPDX-FileCopyrightText: 2019-2022 Connor McLaughlin <stenzek@gmail.com>
// SPDX-License-Identifier: (GPL-3.0 OR CC-BY-NC-ND-4.0)

#include "megamandebugwindow.h"
#include "common/assert.h"
#include "core/cpu_core_private.h"
#include "debuggermodels.h"
#include "qthost.h"
#include "qtutils.h"
#include <QtCore/QSignalBlocker>
#include <QtGui/QFontDatabase>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <string>

MegaManDebugWindow::MegaManDebugWindow(QWidget* parent /* = nullptr */) : QMainWindow(parent)
{

  const std::string& serial = System::GetGameSerial();
  if (serial == "SLUS-00603")
  {
      // NA
    normalItemsAddress = 0xBE420;
    specialItemsAddress = 0xBE41C;
    canteenAddress = 0xc1b8c;
    helmetByteAddess = 0xb531a;
    skatesAndSpringsAddress = 0xb531b;
    armorAddress = 0xb531c;
    equippedBusterSlotAddress = 0xB5600; 
    inventoryBusterSlotAddress = 0xB5604;
    actualBusterValuesAddress = 0xB56AC;
    busterTable = 0x8FE5C;
    specialWeaponHaveAddress = 0xBE410;
  }
  else if (serial == "SLPS-01141")
  {
      // JP
    normalItemsAddress = 0xBE770;
    specialItemsAddress = 0xBE76C;
    canteenAddress = 0xc1edc;
    helmetByteAddess = 0xb565a;
    skatesAndSpringsAddress = 0xb565b;
    armorAddress = 0xb565c;
    equippedBusterSlotAddress = 0xb5940;
    inventoryBusterSlotAddress = 0xb5944;
    actualBusterValuesAddress = 0xb59ec;
    busterTable = 0x901F0;
  }
  else
  {
      Assert(false);
  }

  m_ui.setupUi(this);
  setupAdditionalUi();
  connectSignals();
  createModels();
  setUIEnabled(false);

  updateNormalItems();
  updateSpecialItems();
  initBusterPartsTab();
  initSpecialWeaponsTab();
}

MegaManDebugWindow::~MegaManDebugWindow() = default;

void MegaManDebugWindow::onDebuggerMessageReported(const QString& message)
{
}

void MegaManDebugWindow::closeEvent(QCloseEvent* event)
{
  g_emu_thread->disconnect(this);
  g_emu_thread->setSystemPaused(true, true);
  Host::RunOnCPUThread(&CPU::ClearBreakpoints);
  g_emu_thread->setSystemPaused(false);
  QMainWindow::closeEvent(event);
  emit closed();
}

void MegaManDebugWindow::setupAdditionalUi()
{
}

void MegaManDebugWindow::connectSignals()
{
  connect(g_emu_thread, &EmuThread::systemPaused, this, &MegaManDebugWindow::onEmulationPaused);
  connect(g_emu_thread, &EmuThread::systemResumed, this, &MegaManDebugWindow::onEmulationResumed);
  connect(g_emu_thread, &EmuThread::debuggerMessageReported, this, &MegaManDebugWindow::onDebuggerMessageReported);

  connect(m_ui.actionPause, &QAction::toggled, this, &MegaManDebugWindow::onPauseActionToggled);

  connect(m_ui.setHealthButton, &QPushButton::clicked, this, &MegaManDebugWindow::onSetHealthButtonPressed);
  connect(m_ui.normalItemAddButton, &QPushButton::clicked, this, &MegaManDebugWindow::onNormalItemAddButtonPressed);
  connect(m_ui.normalItemRemoveButton, &QPushButton::clicked, this,
          &MegaManDebugWindow::onNormalItemRemoveButtonPressed);

  connect(m_ui.specialItemEnergyCanteenCheckBox, &QCheckBox::clicked, this,
          &MegaManDebugWindow::onEnergyCanteenChecked);
  connect(m_ui.specialItemBlueRefractorCheckBox, &QCheckBox::clicked, this,
          &MegaManDebugWindow::onBlueRefractorChecked);
  connect(m_ui.specialItemWalkieTalkieCheckBox, &QCheckBox::clicked, this, &MegaManDebugWindow::onWalkieTalkieChecked);
  connect(m_ui.specialItemHelmetCheckBox, &QCheckBox::clicked, this, &MegaManDebugWindow::onHelmetChecked);
  connect(m_ui.specialItemYellowRefractorCheckBox, &QCheckBox::clicked, this,
          &MegaManDebugWindow::onYellowRefractorChecked);
  connect(m_ui.specialItemBonneKeyCheckBox, &QCheckBox::clicked, this, &MegaManDebugWindow::onBonneKeyChecked);
  connect(m_ui.specialItemJetSkatesCheckBox, &QCheckBox::clicked, this, &MegaManDebugWindow::onJetSkatesChecked);
  connect(m_ui.specialItemRedRefractorCheckBox, &QCheckBox::clicked, this, &MegaManDebugWindow::onRedRefractorChecked);
  connect(m_ui.specialItemCardonForestCount, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onCardonKeysChecked);
  connect(m_ui.specialItemJumpSpringsCheckBox, &QCheckBox::clicked, this, &MegaManDebugWindow::onJumpSpringsChecked);
  connect(m_ui.specialItemCitizensCardCheckBox, &QCheckBox::clicked, this, &MegaManDebugWindow::onCitizensCardChecked);
  connect(m_ui.specialItemLakeJyunKeysCount, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onLakeJyunKeysChecked);
  connect(m_ui.specialItemArmorJacketCount, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onArmorJacketChecked);
  connect(m_ui.specialItemClassALicenseCheckBox, &QCheckBox::clicked, this,
          &MegaManDebugWindow::onClassALicenseChecked);
  connect(m_ui.specialItemClozerKeysCount, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onClozerWoodsKeysChecked);
  connect(m_ui.specialItemAdapterPlugCheckBox, &QCheckBox::clicked, this, &MegaManDebugWindow::onAdapterPlugChecked);
  connect(m_ui.specialItemClassBLicenseCheckBox, &QCheckBox::clicked, this,
          &MegaManDebugWindow::onClassBLicenseChecked);
  connect(m_ui.specialItemSubCityKeysCount, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onSubCityKeysChecked);

  connect(m_ui.specialItemEnergyCanteenUpdateButton, &QPushButton::clicked, this,
          &MegaManDebugWindow::onUpdateCanteenPressed);

  connect(m_ui.specialItemHelmetEquipCheckBox, &QCheckBox::clicked, this, &MegaManDebugWindow::onEquipHelmetChecked);
  connect(m_ui.specialItemJetSkatesEquipCheckBox, &QCheckBox::clicked, this,
          &MegaManDebugWindow::onEquipJetSkatesChecked);
  connect(m_ui.specialItemJumpSpringsEquipCheckBox, &QCheckBox::clicked, this,
          &MegaManDebugWindow::onEquipJumpSpringsChecked);
  connect(m_ui.specialItemArmorJacketEquipCheckBox, &QCheckBox::clicked, this,
          &MegaManDebugWindow::onEquipArmorChecked);

  connect(m_ui.busterPartsSlot1ComboBox, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartEquippedSlot1Changed);
  connect(m_ui.busterPartsSlot2ComboBox, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartEquippedSlot2Changed);
  connect(m_ui.busterPartsSlot3ComboBox, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartEquippedSlot3Changed);

  
  connect(m_ui.busterPartsInventorySlot0, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot0Changed);
  connect(m_ui.busterPartsInventorySlot1, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot1Changed);
  connect(m_ui.busterPartsInventorySlot2, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot2Changed);
  connect(m_ui.busterPartsInventorySlot3, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot3Changed);
  connect(m_ui.busterPartsInventorySlot4, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot4Changed);
  connect(m_ui.busterPartsInventorySlot5, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot5Changed);
  connect(m_ui.busterPartsInventorySlot6, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot6Changed);
  connect(m_ui.busterPartsInventorySlot7, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot7Changed);
  connect(m_ui.busterPartsInventorySlot8, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot8Changed);
  connect(m_ui.busterPartsInventorySlot9, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot9Changed);
  connect(m_ui.busterPartsInventorySlot10, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot10Changed);
  connect(m_ui.busterPartsInventorySlot11, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot11Changed);
  connect(m_ui.busterPartsInventorySlot12, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot12Changed);
  connect(m_ui.busterPartsInventorySlot13, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot13Changed);
  connect(m_ui.busterPartsInventorySlot14, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot14Changed);
  connect(m_ui.busterPartsInventorySlot15, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot15Changed);
  connect(m_ui.busterPartsInventorySlot16, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot16Changed);
  connect(m_ui.busterPartsInventorySlot17, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot17Changed);
  connect(m_ui.busterPartsInventorySlot18, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot18Changed);
  connect(m_ui.busterPartsInventorySlot19, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot19Changed);
  connect(m_ui.busterPartsInventorySlot20, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot20Changed);
  connect(m_ui.busterPartsInventorySlot21, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot21Changed);
  connect(m_ui.busterPartsInventorySlot22, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot22Changed);
  connect(m_ui.busterPartsInventorySlot23, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot23Changed);
  connect(m_ui.busterPartsInventorySlot24, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot24Changed);
  connect(m_ui.busterPartsInventorySlot25, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot25Changed);
  connect(m_ui.busterPartsInventorySlot26, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot26Changed);
  connect(m_ui.busterPartsInventorySlot27, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot27Changed);
  connect(m_ui.busterPartsInventorySlot28, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot28Changed);
  connect(m_ui.busterPartsInventorySlot29, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot29Changed);
  connect(m_ui.busterPartsInventorySlot30, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot30Changed);
  connect(m_ui.busterPartsInventorySlot31, &QComboBox::currentIndexChanged, this,
          &MegaManDebugWindow::onBusterPartInventorySlot31Changed);

  connect(m_ui.busterPartsManuallyChangeValuesCheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onBusterPartManuallyChangeValuesChanged);

  connect(m_ui.busterPartsEquippedValuesAttackValue, &QLineEdit::textChanged, this,
          &MegaManDebugWindow::onBusterPartAttackValueChanged);
  connect(m_ui.busterPartsEquippedValuesEnergyValue, &QLineEdit::textChanged, this,
          &MegaManDebugWindow::onBusterPartEnergyValueChanged);
  connect(m_ui.busterPartsEquippedValuesRangeValue, &QLineEdit::textChanged, this,
          &MegaManDebugWindow::onBusterPartRangeValueChanged);
  connect(m_ui.busterPartsEquippedValuesRapidValue, &QLineEdit::textChanged, this,
          &MegaManDebugWindow::onBusterPartRapidValueChanged);

  connect(m_ui.specialWeaponActiveBusterCheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot0Changed);
  connect(m_ui.specialWeaponBladeArmCheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot1Changed);
  connect(m_ui.specialWeaponGrandGrenadeCheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot2Changed);
  connect(m_ui.specialWeaponSplashMineCheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot3Changed);
  connect(m_ui.specialWeaponShieldArmCheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot4Changed);
  connect(m_ui.specialWeaponShiningLaserCheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot5Changed);
  connect(m_ui.specialWeaponUnused1CheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot6Changed);
  connect(m_ui.specialWeaponUnused2CheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot7Changed);
  connect(m_ui.specialWeaponNormalArmCheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot8Changed);
  connect(m_ui.specialWeaponUnused3CheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot9Changed);
  connect(m_ui.specialWeaponMachineBusterCheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot10Changed);
  connect(m_ui.specialWeaponPoweredBusterCheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot11Changed);
  connect(m_ui.specialWeaponDrillArmCheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot12Changed);
  connect(m_ui.specialWeaponGrenadeArmCheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot13Changed);
  connect(m_ui.specialWeaponSpreadBusterCheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot14Changed);
  connect(m_ui.specialWeaponVacuumArmCheckBox, &QCheckBox::toggled, this,
          &MegaManDebugWindow::onSpecialWeaponsSlot15Changed);
}

void MegaManDebugWindow::disconnectSignals()
{
  EmuThread* hi = g_emu_thread;
  hi->disconnect(this);
}

void MegaManDebugWindow::createModels()
{
}

void MegaManDebugWindow::setUIEnabled(bool enabled)
{
}

void MegaManDebugWindow::onEmulationPaused()
{
  {
    QSignalBlocker sb(m_ui.actionPause);
    m_ui.actionPause->setChecked(true);
  }
}

void MegaManDebugWindow::onEmulationResumed()
{
  {
    QSignalBlocker sb(m_ui.actionPause);
    m_ui.actionPause->setChecked(false);
    updateNormalItems();
    updateSpecialItems();
    updateBusterParts();
    updateSpecialWeapons();
  }
}

void MegaManDebugWindow::onPauseActionToggled(bool paused)
{
  if (!paused)
  {
    setUIEnabled(false);
  }

  g_emu_thread->setSystemPaused(paused);
}

void MegaManDebugWindow::onSetHealthButtonPressed()
{
  u8* mem = Bus::GetMemoryRegionPointer(Bus::MemoryRegion::RAM);
  mem = mem + 0xB555E;

  s16* health = (s16*)mem;
  *health = atoi(m_ui.setHealthValue->text().toStdString().c_str());
}

void MegaManDebugWindow::updateNormalItems()
{

  m_ui.normalItemObtainedList->clear();
  u8* mem = Bus::GetMemoryRegionPointer(Bus::MemoryRegion::RAM);
  mem = mem + normalItemsAddress;

  u8 itembytes[8] = {
    *((mem) + 3), *((mem) + 2), *((mem) + 1), *((mem) + 0), *((mem) + 7), *((mem) + 6), *((mem) + 5), *((mem) + 4),
  };

  for (int i = 0; i < 8; i++)
  {
    u8 t = itembytes[i];

    for (int j = 0; j < 8; j++, t = t >> 1)
    {
      if (t & 1)
      {
        m_ui.normalItemObtainedList->addItem(normalItemNames[(i * 8) + 7 - j]);
      }
    }
  }

  m_ui.normalItemUnobtainedList->clear();

  for (int i = 0; i < 64; i++)
  {
    const char* itemName = normalItemNames[i];
    bool found = false;
    for (int j = 0; j < m_ui.normalItemObtainedList->count(); j++)
    {
      if (std::strcmp(itemName, m_ui.normalItemObtainedList->item(j)->text().toStdString().c_str()) == 0)
      {
        found = true;
        break;
      }
    }
    if (!found)
    {
      m_ui.normalItemUnobtainedList->addItem(normalItemNames[i]);
    }
  }
}

void MegaManDebugWindow::onNormalItemAddButtonPressed()
{

  u8* mem = Bus::GetMemoryRegionPointer(Bus::MemoryRegion::RAM);
  mem = mem + normalItemsAddress;

  for (QListWidgetItem* t : m_ui.normalItemUnobtainedList->selectedItems())
  {
    QString x = t->text();
    std::string x2 = x.toStdString();
    const char* selected = x2.c_str();
    for (int i = 0; i < 64; i++)
    {
      int offset = 0;
      if (std::strcmp(normalItemNames[i], selected) == 0)
      {
        if (i >= 32)
        {
          offset = 4;
          i -= 32;
        }

        u32 value = 0;
        bool didItWork = CPU::SafeReadMemoryWord(normalItemsAddress + offset, &value);

        u32 mask = 1 << 31 - i;
        value |= mask;

        bool didItWork2 = CPU::SafeWriteMemoryWord(normalItemsAddress + offset, value);
        break;
      }
    }
  }

  updateNormalItems();
}

void MegaManDebugWindow::onNormalItemRemoveButtonPressed()
{
  u8* mem = Bus::GetMemoryRegionPointer(Bus::MemoryRegion::RAM);
  mem = mem + normalItemsAddress;

  for (QListWidgetItem* t : m_ui.normalItemObtainedList->selectedItems())
  {
    QString x = t->text();
    std::string x2 = x.toStdString();
    const char* selected = x2.c_str();
    for (int i = 0; i < 64; i++)
    {
      int offset = 0;
      if (std::strcmp(normalItemNames[i], selected) == 0)
      {
        if (i >= 32)
        {
          offset = 4;
          i -= 32;
        }

        u32 value = 0;
        bool didItWork = CPU::SafeReadMemoryWord(normalItemsAddress + offset, &value);

        u32 mask = 1 << 31 - i;
        mask = ~mask;
        value &= mask;

        bool didItWork2 = CPU::SafeWriteMemoryWord(normalItemsAddress + offset, value);
        break;
      }
    }
  }

  updateNormalItems();
}

void MegaManDebugWindow::updateSpecialItems()
{

  u8* mem = Bus::GetMemoryRegionPointer(Bus::MemoryRegion::RAM);
  mem = mem + specialItemsAddress;

  u32* x = (u32*)mem;
  u32 specialItems = *x;

  int cardonCount = 0;
  int jyunCount = 0;
  int clozerCount = 0;
  int subCityCount = 0;

  bool kevlarOmega = false;
  bool kevlar = false;
  bool flak = false;

  // Blue Refractor
  m_ui.specialItemBlueRefractorCheckBox->setChecked(specialItems & 1);
  specialItems = specialItems >> 1;

  // Adapter Plug
  m_ui.specialItemAdapterPlugCheckBox->setChecked(specialItems & 1);
  specialItems = specialItems >> 1;

  // Unused
  specialItems = specialItems >> 1;

  // Energy canteen
  m_ui.specialItemEnergyCanteenCheckBox->setChecked(specialItems & 1);
  specialItems = specialItems >> 1;

  // Unused
  specialItems = specialItems >> 1;

  // Jet Skates
  m_ui.specialItemJetSkatesCheckBox->setChecked(specialItems & 1);
  specialItems = specialItems >> 1;

  // Jump Springs
  m_ui.specialItemJumpSpringsCheckBox->setChecked(specialItems & 1);
  specialItems = specialItems >> 1;

  // Helmet
  m_ui.specialItemHelmetCheckBox->setChecked(specialItems & 1);
  specialItems = specialItems >> 1;

  // Cardon forest keys 2, 3
  if (specialItems & 1)
  {
    cardonCount++;
  }
  specialItems = specialItems >> 1;

  if (specialItems & 1)
  {
    cardonCount++;
  }
  specialItems = specialItems >> 1;

  // Bonne Family Key
  m_ui.specialItemBonneKeyCheckBox->setChecked(specialItems & 1);
  specialItems = specialItems >> 1;

  // Class B License
  m_ui.specialItemClassBLicenseCheckBox->setChecked(specialItems & 1);
  specialItems = specialItems >> 1;

  // Class A License
  m_ui.specialItemClassALicenseCheckBox->setChecked(specialItems & 1);
  specialItems = specialItems >> 1;

  // Citizen's Card
  m_ui.specialItemCitizensCardCheckBox->setChecked(specialItems & 1);
  specialItems = specialItems >> 1;

  // Red Refractor
  m_ui.specialItemRedRefractorCheckBox->setChecked(specialItems & 1);
  specialItems = specialItems >> 1;

  // Yellow Refractor
  m_ui.specialItemYellowRefractorCheckBox->setChecked(specialItems & 1);
  specialItems = specialItems >> 1;

  // Sub City Key 3
  if (specialItems & 1)
  {
    subCityCount++;
  }
  specialItems = specialItems >> 1;

  // Clozer Woods Sub-gate Keys
  if (specialItems & 1)
  {
    clozerCount++;
  }
  specialItems = specialItems >> 1;
  if (specialItems & 1)
  {
    clozerCount++;
  }
  specialItems = specialItems >> 1;
  if (specialItems & 1)
  {
    clozerCount++;
  }
  specialItems = specialItems >> 1;

  // Lake Jyun Sub-gate Keys
  if (specialItems & 1)
  {
    jyunCount++;
  }
  specialItems = specialItems >> 1;
  if (specialItems & 1)
  {
    jyunCount++;
  }
  specialItems = specialItems >> 1;
  if (specialItems & 1)
  {
    jyunCount++;
  }
  specialItems = specialItems >> 1;

  // Cardon Forest Key 1
  if (specialItems & 1)
  {
    cardonCount++;
  }
  specialItems = specialItems >> 1;

  // Unused
  specialItems = specialItems >> 1;

  // Unused
  specialItems = specialItems >> 1;

  // Walkie-Talkie
  m_ui.specialItemWalkieTalkieCheckBox->setChecked(specialItems & 1);
  specialItems = specialItems >> 1;

  // Armor Jacket
  kevlarOmega = specialItems & 1;
  specialItems = specialItems >> 1;

  kevlar = specialItems & 1;
  specialItems = specialItems >> 1;

  flak = specialItems & 1;
  specialItems = specialItems >> 1;

  // Sub-City Keys 1, 2
  if (specialItems & 1)
  {
    subCityCount++;
  }
  specialItems = specialItems >> 1;
  if (specialItems & 1)
  {
    subCityCount++;
  }

  m_ui.specialItemCardonForestCount->setCurrentIndex(cardonCount);
  m_ui.specialItemLakeJyunKeysCount->setCurrentIndex(jyunCount);
  m_ui.specialItemClozerKeysCount->setCurrentIndex(clozerCount);
  m_ui.specialItemSubCityKeysCount->setCurrentIndex(subCityCount);

  if (kevlarOmega && kevlar && flak)
  {
    m_ui.specialItemArmorJacketCount->setCurrentIndex(3);
  }
  else if (kevlar && flak)
  {
    m_ui.specialItemArmorJacketCount->setCurrentIndex(2);
  }
  else if (flak)
  {
    m_ui.specialItemArmorJacketCount->setCurrentIndex(1);
  }
  else
  {
    m_ui.specialItemArmorJacketCount->setCurrentIndex(0);
  }

  u8 canteenValue = 0;
  u8 canteenMax = 0;

  CPU::SafeReadMemoryByte(canteenAddress, &canteenValue);
  CPU::SafeReadMemoryByte(canteenAddress + 1, &canteenMax);

  m_ui.specialItemEnergyCanteenCurrentValue->setText(std::to_string(canteenValue).c_str());
  m_ui.specialItemEnergyCanteenMaxValue->setText(std::to_string(canteenMax).c_str());

  u8 helmetValue = 0, skatesAndSpringsValue = 0, armorValue = 0;
  CPU::SafeReadMemoryByte(helmetByteAddess, &helmetValue);
  CPU::SafeReadMemoryByte(skatesAndSpringsAddress, &skatesAndSpringsValue);
  CPU::SafeReadMemoryByte(armorAddress, &armorValue);

  m_ui.specialItemHelmetEquipCheckBox->setChecked(helmetValue > 0);
  m_ui.specialItemJetSkatesEquipCheckBox->setChecked(skatesAndSpringsValue > 1);
  m_ui.specialItemJumpSpringsEquipCheckBox->setChecked(skatesAndSpringsValue == 1 || skatesAndSpringsValue == 3);
  m_ui.specialItemArmorJacketEquipCheckBox->setChecked(armorValue > 0);
}

void MegaManDebugWindow::onEnergyCanteenChecked(bool checked)
{
  flipBitSpecialItem(3, checked);
  updateSpecialItems();
}
void MegaManDebugWindow::onBlueRefractorChecked(bool checked)
{
  flipBitSpecialItem(0, checked);
  updateSpecialItems();
}
void MegaManDebugWindow::onWalkieTalkieChecked(bool checked)
{
  flipBitSpecialItem(26, checked);
  updateSpecialItems();
}
void MegaManDebugWindow::onHelmetChecked(bool checked)
{
  flipBitSpecialItem(7, checked);
  updateSpecialItems();
}
void MegaManDebugWindow::onYellowRefractorChecked(bool checked)
{
  flipBitSpecialItem(15, checked);
  updateSpecialItems();
}
void MegaManDebugWindow::onBonneKeyChecked(bool checked)
{
  flipBitSpecialItem(10, checked);
  updateSpecialItems();
}
void MegaManDebugWindow::onJetSkatesChecked(bool checked)
{
  flipBitSpecialItem(5, checked);
  updateSpecialItems();
}
void MegaManDebugWindow::onRedRefractorChecked(bool checked)
{
  flipBitSpecialItem(14, checked);
  updateSpecialItems();
}
void MegaManDebugWindow::onCardonKeysChecked(int index)
{
  switch (index)
  {
    case 0:
      flipBitSpecialItem(23, false);
      flipBitSpecialItem(9, false);
      flipBitSpecialItem(8, false);
      break;
    case 1:
      flipBitSpecialItem(23, true);
      flipBitSpecialItem(9, false);
      flipBitSpecialItem(8, false);
      break;
    case 2:
      flipBitSpecialItem(23, true);
      flipBitSpecialItem(9, true);
      flipBitSpecialItem(8, false);
      break;
    case 3:
      flipBitSpecialItem(23, true);
      flipBitSpecialItem(9, true);
      flipBitSpecialItem(8, true);
      break;
  }

  updateSpecialItems();
}
void MegaManDebugWindow::onJumpSpringsChecked(bool checked)
{
  flipBitSpecialItem(6, checked);
  updateSpecialItems();
}
void MegaManDebugWindow::onCitizensCardChecked(bool checked)
{
  flipBitSpecialItem(13, checked);
  updateSpecialItems();
}
void MegaManDebugWindow::onLakeJyunKeysChecked(int index)
{
  switch (index)
  {
    case 0:
      flipBitSpecialItem(22, false);
      flipBitSpecialItem(21, false);
      flipBitSpecialItem(20, false);
      break;
    case 1:
      flipBitSpecialItem(22, true);
      flipBitSpecialItem(21, false);
      flipBitSpecialItem(20, false);
      break;
    case 2:
      flipBitSpecialItem(22, true);
      flipBitSpecialItem(21, true);
      flipBitSpecialItem(20, false);
      break;
    case 3:
      flipBitSpecialItem(22, true);
      flipBitSpecialItem(21, true);
      flipBitSpecialItem(20, true);
      break;
  }
  updateSpecialItems();
}
void MegaManDebugWindow::onArmorJacketChecked(int index)
{
  switch (index)
  {
    case 0:
      flipBitSpecialItem(29, false);
      flipBitSpecialItem(28, false);
      flipBitSpecialItem(27, false);
      break;
    case 1:
      flipBitSpecialItem(29, true);
      flipBitSpecialItem(28, false);
      flipBitSpecialItem(27, false);
      break;
    case 2:
      flipBitSpecialItem(29, true);
      flipBitSpecialItem(28, true);
      flipBitSpecialItem(27, false);
      break;
    case 3:
      flipBitSpecialItem(29, true);
      flipBitSpecialItem(28, true);
      flipBitSpecialItem(27, true);
      break;
  }
  updateSpecialItems();
}
void MegaManDebugWindow::onClassALicenseChecked(bool checked)
{
  flipBitSpecialItem(12, checked);
  updateSpecialItems();
}
void MegaManDebugWindow::onClozerWoodsKeysChecked(int index)
{
  switch (index)
  {
    case 0:
      flipBitSpecialItem(19, false);
      flipBitSpecialItem(18, false);
      flipBitSpecialItem(17, false);
      break;
    case 1:
      flipBitSpecialItem(19, true);
      flipBitSpecialItem(18, false);
      flipBitSpecialItem(17, false);
      break;
    case 2:
      flipBitSpecialItem(19, true);
      flipBitSpecialItem(18, true);
      flipBitSpecialItem(17, false);
      break;
    case 3:
      flipBitSpecialItem(19, true);
      flipBitSpecialItem(18, true);
      flipBitSpecialItem(17, true);
      break;
  }
  updateSpecialItems();
}
void MegaManDebugWindow::onAdapterPlugChecked(bool checked)
{
  flipBitSpecialItem(1, checked);
  updateSpecialItems();
}
void MegaManDebugWindow::onClassBLicenseChecked(bool checked)
{
  flipBitSpecialItem(11, checked);
  updateSpecialItems();
}
void MegaManDebugWindow::onSubCityKeysChecked(int index)
{
  switch (index)
  {
    case 0:
      flipBitSpecialItem(31, false);
      flipBitSpecialItem(30, false);
      flipBitSpecialItem(16, false);
      break;
    case 1:
      flipBitSpecialItem(31, true);
      flipBitSpecialItem(30, false);
      flipBitSpecialItem(16, false);
      break;
    case 2:
      flipBitSpecialItem(31, true);
      flipBitSpecialItem(30, true);
      flipBitSpecialItem(16, false);
      break;
    case 3:
      flipBitSpecialItem(31, true);
      flipBitSpecialItem(30, true);
      flipBitSpecialItem(16, true);
      break;
  }
  updateSpecialItems();
}

void MegaManDebugWindow::flipBitSpecialItem(int offset, bool value)
{
  u32 v = 0;
  CPU::SafeReadMemoryWord(specialItemsAddress, &v);

  u32 mask = 1 << offset;
  if (value)
  {
    v |= mask;
  }
  else
  {
    mask = ~mask;
    v &= mask;
  }

  CPU::SafeWriteMemoryWord(specialItemsAddress, v);
}

void MegaManDebugWindow::onUpdateCanteenPressed()
{
  u8 canteenValue = atoi(m_ui.specialItemEnergyCanteenCurrentValue->text().toStdString().c_str());
  u8 canteenMax = atoi(m_ui.specialItemEnergyCanteenMaxValue->text().toStdString().c_str());

  CPU::SafeWriteMemoryByte(canteenAddress, canteenValue);
  CPU::SafeWriteMemoryByte(canteenAddress + 1, canteenMax);
}

void MegaManDebugWindow::onEquipHelmetChecked(bool checked)
{
  CPU::SafeWriteMemoryByte(helmetByteAddess, checked);
  updateSpecialItems();
}
void MegaManDebugWindow::onEquipJetSkatesChecked(bool checked)
{
  u8 value = 0;
  CPU::SafeReadMemoryByte(skatesAndSpringsAddress, &value);
  if (checked)
  {
    value |= 2;
  }
  else
  {
    value &= ~2;
  }

  CPU::SafeWriteMemoryByte(skatesAndSpringsAddress, value);
  updateSpecialItems();
}
void MegaManDebugWindow::onEquipJumpSpringsChecked(bool checked)
{
  u8 value = 0;
  CPU::SafeReadMemoryByte(skatesAndSpringsAddress, &value);
  if (checked)
  {
    value |= 1;
  }
  else
  {
    value &= ~1;
  }
  CPU::SafeWriteMemoryByte(skatesAndSpringsAddress, value);
  updateSpecialItems();
}
void MegaManDebugWindow::onEquipArmorChecked(bool checked)
{
  CPU::SafeWriteMemoryByte(armorAddress, checked);
  updateSpecialItems();
}

void MegaManDebugWindow::initBusterPartsTab()
{

  equippedSlots[0] = m_ui.busterPartsSlot1ComboBox;
  equippedSlots[1] = m_ui.busterPartsSlot2ComboBox;
  equippedSlots[2] = m_ui.busterPartsSlot3ComboBox;

  inventorySlots[0] = m_ui.busterPartsInventorySlot0;
  inventorySlots[1] = m_ui.busterPartsInventorySlot1;
  inventorySlots[2] = m_ui.busterPartsInventorySlot2;
  inventorySlots[3] = m_ui.busterPartsInventorySlot3;
  inventorySlots[4] = m_ui.busterPartsInventorySlot4;
  inventorySlots[5] = m_ui.busterPartsInventorySlot5;
  inventorySlots[6] = m_ui.busterPartsInventorySlot6;
  inventorySlots[7] = m_ui.busterPartsInventorySlot7;
  inventorySlots[8] = m_ui.busterPartsInventorySlot8;
  inventorySlots[9] = m_ui.busterPartsInventorySlot9;
  inventorySlots[10] = m_ui.busterPartsInventorySlot10;
  inventorySlots[11] = m_ui.busterPartsInventorySlot11;
  inventorySlots[12] = m_ui.busterPartsInventorySlot12;
  inventorySlots[13] = m_ui.busterPartsInventorySlot13;
  inventorySlots[14] = m_ui.busterPartsInventorySlot14;
  inventorySlots[15] = m_ui.busterPartsInventorySlot15;
  inventorySlots[16] = m_ui.busterPartsInventorySlot16;
  inventorySlots[17] = m_ui.busterPartsInventorySlot17;
  inventorySlots[18] = m_ui.busterPartsInventorySlot18;
  inventorySlots[19] = m_ui.busterPartsInventorySlot19;
  inventorySlots[20] = m_ui.busterPartsInventorySlot20;
  inventorySlots[21] = m_ui.busterPartsInventorySlot21;
  inventorySlots[22] = m_ui.busterPartsInventorySlot22;
  inventorySlots[23] = m_ui.busterPartsInventorySlot23;
  inventorySlots[24] = m_ui.busterPartsInventorySlot24;
  inventorySlots[25] = m_ui.busterPartsInventorySlot25;
  inventorySlots[26] = m_ui.busterPartsInventorySlot26;
  inventorySlots[27] = m_ui.busterPartsInventorySlot27;
  inventorySlots[28] = m_ui.busterPartsInventorySlot28;
  inventorySlots[29] = m_ui.busterPartsInventorySlot29;
  inventorySlots[30] = m_ui.busterPartsInventorySlot30;
  inventorySlots[31] = m_ui.busterPartsInventorySlot31;

  for (int i = 0; i < 3; i++)
  {
    QComboBox* box = equippedSlots[i];
    box->clear();
  }
  for (int i = 0; i < 32; i++)
  {
    QComboBox* box = inventorySlots[i];
    box->clear();
  }
  for (int i = 0; i < 256; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      QComboBox* box = equippedSlots[j];
      char buf[16];
      sprintf(buf, "(0x%02x)", i);

      std::string s = buf;
      s = s + " " + busterPartNames[i];
      box->addItem(s.c_str());
    }
    for (int j = 0; j < 32; j++)
    {
      QComboBox* box = inventorySlots[j];
      char buf[16];
      sprintf(buf, "(0x%02x)", i);

      std::string s = buf;
      s = s + " " + busterPartNames[i];
      box->addItem(s.c_str());
    }
  }
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartEquippedSlot1Changed(int index)
{
  CPU::SafeWriteMemoryByte(equippedBusterSlotAddress, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartEquippedSlot2Changed(int index)
{
  CPU::SafeWriteMemoryByte(equippedBusterSlotAddress + 1, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartEquippedSlot3Changed(int index)
{
  CPU::SafeWriteMemoryByte(equippedBusterSlotAddress + 2, index);
  updateBusterParts();
}

void MegaManDebugWindow::updateBusterParts()
{
  for (int i = 0; i < 3; i++)
  {
    u8 v = 0;
    CPU::SafeReadMemoryByte(equippedBusterSlotAddress + i, &v);
    if (v != equippedSlots[i]->currentIndex())
    {
      equippedSlots[i]->setCurrentIndex(v);
    }
  }

  for (int i = 0; i < 32; i++)
  {
    u8 v = 0;
    CPU::SafeReadMemoryByte(inventoryBusterSlotAddress + i, &v);
    if (v != inventorySlots[i]->currentIndex())
    {
      inventorySlots[i]->setCurrentIndex(v);
    }
  }

  u8 values[4] = {0, 0, 0, 0};
  for (int j = 0; j < 3; j++)
  {
    if (equippedSlots[j]->currentIndex() == 0)
    {
      continue;
    }

    u8 v = 0;
    int a = busterTable + (equippedSlots[j]->currentIndex() * 8);
    CPU::SafeReadMemoryByte(a + 0, &v);
    values[0] += v;
    v = 0;

    CPU::SafeReadMemoryByte(a + 1, &v);
    values[1] += v;
    v = 0;

    CPU::SafeReadMemoryByte(a + 2, &v);
    values[2] += v;
    v = 0;

    CPU::SafeReadMemoryByte(a + 3, &v);
    values[3] += v;
    v = 0;
  }

  for (int i = 0; i < 3; i++)
  {
    if (values[i] > 7)
    {
      values[i] = 7;
    }
  }
  if (values[3] > 4)
  {
    values[3] = 4;
  }

  m_ui.busterPartsEquippedValuesAttackValue->setText(std::to_string(values[0]).c_str());
  m_ui.busterPartsEquippedValuesEnergyValue->setText(std::to_string(values[1]).c_str());
  m_ui.busterPartsEquippedValuesRangeValue->setText(std::to_string(values[2]).c_str());
  m_ui.busterPartsEquippedValuesRapidValue->setText(std::to_string(values[3]).c_str());
}

void MegaManDebugWindow::onBusterPartInventorySlot0Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 0, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot1Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 1, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot2Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 2, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot3Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 3, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot4Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 4, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot5Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 5, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot6Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 6, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot7Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 7, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot8Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 8, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot9Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 9, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot10Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 10, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot11Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 11, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot12Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 12, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot13Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 13, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot14Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 14, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot15Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 15, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot16Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 16, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot17Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 17, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot18Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 18, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot19Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 19, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot20Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 20, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot21Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 21, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot22Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 22, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot23Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 23, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot24Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 24, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot25Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 25, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot26Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 26, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot27Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 27, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot28Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 28, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot29Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 29, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot30Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 30, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartInventorySlot31Changed(int index)
{
  CPU::SafeWriteMemoryByte(inventoryBusterSlotAddress + 31, index);
  updateBusterParts();
}

void MegaManDebugWindow::onBusterPartManuallyChangeValuesChanged(bool value)
{
  m_ui.busterPartsEquippedValuesAttackValue->setReadOnly(!value);
  m_ui.busterPartsEquippedValuesEnergyValue->setReadOnly(!value);
  m_ui.busterPartsEquippedValuesRangeValue->setReadOnly(!value);
  m_ui.busterPartsEquippedValuesRapidValue->setReadOnly(!value);
}

void MegaManDebugWindow::onBusterPartAttackValueChanged(const QString& value)
{
  CPU::SafeWriteMemoryByte(actualBusterValuesAddress + 0, atoi(value.toStdString().c_str()));
}
void MegaManDebugWindow::onBusterPartEnergyValueChanged(const QString& value)
{
  CPU::SafeWriteMemoryByte(actualBusterValuesAddress + 1, atoi(value.toStdString().c_str()));
}
void MegaManDebugWindow::onBusterPartRangeValueChanged(const QString& value)
{
  CPU::SafeWriteMemoryByte(actualBusterValuesAddress + 2, atoi(value.toStdString().c_str()));
}
void MegaManDebugWindow::onBusterPartRapidValueChanged(const QString& value)
{
  CPU::SafeWriteMemoryByte(actualBusterValuesAddress + 3, atoi(value.toStdString().c_str()));
}

void MegaManDebugWindow::initSpecialWeaponsTab()
{
  specialWeapons[0] = m_ui.specialWeaponActiveBusterCheckBox;
  specialWeapons[1] = m_ui.specialWeaponBladeArmCheckBox;
  specialWeapons[2] = m_ui.specialWeaponGrandGrenadeCheckBox;
  specialWeapons[3] = m_ui.specialWeaponSplashMineCheckBox;
  specialWeapons[4] = m_ui.specialWeaponShieldArmCheckBox;
  specialWeapons[5] = m_ui.specialWeaponShiningLaserCheckBox;
  specialWeapons[6] = m_ui.specialWeaponUnused1CheckBox;
  specialWeapons[7] = m_ui.specialWeaponUnused2CheckBox;
  specialWeapons[8] = m_ui.specialWeaponNormalArmCheckBox;
  specialWeapons[9] = m_ui.specialWeaponUnused3CheckBox;
  specialWeapons[10] = m_ui.specialWeaponMachineBusterCheckBox;
  specialWeapons[11] = m_ui.specialWeaponPoweredBusterCheckBox;
  specialWeapons[12] = m_ui.specialWeaponDrillArmCheckBox;
  specialWeapons[13] = m_ui.specialWeaponGrenadeArmCheckBox;
  specialWeapons[14] = m_ui.specialWeaponSpreadBusterCheckBox;
  specialWeapons[15] = m_ui.specialWeaponVacuumArmCheckBox;
  updateSpecialWeapons();
}

void MegaManDebugWindow::updateSpecialWeapons()
{
 
    u16 v = 0;
    CPU::SafeReadMemoryHalfWord(specialWeaponHaveAddress, &v);

    for (int i = 0; i < 16; i++)
    {
        specialWeapons[i]->setChecked((v >> (15 - i)) & 1);
    }
}

void MegaManDebugWindow::onSpecialWeaponsSlot0Changed(bool value)
{
    flipBitSpecialWeapon(0, value);
}

void MegaManDebugWindow::onSpecialWeaponsSlot1Changed(bool value)
{
    flipBitSpecialWeapon(1, value);
}

void MegaManDebugWindow::onSpecialWeaponsSlot2Changed(bool value)
{
    flipBitSpecialWeapon(2, value);
}

void MegaManDebugWindow::onSpecialWeaponsSlot3Changed(bool value)
{
    flipBitSpecialWeapon(3, value);
}

void MegaManDebugWindow::onSpecialWeaponsSlot4Changed(bool value)
{
    flipBitSpecialWeapon(4, value);
}

void MegaManDebugWindow::onSpecialWeaponsSlot5Changed(bool value)
{
    flipBitSpecialWeapon(5, value);
}

void MegaManDebugWindow::onSpecialWeaponsSlot6Changed(bool value)
{
    flipBitSpecialWeapon(6, value);
}

void MegaManDebugWindow::onSpecialWeaponsSlot7Changed(bool value)
{
    flipBitSpecialWeapon(7, value);
}

void MegaManDebugWindow::onSpecialWeaponsSlot8Changed(bool value)
{
    flipBitSpecialWeapon(8, value);
}

void MegaManDebugWindow::onSpecialWeaponsSlot9Changed(bool value)
{
    flipBitSpecialWeapon(9, value);
}

void MegaManDebugWindow::onSpecialWeaponsSlot10Changed(bool value)
{
    flipBitSpecialWeapon(10, value);
}

void MegaManDebugWindow::onSpecialWeaponsSlot11Changed(bool value)
{
    flipBitSpecialWeapon(11, value);
}

void MegaManDebugWindow::onSpecialWeaponsSlot12Changed(bool value)
{
    flipBitSpecialWeapon(12, value);
}

void MegaManDebugWindow::onSpecialWeaponsSlot13Changed(bool value)
{
    flipBitSpecialWeapon(13, value);
}

void MegaManDebugWindow::onSpecialWeaponsSlot14Changed(bool value)
{
    flipBitSpecialWeapon(14, value);
}

void MegaManDebugWindow::onSpecialWeaponsSlot15Changed(bool value)
{
    flipBitSpecialWeapon(15, value);
}

void MegaManDebugWindow::flipBitSpecialWeapon(int offset, bool value)
{

    u16 v = 0;
    CPU::SafeReadMemoryHalfWord(specialWeaponHaveAddress, &v);

    u16 mask = 1 << (15 - offset);
    if (value)
    {
        v |= mask;
    }
    else
    {
        mask = ~mask;
        v &= mask;
    }

    CPU::SafeWriteMemoryHalfWord(specialWeaponHaveAddress, v);
}