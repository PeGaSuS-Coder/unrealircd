/*
 *   IRC - Internet Relay Chat, src/modules/squit.c
 *   (C) 2004 The UnrealIRCd Team
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

CMD_FUNC(cmd_squit);

#define MSG_SQUIT 	"SQUIT"	

ModuleHeader MOD_HEADER
  = {
	"squit",
	"5.0",
	"command /squit", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SQUIT, cmd_squit, 2, CMD_USER|CMD_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/*
** cmd_squit
**	parv[1] = server name
**	parv[parc-1] = comment
*/
CMD_FUNC(cmd_squit)
{
	char *server;
	Client *target;
	char *comment = (parc > 2 && parv[parc - 1]) ? parv[parc - 1] : client->name;

	// FIXME: this function is way too confusing, and full of old shit?

	if (!ValidatePermissionsForPath("route:local",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "SQUIT");
		return;
	}

	server = parv[1];

	target = find_server_quick(server);
	if (target && IsMe(target))
	{
		target = client->direction;
		server = client->direction->local->sockhost;
	}

	/*
	   ** SQUIT semantics is tricky, be careful...
	   **
	   ** The old (irc2.2PL1 and earlier) code just cleans away the
	   ** server client from the links (because it is never true
	   ** "client->direction == target".
	   **
	   ** This logic here works the same way until "SQUIT host" hits
	   ** the server having the target "host" as local link. Then it
	   ** will do a real cleanup spewing SQUIT's and QUIT's to all
	   ** directions, also to the link from which the orinal SQUIT
	   ** came, generating one unnecessary "SQUIT host" back to that
	   ** link.
	   **
	   ** One may think that this could be implemented like
	   ** "hunt_server" (e.g. just pass on "SQUIT" without doing
	   ** nothing until the server having the link as local is
	   ** reached). Unfortunately this wouldn't work in the real life,
	   ** because either target may be unreachable or may not comply
	   ** with the request. In either case it would leave target in
	   ** links--no command to clear it away. So, it's better just
	   ** clean out while going forward, just to be sure.
	   **
	   ** ...of course, even better cleanout would be to QUIT/SQUIT
	   ** dependant users/servers already on the way out, but
	   ** currently there is not enough information about remote
	   ** clients to do this...   --msa
	 */
	if (!target)
	{
		sendnumeric(client, ERR_NOSUCHSERVER, server);
		return;
	}
	if (MyUser(client) && ((!ValidatePermissionsForPath("route:global",client,NULL,NULL,NULL) && !MyConnect(target)) ||
	    (!ValidatePermissionsForPath("route:local",client,NULL,NULL,NULL) && MyConnect(target))))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}
	/*
	   **  Notify all opers, if my local link is remotely squitted
	 */
	if (MyConnect(target) && !MyUser(client))
	{
		sendto_umode_global(UMODE_OPER, "Received SQUIT %s from %s (%s)",
		    target->name, get_client_name(client, FALSE), comment);
	}
	else if (MyConnect(target))
	{
		if (target->user)
		{
			sendnotice(client, "ERROR: You're connected to %s, we cannot SQUIT ourselves",
			           me.name);
			return;
		}
		sendto_umode_global(UMODE_OPER, "Received SQUIT %s from %s (%s)",
		    target->name, get_client_name(client, FALSE), comment);
	}
	if (IsOper(client))
	{
		/*
		 * It was manually /squit'ed by a human being(we hope),
		 * there is a very good chance they don't want us to
		 * reconnect right away.  -Cabal95
		 */
		SetSQuit(target);
	}

	exit_client(target, recv_mtags, comment);
}
