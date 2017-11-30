#!/usr/bin/env python

import time
import sys
import os

import gtk
import pygtk
import gobject
import threading

v1disp = [
"39 39 27 1",

"a c #00ff00",
"b c #00ff00",
"c c #00ff00",
"d c #00ff00",
"e c #00ff00",
"f c #00ff00",
"g c #00ff00",
"p c #00ff00",

"8 c #00ff00",
"7 c #00ff00",
"6 c #00ff00",
"5 c #00ff00",
"4 c #00ff00",
"3 c #00ff00",
"2 c #00ff00",
"1 c #00ff00",

"l c #00ff00",
"q c #00ff00",
"k c #00ff00",
"x c #00ff00",
"- c #ff00ff",
"u c #00ff00",
"s c #00ff00",
"r c #00ff00",

"m c #00ff00",

". c #000000",
"@ c #ffffff",

".......................................",
".......................................",
".......................................",
".......................................",
".......................................",
".......................................",
".......................................",
"...aaaaaaaaaa..@...lll.................",
"..f.aaaaaaaa.b.@...lll........uu.......",
"..ff........bb.@@@.lll.......uuuu......",
"..ff........bb..............uuuuuu.....",
"..ff........bb.............uuuuuuuu....",
"..ff........bb..@..qqq....uuuuuuuuuu...",
"..ff........bb.@@@.qqq...uuuuuuuuuuuu..",
"..ff........bb.@.@.qqq.....uuuuuuuu....",
"..ff........bb.............uuuuuuuu....",
"..ff........bb.............uuuuuuuu....",
"..fggggggggggb.@.@.kkk...s..........s..",
"..eggggggggggc.@@..kkk..ssssssssssssss.",
"..ee........cc.@.@.kkk.ssssssssssssssss",
"..ee........cc..........ssssssssssssss.",
"..ee........cc...........s..........s..",
"..ee........cc.@.@.xxx.....rrrrrrrr....",
"..ee........cc..@..xxx.....rrrrrrrr....",
"..ee........cc.@.@.xxx...rrrrrrrrrrrr..",
"..ee........cc.............rrrrrrrr....",
"..ee........cc..ppp..........rrrr......",
"..e.dddddddd.c..ppp....................",
"...dddddddddd...ppp....................",
".......................................",
"...888.777.666.555.444.333.222.111.....",
"...888.777.666.555.444.333.222.111.....",
"...888.777.666.555.444.333.222.111.....",
".......................................",
"...mmmmm..m...m.mmmmm..mmmmm...........",
"...m.m.m..m...m...m....m...............",
"...m.m.m..m...m...m....mmmmm...........",
"...m.m.m..m...m...m....m...............",
"...m.m.m...mmm....m....mmmmm...........",
               ]

button = None
disp = None
sevs2ascii = [
 ' ', '~', '.', '.', '.', '.', '1', '7',
 '_', '.', '.', '.', 'j', '.', '.', ']',
 '.', '.', '.', '.', '.', '.', '.', '.',
 'l', '.', '.', '.', 'u', 'v', 'J', '.',

 '.', '.', '.', '^', '.', '.', '.', '.',
 '.', '.', '.', '.', '.', '.', '.', '.',
 '|', '.', '.', '.', '.', '.', '.', '.',
 'L', 'C', '.', '.', '.', 'G', 'U', '0',
 
 '-', '.', '.', '.', '.', '.', '.', '.',
 '=', '#', '.', '.', '.', '.', '.', '3',
 'r', '.', '/', '.', '.', '.', '.', '.',
 'c', '.', '.', '2', 'o', '.', 'd', '.',

 '.', '.', '.', '.', '\\', '.', '4', '.',
 '.', '.', '.', '.', '.', '5', 'y', '9',
 '.', 'F', '.', 'P', 'h', '.', 'H', 'A',
 't', 'E', '.', '.', 'b', '6', '.', '8']


data = None

def dosegment(seg,what):
    seg += 1;
    if what == 2:
        v1disp[seg] = v1disp[seg][0:5] + "ffff00"
    elif what == 1:
        v1disp[seg] = v1disp[seg][0:5] + "ff0000"
    else:
        v1disp[seg] = v1disp[seg][0:5] + "300000"

class displayer(threading.Thread):
    def __init__(self, label):
        global button, v1disp
        global data

        super(displayer, self).__init__()
        self.label = label;
        self.quit = False

        data = [0,0,0,0,0,0,0,0]

        pixbuf = gtk.gdk.pixbuf_new_from_xpm_data(v1disp)
        image = gtk.Image()
        w = button.get_allocation().width
        h = button.get_allocation().height
        if w > h:
            w = h
        image.set_from_pixbuf(pixbuf.scale_simple(w,w,gtk.gdk.INTERP_NEAREST))
        button.set_image(image)

    def run(self):
        global data
        time.sleep(1) # gtk wait until it is up
        while not self.quit:
            sof = serin.read(1)
            if ord(sof) != 0xaa:
                continue
            pkt = serin.read(4)
            if ord(pkt[0]) & 0xd0 == 0xd0 and ord(pkt[1]) & 0xe0 == 0xe0 and ord(pkt[3]) < 20:
                data = serin.read(ord(pkt[3])-1)
                tail = serin.read(2)
#tail[0] is checksum
                if ord(tail[1]) != 0xab :
                    continue
            else:
                continue

            if ord(pkt[0]) == 0xd8  and ord(pkt[1]) == 0xea and ord(pkt[2]) == 0x31 and ord(pkt[3]) == 9:
                gobject.idle_add(self.infDisp, data)

            if ord(pkt[0]) == 0xd4  and ord(pkt[1]) == 0xea and ord(pkt[2]) == 0x43 and ord(pkt[3]) == 8:
                d = [0,0,0,0,0,0,0,0]
                for i in range(7):
                    d[i] = ord(data[i])
                if d[0] == 0 :
                    continue                
                print d[0] & 0x0f, "/", d[0] >> 4, ":", d[1]*256+d[2], " F:", d[3], " R:",d[4], "%02x"%d[5], d[6]>>7 

            if ord(pkt[2]) == 0x02 and ord(pkt[3]) == 8:
# ord(pkt[1])&15    a=V1, 0=dc, 1=ra, 2=sv
                print ord(pkt[1])&15, "Version:", data
            if ord(pkt[2]) == 0x04 and ord(pkt[3]) == 11:
                print ord(pkt[1])&15, "SerialNo:", data
            if ord(pkt[2]) == 0x63 and ord(pkt[3]) == 3:
                print "Batt Voltage:", ord(data[0]), '.', ord(data[1])

            if ord(pkt[2]) == 0x12 and ord(pkt[3]) == 7:
                d = [0,0,0,0,0,0,0,0]
                for i in range(6):
                    d[i] = ord(data[i])
                userset = "12345678AbCdEFGHJuUtL   ";
                print "UserBytes: default"
                x = [0,0,0,0,0,0,0,0]
                y = [0,0,0,0,0,0,0,0]
                z = [0,0,0,0,0,0,0,0]

                for i in range(8):
                     x[i] = d[0]>>i & 1
                     y[i] = d[1]>>i & 1
                     z[i] = d[2]>>i & 1
                print d, x, y , z
                x = x+y+z
                s = ""
                t = ""
                for i in range(24):
                    if x[i] :
                        s = s + userset[i]
                        t = t + ' '
                    else:
                        t = t + userset[i]
                        s = s + ' '
                print s
                print t
                print "UserBytes: option"


    def infDisp(self,data):
        global sevs2ascii, button, disp

        if disp:
            disp.display_state_on()

        window.present()
        d = [0,0,0,0,0,0,0,0]
        for i in range(8):
            d[i] = ord(data[i])

        ddif = d[0] ^ d[1]
        dimg = d[0]
        for i in range (8):
            if (dimg >> i ) & 1 :
                if (ddif >> i ) & 1 :
                    dosegment(i,2)
                else:
                    dosegment(i,1)
            else:
                    dosegment(i,0)

        dimg = d[2]
        for i in range (8):
            if (dimg >> i ) & 1 :
                dosegment(i+8,1)
            else:
                dosegment(i+8 ,0)


        ddif = d[3] ^ d[4]
        dimg = d[3]
        for i in range (8):
            if (dimg >> i ) & 1 :
                if (ddif >> i ) & 1 :
                    dosegment(i+16,2)
                else:
                    dosegment(i+16,1)
            else:
                    dosegment(i+16,0)


    #    if stren == 0 && timeout
    #        window.iconify()


        x = d[3] >> 3 & 1
        r = d[3] >> 7 & 1
        s = d[3] >> 6 & 1
        u = d[3] >> 5 & 1

        l = d[3] >> 0 & 1
        ka = d[3] >> 1 & 1
        k = d[3] >> 2 & 1

        p = d[0] >> 7 & 1
        m = d[5] >> 0 & 1

#d5  custSw euro dispOn SysStat TSHold Mute
        if m :
            dosegment(24,1)
        else:
            dosegment(24,0)
        sevseg = d[0] & 127

        stren = d[2]

        if( stren == 254 ) :
            stren = 9
        else :
            i = 0
            while stren :
                stren >>= 1
                i += 1
            stren = i


#        print  " ^"[u]+" -"[s]+" v"[r] , " X"[x]+" K"[k]+" A"[ka]+" L"[l] , sevs2ascii[sevseg]+" ."[p], stren, m,  sevseg

        pixbuf = gtk.gdk.pixbuf_new_from_xpm_data(v1disp)
        image = gtk.Image()
        w = button.get_allocation().width
        h = button.get_allocation().height
        if w > h:
            w = h
        image.set_from_pixbuf(pixbuf.scale_simple(w,w,gtk.gdk.INTERP_NEAREST))
        button.set_image(image)


for i in range (25):
    dosegment(i,0)

try:
    serin = open("/dev/ttyUSB0", "r+")
except (ValueError, IOError):
    exit()

window = gtk.Window(gtk.WINDOW_TOPLEVEL)
window.connect("destroy", lambda wid: gtk.main_quit())
window.connect("delete_event", lambda a1,a2:gtk.main_quit())
window.set_title("Valentine V1")

button = gtk.Button()
button.set_image(gtk.image_new_from_stock(gtk.STOCK_YES, gtk.ICON_SIZE_MENU))
button.set_name("menu")
button.set_size_request(320,320)
button.show()

window.add(button)
window.show()

gobject.threads_init()
gtk.gdk.threads_init()
t = displayer(gtk.Label())
t.start()
gtk.main()
t.quit = True
