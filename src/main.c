/**
 * @file main.c
 * @brief USB host application configured for STM32 Temperature HID device.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
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

enum commands
{
    CMD_HELP = 0,
    CMD_CONNECT_DEVICE,
    CMD_DEFAULT_DEVICE,
    CMD_DISPLAY_TEXT,
    CMD_DISPLAY_TEMPERATURE,
    CMD_GET_TEMPERATURE,
	CMD_TRANSCEIVE,
    CMD_EXIT
};



// Now accepts pointers to persist the device state across calls
void command_interface(enum commands cmd, libusb_context *ctx, libusb_device_handle **USB_Device)
{
    int width_begin = 10;
    int width_end = 15;
    unsigned int tmp; // Изменено на unsigned int
    unsigned int VID; // Изменено на unsigned int
    unsigned int PID; // Изменено на unsigned int
    unsigned char tx_buffer[REPORT_SIZE];
    unsigned char rx_buffer[REPORT_SIZE];
    int result;
    int transferred = 0;

    switch (cmd)
    {
    case CMD_HELP:
        printf("%*s\n", width_begin, "Available commands:");
        printf("%*d - Help\n", width_end, CMD_HELP);
        printf("%*d - Connect device\n", width_end, CMD_CONNECT_DEVICE);
        printf("%*d - Connect default device\n", width_end, CMD_DEFAULT_DEVICE);
        printf("%*d - Display text\n", width_end, CMD_DISPLAY_TEXT);
        printf("%*d - Display temperature\n", width_end, CMD_DISPLAY_TEMPERATURE);
        printf("%*d - Get temperature\n", width_end, CMD_GET_TEMPERATURE);
        printf("%*d - Transceive data\n", width_end, CMD_TRANSCEIVE);
        printf("%*d - Exit program\n", width_end, CMD_EXIT);
        break;

    case CMD_DEFAULT_DEVICE:
        VID = DEVICE_VID;
        PID = DEVICE_PID;
        
        // Open device and save into main's pointer
        *USB_Device = libusb_open_device_with_vid_pid(ctx, VID, PID);
        if (!(*USB_Device)) {
            fprintf(stderr, "Error: Device VID 0x%04X PID 0x%04X not found.\n", VID, PID);
            return;
        }
        printf("STM32 Temperature device opened successfully.\n");
        
        result = libusb_claim_interface(*USB_Device, INT_INTERFACE);
        if (result < 0) {
            fprintf(stderr, "Error: Cannot claim interface %d (%d)\n", INT_INTERFACE, result);
            libusb_close(*USB_Device);
            *USB_Device = NULL;
            return;
        }
        printf("Interface %d claimed successfully.\n", INT_INTERFACE);
        break; // Fixed missing break

    case CMD_CONNECT_DEVICE:
        printf("%*s write DEVICE_VID (in Hex, e.g. 1155 for 0x0483): ", width_begin, "");
        if (scanf("%x", &tmp) != 1) return; // Read as hex
        VID = tmp;
        
        printf("%*s DEVICE_PID (in Hex, e.g. 22352 for 0x5750): ", width_begin, "");
        if (scanf("%x", &tmp) != 1) return; // Read as hex
        PID = tmp;
        
        *USB_Device = libusb_open_device_with_vid_pid(ctx, VID, PID);
        if (!(*USB_Device)) {
            fprintf(stderr, "Error: Device VID 0x%04X PID 0x%04X not found.\n", VID, PID);
            return;
        }
        printf("Device opened successfully.\n");
        
        result = libusb_claim_interface(*USB_Device, INT_INTERFACE);
        if (result < 0) {
            fprintf(stderr, "Error: Cannot claim interface %d (%d)\n", INT_INTERFACE, result);
            libusb_close(*USB_Device);
            *USB_Device = NULL;
            return;
        }
        printf("Interface %d claimed successfully.\n", INT_INTERFACE);
        break;

    case CMD_DISPLAY_TEXT:
        { // Added brackets to isolate local variables inside case block
            if (!(*USB_Device)) {
                fprintf(stderr, "Error: No device connected. Connect a device first.\n");
                break;
            }

            char user_input[64];
            transferred = 0;

            memset(tx_buffer, 0, REPORT_SIZE);
            tx_buffer[0] = REPORT_ID_OUT; 
            tx_buffer[1] = CMD_DISPLAY_TEXT; 

            printf("%*sEnter text to send (Max 64 characters): ", width_begin, "");
            
            // Clear input buffer safely before taking string input
            int c;
            while ((c = getchar()) != '\n' && c != EOF);

            if (fgets(user_input, sizeof(user_input), stdin) != NULL) {
                user_input[strcspn(user_input, "\n")] = '\0';

                // Safe strncpy limit to prevent buffer overflow
                strncpy((char*)&tx_buffer[2], user_input, REPORT_SIZE - 2);
                
                printf("Sending command packet...\n");
                result = libusb_interrupt_transfer(*USB_Device, EP_OUT, tx_buffer, REPORT_SIZE, &transferred, 1000);
                if (result < 0) {
                    fprintf(stderr, "Error: Data transmission failed (%d)\n", result);
                } else {
                    printf("Sent %d bytes successfully.\n", transferred);
                }
            }
            printf("Displaying text data...\n");
        }
        break;
    case CMD_DISPLAY_TEMPERATURE:
        { // Added brackets to isolate local variables inside case block
            if (!(*USB_Device)) {
                fprintf(stderr, "Error: No device connected. Connect a device first.\n");
                break;
            }

            transferred = 0;

            memset(tx_buffer, 0, REPORT_SIZE);
            tx_buffer[0] = REPORT_ID_OUT; 
            tx_buffer[1] = CMD_DISPLAY_TEMPERATURE;

            printf("%*sEnter text to send (Max 4 characters): ", width_begin, "");
            
            // Clear input buffer safely before taking string input
                
            printf("Sending command packet...\n");
            result = libusb_interrupt_transfer(*USB_Device, EP_OUT, tx_buffer, REPORT_SIZE, &transferred, 1000);
            if (result < 0) {
                fprintf(stderr, "Error: Data transmission failed (%d)\n", result);
            } else {
                printf("Sent %d bytes successfully.\n", transferred);
            }
           
            printf("Displaying temperature data...\n");
        }
        break;
    case CMD_TRANSCEIVE:
        if (!(*USB_Device)) {
            fprintf(stderr, "Error: No device connected. Connect a device first.\n");
            break;
        }

        memset(tx_buffer, 0, REPORT_SIZE);
        tx_buffer[0] = REPORT_ID_OUT; 
        tx_buffer[1] = CMD_TRANSCEIVE; 
        
        memset(rx_buffer, 0, REPORT_SIZE);
        transferred = 0;

        printf("Transceiving data...\n");
        result = libusb_interrupt_transfer(*USB_Device, EP_OUT, tx_buffer, REPORT_SIZE, &transferred, 1000);
        
        if (result < 0) {
            fprintf(stderr, "Error: Data transmission failed (%d)\n", result);
            break;
        } else {
            printf("Sent %d bytes successfully.\n", transferred);
        }

        result = libusb_interrupt_transfer(*USB_Device, EP_IN, rx_buffer, REPORT_SIZE, &transferred, 3000);
        
        if (result < 0) {
            fprintf(stderr, "Error: Data reception failed (%d)\n", result);
        } else {
            printf("Received %d bytes from target.\n", transferred);
            printf("Raw Payload: [0x%02X, 0x%02X, 0x%02X, 0x%02X]\n", 
                    rx_buffer[1], rx_buffer[2], rx_buffer[3], rx_buffer[4]);
        }
        
        float temp = (float)rx_buffer[2] + (float)rx_buffer[3] / 100.0f;
        printf("Temperature data: %.2f C\n", temp);
        break;
    
    case CMD_GET_TEMPERATURE:
        if (!(*USB_Device)) {
            fprintf(stderr, "Error: No device connected. Connect a device first.\n");
            break;
        }

        memset(tx_buffer, 0, REPORT_SIZE);
        tx_buffer[0] = REPORT_ID_OUT; 
        tx_buffer[1] = CMD_GET_TEMPERATURE; // Updated to match correct state
        
        memset(rx_buffer, 0, REPORT_SIZE);
        transferred = 0;

        printf("Waiting for temperature data...\n");
        result = libusb_interrupt_transfer(*USB_Device, EP_IN, rx_buffer, REPORT_SIZE, &transferred, 3000);
        
        if (result < 0) {
            fprintf(stderr, "Error: Data reception failed (%d)\n", result);
        } else {
            printf("Received %d bytes from target.\n", transferred);
            
            if (rx_buffer[0] == REPORT_ID_IN) {
                printf("Valid HID Report ID received (0x02).\n");
                printf("Raw Payload: [0x%02X, 0x%02X, 0x%02X, 0x%02X]\n", 
                        rx_buffer[1], rx_buffer[2], rx_buffer[3], rx_buffer[4]);

                float temp = (float)rx_buffer[2] + (float)rx_buffer[3] / 100.0f;
                printf("Temperature data: %.2f C\n", temp);
            } else {
                fprintf(stderr, "Warning: Unexpected Report ID received: 0x%02X\n", rx_buffer[0]);
            }
        }
        break;

    case CMD_EXIT:
        printf("%*s\n", width_begin, "Exiting program...");
        break;

    default:
        printf("Unknown command - try command help\n");
        break;
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    libusb_context *ctx = NULL;
    libusb_device_handle *USB_Device = NULL;
    int result;
    bool start_flag = true;
    int exit_flag = 0;

    // Initialize the libusb library
    result = libusb_init(&ctx);
    if (result < 0) {
        fprintf(stderr, "Error: Failed to initialize libusb (%d)\n", result);
        return 1;
    }
    printf("Libusb initialized successfully.\n");

    while (exit_flag == 0)
    {
        if (start_flag)
        {
            printf("----------------------Start Menu------------------------\n");
            start_flag = false;
            command_interface(CMD_HELP, ctx, &USB_Device); // Removed ghost variables
        }
        
        printf("\nEnter command number: ");
        int cmd;
        if (scanf("%d", &cmd) == 0) {
            printf("Invalid input. Please enter a valid command number.\n");
            while (getchar() != '\n'); // Clear input buffer
            continue;
        }
        
        if (cmd < CMD_HELP || cmd > CMD_EXIT) // Replaced unknown CMD_LOAD_READY with CMD_EXIT
        {
            printf("Unknown command - try command help\n");
            continue;
        } 
        else if (cmd == CMD_EXIT)
        {
            exit_flag = 1;
            printf("Exiting program...\n");
            
            // Resource cleanup safely handles connection state
            if (USB_Device) {
                libusb_release_interface(USB_Device, INT_INTERFACE);
                libusb_close(USB_Device);
            }
            libusb_exit(ctx);
            break;
        } 
        else 
        {
            command_interface(cmd, ctx, &USB_Device); // Removed ghost variables
        }
    }

    printf("Session closed. Exiting.\n");
    return 0;
}