import serial
# pip install pyserial
# https://pyserial.readthedocs.io/en/latest/pyserial_api.html


com4 = serial.Serial('COM4:', 115200)

def w_byte(ser_port, u8): ser_port.write([u8])
def w_bytes(ser_port, l_u8): ser_port.write(l_u8)
def r_byte(ser_port): return list(ser_port.read())[0]
def r_bytes(ser_port, n): return list(ser_port.read(n))

N = 32768

# 1 バイトずつ送る
for i in range(N):
	w_byte(com4, i%255)
	r = r_byte(com4)
	if r != (i%255+1): print(f'i={i} tx:[3] rx: {r}', end='  ', flush=True)
	print(f'{i:7}', end='\r')

# ブロックで送る
page=1024
for i in range(0, N, page):
	x = [(i+j)%255    for j in range(page)]
	y = [(i+j)%255 +1 for j in range(page)]
	w_bytes(com4, x)
	r = r_bytes(com4, page)
	if r != y: print(f'i={i} tx:[3] rx: {r}', end='  ', flush=True); exit()
	print(f'{i:7}', end='\r')

print(f'\n{N:7} bytes sent')
