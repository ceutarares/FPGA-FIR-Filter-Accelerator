#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <time.h>

#define DEVICE_NAME "/dev/fpga_fir"
#define MAX_SAMPLES 10000

struct fpga_spi_ioc_transfer{
    uint64_t tx_buf; 
    uint64_t rx_buf;
    uint32_t len;
};

#define FPGA_IOC_MAGIC 'k'
#define FPGA_IOC_FULL_DUPLEX _IOWR(FPGA_IOC_MAGIC, 1, struct fpga_spi_ioc_transfer)

uint16_t swap_bytes(uint16_t val) {
    return (val << 8) | (val >> 8);
}

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

int read_csv(const char *file_name, uint16_t *buffer){
    FILE *file = fopen(file_name, "r");
    if(!file){
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

void test_performance(int fd, const char *f_coef, const char* f_in){
    uint16_t *coefficients = malloc(MAX_SAMPLES * sizeof(uint16_t));
    uint16_t *input_data = malloc(MAX_SAMPLES * sizeof(uint16_t));
    uint16_t *output_data = malloc(MAX_SAMPLES * sizeof(uint16_t));
    uint16_t dummy_rx;

    int nr_coef = read_csv(f_coef, coefficients);
    int nr_data = read_csv(f_in, input_data);

    if(nr_coef > 0 && nr_data > 0){
        
        set_load_x(0);
        for(int i = 0; i < nr_coef; i++) {
            uint16_t tx = swap_bytes(coefficients[i]);
            spi_transfer_16bit(fd, &tx, &dummy_rx, 1);
        }

        set_load_x(1);
        usleep(10000);

        // start measurements 
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        for(int i = 0; i < nr_data; i++) {
            uint16_t tx = swap_bytes(input_data[i]);
            uint16_t rx;
            spi_transfer_16bit(fd, &tx, &rx, 1);
            output_data[i] = swap_bytes(rx);
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        // end measurement 

        double time_taken = (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_nsec - start.tv_nsec) / 1e3;
        
        printf("\n--- BENCHMARK RESULTS ---\n");
        printf("Total samples processed: %d\n", nr_data);
        printf("Total time: %.2f microseconds\n", time_taken);
        printf("Average time per sample: %.2f microseconds\n", time_taken / nr_data);
        printf("-------------------------\n");

    }
    free(coefficients);
    free(input_data);
    free(output_data);
}

int main(){
    int fd = open(DEVICE_NAME, O_RDWR);
    if(fd < 0){
        perror("Error opening device");
        return 1;
    }

    test_performance(fd, "coef_low_pass.csv", "date_low_pass.csv");

    close(fd);
    return 0;
}