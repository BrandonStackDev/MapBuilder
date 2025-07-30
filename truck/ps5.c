#include <stdio.h>
#include <libusb-1.0/libusb.h>

#define TARGET_VID 0x054C  // Sony
#define TARGET_PID 0x0CE6  // DualSense Wireless Controller
#define PACKET_SIZE 64

void print_bytes(unsigned char *data, int len) {
    printf("HEX: ");
    for (int i = 0; i < len; i++) printf("%02X ", data[i]);
    printf("\nBIN: ");
    for (int i = 0; i < len; i++) {
        for (int b = 7; b >= 0; b--) {
            printf("%d", (data[i] >> b) & 1);
        }
        printf(" ");
    }
    printf("\nASCII: ");
    for (int i = 0; i < len; i++) printf("%c", data[i] >= 32 && data[i] < 127 ? data[i] : '.');
    printf("\n\n");
}

void ParseDualSenseInput(uint8_t *report, int length) {
    if (length < 64 || report[0] != 0x01) {
        printf("Invalid report\n");
        return;
    }

    int lx = report[1];
    int ly = report[2];
    int rx = report[3];
    int ry = report[4];

    float normLX = (lx - 128) / 127.0f;
    float normLY = (ly - 128) / 127.0f;
    float normRX = (rx - 128) / 127.0f;
    float normRY = (ry - 128) / 127.0f;

    printf("Left Stick:  X=%d Y=%d  (%.2f, %.2f)\n", lx, ly, normLX, normLY);
    printf("Right Stick: X=%d Y=%d  (%.2f, %.2f)\n", rx, ry, normRX, normRY);

    uint8_t buttons1 = report[7];
    uint8_t buttons2 = report[8];
    uint8_t buttons3 = report[9];

    int dpad = buttons1 & 0x0F;//todo: this is clearly broken, fix it....
    int btnSquare  = (buttons2 & 0x10) > 0;
    int btnCross   = (buttons2 & 0x20) > 0;
    int btnCircle  = (buttons2 & 0x40) > 0;
    int btnTriangle= (buttons2 & 0x80) > 0;
    //also todo, middle map button press -> i=10, 0x02 mask

    printf("D-Pad: %d | Square: %d Cross: %d Circle: %d Triangle: %d\n",
           dpad, btnSquare, btnCross, btnCircle, btnTriangle);

    int l2 = report[10];
    int r2 = report[11];
    int btnL1 = (buttons3 & 0x01) > 0;
    int btnR1 = (buttons3 & 0x02) > 0;
    int btnL2 = (buttons3 & 0x04) > 0;
    int btnR2 = (buttons3 & 0x08) > 0;
    int btnL3 = (buttons3 & 0x40) > 0;
    int btnR3 = (buttons3 & 0x80) > 0;
    printf("l2: %d, r2: %d\n", l2, r2);
    printf("L1: %d, R1: %d\n", btnL1, btnR1);
    printf("L2: %d, R2: %d\n", btnL2, btnR2);
    printf("L3: %d, R3: %d\n", btnL3, btnR3);
    for (int i = 0; i < length/3; i++) {
        printf("[%02d]: 0x%02X\n", i, report[i]);
    }
}

int main() {
    libusb_context *ctx = NULL;
    libusb_device **list = NULL;
    libusb_device_handle *handle = NULL;
    ssize_t count;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) {
        fprintf(stderr, "Failed to initialize libusb: %s\n", libusb_error_name(r));
        return 1;
    }

    count = libusb_get_device_list(ctx, &list);
    if (count < 0) {
        fprintf(stderr, "Error getting device list\n");
        libusb_exit(ctx);
        return 1;
    }

    printf("Found %ld USB devices:\n", count);
    for (ssize_t i = 0; i < count; i++) {
        libusb_device *dev = list[i];
        struct libusb_device_descriptor desc;

        libusb_get_device_descriptor(dev, &desc);

        printf("Device %zu: VID: %04X, PID: %04X\n", i, desc.idVendor, desc.idProduct);

        libusb_device_handle *handle;
        if (libusb_open(dev, &handle) == 0) {
            unsigned char buffer[256];

            // Try to read the manufacturer string
            if (desc.iManufacturer) {
                libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, buffer, sizeof(buffer));
                printf("  Manufacturer: %s\n", buffer);
            }
            if (desc.iProduct) {
                libusb_get_string_descriptor_ascii(handle, desc.iProduct, buffer, sizeof(buffer));
                printf("  Product     : %s\n", buffer);
            }
            if (desc.idVendor) {
                libusb_get_string_descriptor_ascii(handle, desc.idVendor, buffer, sizeof(buffer));
                printf("  Vendor id    : %s\n", buffer);
            }
            if (desc.idProduct) {
                libusb_get_string_descriptor_ascii(handle, desc.idProduct, buffer, sizeof(buffer));
                printf("  Product id   : %s\n", buffer);
            }

            libusb_close(handle);
        }
    }

    printf("Scanning for device VID: %04X, PID: %04X...\n", TARGET_VID, TARGET_PID);
    int found = 0;
    libusb_device *gp_dev;
    for (ssize_t i = 0; i < count; ++i) {
        libusb_device *device = list[i];
        struct libusb_device_descriptor desc;

        if (libusb_get_device_descriptor(device, &desc) == 0) {
            if (desc.idVendor == TARGET_VID && desc.idProduct == TARGET_PID) {
                printf("Device found! Opening...\n");
                r = libusb_open(device, &handle);
                if (r == 0 && handle != NULL) {
                    printf("Device opened successfully!\n");
                    gp_dev = device;
                    found = 1;
                    break;
                } else {
                    fprintf(stderr, "Failed to open device: %s\n", libusb_error_name(r));
                    break;
                }
            }
        }
    }

    if (!found) {
        fprintf(stderr, "Target device not found (VID: %04X, PID: %04X)\n", TARGET_VID, TARGET_PID);
    }
    else
    {
        //find interface
        struct libusb_config_descriptor *config;
        libusb_get_active_config_descriptor(gp_dev, &config);

        int foundInterface = -1;

        for (int i = 0; i < config->bNumInterfaces; i++) {
            const struct libusb_interface *interface = &config->interface[i];
            for (int j = 0; j < interface->num_altsetting; j++) {
                const struct libusb_interface_descriptor *desc = &interface->altsetting[j];
                if (desc->bInterfaceClass == LIBUSB_CLASS_HID) {
                    foundInterface = desc->bInterfaceNumber;
                    break;
                }
            }
            if (foundInterface != -1) break;
        }

        libusb_free_config_descriptor(config);

        if (foundInterface != -1) {
            printf("Found HID interface: %d\n", foundInterface);
            // Use foundInterface in libusb_kernel_driver_active(), claim, etc.
        } else {
            fprintf(stderr, "No HID interface found.\n");
        }

        // Detach kernel driver if needed
        if (libusb_kernel_driver_active(handle, 3) == 1) {
            printf("Kernel driver active on interface 3, detaching...\n");
            int detachResult = libusb_detach_kernel_driver(handle, 3);
            if (detachResult != 0) {
                fprintf(stderr, "Failed to detach kernel driver: %s\n", libusb_error_name(detachResult));
                libusb_close(handle);
                libusb_exit(ctx);
                return 1;
            }
        }


        // Claim HID interface (interface 3)
        int r = libusb_claim_interface(handle, 3);
        if (r < 0) {
            fprintf(stderr, "Failed to claim interface 3: %s\n", libusb_error_name(r));
            libusb_close(handle);
            libusb_exit(ctx);
            return 1;
        }

        // Read from interrupt IN endpoint (0x84)
        unsigned char data[PACKET_SIZE];
        int actual_length = 0;
        r = libusb_interrupt_transfer(handle, 0x84, data, sizeof(data), &actual_length, 1000);
        if (r == 0) {
            printf("Read %d bytes:\n", actual_length);
            for (int i = 0; i < actual_length; i++) {
                printf("%02X ", data[i]);
            }
            printf("\n");
            ParseDualSenseInput(data, PACKET_SIZE);
        } else {
            printf("Read error or timeout: %s\n", libusb_error_name(r));
        }
        
    }

    if (handle) libusb_close(handle);
    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return 0;
}


// int main() {
//     libusb_context *ctx = NULL;
//     libusb_device **devs;
//     ssize_t cnt;
//     int r;

//     r = libusb_init(&ctx);
//     if (r < 0) return r;

//     cnt = libusb_get_device_list(ctx, &devs);
//     if (cnt < 0) return (int) cnt;

//     printf("Found %ld USB devices:\n", cnt);

//     for (ssize_t i = 0; i < cnt; i++) {
//         libusb_device *dev = devs[i];
//         struct libusb_device_descriptor desc;

//         libusb_get_device_descriptor(dev, &desc);

//         printf("Device %zu: VID: %04X, PID: %04X\n", i, desc.idVendor, desc.idProduct);

//         libusb_device_handle *handle;
//         if (libusb_open(dev, &handle) == 0) {
//             unsigned char buffer[256];

//             // Try to read the manufacturer string
//             if (desc.iManufacturer) {
//                 libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, buffer, sizeof(buffer));
//                 printf("  Manufacturer: %s\n", buffer);
//             }
//             if (desc.iProduct) {
//                 libusb_get_string_descriptor_ascii(handle, desc.iProduct, buffer, sizeof(buffer));
//                 printf("  Product     : %s\n", buffer);
//             }

//             libusb_close(handle);
//         }
//     }

//     libusb_free_device_list(devs, 1);
//     libusb_exit(ctx);
//     return 0;
// }
