#!/usr/bin/env python3

import glob
import os.path
import binascii

globPattern = os.path.dirname(__file__) + "/**/resources/*";

for path in glob.glob(globPattern, recursive=True):
	if path.endswith(".hxx"):
		continue
	var = os.path.basename(path).replace(".", "_")
	print(path)
	print(var)
	with open(path, 'rb') as file:
		with open(path + ".hxx", 'w') as hxx:
			hxx.write("const unsigned char " + var + "[] = {")
			byteCounter = 0;
			byte = file.read(1)
			while byte:
				if byteCounter%1024 == 0:
					hxx.write("\n// %i"%byteCounter)
				if byteCounter%16 == 0:
					hxx.write("\n");
				byteCounter += 1;
				hxx.write("0x" + binascii.b2a_hex(byte).decode('utf8') + ",")
				byte = file.read(1)
			hxx.write("\n};\n")
