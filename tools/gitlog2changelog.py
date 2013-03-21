#!/usr/bin/python
# Copyright 2008 Marcus D. Hanwell <marcus@cryos.org>
# Distributed under the terms of the GNU General Public License v2 or later

import string, re, os, sys

curRev = ""
prevVer = ""

if len(sys.argv) == 3:
    curRev = sys.argv[2]
    prevVer = sys.argv[1]
else:
    curRevCmd = os.popen("git describe")
    curRev = curRevCmd.read()
    curRev = curRev[0:len(curRev)-1]
    curRevCmd.close()
    if len(sys.argv) == 2:
        prevVer = sys.argv[1]
    else:
        prevVer = re.split('-', curRev, 1)[0]

# Execute git log with the desired command line options.
fin = os.popen('git log ' + prevVer + '..' + curRev +' --summary --stat --no-merges --date=short', 'r')
# Create a ChangeLog file in the current directory.
fout = sys.stdout

# Set up the loop variables in order to locate the blocks we want
authorFound = False
dateFound = False
messageFound = False
filesFound = False
message = ""
commit = ""
messageNL = False
files = ""
prevAuthorLine = ""

# The main part of the loop
for line in fin:
    # The commit line marks the start of a new commit object.
    if re.match('^commit', line) >= 0:
        # Start all over again...
        authorFound = False
        dateFound = False
        messageFound = False
        messageNL = False
        message = ""
        filesFound = False
        files = ""
	commitCmd = os.popen("git describe "+re.split(' ', line, 1)[1])
	commit = commitCmd.read()
	commitCmd.close()
        commit = commit[0:len(commit)-1]
        continue
    # Match the author line and extract the part we want
    elif re.match('^Author:', line) >=0:
        authorList = re.split(': ', line, 1)
        author = re.split('<', authorList[1], 1)[0]
        author = "[" + author[0:len(author)-1]+"]"
        authorFound = True
    # Match the date line
    elif re.match('^Date:', line) >= 0:
        dateList = re.split(':   ', line, 1)
        date = dateList[1]
        date = date[0:len(date)-1]
        dateFound = True
    # The svn-id lines are ignored
    elif re.match('    git-svn-id:', line) >= 0:
        continue
    # The sign off line is ignored too
    elif re.search('Signed-off-by', line) >= 0:
        continue
    # Extract the actual commit message for this commit
    elif authorFound & dateFound & messageFound == False:
        # Find the commit message if we can
        if len(line) == 1:
            if messageNL:
                messageFound = True
            else:
                messageNL = True
        elif len(line) == 4:
            messageFound = True
        else:
            if len(message) == 0:
                message = message + line.strip()
            else:
                message = message + " " + line.strip()
    # If this line is hit all of the files have been stored for this commit
    elif re.search('files changed', line) >= 0:
        filesFound = True
        continue
    # Collect the files for this commit. FIXME: Still need to add +/- to files
    elif authorFound & dateFound & messageFound:
        fileList = re.split(' \| ', line, 2)
        if len(fileList) > 1:
            if len(files) > 0:
                files = files + "\n\t* " + fileList[0].strip()
            else:
                files = "\t* " + fileList[0].strip()
    # All of the parts of the commit have been found - write out the entry
    if authorFound & dateFound & messageFound & filesFound:
        # First the author line, only outputted if it is the first for that
        # author on this day
        authorLine = date + "  " + author
        if len(prevAuthorLine) != 0:
	    fout.write("\n");
	fout.write(authorLine + " " + commit + "\n\n")

        # Assemble the actual commit message line(s) and limit the line length
        # to 80 characters.
        commitLine = message
        i = 0
        commit = ""
        while i < len(commitLine):
            if len(commitLine) < i + 70:
                commit = commit + "\n\t\t" + commitLine[i:len(commitLine)]
                break
            index = commitLine.rfind(' ', i, i+70)
            if index > i:
                commit = commit + "\n\t\t" + commitLine[i:index]
                i = index+1
            else:
                commit = commit + "\n\t\t" + commitLine[i:70]
                i = i+71

        # Write out the commit line
	fout.write(files + "\t\t" + commit + "\n")

        #Now reset all the variables ready for a new commit block.
        authorFound = False
        dateFound = False
        messageFound = False
        messageNL = False
        message = ""
        filesFound = False
        files = ""
        prevAuthorLine = authorLine

# Close the input and output lines now that we are finished.
fin.close()
fout.close()
