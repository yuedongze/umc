from flask import Flask,request,url_for,redirect
import os
import sony
import parser
from time import *
app = Flask(__name__)

t=0

def umc_start():
	sony.connect()
	sleep(1)
	sony.auth()
	sleep(1)

def capture_still():
	lib = parser.parse_devinfo(sony.info())
	sony.setprop(0xD6E2, '02')
	sleep(0.5)
	sony.control(0xD61D, '0200')
	sleep(1)
	sony.control(0xD617, '0200')
	sleep(0.2)
	sony.control(0xD617, '0100')
	sleep(0.2)
	sony.control(0xD61D, '0100')
	sleep(0.2)

@app.route('/', methods=['GET', 'POST'])
def hello():
	res = 'Not Recording'
	inf = 'Not Available'
	b = []
	lib = parser.parse_devinfo(sony.info())
	if (lib['0xd6cd'])['cval'] == '01':
		res = 'Recording'
		
	if request.method == 'GET':
		if (time.time()-t) > 0.1:
			sony.getliveobj()
			t = time.time()
		
	if request.method == 'POST':
		if 'shoot' in request.form.keys():
			sony.mstart()
		
		if 'stop' in request.form.keys():
			sony.mstop()

		if 'sub' in request.form.keys():
			if request.form['val'] in lib.keys():
				inf = str(lib[request.form['val']])

		if 'pset' in request.form.keys():
			sony.setprop(int(request.form['pid'],0),request.form['pdata'])

		if 'cset' in request.form.keys():
			sony.control(int(request.form['cid'],0),request.form['cdata'])

		if 'cap' in request.form.keys():
			capture_still()

	return '''
    <!doctype html>
    <title>Take a video</title>
    <h1>Click to take video :)</h1>
    <form action="" method=post enctype=multipart/form-data>
      <p><input name=shoot type=submit value=SHOOT></p>
      <p><input name=stop type=submit value=STOP></p>
      <p><input name=cap type=submit value="Take a Pic"></p>
	  <p>............LiveView............</p>
	  
	  <img src="object" id="liveview" />
	  
	  <input name=reload type=submit/>
	  
      <p>............Controller............</p>
      <p>Request Prop: <input name=val type=text> <input name=sub type=submit value=REQUEST> </p>
      <p>Set Prop: </p>
      <p>Prop ID: <input name=pid type=text/> 
      <input name=pdata type=text/>
      <input name=pset type=submit value=SET> </p>

      <p>Control Device: </p>
      <p>Control ID: <input name=cid type=text/> 
      <input name=cdata type=text/>
      <input name=cset type=submit value=SEND> </p>
    </form>
    <h2>Current Status: ''' + res +  '''</h2>
    <h2>Requested Information: ''' + inf + '''</h2>
    '''

if __name__ == "__main__":
	umc_start()
	app.run()
