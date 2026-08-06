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

typedef enum {
    CMD_HELP = 0,
    CMD_CONNECT_DEVICE,
    CMD_DEFAULT_DEVICE,
    CMD_DEBUG,
    CMD_EXIT_DEBUG,
    CMD_SET_CURRENT,
    CMD_GET_CURRENT,
    CMD_GET_PARAMETERS,
    CMD_GET_GRAPH,
	CMD_SET_TIME_MEASURING,
    CMD_SET_VOLTAGE_CORRECTION,
    CMD_EXIT
} commands;

typedef struct i_src_param{
    int32_t adc_raw;
    int32_t voltage;
    int32_t voltage_corrected;
    int32_t voltage_correction;
    int32_t i_current_mA;
    uint32_t i_lim;
    uint32_t i;
    uint32_t Vdac;
} i_src_param;


//Global param
i_src_param* graph_param = NULL;
unsigned char tx_buffer[REPORT_SIZE];
unsigned char rx_buffer[REPORT_SIZE];

int width_begin = 10;
int width_end = 15;
unsigned int tmp; // Изменено на unsigned int

/*-----------------------------------------------------------------------Functions---------------------------------------------------------*/

void write_uint32_to_buff(uint8_t* p_src, uint32_t value) {
    // Безопасно для Cortex-M0/M3/M4, не нарушает MISRA C
    memcpy(p_src, &value, sizeof(value));
}

uint32_t read_uint32_from_buff(const uint8_t* p_src) {
    uint32_t value;
    memcpy(&value, p_src, sizeof(value));
    return value;
}

void print_data(i_src_param* param) {
    printf("ADC Raw: %d\n", param->adc_raw);
    printf("Voltage: %d\n", param->voltage);
    printf("Voltage Corrected: %d\n", param->voltage_corrected);
    printf("Current (mA): %d\n", param->i_current_mA);
    printf("Current Limit: %u\n", param->i_lim);
    printf("Current setted: %u\n", param->i);
    printf("Vdac: %u\n", param->Vdac);
}

i_src_param* get_param(i_src_param* param, libusb_device_handle **USB_Device) {
            int result;
            int transferred = 0;
            if (!(*USB_Device)) {
                fprintf(stderr, "Error: No device connected. Connect a device first.\n");
                return NULL;
            }

            memset(rx_buffer, 0, REPORT_SIZE);
            transferred = 0;

            memset(tx_buffer, 0, REPORT_SIZE);
            tx_buffer[0] = REPORT_ID_OUT; 
            tx_buffer[1] = CMD_GET_PARAMETERS; 


            printf("Getting parameters...\n");
            result = libusb_interrupt_transfer(*USB_Device, EP_OUT, tx_buffer, REPORT_SIZE, &transferred, 1000);
            
            if (result < 0) {
                fprintf(stderr, "Error: Data transmission failed (%d)\n", result);
                return NULL;
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
            
            param->adc_raw = read_uint32_from_buff(&rx_buffer[2]);
            param->voltage = read_uint32_from_buff(&rx_buffer[6]);
            param->voltage_corrected = read_uint32_from_buff(&rx_buffer[10]);
            param->i_current_mA = read_uint32_from_buff(&rx_buffer[14]);
            param->i_lim = read_uint32_from_buff(&rx_buffer[18]);
            param->i = read_uint32_from_buff(&rx_buffer[22]);
            param->Vdac = read_uint32_from_buff(&rx_buffer[26]);
            param->voltage_correction = read_uint32_from_buff(&rx_buffer[30]);
    return param;
}

i_src_param* set_current(i_src_param* param, libusb_device_handle **USB_Device) {
            int result;
            int transferred = 0;
            if (!(*USB_Device)) {
                fprintf(stderr, "Error: No device connected. Connect a device first.\n");
                return NULL;
            }

            memset(rx_buffer, 0, REPORT_SIZE);
            transferred = 0;

            memset(tx_buffer, 0, REPORT_SIZE);
            tx_buffer[0] = REPORT_ID_OUT; 
            tx_buffer[1] = CMD_SET_CURRENT; 
            write_uint32_to_buff(&tx_buffer[2], (uint32_t)param->i); // Assuming param->i is the current value to set
            printf("Setting current...\n");
            result = libusb_interrupt_transfer(*USB_Device, EP_OUT, tx_buffer, REPORT_SIZE, &transferred, 1000);
            
            if (result < 0) {
                fprintf(stderr, "Error: Data transmission failed (%d)\n", result);
                return NULL;
            } else {
                printf("Sent %d bytes successfully.\n", transferred);
            }

            result = libusb_interrupt_transfer(*USB_Device, EP_IN, rx_buffer, REPORT_SIZE, &transferred, 3000);
            
            if (result < 0) {
                fprintf(stderr, "Error: Data reception failed (%d)\n", result);
                return NULL;
            } else {
                printf("Received %d bytes from target.\n", transferred);
                printf("Raw Payload: [0x%02X, 0x%02X, 0x%02X, 0x%02X]\n", 
                        rx_buffer[1], rx_buffer[2], rx_buffer[3], rx_buffer[4]);
            }
            
            param->i = read_uint32_from_buff(&rx_buffer[2]);
            printf("Current data: %d mA\n", param->i);// Added brackets to isolate local variables inside case block
    return param;
}


void help() {
        printf("%*s\n", width_begin, "Available commands:");
        printf("%*d - Help\n", width_end, CMD_HELP);
        printf("%*d - Connect device\n", width_end, CMD_CONNECT_DEVICE);
        printf("%*d - Connect default device\n", width_end, CMD_DEFAULT_DEVICE);
        printf("%*d - Debug mode\n", width_end, CMD_DEBUG);
        printf("%*d - Exit debug mode\n", width_end, CMD_EXIT_DEBUG);
        printf("%*d - Get current\n", width_end, CMD_GET_CURRENT);
        printf("%*d - Set current\n", width_end, CMD_SET_CURRENT);
        printf("%*d - Get parameters\n", width_end, CMD_GET_PARAMETERS);
        printf("%*d - Get graph\n", width_end, CMD_GET_GRAPH);
        printf("%*d - Set measuring time\n", width_end, CMD_SET_TIME_MEASURING);
        printf("%*d - Set voltage correction\n", width_end, CMD_SET_VOLTAGE_CORRECTION);
        printf("%*d - Exit program\n", width_end, CMD_EXIT);
}

int32_t connect_device(libusb_context *ctx, libusb_device_handle **USB_Device, unsigned int VID, unsigned int PID) {
    *USB_Device = libusb_open_device_with_vid_pid(ctx, VID, PID);
    if (!(*USB_Device)) {
        fprintf(stderr, "Error: Device VID 0x%04X PID 0x%04X not found.\n", VID, PID);
        return -1;
    }
    printf("Device opened successfully.\n");
    
    int result = libusb_claim_interface(*USB_Device, INT_INTERFACE);
    if (result < 0) {
        fprintf(stderr, "Error: Cannot claim interface %d (%d)\n", INT_INTERFACE, result);
        libusb_close(*USB_Device);
        *USB_Device = NULL;
        return -1;
    }
    printf("Interface %d claimed successfully.\n", INT_INTERFACE);
    return 0;
}

int32_t get_graph(libusb_device_handle **USB_Device, i_src_param* graph_param, i_src_param* param) {
        int steps;
        int min_current;
        int max_current;
        int amount_of_data;
        i_src_param* result_param;
        if (!(*USB_Device)) {
            fprintf(stderr, "Error: No device connected. Connect a device first.\n");
            return -1;
        }
        printf("%*s write minimal current (in mA): ", width_begin, "");
        if (scanf("%u", &tmp) != 1) return -1; // Read as hex
        min_current = tmp;
        printf("%*s write maximum current (in mA): ", width_begin, "");
        if (scanf("%u", &tmp) != 1) return -1; // Read as hex
        max_current = tmp;
        printf("%*s write number of steps: ", width_begin, "");
        if (scanf("%u", &tmp) != 1) return -1; // Read as hex
        steps = tmp;
        amount_of_data = steps + 1; // Including both min and max

        graph_param = calloc(amount_of_data, sizeof(i_src_param));
        if (!graph_param) {
            fprintf(stderr, "Error: Memory allocation failed for graph_param.\n");
            return -1;
        }

        printf("Getting graph data...\n");

        for (int i = 0; i < 100; i++) {
            param->i = min_current + i * (max_current - min_current) / (steps - 1);
            set_current(param, USB_Device);
            result_param = get_param(&graph_param[i], USB_Device);
            if (result_param == NULL) {
                fprintf(stderr, "Error: Failed to retrieve graph data at step %d.\n", i);
                free(graph_param);
                graph_param = NULL;
                return -1;
            }
        }
        printf("Graph data retrieved successfully.\n");

        if (graph_param != NULL) {
            printf("Graph data retrieved successfully.\n");
            // После обработки данных графика обязательно освобождаем память:
            free(graph_param);
            graph_param = NULL;
        }
    
    return 0;
}

int32_t transmit_unar_op( libusb_device_handle **USB_Device, unsigned char command) {
        int result;
        int transferred = 0;
        if (!(*USB_Device)) {
            fprintf(stderr, "Error: No device connected. Connect a device first.\n");
            return -1;
        }
        memset(tx_buffer, 0, REPORT_SIZE);
        tx_buffer[0] = REPORT_ID_OUT; 
        tx_buffer[1] = command;
        memset(rx_buffer, 0, REPORT_SIZE);
        transferred = 0;

        printf("Activating debug mode...\n");
        result = libusb_interrupt_transfer(*USB_Device, EP_OUT, tx_buffer, REPORT_SIZE, &transferred, 1000);
        
        if (result < 0) {
            fprintf(stderr, "Error: Data transmission failed (%d)\n", result);
            return -1;
        } else {
            printf("Sent %d bytes successfully.\n", transferred);
        }

        printf("%*s\n", width_begin, "Debug mode activated.");
        return 0;
}

int32_t set_measuring_time(libusb_device_handle **USB_Device, unsigned int time_ms) {
        int result;
        int confirmed_time;
        int transferred = 0;
        if (!(*USB_Device)) {
            fprintf(stderr, "Error: No device connected. Connect a device first.\n");
            return -1;
        }
        memset(tx_buffer, 0, REPORT_SIZE);
        tx_buffer[0] = REPORT_ID_OUT; 
        tx_buffer[1] = CMD_SET_TIME_MEASURING; 
        write_uint32_to_buff(&tx_buffer[2], time_ms); // Assuming time_ms is the measuring time to set
        memset(rx_buffer, 0, REPORT_SIZE);
        transferred = 0;

        printf("Setting measuring time...\n");
        result = libusb_interrupt_transfer(*USB_Device, EP_OUT, tx_buffer, REPORT_SIZE, &transferred, 1000);
        
        if (result < 0) {
            fprintf(stderr, "Error: Data transmission failed (%d)\n", result);
            return -1;
        } else {
            printf("Sent %d bytes successfully.\n", transferred);
        }

        result = libusb_interrupt_transfer(*USB_Device, EP_IN, rx_buffer, REPORT_SIZE, &transferred, 3000);
        
        if (result < 0) {
            fprintf(stderr, "Error: Data reception failed (%d)\n", result);
            return -1;
        } else {
            printf("Received %d bytes from target.\n", transferred);
            printf("Raw Payload: [0x%02X, 0x%02X, 0x%02X, 0x%02X]\n", 
                    rx_buffer[1], rx_buffer[2], rx_buffer[3], rx_buffer[4]);
        }
        
        confirmed_time = read_uint32_from_buff(&rx_buffer[30]);
        printf("Measuring time: %d ms\n", confirmed_time);
    return confirmed_time;
}

int32_t set_voltage_correction(libusb_device_handle **USB_Device, unsigned int voltage_correction) {
        int result;
        int confirmed_voltage_correction;
        int transferred = 0;
        if (!(*USB_Device)) {
            fprintf(stderr, "Error: No device connected. Connect a device first.\n");
            return -1;
        }
        memset(tx_buffer, 0, REPORT_SIZE);
        tx_buffer[0] = REPORT_ID_OUT; 
        tx_buffer[1] = CMD_SET_VOLTAGE_CORRECTION; 
        write_uint32_to_buff(&tx_buffer[2], voltage_correction);
        memset(rx_buffer, 0, REPORT_SIZE);
        transferred = 0;

        printf("Setting voltage correction...\n");
        result = libusb_interrupt_transfer(*USB_Device, EP_OUT, tx_buffer, REPORT_SIZE, &transferred, 1000);
        
        if (result < 0) {
            fprintf(stderr, "Error: Data transmission failed (%d)\n", result);
            return -1;
        } else {
            printf("Sent %d bytes successfully.\n", transferred);
        }

        result = libusb_interrupt_transfer(*USB_Device, EP_IN, rx_buffer, REPORT_SIZE, &transferred, 3000);
        
        if (result < 0) {
            fprintf(stderr, "Error: Data reception failed (%d)\n", result);
            return -1;
        } else {
            printf("Received %d bytes from target.\n", transferred);
            printf("Raw Payload: [0x%02X, 0x%02X, 0x%02X, 0x%02X]\n", 
                    rx_buffer[1], rx_buffer[2], rx_buffer[3], rx_buffer[4]);
        }
        
        confirmed_voltage_correction = read_uint32_from_buff(&rx_buffer[34]);
        printf("Voltage correction: %d mV\n", confirmed_voltage_correction);
    return confirmed_voltage_correction;
}


int32_t get_current(libusb_device_handle **USB_Device, i_src_param* param) {
        int result;
        int transferred = 0;
        transmit_unar_op(USB_Device, CMD_GET_CURRENT);

            result = libusb_interrupt_transfer(*USB_Device, EP_IN, rx_buffer, REPORT_SIZE, &transferred, 3000);
            
            if (result < 0) {
                fprintf(stderr, "Error: Data reception failed (%d)\n", result);
            } else {
                printf("Received %d bytes from target.\n", transferred);
                printf("Raw Payload: [0x%02X, 0x%02X, 0x%02X, 0x%02X]\n", 
                        rx_buffer[1], rx_buffer[2], rx_buffer[3], rx_buffer[4]);
            }
            
            param->i = read_uint32_from_buff(&rx_buffer[2]);
            printf("Current data: %d mA\n", param->i);
    return 0;
}



// Now accepts pointers to persist the device state across calls
void command_interface(commands cmd, libusb_context *ctx, libusb_device_handle **USB_Device)
{
    unsigned int VID; // Изменено на unsigned int
    unsigned int PID; // Изменено на unsigned int
    i_src_param param ={0, 0, 0, 0, 0, 0, 0, 0}; // Initialize all fields to zero
    i_src_param* graph_param = calloc(100, sizeof(i_src_param));
    if (!graph_param) { 
        fprintf(stderr, "Error: Memory allocation failed for graph_param.\n");
        return;
    }

    switch (cmd){
        case CMD_HELP:
            help();
            break;
        case CMD_DEFAULT_DEVICE:
            VID = DEVICE_VID;
            PID = DEVICE_PID;
            connect_device(ctx, USB_Device, VID, PID);
            break; 

        case CMD_CONNECT_DEVICE:
            printf("%*s write DEVICE_VID (in Hex, e.g. 1155 for 0x0483): ", width_begin, "");
            if (scanf("%x", &tmp) != 1) return; // Read as hex
            VID = tmp;
            
            printf("%*s DEVICE_PID (in Hex, e.g. 22352 for 0x5750): ", width_begin, "");
            if (scanf("%x", &tmp) != 1) return; // Read as hex
            PID = tmp;

            connect_device(ctx, USB_Device, VID, PID);
            printf("Interface %d claimed successfully.\n", INT_INTERFACE);
            break;
        case CMD_GET_CURRENT:
            get_current(USB_Device, &param);
            break;
        case CMD_SET_CURRENT:
            printf("%*s write current (in mA): ", width_begin, "");
            if (scanf("%u", &tmp) != 1) return; // Read as hex
            param.i = tmp;
            set_current(&param, USB_Device);
            break;
        case CMD_GET_PARAMETERS:
            get_param(&param, USB_Device);
            print_data(&param);
            break;
        case CMD_SET_TIME_MEASURING:
            printf("%*s write measuring time (in ms): ", width_begin, "");
            if (scanf("%u", &tmp) != 1) return; // Read as hex
            set_measuring_time(USB_Device, tmp);
            break;
        case CMD_SET_VOLTAGE_CORRECTION:
            printf("%*s write voltage correction (in mV): ", width_begin, "");
            if (scanf("%u", &tmp) != 1) return; // Read as hex
            set_voltage_correction(USB_Device, tmp);
            break;
        case CMD_GET_GRAPH:
            get_graph(USB_Device, graph_param, &param);
            break;
        case CMD_DEBUG:
            transmit_unar_op(USB_Device, CMD_DEBUG);
            break;
        case CMD_EXIT_DEBUG:
            transmit_unar_op(USB_Device, CMD_EXIT_DEBUG);
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