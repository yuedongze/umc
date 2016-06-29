def read_and_add(d, ind, n):
	res = ''
	for i in range(ind, ind+n):
		res = str(d[i])+res
	return res

def hexstr_to_int(input):
	return int('0x'+input, 0)

def parse_datatype(input):
	if input == 1 or input == 2:
		return 1
	elif input == 3 or input == 4:
		return 2
	elif input == 5 or input == 6:
		return 4
	elif input == 7 or input == 8:
		return 8
	elif input == 9 or input == 10:
		return 16
	elif input == 0xFFFF:
		return 0
	elif input == 0x4002:
		return 0
	elif input == 0x4004:
		return 0
	else:
		return 0

def update_dinfo(info):
	total = hexstr_to_int(read_and_add(info, 0, 8))
	print ("The number of total properties is: " + str(total) + ".")
	j = 8
	c = 0
	status = dict()

	while True:

		# locate to the next xxd6
		if 'd6' in info[j:]:
			j = info.index('d6', j)
		else:
			break
		j = j - 1
		ind = j

		# now reading PropertyCode
		pcode = read_and_add(info, j, 2)
		#print ("The Property Code is: " + pcode + ".\n")
		j += 2

		# now reading info type
		dtype = read_and_add(info, j, 2)
		dtype_len = parse_datatype(hexstr_to_int(dtype))
		#print ("The Data Type is: " + dtype + ".\n")
		#print ("It takes " + str(dtype_len) + " Bytes.\n")
		j += 2

		# now reading get set
		gs = read_and_add(info, j, 1)
		#print ("The Get/Set is: " + gs + ".\n")
		j += 1

		# now reading enable
		enab = read_and_add(info, j, 1)
		#print ("The Enable is: " + enab + ".\n")
		j += 1

		# reading fac and cur value
		fval = read_and_add(info, j, dtype_len)
		j += dtype_len
		cval = read_and_add(info, j, dtype_len)
		j += dtype_len

		#print ("The Factory value is " + fval + " and the Current value is " + cval + ".\n")

		# read form flag
		ff = read_and_add(info, j, 1)
		j += 1

		#print ("Form flag is " + ff + ".\n")
		if hexstr_to_int(ff) == 0:
			#print ("No FORM Field!\n")
			pass
		elif hexstr_to_int(ff) == 1:
			pass
			#print ("Range FORM.\n")
			#minVal = hexstr_to_int(read_and_add(info, j, dtype_len))
			#j += dtype_len
			#maxVal = hexstr_to_int(read_and_add(info, j, dtype_len))
			#j += dtype_len
			#stepVal = hexstr_to_int(read_and_add(info, j, dtype_len))
			#j += dtype_len
			#print minVal,maxVal,stepVal
		else:
			pass
			#print ("Enum FORM.\n")
			#dsize = hexstr_to_int(read_and_add(info, j, 2))
			#j += 2
			##print ("Size: " + str(dsize) + ".\n")
			#print ("Enumerating infoset...")
			#for k in range(dsize):
		#		tmp = read_and_add(info, j, dtype_len)
		#		j += dtype_len
		#		print (tmp)

		status['0x'+pcode] = {'dtype':dtype, 'dtype_len':dtype_len, 'gs':gs, 'enable':enab, 'fval':fval, 'cval':cval, 'ff':ff, 'ind':ind}
	
	return status

def parse_devinfo(info):
	b = []
	for i in info:
		if i == '':
			b.append('00')
		else:
			b.append(('0'+hex(ord(i))[2:])[-2:])

	return update_dinfo(b)

if __name__ == '__main__':
	with open ('log.txt', 'r') as myfile:
		data = myfile.readlines()

	for i in range(len(data)):
		if (data[i].find('\n') != -1):
			data[i] = data[i][:-1]
	
	print update_dinfo(data).keys()