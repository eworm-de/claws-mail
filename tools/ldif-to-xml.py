#!/usr/bin/env python

#  * Copyright 2001 Rod Senra <Rodrigo.Senra@ic.unicamp.br>
#  *
#  * This file is free software; you can redistribute it and/or modify it
#  * under the terms of the GNU General Public License as published by
#  * the Free Software Foundation; either version 2 of the License, or
#  * (at your option) any later version.
#  *
#  * This program is distributed in the hope that it will be useful, but
#  * WITHOUT ANY WARRANTY; without even the implied warranty of
#  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  * General Public License for more details.
#  *
#  * You should have received a copy of the GNU General Public License
#  * along with this program; if not, write to the Free Software
#  * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#  *

import re
import sys

header = """<?xml version="1.0" encoding="ISO-8859-1"?>
<addressbook>

<common_address>
"""

footer = """
</common_address>

<personal_address>
</personal_address>

</addressbook>
"""

def printGroupRec(fd,name,members):
    """ Print XML group record from r-tuple"""
    fd.write("    <group name=\"%s\">\n"%(name))
    for each in members:
        printRec(fd,each,"        ")
    fd.write("    </group>\n")

def printRec(fd,r,ident):
    """ Print XML group record from r-tuple"""
    fd.write("%s<item>\n"%(ident) )
    fd.write("%s    <name>%s</name>\n"%(ident,r[0]))
    fd.write("%s    <address>%s</address>\n"%(ident,r[1]))
    fd.write("%s    <remarks>%s</remarks>\n"%(ident,r[2]))
    fd.write("%s</item>\n"%(ident))
    
outfd = open('addressbook.xml','w')


outfd.write(header)
try:
    rec = {}
    for line in  open(sys.argv[1]).readlines():
        line = line[:-1].strip() # clean string
        if line=='':
            try:
                if rec.has_key('description'):
                    str = rec['description']
                elif rec.has_key('xmozillanickname'):
                    str = rec['xmozillanickname']
                elif rec.has_key('sn'):
                    str = rec['sn']
                else:
                    str = ''
                try:
                    if rec.has_key('member'):
                        printGroupRec(outfd,rec['cn'].strip(),rec['member'])
                    elif rec.has_key('mail'):
                        printRec(outfd,(rec['cn'].strip(),rec['mail'].strip(),str.strip()),"    ")

                except KeyError:
                    pass
            finally:
                del rec
                rec = {}
            continue

        try: # parse line
            key,value = line.split(':')
        except:
            continue
        if key=='member':
            name,addr = value.split(',')
            name = name.split('=')[1].strip()
            addr = addr.split('=')[1].strip()
            value = (name,addr,'')
            if rec.has_key('member'):
                rec['member'].append(value)
            else :
                rec['member'] = [value]
        else:
            rec[key]=value
    
finally:
    outfd.write(footer)
    outfd.close()
