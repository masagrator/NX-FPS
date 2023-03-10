import yaml
import sys
from pathlib import Path
import struct

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


file = open(sys.argv[1], "r", encoding="ascii")
DICT = yaml.safe_load(file.read())
file.close()

DATA = []

OBJECTS = ["15FPS", "20FPS", "25FPS", "30FPS", "35FPS", "40FPS", "45FPS", "50FPS", "55FPS", "60FPS"]

for i in range(len(OBJECTS)):
	entry = []
	for x in range(len(DICT[OBJECTS[i]])):
		match(DICT[OBJECTS[i]][x]["type"]):
			case "write":
				entry.append(b"\x01")
				address_count = len(DICT[OBJECTS[i]][x]["address"])
				entry.append(address_count.to_bytes(1, "little"))
				for y in range(address_count):
					entry.append(DICT[OBJECTS[i]][x]["address"][y].to_bytes(4, "little", signed=True))
				if isinstance(DICT[OBJECTS[i]][x]["value"], list) == True:
					DICT[OBJECTS[i]][x]["value"] = DICT[OBJECTS[i]][x]["value"][0]
				entry.append(returnValue(DICT[OBJECTS[i]][x]["value_type"], DICT[OBJECTS[i]][x]["value"]))
			case "compare":
				entry.append(b"\x02")
				compare_address_count = len(DICT[OBJECTS[i]][x]["compare_address"])
				entry.append(compare_address_count.to_bytes(1, "little"))
				for y in range(compare_address_count):
					entry.append(DICT[OBJECTS[i]][x]["compare_address"][y].to_bytes(4, "little", signed=True))
				if isinstance(DICT[OBJECTS[i]][x]["compare_value"], list) == True:
					DICT[OBJECTS[i]][x]["compare_value"] = DICT[OBJECTS[i]][x]["compare_value"][0]
				entry.append(returnCompare(DICT[OBJECTS[i]][x]["compare_type"], DICT[OBJECTS[i]][x]["compare_value_type"], DICT[OBJECTS[i]][x]["compare_value"]))
				address_count = len(DICT[OBJECTS[i]][x]["address"])
				entry.append(address_count.to_bytes(1, "little"))
				for y in range(address_count):
					entry.append(DICT[OBJECTS[i]][x]["address"][y].to_bytes(4, "little", signed=True))
				if isinstance(DICT[OBJECTS[i]][x]["value"], list) == True:
					DICT[OBJECTS[i]][x]["value"] = DICT[OBJECTS[i]][x]["value"][0]
				entry.append(returnValue(DICT[OBJECTS[i]][x]["value_type"], DICT[OBJECTS[i]][x]["value"]))
			case _:
				print(f"Unknown type of patch at {OBJECTS[i]}!")
				print(DICT[OBJECTS[i]])
				sys.exit()
	entry.append(b"\xFF")
	DATA.append(b"".join(entry))

new_file = open(f"{Path(sys.argv[1]).stem}.bin", "wb")
new_file.write(b"LOCK")
base = 44
for i in range(len(OBJECTS)):
	new_file.write(base.to_bytes(4, "little"))
	base += len(DATA[i])
new_file.write(b"".join(DATA))
new_file.close()