/* errwarn.c

   Errors and warnings... */

/*
 * Copyright (c) 1995 RadioMail Corporation.
 * Copyright (c) 1996-2000 Internet Software Consortium.
 *
 * Use is subject to license terms which appear in the file named
 * ISC-LICENSE that should have accompanied this file when you
 * received it.   If a file named ISC-LICENSE did not accompany this
 * file, or you are not sure the one you have is correct, you may
 * obtain an applicable copy of the license at:
 *
 *             http://www.isc.org/isc-license-1.0.html. 
 *
 * This file is part of the ISC DHCP distribution.   The documentation
 * associated with this file is listed in the file DOCUMENTATION,
 * included in the top-level directory of this release.
 *
 * Support and other services are available for ISC products - see
 * http://www.isc.org for more information.
 *
 * This software was written for RadioMail Corporation by Ted Lemon
 * under a contract with Vixie Enterprises.   Further modifications have
 * been made for the Internet Software Consortium under a contract
 * with Vixie Laboratories.
 */

#ifndef lint
static char copyright[] =
"$Id: errwarn.c,v 1.3 2000/02/01 03:19:52 mellon Exp $ Copyright (c) 1996 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include <omapip/omapip_p.h>
#include <errno.h>

#ifdef DEBUG
int log_perror = -1;
#else
int log_perror = 1;
#endif
int log_priority;
void (*log_cleanup) (void);

static char mbuf [1024];
static char fbuf [1024];

/* Log an error message, then exit... */

void log_fatal (const char * fmt, ... )
{
  va_list list;

  do_percentm (fbuf, fmt);

  va_start (list, fmt);
  vsnprintf (mbuf, sizeof mbuf, fbuf, list);
  va_end (list);

#ifndef DEBUG
  syslog (log_priority | LOG_ERR, mbuf);
#endif

  /* Also log it to stderr? */
  if (log_perror) {
	  write (2, mbuf, strlen (mbuf));
	  write (2, "\n", 1);
  }

  syslog (LOG_CRIT, "exiting.");
  if (log_perror) {
	fprintf (stderr, "exiting.\n");
	fflush (stderr);
  }
  if (log_cleanup)
	  (*log_cleanup) ();
  exit (1);
}

/* Log an error message... */

int log_error (const char * fmt, ...)
{
  va_list list;

  do_percentm (fbuf, fmt);

  va_start (list, fmt);
  vsnprintf (mbuf, sizeof mbuf, fbuf, list);
  va_end (list);

#ifndef DEBUG
  syslog (log_priority | LOG_ERR, mbuf);
#endif

  if (log_perror) {
	  write (2, mbuf, strlen (mbuf));
	  write (2, "\n", 1);
  }

  return 0;
}

/* Log a note... */

int log_info (const char *fmt, ...)
{
  va_list list;

  do_percentm (fbuf, fmt);

  va_start (list, fmt);
  vsnprintf (mbuf, sizeof mbuf, fbuf, list);
  va_end (list);

#ifndef DEBUG
  syslog (log_priority | LOG_INFO, mbuf);
#endif

  if (log_perror) {
	  write (2, mbuf, strlen (mbuf));
	  write (2, "\n", 1);
  }

  return 0;
}

/* Log a debug message... */

int log_debug (const char *fmt, ...)
{
  va_list list;

  do_percentm (fbuf, fmt);

  va_start (list, fmt);
  vsnprintf (mbuf, sizeof mbuf, fbuf, list);
  va_end (list);

#ifndef DEBUG
  syslog (log_priority | LOG_DEBUG, mbuf);
#endif

  if (log_perror) {
	  write (2, mbuf, strlen (mbuf));
	  write (2, "\n", 1);
  }

  return 0;
}

/* Find %m in the input string and substitute an error message string. */

void do_percentm (obuf, ibuf)
     char *obuf;
     const char *ibuf;
{
	const char *s = ibuf;
	char *p = obuf;
	int infmt = 0;
	const char *m;

	while (*s)
	{
		if (infmt)
		{
			if (*s == 'm')
			{
#ifndef __CYGWIN32__
				m = strerror (errno);
#else
				m = pWSAError ();
#endif
				if (!m)
					m = "<unknown error>";
				strcpy (p - 1, m);
				p += strlen (p);
				++s;
			}
			else
				*p++ = *s++;
			infmt = 0;
		}
		else
		{
			if (*s == '%')
				infmt = 1;
			*p++ = *s++;
		}
	}
	*p = 0;
}

#ifdef NO_STRERROR
char *strerror (err)
	int err;
{
	extern char *sys_errlist [];
	extern int sys_nerr;
	static char errbuf [128];

	if (err < 0 || err >= sys_nerr) {
		sprintf (errbuf, "Error %d", err);
		return errbuf;
	}
	return sys_errlist [err];
}
#endif /* NO_STRERROR */

#ifdef _WIN32
char *pWSAError ()
{
  int err = WSAGetLastError ();

  switch (err)
    {
    case WSAEACCES:
      return "Permission denied";
    case WSAEADDRINUSE:
      return "Address already in use";
    case WSAEADDRNOTAVAIL:
      return "Cannot assign requested address";
    case WSAEAFNOSUPPORT:
      return "Address family not supported by protocol family";
    case WSAEALREADY:
      return "Operation already in progress";
    case WSAECONNABORTED:
      return "Software caused connection abort";
    case WSAECONNREFUSED:
      return "Connection refused";
    case WSAECONNRESET:
      return "Connection reset by peer";
    case WSAEDESTADDRREQ:
      return "Destination address required";
    case WSAEFAULT:
      return "Bad address";
    case WSAEHOSTDOWN:
      return "Host is down";
    case WSAEHOSTUNREACH:
      return "No route to host";
    case WSAEINPROGRESS:
      return "Operation now in progress";
    case WSAEINTR:
      return "Interrupted function call";
    case WSAEINVAL:
      return "Invalid argument";
    case WSAEISCONN:
      return "Socket is already connected";
    case WSAEMFILE:
      return "Too many open files";
    case WSAEMSGSIZE:
      return "Message too long";
    case WSAENETDOWN:
      return "Network is down";
    case WSAENETRESET:
      return "Network dropped connection on reset";
    case WSAENETUNREACH:
      return "Network is unreachable";
    case WSAENOBUFS:
      return "No buffer space available";
    case WSAENOPROTOOPT:
      return "Bad protocol option";
    case WSAENOTCONN:
      return "Socket is not connected";
    case WSAENOTSOCK:
      return "Socket operation on non-socket";
    case WSAEOPNOTSUPP:
      return "Operation not supported";
    case WSAEPFNOSUPPORT:
      return "Protocol family not supported";
    case WSAEPROCLIM:
      return "Too many processes";
    case WSAEPROTONOSUPPORT:
      return "Protocol not supported";
    case WSAEPROTOTYPE:
      return "Protocol wrong type for socket";
    case WSAESHUTDOWN:
      return "Cannot send after socket shutdown";
    case WSAESOCKTNOSUPPORT:
      return "Socket type not supported";
    case WSAETIMEDOUT:
      return "Connection timed out";
    case WSAEWOULDBLOCK:
      return "Resource temporarily unavailable";
    case WSAHOST_NOT_FOUND:
      return "Host not found";
#if 0
    case WSA_INVALID_HANDLE:
      return "Specified event object handle is invalid";
    case WSA_INVALID_PARAMETER:
      return "One or more parameters are invalid";
    case WSAINVALIDPROCTABLE:
      return "Invalid procedure table from service provider";
    case WSAINVALIDPROVIDER:
      return "Invalid service provider version number";
    case WSA_IO_PENDING:
      return "Overlapped operations will complete later";
    case WSA_IO_INCOMPLETE:
      return "Overlapped I/O event object not in signaled state";
    case WSA_NOT_ENOUGH_MEMORY:
      return "Insufficient memory available";
#endif
    case WSANOTINITIALISED:
      return "Successful WSAStartup not yet performer";
    case WSANO_DATA:
      return "Valid name, no data record of requested type";
    case WSANO_RECOVERY:
      return "This is a non-recoverable error";
#if 0
    case WSAPROVIDERFAILEDINIT:
      return "Unable to initialize a service provider";
    case WSASYSCALLFAILURE:
      return "System call failure";
#endif
    case WSASYSNOTREADY:
      return "Network subsystem is unavailable";
    case WSATRY_AGAIN:
      return "Non-authoritative host not found";
    case WSAVERNOTSUPPORTED:
      return "WINSOCK.DLL version out of range";
    case WSAEDISCON:
      return "Graceful shutdown in progress";
#if 0
    case WSA_OPERATION_ABORTED:
      return "Overlapped operation aborted";
#endif
    }
  return "Unknown WinSock error";
}
#endif /* _WIN32 */