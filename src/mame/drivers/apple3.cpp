// license:BSD-3-Clause
// copyright-holders:Nathan Woods,R. Belmont
/***************************************************************************

    drivers/apple3.c

    Apple ///

    driver by Nathan Woods and R. Belmont

    Special thanks to Chris Smolinski (author of the Sara emulator)
    for his input about this poorly known system.

    Also thanks to Washington Apple Pi for the "Apple III DVD" containing the
    technical manual, schematics, and software.

***************************************************************************/

#include "emu.h"
#include "includes/apple3.h"
#include "includes/apple2.h"
#include "sound/volt_reg.h"
#include "formats/ap2_dsk.h"

#include "bus/a2bus/a2cffa.h"
#include "bus/a2bus/a2applicard.h"
#include "bus/a2bus/a2thunderclock.h"
#include "bus/a2bus/mouse.h"

#include "bus/rs232/rs232.h"

#include "screen.h"
#include "softlist.h"
#include "speaker.h"

static ADDRESS_MAP_START( apple3_map, AS_PROGRAM, 8, apple3_state )
	AM_RANGE(0x0000, 0xffff) AM_READWRITE(apple3_memory_r, apple3_memory_w)
ADDRESS_MAP_END

static SLOT_INTERFACE_START(apple3_cards)
	SLOT_INTERFACE("cffa2", A2BUS_CFFA2_6502)  /* CFFA2000 Compact Flash for Apple II (www.dreher.net), 6502 firmware */
	SLOT_INTERFACE("applicard", A2BUS_APPLICARD)    /* PCPI Applicard */
	SLOT_INTERFACE("thclock", A2BUS_THUNDERCLOCK)    /* ThunderWare ThunderClock Plus - driver assumes slot 2 by default */
	SLOT_INTERFACE("mouse", A2BUS_MOUSE)    /* Apple II Mouse Card */
SLOT_INTERFACE_END

static SLOT_INTERFACE_START( a3_floppies )
	SLOT_INTERFACE( "525", FLOPPY_525_SD )
SLOT_INTERFACE_END

FLOPPY_FORMATS_MEMBER( apple3_state::floppy_formats )
	FLOPPY_A216S_FORMAT, FLOPPY_RWTS18_FORMAT, FLOPPY_EDD_FORMAT
FLOPPY_FORMATS_END

static MACHINE_CONFIG_START( apple3, apple3_state )
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", M6502, 2000000)        /* 2 MHz */
	MCFG_M6502_SYNC_CALLBACK(WRITELINE(apple3_state, apple3_sync_w))
	MCFG_CPU_PROGRAM_MAP(apple3_map)
	MCFG_QUANTUM_TIME(attotime::from_hz(60))

	MCFG_MACHINE_RESET_OVERRIDE(apple3_state, apple3 )

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(2500)) /* not accurate */
	MCFG_SCREEN_SIZE((280*2)+32, 224)
	MCFG_SCREEN_VISIBLE_AREA(0, (280*2)-1,0,192-1)
	MCFG_SCREEN_UPDATE_DRIVER(apple3_state, screen_update_apple3)
	MCFG_SCREEN_PALETTE("palette")

	MCFG_PALETTE_ADD("palette", 32)
	MCFG_PALETTE_INIT_OWNER(apple3_state, apple3 )

	MCFG_VIDEO_START_OVERRIDE(apple3_state, apple3 )

	MCFG_TIMER_DRIVER_ADD_SCANLINE("scantimer", apple3_state, apple3_interrupt, "screen", 0, 1)

	/* keyboard controller */
	MCFG_DEVICE_ADD("ay3600", AY3600, 0)
	MCFG_AY3600_MATRIX_X0(IOPORT("X0"))
	MCFG_AY3600_MATRIX_X1(IOPORT("X1"))
	MCFG_AY3600_MATRIX_X2(IOPORT("X2"))
	MCFG_AY3600_MATRIX_X3(IOPORT("X3"))
	MCFG_AY3600_MATRIX_X4(IOPORT("X4"))
	MCFG_AY3600_MATRIX_X5(IOPORT("X5"))
	MCFG_AY3600_MATRIX_X6(IOPORT("X6"))
	MCFG_AY3600_MATRIX_X7(IOPORT("X7"))
	MCFG_AY3600_MATRIX_X8(IOPORT("X8"))
	MCFG_AY3600_SHIFT_CB(READLINE(apple3_state, ay3600_shift_r))
	MCFG_AY3600_CONTROL_CB(READLINE(apple3_state, ay3600_control_r))
	MCFG_AY3600_DATA_READY_CB(WRITELINE(apple3_state, ay3600_data_ready_w))

	/* slot bus */
	MCFG_DEVICE_ADD("a2bus", A2BUS, 0)
	MCFG_A2BUS_CPU("maincpu")
	MCFG_A2BUS_OUT_IRQ_CB(WRITELINE(apple3_state, a2bus_irq_w))
	MCFG_A2BUS_OUT_NMI_CB(WRITELINE(apple3_state, a2bus_nmi_w))
	//MCFG_A2BUS_OUT_INH_CB(WRITELINE(apple3_state, a2bus_inh_w))
	MCFG_A2BUS_SLOT_ADD("a2bus", "sl1", apple3_cards, nullptr)
	MCFG_A2BUS_SLOT_ADD("a2bus", "sl2", apple3_cards, nullptr)
	MCFG_A2BUS_SLOT_ADD("a2bus", "sl3", apple3_cards, nullptr)
	MCFG_A2BUS_SLOT_ADD("a2bus", "sl4", apple3_cards, nullptr)

	/* fdc */
	MCFG_DEVICE_ADD("fdc", APPLEIII_FDC, 1021800*2)
	MCFG_FLOPPY_DRIVE_ADD("0", a3_floppies, "525", apple3_state::floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD("1", a3_floppies, "525", apple3_state::floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD("2", a3_floppies, "525", apple3_state::floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD("3", a3_floppies, "525", apple3_state::floppy_formats)

	/* softlist for fdc */
	MCFG_SOFTWARE_LIST_ADD("flop525_list","apple3")

	/* acia */
	MCFG_DEVICE_ADD("acia", MOS6551, 0)
	MCFG_MOS6551_XTAL(XTAL_1_8432MHz) // HACK: The schematic shows an external clock generator but using a XTAL is faster to emulate.
	MCFG_MOS6551_IRQ_HANDLER(WRITELINE(apple3_state, apple3_acia_irq_func))
	MCFG_MOS6551_TXD_HANDLER(DEVWRITELINE("rs232", rs232_port_device, write_txd))
	MCFG_MOS6551_RTS_HANDLER(DEVWRITELINE("rs232", rs232_port_device, write_rts))
	MCFG_MOS6551_DTR_HANDLER(DEVWRITELINE("rs232", rs232_port_device, write_dtr))

	MCFG_RS232_PORT_ADD("rs232", default_rs232_devices, nullptr)
	MCFG_RS232_RXD_HANDLER(DEVWRITELINE("acia", mos6551_device, write_rxd))
	MCFG_RS232_DCD_HANDLER(DEVWRITELINE("acia", mos6551_device, write_dcd))
	MCFG_RS232_DSR_HANDLER(DEVWRITELINE("acia", mos6551_device, write_dsr))
	// TODO: remove cts kludge from machine/apple3.c and come up with a good way of coping with pull up resistors.

	/* paddle */
	MCFG_TIMER_DRIVER_ADD("pdltimer", apple3_state, paddle_timer);

	/* rtc */
	MCFG_DEVICE_ADD("rtc", MM58167, XTAL_32_768kHz)

	/* via */
	MCFG_DEVICE_ADD("via6522_0", VIA6522, 1000000)
	MCFG_VIA6522_WRITEPA_HANDLER(WRITE8(apple3_state, apple3_via_0_out_a))
	MCFG_VIA6522_WRITEPB_HANDLER(WRITE8(apple3_state, apple3_via_0_out_b))
	MCFG_VIA6522_IRQ_HANDLER(WRITELINE(apple3_state, apple3_via_0_irq_func))

	MCFG_DEVICE_ADD("via6522_1", VIA6522, 1000000)
	MCFG_VIA6522_WRITEPA_HANDLER(WRITE8(apple3_state, apple3_via_1_out_a))
	MCFG_VIA6522_WRITEPB_HANDLER(WRITE8(apple3_state, apple3_via_1_out_b))
	MCFG_VIA6522_IRQ_HANDLER(WRITELINE(apple3_state, apple3_via_1_irq_func))

	/* sound */
	MCFG_SPEAKER_STANDARD_MONO("speaker")
	MCFG_SOUND_ADD("bell", DAC_1BIT, 0) MCFG_SOUND_ROUTE(ALL_OUTPUTS, "speaker", 0.99)
	MCFG_SOUND_ADD("dac", DAC_6BIT_BINARY_WEIGHTED, 0) MCFG_SOUND_ROUTE(ALL_OUTPUTS, "speaker", 0.125) // 6522.b5(pb0-pb5) + 320k,160k,80k,40k,20k,10k
	MCFG_DEVICE_ADD("vref", VOLTAGE_REGULATOR, 0) MCFG_VOLTAGE_REGULATOR_OUTPUT(5.0)
	MCFG_SOUND_ROUTE_EX(0, "bell", 1.0, DAC_VREF_POS_INPUT)
	MCFG_SOUND_ROUTE_EX(0, "dac", 1.0, DAC_VREF_POS_INPUT) MCFG_SOUND_ROUTE_EX(0, "dac", -1.0, DAC_VREF_NEG_INPUT)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("c040", apple3_state, apple3_c040_tick, attotime::from_hz(2000))

	/* internal ram */
	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("256K")
	MCFG_RAM_EXTRA_OPTIONS("128K, 512K")
MACHINE_CONFIG_END


static INPUT_PORTS_START( apple3 )
	/*
	  KB3600 Keyboard matrix (KB3600 has custom layout mask ROM, Apple p/n 341-0035)

	      | Y0  | Y1  | Y2  | Y3  | Y4  | Y5  | Y6  | Y7  | Y8  | Y9  |
	      |     |     |     |     |     |     |     |     |     |     |
	  ----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----|
	  X0  | ESC |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |
	  ----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----|
	  X1  | TAB |  Q  |  W  |  E  |  R  |  T  |  Y  |  U  |  I  |  O  |
	  ----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----|
	  X2  |  A  |  S  |  D  |  F  |  H  |  G  |  J  |  K  |  ;: |  L  |
	  ----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----|
	  X3  |  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  <, |  >. |  ?/ |
	  ----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----|
	  X4  |     | KP9 |     | KP8 |     | KP7 |  \| |  += |  0  |  -_ |
	  ----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----|
	  X5  |     | KP6 |     | KP5 |     | KP4 |  `~ |  ]} |  P  | [{  |
	  ----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----|
	  X6  |     | KP3 | KP. | KP2 | KP0 | KP1 |RETRN| UP  |BACKS| '"  |
	  ----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----|
	  X7  |     |     |KPENT|SPACE|     | KP- |RIGHT|DOWN |LEFT |     |
	  ----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----|
	*/

/*
    Esc
    0x00

      `    1    2    3    4    5    6    7    8    9    0    -   =   BACKSPACE
    0x38 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x30 0x31 0x2f 0x104

     Tab   Q    W    E    R    T    Y    U   I     O    P    [    ]    \
    0x0a 0x0b 0x0c 0x0d 0x0e 0x0f 0x10 0x11 0x12 0x13 0x3a 0x3b 0x39 0x2e

      A    S    D    F    G    H    J    K    L    ;    '   ENTER
    0x14 0x15 0x16 0x17 0x19 0x18 0x1a 0x1b 0x1d 0x1c 0x105 0x102

      Z    X    C    V    B    N    M   ,     .    /
    0x1e 0x1f 0x20 0x21 0x22 0x23 0x24 0x25 0x26 0x27

    SPACE   UP    LT    DN    RT   KP-   KP7  KP8  KP9
    0x109  0x103 0x10e 0x10d 0x10c 0x10b 0x2d 0x2b 0x29

    KP4  KP5  KP6   KP1  KP2  KP3  KPEN  KP0   KP.
    0x37 0x35 0x33 0x101 0x3f 0x3d 0x108 0x100 0x3e
*/
	PORT_START("X0")
	PORT_BIT(0x001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Esc")      PORT_CODE(KEYCODE_ESC)      PORT_CHAR(27)
	PORT_BIT(0x002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_1)      PORT_CHAR('1') PORT_CHAR('!')
	PORT_BIT(0x004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_2)  PORT_CHAR('2') PORT_CHAR('\"')
	PORT_BIT(0x008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_3)  PORT_CHAR('3') PORT_CHAR('#')
	PORT_BIT(0x010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_4)  PORT_CHAR('4') PORT_CHAR('$')
	PORT_BIT(0x020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_5)  PORT_CHAR('5') PORT_CHAR('%')
	PORT_BIT(0x040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_6)  PORT_CHAR('6') PORT_CHAR('&')
	PORT_BIT(0x080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_7)  PORT_CHAR('7') PORT_CHAR('\'')
	PORT_BIT(0x100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_8)  PORT_CHAR('8') PORT_CHAR('(')
	PORT_BIT(0x200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_9)  PORT_CHAR('9') PORT_CHAR(')')

	PORT_START("X1")
	PORT_BIT(0x001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Tab")      PORT_CODE(KEYCODE_TAB)      PORT_CHAR(9)
	PORT_BIT(0x002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_Q)  PORT_CHAR('Q') PORT_CHAR('q')
	PORT_BIT(0x004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_W)  PORT_CHAR('W') PORT_CHAR('w')
	PORT_BIT(0x008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_E)  PORT_CHAR('E') PORT_CHAR('e')
	PORT_BIT(0x010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_R)  PORT_CHAR('R') PORT_CHAR('r')
	PORT_BIT(0x020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_T)  PORT_CHAR('T') PORT_CHAR('t')
	PORT_BIT(0x040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_Y)  PORT_CHAR('Y') PORT_CHAR('y')
	PORT_BIT(0x080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_U)  PORT_CHAR('U') PORT_CHAR('u')
	PORT_BIT(0x100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_I)  PORT_CHAR('I') PORT_CHAR('i')
	PORT_BIT(0x200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_O)  PORT_CHAR('O') PORT_CHAR('o')

	PORT_START("X2")
	PORT_BIT(0x001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_A)          PORT_CHAR('A') PORT_CHAR('a')
	PORT_BIT(0x002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_S)  PORT_CHAR('S') PORT_CHAR('s')
	PORT_BIT(0x004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_D)  PORT_CHAR('D') PORT_CHAR('d')
	PORT_BIT(0x008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_F)  PORT_CHAR('F') PORT_CHAR('f')
	PORT_BIT(0x010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_H)  PORT_CHAR('H') PORT_CHAR('h')
	PORT_BIT(0x020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_G)  PORT_CHAR('G') PORT_CHAR('g')
	PORT_BIT(0x040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_J)  PORT_CHAR('J') PORT_CHAR('j')
	PORT_BIT(0x080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_K)  PORT_CHAR('K') PORT_CHAR('k')
	PORT_BIT(0x100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_COLON)      PORT_CHAR(';') PORT_CHAR(':')
	PORT_BIT(0x200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_L)  PORT_CHAR('L') PORT_CHAR('l')

	PORT_START("X3")
	PORT_BIT(0x001, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_Z)  PORT_CHAR('Z') PORT_CHAR('z')
	PORT_BIT(0x002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_X)  PORT_CHAR('X') PORT_CHAR('x')
	PORT_BIT(0x004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_C)  PORT_CHAR('C') PORT_CHAR('c')
	PORT_BIT(0x008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_V)  PORT_CHAR('V') PORT_CHAR('v')
	PORT_BIT(0x010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_B)          PORT_CHAR('B') PORT_CHAR('b')
	PORT_BIT(0x020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_N)  PORT_CHAR('N') PORT_CHAR('n')
	PORT_BIT(0x040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_M)  PORT_CHAR('M') PORT_CHAR('m')
	PORT_BIT(0x080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_COMMA)  PORT_CHAR(',') PORT_CHAR('<')
	PORT_BIT(0x100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_STOP)   PORT_CHAR('.') PORT_CHAR('>')
	PORT_BIT(0x200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_SLASH)  PORT_CHAR('/') PORT_CHAR('?')

	PORT_START("X4")
	PORT_BIT(0x001, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_9_PAD)       PORT_CHAR(UCHAR_MAMEKEY(9_PAD))
	PORT_BIT(0x004, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_8_PAD)       PORT_CHAR(UCHAR_MAMEKEY(8_PAD))
	PORT_BIT(0x010, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_7_PAD)   PORT_CHAR(UCHAR_MAMEKEY(7_PAD))
	PORT_BIT(0x040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH)  PORT_CHAR('\\') PORT_CHAR('|')
	PORT_BIT(0x080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_EQUALS)     PORT_CHAR('=') PORT_CHAR('+')
	PORT_BIT(0x100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_0)      PORT_CHAR('0') PORT_CHAR(')')
	PORT_BIT(0x200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_MINUS)  PORT_CHAR('-') PORT_CHAR('_')

	PORT_START("X5")
	PORT_BIT(0x001, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_6_PAD)   PORT_CHAR(UCHAR_MAMEKEY(6_PAD))
	PORT_BIT(0x004, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_5_PAD)   PORT_CHAR(UCHAR_MAMEKEY(5_PAD))
	PORT_BIT(0x010, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_4_PAD)   PORT_CHAR(UCHAR_MAMEKEY(4_PAD))
	PORT_BIT(0x040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_TILDE)      PORT_CHAR('`') PORT_CHAR('~')
	PORT_BIT(0x080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_CLOSEBRACE) PORT_CHAR(']') PORT_CHAR('}')
	PORT_BIT(0x100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_P)  PORT_CHAR('P') PORT_CHAR('p')
	PORT_BIT(0x200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_OPENBRACE)  PORT_CHAR('[') PORT_CHAR('{')

	PORT_START("X6")
	PORT_BIT(0x001, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x002, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_3_PAD)   PORT_CHAR(UCHAR_MAMEKEY(3_PAD))
	PORT_BIT(0x004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_DEL_PAD)     PORT_CHAR(UCHAR_MAMEKEY(DEL_PAD))
	PORT_BIT(0x008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_2_PAD)   PORT_CHAR(UCHAR_MAMEKEY(2_PAD))
	PORT_BIT(0x010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_0_PAD)   PORT_CHAR(UCHAR_MAMEKEY(0_PAD))
	PORT_BIT(0x020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_1_PAD)   PORT_CHAR(UCHAR_MAMEKEY(1_PAD))
	PORT_BIT(0x040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Return")   PORT_CODE(KEYCODE_ENTER)    PORT_CHAR(13)
	PORT_BIT(0x080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(UTF8_UP)        PORT_CODE(KEYCODE_UP)
	PORT_BIT(0x100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Delete")   PORT_CODE(KEYCODE_BACKSPACE)PORT_CHAR(8)
	PORT_BIT(0x200, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_QUOTE)  PORT_CHAR('\'') PORT_CHAR('\"')

	PORT_START("X7")
	PORT_BIT(0x001, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x002, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x004, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_ENTER_PAD)   PORT_CHAR(UCHAR_MAMEKEY(ENTER_PAD))
	PORT_BIT(0x008, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_SPACE)  PORT_CHAR(' ')
	PORT_BIT(0x010, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x020, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_CODE(KEYCODE_MINUS_PAD)   PORT_CHAR(UCHAR_MAMEKEY(MINUS_PAD))
	PORT_BIT(0x040, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(UTF8_RIGHT)     PORT_CODE(KEYCODE_RIGHT)
	PORT_BIT(0x080, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(UTF8_DOWN)      PORT_CODE(KEYCODE_DOWN)     PORT_CHAR(10)
	PORT_BIT(0x100, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME(UTF8_LEFT)      PORT_CODE(KEYCODE_LEFT)
	PORT_BIT(0x200, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START("X8")
	PORT_BIT(0x001, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x002, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x004, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x008, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x010, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x020, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x040, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x080, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x100, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x200, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START("keyb_special")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Caps Lock")    PORT_CODE(KEYCODE_CAPSLOCK) PORT_TOGGLE
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Left Shift")   PORT_CODE(KEYCODE_LSHIFT)   PORT_CHAR(UCHAR_SHIFT_1)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Right Shift")  PORT_CODE(KEYCODE_RSHIFT)   PORT_CHAR(UCHAR_SHIFT_1)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Control")      PORT_CODE(KEYCODE_LCONTROL) PORT_CHAR(UCHAR_SHIFT_2)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Open Apple")   PORT_CODE(KEYCODE_LALT)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Solid Apple")  PORT_CODE(KEYCODE_RALT)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("RESET")        PORT_CODE(KEYCODE_F12)

	PORT_START("joy_1_x")      /* Joystick 1 X Axis */
	PORT_BIT( 0xff, 0x80, IPT_AD_STICK_X) PORT_SENSITIVITY(100) PORT_KEYDELTA(1) PORT_NAME("P1 Joystick X")
	PORT_MINMAX(0, 0xff) PORT_PLAYER(1)

	PORT_START("joy_1_y")      /* Joystick 1 Y Axis */
	PORT_BIT( 0xff, 0x80, IPT_AD_STICK_Y) PORT_SENSITIVITY(100) PORT_KEYDELTA(1) PORT_NAME("P1 Joystick Y")
	PORT_MINMAX(0,0xff) PORT_PLAYER(1)

	PORT_START("joy_2_x")      /* Joystick 2 X Axis */
	PORT_BIT( 0xff, 0x80, IPT_AD_STICK_X) PORT_SENSITIVITY(100) PORT_KEYDELTA(1) PORT_NAME("P2 Joystick X")
	PORT_MINMAX(0,0xff) PORT_PLAYER(2)

	PORT_START("joy_2_y")      /* Joystick 2 Y Axis */
	PORT_BIT( 0xff, 0x80, IPT_AD_STICK_Y) PORT_SENSITIVITY(100) PORT_KEYDELTA(1) PORT_NAME("P2 Joystick Y")
	PORT_MINMAX(0,0xff) PORT_PLAYER(2)

	PORT_START("joy_buttons")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_PLAYER(1) PORT_NAME("Joystick 1 Button 1")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_PLAYER(1) PORT_NAME("Joystick 1 Button 2")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_PLAYER(2) PORT_NAME("Joystick 2 Button 1")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_PLAYER(2) PORT_NAME("Joystick 2 Button 2")
INPUT_PORTS_END

ROM_START(apple3)
	ROM_REGION(0x1000,"maincpu",0)
	ROM_LOAD( "apple3.rom", 0x0000, 0x1000, CRC(55e8eec9) SHA1(579ee4cd2b208d62915a0aa482ddc2744ff5e967))
ROM_END

/*     YEAR     NAME        PARENT  COMPAT  MACHINE    INPUT    INIT    COMPANY             FULLNAME */
COMP( 1980, apple3,     0,      0,      apple3,    apple3, apple3_state,    apple3,     "Apple Computer",   "Apple ///", MACHINE_SUPPORTS_SAVE )
