#include "internals.h"

void main()
{
	struct server *s;
	s = connect_to_server("irc.internetnu.nl", 6667, "bla", "bla", NULL);
	while(1) loop(s);

}
