#include <gtest/gtest.h>
#include "drivers/XboxWirelessAdapter.h"
#include "mocks/Logger.h"
#include <algorithm>
#include <cstring>
#include <deque>
#include <map>
#include <vector>

using namespace controllerlib;

namespace
{
    constexpr uint16_t RegFceDmaAddr = 0x0230;
    constexpr uint16_t RegFceDmaLen = 0x0234;
    constexpr uint16_t RegEfuseCtrl = 0x0024;
    constexpr uint8_t VendDevMode = 0x01;
    constexpr uint8_t VendWriteCfg = 0x46;
    constexpr uint32_t FwLoadIvb = 0x12;

    using Bytes = std::vector<uint8_t>;

    class FakeEndpoint : public IUSBEndpoint
    {
    public:
        explicit FakeEndpoint(uint8_t address)
        {
            m_descriptor.bEndpointAddress = address;
            m_descriptor.wMaxPacketSize = 512;
        }

        Status Open(int) override { return Status::Success; }
        void Close() override {}

        Status Write(const uint8_t *buffer, size_t size) override
        {
            writes.emplace_back(buffer, buffer + size);
            return Status::Success;
        }

        Status Read(uint8_t *buffer, size_t *size, uint64_t timeout_us) override
        {
            if (timeout_us != 0)
                blockingReads++;

            if (reads.empty())
                return Status::Timeout;

            Bytes next = reads.front();
            reads.pop_front();
            *size = std::min(*size, next.size());
            std::memcpy(buffer, next.data(), *size);
            return Status::Success;
        }

        Direction GetDirection() override { return (m_descriptor.bEndpointAddress & 0x80) ? USB_ENDPOINT_IN : USB_ENDPOINT_OUT; }
        EndpointDescriptor *GetDescriptor() override { return &m_descriptor; }

        std::vector<Bytes> writes;
        std::deque<Bytes> reads;
        int blockingReads = 0;

    private:
        EndpointDescriptor m_descriptor{};
    };

    // Emulates the adapter's register file closely enough for every poll in the start sequence to complete.
    class FakeAdapterInterface : public IUSBInterface
    {
    public:
        FakeAdapterInterface()
        {
            m_in[0] = std::make_unique<FakeEndpoint>(0x84);
            m_in[1] = std::make_unique<FakeEndpoint>(0x85);
            m_out[0] = std::make_unique<FakeEndpoint>(0x04);
            m_out[1] = std::make_unique<FakeEndpoint>(0x05);
        }

        Status Open() override { return Status::Success; }
        void Close() override {}

        Status ControlTransferInput(uint8_t, uint8_t request, uint16_t, uint16_t index, void *buffer, uint16_t *length) override
        {
            uint32_t value = registers[index];

            if (index == RegFceDmaAddr && firmwareStarted)
                value = 0x01;
            else if (index == RegFceDmaLen)
                value = registers[index] | 0xc0000000;
            else if (index == RegEfuseCtrl)
                value &= ~(1u << 30);

            reads.push_back({request, index});
            std::memcpy(buffer, &value, sizeof(value));
            *length = sizeof(value);
            return Status::Success;
        }

        Status ControlTransferOutput(uint8_t, uint8_t request, uint16_t value, uint16_t index, const void *buffer, uint16_t length) override
        {
            if (request == VendDevMode)
            {
                firmwareStarted |= value == FwLoadIvb;
                return Status::Success;
            }

            uint32_t word = 0;
            std::memcpy(&word, buffer, std::min<size_t>(length, sizeof(word)));
            registers[index] = word;
            writes.push_back({request, index, word});
            return Status::Success;
        }

        IUSBEndpoint *GetEndpoint(IUSBEndpoint::Direction direction, uint8_t index) override
        {
            if (index >= 2)
                return nullptr;
            return direction == IUSBEndpoint::USB_ENDPOINT_IN ? m_in[index].get() : m_out[index].get();
        }

        InterfaceDescriptor *GetDescriptor() override { return &m_descriptor; }

        FakeEndpoint &PacketIn() { return *m_in[0]; }
        FakeEndpoint &EventIn() { return *m_in[1]; }
        FakeEndpoint &Out() { return *m_out[0]; }
        FakeEndpoint &UnusedOut() { return *m_out[1]; }

        struct Write
        {
            uint8_t request;
            uint16_t index;
            uint32_t value;
        };

        struct Read
        {
            uint8_t request;
            uint16_t index;
        };

        std::map<uint16_t, uint32_t> registers;
        std::vector<Write> writes;
        std::vector<Read> reads;
        bool firmwareStarted = false;

    private:
        std::unique_ptr<FakeEndpoint> m_in[2];
        std::unique_ptr<FakeEndpoint> m_out[2];
        InterfaceDescriptor m_descriptor{};
    };

    class FakeDevice : public IUSBDevice
    {
    public:
        explicit FakeDevice(std::unique_ptr<IUSBInterface> &&interface)
        {
            m_vendorID = 0x045e;
            m_productID = 0x02fe;
            m_interfaces.push_back(std::move(interface));
        }

        Status Open() override { return Status::Success; }
        void Close() override {}
    };

    class MemoryFirmware : public IFirmwareSource
    {
    public:
        explicit MemoryFirmware(Bytes image) : m_image(std::move(image)) {}

        size_t Read(uint8_t *buffer, size_t size) override
        {
            size = std::min(size, m_image.size() - m_position);
            std::memcpy(buffer, m_image.data() + m_position, size);
            m_position += size;
            return size;
        }

    private:
        Bytes m_image;
        size_t m_position = 0;
    };

    void AppendWord(Bytes &bytes, uint32_t word)
    {
        for (int i = 0; i < 4; i++)
            bytes.push_back(static_cast<uint8_t>(word >> (8 * i)));
    }

    Bytes FirmwareImage(uint32_t ilmLength, uint32_t dlmLength)
    {
        Bytes image;
        AppendWord(image, ilmLength);
        AppendWord(image, dlmLength);
        image.resize(32, 0);
        for (uint32_t i = 0; i < ilmLength + dlmLength; i++)
            image.push_back(static_cast<uint8_t>(i * 7));
        return image;
    }

    constexpr uint8_t ClientAddress[6] = {0x7e, 0xed, 0x01, 0x02, 0x03, 0x04};
    constexpr uint8_t AdapterAddress[6] = {0x62, 0x45, 0xbd, 0x00, 0x00, 0x00};

    const Bytes AssociationBody = {0x31, 0xc6, 0x0a, 0x00};

    Bytes Transfer(uint32_t info, const Bytes &packet)
    {
        Bytes transfer;
        AppendWord(transfer, info);
        transfer.insert(transfer.end(), packet.begin(), packet.end());
        AppendWord(transfer, info);
        return transfer;
    }

    Bytes WlanTransfer(uint8_t frameControl, uint8_t wcid, const Bytes &body)
    {
        Bytes packet(32, 0);
        packet[4] = wcid;

        packet.push_back(frameControl);
        packet.push_back(0x00);
        packet.insert(packet.end(), 2, 0);
        packet.insert(packet.end(), AdapterAddress, AdapterAddress + 6);
        packet.insert(packet.end(), ClientAddress, ClientAddress + 6);
        packet.insert(packet.end(), ClientAddress, ClientAddress + 6);
        packet.insert(packet.end(), 2, 0);
        packet.insert(packet.end(), body.begin(), body.end());

        constexpr uint32_t Is80211 = 1u << 19;
        return Transfer(Is80211, packet);
    }

    Bytes GipTransfer(uint8_t wcid, const Bytes &gip)
    {
        Bytes body = {0x00, 0x00, 0x00, 0x00};
        body.insert(body.end(), gip.begin(), gip.end());
        constexpr uint8_t QosData = (2 << 2) | (8 << 4);
        return WlanTransfer(QosData, wcid, body);
    }

    bool Contains(const Bytes &haystack, const Bytes &needle)
    {
        return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) != haystack.end();
    }

    bool AnyWriteContains(const FakeEndpoint &endpoint, const Bytes &needle)
    {
        return std::any_of(endpoint.writes.begin(), endpoint.writes.end(), [&](const Bytes &write)
                           { return Contains(write, needle); });
    }

    struct Fixture
    {
        Fixture(uint32_t ilmLength = 1000, uint32_t dlmLength = 600)
            : image(FirmwareImage(ilmLength, dlmLength))
        {
            auto interfacePtr = std::make_unique<FakeAdapterInterface>();
            interface = interfacePtr.get();

            config.buttonsPin[GamepadButton::A][0] = PinId(2);
            config.buttonsPin[GamepadButton::B][0] = PinId(1);
            config.buttonsPin[GamepadButton::HOME][0] = PinId(12);

            adapter = std::make_unique<XboxWirelessAdapter>(std::make_unique<FakeDevice>(std::move(interfacePtr)), config,
                                                            std::make_unique<MockLogger>(), std::make_unique<MemoryFirmware>(image));
        }

        Status Read(NormalizedButtonData *data, uint16_t *slot)
        {
            *slot = 0xffff;
            return adapter->ReadInput(data, slot, 0);
        }

        Bytes image;
        ControllerConfig config;
        FakeAdapterInterface *interface = nullptr;
        std::unique_ptr<XboxWirelessAdapter> adapter;
    };

    Status Connect(Fixture &fixture, uint8_t wcid)
    {
        NormalizedButtonData data;
        uint16_t slot;

        fixture.interface->PacketIn().reads.push_back(WlanTransfer(0x00, 0, AssociationBody));
        Status result = fixture.Read(&data, &slot);
        if (result != Status::NothingTodo)
            return result;

        Bytes announce = {0x02, 0x20, 0x01, 0x1c};
        announce.resize(4 + 0x1c, 0);
        fixture.interface->PacketIn().reads.push_back(GipTransfer(wcid, announce));
        return fixture.Read(&data, &slot);
    }
} // namespace

TEST(XboxWirelessAdapter, uploads_firmware_in_order_and_starts)
{
    Fixture fixture(1000, 600);
    ASSERT_EQ(fixture.adapter->Initialize(), Status::Success);

    std::vector<uint32_t> chunkAddresses;
    for (const auto &write : fixture.interface->writes)
    {
        if (write.request == VendWriteCfg && write.index == RegFceDmaAddr && write.value != 0)
            chunkAddresses.push_back(write.value);
    }

    const std::vector<uint32_t> expected = {0x80000, 0x80000 + 500, 0x110800, 0x110800 + 500};
    EXPECT_EQ(chunkAddresses, expected);

    const Bytes &firstChunk = fixture.interface->Out().writes[0];
    ASSERT_EQ(firstChunk.size(), 4u + 500u + 4u);
    EXPECT_TRUE(std::equal(firstChunk.begin() + 4, firstChunk.begin() + 504, fixture.image.begin() + 32));

    for (const auto &write : fixture.interface->Out().writes)
        EXPECT_LE(write.size(), Mt76::MaxTransferSize);

    EXPECT_TRUE(fixture.interface->UnusedOut().writes.empty());
    EXPECT_TRUE(fixture.interface->firmwareStarted);
}

TEST(XboxWirelessAdapter, fails_on_truncated_firmware)
{
    Fixture fixture(1000, 600);
    fixture.image.resize(100);
    fixture.adapter = std::make_unique<XboxWirelessAdapter>(
        std::make_unique<FakeDevice>(std::make_unique<FakeAdapterInterface>()), fixture.config,
        std::make_unique<MockLogger>(), std::make_unique<MemoryFirmware>(fixture.image));

    EXPECT_EQ(fixture.adapter->Initialize(), Status::UnexpectedData);
}

TEST(XboxWirelessAdapter, pair_button_enables_pairing_beacon)
{
    Fixture fixture;
    ASSERT_EQ(fixture.adapter->Initialize(), Status::Success);
    fixture.interface->Out().writes.clear();

    constexpr uint32_t ButtonPressEvent = (1u << 27) | (0x04u << 20);
    fixture.interface->EventIn().reads.push_back(Transfer(ButtonPressEvent, {0x00, 0x00, 0x00, 0x00}));

    NormalizedButtonData data;
    uint16_t slot;
    EXPECT_EQ(fixture.Read(&data, &slot), Status::NothingTodo);

    const Bytes pairingElement = {0xdd, 0x10, 0x00, 0x50, 0xf2, 0x11, 0x01, 0x10, 0x01};
    EXPECT_TRUE(AnyWriteContains(fixture.interface->Out(), pairingElement));
}

TEST(XboxWirelessAdapter, controller_connects_after_announce)
{
    Fixture fixture;
    ASSERT_EQ(fixture.adapter->Initialize(), Status::Success);
    fixture.interface->Out().writes.clear();

    NormalizedButtonData data;
    uint16_t slot;

    fixture.interface->PacketIn().reads.push_back(WlanTransfer(0x00, 0, AssociationBody));
    EXPECT_EQ(fixture.Read(&data, &slot), Status::NothingTodo);
    EXPECT_EQ(slot, 0);
    EXPECT_FALSE(fixture.adapter->IsControllerConnected(0));
    EXPECT_TRUE(AnyWriteContains(fixture.interface->Out(), Bytes(ClientAddress, ClientAddress + 6)));

    Bytes announce = {0x02, 0x20, 0x01, 0x1c};
    announce.resize(4 + 0x1c, 0);
    fixture.interface->PacketIn().reads.push_back(GipTransfer(1, announce));
    EXPECT_EQ(fixture.Read(&data, &slot), Status::NothingTodo);
    EXPECT_EQ(slot, 0);
    EXPECT_TRUE(fixture.adapter->IsControllerConnected(0));

    EXPECT_TRUE(AnyWriteContains(fixture.interface->Out(), {0x05, 0x20, 0x01, 0x01, 0x00}));
    EXPECT_TRUE(AnyWriteContains(fixture.interface->Out(), {0x0a, 0x20, 0x02, 0x03, 0x00, 0x01, 0x14}));
}

TEST(XboxWirelessAdapter, input_report_maps_to_buttons)
{
    Fixture fixture;
    ASSERT_EQ(fixture.adapter->Initialize(), Status::Success);
    ASSERT_EQ(Connect(fixture, 1), Status::NothingTodo);

    Bytes input = {0x20, 0x00, 0x05, 0x0e, 0x10, 0x00};
    input.resize(4 + 0x0e, 0);
    fixture.interface->PacketIn().reads.push_back(GipTransfer(1, input));

    NormalizedButtonData data;
    uint16_t slot;
    ASSERT_EQ(fixture.Read(&data, &slot), Status::Success);
    EXPECT_EQ(slot, 0);
    EXPECT_TRUE(data.buttons[GamepadButton::B]);
    EXPECT_FALSE(data.buttons[GamepadButton::A]);
}

TEST(XboxWirelessAdapter, guide_button_is_acknowledged_and_reported)
{
    Fixture fixture;
    ASSERT_EQ(fixture.adapter->Initialize(), Status::Success);
    ASSERT_EQ(Connect(fixture, 1), Status::NothingTodo);
    fixture.interface->Out().writes.clear();

    fixture.interface->PacketIn().reads.push_back(GipTransfer(1, {0x07, 0x30, 0x09, 0x02, 0x01, 0x5b}));

    NormalizedButtonData data;
    uint16_t slot;
    ASSERT_EQ(fixture.Read(&data, &slot), Status::Success);
    EXPECT_TRUE(data.buttons[GamepadButton::HOME]);
    EXPECT_TRUE(AnyWriteContains(fixture.interface->Out(), {0x01, 0x20, 0x09, 0x09, 0x00, 0x07, 0x20, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00}));
}

TEST(XboxWirelessAdapter, client_lost_disconnects_slot)
{
    Fixture fixture;
    ASSERT_EQ(fixture.adapter->Initialize(), Status::Success);
    ASSERT_EQ(Connect(fixture, 1), Status::NothingTodo);
    ASSERT_TRUE(fixture.adapter->IsControllerConnected(0));

    constexpr uint32_t ClientLostEvent = (1u << 27) | (0x0eu << 20);
    fixture.interface->EventIn().reads.push_back(Transfer(ClientLostEvent, {0x01, 0x00, 0x00, 0x00}));

    NormalizedButtonData data;
    uint16_t slot;
    EXPECT_EQ(fixture.Read(&data, &slot), Status::NothingTodo);
    EXPECT_EQ(slot, 0);
    EXPECT_FALSE(fixture.adapter->IsControllerConnected(0));
}

TEST(XboxWirelessAdapter, frames_for_other_addresses_are_ignored)
{
    Fixture fixture;
    ASSERT_EQ(fixture.adapter->Initialize(), Status::Success);

    Bytes transfer = WlanTransfer(0x00, 0, AssociationBody);
    transfer[4 + 32 + 4] ^= 0xff;
    fixture.interface->PacketIn().reads.push_back(transfer);

    NormalizedButtonData data;
    uint16_t slot;
    EXPECT_EQ(fixture.Read(&data, &slot), Status::NothingTodo);
    EXPECT_FALSE(fixture.adapter->IsControllerConnected(0));
}

TEST(XboxWirelessAdapter, idle_reads_block_on_input_only)
{
    Fixture fixture;
    ASSERT_EQ(fixture.adapter->Initialize(), Status::Success);
    ASSERT_EQ(Connect(fixture, 1), Status::NothingTodo);

    NormalizedButtonData data;
    uint16_t slot;
    for (int i = 0; i < 4; i++)
        EXPECT_EQ(fixture.adapter->ReadInput(&data, &slot, 10000), Status::Timeout);

    EXPECT_EQ(fixture.interface->PacketIn().blockingReads, 4);
    EXPECT_EQ(fixture.interface->EventIn().blockingReads, 0);
}

TEST(XboxWirelessAdapter, events_are_picked_up_while_idle)
{
    Fixture fixture;
    ASSERT_EQ(fixture.adapter->Initialize(), Status::Success);
    ASSERT_EQ(Connect(fixture, 1), Status::NothingTodo);

    constexpr uint32_t ClientLostEvent = (1u << 27) | (0x0eu << 20);
    fixture.interface->EventIn().reads.push_back(Transfer(ClientLostEvent, {0x01, 0x00, 0x00, 0x00}));

    NormalizedButtonData data;
    uint16_t slot;
    EXPECT_EQ(fixture.adapter->ReadInput(&data, &slot, 10000), Status::NothingTodo);
    EXPECT_FALSE(fixture.adapter->IsControllerConnected(0));
}
