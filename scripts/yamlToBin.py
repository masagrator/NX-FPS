import ruamel.yaml
import sys
from pathlib import Path
import struct
import keystone

compares = [">", ">=", "<", "<=", "==", "!="]

def returnCompare(compare_type: str, value_type: str, value) -> bytes:
	entry = []
	index = compares.index(compare_type)
	if (index < 0):
		print("Wrong compare type!")
		sys.exit()
	index += 1
	entry.append(index.to_bytes(1, "little"))
	match(value_type):
		case "uint8":
			entry.append(0x1.to_bytes(1, "little"))
			entry.append(value.to_bytes(1, "little"))
		case "uint16":
			entry.append(0x2.to_bytes(1, "little"))
			entry.append(value.to_bytes(2, "little"))
		case "uint32":
			entry.append(0x4.to_bytes(1, "little"))
			entry.append(value.to_bytes(4, "little"))
		case "uint64":
			entry.append(0x8.to_bytes(1, "little"))
			entry.append(value.to_bytes(8, "little"))
		case "int8":
			entry.append(0x11.to_bytes(1, "little"))
			entry.append(value.to_bytes(1, "little", signed=True))
		case "int16":
			entry.append(0x12.to_bytes(1, "little"))
			entry.append(value.to_bytes(2, "little", signed=True))
		case "int32":
			entry.append(0x14.to_bytes(1, "little"))
			entry.append(value.to_bytes(4, "little", signed=True))
		case "int64":
			entry.append(0x18.to_bytes(1, "little"))
			entry.append(value.to_bytes(8, "little", signed=True))
		case "float":
			entry.append(0x24.to_bytes(1, "little"))
			entry.append(struct.pack('<f', value))
		case "double":
			entry.append(0x28.to_bytes(1, "little"))
			entry.append(struct.pack('<d', value))
		case _:
			print("Wrong type!")
			sys.exit()
	return b"".join(entry)
	

def returnValue(value_type: str, value) -> bytes:
	entry = []
	if isinstance(value, list) == True:
		loops = len(value)
	else: 
		loops = 1
		value = [value]
	match(value_type):
		case "uint8":
			entry.append(0x1.to_bytes(1, "little"))
			entry.append(loops.to_bytes(1, "little"))
			for i in range(loops):
				entry.append(value[i].to_bytes(1, "little"))
		case "uint16":
			entry.append(0x2.to_bytes(1, "little"))
			entry.append(loops.to_bytes(1, "little"))
			for i in range(loops):
				entry.append(value[i].to_bytes(2, "little"))
		case "uint32":
			entry.append(0x4.to_bytes(1, "little"))
			entry.append(loops.to_bytes(1, "little"))
			for i in range(loops):
				entry.append(value[i].to_bytes(4, "little"))
		case "uint64":
			entry.append(0x8.to_bytes(1, "little"))
			entry.append(loops.to_bytes(1, "little"))
			for i in range(loops):
				entry.append(value[i].to_bytes(8, "little"))
		case "int8":
			entry.append(0x11.to_bytes(1, "little"))
			entry.append(loops.to_bytes(1, "little"))
			for i in range(loops):
				entry.append(value[i].to_bytes(1, "little", signed=True))
		case "int16":
			entry.append(0x12.to_bytes(1, "little"))
			entry.append(loops.to_bytes(1, "little"))
			for i in range(loops):
				entry.append(value[i].to_bytes(2, "little", signed=True))
		case "int32":
			entry.append(0x14.to_bytes(1, "little"))
			entry.append(loops.to_bytes(1, "little"))
			for i in range(loops):
				entry.append(value[i].to_bytes(4, "little", signed=True))
		case "int64":
			entry.append(0x18.to_bytes(1, "little"))
			entry.append(loops.to_bytes(1, "little"))
			for i in range(loops):
				entry.append(value[i].to_bytes(8, "little", signed=True))
		case "float":
			entry.append(0x24.to_bytes(1, "little"))
			entry.append(loops.to_bytes(1, "little"))
			for i in range(loops):
				entry.append(struct.pack('<f', value[i]))
		case "double":
			entry.append(0x28.to_bytes(1, "little"))
			entry.append(loops.to_bytes(1, "little"))
			for i in range(loops):
				entry.append(struct.pack('<d', value[i]))
		case _:
			print("Wrong type!")
			sys.exit()
	return b"".join(entry)

def returnAssembly(entry_list) -> bytes:
	ks = keystone.Ks(keystone.KS_ARCH_ARM64, keystone.KS_MODE_LITTLE_ENDIAN)
	entry = []
	if isinstance(entry_list, list) == True:
		loops = len(entry_list)
	else: 
		loops = 1
		entry_list = [entry_list]
	entry.append(loops.to_bytes(1, "little"))
	encoding, count = ks.asm(";".join(entry_list), as_bytes=True)
	entry.append(encoding)
	return b"".join(entry)


file = open(sys.argv[1], "r", encoding="ascii")
DICT = ruamel.yaml.safe_load(file.read())
file.close()

DATA = []

OBJECTS = ["15FPS", "20FPS", "25FPS", "30FPS", "35FPS", "40FPS", "45FPS", "50FPS", "55FPS", "60FPS"]

version = 1

MASTER_WRITE_TEMP = b""

if "MASTER_WRITE" in DICT.keys():
	version = 2
	entry = []
	for x in range(len(DICT["MASTER_WRITE"])):
		match(DICT["MASTER_WRITE"][x]["type"]):
			case "bytes":
				entry.append(b"\x01")
				entry.append(DICT["MASTER_WRITE"][x]["main_offset"].to_bytes(4, "little", signed=True))
				entry.append(returnValue(DICT["MASTER_WRITE"][x]["value_type"], DICT["MASTER_WRITE"][x]["value"]))
			case "assembly":
				entry.append(b"\x04")
				if (DICT["MASTER_WRITE"][x]["main_offset"] % 4 != 0):
					print("Assembly main_offset must be divisible by 4!")
					print("Wrong offset: 0x%x" % DICT["MASTER_WRITE"][x]["main_offset"])
					sys.exit()
				entry.append(DICT["MASTER_WRITE"][x]["main_offset"].to_bytes(4, "little", signed=True))
				entry.append(returnAssembly(DICT["MASTER_WRITE"][x]["instructions"]))
	entry.append(b"\xFF")
	MASTER_WRITE_TEMP = b"".join(entry)

for i in range(len(OBJECTS)):
	entry = []
	for x in range(len(DICT[OBJECTS[i]])):
		match(DICT[OBJECTS[i]][x]["type"]):
			case "write":
				entry.append(b"\x01")
				address_count = len(DICT[OBJECTS[i]][x]["address"])
				entry.append(address_count.to_bytes(1, "little"))
				for y in range(address_count):
					if (y == 0):
						match(DICT[OBJECTS[i]][x]["address"][0].upper()):
							case "MAIN":
								entry.append(b"\x01")
							case "HEAP":
								entry.append(b"\x02")
							case "ALIAS":
								entry.append(b"\x03")
							case _:
								print("UNKNOWN REGION!")
								sys.exit()
						continue
					entry.append(DICT[OBJECTS[i]][x]["address"][y].to_bytes(4, "little", signed=True))
				entry.append(returnValue(DICT[OBJECTS[i]][x]["value_type"], DICT[OBJECTS[i]][x]["value"]))
			case "compare":
				entry.append(b"\x02")
				compare_address_count = len(DICT[OBJECTS[i]][x]["compare_address"])
				entry.append(compare_address_count.to_bytes(1, "little"))
				for y in range(compare_address_count):
					if (y == 0):
						match(DICT[OBJECTS[i]][x]["compare_address"][0].upper()):
							case "MAIN":
								entry.append(b"\x01")
							case "HEAP":
								entry.append(b"\x02")
							case "ALIAS":
								entry.append(b"\x03")
							case _:
								print("UNKNOWN REGION!")
								sys.exit()
						continue
					entry.append(DICT[OBJECTS[i]][x]["compare_address"][y].to_bytes(4, "little", signed=True))
				if isinstance(DICT[OBJECTS[i]][x]["compare_value"], list) == True:
					DICT[OBJECTS[i]][x]["compare_value"] = DICT[OBJECTS[i]][x]["compare_value"][0]
				entry.append(returnCompare(DICT[OBJECTS[i]][x]["compare_type"], DICT[OBJECTS[i]][x]["compare_value_type"], DICT[OBJECTS[i]][x]["compare_value"]))
				address_count = len(DICT[OBJECTS[i]][x]["address"])
				entry.append(address_count.to_bytes(1, "little"))
				for y in range(address_count):
					if (y == 0):
						match(DICT[OBJECTS[i]][x]["address"][0].upper()):
							case "MAIN":
								entry.append(b"\x01")
							case "HEAP":
								entry.append(b"\x02")
							case "ALIAS":
								entry.append(b"\x03")
							case _:
								print("UNKNOWN REGION!")
								sys.exit()
						continue
					entry.append(DICT[OBJECTS[i]][x]["address"][y].to_bytes(4, "little", signed=True))
				entry.append(returnValue(DICT[OBJECTS[i]][x]["value_type"], DICT[OBJECTS[i]][x]["value"]))
			case "block":
				entry.append(b"\x03")
				match(DICT[OBJECTS[i]][x]["what"]):
					case "timing":
						entry.append(b"\x01")
					case _:
						print("Unknown block type: %s" % entry["what"])
						sys.exit()
			case _:
				print(f"Unknown type of patch at {OBJECTS[i]}!")
				print(DICT[OBJECTS[i]])
				sys.exit()
	entry.append(b"\xFF")
	DATA.append(b"".join(entry))

if (version == 2):
	DATA.append(MASTER_WRITE_TEMP)

new_file = open(f"{Path(sys.argv[1]).stem}.bin", "wb")
new_file.write(b"LOCK")
new_file.write(version.to_bytes(3, "little"))
if (DICT["unsafeCheck"] == True):
	new_file.write(b"\x01")
else:
	new_file.write(b"\x00")
base = new_file.tell() + (4 * len(DATA))
offsets = []
NEW_DATA = []
for i in range(len(DATA)):
	if (DATA.index(DATA[i]) < i):
		offset = offsets[DATA.index(DATA[i])]
		new_file.write(offset.to_bytes(4, "little"))
		offsets.append(offset)
	else:
		new_file.write(base.to_bytes(4, "little"))
		offsets.append(base)
		NEW_DATA.append(DATA[i])
		base += len(DATA[i])
new_file.write(b"".join(NEW_DATA))
new_file.close()