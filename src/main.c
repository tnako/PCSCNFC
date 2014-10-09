#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include "pcsclite.h"
#include "winscard.h"


#define SV_SOCK_PATH "/tmp/nfcIn"

#define TYPE_NEW  0x17
#define TYPE_DONE 0xFA


#define CHECK(f, rv) \
    if (SCARD_S_SUCCESS != rv) \
{ \
    printf(f ": %s\n", pcsc_stringify_error(rv)); \
    sleep(1); \
    continue; \
    }



int sfd = -1;

static int create_socket()
{
    if (sfd == -1) {
        sfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if(sfd < 0) {
            printf("socket error\n");
            sfd = -1;
            return -1;
        }
    } else {
        close(sfd);
        sfd = -1;
        return -2;
    }

    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SV_SOCK_PATH, sizeof(addr.sun_path)-1);

    if (connect(sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1){
        printf("connection unsuccessfully: %s\n", strerror(errno));
        close(sfd);
        sfd = 0;
        return -3;
    }

    return 1;
}

int main(void)
{

    SCARDCONTEXT hContext = 0;
    LPTSTR mszReaders;
    SCARDHANDLE hCard;
    SCARD_READERSTATE rgReaderStates[1];
    SCARD_IO_REQUEST pioSendPci;

    unsigned long dwReaders, dwActiveProtocol, dwRecvLength;
    long rv;
    ssize_t len_write;

    unsigned char pbRecvBuffer[258];
    unsigned char socketBuffer[256];
    unsigned char socketBufferLength;
    const char cmd1[] = { 0xFF, 0xCA, 0x00, 0x00, 0x00 };

    signal(SIGPIPE, SIG_IGN);

    while (1) {

        if (create_socket() != 1) {
            sleep(1);
            continue;
        }

        if (hContext == 0) {
            rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
            CHECK("SCardEstablishContext", rv);
        }

        dwReaders = SCARD_AUTOALLOCATE;

        rv = SCardListReaders(hContext, NULL, (LPTSTR)&mszReaders, &dwReaders);
        CHECK("SCardListReaders1", rv);

        rgReaderStates[0].szReader = &mszReaders[0];
        rgReaderStates[0].dwCurrentState = SCARD_STATE_EMPTY;

        printf("Waiting for card insertion -> ");
        fflush(stdout);
        rv = SCardGetStatusChange(hContext, INFINITE, rgReaderStates, 1);
        CHECK("SCardGetStatusChange", rv);

        rv = SCardConnect(hContext, mszReaders, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);
        CHECK("SCardConnect", rv);


        switch(dwActiveProtocol)
        {
        case SCARD_PROTOCOL_T0:
            pioSendPci = *SCARD_PCI_T0;
            break;

        case SCARD_PROTOCOL_T1:
            pioSendPci = *SCARD_PCI_T1;
            break;
        }

        dwRecvLength = sizeof(pbRecvBuffer);
        rv = SCardTransmit(hCard, &pioSendPci, (LPCBYTE)cmd1, sizeof(cmd1), NULL, (LPBYTE)pbRecvBuffer, &dwRecvLength);
        CHECK("SCardTransmit", rv);

	socketBufferLength = dwRecvLength;

        if (dwRecvLength > 2) {
            dwRecvLength -= 2;

            printf("UID: ");
            for (unsigned int i = 0; i < dwRecvLength; i++) {
                printf("%02X ", pbRecvBuffer[i]);
            }
            printf("-> ");

            socketBuffer[0] = socketBufferLength-1;
            socketBuffer[1] = TYPE_NEW;
            memcpy(socketBuffer + 2, pbRecvBuffer, socketBufferLength - 2);

            len_write = write(sfd, socketBuffer, socketBufferLength);
            if (len_write != socketBufferLength) {
                printf("socet write TYPE_NEW error: %s\n", strerror(errno));
                sleep(1);
                continue;
            }
        } else {
            printf("no response\n");
        }

        fflush(stdout);

        rgReaderStates[0].dwCurrentState = SCARD_STATE_PRESENT;

        printf("Waiting for card remove -> ");
        fflush(stdout);
        rv = SCardGetStatusChange(hContext, INFINITE, rgReaderStates, 1);
        CHECK("SCardGetStatusChange", rv);

        printf("OK!\n");

	if (socketBufferLength > 2) {
		socketBuffer[1] = TYPE_DONE;
		len_write = write(sfd, socketBuffer, socketBufferLength);
		if (len_write != socketBufferLength) {
		    printf("socet write TYPE_DONE error: %s\n", strerror(errno));
		    sleep(1);
		    continue;

		}
	}

        rv = SCardDisconnect(hCard, SCARD_LEAVE_CARD);
        CHECK("SCardDisconnect", rv);

        rv = SCardFreeMemory(hContext, mszReaders);
        CHECK("SCardFreeMemory", rv);

        rv = SCardReleaseContext(hContext);
        CHECK("SCardReleaseContext", rv);

        hContext = 0;

	if (sfd >= 0) {
		close(sfd);
		sfd = -1;
	}
    }

    return 0;
}
