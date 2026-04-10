#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>

#define DEVICE_NAME "/dev/fpga_fir"
#define MAX_SAMPLES 10000

struct fpga_spi_ioc_transfer{
    uint64_t tx_buf; 
    uint64_t rx_buf;
    uint32_t len;
};

#define FPGA_IOC_MAGIC 'k'
#define FPGA_IOC_FULL_DUPLEX _IOWR(FPGA_IOC_MAGIC, 1, struct fpga_spi_ioc_transfer)

//helper hardware functions 

//when load is high we transfer data
//when load is low we transfer coefficients
void set_load_x (int state){
    if(state == 1){
        system("pinctrl set 23 op dh > /dev/null 2>&1");
    }
    else {
        system("pinctrl set 23 op dl > /dev/null 2>&1");
    }
}

int spi_transfer_16bit(int fd, uint16_t * tx_data, uint16_t *rx_data, int count){
    struct fpga_spi_ioc_transfer tr = {
        .tx_buf = (uintptr_t)tx_data, 
        .rx_buf = (uintptr_t)rx_data,
        .len = count * sizeof(uint16_t)
    };
    return ioctl(fd, FPGA_IOC_FULL_DUPLEX, &tr);
}

//helper file functions 

int read_csv(const char *file_name, uint16_t *buffer){
    FILE *file = fopen(file_name, "r");
    if(!file){
        printf("The file %s was not found. \n", file_name);
        return -1;
    }
    int count = 0, val; 
    while (fscanf(file, "%d", &val) == 1 && count < MAX_SAMPLES){
        buffer[count] = (uint16_t)(val & 0xFFFF); 
        count ++;
    }   
    fclose(file);
    return count; 
}


void write_csv(const char* file_name, uint16_t *input, uint16_t *output, int count){
     FILE *file = fopen(file_name, "w");
    if(!file){
        printf("The file %s could not be created. \n", file_name);
        return;
    }
    fprintf(file, "Index, Unfiltered_temps, Filtered_temps\n");
    for (int i = 0; i < count; i++){
        fprintf(file, "%d, %d, %d\n", i, (int16_t)input[i], (int16_t)output[i]);
    }
    fclose(file);
    printf("The content was written successfully.\n");
}

//rpi is little-endian. 
//we need to swap data when transfer it. 
uint16_t swap_bytes(uint16_t val) {
    return (val << 8) | (val >> 8);
}

void test_filter(int fd, const char *num_test, const char *f_coef, const char* f_in, const char *f_out){
    printf("---Starting test: %s.---\n", num_test);
    uint16_t *coefficients = malloc(MAX_SAMPLES * sizeof(uint16_t));
    uint16_t *input_data = malloc(MAX_SAMPLES * sizeof(uint16_t));
    uint16_t *output_data = malloc(MAX_SAMPLES * sizeof(uint16_t));
    uint16_t *dummy_rx = malloc(MAX_SAMPLES * sizeof(uint16_t));

    int nr_coef = read_csv(f_coef, coefficients);
    int nr_data = read_csv(f_in, input_data);

    if(nr_coef > 0 && nr_data > 0){
        printf("Number of coef readed: %d.\n", nr_coef);
        printf("Number of values readed: %d.\n", nr_data);

        set_load_x(0);
        usleep(10000);
        for(int i = 0; i < nr_coef; i++) {
            //swap the coef before sending it 
            uint16_t tx_val = swap_bytes(coefficients[i]);
            spi_transfer_16bit(fd, &tx_val, &dummy_rx[i], 1);
        }
        printf("Coeff loaded to FPGA.\n");

        set_load_x(1);
        usleep(10000);
        for(int i = 0; i < nr_data; i++) {
            // swap temp
            uint16_t tx_val = swap_bytes(input_data[i]);
            uint16_t rx_val;
            
            spi_transfer_16bit(fd, &tx_val, &rx_val, 1);
            
            // swap the result before writing it 
            output_data[i] = swap_bytes(rx_val);
        }
        printf("Temperatures were transfered to FPGA.\n");

        write_csv(f_out, input_data, output_data, nr_data);
    }
    else {
        printf("I/O error.\n");
    }
    free(coefficients);
    free(input_data);
    free(output_data);
    free(dummy_rx); 
}

int main(){
    printf("FIR FILTER FPGA STARTING.\n");
    int fd = open(DEVICE_NAME, O_RDWR);
    if(fd < 0){
        perror("Acces denied. Try sudo. \n");
        return 1;
    }

    test_filter(fd, "LOW-PASS", "coef_low_pass.csv", 
                "date_low_pass.csv", "out_low_pass.csv");
    
    usleep(50000); 
    test_filter(fd, "HIGH-PASS", 
                        "coef_high_pass.csv", 
                        "date_high_pass.csv", 
                        "out_high_pass.csv");

    close(fd);
    printf("THE PROCCESS WAS DONE SUCCESSFULLY.\n");

    return 0;
}