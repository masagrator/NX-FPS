import ruamel.yaml
import sys
import os
import struct
from pathlib import Path

compares = [">", ">=", "<", "<=", "==", "!="]

def GetCompareType(file):
	compare_type = int.from_bytes(file.read(1), "little", signed=True)
	compare_type -= 1
	return compares[compare_type]

def GetValue(file, type: str):
	match(type):
		case "uint8":
			return int.from_bytes(file.read(1), "little")
		case "uint16":
			return int.from_bytes(file.read(2), "little")
		case "uint32":
			return int.from_bytes(file.read(4), "little")
		case "uint64":
			return int.from_bytes(file.read(4), "little")
		case "int8":
			return int.from_bytes(file.read(1), "little", signed=True)
		case "int16":
			return int.from_bytes(file.read(2), "little", signed=True)
		case "int32":
			return int.from_bytes(file.read(4), "little", signed=True)
		case "int64":
			return int.from_bytes(file.read(8), "little", signed=True)
		case "float":
			return struct.unpack("<f", file.read(4))
		case "double":
			return struct.unpack("<d", file.read(8))

def GetValueType(file) -> str:
	value_type = int.from_bytes(file.read(1), "little")
	match(value_type):
		case 1:
			return "uint8"
		case 2:
			return "uint16"
		case 4:
			return "uint32"
		case 8:
			return "uint64"
		case 0x11:
			return "int8"
		case 0x12:
			return "int16"
		case 0x14:
			return "int32"
		case 0x18:
			return "int64"
		case 0x24:
			return "float"
		case 0x28:
			return "double"
		case _:
			print("Wrong value type 0x%x at offset 0x%x" % (value_type, file.tell()-1))

def processData(file, size):
	ret_list = []
	while(file.tell() < size):
		entry = {}
		OPCODE = int.from_bytes(file.read(1), "little", signed=True)
		match(OPCODE):
			case 1:
				entry["type"] = "write"
				entry["address"] = []
				address_count = int.from_bytes(file.read(1), "little", signed=True)
				for i in range(address_count):
					entry["address"].append(int.from_bytes(file.read(4), "little", signed=True))
				entry["value_type"] = GetValueType(file)
				entry["value"] = GetValue(file, entry["value_type"])
				ret_list.append(entry)
			case 2:
				entry["type"] = "compare"
				entry["compare_address"] = []
				address_count = int.from_bytes(file.read(1), "little", signed=True)
				for i in range(address_count):
					entry["compare_address"].append(int.from_bytes(file.read(4), "little", signed=True))
				entry["compare_type"] = GetCompareType(file)
				entry["compare_value_type"] = GetValueType(file)
				entry["compare_value"] = GetValue(file, entry["compare_value_type"])
				entry["address"] = []
				address_count = int.from_bytes(file.read(1), "little", signed=True)
				for i in range(address_count):
					entry["address"].append(int.from_bytes(file.read(4), "little", signed=True))
				entry["value_type"] = GetValueType(file)
				entry["value"] = GetValue(file, entry["value_type"])
				ret_list.append(entry)
			case -1:
				return ret_list
			case _:
				print("WRONG OPCODE %d at offset 0x%x" % (OPCODE, file.tell()-1))
				sys.exit()

filesize = os.stat(sys.argv[1]).st_size
file = open(sys.argv[1], "rb")

if (file.read(4) != b"LOCK"):
	print("WRONG MAGIC!")
	sys.exit()

OFFSETS = []

for i in range(10):
	OFFSETS.append(int.from_bytes(file.read(4), "little"))

if OFFSETS[0] != 44:
	print("Offset check failed!")
	sys.exit()

DICT = {}
OBJECTS = ["15FPS", "20FPS", "25FPS", "30FPS", "35FPS", "40FPS", "45FPS", "50FPS", "55FPS", "60FPS"]

for i in range(len(OFFSETS)):
	file.seek(OFFSETS[i])
	DICT[OBJECTS[i]] = processData(file, filesize)

file.close()

new_file = open(f"_{Path(sys.argv[1]).stem}.yaml", "w", encoding="ascii")
yaml = ruamel.yaml.YAML(typ="safe")
yaml.dump(DICT, new_file)
new_file.close()