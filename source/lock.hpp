#pragma once
#define NOINLINE __attribute__ ((noinline))
/* Design file how to build binary file for FPSLocker.

1. Helper functions */

namespace LOCK {

	int64_t main_address = 0;
	uint32_t offset = 0;

	template <typename T>
	bool compareValues(T value1, T value2, uint8_t compare_type) { // 1 - >, 2 - >=, 3 - <, 4 - <=, 5 - ==, 6 - !=
		switch(compare_type) {
			case 1:
				return (value1 > value2);
			case 2:
				return (value1 >= value2);
			case 3:
				return (value1 < value2);
			case 4:
				return (value1 <= value2);
			case 5:
				return (value1 == value2);
			case 6:
				return (value1 != value2);
		}
		return false;
	}

	uint8_t read8(uint8_t* buffer) {
		uint8_t ret = buffer[offset];
		offset += 1;
		return ret;
	}

	uint16_t read16(uint8_t* buffer) {
		uint16_t ret = *(uint16_t*)(&buffer[offset]);
		offset += 2;
		return ret;
	}

	uint32_t read32(uint8_t* buffer) {
		uint32_t ret = *(uint32_t*)(&buffer[offset]);
		offset += 4;
		return ret;
	}

	uint64_t read64(uint8_t* buffer) {
		uint64_t ret = *(uint64_t*)(&buffer[offset]);
		offset += 8;
		return ret;
	}

	float readFloat(uint8_t* buffer) {
		float ret = *(float*)(&buffer[offset]);
		offset += 4;
		return ret;
	}

	double readDouble(uint8_t* buffer) {
		double ret = *(double*)(&buffer[offset]);
		offset += 8;
		return ret;
	}

	bool* unsafeCheck = 0;

	bool NOINLINE isAddressValid(int64_t address) {
		MemoryInfo memoryinfo = {0};
		u32 pageinfo = 0;

		if (unsafeCheck && *unsafeCheck) return true;

		if ((address < 0) || (address >= 0x8000000000)) return false;

		Result rc = svcQueryMemory(&memoryinfo, &pageinfo, address);
		if (R_FAILED(rc)) return false;
		if ((memoryinfo.perm & Perm_Rw) && ((address - memoryinfo.addr >= 0) && (address - memoryinfo.addr <= memoryinfo.size)))
			return true;
		return false;
	}

	int64_t NOINLINE getAddress(uint8_t* buffer, uint8_t offsets_count) {
		int64_t address = main_address;
		for (int i = 0; i < offsets_count; i++) {
			int32_t temp_offset = (int32_t)read32(buffer);
			address += temp_offset;
			if (i+1 < offsets_count) {
				if (!isAddressValid(*(int64_t*)address)) return -2;
				address = *(uint64_t*)address;
			}
		}
		return address;
	}


///2. File format and reading

	bool isValid(uint8_t* buffer, size_t filesize) {
		if (*(uint32_t*)buffer != 0x4B434F4C)
			return false;
		if (*(uint32_t*)(&(buffer[4])) != 44)
			return false;
		uint32_t offset = *(uint32_t*)(&(buffer[10]));
		if (offset > filesize)
			return false;
		if (buffer[filesize-1] != 0xFF)
			return false;
		return true;

	}

	Result applyPatch(uint8_t* buffer, size_t filesize, uint8_t FPS) {
		FPS -= 15;
		FPS /= 5;
		FPS *= 4;
		offset = *(uint32_t*)(&buffer[FPS+4]);
		int8_t OPCODE = read8(buffer); // 0 - err, 1 - write, 2 - compare, -1 = endExecution
		if (OPCODE == 1) {
			uint8_t offsets_count = read8(buffer);
			int64_t address = getAddress(buffer, offsets_count);
			if (address < 0) 
				return 6;
			uint8_t value_type = read8(buffer); // 1 - uint8, 2 - uint16, 4 - uint32, 8 - uint64, 0x11 - int8, 0x12 - int16, 0x14 - int32, 0x18 - int64, 0x24 - float, 0x28 - double
			uint8_t loops = read8(buffer);
			switch(value_type) {
				case 1:
				case 0x11: {
					for (uint8_t i = 0; i < loops; i++) {
						*(uint8_t*)address = read8(buffer);
						address += 1;
					}
					break;
				}
				case 2:
				case 0x12: {
					for (uint8_t i = 0; i < loops; i++) {
						*(uint16_t*)address = read16(buffer);
						address += 2;
					}
					break;
				}
				case 4:
				case 0x14:
				case 0x24: {
					for (uint8_t i = 0; i < loops; i++) {
						*(uint32_t*)address = read32(buffer);
						address += 4;
					}
					break;
				}
				case 8:
				case 0x18:
				case 0x28: {
					for (uint8_t i = 0; i < loops; i++) {
						*(uint64_t*)address = read64(buffer);
						address += 8;
					}
					break;
				}
				default:
					return 3;
			}
		}
		else if (OPCODE == 2) {
			uint8_t offsets_count = read8(buffer);
			int64_t address = getAddress(buffer, offsets_count);
			if (address < 0) 
				return 6;

			uint8_t compare_type = read8(buffer); // 1 - >, 2 - >=, 3 - <, 4 - <=, 5 - ==, 6 - !=
			uint8_t value_type = read8(buffer); // 1 - uint8, 2 - uint16, 4 - uint32, 8 - uint64, 0x11 - int8, 0x12 - int16, 0x14 - int32, 0x18 - int64, 0x24 - float, 0x28 - double
			bool passed = false;
			switch(value_type) {
				case 1: {
					uint8_t uint8_compare = *(uint8_t*)address;
					uint8_t uint8_tocompare = read8(buffer);
					passed = compareValues(uint8_compare, uint8_tocompare, compare_type);
					break;
				}
				case 2: {
					uint16_t uint16_compare = *(uint16_t*)address;
					uint16_t uint16_tocompare = read16(buffer);
					passed = compareValues(uint16_compare, uint16_tocompare, compare_type);
					break;
				}
				case 4: {
					uint32_t uint32_compare = *(uint32_t*)address;
					uint32_t uint32_tocompare = read32(buffer);
					passed = compareValues(uint32_compare, uint32_tocompare, compare_type);
					break;
				}
				case 8: {
					uint64_t uint64_compare = *(uint64_t*)address;
					uint64_t uint64_tocompare = read64(buffer);
					passed = compareValues(uint64_compare, uint64_tocompare, compare_type);
					break;
				}
				case 0x11: {
					int8_t int8_compare = *(int8_t*)address;
					int8_t int8_tocompare = (int8_t)read8(buffer);
					passed = compareValues(int8_compare, int8_tocompare, compare_type);
					break;
				}
				case 0x12: {
					int16_t int16_compare = *(int16_t*)address;
					int16_t int16_tocompare = (int16_t)read16(buffer);
					passed = compareValues(int16_compare, int16_tocompare, compare_type);
					break;
				}
				case 0x14: {
					int32_t int32_compare = *(int32_t*)address;
					int32_t int32_tocompare = (int32_t)read32(buffer);
					passed = compareValues(int32_compare, int32_tocompare, compare_type);
					break;
				}
				case 0x18: {
					int64_t int64_compare = *(int64_t*)address;
					int64_t int64_tocompare = (int64_t)read64(buffer);
					passed = compareValues(int64_compare, int64_tocompare, compare_type);
					break;
				}
				case 0x24: {
					float float_compare = *(float*)address;
					float float_tocompare = readFloat(buffer);
					passed = compareValues(float_compare, float_tocompare, compare_type);
					break;
				}
				case 0x28: {
					double double_compare = *(double*)address;
					double double_tocompare = readDouble(buffer);
					passed = compareValues(double_compare, double_tocompare, compare_type);
					break;
				}
				default:
					return 3;
			}

			offsets_count = read8(buffer);
			address = getAddress(buffer, offsets_count);
			if (address < 0) 
				return 6;
			value_type = read8(buffer); // 1 - uint8, 2 - uint16, 4 - uint32, 8 - uint64, 0x11 - int8, 0x12 - int16, 0x14 - int32, 0x18 - int64, 0x24 - float, 0x28 - double
			uint8_t loops = read8(buffer);
			switch(value_type) {
				case 1:
				case 0x11: {
					for (uint8_t i = 0; i < loops; i++) {
						uint8_t value8 = read8(buffer);
						if (passed) *(uint8_t*)address = value8;
						address += 1;
					}
					break;
				}
				case 2:
				case 0x12: {
					for (uint8_t i = 0; i < loops; i++) {
						uint16_t value16 = read16(buffer);
						if (passed) *(uint16_t*)address = value16;
						address += 2;
					}
					break;
				}
				case 4:
				case 0x14:
				case 0x24: {
					for (uint8_t i = 0; i < loops; i++) {
						uint32_t value32 = read32(buffer);
						if (passed) *(uint32_t*)address = value32;
						address += 4;
					}
					break;
				}
				case 8:
				case 0x18:
				case 0x28: {
					for (uint8_t i = 0; i < loops; i++) {
						uint64_t value64 = read64(buffer);
						if (passed) *(uint64_t*)address = value64;
						address += 8;
					}
					break;
				}
				default:
					return 3;
			}
		}
		else if (OPCODE == -1) {
			return -1;
		}
		return 255;
	}
}