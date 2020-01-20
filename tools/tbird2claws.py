#!/usr/bin/env python3

# Script name : tbird2claws.py
# Script purpose : Integrate a Thunderbird folder tree to Claws Mail
# Author : Aleksandar Urosevic aka Urke MMI <urke@gmx.net>
# Licence : GPL
# Author: Rodrigo Dias Arruda Senra

#The script receives two parameters from command-line:
#<Thunderbird folder path> <Claws Mail folder path>

#Best way to use it is to go to inside yout Thunderbird
#root mailfolder directory and invoke it as:

#<path>\python3 <path>\tbird2claws.py . <path to claws-mail>\Mail

import os
import sys
import importlib

__author__ = 'Rodrigo Senra <rsenra@acm.org>'
__date__ =  '2005-03-23'
__version__ =  '0.3'

__doc__ = r"""
This module integrates your Mozilla Thunderbird 1.0 tree to
your Claws Mail MH mailbox tree.

The script receives two parameters from command-line:
 <Thunderbird folder path> <Claws Mail folder path>

Best way to use it is to go to inside your Thunderbird
root mailfolder directory and invoke it as:

  <path>\python3 <path>\tbird2claws.py . <path to claws mail>\Mail

This idiom will avoid the creation of the folder Thunderbird inside
your Claws Mail folder tree.

If the names of your directories match in both trees, files should
be placed in the correct folder.

This is an alpha release, so it may be a little rough around the edges.
Nevertheless, I used it with great success to convert a very large and
deep folder tree.

Please, do backup your claws-mail (destination) folder tree before trying
this out. Live safe and die old!

This code is released in the public domain.
"""

def harvest_offsets(filepath):
    """Given the filepath, this runs through the file finding
    the number of the line where a message begins.
    
    The function returns a list of integers corresponding to
    the beginning of messages.
    """	
    offsets = []
    i = 0
    state = 'begin'
    for i,line in enumerate(open(filepath)):
        if line.startswith('From - ') and state!='found_head':
           offsets.append(i)
           continue
#        elif line.startswith('Return-Path') and state=='found_head':
#	    state = 'found_offset'
#	    offsets.append(i)
#	    continue
    offsets.append(i)
    return offsets

def make_messages(outputdir, filepath, offsets, start):
    """Given a filepath holding several messages in Thunderbird format,
    extract the messages and create individual files for them, inside
    outputdir with appropriate the appropriate naming scheme.
    """ 
    if not os.path.exists(outputdir):
        os.makedirs(outputdir)
    if not os.path.exists(filepath):
        raise Exception('Cannot find message file  %s'%(filepath))
    lines = open(filepath).readlines()
    aux = offsets[:]
    msgoffs = zip(offsets[:-1], aux[1:])
    for i,j in msgoffs:
       fd  = open(os.path.join(outputdir,"%d"%start),"w")
       fd.write(''.join(lines[i:j-1])) #-1 to remove first from line
       fd.close()
       start +=1

def process_file(filepath, outputdir):
    """Integrates a Thunderbird message file into a claws-mail message directory.
    """  
    offs = harvest_offsets(filepath)
    make_messages(outputdir, filepath, offs, 1)

def clean_path(path):
    """Rename all directories and subdirectories <X>.sbd to <X>
    """
    l = []
    f = os.path.basename(path)
    while f and f != "":
        if f.endswith('.sbd'): 
            f = f[:-4]
        l.append(f)
        path = os.path.dirname(path)
        f = os.path.basename(path)
    l.reverse()
    r = os.path.join(*l)
    return r



def convert_tree(in_treepath, out_treepath):
    """Traverse your thunderbird tree, converting each message file found into
    a claws-mail message directory.
    """
    for path,subs,files in os.walk(in_treepath):
        outpath = clean_path(path)
        if files:
            for f in [x for x in files if not x.endswith('.msf')]:
                process_file(os.path.join(path,f),
                             os.path.join(out_treepath,outpath,f))

if __name__=='__main__':
    if len(sys.argv)<3:
        print (__doc__)
    else:
        if sys.version[0] == '2':
            importlib.reload(sys)
            sys.setdefaultencoding('utf8')
        convert_tree(sys.argv[1], sys.argv[2])
