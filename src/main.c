/**
 * @file main.c
 * @brief USB host application configured for STM32 Temperature HID device.
 */

#include <stdio.h>
#include <string.h>
#include <libusb.h>
#include "main.h"

// Hardware constants from device dump
#define DEVICE_VID       0x0483
#define DEVICE_PID       0x5750
#define INT_INTERFACE    0

// Endpoints (Standard Custom HID layout)
#define EP_OUT           0x01
#define EP_IN            0x81

// HID Report configurations from data dump
#define REPORT_SIZE      65
#define REPORT_ID_OUT    0x01
#define REPORT_ID_IN     0x02

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    libusb_context *ctx = NULL;
    libusb_device_handle *USB_Device = NULL;
    int result;
    int transferred = 0;

    // Buffers must match exactly the report size from capabilities dump
    unsigned char tx_buffer[REPORT_SIZE];
    unsigned char rx_buffer[REPORT_SIZE];

    // 1. Initialize the libusb library
    result = libusb_init(&ctx);
    if (result < 0) {
        fprintf(stderr, "Error: Failed to initialize libusb (%d)\n", result);
        return 1;
    }
    printf("Libusb initialized successfully.\n");

    // 2. Open the specific STM32 Temperature device
    USB_Device = libusb_open_device_with_vid_pid(ctx, DEVICE_VID, DEVICE_PID);
    if (!USB_Device) {
        fprintf(stderr, "Error: Device VID 0x%04X PID 0x%04X not found.\n", DEVICE_VID, DEVICE_PID);
        fprintf(stderr, "Check if the device is connected or if Zadig driver is installed.\n");
        libusb_exit(ctx);
        return 1;
    }
    printf("STM32 Temperature device opened successfully.\n");

    // 3. Claim interface 0
    result = libusb_claim_interface(USB_Device, INT_INTERFACE);
    if (result < 0) {
        fprintf(stderr, "Error: Cannot claim interface %d (%d)\n", INT_INTERFACE, result);
        libusb_close(USB_Device);
        libusb_exit(ctx);
        return 1;
    }
    printf("Interface %d claimed successfully.\n", INT_INTERFACE);

    // =================================================================
    // 4. DATA TRANSMISSION (Interrupt OUT)
    // =================================================================
    // Byte 0 MUST be the Output Report ID (0x01) specified in OutputCaps
    tx_buffer[0] = REPORT_ID_OUT; 
    tx_buffer[1] = 'S'; // Example command payload to request data
    tx_buffer[2] = 'E';
    tx_buffer[3] = 'T';
    tx_buffer[4] = '\0';

    printf("Sending command packet...\n");
    result = libusb_interrupt_transfer(USB_Device, EP_OUT, tx_buffer, REPORT_SIZE, &transferred, 1000);
    if (result < 0) {
        fprintf(stderr, "Error: Data transmission failed (%d)\n", result);
    } else {
        printf("Sent %d bytes successfully.\n", transferred);
    }

    // =================================================================
    // 5. DATA RECEPTION (Interrupt IN)
    // =================================================================
    memset(rx_buffer, 0, REPORT_SIZE);
    transferred = 0;

    printf("Waiting for temperature data from STM32...\n");
    // Reading 5 bytes from endpoint 0x81
    result = libusb_interrupt_transfer(USB_Device, EP_IN, rx_buffer, REPORT_SIZE, &transferred, 3000);
    if (result < 0) {
        fprintf(stderr, "Error: Data reception failed (%d)\n", result);
    } else {
        printf("Received %d bytes from target.\n", transferred);
        
        // Validate that Byte 0 contains the Input Report ID (0x02)
        if (rx_buffer[0] == REPORT_ID_IN) {
            printf("Valid HID Report ID received (0x02).\n");
            // Raw bytes processing depending on how STM32 encodes temperature
            printf("Raw Payload: [0x%02X, 0x%02X, 0x%02X, 0x%02X]\n", 
                   rx_buffer[1], rx_buffer[2], rx_buffer[3], rx_buffer[4]);
        } else {
            fprintf(stderr, "Warning: Unexpected Report ID received: 0x%02X\n", rx_buffer[0]);
        }
    }

    // =================================================================
    // 6. RESOURCE CLEANUP
    // =================================================================
    libusb_release_interface(USB_Device, INT_INTERFACE);
    libusb_close(USB_Device);
    libusb_exit(ctx);

    printf("Session closed. Exiting.\n");
    return 0;
}