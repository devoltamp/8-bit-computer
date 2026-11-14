# Control line bit definitions matching CPU28c16.cpp and Ben Eater's microcode
HLT = 0x8000
MI  = 0x4000
RI  = 0x2000
RO  = 0x1000
IO  = 0x0800
II  = 0x0400
AI  = 0x0200
AO  = 0x0100
EO  = 0x0080
SU  = 0x0040
BI  = 0x0020
OI  = 0x0010
CE_ = 0x0008   # Counter Enable
CO  = 0x0004
J   = 0x0002

# 128-entry microcode table (16 instructions × 8 steps)
microcode = [
    MI|CO, RO|II|CE_, 0,     0,     0,     0, 0, 0,   # 0000 NOP
    MI|CO, RO|II|CE_, IO|MI, RO|AI, 0,     0, 0, 0,   # 0001 LDA
    MI|CO, RO|II|CE_, IO|MI, RO|BI, EO|AI, 0, 0, 0,   # 0010 ADD
    MI|CO, RO|II|CE_, 0,     0,     0,     0, 0, 0,   # 0011
    MI|CO, RO|II|CE_, 0,     0,     0,     0, 0, 0,   # 0100
    MI|CO, RO|II|CE_, 0,     0,     0,     0, 0, 0,   # 0101
    MI|CO, RO|II|CE_, 0,     0,     0,     0, 0, 0,   # 0110
    MI|CO, RO|II|CE_, 0,     0,     0,     0, 0, 0,   # 0111
    MI|CO, RO|II|CE_, 0,     0,     0,     0, 0, 0,   # 1000
    MI|CO, RO|II|CE_, 0,     0,     0,     0, 0, 0,   # 1001
    MI|CO, RO|II|CE_, 0,     0,     0,     0, 0, 0,   # 1010
    MI|CO, RO|II|CE_, 0,     0,     0,     0, 0, 0,   # 1011
    MI|CO, RO|II|CE_, 0,     0,     0,     0, 0, 0,   # 1100
    MI|CO, RO|II|CE_, 0,     0,     0,     0, 0, 0,   # 1101
    MI|CO, RO|II|CE_, AO|OI, 0,     0,     0, 0, 0,   # 1110 OUT
    MI|CO, RO|II|CE_, HLT,   0,     0,     0, 0, 0,   # 1111 HLT
]

# Initialize 2048 bytes (2KB) erased to 0xFF
memory = bytearray([0xFF] * 2048)

# Pack 16-bit words into upper and lower halves
for i in range(128):
    val = microcode[i]
    memory[i]       = (val >> 8) & 0xFF  # Upper control byte (A7 = 0)
    memory[i + 128] = val & 0xFF         # Lower control byte (A7 = 1)

# Save as both filenames to satisfy your log and DLL references
with open("28c16_microcode_16bit.bin", "wb") as f:
    f.write(memory)

with open("28c16.bin", "wb") as f:
    f.write(memory)

print("Successfully generated 28c16_microcode_16bit.bin and 28c16.bin (2048 bytes).")
