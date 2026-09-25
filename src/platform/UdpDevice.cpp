#include "UdpDevice.h"
#include "SwitchLogger.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>   // htons/htonl: <netinet/in.h> is not required to declare them, and newlib does not.
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

using namespace controllerlib;

namespace syscon
{
    namespace
    {
        bool g_socket_initialized = false;
        NifmRequest g_nifm_request{};
        size_t g_socket_tmem_size = 0;

        /*
            UDP only, and as small as it will go.

            The transfer memory libnx asks bsd:u for is
                sb_efficiency * page_round(tcp_tx_max + tcp_rx_max + udp_tx + udp_rx)
            and tmemCreate takes it from the process heap with __libnx_aligned_alloc. This
            sysmodule's whole heap is 512 KiB (INNER_HEAP_SIZE in LibnxRuntime.cpp,
            g_heap_memory in AmsRuntime.cpp), so the libnx defaults -- which work out at
            0x234000, about 2.2 MiB -- do not merely waste memory, they fail outright.

            Zeroing the TCP buffers and dropping sb_efficiency from 4 to 1 gives
            page_round(0x1000 + 0x2000) * 1 == 0x3000, i.e. 12 KiB. If bsd:u ever rejects
            zero-sized TCP buffers, give them 0x1000 each and leave the max sizes at 0 (the
            formula then reuses the initial sizes) for 0x5000; do not reach for the defaults.
        */
        constexpr SocketInitConfig g_socketInitConfig = {
            // The pad never opens a TCP socket, so TCP gets one page each; the
            // UDP buffers keep Nintendo's sizes, since undersized ones bind and
            // poll but never deliver a datagram. That is 60 KiB out of the heap.
            .tcp_tx_buf_size = 0x1000,
            .tcp_rx_buf_size = 0x1000,
            .tcp_tx_buf_max_size = 0,
            .tcp_rx_buf_max_size = 0,
            .udp_tx_buf_size = 0x2400,
            .udp_rx_buf_size = 0xA500,
            .sb_efficiency = 1,
            // Session count does not enter the transfer-memory formula, so
            // this costs nothing.
            .num_bsd_sessions = 3,
            .bsd_service_type = BsdServiceType_User,
        };

        size_t ComputeTransferMemorySize(const SocketInitConfig &cfg)
        {
            const uint32_t tcp_tx = cfg.tcp_tx_buf_max_size != 0 ? cfg.tcp_tx_buf_max_size : cfg.tcp_tx_buf_size;
            const uint32_t tcp_rx = cfg.tcp_rx_buf_max_size != 0 ? cfg.tcp_rx_buf_max_size : cfg.tcp_rx_buf_size;
            size_t sum = tcp_tx + tcp_rx + cfg.udp_tx_buf_size + cfg.udp_rx_buf_size;
            sum = (sum + 0xFFF) & ~static_cast<size_t>(0xFFF);
            return cfg.sb_efficiency * sum;
        }
    } // namespace

    Result UdpSocketInitialize()
    {
        if (g_socket_initialized)
            return 0;

        g_socket_tmem_size = ComputeTransferMemorySize(g_socketInitConfig);

        /*
            bsd:u is fetched through libnx's sm, which is not open here in either flavour: the
            libnx runtime closes it at the end of __appInit, and the Atmosphere runtime brings
            up libstratosphere's sm rather than libnx's. Both guards are reference counted, so
            opening and closing around this call is safe whichever build we are in.
        */
        Result rc = smInitialize();
        if (R_FAILED(rc))
        {
            syscon::logger::LogError("NetworkPad: smInitialize failed (0x%08X)", rc);
            return rc;
        }

        rc = socketInitialize(&g_socketInitConfig);
        if (R_FAILED(rc))
        {
            smExit();
            syscon::logger::LogError("NetworkPad: socketInitialize failed (0x%08X) - requested %d bytes of transfer memory", rc, static_cast<int>(g_socket_tmem_size));
            return rc;
        }

        /*
            A bound socket is not enough on Horizon: until this process has an
            accepted nifm request, its bsd session is not attached to the active
            network interface and no datagram is ever delivered to it. bind()
            and poll() behave exactly as if the port were simply idle, which is
            what makes this worth doing explicitly rather than discovering it
            from an error.
        */
        Result nifm_rc = nifmInitialize(NifmServiceType_User);
        if (R_SUCCEEDED(nifm_rc))
        {
            nifm_rc = nifmCreateRequest(&g_nifm_request, true);
            if (R_SUCCEEDED(nifm_rc))
                nifm_rc = nifmRequestSubmitAndWait(&g_nifm_request);
        }
        if (R_FAILED(nifm_rc))
            syscon::logger::LogError("NetworkPad: nifm request failed (0x%08X) - inbound datagrams may never arrive", nifm_rc);
        else
            syscon::logger::LogDebug("NetworkPad: nifm network request accepted");

        smExit();

        g_socket_initialized = true;
        syscon::logger::LogDebug("NetworkPad: socket driver up (%d bytes of transfer memory)", static_cast<int>(g_socket_tmem_size));

        return 0;
    }

    void UdpSocketFinalize()
    {
        if (!g_socket_initialized)
            return;

        socketExit();
        g_socket_initialized = false;
    }

    // -----------------------------------------------------------------------------------
    // Endpoint
    // -----------------------------------------------------------------------------------

    UdpEndpoint::UdpEndpoint(uint16_t port)
        : m_port(port)
    {
        m_descriptor.bLength = sizeof(EndpointDescriptor);
        m_descriptor.bDescriptorType = 0x05; // USB_DT_ENDPOINT
        m_descriptor.bEndpointAddress = USB_ENDPOINT_IN | 0x01;
        m_descriptor.bmAttributes = 0x03; // interrupt
        m_descriptor.wMaxPacketSize = 64;
        m_descriptor.bInterval = 1;
    }

    UdpEndpoint::~UdpEndpoint()
    {
        Close();
    }

    Status UdpEndpoint::Open(int maxPacketSize)
    {
        (void)maxPacketSize;

        if (m_socket >= 0)
            return Status::Success;

        m_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_socket < 0)
        {
            syscon::logger::LogError("NetworkPad: socket() failed (errno %d)", errno);
            return Status::OpenFailed;
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(m_port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(m_socket, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
        {
            syscon::logger::LogError("NetworkPad: bind() to port %d failed (errno %d)", m_port, errno);
            close(m_socket);
            m_socket = -1;
            return Status::OpenFailed;
        }

        /*
            Non-blocking for good: the wait is done by poll() in Read(), so a zero timeout can
            return immediately. BaseController::ReadNextBuffer probes with a zero timeout before
            it blocks, and ReadEndpointLatest drains with one in a loop; a blocking recv would
            turn either into a stall.
        */
        if (fcntl(m_socket, F_SETFL, O_NONBLOCK) < 0)
        {
            syscon::logger::LogError("NetworkPad: fcntl(O_NONBLOCK) failed (errno %d)", errno);
            close(m_socket);
            m_socket = -1;
            return Status::OpenFailed;
        }

        syscon::logger::LogInfo("NetworkPad: listening on UDP port %d", m_port);

        return Status::Success;
    }

    void UdpEndpoint::Close()
    {
        if (m_socket < 0)
            return;

        close(m_socket);
        m_socket = -1;
    }

    Status UdpEndpoint::Write(const uint8_t *inBuffer, size_t bufferSize)
    {
        (void)inBuffer;
        (void)bufferSize;
        return Status::NotImplemented;
    }

    Status UdpEndpoint::Read(uint8_t *outBuffer, size_t *bufferSizeInOut, uint64_t aTimeoutUs)
    {
        if (m_socket < 0)
            return Status::InvalidEndpoint;

        if (aTimeoutUs > 0)
        {
            /*
                poll() with a single fd uses alloca, not the heap (socket.c compares against
                __nx_pollfd_sb_max_fds, which is 64), so this stays allocation-free -- which
                matters on a 512 KiB heap and an 8 KiB polling-thread stack.

                Round the timeout up: a sub-millisecond value truncating to 0 would turn the
                pacing wait into a busy loop on core 3.
            */
            struct pollfd pfd;
            pfd.fd = m_socket;
            pfd.events = POLLIN;
            pfd.revents = 0;

            /*
                poll() only paces this read: its result is advisory and we fall through either
                way. The socket is O_NONBLOCK, so the recv below returns EAGAIN immediately when
                nothing is queued and is the authoritative answer.
            */
            poll(&pfd, 1, static_cast<int>((aTimeoutUs + 999) / 1000));
        }

        const ssize_t received = recv(m_socket, outBuffer, *bufferSizeInOut, 0);
        if (received < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return Status::Timeout;

            // Log sparsely: this runs on the polling thread, potentially every few ms.
            if ((m_dropped_datagrams++ % 100) == 0)
                syscon::logger::LogError("NetworkPad: recv failed (errno %d, %d so far)", errno, static_cast<int>(m_dropped_datagrams));

            return Status::ReadFailed;
        }

        *bufferSizeInOut = static_cast<size_t>(received);
        return Status::Success;
    }

    IUSBEndpoint::Direction UdpEndpoint::GetDirection()
    {
        return USB_ENDPOINT_IN;
    }

    IUSBEndpoint::EndpointDescriptor *UdpEndpoint::GetDescriptor()
    {
        return &m_descriptor;
    }

    // -----------------------------------------------------------------------------------
    // Interface
    // -----------------------------------------------------------------------------------

    UdpInterface::UdpInterface(uint16_t port)
        : m_inEndpoint(std::make_unique<UdpEndpoint>(port))
    {
        m_descriptor.bLength = sizeof(InterfaceDescriptor);
        m_descriptor.bDescriptorType = 0x04; // USB_DT_INTERFACE
        m_descriptor.bInterfaceNumber = 0;
        m_descriptor.bAlternateSetting = 0;
        m_descriptor.bNumEndpoints = 1;
        m_descriptor.bInterfaceClass = 0xFF; // vendor specific
        m_descriptor.bInterfaceSubClass = 0;
        m_descriptor.bInterfaceProtocol = 0;
        m_descriptor.iInterface = 0;
    }

    UdpInterface::~UdpInterface()
    {
    }

    Status UdpInterface::Open()
    {
        return Status::Success;
    }

    void UdpInterface::Close()
    {
        m_inEndpoint->Close();
    }

    Status UdpInterface::ControlTransferInput(uint8_t bmRequestType, uint8_t bmRequest, uint16_t wValue, uint16_t wIndex, void *buffer, uint16_t *wLength)
    {
        (void)bmRequestType;
        (void)bmRequest;
        (void)wValue;
        (void)wIndex;
        (void)buffer;
        (void)wLength;
        return Status::NotImplemented;
    }

    Status UdpInterface::ControlTransferOutput(uint8_t bmRequestType, uint8_t bmRequest, uint16_t wValue, uint16_t wIndex, const void *buffer, uint16_t wLength)
    {
        (void)bmRequestType;
        (void)bmRequest;
        (void)wValue;
        (void)wIndex;
        (void)buffer;
        (void)wLength;
        return Status::NotImplemented;
    }

    IUSBEndpoint *UdpInterface::GetEndpoint(IUSBEndpoint::Direction direction, uint8_t index)
    {
        if (direction == IUSBEndpoint::USB_ENDPOINT_IN && index == 0)
            return m_inEndpoint.get();

        return nullptr;
    }

    IUSBInterface::InterfaceDescriptor *UdpInterface::GetDescriptor()
    {
        return &m_descriptor;
    }

    // -----------------------------------------------------------------------------------
    // Device
    // -----------------------------------------------------------------------------------

    UdpDevice::UdpDevice(uint16_t port)
    {
        m_vendorID = VendorId;
        m_productID = ProductId;
        m_interfaces.push_back(std::make_unique<UdpInterface>(port));
    }

    UdpDevice::~UdpDevice()
    {
        Close();
    }

    Status UdpDevice::Open()
    {
        return Status::Success;
    }

    void UdpDevice::Close()
    {
        for (auto &&interface : m_interfaces)
            interface->Close();
    }
} // namespace syscon
