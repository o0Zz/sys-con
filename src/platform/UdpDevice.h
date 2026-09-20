#pragma once
#include <switch.h>
#include "IUSBDevice.h"
#include "IUSBInterface.h"
#include "IUSBEndpoint.h"
#include <memory>

/*
    A UDP socket dressed up as a USB device, so a software-driven controller reaches the
    console through exactly the same path a real one does.

    NetworkController (src/controllerlib/drivers/NetworkController.h) decodes the datagrams,
    and it is an ordinary BaseController: it wants an IUSBDevice with one IN endpoint it can
    Read() from. Satisfying that interface here, rather than special-casing the pipeline, is what
    keeps usb_module, controller_handler and both virtual-pad handlers untouched -- and means
    the network pad exercises the real mapping, deadzone and combo code rather than a shortcut
    around it.

    The socket is bound once, by the endpoint's Open(), and read with a bounded timeout.
*/
namespace syscon
{
    /*
        Brings up the libnx BSD socket driver for UDP only.

        This is emphatically not socketInitializeDefault(): that asks for ~2.2 MiB of transfer
        memory (sb_efficiency * page_round(tcp_tx_max + tcp_rx_max + udp_tx + udp_rx), see
        libnx's bsd.c), which tmemCreate takes from the process heap -- and this sysmodule's
        heap is 512 KiB in total. Zeroing the TCP buffers and dropping sb_efficiency to 1
        brings it down to 12 KiB, which is what makes this feature possible without growing
        the heap for everyone.

        Safe to call more than once; reference-counted like the libnx service guards.
        Returns a Horizon Result; never aborts, because bsd:u may simply not be up yet.
    */
    Result UdpSocketInitialize();
    void UdpSocketFinalize();

    /// Transfer memory the socket driver was asked for, for logging. Valid after Initialize.
    size_t UdpSocketTransferMemorySize();

    class UdpEndpoint : public controllerlib::IUSBEndpoint
    {
    public:
        explicit UdpEndpoint(uint16_t port);
        ~UdpEndpoint() override;

        controllerlib::Status Open(int maxPacketSize = 0) override;
        void Close() override;

        /// Input-only: the sender gets nothing back, so this always fails.
        controllerlib::Status Write(const uint8_t *inBuffer, size_t bufferSize) override;

        /*
            One datagram, or Status::Timeout.

            aTimeoutUs == 0 must return promptly: BaseController::ReadNextBuffer sweeps with a
            zero timeout before it blocks, and ReadEndpointLatest drains with one in a loop.
            A blocking call here would stall the polling thread instead of pacing it.
        */
        controllerlib::Status Read(uint8_t *outBuffer, size_t *bufferSizeInOut, uint64_t aTimeoutUs) override;

        controllerlib::IUSBEndpoint::Direction GetDirection() override;
        controllerlib::IUSBEndpoint::EndpointDescriptor *GetDescriptor() override;

    private:
        uint16_t m_port;
        int m_socket = -1;
        uint32_t m_dropped_datagrams = 0;
        uint32_t m_received_datagrams = 0;
        uint32_t m_poll_timeouts = 0;
        controllerlib::IUSBEndpoint::EndpointDescriptor m_descriptor{};
    };

    class UdpInterface : public controllerlib::IUSBInterface
    {
    public:
        explicit UdpInterface(uint16_t port);
        ~UdpInterface() override;

        controllerlib::Status Open() override;
        void Close() override;

        // There is no device to talk to, so the control pipe does not exist.
        controllerlib::Status ControlTransferInput(uint8_t bmRequestType, uint8_t bmRequest, uint16_t wValue, uint16_t wIndex, void *buffer, uint16_t *wLength) override;
        controllerlib::Status ControlTransferOutput(uint8_t bmRequestType, uint8_t bmRequest, uint16_t wValue, uint16_t wIndex, const void *buffer, uint16_t wLength) override;

        controllerlib::IUSBEndpoint *GetEndpoint(controllerlib::IUSBEndpoint::Direction direction, uint8_t index) override;
        controllerlib::IUSBInterface::InterfaceDescriptor *GetDescriptor() override;

        controllerlib::Status Reset() override;

    private:
        std::unique_ptr<UdpEndpoint> m_inEndpoint;
        controllerlib::IUSBInterface::InterfaceDescriptor m_descriptor{};
    };

    class UdpDevice : public controllerlib::IUSBDevice
    {
    public:
        /*
            Not a real vendor id. It only has to be stable, and not collide with a real device,
            so the pad picks up [ffff-0001] from config.ini the way any controller picks up its
            own section.
        */
        static constexpr uint16_t VendorId = 0xFFFF;
        static constexpr uint16_t ProductId = 0x0001;

        explicit UdpDevice(uint16_t port);
        ~UdpDevice() override;

        controllerlib::Status Open() override;
        void Close() override;
        void Reset() override;
    };
} // namespace syscon
