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
#include "statsutil.h"
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
#include <popt.h>

#define PERCENTAGE(n,p) (nick_get_property((n),"lines") > 0.0?((1.0 * nick_get_property((n),(p))) / (1.0 * nick_get_property((n), "lines")) * 100.0):0.0)

time_t start_time;

void print_html_header(FILE *out, char *title) {
	fprintf(out, "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
	fprintf(out, "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n");
	fprintf(out, "<head>\n");
	fprintf(out, " <meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\" />\n");
	fprintf(out, " <link rel=\"stylesheet\" title=\"Default\" href=\"style.css\" type=\"text/css\" />\n");
	fprintf(out, " <meta name=\"Author\" content=\"CISG (ControlProxy IRC Stats Generator)\" />\n");
	fprintf(out, " <title>%s</title>\n", title);
	fprintf(out, "</head>\n");
	fprintf(out, "<body>\n");
	fprintf(out, "<div align=\"center\">\n");
	fprintf(out, "<div class=\"icontents\">\n");
}

void print_html_footer(FILE *out) {
	fprintf(out, "<!-- ============================================= -->\n");
	fprintf(out, "</div>\n");
	fprintf(out, "</div>\n");
	fprintf(out, "</body>\n");
	fprintf(out, "</html>\n");
}

void print_html_chan(FILE *out, struct channel *channelstruct)
{
	time_t curtime;
	struct nick *n;
	int i, j;
	curtime = time(NULL);
	char *title;
	GList *nicks;
	asprintf(&title, "CISG Statistics for %s", channelstruct->name);
	print_html_header(out, title);
	free(title);
	nicks = get_nicks_sorted_by_property(channelstruct, "lines");
	
 	fprintf(out, "<div class=\"iinfo\">\n");
 	fprintf(out, "<h1>%s</h1>\n", channelstruct->name);
	fprintf(out, "  <p class=\"idate\">Statistics generated on <b>%s</b></p>\n", ctime(&curtime));
	fprintf(out, "  <p class=\"ivisitors\">A total of <b>%ld</b> different nicks/users were represented on <b>%s</b>.</p>\n", g_list_length(nicks), channelstruct->name);
	fprintf(out, "  <p class=\"imessage\"></p>\n");
	fprintf(out, " </div>\n");
	fprintf(out, "<!-- ============================================= -->\n");
	fprintf(out, " <div class=\"ihourly\">\n");
	fprintf(out, "  <h2>Most active times</h2>\n");
	fprintf(out, "  <table class=\"ihourly\" align=\"center\">\n");
	fprintf(out, "   <tr>\n");
	/*
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
    <th valign="bottom">9.0%<br /><img src="./red-v.png" width="15" height="79" alt="9.0%" /></th>\n*/
	fprintf(out, "   </tr>\n");
	fprintf(out, "   <tr>\n");
	fprintf(out, "    <td class=\"hirank\">0</td>\n");
	fprintf(out, "    <td class=\"rank\">1</td>\n");
	fprintf(out, "    <td class=\"rank\">2</td>\n");
	fprintf(out, "    <td class=\"rank\">3</td>\n");
	fprintf(out, "    <td class=\"rank\">4</td>\n");
	fprintf(out, "    <td class=\"rank\">5</td>\n");
	fprintf(out, "    <td class=\"rank\">6</td>\n");
	fprintf(out, "    <td class=\"rank\">7</td>\n");
	fprintf(out, "    <td class=\"rank\">8</td>\n");
	fprintf(out, "    <td class=\"rank\">9</td>\n");
	fprintf(out, "    <td class=\"rank\">10</td>\n");
	fprintf(out, "    <td class=\"rank\">11</td>\n");
	fprintf(out, "    <td class=\"rank\">12</td>\n");
	fprintf(out, "    <td class=\"rank\">13</td>\n");
	fprintf(out, "    <td class=\"rank\">14</td>\n");
	fprintf(out, "    <td class=\"rank\">15</td>\n");
	fprintf(out, "    <td class=\"rank\">16</td>\n");
	fprintf(out, "    <td class=\"rank\">17</td>\n");
	fprintf(out, "    <td class=\"rank\">18</td>\n");
	fprintf(out, "    <td class=\"rank\">19</td>\n");
	fprintf(out, "    <td class=\"rank\">20</td>\n");
	fprintf(out, "    <td class=\"rank\">21</td>\n");
	fprintf(out, "    <td class=\"rank\">22</td>\n");
	fprintf(out, "    <td class=\"rank\">23</td>\n");
	fprintf(out, "   </tr>\n");
	fprintf(out, "  </table>\n");
	fprintf(out, "\n");
	fprintf(out, "  <table  class=\"ihourbars\" align=\"center\">\n");
	fprintf(out, "   <tr>\n");
	fprintf(out, "    <td class=\"asmall\"><img src=\"./blue-h.png\" width=\"40\" height=\"15\" alt=\"0-5\" /> = 0-5</td>\n");
	fprintf(out, "    <td class=\"asmall\"><img src=\"./green-h.png\" width=\"40\" height=\"15\" alt=\"6-11\" /> = 6-11</td>\n");
	fprintf(out, "    <td class=\"asmall\"><img src=\"./yellow-h.png\" width=\"40\" height=\"15\" alt=\"12-17\" /> = 12-17</td>\n");
	fprintf(out, "    <td class=\"asmall\"><img src=\"./red-h.png\" width=\"40\" height=\"15\" alt=\"18-23\" /> = 18-23</td>\n");
	fprintf(out, "   </tr>\n");
	fprintf(out, "  </table>\n");
	fprintf(out, " </div>\n");
	fprintf(out, "<!-- ============================================= -->\n");
	fprintf(out, " <div class=\"itoptalkers\">\n");
	fprintf(out, "  <h2>Most active users</h2>\n");
	fprintf(out, "  <table  class=\"itoptalkers\" align=\"center\">\n");
	fprintf(out, "   <tr>\n");
	fprintf(out, "    <th width=\"2%%\" class=\"nrank\">#</th>\n");
	fprintf(out, "    <th width=\"2%%\" class=\"nhappiness\">?</th>\n");
	fprintf(out, "    <th width=\"10%%\" class=\"nhandle\">Nickname</th>\n");
	fprintf(out, "    <th width=\"6%%\" class=\"npublics\">Lines</th>\n");
	fprintf(out, "    <th width=\"15%\" class=\"nactivity\">Activity</th>\n");
	fprintf(out, "    <th width=\"5%%\" class=\"nwords\">Words</th>\n");
	fprintf(out, "    <th width=\"2%%\" class=\"nwpp\">W/P</th>\n");
	fprintf(out, "    <th width=\"2%%\" class=\"ncpw\">C/W</th>\n");
	fprintf(out, "    <th width=\"50%%\" class=\"ncomment\">Comment</th>\n");
	fprintf(out, "   </tr>\n");


	for(i = 1; i <= 15 && nicks; nicks = nicks->next, i++) {
		n = (struct nick *)nicks->data;
		
		fprintf(out, "   <tr>\n");
		fprintf(out, "    <td class=\"nrank\">%d</td>\n", i);
		fprintf(out, "    <td class=\"nhappiness\"><img src=\"./happy1.gif\" alt=\"1\" /></td>\n");
		fprintf(out, "    <td class=\"nhandle\">%s</td>\n", n->name);
		fprintf(out, "    <td class=\"npublics\">%ld</td>\n", nick_get_property(n, "lines"));
		fprintf(out, "    <td class=\"nactivity\"><img src=\"./blue-h.png\" width=\"15\" height=\"15\" alt=\"\" /><img src=\"./yellow-h.png\" width=\"18\" height=\"15\" alt=\"\" /><img src=\"./red-h.png\" width=\"31\" height=\"15\" alt=\"\" /></td>\n");
		fprintf(out, "    <td class=\"nwords\">%ld</td>\n", nick_get_property(n, "word"));
		fprintf(out, "    <td class=\"nwpp\">%f</td>\n", PERCENTAGE(n, "happy"));
		fprintf(out, "    <td class=\"ncpw\">%f</td>\n", PERCENTAGE(n, "unhappy"));
		fprintf(out, "    <td class=\"ncomment\">FIXME</td>\n");
		fprintf(out, "   </tr>\n");
	}
	fprintf(out, "  </table>\n");

	fprintf(out, "<!-- ============================================= -->\n");
	fprintf(out, " <h3>These didn't make it to the top:</h3>\n");
	fprintf(out, " <table  class=\"ialmosttop\">\n");

	for(i = 0; i < 5 && nicks; i++) {
		fprintf(out, "  <tr>\n");
		for(j = 0; j < 4 && nicks; j++) {
			n = (struct nick *)nicks->data;
			nicks = nicks->next;
			fprintf(out, "   <td>%s (%ld)</td>\n", n->name, nick_get_property(n, "lines"));
		}
		fprintf(out, "  </tr>\n");
	}
	
	fprintf(out, " </table>\n");

	for(i = 0; nicks; i++) nicks = nicks->next;
	g_list_free(nicks);

	fprintf(out, " <h3>There were also <b>%ld</b> other nicks</h3>\n", i);
	fprintf(out, " </div>\n");
	fprintf(out, "<!-- ============================================= -->\n");
	fprintf(out, "<div class=\"ibignumbers\">\n");
	fprintf(out, " <h2>Big Numbers</h2>\n");

	n = get_highest_nick_for_property(channelstruct, "question");
	if(n) {
		fprintf(out, " <p class=\"isection\">\n");
		fprintf(out, "  Is <b>%s</b> stupid or just asking too many questions? <b>%f%%</b> of his lines contained a question!\n", n->name, PERCENTAGE(n, "question"));
		fprintf(out, " </p>\n");
	}
	
	n = get_highest_nick_for_property(channelstruct, "loud");
	if(n) {
		fprintf(out, " <p class=\"isection\">\n");
		fprintf(out, "  The loudest one was <b>%s</b>, who yelled <b>%f%%</b> of the time. </p>\n", n->name, PERCENTAGE(n, "loud"));
	}
	
	n = get_highest_nick_for_property(channelstruct, "url");
	if(n) {
		fprintf(out, " <p class=\"isection\">\n");
		fprintf(out, "  Total of <b>%ld</b> URLs were pasted by <b>%s</b>!!\n", nick_get_property(n, "url"), n->name);
		fprintf(out, " </p>\n");
	}

	n = get_highest_nick_for_property(channelstruct, "joins");
	if(n) {
		fprintf(out, " <p class=\"isection\">\n");
		fprintf(out, "  <b>%s</b> didn't know whether to stay. He/she joined the channel <b>%ld</b> times!\n", n->name, nick_get_property(n, "joins"));
		fprintf(out, " </p>\n");
	}

	n = get_highest_nick_for_property(channelstruct, "dokick");
	if(n) {
		fprintf(out, " <p class=\"isection\">\n");
		fprintf(out, "  <b>%s</b> kicked the ass most, <b>%ld</b> times to be exact!\n", n->name, nick_get_property(n, "dokick"));
		fprintf(out, " </p>\n");
	}

	n = get_highest_nick_for_property(channelstruct, "getkick");
	if(n) {
		fprintf(out, " <p class=\"isection\">\n");
		fprintf(out, "  Obviously someone does not like <b>%s</b>, he/she was kicked <b>%ld</b> times!\n", n->name, nick_get_property(n, "getkick"));
		fprintf(out, " </p>\n");
	}

	n = get_highest_nick_for_property(channelstruct, "caps");
	if(n) {
		fprintf(out, " <p class=\"isection\">\n");
		fprintf(out, "  <b>%s</b> is a clear caps-abuser, <b>%f%%</b> of time he/she wrote in CAPS. </p>\n", n->name, PERCENTAGE(n, "caps"));
	}

	n = get_highest_nick_for_property(channelstruct, "happy");
	if(n) {
		fprintf(out, " <p class=\"isection\">\n");
		fprintf(out, "  <b>%s</b> is either using drugs or is otherwise very <b>happy</b> person <b>;D</b> </p>\n", n->name);
	}
	
	n = get_highest_nick_for_property(channelstruct, "unhappy");
	if(n) {
		fprintf(out, " <p class=\"isection\">\n");
		fprintf(out, "  On the other hand <b>%s</b> seems to be quite <b>sad</b> <b>:(</b> </p>\n", n->name);
	}

	fprintf(out, "</div>\n");

	fprintf(out, "<!-- ============================================= -->\n");
	fprintf(out, " <div class=\"irestinfo\">\n");
	fprintf(out, "  <p class=\"iauthor\">Statistics generated by CISG (part of <a href=\"http://ctrlproxy.vernstok.nl/\">ctrlproxy</a> by Jelmer Vernooij). HTML output based on the HTML output from <a href=\"http://www.tnsp.org/fisg.php\">FISG (Fast IRC Stats Generator) v0.3.8</a> by ccr/TNSP (C) Copyright 2003 TNSP</p>\n");
	fprintf(out, "  <p class=\"itime\">Stats generated in <b>%ld seconds</b>.</p>\n", time(NULL) - start_time);
	fprintf(out, " </div>\n");
	print_html_footer(out);
}

void print_plain_chan(FILE *out, struct channel *c)
{
	/* FIXME */
}

void print_plain_networklist(FILE *out) 
{
	GList *l = get_networks();
	while(l) {
		struct network *n = (struct network *)l->data;
		fprintf(out, "%s\n", n->name);
		l = l->next;
	}
}

char *urlencode(char *a)
{
	char *ret;
	if(a[0] == '#') {
		asprintf(&ret, "%%23%s", a+1);
	} else { 
		ret = strdup(a);
	}
	return ret;
}

void print_html_networklist(FILE *out, char *linkformat)
{
	GList *l = get_networks();
	print_html_header(out, "Networks");
	fprintf(out, "<ul>");
	while(l) {
		struct network *n = (struct network *)l->data;
		if(linkformat) fprintf(out, "<li><a href=\"%s%s\">%s</a>", linkformat, n->name, n->name);
		else fprintf(out, "<li>%s", n->name);
		l = l->next;
	}
	fprintf(out, "</ul>");
	print_html_footer(out);
}

void print_html_channellist(FILE *out, struct network *n, char *linkformat) 
{
	GList *l = get_channels(n);
	char *title;
	asprintf(&title, "Channels on %s", n->name);
	print_html_header(out, title);
	free(title);
	fprintf(out, "<ul>");

	while(l) { 
		struct channel *c = (struct channel *)l->data;
		if(linkformat) {
			char *encchan = urlencode(c->name);
			fprintf(out, "<li><a href=\"%s%s/%s\">%s</a>", linkformat, n->name, encchan, c->name);
			free(encchan);
		} else fprintf(out, "<li>%s", c->name);
		l = l->next;
	}

	fprintf(out, "</ul>");
	print_html_footer(out);
}

void print_plain_channellist(FILE *out, struct network *network)
{
	GList *l = get_channels(network);
	while(l) {
		struct channel *c = (struct channel *)l->data;
		fprintf(out, "%s\n", c->name);
		l = l->next;
	}
}

int main(int argc, const char **argv)
{
	const char *statsfile, *fullchan;
	char *network = NULL, *channel = NULL;
	struct network *networkstruct;
	struct channel *channelstruct;
	char *linkformat = NULL;
	int html = 0, all = 0, tofile = 0;
	FILE *out = stdout;
	int arg;
	struct poptOption options[] = {
		POPT_AUTOHELP
		{"all", 'a', POPT_ARG_NONE, &all, 0, "Process all channels/networks" },
		{"tofile", 'f', POPT_ARG_NONE, &tofile, 0, "Write output to arg.html"},
		{"html", 'h', POPT_ARG_NONE, &html, 0, "HTML output" },
		{"linkprepend", 'l', POPT_ARG_STRING, &linkformat, 0, "String to put before each link in HTML files" },
		POPT_TABLEEND
	};
	poptContext pc;

	start_time = time(NULL);
	
	pc = poptGetContext(argv[0], argc, argv, options, 0);

	poptSetOtherOptionHelp(pc, "stats-file [network[/channel]] ...");
	
	while((arg = poptGetNextOpt(pc)) >= 0) {
		
	}

	stats_init();

	statsfile = poptGetArg(pc);
	if(!statsfile) {
		poptPrintHelp(pc, stderr, 0);
		return 1;
	}

	stats_parse_file(statsfile);

	if(!poptPeekArg(pc)) { 
		if(html) print_html_networklist(out, linkformat);
		else print_plain_networklist(out);
		return 0;
	}

	while(fullchan = poptGetArg(pc)) {
		if(tofile) {
			char *fname;
			fclose(out);
			asprintf(&fname, "%s.html", fullchan);
			out = fopen(fname, "w+");
			free(fname);
		}
		network = strdup(fullchan);

		channel = strchr(network, '/');
		if(!channel) {
			networkstruct = get_network(network);
			if(!networkstruct) {
				fprintf(stderr, "No such network '%s'\n", network);
				continue;
			}
			if(html) print_html_channellist(out, networkstruct, linkformat);
			else print_plain_channellist(out, networkstruct);
			continue;
		}

		*channel = '\0'; channel++;
		networkstruct = get_network(network);
		if(!networkstruct) {
			fprintf(stderr, "No such network '%s'\n", network);
			continue;
		}

		channelstruct = get_channel(networkstruct, channel);
		if(!channelstruct) {
			fprintf(stderr, "No such channel '%s' on network '%s'\n", channel, network);
			continue;
		}			
		if(html) print_html_chan(out, channelstruct);
		else print_plain_chan(out, channelstruct);
	}

	stats_fini();
	poptFreeContext(pc);
							
	return 0;
}
