#pragma once
#include "IUSBInterface.h"
#include <memory>
#include <cstdio>
#include <vector>

namespace controllerlib
{
#ifndef _PACKED
    #ifdef __GNUC__
        #define _PACKED(x) x __attribute__((__packed__))
    #endif

    #ifdef _MSC_VER
        #define _PACKED(x) __pragma(pack(push, 1)) x __pragma(pack(pop))
    #endif
#endif

    class IUSBDevice
    {
    protected:
        std::vector<std::unique_ptr<IUSBInterface>> m_interfaces{};

        uint16_t m_vendorID = 0;
        uint16_t m_productID = 0;

    public:
        virtual ~IUSBDevice() = default;

        virtual Status Open() = 0;
        virtual void Close() = 0;

        virtual std::vector<std::unique_ptr<IUSBInterface>> &GetInterfaces() { return m_interfaces; }

        virtual uint16_t GetVendor() { return m_vendorID; }
        virtual uint16_t GetProduct() { return m_productID; }
    };
} // namespace controllerlib
