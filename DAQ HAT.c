/*****************************************************************************
To be used with the mccdaq library: https://github.com/mccdaq/daqhats/tree/master
    To reach:
    * cd 
    * cd daqhats/examples/c/mcc118/continuous_scan
    * 
    * To update: while in the folder:
    * make
    * 
    * To run:
    * ./continuous_scan

    Purpose:
        Perform a continuous acquisition on 1 or more channels.

*****************************************************************************/

/************** Libraries ***************/

#include "../../daqhats_utils.h"
#include <wiringPi.h>
#include <stdio.h>

/*************** Parameters **************/

const double temperatureMax = 25.0;         //We define the limit physical values
const double pressureMax = 15.0;
const double fireTime = 7.0;
const double valve2_open_time = 2.0; // Time in seconds to wait before opening valve 2
int valve1_opened = 0; // 0: closed, 1: opened
int valve2_opened = 0; // 0: closed, 1: opened


/********** Functions ***********/

double voltageToTemperature(double voltage)
{
    return (voltage * 1000.0)/5.0-250.0;
}

double voltageToPressure(double voltage)
{
    return (voltage-0.8)/0.032 + 1.27;
}

/************ Main code ***********/
int main(void)
{    
    wiringPiSetup();                                // We define wiringPi for the pin allocation
    pinMode(28, OUTPUT);                            //Relay -> GPIO 28             Pin physical : 38
    pinMode(25, OUTPUT);                            //Relay -> GPIO 26             Pin physical : 37
    
    int result = RESULT_SUCCESS;
    uint8_t address = 0;
    char c;
    char display_header[512];
    int i;
    char channel_string[512];
    char options_str[512];

    // Set the channel mask which is used by the library function
    // mcc118_a_in_scan_start to specify the channels to acquire.
    // The functions below, will parse the channel mask into a
    // character string for display purposes.
    // | CHAN1 | CHAN2 | CHAN3 | CHAN4
    uint8_t channel_mask = {CHAN0 | CHAN5 | CHAN5};
    convert_chan_mask_to_string(channel_mask, channel_string);
    
    uint32_t samples_per_channel = 0;

    int max_channel_array_length = mcc118_info()->NUM_AI_CHANNELS;
    int channel_array[max_channel_array_length];
    uint8_t num_channels = convert_chan_mask_to_array(channel_mask, 
        channel_array);

    uint32_t internal_buffer_size_samples = 0;
    uint32_t user_buffer_size = 1000 * num_channels;
    double read_buf[user_buffer_size];
    int total_samples_read = 0;
    
    //We set the pin values to 0, so the relays are closed, hence the valves
    digitalWrite(25,LOW);
    digitalWrite(28,LOW);
    
    int32_t read_request_size = READ_ALL_AVAILABLE;

    // When doing a continuous scan, the timeout value will be ignored in the
    // call to mcc118_a_in_scan_read because we will be requesting that all
    // available samples (up to the default buffer size) be returned.
    double timeout = 5.0;

    double scan_rate = 1000.0;
    double actual_scan_rate = 0.0;
    mcc118_a_in_scan_actual_rate(num_channels, scan_rate, &actual_scan_rate);

    uint32_t options = OPTS_CONTINUOUS;

    uint16_t read_status = 0;
    uint32_t samples_read_per_channel = 0;


    // Select an MCC118 HAT device to use.
    // To be used if more than 1 HAT
    if (select_hat_device(HAT_ID_MCC_118, &address))
    {
        // Error getting device.
        return -1;
    }

    printf ("\nSelected MCC 118 device at address %d\n", address);

    // Open a connection to the device.
    result = mcc118_open(address);
    STOP_ON_ERROR(result);

    convert_options_to_string(options, options_str);
    convert_chan_mask_to_string(channel_mask, channel_string) ;

    //UI style

    printf("\nMCC 118 continuous scan example\n");
    printf("    Functions demonstrated:\n");
    printf("        mcc118_a_in_scan_start\n");
    printf("        mcc118_a_in_scan_read\n");
    printf("        mcc118_a_in_scan_stop\n");
    printf("    Channels: %s\n", channel_string);
    printf("    Requested scan rate: %-10.2f\n", scan_rate);
    printf("    Actual scan rate: %-10.2f\n", actual_scan_rate);
    printf("    Options: %s\n", options_str);

    printf("\nPress ENTER to continue ...\n");
    scanf("%c", &c);

    // Configure and start the scan.
    // Since the continuous option is being used, the samples_per_channel 
    // parameter is ignored if the value is less than the default internal
    // buffer size (10000 * num_channels in this case). If a larger internal
    // buffer size is desired, set the value of this parameter accordingly.
    result = mcc118_a_in_scan_start(address, channel_mask, samples_per_channel,
        scan_rate, options);
    STOP_ON_ERROR(result);

    STOP_ON_ERROR(mcc118_a_in_scan_buffer_size(address,
        &internal_buffer_size_samples));
    printf("Internal data buffer size:  %d\n", internal_buffer_size_samples);
    printf("\nStarting scan ... Press ENTER to stop\n\n");

    FILE *outputFile = fopen("dataDAQ.csv", "w"); //Create and open a csv file to store the data

    // Create the header containing the column names.
    strcpy(display_header, "Samples Read    Scan Count    ");
    for (i = 0; i < num_channels; i++)
    {
        sprintf(channel_string, "Channel %d   ", channel_array[i]);
        strcat(display_header, channel_string);
    }
    strcat(display_header, "\n");
    printf("%s", display_header);


    printf("\nStarting scan ... Press ENTER to start firing\n\n");
    fflush(stdout);
    getchar(); // Wait for the user to press Enter to start the firing

    // Open valve 1
    digitalWrite(25, HIGH);
    valve1_opened = 1; // Set the flag to indicate valve 1 is opened

    // When the valve 1 is opened, the timer starts. The time value is set in the parameters
    auto start_time = std::chrono::steady_clock::now();
    auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count();

    printf("\nFiring started!\n");
    // Continuously update the display value until enter key is pressed
    do 
    {
        // Since the read_request_size is set to -1 (READ_ALL_AVAILABLE), this
        // function returns immediately with whatever samples are available (up
        // to user_buffer_size) and the timeout parameter is ignored.
        result = mcc118_a_in_scan_read(address, &read_status, read_request_size,
            timeout, read_buf, user_buffer_size, &samples_read_per_channel);
        STOP_ON_ERROR(result);
        if (read_status & STATUS_HW_OVERRUN)
        {
            printf("\n\nHardware overrun\n");
            break;
        }
        else if (read_status & STATUS_BUFFER_OVERRUN)
        {
            printf("\n\nBuffer overrun\n");
            break;
        }

        total_samples_read += samples_read_per_channel;

        // Display the last sample for each channel.
        printf("\r%12.0d    %10.0d ", samples_read_per_channel,
            total_samples_read);
        if (samples_read_per_channel > 0)
        {
            int index = samples_read_per_channel * num_channels - num_channels;

            for (i = 0; i < num_channels; i++)
            {
                // Opens the valve 2 under conditions:
                if (valve1_opened && !valve2_opened && elapsed_time >= valve2_open_time)
                {
                    printf("\nOpening valve 2...\n");
                    digitalWrite(28, HIGH);
                    valve2_opened = 1; // Set the flag to indicate valve 2 is opened
                }

                // Check if the specified fireTime has been reached. If yes, closes the valves, end of run
                if (elapsed_time >= fireTime)
                {
                    printf("\nTime limit reached: Turning off relay\n");
                    digitalWrite(25, LOW);
                    digitalWrite(28, LOW);
                break;
                }

                printf("%10.5f V", read_buf[index + i]);
                double voltage = read_buf[index +i];
                
                elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count();

                // For pressure transducer, read the output value and convert the voltage value to pressure directly displayed
                if (channel_array[i] ==0 || channel_array[i] ==1 || channel_array[i] ==2 ) {
                    double pressure = voltageToPressure(voltage);
                    printf("Channel %d: %.2f bar\n", channel_array[i], pressure);

                    // If the fireTime is reached or physical values reched, stop the run, 2nd security check
                    if (pressure >= pressureMax || elapsed_time >=fireTime){
                        if (pressure >= pressureMax) {
                        printf("\nPressure above limit: Turning off relay\n");
                        digitalWrite(25, LOW);
                        digitalWrite(28, LOW);
                        }
                        if (elapsed_time >=fireTime){
                        printf("\nTime limit reached: Turning off relay\n");
                        digitalWrite(25, LOW);
                        digitalWrite(28, LOW);
                        }
                        break;
                    }
                    //Otherwise stay opened
                    else {
                        digitalWrite(25, HIGH);
                        digitalWrite(28, HIGH);
                    }
                }
                //Same for the thermocouples
                else if (channel_array[i] ==4 || channel_array[i] ==5 || channel_array[i] ==6 || channel_array[i] ==7) {
                    double temperature = voltageToTemperature(voltage);
                    printf("Channel %d: %.2f °C\n", channel_array[i], temperature);
                        
                    if (temperature >= temperatureMax){
                        if (temperature >= temperatureMax) {
                        printf("\nTemperature above limit: Turning off relay\n");
                        digitalWrite(25, LOW);
                        digitalWrite(28, LOW);
                        }
                        if (elapsed_time >=fireTime){
                        printf("\nTime limit reached: Turning off relay\n");
                        digitalWrite(25, LOW);
                        digitalWrite(28, LOW);
                        }
                        break;
                    }
                    else {
                        digitalWrite(25, HIGH);
                        digitalWrite(28, HIGH);
                    }
                }
                fprintf(outputFile, "%f,%f,%f\n", elapsed_time, pressure, temperature); //Write values within the CSV file
            }
            fclose(outputFile);
            fflush(stdout);
        }
        usleep(500000);
    } while ((result == RESULT_SUCCESS) && ((read_status & STATUS_RUNNING) == STATUS_RUNNING) && !enter_press());
    digitalWrite(25, LOW);          //Close the valves if he system is stopped
    digitalWrite(28, LOW);
    printf("\nFiring completed!\n");

    printf("\n");

stop:
    print_error(mcc118_a_in_scan_stop(address));
    print_error(mcc118_a_in_scan_cleanup(address));
    print_error(mcc118_close(address));
    digitalWrite(25, LOW);          //Close the valves if he system is stopped
    digitalWrite(28, LOW);
    printf("\n End of sequence, valves closed");
    return 0;
}
