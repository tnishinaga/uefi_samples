#include <efi.h>
#include <efilib.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


VOID
efi_panic(EFI_STATUS efi_status, INTN line)
{
    Print(L"panic at line:%d\n", line);
    Print(L"EFI_STATUS = %d\n", efi_status);
    while (1);
    // TODO: change while loop to BS->Exit
}

static inline VOID
MY_EFI_ASSERT(EFI_STATUS status, INTN line)
{
    if (EFI_ERROR(status)) {
        efi_panic(status, line);
    }
}

EFI_STATUS
find_tcp4_service_binding_handlers (
    EFI_HANDLE **handlers,
    UINTN *nohandlers
)
{
    EFI_STATUS efi_status;
    // load handler
    EFI_GUID tcp4_service_binding_protocol_guid = EFI_TCP4_SERVICE_BINDING_PROTOCOL;
    efi_status = uefi_call_wrapper(
        BS->LocateHandleBuffer,
        5,
        ByProtocol,
        &tcp4_service_binding_protocol_guid,
        NULL, /* SearchKey is ignored */
        (UINTN *)nohandlers,
        (EFI_HANDLE **)handlers
    );
    return efi_status;
}

EFI_STATUS
load_tcp4_service_binding_protocol(
    EFI_SERVICE_BINDING **protocol,
    EFI_HANDLE handlers
)
{
    EFI_STATUS efi_status;
    EFI_GUID tcp4_service_binding_protocol_guid = EFI_TCP4_SERVICE_BINDING_PROTOCOL;
    efi_status = uefi_call_wrapper(
        BS->HandleProtocol,
        3,
        handlers,
        &tcp4_service_binding_protocol_guid,
        (VOID **)protocol
    );
    return efi_status;
}

EFI_STATUS
open_tcp4_protocol(
    EFI_TCP4 **protocol,
    EFI_HANDLE handler
)
{
    EFI_STATUS efi_status;
    EFI_GUID tcp4_protocol_guid = EFI_TCP4_PROTOCOL;
    efi_status = uefi_call_wrapper(
        BS->HandleProtocol,
        3,
        handler,
        &tcp4_protocol_guid,
        (VOID **)protocol
    );
    return efi_status;
}


static inline EFI_STATUS
CreateTcp4NotifyEvent(
    EFI_EVENT *Event, 
    EFI_EVENT_NOTIFY NotifyFunc,
    VOID *Context
    )
{
    EFI_STATUS efi_status;

    efi_status = uefi_call_wrapper(
        BS->CreateEvent,
        5,
        EFI_EVENT_NOTIFY_SIGNAL,
        EFI_TPL_CALLBACK,
        (EFI_EVENT_NOTIFY)NotifyFunc,
        (VOID *)Context,
        Event
    );
    return efi_status;
}


#define BUFFER_SIZE 512

typedef struct {
    UINT8 bufffer[BUFFER_SIZE];
    UINTN pointer;
    UINTN size;
} MY_EFI_RING_BUFFER;

typedef struct {
    EFI_TCP4 *protocol;
    struct {
        EFI_TCP4_LISTEN_TOKEN *listen;
        EFI_TCP4_IO_TOKEN *io;
        EFI_TCP4_CLOSE_TOKEN *close;
    } token;
    MY_EFI_RING_BUFFER tx_buffer;
    MY_EFI_RING_BUFFER rx_buffer;
} MY_EFI_SOCKET;


// Notify func
VOID
AcceptNotify(
    EFI_EVENT Event,
    VOID *Context
    )
{
    EFI_STATUS efi_status;

    if (Context == NULL) {
        Print(L"Error: Contect is NULL\n");
        MY_EFI_ASSERT(EFI_ABORTED, __LINE__);
    }
    MY_EFI_SOCKET *sock = (MY_EFI_SOCKET *)Context;
    if (sock->token.listen == NULL) {
        Print(L"Error: listen token is NULL\n");
        MY_EFI_ASSERT(EFI_ABORTED, __LINE__);
    }
    EFI_TCP4_LISTEN_TOKEN *accept_token = (EFI_TCP4_LISTEN_TOKEN *)sock->token.listen;

    // close event
    efi_status = uefi_call_wrapper(
        BS->CloseEvent,
        1,
        Event
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    Print(L"Accept Notify\n");

    // bad status
    if (accept_token->CompletionToken.Status != EFI_SUCCESS) {
        // TODO: improve
        MY_EFI_ASSERT(accept_token->CompletionToken.Status, __LINE__);
    }

    // open new child handle
    EFI_TCP4 *tcp4_child = NULL;
    efi_status = open_tcp4_protocol(&tcp4_child, accept_token->NewChildHandle);
    MY_EFI_ASSERT(efi_status, __LINE__);
    // set protocol to socket
    sock->protocol = tcp4_child;

    // free token
    efi_status = uefi_call_wrapper(BS->FreePool, 1, accept_token);
    MY_EFI_ASSERT(efi_status, __LINE__);
    sock->token.listen = NULL;


    return;
}


MY_EFI_SOCKET *
tcp4_accept(
    EFI_TCP4 *tcp4_protocol
)
{
    EFI_STATUS efi_status;
    MY_EFI_SOCKET *sock;
    EFI_TCP4_LISTEN_TOKEN *accept_token;

    // alocate token
    efi_status = uefi_call_wrapper(BS->AllocatePool,
        3,
        EfiBootServicesData,
        sizeof(EFI_TCP4_LISTEN_TOKEN),
        (VOID **)&accept_token
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    // allocate socket
    efi_status = uefi_call_wrapper(BS->AllocatePool,
        3,
        EfiBootServicesData,
        sizeof(MY_EFI_SOCKET),
        (VOID **)&sock
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    efi_status = CreateTcp4NotifyEvent(
        &accept_token->CompletionToken.Event,
        (EFI_EVENT_NOTIFY)AcceptNotify,
        (VOID *)sock
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    sock->protocol = NULL;
    sock->token.listen = accept_token;
    sock->token.io = NULL;
    sock->token.close = NULL;

    efi_status = uefi_call_wrapper(
        tcp4_protocol->Accept,
        2,
        tcp4_protocol,
        sock->token.listen
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    return sock;
}


VOID
TransmitNotify(
    EFI_EVENT Event,
    VOID *Context
    )
{
    EFI_STATUS efi_status;
    MY_EFI_SOCKET *sock = (MY_EFI_SOCKET *)Context;
    EFI_TCP4_IO_TOKEN *iotoken = sock->token.io;

    efi_status = iotoken->CompletionToken.Status;
    if (efi_status != EFI_SUCCESS) {
        // if (efi_status == EFI_CONNECTION_FIN) {
            // fin
            // TODO: write
        // }
        // else {
            // error
        // }
        return;
    }

    Print(L"TransmitNotify\n");

    // Close Event
    efi_status = uefi_call_wrapper(
        BS->CloseEvent,
        1,
        Event
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    // free
    // free fragment buffer
    efi_status = uefi_call_wrapper(BS->FreePool, 1, iotoken->Packet.TxData->FragmentTable[0].FragmentBuffer);
    MY_EFI_ASSERT(efi_status, __LINE__);
    // free transmit data
    efi_status = uefi_call_wrapper(BS->FreePool, 1, iotoken->Packet.TxData);
    MY_EFI_ASSERT(efi_status, __LINE__);
    // free io token
    efi_status = uefi_call_wrapper(BS->FreePool, 1, iotoken);
    MY_EFI_ASSERT(efi_status, __LINE__);
    sock->token.io = NULL;

    return;
}


EFI_STATUS
tcp4_write(MY_EFI_SOCKET *sock, VOID *buf, UINTN size)
{
    EFI_STATUS efi_status;
    EFI_TCP4 *tcp4_protocol = sock->protocol;
    UINT8 *buffer = (UINT8 *)buf;
    
    EFI_TCP4_IO_TOKEN *iotoken;
    efi_status = uefi_call_wrapper(BS->AllocatePool,
        3,
        EfiBootServicesData,
        sizeof(EFI_TCP4_IO_TOKEN),
        (VOID **)&iotoken
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    // set token to sock
    sock->token.io = iotoken;

    // create event
    efi_status = CreateTcp4NotifyEvent(
        &iotoken->CompletionToken.Event, 
        (EFI_EVENT_NOTIFY)TransmitNotify, 
        (VOID *)sock
    );
    MY_EFI_ASSERT(efi_status, __LINE__);
    // create tx buffer
    UINT8 *txbuffer;
    efi_status = uefi_call_wrapper(BS->AllocatePool,
        3,
        EfiBootServicesData,
        sizeof(UINT8) * size,
        (VOID **)&txbuffer
    );
    MY_EFI_ASSERT(efi_status, __LINE__);
    // copy buffer to txbuffer
    for (UINTN i = 0; i < size; i++) {
        txbuffer[i] = buffer[i];
    }

    EFI_TCP4_TRANSMIT_DATA *txdata;
    efi_status = uefi_call_wrapper(BS->AllocatePool,
        3,
        EfiBootServicesData,
        sizeof(EFI_TCP4_TRANSMIT_DATA),
        (VOID **)&txdata
    );
    MY_EFI_ASSERT(efi_status, __LINE__);
    txdata->Push = FALSE;
    txdata->Urgent = FALSE;
    txdata->DataLength = sizeof(UINT8) * size;
    txdata->FragmentCount = 1;
    txdata->FragmentTable[0].FragmentLength = txdata->DataLength;
    txdata->FragmentTable[0].FragmentBuffer = txbuffer;
    

    // set TxData to token
    iotoken->Packet.TxData = txdata;

    efi_status = uefi_call_wrapper(
        tcp4_protocol->Transmit,
        2,
        tcp4_protocol,
        iotoken
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    return EFI_SUCCESS;
}

VOID
CloseNotify(
    EFI_EVENT Event,
    VOID *Context
    )
{
    EFI_STATUS efi_status;
    MY_EFI_SOCKET *sock = (MY_EFI_SOCKET *)Context;
    EFI_TCP4_CLOSE_TOKEN *close_token = sock->token.close;

    // Close Event
    efi_status = uefi_call_wrapper(
        BS->CloseEvent,
        1,
        Event
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    Print(L"CloseNotify\n");

    efi_status = uefi_call_wrapper(BS->FreePool, 1, close_token);
    MY_EFI_ASSERT(efi_status, __LINE__);
    sock->token.close = NULL;
    return;
}


EFI_STATUS
tcp4_close(MY_EFI_SOCKET *sock)
{
    EFI_STATUS efi_status;
    EFI_TCP4_CLOSE_TOKEN *close_token;
    EFI_TCP4 *tcp4_protocol = sock->protocol;

    // check TCP4 state
    EFI_TCP4_CONNECTION_STATE state;
    efi_status = uefi_call_wrapper(
        tcp4_protocol->GetModeData,
        6,
        tcp4_protocol,
        (EFI_TCP4_CONNECTION_STATE *)&state,
        (EFI_TCP4_CONFIG_DATA *)NULL,
        (EFI_IP4_MODE_DATA *)NULL, /* ignore Ip4ModeData */
        (EFI_MANAGED_NETWORK_CONFIG_DATA *)NULL, /* ignore MnpConfigData */
        (EFI_SIMPLE_NETWORK_MODE *)NULL  /* ignore SnpModeData */
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    if (state == Tcp4StateClosed) {
        // closed. nothing to do.
        Print(L"State is Tcp4StateClosed\n");
        return EFI_SUCCESS;
    }
        
    efi_status = uefi_call_wrapper(BS->AllocatePool,
        3,
        EfiBootServicesData,
        sizeof(EFI_TCP4_CLOSE_TOKEN),
        (VOID **)&close_token
    );
    MY_EFI_ASSERT(efi_status, __LINE__);
    sock->token.close = close_token;
    
    efi_status = CreateTcp4NotifyEvent(
        close_token->CompletionToken.Event, 
        (EFI_EVENT_NOTIFY)CloseNotify, 
        (VOID *)sock
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    efi_status = uefi_call_wrapper(
        tcp4_protocol->Close,
        2,
        tcp4_protocol,
        close_token
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    return EFI_SUCCESS;
}


EFI_STATUS
efi_main (EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab)
{
    EFI_STATUS efi_status;

    InitializeLib(image_handle, systab);

    // load handlers
    EFI_HANDLE *tcp4_service_binding_handlers = NULL;
    UINTN tcp4_service_binding_nohandlers;
    efi_status = find_tcp4_service_binding_handlers(
        &tcp4_service_binding_handlers, &tcp4_service_binding_nohandlers
    );
    MY_EFI_ASSERT(efi_status, __LINE__);
    if (tcp4_service_binding_nohandlers < 1 || tcp4_service_binding_handlers == NULL) {
        Print(L"Error: TCP4 handlers not found!\n");
        return EFI_ABORTED;
    }

    // load protocol from handler
    EFI_SERVICE_BINDING *tcp4_service_binding_protocol = NULL;
    efi_status = load_tcp4_service_binding_protocol(
        &tcp4_service_binding_protocol, tcp4_service_binding_handlers[0]
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    // Create Child
    EFI_HANDLE tcp4_handler;
    efi_status = uefi_call_wrapper(
        tcp4_service_binding_protocol->CreateChild,
        2,
        tcp4_service_binding_protocol,
        &tcp4_handler
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    EFI_TCP4 *tcp4_protocol = NULL;
    efi_status = open_tcp4_protocol(&tcp4_protocol, tcp4_handler);
    MY_EFI_ASSERT(efi_status, __LINE__);
    if (tcp4_protocol == NULL) {
        Print(L"Error: failed to load TCP4 protocol!\n");
        return EFI_ABORTED;
    }

    // configure
    EFI_TCP4_ACCESS_POINT ap = {
        .StationAddress.Addr = {192,168,20,20},
        .SubnetMask.Addr = {255,255,255,0},
        .StationPort = (UINT16)10000,
        .RemoteAddress.Addr = {192,168,20,1},
        .RemotePort = (UINT16)10010,
        .ActiveFlag = FALSE /* set FALSE when server */
    };
    EFI_TCP4_CONFIG_DATA conf = {
        .TypeOfService = 0, /* ToS */
        .TimeToLive = 100,
        .AccessPoint = ap,
        .ControlOption = NULL
    };
    efi_status = uefi_call_wrapper(
        tcp4_protocol->Configure,
        2,
        tcp4_protocol,
        &conf
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    // set route
    EFI_IPv4_ADDRESS subaddr = {.Addr = {192,168,20,0}};
    EFI_IPv4_ADDRESS submask = {.Addr = {255,255,255,0}};
    EFI_IPv4_ADDRESS gateway = {.Addr = {192,168,20,1}};
    efi_status = uefi_call_wrapper(
        tcp4_protocol->Routes,
        5,
        tcp4_protocol,
        FALSE,
        &subaddr,
        &submask,
        &gateway
    );
    MY_EFI_ASSERT(efi_status, __LINE__);


    Print(L"Waiting accept...\n");
    MY_EFI_SOCKET *sock = tcp4_accept(tcp4_protocol);
    while (sock->token.listen != NULL);
    Print(L"Accept done!\n");

    Print(L"Transmiting hello...\n");
    tcp4_write(sock, "hello", 5);
    while (sock->token.io != NULL);
    Print(L"Transmit done!\n");


    Print(L"Closing...\n");
    tcp4_close(sock);
    // check TCP4 state
    volatile EFI_TCP4_CONNECTION_STATE state;
    while (sock->token.close != NULL) {
        // check status
        efi_status = uefi_call_wrapper(
            sock->protocol->GetModeData,
            6,
            sock->protocol,
            (EFI_TCP4_CONNECTION_STATE *)&state,
            (EFI_TCP4_CONFIG_DATA *)NULL,
            (EFI_IP4_MODE_DATA *)NULL, /* ignore Ip4ModeData */
            (EFI_MANAGED_NETWORK_CONFIG_DATA *)NULL, /* ignore MnpConfigData */
            (EFI_SIMPLE_NETWORK_MODE *)NULL  /* ignore SnpModeData */
        );
        MY_EFI_ASSERT(efi_status, __LINE__);
        if (state == Tcp4StateClosed) {
            // socket closed
            // Force call CloseNotify
            Print(L"Send signal to Close Event\n");
            efi_status = uefi_call_wrapper(
                BS->SignalEvent,
                1,
                sock->token.close->CompletionToken.Event
            );
            break;
        }

    };
    Print(L"Close done\n");

    // free sock
    efi_status = uefi_call_wrapper(BS->FreePool, 1, sock);
    MY_EFI_ASSERT(efi_status, __LINE__);


    // Destroy Child
    efi_status = uefi_call_wrapper(
        tcp4_service_binding_protocol->DestroyChild,
        2,
        tcp4_service_binding_protocol,
        tcp4_handler
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

	return EFI_SUCCESS;
}

