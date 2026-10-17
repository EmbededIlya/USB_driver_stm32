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

#define TX_TIMEOUT      3000
#define RX_TIMEOUT      3000



typedef enum{
    ERROR_NONE = 0,
    ERROR_CURRENT_DRIVER_NOT_CONNECTED,
    ERROR_CURRENT_DRIVER_INVALID_PARAMETER,
    ERROR_CURRENT_DRIVER_TRANSFER_FAILED
} error_codes_t;

typedef enum {
    CMD_NONE= 0,
    CMD_DEBUG,
    CMD_EXIT_DEBUG,
    CMD_SET_CURRENT,
    CMD_GET_PARAMETERS,
	CMD_SET_TIME_MEASURING,
	CMD_SET_CONTROL_MODE,
	CMD_RETRY,
    CMD_INIT,
    CMD_END_INIT
} MCU_commands;

typedef enum {
    CMD_HELP_CONSOLE = 0,
    CMD_CONNECT_DEVICE_CONSOLE,
    CMD_DEFAULT_DEVICE_CONSOLE,
    CMD_DEBUG_CONSOLE,
    CMD_EXIT_DEBUG_CONSOLE,
    CMD_SET_CURRENT_CONSOLE,
    CMD_GET_CURRENT_CONSOLE,
    CMD_GET_PARAMETERS_CONSOLE,
    CMD_GET_GRAPH_CONSOLE,
	CMD_SET_TIME_MEASURING_CONSOLE,
	CMD_SET_CONTROL_MODE_CONSOLE,
	CMD_RETRY_CONSOLE,
    CMD_INIT_CONSOLE,
    CMD_END_INIT_CONSOLE,
    CMD_NONE_CONSOLE,
    CMD_EXIT_CONSOLE
} commands;




typedef enum CurrentSourceRegulationMode_t{
	NOT_DEFINED = 0,
	SIMPLE,
	PI,
	PID
} CurrentSourceRegulationMode_t;

typedef struct i_src_param{
    int32_t adc_raw;
    int32_t voltage;
    int32_t voltage_corrected;
    int32_t voltage_correction;
    int32_t i_current_mA;
    uint32_t i_lim;
    uint32_t i;
    uint32_t Vdac;
    uint32_t measuring_time;
    CurrentSourceRegulationMode_t mode;
} i_src_param;

//Global param
i_src_param* graph_param = NULL;

int width_begin = 10;
int width_end = 15;
unsigned int tmp; // Изменено на unsigned int

/*-----------------------------------------------------------------------Functions---------------------------------------------------------*/

void help() {
        printf("%*s\n", width_begin, "Available commands:");
        printf("%*d - Help\n", width_end, CMD_HELP_CONSOLE);
        printf("%*d - Connect device\n", width_end, CMD_CONNECT_DEVICE_CONSOLE);
        printf("%*d - Connect default device\n", width_end, CMD_DEFAULT_DEVICE_CONSOLE);
        printf("%*d - Debug mode\n", width_end, CMD_DEBUG_CONSOLE);
        printf("%*d - Exit debug mode\n", width_end, CMD_EXIT_DEBUG_CONSOLE);
        printf("%*d - Get current\n", width_end, CMD_GET_CURRENT_CONSOLE);
        printf("%*d - Set current\n", width_end, CMD_SET_CURRENT_CONSOLE);
        printf("%*d - Get parameters\n", width_end, CMD_GET_PARAMETERS_CONSOLE);
        printf("%*d - Get graph\n", width_end, CMD_GET_GRAPH_CONSOLE);
        printf("%*d - Set measuring time\n", width_end, CMD_SET_TIME_MEASURING_CONSOLE);
        printf("%*d - Init current_source\n", width_end, CMD_INIT_CONSOLE);
        printf("%*d - Exit program\n", width_end, CMD_EXIT_CONSOLE);
}

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
    printf("Measuring Time: %u\n", param->measuring_time);
}


error_codes_t transmit_data(unsigned char* tx_buff, unsigned char* rx_buff, MCU_commands cmd, libusb_device_handle **USB_Device){
            if (!(*USB_Device)) {
                fprintf(stderr, "Error: No device connected. Connect a device first.\n");
                return ERROR_CURRENT_DRIVER_NOT_CONNECTED;
            }
            int result;
            int transferred = 0;
            tx_buff[0] = REPORT_ID_OUT; 
            tx_buff[1] = cmd; 
            result = libusb_interrupt_transfer(*USB_Device, EP_OUT, tx_buff, REPORT_SIZE, &transferred, TX_TIMEOUT);
            if (result < 0) {
                fprintf(stderr, "Error: Data transmission failed (%d)\n", result);
                return ERROR_CURRENT_DRIVER_TRANSFER_FAILED;
            } else {
                printf("Sent %d bytes successfully.\n", transferred);
            }

            result = libusb_interrupt_transfer(*USB_Device, EP_IN, rx_buff, REPORT_SIZE, &transferred, RX_TIMEOUT);
            if (result < 0) {
                fprintf(stderr, "Error: Data reception failed (%d)\n", result);
                return ERROR_CURRENT_DRIVER_TRANSFER_FAILED; // Return a zeroed buffer on error
            } else {
                printf("Received %d bytes from target.\n", transferred);
                printf("Raw Payload: [");
                for (int i = 0; i < REPORT_SIZE; i++) {
                    printf("0x%02X%s", rx_buff[i],
                        (i < REPORT_SIZE - 1) ? ", " : "");
                }
                printf("]\n");
            }

            return ERROR_NONE; // Return success code
}


MCU_commands get_command_from_rx_buffer(unsigned char* rx_buff){
    return rx_buff[1]; // Return the command byte from the received buffer
}


i_src_param* get_param(i_src_param* param, libusb_device_handle **USB_Device) {
            unsigned char tx_buffer[REPORT_SIZE];
            unsigned char rx_buffer[REPORT_SIZE];
            transmit_data(tx_buffer, rx_buffer, CMD_GET_PARAMETERS, USB_Device);
            param->adc_raw = read_uint32_from_buff(&rx_buffer[2]);
            param->voltage = read_uint32_from_buff(&rx_buffer[6]);
            param->voltage_corrected = read_uint32_from_buff(&rx_buffer[10]);
            param->i_current_mA = read_uint32_from_buff(&rx_buffer[14]);
            param->i_lim = read_uint32_from_buff(&rx_buffer[18]);
            param->i = read_uint32_from_buff(&rx_buffer[22]);
            param->Vdac = read_uint32_from_buff(&rx_buffer[26]);
            param->measuring_time = read_uint32_from_buff(&rx_buffer[30]);
            param->voltage_correction = read_uint32_from_buff(&rx_buffer[34]);

    return param;
}

error_codes_t set_current(i_src_param* param, libusb_device_handle **USB_Device) {
            unsigned char tx_buffer[REPORT_SIZE];
            unsigned char rx_buffer[REPORT_SIZE];
            write_uint32_to_buff(&tx_buffer[2], (uint32_t)param->i); // Assuming param->i is the current value to set
            transmit_data(tx_buffer, rx_buffer, CMD_SET_CURRENT, USB_Device);
            printf("Setting current...\n");
            param->i = read_uint32_from_buff(&rx_buffer[2]);
            printf("Current data: %d mA\n", param->i);// Added brackets to isolate local variables inside case block
    return ERROR_NONE; // Return success code
}



error_codes_t connect_device(libusb_context *ctx, libusb_device_handle **USB_Device, unsigned int VID, unsigned int PID) {
    *USB_Device = libusb_open_device_with_vid_pid(ctx, VID, PID);
    if (!(*USB_Device)) {
        fprintf(stderr, "Error: Device VID 0x%04X PID 0x%04X not found.\n", VID, PID);
        return ERROR_CURRENT_DRIVER_NOT_CONNECTED;
    }
    printf("Device opened successfully.\n");
    
    int result = libusb_claim_interface(*USB_Device, INT_INTERFACE);
    if (result < 0) {
        fprintf(stderr, "Error: Cannot claim interface %d (%d)\n", INT_INTERFACE, result);
        libusb_close(*USB_Device);
        *USB_Device = NULL;
        return ERROR_CURRENT_DRIVER_TRANSFER_FAILED;
    }
    printf("Interface %d claimed successfully.\n", INT_INTERFACE);
    return ERROR_NONE;
}

error_codes_t get_graph(libusb_device_handle **USB_Device, i_src_param* graph_param, i_src_param* param) {
        int steps;
        int min_current;
        int max_current;
        int amount_of_data;
        i_src_param* result_param;
        if (!(*USB_Device)) {
            fprintf(stderr, "Error: No device connected. Connect a device first.\n");
            return ERROR_CURRENT_DRIVER_NOT_CONNECTED;
        }
        printf("%*s write minimal current (in mA): ", width_begin, "");
        if (scanf("%u", &tmp) != 1) return ERROR_CURRENT_DRIVER_INVALID_PARAMETER; // Read as hex
        min_current = tmp;
        printf("%*s write maximum current (in mA): ", width_begin, "");
        if (scanf("%u", &tmp) != 1) return ERROR_CURRENT_DRIVER_INVALID_PARAMETER; // Read as hex
        max_current = tmp;
        printf("%*s write number of steps: ", width_begin, "");
        if (scanf("%u", &tmp) != 1) return ERROR_CURRENT_DRIVER_INVALID_PARAMETER; // Read as hex
        steps = tmp;
        amount_of_data = steps + 1; // Including both min and max

        graph_param = calloc(amount_of_data, sizeof(i_src_param));
        if (!graph_param) {
            fprintf(stderr, "Error: Memory allocation failed for graph_param.\n");
            return ERROR_CURRENT_DRIVER_TRANSFER_FAILED;
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
                return ERROR_CURRENT_DRIVER_TRANSFER_FAILED;
            }
        }
        printf("Graph data retrieved successfully.\n");

        if (graph_param != NULL) {
            printf("Graph data retrieved successfully.\n");
            // После обработки данных графика обязательно освобождаем память:
            free(graph_param);
            graph_param = NULL;
        }
    
    return ERROR_NONE;
}

error_codes_t transmit_unar_op( libusb_device_handle **USB_Device, MCU_commands cmd,unsigned char* rx_buffer) {
        unsigned char tx_buffer[REPORT_SIZE];
        tx_buffer[0] = REPORT_ID_OUT; 
        tx_buffer[1] = cmd;
        return  transmit_data(tx_buffer, rx_buffer, cmd, USB_Device);
}

error_codes_t set_measuring_time(libusb_device_handle **USB_Device, i_src_param* param, unsigned int time_ms, unsigned char* rx_buffer) {
        error_codes_t result;
        unsigned char tx_buffer[REPORT_SIZE];
        write_uint32_to_buff(&tx_buffer[2], time_ms); // Assuming time_ms is the measuring time to set
        printf("Setting measuring time...\n");
        result = transmit_data(tx_buffer, rx_buffer, CMD_SET_TIME_MEASURING, USB_Device);
        uint32_t confirmed_time = read_uint32_from_buff(&rx_buffer[30]);
        param->measuring_time = confirmed_time; // Update the measuring time in the param structure
        printf("Measuring time: %d ms\n", confirmed_time);
        if (result != ERROR_NONE) {
            printf("FAIL\n");
            return result;
        }
    return ERROR_NONE; // Return the command byte from the received buffer
}

// int32_t set_voltage_correction(libusb_device_handle **USB_Device, unsigned int voltage_correction) {
//         int result;
//         int confirmed_voltage_correction;
//         int transferred = 0;
//         if (!(*USB_Device)) {
//             fprintf(stderr, "Error: No device connected. Connect a device first.\n");
//             return -1;
//         }
//         memset(tx_buffer, 0, REPORT_SIZE);
//         tx_buffer[0] = REPORT_ID_OUT; 
//         tx_buffer[1] = CMD_SET_VOLTAGE_CORRECTION; 
//         write_uint32_to_buff(&tx_buffer[2], voltage_correction);
//         memset(rx_buffer, 0, REPORT_SIZE);
//         transferred = 0;

//         printf("Setting voltage correction...\n");
//         result = libusb_interrupt_transfer(*USB_Device, EP_OUT, tx_buffer, REPORT_SIZE, &transferred, 1000);
        
//         if (result < 0) {
//             fprintf(stderr, "Error: Data transmission failed (%d)\n", result);
//             return -1;
//         } else {
//             printf("Sent %d bytes successfully.\n", transferred);
//         }

//         result = libusb_interrupt_transfer(*USB_Device, EP_IN, rx_buffer, REPORT_SIZE, &transferred, 3000);
        
//         if (result < 0) {
//             fprintf(stderr, "Error: Data reception failed (%d)\n", result);
//             return -1;
//         } else {
//             printf("Received %d bytes from target.\n", transferred);
//             printf("Raw Payload: [0x%02X, 0x%02X, 0x%02X, 0x%02X]\n", 
//                     rx_buffer[1], rx_buffer[2], rx_buffer[3], rx_buffer[4]);
//         }
        
//         confirmed_voltage_correction = read_uint32_from_buff(&rx_buffer[34]);
//         printf("Voltage correction: %d mV\n", confirmed_voltage_correction);
//     return confirmed_voltage_correction;
// }


error_codes_t get_current(libusb_device_handle **USB_Device, i_src_param* param) {
    error_codes_t result;
    unsigned char rx_buffer[REPORT_SIZE];
    memset(rx_buffer, 0, REPORT_SIZE);
    result = transmit_unar_op(USB_Device, CMD_GET_PARAMETERS, rx_buffer);
    if (result != ERROR_NONE) {
        return result;
    }
    param->i = read_uint32_from_buff(&rx_buffer[42]);
    printf("Current data: %d mA\n", param->i);
    return ERROR_NONE;
}

error_codes_t CurrentSource_init(i_src_param* param, libusb_device_handle **USB_Device) {
            unsigned char rx_buffer[REPORT_SIZE];
            unsigned char tx_buffer[REPORT_SIZE];
            uint32_t tmp;
            error_codes_t result = ERROR_NONE;
            
            printf("Setting time...\n");
            printf("%*s write measuring time (in ms): ", width_begin, "");
            if (scanf("%u", &tmp) != 1) return ERROR_CURRENT_DRIVER_INVALID_PARAMETER; // Read as hex
            result = set_measuring_time(USB_Device, param, tmp, rx_buffer);
            while(get_command_from_rx_buffer(rx_buffer) == CMD_RETRY || param->measuring_time == 0) {
                printf("Retrying to set measuring time...\n");
                printf("%*s write measuring time (in ms): ", width_begin, "");
                if (scanf("%u", &tmp) != 1) return ERROR_CURRENT_DRIVER_INVALID_PARAMETER; // Read as hex
                result = set_measuring_time(USB_Device, param, tmp, rx_buffer);
                if(result != ERROR_NONE){
                    return result;
                }
            }
            if(result == ERROR_CURRENT_DRIVER_NOT_CONNECTED){
                return ERROR_CURRENT_DRIVER_NOT_CONNECTED;
            }
            printf("Setting control mode...\n");
            printf("%*s\n", width_begin, "Available modes:");
            printf("%*d - Simple\n", width_end, SIMPLE);
            printf("%*d - PI\n", width_end, PI);
            printf("%*d - PID\n", width_end, PID);
            if (scanf("%u", &tmp) != 1) return ERROR_CURRENT_DRIVER_INVALID_PARAMETER; // Read as hex
            param->mode = tmp;
            tx_buffer[2] = param->mode;
            result = transmit_data(tx_buffer, rx_buffer, CMD_SET_CONTROL_MODE, USB_Device);
            while(get_command_from_rx_buffer(rx_buffer) == CMD_RETRY) {
                printf("Retrying to set control mode? invalid value...\n");
                printf("Setting control mode...\n");
                printf("%*s\n", width_begin, "Available modes:");
                printf("%*d - Simple\n", width_end, SIMPLE);
                printf("%*d - PI\n", width_end, PI);
                printf("%*d - PID\n", width_end, PID);
                if (scanf("%u", &tmp) != 1) return ERROR_CURRENT_DRIVER_INVALID_PARAMETER; // Read as hex
                param->mode = tmp;
                tx_buffer[2] = param->mode;
                result = transmit_data(tx_buffer, rx_buffer, CMD_SET_CONTROL_MODE, USB_Device);
            }

            printf("Setting current value...\n");
            printf("%*s write current (in mA): ", width_begin, "");
            if (scanf("%u", &tmp) != 1) return ERROR_CURRENT_DRIVER_INVALID_PARAMETER; // Read as hex
            param->i = tmp;
            result = transmit_data(tx_buffer, rx_buffer, CMD_SET_CURRENT, USB_Device);
            while(get_command_from_rx_buffer(rx_buffer) == CMD_RETRY || param->i == 0) {
                printf("Retrying to set current...\n");
                printf("%*s write current (in mA): ", width_begin, "");
                if (scanf("%u", &tmp) != 1) return ERROR_CURRENT_DRIVER_INVALID_PARAMETER; // Read as hex
                param->i = tmp;
                result = transmit_data(tx_buffer, rx_buffer, CMD_SET_CURRENT, USB_Device);
            }

            printf("INITIALIZED\n");// Added brackets to isolate local variables inside case block
    return result; // Return the result of the last operation
}

// Now accepts pointers to persist the device state across calls
void command_interface(commands cmd, libusb_context *ctx, libusb_device_handle **USB_Device)
{
    unsigned char tx_buffer[REPORT_SIZE];
    unsigned char rx_buffer[REPORT_SIZE];
    unsigned int VID; // Изменено на unsigned int
    unsigned int PID; // Изменено на unsigned int
    i_src_param param ={0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Initialize all fields to zero
    i_src_param* graph_param = calloc(100, sizeof(i_src_param));
    if (!graph_param) { 
        fprintf(stderr, "Error: Memory allocation failed for graph_param.\n");
        return;
    }

    switch (cmd){
        case CMD_HELP_CONSOLE:
            help();
            break;
        case CMD_DEFAULT_DEVICE_CONSOLE:
            VID = DEVICE_VID;
            PID = DEVICE_PID;
            connect_device(ctx, USB_Device, VID, PID);
            break; 

        case CMD_CONNECT_DEVICE_CONSOLE:
            printf("%*s write DEVICE_VID (in Hex, e.g. 1155 for 0x0483): ", width_begin, "");
            if (scanf("%x", &tmp) != 1) return; // Read as hex
            VID = tmp;
            
            printf("%*s DEVICE_PID (in Hex, e.g. 22352 for 0x5750): ", width_begin, "");
            if (scanf("%x", &tmp) != 1) return; // Read as hex
            PID = tmp;

            connect_device(ctx, USB_Device, VID, PID);
            printf("Interface %d claimed successfully.\n", INT_INTERFACE);
            break;
        case CMD_INIT_CONSOLE:
            CurrentSource_init(&param, USB_Device);
            break;
        case CMD_GET_CURRENT_CONSOLE:
            get_current(USB_Device, &param);
            break;
        case CMD_SET_CURRENT_CONSOLE:
            printf("%*s write current (in mA): ", width_begin, "");
            if (scanf("%u", &tmp) != 1) return; // Read as hex
            param.i = tmp;
            transmit_data(tx_buffer, rx_buffer, CMD_SET_CURRENT, USB_Device);
            while(get_command_from_rx_buffer(rx_buffer) == CMD_RETRY) {
                printf("Retrying to set current...\n");
            }
            break;
        case CMD_GET_PARAMETERS_CONSOLE:
            get_param(&param, USB_Device);
            print_data(&param);
            break;
        case CMD_SET_TIME_MEASURING_CONSOLE:
            printf("%*s write measuring time (in ms): ", width_begin, "");
            if (scanf("%u", &tmp) != 1) return; // Read as hex
            set_measuring_time(USB_Device, &param, tmp, rx_buffer);
            while(get_command_from_rx_buffer(rx_buffer) == CMD_RETRY) {
                printf("Retrying to set measuring time...\n");
                if (scanf("%u", &tmp) != 1) return; // Read as hex
                set_measuring_time(USB_Device, &param, tmp, rx_buffer);
            }
            break;
        // case CMD_SET_VOLTAGE_CORRECTION:
        //     printf("%*s write voltage correction (in mV): ", width_begin, "");
        //     if (scanf("%u", &tmp) != 1) return; // Read as hex
        //     while(get_command_from_rx_buffer(rx_buffer) == CMD_RETRY) {
        //         printf("Retrying to set voltage correction...\n");
        //     }
        //     break;
        case CMD_GET_GRAPH_CONSOLE:
            get_graph(USB_Device, graph_param, &param);
            break;
        case CMD_DEBUG_CONSOLE:
            transmit_unar_op(USB_Device, CMD_DEBUG, rx_buffer);
            break;
        case CMD_EXIT_DEBUG_CONSOLE:
            transmit_unar_op(USB_Device, CMD_EXIT_DEBUG, rx_buffer);
            break;
        case CMD_EXIT_CONSOLE:
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
            command_interface(CMD_HELP_CONSOLE, ctx, &USB_Device); // Removed ghost variables
        }
        
        printf("\nEnter command number: ");
        int cmd;
        if (scanf("%d", &cmd) == 0) {
            printf("Invalid input. Please enter a valid command number.\n");
            while (getchar() != '\n'); // Clear input buffer
            continue;
        }
        
        if (cmd < CMD_HELP_CONSOLE || cmd > CMD_EXIT_CONSOLE) // Replaced unknown CMD_LOAD_READY with CMD_EXIT_CONSOLE
        {
            printf("Unknown command - try command help\n");
            continue;
        } 
        else if (cmd == CMD_EXIT_CONSOLE)
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