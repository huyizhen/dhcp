/* bootp.c

   BOOTP Protocol support. */

/*
 * Copyright (c) 1995, 1996, 1997, 1998 The Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: bootp.c,v 1.34 1998/11/09 02:45:59 mellon Exp $ Copyright (c) 1995, 1996 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"

void bootp (packet)
	struct packet *packet;
{
	int result;
	struct host_decl *hp;
	struct host_decl *host = (struct host_decl *)0;
	struct packet outgoing;
	struct dhcp_packet raw;
	struct sockaddr_in to;
	struct in_addr from;
	struct hardware hto;
	struct option_state options;
	struct subnet *subnet;
	struct lease *lease;
	struct iaddr ip_address;
	int i;
	struct data_string d1;
	struct option_cache *oc;

	if (packet -> raw -> op != BOOTREQUEST)
		return;

	note ("BOOTREQUEST from %s via %s",
	      print_hw_addr (packet -> raw -> htype,
			     packet -> raw -> hlen,
			     packet -> raw -> chaddr),
	      packet -> raw -> giaddr.s_addr
	      ? inet_ntoa (packet -> raw -> giaddr)
	      : packet -> interface -> name);



	if (!locate_network (packet))
		return;

	hp = find_hosts_by_haddr (packet -> raw -> htype,
				  packet -> raw -> chaddr,
				  packet -> raw -> hlen);

	lease = find_lease (packet, packet -> shared_network, 0);

	/* Find an IP address in the host_decl that matches the
	   specified network. */
	if (hp)
		subnet = find_host_for_network (&hp, &ip_address,
						packet -> shared_network);
	else
		subnet = (struct subnet *)0;

	if (!subnet) {
		/* We didn't find an applicable host declaration.
		   Just in case we may be able to dynamically assign
		   an address, see if there's a host declaration
		   that doesn't have an ip address associated with it. */
		if (hp) {
			for (; hp; hp = hp -> n_ipaddr) {
				if (!hp -> fixed_addr) {
					host = hp;
					break;
				}
			}
		}

		/* If a lease has already been assigned to this client,
		   use it. */
		if (lease) {
			ack_lease (packet, lease, 0, 0);
			return;
		}

		/* Otherwise, try to allocate one. */
		lease = allocate_lease (packet,
					packet -> shared_network -> pools, 0);
		if (lease) {
			lease -> host = host;
			ack_lease (packet, lease, 0, 0);
			return;
		}
		note ("No available leases for BOOTP client %s",
		      print_hw_addr (packet -> raw -> htype,
				     packet -> raw -> hlen,
				     packet -> raw -> chaddr));
		return;
	}

	/* Run the executable statements to compute the client and server
	   options. */

	memset (&options, 0, sizeof options);
	
	/* Execute the subnet statements. */
	execute_statements_in_scope (packet, &options,
				     lease -> subnet -> group,
				     (struct group *)0);
	
	/* Execute the host statements. */
	execute_statements_in_scope (packet, &options, hp -> group,
				     lease -> subnet -> group);
	
	/* Drop the request if it's not allowed for this client. */
	if (evaluate_boolean_option_cache (packet, &options,
					   lookup_option (options.dhcp_hash,
							  SV_ALLOW_BOOTP))) {
		note ("Ignoring BOOTP client %s",
		      print_hw_addr (packet -> raw -> htype,
				     packet -> raw -> hlen,
				     packet -> raw -> chaddr));
		return;
	} 

	if (evaluate_boolean_option_cache (packet, &options,
					   lookup_option (options.dhcp_hash,
							  SV_ALLOW_BOOTP))) {
		note ("Declining to boot client %s",
		      lease -> host -> name
		      ? lease -> host -> name
		      : print_hw_addr (packet -> raw -> htype,
				       packet -> raw -> hlen,
				       packet -> raw -> chaddr));
		return;
	}

	/* Set up the outgoing packet... */
	memset (&outgoing, 0, sizeof outgoing);
	memset (&raw, 0, sizeof raw);
	outgoing.raw = &raw;

	/* If we didn't get a known vendor magic number on the way in,
	   just copy the input options to the output. */
	if (!packet -> options_valid) {
		memcpy (outgoing.raw -> options,
			packet -> raw -> options, DHCP_OPTION_LEN);
		outgoing.packet_length = BOOTP_MIN_LEN;
	} else {
		/* Pack the options into the buffer.  Unlike DHCP, we
		   can't pack options into the filename and server
		   name buffers. */

		outgoing.packet_length =
			cons_options (packet, outgoing.raw, 0,
				      &options, (struct agent_options *)0,
				      0, 0, 1);
		if (outgoing.packet_length < BOOTP_MIN_LEN)
			outgoing.packet_length = BOOTP_MIN_LEN;
	}

	/* Take the fields that we care about... */
	raw.op = BOOTREPLY;
	raw.htype = packet -> raw -> htype;
	raw.hlen = packet -> raw -> hlen;
	memcpy (raw.chaddr, packet -> raw -> chaddr, sizeof raw.chaddr);
	raw.hops = packet -> raw -> hops;
	raw.xid = packet -> raw -> xid;
	raw.secs = packet -> raw -> secs;
	raw.flags = 0;
	raw.ciaddr = packet -> raw -> ciaddr;
	memcpy (&raw.yiaddr, ip_address.iabuf, sizeof raw.yiaddr);

	/* Figure out the address of the next server. */
	raw.siaddr = lease -> shared_network -> interface -> primary_address;
	oc = lookup_option (options.dhcp_hash, SV_NEXT_SERVER);
	if (oc &&
	    evaluate_option_cache (&d1, packet, &options, oc)) {
		/* If there was more than one answer, take the first. */
		if (d1.len >= 4 && d1.data)
			memcpy (&raw.siaddr, d1.data, 4);
		data_string_forget (&d1, "bootrequest");
	}

	raw.giaddr = packet -> raw -> giaddr;

	/* Figure out the filename. */
	oc = lookup_option (options.dhcp_hash, SV_FILENAME);
	if (oc &&
	    evaluate_option_cache (&d1, packet, &options, oc)) {
		memcpy (raw.file, d1.data,
			d1.len > sizeof raw.file ? sizeof raw.file : d1.len);
		if (sizeof raw.file > d1.len)
			memset (&raw.file [d1.len],
				0, (sizeof raw.file) - d1.len);
		data_string_forget (&d1, "bootrequest");
	}

	/* Choose a server name as above. */
	oc = lookup_option (options.dhcp_hash, SV_SERVER_NAME);
	if (oc &&
	    evaluate_option_cache (&d1, packet, &options, oc)) {
		memcpy (raw.sname, d1.data,
			d1.len > sizeof raw.sname ? sizeof raw.sname : d1.len);
		if (sizeof raw.sname > d1.len)
			memset (&raw.sname [d1.len],
				0, (sizeof raw.sname) - d1.len);
		data_string_forget (&d1, "bootrequest");
	}

	/* Set up the hardware destination address... */
	hto.htype = packet -> raw -> htype;
	hto.hlen = packet -> raw -> hlen;
	memcpy (hto.haddr, packet -> raw -> chaddr, hto.hlen);

	from = packet -> interface -> primary_address;

	/* Report what we're doing... */
	note ("BOOTREPLY for %s to %s (%s) via %s",
	      piaddr (ip_address), hp -> name,
	      print_hw_addr (packet -> raw -> htype,
			     packet -> raw -> hlen,
			     packet -> raw -> chaddr),
	      packet -> raw -> giaddr.s_addr
	      ? inet_ntoa (packet -> raw -> giaddr)
	      : packet -> interface -> name);

	/* Set up the parts of the address that are in common. */
	to.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
	to.sin_len = sizeof to;
#endif
	memset (to.sin_zero, 0, sizeof to.sin_zero);

	/* If this was gatewayed, send it back to the gateway... */
	if (raw.giaddr.s_addr) {
		to.sin_addr = raw.giaddr;
		to.sin_port = local_port;

#ifdef USE_FALLBACK
		result = send_fallback (&fallback_interface,
					(struct packet *)0,
					&raw, outgoing.packet_length,
					from, &to, &hto);
		if (result < 0)
			warn ("send_fallback: %m");
		return;
#endif
	/* Otherwise, broadcast it on the local network. */
	} else {
		to.sin_addr.s_addr = INADDR_BROADCAST;
		to.sin_port = remote_port; /* XXX */
	}

	errno = 0;
	result = send_packet (packet -> interface,
			      packet, &raw, outgoing.packet_length,
			      from, &to, &hto);
	if (result < 0)
		warn ("send_packet: %m");
}
