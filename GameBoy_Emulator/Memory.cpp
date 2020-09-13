#include "Memory.h"
#include "PPU.h"
#include "Timer.h"
#include "Cartridge.h"


std::array<uint8_t, 256> Memory::boot_rom;


Memory::Memory()
{
	memset(mem.data(), 0, 0x10000);
}


Memory::Memory(std::istream& file)
{
	file.seekg(0x148); // 0148 - ROM Size
	uint8_t rom_size_flag = file.get();
	if (rom_size_flag > 8)
		throw std::runtime_error("unsupported rom size");
	size_t rom_size = (32 * KB) << rom_size_flag;

	std::vector<uint8_t> data(rom_size);
	file.seekg(0);
	file.read(reinterpret_cast<char*>(data.data()), rom_size);
	if (file.gcount() != rom_size)
		throw std::runtime_error("corrupted cartridge file");
	cartridge = create_cartridge(data);
	boot_rom =
	{
		0x31, 0xFE, 0xFF, 0xAF, 0x21, 0xFF, 0x9F, 0x32, 0xCB, 0x7C, 0x20, 0xFB, 0x21, 0x26, 0xFF, 0x0E,
		0x11, 0x3E, 0x80, 0x32, 0xE2, 0x0C, 0x3E, 0xF3, 0xE2, 0x32, 0x3E, 0x77, 0x77, 0x3E, 0xFC, 0xE0,
		0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1A, 0xCD, 0x95, 0x00, 0xCD, 0x96, 0x00, 0x13, 0x7B,
		0xFE, 0x34, 0x20, 0xF3, 0x11, 0xD8, 0x00, 0x06, 0x08, 0x1A, 0x13, 0x22, 0x23, 0x05, 0x20, 0xF9,
		0x3E, 0x19, 0xEA, 0x10, 0x99, 0x21, 0x2F, 0x99, 0x0E, 0x0C, 0x3D, 0x28, 0x08, 0x32, 0x0D, 0x20,
		0xF9, 0x2E, 0x0F, 0x18, 0xF3, 0x67, 0x3E, 0x64, 0x57, 0xE0, 0x42, 0x3E, 0x91, 0xE0, 0x40, 0x04,
		0x1E, 0x02, 0x0E, 0x0C, 0xF0, 0x44, 0xFE, 0x90, 0x20, 0xFA, 0x0D, 0x20, 0xF7, 0x1D, 0x20, 0xF2,
		0x0E, 0x13, 0x24, 0x7C, 0x1E, 0x83, 0xFE, 0x62, 0x28, 0x06, 0x1E, 0xC1, 0xFE, 0x64, 0x20, 0x06,
		0x7B, 0xE2, 0x0C, 0x3E, 0x87, 0xE2, 0xF0, 0x42, 0x90, 0xE0, 0x42, 0x15, 0x20, 0xD2, 0x05, 0x20,
		0x4F, 0x16, 0x20, 0x18, 0xCB, 0x4F, 0x06, 0x04, 0xC5, 0xCB, 0x11, 0x17, 0xC1, 0xCB, 0x11, 0x17,
		0x05, 0x20, 0xF5, 0x22, 0x23, 0x22, 0x23, 0xC9, 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
		0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
		0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
		0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E, 0x3C, 0x42, 0xB9, 0xA5, 0xB9, 0xA5, 0x42, 0x3C,
		0x21, 0x04, 0x01, 0x11, 0xA8, 0x00, 0x1A, 0x13, 0xBE, 0x20, 0xFE, 0x23, 0x7D, 0xFE, 0x34, 0x20,
		0xF5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xFB, 0x86, 0x20, 0xFE, 0x3E, 0x01, 0xE0, 0x50
	};
}

void Memory::set_timer(Timer* timer)
{
	this->timer = timer;
}

void Memory::set_PPU(PPU* ppu)
{
	this->ppu = ppu;
}

uint8_t Memory::read_byte(uint16_t addr) const
{
	if (boot_mode && addr < 0x100)
		return boot_rom[addr];

	else if (IN_RANGE(addr, ADDR_BANK_0_START, ADDR_SWITCH_BANK_END)
		|| IN_RANGE(addr, ADDR_EXTERNAL_RAM_START, ADDR_EXTERNAL_RAM_END))
		return cartridge->read_byte(addr);
	else if (IN_RANGE(addr, ADDR_VRAM_START, ADDR_VRAM_END))
		return ppu->read_byte_VRAM(addr);
	else if (IN_RANGE(addr, ADDR_OAM_START, ADDR_OAM_END))
		return ppu->read_byte_OAM(addr);

	else
		return mem[addr];
}

void Memory::write_byte(uint16_t addr, uint8_t value)
{
	if (addr == 0xFF46)
		init_dma_transfer(value);
	else if (addr == 0xFF00)
		value |= 0x0F;
	else if (boot_mode && addr == 0xFF50 && value == 1) // turn off boot ROM
		init();

	else if (IN_RANGE(addr, ADDR_VRAM_START, ADDR_VRAM_END))
		return ppu->write_byte_VRAM(addr, value);
	else if (IN_RANGE(addr, ADDR_OAM_START, ADDR_OAM_END))
		return ppu->write_byte_OAM(addr, value);
	else if (IN_RANGE(addr, ADDR_IO_DIV, ADDR_IO_TAC))
		return timer->process_timer_IO_write(addr, value);
	else if (IN_RANGE(addr, ADDR_BANK_0_START, ADDR_SWITCH_BANK_END)
		|| IN_RANGE(addr, ADDR_EXTERNAL_RAM_START, ADDR_EXTERNAL_RAM_END))
		return cartridge->write_byte(addr, value);

	mem[addr] = value;
}

void Memory::write_two_bytes(uint16_t addr, uint16_t value)
{
	write_byte(addr, value & 0xFF);
	write_byte(addr + 1, (value >> 8) & 0xFF);
}

void Memory::write_bytes(uint16_t addr, std::initializer_list<uint8_t> l)
{
	std::copy(l.begin(), l.end(), mem.data() + addr);
}

void Memory::init_dma_transfer(uint8_t value)
{
	if (dma_transfer_mode)
		throw std::runtime_error("OAM transfer already running");
	dma_transfer_mode = true;
	machine_cycles = 0;
	uint16_t src_addr = value * 0x100;
	
	if (IN_RANGE(src_addr, ADDR_BANK_0_START, ADDR_BANK_0_END))
		memcpy(ppu->OAM.data(), cartridge->get_rom_bank_0() + src_addr - ADDR_BANK_0_START, SIZE_OAM);
	else if (IN_RANGE(src_addr, ADDR_SWITCH_BANK_START, ADDR_SWITCH_BANK_END))
		memcpy(ppu->OAM.data(), cartridge->get_rom_switch_bank() + src_addr - ADDR_SWITCH_BANK_START, SIZE_OAM);
	else if (IN_RANGE(src_addr, ADDR_VRAM_START, ADDR_VRAM_END))
		memcpy(ppu->OAM.data(), ppu->VRAM.data() + src_addr - ADDR_VRAM_START, SIZE_OAM);
	else if (IN_RANGE(src_addr, ADDR_EXTERNAL_RAM_START, ADDR_EXTERNAL_RAM_END))
		memcpy(ppu->OAM.data(), cartridge->get_ram() + src_addr - ADDR_EXTERNAL_RAM_START, SIZE_OAM);
	else if (IN_RANGE(src_addr, ADDR_OAM_START, ADDR_OAM_END))
		; // do nothing
	else
		memcpy(ppu->OAM.data(), mem.data() + src_addr, SIZE_OAM);
}