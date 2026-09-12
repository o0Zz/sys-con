#pragma once

/*
 * Global Lock for USBHS API Calls
 *
 * This lock is required for the entire application when using any usbHsXXXX API.
 *
 * Reason:
 * The usbHsXXXX APIs cannot be called in parallel for an unknown reason. They must
 * be executed sequentially to avoid unexpected behavior (e.g. Initialization failure, ...).
 *
 * Usage:
 * Always ensure you acquire this lock before calling any usbHsXXXX API.
 */

class SwitchUSBLock
{
public:
    SwitchUSBLock(bool scoped = true);
    ~SwitchUSBLock();

    void lock();
    void unlock();

private:
    bool m_scoped;
};