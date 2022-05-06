/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Colin Leroy <colin@colino.net>
 * and the Claws Mail Team
 *
 * This file Copyright (C) 2002-2007 Randall Hand <yerase@yerot.com> 
 * Thanks to him for allowing redistribution of this code as GPLv3.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
 
#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#ifdef YTNEF_H_SUBDIR
#include <libytnef/tnef-types.h>
#include <libytnef/ytnef.h>
#include <libytnef/mapi.h>
#include <libytnef/mapidefs.h>
#else
#include <tnef-types.h>
#include <ytnef.h>
#include <mapi.h>
#include <mapidefs.h>
#endif

#include "common/claws.h"
#include "common/version.h"
#include "main.h"
#include "plugin.h"
#include "procmime.h"
#include "utils.h"

#include "tnef_dump.h"

#define PRODID "PRODID:-//The Gauntlet//" PACKAGE " TNEF Parser " VERSION "//EN\n"

static unsigned char GetRruleCount(unsigned char a, unsigned char b) {
    return ((a << 8) | b);
}

static unsigned char GetRruleMonthNum(unsigned char a, unsigned char b) {
    switch (a) {
        case 0x00:
            switch (b) {
                case 0x00:
                    // Jan
                    return(1);
                case 0xA3:
                    // May
                    return(5);
                case 0xAE:
                    // Nov
                    return(11);
            }
            break;
        case 0x60:
            switch (b) {
                case 0xAE:
                    // Feb
                    return(2);
                case 0x51:
                    // Jun
                    return(6);
            }
            break;
        case 0xE0:
            switch (b) {
                case 0x4B:
                    // Mar
                    return(3);
                case 0x56:
                    // Sep
                    return(9);
            }
            break;
        case 0x40:
            switch (b) {
                case 0xFA:
                    // Apr
                    return(4);
            }
            break;
        case 0x20:
            if (b == 0xFA) {
                // Jul
                return(7);
            }
            break;
        case 0x80:
            if (b == 0xA8) {
                // Aug
                return(8);
            }
            break;
        case 0xA0:
            if (b == 0xFF) {
                // Oct
                return(10);
            }
            break;
        case 0xC0:
            if (b == 0x56) {
                return(12);
            }
    }

    // Error
    return(0);
}

static char * GetRruleDayname(unsigned char a) {
    static char daystring[25];

    *daystring = 0;

    if (a & 0x01) {
        strcat(daystring, "SU,");
    }
    if (a & 0x02) {
        strcat(daystring, "MO,");
    }
    if (a & 0x04) {
        strcat(daystring, "TU,");
    }
    if (a & 0x08) {
        strcat(daystring, "WE,");
    }
    if (a & 0x10) {
        strcat(daystring, "TH,");
    }
    if (a & 0x20) {
        strcat(daystring, "FR,");
    }
    if (a & 0x40) {
        strcat(daystring, "SA,");
    }

    if (strlen(daystring)) {
        daystring[strlen(daystring) - 1] = 0;
    }

    return(daystring);
}

static void PrintRrule(FILE *fptr, char *recur_data, int size, TNEFStruct *TNEF) {
    variableLength *filename;

    if (size < 0x1F) {
        return;
    }

    fprintf(fptr, "RRULE:FREQ=");

    if (recur_data[0x04] == 0x0A) {
        fprintf(fptr, "DAILY");

        if (recur_data[0x16] == 0x23 || recur_data[0x16] == 0x22 ||
                recur_data[0x16] == 0x21) {
            if ((filename=MAPIFindUserProp(&(TNEF->MapiProperties),
                    PROP_TAG(PT_I2, 0x0011))) != MAPI_UNDEFINED) {
                fprintf(fptr, ";INTERVAL=%d", *(filename->data));
            }
            if (recur_data[0x16] == 0x22 || recur_data[0x16] == 0x21) {
                fprintf(fptr, ";COUNT=%d",
                    GetRruleCount(recur_data[0x1B], recur_data[0x1A]));
            }
        } else if (recur_data[0x16] == 0x3E) {
            fprintf(fptr, ";BYDAY=MO,TU,WE,TH,FR");
            if (recur_data[0x1A] == 0x22 || recur_data[0x1A] == 0x21) {
                fprintf(fptr, ";COUNT=%d",
                    GetRruleCount(recur_data[0x1F], recur_data[0x1E]));
            }
        }
    } else if (recur_data[0x04] == 0x0B) {
        fprintf(fptr, "WEEKLY;INTERVAL=%d;BYDAY=%s",
            recur_data[0x0E], GetRruleDayname(recur_data[0x16]));
        if (recur_data[0x1A] == 0x22 || recur_data[0x1A] == 0x21) {
            fprintf(fptr, ";COUNT=%d",
                GetRruleCount(recur_data[0x1F], recur_data[0x1E]));
        }
    } else if (recur_data[0x04] == 0x0C) {
        fprintf(fptr, "MONTHLY");
        if (recur_data[0x06] == 0x02) {
            fprintf(fptr, ";INTERVAL=%d;BYMONTHDAY=%d", recur_data[0x0E],
                recur_data[0x16]);
            if (recur_data[0x1A] == 0x22 || recur_data[0x1A] == 0x21) {
                fprintf(fptr, ";COUNT=%d", GetRruleCount(recur_data[0x1F],
                    recur_data[0x1E]));
            }
        } else if (recur_data[0x06] == 0x03) {
            fprintf(fptr, ";BYDAY=%s;BYSETPOS=%d;INTERVAL=%d",
                GetRruleDayname(recur_data[0x16]),
                recur_data[0x1A] == 0x05 ? -1 : recur_data[0x1A],
                recur_data[0x0E]);
            if (recur_data[0x1E] == 0x22 || recur_data[0x1E] == 0x21) {
                fprintf(fptr, ";COUNT=%d", GetRruleCount(recur_data[0x23],
                    recur_data[0x22]));
            }
        }
    } else if (recur_data[0x04] == 0x0D) {
        fprintf(fptr, "YEARLY;BYMONTH=%d",
                GetRruleMonthNum(recur_data[0x0A], recur_data[0x0B]));
        if (recur_data[0x06] == 0x02) {
            fprintf(fptr, ";BYMONTHDAY=%d", recur_data[0x16]);
        } else if (recur_data[0x06] == 0x03) {
            fprintf(fptr, ";BYDAY=%s;BYSETPOS=%d",
                GetRruleDayname(recur_data[0x16]),
                recur_data[0x1A] == 0x05 ? -1 : recur_data[0x1A]);
        }
        if (recur_data[0x1E] == 0x22 || recur_data[0x1E] == 0x21) {
            fprintf(fptr, ";COUNT=%d", GetRruleCount(recur_data[0x23],
                recur_data[0x22]));
        }
    }
    fprintf(fptr, "\n");
}

static void fprintProperty(TNEFStruct *TNEF, FILE *FPTR, DWORD PROPTYPE, DWORD PROPID, char TEXT[]) {
    variableLength *vl;
    if ((vl=MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PROPTYPE, PROPID))) != MAPI_UNDEFINED) {
        if (vl->size > 0) {
            if ((vl->size == 1) && (vl->data[0] == 0)) {
            } else {
                fprintf(FPTR, TEXT, vl->data);
            }
	}
    }
}

static void fprintUserProp(TNEFStruct *TNEF, FILE *FPTR, DWORD PROPTYPE, DWORD PROPID, char TEXT[]) {
    variableLength *vl;
    if ((vl=MAPIFindUserProp(&(TNEF->MapiProperties), PROP_TAG(PROPTYPE, PROPID))) != MAPI_UNDEFINED) {
        if (vl->size > 0) {
            if ((vl->size == 1) && (vl->data[0] == 0)) {
            } else {
                fprintf(FPTR, TEXT, vl->data);
            }
	}
    }
}

static void quotedfprint(FILE *FPTR, variableLength *VL) {
    int index;

    for (index=0;index<VL->size-1; index++) {
        if (VL->data[index] == '\n') {
            fprintf(FPTR, "=0A");
        } else if (VL->data[index] == '\r') {
        } else {
            fprintf(FPTR, "%c", VL->data[index]);
        }
    }
}

static void Cstylefprint(FILE *FPTR, variableLength *VL) {
    int index;

    for (index=0;index<VL->size-1; index++) {
        if (VL->data[index] == '\n') {
            fprintf(FPTR, "\\n");
        } else if (VL->data[index] == '\r') {
            // Print nothing.
        } else if (VL->data[index] == ';') {
            fprintf(FPTR, "\\;");
        } else if (VL->data[index] == ',') {
            fprintf(FPTR, "\\,");
        } else if (VL->data[index] == '\\') {
            fprintf(FPTR, "\\");
        } else {
            fprintf(FPTR, "%c", VL->data[index]);
        }
    }
}

static void PrintRTF(FILE *fptr, variableLength *VL) {
    int index;
    char *byte;
    int brace_ct;
    int key;

    key = 0;
    brace_ct = 0;

    for(index = 0, byte=VL->data; index < VL->size; index++, byte++) {
        if (*byte == '}') {
            brace_ct--;
            key = 0;
            continue;
        }
        if (*byte == '{') {
            brace_ct++;
            continue;
        }
        if (*byte == '\\') {
            key = 1;
        }
        if (isspace(*byte)) {
            key = 0;
        }
        if ((brace_ct == 1) && (key == 0)) {
            if (*byte == '\n') {
                fprintf(fptr, "\\n");
            } else if (*byte == '\r') {
                // Print nothing.
            } else if (*byte == ';') {
                fprintf(fptr, "\\;");
            } else if (*byte == ',') {
                fprintf(fptr, "\\,");
            } else if (*byte == '\\') {
                fprintf(fptr, "\\");
            } else {
                fprintf(fptr, "%c", *byte );
            }
        }
    }
    fprintf(fptr, "\n");
}

gboolean SaveVCalendar(FILE *fptr, TNEFStruct *TNEF) {
    variableLength *filename;
    char *charptr, *charptr2;
    int index;
    DDWORD *ddword_ptr;
    DDWORD ddword_val;
    dtr thedate;

        fprintf(fptr, "BEGIN:VCALENDAR\n");
        if (TNEF->messageClass[0] != 0) {
            charptr2=TNEF->messageClass;
            charptr=charptr2;
            while (*charptr != 0) {
                if (*charptr == '.') {
                    charptr2 = charptr;
                }
                charptr++;
            }
            if (strcmp(charptr2, ".MtgCncl") == 0) {
                fprintf(fptr, "METHOD:CANCEL\n");
            } else {
                fprintf(fptr, "METHOD:REQUEST\n");
            }
        } else {
            fprintf(fptr, "METHOD:REQUEST\n");
        }
        fprintf(fptr, PRODID);
        fprintf(fptr, "VERSION:2.0\n");
        fprintf(fptr, "BEGIN:VEVENT\n");

        // UID
        // After alot of comparisons, I'm reasonably sure this is totally
        // wrong.  But it's not really necessary.
        //
        // I think it only exists to connect future modification entries to
        // this entry. so as long as it's incorrectly interpreted the same way
        // every time, it should be ok :)
        filename = NULL;
        if ((filename=MAPIFindUserProp(&(TNEF->MapiProperties),
                        PROP_TAG(PT_BINARY, 0x3))) == MAPI_UNDEFINED) {
            if ((filename=MAPIFindUserProp(&(TNEF->MapiProperties),
                            PROP_TAG(PT_BINARY, 0x23))) == MAPI_UNDEFINED) {
                filename = NULL;
            }
        }
        if (filename!=NULL) {
            fprintf(fptr, "UID:");
            for(index=0;index<filename->size;index++) {
                fprintf(fptr,"%02X", (unsigned char)filename->data[index]);
            }
            fprintf(fptr,"\n");
        }

        // Sequence
        filename = NULL;
        if ((filename=MAPIFindUserProp(&(TNEF->MapiProperties),
                        PROP_TAG(PT_LONG, 0x8201))) != MAPI_UNDEFINED) {
            ddword_ptr = (DDWORD*)filename->data;
            fprintf(fptr, "SEQUENCE:%i\n", (int) *ddword_ptr);
        }
        if ((filename=MAPIFindProperty(&(TNEF->MapiProperties),
                        PROP_TAG(PT_BINARY, PR_SENDER_SEARCH_KEY)))
                != MAPI_UNDEFINED) {
            charptr = filename->data;
            charptr2 = strstr(charptr, ":");
            if (charptr2 == NULL)
                charptr2 = charptr;
            else
                charptr2++;
            fprintf(fptr, "ORGANIZER;CN=\"%s\":MAILTO:%s\n",
                    charptr2, charptr2);
        }

        // Required Attendees
        if ((filename = MAPIFindUserProp(&(TNEF->MapiProperties),
                        PROP_TAG(PT_STRING8, 0x823b))) != MAPI_UNDEFINED) {
                // We have a list of required participants, so
                // write them out.
            if (filename->size > 1) {
                charptr = filename->data-1;
                charptr2=strstr(charptr+1, ";");
                while (charptr != NULL) {
                    charptr++;
                    charptr2 = strstr(charptr, ";");
                    if (charptr2 != NULL) {
                        *charptr2 = 0;
                    }
                    while (*charptr == ' ')
                        charptr++;
                    fprintf(fptr, "ATTENDEE;PARTSTAT=NEEDS-ACTION;");
                    fprintf(fptr, "ROLE=REQ-PARTICIPANT;RSVP=TRUE;");
                    fprintf(fptr, "CN=\"%s\":MAILTO:%s\n",
                                charptr, charptr);
                    charptr = charptr2;
                }
            }
            // Optional attendees
            if ((filename = MAPIFindUserProp(&(TNEF->MapiProperties),
                            PROP_TAG(PT_STRING8, 0x823c))) != MAPI_UNDEFINED) {
                    // The list of optional participants
                if (filename->size > 1) {
                    charptr = filename->data-1;
                    charptr2=strstr(charptr+1, ";");
                    while (charptr != NULL) {
                        charptr++;
                        charptr2 = strstr(charptr, ";");
                        if (charptr2 != NULL) {
                            *charptr2 = 0;
                        }
                        while (*charptr == ' ')
                            charptr++;
                        fprintf(fptr, "ATTENDEE;PARTSTAT=NEEDS-ACTION;");
                        fprintf(fptr, "ROLE=OPT-PARTICIPANT;RSVP=TRUE;");
                        fprintf(fptr, "CN=\"%s\":MAILTO:%s\n",
                                charptr, charptr);
                        charptr = charptr2;
                    }
                }
            }
        } else if ((filename = MAPIFindUserProp(&(TNEF->MapiProperties),
                        PROP_TAG(PT_STRING8, 0x8238))) != MAPI_UNDEFINED) {
            if (filename->size > 1) {
                charptr = filename->data-1;
                while (charptr != NULL) {
                    charptr++;
                    charptr2 = strstr(charptr, ";");
                    if (charptr2 != NULL) {
                        *charptr2 = 0;
                    }
                    while (*charptr == ' ')
                        charptr++;
                    fprintf(fptr, "ATTENDEE;PARTSTAT=NEEDS-ACTION;");
                    fprintf(fptr, "ROLE=REQ-PARTICIPANT;RSVP=TRUE;");
                    fprintf(fptr, "CN=\"%s\":MAILTO:%s\n",
                                charptr, charptr);
                    charptr = charptr2;
                }
            }

        }
        // Summary
        filename = NULL;
        if((filename=MAPIFindProperty(&(TNEF->MapiProperties),
                        PROP_TAG(PT_STRING8, PR_CONVERSATION_TOPIC)))
                != MAPI_UNDEFINED) {
            fprintf(fptr, "SUMMARY:");
            Cstylefprint(fptr, filename);
            fprintf(fptr, "\n");
        }

        // Description
        if ((filename=MAPIFindProperty(&(TNEF->MapiProperties),
                                PROP_TAG(PT_BINARY, PR_RTF_COMPRESSED)))
                != MAPI_UNDEFINED) {
            variableLength buf;
            if ((buf.data = DecompressRTF(filename, &(buf.size))) != NULL) {
                fprintf(fptr, "DESCRIPTION:");
                PrintRTF(fptr, &buf);
                free(buf.data);
            }

        }

        // Location
        filename = NULL;
        if ((filename=MAPIFindUserProp(&(TNEF->MapiProperties),
                        PROP_TAG(PT_STRING8, 0x0002))) == MAPI_UNDEFINED) {
            if ((filename=MAPIFindUserProp(&(TNEF->MapiProperties),
                            PROP_TAG(PT_STRING8, 0x8208))) == MAPI_UNDEFINED) {
                filename = NULL;
            }
        }
        if (filename != NULL) {
            fprintf(fptr,"LOCATION: %s\n", filename->data);
        }
        // Date Start
        filename = NULL;
        if ((filename=MAPIFindUserProp(&(TNEF->MapiProperties),
                        PROP_TAG(PT_SYSTIME, 0x820d))) == MAPI_UNDEFINED) {
            if ((filename=MAPIFindUserProp(&(TNEF->MapiProperties),
                            PROP_TAG(PT_SYSTIME, 0x8516))) == MAPI_UNDEFINED) {
                filename=NULL;
            }
        }
        if (filename != NULL) {
            fprintf(fptr, "DTSTART:");
            MAPISysTimetoDTR(filename->data, &thedate);
            fprintf(fptr,"%04i%02i%02iT%02i%02i%02iZ\n",
                    thedate.wYear, thedate.wMonth, thedate.wDay,
                    thedate.wHour, thedate.wMinute, thedate.wSecond);
        }
        // Date End
        filename = NULL;
        if ((filename=MAPIFindUserProp(&(TNEF->MapiProperties),
                        PROP_TAG(PT_SYSTIME, 0x820e))) == MAPI_UNDEFINED) {
            if ((filename=MAPIFindUserProp(&(TNEF->MapiProperties),
                            PROP_TAG(PT_SYSTIME, 0x8517))) == MAPI_UNDEFINED) {
                filename=NULL;
            }
        }
        if (filename != NULL) {
            fprintf(fptr, "DTEND:");
            MAPISysTimetoDTR(filename->data, &thedate);
            fprintf(fptr,"%04i%02i%02iT%02i%02i%02iZ\n",
                    thedate.wYear, thedate.wMonth, thedate.wDay,
                    thedate.wHour, thedate.wMinute, thedate.wSecond);
        }
        // Date Stamp
        filename = NULL;
        if ((filename=MAPIFindUserProp(&(TNEF->MapiProperties),
                        PROP_TAG(PT_SYSTIME, 0x8202))) != MAPI_UNDEFINED) {
            fprintf(fptr, "CREATED:");
            MAPISysTimetoDTR(filename->data, &thedate);
            fprintf(fptr,"%04i%02i%02iT%02i%02i%02iZ\n",
                    thedate.wYear, thedate.wMonth, thedate.wDay,
                    thedate.wHour, thedate.wMinute, thedate.wSecond);
        }
        // Class
        filename = NULL;
        if ((filename=MAPIFindUserProp(&(TNEF->MapiProperties),
                        PROP_TAG(PT_BOOLEAN, 0x8506))) != MAPI_UNDEFINED) {
            ddword_ptr = (DDWORD*)filename->data;
#ifdef YTNEF_OLD_SWAPDDWORD
            ddword_val = SwapDDWord((BYTE*)ddword_ptr);
#else
            ddword_val = SwapDDWord((BYTE*)ddword_ptr, sizeof(DDWORD));
#endif
            fprintf(fptr, "CLASS:" );
            if (ddword_val == 1) {
                fprintf(fptr,"PRIVATE\n");
            } else {
                fprintf(fptr,"PUBLIC\n");
            }
        }
        // Recurrence
        filename = NULL;
        if ((filename=MAPIFindUserProp(&(TNEF->MapiProperties),
                        PROP_TAG(PT_BINARY, 0x8216))) != MAPI_UNDEFINED) {
            PrintRrule(fptr, filename->data, filename->size, TNEF);
        }

        // Wrap it up
        fprintf(fptr, "END:VEVENT\n");
        fprintf(fptr, "END:VCALENDAR\n");
	return TRUE;
}

    	

gboolean SaveVTask(FILE *fptr, TNEFStruct *TNEF) {
	variableLength *filename;
	char *charptr, *charptr2;
	dtr thedate;
	DDWORD *ddword_ptr;
	DDWORD ddword_val;

        fprintf(fptr, "BEGIN:VCALENDAR\n");
        fprintf(fptr, PRODID);
        fprintf(fptr, "VERSION:2.0\n");
        fprintf(fptr, "METHOD:PUBLISH\n");
        filename = NULL;

        fprintf(fptr, "BEGIN:VTODO\n");
        if (TNEF->messageID[0] != 0) {
            fprintf(fptr,"UID:%s\n", TNEF->messageID);
        }
        filename = MAPIFindUserProp(&(TNEF->MapiProperties), \
                        PROP_TAG(PT_STRING8, 0x8122));
        if (filename != MAPI_UNDEFINED) {
            fprintf(fptr, "ORGANIZER:%s\n", filename->data);
        }


        if ((filename = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_DISPLAY_TO))) != MAPI_UNDEFINED) {
            filename = MAPIFindUserProp(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, 0x811f));
        }
        if ((filename != MAPI_UNDEFINED) && (filename->size > 1)) {
            charptr = filename->data-1;
            while (charptr != NULL) {
                charptr++;
                charptr2 = strstr(charptr, ";");
                if (charptr2 != NULL) {
                    *charptr2 = 0;
                }
                while (*charptr == ' ')
                    charptr++;
                fprintf(fptr, "ATTENDEE;CN=%s;ROLE=REQ-PARTICIPANT:%s\n", charptr, charptr);
                charptr = charptr2;
            }
        }

        if (TNEF->subject.size > 0) {
            fprintf(fptr,"SUMMARY:");
            Cstylefprint(fptr,&(TNEF->subject));
            fprintf(fptr,"\n");
        }

        if (TNEF->body.size > 0) {
            fprintf(fptr,"DESCRIPTION:");
            Cstylefprint(fptr,&(TNEF->body));
            fprintf(fptr,"\n");
        }

        filename = MAPIFindProperty(&(TNEF->MapiProperties), \
                    PROP_TAG(PT_SYSTIME, PR_CREATION_TIME));
        if (filename != MAPI_UNDEFINED) {
            fprintf(fptr, "DTSTAMP:");
            MAPISysTimetoDTR(filename->data, &thedate);
            fprintf(fptr,"%04i%02i%02iT%02i%02i%02iZ\n",
                    thedate.wYear, thedate.wMonth, thedate.wDay,
                    thedate.wHour, thedate.wMinute, thedate.wSecond);
        }

        filename = MAPIFindUserProp(&(TNEF->MapiProperties), \
                    PROP_TAG(PT_SYSTIME, 0x8517));
        if (filename != MAPI_UNDEFINED) {
            fprintf(fptr, "DUE:");
            MAPISysTimetoDTR(filename->data, &thedate);
            fprintf(fptr,"%04i%02i%02iT%02i%02i%02iZ\n",
                    thedate.wYear, thedate.wMonth, thedate.wDay,
                    thedate.wHour, thedate.wMinute, thedate.wSecond);
        }
        filename = MAPIFindProperty(&(TNEF->MapiProperties), \
                    PROP_TAG(PT_SYSTIME, PR_LAST_MODIFICATION_TIME));
        if (filename != MAPI_UNDEFINED) {
            fprintf(fptr, "LAST-MODIFIED:");
            MAPISysTimetoDTR(filename->data, &thedate);
            fprintf(fptr,"%04i%02i%02iT%02i%02i%02iZ\n",
                    thedate.wYear, thedate.wMonth, thedate.wDay,
                    thedate.wHour, thedate.wMinute, thedate.wSecond);
        }
        // Class
        filename = MAPIFindUserProp(&(TNEF->MapiProperties), \
                        PROP_TAG(PT_BOOLEAN, 0x8506));
        if (filename != MAPI_UNDEFINED) {
            ddword_ptr = (DDWORD*)filename->data;
#ifdef YTNEF_OLD_SWAPDDWORD
            ddword_val = SwapDDWord((BYTE*)ddword_ptr);
#else
            ddword_val = SwapDDWord((BYTE*)ddword_ptr, sizeof(DDWORD));
#endif
            fprintf(fptr, "CLASS:" );
            if (ddword_val == 1) {
                fprintf(fptr,"PRIVATE\n");
            } else {
                fprintf(fptr,"PUBLIC\n");
            }
        }
        fprintf(fptr, "END:VTODO\n");
        fprintf(fptr, "END:VCALENDAR\n");

        return TRUE;
}

gboolean SaveVCard(FILE *fptr, TNEFStruct *TNEF) {
    variableLength *vl;
    variableLength *pobox, *street, *city, *state, *zip, *country;
    dtr thedate;
    int boolean;

    if ((vl = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_DISPLAY_NAME))) == MAPI_UNDEFINED) {
        vl=MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_COMPANY_NAME));
    }

        fprintf(fptr, "BEGIN:VCARD\n");
        fprintf(fptr, "VERSION:2.1\n");
        if (vl != MAPI_UNDEFINED) {
            fprintf(fptr, "FN:%s\n", vl->data);
        }
        fprintProperty(TNEF, fptr, PT_STRING8, PR_NICKNAME, "NICKNAME:%s\n");
        fprintUserProp(TNEF, fptr, PT_STRING8, 0x8554, "MAILER:Microsoft Outlook %s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_SPOUSE_NAME, "X-EVOLUTION-SPOUSE:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_MANAGER_NAME, "X-EVOLUTION-MANAGER:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_ASSISTANT, "X-EVOLUTION-ASSISTANT:%s\n");

        // Organizational
        if ((vl=MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_COMPANY_NAME))) != MAPI_UNDEFINED) {
            if (vl->size > 0) {
                if ((vl->size == 1) && (vl->data[0] == 0)) {
                } else {
                    fprintf(fptr,"ORG:%s", vl->data);
                    if ((vl=MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_DEPARTMENT_NAME))) != MAPI_UNDEFINED) {
                        fprintf(fptr,";%s", vl->data);
                    }
                    fprintf(fptr, "\n");
                }
            }
        }

        fprintProperty(TNEF, fptr, PT_STRING8, PR_OFFICE_LOCATION, "X-EVOLUTION-OFFICE:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_TITLE, "TITLE:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_PROFESSION, "ROLE:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_BODY, "NOTE:%s\n");
        if (TNEF->body.size > 0) {
            fprintf(fptr, "NOTE;QUOTED-PRINTABLE:");
            quotedfprint(fptr, &(TNEF->body));
            fprintf(fptr,"\n");
        }


        // Business Address
        boolean = 0;
        if ((pobox = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_POST_OFFICE_BOX))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if ((street = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_STREET_ADDRESS))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if ((city = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_LOCALITY))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if ((state = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_STATE_OR_PROVINCE))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if ((zip = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_POSTAL_CODE))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if ((country = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_COUNTRY))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if (boolean == 1) {
            fprintf(fptr, "ADR;QUOTED-PRINTABLE;WORK:");
            if (pobox != MAPI_UNDEFINED) {
                quotedfprint(fptr, pobox);
            }
            fprintf(fptr, ";;");
            if (street != MAPI_UNDEFINED) {
                quotedfprint(fptr, street);
            }
            fprintf(fptr, ";");
            if (city != MAPI_UNDEFINED) {
                quotedfprint(fptr, city);
            }
            fprintf(fptr, ";");
            if (state != MAPI_UNDEFINED) {
                quotedfprint(fptr, state);
            }
            fprintf(fptr, ";");
            if (zip != MAPI_UNDEFINED) {
                quotedfprint(fptr, zip);
            }
            fprintf(fptr, ";");
            if (country != MAPI_UNDEFINED) {
                quotedfprint(fptr, country);
            }
            fprintf(fptr,"\n");
            if ((vl = MAPIFindUserProp(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, 0x801b))) != MAPI_UNDEFINED) {
                fprintf(fptr, "LABEL;QUOTED-PRINTABLE;WORK:");
                quotedfprint(fptr, vl);
                fprintf(fptr,"\n");
            }
        }

        // Home Address
        boolean = 0;
        if ((pobox = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_HOME_ADDRESS_POST_OFFICE_BOX))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if ((street = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_HOME_ADDRESS_STREET))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if ((city = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_HOME_ADDRESS_CITY))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if ((state = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_HOME_ADDRESS_STATE_OR_PROVINCE))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if ((zip = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_HOME_ADDRESS_POSTAL_CODE))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if ((country = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_HOME_ADDRESS_COUNTRY))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if (boolean == 1) {
            fprintf(fptr, "ADR;QUOTED-PRINTABLE;HOME:");
            if (pobox != MAPI_UNDEFINED) {
                quotedfprint(fptr, pobox);
            }
            fprintf(fptr, ";;");
            if (street != MAPI_UNDEFINED) {
                quotedfprint(fptr, street);
            }
            fprintf(fptr, ";");
            if (city != MAPI_UNDEFINED) {
                quotedfprint(fptr, city);
            }
            fprintf(fptr, ";");
            if (state != MAPI_UNDEFINED) {
                quotedfprint(fptr, state);
            }
            fprintf(fptr, ";");
            if (zip != MAPI_UNDEFINED) {
                quotedfprint(fptr, zip);
            }
            fprintf(fptr, ";");
            if (country != MAPI_UNDEFINED) {
                quotedfprint(fptr, country);
            }
            fprintf(fptr,"\n");
            if ((vl = MAPIFindUserProp(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, 0x801a))) != MAPI_UNDEFINED) {
                fprintf(fptr, "LABEL;QUOTED-PRINTABLE;WORK:");
                quotedfprint(fptr, vl);
                fprintf(fptr,"\n");
            }
        }

        // Other Address
        boolean = 0;
        if ((pobox = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_OTHER_ADDRESS_POST_OFFICE_BOX))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if ((street = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_OTHER_ADDRESS_STREET))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if ((city = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_OTHER_ADDRESS_CITY))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if ((state = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_OTHER_ADDRESS_STATE_OR_PROVINCE))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if ((zip = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_OTHER_ADDRESS_POSTAL_CODE))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if ((country = MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, PR_OTHER_ADDRESS_COUNTRY))) != MAPI_UNDEFINED) {
            boolean = 1;
        }
        if (boolean == 1) {
            fprintf(fptr, "ADR;QUOTED-PRINTABLE;OTHER:");
            if (pobox != MAPI_UNDEFINED) {
                quotedfprint(fptr, pobox);
            }
            fprintf(fptr, ";;");
            if (street != MAPI_UNDEFINED) {
                quotedfprint(fptr, street);
            }
            fprintf(fptr, ";");
            if (city != MAPI_UNDEFINED) {
                quotedfprint(fptr, city);
            }
            fprintf(fptr, ";");
            if (state != MAPI_UNDEFINED) {
                quotedfprint(fptr, state);
            }
            fprintf(fptr, ";");
            if (zip != MAPI_UNDEFINED) {
                quotedfprint(fptr, zip);
            }
            fprintf(fptr, ";");
            if (country != MAPI_UNDEFINED) {
                quotedfprint(fptr, country);
            }
            fprintf(fptr,"\n");
        }


        fprintProperty(TNEF, fptr, PT_STRING8, PR_CALLBACK_TELEPHONE_NUMBER, "TEL;X-EVOLUTION-CALLBACK:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_PRIMARY_TELEPHONE_NUMBER, "TEL;PREF:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_MOBILE_TELEPHONE_NUMBER, "TEL;CELL:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_RADIO_TELEPHONE_NUMBER, "TEL;X-EVOLUTION-RADIO:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_CAR_TELEPHONE_NUMBER, "TEL;CAR:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_OTHER_TELEPHONE_NUMBER, "TEL;VOICE:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_PAGER_TELEPHONE_NUMBER, "TEL;PAGER:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_TELEX_NUMBER, "TEL;X-EVOLUTION-TELEX:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_ISDN_NUMBER, "TEL;ISDN:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_HOME2_TELEPHONE_NUMBER, "TEL;HOME:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_TTYTDD_PHONE_NUMBER, "TEL;X-EVOLUTION-TTYTDD:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_HOME_TELEPHONE_NUMBER, "TEL;HOME;VOICE:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_ASSISTANT_TELEPHONE_NUMBER, "TEL;X-EVOLUTION-ASSISTANT:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_COMPANY_MAIN_PHONE_NUMBER, "TEL;WORK:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_BUSINESS_TELEPHONE_NUMBER, "TEL;WORK:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_BUSINESS2_TELEPHONE_NUMBER, "TEL;WORK;VOICE:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_PRIMARY_FAX_NUMBER, "TEL;PREF;FAX:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_BUSINESS_FAX_NUMBER, "TEL;WORK;FAX:%s\n");
        fprintProperty(TNEF, fptr, PT_STRING8, PR_HOME_FAX_NUMBER, "TEL;HOME;FAX:%s\n");


        // Email addresses
        if ((vl=MAPIFindUserProp(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, 0x8083))) == MAPI_UNDEFINED) {
            vl=MAPIFindUserProp(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, 0x8084));
        }
        if (vl != MAPI_UNDEFINED) {
            if (vl->size > 0)
                fprintf(fptr, "EMAIL:%s\n", vl->data);
        }
        if ((vl=MAPIFindUserProp(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, 0x8093))) == MAPI_UNDEFINED) {
            vl=MAPIFindUserProp(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, 0x8094));
        }
        if (vl != MAPI_UNDEFINED) {
            if (vl->size > 0)
                fprintf(fptr, "EMAIL:%s\n", vl->data);
        }
        if ((vl=MAPIFindUserProp(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, 0x80a3))) == MAPI_UNDEFINED) {
            vl=MAPIFindUserProp(&(TNEF->MapiProperties), PROP_TAG(PT_STRING8, 0x80a4));
        }
        if (vl != MAPI_UNDEFINED) {
            if (vl->size > 0)
                fprintf(fptr, "EMAIL:%s\n", vl->data);
        }

        fprintProperty(TNEF, fptr, PT_STRING8, PR_BUSINESS_HOME_PAGE, "URL:%s\n");
        fprintUserProp(TNEF, fptr, PT_STRING8, 0x80d8, "FBURL:%s\n");



        //Birthday
        if ((vl=MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_SYSTIME, PR_BIRTHDAY))) != MAPI_UNDEFINED) {
            fprintf(fptr, "BDAY:");
            MAPISysTimetoDTR(vl->data, &thedate);
            fprintf(fptr, "%i-%02i-%02i\n", thedate.wYear, thedate.wMonth, thedate.wDay);
        }

        //Anniversary
        if ((vl=MAPIFindProperty(&(TNEF->MapiProperties), PROP_TAG(PT_SYSTIME, PR_WEDDING_ANNIVERSARY))) != MAPI_UNDEFINED) {
            fprintf(fptr, "X-EVOLUTION-ANNIVERSARY:");
            MAPISysTimetoDTR(vl->data, &thedate);
            fprintf(fptr, "%i-%02i-%02i\n", thedate.wYear, thedate.wMonth, thedate.wDay);
        }
        fprintf(fptr, "END:VCARD\n");
	return TRUE;
}
