#include "drivers/XboxWirelessAdapter.h"
#include "drivers/XboxOneController.h"
#include <cstring>

namespace controllerlib
{
    namespace
    {
        // GIP, the Game Input Protocol. Over the radio, frames are the same as over USB.
        constexpr uint8_t GipCmdAck = 0x01;
        constexpr uint8_t GipCmdAnnounce = 0x02;
        constexpr uint8_t GipCmdPower = 0x05;
        constexpr uint8_t GipCmdVirtualKey = 0x07;
        constexpr uint8_t GipCmdRumble = 0x09;
        constexpr uint8_t GipCmdLed = 0x0a;
        constexpr uint8_t GipCmdInput = 0x20;

        constexpr uint8_t GipOptAck = 0x10;
        constexpr uint8_t GipOptInternal = 0x20;
        constexpr uint8_t GipDeviceIdMask = 0x0f;
        constexpr size_t GipHeaderSize = 4;

        constexpr uint8_t GipPowerOn = 0x00;
        constexpr uint8_t GipPowerOff = 0x04;
        constexpr uint8_t GipLedOn = 0x01;
        // LED brightness ranges from 0x00 to 0x20; the Windows driver dims it to this.
        constexpr uint8_t GipLedBrightness = 0x14;
        constexpr uint8_t GipMotorAll = 0x0f;
        constexpr uint8_t GipMotorLevelMax = 100;

        constexpr PinId GuideButtonPin{12};

        uint8_t WcidOf(uint16_t slot) { return static_cast<uint8_t>(slot + 1); }
    } // namespace

    XboxWirelessAdapter::XboxWirelessAdapter(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger, std::unique_ptr<IFirmwareSource> &&firmware)
        : BaseController(std::move(device), config, std::move(logger)),
          m_firmware(std::move(firmware))
    {
    }

    XboxWirelessAdapter::~XboxWirelessAdapter()
    {
    }

    Status XboxWirelessAdapter::OpenInterfaces()
    {
        Status result = m_device->Open();
        if (result != Status::Success)
            return result;

        auto &interfaces = m_device->GetInterfaces();
        if (interfaces.empty())
            return Status::NoInterfaces;

        IUSBInterface *interface = interfaces[0].get();
        if ((result = interface->Open()) != Status::Success)
            return result;

        m_interfaces.push_back(interface);

        for (uint8_t idx = 0; idx < 15; idx++)
        {
            for (IUSBEndpoint::Direction direction : {IUSBEndpoint::USB_ENDPOINT_IN, IUSBEndpoint::USB_ENDPOINT_OUT})
            {
                IUSBEndpoint *endpoint = interface->GetEndpoint(direction, idx);
                if (endpoint == nullptr)
                    continue;

                const uint8_t address = endpoint->GetDescriptor()->bEndpointAddress;
                IUSBEndpoint **slot = address == Mt76::EndpointRead         ? &m_eventEndpoint
                                      : address == Mt76::EndpointReadPacket ? &m_packetEndpoint
                                      : address == Mt76::EndpointWrite      ? &m_writeEndpoint
                                                                            : nullptr;
                if (slot == nullptr)
                    continue;

                if ((result = endpoint->Open(0)) != Status::Success)
                {
                    m_logger->Log(LogLevel::Error, "XboxWirelessAdapter: Failed to open endpoint 0x%02x (%s)", address, ToString(result));
                    return result;
                }

                *slot = endpoint;
                (direction == IUSBEndpoint::USB_ENDPOINT_IN ? m_inPipe : m_outPipe).push_back(endpoint);
            }
        }

        if (m_eventEndpoint == nullptr || m_packetEndpoint == nullptr || m_writeEndpoint == nullptr)
        {
            m_logger->Log(LogLevel::Error, "XboxWirelessAdapter: Endpoints 0x%02x/0x%02x/0x%02x not all present", Mt76::EndpointRead, Mt76::EndpointReadPacket, Mt76::EndpointWrite);
            return Status::InvalidEndpoint;
        }

        return Status::Success;
    }

    Status XboxWirelessAdapter::Initialize()
    {
        m_logger->Log(LogLevel::Info, "XboxWirelessAdapter[%04x-%04x] Starting radio ...", m_device->GetVendor(), m_device->GetProduct());

        Status result = BaseController::Initialize();
        if (result != Status::Success)
            return result;

        m_radio = std::make_unique<Mt76>(*m_interfaces[0], *m_writeEndpoint, *m_logger);
        result = m_radio->Start(*m_firmware);
        m_firmware.reset();

        if (result != Status::Success)
        {
            m_logger->Log(LogLevel::Error, "XboxWirelessAdapter[%04x-%04x] Radio failed to start (%s)", m_device->GetVendor(), m_device->GetProduct(), ToString(result));
            return result;
        }

        m_logger->Log(LogLevel::Info, "XboxWirelessAdapter[%04x-%04x] Ready - press the button on the adapter, then the pair button on the controller", m_device->GetVendor(), m_device->GetProduct());
        return Status::Success;
    }

    void XboxWirelessAdapter::Exit()
    {
        if (m_radio)
        {
            for (uint16_t slot = 0; slot < m_clients.size(); slot++)
            {
                if (m_clients[slot].announced)
                {
                    const uint8_t mode = GipPowerOff;
                    (void)SendGip(slot, GipCmdPower, GipOptInternal, &mode, sizeof(mode));
                }
            }

            m_radio->Stop();
            m_radio.reset();
        }

        m_clients = {};
        m_eventEndpoint = m_packetEndpoint = m_writeEndpoint = nullptr;
        BaseController::Exit();
    }

    bool XboxWirelessAdapter::IsControllerConnected(uint16_t input_idx)
    {
        return input_idx < m_clients.size() && m_clients[input_idx].announced;
    }

    Status XboxWirelessAdapter::ReadNextBuffer(uint8_t *buffer, size_t *size, uint16_t *input_idx, uint32_t timeout_us)
    {
        uint8_t transfer[Mt76::MaxTransferSize];

        for (IUSBEndpoint *endpoint : {m_packetEndpoint, m_eventEndpoint})
        {
            size_t transferSize = sizeof(transfer);
            if (endpoint->Read(transfer, &transferSize, 0) == Status::Success && transferSize > 0)
                return HandleTransfer(transfer, transferSize, buffer, size, input_idx);
        }

        // Only input paces the thread: a wait on the event endpoint would hold back input that
        // lands meanwhile for the whole timeout, while pairing and disconnect events can afford
        // to be picked up by the probe above one period late.
        size_t transferSize = sizeof(transfer);
        Status result = m_packetEndpoint->Read(transfer, &transferSize, timeout_us);
        if (result != Status::Success)
            return result;

        return HandleTransfer(transfer, transferSize, buffer, size, input_idx);
    }

    Status XboxWirelessAdapter::HandleTransfer(const uint8_t *transfer, size_t transferSize, uint8_t *buffer, size_t *size, uint16_t *input_idx)
    {
        const RadioEvent event = m_radio->ParseTransfer(transfer, transferSize);

        switch (event.type)
        {
            case RadioEvent::Type::None:
                return Status::NothingTodo;

            case RadioEvent::Type::PairButtonPressed:
                return OnPairButtonPressed();

            case RadioEvent::Type::ClientPairing:
                return OnPairRequest(event.address);

            case RadioEvent::Type::ClientAssociating:
                return OnClientAssociating(event.address, input_idx);

            case RadioEvent::Type::ClientLost:
                return OnClientLost(event.wcid, input_idx);

            case RadioEvent::Type::ClientData:
            {
                if (event.wcid == 0 || event.wcid > m_clients.size() || !m_clients[event.wcid - 1].associated)
                    return Status::NothingTodo;

                if (event.size > *size)
                {
                    m_logger->Log(LogLevel::Error, "XboxWirelessAdapter: GIP frame of %d bytes from client %d exceeds the %d-byte buffer", static_cast<int>(event.size), event.wcid, static_cast<int>(*size));
                    return Status::UnexpectedData;
                }

                std::memcpy(buffer, event.data, event.size);
                *size = event.size;
                *input_idx = event.wcid - 1;
                return Status::Success;
            }
        }

        return Status::NothingTodo;
    }

    Status XboxWirelessAdapter::OnPairButtonPressed()
    {
        Status result;
        if ((result = m_radio->SetPairing(true)) != Status::Success ||
            (result = m_radio->SetLed(Mt76::Led::Blink)) != Status::Success)
            return result;

        return Status::NothingTodo;
    }

    Status XboxWirelessAdapter::OnPairRequest(const MacAddress &address)
    {
        m_logger->Log(LogLevel::Info, "XboxWirelessAdapter: Pairing controller %02x:%02x:%02x:%02x:%02x:%02x",
                      address[0], address[1], address[2], address[3], address[4], address[5]);

        Status result;
        if ((result = m_radio->PairClient(address)) != Status::Success ||
            (result = m_radio->SetPairing(false)) != Status::Success ||
            (result = m_radio->SetLed(Mt76::Led::On)) != Status::Success)
            return result;

        return Status::NothingTodo;
    }

    Status XboxWirelessAdapter::OnClientAssociating(const MacAddress &address, uint16_t *input_idx)
    {
        // A controller that reassociates without having been lost keeps its slot.
        std::optional<uint16_t> slot = FindSlot(address);
        if (!slot)
            slot = FindFreeSlot();

        if (!slot)
        {
            m_logger->Log(LogLevel::Warning, "XboxWirelessAdapter: All %d slots taken, ignoring controller %02x:%02x:%02x:%02x:%02x:%02x",
                          static_cast<int>(m_clients.size()), address[0], address[1], address[2], address[3], address[4], address[5]);
            return Status::NothingTodo;
        }

        Status result = m_radio->AssociateClient(WcidOf(*slot), address);
        if (result != Status::Success)
            return result;

        Client &client = m_clients[*slot];
        client = Client{};
        client.associated = true;
        client.address = address;
        *input_idx = *slot;

        m_logger->Log(LogLevel::Info, "XboxWirelessAdapter: Controller %02x:%02x:%02x:%02x:%02x:%02x associated on slot %d",
                      address[0], address[1], address[2], address[3], address[4], address[5], *slot);

        if ((result = m_radio->SetLed(Mt76::Led::On)) != Status::Success)
            return result;

        return Status::NothingTodo;
    }

    Status XboxWirelessAdapter::OnClientLost(uint8_t wcid, uint16_t *input_idx)
    {
        if (wcid == 0 || wcid > m_clients.size() || !m_clients[wcid - 1].associated)
            return Status::NothingTodo;

        const uint16_t slot = wcid - 1;
        m_clients[slot] = Client{};
        *input_idx = slot;

        m_logger->Log(LogLevel::Info, "XboxWirelessAdapter: Controller on slot %d disconnected", slot);

        Status result = m_radio->RemoveClient(wcid);
        if (result != Status::Success)
            return result;

        if (!HasClients() && (result = m_radio->SetLed(Mt76::Led::Off)) != Status::Success)
            return result;

        return Status::NothingTodo;
    }

    Status XboxWirelessAdapter::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
    {
        if (size < GipHeaderSize)
            return Status::UnexpectedData;

        const uint16_t slot = *input_idx;
        const uint8_t command = buffer[0];
        const uint8_t options = buffer[1];

        if (options & GipOptAck)
        {
            Status result = SendGipAck(slot, buffer);
            if (result != Status::Success)
                return result;
        }

        // Non-zero device IDs are accessories such as the chatpad or headset.
        if ((options & GipDeviceIdMask) != 0)
            return Status::NothingTodo;

        Client &client = m_clients[slot];

        if (command == GipCmdAnnounce)
            return OnAnnounce(slot);

        if (!client.announced)
            return Status::NothingTodo;

        if (command == GipCmdInput)
        {
            if (size < sizeof(XboxOneButtonData))
                return Status::UnexpectedData;

            XboxOneButtonData report;
            std::memcpy(&report, buffer, sizeof(report));
            XboxOneController::DecodeInput(report, &client.input);
            *rawData = client.input;
            return Status::Success;
        }

        if (command == GipCmdVirtualKey)
        {
            if (size < GipHeaderSize + 1)
                return Status::UnexpectedData;

            client.input.buttons[GuideButtonPin] = buffer[GipHeaderSize] != 0;
            *rawData = client.input;
            return Status::Success;
        }

        return Status::NothingTodo;
    }

    Status XboxWirelessAdapter::OnAnnounce(uint16_t slot)
    {
        Client &client = m_clients[slot];

        const uint8_t powerOn = GipPowerOn;
        const uint8_t led[] = {0x00, GipLedOn, GipLedBrightness};

        Status result;
        if ((result = SendGip(slot, GipCmdPower, GipOptInternal, &powerOn, sizeof(powerOn))) != Status::Success ||
            (result = SendGip(slot, GipCmdLed, GipOptInternal, led, sizeof(led))) != Status::Success)
        {
            m_logger->Log(LogLevel::Error, "XboxWirelessAdapter: Failed to power on the controller on slot %d (%s)", slot, ToString(result));
            return result;
        }

        if (!client.announced)
            m_logger->Log(LogLevel::Info, "XboxWirelessAdapter: Controller on slot %d ready", slot);

        client.announced = true;
        return Status::NothingTodo;
    }

    Status XboxWirelessAdapter::SetRumble(uint16_t input_idx, const RumbleValue &rumble)
    {
        if (input_idx >= m_clients.size() || !m_clients[input_idx].announced)
            return Status::InvalidIndex;

        // The last byte is the repeat count; with 0 the effect plays once and dies while the
        // game keeps holding the same amplitude.
        const uint8_t payload[] = {
            0x00, GipMotorAll, 0x00, 0x00,
            static_cast<uint8_t>(ScaleAmplitude(rumble.LowAmplitude(), GipMotorLevelMax)),
            static_cast<uint8_t>(ScaleAmplitude(rumble.HighAmplitude(), GipMotorLevelMax)),
            0xff, 0x00, 0xff};

        return SendGip(input_idx, GipCmdRumble, 0x00, payload, sizeof(payload));
    }

    Status XboxWirelessAdapter::SendGip(uint16_t slot, uint8_t command, uint8_t options, const uint8_t *payload, uint8_t payloadSize)
    {
        Client &client = m_clients[slot];

        uint8_t frame[GipHeaderSize + UINT8_MAX];
        frame[0] = command;
        frame[1] = options;
        frame[2] = NextSequence(client);
        frame[3] = payloadSize;
        std::memcpy(frame + GipHeaderSize, payload, payloadSize);

        return m_radio->SendClientPacket(WcidOf(slot), client.address, frame, GipHeaderSize + payloadSize);
    }

    Status XboxWirelessAdapter::SendGipAck(uint16_t slot, const uint8_t *frame)
    {
        const uint8_t internal = (frame[1] & GipDeviceIdMask) | GipOptInternal;
        const uint8_t ack[] = {
            GipCmdAck, internal, frame[2], 0x09,
            0x00, frame[0], internal, frame[3], 0x00,
            0x00, 0x00, 0x00, 0x00};

        return m_radio->SendClientPacket(WcidOf(slot), m_clients[slot].address, ack, sizeof(ack));
    }

    uint8_t XboxWirelessAdapter::NextSequence(Client &client)
    {
        // Sequence 0 is invalid in GIP.
        if (++client.sequence == 0)
            client.sequence = 1;

        return client.sequence;
    }

    std::optional<uint16_t> XboxWirelessAdapter::FindSlot(const MacAddress &address) const
    {
        for (uint16_t slot = 0; slot < m_clients.size(); slot++)
        {
            if (m_clients[slot].associated && m_clients[slot].address == address)
                return slot;
        }

        return std::nullopt;
    }

    std::optional<uint16_t> XboxWirelessAdapter::FindFreeSlot() const
    {
        for (uint16_t slot = 0; slot < m_clients.size(); slot++)
        {
            if (!m_clients[slot].associated)
                return slot;
        }

        return std::nullopt;
    }

    bool XboxWirelessAdapter::HasClients() const
    {
        for (const Client &client : m_clients)
        {
            if (client.associated)
                return true;
        }

        return false;
    }
} // namespace controllerlib
