/*
* socket-io-windows.c: Windows specific socket code.
*
* Copyright 2016 Microsoft
* Licensed under the MIT license. See LICENSE file in the project root for full license information.
*/
#include <config.h>
#include <glib.h>

#include "mono/metadata/socket-io-windows-internals.h"

#define LOGDEBUG(...)  

static gboolean set_blocking (SOCKET sock, gboolean block)
{
	u_long non_block = block ? 0 : 1;
	return ioctlsocket (sock, FIONBIO, &non_block) != SOCKET_ERROR;
}

static DWORD get_socket_timeout (SOCKET sock, int optname)
{
	DWORD timeout = 0;
	int optlen = sizeof (DWORD);
	if (getsockopt (sock, SOL_SOCKET, optname, (char *)&timeout, &optlen) == SOCKET_ERROR) {
		WSASetLastError (0);
		return WSA_INFINITE;
	}
	if (timeout == 0)
		timeout = WSA_INFINITE; // 0 means infinite
	return timeout;
}

/*
* Performs an alertable wait for the specified event (FD_ACCEPT_BIT,
* FD_CONNECT_BIT, FD_READ_BIT, FD_WRITE_BIT) on the specified socket.
* Returns TRUE if the event is fired without errors. Calls WSASetLastError()
* with WSAEINTR and returns FALSE if the thread is alerted. If the event is
* fired but with an error WSASetLastError() is called to set the error and the
* function returns FALSE.
*/
static gboolean alertable_socket_wait (SOCKET sock, int event_bit)
{
	static char *EVENT_NAMES[] = { "FD_READ", "FD_WRITE", NULL /*FD_OOB*/, "FD_ACCEPT", "FD_CONNECT", "FD_CLOSE" };
	gboolean success = FALSE;
	int error = -1;
	DWORD timeout = WSA_INFINITE;
	if (event_bit == FD_READ_BIT || event_bit == FD_WRITE_BIT) {
		timeout = get_socket_timeout (sock, event_bit == FD_READ_BIT ? SO_RCVTIMEO : SO_SNDTIMEO);
	}
	WSASetLastError (0);
	WSAEVENT event = WSACreateEvent ();
	if (event != WSA_INVALID_EVENT) {
		if (WSAEventSelect (sock, event, (1 << event_bit) | FD_CLOSE) != SOCKET_ERROR) {
			LOGDEBUG (g_message ("%06d - Calling WSAWaitForMultipleEvents () on socket %d", GetCurrentThreadId (), sock));
			DWORD ret = WSAWaitForMultipleEvents (1, &event, TRUE, timeout, TRUE);
			if (ret == WSA_WAIT_IO_COMPLETION) {
				LOGDEBUG (g_message ("%06d - WSAWaitForMultipleEvents () returned WSA_WAIT_IO_COMPLETION for socket %d", GetCurrentThreadId (), sock));
				error = WSAEINTR;
			} else if (ret == WSA_WAIT_TIMEOUT) {
				error = WSAETIMEDOUT;
			} else {
				g_assert (ret == WSA_WAIT_EVENT_0);
				WSANETWORKEVENTS ne = { 0 };
				if (WSAEnumNetworkEvents (sock, event, &ne) != SOCKET_ERROR) {
					if (ne.lNetworkEvents & (1 << event_bit) && ne.iErrorCode[event_bit]) {
						LOGDEBUG (g_message ("%06d - %s error %d on socket %d", GetCurrentThreadId (), EVENT_NAMES[event_bit], ne.iErrorCode[event_bit], sock));
						error = ne.iErrorCode[event_bit];
					} else if (ne.lNetworkEvents & FD_CLOSE_BIT && ne.iErrorCode[FD_CLOSE_BIT]) {
						LOGDEBUG (g_message ("%06d - FD_CLOSE error %d on socket %d", GetCurrentThreadId (), ne.iErrorCode[FD_CLOSE_BIT], sock));
						error = ne.iErrorCode[FD_CLOSE_BIT];
					} else {
						LOGDEBUG (g_message ("%06d - WSAEnumNetworkEvents () finished successfully on socket %d", GetCurrentThreadId (), sock));
						success = TRUE;
						error = 0;
					}
				}
			}
			WSAEventSelect (sock, NULL, 0);
		}
		WSACloseEvent (event);
	}
	if (error != -1) {
		WSASetLastError (error);
	}
	return success;
}

#define ALERTABLE_SOCKET_CALL(event_bit, blocking, repeat, ret, op, sock, ...) \
	LOGDEBUG (g_message ("%06d - Performing %s " #op " () on socket %d", GetCurrentThreadId (), blocking ? "blocking" : "non-blocking", sock)); \
	if (blocking) { \
		if (set_blocking(sock, FALSE)) { \
			while (-1 == (int) (ret = op (sock, __VA_ARGS__))) { \
				int _error = WSAGetLastError ();\
				if (_error != WSAEWOULDBLOCK && _error != WSA_IO_PENDING) \
					break; \
				if (!alertable_socket_wait (sock, event_bit) || !repeat) \
					break; \
			} \
			int _saved_error = WSAGetLastError (); \
			set_blocking (sock, TRUE); \
			WSASetLastError (_saved_error); \
		} \
	} else { \
		ret = op (sock, __VA_ARGS__); \
	} \
	int _saved_error = WSAGetLastError (); \
	LOGDEBUG (g_message ("%06d - Finished %s " #op " () on socket %d (ret = %d, WSAGetLastError() = %d)", GetCurrentThreadId (), \
		blocking ? "blocking" : "non-blocking", sock, ret, _saved_error)); \
	WSASetLastError (_saved_error);

SOCKET alertable_accept (SOCKET s, struct sockaddr *addr, int *addrlen, gboolean blocking)
{
	SOCKET newsock = INVALID_SOCKET;
	ALERTABLE_SOCKET_CALL (FD_ACCEPT_BIT, blocking, TRUE, newsock, accept, s, addr, addrlen);
	return newsock;
}

int alertable_connect (SOCKET s, const struct sockaddr *name, int namelen, gboolean blocking)
{
	int ret = SOCKET_ERROR;
	ALERTABLE_SOCKET_CALL (FD_CONNECT_BIT, blocking, FALSE, ret, connect, s, name, namelen);
	ret = WSAGetLastError () != 0 ? SOCKET_ERROR : 0;
	return ret;
}

int alertable_recv (SOCKET s, char *buf, int len, int flags, gboolean blocking)
{
	int ret = SOCKET_ERROR;
	ALERTABLE_SOCKET_CALL (FD_READ_BIT, blocking, TRUE, ret, recv, s, buf, len, flags);
	return ret;
}

int alertable_recvfrom (SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen, gboolean blocking)
{
	int ret = SOCKET_ERROR;
	ALERTABLE_SOCKET_CALL (FD_READ_BIT, blocking, TRUE, ret, recvfrom, s, buf, len, flags, from, fromlen);
	return ret;
}

int alertable_WSARecv (SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine, gboolean blocking)
{
	int ret = SOCKET_ERROR;
	ALERTABLE_SOCKET_CALL (FD_READ_BIT, blocking, TRUE, ret, WSARecv, s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
	return ret;
}

int alertable_send (SOCKET s, char *buf, int len, int flags, gboolean blocking)
{
	int ret = SOCKET_ERROR;
	ALERTABLE_SOCKET_CALL (FD_WRITE_BIT, blocking, FALSE, ret, send, s, buf, len, flags);
	return ret;
}

int alertable_sendto (SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen, gboolean blocking)
{
	int ret = SOCKET_ERROR;
	ALERTABLE_SOCKET_CALL (FD_WRITE_BIT, blocking, FALSE, ret, sendto, s, buf, len, flags, to, tolen);
	return ret;
}

int alertable_WSASend (SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, DWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine, gboolean blocking)
{
	int ret = SOCKET_ERROR;
	ALERTABLE_SOCKET_CALL (FD_WRITE_BIT, blocking, FALSE, ret, WSASend, s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
	return ret;
}

BOOL alertable_TransmitFile (SOCKET hSocket, HANDLE hFile, DWORD nNumberOfBytesToWrite, DWORD nNumberOfBytesPerSend, LPOVERLAPPED lpOverlapped, LPTRANSMIT_FILE_BUFFERS lpTransmitBuffers, DWORD dwReserved, gboolean blocking)
{
	LOGDEBUG (g_message ("%06d - Performing %s TransmitFile () on socket %d", GetCurrentThreadId (), blocking ? "blocking" : "non-blocking", hSocket));

	int error = 0;
	if (blocking) {
		g_assert (lpOverlapped == NULL);
		OVERLAPPED overlapped = { 0 };
		overlapped.hEvent = WSACreateEvent ();
		if (overlapped.hEvent == WSA_INVALID_EVENT)
			return FALSE;
		if (!TransmitFile (hSocket, hFile, nNumberOfBytesToWrite, nNumberOfBytesPerSend, &overlapped, lpTransmitBuffers, dwReserved)) {
			error = WSAGetLastError ();
			if (error == WSA_IO_PENDING) {
				error = 0;
				// NOTE: .NET's Socket.SendFile() doesn't honor the Socket's SendTimeout so we shouldn't either
				DWORD ret = WaitForSingleObjectEx (overlapped.hEvent, INFINITE, TRUE);
				if (ret == WAIT_IO_COMPLETION) {
					LOGDEBUG (g_message ("%06d - WaitForSingleObjectEx () returned WSA_WAIT_IO_COMPLETION for socket %d", GetCurrentThreadId (), hSocket));
					error = WSAEINTR;
				} else if (ret == WAIT_TIMEOUT) {
					error = WSAETIMEDOUT;
				} else if (ret != WAIT_OBJECT_0) {
					error = GetLastError ();
				}
			}
		}
		WSACloseEvent (overlapped.hEvent);
	} else {
		if (!TransmitFile (hSocket, hFile, nNumberOfBytesToWrite, nNumberOfBytesPerSend, lpOverlapped, lpTransmitBuffers, dwReserved)) {
			error = WSAGetLastError ();
		}
	}

	LOGDEBUG (g_message ("%06d - Finished %s TransmitFile () on socket %d (ret = %d, WSAGetLastError() = %d)", GetCurrentThreadId (), \
		blocking ? "blocking" : "non-blocking", hSocket, error == 0, error));
	WSASetLastError (error);

	return error == 0;
}