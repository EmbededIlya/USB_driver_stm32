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


//////////////////////////////////////////
enum commands
{
    CMD_HELP = 0,
    CMD_CONNECT_DEVICE,
    CMD_DEFAULT_DEVICE,
    CMD_DISPLAY_TEXT,
    CMD_DISPLAY_TEMPERATURE,
    CMD_EXIT
};


void command_interface(enum commands cmd, libusb_context *ctx, libusb_device_handle *USB_Device)
{
    // Реализация командного интерфейса
    int width_begin = 10;
    int width_end = 15;
    char buff[100];
    int tmp;
    int VID;
    int PID;
    unsigned char tx_buffer[REPORT_SIZE];
    unsigned char rx_buffer[REPORT_SIZE];

    int result;
    int transferred = 0;
    switch (cmd)
    {
    case CMD_HELP:
        printf("%*s\n", width_begin, "Available commands:");
        printf("%*d - Help\n", width_end, CMD_HELP);
        printf("%*d - Connect default device\n", width_end, CMD_DEFAULT_DEVICE);
        printf("%*d - Connect device\n", width_end, CMD_CONNECT_DEVICE);
        printf("%*d - Display text\n", width_end, CMD_DISPLAY_TEXT);
        printf("%*d - Display temperature\n", width_end, CMD_DISPLAY_TEMPERATURE);
        printf("%*d - Exit program\n", width_end, CMD_EXIT);
        break;
    case CMD_DEFAULT_DEVICE:
        VID = DEVICE_VID;
        PID = DEVICE_PID;
        // 2. Open the specific STM32 Temperature device
        USB_Device = libusb_open_device_with_vid_pid(ctx, VID, PID);
        if (!USB_Device) {
            fprintf(stderr, "Error: Device VID 0x%04X PID 0x%04X not found.\n", DEVICE_VID, DEVICE_PID);
            fprintf(stderr, "Check if the device is connected or if Zadig driver is installed.\n");
            libusb_exit(ctx);
            return 1;
        }
        printf("STM32 Temperature device opened successfully.\n");
         result = libusb_claim_interface(USB_Device, INT_INTERFACE);
        if (result < 0) {
            fprintf(stderr, "Error: Cannot claim interface %d (%d)\n", INT_INTERFACE, result);
            libusb_close(USB_Device);
            libusb_exit(ctx);
            return 1;
        }
        printf("Interface %d claimed successfully.\n", INT_INTERFACE);
        break;
    case CMD_CONNECT_DEVICE:
        printf("%*s\n", width_begin, "write DEVICE_VID:");
        scanf("%d", &tmp);
        VID = tmp;
        printf("%*s\n", width_begin, "DEVICE_PID:");
        scanf("%d", &tmp);
        PID = tmp;
        // 2. Open the specific STM32 Temperature device
        USB_Device = libusb_open_device_with_vid_pid(ctx, VID, PID);
        if (!USB_Device) {
            fprintf(stderr, "Error: Device VID 0x%04X PID 0x%04X not found.\n", DEVICE_VID, DEVICE_PID);
            fprintf(stderr, "Check if the device is connected or if Zadig driver is installed.\n");
            libusb_exit(ctx);
            return 1;
        }
        printf("Device opened successfully.\n");
        // 3. Claim interface 0
        result = libusb_claim_interface(USB_Device, INT_INTERFACE);
        if (result < 0) {
            fprintf(stderr, "Error: Cannot claim interface %d (%d)\n", INT_INTERFACE, result);
            libusb_close(USB_Device);
            libusb_exit(ctx);
            return 1;
        }
        printf("Interface %d claimed successfully.\n", INT_INTERFACE);
        break;
    case CMD_DISPLAY_TEXT:
       // =================================================================
        // 4. DATA TRANSMISSION (Interrupt OUT) - String Input Method
        // =================================================================
        char user_input[64];
        transferred = 0;

        // Clean the tx_buffer and set Report ID
        memset(tx_buffer, 0, REPORT_SIZE);
        tx_buffer[0] = REPORT_ID_OUT; 
        tx_buffer[1] = CMD_DISPLAY_TEXT; 

        printf("%*sEnter text to send (Max 4 characters): ", width_begin, "");
        
        // Clear standard input buffer and read string
        fflush(stdin);
        if (fgets(user_input, sizeof(user_input), stdin) != NULL) {
            // Strip the trailing newline character added by fgets
            user_input[strcspn(user_input, "\n")] = '\0';

            // Safely copy up to 4 characters into tx_buffer starting from index 1
            strncpy((char*)&tx_buffer[2], user_input, REPORT_SIZE - 1);
            
            printf("Sending command packet...\n");
            // Send exactly 5 bytes
            result = libusb_interrupt_transfer(USB_Device, EP_OUT, tx_buffer, REPORT_SIZE, &transferred, 1000);
            if (result < 0) {
                fprintf(stderr, "Error: Data transmission failed (%d)\n", result);
            } else {
                printf("Sent %d bytes successfully.\n", transferred);
            }
        }
        printf("Displaying text data...\n");
        transferred = 0;
        break;
    case CMD_DISPLAY_TEMPERATURE:
       // =================================================================
        // 5. DATA RECEPTION (Interrupt IN)
        // =================================================================
        // (char user_input[64] removed if not used here to avoid warnings)

        // Clean the tx_buffer and set Report ID (if preparing for next steps)
        memset(tx_buffer, 0, REPORT_SIZE);
        tx_buffer[0] = REPORT_ID_OUT; 
        tx_buffer[1] = CMD_DISPLAY_TEXT; 
        
        memset(rx_buffer, 0, REPORT_SIZE);
        transferred = 0;

        printf("Waiting for temperature data...\n");
        // Reading 5 bytes from endpoint 0x81
        result = libusb_interrupt_transfer(USB_Device, EP_IN, rx_buffer, REPORT_SIZE, &transferred, 3000);
        
        if (result < 0) {
            fprintf(stderr, "Error: Data reception failed (%d)\n", result);
        } else {
            printf("Received %d bytes from target.\n", transferred);
            
            // FIX 3: Correct logical condition without duplication
            if (rx_buffer[0] == REPORT_ID_IN) {
                printf("Valid HID Report ID received (0x02).\n");
                
                printf("Raw Payload: [0x%02X, 0x%02X, 0x%02X, 0x%02X]\n", 
                        rx_buffer[1], rx_buffer[2], rx_buffer[3], rx_buffer[4]);

                // FIX 1 & 2: Moved calculation inside the valid block and fixed standard C casts
                // FIX 4: Changed division to 100.0f (adjust to 10.0f or 100.0f depending on your STM32 logic)
                float temp = (float)rx_buffer[2] + (float)rx_buffer[3] / 100.0f;
                printf("Temperature data: %.2f C\n", temp);
                
            } else {
                fprintf(stderr, "Warning: Unexpected Report ID received: 0x%02X\n", rx_buffer[0]);
            }
        }
        
        transferred = 0;
        break;
    case CMD_EXIT:
        // Выход из программы
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
    int transferred = 0;
    bool start_flag = 1;
    int exit_flag = 0;

    // 1. Initialize the libusb library
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
            command_interface(CMD_HELP, ctx, USB_Device);
        }
        printf("Enter command number: ");
        int cmd;
        if(scanf("%d", &cmd) == 0){
            printf("Invalid input. Please enter a valid command number.\n");
            while (getchar() != '\n'); // Clear input buffer
            continue;
        }
        else if (cmd < CMD_HELP || cmd > CMD_LOAD_READY)
        {
            printf("Unknown command - try command help\n");
            continue;
        }else if (cmd == CMD_EXIT)
        {
            exit_flag = 1;
            printf("Exiting program...\n");
            // =================================================================
            // 6. RESOURCE CLEANUP
            // =================================================================
            libusb_release_interface(USB_Device, INT_INTERFACE);
            libusb_close(USB_Device);
            libusb_exit(ctx);
            break;
        }else{
            command_interface(cmd, ctx, USB_Device);
        }
    }

    printf("Session closed. Exiting.\n");
    return 0;
}