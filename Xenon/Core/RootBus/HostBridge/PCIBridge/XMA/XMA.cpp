// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "XMA.h"

Xe::PCIDev::XMA::XMA(const std::string &deviceName, u64 size) : PCIDevice(deviceName, size) {
  // Set PCI Properties
  pciConfigSpace.configSpaceHeader.reg0.hexData = 0x58011414;
  pciConfigSpace.configSpaceHeader.reg1.hexData = 0x02000002;
  // Set our PCI Dev Sizes
  pciDevSizes[0] = 0x400; // BAR0
}

void Xe::PCIDev::XMA::Read(u64 readAddress, u8 *data, u64 size)
{}

void Xe::PCIDev::XMA::Write(u64 writeAddress, const u8 *data, u64 size)
{}

void Xe::PCIDev::XMA::MemSet(u64 writeAddress, s32 data, u64 size)
{}

void Xe::PCIDev::XMA::ConfigRead(u64 readAddress, u8 *data, u64 size) {
  memcpy(data, &pciConfigSpace.data[static_cast<u8>(readAddress)], size);
}

void Xe::PCIDev::XMA::ConfigWrite(u64 writeAddress, const u8 *data, u64 size) {
  // Check if we're being scanned
  u64 tmp = 0;
  memcpy(&tmp, data, size);
  if (static_cast<u8>(writeAddress) >= 0x10 && static_cast<u8>(writeAddress) < 0x34) {
    const u32 regOffset = (static_cast<u8>(writeAddress) - 0x10) >> 2;
    if (pciDevSizes[regOffset] != 0) {
      if (tmp == 0xFFFFFFFF) { // PCI BAR Size discovery
        u64 x = 2;
        for (int idx = 2; idx < 31; idx++) {
          tmp &= ~x;
          x <<= 1;
          if (x >= pciDevSizes[regOffset]) {
            break;
          }
        }
        tmp &= ~0x3;
      }
    }
    if (static_cast<u8>(writeAddress) == 0x30) { // Expansion ROM Base Address
      tmp = 0; // Register not implemented
    }
  }

  memcpy(&pciConfigSpace.data[static_cast<u8>(writeAddress)], &tmp, size);
}
