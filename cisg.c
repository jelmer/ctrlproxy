/* 
    cisg: ControlProxy IRC stats generator
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#define _GNU_SOURCE
#include "ctrlproxy.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <tdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_POPT_H
#include <popt.h>
#endif

time_t start_time;

int traverse_keys(TDB_CONTEXT *tdb_context, TDB_DATA key, TDB_DATA value, void *pattern)
{
	long *ivalue;
	if(!key.dptr) return 0;

	ivalue = (long *)value.dptr;
	printf("%s: %ld\n", key.dptr, *ivalue);
	return 0;
}

void print_html_chan(TDB_CONTEXT *tdb, char *network, char *channel) 
{
	time_t curtime;
	curtime = time(NULL);
	printf("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
	printf("<html xmlns=\"http://www.w3.org/1999/xhtml\">\n");
	printf("<head>\n");
	pritnf(" <meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\" />\n");
	printf(" <link rel=\"stylesheet\" title=\"Default\" href=\"style.css\" type=\"text/css\" />\n");
	printf(" <meta name=\"Author\" content=\"FISG v0.3.8 (Fast IRC Stats Generator)\" />\n");
	printf(" <title>CISG Statistics for #%s</title>\n", channel);
	printf("</head>\n");
	printf("<body>\n");
	printf("<div align=\"center\">\n");
	printf("<div class=\"icontents\">\n");
 	printf("<div class="iinfo">\n");
 	printf("<h1>#%s</h1>\n", channel);
	printf("  <p class=\"idate\">Statistics generated on <b>%s</b></p>\n", asctime(&curtime));
	printf("  <p class=\"ivisitors\">A total of <b>%d</b> different nicks/users were represented on <b>#%s</b>.</p>\n", channel);
	printf("  <p class=\"imessage\"></p>\n");
	printf(" </div>\n");
	printf("<!-- ============================================= -->\n");
	printf(" <div class=\"ihourly\">\n");
	printf("  <h2>Most active times</h2>\n");
	printf("  <table class=\"ihourly\" align=\"center\">\n");
	printf("   <tr>\n");
    <th valign="bottom">11.3%<br /><img src="./blue-v.png" width="15" height="100" alt="11.3%" /></th>\n
    <th valign="bottom">8.9%<br /><img src="./blue-v.png" width="15" height="78" alt="8.9%" /></th>\n
    <th valign="bottom">2.7%<br /><img src="./blue-v.png" width="15" height="24" alt="2.7%" /></th>\n
    <th valign="bottom">1.1%<br /><img src="./blue-v.png" width="15" height="9" alt="1.1%" /></th>\n
    <th valign="bottom">0.2%<br /><img src="./blue-v.png" width="15" height="2" alt="0.2%" /></th>\n
    <th valign="bottom">0.1%<br /><img src="./blue-v.png" width="15" height="1" alt="0.1%" /></th>\n
    <th valign="bottom">0.0%<br /><img src="./green-v.png" width="15" height="0" alt="0.0%" /></th>\n
    <th valign="bottom">0.0%<br /><img src="./green-v.png" width="15" height="0" alt="0.0%" /></th>\n
    <th valign="bottom">0.0%<br /><img src="./green-v.png" width="15" height="0" alt="0.0%" /></th>\n
    <th valign="bottom">0.1%<br /><img src="./green-v.png" width="15" height="0" alt="0.1%" /></th>\n
    <th valign="bottom">0.7%<br /><img src="./green-v.png" width="15" height="6" alt="0.7%" /></th>\n
    <th valign="bottom">1.5%<br /><img src="./green-v.png" width="15" height="13" alt="1.5%" /></th>\n
    <th valign="bottom">2.6%<br /><img src="./yellow-v.png" width="15" height="23" alt="2.6%" /></th>\n
    <th valign="bottom">3.0%<br /><img src="./yellow-v.png" width="15" height="26" alt="3.0%" /></th>\n
    <th valign="bottom">3.6%<br /><img src="./yellow-v.png" width="15" height="32" alt="3.6%" /></th>\n
    <th valign="bottom">4.4%<br /><img src="./yellow-v.png" width="15" height="39" alt="4.4%" /></th>\n
    <th valign="bottom">6.0%<br /><img src="./yellow-v.png" width="15" height="52" alt="6.0%" /></th>\n
    <th valign="bottom">5.1%<br /><img src="./yellow-v.png" width="15" height="45" alt="5.1%" /></th>\n
    <th valign="bottom">5.8%<br /><img src="./red-v.png" width="15" height="51" alt="5.8%" /></th>\n
    <th valign="bottom">6.8%<br /><img src="./red-v.png" width="15" height="59" alt="6.8%" /></th>\n
    <th valign="bottom">9.3%<br /><img src="./red-v.png" width="15" height="82" alt="9.3%" /></th>\n
    <th valign="bottom">8.6%<br /><img src="./red-v.png" width="15" height="75" alt="8.6%" /></th>\n
    <th valign="bottom">9.0%<br /><img src="./red-v.png" width="15" height="79" alt="9.0%" /></th>\n
    <th valign="bottom">9.0%<br /><img src="./red-v.png" width="15" height="79" alt="9.0%" /></th>\n
	printf("   </tr>\n");
	printf("   <tr>\n");
	printf("    <td class=\"hirank\">0</td>\n");
	printf("    <td class=\"rank\">1</td>\n");
	printf("    <td class=\"rank\">2</td>\n");
	printf("    <td class=\"rank\">3</td>\n");
	printf("    <td class=\"rank\">4</td>\n");
	printf("    <td class=\"rank\">5</td>\n");
	printf("    <td class=\"rank\">6</td>\n");
	printf("    <td class=\"rank\">7</td>\n");
	printf("    <td class=\"rank\">8</td>\n");
	printf("    <td class=\"rank\">9</td>\n");
	printf("    <td class=\"rank\">10</td>\n");
	printf("    <td class=\"rank\">11</td>\n");
	printf("    <td class=\"rank\">12</td>\n");
	printf("    <td class=\"rank\">13</td>\n");
	printf("    <td class=\"rank\">14</td>\n");
	printf("    <td class=\"rank\">15</td>\n");
	printf("    <td class=\"rank\">16</td>\n");
	printf("    <td class=\"rank\">17</td>\n");
	printf("    <td class=\"rank\">18</td>\n");
	printf("    <td class=\"rank\">19</td>\n");
	printf("    <td class=\"rank\">20</td>\n");
	printf("    <td class=\"rank\">21</td>\n");
	printf("    <td class=\"rank\">22</td>\n");
	printf("    <td class=\"rank\">23</td>\n");
	printf("   </tr>\n");
	printf("  </table>\n");
	printf("\n");
	printf("  <table  class=\"ihourbars\" align=\"center\">\n");
	printf("   <tr>\n");
	printf("    <td class=\"asmall\"><img src=\"./blue-h.png\" width=\"40\" height=\"15\" alt=\"0-5\" /> = 0-5</td>\n");
	printf("    <td class=\"asmall\"><img src=\"./green-h.png\" width=\"40\" height=\"15\" alt=\"6-11\" /> = 6-11</td>\n");
	printf("    <td class=\"asmall\"><img src=\"./yellow-h.png\" width=\"40\" height=\"15\" alt=\"12-17\" /> = 12-17</td>\n");
	printf("    <td class=\"asmall\"><img src=\"./red-h.png\" width=\"40\" height=\"15\" alt=\"18-23\" /> = 18-23</td>\n");
	printf("   </tr>\n");
	printf("  </table>\n");
	printf(" </div>\n");
	printf("<!-- ============================================= -->\n");
	printf(" <div class=\"itoptalkers\">\n");
	printf("  <h2>Most active users</h2>\n");
	printf("  <table  class=\"itoptalkers\" align=\"center\">\n");
	printf("   <tr>\n");
	printf("    <th width=\"2%%\" class=\"nrank\">#</th>\n");
	printf("    <th width=\"2%%\" class=\"nhappiness\">?</th>\n");
	printf("    <th width=\"10%%\" class=\"nhandle\">Nickname</th>\n");
	printf("    <th width=\"6%%\" class=\"npublics\">Lines</th>\n");
	printf("    <th width=\"15%\" class=\"nactivity\">Activity</th>\n");
	printf("    <th width=\"5%%\" class=\"nwords\">Words</th>\n");
	printf("    <th width=\"2%%\" class=\"nwpp\">W/P</th>\n");
	printf("    <th width=\"2%%\" class=\"ncpw\">C/W</th>\n");
	printf("    <th width=\"50%%\" class=\"ncomment\">Comment</th>\n");
	printf("   </tr>\n");

	/* FIXME: Do this 15 times */
	printf("   <tr>\n");
	printf("    <td class=\"nrank">1</td>\n");
	printf("    <td class=\"nhappiness\"><img src="./happy1.gif" alt="1" /></td>\n");
	printf("    <td class=\"nhandle\">wilmer</td>\n");
	printf("    <td class=\"npublics\">26879</td>\n");
	printf("    <td class=\"nactivity\"><img src=\"./blue-h.png\" width="15" height="15" alt="" /><img src="./yellow-h.png" width="18" height="15" alt="" /><img src="./red-h.png" width="31" height="15" alt="" /></td>\n");
	printf("    <td class=\"nwords">120158</td>\n");
	printf("    <td class=\"nwpp">4.47</td>\n");
	printf("    <td class=\"ncpw">6.83</td>\n");
	printf("    <td class=\"ncomment">yep, dat las ik. cool :-)</td>\n");
	printf("   </tr>\n");
	printf("   <tr>\n");
	printf("    <td class=\"nrank">2</td>\n");
	printf("    <td class=\"nhappiness"><img src="./happy3.gif" alt="3" /></td>\n
	printf("    <td class=\"nhandle">Pink</td>\n");
	printf("    <td class=\"npublics">12857</td>\n");
	printf("    <td class=\"nactivity"><img src="./blue-h.png" width="11" height="15" alt="" /><img src="./green-h.png" width="3" height="15" alt="" /><img src="./yellow-h.png" width="19" height="15" alt="" /><img src="./red-h.png" width="32" height="15" alt="" /></td>\n");
	printf("    <td class=\"nwords">123876</td>\n");
	printf("    <td class=\"nwpp">9.63</td>\n");
	printf("    <td class=\"ncpw">7.17</td>\n");
	printf("    <td class=\"ncomment">[Maurits] < ctrlsoft> zieje, het niveau daalt idd als ik wegga ;-P< Maurits> mwah < Maurits> het niveau zit op de spreekwoordelijke 0 kelvin < Maurits> :) </td>\n");
	printf("   </tr>\n");
	/* FIXME */

	
	printf("  </table>\n");
	printf("<!-- ============================================= -->\n");
	printf(" <h3>These didn't make it to the top:</h3>\n");
	printf(" <table  class=\"ialmosttop\">\n");

	/* FIXME: Print 5 * 4 almost on top */
	printf("  <tr>\n");
	printf("   <td>vietyen (1041)</td>\n");
	printf("   <td>jelmer (882)</td>\n");
	printf("   <td>rel (860)</td>\n");
	printf("   <td>surrounder (840)</td>\n");
	printf("   <td>inx (864)</td>\n");
	printf("  </tr>\n");
	/* FIXME */
	
	printf(" </table>\n");
	printf(" <h3>There were also <b>%d</b> other nicks</h3>\n", FIXME);
	printf(" </div>\n");
	printf("<!-- ============================================= -->\n");
	printf("<div class=\"ibignumbers\">\n");
	printf(" <h2>Big Numbers</h2>\n");
	printf(" <p class=\"isection\">\n");
	printf("  Is <b>%s</b> stupid or just asking too many questions? <b>%f%%</b> of his lines contained a question!\n", FIXME, FIXME);
	printf(" </p>\n");
	printf(" <p class=\"isection\">\n");
	printf("  The loudest one was <b>%s</b>, who yelled <b>%f%%</b> of the time. </p>\n", FIXME, FIXME);
	printf(" <p class=\"isection\">\n");
	printf("  Total of <b>%d</b> URLs were pasted by <b>%s</b>!!\n", FIXME, FIXME);
	printf(" </p>\n");
	printf(" <p class=\"isection\">\n");
	printf("  <b>%s</b> didn't know whether to stay. He/she joined the channel <b>%d</b> times!\n", FIXME, FIXME);
	printf(" </p>\n");
	printf(" <p class=\"isection\">\n");
	printf("  <b>%s</b> kicked the ass most, <b>%d</b> times to be exact!\n");
	printf(" </p>\n");
	printf(" <p class=\"isection\">\n");
	printf("  Obviously someone does not like <b>%s</b>, he/she was kicked <b>%d</b> times!\n");
	printf(" </p>\n");
	printf(" <p class=\"isection\">\n");
	printf("  <b>%s</b> is a clear caps-abuser, <b>%f%%</b> of time he/she wrote in CAPS. </p>\n");
	printf(" <p class=\"isection\">\n");
	printf("  <b>%s</b> is either using drugs or is otherwise very <b>happy</b> person <b>;D</b> </p>\n", FIXME);
	printf(" <p class=\"isection\">\n");
	printf("  On the other hand <b>%s</b> seems to be quite <b>sad</b> <b>:(</b> </p>\n", FIXME);
	printf("</div>\n");
	printf("<!-- ============================================= -->\n");
	printf(" <div class=\"irestinfo\">\n");
	printf("  <p class=\"iauthor\">Statistics generated by CISG (part of <a href=\"http://ctrlproxy.vernstok.nl/\">ctrlproxy</a> by Jelmer Vernooij). HTML output based on the HTML output from <a href=\"http://www.tnsp.org/fisg.php\">FISG (Fast IRC Stats Generator) v0.3.8</a> by ccr/TNSP (C) Copyright 2003 TNSP</p>\n");
	printf("  <p class=\"itime\">Stats generated in <b>%d seconds</b>.</p>\n", time(NULL) - start_time);
	printf(" </div>\n");
	printf("<!-- ============================================= -->\n");
	printf("</div>\n");
	printf("</div>\n");
	printf("</body>\n");
	printf("</html>\n");
}

int main(int argc, char **argv)
{
	TDB_CONTEXT *tdb_context;
	char *statsfile, *fullchan, *network = NULL, *channel = NULL;
	struct popt_option options[] = {
		{'h', "html", "HTML output" }
	};

	start_time = time(NULL);
	
	poptContext pc;
	
	pc = poptGetContext(argv[0], argc, argv, options, 0);

	poptSetOtherOptionHelp(pc, "stats-file [network[/channel]]");
	
	while(arg = poptGetNextOpt(pc)) {
	}

	statsfile = poptGetArg(pc);
	if(!statsfile) {
		poptPrintHelp(pc, stderr, 0);
		return 1;
	}

	tdb_context = tdb_open(statsfile, 0, 0, O_RDONLY, 00700);

	fullchan = poptGetArg(pc);
	if(!fullchan) { 
		if(html) print_html_networklist(tdb_context);
		else print_plain_networklist(tdb_context);
		return 0;
	}

	network = strdup(fullchan);

	channel = strchr(fullchan, '/');
	if(!channel) {
		if(html) print_html_channellist(tdb_context, network);
		else print_plain_channellist(tdb_context, network);
		return 0;
	}

	*channel = '\0'; channel++;
		
	if(html) print_html_chan(tdb_context, network, channel);
	else print_plain_chan(tdb_context, network, channel);
							
	tdb_traverse(tdb_context, traverse_keys, NULL);
	return 0;
}
