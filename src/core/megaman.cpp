#include "megaman.h"
#include "imgui.h"
#include "host.h"
#include "util/imgui_manager.h"
#include "bus.h"
#include "system.h"

void MegaMan::DrawDebugStateWindow()
{
  static const ImVec4 active_color{1.0f, 1.0f, 1.0f, 1.0f};
  static const ImVec4 inactive_color{0.4f, 0.4f, 0.4f, 1.0f};
  const float framebuffer_scale = Host::GetOSDScale();

  ImGui::SetNextWindowSize(ImVec2(400.0f * framebuffer_scale, 100.0f * framebuffer_scale), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Mega Man State", nullptr))
  {
    ImGui::End();
    return;
  }

  // Engine related memory addresses
  int cameraXAddress;
  int cameraZAddress;
  int cameraYAddress;
  int cameraVAddress;
  int cameraHAddress;
  int cameraAngleAddress;

  // Room related
  int roomAddress;
  int subRoomAddress;

  const std::string& serial = System::GetGameSerial();
  if (serial == "SLUS-00603")
  {
    // Engine related memory addresses
     cameraXAddress = 0xA39A0;
     cameraZAddress = 0xA39A4;
     cameraYAddress = 0xA39A8;
     cameraVAddress = 0xA396C;
     cameraHAddress = 0xA3974;
     cameraAngleAddress = 0xA3968;

     roomAddress = 0xC356E;
     subRoomAddress = 0xC1B61;

  }
  else if (serial == "SLPS-01141")
  {
    // Engine related memory addresses
     cameraXAddress = 0xa3ce0;
     cameraZAddress = 0xa3ce4;
     cameraYAddress = 0xa3ce8;
     cameraVAddress = 0xA3CAC; 
     cameraHAddress = 0xA3CB4;

     cameraAngleAddress = 0xA3CA8;

     roomAddress = 0xc38be;
     subRoomAddress = 0xc38bf;
  }
  else
  {
     ImGui::End();
     return;
  }

  
  const u8* mem = GetMemoryRegionPointer(Bus::MemoryRegion::RAM);
  const s16* health = (s16*)(mem + 0xB555E);
  std::string s = "Health: " + std::to_string(*health);
  ImGui::Text(s.c_str());

  const u8* kickStorage = (u8*)(mem + 0xb5688);
  s = "Kick Storage: " + std::to_string(*kickStorage);
  ImGui::Text(s.c_str());

  const s16* camerax = (s16*)(mem + cameraXAddress);
  const s16* cameray = (s16*)(mem + cameraYAddress);
  const s16* cameraz = (s16*)(mem + cameraZAddress);
  const s16* camerav = (s16*)(mem + cameraVAddress);
  const s16* camerah = (s16*)(mem + cameraHAddress);
  const s16* cameraang = (s16*)(mem + cameraAngleAddress);

  const u8* room = (u8*)(mem + roomAddress);
  const u8* subroom = (u8*)(mem + subRoomAddress);

  s = "Camera Pos X: " + std::to_string(*camerax) + " Y: " + std::to_string(*cameray) +
      " Z: " + std::to_string(*cameraz) + " Pitch: " + std::to_string(*camerav) +
      " Yaw: " + std::to_string(*cameraang) + " Rotation: " + std::to_string(*camerah);

  ImGui::Text(s.c_str());

  s = "Room: " + std::to_string(*room) + " Subroom: " + std::to_string(*subroom);
  ImGui::Text(s.c_str());

  ImGui::End();
}