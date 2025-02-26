#include <stdio.h>
#include "pico/stdlib.h"
#include <time.h>
#include "hardware/adc.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "hardware/i2c.h"

#define joystick_X 26      // GPIO para eixo X
#define joystick_Y 27      // GPIO para eixo Y
#define joystick_Button 22 // GPIO para botão do Joystick

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define botao_A 5
#define botao_B 6

#define step 0.2    // Passo de variação para cada movimento do joystick
#define deadzone 50 // Zona morta do joystick

#define limiteMaxUmidade 65.0
#define limiteMinUmidade 35.0

#define limiteMaxTemperatura 8.0
#define limiteMinTemperatura -60.0

double umidade = 45.0;      // Começa com 50% de umidade
double temperatura = -15.0; // Começa com -15°C

double maxUmidade = 45.0;
double minUmidade = 45.0;

double maxTemperatura = -15.0;
double minTemperatura = -15.0;
ssd1306_t ssd; // Inicializa a estrutura do display

volatile int frame = 1;
volatile uint32_t last_time_button_A = 0;
volatile uint32_t last_time_button_B = 0;
volatile uint32_t last_time_uart = 0;
volatile bool display_on = false;

uint16_t get_center(uint8_t adc_channel)
{
    uint32_t sum = 0;
    for (int i = 0; i < 100; i++)
    {
        adc_select_input(adc_channel);
        sum += adc_read();
        sleep_ms(5);
    }
    return sum / 100;
}

void gpio_irq_handler(uint gpio, uint32_t event)
{

    uint32_t current_time = to_us_since_boot(get_absolute_time());
    if (gpio == botao_B && current_time - last_time_button_B > 200000)
    {
        frame++;
        if (frame == 4)
        {
            frame = 1;
        }
        last_time_button_B = current_time;
    }
    if (gpio == botao_A && current_time - last_time_button_A > 200000)
    {
        display_on = !display_on;
        if (display_on == false)
        {
            // Limpa o display antes de atualizar
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
        }
        last_time_button_A = current_time;
    }
}

int main()
{
    stdio_init_all();

    gpio_init(joystick_Button);
    gpio_set_dir(joystick_Button, GPIO_IN);
    gpio_pull_up(joystick_Button);

    gpio_init(botao_B);
    gpio_set_dir(botao_B, GPIO_IN);
    gpio_pull_up(botao_B);

    gpio_init(botao_A);
    gpio_set_dir(botao_A, GPIO_IN);
    gpio_pull_up(botao_A);

    gpio_set_irq_enabled_with_callback(botao_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(botao_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    adc_init();
    adc_gpio_init(joystick_X);
    adc_gpio_init(joystick_Y);

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL);                                        // Pull up the clock line
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd);                                         // Configura o display
    ssd1306_send_data(&ssd);                                      // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    uint16_t adc_value_x;
    uint16_t adc_value_y;
    uint16_t center_x = get_center(0);
    uint16_t center_y = get_center(1);

    while (true)
    {
        // Leitura do Joystick
        adc_select_input(0); // Eixo X (pino 26)
        adc_value_x = adc_read();
        adc_select_input(1); // Eixo Y (pino 27)
        adc_value_y = adc_read();

        int16_t deviation_x = adc_value_x - center_x;
        int16_t deviation_y = adc_value_y - center_y;

        // Ajuste de temperatura (eixo X)
        if (abs(deviation_x) > deadzone)
        {
            temperatura += (deviation_x > 0) ? step : -step;
        }

        // Ajuste de umidade (eixo Y)
        if (abs(deviation_y) > deadzone)
        {
            umidade += (deviation_y > 0) ? step : -step;
        }

        if (temperatura > maxTemperatura)
        {
            maxTemperatura = temperatura;
        }
        else if (temperatura < minTemperatura)
        {
            minTemperatura = temperatura;
        }

        if (umidade > maxUmidade)
        {
            maxUmidade = umidade;
        }
        else if (umidade < minUmidade)
        {
            minUmidade = umidade;
        }

        // Exibir os valores (pode ser em um display ou no console)
        uint32_t current_time = to_us_since_boot(get_absolute_time());
        if (current_time - last_time_uart > 5000000)
        {
            last_time_uart = current_time;

            if (temperatura > limiteMaxTemperatura)
            {
                printf("AVISO: LIMITE MÁXIMO TEMPERATURA ULTRAPASSADO!\n");
            }
            if (temperatura < limiteMinTemperatura)
            {
                printf("AVISO: LIMITE MÍNIMO TEMPERATURA ULTRAPASSADO!\n");
            }
            if (umidade > limiteMaxUmidade)
            {
                printf("AVISO: LIMITE MÁXIMO UMIDADE ULTRAPASSADO!\n");
            }
            if (umidade < limiteMinUmidade)
            {
                printf("AVISO: LIMITE MÍNIMO UMIDADE ULTRAPASSADO!\n");
            }

            printf("--------------------------------------------------------------------------------\n");
            printf("Condições atuais:");
            printf("Temperatura: %.1f°C | Umidade: %.1f%%", temperatura, umidade);

            printf("\nCondições Máximas registradas:");
            printf("Temperatura: %.1f°C | Umidade: %.1f%%", maxTemperatura, maxUmidade);

            printf("\nCondições Mínimas registradas:");
            printf("Temperatura: %.1f°C | Umidade: %.1f%%\n", minTemperatura, minUmidade);
            printf("--------------------------------------------------------------------------------\n\n");
        }

        if (display_on)
        {
            // Buffers para armazenar os valores formatados
            char temp_str[20];
            char umid_str[20];

            if (frame == 1)
            {
                // Formatar os valores em strings
                sprintf(temp_str, "Temp: %.1f C", temperatura);
                sprintf(umid_str, "Umi:  %.1f %%", umidade);

                // Limpa o display antes de atualizar
                ssd1306_fill(&ssd, false);

                // Exibir os valores formatados no display
                ssd1306_draw_string(&ssd, "Atual", 47, 8);
                ssd1306_draw_string(&ssd, temp_str, 15, 30);
                ssd1306_draw_string(&ssd, umid_str, 15, 45);
            }
            else if (frame == 2)
            {
                // Formatar os valores em strings
                sprintf(temp_str, "Temp: %.1f C", maxTemperatura);
                sprintf(umid_str, "Umi:  %.1f %%", maxUmidade);

                // Limpa o display antes de atualizar
                ssd1306_fill(&ssd, false);

                // Exibir os valores formatados no display
                ssd1306_draw_string(&ssd, "Max", 56, 8);
                ssd1306_draw_string(&ssd, temp_str, 15, 30);
                ssd1306_draw_string(&ssd, umid_str, 15, 45);
            }
            else if (frame == 3)
            {
                // Formatar os valores em strings
                sprintf(temp_str, "Temp: %.1f C", minTemperatura);
                sprintf(umid_str, "Umi:  %.1f %%", minUmidade);

                // Limpa o display antes de atualizar
                ssd1306_fill(&ssd, false);

                // Exibir os valores formatados no display
                ssd1306_draw_string(&ssd, "Min", 56, 8);
                ssd1306_draw_string(&ssd, temp_str, 15, 30);
                ssd1306_draw_string(&ssd, umid_str, 15, 45);
            }

            // Atualiza o display com os novos dados
            ssd1306_send_data(&ssd);
            sleep_ms(50); // Pequeno atraso para evitar atualizações rápidas demais
        }
    }
}