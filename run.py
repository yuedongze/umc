import parser
import sony
from time import sleep

if __name__ == '__main__':
	sony.connect()
	sleep(1)
	sony.auth()
	sleep(1)
	a = sony.info()
	b = []

	for i in a:
		if i == '':
			b.append('00')
		else:
			b.append(('0'+hex(ord(i))[2:])[-2:])

	lib = parser.update_dinfo(b)

	print lib