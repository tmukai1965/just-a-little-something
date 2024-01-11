import serial
comX = 'COM4:'; bps = 115200

ser_port = serial.Serial(comX, bps)
def w_byte (u8)  : ser_port.write([u8])
def w_bytes(l_u8): ser_port.write(l_u8)
def r_byte ()    : return list(ser_port.read())[0]
def r_bytes(n)   : return list(ser_port.read(n))

def peek(adr):
	x = []
	for i in range(4):
		x.append(adr & 0xFF)
		adr >>= 8
	x.reverse()
	w_bytes(x)
	return r_byte()

mem = [0xFF]*65536
a0 = 0x0000
a1 = 0x3FFF
for address in range(a0, a1+1): mem[address] = peek(address)
for i in range(a0, a1, 16):
	print(f'{i:04X} : ', end='')
	for j in range(16): print(f'{mem[i+j]:02X}', end=' ')
	print()
