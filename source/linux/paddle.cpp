#include "StdAfx.h"

#include "linux/paddle.h"

#include "Log.h"
#include "Memory.h"
#include "Common.h"
#include "CPU.h"

unsigned __int64 g_nJoyCntrResetCycle = 0;	// Abs cycle that joystick counters were reset
const double PDL_CNTR_INTERVAL = 2816.0 / 255.0;	// 11.04 (From KEGS)

Paddle::Paddle()
{
}

bool Paddle::getButton(int i) const
{
  return false;
}

int Paddle::getAxis(int i) const
{
  return 0;
}

std::shared_ptr<const Paddle> & Paddle::instance()
{
  static std::shared_ptr<const Paddle> singleton = std::make_shared<Paddle>();
  return singleton;
}

Paddle::~Paddle()
{
}

BYTE __stdcall JoyReadButton(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
  addr &= 0xFF;
  BOOL pressed = 0;

  const std::shared_ptr<const Paddle> & paddle = Paddle::instance();

  if (paddle)
  {
    switch (addr)
    {
    case 0x61:
      pressed = paddle->getButton(0);
      break;
    case 0x62:
      pressed = paddle->getButton(1);
      break;
    case 0x63:
      break;
    }
  }

  return MemReadFloatingBus(pressed, nCyclesLeft);
}

BYTE __stdcall JoyReadPosition(WORD pc, WORD address, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
  const int nJoyNum = (address & 2) ? 1 : 0;	// $C064..$C067

  CpuCalcCycles(nCyclesLeft);
  BOOL nPdlCntrActive = 0;

  const std::shared_ptr<const Paddle> & paddle = Paddle::instance();

  if (paddle)
  {
    if (nJoyNum == 0)
    {
      int axis = address & 1;
      int pdl = paddle->getAxis(axis);
      // This is from KEGS. It helps games like Championship Lode Runner & Boulderdash
      if (pdl >= 255)
	pdl = 280;

      nPdlCntrActive  = g_nCumulativeCycles <= (g_nJoyCntrResetCycle + (unsigned __int64) ((double)pdl * PDL_CNTR_INTERVAL));
    }
  }

  return MemReadFloatingBus(nPdlCntrActive, nCyclesLeft);
}

BYTE __stdcall JoyResetPosition(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
  CpuCalcCycles(nCyclesLeft);
  g_nJoyCntrResetCycle = g_nCumulativeCycles;

  return MemReadFloatingBus(nCyclesLeft);
}