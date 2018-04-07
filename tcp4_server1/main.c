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

// inline を削除すると動かなくなるので注意
static inline
EFI_STATUS
show_tcp4_ap(IN EFI_TCP4 *tcp4_protocol)
{
    EFI_STATUS efi_status;
    volatile EFI_TCP4_CONFIG_DATA config_data;

    efi_status = uefi_call_wrapper(
        tcp4_protocol->GetModeData,
        6,
        tcp4_protocol,
        (EFI_TCP4_CONNECTION_STATE *)NULL, /* ignore Tcp4State*/
        (EFI_TCP4_CONFIG_DATA *)&config_data,
        (EFI_IP4_MODE_DATA *)NULL, /* ignore Ip4ModeData */
        (EFI_MANAGED_NETWORK_CONFIG_DATA *)NULL, /* ignore MnpConfigData */
        (EFI_SIMPLE_NETWORK_MODE *)NULL  /* ignore SnpModeData */
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    // show current tcp4 access point
    EFI_TCP4_ACCESS_POINT cap = config_data.AccessPoint;
    EFI_IPv4_ADDRESS csip = cap.StationAddress;
    EFI_IPv4_ADDRESS cssub = cap.SubnetMask;
    UINT16 csport = cap.StationPort;
    EFI_IPv4_ADDRESS crip = cap.RemoteAddress;
    UINT16 crport = cap.RemotePort;
    Print(L"StationAddress = %02x %02x %02x %02x\n", csip.Addr[0], csip.Addr[1], csip.Addr[2], csip.Addr[3]);
    Print(L"SubnetMask = %02x %02x %02x %02x\n", cssub.Addr[0], cssub.Addr[1], cssub.Addr[2], cssub.Addr[3]);
    Print(L"StationPort = %d\n", csport);
    Print(L"RemoteAddress = %02x %02x %02x %02x\n", crip.Addr[0], crip.Addr[1], crip.Addr[2], crip.Addr[3]);
    Print(L"RemotePort = %d\n", crport);

    return efi_status;
}

static inline 
EFI_TCP4_CONNECTION_STATE
tcp4_connection_state(IN EFI_TCP4 *tcp4_protocol)
{
    EFI_STATUS efi_status;
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

    return state;
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

// Notify func

volatile BOOLEAN accepted;
volatile BOOLEAN transmitted;
volatile BOOLEAN closed;

VOID
AcceptNotify(
    EFI_EVENT Event,
    VOID *Context
    )
{
    (void)Event;
    (void)Context;
    Print(L"Notify\n");
    accepted = TRUE;
    return;
}

VOID
TransmitNotify(
    EFI_EVENT Event,
    VOID *Context
    )
{
    (void)Event;
    (void)Context;
    Print(L"TransmitNotify\n");
    transmitted = TRUE;
    return;
}

VOID
CloseNotify(
    EFI_EVENT Event,
    VOID *Context
    )
{
    (void)Event;
    (void)Context;
    Print(L"CloseNotify\n");
    closed = TRUE;
    return;
}



EFI_STATUS
efi_main (EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab)
{
    EFI_STATUS efi_status;

    InitializeLib(image_handle, systab);

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

    // show current tcp4 access point
    // Print(L"Show current AP infomation\n");
    // efi_status = show_tcp4_ap(tcp4_protocol);
    // MY_EFI_ASSERT(efi_status, __LINE__);


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


    EFI_TCP4_LISTEN_TOKEN accept_token;
    efi_status = CreateTcp4NotifyEvent(&accept_token.CompletionToken.Event, (EFI_EVENT_NOTIFY)AcceptNotify, (VOID *)NULL);
    MY_EFI_ASSERT(efi_status, __LINE__);

    accepted = FALSE;
    efi_status = uefi_call_wrapper(
        tcp4_protocol->Accept,
        2,
        tcp4_protocol,
        &accept_token
    );
    MY_EFI_ASSERT(efi_status, __LINE__);
    
    // wait until connect
    Print(L"Waiting Accept...\n");
    while (accepted == FALSE);
    Print(L"Accept complete!\n");

    // close event
    efi_status = uefi_call_wrapper(
        BS->CloseEvent,
        1,
        accept_token.CompletionToken.Event
    );

    // get new child handle
    EFI_TCP4 *tcp4_protocol_server = NULL;
    efi_status = open_tcp4_protocol(&tcp4_protocol_server, accept_token.NewChildHandle);
    MY_EFI_ASSERT(efi_status, __LINE__);
    if (tcp4_protocol_server == NULL) {
        Print(L"Error: failed to load TCP4 protocol!\n");
        return EFI_ABORTED;
    }

    // show current tcp4 access point
    // efi_status = show_tcp4_ap(tcp4_protocol_server);
    // MY_EFI_ASSERT(efi_status, __LINE__);

    // show state
    // EFI_TCP4_CONNECTION_STATE accepted_state = tcp4_connection_state(tcp4_protocol_server);
    // Print(L"Accepted state = %d\n", accepted_state);


    // transmit
    EFI_TCP4_IO_TOKEN iotoken;
    efi_status = CreateTcp4NotifyEvent(&iotoken.CompletionToken.Event, (EFI_EVENT_NOTIFY)TransmitNotify, (VOID *)NULL);
    MY_EFI_ASSERT(efi_status, __LINE__);

    CHAR8 txbuffer[] = "Hello";
    EFI_TCP4_FRAGMENT_DATA txfragment = {
        .FragmentLength = strlena(txbuffer),
        .FragmentBuffer = txbuffer
    };

    EFI_TCP4_TRANSMIT_DATA txdata = {
        .Push = FALSE,
        .Urgent = FALSE,
        .DataLength = txfragment.FragmentLength,
        .FragmentCount = 1,
        .FragmentTable = { txfragment }
    };
    iotoken.Packet.TxData = &txdata;
    Print(L"Transmitting...\n");
    efi_status = uefi_call_wrapper(
        tcp4_protocol_server->Transmit,
        2,
        tcp4_protocol_server,
        &iotoken
    );
    MY_EFI_ASSERT(efi_status, __LINE__);
    while (transmitted == FALSE);
    // transmit finish
    Print(L"Transmit complete\n");

    // close event
    efi_status = uefi_call_wrapper(
        BS->CloseEvent,
        1,
        iotoken.CompletionToken.Event
    );
    MY_EFI_ASSERT(efi_status, __LINE__);

    // Close TCP
    EFI_TCP4_CLOSE_TOKEN close_token;
    closed = FALSE;
    efi_status = CreateTcp4NotifyEvent(&close_token.CompletionToken.Event, (EFI_EVENT_NOTIFY)CloseNotify, (VOID *)NULL);
    MY_EFI_ASSERT(efi_status, __LINE__);
    Print(L"Closing...\n");
    efi_status = uefi_call_wrapper(
        tcp4_protocol_server->Close,
        2,
        tcp4_protocol_server,
        &close_token
    );
    MY_EFI_ASSERT(efi_status, __LINE__);
    while (closed == FALSE);
    Print(L"Close complete\n");

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

